#include "QtRocket/aero/lookup/MachAoALookup.h"

#include <array>
#include <cmath>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/lookup/CsvMachAoALookup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"
#include "TestTempDir.h"

namespace
{

using QtRocket::BugError;
using QtRocket::ErrorCode;
using QtRocket::MachAoALookup;
using QtRocket::Test::TempDir;
namespace CsvMachAoALookup = QtRocket::CsvMachAoALookup;

constexpr double kEpsilon = 1e-6;  // MachAoALookupTest.EPSILON
constexpr double kNaN     = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf     = std::numeric_limits<double>::infinity();

/// The drag columns (List.of("cd")).
[[nodiscard]] std::vector<std::string> cdColumns()
{
    return {"cd"};
}

// ---- Ported from MachAoALookupTest.java ----

// MachAoALookupTest.interpolatesSingleAxis
TEST(MachAoALookup, InterpolatesSingleAxis)
{
    const TempDir dir;
    const auto    table =
        CsvMachAoALookup::fromCsv(dir.write("drag.csv", "mach,cd\n0,0.30\n1,0.50\n"), cdColumns());
    ASSERT_TRUE(table.has_value()) << table.error().toString();

    const double cd = table->interpolate(0.5, 0, "cd");
    EXPECT_NEAR(0.40, cd, 1e-9);
}

// MachAoALookupTest.interpolatesMachAndAoA
TEST(MachAoALookup, InterpolatesMachAndAoA)
{
    const TempDir dir;
    const auto    table = CsvMachAoALookup::fromCsv(
        dir.write("drag_aoa.csv", "mach,aoa,cd\n0,0,0.20\n0,10,0.40\n1,0,0.30\n1,10,0.50\n"),
        cdColumns());
    ASSERT_TRUE(table.has_value()) << table.error().toString();

    const double cd = table->interpolate(0.5, 5, "cd");
    EXPECT_NEAR(0.35, cd, kEpsilon);
}

// MachAoALookupTest.clampsOutsideRange
TEST(MachAoALookup, ClampsOutsideRange)
{
    const TempDir dir;
    const auto    table = CsvMachAoALookup::fromCsv(
        dir.write("drag_bounds.csv", "mach,cd\n0.2,0.20\n0.4,0.40\n"), cdColumns());
    ASSERT_TRUE(table.has_value()) << table.error().toString();

    const double low  = table->interpolate(0.0, 0, "cd");
    const double high = table->interpolate(0.8, 0, "cd");

    EXPECT_NEAR(0.20, low, kEpsilon);
    EXPECT_NEAR(0.40, high, kEpsilon);
    EXPECT_FALSE(table->hasAoA());
}

// MachAoALookupTest.buildsFromBuilder
TEST(MachAoALookup, BuildsFromBuilder)
{
    const auto table = MachAoALookup::builder(cdColumns())
                           .addData(0.0, MachAoALookup::Values{{"cd", 0.2}})
                           .addData(1.0, MachAoALookup::Values{{"cd", 0.6}})
                           .build();
    ASSERT_TRUE(table.has_value()) << table.error().toString();

    EXPECT_NEAR(0.4, table->interpolate(0.5, 0, "cd"), kEpsilon);
}

// MachAoALookupTest.supportsCustomSeparator
TEST(MachAoALookup, SupportsCustomSeparator)
{
    const TempDir dir;
    const auto    table = CsvMachAoALookup::fromCsv(
        dir.write("drag_semicolon.csv", "mach;cd\n0;0.20\n1;0.60\n"), cdColumns(), ';');
    ASSERT_TRUE(table.has_value()) << table.error().toString();

    EXPECT_NEAR(0.40, table->interpolate(0.5, 0, "cd"), kEpsilon);
}

// ---- Beyond the JUnit tests (values pinned with OpenRocket's MachAoALookup on JDK 17) ----

/// The table of the pinned cases: Mach 0.3 with three angles, 0.9 with two and 1.5 with one.
[[nodiscard]] MachAoALookup pinnedTable()
{
    auto table = MachAoALookup::builder(cdColumns())
                     .addData(0.3, 0.0, MachAoALookup::Values{{"cd", 0.31}})
                     .addData(0.3, 4.0, MachAoALookup::Values{{"cd", 0.35}})
                     .addData(0.3, 8.0, MachAoALookup::Values{{"cd", 0.42}})
                     .addData(0.9, 0.0, MachAoALookup::Values{{"cd", 0.55}})
                     .addData(0.9, 8.0, MachAoALookup::Values{{"cd", 0.72}})
                     .addData(1.5, 4.0, MachAoALookup::Values{{"cd", 0.61}})
                     .build();
    EXPECT_TRUE(table.has_value());
    return *table;
}

TEST(MachAoALookup, InterpolationIsJavas)
{
    struct Case
    {
        double mach;
        double aoa;
        double cd;
    };
    constexpr std::array<Case, 12> kCases{{
        {.mach = 0.3, .aoa = 0.0, .cd = 0.31},
        {.mach = 0.3, .aoa = 2.0, .cd = 0.32999999999999996},
        {.mach = 0.6, .aoa = 2.0, .cd = 0.46124999999999994},
        {.mach = 0.6, .aoa = 6.0, .cd = 0.53125},
        {.mach = 1.2, .aoa = 1.0, .cd = 0.590625},
        {.mach = 1.2, .aoa = 7.0, .cd = 0.654375},
        {.mach = 2.0, .aoa = 3.0, .cd = 0.61},   // Mach clamped to 1.5
        {.mach = 0.1, .aoa = -5.0, .cd = 0.31},  // both clamped low
        {.mach = 0.95, .aoa = 8.0, .cd = 0.71083333333333330},
        {.mach = 0.45, .aoa = 10.0, .cd = 0.495},  // angle clamped to 8
        {.mach = kNaN, .aoa = 2.0, .cd = 0.61},    // floorKey(NaN) is the last key
        {.mach = 1.5, .aoa = 0.0, .cd = 0.61},     // a single row ignores the angle
    }};
    const MachAoALookup            table = pinnedTable();
    for (const Case& c : kCases)
    {
        // Linear interpolation only: the same doubles as Java.
        EXPECT_EQ(table.interpolate(c.mach, c.aoa, "cd"), c.cd)
            << "mach " << c.mach << " aoa " << c.aoa;
    }
}

TEST(MachAoALookup, RangeAndNaNAngleAreJavas)
{
    const MachAoALookup table = pinnedTable();
    EXPECT_TRUE(std::isnan(table.interpolate(0.6, kNaN, "cd")));
    EXPECT_EQ(table.getMinMach(), 0.3);
    EXPECT_EQ(table.getMaxMach(), 1.5);
    EXPECT_EQ(table.getMinAoA(), 0.0);
    EXPECT_EQ(table.getMaxAoA(), 8.0);
    EXPECT_TRUE(table.hasAoA());
}

TEST(MachAoALookup, RowsAreSortedByAngleWhateverTheOrderAdded)
{
    const auto table = MachAoALookup::dragBuilder()
                           .addDragData(1.0, 10.0, 0.9)
                           .addDragData(0.0, 5.0, 0.4)
                           .addDragData(1.0, 0.0, 0.5)
                           .addDragData(0.0, -5.0, 0.2)
                           .build();
    ASSERT_TRUE(table.has_value());
    EXPECT_EQ(table->interpolate(1.0, 5.0, "cd"), 0.7);
    EXPECT_EQ(table->interpolate(0.0, 0.0, "cd"), 0.30000000000000004);
    EXPECT_EQ(table->getMinAoA(), -5.0);
    EXPECT_EQ(table->getMaxAoA(), 10.0);
    EXPECT_EQ(table->getMinMach(), 0.0);
    EXPECT_EQ(table->getMaxMach(), 1.0);
}

TEST(MachAoALookup, WithoutAnglesTheAngleIsIgnoredAndTheRangeIsNaN)
{
    const auto table = MachAoALookup::dragBuilder().addDragData(0, 0.2).addDragData(2, 0.6).build();
    ASSERT_TRUE(table.has_value());
    EXPECT_FALSE(table->hasAoA());
    EXPECT_TRUE(std::isnan(table->getMinAoA()));
    EXPECT_TRUE(std::isnan(table->getMaxAoA()));
    EXPECT_EQ(table->interpolate(1, 1000, "cd"), 0.4);
    EXPECT_EQ(table->interpolate(1, kNaN, "cd"), 0.4);
}

TEST(MachAoALookup, ColumnNamesAreNormalized)
{
    EXPECT_EQ(MachAoALookup::normalize("  C_D\t"), "cd");
    EXPECT_EQ(MachAoALookup::normalize("Angle Of\nAttack"), "angleofattack");
    EXPECT_EQ(MachAoALookup::normalize("Mach"), "mach");
    EXPECT_EQ(MachAoALookup::normalize("_ _"), "");

    const std::vector<std::string> columns{"CN", "c_n", "Angle of Attack", "Cm"};
    const std::vector<std::string> expected{"cn", "aoa", "cm"};
    EXPECT_EQ(MachAoALookup::normalizeColumns(columns), expected);

    // The table answers for any spelling of a column; values are matched by normalized name.
    const auto table = MachAoALookup::builder(std::vector<std::string>{"C D"})
                           .addData(0, MachAoALookup::Values{{" c_d ", 0.25}})
                           .build();
    ASSERT_TRUE(table.has_value()) << table.error().toString();
    EXPECT_EQ(table->getValueColumns(), cdColumns());
    EXPECT_EQ(table->interpolate(0, 0, "CD"), 0.25);
    EXPECT_EQ(table->interpolate(0, 0, "c_d"), 0.25);
}

TEST(MachAoALookup, AnExactColumnNameWinsOverANormalizedOne)
{
    const auto table = MachAoALookup::builder(cdColumns())
                           .addData(0, MachAoALookup::Values{{"CD", 1.0}, {"cd", 2.0}})
                           .build();
    ASSERT_TRUE(table.has_value());
    EXPECT_EQ(table->interpolate(0, 0, "cd"), 2.0);
}

TEST(MachAoALookup, UnknownColumnIsABug)
{
    const MachAoALookup table = pinnedTable();
    EXPECT_THROW((void)table.interpolate(0.5, 0, "cn"), BugError);
}

TEST(MachAoALookup, MachKeysFollowDoubleCompare)
{
    // Java's TreeMap keeps -0.0 and 0.0 apart.
    const auto table = MachAoALookup::dragBuilder()
                           .addDragData(-0.0, 0.1)
                           .addDragData(0.0, 0.2)
                           .addDragData(1, 0.3)
                           .build();
    ASSERT_TRUE(table.has_value());
    EXPECT_EQ(table->interpolate(0.0, 0, "cd"), 0.2);
    EXPECT_EQ(table->interpolate(-0.0, 0, "cd"), 0.1);
    EXPECT_EQ(table->getMinMach(), -0.0);
    EXPECT_TRUE(std::signbit(table->getMinMach()));
    EXPECT_EQ(table->interpolate(kInf, 0, "cd"), 0.3);
    EXPECT_EQ(table->interpolate(-kInf, 0, "cd"), 0.1);
}

TEST(MachAoALookup, BuilderErrorsAreJavasMessages)
{
    const auto noColumns = MachAoALookup::builder(std::vector<std::string>{}).build();
    ASSERT_FALSE(noColumns.has_value());
    EXPECT_EQ(noColumns.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(noColumns.error().message, "At least one value column is required");

    const auto noData = MachAoALookup::dragBuilder().build();
    ASSERT_FALSE(noData.has_value());
    EXPECT_EQ(noData.error().message, "No lookup data added");

    auto inconsistent = MachAoALookup::dragBuilder();
    inconsistent.addDragData(0, 0.1).addDragData(1, 5, 0.2);
    EXPECT_TRUE(inconsistent.hasError());
    ASSERT_FALSE(inconsistent.build().has_value());
    EXPECT_EQ(inconsistent.build().error().message, "Inconsistent AoA usage across data rows");

    const auto missing = MachAoALookup::stabilityBuilder()
                             .addData(0, MachAoALookup::Values{{"cn", 1}, {"cp", 2}})
                             .build();
    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error().message, "Value for column 'cm' missing");

    const auto wrongColumns = MachAoALookup::dragBuilder().addStabilityData(0, 1, 2, 3).build();
    ASSERT_FALSE(wrongColumns.has_value());
    EXPECT_EQ(wrongColumns.error().message,
              "Builder configured for columns [cd] cannot accept data for [cn, cm, cp]");

    const auto wrongColumns2 = MachAoALookup::stabilityBuilder().addDragData(0, 1).build();
    ASSERT_FALSE(wrongColumns2.has_value());
    EXPECT_EQ(wrongColumns2.error().message,
              "Builder configured for columns [cn, cm, cp] cannot accept data for [cd]");

    // The first error is kept; later calls change nothing.
    auto first = MachAoALookup::dragBuilder();
    first.addStabilityData(0, 1, 2, 3).addDragData(0, 5, 0.1).addDragData(1, 0.2);
    EXPECT_EQ(first.build().error().message,
              "Builder configured for columns [cd] cannot accept data for [cn, cm, cp]");
}

TEST(MachAoALookup, StabilityBuilderTakesTheThreeColumns)
{
    const auto table = MachAoALookup::stabilityBuilder()
                           .addStabilityData(0, 0.1, 0.2, 0.3)
                           .addStabilityData(1, 0.3, 0.4, 0.5)
                           .build();
    ASSERT_TRUE(table.has_value());
    const std::vector<std::string> expected{"cn", "cm", "cp"};
    EXPECT_EQ(table->getValueColumns(), expected);
    EXPECT_EQ(table->interpolate(0.5, 0, "cn"), 0.2);
    EXPECT_EQ(table->interpolate(0.5, 0, "cm"), 0.30000000000000004);
    EXPECT_EQ(table->interpolate(0.5, 0, "cp"), 0.4);

    // Columns in another order are the same set.
    const auto reordered = MachAoALookup::builder(std::vector<std::string>{"CP", "CN", "CM"})
                               .addStabilityData(0, 5, 0.1, 0.2, 0.3)
                               .build();
    ASSERT_TRUE(reordered.has_value()) << reordered.error().toString();
    EXPECT_EQ(reordered->interpolate(0, 5, "cp"), 0.3);
    EXPECT_TRUE(reordered->hasAoA());
}

// ---- findNonFinite(): QtRocket's own (OpenRocket has no such question) ----

/// What the number @p number is: "mach", "aoa", or the value column of a value.
[[nodiscard]] std::string whatOf(const MachAoALookup::NonFiniteNumber& number)
{
    switch (number.kind)
    {
        case MachAoALookup::NonFiniteNumber::Kind::MACH:
            return "mach" + number.column;
        case MachAoALookup::NonFiniteNumber::Kind::AOA:
            return "aoa" + number.column;
        case MachAoALookup::NonFiniteNumber::Kind::VALUE:
            break;
    }
    return number.column;
}

/// findNonFinite() of @p table as "<what> at Mach <mach>, <angle> degrees: <number>", the
/// numbers as Java prints them and <what> being "mach", "aoa" or the column of a value (a Mach
/// number and an angle have no column: one would show after the word); "finite" when every
/// number of the table is finite, and "no table: <message>" when @p table is the error of a
/// builder or of the CSV reader.
[[nodiscard]] std::string nonFinite(const QtRocket::Result<MachAoALookup>& table)
{
    if (!table.has_value())
    {
        return "no table: " + table.error().message;
    }
    const std::optional<MachAoALookup::NonFiniteNumber> number = table->findNonFinite();
    if (!number.has_value())
    {
        return "finite";
    }
    return std::format("{} at Mach {}, {} degrees: {}", whatOf(*number),
                       QtRocket::Strings::javaDoubleToString(number->mach),
                       QtRocket::Strings::javaDoubleToString(number->aoa),
                       QtRocket::Strings::javaDoubleToString(number->value));
}

TEST(MachAoALookup, ATableOfFiniteNumbersHasNoNonFiniteOne)
{
    EXPECT_EQ(nonFinite(MachAoALookup::dragBuilder().addDragData(0, 0.3).build()), "finite");
    EXPECT_EQ(nonFinite(MachAoALookup::dragBuilder()
                            .addDragData(0, 0, 0.3)
                            .addDragData(0, 10, 0.4)
                            .addDragData(2, 0, -1e300)
                            .build()),
              "finite");
    EXPECT_EQ(nonFinite(MachAoALookup::stabilityBuilder()
                            .addStabilityData(0, 0.1, 0.2, 0.3)
                            .addStabilityData(1, 0.3, 0.4, 0.5)
                            .build()),
              "finite");
}

TEST(MachAoALookup, FindsANonFiniteValueOfAnyColumn)
{
    // Without angles of attack the angle of the row is no number.
    EXPECT_EQ(nonFinite(MachAoALookup::dragBuilder()
                            .addDragData(0, 0.3)
                            .addDragData(1, kNaN)
                            .addDragData(2, 0.5)
                            .build()),
              "cd at Mach 1.0, NaN degrees: NaN");
    EXPECT_EQ(nonFinite(MachAoALookup::dragBuilder().addDragData(0.5, kInf).build()),
              "cd at Mach 0.5, NaN degrees: Infinity");
    EXPECT_EQ(
        nonFinite(
            MachAoALookup::dragBuilder().addDragData(0, 0, 0.3).addDragData(0, 10, -kInf).build()),
        "cd at Mach 0.0, 10.0 degrees: -Infinity");
    // The columns of a row in the order of the table's columns.
    EXPECT_EQ(nonFinite(MachAoALookup::stabilityBuilder()
                            .addStabilityData(0, 0.1, 0.2, 0.3)
                            .addStabilityData(1, 0.3, 0.4, kNaN)
                            .build()),
              "cp at Mach 1.0, NaN degrees: NaN");
    EXPECT_EQ(
        nonFinite(
            MachAoALookup::stabilityBuilder().addStabilityData(1, 4, 0.3, kInf, kNaN).build()),
        "cm at Mach 1.0, 4.0 degrees: Infinity");
    EXPECT_EQ(nonFinite(MachAoALookup::builder(std::vector<std::string>{"CP", "CN", "CM"})
                            .addStabilityData(1, kNaN, kNaN, 0.5)
                            .build()),
              "cn at Mach 1.0, NaN degrees: NaN");
}

TEST(MachAoALookup, FindsANonFiniteMachNumberOrAngleOfAttack)
{
    EXPECT_EQ(
        nonFinite(MachAoALookup::dragBuilder().addDragData(kNaN, 0.3).addDragData(1, 0.4).build()),
        "mach at Mach NaN, NaN degrees: NaN");
    EXPECT_EQ(
        nonFinite(MachAoALookup::dragBuilder().addDragData(0, 0.3).addDragData(kInf, 0.4).build()),
        "mach at Mach Infinity, NaN degrees: Infinity");
    EXPECT_EQ(nonFinite(MachAoALookup::dragBuilder()
                            .addDragData(0, 0, 0.3)
                            .addDragData(0, kNaN, 0.4)
                            .addDragData(1, 0, 0.3)
                            .build()),
              "aoa at Mach 0.0, NaN degrees: NaN");
    EXPECT_EQ(
        nonFinite(
            MachAoALookup::dragBuilder().addDragData(1, -kInf, 0.3).addDragData(1, 2, 0.4).build()),
        "aoa at Mach 1.0, -Infinity degrees: -Infinity");
    // In a row the Mach number comes first, then the angle, then the values.
    EXPECT_EQ(nonFinite(MachAoALookup::dragBuilder().addDragData(-kInf, kInf, kNaN).build()),
              "mach at Mach -Infinity, Infinity degrees: -Infinity");
    EXPECT_EQ(nonFinite(MachAoALookup::dragBuilder().addDragData(1, kInf, kNaN).build()),
              "aoa at Mach 1.0, Infinity degrees: Infinity");
}

// The rows in the table's order, whatever order they were added in: by Mach number, with a NaN
// last, and for one Mach number by angle of attack.
TEST(MachAoALookup, FindsTheFirstNonFiniteNumberInTheOrderOfTheTable)
{
    EXPECT_EQ(nonFinite(MachAoALookup::dragBuilder()
                            .addDragData(2, kNaN)
                            .addDragData(1, kInf)
                            .addDragData(0, 0.3)
                            .build()),
              "cd at Mach 1.0, NaN degrees: Infinity");
    EXPECT_EQ(
        nonFinite(MachAoALookup::dragBuilder().addDragData(kNaN, 0.3).addDragData(3, kInf).build()),
        "cd at Mach 3.0, NaN degrees: Infinity");
    EXPECT_EQ(nonFinite(MachAoALookup::dragBuilder()
                            .addDragData(1, 10, kNaN)
                            .addDragData(1, 5, kInf)
                            .addDragData(1, 0, 0.3)
                            .build()),
              "cd at Mach 1.0, 5.0 degrees: Infinity");
}

// The CSV reader takes "NaN" and "Infinity" for numbers, as OpenRocket's does: a table from a
// file can hold them.
TEST(MachAoALookup, FindsANonFiniteNumberOfATableReadFromCsv)
{
    const TempDir dir;
    EXPECT_EQ(nonFinite(CsvMachAoALookup::fromCsv(
                  dir.write("finite.csv", "mach,cd\n0,0.30\n1,0.50\n"), cdColumns())),
              "finite");
    EXPECT_EQ(nonFinite(CsvMachAoALookup::fromCsv(dir.write("nan.csv", "mach,cd\n0,0.30\n1,NaN\n"),
                                                  cdColumns())),
              "cd at Mach 1.0, NaN degrees: NaN");
    EXPECT_EQ(
        nonFinite(CsvMachAoALookup::fromCsv(
            dir.write("infinity.csv", "mach,aoa,cd\n0,0,0.30\n0,Infinity,0.4\n"), cdColumns())),
        "aoa at Mach 0.0, Infinity degrees: Infinity");
}

}  // namespace
