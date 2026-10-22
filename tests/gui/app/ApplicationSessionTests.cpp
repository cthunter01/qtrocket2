#include "QtRocket/gui/app/ApplicationSession.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QFileOpenEvent>
#include <QGuiApplication>
#include <QObject>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QWindow>
#include <Qt>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/GeneralRocketSaver.h"
#include "QtRocket/gui/app/Application.h"
#include "QtRocket/gui/app/ApplicationContext.h"
#include "QtRocket/gui/app/DataPaths.h"
#include "QtRocket/gui/main/DocumentFileOperations.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/main/WindowManager.h"
#include "QtRocket/gui/settings/SettingsPreferences.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "RecordingGuardHook.h"
#include "ScriptedUserPrompts.h"
#include "TestTempDir.h"
#include "unit/DefaultUnitsGuard.h"

// The start of the application as main() makes it (gui/main.cpp): an ApplicationContext over a
// preferences file of the test's own and the data of the build tree, then the ApplicationSession,
// on the tests' Application, without the event loop (or with it, for a moment).

namespace QtRocket::Gui
{

namespace
{

using Behaviour  = WindowManager::LastWindowBehaviour;
using PromptKind = Gui::Test::ScriptedUserPrompts::Kind;

class ApplicationSessionTest : public Test::GuiTest
{
protected:
    void TearDown() override
    {
        // The windows, the documents and the services (the questions among them) go first.
        stop();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        GuiTest::TearDown();
    }

    [[nodiscard]] static Application& application()
    {
        Application* const current = Application::current();
        QTROCKET_ASSERT(current != nullptr);
        return *current;
    }

    /// The options of a start with @p behaviour (the platform's would differ on macOS).
    [[nodiscard]] static ApplicationSession::Options options(Behaviour behaviour = Behaviour::EXIT)
    {
        ApplicationSession::Options options;
        options.lastWindowBehaviour = behaviour;
        return options;
    }

    /// The preferences file of the test.
    [[nodiscard]] QString preferencesFile() const
    {
        return QtStrings::fromPath(m_directory.resolve("QtRocket.ini"));
    }

    /// The services as main() makes them, with the test's questions and @p dataPaths.
    [[nodiscard]] std::unique_ptr<ApplicationContext> makeContext(
        const DataPaths& dataPaths = DataPaths::forApplication())
    {
        auto prompts = std::make_unique<Gui::Test::ScriptedUserPrompts>();
        m_prompts    = prompts.get();
        return std::make_unique<ApplicationContext>(
            std::make_unique<SettingsPreferences>(preferencesFile()), dataPaths,
            std::move(prompts));
    }

    /// Starts the application on @p context.
    ApplicationSession& start(std::unique_ptr<ApplicationContext> context,
                              const ApplicationSession::Options&  startOptions = options())
    {
        m_session =
            std::make_unique<ApplicationSession>(application(), std::move(context), startOptions);
        return *m_session;
    }

    /// Starts the application as main() does.
    ApplicationSession& start(const ApplicationSession::Options& startOptions = options())
    {
        return start(makeContext(), startOptions);
    }

    /// Ends the application: its windows, documents and services.
    void stop()
    {
        m_session.reset();
        m_prompts = nullptr;
    }

    /// The questions of the running application.
    [[nodiscard]] Gui::Test::ScriptedUserPrompts& prompts() const
    {
        QTROCKET_ASSERT(m_prompts != nullptr);
        return *m_prompts;
    }

    [[nodiscard]] const QtRocket::Test::TempDir& directory() const noexcept { return m_directory; }

    /// Writes a design whose rocket is named @p rocketName to @p name in the directory.
    [[nodiscard]] std::filesystem::path writeDesign(const std::filesystem::path& name,
                                                    std::string_view             rocketName) const
    {
        const std::filesystem::path         file     = m_directory.resolve(name);
        std::unique_ptr<OpenRocketDocument> document = OpenRocketDocumentFactory::createNewRocket();
        document->getRocket().setName(rocketName);
        const InMemoryPreferences preferences;
        GeneralRocketSaver        saver(preferences);
        EXPECT_TRUE(saver.save(file, *document).has_value());
        return file;
    }

private:
    QtRocket::Test::TempDir             m_directory;
    Gui::Test::ScriptedUserPrompts*     m_prompts{nullptr};
    std::unique_ptr<ApplicationSession> m_session;
};

void throwBug()
{
    bug("the slot broke");
}

/// An object whose event handler throws a BugError for QEvent::User.
class BrokenObject : public QObject
{
public:
    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::User)
        {
            bug("the event handler broke");
        }
        return QObject::event(event);
    }
};

