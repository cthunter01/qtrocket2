// Tab, Shift+Tab, Right and Left move a table's selection from row to row (Java:
// TableRowTraversalPolicy, swing utils/TableRowTraversalPolicy.java).
//
// The first seven tests are the ports of the seven methods of OpenRocket's
// TableRowTraversalPolicyTest (swing src/test/java/info/openrocket/swing/utils/): its table is a
// JTable(3, 2) with cell selection (row and column selection allowed: Qt's SelectItems with
// ExtendedSelection), its actions run through the action map (TableRowTraversal::perform()) and
// its key bindings are read from the input map (TableRowTraversal::actionFor()).
//
// Expectations marked "probe" are OpenRocket's own results, printed by
// tools/openrocket-gui-pins/preset-chooser/probe/TableRowTraversalProbe.java (run on OpenRocket
// 5f164fd0e with JDK 17; the family's hand-pinned.txt keeps its transcript): the policy on JTables
// of other shapes and selections, its key bindings and JTable's focus traversal keys.

#include "QtRocket/gui/widgets/TableRowTraversal.h"

#include <optional>

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QHeaderView>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QList>
#include <QModelIndex>
#include <QPushButton>
#include <QRect>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QString>
#include <QTableView>
#include <QTest>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using TableRowTraversalTest = Test::GuiTest;
using Action                = TableRowTraversal::Action;
using FocusMove             = TableRowTraversal::FocusMove;
using Test::showActiveWith;

/// A table of @p rows rows and @p columns columns with the policy installed. By default Java's
/// test table (createTableWithPolicy(): JTable(3, 2) with cell selection), a table that selects
/// items in extended selection; the parts library's selects single rows.
struct PolicyTable
{
    QStandardItemModel model;
    QTableView         view;

    explicit PolicyTable(
        int rows = 3, int columns = 2,
        QAbstractItemView::SelectionBehavior behavior = QAbstractItemView::SelectItems,
        QAbstractItemView::SelectionMode     mode     = QAbstractItemView::ExtendedSelection)
      : model(rows, columns)
    {
        view.setModel(&model);
        view.setSelectionBehavior(behavior);
        view.setSelectionMode(mode);
        TableRowTraversal::install(view);
    }

    [[nodiscard]] QModelIndex at(int row, int column) const { return model.index(row, column); }

    /// Java's setRowSelectionInterval(@p first, @p last): those rows, every column.
    void selectRows(int first, int last)
    {
        view.selectionModel()->select(
            QItemSelection(at(first, 0), at(last, model.columnCount() - 1)),
            QItemSelectionModel::ClearAndSelect);
    }

    /// Java's setRowSelectionInterval(@p row, @p row) and setColumnSelectionInterval(@p column,
    /// @p column) with cell selection: that cell, current.
    void selectCell(int row, int column)
    {
        view.selectionModel()->setCurrentIndex(at(row, column),
                                               QItemSelectionModel::ClearAndSelect);
    }

    /// Java's getColumnModel().getSelectionModel().getMinSelectionIndex(): the lowest selected
    /// column, -1 for none.
    [[nodiscard]] int minSelectedColumn() const
    {
        int column = -1;
        for (const QItemSelectionRange& range : view.selectionModel()->selection())
        {
            if (column < 0 || range.left() < column)
            {
                column = range.left();
            }
        }
        return column;
    }

    /// The selected cells, in the selection's order.
    [[nodiscard]] QList<QModelIndex> selected() const
    {
        return view.selectionModel()->selectedIndexes();
    }
};

/// A key press of @p key with @p modifiers, as the platform sends it.
[[nodiscard]] QKeyEvent keyPress(int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    return {QEvent::KeyPress, key, modifiers};
}

// ---------------------------------------------------------------- TableRowTraversalPolicyTest

// Port of TableRowTraversalPolicyTest.nextRowActionAdvancesSelectionAndResetsColumn.
TEST_F(TableRowTraversalTest, NextRowActionAdvancesSelectionAndResetsColumn)
{
    PolicyTable table;
    table.selectCell(0, 1);

    TableRowTraversal::perform(table.view, Action::NEXT_ROW);

    EXPECT_EQ(TableRowTraversal::selectedRow(table.view), 1);
    EXPECT_EQ(table.minSelectedColumn(), 0);
}

