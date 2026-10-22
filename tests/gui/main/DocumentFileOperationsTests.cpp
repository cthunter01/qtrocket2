#include "QtRocket/gui/main/DocumentFileOperations.h"

#include <cstddef>
#include <filesystem>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QAction>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QGuiApplication>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QtContainerFwd>
#include <QtCore/qtestsupport_core.h>

#include <gtest/gtest.h>

#include "ExpectedQtMessage.h"
#include "GuiTestSupport.h"
#include "QtRocket/document/Attachment.h"
#include "QtRocket/document/DecalImage.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/document/StorageOptions.h"
#include "QtRocket/document/events/DocumentChangeEvent.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/GeneralRocketSaver.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/file/ZipArchive.h"
#include "QtRocket/gui/main/DesignPanel.h"
#include "QtRocket/gui/main/MainMenus.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/main/WindowManager.h"
#include "QtRocket/gui/tree/DocumentSelectionModel.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/Appearance.h"
#include "QtRocket/rocket/Decal.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Signal.h"
#include "ScriptedUserPrompts.h"
#include "TestGuiServices.h"
#include "TestPaths.h"
#include "TestTempDir.h"
#include "document/TestAttachments.h"

// The file operations over a window manager of test services, with the questions answered by
// ScriptedUserPrompts; the facts of OpenRocket that the Java probe FileOperationsProbe pinned
// (probes/m2r3-P3/java/facts.txt) are pinned here.

namespace QtRocket::Gui
{

namespace
{

using LastWindowBehaviour = WindowManager::LastWindowBehaviour;
using PromptKind          = Gui::Test::ScriptedUserPrompts::Kind;
using Prompt              = Gui::Test::ScriptedUserPrompts::Prompt;
using SaveChangesAnswer   = UserPrompts::SaveChangesAnswer;
using Paths               = std::vector<std::filesystem::path>;

/// A design file with a component the loader does not know: one warning.
constexpr std::string_view kWarnedDesign = R"(<?xml version='1.0' encoding='utf-8'?>
<openrocket version="1.9" creator="QtRocket test">
  <rocket>
    <name>Warned</name>
    <subcomponents>
      <stage>
        <name>Sustainer</name>
        <subcomponents>
          <unknowncomponent/>
        </subcomponents>
      </stage>
    </subcomponents>
  </rocket>
</openrocket>
)";

/// The name and the directory Save as proposes.
struct Proposal
{
    QString               fileName;
    std::filesystem::path directory;

    [[nodiscard]] bool operator==(const Proposal& other) const = default;
};

/// The window manager and its file operations over test services and a directory of the test.
class DocumentFileOperationsTest : public Test::GuiTest
{
protected:
    void SetUp() override
    {
        GuiTest::SetUp();
        // The "Save Design Info" dialog is asked about in a test of its own.
        m_services.preferences().setShowSaveRocketInfo(false);
    }

    void TearDown() override
    {
        // The operations, then the windows (and those still to be deleted), before the fixture
        // looks for windows.
        m_operations.reset();
        m_manager.reset();
        deleteLaterNow();
        GuiTest::TearDown();
    }

    /// The manager, made the first time with @p behaviour, with its file operations.
    [[nodiscard]] WindowManager& manager(LastWindowBehaviour behaviour = LastWindowBehaviour::EXIT)
    {
        if (m_manager == nullptr)
        {
            m_manager = std::make_unique<WindowManager>(m_services.services(), behaviour);
            m_operations =
                std::make_unique<DocumentFileOperations>(*m_manager, m_services.services());
        }
        return *m_manager;
    }

    [[nodiscard]] DocumentFileOperations& operations()
    {
        static_cast<void>(manager());
        return *m_operations;
    }

    [[nodiscard]] Gui::Test::ScriptedUserPrompts& prompts() { return m_services.prompts(); }
    [[nodiscard]] InMemoryPreferences&            preferences() { return m_services.preferences(); }
    [[nodiscard]] const GuiServices&              services() const { return m_services.services(); }
    [[nodiscard]] const QtRocket::Test::TempDir&  directory() const { return m_directory; }

    /// The questions and messages so far.
    [[nodiscard]] const std::vector<Prompt>& asked() const
    {
        return m_services.prompts().prompts();
    }

    /// Writes a design whose rocket is named @p rocketName to @p name in the directory.
    [[nodiscard]] std::filesystem::path writeDesign(const std::filesystem::path& name,
                                                    std::string_view             rocketName)
    {
        const std::filesystem::path         file     = m_directory.resolve(name);
        std::unique_ptr<OpenRocketDocument> document = OpenRocketDocumentFactory::createNewRocket();
        document->getRocket().setName(rocketName);
        GeneralRocketSaver saver(m_services.preferences());
        EXPECT_TRUE(saver.save(file, *document).has_value());
        return file;
    }

    /// The design @p file holds, loaded with the services.
    [[nodiscard]] std::unique_ptr<OpenRocketDocument> reload(const std::filesystem::path& file)
    {
        const GeneralRocketLoader loader(m_services.services().loadingContext());
        Result<LoadedDocument>    loaded = loader.load(file);
        EXPECT_TRUE(loaded.has_value()) << (loaded ? std::string() : loaded.error().toString());
        return loaded ? std::move(loaded->document) : nullptr;
    }

    /// The name of the rocket @p file holds.
    [[nodiscard]] std::string savedRocketName(const std::filesystem::path& file)
    {
        const std::unique_ptr<OpenRocketDocument> document = reload(file);
        return document == nullptr ? std::string() : document->getRocket().getName();
    }

    /// A copy of the example design @p name (data/examples) in the directory.
    [[nodiscard]] std::filesystem::path copyExample(std::string_view name) const
    {
        const std::filesystem::path file = pathFromUtf8(name);
        return m_directory.copy(QtRocket::Test::dataDir() / "examples" / file, file);
    }

    /// The name and the directory Save as proposes for a design whose file is @p file, asked for
    /// a window of its own and cancelled.
    [[nodiscard]] Proposal proposalFor(const std::filesystem::path& file)
    {
        std::unique_ptr<OpenRocketDocument> document = OpenRocketDocumentFactory::createNewRocket();
        document->setFile(file);
        MainWindow& window = manager().addDocument(std::move(document));
        prompts().answerSaveFile(std::nullopt);
        EXPECT_FALSE(operations().saveAs(window));
        const UserPrompts::SaveFileRequest& request = prompts().saveFileRequests().back();
        return {.fileName = request.fileName, .directory = request.directory};
    }

    /// Deletes what was given to deleteLater().
    static void deleteLaterNow()
    {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

private:
    QtRocket::Test::TempDir                 m_directory;
    Gui::Test::TestGuiServices              m_services;
    std::unique_ptr<WindowManager>          m_manager;
    std::unique_ptr<DocumentFileOperations> m_operations;
};

/// Records the files a signal tells.
struct RecordFiles
{
    QStringList* files;

