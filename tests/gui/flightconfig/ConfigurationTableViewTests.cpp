// ConfigurationTableView: a QTableView with a RowColumnSelectionModel that selects with the mouse
// and the keys as a JTable with cell selection does, and tells its changes as a JTable's two
// selection models do: the steps of a gesture of the mouse adjusting, its end once more, the rows
// first. The expectations are those of the M3 run 2 probes GestureProbe and GestureProbe2 (JDK 17,
// BasicTableUI's own mouse listeners and actions under xvfb, a DefaultTableModel of 6 rows and 3
// columns with cell selection and MULTIPLE_INTERVAL_SELECTION).

#include "QtRocket/gui/flightconfig/ConfigurationTableView.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <QAbstractItemView>
#include <QCoreApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QString>
#include <QTest>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/flightconfig/RowColumnSelectionModel.h"
#include "QtRocket/gui/util/MakeChild.h"

namespace QtRocket::Gui
{

namespace
{

using ConfigurationTableViewTest = Gui::Test::GuiTest;
using Lines                      = RowColumnSelectionModel::Lines;
using LinesChange                = RowColumnSelectionModel::LinesChange;
using Order                      = RowColumnSelectionModel::Order;

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

/// "rows=[..] cols=[..]" of @p lines, as the probes print a JTable's selection.
[[nodiscard]] std::string text(const Lines& lines)
{
    return "rows=" + listOf(lines.rows) + " cols=" + listOf(lines.columns);
}

/// "adjusting rows=[..] cols=[..] -> rows=[..] cols=[..]" (or "final", and "rows first") of
/// @p change.
[[nodiscard]] std::string text(const LinesChange& change)
{
    return std::string(change.adjusting ? "adjusting " : "final ") +
           (change.order == Order::ROWS_FIRST ? "rows first " : "columns first ") +
           text(change.before) + " -> " + text(change.after);
}

/// The probes' table: 6 rows and 3 columns in a ConfigurationTableView with a
/// RowColumnSelectionModel, shown offscreen; every change recorded.
class Table
{
public:
    Table() : m_selection(makeChild<RowColumnSelectionModel>(m_view.get(), m_model))
    {
        m_view->setModel(m_model);
        m_view->setSelectionModel(m_selection);
        m_view->setSelectionBehavior(QAbstractItemView::SelectItems);
        m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
        m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
        QObject::connect(m_selection, &RowColumnSelectionModel::linesChanged, m_view.get(),
                         [this](const LinesChange& change) { m_changes.push_back(change); });
        m_view->resize(500, 300);
        m_view->show();
        QCoreApplication::processEvents();
    }

    [[nodiscard]] RowColumnSelectionModel&  selection() const noexcept { return *m_selection; }
    [[nodiscard]] ConfigurationTableView&   view() const noexcept { return *m_view; }
    [[nodiscard]] QStandardItemModel&       model() const noexcept { return *m_model; }
    [[nodiscard]] QWidget&                  viewport() const { return *m_view->viewport(); }
    [[nodiscard]] std::vector<LinesChange>& changes() noexcept { return m_changes; }
    [[nodiscard]] std::string               state() const { return text(m_selection->lines()); }

    /// The changes since the last call, as text, the adjusting ones too.
    [[nodiscard]] std::vector<std::string> takeChanges()
    {
        std::vector<std::string> texts;
        texts.reserve(m_changes.size());
        for (const LinesChange& change : m_changes)
        {
            texts.push_back(text(change));
        }
        m_changes.clear();
        return texts;
    }

    /// The centre of the cell (@p row, @p column) in the viewport.
    [[nodiscard]] QPoint center(int row, int column) const
    {
        return m_view->visualRect(m_model->index(row, column)).center();
    }

    /// A click of the left button on the cell (@p row, @p column); the state after it.
    [[nodiscard]] std::string click(int row, int column, Qt::KeyboardModifiers modifiers = {}) const
    {
        QTest::mouseClick(&viewport(), Qt::LeftButton, modifiers, center(row, column));
        return state();
    }

    /// A press of the left button on (@p row, @p column).
    void press(int row, int column, Qt::KeyboardModifiers modifiers = {}) const
    {
        QTest::mousePress(&viewport(), Qt::LeftButton, modifiers, center(row, column));
    }

    /// A move to (@p row, @p column) with the left button held; the state after it.
    [[nodiscard]] std::string dragTo(int row, int column,
                                     Qt::KeyboardModifiers modifiers = {}) const
    {
        const QPoint position = center(row, column);
        QMouseEvent  move(QEvent::MouseMove, QPointF(position), viewport().mapToGlobal(position),
                          Qt::NoButton, Qt::LeftButton, modifiers);
        QCoreApplication::sendEvent(&viewport(), &move);
        return state();
    }