// Port of TableRowTraversalPolicyTest.nextRowActionDoesNotWrapWithoutCycle.
TEST_F(TableRowTraversalTest, NextRowActionDoesNotWrapWithoutCycle)
{
    PolicyTable table;
    table.selectRows(2, 2);

    TableRowTraversal::perform(table.view, Action::NEXT_ROW);

    EXPECT_EQ(TableRowTraversal::selectedRow(table.view), 2);
}

// Port of TableRowTraversalPolicyTest.nextRowCycleWrapsToTop.
TEST_F(TableRowTraversalTest, NextRowCycleWrapsToTop)
{
    PolicyTable table;
    table.selectRows(2, 2);

    TableRowTraversal::perform(table.view, Action::NEXT_ROW_CYCLE);

    EXPECT_EQ(TableRowTraversal::selectedRow(table.view), 0);
}

// Port of TableRowTraversalPolicyTest.previousRowActionDoesNotWrapWithoutCycle.
TEST_F(TableRowTraversalTest, PreviousRowActionDoesNotWrapWithoutCycle)
{
    PolicyTable table;
    table.selectRows(0, 0);

    TableRowTraversal::perform(table.view, Action::PREVIOUS_ROW);

    EXPECT_EQ(TableRowTraversal::selectedRow(table.view), 0);
}

// Port of TableRowTraversalPolicyTest.previousRowActionMovesUpAndResetsColumn.
TEST_F(TableRowTraversalTest, PreviousRowActionMovesUpAndResetsColumn)
{
    PolicyTable table;
    table.selectCell(2, 1);

    TableRowTraversal::perform(table.view, Action::PREVIOUS_ROW);

    EXPECT_EQ(TableRowTraversal::selectedRow(table.view), 1);
    EXPECT_EQ(table.minSelectedColumn(), 0);
}

// Port of TableRowTraversalPolicyTest.previousRowCycleWrapsToBottom.
TEST_F(TableRowTraversalTest, PreviousRowCycleWrapsToBottom)
{
    PolicyTable table;
    table.selectRows(0, 0);

    TableRowTraversal::perform(table.view, Action::PREVIOUS_ROW_CYCLE);

    EXPECT_EQ(TableRowTraversal::selectedRow(table.view), 2);
}

// Port of TableRowTraversalPolicyTest.installsExpectedKeyBindings: the input map's entries for
// VK_TAB, VK_RIGHT, VK_TAB with SHIFT_DOWN_MASK and VK_LEFT. Qt's platforms send Shift+Tab as
// Backtab with Shift; Tab with Shift means the same to Qt's widgets.
TEST_F(TableRowTraversalTest, InstallsExpectedKeyBindings)
{
    EXPECT_EQ(TableRowTraversal::actionFor(keyPress(Qt::Key_Tab)), Action::NEXT_ROW_CYCLE);
    EXPECT_EQ(TableRowTraversal::actionFor(keyPress(Qt::Key_Right)), Action::NEXT_ROW);
    EXPECT_EQ(TableRowTraversal::actionFor(keyPress(Qt::Key_Backtab, Qt::ShiftModifier)),
              Action::PREVIOUS_ROW_CYCLE);
    EXPECT_EQ(TableRowTraversal::actionFor(keyPress(Qt::Key_Tab, Qt::ShiftModifier)),
              Action::PREVIOUS_ROW_CYCLE);
    EXPECT_EQ(TableRowTraversal::actionFor(keyPress(Qt::Key_Left)), Action::PREVIOUS_ROW);
}

// ------------------------------------------------------------------------ the other keys

