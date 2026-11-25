#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include <QObject>
#include <QPoint>
#include <QString>
#include <QTableView>
#include <Qt>

#include "QtRocket/gui/simulation/SimulationColumns.h"

class QAbstractItemModel;
class QContextMenuEvent;
class QKeyEvent;
class QMouseEvent;
class QWidget;

namespace QtRocket
{
class Simulation;
}

namespace QtRocket::Gui
{

class SimulationTableModel;

/// The simulations table of the "Flight simulations" tab (OpenRocket's ColumnTable as
/// SimulationPanel.java:257-387 sets it up, with its mouse listener; the column chooser is
/// SimulationColumnVisibility): a QTableView of a SimulationTableModel, which sorts itself (the
/// view never calls setSortingEnabled()).
///
/// The table (JTable's defaults and SimulationPanel's settings):
/// - several rows selectable at once, in several intervals (MULTIPLE_INTERVAL_SELECTION), whole
///   rows; nothing editable; no row header;
/// - the columns keep their widths and the table scrolls sideways (AUTO_RESIZE_OFF); they can be
///   moved by dragging their headers (JTable's reorderingAllowed) and made as narrow as
///   kMinimumColumnWidth (Swing's TableColumn minimum). The widths are ColumnTableModel.
///   setColumnWidths()'s: the status column exactly SimulationColumns::kStatusColumnWidth (not
///   resizable), the warnings column at least SimulationColumns::defaultWidth() (its minimum),
///   the others their default width; then setupAutoSizeColumns() (ColumnTable's, once, as the
///   panel does when it is made) widens the warnings column to fit its cells;
/// - the header is the model's (SimulationTableModel::connectHeader()): a click on a column's
///   header toggles its sort key, wherever the column was moved (the header's logical sections
///   are the model's columns); a double click toggles it once, as JTableHeader does (Qt's header
///   reports the first click of a double click only); the right button never sorts. Its sections
///   are not highlighted for the selected rows (a JTableHeader never is);
/// - the warnings column is drawn by SimulationWarningsDelegate;
/// - Tab, Shift+Tab, Right and Left move from row to row (TableRowTraversal, the panel's
///   TableRowTraversalPolicy).
///
/// The mouse (SimulationPanel's mouseClicked() listener), each rule on the release of a click
/// (a press and a release of one button at the same place: AWT's MOUSE_CLICKED), after the
/// view selected as Qt's views do (the left and the right button; the others select nothing, as
/// in a JTable):
/// - a click of any button where there is no row, or one that leaves no row selected (a
///   Ctrl+click on the only selected row), clears the selection (Qt keeps it on a Ctrl+click
///   beside the rows; OpenRocket clears it);
/// - a click of the left button on a row: cellClicked() with the first selected row's simulation
///   (Java's getSelectedRow(), the topmost selected row, which a Ctrl+click need not have made
///   the clicked one) and the clicked cell's column;
/// - a double click of the left button on a row (its second click): cellDoubleClicked() with the
///   same simulation and column, after a double click on a column other than the status and the
///   warnings column made the first selected row the only selected one (the panel then opens
///   that simulation's dialog).
/// SimulationPanel handles them already (its cellClicked() and cellDoubleClicked(), connected
/// when it makes the view; connect them nowhere else): it runs the selected simulations after a
/// click on the status column when the table's status of the signal's simulation
/// (SimulationTableModel::status()) is NOT_SIMULATED or OUTDATED, opens the settings dialog on
/// its warnings tab after a double click on the warnings column, nothing after one on the status
/// column, and the dialog of the simulation (its plot tab when the data is up to date) after one
/// on any other column, through the services' UserPrompts::runSimulations() and
/// SimulationDialogs (the application's: SimulationRunDialog and SimulationConfigDialog).
/// - A click of the right button on a row selects that row alone when it is not selected, then
///   asks for the context menu (contextMenuRequested()); beside the rows it clears the
///   selection and asks for nothing.
///
/// The keys: Enter and Shift+Enter move to the next and the previous cell (JTable's
/// selectNextRowCell and selectPreviousRowCell, as the probe shows them): down the current
/// column (and up), from the last row to the first row of the next shown column (and back),
/// from the last cell to the first. With no row selected, or one row whose cell is the current
/// one, the cell's row becomes the selection (without a current cell Enter goes to the first
/// cell and Shift+Enter to the last); with more than one row selected, or one row and the
/// current cell in another row, the current cell moves within the rows from the first to the
/// last selected one and skips the rows that are not selected, and the selection stays. A
/// current cell in a hidden column counts as in the shown column before it (JTable moves the
/// lead there). Ctrl+A selects every row. The keys of the window's actions are not the table's
/// (SimulationPanel binds them to "none" in the table's input map so that the menu bar's
/// accelerators act): Copy, Cut, Paste (QKeySequence's), Ctrl+E, Ctrl+D and Ctrl+Delete, and
/// Delete, which JTable does not bind either, go to the window. Typed text does nothing
/// (keyboardSearch(): a JTable starts an editor, and no cell is editable).
///
/// The selection belongs to the simulations (OpenRocket bug 28, fixed; the model moves the
/// views' indexes with their simulations across a new order or a change of the list). One case
/// has no simulation to follow: an undo or a redo that changes the number of simulations
/// replaces all of them with copies (OpenRocketDocument's undo history). When no simulation of
/// the table before such a change is in it afterwards, the view selects the same row numbers
/// again, as OpenRocket's fireMaintainSelection() does after every change, the last of them the
/// current row.
///
/// selectedSimulationsChanged() tells of every change of the selected simulations (Java's
/// addSimulationTableListSelectionListener(): listen to it, not to the selection model). The
/// selection model's selectionChanged() does not tell of all of them: a layout change of the
/// model that takes selected rows away (a selected simulation that left the document, an undo
/// or a redo whose copies the view cannot select again) shrinks the selection without it, where
/// OpenRocket's fireMaintainSelection() selects anew and its listeners hear of it. A new order of
/// the same selected simulations is no change: the view tells nothing the selection model does
/// not tell then.
///
/// Deviations from OpenRocket:
/// - OpenRocket bug 26, fixed: the status click and the double clicks are told with the
///   clicked cell's column (the model's), wherever it was moved and whatever is hidden.
///   OpenRocket tests the view column: column 0 runs, column 1 opens the warnings and columns
///   from 2 on open the dialog, so after the status or the warnings column is hidden or moved,
///   a click on whatever column is first runs the selected simulations, and a double click on
///   the second one opens the warnings tab.
/// - OpenRocket bug 28, fixed: see above.
/// - A press of the right button selects the row under it, as Qt's views do: in OpenRocket the
///   press selects nothing, so a right click on a row while no row is selected does nothing
///   (the mouseClicked() listener clears the selection) where here it selects the row and asks
///   for the menu.
/// - The keyboard's context menu key asks for the menu too, at the current row, when a row is
///   selected (OpenRocket has no such key).
/// - selectSimulations() scrolls the table to the current row, as a Qt view follows its current
///   index; JTable's setRowSelectionInterval() scrolls nowhere.
/// - A third click in a row is a new click (Qt starts again after a double click): a triple click
///   tells cellClicked() twice and cellDoubleClicked() once. OpenRocket's third click has the
///   click count 3, which its listener ignores.
///
/// The model must outlive the view; the view shows no other (setModel() is a BugError).
class SimulationTableView final : public QTableView
{
    Q_OBJECT

public:
    using Column = SimulationColumns::Column;

