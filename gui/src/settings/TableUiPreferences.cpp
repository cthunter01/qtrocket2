#include "QtRocket/gui/settings/TableUiPreferences.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <QAbstractItemView>
#include <QHeaderView>
#include <QTableView>

#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

using Identifiers = std::span<const std::optional<std::string>>;

/// The identifier of the logical column @p logical.
/// @throws BugError when the column is not one of the table's (its identifier is nullopt)
[[nodiscard]] const std::string& identifierOf(Identifiers identifiers, int logical)
{
    const std::optional<std::string>& identifier = identifiers[static_cast<std::size_t>(logical)];
    if (!identifier.has_value())
    {
        bug("TableUiPreferences: a column that is not the table's has no identifier");
    }
    return *identifier;
}

/// Whether the logical column @p logical is one of the table's columns.
[[nodiscard]] bool isTableColumn(Identifiers identifiers, int logical)
{
    return identifiers[static_cast<std::size_t>(logical)].has_value();
}

/// @throws BugError when @p identifiers does not name every column of @p header
void checkIdentifiers(const QHeaderView& header, Identifiers identifiers)
{
    if (identifiers.size() != static_cast<std::size_t>(header.count()))
    {
        bug("TableUiPreferences: one identifier (or nullopt) per column of the table is needed");
    }
}

/// The int under @p key of @p node, or @p defaultValue when the node, the key or an integer is
/// missing (Java: Preferences.getInt()).
[[nodiscard]] int getInt(const Preferences* node, const std::string& key, int defaultValue)
{
    return node != nullptr ? node->getInt(key, defaultValue) : defaultValue;
}

/// Whether @p node holds @p key (Java: preferences.get(key, null) != null).
[[nodiscard]] bool hasKey(const Preferences* node, const std::string& key)
{
    return node != nullptr && node->get(key).has_value();
}

/// Moves the shown column at view index @p from to view index @p to (Swing:
/// TableColumnModel.moveColumn(from, to) among the shown columns): the section lands right after
/// the shown column now at @p to when it moves right, right before it when it moves left, so the
/// hidden columns between shown ones keep their places relative to those (XTableColumnModel's
/// list of all columns).
void moveShownColumn(QHeaderView& header, std::span<const int> shown, int from, int to)
{
    const int fromVisual = header.visualIndex(shown[static_cast<std::size_t>(from)]);
    const int toVisual   = header.visualIndex(shown[static_cast<std::size_t>(to)]);
    header.moveSection(fromVisual, toVisual);
}

/// Step 3 of load(): the order, in Java's one pass.
void restoreOrder(QHeaderView& header, Identifiers identifiers, std::string_view tableName,
                  const Preferences* node)
{
    const int count =
        static_cast<int>(TableUiPreferences::visibleColumns(header, identifiers).size());
    for (int i = 0; i < count; ++i)
    {
        // The column at i now: an earlier move may have put another one there.
        const std::vector<int> shown  = TableUiPreferences::visibleColumns(header, identifiers);
        const int              column = shown[static_cast<std::size_t>(i)];
        const int              stored = getInt(
            node,
            GuiPreferenceKeys::tableColumnOrderKey(tableName, identifierOf(identifiers, column)),
            i);
        // Java: storedOrder != i && storedOrder < getColumnCount(); a negative index, for which
        // Swing throws, is ignored.
        if (stored != i && stored >= 0 && stored < count)
        {
            moveShownColumn(header, shown, i, stored);
        }
    }
}

/// Step 4 of load(): the widths.
void restoreWidths(QTableView& table, Identifiers identifiers, std::string_view tableName,
                   const Preferences* node)
{
    QHeaderView&           header = *table.horizontalHeader();
    const std::vector<int> shown  = TableUiPreferences::visibleColumns(header, identifiers);
    const bool             computeOptimalWidths = std::ranges::any_of(shown, [&](int column) {
        return !hasKey(node, GuiPreferenceKeys::tableColumnWidthKey(
                                 tableName, identifierOf(identifiers, column)));
    });
    std::vector<int>       optimalWidths;
    if (computeOptimalWidths)
    {
        optimalWidths = TableUiPreferences::computeOptimalColumnWidths(
            table, shown, TableUiPreferences::kOptimalWidthMax, true);
    }
    for (std::size_t i = 0; i < shown.size(); ++i)
    {
        const int column = shown[i];
        const int defaultWidth =
            computeOptimalWidths ? optimalWidths[i] : header.sectionSize(column);
        const int width = getInt(
            node,
            GuiPreferenceKeys::tableColumnWidthKey(tableName, identifierOf(identifiers, column)),
            defaultWidth);
        // Swing's setPreferredWidth() keeps the width within the column's minimum and maximum: a
        // column whose minimum and maximum are equal keeps its width (a fixed section here).
        if (header.sectionResizeMode(column) == QHeaderView::Fixed)
        {
            continue;
        }
        header.resizeSection(
            column, std::clamp(width, header.minimumSectionSize(), header.maximumSectionSize()));
    }
}

}  // namespace