TEST_F(TableRowTraversalTest, OtherKeysAndModifiersAreNotBound)
{
    // probe: "shift pressed RIGHT: focused null", "ctrl pressed RIGHT: focused null", the same for
    // LEFT, and DOWN, UP and ENTER keep JTable's own actions.
    EXPECT_EQ(TableRowTraversal::actionFor(keyPress(Qt::Key_Right, Qt::ShiftModifier)),
              std::nullopt);
    EXPECT_EQ(TableRowTraversal::actionFor(keyPress(Qt::Key_Right, Qt::ControlModifier)),
              std::nullopt);
    EXPECT_EQ(TableRowTraversal::actionFor(keyPress(Qt::Key_Left, Qt::ShiftModifier)),
              std::nullopt);
    EXPECT_EQ(TableRowTraversal::actionFor(keyPress(Qt::Key_Left, Qt::ControlModifier)),
              std::nullopt);
    EXPECT_EQ(TableRowTraversal::actionFor(keyPress(Qt::Key_Down)), std::nullopt);
    EXPECT_EQ(TableRowTraversal::actionFor(keyPress(Qt::Key_Up)), std::nullopt);
    EXPECT_EQ(TableRowTraversal::actionFor(keyPress(Qt::Key_Return)), std::nullopt);
    EXPECT_EQ(TableRowTraversal::actionFor(keyPress(Qt::Key_Tab, Qt::AltModifier)), std::nullopt);
    // probe: "ctrl pressed TAB: focused null": a focus traversal key of the JTable instead.
    EXPECT_EQ(TableRowTraversal::actionFor(keyPress(Qt::Key_Tab, Qt::ControlModifier)),
              std::nullopt);
    // Deviation (TableRowTraversal.h): the keypad's arrows move rows too (Qt marks every arrow of
    // macOS with the keypad modifier); probe: "pressed KP_RIGHT: focused null, ancestor
    // selectNextColumn".
    EXPECT_EQ(TableRowTraversal::actionFor(keyPress(Qt::Key_Right, Qt::KeypadModifier)),
              Action::NEXT_ROW);
    EXPECT_EQ(TableRowTraversal::actionFor(keyPress(Qt::Key_Left, Qt::KeypadModifier)),
              Action::PREVIOUS_ROW);
}

TEST_F(TableRowTraversalTest, CtrlTabAndCtrlShiftTabAreTheFocusTraversalKeys)
{
    // probe: "focus forward [ctrl pressed TAB] backward [shift ctrl pressed TAB]".
    EXPECT_EQ(TableRowTraversal::focusMoveFor(keyPress(Qt::Key_Tab, Qt::ControlModifier)),
              FocusMove::FORWARD);
    EXPECT_EQ(TableRowTraversal::focusMoveFor(
                  keyPress(Qt::Key_Tab, Qt::ControlModifier | Qt::ShiftModifier)),
              FocusMove::BACKWARD);
    EXPECT_EQ(TableRowTraversal::focusMoveFor(
                  keyPress(Qt::Key_Backtab, Qt::ControlModifier | Qt::ShiftModifier)),
              FocusMove::BACKWARD);
    // macOS's Control key is Qt's Meta.
    EXPECT_EQ(TableRowTraversal::focusMoveFor(keyPress(Qt::Key_Tab, Qt::MetaModifier)),
              FocusMove::FORWARD);
    EXPECT_EQ(TableRowTraversal::focusMoveFor(keyPress(Qt::Key_Tab)), std::nullopt);
    EXPECT_EQ(TableRowTraversal::focusMoveFor(keyPress(Qt::Key_Backtab, Qt::ShiftModifier)),
              std::nullopt);
    EXPECT_EQ(TableRowTraversal::focusMoveFor(
                  keyPress(Qt::Key_Tab, Qt::ControlModifier | Qt::AltModifier)),
              std::nullopt);
    EXPECT_EQ(TableRowTraversal::focusMoveFor(keyPress(Qt::Key_Right, Qt::ControlModifier)),
              std::nullopt);
}

// ------------------------------------------------------------------- the actions, probed

TEST_F(TableRowTraversalTest, WithoutASelectionRightAndTabTakeTheFirstRow)
{
    // probe, "no selection (cell table, 3 rows)": NextRow and NextRowCycle select row 0 and
    // column 0, PreviousRow nothing, PreviousRowCycle row 2 and column 0.
    for (const Action action : {Action::NEXT_ROW, Action::NEXT_ROW_CYCLE})
    {
        PolicyTable table;
        TableRowTraversal::perform(table.view, action);
        EXPECT_EQ(table.selected(), QList<QModelIndex>{table.at(0, 0)});
        EXPECT_EQ(table.view.currentIndex(), table.at(0, 0));
    }
    PolicyTable left;
    TableRowTraversal::perform(left.view, Action::PREVIOUS_ROW);
    EXPECT_EQ(TableRowTraversal::selectedRow(left.view), -1);
    EXPECT_TRUE(left.selected().isEmpty());
    PolicyTable back;
    TableRowTraversal::perform(back.view, Action::PREVIOUS_ROW_CYCLE);
    EXPECT_EQ(back.selected(), QList<QModelIndex>{back.at(2, 0)});
}

