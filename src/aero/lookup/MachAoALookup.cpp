#include "QtRocket/aero/lookup/MachAoALookup.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <expected>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/// The index of @p column in @p columns, or nullopt.
[[nodiscard]] std::optional<std::size_t> indexOf(std::span<const std::string> columns,
                                                 std::string_view             column)
{
    const auto found = std::ranges::find(columns, column);
    if (found == columns.end())
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(std::ranges::distance(columns.begin(), found));
}

/// Java's Set.equals for two lists without duplicates: the same names in any order.
[[nodiscard]] bool sameColumns(std::span<const std::string> a, std::span<const std::string> b)
{
    return a.size() == b.size() && std::ranges::all_of(a, [b](const std::string& name) {
               return indexOf(b, name).has_value();
           });
}

/// "[a, b, c]", as Java's Set.toString() prints the names.
[[nodiscard]] std::string columnList(std::span<const std::string> columns)
{
    return "[" + Strings::join(", ", columns) + "]";
}

/// Java's Builder.findValue(): the value under @p column exactly, else the first entry (in name
/// order) whose name normalises to @p column. Java walks a HashMap there, whose order differs.
[[nodiscard]] std::optional<double> findValue(const MachAoALookup::Values& values,
                                              const std::string&           column)
{
    const auto exact = values.find(column);
    if (exact != values.end())
    {
        return exact->second;
    }
    for (const auto& [name, value] : values)
    {
        if (MachAoALookup::normalize(name) == column)
        {
            return value;
        }
    }
    return std::nullopt;
}

}  // namespace

// ================================================================================== MachAoALookup

MachAoALookup::MachAoALookup(RowsByMach rowsByMach, std::vector<std::string> valueColumns,
                             bool hasAoA, double minMach, double maxMach, double minAoA,
                             double maxAoA)
  : m_rowsByMach(std::move(rowsByMach)),
    m_valueColumns(std::move(valueColumns)),
    m_hasAoA(hasAoA),
    m_minMach(minMach),
    m_maxMach(maxMach),
    m_minAoA(minAoA),
    m_maxAoA(maxAoA)
{
}

double MachAoALookup::interpolate(double mach, double aoaDegrees, std::string_view column) const
{
    const std::optional<std::size_t> index = indexOf(m_valueColumns, normalize(column));
    if (!index)
    {
        bug("Column '" + std::string{column} + "' is not present in the table");
    }

    const double clampedMach = clampMach(mach);

    // TreeMap.floorKey() (the greatest key <= clampedMach) and ceilingKey() (the least key >=
    // clampedMach), each falling back to the first or last key when there is none.
    const auto above = m_rowsByMach.upper_bound(clampedMach);
    auto       lower = (above == m_rowsByMach.begin()) ? m_rowsByMach.end() : std::prev(above);
    auto       upper = m_rowsByMach.lower_bound(clampedMach);
    if (lower == m_rowsByMach.end())
    {
        lower = m_rowsByMach.begin();
    }
    if (upper == m_rowsByMach.end())
    {
        upper = std::prev(m_rowsByMach.end());
    }

    const double lowerValue = interpolateAoA(lower->second, aoaDegrees, *index);
    const double upperValue = interpolateAoA(upper->second, aoaDegrees, *index);

    if (MathUtil::javaDoubleCompare(lower->first, upper->first) == 0)
    {
        return lowerValue;
    }

    const double fraction = (clampedMach - lower->first) / (upper->first - lower->first);
    return MathUtil::interpolate(lowerValue, upperValue, fraction);
}

std::optional<MachAoALookup::NonFiniteNumber> MachAoALookup::findNonFinite() const
{
    using Kind = NonFiniteNumber::Kind;
    for (const auto& [mach, rows] : m_rowsByMach)
    {
        for (const Row& row : rows)
        {
            const double aoa = m_hasAoA ? row.aoa : kNaN;
            if (!std::isfinite(mach))
            {
                return NonFiniteNumber{
                    .kind = Kind::MACH, .column = {}, .mach = mach, .aoa = aoa, .value = mach};
            }
            if (m_hasAoA && !std::isfinite(aoa))
            {
                return NonFiniteNumber{
                    .kind = Kind::AOA, .column = {}, .mach = mach, .aoa = aoa, .value = aoa};
            }
            for (std::size_t column = 0; column < row.values.size(); column++)
            {
                if (!std::isfinite(row.values[column]))
                {
                    return NonFiniteNumber{.kind   = Kind::VALUE,
                                           .column = m_valueColumns[column],
                                           .mach   = mach,
                                           .aoa    = aoa,
                                           .value  = row.values[column]};
                }
            }
        }
    }
    return std::nullopt;
}

