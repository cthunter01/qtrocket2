#pragma once

#include <optional>
#include <vector>

#include <QItemSelection>
#include <QItemSelectionModel>
#include <QModelIndex>
#include <QObject>
#include <QPersistentModelIndex>

class QAbstractItemModel;

namespace QtRocket::Gui
{

/// The selection of a flight configuration table, as a JTable selects cells when its cell
/// selection is enabled (OpenRocket's FlightConfigurablePanel: setCellSelectionEnabled(true),
/// MULTIPLE_INTERVAL_SELECTION): a set of selected rows and a set of selected columns, each kept
/// on its own (a JTable's row and column selection models), and the selected cells are every
/// selected row crossed with every selected column. The rows can be selected while no column is
/// (and the other way round): no cell is selected then.
///
/// What a view asks for (QItemSelectionModel::select(), from its mouse and keys) changes the rows
/// and the columns of the cells it names, as JTable.changeSelection() changes its two models (the
/// rules below are those its documentation gives, confirmed by the M3 run 2 probe
/// JTableSelectionProbe, JDK 17 headless):
/// - Select: the cells' rows and columns are added; with Clear, they are all that is selected (a
///   click, an arrow key, Ctrl+A);
/// - Deselect: the cells' rows and columns are removed (Qt's Ctrl+click on a selected cell; a
///   JTable's Ctrl+click toggles a selected cell off by removing its row and its column);
/// - Toggle: a selected cell's row and column are removed, those of a cell that is not selected
///   added (Ctrl+Space);
/// - Current (a drag, Shift+click, Shift+arrow): the rows and the columns from the anchor to the
///   cell (JTable.changeSelection(row, column, false, true)), replacing what was selected, so
///   that each step of a drag replaces the previous one; with Ctrl held (setToggleHeld()), the
///   anchor cell's state (selected or not) is given to them in the selection as it is
///   (changeSelection(row, column, true, true): a Ctrl+drag keeps what it crossed). The anchor is
///   the cell of the last
///   change that was not Current (a click, a Ctrl+click, an arrow key: the view's current index
///   then) or of the last selectCell(), addCell() or setLines() (its first row and column); the
///   cell is the one the
///   view names (setInputCell(): the mouse's), else the view's current index. Without an anchor
///   (after a reset of the model) the cells the view names are taken.
/// The view then shows the cross product (the base class holds it). Qt's Rows and Columns flags
/// name every column or every row of the cells.
///
/// A gesture of the mouse is told as a JTable's look and feel tells it (setValueIsAdjusting(),
/// which ConfigurationTableView calls from a press to its release, as BasicTableUI does): while
/// it lasts, each change is told as adjusting (LinesChange::adjusting); its end tells the whole
/// change of the gesture once more, not adjusting, the rows first (a JTable's row model ends its
/// adjusting before its column model), when it changed something.
///
/// Each change of the rows or the columns is told by linesChanged(), with the rows and columns
/// before and after it and the order in which a JTable's models would have told of them (a
/// JTable's changeSelection() changes the columns first, its clearSelection() the rows first): a
/// listener of a JTable's column model sees the old rows when the columns change first. A reset of
/// the model clears the rows and the columns without a signal, as QItemSelectionModel::reset()
/// does (Qt calls it at a reset of the model); a model that inserts, removes or moves rows or
/// columns clears them too, with the signal (the flight configuration models only reset).
///
/// The rules were confirmed by the M3 run 2 probe GestureProbe (JDK 17, BasicTableUI's own mouse
/// listeners under xvfb: the events of a click, a drag, Shift, Ctrl and Ctrl+Shift, the anchor).
class RowColumnSelectionModel final : public QItemSelectionModel
{
    Q_OBJECT

public:
    /// The selected rows and the selected columns, each in increasing order.
    struct Lines
    {
        std::vector<int> rows;
        std::vector<int> columns;

        friend bool operator==(const Lines&, const Lines&) = default;
    };

    /// The order in which a JTable's models would tell of a change of both.
    enum class Order
    {
        /// The columns, then the rows (JTable.changeSelection(), what the mouse and the keys do).
        COLUMNS_FIRST,
        /// The rows, then the columns (JTable.clearSelection(), setRowSelectionInterval() before
        /// setColumnSelectionInterval()).
        ROWS_FIRST,
    };

    /// A change of the rows or the columns.
    struct LinesChange
    {
        Lines before;
        Lines after;
        Order order = Order::COLUMNS_FIRST;
        /// Whether the change is a step of a gesture that has not ended (a JTable's
        /// ListSelectionEvent.getValueIsAdjusting()); the gesture's end tells it again.
        bool adjusting = false;
    };

