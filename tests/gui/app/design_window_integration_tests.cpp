// The design windows of the running application (ApplicationSession, as main() makes it, with the
// test's questions) and what runs 1 to 3 left to connect: File > Properties and the examples
// asking for the rocket's configuration dialog, Save as with the "Save Design Info" dialog (the
// application's own, answered by NextModalDialog; the user's decisions R3-1 and R3-3), the edits
// of the configuration dialog committed before a save, and components dragged or pasted from one
// window into another with their decal images.

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QEvent>
#include <QLineEdit>
#include <QMimeData>
#include <QModelIndex>
#include <QObject>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPushButton>
#include <QRect>
#include <QString>
#include <QTest>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "ForwardingUserPrompts.h"
#include "GuiTestSupport.h"
#include "NextModalDialog.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/app/Application.h"
#include "QtRocket/gui/app/ApplicationContext.h"
#include "QtRocket/gui/app/ApplicationSession.h"
#include "QtRocket/gui/app/DataPaths.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/configdialog/ComponentConfigDialog.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/configdialog/RocketConfig.h"
#include "QtRocket/gui/dialogs/SaveDesignInfoDialog.h"
#include "QtRocket/gui/main/DesignPanel.h"
#include "QtRocket/gui/main/MainMenus.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/main/OpenRocketClipboard.h"
#include "QtRocket/gui/main/WidgetUserPrompts.h"
#include "QtRocket/gui/main/WindowManager.h"
#include "QtRocket/gui/settings/SettingsPreferences.h"
#include "QtRocket/gui/tree/ComponentTreeModel.h"
#include "QtRocket/gui/tree/ComponentTreeView.h"
#include "QtRocket/gui/tree/DocumentSelectionModel.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "ScriptedUserPrompts.h"
#include "TestTempDir.h"
#include "file/DesignFileEnvironment.h"
#include "main/FileMenusTestSupport.h"
#include "tree/ProbeDesign.h"
#include "tree/TreeEditingSupport.h"
#include "tree/TreeViewMouse.h"

namespace QtRocket::Gui
{

namespace
{

using Behaviour  = WindowManager::LastWindowBehaviour;
using PromptKind = Gui::Test::ScriptedUserPrompts::Kind;
using Gui::Test::ProbeDesign;

/// The questions of the running application: the test's script (ScriptedUserPrompts), but for the
/// "Save Design Info" dialog, which is the application's own (WidgetUserPrompts).
class DesignInformationPrompts final : public Gui::Test::ForwardingUserPrompts
{
public:
    explicit DesignInformationPrompts(UserPrompts& delegate) noexcept
      : ForwardingUserPrompts(delegate)
    {
    }

    void editDesignInformation(QWidget* parent, const GuiServices& services,
                               OpenRocketDocument& document) override
    {
        m_widgets.editDesignInformation(parent, services, document);
    }

private:
    WidgetUserPrompts m_widgets;
};

/// What the "Save Design Info" dialog showed, and how NextModalDialog answered it.
struct DesignInformationAnswer
{
    enum class Button
    {
        OK,
        CANCEL,
    };