    void operator()(const QString& file) const { files->append(file); }
};

/// Counts the documentSaving() events of a document.
struct CountSavings
{
    int* count;

    void operator()(const DocumentChangeEvent& /*event*/) const { ++*count; }
};

/// FileHelper.forceExtension(@p file, "ork").
[[nodiscard]] std::filesystem::path forceOrk(const std::filesystem::path& file)
{
    return DocumentFileOperations::forceExtension(file, QStringLiteral("ork"));
}

/// Whether @p components holds a component.
struct NotEmpty
{
    const std::vector<RocketComponent*>* components;

    [[nodiscard]] bool operator()() const { return !components->empty(); }
};

// ------------------------------------------------------------------ the texts and rules

TEST_F(DocumentFileOperationsTest, ForcesTheExtensionAsFileHelperDoes)
{
    // FileHelper.forceExtension(file, "ork"), the Java probe's FORCE lines (/d is the directory).
    const std::filesystem::path& d = directory().path();
    EXPECT_EQ(forceOrk(d / "x"), d / "x.ork");
    EXPECT_EQ(forceOrk(d / "x.ork"), d / "x.ork");
    EXPECT_EQ(forceOrk(d / "x.ORK"), d / "x.ORK");
    EXPECT_EQ(forceOrk(d / "x.Ork"), d / "x.Ork");
    EXPECT_EQ(forceOrk(d / "x.ork.gz"), d / "x.ork.gz.ork");
    EXPECT_EQ(forceOrk(d / "x.ORK.GZ"), d / "x.ORK.GZ.ork");
    EXPECT_EQ(forceOrk(d / "x.v2"), d / "x.v2.ork");
    EXPECT_EQ(forceOrk(d / ".ork"), d / ".ork.ork");
    EXPECT_EQ(forceOrk(d / "x."), d / "x..ork");
    EXPECT_EQ(forceOrk(d / "x..ork"), d / "x..ork");
    EXPECT_EQ(forceOrk(d / ".hidden"), d / ".hidden.ork");
    EXPECT_EQ(forceOrk(d / "x.rkt"), d / "x.rkt.ork");
    EXPECT_EQ(forceOrk(d / "x.orkk"), d / "x.orkk.ork");
    EXPECT_EQ(forceOrk(d / "x .ork"), d / "x .ork");
    EXPECT_EQ(forceOrk(d / "a.b.c"), d / "a.b.c.ork");
    // Java lowercases the extension in Locale.ENGLISH: the Kelvin sign is a 'k'; the dotted
    // capital I is not an 'o'.
    const std::filesystem::path kelvin = d / QtStrings::toPath(QStringLiteral(u"x.or\u212A"));
    EXPECT_EQ(forceOrk(kelvin), kelvin);
    EXPECT_EQ(forceOrk(d / QtStrings::toPath(QStringLiteral(u"x.\u0130RK"))),
              d / QtStrings::toPath(QStringLiteral(u"x.\u0130RK.ork")));
    EXPECT_EQ(forceOrk(d / QtStrings::toPath(QStringLiteral(u"\u00E9t\u00E9.ork"))),
              d / QtStrings::toPath(QStringLiteral(u"\u00E9t\u00E9.ork")));
    // A dot in the directory is no extension; an extension given with its dot is added as it is.
    EXPECT_EQ(forceOrk(d / "d.dir" / "x"), d / "d.dir" / "x.ork");
    EXPECT_EQ(DocumentFileOperations::forceExtension(d / "x", QStringLiteral(".ork")), d / "x.ork");
    // A relative file: unchanged with the extension, else made absolute as Java's
    // File.getAbsolutePath() makes it.
    EXPECT_EQ(forceOrk("x.ork"), std::filesystem::path("x.ork"));
    EXPECT_EQ(forceOrk("x"), std::filesystem::current_path() / "x.ork");
}

TEST_F(DocumentFileOperationsTest, TheFiltersAreTheDesignsAndAllFiles)
{
    // FileHelper.OPENROCKET_DESIGN_FILTER ("OpenRocket designs (*.ork)", which takes .ork.gz
    // too) and Swing's accept-all filter "All Files", which JFileChooser lists first (the JDK
    // probe of BasicFrame.openAction()'s calls: All Files, All rocket designs, OpenRocket
    // designs; the second comes with RockSim in Milestone 8).
    EXPECT_EQ(DocumentFileOperations::openRocketDesignFilter(),
              QStringLiteral("OpenRocket designs (*.ork *.ork.gz)"));
    EXPECT_EQ(DocumentFileOperations::allFilesFilter(), QStringLiteral("All Files (*)"));
    EXPECT_EQ(DocumentFileOperations::openFilters(),
              (QStringList{QStringLiteral("All Files (*)"),
                           QStringLiteral("OpenRocket designs (*.ork *.ork.gz)")}));
}

// ------------------------------------------------------------------ open

TEST_F(DocumentFileOperationsTest, OpensADesignFileInAWindowOfItsOwn)
{
    const std::filesystem::path file = writeDesign("rocket.ork", "Mars");
    QStringList                 opened;
    QObject::connect(&operations(), &DocumentFileOperations::designFileOpened, &operations(),
                     RecordFiles{.files = &opened});

    const MainWindow* const window = operations().open(file, nullptr);
    ASSERT_NE(window, nullptr);
    EXPECT_EQ(manager().windowCount(), 1U);
    EXPECT_EQ(window->document().getFile(), std::optional(std::filesystem::absolute(file)));
    EXPECT_TRUE(window->document().isSaved());
    EXPECT_EQ(window->document().getRocket().getName(), "Mars");
    EXPECT_EQ(window->windowTitle(), QStringLiteral("[*]Mars (rocket.ork)"));
    EXPECT_FALSE(window->isWindowModified());
    EXPECT_FALSE(manager().isReplaceable(*window));
    EXPECT_TRUE(asked().empty());
    EXPECT_EQ(opened, QStringList{QtStrings::fromPath(std::filesystem::absolute(file))});
    // The wait cursor of the load is gone.
    EXPECT_EQ(QGuiApplication::overrideCursor(), nullptr);
}

TEST_F(DocumentFileOperationsTest, AFileThatCannotBeReadIsNotFound)
{
    // BasicFrame.open(): FileNotFoundException, for a missing file and for a directory (the
    // Java probe's FIS lines).
    static_cast<void>(directory().write("sub/inside.txt", "x"));
    EXPECT_EQ(operations().open(directory().resolve("missing.ork"), nullptr), nullptr);
    EXPECT_EQ(operations().open(directory().resolve("sub"), nullptr), nullptr);
    EXPECT_EQ(manager().windowCount(), 0U);
    ASSERT_EQ(asked().size(), 2U);
    EXPECT_EQ(asked().at(0).kind, PromptKind::MESSAGE);
    EXPECT_EQ(asked().at(0).title, QStringLiteral("Error opening file"));
    EXPECT_EQ(asked().at(0).texts, (QStringList{QStringLiteral("ERROR"),
                                                QStringLiteral("File not found: missing.ork")}));
    EXPECT_EQ(asked().at(1).texts,
              (QStringList{QStringLiteral("ERROR"), QStringLiteral("File not found: sub")}));
}

TEST_F(DocumentFileOperationsTest, AFileThatIsNoDesignCannotBeOpened)
{
    const std::filesystem::path file =
        directory().write("broken.ork", "this is not a design file at all");
    EXPECT_EQ(operations().open(file, nullptr), nullptr);
    EXPECT_EQ(manager().windowCount(), 0U);
    ASSERT_EQ(asked().size(), 1U);
    EXPECT_EQ(asked().front().kind, PromptKind::MESSAGE);
    EXPECT_EQ(asked().front().title, QStringLiteral("Error opening file"));
    EXPECT_EQ(asked().front().texts,
              (QStringList{QStringLiteral("ERROR"),
                           QStringLiteral(
                               "Unable to open file 'broken.ork': Unsupported or corrupt file.")}));
    EXPECT_EQ(QGuiApplication::overrideCursor(), nullptr);
}

/// Records how many questions and messages there were when a window opened.
struct PromptsWhenOpened
{
    const Gui::Test::ScriptedUserPrompts* prompts;
    std::size_t*                          count;