/// The name of the material of a new @p Component, which the start made the default one.
template <class Component>
[[nodiscard]] std::string startMaterial(const ApplicationSession& session)
{
    Component component;
    component.applyDefaultMaterial(session.context().services().preferences(),
                                   session.context().services().materials());
    return component.getMaterial().getName();
}

/// The first window of @p session.
[[nodiscard]] MainWindow& firstWindow(const ApplicationSession& session)
{
    QTROCKET_ASSERT(session.windows().windowCount() > 0);
    return *session.windows().windows().front();
}

/// Sends a QEvent::Quit to the application (the macOS Dock's Quit); whether it was accepted.
[[nodiscard]] bool sendQuit()
{
    QEvent quit(QEvent::Quit);
    quit.ignore();
    QCoreApplication::sendEvent(Application::current(), &quit);
    return quit.isAccepted();
}

TEST_F(ApplicationSessionTest, StartsWithOneWindowOfANewDesign)
{
    const ApplicationSession& session = start();
    ASSERT_EQ(session.windows().windowCount(), 1U);
    const MainWindow& window = firstWindow(session);
    EXPECT_TRUE(window.isVisible());
    EXPECT_EQ(window.windowTitle(), QStringLiteral("[*]Rocket"));
    EXPECT_FALSE(window.isWindowModified());
    ASSERT_NE(window.windowHandle(), nullptr);
    EXPECT_EQ(window.windowHandle()->title(), QStringLiteral("Rocket"));
    EXPECT_TRUE(window.document().isSaved());
    EXPECT_TRUE(session.windows().isReplaceable(window));
    EXPECT_EQ(&window.services().preferences(), &session.context().preferences());
    EXPECT_TRUE(prompts().prompts().empty());
}

TEST_F(ApplicationSessionTest, TheServicesAreReadyBeforeTheFirstWindow)
{
    // SwingStartup.runInEDT(): the default units, then the materials and the default component
    // materials, then the motors, all before a window opens.
    const QtRocket::Test::DefaultUnitsGuard units;
    {
        SettingsPreferences stored(preferencesFile());
        ASSERT_TRUE(unitGroup(UnitGroupId::LENGTH).setDefaultUnit("in"));
        stored.storeDefaultUnits();
        ASSERT_TRUE(stored.sync().has_value());
    }
    UnitGroup::resetDefaultUnits();

    std::unique_ptr<ApplicationContext> context = makeContext();
    EXPECT_EQ(unitGroup(UnitGroupId::LENGTH).getDefaultUnit().getUnit(), "in");
    EXPECT_FALSE(context->motors().getMotorSets().empty());
    EXPECT_TRUE(QApplication::topLevelWidgets().isEmpty());

    const ApplicationSession& session = start(std::move(context));
    EXPECT_EQ(session.windows().windowCount(), 1U);
    EXPECT_EQ(startMaterial<NoseCone>(session), "Polystyrene");
    EXPECT_EQ(startMaterial<TrapezoidFinSet>(session), "Balsa");
}

TEST_F(ApplicationSessionTest, AMissingFileOfTheCommandLineIsNotFoundAndANewDesignOpens)
{
    // SwingStartup.handleCommandLine() opened nothing: BasicFrame.reopen().
    ApplicationSession::Options startOptions = options();
    startOptions.files                       = {directory().resolve("designs") / "rocket.ork"};
    const ApplicationSession& session        = start(startOptions);

    ASSERT_EQ(prompts().prompts().size(), 1U);
    const Gui::Test::ScriptedUserPrompts::Prompt& message = prompts().prompts().front();
    EXPECT_EQ(message.kind, PromptKind::MESSAGE);
    EXPECT_EQ(message.title, QStringLiteral("Error opening file"));
    EXPECT_EQ(message.texts,
              (QStringList{QStringLiteral("ERROR"), QStringLiteral("File not found: rocket.ork")}));
    ASSERT_EQ(session.windows().windowCount(), 1U);
    EXPECT_TRUE(session.windows().isReplaceable(firstWindow(session)));
}