double MachAoALookup::interpolateAoA(std::span<const Row> rows, double aoaDegrees,
                                     std::size_t column) const
{
    if (!m_hasAoA || rows.size() == 1)
    {
        return rows.front().values[column];
    }

    const double clampedAoA = clampAoA(aoaDegrees);

    const Row* lower = &rows.front();
    const Row* upper = &rows.back();
    for (const Row& row : rows)
    {
        if (row.aoa <= clampedAoA)
        {
            lower = &row;
        }
        if (row.aoa >= clampedAoA)
        {
            upper = &row;
            break;
        }
    }

    if (MathUtil::javaDoubleCompare(lower->aoa, upper->aoa) == 0)
    {
        return lower->values[column];
    }

    const double fraction = (clampedAoA - lower->aoa) / (upper->aoa - lower->aoa);
    return MathUtil::interpolate(lower->values[column], upper->values[column], fraction);
}

double MachAoALookup::clampMach(double mach) const noexcept
{
    // Java logs a warning the first time on each side.
    if (mach < m_minMach)
    {
        return m_minMach;
    }
    if (mach > m_maxMach)
    {
        return m_maxMach;
    }
    return mach;
}

double MachAoALookup::clampAoA(double aoa) const noexcept
{
    if (!m_hasAoA)
    {
        return aoa;
    }
    // Java logs a warning the first time on each side.
    if (aoa < m_minAoA)
    {
        return m_minAoA;
    }
    if (aoa > m_maxAoA)
    {
        return m_maxAoA;
    }
    return aoa;
}

MachAoALookup::Builder MachAoALookup::builder(std::span<const std::string> valueColumns)
{
    return Builder{valueColumns};
}

MachAoALookup::Builder MachAoALookup::dragBuilder()
{
    const std::vector<std::string> columns{"cd"};
    return Builder{columns};
}

MachAoALookup::Builder MachAoALookup::stabilityBuilder()
{
    const std::vector<std::string> columns{"cn", "cm", "cp"};
    return Builder{columns};
}

std::string MachAoALookup::normalize(std::string_view value)
{
    const std::string lower = Strings::toLower(Strings::trim(value));
    std::string       result;
    result.reserve(lower.size());
    for (const char c : lower)
    {
        // Java's replaceAll("[\\s_]", ""): \s is [ \t\n\x0B\f\r].
        const bool removed =
            c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r' || c == '_';
        if (!removed)
        {
            result += c;
        }
    }
    return result;
}

std::vector<std::string> MachAoALookup::normalizeColumns(std::span<const std::string> columns)
{
    std::vector<std::string> normalized;
    for (const std::string& column : columns)
    {
        std::string name = normalize(column);
        if (name == "angleofattack")
        {
            name = "aoa";
        }
        if (!indexOf(normalized, name).has_value())
        {
            normalized.push_back(std::move(name));
        }
    }
    return normalized;
}

MachAoALookup MachAoALookup::buildFromRows(std::vector<Row> rows, std::vector<std::string> columns,
                                           bool hasAoA)
{
    RowsByMach byMach;
    double     minMach = std::numeric_limits<double>::infinity();
    double     maxMach = -std::numeric_limits<double>::infinity();
    double     minAoA  = std::numeric_limits<double>::infinity();
    double     maxAoA  = -std::numeric_limits<double>::infinity();

    for (Row& row : rows)
    {
        minMach = MathUtil::javaMin(minMach, row.mach);
        maxMach = MathUtil::javaMax(maxMach, row.mach);
        if (hasAoA)
        {
            minAoA = MathUtil::javaMin(minAoA, row.aoa);
            maxAoA = MathUtil::javaMax(maxAoA, row.aoa);
        }
        byMach[row.mach].push_back(std::move(row));
    }

    // Java's List.sort is stable, by Double.compare of the angles.
    for (auto& entry : byMach)
    {
        std::ranges::stable_sort(entry.second, [](const Row& a, const Row& b) {
            return MathUtil::javaDoubleCompare(a.aoa, b.aoa) < 0;
        });
    }

    if (!hasAoA)
    {
        minAoA = kNaN;
        maxAoA = kNaN;
    }

    return MachAoALookup{
        std::move(byMach), std::move(columns), hasAoA, minMach, maxMach, minAoA, maxAoA};
}