    void operator()() const { *count = prompts->prompts().size(); }
};

TEST_F(DocumentFileOperationsTest, TheWarningsOfALoadAreShownBeforeTheWindowOpens)
{
    const std::filesystem::path file       = directory().write("warned.ork", kWarnedDesign);
    std::size_t                 whenOpened = 0;
    QObject::connect(&manager(), &WindowManager::windowOpened, &manager(),
                     PromptsWhenOpened{.prompts = &prompts(), .count = &whenOpened});
    const MainWindow* const window = operations().open(file, nullptr);
    ASSERT_NE(window, nullptr);
    EXPECT_EQ(window->document().getRocket().getName(), "Warned");
    EXPECT_TRUE(window->document().isSaved());
    ASSERT_EQ(asked().size(), 1U);
    EXPECT_EQ(whenOpened, 1U);
    const Prompt& warnings = asked().front();
    EXPECT_EQ(warnings.kind, PromptKind::WARNINGS);
    // The texts of the Java probe: OPEN-TITLE, OPEN-TXT1, OPEN-TXT2.
    EXPECT_EQ(warnings.title, QStringLiteral("Warnings while opening file"));
    EXPECT_EQ(
        warnings.texts,
        (QStringList{
            QStringLiteral("The following problems were encountered while opening warned.ork."),
            QStringLiteral("Some design features may not have been loaded correctly."),
            QStringLiteral("Unknown element unknowncomponent, ignoring.")}));
}

TEST_F(DocumentFileOperationsTest, AnOpenedFileHasThePlatformsSeparators)
{
    // Java's File has the platform's separators whatever it was named with: a file named with
    // '/' (as Qt's file dialogs name it on Windows) is the same document file and recent file as
    // the one the command line names with '\' (on Linux and macOS both are '/' anyway).
    const std::filesystem::path file = writeDesign("rocket.ork", "Mars");
    const QString native = QtStrings::fromPath(std::filesystem::path(file).make_preferred());
    const std::filesystem::path generic =
        QtStrings::toPath(QDir::fromNativeSeparators(QtStrings::fromPath(file)));
    QStringList opened;
    QObject::connect(&operations(), &DocumentFileOperations::designFileOpened, &operations(),
                     RecordFiles{.files = &opened});
    const MainWindow* const window = operations().open(generic, nullptr);
    ASSERT_NE(window, nullptr);
    EXPECT_EQ(QtStrings::fromPath(window->document().getFile().value_or(std::filesystem::path())),
              native);
    EXPECT_EQ(opened, QStringList{native});
    // A file whose extension is forced, too.
    const std::filesystem::path bare = QtStrings::toPath(
        QDir::fromNativeSeparators(QtStrings::fromPath(directory().resolve("x"))));
    std::filesystem::path forced = directory().resolve("x.ork");
    EXPECT_EQ(QtStrings::fromPath(forceOrk(bare)), QtStrings::fromPath(forced.make_preferred()));
}

TEST_F(DocumentFileOperationsTest, TheSameFileOpenedTwiceActivatesItsWindow)
{
    // Decision U4 (OpenRocket opens a second window on the same file).
    const std::filesystem::path file = writeDesign("rocket.ork", "Mars");
    QStringList                 opened;
    QObject::connect(&operations(), &DocumentFileOperations::designFileOpened, &operations(),
                     RecordFiles{.files = &opened});
    const MainWindow* const first = operations().open(file, nullptr);
    ASSERT_NE(first, nullptr);
    // The window is raised: the offscreen platform cannot, and says so.
    const Gui::Test::ExpectedQtMessage raised(QStringLiteral("does not support raise\\(\\)"));
    EXPECT_EQ(operations().open(file, nullptr), first);
    // By another path too.
    static_cast<void>(directory().write("sub/other.txt", "x"));
    EXPECT_EQ(operations().open(directory().resolve("sub") / ".." / "rocket.ork", nullptr), first);
    EXPECT_EQ(raised.count(), 2);
    EXPECT_EQ(operations().findWindowOfFile(file), first);
    EXPECT_EQ(manager().windowCount(), 1U);
    EXPECT_TRUE(asked().empty());
    EXPECT_EQ(opened.size(), 3);
    EXPECT_EQ(operations().findWindowOfFile(directory().resolve("other.ork")), nullptr);
}

TEST_F(DocumentFileOperationsTest, OpenAsksForTheFilesInTheDefaultDirectory)
{
    const std::filesystem::path first  = writeDesign("first.ork", "First");
    const std::filesystem::path second = writeDesign("second.ork", "Second");
    const std::filesystem::path chosen = directory().resolve("chosen");
    preferences().setDefaultDirectory(directory().path());

    // BasicFrame.openAction(): JFileChooser with the filters, several files, the default
    // directory, which the chooser's directory then becomes.
    prompts().answerOpenFiles(
        UserPrompts::OpenFileAnswer{.files = {first, second}, .directory = chosen});
    operations().openAction(nullptr);
    EXPECT_EQ(manager().windowCount(), 2U);
    ASSERT_EQ(prompts().openFileRequests().size(), 1U);
    const UserPrompts::OpenFileRequest& request = prompts().openFileRequests().front();
    EXPECT_TRUE(request.title.isEmpty());
    EXPECT_EQ(request.nameFilters, DocumentFileOperations::openFilters());
    // "SELECTED OpenRocket designs (*.ork)".
    EXPECT_EQ(request.selectedNameFilter, DocumentFileOperations::openRocketDesignFilter());
    EXPECT_EQ(request.directory, directory().path());
    EXPECT_TRUE(request.multipleFiles);
    EXPECT_TRUE(request.description.isEmpty());
    EXPECT_EQ(preferences().getDefaultDirectory(), std::optional(chosen));

    // Cancelled: nothing opens, and the directory stays.
    prompts().answerOpenFiles(std::nullopt);
    operations().openAction(nullptr);
    EXPECT_EQ(manager().windowCount(), 2U);
    EXPECT_EQ(preferences().getDefaultDirectory(), std::optional(chosen));
}

TEST_F(DocumentFileOperationsTest, AFileOpenedFromANewDesignTakesItsPlace)
{
    // BasicFrame.closeIfReplaceable().
    const std::filesystem::path file      = writeDesign("rocket.ork", "Mars");
    MainWindow&                 newDesign = manager().newDesign();
    prompts().answerOpenFiles(
        UserPrompts::OpenFileAnswer{.files = {file}, .directory = directory().path()});
    operations().openAction(&newDesign);
    deleteLaterNow();
    ASSERT_EQ(manager().windowCount(), 1U);
    EXPECT_EQ(manager().windows().front()->document().getRocket().getName(), "Mars");
}

TEST_F(DocumentFileOperationsTest, OpenFilesOpensEachAndTellsWhetherAnyOpened)
{
    // SwingStartup.handleCommandLine().
    const std::filesystem::path file = writeDesign("rocket.ork", "Mars");
    EXPECT_FALSE(operations().openFiles({}, nullptr));
    EXPECT_FALSE(operations().openFiles({directory().resolve("missing.ork")}, nullptr));
    EXPECT_TRUE(operations().openFiles({directory().resolve("gone.ork"), file}, nullptr));
    EXPECT_EQ(manager().windowCount(), 1U);
    EXPECT_EQ(asked().size(), 2U);
}

// ------------------------------------------------------------------ save and save as

TEST_F(DocumentFileOperationsTest, SaveWritesTheDesignToItsFile)
{
    const std::filesystem::path file   = writeDesign("rocket.ork", "Mars");
    MainWindow* const           window = operations().open(file, nullptr);
    ASSERT_NE(window, nullptr);
    int                                                        savings = 0;
    const Signal<const DocumentChangeEvent&>::ScopedConnection counting(
        window->document().documentSaving().connect(CountSavings{.count = &savings}));
    window->document().getRocket().setName("Venus");
    EXPECT_FALSE(window->document().isSaved());

    EXPECT_TRUE(operations().save(*window));
    EXPECT_EQ(savings, 1);
    EXPECT_TRUE(window->document().isSaved());
    EXPECT_EQ(window->windowTitle(), QStringLiteral("[*]Venus (rocket.ork)"));
    EXPECT_FALSE(window->isWindowModified());
    EXPECT_EQ(savedRocketName(file), "Venus");
    EXPECT_TRUE(asked().empty());
    // No preview image (decision O26) and the type of the file.
    EXPECT_FALSE(window->document().getDefaultStorageOptions().getPreviewImage().has_value());
    EXPECT_EQ(window->document().getDefaultStorageOptions().getFileType(),
              StorageOptions::FileType::OPENROCKET);
    EXPECT_EQ(QGuiApplication::overrideCursor(), nullptr);
}

TEST_F(DocumentFileOperationsTest, SaveOfADesignWithoutAFileIsSaveAs)
{
    MainWindow&                                                window  = manager().newDesign();
    int                                                        savings = 0;
    const Signal<const DocumentChangeEvent&>::ScopedConnection counting(
        window.document().documentSaving().connect(CountSavings{.count = &savings}));
    QStringList savedAs;
    QObject::connect(&operations(), &DocumentFileOperations::designFileSavedAs, &operations(),
                     RecordFiles{.files = &savedAs});
    const std::filesystem::path chosenDirectory = directory().resolve("chosen");
    prompts().answerSaveFile(UserPrompts::SaveFileAnswer{.file      = directory().resolve("new"),
                                                         .directory = chosenDirectory,
                                                         .saveSimulationData = std::nullopt});

    EXPECT_TRUE(operations().save(window));
    EXPECT_EQ(savings, 1);
    const std::filesystem::path file = directory().resolve("new.ork");
    EXPECT_EQ(window.document().getFile(), std::optional(file));
    EXPECT_TRUE(window.document().isSaved());
    EXPECT_EQ(window.windowTitle(), QStringLiteral("[*]Rocket (new.ork)"));
    EXPECT_EQ(savedRocketName(file), "Rocket");
    EXPECT_EQ(savedAs, QStringList{QtStrings::fromPath(file)});
    EXPECT_EQ(preferences().getDefaultDirectory(), std::optional(chosenDirectory));
    // StorageOptionChooser.storeOptions() marks the options set, whatever they are.
    EXPECT_TRUE(window.document().getDefaultStorageOptions().isExplicitlySet());

    // DesignFileSaveAsFileChooser: the title, the OpenRocket designs only, no default directory
    // and no file yet, no simulated data to store.
    ASSERT_EQ(prompts().saveFileRequests().size(), 1U);
    const UserPrompts::SaveFileRequest& request = prompts().saveFileRequests().front();
    EXPECT_EQ(request.title, QStringLiteral("Save as OpenRocket ork file"));
    EXPECT_EQ(request.nameFilters,
              QStringList{QStringLiteral("OpenRocket designs (*.ork *.ork.gz)")});
    EXPECT_TRUE(request.directory.empty());
    EXPECT_TRUE(request.fileName.isEmpty());
    EXPECT_EQ(request.saveSimulationData, std::nullopt);
    // SaveFileChooser.approveSelection(): names with illegal characters refused.
    EXPECT_TRUE(request.refuseIllegalNames);
    ASSERT_EQ(asked().size(), 1U);
    EXPECT_EQ(asked().front().kind, PromptKind::SAVE_FILE);
}

TEST_F(DocumentFileOperationsTest, SaveAsProposesTheDesignsFileWithTheExtensionForced)
{
    // forceExtension(document.getFileNoExtension(), "ork") (the Java probe's PROPOSED lines), for
    // files of the default directory, where SaveFileChooser.setSelectedFile() takes them.
    const std::filesystem::path defaultDirectory = directory().resolve("default");
    preferences().setDefaultDirectory(defaultDirectory);
    const Proposal gzip = proposalFor(defaultDirectory / "x.ork.gz");
    EXPECT_EQ(gzip.fileName, QStringLiteral("x.ork.gz.ork"));
    EXPECT_EQ(gzip.directory, defaultDirectory);
    EXPECT_EQ(proposalFor(defaultDirectory / "x.ORK").fileName, QStringLiteral("x.ORK"));
    EXPECT_EQ(proposalFor(defaultDirectory / "x.rkt").fileName, QStringLiteral("x.ork"));
    EXPECT_EQ(proposalFor(defaultDirectory / "x").fileName, QStringLiteral("x.ork"));
}

TEST_F(DocumentFileOperationsTest, SaveAsProposesOnlyAFileSaveFileChooserTakes)
{
    // SaveFileChooser.setSelectedFile(): the file's path relative to the chooser's directory
    // (getFilenameInput()) must be a legal name; JFileChooser.setSelectedFile() then takes the
    // chooser to the file's directory. The probe of DesignFileSaveAsFileChooser.build() with the
    // default directory <work>/default (selected file, directory):
    const std::filesystem::path defaultDirectory = directory().resolve("default");
    preferences().setDefaultDirectory(defaultDirectory);
    // default/x.ork: proposed, in default.
    const Proposal inDefault = proposalFor(defaultDirectory / "x.ork");
    EXPECT_EQ(inDefault.fileName, QStringLiteral("x.ork"));
    EXPECT_EQ(inDefault.directory, defaultDirectory);
    // other/y.ork, default/sub/z.ork, default/a:b.ork and other/w.rkt: nothing, in default.
    const Proposal nothing{.fileName = QString(), .directory = defaultDirectory};
    EXPECT_EQ(proposalFor(directory().resolve("other") / "y.ork"), nothing);
    EXPECT_EQ(proposalFor(defaultDirectory / "sub" / "z.ork"), nothing);
    EXPECT_EQ(proposalFor(defaultDirectory / "a:b.ork"), nothing);
    EXPECT_EQ(proposalFor(directory().resolve("other") / "w.rkt"), nothing);
    // defaultX/s.ork: "X" + "/s.ork" without its first separator is a legal name: proposed, in
    // defaultX.
    const Proposal sibling = proposalFor(directory().resolve("defaultX") / "s.ork");
    EXPECT_EQ(sibling.fileName, QStringLiteral("s.ork"));
    EXPECT_EQ(sibling.directory, directory().resolve("defaultX"));

    // Without a default directory, the chooser's directory is the user's home directory, which
    // the test's directory is not: nothing is proposed.
    preferences().setDefaultDirectory(std::nullopt);
    const Proposal noDefault = proposalFor(defaultDirectory / "x.ork");
    EXPECT_TRUE(noDefault.fileName.isEmpty());
    EXPECT_TRUE(noDefault.directory.empty());
}

TEST_F(DocumentFileOperationsTest, SaveAsAsksBeforeOverwritingAFile)
{
    const std::filesystem::path taken  = directory().write("taken.ork", "taken");
    MainWindow&                 window = manager().newDesign();
    window.document().getRocket().setName("Mars");

    // FileHelper.confirmWrite() after the extension is forced: No keeps the file.
    prompts().answerSaveFile(UserPrompts::SaveFileAnswer{.file      = directory().resolve("taken"),
                                                         .directory = directory().path(),
                                                         .saveSimulationData = std::nullopt});
    prompts().answerOverwrite(false);
    EXPECT_FALSE(operations().saveAs(window));
    EXPECT_EQ(readTextFile(taken).value_or(""), "taken");
    EXPECT_EQ(window.document().getFile(), std::nullopt);
    EXPECT_FALSE(window.document().isSaved());
    ASSERT_EQ(asked().size(), 2U);
    EXPECT_EQ(asked().at(1).kind, PromptKind::OVERWRITE);
    EXPECT_EQ(asked().at(1).texts, QStringList{QStringLiteral("taken.ork")});

    // Yes overwrites it.
    prompts().answerSaveFile(UserPrompts::SaveFileAnswer{
        .file = taken, .directory = directory().path(), .saveSimulationData = std::nullopt});
    prompts().answerOverwrite(true);
    EXPECT_TRUE(operations().saveAs(window));
    EXPECT_EQ(savedRocketName(taken), "Mars");
    EXPECT_EQ(window.document().getFile(), std::optional(taken));
}

TEST_F(DocumentFileOperationsTest, SaveAsCancelledSavesNothing)
{
    MainWindow& window = manager().newDesign();
    window.document().getRocket().setName("Mars");
    prompts().answerSaveFile(std::nullopt);
    EXPECT_FALSE(operations().saveAs(window));
    EXPECT_EQ(window.document().getFile(), std::nullopt);
    EXPECT_FALSE(window.document().isSaved());
    EXPECT_FALSE(window.document().getDefaultStorageOptions().isExplicitlySet());
    EXPECT_EQ(preferences().getDefaultDirectory(), std::nullopt);
}

/// The design information's edit: the designer.
struct SetDesigner
{
    void operator()(OpenRocketDocument& document) const
    {
        document.getRocket().setDesigner("Sampo");
    }
};

TEST_F(DocumentFileOperationsTest, SaveAsShowsTheDesignInformationFirstWithTheRocketSelected)
{
    // BasicFrame.showSaveRocketInfoDialog() when the preference asks for it.
    preferences().setShowSaveRocketInfo(true);
    MainWindow& window = manager().newDesign();
    prompts().answerDesignInformation(SetDesigner{});
    prompts().answerSaveFile(UserPrompts::SaveFileAnswer{.file = directory().resolve("info.ork"),
                                                         .directory          = directory().path(),
                                                         .saveSimulationData = std::nullopt});
    EXPECT_TRUE(operations().saveAs(window));
    ASSERT_EQ(asked().size(), 2U);
    EXPECT_EQ(asked().at(0).kind, PromptKind::DESIGN_INFORMATION);
    EXPECT_EQ(asked().at(1).kind, PromptKind::SAVE_FILE);
    const DesignPanel* const panel = window.componentDesignPanel();
    ASSERT_NE(panel, nullptr);
    EXPECT_EQ(panel->selection().selectedComponent(), &window.document().getRocket());
    const std::unique_ptr<OpenRocketDocument> saved = reload(directory().resolve("info.ork"));
    ASSERT_NE(saved, nullptr);
    EXPECT_EQ(saved->getRocket().getDesigner(), "Sampo");
}

TEST_F(DocumentFileOperationsTest, AFailedSaveSaysSoAndSavesNothing)
{
    MainWindow& window = manager().newDesign();
    window.document().getRocket().setName("Mars");
    prompts().answerSaveFile(
        UserPrompts::SaveFileAnswer{.file               = directory().resolve("nowhere") / "x",
                                    .directory          = directory().path(),
                                    .saveSimulationData = std::nullopt});
    EXPECT_FALSE(operations().saveAs(window));
    EXPECT_EQ(window.document().getFile(), std::nullopt);
    EXPECT_FALSE(window.document().isSaved());
    ASSERT_EQ(asked().size(), 2U);
    const Prompt& message = asked().at(1);
    EXPECT_EQ(message.kind, PromptKind::MESSAGE);
    EXPECT_EQ(message.title, QStringLiteral("Saving failed"));
    ASSERT_EQ(message.texts.size(), 3);
    EXPECT_EQ(message.texts.at(0), QStringLiteral("ERROR"));
    EXPECT_EQ(message.texts.at(1), QStringLiteral("An I/O error occurred while saving:"));
    EXPECT_FALSE(message.texts.at(2).isEmpty());
    EXPECT_EQ(QGuiApplication::overrideCursor(), nullptr);
}

/// The number of entries of the directory @p directory.
[[nodiscard]] std::ptrdiff_t entryCount(const std::filesystem::path& directory)
{
    return std::distance(std::filesystem::directory_iterator(directory),
                         std::filesystem::directory_iterator());
}

TEST_F(DocumentFileOperationsTest, AFailedSaveOfADesignWithAFileKeepsItsFile)
{
    // BasicFrame.saveAsOpenRocket(): an IOException leaves the document's file as it was and the
    // design not saved. Here a directory took the file's place, which the save cannot replace
    // (as root too, as the CI containers run); it stays as it was, and no temporary file is left.
    const std::filesystem::path file   = writeDesign("rocket.ork", "Mars");
    MainWindow* const           window = operations().open(file, nullptr);
    ASSERT_NE(window, nullptr);
    const std::optional<std::filesystem::path> opened = window->document().getFile();
    ASSERT_TRUE(std::filesystem::remove(file));
    static_cast<void>(directory().write("rocket.ork/kept.txt", "kept"));
    window->document().getRocket().setName("Venus");
    EXPECT_FALSE(operations().save(*window));
    EXPECT_EQ(window->document().getFile(), opened);
    EXPECT_FALSE(window->document().isSaved());
    ASSERT_EQ(asked().size(), 1U);
    EXPECT_EQ(asked().front().kind, PromptKind::MESSAGE);
    EXPECT_EQ(asked().front().title, QStringLiteral("Saving failed"));
    EXPECT_EQ(asked().front().texts.value(1),
              QStringLiteral("An I/O error occurred while saving:"));
    EXPECT_TRUE(std::filesystem::is_directory(file));
    EXPECT_EQ(readTextFile(file / "kept.txt").value_or(""), "kept");
    EXPECT_EQ(entryCount(directory().path()), 1);
    EXPECT_EQ(QGuiApplication::overrideCursor(), nullptr);
}

TEST_F(DocumentFileOperationsTest, TheSaversWarningsAreShownAfterTheSave)
{
    // Decision U4: GeneralRocketSaver's warnings, here the read-only file it replaced, with the
    // texts of BasicFrame's RASAero save (the Java probe: SAVE-TITLE, SAVE-TXT1, SAVE-TXT2).
    const std::filesystem::path file   = writeDesign("rocket.ork", "Mars");
    MainWindow* const           window = operations().open(file, nullptr);
    ASSERT_NE(window, nullptr);
    ASSERT_TRUE(QtRocket::Test::makeReadOnly(file));
    window->document().getRocket().setName("Venus");
    EXPECT_TRUE(operations().save(*window));
    EXPECT_TRUE(window->document().isSaved());
    EXPECT_EQ(savedRocketName(file), "Venus");
    ASSERT_EQ(asked().size(), 1U);
    const Prompt& warnings = asked().front();
    EXPECT_EQ(warnings.kind, PromptKind::WARNINGS);
    EXPECT_EQ(warnings.title, QStringLiteral("Warnings while saving file"));
    EXPECT_EQ(warnings.texts,
              (QStringList{QStringLiteral("The following problems were encountered while saving "
                                          "'rocket.ork'."),
                           QStringLiteral("Some design features may not have exported correctly."),
                           QtStrings::fromUtf8(GeneralRocketSaver::fileWasReadOnly(
                               std::filesystem::absolute(file)))}));
}

// ------------------------------------------------------------------ the storage options

TEST_F(DocumentFileOperationsTest, SaveAsksForTheStorageOptionsOfSimulatedData)
{
    // A loaded example with stored flight data: options not set, all data saved (the Java
    // probe: STORAGE Tube fin rocket.ork saveSimulationData=true explicitlySet=false
    // hasData=true; without motors its simulation cannot run, and still has data).
    const std::filesystem::path file   = copyExample("Tube fin rocket.ork");
    MainWindow* const           window = operations().open(file, nullptr);
    ASSERT_NE(window, nullptr);
    const StorageOptions& options = window->document().getDefaultStorageOptions();
    EXPECT_FALSE(options.isExplicitlySet());
    EXPECT_TRUE(options.getSaveSimulationData());
    EXPECT_TRUE(DocumentFileOperations::hasSimulatedData(window->document()));
    const std::size_t                    loadMessages = asked().size();
    const Result<std::vector<std::byte>> before       = readFile(file);
    ASSERT_TRUE(before.has_value());

    // StorageOptionChooser.verifyStorageOptions(): Cancel cancels the save.
    prompts().answerStorageOptions(std::nullopt);
    EXPECT_FALSE(operations().save(*window));
    EXPECT_EQ(readFile(file).value_or(std::vector<std::byte>()), *before);
    ASSERT_EQ(asked().size(), loadMessages + 1);
    EXPECT_EQ(asked().back().kind, PromptKind::STORAGE_OPTIONS);
    EXPECT_EQ(asked().back().texts, QStringList{QStringLiteral("all simulated data")});

    // The summary only: the options are set, and the file holds no flight data.
    prompts().answerStorageOptions(false);
    EXPECT_TRUE(operations().save(*window));
    EXPECT_TRUE(options.isExplicitlySet());
    EXPECT_FALSE(options.getSaveSimulationData());
    const std::unique_ptr<OpenRocketDocument> saved = reload(file);
    ASSERT_NE(saved, nullptr);
    EXPECT_FALSE(DocumentFileOperations::hasSimulatedData(*saved));

    // Set explicitly: the next save asks nothing.
    const std::size_t asks = asked().size();
    EXPECT_TRUE(operations().save(*window));
    EXPECT_EQ(asked().size(), asks);
}

TEST_F(DocumentFileOperationsTest, SaveAsOffersTheStorageOptionOfSimulatedData)
{
    // The design's file in the default directory, where Save as proposes it.
    preferences().setDefaultDirectory(directory().path());
    const std::filesystem::path file   = copyExample("Tube fin rocket.ork");
    MainWindow* const           window = operations().open(file, nullptr);
    ASSERT_NE(window, nullptr);
    prompts().answerSaveFile(UserPrompts::SaveFileAnswer{.file = directory().resolve("copy.ork"),
                                                         .directory          = directory().path(),
                                                         .saveSimulationData = false});
    EXPECT_TRUE(operations().saveAs(*window));
    ASSERT_FALSE(prompts().saveFileRequests().empty());
    EXPECT_EQ(prompts().saveFileRequests().back().saveSimulationData, std::optional<bool>(true));
    EXPECT_EQ(prompts().saveFileRequests().back().fileName, QStringLiteral("Tube fin rocket.ork"));
    const StorageOptions& options = window->document().getDefaultStorageOptions();
    EXPECT_TRUE(options.isExplicitlySet());
    EXPECT_FALSE(options.getSaveSimulationData());
    // No second question: the options are set.
    EXPECT_NE(asked().back().kind, PromptKind::STORAGE_OPTIONS);
}

// ------------------------------------------------------------------ the decal loop

/// A design with a nose cone whose decal image's source file is missing, saved as decal.ork.
struct MissingDecal
{
    std::unique_ptr<OpenRocketDocument> document = OpenRocketDocumentFactory::createNewRocket();
    std::shared_ptr<DecalImage>         image;