TEST_F(ApplicationSessionTest, TheFilesOfTheCommandLineOpenInWindowsOfTheirOwn)
{
    const std::filesystem::path first        = writeDesign("first.ork", "First");
    const std::filesystem::path second       = writeDesign("second.ork", "Second");
    ApplicationSession::Options startOptions = options();
    startOptions.files                       = {first, second};
    const ApplicationSession& session        = start(startOptions);
    EXPECT_TRUE(prompts().prompts().empty());
    ASSERT_EQ(session.windows().windowCount(), 2U);
    const MainWindow& window = firstWindow(session);
    EXPECT_EQ(window.document().getRocket().getName(), "First");
    EXPECT_EQ(window.document().getFile(), std::optional(first));
    EXPECT_TRUE(window.document().isSaved());
    EXPECT_FALSE(session.windows().isReplaceable(window));
    EXPECT_EQ(&session.fileOperations().findWindowOfFile(second)->document().getRocket(),
              &session.windows().windows().back()->document().getRocket());
}

TEST_F(ApplicationSessionTest, AGuardedSlotThatThrowsShowsTheInternalErrorAndTheApplicationGoesOn)
{
    const ApplicationSession& session = start();
    const QSignalSpy          exits(&session, &ApplicationSession::exitRequested);
    QObject                   sender;
    QObject::connect(&sender, &QObject::objectNameChanged, &sender,
                     guarded(&throwBug, QStringLiteral("Rename")));

    prompts().answerInternalError(UserPrompts::InternalErrorAnswer::CONTINUE);
    sender.setObjectName(QStringLiteral("renamed"));
    ASSERT_EQ(prompts().prompts().size(), 1U);
    const Gui::Test::ScriptedUserPrompts::Prompt& error = prompts().prompts().front();
    EXPECT_EQ(error.kind, PromptKind::INTERNAL_ERROR);
    ASSERT_EQ(error.texts.size(), 3);
    EXPECT_EQ(error.texts.at(0), QStringLiteral("BUG"));
    EXPECT_TRUE(error.texts.at(1).contains(QStringLiteral("the slot broke")));
    EXPECT_EQ(error.texts.at(2), QStringLiteral("Rename"));
    EXPECT_EQ(exits.count(), 0);
    EXPECT_EQ(session.windows().windowCount(), 1U);

    // Quit in the dialog ends the event loop at once, with the internal error's exit code.
    prompts().answerInternalError(UserPrompts::InternalErrorAnswer::QUIT);
    sender.setObjectName(QStringLiteral("again"));
    ASSERT_EQ(exits.count(), 1);
    EXPECT_EQ(exits.at(0).at(0).toInt(), ApplicationSession::kInternalErrorExitCode);
    EXPECT_EQ(session.windows().windowCount(), 1U);
}

/// Reports an error condition, from the thread it runs on.
void reportFromAWorker()
{
    Guard::reportErrorCondition(QStringLiteral("from a worker"), QStringLiteral("Worker"));
}

TEST_F(ApplicationSessionTest, AReportFromAnotherThreadIsShownOnTheGuiThread)
{
    const ApplicationSession& session = start();
    prompts().answerInternalError(UserPrompts::InternalErrorAnswer::CONTINUE);
    std::thread worker(&reportFromAWorker);
    worker.join();
    // Not asked on the worker: the GUI thread's event loop asks.
    EXPECT_TRUE(prompts().prompts().empty());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    ASSERT_EQ(prompts().prompts().size(), 1U);
    EXPECT_EQ(prompts().prompts().front().kind, PromptKind::INTERNAL_ERROR);
    EXPECT_EQ(prompts().prompts().front().texts,
              (QStringList{QStringLiteral("ERROR_CONDITION"), QStringLiteral("from a worker"),
                           QStringLiteral("Worker")}));
    EXPECT_EQ(session.windows().windowCount(), 1U);
}

