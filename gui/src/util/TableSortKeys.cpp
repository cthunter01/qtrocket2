#include "QtRocket/gui/util/TableSortKeys.h"

#include <cstddef>
#include <format>
#include <optional>
#include <utility>
#include <vector>

#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

TableSortKeys::TableSortKeys(int columnCount) : m_columnCount(columnCount)
{
    QTROCKET_ASSERT(columnCount >= 0);
}

std::optional<TableSortKeys::SortKey> TableSortKeys::primaryKey() const noexcept
{
    if (m_keys.empty())
    {
        return std::nullopt;
    }
    return m_keys.front();
}

void TableSortKeys::setKeys(std::vector<SortKey> keys)
{
    std::vector<bool> named(static_cast<std::size_t>(m_columnCount), false);
    for (const SortKey& key : keys)
    {
        checkColumn(key.column);
        const auto column = static_cast<std::size_t>(key.column);
        if (named[column])
        {
            bug(std::format("TableSortKeys: column {} has two sort keys", key.column));
        }
        named[column] = true;
    }
    m_keys = std::move(keys);
}

void TableSortKeys::toggle(int column)
{
    checkColumn(column);
    if (!m_keys.empty() && m_keys.front().column == column)
    {
        SortKey& primary = m_keys.front();
        primary.order =
            primary.order == SortOrder::ASCENDING ? SortOrder::DESCENDING : SortOrder::ASCENDING;
    }
    else
    {
        std::erase_if(m_keys, [column](const SortKey& key) { return key.column == column; });
        m_keys.insert(m_keys.begin(), SortKey{.column = column, .order = SortOrder::ASCENDING});
    }
    // Every click cuts the keys, a click on the primary key too (keys setKeys() gave beyond the
    // maximum go with it, as JDK 17's TableRowSorter drops them).
    if (m_keys.size() > kMaxSortKeys)
    {
        m_keys.resize(kMaxSortKeys);
    }
}

void TableSortKeys::checkColumn(int column) const
{
    if (column < 0 || column >= m_columnCount)
    {
        bug(std::format("TableSortKeys: column {} is outside the table's {} columns", column,
                        m_columnCount));
    }
}

}  // namespace QtRocket::Gui
