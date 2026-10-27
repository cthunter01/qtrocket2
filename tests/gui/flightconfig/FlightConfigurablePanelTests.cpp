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
#include <QEvent>
#include <QHeaderView>
#include <QMenu>
#include <QMouseEvent>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QPushButton>
#include <QRect>
#include <QString>
#include <QTableView>
#include <QTest>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/flightconfig/ConfigurationTableView.h"
#include "QtRocket/gui/flightconfig/FlightConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/MotorConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/RecoveryConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/RowColumnSelectionModel.h"
#include "QtRocket/gui/flightconfig/SeparationConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/TableTransferKeys.h"
#include "QtRocket/gui/tree/DocumentSelectionModel.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
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

/// Selects rows 1 and 2 of @p panel's table in column 1.
void selectTwoRows(const FlightConfigurablePanel& panel)
{
    panel.tableSelection().setLines({.rows = {1, 2}, .columns = {1}},
                                    RowColumnSelectionModel::Order::COLUMNS_FIRST);
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
    // A JTable's columns: resizable by hand, the last one taking the rest of the width.
    EXPECT_EQ(table.horizontalHeader()->sectionResizeMode(0), QHeaderView::Interactive);
    EXPECT_TRUE(table.horizontalHeader()->stretchLastSection());
    // Its mouse and keys select as a JTable's.
    EXPECT_NE(qobject_cast<const ConfigurationTableView*>(&table), nullptr);
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
    // The other tables too (every MOTOR change is one of their structure).
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

TEST_F(FlightConfigurablePanelTest, EveryMotorOrTreeChangeClearsTheTablesAsJavasStructureChange)
{
    const Tab tab(Design::TWO_STAGES);
    tab.show();
    // The M3 run 2 verifier's probe (OpenRocket's real panels, "Two stage high power rocket.ork"):
    // [1]/[1] becomes the rocket's row in the last column after an external MOTOR change, and
    // again after a TREE change, the rows and the columns unchanged.
    Tab::click(tab.motors(), 1, 1);
    tab.rocket().fireComponentChangeEvent(ComponentChangeEvent::kMotorChange);
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[1] cols=[3]");
    Tab::click(tab.motors(), 1, 1);
    tab.rocket().fireComponentChangeEvent(ComponentChangeEvent::kTreeChange);
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[1] cols=[3]");
    EXPECT_EQ(tab.selectedRow(), 1);

    // A NONFUNCTIONAL change of a body tube (whose name heads a motor column) clears the motor
    // table alone, one of a stage the stage table alone, one of a parachute none (Java's motor
    // and stage table models' componentChanged()).
    selectTwoRows(tab.motors());
    selectTwoRows(tab.recovery());
    selectTwoRows(tab.stages());
    tab.find("Chute").setComment("a comment");
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[1, 2] cols=[1]");
    EXPECT_EQ(Tab::lines(tab.recovery()), "rows=[1, 2] cols=[1]");
    EXPECT_EQ(Tab::lines(tab.stages()), "rows=[1, 2] cols=[1]");
    tab.find("Body").setComment("a comment");
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[1] cols=[3]");
    EXPECT_EQ(Tab::lines(tab.recovery()), "rows=[1, 2] cols=[1]");
    EXPECT_EQ(Tab::lines(tab.stages()), "rows=[1, 2] cols=[1]");
    selectTwoRows(tab.motors());
    tab.find("Booster").setComment("a comment");
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[1, 2] cols=[1]");
    EXPECT_EQ(Tab::lines(tab.recovery()), "rows=[1, 2] cols=[1]");
    EXPECT_EQ(Tab::lines(tab.stages()), "rows=[1] cols=[1]");
}

TEST_F(FlightConfigurablePanelTest, TheFirstCellIsTheOneSelectedWhenTheOperationFires)
{
    const Tab tab(Design::TWO_STAGES);
    tab.show();
    // The M3 run 2 verifier's probe (OpenRocket's real panels): a selection made from the bottom
    // up, the rocket on its lower row; after Delete motor(s), Java's first cell, read before the
    // operation's event (which synchronizes the table to the rocket's row), is the upper row's,
    // and the rocket follows it.
    Tab::click(tab.motors(), 2, 1);
    Tab::click(tab.motors(), 1, 1, Qt::ShiftModifier);
    ASSERT_EQ(Tab::lines(tab.motors()), "rows=[1, 2] cols=[1]");
    ASSERT_EQ(tab.selectedRow(), 2);
    tab.motors().deleteMotorAction()->trigger();
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[1] cols=[1]");
    EXPECT_EQ(tab.selectedRow(), 1);
    // From row 0: Java's restoreSelection() synchronizes (row <= 0): the rocket's row in the last
    // column.
    Tab::click(tab.motors(), 0, 1, Qt::ShiftModifier);
    ASSERT_EQ(Tab::lines(tab.motors()), "rows=[0, 1] cols=[1]");
    ASSERT_EQ(tab.selectedRow(), 1);
    tab.motors().deleteMotorAction()->trigger();
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[1] cols=[3]");
    EXPECT_EQ(tab.selectedRow(), 1);
}

// ================================================================= the gestures of the mouse

TEST_F(FlightConfigurablePanelTest, AClickIntoAnEmptyTableSelectsItsComponentInTheDesign)
{
    const Tab tab(Design::TWO_STAGES);
    tab.show();
    tab.motors().clearSelection();
    tab.designSelection().clearComponentSelection();
    ASSERT_EQ(Tab::lines(tab.motors()), "rows=[] cols=[]");
    // The review's ClickProbe (BasicTableUI): at the release the rows' change, then the columns'
    // with the row selected, which selects the column's component in the design.
    Tab::click(tab.motors(), 1, 2);
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[1] cols=[2]");
    EXPECT_EQ(tab.selectedRow(), 1);
    EXPECT_EQ(tab.designSelectionNames(), "[Motor tube]");
}

TEST_F(FlightConfigurablePanelTest, ADragOverTwoRowsLeavesTheRocketsConfiguration)
{
    const Tab tab(Design::TWO_STAGES);
    tab.show();
    Tab::click(tab.motors(), 0, 1);
    ASSERT_EQ(tab.selectedRow(), 0);
    // The review's DragProbe: the press and the drag are adjusting, and the release tells rows of
    // two: Java's listener selects no configuration.
    QWidget&     viewport = *tab.motors().table().viewport();
    const QPoint to       = Tab::cellCenter(tab.motors(), 1, 1);
    QTest::mousePress(&viewport, Qt::LeftButton, {}, Tab::cellCenter(tab.motors(), 2, 1));
    EXPECT_EQ(tab.selectedRow(), 0);
    QMouseEvent move(QEvent::MouseMove, QPointF(to), viewport.mapToGlobal(to), Qt::NoButton,
                     Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&viewport, &move);
    QTest::mouseRelease(&viewport, Qt::LeftButton, {}, to);
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[1, 2] cols=[1]");
    EXPECT_EQ(tab.selectedRow(), 0);
}

TEST_F(FlightConfigurablePanelTest, ATypedKeySelectsNoRow)
{
    const Tab tab(Design::TWO_STAGES);
    tab.show();
    Tab::click(tab.motors(), 2, 1);
    ASSERT_EQ(tab.motors().tableModel().index(0, 1).data().toString(), QStringLiteral("C6-5"));
    // A JTable has no type-ahead search: "C" leaves the selection, and the rocket, on row 2
    // (Qt's search would select row 0).
    QTest::keyClicks(&tab.motors().table(), QStringLiteral("C"));
    EXPECT_EQ(Tab::lines(tab.motors()), "rows=[2] cols=[1]");
    EXPECT_EQ(tab.selectedRow(), 2);
}

// ========================================================================= the columns

TEST_F(FlightConfigurablePanelTest, TheColumnsShareTheWidthUntilOneIsResizedByHand)
{
    const Tab tab(Design::TWO_STAGES);
    tab.show();
    const QTableView&  table  = tab.motors().table();
    const QHeaderView& header = *table.horizontalHeader();
    const int          width  = table.viewport()->width();
    ASSERT_EQ(header.count(), 4);
    // Equal widths, the last column taking what the division leaves.
    EXPECT_EQ(header.sectionSize(0), width / 4);
    EXPECT_EQ(header.sectionSize(2), width / 4);
    EXPECT_EQ(header.length(), width);
    EXPECT_FALSE(tab.motors().columnsResizedByHand());

    // A drag of the border between the first two columns 40 pixels to the right.
    QWidget&     headerViewport = *header.viewport();
    const int    border         = header.sectionViewportPosition(1);
    const QPoint from(border - 1, header.height() / 2);
    const QPoint to(border + 39, header.height() / 2);
    QTest::mousePress(&headerViewport, Qt::LeftButton, {}, from);
    QMouseEvent move(QEvent::MouseMove, QPointF(to), headerViewport.mapToGlobal(to), Qt::NoButton,
                     Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&headerViewport, &move);
    QTest::mouseRelease(&headerViewport, Qt::LeftButton, {}, to);
    EXPECT_TRUE(tab.motors().columnsResizedByHand());
    EXPECT_EQ(header.sectionSize(0), (width / 4) + 40);

    // A new row (no change of the structure: the model resets with the same columns) keeps the
    // widths, as Java's JTable.
    FlightConfiguration         configuration{tab.rocket(), FlightConfigurationId{}};
    const FlightConfigurationId id = configuration.getId();
    tab.rocket().setFlightConfiguration(id, std::move(configuration));
    ASSERT_EQ(tab.motors().tableModel().rowCount(), 4);
    EXPECT_EQ(header.sectionSize(0), (width / 4) + 40);
    // A change of the structure shares the width again.
    tab.rocket().fireComponentChangeEvent(ComponentChangeEvent::kMotorChange);
    EXPECT_FALSE(tab.motors().columnsResizedByHand());
    EXPECT_EQ(header.sectionSize(0), width / 4);
    EXPECT_EQ(header.length(), width);
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
