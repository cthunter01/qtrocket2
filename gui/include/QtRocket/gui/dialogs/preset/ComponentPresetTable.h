#pragma once

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <QObject>
#include <QTableView>

#include "QtRocket/rocket/preset/ComponentPresetType.h"

class QKeyEvent;
class QMenu;
class QModelIndex;
class QPoint;
class QWidget;

namespace QtRocket
{
class ComponentPreset;
class Preferences;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class ComponentPresetFilterModel;
class ComponentPresetTableModel;

/// The parts library's table (OpenRocket: ComponentPresetTable with its XTableColumnModel, swing
/// gui/dialogs/preset/ComponentPresetTable.java and XTableColumnModel.java, and what
/// ComponentPresetChooserDialog sets up on it: the favourite column's width, the row traversal,
/// the legacy box's column and filter, the stored column layout): the presets of one type
/// (ComponentPresetTableModel) through their filters and sort keys (ComponentPresetFilterModel),
/// one row selected at most (SINGLE_SELECTION, whole rows). The selection is a model row, as
/// JTable keeps it through its sorter's changes: a filter that hides the selected row leaves no
/// row selected (Qt's single selection would select the next one, which Close would then apply),
/// and when a filter shows that row again it is selected again, unless another row was selected
/// or the selection cleared while the row was shown (probe: "== the selection through the
/// filters").
///
/// The columns: the model's 31 columns, of which the table's are the favourite and the keys the
/// type displays (isTableColumn(); OpenRocket removes the others from its column model, so they
/// never show and are not in the header menu, but the text filter searches them). A table column
/// can be hidden and shown again in its place (setColumnVisible(): QHeaderView's hidden sections,
/// XTableColumnModel's invisible columns) and moved by dragging its header
/// (setReorderingAllowed(true)), and made as narrow as kMinimumColumnWidth (Swing's minimum). The
/// favourite column is as narrow as possible: a fixed section of the row height and 4 pixels (the
/// dialog's "setPreferredWidth(rowHeight + 4)" with that minimum and maximum).
///
/// The order: a click on a header toggles its sort key (ComponentPresetFilterModel::
/// toggleSortOrder(), TableRowSorter's), the header's sort indicator shows the primary key; the
/// table starts sorted by Manufacturer. Tab, Shift+Tab, Right and Left move from row to row
/// (TableRowTraversal, the dialog's TableRowTraversalPolicy). Enter and Shift+Enter move to the
/// next and the previous cell, column after column (JTable's selectNextRowCell and
/// selectPreviousRowCell: down the current column, from its last row to the first row of the next
/// shown column, from the last cell to the first, and back the same way), selecting the cell's
/// row; without a selected row Enter goes to the first cell and Shift+Enter to the last (JTable
/// without a lead cell, as a new table has); in a table without rows or shown columns they do
/// nothing. Either way the key goes no further: Enter in the table does not press the dialog's
/// default button, as in OpenRocket (a probe of its shown dialog under xvfb). Space ticks or
/// unticks the favourite of the current row when the current cell is in the favourite column
/// (Qt's check box items; after Tab the current cell is in the first shown column, the favourite
/// unless it was moved) and changes nothing in another column, as in OpenRocket (the same probe:
/// JTable binds Space to addToSelection, and the favourite's check box editor takes the key all
/// the same).
///
/// The header menu (a right click on the header; populateHeaderMenu(), Java's ColumnPopupMenu):
/// on a column, "Sort Ascending" and "Sort Descending" (check box items, never ticked), which sort
/// by that column alone, a separator, for a length or mass column a "Units" menu of its group's
/// units (the column's own one ticked; choosing one shows the column in it, never stored) and a
/// separator; then, on a column or beside the columns, a check box item per table column in the
/// model's order, ticked while the column is shown, which hides or shows it.
///
/// Legacy (the dialog's "Show Legacy Database" box, isLegacyShown()): the Legacy column and the
/// legacy filter go together. While Legacy is not shown its column is hidden and the legacy rows
/// are filtered out (ComponentPresetFilterModel::setHideLegacy()); a new table starts so, as the
/// dialog opens. setLegacyShown() is the box: it shows or hides the column (a shown column 50
/// pixels wide, as the box makes it) and the rows; the header menu's Legacy item does the same
/// without the width; both tell legacyShownChanged(), which the box follows. loadUiPreferences()
/// restores the stored layout (TableUiPreferences::load(), the columns named by their English
/// headers, ComponentPresetTableModel::columnIdentifier()), in which the Legacy column may be
/// stored as shown, and then puts the Legacy state back as it was before, as the dialog's
/// constructor does (so at open the Legacy column is hidden and the box unticked, whatever was
/// stored); storeUiPreferences() stores the layout (TableUiPreferences::store()), what the
/// dialog's Close does.
///
/// Deviations from OpenRocket:
/// - The header menu's Sort items sort by the clicked column (bug 13 of the plan's section 10.3,
///   fixed). OpenRocket's take the clicked column's view index as its model index, so with
///   Legacy hidden "Sort Ascending" on Manufacturer sorts by Legacy and on Outer Diameter by
///   Description.
/// - Showing or hiding the Legacy column through the header menu shows or hides the legacy rows
///   too, as the box does (bug 14, fixed). In OpenRocket the column listener ticks the box, whose
///   listener then sees the column already in that state and returns before it changes the
///   filter, so the box and the rows disagree. Nor does hiding another column untick the box:
///   OpenRocket's listener compares the removed column's view index with Legacy's model index 1,
///   so hiding whatever column is at view index 1 unticks the box and hides the Legacy column
///   and its rows (an unlisted OpenRocket bug, fixed with bug 14).
/// - Choosing a unit keeps the selected row; OpenRocket's fireTableDataChanged() clears the
///   selection (probe: "selected row after: -1").
/// - The columns keep their widths and the table scrolls sideways when they are wider than it
///   (QHeaderView's interactive sections); Swing's JTable (AUTO_RESIZE_SUBSEQUENT_COLUMNS) fits
///   them to its width. The favourite's check box is where Qt's style puts it, not centred as
///   Swing's Boolean renderer does.
class ComponentPresetTable final : public QTableView
{
    Q_OBJECT

public:
    using PresetPtr = std::shared_ptr<const ComponentPreset>;

