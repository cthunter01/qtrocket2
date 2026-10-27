// FlightConfigurationPanel: the "Motors & Configuration" tab, its buttons and sub-tabs, their
// enable rules (the sequence of the scouting's TabProbe over a new design, tab-new.txt), the
// sub-tab switch, the tables following the rocket and the design, the popup of column 0, and the
// operations New, Duplicate, Rename and Delete with the selection they leave (TabProbe's
// tab-simple.txt). The operations themselves are FlightConfigurationEditing's (its tests and the
// operation pins).

#include "QtRocket/gui/flightconfig/FlightConfigurationPanel.h"

#include <memory>
#include <optional>
#include <string>

#include <QAction>
#include <QCoreApplication>
#include <QMenu>
#include <QObject>
#include <QString>
#include <QTabWidget>
#include <QTableView>
#include <QTest>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/flightconfig/FlightConfigurablePanel.h"
#include "QtRocket/gui/flightconfig/FlightConfigurableTableModel.h"
#include "QtRocket/gui/flightconfig/MotorConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/RecoveryConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/SeparationConfigurationPanel.h"
#include "QtRocket/gui/tree/DocumentSelectionModel.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/IconButton.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "ScriptedUserPrompts.h"
#include "flightconfig/FlightConfigurationTabSupport.h"