// ======================================================================================== Builder

MachAoALookup::Builder::Builder(std::span<const std::string> valueColumns)
  : m_valueColumns(normalizeColumns(valueColumns))
{
    if (m_valueColumns.empty())
    {
        recordError("At least one value column is required");
    }
}

void MachAoALookup::Builder::recordError(std::string message)
{
    if (!m_error)
    {
        m_error = fail(ErrorCode::INVALID_ARGUMENT, std::move(message)).error();
    }
}

MachAoALookup::Builder& MachAoALookup::Builder::addData(double mach, const Values& values)
{
    return addData(mach, std::nullopt, values);
}

MachAoALookup::Builder& MachAoALookup::Builder::addData(double                mach,
                                                        std::optional<double> aoaDegrees,
                                                        const Values&         values)
{
    if (m_error)
    {
        return *this;
    }

    const bool rowHasAoA = aoaDegrees.has_value();
    if (!m_usesAoA)
    {
        m_usesAoA = rowHasAoA;
    }
    else if (*m_usesAoA != rowHasAoA)
    {
        recordError("Inconsistent AoA usage across data rows");
        return *this;
    }

    std::vector<double> rowValues;
    rowValues.reserve(m_valueColumns.size());
    for (const std::string& column : m_valueColumns)
    {
        const std::optional<double> value = findValue(values, column);
        if (!value)
        {
            recordError("Value for column '" + column + "' missing");
            return *this;
        }
        rowValues.push_back(*value);
    }

    m_rows.push_back(
        Row{.mach = mach, .aoa = aoaDegrees.value_or(0.0), .values = std::move(rowValues)});
    return *this;
}

bool MachAoALookup::Builder::ensureColumns(std::span<const std::string> expected)
{
    if (m_error)
    {
        return false;
    }
    if (!sameColumns(m_valueColumns, expected))
    {
        recordError("Builder configured for columns " + columnList(m_valueColumns) +
                    " cannot accept data for " + columnList(expected));
        return false;
    }
    return true;
}

MachAoALookup::Builder& MachAoALookup::Builder::addDragData(double mach, double cd)
{
    const std::vector<std::string> expected{"cd"};
    if (ensureColumns(expected))
    {
        addData(mach, std::nullopt, Values{{"cd", cd}});
    }
    return *this;
}

MachAoALookup::Builder& MachAoALookup::Builder::addDragData(double mach, double aoaDegrees,
                                                            double cd)
{
    const std::vector<std::string> expected{"cd"};
    if (ensureColumns(expected))
    {
        addData(mach, aoaDegrees, Values{{"cd", cd}});
    }
    return *this;
}

MachAoALookup::Builder& MachAoALookup::Builder::addStabilityData(double mach, double cn, double cm,
                                                                 double cp)
{
    const std::vector<std::string> expected{"cn", "cm", "cp"};
    if (ensureColumns(expected))
    {
        addData(mach, std::nullopt, Values{{"cn", cn}, {"cm", cm}, {"cp", cp}});
    }
    return *this;
}

MachAoALookup::Builder& MachAoALookup::Builder::addStabilityData(double mach, double aoaDegrees,
                                                                 double cn, double cm, double cp)
{
    const std::vector<std::string> expected{"cn", "cm", "cp"};
    if (ensureColumns(expected))
    {
        addData(mach, aoaDegrees, Values{{"cn", cn}, {"cm", cm}, {"cp", cp}});
    }
    return *this;
}

Result<MachAoALookup> MachAoALookup::Builder::build() const
{
    if (m_error)
    {
        return std::unexpected(*m_error);
    }
    if (m_rows.empty())
    {
        return fail(ErrorCode::INVALID_ARGUMENT, "No lookup data added");
    }
    const bool hasAoA = m_usesAoA.value_or(false);
    return buildFromRows(m_rows, m_valueColumns, hasAoA);
}

}  // namespace QtRocket