    explicit MissingDecal(const std::filesystem::path& gone)
    {
        RocketComponent& stage = document->getRocket().getChild(0);
        auto&            nose  = stage.addChild(std::make_unique<NoseCone>());
        image                  = document->getDecalImage(
            std::make_shared<QtRocket::Test::MemoryAttachment>("decals/logo.png", "attached"));
        image->setDecalFile(gone);
        nose.setAppearance(Appearance(Color(10, 20, 30), 0.5,
                                      Decal(Coordinate(0, 0), Coordinate(0, 0), Coordinate(1, 1),
                                            0.0, "decals/logo.png", Decal::EdgeMode::REPEAT)));
    }
};

/// The bytes of the entry @p name of the archive @p file, as text; nullopt when it has none.
[[nodiscard]] std::optional<std::string> entryText(const std::filesystem::path& file,
                                                   std::string_view             name)
{
    const Result<std::vector<std::byte>> bytes = readFile(file);
    if (!bytes.has_value())
    {
        return std::nullopt;
    }
    const Result<ZipArchive> archive = ZipArchive::fromBytes(*bytes);
    if (!archive.has_value())
    {
        return std::nullopt;
    }
    const std::vector<std::byte>* const entry = archive->find(name);
    if (entry == nullptr)
    {
        return std::nullopt;
    }
    return bytesToString(*entry);
}

TEST_F(DocumentFileOperationsTest, AMissingDecalFileIsLookedForAndTheSaveTriedAgain)
{
    // BasicFrame.saveAsOpenRocket(): DecalNotFoundException -> DecalNotFoundDialog -> the chosen
    // file is the image's source, and the save is tried again.
    const std::filesystem::path gone  = directory().resolve("gone.png");
    const std::filesystem::path found = directory().write("pictures/found.png", "found");
    MissingDecal                design(gone);
    design.document->setFile(directory().resolve("decal.ork"));
    MainWindow& window = manager().addDocument(std::move(design.document));
    preferences().setDefaultDirectory(directory().path());
    prompts().answerDecalFile(
        UserPrompts::OpenFileAnswer{.files = {found}, .directory = found.parent_path()});

    EXPECT_TRUE(operations().save(window));
    EXPECT_TRUE(window.document().isSaved());
    ASSERT_EQ(asked().size(), 1U);
    EXPECT_EQ(asked().front().kind, PromptKind::DECAL_FILE);
    EXPECT_EQ(
        asked().front().texts,
        (QStringList{QtStrings::fromUtf8(QtRocket::decalNotFound(
                                             QtRocket::pathToUtf8(std::filesystem::absolute(gone)))
                                             .error()
                                             .message),
                     QtStrings::fromPath(directory().path())}));
    EXPECT_EQ(design.image->getDecalFile(), std::optional(found));
    EXPECT_EQ(preferences().getDefaultDirectory(), std::optional(found.parent_path()));
    EXPECT_EQ(entryText(directory().resolve("decal.ork"), "decals/logo.png"),
              std::optional<std::string>("found"));
}

TEST_F(DocumentFileOperationsTest, AMissingDecalFileNotLookedForIsLeftOutOfTheSave)
{
    // DecalNotFoundDialog answered No (or its chooser cancelled): decal.setIgnored(true), and the
    // save is tried again without the image, which is not ignored afterwards.
    MissingDecal design(directory().resolve("gone.png"));
    design.document->setFile(directory().resolve("decal.ork"));
    MainWindow& window = manager().addDocument(std::move(design.document));
    prompts().answerDecalFile(std::nullopt);

    EXPECT_TRUE(operations().save(window));
    EXPECT_TRUE(window.document().isSaved());
    EXPECT_EQ(entryText(directory().resolve("decal.ork"), "decals/logo.png"), std::nullopt);
    EXPECT_TRUE(entryText(directory().resolve("decal.ork"), "rocket.ork").has_value());
    EXPECT_FALSE(design.image->isIgnored());
    EXPECT_EQ(preferences().getDefaultDirectory(), std::nullopt);
}

// ------------------------------------------------------------------ closing and the menus

TEST_F(DocumentFileOperationsTest, ClosingAChangedDesignSavesItWhenAsked)
{
    // BasicFrame.closeAction(): Save in "Design not saved" is saveAction().
    const std::filesystem::path file   = writeDesign("rocket.ork", "Mars");
    MainWindow* const           window = operations().open(file, nullptr);
    ASSERT_NE(window, nullptr);
    window->document().getRocket().setName("Venus");
    prompts().answerSaveChanges(SaveChangesAnswer::SAVE);
    EXPECT_TRUE(manager().close(*window));
    EXPECT_EQ(manager().windowCount(), 0U);
    EXPECT_EQ(savedRocketName(file), "Venus");
}

TEST_F(DocumentFileOperationsTest, ClosingANewDesignWhoseSaveIsCancelledKeepsTheWindow)
{
    MainWindow& window = manager().newDesign();
    window.document().getRocket().setName("Venus");
    prompts().answerSaveChanges(SaveChangesAnswer::SAVE);
    prompts().answerSaveFile(std::nullopt);
    EXPECT_FALSE(manager().close(window));
    EXPECT_EQ(manager().windowCount(), 1U);
}

/// Records the components whose configuration dialog a window asks for.
struct RecordDialogRequests
{
    std::vector<RocketComponent*>* components;

