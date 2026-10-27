#pragma once

#include <optional>

#include <QObject>

class QEvent;
class QKeyEvent;
class QTableView;

namespace QtRocket::Gui
{

/// Tab, Shift+Tab, Right and Left move a table's selection from row to row instead of from cell
/// to cell (OpenRocket's TableRowTraversalPolicy, swing utils/TableRowTraversalPolicy.java, on the
/// parts library's table and on the simulations table).
///
/// The keys and their actions (Java: the table's WHEN_FOCUSED input map and its action map):
/// Tab is NEXT_ROW_CYCLE, Shift+Tab (Qt: Backtab, or Tab with Shift) PREVIOUS_ROW_CYCLE, Right
/// NEXT_ROW and Left PREVIOUS_ROW, each without another modifier (actionFor()). An action
/// (perform()) takes the selected row (selectedRow(): the lowest selected row, Java's
/// JTable.getSelectedRow(), -1 for none) and selects the row after it or before it; past the last
/// row the cycling action goes on at the first row and the other does nothing, before the first
/// row the cycling action goes on at the last row and the other does nothing. The new selection
/// is that row alone and, as Java sets the column selection to view column 0, the current index is
/// in the first column the table shows in its header's order: a table that selects rows selects
/// the whole row (QItemSelectionModel::Rows), one that selects columns the whole column, one that
/// selects items that cell. So with no selection, Right and Tab select the first row, Left does
/// nothing and Shift+Tab selects the last row (probe). The keys do nothing else: Right on the
/// last row is taken and does nothing, as Java's bound action does.
///
/// Like Java's WHEN_FOCUSED bindings, which come before the window's, the keys are the table's
/// while it has the focus: a shortcut of the window on one of them does not fire (the filter
/// accepts their QEvent::ShortcutOverride). A cell editor that has the focus keeps its keys.
///
/// Ctrl+Tab and Ctrl+Shift+Tab move the focus out of the table, to the next or the previous
/// widget in the focus chain, as JTable's focus traversal keys do (probe: forward [ctrl pressed
/// TAB], backward [shift ctrl pressed TAB]); Qt's table would move to the next cell. The Meta key
/// counts as Ctrl there, as Qt gives macOS's Control key as Qt::MetaModifier.
///
/// Deviations from OpenRocket:
/// - The table scrolls to the new row, as a Qt view follows its current index
///   (QAbstractItemView::currentChanged()). OpenRocket's actions set the selection models only,
///   so the selected row leaves the visible rows without the table scrolling (probe: 20 x Right
///   from row 5 of 100 in a 200-pixel viewport leaves the view at the top), where JTable's own
///   Down scrolls to its row (probe: the view at y = 218 after 20 x Down).
/// - In a table without rows (a filter that hides every row) the actions do nothing. Java's
///   cycling ones change the selection models all the same: Tab selects row 0 of no rows (probe:
///   getSelectedRow() is 0 in an empty table; the parts library's Close then fails with an
///   ArrayIndexOutOfBoundsException in convertRowIndexToModel(0)) and Shift+Tab selects column 0.
/// - The arrows of the keypad move rows as well: Qt marks every arrow key of macOS with
///   Qt::KeypadModifier, which the keys therefore ignore. OpenRocket binds VK_RIGHT and VK_LEFT
///   only, and the keypad's VK_KP_RIGHT and VK_KP_LEFT keep JTable's moves to the next and the
///   previous column (probe).
///
/// "probe": OpenRocket's results, printed by
/// tools/openrocket-gui-pins/preset-chooser/probe/TableRowTraversalProbe.java (the family's
/// hand-pinned.txt keeps them; TableRowTraversalTests.cpp pins them).
///
/// install() puts the filter on a table, which owns it. Tests and other callers can look a key
/// up (actionFor(), focusMoveFor()) or run an action without a key (perform()), as Java's test
/// does through the input and action maps.
class TableRowTraversal : public QObject
{
    Q_OBJECT

public:
    /// The four actions (Java: "Action.NextRow", "Action.NextRowCycle", "Action.PreviousRow" and
    /// "Action.PreviousRowCycle").
    enum class Action
    {
        NEXT_ROW,
        NEXT_ROW_CYCLE,
        PREVIOUS_ROW,
        PREVIOUS_ROW_CYCLE,
    };

    /// Where Ctrl+Tab and Ctrl+Shift+Tab move the focus (JTable's focus traversal keys).
    enum class FocusMove
    {
        FORWARD,
        BACKWARD,
    };

    /// Java's setTableRowTraversalPolicy(table): installs a filter on @p table, which owns it. A
    /// second install() on the same table does nothing more.
    static void install(QTableView& table);

    /// The action @p event's key is bound to (Java: the input map), or nullopt.
    [[nodiscard]] static std::optional<Action> actionFor(const QKeyEvent& event);

    /// The focus move @p event's key is (Ctrl+Tab, Ctrl+Shift+Tab), or nullopt.
    [[nodiscard]] static std::optional<FocusMove> focusMoveFor(const QKeyEvent& event);

    /// Runs @p action on @p table (Java: the action's actionPerformed()); see the class comment.
    /// Nothing happens without a model, a selection model or a row to go to.
    static void perform(QTableView& table, Action action);

    /// The lowest row of @p table's selection (Java: JTable.getSelectedRow()), or -1 when nothing
    /// is selected.
    [[nodiscard]] static int selectedRow(const QTableView& table);

    /// A filter owned by @p parent; install() installs it as well.
    explicit TableRowTraversal(QTableView* parent);

    /// Takes the keys of the class comment on the table; every other event goes on.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// Moves the focus from @p table to the next or the previous widget of the focus chain.
    void moveFocus(QTableView& table, FocusMove direction);

    /// Set while moveFocus() hands the table the Tab that moves the focus.
    bool m_passing{false};
};

}  // namespace QtRocket::Gui
