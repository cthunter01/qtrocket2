#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

#include <QAbstractTableModel>
#include <QModelIndex>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariant>
#include <Qt>

#include "QtRocket/motor/chooser/ThrustCurveMotorColumns.h"

class QHeaderView;

namespace QtRocket
{
class Preferences;
class ThrustCurveMotorSelection;
class ThrustCurveMotorSet;
class Unit;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// The motor chooser's table (OpenRocket's ThrustCurveMotorDatabaseModel, swing
/// gui/dialogs/motor/thrustcurve/ThrustCurveMotorDatabaseModel.java, behind the JTable and the
/// TableRowSorter of ThrustCurveMotorSelectionPanel.java:339-388): a row per motor set the
/// filter shows, in the table's order, and the six columns of ThrustCurveMotorColumns
/// (Manufacturer, Name, Total Impulse, Case, Diameter, Length) with their texts
/// (getValueText(): the display role, left aligned; no tooltips, as Java's table, whose
/// getToolTipText() is dead code). The filter, the sort and the selection are the core's
/// (ThrustCurveMotorSelection: MotorRowFilter, MotorTableSorter); the model shows their view order
/// (getViewRows()) and is no QSortFilterProxyModel: Qt's sorts by one column, Java's by up to
/// three keys with the model order last.
///
/// The rows ("view rows") map to the rows of the core's model ("model rows", the order of the
/// sets in ThrustCurveMotorDatabaseModel) as JTable's convertRowIndexToModel() and
/// convertRowIndexToView() do: modelRow(), row(). refresh() is the sorter's sort(): the selection
/// panel calls it after every change of the filter, the search or the name column and when the
/// chooser opens. It tells the views of the new rows as a layout change (Qt's own proxy does the
/// same for a new filter): an index the views keep (the selection, the current index) stays on the
/// set it shows while the set is shown, also when the list of sets changed (a motor of no set got
/// one of its own), and goes when the set is filtered out, as JTable keeps the selection of a
/// model row across a sort.
///
/// The cells' texts are read from the core once per row and kept until the default unit of the
/// motor dimensions, the name column (Preferences::getMotorNameColumn()) or the list of sets
/// changes, which refresh() notices (Java renders the cell values, fresh Values in the default
/// unit, at every paint, so the texts always follow these; the units only change in the
/// preferences, while the modal chooser is closed, and the chooser refreshes when it opens).
///
/// The header: a click on a column's header toggles its sort key (toggleSortOrder(), JDK's
/// DefaultRowSorter.toggleSortOrder() through MotorTableSorter), and the column of the primary key
/// shows the sort arrow (as Swing's header renderer shows it for the first key only), once
/// connectHeader() is given the view's horizontal header. The view must not sort itself
/// (QTableView::setSortingEnabled() stays off).
///
/// The selection and the preferences must outlive the model. Not copyable.
class MotorTableModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    /// The number of columns, those of ThrustCurveMotorColumns.
    static constexpr int kColumnCount = static_cast<int>(kAllThrustCurveMotorColumns.size());

    /// The table of @p selection's rows, their texts by @p preferences' name column, as @p
    /// selection's getViewRows() gives them now.
    MotorTableModel(ThrustCurveMotorSelection& selection, const Preferences& preferences,
                    QObject* parent);
    ~MotorTableModel() override;
    MotorTableModel(const MotorTableModel&)            = delete;
    MotorTableModel& operator=(const MotorTableModel&) = delete;
    MotorTableModel(MotorTableModel&&)                 = delete;
    MotorTableModel& operator=(MotorTableModel&&)      = delete;

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    /// The cell's text for the display role; nothing for the other roles.
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    /// The column's title (ThrustCurveMotorColumns' getTitle()) for the display role of the
    /// horizontal header; nothing else (the table has no row header).
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                      int role) const override;

    /// The rows the selection shows now, in its order (TableRowSorter.sort(); see the class
    /// comment); nothing happens when neither the rows nor their texts changed.
    void refresh();

    /// A click on the header of the column @p column: its sort key toggles
    /// (MotorTableSorter::toggleSortOrder()), and the rows are sorted again (refresh()).
    /// @throws BugError for a column outside the table
    void toggleSortOrder(int column);

    /// Makes @p header, the horizontal header of a view of this model, the table's header: its
    /// sections are clickable and a click toggles the column's sort key, and it shows the sort
    /// arrow of the primary key (see the class comment). The header must belong to a view of this
    /// model; the model follows only the last header given.
    void connectHeader(QHeaderView& header);

    /// The model row of the view row @p row (convertRowIndexToModel()).
    /// @throws BugError when @p row is not a row of the table
    [[nodiscard]] std::size_t modelRow(int row) const;

    /// The view row of the model row @p modelRow (convertRowIndexToView()): -1 when the filter
    /// leaves it out or it is no row of the core's model.
    [[nodiscard]] int row(std::size_t modelRow) const noexcept;

    /// The view row of @p set (the first model row holding that very set,
    /// ThrustCurveMotorDatabaseModel
    /// ::getIndex()), or -1 when it is not shown or not in the model.
    [[nodiscard]] int rowOf(const ThrustCurveMotorSet& set) const;

    /// The set of the view row @p row.
    /// @throws BugError when @p row is not a row of the table
    [[nodiscard]] const ThrustCurveMotorSet& motorSet(int row) const;

    /// The column of the section @p column (ThrustCurveMotorColumns.values()[column]).
    /// @throws BugError for a column outside the table
    [[nodiscard]] static ThrustCurveMotorColumns columnAt(int column);

    /// The preferred width in pixels of the section @p column (ThrustCurveMotorColumns'
    /// getWidth(), which Java gives the table column as its preferred width).
    /// @throws BugError for a column outside the table
    [[nodiscard]] static int preferredWidth(int column);

private:
    using RowTexts = std::array<QString, kColumnCount>;

    /// Whether the texts kept were made for the current unit and name column.
    [[nodiscard]] bool textsAreCurrent() const;
    /// Whether the core's model still holds the sets of m_sets, in that order.
    [[nodiscard]] bool setsAreCurrent() const;
    /// The texts of the model row @p modelRow, made when not kept yet.
    [[nodiscard]] const RowTexts& textsOf(std::size_t modelRow) const;
    /// Takes the core's sets, the unit and the name column of now, and forgets the texts.
    void resetTexts();
    /// Takes @p rows as the view rows and builds their reverse map.
    void setRows(std::vector<std::size_t> rows);
    /// Moves the indexes the views keep to the rows of the sets they show (refresh()).
    void movePersistentIndexes(const std::vector<std::size_t>&                oldRows,
                               const std::vector<const ThrustCurveMotorSet*>& oldSets,
                               bool                                           sameSets);
    /// Shows the primary key's sort arrow on the header.
    void updateSortIndicator();
    /// A BugError unless @p row is a view row.
    void checkRow(int row) const;

    ThrustCurveMotorSelection* m_selection;
    const Preferences*         m_preferences;
    /// The view rows: the model row of each (the view-to-model index).
    std::vector<std::size_t> m_rows;
    /// The view row of each model row, -1 when it is not shown.
    std::vector<int> m_rowOfModelRow;
    /// The core model's sets when the texts were made.
    std::vector<const ThrustCurveMotorSet*> m_sets;
    /// The unit and the name column the texts were made for.
    const Unit* m_dimensionUnit{nullptr};
    bool        m_designation{true};
    /// The texts of each model row, made when a view first asks for them.
    mutable std::vector<std::optional<RowTexts>> m_texts;
    QPointer<QHeaderView>                        m_header;
};

}  // namespace QtRocket::Gui
