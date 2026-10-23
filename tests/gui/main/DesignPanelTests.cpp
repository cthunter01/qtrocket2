// The design panel in its window (DesignPanel, as WindowManager puts one into every window): the
// tree with the first stage selected, the Edit menu and the buttons acting on the tree's
// selection, the window's keys with the tree focused, the popup menu, the requests for the
// configuration dialog, and the arrangement of OpenRocket's DesignPanel (MigLayout's result: the
// M2 run 2 probe DesignPanelLayoutProbe, OpenRocket 5f164fd0e's MigLayout run headless).

#include "QtRocket/gui/main/DesignPanel.h"

#include <array>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QFont>
#include <QGroupBox>
#include <QList>
#include <QMenu>
#include <QMimeData>
#include <QObject>
#include <QPoint>
#include <QRect>
#include <QScrollArea>
#include <QSignalSpy>
#include <QSizePolicy>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QTest>
#include <QWidget>
#include <Qt>
#include <QtWidgets/qtestsupport_widgets.h>

#include <gtest/gtest.h>

#include "ExpectedQtMessage.h"
#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/gui/main/ComponentAddButtons.h"
#include "QtRocket/gui/main/ComponentButton.h"
#include "QtRocket/gui/main/MainMenus.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/main/OpenRocketClipboard.h"
#include "QtRocket/gui/main/RocketActions.h"
#include "QtRocket/gui/main/SplitPane.h"
#include "QtRocket/gui/main/UndoRedoActions.h"
#include "QtRocket/gui/main/WindowManager.h"
#include "QtRocket/gui/tree/ComponentTreeModel.h"
#include "QtRocket/gui/tree/ComponentTreeView.h"
#include "QtRocket/gui/tree/DocumentSelectionModel.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/IconButton.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "ScriptedUserPrompts.h"
#include "TestGuiServices.h"
#include "tree/ComponentTreeTranscript.h"
#include "tree/ProbeDesign.h"
#include "tree/TreeEditingSupport.h"
#include "tree/TreeViewMouse.h"

namespace QtRocket::Gui
{

namespace
{

using Lines               = std::vector<std::string>;
using LastWindowBehaviour = WindowManager::LastWindowBehaviour;
using Test::fillUpTo;
using Test::ProbeDesign;
using Booster = ProbeDesign::Booster;

/// The probes' design (ProbeDesign) with its single stage: Sustainer[NC, BT1[IT[EB], CR1, Fins],
/// BT2[Chute]].
constexpr std::string_view kDesign = "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]]]";

/// "[A, B]" of the components' names.
[[nodiscard]] std::string namesOf(const std::vector<RocketComponent*>& components)
{
    std::string text = "[";
    for (std::size_t i = 0; i < components.size(); ++i)
    {
        text += (i > 0 ? ", " : "") + components[i]->getName();
    }
    return text + "]";
}

/// The object names of those of @p actions that are enabled.
[[nodiscard]] Lines enabledNames(std::initializer_list<const QAction*> actions)
{
    Lines names;
    for (const QAction* action : actions)
    {
        if (action->isEnabled())
        {
            names.push_back(QtStrings::toUtf8(action->objectName()));
        }
    }
    return names;
}

/// @p widget's rectangle in the coordinates of its ancestor @p ancestor.
[[nodiscard]] QRect rectIn(const QWidget& widget, const QWidget& ancestor)
{
    return {widget.mapTo(&ancestor, QPoint(0, 0)), widget.size()};
}

/// The windows of a window manager over test services and a clipboard of the test's own, each
/// window's requests for the configuration dialog recorded: "dialog <name> [<others>]", then
/// " new" for a new component and " tab" when the previous tab is to be remembered; "close" for a
/// request to close it.
class DesignPanelTest : public Test::GuiTest
{
protected:
    void TearDown() override
    {
        // The windows, and the documents and panels they leave to deleteLater(), go first.
        m_manager = nullptr;
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        GuiTest::TearDown();
    }

    /// Opens a window on a new design of @p booster (ProbeDesign) and makes it the test's.
    void open(Booster booster = Booster::NONE)
    {
        use(manager().addDocument(ProbeDesign::makeDocument(booster)));
    }

