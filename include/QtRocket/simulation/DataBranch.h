#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <deque>
#include <format>
#include <functional>
#include <initializer_list>
#include <limits>
#include <optional>
#include <source_location>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "QtRocket/simulation/DataType.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Mutable.h"

namespace QtRocket
{

/// What DataBranch needs of its data types, the Java methods it relies on: equals() and
/// hashCode() (its map keys) and compareTo() (getTypes() sorts with it).
template <class T>
concept BranchDataType = std::derived_from<T, DataType> && requires(const T& a, const T& b) {
    { a.equals(b) } -> std::convertible_to<bool>;
    { a.hashCode() } -> std::convertible_to<int>;
    { a.compareTo(b) } -> std::convertible_to<int>;
};

/// A branch of data (OpenRocket's simulation/DataBranch): a column of doubles per data type, all
/// columns of the same length, one row per data point, plus the running minimum and maximum of
/// every column. FlightDataBranch builds on it with FlightDataType.
///
/// Rows (two phases, as in OpenRocket): addPoint() opens a new row with NaN in every column, then
/// setValue() writes the last row of one column. A type set for the first time gets a new column
/// that is NaN in every earlier row. setValue() before the first addPoint() writes no value but
/// still updates the minimum and maximum.
///
/// Keys: the columns are keyed by the address of their type (const T*) and keep the order the
/// types were added in. As in Java, whose map compares keys with equals() and hashCode(), a type
/// that is a different object but equal to a key (for FlightDataType: the same name ignoring
/// case, such as the replacement FlightDataType::getType() makes when a custom expression's unit
/// changes) finds that key's column. The types must outlive the branch; FlightDataTypes live for
/// the whole process.
///
/// Immutability (Java: Mutable): after immute() every change (addType(), addPoint(), setValue())
/// throws BugError naming where immute() was called. A copy (or move) keeps the source's
/// immutability and ModId; clone() gives a mutable copy.
///
/// Monitoring (Java: Monitorable): modId() is ModId::invalid() until the first change and is
/// redrawn on every change.
///
/// Programming errors throw BugError where Java throws IllegalArgumentException or
/// IllegalStateException: an empty type list given to the constructor, a type added twice, an
/// index beyond the last row, getTypes() of a branch without types.
///
/// Deviation: Java's addType() neither checks mutability nor draws a ModId, and gives the new
/// column no rows even when the others have some (a column shorter than the branch); here it is a
/// change like setValue(): checked, a fresh ModId, and NaN in every existing row.
template <BranchDataType T>
class DataBranch
{
public:
    /// A branch without types (Java: DataBranch(String name)); setValue() adds them.
    explicit DataBranch(std::string name) : m_name(std::move(name)) { }

    /// A branch with columns for @p types, in that order (Java: DataBranch(String, T...)).
    /// @throws BugError when @p types is empty, holds a null pointer or holds a type twice
    DataBranch(std::string name, std::span<const T* const> types) : m_name(std::move(name))
    {
        if (types.empty())
        {
            bug("Must specify at least one data type.");
        }
        for (const T* type : types)
        {
            QTROCKET_ASSERT(type != nullptr);
            addColumn(*type);
        }
    }

    /// The same with the types written in place: DataBranch("b", {timeType, altitudeType}).
    DataBranch(std::string name, std::initializer_list<std::reference_wrapper<const T>> types)
      : m_name(std::move(name))
    {
        if (types.size() == 0)
        {
            bug("Must specify at least one data type.");
        }
        for (const T& type : types)
        {
            addColumn(type);
        }
    }

    /// Adds a column for @p type, NaN in every existing row.
    /// @throws BugError when the branch is immutable or already has the type
    void addType(const T& type)
    {
        m_mutable.check();
        addColumn(type);
        m_modId = ModId{};
    }

    /// Opens a new row: NaN in every column (Java: addPoint()).
    /// @throws BugError when the branch is immutable
    void addPoint()
    {
        m_mutable.check();
        for (Column& column : m_columns)
        {
            column.values.push_back(std::numeric_limits<double>::quiet_NaN());
        }
        m_modId = ModId{};
    }

    /// Writes @p value to the last row of @p type's column, adding the column when the branch
    /// does not have it yet, and updates the column's minimum and maximum: a NaN value becomes
    /// the minimum and maximum only while they are NaN (Java: setValue()).
    /// @throws BugError when the branch is immutable
    void setValue(const T& type, double value)
    {
        m_mutable.check();

        const std::optional<std::size_t> index = indexOf(type);
        Column& column = index.has_value() ? m_columns.at(*index) : addColumn(type);

        if (!column.values.empty())
        {
            column.values.back() = value;
        }
        if (std::isnan(column.minimum) || value < column.minimum)
        {
            column.minimum = value;
        }
        if (std::isnan(column.maximum) || value > column.maximum)
        {
            column.maximum = value;
        }
        m_modId = ModId{};
    }

    /// Whether the branch has a column for @p type (or a type equal to it).
    [[nodiscard]] bool hasType(const T& type) const { return indexOf(type).has_value(); }

    /// A copy of @p type's values, or nullopt when the branch does not have the type (Java:
    /// get(), which returns a clone or null).
    [[nodiscard]] std::optional<std::vector<double>> get(const T& type) const
    {
        const std::optional<std::size_t> index = indexOf(type);
        if (!index.has_value())
        {
            return std::nullopt;
        }
        return m_columns.at(*index).values;
    }

