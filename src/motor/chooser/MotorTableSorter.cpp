#include "QtRocket/motor/chooser/MotorTableSorter.h"

#include <algorithm>
#include <cstddef>
#include <span>
#include <vector>

#include "QtRocket/motor/chooser/ThrustCurveMotorColumns.h"
#include "QtRocket/motor/chooser/ThrustCurveMotorDatabaseModel.h"

namespace QtRocket
{

MotorTableSorter::MotorTableSorter() : m_sortKeys(kInitialSortKeys.begin(), kInitialSortKeys.end())
{
}

void MotorTableSorter::setSortKeys(std::span<const SortKey> sortKeys)
{
    m_sortKeys.assign(sortKeys.begin(), sortKeys.end());
}

void MotorTableSorter::toggleSortOrder(ThrustCurveMotorColumns column)
{
    const auto isColumn = [column](const SortKey& key) { return key.column == column; };
    if (!m_sortKeys.empty() && isColumn(m_sortKeys.front()))
    {
        // The primary key: its order reversed.
        SortKey& primary  = m_sortKeys.front();
        primary.sortOrder = primary.sortOrder == SortOrder::ASCENDING ? SortOrder::DESCENDING
                                                                      : SortOrder::ASCENDING;
    }
    else
    {
        // Another column: it becomes the primary key, ascending, its old key removed.
        std::erase_if(m_sortKeys, isColumn);
        m_sortKeys.insert(m_sortKeys.begin(),
                          {.column = column, .sortOrder = SortOrder::ASCENDING});
    }
    if (m_sortKeys.size() > kMaxSortKeys)
    {
        m_sortKeys.resize(kMaxSortKeys);
    }
}

int MotorTableSorter::compare(std::size_t rowA, std::size_t rowB,
                              const ThrustCurveMotorDatabaseModel& model,
                              const Preferences&                   preferences) const
{
    const ThrustCurveMotorSet& setA = model.getMotorSet(rowA);
    const ThrustCurveMotorSet& setB = model.getMotorSet(rowB);
    for (const SortKey& key : m_sortKeys)
    {
        int result = compareValues(key.column, setA, setB, preferences);
        if (result != 0)
        {
            // The sign only: a comparator's own value may be any int.
            result = result < 0 ? -1 : 1;
            return key.sortOrder == SortOrder::DESCENDING ? -result : result;
        }
    }
    // Equal on every key: the model's order.
    if (rowA == rowB)
    {
        return 0;
    }
    return rowA < rowB ? -1 : 1;
}

std::vector<std::size_t> MotorTableSorter::sort(std::span<const std::size_t>         modelRows,
                                                const ThrustCurveMotorDatabaseModel& model,
                                                const Preferences& preferences) const
{
    std::vector<std::size_t> rows(modelRows.begin(), modelRows.end());
    // A stable sort: it also stays safe should a comparator not be a strict weak order.
    std::ranges::stable_sort(
        rows, [&](std::size_t a, std::size_t b) { return compare(a, b, model, preferences) < 0; });
    return rows;
}

}  // namespace QtRocket