TEST_F(ApplicationSessionTest, TheBackstopShowsTheInternalErrorToo)
{
    // On macOS an exception thrown in an event handler cannot unwind through Qt's own frames:
    // the program terminates before notify() can catch it (CI, Qt 6.8.3). Only the guarded
    // connections (Guard) protect there (Application.h).
#ifdef __APPLE__
    GTEST_SKIP() << "macOS: an exception cannot cross Qt's frames to reach notify()";
#endif
    const ApplicationSession& session = start();
    prompts().answerInternalError(UserPrompts::InternalErrorAnswer::CONTINUE);
    BrokenObject object;
    QEvent       event(QEvent::User);
    EXPECT_FALSE(QCoreApplication::sendEvent(&object, &event));
    ASSERT_EQ(prompts().prompts().size(), 1U);
    EXPECT_EQ(prompts().prompts().front().kind, PromptKind::INTERNAL_ERROR);
    EXPECT_EQ(prompts().prompts().front().texts.value(2),
              QStringLiteral("the User event of QObject"));
    EXPECT_EQ(session.windows().windowCount(), 1U);
}

TEST_F(ApplicationSessionTest, QuitAfterStartQuitsAsFileQuitDoesOnceTheEventLoopRuns)
{
    ApplicationSession::Options startOptions = options();
    startOptions.quitAfterStart              = true;
    const ApplicationSession& session        = start(startOptions);
    QSignalSpy                exits(&session, &ApplicationSession::exitRequested);
    EXPECT_EQ(session.windows().windowCount(), 1U);

    ASSERT_TRUE(exits.wait());
    EXPECT_EQ(exits.at(0).at(0).toInt(), 0);
    EXPECT_EQ(session.windows().windowCount(), 0U);
    EXPECT_TRUE(prompts().prompts().empty());
}

TEST_F(ApplicationSessionTest, ClosingTheLastWindowEndsTheApplication)
{
    const ApplicationSession& session = start();
    const QSignalSpy          exits(&session, &ApplicationSession::exitRequested);
    EXPECT_TRUE(session.windows().close(firstWindow(session)));
    ASSERT_EQ(exits.count(), 1);
    EXPECT_EQ(exits.at(0).at(0).toInt(), 0);
}

TEST_F(ApplicationSessionTest, AQuitEventClosesTheWindowsAsFileQuitDoes)
{
    const ApplicationSession& session = start();
    const QSignalSpy          exits(&session, &ApplicationSession::exitRequested);

    // A design not saved is asked about; Cancel keeps it and refuses the quit.
    firstWindow(session).document().getRocket().setName("Mars");
    prompts().answerSaveChanges(UserPrompts::SaveChangesAnswer::CANCEL);
    EXPECT_FALSE(sendQuit());
    EXPECT_EQ(session.windows().windowCount(), 1U);
    EXPECT_EQ(exits.count(), 0);

    prompts().answerSaveChanges(UserPrompts::SaveChangesAnswer::DISCARD);
    EXPECT_TRUE(sendQuit());
    EXPECT_EQ(session.windows().windowCount(), 0U);
    ASSERT_EQ(exits.count(), 1);
    EXPECT_EQ(exits.at(0).at(0).toInt(), 0);
}

TEST_F(ApplicationSessionTest, AFileTheSystemAsksToOpenTakesThePlaceOfTheNewDesign)
{
    // OSXSetup's open-file handler: BasicFrame.open(file, lastFrameInstance).
    const std::filesystem::path file    = writeDesign("rocket.ork", "Mars");
    const ApplicationSession&   session = start();
    QFileOpenEvent              event(QtStrings::fromPath(file));
    QCoreApplication::sendEvent(&application(), &event);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_TRUE(prompts().prompts().empty());
    ASSERT_EQ(session.windows().windowCount(), 1U);
    EXPECT_EQ(firstWindow(session).document().getRocket().getName(), "Mars");

    // One that cannot be opened is not found, and the window stays.
    QFileOpenEvent missing(QtStrings::fromPath(directory().resolve("missing.ork")));
    QCoreApplication::sendEvent(&application(), &missing);
    ASSERT_EQ(prompts().prompts().size(), 1U);
    EXPECT_EQ(prompts().prompts().front().texts.value(1),
              QStringLiteral("File not found: missing.ork"));
    EXPECT_EQ(session.windows().windowCount(), 1U);
}

