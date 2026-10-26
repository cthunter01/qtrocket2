#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <vector>

#include "QtRocket/motor/chooser/ThrustCurveMotorColumns.h"

namespace QtRocket
{

class Preferences;
class ThrustCurveMotorDatabaseModel;

/// The order of the motor chooser's table: the sort keys of the Swing TableRowSorter OpenRocket's
/// ThrustCurveMotorSelectionPanel sorts the table with (ThrustCurveMotorSelectionPanel.java
/// :346-361), and the order they give. Each column sorts with its comparator
/// (ThrustCurveMotorColumns' compareValues()).
///
/// The behaviour of the JDK's DefaultRowSorter is reimplemented from its documentation and from
/// what OpenRocket's chooser shows (the probes of M3's motor-chooser pins), not from its source:
/// - The keys are compared in turn, the first that tells two rows apart deciding, its order
///   reversed for a descending key. Rows equal on every key keep the model's order (the lower model
///   index first), so the order is total and stable.
/// - toggleSortOrder(column), a click on a column's header: when the column is the primary key, its
///   order is reversed; otherwise the column becomes the primary key, ascending, and its old key
///   (if it had one) is removed. Then the keys are cut to the first kMaxSortKeys (3).
/// - setSortKeys() takes the keys as given, longer lists included ("You can specify more sort keys
///   by invoking setSortKeys directly and they will all be honored").
///
/// The chooser starts with the keys Diameter, Total Impulse, Manufacturer, all ascending
/// (kInitialSortKeys).
class MotorTableSorter
{
public:
    /// SortOrder: ascending or descending (Swing's UNSORTED, which no key of the chooser has, is
    /// not ported).
    enum class SortOrder
    {
        ASCENDING,
        DESCENDING,
    };

    /// RowSorter.SortKey: a column and its order.
    struct SortKey
    {
        ThrustCurveMotorColumns column;
        SortOrder               sortOrder;

        [[nodiscard]] bool operator==(const SortKey& other) const noexcept = default;
    };

    /// DefaultRowSorter's default maximum number of sort keys, which toggleSortOrder() enforces.
    static constexpr std::size_t kMaxSortKeys = 3;

    /// The chooser's keys at first: Diameter, Total Impulse, Manufacturer, all ascending.
    static constexpr std::array<SortKey, 3> kInitialSortKeys{{
        {.column = ThrustCurveMotorColumns::DIAMETER, .sortOrder = SortOrder::ASCENDING},
        {.column = ThrustCurveMotorColumns::TOTAL_IMPULSE, .sortOrder = SortOrder::ASCENDING},
        {.column = ThrustCurveMotorColumns::MANUFACTURER, .sortOrder = SortOrder::ASCENDING},
    }};

    /// A sorter with kInitialSortKeys.
    MotorTableSorter();

    /// The sort keys, the primary one first.
    [[nodiscard]] const std::vector<SortKey>& getSortKeys() const noexcept { return m_sortKeys; }

    /// Replaces the sort keys by @p sortKeys, all of them.
    void setSortKeys(std::span<const SortKey> sortKeys);

    /// A click on the header of @p column, see the class comment.
    void toggleSortOrder(ThrustCurveMotorColumns column);

    /// DefaultRowSorter.compare(): negative when the model row @p rowA sorts before @p rowB,
    /// positive when after; 0 only for the same row.
    /// @throws BugError when a row is not a row of @p model.
    [[nodiscard]] int compare(std::size_t rowA, std::size_t rowB,
                              const ThrustCurveMotorDatabaseModel& model,
                              const Preferences&                   preferences) const;

    /// The model rows @p modelRows (the rows the filter keeps, in any order) in the table's order:
    /// the view-to-model index of the table.
    /// @throws BugError when a row is not a row of @p model.
    [[nodiscard]] std::vector<std::size_t> sort(std::span<const std::size_t>         modelRows,
                                                const ThrustCurveMotorDatabaseModel& model,
                                                const Preferences& preferences) const;

private:
    std::vector<SortKey> m_sortKeys;
};

}  // namespace QtRocket
