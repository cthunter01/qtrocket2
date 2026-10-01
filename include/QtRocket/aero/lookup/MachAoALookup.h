#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/util/Error.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

/// A table of coefficients over the Mach number and, optionally, the angle of attack in degrees
/// (OpenRocket's aerodynamics/lookup/MachAoALookup), interpolated linearly in both. The lookup
/// calculators read their coefficients from one ("cd" for the drag; "cn", "cm" and "cp" for the
/// stability). Build one with a Builder (builder(), dragBuilder(), stabilityBuilder()) or read
/// one from CSV (CsvMachAoALookup).
///
/// Column names are normalised (normalize()): trimmed, lower-cased and stripped of whitespace and
/// underscores, and "angleofattack" counts as "aoa".
///
/// Immutable once built. Deviation: Java logs a warning (once per table and side) when a Mach
/// number or angle of attack outside the table is clamped; QtRocket has no logger, so the clamping
/// is silent and interpolate() is const.
class MachAoALookup
{
public:
    /// The values of one data row by column name (Java: Map<String, Double>); a name that is not
    /// exactly a column is matched after normalize().
    using Values = std::map<std::string, double, std::less<>>;

    class Builder;

    /// Whether the rows have angles of attack (otherwise each Mach number has one row).
    [[nodiscard]] bool hasAoA() const noexcept { return m_hasAoA; }

    /// The smallest and largest angle of attack in degrees (NaN without angles of attack).
    [[nodiscard]] double getMinAoA() const noexcept { return m_minAoA; }
    [[nodiscard]] double getMaxAoA() const noexcept { return m_maxAoA; }

    /// The smallest and largest Mach number.
    [[nodiscard]] double getMinMach() const noexcept { return m_minMach; }
    [[nodiscard]] double getMaxMach() const noexcept { return m_maxMach; }

    /// The normalised value columns, in the order they were first given.
    [[nodiscard]] const std::vector<std::string>& getValueColumns() const noexcept
    {
        return m_valueColumns;
    }

    /// The value of @p column at @p mach and @p aoaDegrees: the Mach number and the angle are
    /// clamped to the table's range (a NaN passes), the rows of the two Mach numbers around it
    /// are interpolated in the angle of attack (when the table has angles and the Mach number
    /// more than one row), and the two results linearly in the Mach number. A Mach number between
    /// two table values is looked up with Java's TreeMap floorKey()/ceilingKey() under
    /// Double.compare, so a NaN Mach number gives the largest Mach number's value.
    /// @throws BugError when the table has no such column (Java: IllegalArgumentException
    ///         "Column '<column>' is not present in the table"); the calculators ask only for
    ///         the columns they built the table with.
    [[nodiscard]] double interpolate(double mach, double aoaDegrees, std::string_view column) const;

    /// A builder for a table of @p valueColumns (normalised, duplicates dropped). An empty list
    /// makes build() fail ("At least one value column is required").
    [[nodiscard]] static Builder builder(std::span<const std::string> valueColumns);
    /// A builder for a drag table: the column "cd".
    [[nodiscard]] static Builder dragBuilder();
    /// A builder for a stability table: the columns "cn", "cm" and "cp".
    [[nodiscard]] static Builder stabilityBuilder();

    /// Java's normalize(): @p value trimmed (String.trim()), lower-cased and stripped of '_' and
    /// regex whitespace (space, \t, \n, \x0B, \f, \r). Deviation: only ASCII letters are
    /// lower-cased (Java's toLowerCase(Locale.ROOT) folds every letter).
    [[nodiscard]] static std::string normalize(std::string_view value);

    /// Java's normalizeColumns(): every name normalised, "angleofattack" made "aoa", duplicates
    /// dropped, in order.
    [[nodiscard]] static std::vector<std::string> normalizeColumns(
        std::span<const std::string> columns);

private:
    struct Row
    {
        double              mach;
        double              aoa;
        std::vector<double> values;  ///< by column, in the order of m_valueColumns
    };

