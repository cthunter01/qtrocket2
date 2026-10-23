// The windows' requests for the configuration dialog answered, as the application has them
// (ConfigDialogRequests over a WindowManager): in real windows, the dialog of what a double-click
// in the tree or the figure, Edit or an add button asks for; the dialog following the selection
// with the remembered tab; closed before a change of the design and when its window closes; its
// edits still being typed committed before an undo or a redo; the dialog of another window; and
// nothing answered without it.

#include "QtRocket/gui/main/ConfigDialogRequests.h"

#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QString>
#include <Qt>
#include <QtCore/qtestsupport_core.h>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/configdialog/ComponentConfigDialog.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/figure/RocketFigure.h"
#include "QtRocket/gui/figure/RocketPanel.h"
#include "QtRocket/gui/main/ComponentAddButtons.h"
#include "QtRocket/gui/main/ComponentButton.h"
#include "QtRocket/gui/main/DesignPanel.h"
#include "QtRocket/gui/main/MainMenus.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/main/OpenRocketClipboard.h"
#include "QtRocket/gui/main/UndoRedoActions.h"
#include "QtRocket/gui/main/WindowManager.h"
#include "QtRocket/gui/tree/ComponentTreeModel.h"
#include "QtRocket/gui/tree/ComponentTreeView.h"
#include "QtRocket/gui/tree/DocumentSelectionModel.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "ScriptedUserPrompts.h"
#include "configdialog/ConfigDialogTestSupport.h"
#include "figure/FigureMouse.h"
#include "tree/ProbeDesign.h"
#include "tree/TreeViewMouse.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using LastWindowBehaviour = WindowManager::LastWindowBehaviour;
using Gui::Test::ProbeDesign;

/// The open dialog as "<title>: <main> [<others>] <tab>", or "none" when no dialog is visible.
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
    return text + "] " + QtStrings::toUtf8(dialog->selectedTabName());
}

/// The windows of a window manager whose requests for the configuration dialog are answered
/// (ConfigDialogRequests), over services with OpenRocket's built-in materials and a clipboard of
/// the test's own; what the closed dialogs leave for the next one forgotten (ConfigDialogTest).
class ConfigDialogRequestsTest : public Gui::Test::ConfigDialogTest
{
protected:
    void SetUp() override
    {
        ConfigDialogTest::SetUp();
        m_manager  = std::make_unique<WindowManager>(m_services.services(), m_clipboard,
                                                     LastWindowBehaviour::EXIT);
        m_requests = std::make_unique<ConfigDialogRequests>(*m_manager);
    }

    void TearDown() override
    {
        // The dialog goes with its window.
        m_requests = nullptr;
        m_manager  = nullptr;
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        ConfigDialogTest::TearDown();
    }

    /// Opens a window on the probes' design with one stage (ProbeDesign).
    MainWindow& open()
    {
        return m_manager->addDocument(ProbeDesign::makeDocument(ProbeDesign::Booster::NONE));
    }

    [[nodiscard]] WindowManager& manager() const noexcept { return *m_manager; }
    void                         destroyRequests() noexcept { m_requests = nullptr; }
    void makeRequests() { m_requests = std::make_unique<ConfigDialogRequests>(*m_manager); }
    [[nodiscard]] Gui::Test::ScriptedUserPrompts& prompts() noexcept
    {
        return m_services.prompts();
    }

    [[nodiscard]] static DesignPanel& panelOf(const MainWindow& window)
    {
        DesignPanel* const panel = window.componentDesignPanel();
        QTROCKET_ASSERT(panel != nullptr);
        return *panel;
    }

    [[nodiscard]] static RocketComponent& find(const MainWindow& window, std::string_view name)
    {
        return ProbeDesign::find(window.document().getRocket(), name);
    }

    /// Selects the components @p names of @p window, in this order.
    static void select(const MainWindow& window, std::initializer_list<std::string_view> names)
    {
        std::vector<RocketComponent*> components;
        for (const std::string_view name : names)
        {
            components.push_back(&find(window, name));
        }
        panelOf(window).selection().setSelectedComponents(components);
    }