    /// @p type's values themselves, read-only, or null when the branch does not have the type
    /// (Java: getView()). The vector is live: it grows with addPoint() and changes with
    /// setValue(), and stays valid (at the same address) while the branch lives, even when other
    /// columns are added. Hold it no longer than the branch, and take no iterators or spans into
    /// it across an addPoint().
    [[nodiscard]] const std::vector<double>* getView(const T& type) const
    {
        const std::optional<std::size_t> index = indexOf(type);
        return index.has_value() ? &m_columns.at(*index).values : nullptr;
    }

    /// The value of @p type in row @p index, or nullopt when the branch does not have the type
    /// (Java: getByIndex()).
    /// @throws BugError when @p index is not below getLength()
    [[nodiscard]] std::optional<double> getByIndex(const T& type, std::size_t index) const
    {
        if (index >= getLength())
        {
            bug(std::format("Index {} out of bounds (length {})", index, getLength()));
        }
        const std::optional<std::size_t> column = indexOf(type);
        if (!column.has_value())
        {
            return std::nullopt;
        }
        return m_columns.at(*column).values.at(index);
    }

    /// The value of @p type in the last row, or NaN when the branch does not have the type or has
    /// no rows (Java: getLast()).
    [[nodiscard]] double getLast(const T& type) const
    {
        const std::optional<std::size_t> index = indexOf(type);
        if (!index.has_value() || m_columns.at(*index).values.empty())
        {
            return std::numeric_limits<double>::quiet_NaN();
        }
        return m_columns.at(*index).values.back();
    }

    /// The smallest value set for @p type, or NaN when none was (Java: getMinimum()).
    [[nodiscard]] double getMinimum(const T& type) const
    {
        const std::optional<std::size_t> index = indexOf(type);
        return index.has_value() ? m_columns.at(*index).minimum
                                 : std::numeric_limits<double>::quiet_NaN();
    }

    /// The largest value set for @p type, or NaN when none was (Java: getMaximum()).
    [[nodiscard]] double getMaximum(const T& type) const
    {
        const std::optional<std::size_t> index = indexOf(type);
        return index.has_value() ? m_columns.at(*index).maximum
                                 : std::numeric_limits<double>::quiet_NaN();
    }

    /// The number of rows (Java: getLength()); 0 without types.
    [[nodiscard]] std::size_t getLength() const noexcept
    {
        return m_columns.empty() ? 0 : m_columns.front().values.size();
    }

    /// The branch's types sorted by T::compareTo(), types that compare equal in the order they
    /// were added (Java: getTypes(), with Arrays.sort, a stable sort).
    /// @throws BugError when the branch has no types
    [[nodiscard]] std::vector<const T*> getTypes() const
    {
        if (m_columns.empty())
        {
            bug("No data types have been added to branch " + m_name);
        }
        std::vector<const T*> types;
        types.reserve(m_columns.size());
        for (const Column& column : m_columns)
        {
            types.push_back(column.type);
        }
        std::ranges::stable_sort(types,
                                 [](const T* a, const T* b) { return a->compareTo(*b) < 0; });
        return types;
    }

    [[nodiscard]] const std::string& getName() const noexcept { return m_name; }

    /// Makes the branch immutable for good; repeated calls do nothing. @p where, the call site by
    /// default, is what a refused change reports.
    void immute(std::source_location where = std::source_location::current()) noexcept
    {
        m_mutable.immute(where);
    }
    [[nodiscard]] bool isMutable() const noexcept { return m_mutable.isMutable(); }

    /// The modification id (Java: getModID()).
    [[nodiscard]] ModId modId() const noexcept { return m_modId; }

    /// A copy of the branch that is mutable whatever this one is, with the same types, values,
    /// minima, maxima and ModId (what FlightDataBranch.clone() copies).
    [[nodiscard]] DataBranch clone() const
    {
        DataBranch copy(*this);
        copy.m_mutable = Mutable{};
        return copy;
    }

private:
    struct Column
    {
        const T*            type{nullptr};
        std::vector<double> values;
        double              minimum{std::numeric_limits<double>::quiet_NaN()};
        double              maximum{std::numeric_limits<double>::quiet_NaN()};
    };

    /// The column of @p type: the one keyed by its address, else the first whose type equals it
    /// (Java's map compares the hash codes, then equals()).
    [[nodiscard]] std::optional<std::size_t> indexOf(const T& type) const
    {
        if (const auto found = m_index.find(&type); found != m_index.end())
        {
            return found->second;
        }
        const int hash = type.hashCode();
        for (std::size_t i = 0; i < m_columns.size(); i++)
        {
            const T& key = *m_columns.at(i).type;
            if (key.hashCode() == hash && key.equals(type))
            {
                return i;
            }
        }
        return std::nullopt;
    }

    /// Adds a column for @p type with NaN in every existing row and NaN as minimum and maximum.
    Column& addColumn(const T& type)
    {
        if (indexOf(type).has_value())
        {
            bug(std::format("Value type {} already exists.", type.getName()));
        }
        m_index.emplace(&type, m_columns.size());
        return m_columns.emplace_back(Column{
            .type   = &type,
            .values = std::vector<double>(getLength(), std::numeric_limits<double>::quiet_NaN())});
    }

    std::string m_name;
    /// The columns in the order their types were added. A deque, so that a column (and the
    /// vector getView() hands out) keeps its address when columns are added.
    std::deque<Column> m_columns;
    /// Column index by type address.
    std::unordered_map<const T*, std::size_t> m_index;
    Mutable                                   m_mutable;
    ModId                                     m_modId{ModId::invalid()};
};

}  // namespace QtRocket