    void operator()(RocketComponent* component) const { components->push_back(component); }
};

TEST_F(DocumentFileOperationsTest, TheFileMenuOpensSavesAndShowsTheProperties)
{
    MainWindow&      window = manager().newDesign();
    const MainMenus& menus  = window.menus();
    EXPECT_TRUE(menus.openAction()->isEnabled());
    EXPECT_TRUE(menus.saveAction()->isEnabled());
    EXPECT_TRUE(menus.saveAsAction()->isEnabled());
    EXPECT_TRUE(menus.propertiesAction()->isEnabled());

    prompts().answerOpenFiles(std::nullopt);
    menus.openAction()->trigger();
    prompts().answerSaveFile(std::nullopt);
    menus.saveAction()->trigger();
    prompts().answerSaveFile(std::nullopt);
    menus.saveAsAction()->trigger();
    ASSERT_EQ(asked().size(), 3U);
    EXPECT_EQ(asked().at(0).kind, PromptKind::OPEN_FILES);
    EXPECT_EQ(asked().at(1).kind, PromptKind::SAVE_FILE);
    EXPECT_EQ(asked().at(2).kind, PromptKind::SAVE_FILE);

    // File > Properties: the rocket's configuration dialog (BasicFrame: showDialog(rocket)).
    std::vector<RocketComponent*> requested;
    QObject::connect(&window, &MainWindow::configDialogRequested, &window,
                     RecordDialogRequests{.components = &requested});
    menus.propertiesAction()->trigger();
    EXPECT_EQ(requested, std::vector<RocketComponent*>{&window.document().getRocket()});
}

TEST_F(DocumentFileOperationsTest, TheMenusOfNoWindowOpenFiles)
{
    // macOS without a window (DummyFrameMenuOSX): File > Open.
    const std::filesystem::path file   = writeDesign("rocket.ork", "Mars");
    MainWindow&                 window = manager(LastWindowBehaviour::KEEP_RUNNING).newDesign();
    static_cast<void>(operations());
    EXPECT_TRUE(manager().close(window));
    deleteLaterNow();
    const MainMenus* const menus = manager().noWindowMenus();
    ASSERT_NE(menus, nullptr);
    EXPECT_TRUE(menus->openAction()->isEnabled());
    prompts().answerOpenFiles(
        UserPrompts::OpenFileAnswer{.files = {file}, .directory = directory().path()});
    menus->openAction()->trigger();
    ASSERT_EQ(manager().windowCount(), 1U);
    EXPECT_EQ(manager().windows().front()->document().getRocket().getName(), "Mars");
}

TEST_F(DocumentFileOperationsTest, AnExampleOpensWithoutAFileAndAsksForTheRocketsDialog)
{
    // BasicFrame.open(URL, parent): no file, saved, not a recent file, the rocket's dialog once
    // the window is shown.
    const std::filesystem::path file = copyExample("Tube fin rocket.ork");
    QStringList                 opened;
    QObject::connect(&operations(), &DocumentFileOperations::designFileOpened, &operations(),
                     RecordFiles{.files = &opened});
    const MainWindow* const window = operations().openExample(file, nullptr);
    ASSERT_NE(window, nullptr);
    EXPECT_EQ(window->document().getFile(), std::nullopt);
    EXPECT_TRUE(window->document().isSaved());
    EXPECT_EQ(
        window->windowTitle(),
        QStringLiteral("[*]") + QtStrings::fromUtf8(window->document().getRocket().getName()));
    EXPECT_TRUE(opened.isEmpty());
    std::vector<RocketComponent*> requested;
    QObject::connect(window, &MainWindow::configDialogRequested, window,
                     RecordDialogRequests{.components = &requested});
    EXPECT_TRUE(requested.empty());
    EXPECT_TRUE(QTest::qWaitFor(NotEmpty{.components = &requested}));
    EXPECT_EQ(requested, std::vector<RocketComponent*>{&window->document().getRocket()});
    // A missing example is not found.
    EXPECT_EQ(operations().openExample(directory().resolve("missing.ork"), nullptr), nullptr);
    EXPECT_EQ(asked().back().texts, (QStringList{QStringLiteral("ERROR"),
                                                 QStringLiteral("File not found: missing.ork")}));
}

// ------------------------------------------------------------------ a window that goes

/// Questions that close the window of a Save as while it asks for the file (another window's
/// event, a timer), then answer with a file; everything else is answered by cancelling.
class ClosingPrompts final : public UserPrompts
{
public:
    void closeOnSaveFile(WindowManager& manager, MainWindow& window, std::filesystem::path file)
    {
        m_manager = &manager;
        m_window  = &window;
        m_file    = std::move(file);
    }