    /// A double-click on the row of @p name in @p window's tree, with @p modifiers.
    static void doubleClickRow(const MainWindow& window, std::string_view name,
                               Qt::KeyboardModifiers modifiers = {})
    {
        const ComponentTreeView& tree = panelOf(window).tree();
        const QPoint             center =
            tree.visualRect(tree.componentModel().indexOf(find(window, name))).center();
        Gui::Test::doubleClickAt(*tree.viewport(), center, modifiers);
    }

    /// The open dialog made the active window, for typing into it; whether it is.
    [[nodiscard]] static bool activateDialog()
    {
        ComponentConfigDialog* const dialog = ComponentConfigDialog::dialog();
        if (dialog == nullptr)
        {
            return false;
        }
        dialog->activateWindow();
        return QTest::qWaitFor([dialog] { return QApplication::activeWindow() == dialog; });
    }

    /// Types @p text into the spin box named @p name of the open dialog, an edit still pending.
    static void typeIntoDialog(const QString& name, const QString& text)
    {
        const ComponentConfigDialog* const dialog = ComponentConfigDialog::dialog();
        ASSERT_NE(dialog, nullptr);
        auto* const spinBox = dialog->findChild<UnitSpinBox*>(name);
        ASSERT_NE(spinBox, nullptr) << QtStrings::toUtf8(name);
        Gui::Test::typeInto(*spinBox, text);
    }

private:
    Gui::Test::ConfigDialogServices       m_services;
    OpenRocketClipboard                   m_clipboard;
    std::unique_ptr<WindowManager>        m_manager;
    std::unique_ptr<ConfigDialogRequests> m_requests;
};

/// The undo description the document's next change would get (the dialog's undo position).
[[nodiscard]] std::optional<std::string> nextUndo(const MainWindow& window)
{
    return window.document().getUndoDetail().nextDescription;
}

// =========================================================================== the requests

TEST_F(ConfigDialogRequestsTest, ADoubleClickInTheTreeOpensTheDialogOfItsComponent)
{
    // DesignPanel: ComponentConfigDialog.showDialog(parent, document, component).
    const MainWindow& window = open();
    doubleClickRow(window, "BT1");
    EXPECT_EQ(openDialog(), "Body Tube configuration: BT1 [] General");
    const ComponentConfigDialog* const dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    EXPECT_EQ(dialog->parentWidget(), &window);
    EXPECT_FALSE(dialog->isModal());
    EXPECT_FALSE(dialog->isNewComponent());
    EXPECT_EQ(nextUndo(window), std::optional<std::string>("Modify Body Tube"));
}

TEST_F(ConfigDialogRequestsTest, AShiftOrCtrlDoubleClickOpensTheDialogOfTheSelection)
{
    // The double-clicked component with the other selected ones (multi-component edit).
    const MainWindow& window = open();
    select(window, {"BT2"});
    doubleClickRow(window, "BT1", Qt::ControlModifier);
    EXPECT_EQ(openDialog(), "(Multi-component) Body Tube configuration: BT1 [BT2] General");
    EXPECT_EQ(nextUndo(window), std::optional<std::string>("Modify components"));

    ComponentConfigDialog::disposeDialog();
    select(window, {"NC"});
    doubleClickRow(window, "BT1", Qt::ShiftModifier);
    EXPECT_EQ(openDialog(), "Multi-component configuration: BT1 [NC] Override");
}

TEST_F(ConfigDialogRequestsTest, EditOpensTheDialogOfTheFirstSelectedWithTheOthers)
{
    const MainWindow& window = open();
    select(window, {"BT2", "BT1"});
    window.menus().editAction()->trigger();
    EXPECT_EQ(openDialog(), "(Multi-component) Body Tube configuration: BT2 [BT1] General");
}