    /// Opens a window on a new design, as File > New does, and makes it the test's.
    void openNew() { use(manager().newDesign()); }

    /// Makes @p window the test's and records its requests.
    void use(MainWindow& window)
    {
        m_window = &window;
        record(window);
    }

    /// Records the requests of @p window for the configuration dialog.
    void record(MainWindow& window)
    {
        QObject::connect(
            &window, &MainWindow::configDialogRequested, &window,
            [this](RocketComponent* component, const std::vector<RocketComponent*>& others,
                   bool isNewComponent, bool rememberPreviousTab) {
                m_requests.push_back("dialog " + component->getName() + " " + namesOf(others) +
                                     (isNewComponent ? " new" : "") +
                                     (rememberPreviousTab ? " tab" : ""));
            });
        QObject::connect(&window, &MainWindow::closeConfigDialogRequested, &window,
                         [this] { m_requests.emplace_back("close"); });
    }

    [[nodiscard]] WindowManager& manager()
    {
        if (m_manager == nullptr)
        {
            m_manager = std::make_unique<WindowManager>(m_services.services(), m_clipboard,
                                                        LastWindowBehaviour::EXIT);
        }
        return *m_manager;
    }

    [[nodiscard]] MainWindow& window() const
    {
        QTROCKET_ASSERT(m_window != nullptr);
        return *m_window;
    }

    /// The window's design panel.
    [[nodiscard]] DesignPanel& panel() const
    {
        DesignPanel* const panel = window().componentDesignPanel();
        QTROCKET_ASSERT(panel != nullptr);
        return *panel;
    }

    [[nodiscard]] MainMenus&  menus() const { return window().menus(); }
    [[nodiscard]] Rocket&     rocket() const { return window().document().getRocket(); }
    [[nodiscard]] QWidget&    viewport() const { return *panel().tree().viewport(); }
    [[nodiscard]] std::string tree() const { return ProbeDesign::plainTree(rocket()); }
    [[nodiscard]] Lines&      requests() noexcept { return m_requests; }
    [[nodiscard]] Gui::Test::ScriptedUserPrompts& prompts() { return m_services.prompts(); }
    [[nodiscard]] const OpenRocketClipboard&      clipboard() const { return m_clipboard; }

    [[nodiscard]] RocketComponent& find(std::string_view name) const
    {
        return ProbeDesign::find(rocket(), name);
    }

    /// Selects the components @p names, in this order.
    void select(std::initializer_list<std::string_view> names) const
    {
        std::vector<RocketComponent*> components;
        for (const std::string_view name : names)
        {
            components.push_back(&find(name));
        }
        panel().selection().setSelectedComponents(components);
    }

    /// The names of the selected components.
    [[nodiscard]] std::string selection() const
    {
        return namesOf(panel().selection().selectedComponents());
    }

    /// The tree as the probes print it (ComponentTreeTranscript).
    [[nodiscard]] std::string transcript(std::string_view label) const
    {
        return Gui::Test::treeTranscript(label, panel().tree());
    }

    /// The middle of the row of the component @p name, in the viewport.
    [[nodiscard]] QPoint center(std::string_view name) const
    {
        const ComponentTreeView& view = panel().tree();
        return view.visualRect(view.componentModel().indexOf(find(name))).center();
    }

    /// The undo description of the window's document; "-" for none.
    [[nodiscard]] std::string undoDescription() const
    {
        return window().document().getUndoDescription().value_or("-");
    }

    /// The enabled items of the Edit menu's component items (RocketActions').
    [[nodiscard]] Lines enabledEditItems() const
    {
        const MainMenus& m = menus();
        return enabledNames({m.editAction(), m.cutAction(), m.copyAction(), m.pasteAction(),
                             m.duplicateAction(), m.deleteAction(), m.deselectAllAction(),
                             m.toggleVisibilityAction(), m.showAllComponentsAction(),
                             panel().rocketActions().moveUpAction(),
                             panel().rocketActions().moveDownAction()});
    }

