#pragma once

#include <algorithm>
#include <compare>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <vector>

namespace QtRocket::Gui
{

/// The sort keys of a sortable table and the row order they give: what Swing's
/// DefaultRowSorter (TableRowSorter) keeps for OpenRocket's sorted tables, the motor chooser's,
/// the parts library's and Milestone 4's simulation table. A column header click is toggle(), a
/// "Sort Ascending" / "Sort Descending" menu item setKeys() of one key; a table model sorts its
/// view order with sortRows() and the column comparators of its own. Qt-free, and no
/// QSortFilterProxyModel: Qt's sorts on one column.
///
/// The rules, reimplemented from the JDK's documentation (DefaultRowSorter.toggleSortOrder(),
/// setSortKeys(), setMaxSortKeys()) and from a probe of JDK 17's TableRowSorter (click sequences,
/// set keys and the view orders they give; no JDK source was used):
/// - toggle(column): the column of the first key (the primary key) changes its order, ascending
///   to descending and back; any other column becomes the first key, ascending (also when it was
///   a later key, descending: it starts again ascending), removed from where it was; then, after
///   either, the keys are cut to kMaxSortKeys (3, DefaultRowSorter's default; OpenRocket never
///   changes it).
/// - setKeys(): the keys as given, in order, also more than kMaxSortKeys (Java keeps them all
///   and sorts by all of them; the next toggle() cuts them, of the primary key's column too).
/// - The order: the first key's column decides, then the next key's for rows it calls equal,
///   and so on; rows that every key calls equal keep their model order, in a descending key too
///   (the model index is the last key, always ascending). A descending key reverses its column's
///   comparison, and only that.
/// - A missing value (Java's null, which the sorter decides itself before it calls the column's
///   comparator) comes before every value, and two missing values are equal: compareMissingFirst()
///   (so a descending key puts the missing values last).
///
/// Deviations: a column outside the table or one named twice in setKeys() is a bug (BugError;
/// Java throws IllegalArgumentException or IndexOutOfBoundsException for a column outside the
/// table, and keeps a duplicate, after which a toggle of that column misbehaves). SortOrder has no
/// UNSORTED (no table of OpenRocket's sets one; a toggle of an UNSORTED primary key gives
/// ASCENDING), and every column is sortable (setSortable() is never called).
class TableSortKeys
{
public:
    /// Swing's SortOrder, without UNSORTED (see the class comment).
    enum class SortOrder
    {
        ASCENDING,
        DESCENDING,
    };

    /// RowSorter.SortKey: a column and its order.
    struct SortKey
    {
        int       column = 0;
        SortOrder order  = SortOrder::ASCENDING;

        [[nodiscard]] bool operator==(const SortKey& other) const noexcept = default;
    };

    /// DefaultRowSorter's default maximum of sort keys, to which toggle() cuts the keys.
    static constexpr std::size_t kMaxSortKeys = 3;

    /// No keys (the model order), for a table of @p columnCount columns (at least 0: a negative
    /// count is a BugError).
    explicit TableSortKeys(int columnCount);

    /// The table's number of columns.
    [[nodiscard]] int columnCount() const noexcept { return m_columnCount; }

    /// The keys, the primary key first (getSortKeys()); none for the model order.
    [[nodiscard]] const std::vector<SortKey>& keys() const noexcept { return m_keys; }

    /// The first key (the column whose header shows the sort arrow), or nullopt without keys.
    [[nodiscard]] std::optional<SortKey> primaryKey() const noexcept;

    /// setSortKeys(@p keys): replaces the keys (none for the model order).
    /// @throws BugError for a column outside the table or a column named twice.
    void setKeys(std::vector<SortKey> keys);

    /// toggleSortOrder(@p column): a click on the column's header (see the class comment).
    /// @throws BugError for a column outside the table.
    void toggle(int column);

    /// How the keys order the model rows @p rowA and @p rowB: less when @p rowA comes first.
    /// @p compare(column, rowA, rowB) compares the two rows' values in a column, ascending, as
    /// the column's comparator does (an integer as Java's: negative, 0 or positive; or a
    /// std::weak_ordering or std::strong_ordering), consistently (a strict weak order); this
    /// applies the keys' orders and the model order of equal rows.
    template <class Compare>
    [[nodiscard]] std::weak_ordering compareRows(int rowA, int rowB, const Compare& compare) const
    {
        for (const SortKey& key : m_keys)
        {
            const std::weak_ordering result = toOrdering(compare(key.column, rowA, rowB));
            if (std::is_neq(result))
            {
                return key.order == SortOrder::ASCENDING ? result : reversed(result);
            }
        }
        return rowA <=> rowB;
    }

    /// Sorts @p rows (model row indices, each once: the rows a filter lets through) into the view
    /// order of the keys (compareRows()): without keys, the model order.
    template <class Compare>
    void sortRows(std::vector<int>& rows, const Compare& compare) const
    {
        std::ranges::sort(rows, [this, &compare](int rowA, int rowB) {
            return std::is_lt(compareRows(rowA, rowB, compare));
        });
    }

    /// DefaultRowSorter's rule for missing values (see the class comment): @p a and @p b
    /// compared ascending, a missing value first and two missing values equal, two values by
    /// @p compare(a, b) (an integer as Java's, or a weak or strong ordering).
    template <class T, class Compare>
    [[nodiscard]] static std::weak_ordering compareMissingFirst(const std::optional<T>& a,
                                                                const std::optional<T>& b,
                                                                const Compare&          compare)
    {
        if (!a.has_value() || !b.has_value())
        {
            return a.has_value() <=> b.has_value();
        }
        return toOrdering(compare(*a, *b));
    }

private:
    /// @p result of a comparator as an ordering: a Java-style int by its sign (a comparator gives
    /// an integer, a std::weak_ordering or a std::strong_ordering).
    template <class Result>
    [[nodiscard]] static constexpr std::weak_ordering toOrdering(const Result& result) noexcept
    {
        if constexpr (std::is_integral_v<Result>)
        {
            return result <=> Result{};
        }
        else
        {
            return result;
        }
    }

    /// @p ordering the other way round (a descending key).
    [[nodiscard]] static constexpr std::weak_ordering reversed(std::weak_ordering ordering) noexcept
    {
        if (std::is_lt(ordering))
        {
            return std::weak_ordering::greater;
        }
        return std::is_gt(ordering) ? std::weak_ordering::less : std::weak_ordering::equivalent;
    }

    /// A BugError unless @p column is one of the table's.
    void checkColumn(int column) const;

    int                  m_columnCount;
    std::vector<SortKey> m_keys;
};

}  // namespace QtRocket::Gui