TEST_F(ConfigDialogRequestsTest, TheFiguresDoubleClickOpensTheDialog)
{
    // RocketPanel.handleDoubleComponentClick(): a first click selects, a double-click on the
    // selected component opens its dialog.
    const MainWindow& window = open();
    // The window's layouts and its figure settle.
    constexpr int kPasses = 4;
    for (int pass = 0; pass < kPasses; ++pass)
    {
        QCoreApplication::processEvents();
    }
    const RocketPanel* const panel = window.componentRocketPanel();
    ASSERT_NE(panel, nullptr);
    RocketFigure&               figure = panel->figure();
    const std::optional<QPoint> pixel  = Gui::Test::figurePixelOf(figure, find(window, "BT2"));
    ASSERT_TRUE(pixel.has_value());
    const QPoint at = pixel.value_or(QPoint());
    Gui::Test::clickAt(figure, at);
    EXPECT_EQ(openDialog(), "none");
    Gui::Test::secondClickAt(figure, at);
    EXPECT_EQ(openDialog(), "Body Tube configuration: BT2 [] General");
    const ComponentConfigDialog* const dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    EXPECT_EQ(dialog->parentWidget(), &window);
}

TEST_F(ConfigDialogRequestsTest, AnAddButtonOpensTheNewComponentsDialogOnItsFirstTab)
{
    // ComponentButton: showDialog(parentFrame, document, component, false, true): a new component,
    // the tab of the previous dialog not remembered, no undo position of its own.
    const MainWindow& window = open();
    doubleClickRow(window, "BT1");
    const ComponentConfigDialog* const bodyDialog = ComponentConfigDialog::dialog();
    ASSERT_NE(bodyDialog, nullptr);
    bodyDialog->configurator().setSelectedTab(QStringLiteral("Motor"));
    EXPECT_EQ(openDialog(), "Body Tube configuration: BT1 [] Motor");

    ComponentButton* const mass =
        panelOf(window).addButtons().button(ComponentKind::MASS_COMPONENT);
    ASSERT_NE(mass, nullptr);
    mass->click();
    QCoreApplication::processEvents();
    const ComponentConfigDialog* const dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    EXPECT_NE(dialog, bodyDialog);
    EXPECT_TRUE(dialog->isVisible());
    EXPECT_TRUE(dialog->isNewComponent());
    EXPECT_EQ(dialog->configurator().selectedTabIndex(), 0);
    const RocketComponent* const added = panelOf(window).selection().selectedComponent();
    ASSERT_NE(added, nullptr);
    EXPECT_EQ(added->kind(), ComponentKind::MASS_COMPONENT);
    EXPECT_EQ(&dialog->configurator().selection()->getMain(), added);
    EXPECT_EQ(window.document().getUndoDescription(),
              std::optional<std::string>("Add Mass Component"));
}

// =========================================================================== the selection

TEST_F(ConfigDialogRequestsTest, TheDialogFollowsTheSelectionWithTheRememberedTab)
{
    // DesignPanel's selection listener: disposeDialog(), then showDialog() for the first selected
    // component with the others, remembering the tab; one turn of the event loop later.
    const MainWindow& window = open();
    doubleClickRow(window, "BT1");
    ComponentConfigDialog* const firstDialog = ComponentConfigDialog::dialog();
    ASSERT_NE(firstDialog, nullptr);
    firstDialog->configurator().setSelectedTab(QStringLiteral("Motor"));
    const QPointer<ComponentConfigDialog> first(firstDialog);

    select(window, {"BT2"});
    EXPECT_EQ(openDialog(), "Body Tube configuration: BT1 [] Motor");
    QCoreApplication::processEvents();
    EXPECT_EQ(openDialog(), "Body Tube configuration: BT2 [] Motor");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_TRUE(first.isNull());

    select(window, {"BT1", "BT2"});
    QCoreApplication::processEvents();
    EXPECT_EQ(openDialog(), "(Multi-component) Body Tube configuration: BT1 [BT2] Motor");

    // A nose cone has no Motor tab: its first.
    select(window, {"NC"});
    QCoreApplication::processEvents();
    EXPECT_EQ(openDialog(), "Nose Cone configuration: NC [] General");
    EXPECT_EQ(nextUndo(window), std::optional<std::string>("Modify Nose Cone"));

    // Nothing selected: the dialog stays.
    panelOf(window).selection().clearComponentSelection();
    QCoreApplication::processEvents();
    EXPECT_EQ(openDialog(), "Nose Cone configuration: NC [] General");
}