TEST_F(TableRowTraversalTest, SeveralSelectedRowsMoveFromTheLowest)
{
    // probe, "rows 1..2 selected (cell table, 4 rows), column 1": NextRow row 2, PreviousRow row
    // 0, each alone, in column 0.
    PolicyTable next(4);
    next.selectRows(1, 2);
    TableRowTraversal::perform(next.view, Action::NEXT_ROW);
    EXPECT_EQ(next.selected(), QList<QModelIndex>{next.at(2, 0)});
    PolicyTable previous(4);
    previous.selectRows(1, 2);
    TableRowTraversal::perform(previous.view, Action::PREVIOUS_ROW);
    EXPECT_EQ(previous.selected(), QList<QModelIndex>{previous.at(0, 0)});
}

TEST_F(TableRowTraversalTest, AnActionWithNowhereToGoChangesNothing)
{
    // probe, "one row, row 0 selected": NextRow and PreviousRow leave the row and the column
    // selection as they were; the cycling ones select row 0 again, in column 0.
    PolicyTable table(1);
    table.selectCell(0, 1);
    TableRowTraversal::perform(table.view, Action::NEXT_ROW);
    TableRowTraversal::perform(table.view, Action::PREVIOUS_ROW);
    EXPECT_EQ(table.selected(), QList<QModelIndex>{table.at(0, 1)});
    EXPECT_EQ(table.view.currentIndex(), table.at(0, 1));
    TableRowTraversal::perform(table.view, Action::NEXT_ROW_CYCLE);
    EXPECT_EQ(table.selected(), QList<QModelIndex>{table.at(0, 0)});
    table.selectCell(0, 1);
    TableRowTraversal::perform(table.view, Action::PREVIOUS_ROW_CYCLE);
    EXPECT_EQ(table.selected(), QList<QModelIndex>{table.at(0, 0)});
}