    /// The width the legacy box gives the Legacy column it shows (getFilterCheckboxes()).
    static constexpr int kLegacyColumnWidth = 50;
    /// The narrowest a column can be made: Swing's TableColumn's default minimum width.
    static constexpr int kMinimumColumnWidth = 15;

    /// A table of @p presets of @p type (ComponentPresetTableModel's arguments; @p preferences
    /// keeps the favourites and the stored layout and outlives the table), sorted by
    /// Manufacturer, Legacy not shown.
    ComponentPresetTable(ComponentPresetType type, std::vector<PresetPtr> presets,
                         Preferences& preferences, QWidget* parent);
    ~ComponentPresetTable() override;

    ComponentPresetTable(const ComponentPresetTable&)            = delete;
    ComponentPresetTable& operator=(const ComponentPresetTable&) = delete;
    ComponentPresetTable(ComponentPresetTable&&)                 = delete;
    ComponentPresetTable& operator=(ComponentPresetTable&&)      = delete;

    [[nodiscard]] ComponentPresetTableModel&  tableModel() const noexcept { return *m_model; }
    [[nodiscard]] ComponentPresetFilterModel& filterModel() const noexcept { return *m_filter; }

    // ---- the columns (the model's columns, logical sections of the header)

    /// Every model column's identifier for TableUiPreferences: the English header of a table
    /// column, nullopt for the others.
    [[nodiscard]] std::span<const std::optional<std::string>> columnIdentifiers() const noexcept
    {
        return m_identifiers;
    }
    /// Whether @p column is one of the table's columns (ComponentPresetTableModel::isDisplayed()).
    [[nodiscard]] bool isTableColumn(int column) const;
    /// Whether @p column is shown (XTableColumnModel.isColumnVisible()).
    [[nodiscard]] bool isColumnVisible(int column) const;
    /// Shows or hides the table column @p column in its place (the header menu's check box
    /// items); the Legacy column with the legacy rows (see the class comment).
    /// @throws BugError for a column that is not the table's
    void setColumnVisible(int column, bool visible);
    /// The shown columns in view order (Java: the column model's columns).
    [[nodiscard]] std::vector<int> visibleColumns() const;
    /// The view index of @p column among the shown columns (convertColumnIndexToView()), -1 when
    /// it is hidden.
    [[nodiscard]] int viewColumn(int column) const;

    // ---- legacy

    /// Whether Legacy is shown (the "Show Legacy Database" box): its column and its rows.
    [[nodiscard]] bool isLegacyShown() const noexcept { return m_legacyShown; }
    /// The box ticked or unticked (see the class comment).
    void setLegacyShown(bool shown);

    // ---- the header menu

    /// Fills @p menu with the header menu of the model column @p column, or of no column for -1
    /// (a click beside the columns). Its items act on the table while it lives.
    void populateHeaderMenu(QMenu& menu, int column);

    // ---- the stored layout (the node "tables" of the preferences)

    /// Restores the layout stored under @p tableName (GuiPreferenceKeys::
    /// componentPresetTableName(): "CmpPrst.Body Tube"), then the Legacy state of before.
    void loadUiPreferences(std::string_view tableName);
    /// Stores the layout under @p tableName.
    void storeUiPreferences(std::string_view tableName);

    // ---- the selection

    /// The preset of the selected row (getSelectedComponentPreset()), null when none is.
    [[nodiscard]] PresetPtr selectedPreset() const;

Q_SIGNALS:
    /// Legacy was shown or hidden (setLegacyShown(), the header menu's Legacy item).
    void legacyShownChanged(bool shown);

protected:
    /// Rows a filter hides leave the selection: when they hold the current row, nothing is
    /// selected afterwards (see the class comment).
    void rowsAboutToBeRemoved(const QModelIndex& parent, int start, int end) override;
    /// Enter and Shift+Enter (see the class comment); the other keys are QTableView's.
    void keyPressEvent(QKeyEvent* event) override;

private:
    /// JTable's selectNextRowCell (@p forward) or selectPreviousRowCell (see the class comment).
    void moveToCell(bool forward);
    /// Shows or hides the Legacy column and its rows; legacyShownChanged() when the state changed.
    void applyLegacy(bool shown);
    void updateSortIndicator();
    /// Remembers the selected model row, or forgets it when the selection was cleared while the
    /// row is shown.
    void rememberSelection();
    /// Selects the remembered model row again when nothing is selected and the filters show it.
    void restoreSelection();
    void showHeaderMenu(const QPoint& position);

    Preferences*                            m_preferences;
    ComponentPresetTableModel*              m_model;
    ComponentPresetFilterModel*             m_filter;
    std::vector<std::optional<std::string>> m_identifiers;
    bool                                    m_legacyShown{true};
    /// The selected model row, also while the filters hide it.
    std::optional<int> m_selectedModelRow;
};

}  // namespace QtRocket::Gui