TEST_F(ConfigDialogRequestsTest, WhileTheDialogIsVisibleADoubleClickSelectsOnly)
{
    // DesignPanel: no edit while ComponentConfigDialog.isDialogVisible(); the selection's change
    // opens the dialog again for the row.
    const MainWindow& window = open();
    doubleClickRow(window, "BT1");
    ASSERT_TRUE(ComponentConfigDialog::isDialogVisible());
    doubleClickRow(window, "NC");
    EXPECT_EQ(openDialog(), "Body Tube configuration: BT1 [] General");
    QCoreApplication::processEvents();
    EXPECT_EQ(openDialog(), "Nose Cone configuration: NC [] General");
}

TEST_F(ConfigDialogRequestsTest, TheDialogFollowsTheSelectionOfAnotherWindow)
{
    // The dialog is the application's: another window's selection opens it there.
    const MainWindow& first  = open();
    const MainWindow& second = open();
    doubleClickRow(first, "BT1");
    const ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    EXPECT_EQ(dialog->parentWidget(), &first);

    select(second, {"BT2"});
    QCoreApplication::processEvents();
    EXPECT_EQ(openDialog(), "Body Tube configuration: BT2 [] General");
    dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    EXPECT_EQ(dialog->parentWidget(), &second);
    EXPECT_EQ(&dialog->configurator().selection()->getMain(), &find(second, "BT2"));
}

// =========================================================================== changes and undo

TEST_F(ConfigDialogRequestsTest, AChangeOfTheDesignClosesTheDialogKeepingItsEdit)
{
    // RocketActions: ComponentConfigDialog.disposeDialog() before the duplicate; the edit still
    // being typed is committed first.
    const MainWindow& window = open();
    doubleClickRow(window, "BT1");
    ASSERT_TRUE(activateDialog());
    typeIntoDialog(QStringLiteral("Length"), QStringLiteral("5"));
    EXPECT_NE(find(window, "BT1").getLength(), 0.05);
    window.menus().duplicateAction()->trigger();
    EXPECT_EQ(openDialog(), "none");
    QCoreApplication::processEvents();
    EXPECT_EQ(openDialog(), "none");
    EXPECT_DOUBLE_EQ(find(window, "BT1").getLength(), 0.05);
    const RocketComponent* const copy = panelOf(window).selection().selectedComponent();
    ASSERT_NE(copy, nullptr);
    EXPECT_NE(copy, &find(window, "BT1"));
    EXPECT_DOUBLE_EQ(copy->getLength(), 0.05);
}

TEST_F(ConfigDialogRequestsTest, AnUndoCommitsTheEditBeingTypedThenClosesTheDialog)
{
    // The undo replaces every component (Rocket::loadFrom()): the dialog closes in its event and
    // touches none of them afterwards (AddressSanitizer). The edit typed in it was committed
    // first, so the undo takes it back with the dialog's step, and a redo brings it again.
    const MainWindow& window = open();
    const double      length = find(window, "BT1").getLength();
    doubleClickRow(window, "BT1");
    ASSERT_TRUE(activateDialog());
    typeIntoDialog(QStringLiteral("Length"), QStringLiteral("5"));
    const QPointer<ComponentConfigDialog> dialog(ComponentConfigDialog::dialog());
    window.undoRedoActions().undoAction()->trigger();
    EXPECT_EQ(openDialog(), "none");
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_TRUE(dialog.isNull());
    EXPECT_EQ(find(window, "BT1").getLength(), length);
    EXPECT_EQ(window.document().getRedoDescription(),
              std::optional<std::string>("Modify Body Tube"));

    window.undoRedoActions().redoAction()->trigger();
    EXPECT_DOUBLE_EQ(find(window, "BT1").getLength(), 0.05);
}

TEST_F(ConfigDialogRequestsTest, ARedoWhoseHistoryTheTypedEditRemovesDoesNothing)
{
    // A change removes the redo history: the edit committed before the redo stays, and the redo
    // is not there any more (no undo error).
    const MainWindow& window = open();
    window.document().addUndoPosition("Rename");
    window.document().getRocket().setName("Mars");
    window.undoRedoActions().undoAction()->trigger();
    ASSERT_TRUE(window.document().isRedoAvailable());
    doubleClickRow(window, "BT1");
    ASSERT_TRUE(window.document().isRedoAvailable());
    ASSERT_TRUE(activateDialog());
    typeIntoDialog(QStringLiteral("Length"), QStringLiteral("5"));
    window.undoRedoActions().redoAction()->trigger();
    QCoreApplication::processEvents();
    EXPECT_DOUBLE_EQ(find(window, "BT1").getLength(), 0.05);
    EXPECT_EQ(window.document().getRocket().getName(), "Rocket");
    EXPECT_FALSE(window.document().isRedoAvailable());
    EXPECT_FALSE(window.undoRedoActions().redoAction()->isEnabled());
    EXPECT_EQ(openDialog(), "*Body Tube configuration: BT1 [] General");
}

