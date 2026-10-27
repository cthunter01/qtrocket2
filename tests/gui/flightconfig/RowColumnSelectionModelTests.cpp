// RowColumnSelectionModel: a QTableView selecting cells as a JTable with cell selection does (the
// rows and the columns kept apart, the cells their cross product), driven by Qt's mouse and keys
// and by its own calls, with its notifications. The expectations of the JTable cases are the M3
// run 2 probe JTableSelectionProbe's (JDK 17, headless: JTable.changeSelection() with the
// arguments BasicTableUI gives a click, a Ctrl+click and a Shift+click).

#include "QtRocket/gui/flightconfig/RowColumnSelectionModel.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <QAbstractItemView>
#include <QCoreApplication>
#include <QEvent>
#include <QItemSelectionModel>
#include <QMouseEvent>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QStandardItemModel>
#include <QTableView>
#include <QTest>
#include <QWidget>
#include <Qt>
#include <QtTypes>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/MakeChild.h"

namespace QtRocket::Gui
{

namespace
{

using RowColumnSelectionModelTest = Gui::Test::GuiTest;
using Lines                       = RowColumnSelectionModel::Lines;
using Order                       = RowColumnSelectionModel::Order;

/// "[0, 1]" of @p values.
[[nodiscard]] std::string listOf(const std::vector<int>& values)
{
    std::string text = "[";
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        text += (i > 0 ? ", " : "") + std::to_string(values[i]);
    }
    return text + "]";
}

/// "rows=[..] cols=[..]" of @p lines, as the probe prints a JTable's selection.
[[nodiscard]] std::string text(const Lines& lines)
{
    return "rows=" + listOf(lines.rows) + " cols=" + listOf(lines.columns);
}

/// A move of the mouse to @p position of @p viewport with the left button held.
void dragTo(QWidget& viewport, const QPoint& position)
{
    QMouseEvent move(QEvent::MouseMove, QPointF(position), viewport.mapToGlobal(position),
                     Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&viewport, &move);
}

/// A table of 5 rows and 4 columns with the selection, shown offscreen; every notification
/// recorded.
class Table
{
public:
    Table() : m_selection(makeChild<RowColumnSelectionModel>(m_view.get(), m_model))
    {
        m_view->setModel(m_model);
        m_view->setSelectionModel(m_selection);
        m_view->setSelectionBehavior(QAbstractItemView::SelectItems);
        m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
        QObject::connect(m_selection, &RowColumnSelectionModel::linesChanged, m_view.get(),
                         [this](const RowColumnSelectionModel::LinesChange& change) {
                             m_changes.push_back(change);
                         });
        m_view->resize(500, 300);
        m_view->show();
        QCoreApplication::processEvents();
    }

    [[nodiscard]] RowColumnSelectionModel& selection() const noexcept { return *m_selection; }
    [[nodiscard]] QTableView&              view() const noexcept { return *m_view; }
    [[nodiscard]] QStandardItemModel&      model() const noexcept { return *m_model; }
    [[nodiscard]] std::vector<RowColumnSelectionModel::LinesChange>& changes() noexcept
    {
        return m_changes;
    }
    [[nodiscard]] std::string state() const { return text(m_selection->lines()); }

    /// The centre of the cell (@p row, @p column) in the viewport.
    [[nodiscard]] QPoint center(int row, int column) const
    {
        return m_view->visualRect(m_model->index(row, column)).center();
    }

    /// A click of the left button on the cell (@p row, @p column).
    void click(int row, int column, Qt::KeyboardModifiers modifiers = {}) const
    {
        QTest::mouseClick(m_view->viewport(), Qt::LeftButton, modifiers, center(row, column));
    }

