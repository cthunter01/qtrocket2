#include "QtRocket/gui/util/TableSortKeys.h"

#include <array>
#include <compare>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/util/BugError.h"

// The expectations are the transcripts of a probe of JDK 17's TableRowSorter (a black-box Java
// program, SortKeysProbe: toggleSortOrder() and setSortKeys() on a four-column model of Integers,
// getSortKeys() after each, and the view orders, convertRowIndexToModel(), of a sort by one to
// four keys of the data below; the column comparators never see a null).

namespace QtRocket::Gui
{

namespace
{

using TableSortKeysTest = Test::GuiTest;
using SortKey           = TableSortKeys::SortKey;
using SortOrder         = TableSortKeys::SortOrder;

[[nodiscard]] SortKey ascending(int column)
{
    return {.column = column, .order = SortOrder::ASCENDING};
}

[[nodiscard]] SortKey descending(int column)
{
    return {.column = column, .order = SortOrder::DESCENDING};
}

/// The keys after each click on @p columns, in order.
[[nodiscard]] std::vector<std::vector<SortKey>> keysAfterClicks(TableSortKeys&          keys,
                                                                const std::vector<int>& columns)
{
    std::vector<std::vector<SortKey>> after;
    for (const int column : columns)
    {
        keys.toggle(column);
        after.push_back(keys.keys());
    }
    return after;
}

/// The probe's model: column 0 with ties and a null, column 1 with ties, column 2 distinct,
/// column 3 with nulls.
using Row = std::array<std::optional<int>, 4>;
constexpr std::array<Row, 6> kProbeRows{{
    {2, 1, 50, std::nullopt},
    {1, 2, 40, 7},
    {2, 2, 30, std::nullopt},
    {std::nullopt, 1, 20, 3},
    {1, 1, 10, 7},
    {2, 1, 60, 1},
}};

/// The value of the probe's model at @p row and @p column.
[[nodiscard]] const std::optional<int>& valueAt(std::span<const Row> rows, int row, int column)
{
    return rows[static_cast<std::size_t>(row)].at(static_cast<std::size_t>(column));
}

/// The view order of @p rows sorted by @p keys, a null before every value (TableRowSorter's
/// comparison of Integers).
[[nodiscard]] std::vector<int> viewOrder(std::span<const Row>        rows,
                                         const std::vector<SortKey>& keys)
{
    TableSortKeys sortKeys(4);
    sortKeys.setKeys(keys);
    std::vector<int> order;
    order.reserve(rows.size());
    for (std::size_t row = 0; row < rows.size(); ++row)
    {
        order.push_back(static_cast<int>(row));
    }
    sortKeys.sortRows(order, [rows](int column, int rowA, int rowB) {
        return TableSortKeys::compareMissingFirst(valueAt(rows, rowA, column),
                                                  valueAt(rows, rowB, column),
                                                  [](int a, int b) { return a <=> b; });
    });
    return order;
}

TEST_F(TableSortKeysTest, ANewTableHasNoKeysAndTheModelOrder)
{
    const TableSortKeys keys(4);
    EXPECT_EQ(keys.columnCount(), 4);
    EXPECT_TRUE(keys.keys().empty());
    EXPECT_EQ(keys.primaryKey(), std::nullopt);
    EXPECT_EQ(viewOrder(kProbeRows, {}), (std::vector<int>{0, 1, 2, 3, 4, 5}));
    // Without keys, the rows go into the model order whatever their order was.
    std::vector<int> rows{3, 1, 2};
    keys.sortRows(rows, [](int /*column*/, int /*rowA*/, int /*rowB*/) { return 0; });
    EXPECT_EQ(rows, (std::vector<int>{1, 2, 3}));
    EXPECT_EQ(TableSortKeys::kMaxSortKeys, 3U);
}

TEST_F(TableSortKeysTest, AClickTogglesThePrimaryKeyOrPutsItsColumnFirstAscending)
{
    TableSortKeys keys(4);
    EXPECT_EQ(keysAfterClicks(keys, {0, 0, 0, 1, 2, 3, 1, 1, 3, 0, 0, 2}),
              (std::vector<std::vector<SortKey>>{
                  {ascending(0)},
                  {descending(0)},
                  {ascending(0)},
                  {ascending(1), ascending(0)},
                  {ascending(2), ascending(1), ascending(0)},
                  // A fourth column: the keys are cut to three.
                  {ascending(3), ascending(2), ascending(1)},
                  // A later key moves to the front.
                  {ascending(1), ascending(3), ascending(2)},
                  {descending(1), ascending(3), ascending(2)},
                  {ascending(3), descending(1), ascending(2)},
                  {ascending(0), ascending(3), descending(1)},
                  {descending(0), ascending(3), descending(1)},
                  {ascending(2), descending(0), ascending(3)},
              }));
    EXPECT_EQ(keys.primaryKey(), ascending(2));
}

TEST_F(TableSortKeysTest, ClicksAfterTheMotorTablesInitialKeys)
{
    // ThrustCurveMotorSelectionPanel's initial keys: Diameter, Total Impulse, Manufacturer.
    TableSortKeys keys(4);
    keys.setKeys({ascending(2), ascending(1), ascending(0)});
    EXPECT_EQ(keys.keys(), (std::vector<SortKey>{ascending(2), ascending(1), ascending(0)}));
    EXPECT_EQ(keysAfterClicks(keys, {0, 0, 3, 2, 2}),
              (std::vector<std::vector<SortKey>>{
                  {ascending(0), ascending(2), ascending(1)},
                  {descending(0), ascending(2), ascending(1)},
                  {ascending(3), descending(0), ascending(2)},
                  {ascending(2), ascending(3), descending(0)},
                  {descending(2), ascending(3), descending(0)},
              }));
}

TEST_F(TableSortKeysTest, ADescendingKeyClickedAgainStartsAscending)
{
    TableSortKeys keys(4);
    keys.setKeys({descending(1), ascending(0)});
    EXPECT_EQ(keysAfterClicks(keys, {0, 1}), (std::vector<std::vector<SortKey>>{
                                                 {ascending(0), descending(1)},
                                                 {ascending(1), ascending(0)},
                                             }));
}

TEST_F(TableSortKeysTest, SetKeysKeepsMoreThanThreeKeysUntilTheNextClick)
{
    TableSortKeys keys(4);
    keys.setKeys({ascending(0), descending(1), ascending(2), descending(3)});
    EXPECT_EQ(keys.keys().size(), 4U);
    EXPECT_EQ(keysAfterClicks(keys, {3, 1}), (std::vector<std::vector<SortKey>>{
                                                 {ascending(3), ascending(0), descending(1)},
                                                 {ascending(1), ascending(3), ascending(0)},
                                             }));
    keys.setKeys({});
    EXPECT_TRUE(keys.keys().empty());
}

TEST_F(TableSortKeysTest, SortsByEveryKeyAndKeepsTheModelOrderOfEqualRows)
{
    // Ties keep the model order also under a descending key; a null comes first ascending and
    // last descending.
    EXPECT_EQ(viewOrder(kProbeRows, {ascending(0)}), (std::vector<int>{3, 1, 4, 0, 2, 5}));
    EXPECT_EQ(viewOrder(kProbeRows, {descending(0)}), (std::vector<int>{0, 2, 5, 1, 4, 3}));
    EXPECT_EQ(viewOrder(kProbeRows, {ascending(1)}), (std::vector<int>{0, 3, 4, 5, 1, 2}));
    EXPECT_EQ(viewOrder(kProbeRows, {descending(1)}), (std::vector<int>{1, 2, 0, 3, 4, 5}));
    EXPECT_EQ(viewOrder(kProbeRows, {ascending(3)}), (std::vector<int>{0, 2, 5, 3, 1, 4}));
    EXPECT_EQ(viewOrder(kProbeRows, {descending(3)}), (std::vector<int>{1, 4, 3, 5, 0, 2}));
    EXPECT_EQ(viewOrder(kProbeRows, {ascending(0), descending(1)}),
              (std::vector<int>{3, 1, 4, 2, 0, 5}));
    EXPECT_EQ(viewOrder(kProbeRows, {descending(1), ascending(0)}),
              (std::vector<int>{1, 2, 3, 4, 0, 5}));
    EXPECT_EQ(viewOrder(kProbeRows, {ascending(1), descending(3)}),
              (std::vector<int>{4, 3, 5, 0, 1, 2}));
    EXPECT_EQ(viewOrder(kProbeRows, {descending(0), ascending(1), descending(2)}),
              (std::vector<int>{5, 0, 2, 4, 1, 3}));
    EXPECT_EQ(viewOrder(kProbeRows, {ascending(0), descending(1), ascending(2), descending(3)}),
              (std::vector<int>{3, 1, 4, 2, 0, 5}));
}

TEST_F(TableSortKeysTest, AFourthKeySetTakesPartInTheSort)
{
    // The probe's second model: the rows are equal in columns 0 to 2 but for the last.
    const std::array<Row, 4> rows{{{1, 1, 1, 9}, {1, 1, 1, 5}, {1, 1, 1, 7}, {0, 1, 1, 8}}};
    EXPECT_EQ(viewOrder(rows, {ascending(0), ascending(1), ascending(2), ascending(3)}),
              (std::vector<int>{3, 1, 2, 0}));
    EXPECT_EQ(viewOrder(rows, {ascending(0), ascending(1), ascending(2), descending(3)}),
              (std::vector<int>{3, 0, 2, 1}));
}

TEST_F(TableSortKeysTest, ComparesRowsWithJavasIntComparatorsOrOrderings)
{
    TableSortKeys keys(2);
    keys.setKeys({descending(1)});
    const std::array<int, 3> values{5, 7, 7};
    const auto               javaCompare = [&values](int /*column*/, int rowA, int rowB) {
        return values.at(static_cast<std::size_t>(rowA)) -
               values.at(static_cast<std::size_t>(rowB));
    };
    EXPECT_EQ(keys.compareRows(0, 1, javaCompare), std::weak_ordering::greater);
    EXPECT_EQ(keys.compareRows(1, 0, javaCompare), std::weak_ordering::less);
    // Equal values: the model order.
    EXPECT_EQ(keys.compareRows(2, 1, javaCompare), std::weak_ordering::greater);
    EXPECT_EQ(keys.compareRows(1, 1, javaCompare), std::weak_ordering::equivalent);
    const auto ordering = [&values](int /*column*/, int rowA, int rowB) {
        return values.at(static_cast<std::size_t>(rowA)) <=>
               values.at(static_cast<std::size_t>(rowB));
    };
    EXPECT_EQ(keys.compareRows(0, 1, ordering), std::weak_ordering::greater);
}

TEST_F(TableSortKeysTest, AMissingValueComesFirst)
{
    const auto               compare = [](int a, int b) { return a <=> b; };
    const std::optional<int> missing;
    EXPECT_EQ(TableSortKeys::compareMissingFirst(missing, std::optional<int>(-5), compare),
              std::weak_ordering::less);
    EXPECT_EQ(TableSortKeys::compareMissingFirst(std::optional<int>(-5), missing, compare),
              std::weak_ordering::greater);
    EXPECT_EQ(TableSortKeys::compareMissingFirst(missing, missing, compare),
              std::weak_ordering::equivalent);
    EXPECT_EQ(
        TableSortKeys::compareMissingFirst(std::optional<int>(2), std::optional<int>(3), compare),
        std::weak_ordering::less);
}

TEST_F(TableSortKeysTest, AColumnOutsideTheTableOrNamedTwiceIsABug)
{
    // Java: IndexOutOfBoundsException for a click, IllegalArgumentException for a key outside the
    // table, and a duplicate kept.
    TableSortKeys keys(4);
    keys.setKeys({ascending(1)});
    EXPECT_THROW(keys.toggle(-1), BugError);
    EXPECT_THROW(keys.toggle(4), BugError);
    EXPECT_THROW(keys.setKeys({ascending(4)}), BugError);
    EXPECT_THROW(keys.setKeys({ascending(0), descending(0)}), BugError);
    EXPECT_EQ(keys.keys(), std::vector<SortKey>{ascending(1)});
    EXPECT_THROW({ const TableSortKeys negative(-1); }, BugError);
}

}  // namespace

}  // namespace QtRocket::Gui