TEST_F(ConfigDialogRequestsTest, ClosingTheWindowClosesTheDialogFirst)
{
    // BasicFrame.closeAction(): ComponentConfigDialog.disposeDialog() before the question whether
    // to save; the edit still being typed is committed.
    MainWindow& window = open();
    window.document().setSaved(false);
    doubleClickRow(window, "BT1");
    ASSERT_TRUE(activateDialog());
    typeIntoDialog(QStringLiteral("Length"), QStringLiteral("5"));
    prompts().answerSaveChanges(UserPrompts::SaveChangesAnswer::CANCEL);
    EXPECT_FALSE(manager().close(window));
    EXPECT_EQ(openDialog(), "none");
    EXPECT_DOUBLE_EQ(find(window, "BT1").getLength(), 0.05);
}

TEST_F(ConfigDialogRequestsTest, AnEditBeingTypedInASavedDesignIsAskedAboutOnClosing)
{
    // The dialog is closed, committing the edit, before the window looks whether its design is
    // saved: the edit makes it unsaved, so the question comes (OpenRocket closes a saved design
    // and loses the edit; the edit is committed as before a save or an undo).
    MainWindow& window = open();
    window.document().setSaved(true);
    doubleClickRow(window, "BT1");
    ASSERT_TRUE(activateDialog());
    typeIntoDialog(QStringLiteral("Length"), QStringLiteral("5"));
    ASSERT_TRUE(window.document().isSaved());
    prompts().answerSaveChanges(UserPrompts::SaveChangesAnswer::CANCEL);
    EXPECT_FALSE(manager().close(window));
    ASSERT_EQ(prompts().prompts().size(), 1U);
    EXPECT_EQ(prompts().prompts().front().kind, Gui::Test::ScriptedUserPrompts::Kind::SAVE_CHANGES);
    EXPECT_EQ(manager().windowCount(), 1U);
    EXPECT_EQ(openDialog(), "none");
    EXPECT_DOUBLE_EQ(find(window, "BT1").getLength(), 0.05);
    EXPECT_FALSE(window.document().isSaved());
}

TEST_F(ConfigDialogRequestsTest, ASavedDesignWithoutAnEditClosesWithoutAQuestion)
{
    MainWindow& window = open();
    window.document().setSaved(true);
    doubleClickRow(window, "BT1");
    ASSERT_TRUE(ComponentConfigDialog::isDialogVisible());
    ASSERT_TRUE(window.document().isSaved());
    EXPECT_TRUE(manager().close(window));
    EXPECT_TRUE(prompts().prompts().empty());
    EXPECT_EQ(manager().windowCount(), 0U);
    EXPECT_EQ(openDialog(), "none");
}

// =========================================================================== without it

TEST_F(ConfigDialogRequestsTest, ItAnswersTheWindowsOpenWhenItIsMade)
{
    // Those open when it is made (the class comment), as those opened later.
    destroyRequests();
    const MainWindow& window = open();
    makeRequests();
    doubleClickRow(window, "BT1");
    EXPECT_EQ(openDialog(), "Body Tube configuration: BT1 [] General");
    // The design panel learnt whether the dialog is visible: the dialog follows the selection.
    select(window, {"NC"});
    QCoreApplication::processEvents();
    EXPECT_EQ(openDialog(), "Nose Cone configuration: NC [] General");
}

TEST_F(ConfigDialogRequestsTest, WithoutItTheRequestsGoUnanswered)
{
    const MainWindow& window = open();
    destroyRequests();
    doubleClickRow(window, "BT1");
    EXPECT_EQ(openDialog(), "none");
    window.menus().editAction()->trigger();
    EXPECT_EQ(openDialog(), "none");
}

}  // namespace

}  // namespace QtRocket::Gui