    /// Java's Double.compareTo order for the TreeMap of rows: -0.0 before 0.0, NaN last.
    struct JavaDoubleLess
    {
        [[nodiscard]] bool operator()(double a, double b) const noexcept
        {
            return MathUtil::javaDoubleCompare(a, b) < 0;
        }
    };

    using RowsByMach = std::map<double, std::vector<Row>, JavaDoubleLess>;

    MachAoALookup(RowsByMach rowsByMach, std::vector<std::string> valueColumns, bool hasAoA,
                  double minMach, double maxMach, double minAoA, double maxAoA);

    /// Java's buildFromRows().
    [[nodiscard]] static MachAoALookup buildFromRows(std::vector<Row>         rows,
                                                     std::vector<std::string> columns, bool hasAoA);

    /// The angle-of-attack interpolation within the rows of one Mach number.
    [[nodiscard]] double interpolateAoA(std::span<const Row> rows, double aoaDegrees,
                                        std::size_t column) const;

    [[nodiscard]] double clampMach(double mach) const noexcept;
    [[nodiscard]] double clampAoA(double aoa) const noexcept;

    RowsByMach               m_rowsByMach;
    std::vector<std::string> m_valueColumns;
    bool                     m_hasAoA;
    double                   m_minMach;
    double                   m_maxMach;
    double                   m_minAoA;
    double                   m_maxAoA;
};

/// Builds a MachAoALookup row by row (Java: MachAoALookup.Builder). Java throws from the builder
/// methods; here the first error is kept, later calls do nothing, and build() returns it, so the
/// calls still chain: MachAoALookup::builder(columns).addData(...).addData(...).build().
class MachAoALookup::Builder
{
public:
    /// Adds a row without an angle of attack. See addData(double, std::optional<double>, ...).
    Builder& addData(double mach, const Values& values);

    /// Adds a row: @p aoaDegrees is nullopt for a table without angles of attack (Java: null),
    /// and every row must agree on that ("Inconsistent AoA usage across data rows"). @p values
    /// must hold every value column, by its exact name or one that normalises to it ("Value for
    /// column '<column>' missing"); other entries are ignored.
    Builder& addData(double mach, std::optional<double> aoaDegrees, const Values& values);

    /// Adds a drag row. The builder must be for exactly the column "cd" ("Builder configured for
    /// columns [...] cannot accept data for [cd]"; the columns are listed in the builder's order,
    /// where Java prints its sets in their unspecified Set.of() order).
    Builder& addDragData(double mach, double cd);
    Builder& addDragData(double mach, double aoaDegrees, double cd);

    /// Adds a stability row. The builder must be for exactly the columns "cn", "cm" and "cp".
    Builder& addStabilityData(double mach, double cn, double cm, double cp);
    Builder& addStabilityData(double mach, double aoaDegrees, double cn, double cm, double cp);

    /// Whether a call so far failed (where Java would have thrown); build() returns the error.
    [[nodiscard]] bool hasError() const noexcept { return m_error.has_value(); }

    /// The table, or the first error (ErrorCode::INVALID_ARGUMENT, Java's message); with no error
    /// so far, it fails when no row was added ("No lookup data added").
    [[nodiscard]] Result<MachAoALookup> build() const;

private:
    friend class MachAoALookup;

    explicit Builder(std::span<const std::string> valueColumns);

    /// Java's ensureColumns(): records an error unless the columns are exactly @p expected.
    [[nodiscard]] bool ensureColumns(std::span<const std::string> expected);

    /// Records @p message as the error unless there is one already.
    void recordError(std::string message);

    std::vector<std::string> m_valueColumns;
    std::vector<Row>         m_rows;
    std::optional<bool>      m_usesAoA;
    std::optional<Error>     m_error;
};

}  // namespace QtRocket