TEST_F(ApplicationSessionTest, SaveInTheQuestionOfAClosingWindowSavesTheDesign)
{
    // The windows' save hook is the file operations' Save (BasicFrame.closeAction()).
    const ApplicationSession& session = start();
    session.context().preferences().setShowSaveRocketInfo(false);
    MainWindow& window = firstWindow(session);
    window.document().getRocket().setName("Mars");
    const std::filesystem::path file = directory().resolve("mars.ork");
    prompts().answerSaveChanges(UserPrompts::SaveChangesAnswer::SAVE);
    prompts().answerSaveFile(UserPrompts::SaveFileAnswer{
        .file = file, .directory = directory().path(), .saveSimulationData = std::nullopt});
    EXPECT_TRUE(session.windows().close(window));
    EXPECT_EQ(session.windows().windowCount(), 0U);
    EXPECT_TRUE(std::filesystem::exists(file));
}

TEST_F(ApplicationSessionTest, WithoutAWindowTheApplicationReopensANewDesign)
{
    // macOS: the application runs on without a window, and opens one when it is reopened.
    ApplicationSession& session = start(options(Behaviour::KEEP_RUNNING));
    const QSignalSpy    exits(&session, &ApplicationSession::exitRequested);
    EXPECT_TRUE(session.windows().close(firstWindow(session)));
    EXPECT_EQ(session.windows().windowCount(), 0U);
    EXPECT_EQ(exits.count(), 0);

    // Qt reports the Dock's reopen as the application becoming active.
    Q_EMIT application().applicationStateChanged(Qt::ApplicationActive);
    EXPECT_EQ(session.windows().windowCount(), 1U);
    session.reopenIfNoWindow();
    EXPECT_EQ(session.windows().windowCount(), 1U);
    EXPECT_TRUE(session.windows().isReplaceable(firstWindow(session)));
}

TEST_F(ApplicationSessionTest, AMissingMotorDatabaseIsShownOverTheFirstWindow)
{
    const DataPaths           nowhere(directory().resolve("no data"));
    const ApplicationSession& session = start(makeContext(nowhere));
    EXPECT_EQ(session.windows().windowCount(), 1U);
    ASSERT_EQ(prompts().prompts().size(), 1U);
    const Gui::Test::ScriptedUserPrompts::Prompt& message = prompts().prompts().front();
    EXPECT_EQ(message.kind, PromptKind::MESSAGE);
    EXPECT_EQ(message.title, QStringLiteral("Motor database"));
    // The severity, the line about the motors, then each problem (the motor directory cannot be
    // listed, no database found in it).
    ASSERT_EQ(message.texts.size(), 4);
    EXPECT_EQ(message.texts.at(0), QStringLiteral("WARNING"));
    EXPECT_EQ(message.texts.at(1),
              QStringLiteral("Not every motor of the motor database could be loaded: a design "
                             "that uses a missing motor opens without it."));
    EXPECT_TRUE(message.texts.at(3).startsWith(QtStrings::fromPath(nowhere.motorDirectory()) +
                                               QStringLiteral(": No SQLite motor database")));
}

TEST_F(ApplicationSessionTest, PutsTheHooksItReplacedBack)
{
    const Gui::Test::RecordingGuardHook before;
    start();
    prompts().answerInternalError(UserPrompts::InternalErrorAnswer::CONTINUE);
    Guard::reportErrorCondition(QStringLiteral("while running"));
    EXPECT_EQ(prompts().prompts().size(), 1U);
    EXPECT_TRUE(before.failures().empty());

    stop();
    Guard::reportErrorCondition(QStringLiteral("after"));
    ASSERT_EQ(before.failures().size(), 1U);
    EXPECT_EQ(before.failures().front().message, QStringLiteral("after"));
    EXPECT_FALSE(application().setQuitHook({}));
    EXPECT_FALSE(application().setOpenFilesHook({}));
}

}  // namespace

}  // namespace QtRocket::Gui