std::vector<int> TableUiPreferences::visibleColumns(
    const QHeaderView& header, std::span<const std::optional<std::string>> identifiers)
{
    checkIdentifiers(header, identifiers);
    std::vector<int> columns;
    for (int visual = 0; visual < header.count(); ++visual)
    {
        const int logical = header.logicalIndex(visual);
        if (isTableColumn(identifiers, logical) && !header.isSectionHidden(logical))
        {
            columns.push_back(logical);
        }
    }
    return columns;
}

void TableUiPreferences::store(const QTableView&                           table,
                               std::span<const std::optional<std::string>> identifiers,
                               std::string_view tableName, Preferences& preferences)
{
    const QHeaderView&     header = *table.horizontalHeader();
    const std::vector<int> shown  = visibleColumns(header, identifiers);
    Preferences&           node   = preferences.getNode(GuiPreferenceKeys::kNodeTables);
    // Java: the widths of the shown columns, then their view indices, then the visibility of every
    // column of the column model.
    for (const int column : shown)
    {
        node.putInt(
            GuiPreferenceKeys::tableColumnWidthKey(tableName, identifierOf(identifiers, column)),
            header.sectionSize(column));
    }
    for (std::size_t i = 0; i < shown.size(); ++i)
    {
        node.putInt(
            GuiPreferenceKeys::tableColumnOrderKey(tableName, identifierOf(identifiers, shown[i])),
            static_cast<int>(i));
    }
    for (int visual = 0; visual < header.count(); ++visual)
    {
        const int logical = header.logicalIndex(visual);
        if (isTableColumn(identifiers, logical))
        {
            node.putBoolean(GuiPreferenceKeys::tableColumnVisibilityKey(
                                tableName, identifierOf(identifiers, logical)),
                            !header.isSectionHidden(logical));
        }
    }
}

void TableUiPreferences::load(QTableView&                                 table,
                              std::span<const std::optional<std::string>> identifiers,
                              std::string_view tableName, const Preferences& preferences)
{
    QHeaderView& header = *table.horizontalHeader();
    checkIdentifiers(header, identifiers);
    const Preferences* const node = preferences.findNode(GuiPreferenceKeys::kNodeTables);

    // Java first adds back every column of the column model that the view does not show, which
    // finds none (it walks the shown columns themselves). Here the columns that are not the
    // table's are hidden, Java's removed columns.
    for (int logical = 0; logical < header.count(); ++logical)
    {
        if (!isTableColumn(identifiers, logical))
        {
            header.setSectionHidden(logical, true);
        }
    }

    // The visibility of every column of the table; a column shown again keeps its visual place.
    for (int logical = 0; logical < header.count(); ++logical)
    {
        if (isTableColumn(identifiers, logical))
        {
            const bool visible =
                node == nullptr
                    ? GuiPreferenceKeys::kTableColumnVisibleDefault
                    : node->getBoolean(GuiPreferenceKeys::tableColumnVisibilityKey(
                                           tableName, identifierOf(identifiers, logical)),
                                       GuiPreferenceKeys::kTableColumnVisibleDefault);
            header.setSectionHidden(logical, !visible);
        }
    }

    restoreOrder(header, identifiers, tableName, node);
    restoreWidths(table, identifiers, tableName, node);
}

std::vector<int> TableUiPreferences::computeOptimalColumnWidths(QTableView&          table,
                                                                std::span<const int> columns,
                                                                int max, bool includeHeaderWidth)
{
    QHeaderView& header = *table.horizontalHeader();
    // QTableView::sizeHintForColumn() reads as many rows as the header's
    // resizeContentsPrecision() says (1000 by default); Java reads every row.
    const int precision = header.resizeContentsPrecision();
    header.setResizeContentsPrecision(-1);
    std::vector<int> widths;
    widths.reserve(columns.size());
    // QTableView's sizeHintForColumn() is protected; QAbstractItemView's is public and virtual.
    const QAbstractItemView& view = table;
    for (const int column : columns)
    {
        // Java starts every width at 1 and replaces it with the header's when that is included.
        const int headerWidth = includeHeaderWidth ? header.sectionSizeHint(column) : 1;
        const int width       = std::max(headerWidth, view.sizeHintForColumn(column));
        widths.push_back(std::min(width, max * 100));
    }
    header.setResizeContentsPrecision(precision);
    return widths;
}

}  // namespace QtRocket::Gui