TEST_F(TableRowTraversalTest, ARowTableSelectsTheWholeRowFromItsFirstColumn)
{
    // probe, "row table (single selection), row 1 col lead 2, 3 rows 3 columns": every action
    // moves the row and puts the lead column at 0 (the parts library's table).
    PolicyTable table(3, 3, QAbstractItemView::SelectRows, QAbstractItemView::SingleSelection);
    table.view.selectionModel()->setCurrentIndex(
        table.at(1, 2), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    TableRowTraversal::perform(table.view, Action::NEXT_ROW);
    EXPECT_EQ(table.selected(),
              (QList<QModelIndex>{table.at(2, 0), table.at(2, 1), table.at(2, 2)}));
    EXPECT_EQ(table.view.currentIndex(), table.at(2, 0));
    EXPECT_TRUE(table.view.selectionModel()->isRowSelected(2));
    TableRowTraversal::perform(table.view, Action::PREVIOUS_ROW);
    TableRowTraversal::perform(table.view, Action::PREVIOUS_ROW);
    EXPECT_EQ(TableRowTraversal::selectedRow(table.view), 0);
    EXPECT_TRUE(table.view.selectionModel()->isRowSelected(0));
    EXPECT_EQ(table.view.currentIndex(), table.at(0, 0));
}

TEST_F(TableRowTraversalTest, ColumnZeroIsTheFirstColumnTheTableShows)
{
    // Java's view column 0 is the first column of the JTable's column model, which holds the shown
    // columns in their order: here column 2, moved first, after the hidden column 1.
    PolicyTable  table(3, 3);
    QHeaderView& header = *table.view.horizontalHeader();
    header.moveSection(2, 0);  // the order 2, 0, 1
    header.moveSection(2, 0);  // the order 1, 2, 0
    table.view.setColumnHidden(1, true);
    table.selectCell(0, 0);
    TableRowTraversal::perform(table.view, Action::NEXT_ROW);
    EXPECT_EQ(table.selected(), QList<QModelIndex>{table.at(1, 2)});
    EXPECT_EQ(table.view.currentIndex(), table.at(1, 2));
}

TEST_F(TableRowTraversalTest, AnEmptyTableSelectsNothing)
{
    // Deviation (TableRowTraversal.h); probe, "empty table": Java's NextRowCycle selects row 0 of
    // no rows ("row 0 rows [0,0] ... count 1") and PreviousRowCycle column 0.
    for (const Action action : {Action::NEXT_ROW, Action::NEXT_ROW_CYCLE, Action::PREVIOUS_ROW,
                                Action::PREVIOUS_ROW_CYCLE})
    {
        PolicyTable table(0);
        TableRowTraversal::perform(table.view, action);
        EXPECT_EQ(TableRowTraversal::selectedRow(table.view), -1);
        EXPECT_FALSE(table.view.currentIndex().isValid());
    }
    PolicyTable noColumns(3, 0);
    TableRowTraversal::perform(noColumns.view, Action::NEXT_ROW_CYCLE);
    EXPECT_EQ(TableRowTraversal::selectedRow(noColumns.view), -1);
}

TEST_F(TableRowTraversalTest, ATableWithoutAModelIsLeftAlone)
{
    QTableView view;
    TableRowTraversal::install(view);
    EXPECT_EQ(TableRowTraversal::selectedRow(view), -1);
    TableRowTraversal::perform(view, Action::NEXT_ROW_CYCLE);
    EXPECT_EQ(view.model(), nullptr);
}

TEST_F(TableRowTraversalTest, InstallingTwiceInstallsOneFilter)
{
    PolicyTable table;
    TableRowTraversal::install(table.view);
    EXPECT_EQ(
        table.view.findChildren<TableRowTraversal*>(QString(), Qt::FindDirectChildrenOnly).size(),
        1);
    // One Tab, one row.
    table.selectCell(0, 0);
    QTest::keyClick(&table.view, Qt::Key_Tab);
    EXPECT_EQ(TableRowTraversal::selectedRow(table.view), 1);
}

// ------------------------------------------------------------------------- the keys, typed

TEST_F(TableRowTraversalTest, TheKeysMoveTheRowsOfTheTable)
{
    QWidget window;
    auto*   model = makeChild<QStandardItemModel>(&window, 3, 2);
    auto*   view  = makeChild<QTableView>(&window);
    view->setModel(model);
    TableRowTraversal::install(*view);
    ASSERT_TRUE(showActiveWith(window, {view}));
    view->setFocus();
    view->selectionModel()->setCurrentIndex(model->index(0, 1),
                                            QItemSelectionModel::ClearAndSelect);

    QTest::keyClick(view, Qt::Key_Right);
    EXPECT_EQ(view->currentIndex(), model->index(1, 0));
    QTest::keyClick(view, Qt::Key_Tab);
    EXPECT_EQ(view->currentIndex(), model->index(2, 0));
    QTest::keyClick(view, Qt::Key_Right);  // the last row: taken, nothing happens
    EXPECT_EQ(view->currentIndex(), model->index(2, 0));
    QTest::keyClick(view, Qt::Key_Tab);  // cycles
    EXPECT_EQ(view->currentIndex(), model->index(0, 0));
    QTest::keyClick(view, Qt::Key_Backtab, Qt::ShiftModifier);
    EXPECT_EQ(view->currentIndex(), model->index(2, 0));
    QTest::keyClick(view, Qt::Key_Left);
    EXPECT_EQ(view->currentIndex(), model->index(1, 0));
    QTest::keyClick(view, Qt::Key_Tab, Qt::ShiftModifier);
    EXPECT_EQ(view->currentIndex(), model->index(0, 0));
    QTest::keyClick(view, Qt::Key_Left);  // the first row: taken, nothing happens
    EXPECT_EQ(view->currentIndex(), model->index(0, 0));
    EXPECT_TRUE(view->hasFocus());

    // The other keys are the table's own: Down moves down, Shift+Right extends to the next cell.
    QTest::keyClick(view, Qt::Key_Down);
    EXPECT_EQ(view->currentIndex(), model->index(1, 0));
    QTest::keyClick(view, Qt::Key_Right, Qt::ShiftModifier);
    EXPECT_EQ(view->currentIndex(), model->index(1, 1));
}

TEST_F(TableRowTraversalTest, CtrlTabLeavesTheTable)
{
    QWidget window;
    auto*   before = makeChild<QPushButton>(&window, QStringLiteral("before"));
    auto*   model  = makeChild<QStandardItemModel>(&window, 3, 2);
    auto*   view   = makeChild<QTableView>(&window);
    auto*   after  = makeChild<QLineEdit>(&window);
    before->setFocusPolicy(Qt::StrongFocus);
    view->setModel(model);
    TableRowTraversal::install(*view);
    ASSERT_TRUE(showActiveWith(window, {before, view, after}));
    view->setFocus();
    view->setCurrentIndex(model->index(1, 1));

    QTest::keyClick(view, Qt::Key_Tab, Qt::ControlModifier);
    EXPECT_TRUE(after->hasFocus());
    EXPECT_EQ(view->currentIndex(), model->index(1, 1));  // the table did not move
    EXPECT_TRUE(view->tabKeyNavigation());

    view->setFocus();
    QTest::keyClick(view, Qt::Key_Backtab, Qt::ControlModifier | Qt::ShiftModifier);
    EXPECT_TRUE(before->hasFocus());
    EXPECT_EQ(view->currentIndex(), model->index(1, 1));

    view->setFocus();
    QTest::keyClick(view, Qt::Key_Tab, Qt::MetaModifier);
    EXPECT_TRUE(after->hasFocus());
}

TEST_F(TableRowTraversalTest, TheKeysComeBeforeTheWindowsShortcuts)
{
    // Java: the table's WHEN_FOCUSED bindings come before the window's.
    QWidget window;
    auto*   model = makeChild<QStandardItemModel>(&window, 3, 2);
    auto*   view  = makeChild<QTableView>(&window);
    auto*   right = makeChild<QAction>(&window);
    right->setShortcut(QKeySequence(Qt::Key_Right));
    right->setShortcutContext(Qt::WindowShortcut);
    window.addAction(right);
    const QSignalSpy triggered(right, &QAction::triggered);
    view->setModel(model);
    TableRowTraversal::install(*view);
    ASSERT_TRUE(showActiveWith(window, {view}));
    view->setFocus();
    view->setCurrentIndex(model->index(0, 0));
    QTest::keyClick(view, Qt::Key_Right);
    EXPECT_EQ(view->currentIndex(), model->index(1, 0));
    EXPECT_EQ(triggered.count(), 0);
    // Without the table's focus the window's shortcut fires.
    window.setFocus();
    QTest::keyClick(&window, Qt::Key_Right);
    EXPECT_EQ(triggered.count(), 1);
}

TEST_F(TableRowTraversalTest, TheTableScrollsToTheNewRow)
{
    // Deviation (TableRowTraversal.h); probe, "scrolling: 100 rows, viewport 200 px high": after
    // 20 x NextRow from row 5, Java's view is still at the top.
    QWidget window;
    auto*   model = makeChild<QStandardItemModel>(&window, 100, 2);
    auto*   view  = makeChild<QTableView>(&window);
    view->setModel(model);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setSelectionMode(QAbstractItemView::SingleSelection);
    view->setFixedHeight(200);
    TableRowTraversal::install(*view);
    ASSERT_TRUE(showActiveWith(window, {view}));
    view->setCurrentIndex(model->index(5, 0));
    for (int step = 0; step < 20; ++step)
    {
        TableRowTraversal::perform(*view, Action::NEXT_ROW);
    }
    EXPECT_EQ(TableRowTraversal::selectedRow(*view), 25);
    const QRect row = view->visualRect(model->index(25, 0));
    EXPECT_TRUE(view->viewport()->rect().contains(row)) << row.y();
    TableRowTraversal::perform(*view, Action::PREVIOUS_ROW_CYCLE);
    view->setCurrentIndex(model->index(0, 0));
    TableRowTraversal::perform(*view, Action::PREVIOUS_ROW_CYCLE);
    EXPECT_EQ(TableRowTraversal::selectedRow(*view), 99);
    EXPECT_TRUE(view->viewport()->rect().contains(view->visualRect(model->index(99, 0))));
}

}  // namespace

}  // namespace QtRocket::Gui
