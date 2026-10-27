#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class QHeaderView;
class QTableView;

namespace QtRocket
{
class Preferences;
}

namespace QtRocket::Gui
{

/// The column widths, order and visibility of a table, stored in the preferences and restored
/// (OpenRocket: gui/util/TableUIPreferences.java, with GUIUtil.computeOptimalColumnWidths(),
/// GUIUtil.java:526-565). The parts library stores its table's columns when it is closed with
/// its Close button and restores them when it opens (ComponentPresetChooserDialog: the table
/// named GuiPreferenceKeys::componentPresetTableName()).
///
/// The keys, in the preferences' node "tables" (GuiPreferenceKeys::kNodeTables), for a table
/// named T and a column whose identifier is C: T + ".cw." + C, the column's width in pixels;
/// T + ".co." + C, its view index (its place among the shown columns, from 0); T + ".cv." + C,
/// whether it is shown ("true", the default, or "false") (GuiPreferenceKeys::tableColumnWidthKey(),
/// tableColumnOrderKey(), tableColumnVisibilityKey()).
///
/// The columns: Swing's table has a column model (XTableColumnModel) whose columns can be hidden
/// and shown again in their place and moved by the user; columns that the table never shows are
/// not in it at all (ComponentPresetTable removes the columns of the keys the preset type does not
/// display). Here a QTableView's horizontal header holds every column of the model; the caller
/// names the table's columns by giving each logical column its identifier (Swing's
/// TableColumn.getIdentifier(), the column's header text: QtRocket passes the English header,
/// so that the keys do not change with the language, as componentPresetTableName() uses the
/// English component name), and nullopt for a column that is not one of the table's (Java's
/// removed column): such a column is hidden by load() and never stored. A column of the table
/// that is hidden is Swing's invisible column. The header's visual order of the table's columns,
/// hidden ones included, is the order of XTableColumnModel's list of all columns, which moving a
/// shown column (moveColumn()) changes as QHeaderView::moveSection() changes the visual order, so
/// that a column shown again comes back where Swing puts it.
///
/// Deviations from OpenRocket, each where Java would fail or Qt has no counterpart:
/// - A stored view index below 0 is ignored; Swing's moveColumn() throws an
///   IllegalArgumentException, which aborts the dialog's construction.
/// - A width is set within the header's minimum and maximum section sizes (QHeaderView's
///   minimumSectionSize() and maximumSectionSize()); Swing's setPreferredWidth() keeps it within
///   the column's own minimum (15 pixels by default) and maximum. A column whose header section
///   is of the resize mode QHeaderView::Fixed keeps its width, as a Swing column whose minimum
///   and maximum width are equal does (the parts library's favourite column).
/// - The widths are Qt's: the header's QHeaderView::sectionSizeHint() and the delegate's size
///   hint of every row (QTableView::sizeHintForColumn() over all rows) in place of the Swing
///   renderers' preferred widths, so the optimal widths differ by the platform's text metrics,
///   as Java's do between look and feels.
class TableUiPreferences final
{
public:
    TableUiPreferences() = delete;

    /// GUIUtil.computeOptimalColumnWidths(table, max, true) as TableUIPreferences calls it: max
    /// 20, a column at most 20 * 100 = 2000 pixels wide.
    static constexpr int kOptimalWidthMax = 20;

    /// TableUIPreferences.storeTableUIPreferences(table, tableName, node "tables"): for each shown
    /// column of the table, in view order, its width (QHeaderView::sectionSize()) and its view
    /// index; for each column of the table, hidden ones too, whether it is shown. Writes into the
    /// node "tables" of @p preferences (created as needed).
    /// @param identifiers the identifier of each logical column of @p table's model, or nullopt
    ///        for a column that is not one of the table's (see the class comment)
    /// @throws BugError when @p identifiers does not have one entry per column of the header
    static void store(const QTableView&                           table,
                      std::span<const std::optional<std::string>> identifiers,
                      std::string_view tableName, Preferences& preferences);

    /// TableUIPreferences.loadTableUIPreferences(table, tableName, node "tables"):
    /// 1. every column without an identifier is hidden (Java's removed columns are not in the
    ///    table's column model);
    /// 2. each column of the table is shown or hidden as its ".cv." key says (shown when absent
    ///    or not a boolean);
    /// 3. the order: for each view index i from the first to the last, the column at i then (the
    ///    moves before it may have changed which one that is) is moved to its stored ".co." index
    ///    when that is another one inside the shown columns, in one pass (so a stored order is not
    ///    always restored exactly, as in Java);
    /// 4. the widths: when a shown column has no ".cw." key, the optimal widths of the shown
    ///    columns are computed (computeOptimalColumnWidths() with kOptimalWidthMax and the
    ///    header); then each shown column gets its stored width, or else its optimal width (or
    ///    its current width when no key is missing and the stored text is not an integer); see the
    ///    class comment for the bounds and the fixed columns.
    /// Nothing is written. A missing node "tables" reads as an empty one.
    /// @throws BugError when @p identifiers does not have one entry per column of the header
    static void load(QTableView& table, std::span<const std::optional<std::string>> identifiers,
                     std::string_view tableName, const Preferences& preferences);

    /// GUIUtil.computeOptimalColumnWidths(table, max, includeHeaderWidth) for the logical
    /// columns @p columns (Java: every column of the view, in view order): for each, the widest
    /// of its header's size hint (when @p includeHeaderWidth; otherwise at least 1) and the size
    /// hints of the cells of every row of the view (QTableView::sizeHintForColumn() over all rows,
    /// whatever the header's resizeContentsPrecision()), then at most @p max * 100 pixels.
    [[nodiscard]] static std::vector<int> computeOptimalColumnWidths(QTableView&          table,
                                                                     std::span<const int> columns,
                                                                     int                  max,
                                                                     bool includeHeaderWidth);

    /// The logical indices of the shown columns of the table, in view order: the sections of
    /// @p header in visual order that have an identifier and are not hidden (Java: the columns
    /// of the column model).
    /// @throws BugError when @p identifiers does not have one entry per column of the header
    [[nodiscard]] static std::vector<int> visibleColumns(
        const QHeaderView& header, std::span<const std::optional<std::string>> identifiers);
};

}  // namespace QtRocket::Gui
