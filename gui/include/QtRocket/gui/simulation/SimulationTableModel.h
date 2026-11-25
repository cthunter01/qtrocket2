#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include <QAbstractTableModel>
#include <QModelIndex>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariant>
#include <Qt>

#include "QtRocket/document/events/DocumentChangeEvent.h"
#include "QtRocket/gui/simulation/SimulationColumns.h"
#include "QtRocket/gui/util/TableSortKeys.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/Signal.h"

class QHeaderView;

namespace QtRocket
{
class OpenRocketDocument;
class Preferences;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// The simulations table of the "Flight simulations" tab (OpenRocket's SimulationTableModel,
/// SimulationPanel.java:1487-1758, behind the ColumnTable and the ColumnTableRowSorter of
/// SimulationPanel.java:257-270): a row per simulation of the document, in the table's order (the
/// view order: the rows sorted by the sort keys, equal rows in the document's order), and the
/// fourteen columns of SimulationColumns. The model sorts itself and is no
/// QSortFilterProxyModel (Qt's sorts by one column, Java's by up to three keys with the model
/// order last: TableSortKeys); its rows are the view's rows.
///
/// What a row shows:
/// - the display role: SimulationColumns::cellText() (nothing for the status and warnings
///   columns, and for a value column without data);
/// - the decoration role of the status column: the status icon (util/SimulationIcons);
/// - the tooltip of the warnings column: SimulationColumns::warningsToolTip(), of every other
///   column: SimulationColumns::statusToolTip(), both with the simulation's name and in the
///   application palette's colours of the moment they are asked for;
/// - the accessible text of the status column: SimulationColumns::statusIconText();
/// - the warnings column's counts, for SimulationWarningsDelegate: kCriticalWarningsRole,
///   kNormalWarningsRole and kInformationalWarningsRole (an int each; nothing without data).
/// The header: SimulationColumns' header texts and tooltips; no row header.
///
/// When the rows are read: once per document event (documentChanged(), every kind and source: a
/// change of the rocket, of a simulation, of the list of simulations, an undo or redo, a document
/// preference) and by refresh(), never in data(). Each reading calls getStatus() of every
/// simulation exactly once (O-M4-6): OpenRocket calls it whenever its table is painted, and
/// getStatus() stores what it finds (an OUTDATED status stays, and an undo position copies the
/// stored status), so that what an undo brings back depends on whether the table was painted in
/// between; here it depends on the document's events only. The cells are made then too
/// (SimulationColumns::cellValue(): the default units of that moment), and the rows are sorted
/// again by them. A header click sorts the rows that were read (it reads nothing): the values only
/// change with a document event, or with the default units (no event: whoever changes the units
/// calls refresh(), as the preferences dialog does through the window).
///
/// The rows are the simulations themselves (std::shared_ptr), never their indices: across a new
/// order or a change of the list, every index the views keep (the selection, the current index)
/// stays on its simulation, and an index of a simulation that left the list goes (a layout
/// change, with the persistent indexes moved by identity). A reading that finds the same
/// simulations in the same order tells the views of new data instead (dataChanged() over every
/// cell). refreshed() follows every reading.
///
/// The header: connectHeader() makes a view's header the table's, as MotorTableModel does: a
/// click on a section toggles the column's sort key (toggleSortOrder(), Java's
/// DefaultRowSorter.toggleSortOrder()), and the primary key's column shows the sort arrow. The
/// view must not sort itself (QTableView::setSortingEnabled() stays off).
///
/// Deviations from OpenRocket (each fixed where OpenRocket's table works by view rows):
/// - OpenRocket bug 23: the warnings column sorts (SimulationColumns).
/// - OpenRocket bug 28 (the selection after a document event): after every document event
///   OpenRocket's fireMaintainSelection() calls fireTableDataChanged(), which clears JTable's
///   selection and sorts the rows again, and then selects the same view row numbers again; so in a
///   sorted table a change that reorders the rows (statuses or values changing, a simulation
///   added or removed) moves the selection to other simulations. Here the selection stays on its
///   simulations (the persistent indexes follow them). A header click keeps the selection on its
///   simulations in OpenRocket too (JTable keeps it by model row across a new sort order).
/// - The statuses are read once per document event (see above), not at every paint.
/// - The cells' units are those of the last reading: OpenRocket's cells take the default units
///   of each paint (a units change shows at once; here at the next document event or refresh()).
///
/// The document and the preferences must outlive the model. Not copyable.
class SimulationTableModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    using Column = SimulationColumns::Column;

    /// The role of the warnings column's number of critical (HIGH) warnings, an int; nothing
    /// without data.
    static constexpr int kCriticalWarningsRole = Qt::UserRole + 1;
    /// The role of the warnings column's number of normal warnings, an int; nothing without data.
    static constexpr int kNormalWarningsRole = Qt::UserRole + 2;
    /// The role of the warnings column's number of informational (LOW) warnings, an int; nothing
    /// without data.
    static constexpr int kInformationalWarningsRole = Qt::UserRole + 3;