    /// The narrowest a column can be made by dragging its edge (Swing's TableColumn minimum).
    static constexpr int kMinimumColumnWidth = 15;

    /// The table of @p model, which must outlive it: the widths of ColumnTableModel.
    /// setColumnWidths() and setupAutoSizeColumns(), every column shown.
    SimulationTableView(SimulationTableModel& model, QWidget* parent);
    ~SimulationTableView() override;

    SimulationTableView(const SimulationTableView&)            = delete;
    SimulationTableView& operator=(const SimulationTableView&) = delete;
    SimulationTableView(SimulationTableView&&)                 = delete;
    SimulationTableView& operator=(SimulationTableView&&)      = delete;

    /// The model, set at construction.
    [[nodiscard]] SimulationTableModel& tableModel() const noexcept { return *m_model; }

    /// A BugError: the view shows the model it was made with, and only that one.
    void setModel(QAbstractItemModel* model) override;

    // ---- the columns (the model's columns are the header's logical sections)

    /// The column of the cell at @p position of the viewport (a model column, wherever it was
    /// moved); nullopt beside the columns.
    [[nodiscard]] std::optional<Column> columnAtPoint(const QPoint& position) const;

    /// The columns shown, from the left (Java: the column model's columns).
    [[nodiscard]] std::vector<Column> visibleColumns() const;

    /// Whether @p column is shown.
    [[nodiscard]] bool isColumnVisible(Column column) const;

    /// The place of @p column among the columns shown, from 0 at the left
    /// (convertColumnIndexToView()); -1 when it is hidden.
    [[nodiscard]] int viewColumn(Column column) const;

    /// ColumnTable.setupAutoSizeColumns(): each auto-sized column (the warnings column) as wide
    /// as its widest cell (its delegate's size hint; the header's too when the column counts it)
    /// and SimulationColumns::kAutoSizePadding, at least its minimum width. The view does it
    /// when it is made, as the panel does; the widths stay afterwards.
    void setupAutoSizeColumns();