    /// Makes the window the active one; whether it is.
    [[nodiscard]] bool activate() const
    {
        window().activateWindow();
        return QTest::qWaitForWindowActive(&window());
    }

private:
    Gui::Test::TestGuiServices     m_services;
    OpenRocketClipboard            m_clipboard;
    std::unique_ptr<WindowManager> m_manager;
    MainWindow*                    m_window = nullptr;
    Lines                          m_requests;
};

// =========================================================================== the new window

TEST_F(DesignPanelTest, ANewWindowShowsTheTreeWithTheFirstStageSelected)
{
    openNew();
    EXPECT_EQ(window().designPanel(), &panel());
    ASSERT_EQ(window().tabs()->count(), 1);
    EXPECT_EQ(window().tabs()->widget(0), &panel());
    EXPECT_EQ(window().tabs()->tabText(0), QStringLiteral("Rocket design"));
    EXPECT_EQ(panel().objectName(), QStringLiteral("designPanel"));
    EXPECT_TRUE(panel().tree().isVisible());
    EXPECT_EQ(transcript("new"), "new: rows=2 {Rocket+, Sustainer+, } selected=[Sustainer]");

    // The Edit menu follows the selection: the only stage can be edited, copied and duplicated,
    // not cut, deleted or moved; the clipboard is empty.
    EXPECT_EQ(enabledEditItems(), (Lines{"editAction", "copyAction", "duplicateAction",
                                         "deselectAllAction", "toggleVisibilityAction"}));

    // The tree has the keyboard focus.
    ASSERT_TRUE(activate());
    EXPECT_EQ(QApplication::focusWidget(), &panel().tree());
    EXPECT_TRUE(requests().empty());
}

TEST_F(DesignPanelTest, ConnectsTheTreeItsSelectionAndTheActions)
{
    open();
    EXPECT_EQ(&panel().mainWindow(), &window());
    EXPECT_EQ(&panel().tree().componentModel(), &panel().treeModel());
    EXPECT_EQ(&panel().treeModel().document(), &window().document());
    EXPECT_EQ(&panel().selection().view(), &panel().tree());
    EXPECT_EQ(&panel().rocketActions().selection(), &panel().selection());
    EXPECT_EQ(panel().rocketActions().editAction(), menus().editAction());
    // The popup menu belongs to the tree.
    EXPECT_EQ(panel().tree().findChild<QMenu*>(QStringLiteral("componentTreePopupMenu"),
                                               Qt::FindDirectChildrenOnly),
              panel().rocketActions().contextMenu());
    EXPECT_EQ(selection(), "[Sustainer]");
}

// =========================================================================== the arrangement

TEST_F(DesignPanelTest, HoldsTheTreeWithItsButtonsBesideTheAddButtons)
{
    open();
    const SplitPane& split = *panel().splitter();
    EXPECT_EQ(split.objectName(), QStringLiteral("designSplitPane"));
    EXPECT_EQ(split.orientation(), Qt::Horizontal);
    EXPECT_DOUBLE_EQ(split.resizeWeight(), DesignPanel::kResizeWeight);
    EXPECT_DOUBLE_EQ(split.dividerLocation(), DesignPanel::kDividerLocation);
    EXPECT_FALSE(split.childrenCollapsible());
    ASSERT_EQ(split.count(), 2);
    EXPECT_TRUE(split.widget(0)->isAncestorOf(&panel().tree()));
    EXPECT_EQ(split.widget(1), panel().addButtonsBox());
    EXPECT_EQ(panel().tree().minimumWidth(), 140);
    EXPECT_EQ(panel().tree().objectName(), QStringLiteral("componentTree"));

    const std::array<IconButton*, DesignPanel::kTreeButtonCount>& buttons = panel().treeButtons();
    const RocketActions&                                          actions = panel().rocketActions();
    EXPECT_EQ(buttons[0]->action(), actions.moveUpAction());
    EXPECT_EQ(buttons[1]->action(), actions.moveDownAction());
    EXPECT_EQ(buttons[2]->action(), menus().editAction());
    EXPECT_EQ(buttons[3]->action(), menus().duplicateAction());
    EXPECT_EQ(buttons[4]->action(), menus().deleteAction());
    EXPECT_EQ(buttons[0]->text(), QStringLiteral("Move up"));
    EXPECT_EQ(buttons[1]->text(), QStringLiteral("Move down"));
    EXPECT_EQ(buttons[2]->text(), QStringLiteral("Edit"));
    EXPECT_EQ(buttons[3]->text(), QStringLiteral("Duplicate"));
    EXPECT_EQ(buttons[4]->text(), QStringLiteral("Delete"));
    EXPECT_EQ(buttons[0]->objectName(), QStringLiteral("moveUpButton"));
    EXPECT_EQ(buttons[4]->objectName(), QStringLiteral("deleteButton"));

    const QGroupBox& box = *panel().addButtonsBox();
    EXPECT_EQ(box.title(), QStringLiteral("Add new component"));
    EXPECT_TRUE(box.font().bold());
    EXPECT_EQ(box.sizePolicy().horizontalPolicy(), QSizePolicy::Ignored);
    const QScrollArea& scroll = *panel().addButtonsScrollArea();
    EXPECT_EQ(scroll.widget(), &panel().addButtons());
    EXPECT_FALSE(scroll.font().bold());
    EXPECT_FALSE(panel().addButtons().font().bold());
    EXPECT_TRUE(scroll.widgetResizable());
    EXPECT_EQ(scroll.horizontalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
    EXPECT_EQ(scroll.verticalScrollBarPolicy(), Qt::ScrollBarAsNeeded);
}

TEST_F(DesignPanelTest, PlacesItsDividerAtFortyPercentWhenShown)
{
    open();
    const SplitPane& split = *panel().splitter();
    EXPECT_TRUE(split.isDividerPlaced());
    const QList<int> sizes  = split.sizes();
    const int        length = split.width() - split.handleWidth();
    ASSERT_EQ(sizes.size(), 2);
    EXPECT_EQ(sizes.at(0), static_cast<int>(length * DesignPanel::kDividerLocation));
    EXPECT_EQ(sizes.at(0) + sizes.at(1), length);
}

/// The buttons beside the tree as MigLayout places them in DesignPanel (DesignPanelLayoutProbe):
/// five rows of one height, which the tree spans; Move up at 65 % of its row's free space, Move
/// down at the top of its row, the others in the middle; one width for all, right of the tree.
TEST_F(DesignPanelTest, ArrangesTheButtonsAsOpenRocketsLayoutDoes)
{
    open();
    // Tall enough for rows higher than a button.
    window().resize(1000, 1500);
    const QWidget& side = *panel().splitter()->widget(0);
    const std::array<IconButton*, DesignPanel::kTreeButtonCount>& buttons = panel().treeButtons();
    const QRect moveUp    = rectIn(*buttons[0], side);
    const QRect moveDown  = rectIn(*buttons[1], side);
    const QRect edit      = rectIn(*buttons[2], side);
    const QRect duplicate = rectIn(*buttons[3], side);
    const QRect remove    = rectIn(*buttons[4], side);
    const QRect tree      = rectIn(panel().tree(), side);

    // The rows' pitch (a row and the space below it) and height, from the last rows: Delete is
    // in the middle of the last row, which ends where the tree does.
    const double pitch = duplicate.center().y() - edit.center().y();
    const double row   = 2.0 * (tree.bottom() - remove.center().y());
    ASSERT_GT(row, 3.0 * moveUp.height());
    EXPECT_NEAR(remove.center().y() - duplicate.center().y(), pitch, 1.0);
    EXPECT_NEAR(edit.center().y() - tree.top(), (2.0 * pitch) + (row / 2.0), 2.0);
    EXPECT_NEAR(moveDown.top() - tree.top(), pitch, 2.0);
    EXPECT_NEAR(moveUp.top() - tree.top(), 0.65 * (row - moveUp.height()), 2.0);

    const std::set<int> widths{moveUp.width(), moveDown.width(), edit.width(), duplicate.width(),
                               remove.width()};
    EXPECT_EQ(widths.size(), 1U);
    const std::set<int> lefts{moveUp.left(), moveDown.left(), edit.left(), duplicate.left(),
                              remove.left()};
    EXPECT_EQ(lefts.size(), 1U);
    EXPECT_GT(moveUp.left(), tree.right());
}

// =========================================================================== editing

TEST_F(DesignPanelTest, EditMenuItemsActOnTheTreesSelection)
{
    open();
    select({"BT1"});
    menus().copyAction()->trigger();
    EXPECT_FALSE(clipboard().isEmpty());
    EXPECT_EQ(tree(), kDesign);

    // Paste after the nose cone, which takes no body tube; the copy is selected.
    select({"NC"});
    menus().pasteAction()->trigger();
    EXPECT_EQ(tree(),
              "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT1[IT[EB], CR1, Fins], BT2[Chute]]]");
    EXPECT_EQ(panel().selection().selectedComponent(), &rocket().getChild(0).getChild(1));
    EXPECT_EQ(undoDescription(), "Paste Body Tube");

    // Undo through the window's Undo: the copy's place goes to its parent.
    window().undoRedoActions().undoAction()->trigger();
    EXPECT_EQ(tree(), kDesign);
    EXPECT_EQ(selection(), "[Sustainer]");
    window().undoRedoActions().redoAction()->trigger();
    EXPECT_EQ(tree(),
              "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT1[IT[EB], CR1, Fins], BT2[Chute]]]");
    window().undoRedoActions().undoAction()->trigger();

    // Delete: the selection goes to the parent.
    select({"BT2"});
    menus().deleteAction()->trigger();
    EXPECT_EQ(tree(), "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins]]]");
    EXPECT_EQ(selection(), "[Sustainer]");
    EXPECT_EQ(undoDescription(), "Delete Body Tube");
    window().undoRedoActions().undoAction()->trigger();
    EXPECT_EQ(tree(), kDesign);

    // Duplicate: the copy goes to the end of its parent, selected, and onto the clipboard.
    select({"Chute"});
    menus().duplicateAction()->trigger();
    EXPECT_EQ(tree(), "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute, Chute]]]");
    EXPECT_EQ(panel().selection().selectedComponent(),
              &rocket().getChild(0).getChild(2).getChild(1));
    EXPECT_EQ(undoDescription(), "Duplicate Parachute");
    EXPECT_EQ(clipboard().components().size(), 1U);

    // Cut, then paste into the first body tube.
    select({"IT"});
    menus().cutAction()->trigger();
    EXPECT_EQ(tree(), "Rocket[Sustainer[NC, BT1[CR1, Fins], BT2[Chute, Chute]]]");
    select({"BT1"});
    menus().pasteAction()->trigger();
    EXPECT_EQ(tree(), "Rocket[Sustainer[NC, BT1[CR1, Fins, IT[EB]], BT2[Chute, Chute]]]");
    EXPECT_EQ(selection(), "[IT]");
    // No request for the dialog, but to close it before each change.
    EXPECT_EQ(requests(), (Lines{"close", "close", "close", "close", "close"}));
}

TEST_F(DesignPanelTest, ButtonsBesideTheTreeActOnTheSelection)
{
    open();
    select({"NC"});
    const std::array<IconButton*, DesignPanel::kTreeButtonCount>& buttons = panel().treeButtons();
    // The nose cone is first: it moves down only.
    EXPECT_FALSE(buttons[0]->isEnabled());
    EXPECT_TRUE(buttons[1]->isEnabled());
    buttons[1]->click();
    EXPECT_EQ(tree(), "Rocket[Sustainer[BT1[IT[EB], CR1, Fins], NC, BT2[Chute]]]");
    EXPECT_EQ(selection(), "[NC]");
    EXPECT_TRUE(buttons[0]->isEnabled());
    buttons[0]->click();
    EXPECT_EQ(tree(), kDesign);

    buttons[3]->click();
    EXPECT_EQ(tree(), "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute], NC]]");
    buttons[4]->click();
    EXPECT_EQ(tree(), kDesign);
    select({"BT1"});
    buttons[2]->click();
    EXPECT_EQ(requests(),
              (Lines{"close", "close", "close", "close", "close", "dialog BT1 [] tab"}));

    // The only stage cannot be deleted: its button is disabled with the menu's item.
    select({"Sustainer"});
    EXPECT_FALSE(buttons[4]->isEnabled());
    EXPECT_FALSE(menus().deleteAction()->isEnabled());
}

TEST_F(DesignPanelTest, WindowKeysWorkWhileTheTreeHasTheFocus)
{
    open();
    ASSERT_TRUE(activate());
    panel().tree().setFocus();
    ASSERT_EQ(QApplication::focusWidget(), &panel().tree());
    QWidget& view = panel().tree();

    select({"NC"});
    QTest::keySequence(&view, menus().copyAction()->shortcut());
    select({"BT2"});
    QTest::keySequence(&view, menus().pasteAction()->shortcut());
    EXPECT_EQ(tree(), "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute], NC]]");
    EXPECT_EQ(selection(), "[NC]");
    QTest::keySequence(&view, menus().duplicateAction()->shortcut());
    EXPECT_EQ(tree(), "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute], NC, NC]]");
    QTest::keySequence(&view, menus().deleteAction()->shortcut());
    EXPECT_EQ(tree(), "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute], NC]]");
    QTest::keySequence(&view, menus().undoAction()->shortcut());
    EXPECT_EQ(tree(), "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute], NC, NC]]");
    QTest::keySequence(&view, menus().redoAction()->shortcut());
    EXPECT_EQ(tree(), "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute], NC]]");
    select({"Chute"});
    QTest::keySequence(&view, menus().cutAction()->shortcut());
    EXPECT_EQ(tree(), "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2, NC]]");

    requests().clear();
    select({"BT1", "NC"});
    QTest::keySequence(&view, menus().editAction()->shortcut());
    EXPECT_EQ(requests(), (Lines{"close", "dialog BT1 [NC] tab"}));
    // The tree's own Select all.
    QTest::keyClick(&view, Qt::Key_A, Qt::ControlModifier);
    EXPECT_EQ(selection(), "[Sustainer, NC, BT1, IT, EB, CR1, Fins, BT2, NC]");
}

TEST_F(DesignPanelTest, WindowsShareTheClipboard)
{
    open();
    const MainWindow& first = window();
    select({"BT2"});
    menus().copyAction()->trigger();
    open();
    ASSERT_NE(&window(), &first);
    EXPECT_TRUE(menus().pasteAction()->isEnabled());
    select({"NC"});
    menus().pasteAction()->trigger();
    EXPECT_EQ(tree(), "Rocket[Sustainer[NC, BT2[Chute], BT1[IT[EB], CR1, Fins], BT2[Chute]]]");
}

// The tree's refusals reach the window's prompts (ComponentTreeModel::setUserPrompts()): a copy
// dropped into the tree beyond the component bound (decision O27), told once the drop is over.
TEST_F(DesignPanelTest, ARefusedDropIsToldThroughTheWindowsPrompts)
{
    open();
    fillUpTo(rocket(), find("BT2"), DocumentConfig::kMaxComponents - 4);
    const std::size_t                components = DocumentConfig::componentCount(rocket());
    const ComponentTreeModel&        model      = panel().treeModel();
    const std::unique_ptr<QMimeData> data(model.mimeData({model.indexOf(find("BT1"))}));
    ASSERT_NE(data, nullptr);

    // In the bottom margin of the nose cone's row: after it in the stage.
    const QRect               nose = panel().tree().visualRect(model.indexOf(find("NC")));
    const Gui::Test::ViewDrop drop = Gui::Test::dropAt(
        viewport(), *data, QPoint(nose.center().x(), nose.bottom() - 1), Qt::ControlModifier);

    EXPECT_TRUE(drop.moveAccepted);
    EXPECT_FALSE(drop.dropAccepted);
    EXPECT_EQ(DocumentConfig::componentCount(rocket()), components);
    EXPECT_TRUE(prompts().prompts().empty());
    QCoreApplication::processEvents();
    ASSERT_EQ(prompts().prompts().size(), 1U);
    EXPECT_EQ(prompts().prompts()[0].kind, Gui::Test::ScriptedUserPrompts::Kind::MESSAGE);
    EXPECT_EQ(prompts().prompts()[0].title, QStringLiteral("Copy component"));
    EXPECT_EQ(prompts().prompts()[0].texts,
              (QStringList{QStringLiteral("WARNING"),
                           QStringLiteral("The rocket would have 1001 components. QtRocket opens "
                                          "at most 1000 components of a design and would ignore "
                                          "the ones beyond when it opens this design again.")}));
}

// =========================================================================== the popup

TEST_F(DesignPanelTest, ARightClickShowsThePopupMenuForTheRow)
{
    open();
    select({"NC"});
    QMenu&           menu = *panel().rocketActions().contextMenu();
    const QSignalSpy shown(&menu, &QMenu::aboutToShow);
    {
        // The offscreen platform cannot raise the menu or grab the keyboard for it.
        const Gui::Test::ExpectedQtMessage noRaise(QStringLiteral("does not support raise"));
        const Gui::Test::ExpectedQtMessage noGrab(QStringLiteral("grabbing the keyboard"));
        Gui::Test::rightClickAt(viewport(), center("BT2"));
        EXPECT_EQ(shown.count(), 1);
        EXPECT_TRUE(menu.isVisible());
        menu.hide();
    }
    // The row was selected alone first (DesignPanel).
    EXPECT_EQ(selection(), "[BT2]");
}

// =========================================================================== the dialog

TEST_F(DesignPanelTest, ADoubleClickAsksForTheDialogOfTheRow)
{
    open();
    Gui::Test::doubleClickAt(viewport(), center("BT1"));
    EXPECT_EQ(requests(), (Lines{"dialog BT1 [] tab"}));
    EXPECT_EQ(selection(), "[BT1]");

    // With Ctrl, the row joins the selection and the dialog is for all of it.
    requests().clear();
    Gui::Test::doubleClickAt(viewport(), center("NC"), Qt::ControlModifier);
    EXPECT_EQ(requests(), (Lines{"dialog NC [BT1] tab"}));
    EXPECT_EQ(selection(), "[BT1, NC]");
}

TEST_F(DesignPanelTest, EditAsksForTheDialogOfTheFirstSelectedWithTheOthers)
{
    open();
    select({"BT2", "NC", "IT"});
    menus().editAction()->trigger();
    EXPECT_EQ(requests(), (Lines{"close", "dialog BT2 [NC, IT] tab"}));
}

TEST_F(DesignPanelTest, AnAddButtonAsksForTheNewComponentsDialog)
{
    open();
    select({"Sustainer"});
    ComponentButton* const stage = panel().addButtons().button(ComponentKind::AXIAL_STAGE);
    ASSERT_NE(stage, nullptr);
    stage->click();
    EXPECT_EQ(tree(), "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Stage]");
    EXPECT_EQ(selection(), "[Stage]");
    // ComponentButton: a new component's dialog, without the remembered tab.
    EXPECT_EQ(requests(), (Lines{"dialog Stage [] new"}));
}

TEST_F(DesignPanelTest, TheDialogFollowsTheSelectionWhileItIsVisible)
{
    open();
    bool visible = false;
    panel().setConfigDialogVisible([&visible] { return visible; });
    select({"BT1"});
    QCoreApplication::processEvents();
    EXPECT_TRUE(requests().empty());

    // Visible: the dialog opens again for the new selection, once the event loop runs.
    visible = true;
    select({"NC", "BT2"});
    select({"BT2", "NC"});
    EXPECT_TRUE(requests().empty());
    QCoreApplication::processEvents();
    EXPECT_EQ(requests(), (Lines{"close", "dialog BT2 [NC] tab"}));

    // Nothing selected: the dialog stays as it is.
    requests().clear();
    panel().selection().clearComponentSelection();
    QCoreApplication::processEvents();
    EXPECT_TRUE(requests().empty());

    // Closed in between: nothing.
    select({"Chute"});
    visible = false;
    QCoreApplication::processEvents();
    EXPECT_TRUE(requests().empty());
}

TEST_F(DesignPanelTest, TheDialogFollowsTheSelectionAsItIsWhenTheEventLoopRuns)
{
    open();
    window().document().addUndoPosition("Rename");
    rocket().setName("Renamed");
    bool visible = true;
    panel().setConfigDialogVisible([&visible] { return visible; });
    select({"BT1", "Chute"});
    // An undo before the event loop runs replaces every component (Rocket::loadFrom()); the
    // selection is restored by id. The dialog is asked for with the components as they are then.
    window().undoRedoActions().undoAction()->trigger();
    EXPECT_EQ(rocket().getName(), "Rocket");
    EXPECT_EQ(selection(), "[BT1, Chute]");
    QCoreApplication::processEvents();
    EXPECT_EQ(requests(), (Lines{"close", "dialog BT1 [Chute] tab"}));
}

TEST_F(DesignPanelTest, ADoubleClickDoesNotEditWhileTheDialogIsVisible)
{
    open();
    bool visible = true;
    panel().setConfigDialogVisible([&visible] { return visible; });
    Gui::Test::doubleClickAt(viewport(), center("BT1"));
    // The click selected the row: the dialog follows the selection.
    QCoreApplication::processEvents();
    EXPECT_EQ(requests(), (Lines{"close", "dialog BT1 [] tab"}));

    visible = false;
    requests().clear();
    Gui::Test::doubleClickAt(viewport(), center("BT2"));
    EXPECT_EQ(requests(), (Lines{"dialog BT2 [] tab"}));
    panel().setConfigDialogVisible({});
    requests().clear();
    Gui::Test::doubleClickAt(viewport(), center("NC"));
    QCoreApplication::processEvents();
    EXPECT_EQ(requests(), (Lines{"dialog NC [] tab"}));
}

TEST_F(DesignPanelTest, ANewComponentsDialogIsNotReopenedForItsSelection)
{
    open();
    bool visible = true;
    panel().setConfigDialogVisible([&visible] { return visible; });
    select({"BT1"});
    ComponentButton* const mass = panel().addButtons().button(ComponentKind::MASS_COMPONENT);
    ASSERT_NE(mass, nullptr);
    mass->click();
    QCoreApplication::processEvents();
    // The add button's request holds; the selection's change asks for nothing more.
    ASSERT_EQ(requests().size(), 1U);
    EXPECT_EQ(requests().front().substr(requests().front().size() - 4), " new");
    EXPECT_EQ(selection(), "[" + rocket().getChild(0).getChild(1).getChild(3).getName() + "]");
}

// =========================================================================== the figure

TEST_F(DesignPanelTest, HighlightsTheFirstStageAloneUntilTheTreeIsClicked)
{
    open();
    EXPECT_TRUE(panel().isStartHighlight());
    EXPECT_EQ(namesOf(panel().highlightedComponents()), "[Sustainer]");
    const QSignalSpy changed(&panel(), &DesignPanel::highlightChanged);
    // A click on the selected stage: no change of the selection, but of the highlight.
    QTest::mouseClick(&viewport(), Qt::LeftButton, Qt::NoModifier, center("Sustainer"));
    EXPECT_EQ(selection(), "[Sustainer]");
    EXPECT_FALSE(panel().isStartHighlight());
    EXPECT_EQ(changed.count(), 1);
    EXPECT_EQ(namesOf(panel().highlightedComponents()),
              "[Sustainer, NC, BT1, IT, EB, CR1, Fins, BT2, Chute]");
    QTest::mouseClick(&viewport(), Qt::LeftButton, Qt::NoModifier, center("Sustainer"));
    EXPECT_EQ(changed.count(), 1);
    select({"BT1"});
    EXPECT_EQ(changed.count(), 2);
    EXPECT_EQ(namesOf(panel().highlightedComponents()), "[BT1]");
}

TEST_F(DesignPanelTest, HighlightsTheFirstStageAloneUntilTheSelectionChanges)
{
    open();
    const QSignalSpy changed(&panel(), &DesignPanel::highlightChanged);
    select({"Sustainer", "NC"});
    EXPECT_FALSE(panel().isStartHighlight());
    EXPECT_EQ(changed.count(), 1);
    EXPECT_EQ(namesOf(panel().highlightedComponents()),
              "[Sustainer, NC, BT1, IT, EB, CR1, Fins, BT2, Chute]");
}

// =========================================================================== closing

TEST_F(DesignPanelTest, ClosingTheWindowClosesTheDialog)
{
    open();
    // BasicFrame.closeAction(): before the question and after the window closed.
    rocket().setName("Changed");
    prompts().answerSaveChanges(UserPrompts::SaveChangesAnswer::DISCARD);
    EXPECT_TRUE(manager().close(window()));
    EXPECT_EQ(requests(), (Lines{"close", "close"}));

    // A saved design: no question; before the window looks whether its design is saved (closing
    // the dialog commits an edit still being typed, which then counts: WindowManager) and after
    // the window closed.
    requests().clear();
    open();
    window().document().setSaved(true);
    EXPECT_TRUE(manager().close(window()));
    EXPECT_EQ(requests(), (Lines{"close", "close"}));
}

}  // namespace

}  // namespace QtRocket::Gui