    [[nodiscard]] InsertPositionAnswer askInsertPosition(
        QWidget* /*parent*/, InsertPositionQuestion /*question*/) override
    {
        return {};
    }
    [[nodiscard]] DiscardAnswer askDiscardChanges(QWidget* /*parent*/,
                                                  DiscardQuestion /*question*/) override
    {
        return {};
    }
    [[nodiscard]] SaveChangesAnswer askSaveChanges(QWidget* /*parent*/,
                                                   const QString& /*designName*/) override
    {
        return SaveChangesAnswer::DISCARD;
    }
    [[nodiscard]] bool askOverwrite(QWidget* /*parent*/, const QString& /*fileName*/) override
    {
        return true;
    }
    [[nodiscard]] std::optional<bool> askStorageOptions(QWidget* /*parent*/,
                                                        bool /*saveSimulationData*/) override
    {
        return true;
    }
    [[nodiscard]] bool askYesNo(QWidget* /*parent*/, const Question& /*question*/) override
    {
        return false;
    }
    [[nodiscard]] std::optional<OpenFileAnswer> askDecalFile(
        QWidget* /*parent*/, const QString& /*message*/,
        const std::filesystem::path& /*directory*/) override
    {
        return std::nullopt;
    }
    [[nodiscard]] std::optional<OpenFileAnswer> askOpenFiles(
        QWidget* /*parent*/, const OpenFileRequest& /*request*/) override
    {
        return std::nullopt;
    }
    [[nodiscard]] std::optional<SaveFileAnswer> askSaveFile(
        QWidget* /*parent*/, const SaveFileRequest& /*request*/) override
    {
        // The window is closed and destroyed while the question is open.
        EXPECT_TRUE(m_manager->close(*m_window));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        return SaveFileAnswer{.file = m_file, .directory = {}, .saveSimulationData = std::nullopt};
    }
    void editDesignInformation(QWidget* /*parent*/, OpenRocketDocument& /*document*/) override { }
    void showMessage(QWidget* /*parent*/, const Message& /*message*/) override { }
    void showWarnings(QWidget* /*parent*/, const WarningsMessage& /*message*/) override { }
    void showErrorsAndWarnings(QWidget* /*parent*/,
                               const ErrorsAndWarningsMessage& /*message*/) override
    {
    }
    [[nodiscard]] InternalErrorAnswer showInternalError(QWidget* /*parent*/,
                                                        const Guard::Failure& /*failure*/) override
    {
        return InternalErrorAnswer::CONTINUE;
    }

private:
    WindowManager*        m_manager{nullptr};
    MainWindow*           m_window{nullptr};
    std::filesystem::path m_file;
};

TEST_F(DocumentFileOperationsTest, ASaveWhoseWindowClosesDuringAQuestionStops)
{
    // The window and its document are gone when the question returns: the save stops (ASan
    // checks that nothing of them is used).
    ClosingPrompts         closing;
    const GuiServices      base = services();
    const GuiServices      closingServices(base.preferences(), base.materials(), base.motors(),
                                           base.loadingContext(), closing);
    WindowManager          windows(closingServices);
    DocumentFileOperations fileOperations(windows, closingServices);
    MainWindow&            window = windows.newDesign();
    window.document().getRocket().setName("Mars");
    const std::filesystem::path file = directory().resolve("closed.ork");
    closing.closeOnSaveFile(windows, window, file);
    EXPECT_FALSE(fileOperations.saveAs(window));
    EXPECT_EQ(windows.windowCount(), 0U);
    EXPECT_FALSE(std::filesystem::exists(file));
}

}  // namespace

}  // namespace QtRocket::Gui