    // ---- the selection

    /// The simulations of the selected rows, from the top (Java: getSelectedSimulations()).
    [[nodiscard]] std::vector<std::shared_ptr<Simulation>> selectedSimulations() const;

    /// The simulation of the topmost selected row (Java: getSelectedRow()); null when no row is
    /// selected.
    [[nodiscard]] std::shared_ptr<Simulation> firstSelectedSimulation() const;

    /// Selects the rows of @p simulations and nothing else, the row of the last of them the
    /// current one in the current cell's column (Java's setSelectionInterval() and
    /// addRowSelectionInterval(): the lead is the last row given); a simulation that is no row
    /// of the table is left out, and with none the selection is cleared.
    void selectSimulations(const std::vector<std::shared_ptr<Simulation>>& simulations);

    /// Does nothing (see the class comment; QAbstractItemView selects the next row whose text
    /// starts with @p search).
    void keyboardSearch(const QString& search) override;

Q_SIGNALS:
    /// A click of the left button on a cell of @p column: @p simulation is the first selected
    /// row's (see the class comment).
    void cellClicked(const std::shared_ptr<QtRocket::Simulation>& simulation,
                     QtRocket::Gui::SimulationColumns::Column     column);

    /// A double click of the left button on a cell of @p column: @p simulation is the first
    /// selected row's (see the class comment).
    void cellDoubleClicked(const std::shared_ptr<QtRocket::Simulation>& simulation,
                           QtRocket::Gui::SimulationColumns::Column     column);

    /// The context menu was asked for at @p globalPosition, the selected rows being its subject.
    void contextMenuRequested(const QPoint& globalPosition);

    /// The selected simulations changed (see the class comment): after the selection model's
    /// selectionChanged(), and after a layout change of the model that changed them without one.
    void selectedSimulationsChanged();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    /// A press of a mouse button, for the click its release may end.
    struct Press
    {
        QPoint          position;
        Qt::MouseButton button = Qt::NoButton;
        /// Whether it was the second press of a double click.
        bool second = false;
    };

    /// What the table showed before a layout change of the model.
    struct Before
    {
        /// The simulations of the rows.
        std::vector<std::shared_ptr<Simulation>> simulations;
        /// The selected rows, ascending.
        std::vector<int> selectedRows;
        /// The current cell's column (a model column), -1 without one.
        int currentColumn = -1;
        /// The selected simulations (selectionByIdentity()).
        std::vector<std::shared_ptr<Simulation>> selected;
        /// m_selectionChanges then.
        std::size_t selectionChanges = 0;
    };

    /// ColumnTableModel.setColumnWidths().
    void setColumnWidths();
    /// Keeps a column that has a minimum width (the warnings column) at least as wide.
    void keepMinimumWidth(int section);
    /// The rule of a click of @p button released at @p position (the class comment), the
    /// second click of a double click when @p second.
    void clicked(const QPoint& position, Qt::MouseButton button, bool second);
    /// Enter (@p forward) and Shift+Enter (see the class comment).
    void selectNextRowCell(bool forward);
    /// The selected rows, ascending.
    [[nodiscard]] std::vector<int> selectedRows() const;
    /// The shown sections (model columns) from the left.
    [[nodiscard]] std::vector<int> visibleSections() const;
    /// The section of the current cell, or of the shown column before it when it is hidden
    /// (the first shown one when none is before it); -1 without a current cell.
    [[nodiscard]] int currentSection() const;
    /// @p section, or without one (-1) the first shown section (the first section when none is
    /// shown): the column of a current cell that the view makes.
    [[nodiscard]] int leadSection(int section) const;
    /// The selected simulations, in the order of their addresses (a set, by identity).
    [[nodiscard]] std::vector<std::shared_ptr<Simulation>> selectionByIdentity() const;
    /// The selection model's selectionChanged: selectedSimulationsChanged().
    void selectionModelChanged();
    /// The model's layoutAboutToBeChanged: remembers the rows and the selection.
    void rememberRows();
    /// The model's layoutChanged: reselectReplacedRows(), then selectedSimulationsChanged() when
    /// the selected simulations changed and the selection model did not tell.
    void layoutChangedByModel();
    /// Selects the same rows as @p before again when none of the rows' simulations is in the
    /// table any more (see the class comment).
    void reselectReplacedRows(const Before& before);

    SimulationTableModel* m_model;
    std::optional<Press>  m_press;
    std::optional<Before> m_before;
    bool                  m_resizing{false};
    /// How often selectedSimulationsChanged() was emitted.
    std::size_t m_selectionChanges{0};
};

}  // namespace QtRocket::Gui