    Button button        = Button::OK;
    bool   dontShowAgain = false;
    /// What it showed: the design name field, and whether the tree had the rocket selected.
    QString*                      designName     = nullptr;
    bool*                         rocketSelected = nullptr;
    const DocumentSelectionModel* selection      = nullptr;

    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<SaveDesignInfoDialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        *designName                           = dialog->panel().nameField().text();
        const RocketComponent* const selected = selection->selectedComponent();
        *rocketSelected = selected != nullptr && selected->kind() == ComponentKind::ROCKET;
        QTest::keyClicks(&dialog->panel().designerArea(), QStringLiteral("Ann"));
        if (dontShowAgain)
        {
            dialog->dontShowAgainBox().click();
        }
        if (button == Button::OK)
        {
            dialog->okButton().click();
        }
        else
        {
            dialog->cancelButton().click();
        }
    }
};

/// The open configuration dialog as "<title>: <main> [<others>]", or "none".
[[nodiscard]] std::string openDialog()
{
    const ComponentConfigDialog* const dialog = ComponentConfigDialog::dialog();
    if (dialog == nullptr || !dialog->isVisible())
    {
        return "none";
    }
    const ComponentSelection& selection = *dialog->configurator().selection();
    std::string               text =
        QtStrings::toUtf8(dialog->windowTitle()) + ": " + selection.getMain().getName() + " [";
    bool first = true;
    for (const RocketComponent* const other : selection.getOthers())
    {
        text += (first ? "" : ", ") + other->getName();
        first = false;
    }
    return text + "]";
}

/// The running application, started as main() starts it (with the platform's data directory of
/// the build tree, a preferences file of the test's own and the test's questions), and the
/// configuration dialog's state forgotten before and after (ConfigDialogTest's rule).
class DesignWindowIntegrationTest : public Gui::Test::GuiTest
{
protected:
    void SetUp() override
    {
        GuiTest::SetUp();
        ComponentConfigDialog::forgetPreviousDialogs();
        auto prompts = std::make_unique<DesignInformationPrompts>(m_scripted);
        auto context = std::make_unique<ApplicationContext>(
            std::make_unique<SettingsPreferences>(
                QtStrings::fromPath(m_directory.resolve("QtRocket.ini"))),
            DataPaths::forApplication(), std::move(prompts));
        ApplicationSession::Options options;
        options.lastWindowBehaviour    = Behaviour::EXIT;
        Application* const application = Application::current();
        QTROCKET_ASSERT(application != nullptr);
        m_session = std::make_unique<ApplicationSession>(*application, std::move(context), options);
    }

    void TearDown() override
    {
        // The windows, the documents and the services go first; the application's component
        // clipboard is left empty.
        m_session = nullptr;
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        OpenRocketClipboard::instance().clear();
        ComponentConfigDialog::forgetPreviousDialogs();
        GuiTest::TearDown();
    }

    [[nodiscard]] ApplicationSession&             session() const { return *m_session; }
    [[nodiscard]] Gui::Test::ScriptedUserPrompts& prompts() noexcept { return m_scripted; }
    [[nodiscard]] Preferences&                    preferences() const
    {
        return m_session->context().services().preferences();
    }
    [[nodiscard]] const QtRocket::Test::TempDir& directory() const noexcept { return m_directory; }

    /// The window the application started with (a New design).
    [[nodiscard]] MainWindow& firstWindow() const
    {
        QTROCKET_ASSERT(m_session->windows().windowCount() > 0);
        return *m_session->windows().windows().front();
    }

    /// A window opened on the probes' design (ProbeDesign, one stage).
    [[nodiscard]] MainWindow& openProbeDesign() const
    {
        return m_session->windows().addDocument(
            ProbeDesign::makeDocument(ProbeDesign::Booster::NONE));
    }

    /// The answer of the next Save As's file question: @p name in the test's directory.
    void answerSaveAs(const std::string& name)
    {
        m_scripted.answerSaveFile(UserPrompts::SaveFileAnswer{.file = m_directory.resolve(name),
                                                              .directory = m_directory.path(),
                                                              .saveSimulationData = std::nullopt});
    }

    /// The design saved as @p name in the test's directory, loaded again; null (with a test
    /// failure) when it cannot be.
    [[nodiscard]] std::unique_ptr<OpenRocketDocument> reload(const std::string& name) const
    {
        const QtRocket::Test::DesignFileEnvironment environment;
        Result<LoadedDocument>                      loaded =
            GeneralRocketLoader(environment.context()).load(m_directory.resolve(name));
        EXPECT_TRUE(loaded.has_value()) << (loaded.has_value() ? "" : loaded.error().message);
        return loaded.has_value() ? std::move(loaded->document) : nullptr;
    }