    /// The release of the left button on (@p row, @p column); the state after it.
    [[nodiscard]] std::string release(int row, int column,
                                      Qt::KeyboardModifiers modifiers = {}) const
    {
        QTest::mouseRelease(&viewport(), Qt::LeftButton, modifiers, center(row, column));
        return state();
    }

    /// A key; the state after it.
    [[nodiscard]] std::string key(Qt::Key key, Qt::KeyboardModifiers modifiers = {}) const
    {
        QTest::keyClick(m_view.get(), key, modifiers);
        return state();
    }

private:
    std::unique_ptr<ConfigurationTableView> m_view = std::make_unique<ConfigurationTableView>();
    QStandardItemModel*      m_model = makeChild<QStandardItemModel>(m_view.get(), 6, 3);
    RowColumnSelectionModel* m_selection;
    std::vector<LinesChange> m_changes;
};

constexpr Qt::KeyboardModifiers kCtrl  = Qt::ControlModifier;
constexpr Qt::KeyboardModifiers kShift = Qt::ShiftModifier;

// ==================================================================== the gestures' changes

TEST_F(ConfigurationTableViewTest, AClickTellsItsStepThenTheRowsAndTheColumns)
{
    Table table;
    // GestureProbe "CLICK (2,1)" from nothing: COL adj=true, ROW adj=true, then at the release
    // ROW adj=false, COL adj=false.
    EXPECT_EQ(table.click(2, 1), "rows=[2] cols=[1]");
    EXPECT_EQ(
        table.takeChanges(),
        (std::vector<std::string>{"adjusting columns first rows=[] cols=[] -> rows=[2] cols=[1]",
                                  "final rows first rows=[] cols=[] -> rows=[2] cols=[1]"}));
    EXPECT_FALSE(table.selection().valueIsAdjusting());
}

TEST_F(ConfigurationTableViewTest, AClickInTheSameColumnChangesTheRowsAlone)
{
    Table table;
    static_cast<void>(table.click(2, 1));
    table.changes().clear();
    // "CLICK same column (4,1)": ROW events only; "CLICK same cell": none; "CLICK same row
    // (4,2)": COL events only.
    EXPECT_EQ(table.click(4, 1), "rows=[4] cols=[1]");
    ASSERT_EQ(table.changes().size(), 2U);
    EXPECT_FALSE(table.changes()[1].adjusting);
    EXPECT_EQ(table.changes()[1].before.columns, table.changes()[1].after.columns);
    table.changes().clear();
    EXPECT_EQ(table.click(4, 1), "rows=[4] cols=[1]");
    EXPECT_TRUE(table.changes().empty());
    EXPECT_EQ(table.click(4, 2), "rows=[4] cols=[2]");
    ASSERT_EQ(table.changes().size(), 2U);
    EXPECT_EQ(table.changes()[1].before.rows, table.changes()[1].after.rows);
}

TEST_F(ConfigurationTableViewTest, ADragTellsItsWholeChangeOnceAtTheRelease)
{
    Table table;
    static_cast<void>(table.click(0, 2));
    table.changes().clear();
    // The reviewer's DragProbe: from rows=[0] cols=[2], a press on (2,1) and a drag to (4,1):
    // adjusting events, then at the release ROW (count 3) and COL.
    table.press(2, 1);
    EXPECT_EQ(table.dragTo(4, 1), "rows=[2, 3, 4] cols=[1]");
    EXPECT_EQ(table.release(4, 1), "rows=[2, 3, 4] cols=[1]");
    const std::vector<std::string> changes = table.takeChanges();
    ASSERT_FALSE(changes.empty());
    for (std::size_t i = 0; i + 1 < changes.size(); ++i)
    {
        EXPECT_EQ(changes[i].rfind("adjusting ", 0), 0U) << changes[i];
    }
    EXPECT_EQ(changes.back(), "final rows first rows=[0] cols=[2] -> rows=[2, 3, 4] cols=[1]");
}

TEST_F(ConfigurationTableViewTest, TheReleaseIsToldAfterTheGestureEnded)
{
    Table            table;
    const QSignalSpy released(&table.view(), &ConfigurationTableView::leftButtonReleased);
    bool             adjustingAtTheSignal = true;
    std::size_t      changesAtTheSignal   = 0;
    QObject::connect(&table.view(), &ConfigurationTableView::leftButtonReleased, &table.view(),
                     [&table, &adjustingAtTheSignal, &changesAtTheSignal] {
                         adjustingAtTheSignal = table.selection().valueIsAdjusting();
                         changesAtTheSignal   = table.changes().size();
                     });
    static_cast<void>(table.click(3, 2));
    ASSERT_EQ(released.count(), 1);
    EXPECT_EQ(released.at(0).at(0).toPoint(), table.center(3, 2));
    EXPECT_FALSE(adjustingAtTheSignal);
    EXPECT_EQ(changesAtTheSignal, 2U);
    // The right button starts no gesture and tells no release.
    QTest::mouseClick(&table.viewport(), Qt::RightButton, {}, table.center(1, 1));
    EXPECT_EQ(released.count(), 1);
    EXPECT_FALSE(table.selection().valueIsAdjusting());
}

// ======================================================================= the probes' clicks

TEST_F(ConfigurationTableViewTest, ClicksWithShiftAndCtrlSelectAsAJTables)
{
    const Table table;
    // GestureProbe, in order.
    EXPECT_EQ(table.click(2, 1), "rows=[2] cols=[1]");
    EXPECT_EQ(table.click(4, 1), "rows=[4] cols=[1]");
    EXPECT_EQ(table.click(4, 1), "rows=[4] cols=[1]");
    EXPECT_EQ(table.click(4, 2), "rows=[4] cols=[2]");
    EXPECT_EQ(table.click(1, 1, kCtrl), "rows=[1, 4] cols=[1, 2]");
    // Shift: the rows and the columns from the anchor (1,1) alone.
    EXPECT_EQ(table.click(3, 2, kShift), "rows=[1, 2, 3] cols=[1, 2]");
    EXPECT_EQ(table.click(5, 0, kShift), "rows=[1, 2, 3, 4, 5] cols=[0, 1]");
    // Ctrl on a selected cell takes its row and its column away; the anchor moves there.
    EXPECT_EQ(table.click(1, 1, kCtrl), "rows=[2, 3, 4, 5] cols=[0]");
    EXPECT_EQ(table.click(0, 0, kShift), "rows=[0, 1] cols=[0, 1]");
    // Ctrl+Shift adds the anchor's rectangle to the selection (the anchor is selected).
    EXPECT_EQ(table.click(2, 2, kCtrl | kShift), "rows=[0, 1, 2] cols=[0, 1, 2]");
    EXPECT_EQ(table.click(1, 1), "rows=[1] cols=[1]");
}

TEST_F(ConfigurationTableViewTest, AShiftClickDropsWhatCtrlAddedBefore)
{
    const Table table;
    // The reviewer's ClickProbe: (2,1), Ctrl+click (4,2), Shift+click (3,2) gives the rows from
    // the anchor (4,2) to the cell and its column alone.
    EXPECT_EQ(table.click(2, 1), "rows=[2] cols=[1]");
    EXPECT_EQ(table.click(4, 2, kCtrl), "rows=[2, 4] cols=[1, 2]");
    EXPECT_EQ(table.click(3, 2, kShift), "rows=[3, 4] cols=[2]");
}

TEST_F(ConfigurationTableViewTest, CtrlAndShiftWithTheAnchorUnselectedTakeTheRectangleAway)
{
    const Table table;
    // GestureProbe2.
    EXPECT_EQ(table.click(1, 1), "rows=[1] cols=[1]");
    table.press(3, 2, kCtrl);
    EXPECT_EQ(table.dragTo(5, 2, kCtrl), "rows=[1, 3, 4, 5] cols=[1, 2]");
    EXPECT_EQ(table.release(5, 2, kCtrl), "rows=[1, 3, 4, 5] cols=[1, 2]");
    EXPECT_EQ(table.click(4, 2, kCtrl), "rows=[1, 3, 5] cols=[1]");
    EXPECT_EQ(table.click(0, 0, kCtrl | kShift), "rows=[5] cols=[]");
}

// ======================================================================== the probes' drags

TEST_F(ConfigurationTableViewTest, DragsSelectFromTheAnchorAsAJTables)
{
    const Table table;
    // GestureProbe.
    EXPECT_EQ(table.click(1, 1), "rows=[1] cols=[1]");
    table.press(2, 1);
    EXPECT_EQ(table.dragTo(4, 2), "rows=[2, 3, 4] cols=[1, 2]");
    EXPECT_EQ(table.release(4, 2), "rows=[2, 3, 4] cols=[1, 2]");
    // A press on a selected cell without a move selects it alone.
    table.press(3, 2);
    EXPECT_EQ(table.dragTo(3, 2), "rows=[3] cols=[2]");
    EXPECT_EQ(table.release(3, 2), "rows=[3] cols=[2]");
    // Shift: from the anchor (3,2), each step replacing the one before.
    table.press(0, 1, kShift);
    EXPECT_EQ(table.state(), "rows=[0, 1, 2, 3] cols=[1, 2]");
    EXPECT_EQ(table.dragTo(1, 1, kShift), "rows=[1, 2, 3] cols=[1, 2]");
    EXPECT_EQ(table.release(1, 1, kShift), "rows=[1, 2, 3] cols=[1, 2]");
    EXPECT_EQ(table.click(1, 1), "rows=[1] cols=[1]");
    // Ctrl: added to the selection.
    table.press(3, 2, kCtrl);
    EXPECT_EQ(table.dragTo(4, 2, kCtrl), "rows=[1, 3, 4] cols=[1, 2]");
    EXPECT_EQ(table.release(4, 2, kCtrl), "rows=[1, 3, 4] cols=[1, 2]");
}

TEST_F(ConfigurationTableViewTest, ADragThatComesBackReplacesButACtrlDragKeeps)
{
    const Table table;
    // GestureProbe2.
    EXPECT_EQ(table.click(1, 1), "rows=[1] cols=[1]");
    table.press(3, 2, kCtrl);
    EXPECT_EQ(table.dragTo(5, 2, kCtrl), "rows=[1, 3, 4, 5] cols=[1, 2]");
    EXPECT_EQ(table.dragTo(4, 2, kCtrl), "rows=[1, 3, 4, 5] cols=[1, 2]");
    EXPECT_EQ(table.release(4, 2, kCtrl), "rows=[1, 3, 4, 5] cols=[1, 2]");
    EXPECT_EQ(table.click(1, 1), "rows=[1] cols=[1]");
    table.press(1, 1);
    EXPECT_EQ(table.dragTo(4, 2), "rows=[1, 2, 3, 4] cols=[1, 2]");
    EXPECT_EQ(table.dragTo(2, 1), "rows=[1, 2] cols=[1]");
    EXPECT_EQ(table.release(2, 1), "rows=[1, 2] cols=[1]");
    EXPECT_EQ(table.click(5, 2, kCtrl), "rows=[1, 2, 5] cols=[1, 2]");
    table.press(0, 0, kShift);
    EXPECT_EQ(table.state(), "rows=[0, 1, 2, 3, 4, 5] cols=[0, 1, 2]");
    EXPECT_EQ(table.dragTo(3, 1, kShift), "rows=[3, 4, 5] cols=[1, 2]");
    EXPECT_EQ(table.release(3, 1, kShift), "rows=[3, 4, 5] cols=[1, 2]");
}

// ============================================================================== the keys

TEST_F(ConfigurationTableViewTest, TheKeysSelectAsAJTables)
{
    Table table;
    // GestureProbe: the arrows' changes are not adjusting; Select All is adjusting, then once
    // the rows and the columns.
    EXPECT_EQ(table.click(2, 1), "rows=[2] cols=[1]");
    table.changes().clear();
    EXPECT_EQ(table.key(Qt::Key_Down), "rows=[3] cols=[1]");
    EXPECT_EQ(table.key(Qt::Key_Down, kShift), "rows=[3, 4] cols=[1]");
    EXPECT_EQ(table.key(Qt::Key_Right, kShift), "rows=[3, 4] cols=[1, 2]");
    EXPECT_EQ(table.takeChanges(),
              (std::vector<std::string>{
                  "final columns first rows=[2] cols=[1] -> rows=[3] cols=[1]",
                  "final columns first rows=[3] cols=[1] -> rows=[3, 4] cols=[1]",
                  "final columns first rows=[3, 4] cols=[1] -> rows=[3, 4] cols=[1, 2]"}));
    EXPECT_EQ(table.key(Qt::Key_A, kCtrl), "rows=[0, 1, 2, 3, 4, 5] cols=[0, 1, 2]");
    const std::vector<std::string> changes = table.takeChanges();
    ASSERT_FALSE(changes.empty());
    EXPECT_EQ(changes.back(),
              "final rows first rows=[3, 4] cols=[1, 2] -> rows=[0, 1, 2, 3, 4, 5] cols=[0, 1, 2]");
    EXPECT_FALSE(table.selection().valueIsAdjusting());
}

TEST_F(ConfigurationTableViewTest, ATypedKeySelectsNothing)
{
    Table table;
    for (int row = 0; row < 6; ++row)
    {
        table.model().setData(table.model().index(row, 1), QStringLiteral("C%1").arg(row));
    }
    EXPECT_EQ(table.click(2, 1), "rows=[2] cols=[1]");
    table.changes().clear();
    // A JTable has no type-ahead search; Qt's would select row 0 ("C0").
    QTest::keyClicks(&table.view(), QStringLiteral("C"));
    EXPECT_EQ(table.state(), "rows=[2] cols=[1]");
    EXPECT_TRUE(table.changes().empty());
}

}  // namespace

}  // namespace QtRocket::Gui