    /// The number of the view's selected cells.
    [[nodiscard]] qsizetype cellCount() const { return m_selection->selectedIndexes().size(); }

private:
    std::unique_ptr<QTableView> m_view  = std::make_unique<QTableView>();
    QStandardItemModel*         m_model = makeChild<QStandardItemModel>(m_view.get(), 5, 4);
    RowColumnSelectionModel*    m_selection;
    std::vector<RowColumnSelectionModel::LinesChange> m_changes;
};

// ================================================================== the mouse (the probe's)

TEST_F(RowColumnSelectionModelTest, AClickSelectsTheCellAlone)
{
    Table table;
    table.click(1, 1);
    // changeSelection(1,1,false,false) -> rows=[1] cols=[1]
    EXPECT_EQ(table.state(), "rows=[1] cols=[1]");
    EXPECT_EQ(table.cellCount(), 1);
    EXPECT_TRUE(table.selection().isCellSelected(1, 1));
    EXPECT_EQ(table.selection().firstRow(), 1);
    EXPECT_EQ(table.selection().firstColumn(), 1);
    ASSERT_EQ(table.changes().size(), 1U);
    EXPECT_EQ(text(table.changes()[0].before), "rows=[] cols=[]");
    EXPECT_EQ(text(table.changes()[0].after), "rows=[1] cols=[1]");
    EXPECT_EQ(table.changes()[0].order, Order::COLUMNS_FIRST);
}

TEST_F(RowColumnSelectionModelTest, ACtrlClickAddsTheRowAndTheColumnOfTheCell)
{
    const Table table;
    table.click(1, 1);
    table.click(2, 2, Qt::ControlModifier);
    // changeSelection(2,2,true,false) [ctrl add] -> rows=[1, 2] cols=[1, 2]: four cells.
    EXPECT_EQ(table.state(), "rows=[1, 2] cols=[1, 2]");
    EXPECT_EQ(table.cellCount(), 4);
    EXPECT_TRUE(table.selection().isCellSelected(1, 2));
    EXPECT_TRUE(table.selection().isCellSelected(2, 1));
}

TEST_F(RowColumnSelectionModelTest, ACtrlClickOnASelectedCellRemovesItsRowAndItsColumn)
{
    const Table table;
    table.click(1, 1);
    table.click(2, 2, Qt::ControlModifier);
    table.click(1, 2, Qt::ControlModifier);
    // changeSelection(1,2,true,false) [ctrl on selected cell] -> rows=[2] cols=[1]
    EXPECT_EQ(table.state(), "rows=[2] cols=[1]");
    EXPECT_EQ(table.cellCount(), 1);
}

TEST_F(RowColumnSelectionModelTest, ACtrlClickOnTheLastCellOfAColumnLeavesItsRowsSelected)
{
    const Table table;
    table.click(1, 1);
    table.click(2, 1, Qt::ShiftModifier);
    ASSERT_EQ(table.state(), "rows=[1, 2] cols=[1]");
    table.click(2, 1, Qt::ControlModifier);
    // changeSelection(2,1,true,false) [ctrl on selected (2,1) in {1,2}x{1}] -> rows=[1] cols=[]
    EXPECT_EQ(table.state(), "rows=[1] cols=[]");
    EXPECT_EQ(table.cellCount(), 0);
    EXPECT_EQ(table.selection().firstRow(), 1);
    EXPECT_EQ(table.selection().firstColumn(), -1);
}

TEST_F(RowColumnSelectionModelTest, AShiftClickSelectsTheRowsAndColumnsFromTheLastClick)
{
    const Table table;
    table.click(0, 1);
    table.click(2, 3, Qt::ShiftModifier);
    // changeSelection(2,3,false,true) [shift] -> rows=[0, 1, 2] cols=[1, 2, 3]
    EXPECT_EQ(table.state(), "rows=[0, 1, 2] cols=[1, 2, 3]");
    EXPECT_EQ(table.cellCount(), 9);

    // A click narrows it to one cell again (changeSelection(1,1,false,false) [narrow to a
    // selected row] -> rows=[1] cols=[1]).
    table.click(1, 1);
    EXPECT_EQ(table.state(), "rows=[1] cols=[1]");
}

TEST_F(RowColumnSelectionModelTest, ADragSelectsTheRectangleItCrosses)
{
    const Table table;
    QWidget&    viewport = *table.view().viewport();
    QTest::mousePress(&viewport, Qt::LeftButton, {}, table.center(3, 2));
    dragTo(viewport, table.center(1, 3));
    EXPECT_EQ(table.state(), "rows=[1, 2, 3] cols=[2, 3]");
    // Each step of the drag replaces the one before.
    dragTo(viewport, table.center(2, 0));
    EXPECT_EQ(table.state(), "rows=[2, 3] cols=[0, 1, 2]");
    QTest::mouseRelease(&viewport, Qt::LeftButton, {}, table.center(2, 0));
    EXPECT_EQ(table.state(), "rows=[2, 3] cols=[0, 1, 2]");
    EXPECT_EQ(table.cellCount(), 6);
}

// =========================================================================== the keys

TEST_F(RowColumnSelectionModelTest, TheArrowKeysMoveAndShiftExtends)
{
    const Table table;
    table.click(1, 1);
    QTest::keyClick(&table.view(), Qt::Key_Down);
    EXPECT_EQ(table.state(), "rows=[2] cols=[1]");
    QTest::keyClick(&table.view(), Qt::Key_Right, Qt::ShiftModifier);
    EXPECT_EQ(table.state(), "rows=[2] cols=[1, 2]");
    QTest::keyClick(&table.view(), Qt::Key_Down, Qt::ShiftModifier);
    EXPECT_EQ(table.state(), "rows=[2, 3] cols=[1, 2]");
    QTest::keyClick(&table.view(), Qt::Key_A, Qt::ControlModifier);
    EXPECT_EQ(table.state(), "rows=[0, 1, 2, 3, 4] cols=[0, 1, 2, 3]");
    EXPECT_EQ(table.cellCount(), 20);
}

// ======================================================================== its own calls

TEST_F(RowColumnSelectionModelTest, ItsCallsSelectAndTellInJavasOrder)
{
    Table table;
    table.selection().selectCell(3, 2);
    EXPECT_EQ(table.state(), "rows=[3] cols=[2]");
    EXPECT_EQ(table.selection().currentIndex(), table.model().index(3, 2));
    table.selection().addCell(1, 0);
    EXPECT_EQ(table.state(), "rows=[1, 3] cols=[0, 2]");
    EXPECT_EQ(table.cellCount(), 4);
    table.selection().setLines(Lines{.rows = {4, 0, 4, 9}, .columns = {3, -1}}, Order::ROWS_FIRST);
    // Only the model's rows and columns, in order, each once.
    EXPECT_EQ(table.state(), "rows=[0, 4] cols=[3]");
    table.selection().clearLines();
    EXPECT_EQ(table.state(), "rows=[] cols=[]");
    EXPECT_EQ(table.cellCount(), 0);

    ASSERT_EQ(table.changes().size(), 4U);
    EXPECT_EQ(table.changes()[0].order, Order::COLUMNS_FIRST);
    EXPECT_EQ(table.changes()[1].order, Order::COLUMNS_FIRST);
    EXPECT_EQ(text(table.changes()[1].before), "rows=[3] cols=[2]");
    EXPECT_EQ(table.changes()[2].order, Order::ROWS_FIRST);
    // JTable.clearSelection(): the rows first.
    EXPECT_EQ(table.changes()[3].order, Order::ROWS_FIRST);
    EXPECT_EQ(text(table.changes()[3].after), "rows=[] cols=[]");

    // A selection that does not change tells nothing.
    table.selection().clearLines();
    EXPECT_EQ(table.changes().size(), 4U);
}

TEST_F(RowColumnSelectionModelTest, AResetOfTheModelClearsWithoutASignal)
{
    Table table;
    table.click(1, 1);
    table.click(2, 2, Qt::ControlModifier);
    table.changes().clear();
    table.model().setRowCount(5);  // no change of the structure
    EXPECT_EQ(table.state(), "rows=[1, 2] cols=[1, 2]");
    table.model().clear();
    table.model().setRowCount(3);
    table.model().setColumnCount(3);
    EXPECT_EQ(table.state(), "rows=[] cols=[]");
    EXPECT_EQ(table.cellCount(), 0);
    // QStandardItemModel::clear() resets the model: no signal.
    EXPECT_TRUE(table.changes().empty());
}

}  // namespace

}  // namespace QtRocket::Gui