    [[nodiscard]] static DesignPanel& panelOf(const MainWindow& window)
    {
        DesignPanel* const panel = window.componentDesignPanel();
        QTROCKET_ASSERT(panel != nullptr);
        return *panel;
    }

private:
    QtRocket::Test::TempDir             m_directory;
    Gui::Test::ScriptedUserPrompts      m_scripted;
    std::unique_ptr<ApplicationSession> m_session;
};

// =========================================================================== the rocket's dialog

TEST_F(DesignWindowIntegrationTest, FilePropertiesOpensTheRocketsDialog)
{
    // BasicFrame: ComponentConfigDialog.showDialog(BasicFrame.this, document, rocket).
    const MainWindow& window = firstWindow();
    window.menus().propertiesAction()->trigger();
    EXPECT_EQ(openDialog(), "Rocket configuration: Rocket []");
    const ComponentConfigDialog* const dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    EXPECT_EQ(dialog->parentWidget(), &window);
    EXPECT_EQ(window.document().getUndoDetail().nextDescription,
              std::optional<std::string>("Modify Rocket"));
    EXPECT_TRUE(prompts().prompts().empty());
}

TEST_F(DesignWindowIntegrationTest, AnExampleOpensWithItsRocketsDialog)
{
    // ExampleDesignFileAction, BasicFrame.open(URL, parent): the window, then (invokeLater) the
    // rocket's dialog over it.
    const MainWindow& start   = firstWindow();
    QAction* const    example = Gui::Test::menuItem(*start.menus().openExampleMenu(),
                                                    QStringLiteral("A simple model rocket"));
    ASSERT_NE(example, nullptr);
    example->trigger();
    ASSERT_EQ(session().windows().windowCount(), 1U);  // the New design was replaced
    const MainWindow& window = firstWindow();
    EXPECT_NE(&window, &start);
    EXPECT_EQ(openDialog(), "none");
    QCoreApplication::processEvents();
    const std::string rocket = window.document().getRocket().getName();
    EXPECT_EQ(openDialog(), "Rocket configuration: " + rocket + " []");
    const ComponentConfigDialog* const dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    EXPECT_EQ(dialog->parentWidget(), &window);
    EXPECT_TRUE(prompts().prompts().empty());
}

// =========================================================================== Save as

TEST_F(DesignWindowIntegrationTest, SaveAsShowsTheDesignInformationThenTheFileChooser)
{
    // BasicFrame.saveAsAction(): showSaveRocketInfoDialog() with the rocket selected in the tree
    // and its name shown (the user's decision R3-1), then the file.
    const MainWindow& window = firstWindow();
    ASSERT_TRUE(preferences().isShowSaveRocketInfo());
    QString                          designName;
    bool                             rocketSelected = false;
    const Gui::Test::NextModalDialog answer{
        DesignInformationAnswer{.button         = DesignInformationAnswer::Button::OK,
                                .dontShowAgain  = false,
                                .designName     = &designName,
                                .rocketSelected = &rocketSelected,
                                .selection      = &panelOf(window).selection()}};
    answerSaveAs("info.ork");
    window.menus().saveAsAction()->trigger();
    EXPECT_TRUE(answer.acted());
    EXPECT_EQ(designName, QStringLiteral("Rocket"));
    EXPECT_TRUE(rocketSelected);
    ASSERT_EQ(prompts().prompts().size(), 1U);
    EXPECT_EQ(prompts().prompts().front().kind, PromptKind::SAVE_FILE);
    EXPECT_EQ(window.document().getFile(), std::optional(directory().resolve("info.ork")));
    const std::unique_ptr<OpenRocketDocument> saved = reload("info.ork");
    ASSERT_NE(saved, nullptr);
    EXPECT_EQ(saved->getRocket().getDesigner(), "Ann");
    EXPECT_TRUE(preferences().isShowSaveRocketInfo());
}

TEST_F(DesignWindowIntegrationTest, ACancelOfTheDesignInformationGoesOnToTheFileChooser)
{
    // The user's decision R3-3, OpenRocket's: Save as does not look at how the dialog closed, and
    // its Cancel undoes nothing.
    const MainWindow&                window = firstWindow();
    QString                          designName;
    bool                             rocketSelected = false;
    const Gui::Test::NextModalDialog answer{
        DesignInformationAnswer{.button         = DesignInformationAnswer::Button::CANCEL,
                                .dontShowAgain  = false,
                                .designName     = &designName,
                                .rocketSelected = &rocketSelected,
                                .selection      = &panelOf(window).selection()}};
    prompts().answerDiscardChanges({.discard = true, .doNotAskAgain = false});
    answerSaveAs("cancelled.ork");
    window.menus().saveAsAction()->trigger();
    EXPECT_TRUE(answer.acted());
    ASSERT_EQ(prompts().prompts().size(), 2U);
    EXPECT_EQ(prompts().prompts().at(0).kind, PromptKind::DISCARD_CHANGES);
    EXPECT_EQ(prompts().prompts().at(1).kind, PromptKind::SAVE_FILE);
    EXPECT_EQ(window.document().getRocket().getDesigner(), "Ann");
    const std::unique_ptr<OpenRocketDocument> saved = reload("cancelled.ork");
    ASSERT_NE(saved, nullptr);
    EXPECT_EQ(saved->getRocket().getDesigner(), "Ann");
}

TEST_F(DesignWindowIntegrationTest, DontShowThisDialogAgainLeavesItOutOfTheNextSaveAs)
{
    const MainWindow& window = firstWindow();
    QString           designName;
    bool              rocketSelected = false;
    {
        const Gui::Test::NextModalDialog answer{
            DesignInformationAnswer{.button         = DesignInformationAnswer::Button::OK,
                                    .dontShowAgain  = true,
                                    .designName     = &designName,
                                    .rocketSelected = &rocketSelected,
                                    .selection      = &panelOf(window).selection()}};
        answerSaveAs("first.ork");
        window.menus().saveAsAction()->trigger();
        EXPECT_TRUE(answer.acted());
    }
    EXPECT_FALSE(preferences().isShowSaveRocketInfo());
    // No dialog: the file only.
    answerSaveAs("second.ork");
    window.menus().saveAsAction()->trigger();
    ASSERT_EQ(prompts().prompts().size(), 2U);
    EXPECT_EQ(prompts().prompts().at(1).kind, PromptKind::SAVE_FILE);
    EXPECT_EQ(window.document().getFile(), std::optional(directory().resolve("second.ork")));
}

TEST_F(DesignWindowIntegrationTest, SaveAsShowsTheRocketInTheOpenDialog)
{
    // The rocket selected for the design information: the open configuration dialog follows the
    // selection (DesignPanel), as in OpenRocket.
    const MainWindow& window = firstWindow();
    window.menus().editAction()->trigger();
    EXPECT_EQ(openDialog(), "Stage configuration: Sustainer []");
    QString                          designName;
    bool                             rocketSelected = false;
    const Gui::Test::NextModalDialog answer{
        DesignInformationAnswer{.button         = DesignInformationAnswer::Button::OK,
                                .dontShowAgain  = false,
                                .designName     = &designName,
                                .rocketSelected = &rocketSelected,
                                .selection      = &panelOf(window).selection()}};
    answerSaveAs("open.ork");
    window.menus().saveAsAction()->trigger();
    EXPECT_TRUE(answer.acted());
    QCoreApplication::processEvents();
    // Opened before the designer was typed in the design information, which modified it.
    EXPECT_EQ(openDialog(), "*Rocket configuration: Rocket []");
    EXPECT_EQ(window.document().getRocket().getDesigner(), "Ann");
}

TEST_F(DesignWindowIntegrationTest, SaveCommitsTheEditStillBeingTypedInTheDialog)
{
    // A menu item (or the macOS menu bar, which takes no focus) leaves the dialog's edit pending:
    // it is committed before the save.
    preferences().setShowSaveRocketInfo(false);
    const MainWindow& window = firstWindow();
    window.menus().propertiesAction()->trigger();
    const ComponentConfigDialog* const dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    QLineEdit& name = dialog->configurator().nameField();
    name.selectAll();
    QTest::keyClicks(&name, QStringLiteral("Mars"));
    EXPECT_EQ(window.document().getRocket().getName(), "Rocket");
    answerSaveAs("mars.ork");
    window.menus().saveAction()->trigger();
    EXPECT_EQ(window.document().getRocket().getName(), "Mars");
    const std::unique_ptr<OpenRocketDocument> saved = reload("mars.ork");
    ASSERT_NE(saved, nullptr);
    EXPECT_EQ(saved->getRocket().getName(), "Mars");
    EXPECT_TRUE(window.document().isSaved());
    EXPECT_EQ(window.windowTitle(), QStringLiteral("[*]Mars (mars.ork)"));
}

// =========================================================================== between windows

TEST_F(DesignWindowIntegrationTest, AComponentDraggedIntoAnotherWindowIsCopiedWithItsDecal)
{
    // A drop from another document is a copy (ComponentTreeTransferHandler); its decal images
    // come along (DecalRegistry, decision of plan 9.4).
    const MainWindow& source = openProbeDesign();
    const MainWindow& target = openProbeDesign();
    Gui::Test::giveDecal(source.document(), ProbeDesign::find(source.document().getRocket(), "BT1"),
                         "decals/flame.png");
    ASSERT_TRUE(Gui::Test::decalListOf(target.document()).empty());

    const ComponentTreeModel&        sourceModel = panelOf(source).treeModel();
    const std::unique_ptr<QMimeData> data(sourceModel.mimeData(
        {sourceModel.indexOf(ProbeDesign::find(source.document().getRocket(), "BT1"))}));
    ASSERT_NE(data, nullptr);
    const ComponentTreeView& tree = panelOf(target).tree();
    const QModelIndex        stage =
        tree.componentModel().indexOf(target.document().getRocket().getChild(0));
    const QRect               row = tree.visualRect(stage);
    const Gui::Test::ViewDrop drop =
        Gui::Test::dropAt(*tree.viewport(), *data, row.center(), Qt::ShiftModifier);
    EXPECT_TRUE(drop.dropAccepted);
    EXPECT_EQ(drop.dropAction, Qt::CopyAction);

    EXPECT_EQ(ProbeDesign::plainTree(target.document().getRocket()),
              "Rocket[Sustainer[BT1[IT[EB], CR1, Fins], NC, BT1[IT[EB], CR1, Fins], BT2[Chute]]]");
    EXPECT_EQ(ProbeDesign::plainTree(source.document().getRocket()),
              "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]]]");
    EXPECT_EQ(Gui::Test::decalListOf(target.document()),
              std::vector<std::string>{"decals/flame.png"});
    EXPECT_TRUE(prompts().prompts().empty());
}

TEST_F(DesignWindowIntegrationTest, AComponentPastedIntoAnotherWindowBringsItsDecal)
{
    // The application's component clipboard (OpenRocketClipboard) serves every window.
    const MainWindow& source = openProbeDesign();
    const MainWindow& target = openProbeDesign();
    RocketComponent&  tube   = ProbeDesign::find(source.document().getRocket(), "BT2");
    Gui::Test::giveDecal(source.document(), tube, "decals/stripe.png");
    panelOf(source).selection().setSelectedComponent(tube);
    source.menus().copyAction()->trigger();
    EXPECT_FALSE(OpenRocketClipboard::instance().isEmpty());

    panelOf(target).selection().setSelectedComponent(
        ProbeDesign::find(target.document().getRocket(), "NC"));
    ASSERT_TRUE(target.menus().pasteAction()->isEnabled());
    target.menus().pasteAction()->trigger();
    EXPECT_EQ(ProbeDesign::plainTree(target.document().getRocket()),
              "Rocket[Sustainer[NC, BT2[Chute], BT1[IT[EB], CR1, Fins], BT2[Chute]]]");
    EXPECT_EQ(Gui::Test::decalListOf(target.document()),
              std::vector<std::string>{"decals/stripe.png"});
    EXPECT_TRUE(prompts().prompts().empty());
}

}  // namespace

}  // namespace QtRocket::Gui