    /// The selection of the cells of @p model.
    explicit RowColumnSelectionModel(QAbstractItemModel* model, QObject* parent = nullptr);
    ~RowColumnSelectionModel() override;

    RowColumnSelectionModel(const RowColumnSelectionModel&)            = delete;
    RowColumnSelectionModel& operator=(const RowColumnSelectionModel&) = delete;
    RowColumnSelectionModel(RowColumnSelectionModel&&)                 = delete;
    RowColumnSelectionModel& operator=(RowColumnSelectionModel&&)      = delete;

    /// The rows and the columns of @p index, as the class comment says.
    void select(const QModelIndex& index, QItemSelectionModel::SelectionFlags command) override;
    /// The rows and the columns of the cells of @p selection, as the class comment says.
    void select(const QItemSelection&               selection,
                QItemSelectionModel::SelectionFlags command) override;
    /// Clears the rows, the columns and the current index without a signal
    /// (QItemSelectionModel::reset()).
    void reset() override;

    /// The selected rows and columns.
    [[nodiscard]] const Lines& lines() const noexcept { return m_lines; }

    /// Whether the cell (@p row, @p column) is selected: its row and its column are
    /// (JTable.isCellSelected()).
    [[nodiscard]] bool isCellSelected(int row, int column) const noexcept;

    /// The first selected row; -1 when none is (JTable.getSelectedRow()).
    [[nodiscard]] int firstRow() const noexcept;

    /// The first selected column; -1 when none is (JTable.getSelectedColumn()).
    [[nodiscard]] int firstColumn() const noexcept;

    /// Selects the rows and the columns of @p lines (those in the model; in any order, each once),
    /// telling of the change in @p order.
    void setLines(const Lines& lines, Order order);

    /// Selects the cell (@p row, @p column) alone (JTable.changeSelection(row, column, false,
    /// false)), its index the current one.
    void selectCell(int row, int column);

    /// Adds the row and the column of the cell (@p row, @p column) (JTable.changeSelection(row,
    /// column, true, false) of a cell that is not selected), its index the current one.
    void addCell(int row, int column);

    /// Selects nothing (JTable.clearSelection(): the rows first). The anchor stays.
    void clearLines();

    /// Starts (true) or ends (false) a gesture of the mouse (see the class comment); a start
    /// during a gesture, or an end without one, does nothing.
    void setValueIsAdjusting(bool adjusting);
    /// Whether a gesture lasts.
    [[nodiscard]] bool valueIsAdjusting() const noexcept { return m_adjusting; }

    /// Whether Ctrl is held in the view's input of the moment (see the class comment).
    void setToggleHeld(bool held) noexcept { m_toggleHeld = held; }
    /// The cell under the mouse of the view's input of the moment (an invalid index for a key):
    /// the cell an extending change goes to.
    void setInputCell(const QModelIndex& cell);

Q_SIGNALS:
    /// The rows or the columns changed (see the class comment).
    void linesChanged(const QtRocket::Gui::RowColumnSelectionModel::LinesChange& change);

private:
    /// What a command names: the rows and the columns of its cells.
    [[nodiscard]] Lines linesOf(const QItemSelection&               selection,
                                QItemSelectionModel::SelectionFlags command) const;
    /// @p lines without the rows and columns the model does not have, in order, each once.
    [[nodiscard]] Lines valid(Lines lines) const;
    /// An extending change (Current) the view asks for (see the class comment).
    void selectExtension(const QItemSelection&               selection,
                         QItemSelectionModel::SelectionFlags command);
    /// Makes @p lines the selection, shows its cells and tells of a change in @p order.
    void apply(Lines lines, Order order);
    /// The rows and the columns from the anchor to the cell of an extending change that names
    /// @p named; nullopt without an anchor or a cell.
    [[nodiscard]] std::optional<Lines> extension(const Lines& named) const;
    /// Makes (@p row, @p column) the anchor.
    void setAnchor(int row, int column) noexcept;
    /// Shows the cells of the rows and columns (the base class's selection).
    void showCells();
    /// The model inserted, removed or moved rows or columns.
    void structureChanged();

    Lines m_lines;
    /// The selection when the gesture started.
    Lines m_gestureStart;
    /// The anchor (JTable's anchor of its row and column models); -1 for none.
    int m_anchorRow{-1};
    int m_anchorColumn{-1};
    /// The cell of the view's input of the moment.
    QPersistentModelIndex m_inputCell;
    bool                  m_adjusting{false};
    bool                  m_toggleHeld{false};
};

}  // namespace QtRocket::Gui
