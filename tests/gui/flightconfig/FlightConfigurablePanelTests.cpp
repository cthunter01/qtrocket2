// FlightConfigurablePanel: what the three panels' tables share (OpenRocket's
// FlightConfigurablePanel): the table's set-up, Enter and the double click, a click outside the
// cells, the right button's selection and popups (their items disabled like their buttons:
// U-M3-6 d), the selection after an operation (Java's fireTableDataChanged() and its
// restoreSelection()), the design's selection one turn later while the rocket delivers its
// event, and the selection through the model's resets.

#include "QtRocket/gui/flightconfig/FlightConfigurablePanel.h"

#include <optional>
#include <string>
#include <utility>

#include <QAbstractItemView>
#include <QCoreApplication>
#include <QHeaderView>
#include <QMenu>
#include <QPoint>
#include <QPushButton>
#include <QRect>
#include <QString>
#include <QTableView>
#include <QTest>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/flightconfig/FlightConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/MotorConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/RecoveryConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/RowColumnSelectionModel.h"
#include "QtRocket/gui/flightconfig/TableTransferKeys.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/Rocket.h"
#include "ScriptedUserPrompts.h"
#include "flightconfig/FlightConfigurationTabSupport.h"

namespace QtRocket::Gui
{

namespace
{

using FlightConfigurablePanelTest = Gui::Test::GuiTest;
using Tab                         = Gui::Test::FlightConfigurationTab;
using Design                      = Tab::Design;

/// A point of @p panel's viewport below the table's last row.
[[nodiscard]] QPoint belowTheRows(const FlightConfigurablePanel& panel)
{
    const QTableView& table = panel.table();
    const QRect last = table.visualRect(table.model()->index(table.model()->rowCount() - 1, 0));
    return {last.center().x(), last.bottom() + 20};
}

// ============================================================================ the table

TEST_F(FlightConfigurablePanelTest, TheTableSelectsCellsAndEditsNothing)
{
    const Tab         tab(Design::TWO_STAGES);
    const QTableView& table = tab.motors().table();
    EXPECT_EQ(table.objectName(), QStringLiteral("table"));
    EXPECT_EQ(table.selectionModel(), &tab.motors().tableSelection());
    EXPECT_EQ(table.model(), &tab.motors().tableModel());
    EXPECT_EQ(table.selectionBehavior(), QAbstractItemView::SelectItems);
    EXPECT_EQ(table.selectionMode(), QAbstractItemView::ExtendedSelection);
    EXPECT_EQ(table.editTriggers(), QAbstractItemView::NoEditTriggers);
    EXPECT_TRUE(table.verticalHeader()->isHidden());
    EXPECT_FALSE(table.horizontalHeader()->sectionsMovable());
    EXPECT_FALSE(table.horizontalHeader()->sectionsClickable());
    EXPECT_EQ(table.horizontalHeader()->sectionResizeMode(0), QHeaderView::Stretch);
    EXPECT_EQ(table.contextMenuPolicy(), Qt::CustomContextMenu);
    EXPECT_EQ(tab.motors().fullPopupMenu().objectName(), QStringLiteral("popupMenuFull"));
}

// ================================================================= the mouse and the keys

TEST_F(FlightConfigurablePanelTest, EnterRunsTheSelectOperationOfTheSelectedCells)
{
    Tab tab(Design::TWO_STAGES);
    tab.show();
    Tab::click(tab.motors(), 1, 2);
    tab.prompts().answerMotor(std::nullopt);
    QTest::keyClick(&tab.motors().table(), Qt::Key_Return);
    ASSERT_EQ(tab.prompts().motorQuestions().size(), 1U);
    EXPECT_EQ(tab.prompts().motorQuestions()[0].mountName, QStringLiteral("Motor tube"));
    EXPECT_EQ(tab.prompts().motorQuestions()[0].configurationId, tab.id(1));
    // The keypad's Enter too, not with a modifier; Enter moves nothing.
    tab.prompts().answerMotor(std::nullopt);
    QTest::keyClick(&tab.motors().table(), Qt::Key_Enter, Qt::KeypadModifier);
    EXPECT_EQ(tab.prompts().motorQuestions().size(), 2U);
    QTest::keyClick(&tab.motors().table(), Qt::Key_Return, Qt::ShiftModifier);
    EXPECT_EQ(tab.prompts().motorQuestions().size(), 2U);
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[1] cols=[2]");
}

TEST_F(FlightConfigurablePanelTest, ADoubleClickRunsItWhenTheSelectedColumnIsAComponents)
{
    Tab tab(Design::TWO_STAGES);
    tab.show();
    tab.prompts().answerMotor(std::nullopt);
    Tab::doubleClick(tab.motors(), 2, 3);
    ASSERT_EQ(tab.prompts().motorQuestions().size(), 1U);
    EXPECT_EQ(tab.prompts().motorQuestions()[0].mountName, QStringLiteral("Booster tube"));
    EXPECT_EQ(tab.prompts().motorQuestions()[0].configurationId, tab.id(2));
    // Column 0: nothing.
    Tab::doubleClick(tab.motors(), 1, 0);
    EXPECT_EQ(tab.prompts().motorQuestions().size(), 1U);
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[1] cols=[0]");
}

TEST_F(FlightConfigurablePanelTest, AClickOutsideTheCellsClearsTheSelection)
{
    const Tab tab(Design::TWO_STAGES);
    tab.show();
    Tab::click(tab.motors(), 1, 1);
    QTest::mouseClick(tab.motors().table().viewport(), Qt::LeftButton, {},
                      belowTheRows(tab.motors()));
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[] cols=[]");
    // The rocket keeps its configuration (Java's clearSelection() selects none).
    EXPECT_EQ(tab.selectedRow(), 1);
    EXPECT_FALSE(tab.motors().selectMotorAction()->isEnabled());
}

TEST_F(FlightConfigurablePanelTest, TheRightButtonSelectsTheCellAndOpensItsPopup)
{
    const Tab tab(Design::TWO_STAGES);
    tab.show();
    // A press of the right button alone selects nothing (Java's table ignores it).
    QTest::mouseClick(tab.motors().table().viewport(), Qt::RightButton, {},
                      Tab::cellCenter(tab.motors(), 2, 2));
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[0] cols=[3]");

    // A component's cell: selected alone (its rows first: the rocket and then the design follow),
    // and the panel's popup.
    const QMenu* menu = Tab::rightClick(tab.motors(), 2, 2);
    EXPECT_EQ(menu, &tab.motors().fullPopupMenu());
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[2] cols=[2]");
    EXPECT_EQ(tab.selectedRow(), 2);
    EXPECT_EQ(tab.designSelectionNames(), "[Motor tube]");

    // A selected cell keeps the selection.
    Tab::click(tab.motors(), 1, 1, Qt::ShiftModifier);
    ASSERT_EQ(Tab::lines(tab.motors()), "rows=[1, 2] cols=[1, 2]");
    menu = Tab::rightClick(tab.motors(), 1, 2);
    EXPECT_EQ(menu, &tab.motors().fullPopupMenu());
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[1, 2] cols=[1, 2]");

    // Column 0: the tab's configuration popup.
    menu = Tab::rightClick(tab.motors(), 0, 0);
    EXPECT_EQ(menu, &tab.panel().configurationPopupMenu());
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[0] cols=[0]");
    EXPECT_EQ(tab.selectedRow(), 0);

    // Outside the cells: nothing selected, no popup.
    menu = Tab::rightClickAt(tab.motors(), belowTheRows(tab.motors()));
    EXPECT_EQ(menu, nullptr);
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[] cols=[]");
}

TEST_F(FlightConfigurablePanelTest, ThePopupsItemsAreDisabledLikeTheirButtons)
{
    const Tab tab(Design::TWO_STAGES);
    tab.show();
    EXPECT_EQ(Tab::items(tab.motors().fullPopupMenu()),
              "selectMotorAction, deleteMotorAction, -, selectIgnitionAction, "
              "resetIgnitionAction, -, renameConfigAction, deleteConfigAction, "
              "duplicateConfigAction");
    // The first selected column is column 0: the motor buttons, and so their items, are off
    // (Java's popup items always act: U-M3-6 d).
    Tab::click(tab.motors(), 1, 0);
    Tab::click(tab.motors(), 1, 2, Qt::ControlModifier);
    ASSERT_EQ(Tab::lines(tab.motors()), "rows=[1] cols=[0, 2]");
    EXPECT_FALSE(tab.motors().selectMotorButton()->isEnabled());
    const QMenu* menu = Tab::rightClick(tab.motors(), 1, 2);
    ASSERT_EQ(menu, &tab.motors().fullPopupMenu());
    EXPECT_EQ(Tab::items(*menu),
              "selectMotorAction (disabled), deleteMotorAction (disabled), -, "
              "selectIgnitionAction (disabled), resetIgnitionAction (disabled), -, "
              "renameConfigAction, deleteConfigAction, duplicateConfigAction");

    // The default configuration selected: Rename, Delete and Duplicate are off in every popup.
    tab.rocket().setSelectedConfiguration(FlightConfigurationId::defaultValueId());
    EXPECT_EQ(Tab::items(tab.panel().configurationPopupMenu()),
              "newConfigAction, renameConfigAction (disabled), deleteConfigAction (disabled), "
              "duplicateConfigAction (disabled)");
}

// ============================================================== after an operation (Java's)

TEST_F(FlightConfigurablePanelTest, AnOperationThatChangedTheDesignLeavesTheFirstCellSelected)
{
    Tab tab(Design::TWO_STAGES);
    tab.show();
    Tab::click(tab.motors(), 1, 1);
    Tab::click(tab.motors(), 2, 3, Qt::ShiftModifier);
    ASSERT_EQ(Tab::lines(tab.motors()), "rows=[1, 2] cols=[1, 2, 3]");
    // Select motor cancelled changed nothing: the selection stays (Java gives the table the
    // focus).
    tab.prompts().answerMotor(std::nullopt);
    tab.motors().selectMotorAction()->trigger();
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[1, 2] cols=[1, 2, 3]");

    tab.motors().deleteMotorAction()->trigger();
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[1] cols=[1]");
    EXPECT_EQ(tab.selectedRow(), 1);

    // A first cell in row 0 (or column 0): the rocket's selected configuration in the last column
    // (Java's restoreSelection(): row <= 0 || col <= 0; the operation pins' CELLS lines).
    Tab::click(tab.motors(), 0, 1);
    Tab::click(tab.motors(), 1, 2, Qt::ShiftModifier);
    tab.prompts().answerIgnition(
        UserPrompts::IgnitionChoice{.scope = UserPrompts::ConfigurationScope::THIS_CONFIGURATION,
                                    .event = IgnitionEvent::LAUNCH,
                                    .delay = 1});
    tab.motors().selectIgnitionAction()->trigger();
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[0] cols=[3]");
    EXPECT_EQ(tab.selectedRow(), 0);
}

TEST_F(FlightConfigurablePanelTest, TheSynchronizationSetsTheDesignOneTurnLater)
{
    const Tab tab(Design::TWO_STAGES);
    tab.show();
    Tab::click(tab.motors(), 1, 1);
    Tab::click(tab.motors(), 1, 2, Qt::ShiftModifier);
    ASSERT_EQ(Tab::lines(tab.motors()), "rows=[1] cols=[1, 2]");
    ASSERT_EQ(tab.designSelectionNames(), "[Body, Motor tube]");
    // The rocket selects another configuration: the table selects it in its first selected
    // column, and the design follows that column after the rocket's event.
    tab.rocket().setSelectedConfiguration(tab.id(2));
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[2] cols=[1]");
    EXPECT_EQ(tab.designSelectionNames(), "[Body, Motor tube]");
    QCoreApplication::processEvents();
    EXPECT_EQ(tab.designSelectionNames(), "[Body]");
}

// ======================================================================= the model's resets

TEST_F(FlightConfigurablePanelTest, AResetByAMotorEventClearsAndFollowsTheRocket)
{
    const Tab tab(Design::TWO_STAGES);
    tab.show();
    Tab::click(tab.motors(), 2, 1);
    Tab::click(tab.motors(), 1, 3, Qt::ShiftModifier);
    ASSERT_EQ(Tab::lines(tab.motors()), "rows=[1, 2] cols=[1, 2, 3]");
    // Motor tube stops acting as a motor mount (from the component dialog, say): the motor table
    // loses a column, as Java's structure change clears it, and selects the rocket's
    // configuration in its last column.
    dynamic_cast<MotorMount&>(tab.find("Motor tube")).setMotorMount(false);
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[2] cols=[2]");
    EXPECT_EQ(tab.motors().tableModel().columnCount(), 3);
    // The other tables keep theirs.
    EXPECT_EQ(Tab::lines(tab.recovery()), "rows=[2] cols=[1]");
}

TEST_F(FlightConfigurablePanelTest, AConfigurationAddedKeepsTheSelectionOfTheTables)
{
    const Tab tab(Design::TWO_STAGES);
    tab.show();
    Tab::click(tab.motors(), 1, 1);
    Tab::click(tab.motors(), 2, 2, Qt::ShiftModifier);
    // A new row stored as New Configuration stores it (NONFUNCTIONAL_CHANGE): Java's table sees
    // no change of its structure and keeps its selection.
    FlightConfiguration         configuration{tab.rocket(), FlightConfigurationId{}};
    const FlightConfigurationId id = configuration.getId();
    tab.rocket().setFlightConfiguration(id, std::move(configuration));
    EXPECT_EQ(tab.motors().tableModel().rowCount(), 4);
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[1, 2] cols=[1, 2]");
    EXPECT_EQ(tab.selectedRow(), 1);

    // A configuration made by createFlightConfiguration() comes with TREE_CHANGE: cleared, then
    // the rocket's configuration in the last column, as Java.
    static_cast<void>(tab.rocket().createFlightConfiguration());
    EXPECT_EQ(tab.motors().tableModel().rowCount(), 5);
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[1] cols=[3]");
}

// ===================================================================== the clipboard keys

TEST_F(FlightConfigurablePanelTest, CopyTakesJavasValuesOfTheSelectedCells)
{
    const Tab                      tab(Design::TWO_STAGES);
    const MotorConfigurationPanel& motors = tab.motors();
    const std::string              first  = tab.id(0).toString();
    const std::string              third  = tab.id(2).toString();
    // Java's getValueAt(row, column).toString() of the selected rows x columns: the
    // configuration's id in column 0, the Pair of it and the mount ("[id;name]") in a mount's.
    motors.tableSelection().setLines({.rows = {0, 2}, .columns = {0, 2}},
                                     RowColumnSelectionModel::Order::COLUMNS_FIRST);
    EXPECT_EQ(TableTransferKeys::plainText(motors.transferCells()),
              QtStrings::fromUtf8(first + "\t[" + first + ";Motor tube]\n" + third + "\t[" + third +
                                  ";Motor tube]"));
    // The recovery table's cells name the device.
    tab.recovery().tableSelection().selectCell(1, 1);
    EXPECT_EQ(TableTransferKeys::plainText(tab.recovery().transferCells()),
              QtStrings::fromUtf8("[" + tab.id(1).toString() + ";Chute]"));
    // Nothing selected: nothing to copy.
    motors.tableSelection().clearLines();
    EXPECT_TRUE(motors.transferCells().empty());
}

}  // namespace

}  // namespace QtRocket::Gui