    /// The table of @p document's simulations, read now, in the document's order (no sort keys);
    /// the configurations' names are made with @p preferences' motor name column.
    SimulationTableModel(OpenRocketDocument& document, const Preferences& preferences,
                         QObject* parent);
    ~SimulationTableModel() override;
    SimulationTableModel(const SimulationTableModel&)            = delete;
    SimulationTableModel& operator=(const SimulationTableModel&) = delete;
    SimulationTableModel(SimulationTableModel&&)                 = delete;
    SimulationTableModel& operator=(SimulationTableModel&&)      = delete;

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    /// The cell's roles (see the class comment).
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    /// The column's header text (display role) and tooltip (tooltip role, nothing when empty) of
    /// the horizontal header; nothing for the vertical one.
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                      int role) const override;
    /// Enabled and selectable, never editable.
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;

    /// The document whose simulations the rows are.
    [[nodiscard]] OpenRocketDocument& document() const noexcept { return *m_document; }

    // ---- the rows

    /// The simulation of the row @p row.
    /// @throws BugError when @p row is not a row of the table
    [[nodiscard]] const std::shared_ptr<Simulation>& simulation(int row) const;

    /// The row of @p simulation (the very object), or -1 when it is no row of the table.
    [[nodiscard]] int rowOf(const Simulation& simulation) const noexcept;

    /// The simulations of the rows, in the table's order.
    [[nodiscard]] std::vector<std::shared_ptr<Simulation>> simulations() const;

    /// The status the last reading found for the row @p row (what its status icon shows).
    /// @throws BugError when @p row is not a row of the table
    [[nodiscard]] Simulation::Status status(int row) const;

    /// The value of the row @p row in @p column, as the last reading made it.
    /// @throws BugError when @p row is not a row of the table
    [[nodiscard]] const SimulationColumns::CellValue& cellValue(int row, Column column) const;

    /// The text the row @p row shows in @p column (SimulationColumns::cellText()).
    /// @throws BugError when @p row is not a row of the table
    [[nodiscard]] QString cellText(int row, Column column) const;

    /// The warning counts of the row @p row; nullopt without data.
    /// @throws BugError when @p row is not a row of the table
    [[nodiscard]] std::optional<SimulationColumns::WarningCounts> warningCounts(int row) const;

    // ---- the order

    /// The sort keys, the primary key first (TableRowSorter.getSortKeys()).
    [[nodiscard]] const TableSortKeys& sortKeys() const noexcept { return m_sortKeys; }

    /// A click on the header of the column @p column: its sort key toggles
    /// (TableSortKeys::toggle()), and the rows are sorted again.
    /// @throws BugError for a column outside the table
    void toggleSortOrder(int column);

    /// Replaces the sort keys (setSortKeys(); none for the document's order) and sorts the rows
    /// again.
    /// @throws BugError for a column outside the table or one named twice
    void setSortKeys(std::vector<TableSortKeys::SortKey> keys);

    /// Makes @p header, the horizontal header of a view of this model, the table's header (see
    /// the class comment). The model follows only the last header given.
    void connectHeader(QHeaderView& header);

    // ---- reading

    /// Reads every simulation of the document again (its status, once, and its cells, in the
    /// default units of now) and sorts the rows again, as a document event does: for a change
    /// that sends no document event, the default units.
    void refresh();

Q_SIGNALS:
    /// The sort keys changed (toggleSortOrder(), setSortKeys()).
    void sortKeysChanged();

    /// The rows were read again (a document event, refresh()), or sorted into another order by
    /// new sort keys, and the views told of it.
    void refreshed();

private:
    using Cells = std::array<SimulationColumns::CellValue, SimulationColumns::kColumnCount>;

    /// A row: its simulation and what the last reading found.
    struct Row
    {
        std::shared_ptr<Simulation> simulation;
        /// The simulation's index in the document when it was read (the model row of Java).
        std::size_t        documentIndex = 0;
        Simulation::Status status{Simulation::Status::NOT_SIMULATED};
        Cells              cells;
    };

    /// The row of @p simulation, the document's simulation @p documentIndex, read now
    /// (getStatus() once).
    [[nodiscard]] Row readRow(const std::shared_ptr<Simulation>& simulation,
                              std::size_t                        documentIndex) const;
    /// Every simulation of the document, read now, in its order.
    [[nodiscard]] std::vector<Row> readRows() const;
    /// @p rows sorted by the keys, equal rows in the document's order.
    [[nodiscard]] std::vector<Row> sorted(std::vector<Row> rows) const;
    /// Takes @p rows as the rows and tells the views (see the class comment).
    void setRows(std::vector<Row> rows);
    /// Sorts the rows as they are again by the keys.
    void resort();
    /// Shows the primary key's sort arrow on the header.
    void updateSortIndicator();
    /// The row @p row; a BugError unless it is one.
    [[nodiscard]] const Row& rowAt(int row) const;

    OpenRocketDocument*   m_document;
    const Preferences*    m_preferences;
    std::vector<Row>      m_rows;
    TableSortKeys         m_sortKeys{SimulationColumns::kColumnCount};
    QPointer<QHeaderView> m_header;
    Signal<const DocumentChangeEvent&>::ScopedConnection m_documentChanged;
};

}  // namespace QtRocket::Gui