namespace QtRocket::Gui
{

namespace
{

using FlightConfigurationPanelTest = Gui::Test::GuiTest;
using Tab                          = Gui::Test::FlightConfigurationTab;
using Design                       = Tab::Design;

/// The text, the enabled state and the object name of @p action, "New Configuration (newConfig
/// Action)" with " disabled" when it is.
[[nodiscard]] std::string describe(const QAction& action)
{
    return QtStrings::toUtf8(action.text()) + (action.isEnabled() ? "" : " (disabled)");
}

/// The four configuration actions of @p panel, enabled or not.
[[nodiscard]] std::string configurationActions(const FlightConfigurationPanel& panel)
{
    return describe(*panel.newConfigurationAction()) + ", " +
           describe(*panel.renameConfigurationAction()) + ", " +
           describe(*panel.deleteConfigurationAction()) + ", " +
           describe(*panel.duplicateConfigurationAction());
}

/// The sub-tabs of @p panel as TabProbe prints them: "sel=0 [Motors] [Recovery (disabled)] ...".
[[nodiscard]] std::string subTabs(const FlightConfigurationPanel& panel)
{
    const QTabWidget& tabs = *panel.tabs();
    std::string       text = "sel=" + std::to_string(tabs.currentIndex());
    for (int i = 0; i < tabs.count(); ++i)
    {
        text += " [" + QtStrings::toUtf8(tabs.tabText(i)) +
                (tabs.isTabEnabled(i) ? "" : " (disabled)") + "]";
    }
    return text;
}

/// Makes the three mounts of the two stages' design stop acting as motor mounts, in one event.
void stopActingAsMotorMounts(const Tab& tab)
{
    tab.rocket().freeze();
    for (const char* name : {"Body", "Motor tube", "Booster tube"})
    {
        dynamic_cast<MotorMount&>(tab.find(name)).setMotorMount(false);
    }
    tab.rocket().thaw();
}

// =========================================================================== the widgets

TEST_F(FlightConfigurationPanelTest, ItHasJavasFourButtonsTheirActionsAndThreeSubTabs)
{
    const Tab                       tab(Design::TWO_STAGES);
    const FlightConfigurationPanel& panel = tab.panel();
    EXPECT_EQ(panel.objectName(), QStringLiteral("flightConfigurationPanel"));
    EXPECT_EQ(configurationActions(panel),
              "New Configuration, Rename Configuration, Delete Configuration, Duplicate "
              "Configuration");
    EXPECT_EQ(panel.newConfigurationButton()->objectName(), QStringLiteral("newConfButton"));
    EXPECT_EQ(panel.renameConfigurationButton()->objectName(), QStringLiteral("renameConfButton"));
    EXPECT_EQ(panel.deleteConfigurationButton()->objectName(), QStringLiteral("removeConfButton"));
    EXPECT_EQ(panel.duplicateConfigurationButton()->objectName(),
              QStringLiteral("duplicateConfButton"));
    EXPECT_EQ(panel.newConfigurationButton()->action(), panel.newConfigurationAction());
    EXPECT_EQ(panel.renameConfigurationButton()->action(), panel.renameConfigurationAction());
    EXPECT_EQ(panel.deleteConfigurationButton()->action(), panel.deleteConfigurationAction());
    EXPECT_EQ(panel.duplicateConfigurationButton()->action(), panel.duplicateConfigurationAction());
    EXPECT_FALSE(panel.newConfigurationAction()->icon().isNull());
    EXPECT_FALSE(panel.renameConfigurationAction()->icon().isNull());
    // New's icon is Java's large icon, which a menu item does not show.
    EXPECT_FALSE(panel.newConfigurationAction()->isIconVisibleInMenu());
    EXPECT_TRUE(panel.renameConfigurationAction()->isIconVisibleInMenu());
    EXPECT_EQ(Tab::items(panel.configurationPopupMenu()),
              "newConfigAction, renameConfigAction, deleteConfigAction, duplicateConfigAction");

    EXPECT_EQ(subTabs(panel), "sel=0 [Motors] [Recovery] [Stages]");
    EXPECT_EQ(panel.tabs()->widget(0), &tab.motors());
    EXPECT_EQ(panel.tabs()->widget(1), &tab.recovery());
    EXPECT_EQ(panel.tabs()->widget(2), &tab.stages());
    EXPECT_EQ(&panel.currentPanel(), &tab.motors());
    EXPECT_EQ(tab.motors().objectName(), QStringLiteral("motorConfigurationPanel"));
    EXPECT_EQ(tab.recovery().objectName(), QStringLiteral("recoveryConfigurationPanel"));
    EXPECT_EQ(tab.stages().objectName(), QStringLiteral("separationConfigurationPanel"));
}

TEST_F(FlightConfigurationPanelTest, TheTablesStartOnTheSelectedConfigurationInTheirLastColumn)
{
    const Tab tab(Design::TWO_STAGES);
    // Java's doTableInitialization(): the rocket's selected configuration in the last column.
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[0] cols=[3]");
    EXPECT_EQ(Tab::lines(tab.recovery()), "rows=[0] cols=[1]");
    EXPECT_EQ(Tab::lines(tab.stages()), "rows=[0] cols=[1]");
    // Before any listener: the design's selection stays.
    EXPECT_EQ(tab.designSelectionNames(), "[]");
    // The Motors panel refreshes its buttons at its making; the other two start disabled, as
    // Java's (TabProbe's tab-simple.txt, "initial").
    EXPECT_TRUE(tab.motors().selectMotorAction()->isEnabled());
    EXPECT_FALSE(tab.recovery().selectDeploymentAction()->isEnabled());
    EXPECT_FALSE(tab.stages().selectSeparationAction()->isEnabled());
    EXPECT_EQ(configurationActions(tab.panel()),
              "New Configuration, Rename Configuration, Delete Configuration, Duplicate "
              "Configuration");
}

// ======================================================================= the enable rules

TEST_F(FlightConfigurationPanelTest, TheEnableRulesFollowANewDesignAsInOpenRocket)
{
    Tab tab(Design::NEW);
    tab.show();
    const FlightConfigurationPanel& panel = tab.panel();
    // TabProbe's tab-new.txt, step by step.
    EXPECT_EQ(subTabs(panel), "sel=0 [Motors] [Recovery (disabled)] [Stages (disabled)]");
    EXPECT_EQ(configurationActions(panel),
              "New Configuration (disabled), Rename Configuration (disabled), Delete "
              "Configuration (disabled), Duplicate Configuration (disabled)");
    EXPECT_TRUE(tab.motors().isShowingHelp());

    // "nose and body added"
    RocketComponent& stage = tab.rocket().getChild(0);
    static_cast<void>(stage.addChild(std::make_unique<NoseCone>()));
    auto& body = stage.addChild(std::make_unique<BodyTube>());
    EXPECT_EQ(subTabs(panel), "sel=0 [Motors] [Recovery (disabled)] [Stages (disabled)]");
    EXPECT_FALSE(panel.newConfigurationAction()->isEnabled());
    EXPECT_TRUE(tab.motors().isShowingHelp());

    // "body is a mount"
    body.setMotorMount(true);
    EXPECT_TRUE(panel.newConfigurationAction()->isEnabled());
    EXPECT_FALSE(panel.renameConfigurationAction()->isEnabled());
    EXPECT_FALSE(tab.motors().isShowingHelp());
    EXPECT_EQ(subTabs(panel), "sel=0 [Motors] [Recovery (disabled)] [Stages (disabled)]");

    // "parachute added"
    static_cast<void>(body.addChild(std::make_unique<Parachute>()));
    EXPECT_EQ(subTabs(panel), "sel=0 [Motors] [Recovery] [Stages (disabled)]");

    // "new configuration D12-5" (here Estes C6-5)
    tab.prompts().answerMotor(UserPrompts::MotorChoice{.motor = tab.estes("C6"), .delay = 5});
    panel.newConfigurationAction()->trigger();
    EXPECT_EQ(configurationActions(panel),
              "New Configuration, Rename Configuration, Delete Configuration, Duplicate "
              "Configuration");
    EXPECT_EQ(tab.selectedRow(), 0);
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[0] cols=[1]");
    EXPECT_EQ(Tab::lines(tab.recovery()), "rows=[0] cols=[1]");
    EXPECT_EQ(Tab::lines(tab.stages()), "rows=[0] cols=[0]");
    EXPECT_TRUE(tab.motors().selectMotorAction()->isEnabled());
    EXPECT_FALSE(tab.stages().selectSeparationAction()->isEnabled());

    // "second stage added"
    auto& second = tab.rocket().addChild(std::make_unique<AxialStage>());
    second.setName("Stage");
    static_cast<void>(second.addChild(std::make_unique<BodyTube>()));
    EXPECT_EQ(subTabs(panel), "sel=0 [Motors] [Recovery] [Stages]");
    EXPECT_EQ(Tab::lines(tab.stages()), "rows=[0] cols=[1]");
    EXPECT_TRUE(tab.stages().selectSeparationAction()->isEnabled());

    // "stages tab": the tree selects the stage.
    panel.tabs()->setCurrentIndex(FlightConfigurationPanel::kSeparationTabIndex);
    EXPECT_EQ(tab.designSelectionNames(), "[Stage]");

    // "second stage removed": the Stages tab is disabled and the Motors tab current.
    static_cast<void>(tab.rocket().removeChild(&second));
    EXPECT_EQ(subTabs(panel), "sel=0 [Motors] [Recovery] [Stages (disabled)]");
    EXPECT_EQ(Tab::lines(tab.stages()), "rows=[0] cols=[0]");
    QCoreApplication::processEvents();
}

TEST_F(FlightConfigurationPanelTest, ADisabledCurrentSubTabMovesToTheMotorsTab)
{
    const Tab                       tab(Design::TWO_STAGES);
    const FlightConfigurationPanel& panel = tab.panel();
    // The last recovery device goes from the Recovery tab (Java moves too).
    panel.tabs()->setCurrentIndex(FlightConfigurationPanel::kRecoveryTabIndex);
    RocketComponent& chute = tab.find("Chute");
    static_cast<void>(chute.getParent()->removeChild(&chute));
    EXPECT_EQ(subTabs(panel), "sel=0 [Motors] [Recovery (disabled)] [Stages]");

    // The Stages tab disabled for want of an acting motor mount (Java leaves it current: U-M3-6
    // f).
    panel.tabs()->setCurrentIndex(FlightConfigurationPanel::kSeparationTabIndex);
    ASSERT_EQ(panel.tabs()->currentIndex(), FlightConfigurationPanel::kSeparationTabIndex);
    stopActingAsMotorMounts(tab);
    EXPECT_EQ(subTabs(panel), "sel=0 [Motors] [Recovery (disabled)] [Stages (disabled)]");
    EXPECT_FALSE(panel.newConfigurationAction()->isEnabled());
    EXPECT_TRUE(tab.motors().isShowingHelp());
    QCoreApplication::processEvents();
}

// ========================================================================= the selection

TEST_F(FlightConfigurationPanelTest, ARowSelectsTheConfigurationAndTheOtherTablesFollow)
{
    const Tab tab(Design::TWO_STAGES);
    tab.show();
    Tab::click(tab.motors(), 2, 1);
    EXPECT_EQ(tab.selectedRow(), 2);
    // Every table selects the rocket's configuration in its selected column.
    EXPECT_EQ(Tab::lines(tab.recovery()), "rows=[2] cols=[1]");
    EXPECT_EQ(Tab::lines(tab.stages()), "rows=[2] cols=[1]");
    // The column changed while a row was selected: the design selects its mount.
    EXPECT_EQ(tab.designSelectionNames(), "[Body]");

    // Another column of the row.
    Tab::click(tab.motors(), 2, 2);
    EXPECT_EQ(tab.designSelectionNames(), "[Motor tube]");
    EXPECT_EQ(tab.selectedRow(), 2);

    // Several rows select no configuration; the design selects the columns' mounts.
    Tab::click(tab.motors(), 0, 3, Qt::ControlModifier);
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[0, 2] cols=[2, 3]");
    EXPECT_EQ(tab.selectedRow(), 2);
    EXPECT_EQ(tab.designSelectionNames(), "[Motor tube, Booster tube]");
}

TEST_F(FlightConfigurationPanelTest, TheRocketsSelectedConfigurationSelectsItsRowInEveryTable)
{
    const Tab tab(Design::TWO_STAGES);
    tab.rocket().setSelectedConfiguration(tab.id(1));
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[1] cols=[3]");
    EXPECT_EQ(Tab::lines(tab.recovery()), "rows=[1] cols=[1]");
    EXPECT_EQ(Tab::lines(tab.stages()), "rows=[1] cols=[1]");
    // The tables' columns did not change: the design keeps its selection.
    EXPECT_EQ(tab.designSelectionNames(), "[]");

    // The default configuration: nothing selected, nothing to rename, delete or duplicate.
    tab.rocket().setSelectedConfiguration(FlightConfigurationId::defaultValueId());
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[] cols=[]");
    EXPECT_EQ(Tab::lines(tab.recovery()), "rows=[] cols=[]");
    EXPECT_EQ(Tab::lines(tab.stages()), "rows=[] cols=[]");
    EXPECT_EQ(configurationActions(tab.panel()),
              "New Configuration, Rename Configuration (disabled), Delete Configuration "
              "(disabled), Duplicate Configuration (disabled)");
    EXPECT_FALSE(tab.motors().selectMotorAction()->isEnabled());

    // Back: the last column of each table.
    tab.rocket().setSelectedConfiguration(tab.id(2));
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[2] cols=[3]");
    EXPECT_EQ(Tab::lines(tab.stages()), "rows=[2] cols=[1]");
}

TEST_F(FlightConfigurationPanelTest, ASubTabSwitchSelectsThePreviousRowsAndTheDesignFollows)
{
    const Tab tab(Design::TWO_STAGES);
    tab.show();
    QTabWidget& tabs = *tab.panel().tabs();
    Tab::click(tab.motors(), 1, 1);
    Tab::click(tab.motors(), 2, 1, Qt::ShiftModifier);
    ASSERT_EQ(Tab::lines(tab.motors()), "rows=[1, 2] cols=[1]");
    EXPECT_EQ(tab.selectedRow(), 1);

    // Java's synchronizePanelSelection(): the rows in column 1; then the buttons, the focus and
    // the design's selection.
    tabs.setCurrentIndex(FlightConfigurationPanel::kRecoveryTabIndex);
    EXPECT_EQ(Tab::lines(tab.recovery()), "rows=[1, 2] cols=[1]");
    EXPECT_TRUE(tab.recovery().selectDeploymentAction()->isEnabled());
    EXPECT_EQ(tab.designSelectionNames(), "[Chute]");
    EXPECT_EQ(&tab.panel().currentPanel(), &tab.recovery());

    tabs.setCurrentIndex(FlightConfigurationPanel::kSeparationTabIndex);
    EXPECT_EQ(Tab::lines(tab.stages()), "rows=[1, 2] cols=[1]");
    EXPECT_TRUE(tab.stages().selectSeparationAction()->isEnabled());
    EXPECT_EQ(tab.designSelectionNames(), "[Booster]");

    // Nothing selected: the next sub-tab selects nothing either.
    tab.stages().clearSelection();
    tabs.setCurrentIndex(FlightConfigurationPanel::kMotorTabIndex);
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[] cols=[]");
    EXPECT_FALSE(tab.motors().selectMotorAction()->isEnabled());
    EXPECT_EQ(tab.designSelectionNames(), "[Booster]");

    // The tab's spotlight (the window chose the tab): the design selects the selected columns.
    Tab::click(tab.motors(), 0, 2);
    tab.designSelection().clearComponentSelection();
    tab.panel().takeTheSpotlight();
    EXPECT_EQ(tab.designSelectionNames(), "[Motor tube]");
}

// ======================================================================== the operations

TEST_F(FlightConfigurationPanelTest, NewAsksForTheMotorOfTheNewRowInTheSelectedColumn)
{
    Tab tab(Design::TWO_STAGES);
    tab.show();
    Tab::click(tab.motors(), 1, 2);
    tab.prompts().answerMotor(UserPrompts::MotorChoice{.motor = tab.estes("A8"), .delay = 3});
    tab.panel().newConfigurationAction()->trigger();

    ASSERT_EQ(tab.rocket().getConfigurationCount(), 4);
    EXPECT_EQ(tab.selectedRow(), 3);
    ASSERT_EQ(tab.prompts().motorQuestions().size(), 1U);
    EXPECT_EQ(tab.prompts().motorQuestions().front().mountName, QStringLiteral("Motor tube"));
    EXPECT_EQ(tab.prompts().motorQuestions().front().configurationId, tab.id(3));
    // Java's restoreSelection(): the first cell, in each table.
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[3] cols=[2]");
    EXPECT_EQ(Tab::lines(tab.recovery()), "rows=[3] cols=[1]");
    EXPECT_EQ(Tab::lines(tab.stages()), "rows=[3] cols=[1]");
    EXPECT_EQ(tab.document().getUndoDescription().value_or("-"), "Select motor");
    EXPECT_EQ(tab.motors().tableModel().data(tab.motors().tableModel().index(3, 2)).toString(),
              QStringLiteral("A8-3"));
}

TEST_F(FlightConfigurationPanelTest, NewOnTheRecoveryTabAsksForTheDeployment)
{
    Tab tab(Design::TWO_STAGES);
    tab.panel().tabs()->setCurrentIndex(FlightConfigurationPanel::kRecoveryTabIndex);
    tab.prompts().answerDeployment(std::nullopt);
    tab.panel().newConfigurationAction()->trigger();
    ASSERT_EQ(tab.prompts().deploymentRequests().size(), 1U);
    EXPECT_TRUE(tab.prompts().motorQuestions().empty());
    EXPECT_EQ(tab.selectedRow(), 3);
    EXPECT_EQ(Tab::lines(tab.recovery()), "rows=[3] cols=[1]");
    EXPECT_EQ(tab.document().getUndoDescription().value_or("-"), "Select deployment");
}

TEST_F(FlightConfigurationPanelTest, DuplicateRenameAndDeleteActOnTheCurrentSubTabsRows)
{
    Tab tab(Design::TWO_STAGES);
    tab.show();
    const FlightConfigurationPanel& panel = tab.panel();
    Tab::click(tab.motors(), 1, 1);
    Tab::click(tab.motors(), 2, 1, Qt::ShiftModifier);

    // Duplicate: the copies, the first selected (TabProbe's "duplicate").
    panel.duplicateConfigurationAction()->trigger();
    ASSERT_EQ(tab.rocket().getConfigurationCount(), 5);
    EXPECT_EQ(tab.selectedRow(), 3);
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[3] cols=[1]");
    EXPECT_EQ(Tab::lines(tab.recovery()), "rows=[3] cols=[1]");

    // Rename: the rows of the current sub-tab.
    Tab::click(tab.motors(), 4, 1, Qt::ShiftModifier);
    tab.prompts().answerRenameConfiguration(
        {.action = UserPrompts::RenameAction::RENAME, .name = QStringLiteral("Copy")});
    panel.renameConfigurationAction()->trigger();
    EXPECT_EQ(tab.rocket().getFlightConfiguration(tab.id(3)).getNameRaw(
                  tab.panel().services().preferences()),
              "Copy");
    EXPECT_EQ(tab.rocket().getFlightConfiguration(tab.id(4)).getNameRaw(
                  tab.panel().services().preferences()),
              "Copy");
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[3] cols=[1]");

    // Delete: the selected configuration goes, the rocket selects the default one, nothing is
    // selected and Rename, Delete and Duplicate are disabled (TabProbe's "delete
    // configuration").
    Tab::click(tab.motors(), 4, 1, Qt::ShiftModifier);
    panel.deleteConfigurationAction()->trigger();
    ASSERT_EQ(tab.rocket().getConfigurationCount(), 3);
    EXPECT_EQ(tab.selectedRow(), -1);
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[] cols=[]");
    EXPECT_EQ(Tab::lines(tab.recovery()), "rows=[] cols=[]");
    EXPECT_EQ(Tab::lines(tab.stages()), "rows=[] cols=[]");
    EXPECT_EQ(configurationActions(panel),
              "New Configuration, Rename Configuration (disabled), Delete Configuration "
              "(disabled), Duplicate Configuration (disabled)");
    EXPECT_EQ(tab.document().getUndoDescription().value_or("-"), "Remove configuration(s)");

    // Its undo brings the configurations back, the first selected again in each table's last
    // column (TabProbe's "undo delete").
    tab.document().undo();
    ASSERT_EQ(tab.rocket().getConfigurationCount(), 5);
    EXPECT_EQ(tab.selectedRow(), 3);
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[3] cols=[3]");
    EXPECT_EQ(Tab::lines(tab.stages()), "rows=[3] cols=[1]");
    EXPECT_TRUE(panel.renameConfigurationAction()->isEnabled());
}

TEST_F(FlightConfigurationPanelTest, AnUndoWhileACellIsSelectedKeepsTheTablesOnTheNewComponents)
{
    const Tab tab(Design::TWO_STAGES);
    tab.show();
    const FlightConfigurableTableModel& model = tab.motors().tableModel();
    Tab::click(tab.motors(), 1, 1);
    tab.motors().deleteMotorAction()->trigger();
    ASSERT_EQ(tab.document().getUndoDescription().value_or("-"), "Delete motor(s)");
    ASSERT_EQ(Tab::lines(tab.motors()), "rows=[1] cols=[1]");
    ASSERT_EQ(model.data(model.index(1, 1)).toString(), QStringLiteral("None"));
    const RocketComponent* before = &tab.find("Body");

    // The undo replaces every component (Rocket::loadFrom()); the tables keep their keys.
    tab.document().undo();
    QCoreApplication::processEvents();
    EXPECT_NE(&tab.find("Body"), before);
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[1] cols=[1]");
    EXPECT_EQ(tab.motors().selectedComponent(), &tab.find("Body"));
    EXPECT_EQ(model.data(model.index(1, 1)).toString(), QStringLiteral("C6-5"));
    // The design follows a click on the new objects.
    Tab::click(tab.motors(), 1, 2);
    EXPECT_EQ(tab.designSelectionNames(), "[Motor tube]");
    EXPECT_EQ(tab.designSelection().selectedComponent(), &tab.find("Motor tube"));

    tab.document().redo();
    QCoreApplication::processEvents();
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[1] cols=[2]");
    EXPECT_EQ(model.data(model.index(1, 1)).toString(), QStringLiteral("None"));
    Tab::click(tab.motors(), 2, 1);
    EXPECT_EQ(tab.designSelectionNames(), "[Body]");
    EXPECT_EQ(tab.designSelection().selectedComponent(), &tab.find("Body"));
}

}  // namespace

}  // namespace QtRocket::Gui
