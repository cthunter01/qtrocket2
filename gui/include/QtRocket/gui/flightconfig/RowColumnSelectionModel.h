#pragma once

#include <vector>

#include <QItemSelection>
#include <QItemSelectionModel>
#include <QModelIndex>
#include <QObject>

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
/// - Current (a drag, Shift+click, Shift+arrow): the change starts from the selection as the last
///   other change left it, so that each step of the gesture replaces the previous one.
/// The view then shows the cross product (the base class holds it). Qt's Rows and Columns flags
/// name every column or every row of the cells.
///
/// Each change of the rows or the columns is told by linesChanged(), with the rows and columns
/// before and after it and the order in which a JTable's models would have told of them (a
/// JTable's changeSelection() changes the columns first, its clearSelection() the rows first): a
/// listener of a JTable's column model sees the old rows when the columns change first. A reset of
/// the model clears the rows and the columns without a signal, as QItemSelectionModel::reset()
/// does (Qt calls it at a reset of the model); a model that inserts, removes or moves rows or
/// columns clears them too, with the signal (the flight configuration models only reset).
///
/// Deviation from OpenRocket: the mouse and the keys select as Qt's views do (Shift+click extends
/// from the cell of the last click, keeping what Ctrl+click added before it; a JTable's Shift+click
/// selects only the rows and the columns from its anchor to the cell).
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

    /// Selects nothing (JTable.clearSelection(): the rows first).
    void clearLines();

Q_SIGNALS:
    /// The rows or the columns changed (see the class comment).
    void linesChanged(const QtRocket::Gui::RowColumnSelectionModel::LinesChange& change);

private:
    /// What a command names: the rows and the columns of its cells.
    [[nodiscard]] Lines linesOf(const QItemSelection&               selection,
                                QItemSelectionModel::SelectionFlags command) const;
    /// @p lines without the rows and columns the model does not have, in order, each once.
    [[nodiscard]] Lines valid(Lines lines) const;
    /// Makes @p lines the selection, shows its cells and tells of a change in @p order.
    void apply(Lines lines, Order order);
    /// Shows the cells of the rows and columns (the base class's selection).
    void showCells();
    /// The model inserted, removed or moved rows or columns.
    void structureChanged();

    Lines m_lines;
    /// The selection the last change that was not part of a gesture (Current) left.
    Lines m_committed;
};

}  // namespace QtRocket::Gui
