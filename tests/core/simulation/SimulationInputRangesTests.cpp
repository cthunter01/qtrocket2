#include "QtRocket/simulation/SimulationInputRanges.h"

#include <cmath>
#include <format>
#include <iostream>
#include <limits>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/lookup/MachAoALookup.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

// The table of ranges that Simulation::validateInputs() and the .ork reader hold the numbers of
// a simulation to (decision U9 of tier 9). The table is QtRocket's own, with nothing of
// OpenRocket's to compare it with: it is pinned here row by row, because a change of a range is
// a change of what a design may hold and of what a user is told.

namespace
{

using QtRocket::MachAoALookup;
using QtRocket::Result;
using Ranges = QtRocket::SimulationInputRanges;
using Range  = QtRocket::SimulationInputRanges::Range;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

TEST(SimulationInputRanges, ARangeHoldsItsBoundsAndWhatLiesBetweenThem)
{
    constexpr Range kRange{.min = -1.5, .max = 4.0, .minExcluded = false, .text = "any"};
    EXPECT_TRUE(kRange.contains(-1.5));
    EXPECT_TRUE(kRange.contains(0.0));
    EXPECT_TRUE(kRange.contains(4.0));
    EXPECT_FALSE(kRange.contains(std::nextafter(-1.5, -2.0)));
    EXPECT_FALSE(kRange.contains(std::nextafter(4.0, 5.0)));
    EXPECT_FALSE(kRange.contains(1e300));
    EXPECT_FALSE(kRange.contains(-1e300));
    // Nothing that is not finite is in a range.
    EXPECT_FALSE(kRange.contains(kNaN));
    EXPECT_FALSE(kRange.contains(kInf));
    EXPECT_FALSE(kRange.contains(-kInf));
    // A range can be asked at compile time.
    static_assert(kRange.contains(1.0));
    static_assert(!kRange.contains(5.0));
}

TEST(SimulationInputRanges, ARangeWhoseLowerBoundIsExcludedStartsAboveIt)
{
    constexpr Range kRange{.min = 0.0, .max = 10.0, .minExcluded = true, .text = "any"};
    EXPECT_FALSE(kRange.contains(0.0));
    EXPECT_FALSE(kRange.contains(-0.0));
    EXPECT_TRUE(kRange.contains(std::numeric_limits<double>::denorm_min()));
    EXPECT_TRUE(kRange.contains(10.0));
    EXPECT_FALSE(kRange.contains(std::nextafter(10.0, 11.0)));
    EXPECT_FALSE(kRange.contains(kNaN));
    // Zero has no sign for a range that includes it.
    EXPECT_TRUE(Ranges::kLaunchRodLength.contains(-0.0));
}

/// A row of the table as a test pins it: the input, the bounds as Java prints doubles, "(" in
/// front of a lower bound that is outside the range and "[" in front of one inside it, and the
/// range in words.
[[nodiscard]] std::string told(const Ranges::Row& row)
{
    return std::format("{}: {}{}, {}] {}", row.input, row.range.minExcluded ? "(" : "[",
                       QtRocket::Strings::javaDoubleToString(row.range.min),
                       QtRocket::Strings::javaDoubleToString(row.range.max), row.range.text);
}

/// Every row of the table, told, one to a line.
[[nodiscard]] std::string theTable()
{
    std::string rows;
    for (const Ranges::Row& row : Ranges::kTable)
    {
        rows += told(row) + "\n";
    }
    return rows;
}

/// The table as the user reviewed it. (pi/3 is 1.0471975511965976, 2 pi 6.283185307179586,
/// pi/180 0.017453292519943295 and pi/9 0.3490658503988659.)
constexpr std::string_view kTheTable =
    R"(the launch rod length: [0.0, 1000.0] from 0 to 1000 m
the launch rod angle: [-1.0471975511965976, 1.0471975511965976] from -pi/3 to pi/3 rad
the launch latitude: [-90.0, 90.0] from -90 to 90 degrees
the launch longitude: [-180.0, 180.0] from -180 to 180 degrees
the launch altitude: [-1000.0, 100000.0] from -1000 to 100000 m
the average wind speed: [0.0, 1000.0] from 0 to 1000 m/s
the standard deviation of the wind speed: [0.0, 1000.0] from 0 to 1000 m/s
the wind direction: [-6.283185307179586, 6.283185307179586] from -2 pi to 2 pi rad
the altitude of a wind level: [-1000.0, 1000000.0] from -1000 to 1000000 m
the wind speed of a wind level: [0.0, 1000.0] from 0 to 1000 m/s
the wind direction of a wind level: [-6.283185307179586, 6.283185307179586] from -2 pi to 2 pi rad
the standard deviation of the wind speed of a wind level: [0.0, 1000.0] from 0 to 1000 m/s
the launch rod direction: [-6.283185307179586, 6.283185307179586] from -2 pi to 2 pi rad
the launch temperature: [1.0, 1000.0] from 1 to 1000 K
the launch pressure: [0.001, 1000000.0] from 0.001 to 1000000 Pa
the launch relative humidity: [0.0, 1.0] from 0 to 1
the constant gravity: [0.0, 100.0] from 0 to 100 m/s^2
a Mach number of a lookup table: [0.0, 100.0] from 0 to 100
an angle of attack of a lookup table: [-180.0, 180.0] from -180 to 180 degrees
a coefficient of a lookup table ('cd', 'cn', 'cm'): [-1000000.0, 1000000.0] from -1000000 to 1000000
the centre of pressure of a lookup table ('cp'): [-1000.0, 1000.0] from -1000 to 1000 m
the time step: (0.0, 10.0] above 0 and at most 10 s
the maximum simulation time: [0.0, 1000000.0] from 0 to 1000000 s
the maximum step angle: [0.017453292519943295, 0.3490658503988659] from pi/180 to pi/9 rad
a recovery speed warning threshold (each of the four): [0.0, 10000.0] from 0 to 10000 m/s
the ignition delay and the ejection delay of a motor: [0.0, 1000000.0] from 0 to 1000000 s
the deployment delay of a recovery device and the separation delay of a stage: [0.0, 1000000.0] from 0 to 1000000 s
the deployment altitude of a recovery device: [0.0, 1000000.0] from 0 to 1000000 m
the separation altitude of a stage: [0.0, 1000000.0] from 0 to 1000000 m
)";

TEST(SimulationInputRanges, TheTableIsWhatWasDecided)
{
    EXPECT_EQ(theTable(), kTheTable);
    EXPECT_EQ(Ranges::kTable.size(), 29U);
}

/// The rows of the table that are not as a row has to be: a range that is no range, a text
/// that does not start as the bounds are, or a reason that is no sentence.
[[nodiscard]] std::vector<std::string> badRows()
{
    std::vector<std::string> bad;
    for (const Ranges::Row& row : Ranges::kTable)
    {
        const Range&           range   = row.range;
        const std::string_view opening = range.minExcluded ? "above " : "from ";
        const bool             ordered =
            std::isfinite(range.min) && std::isfinite(range.max) && range.min < range.max;
        if (!ordered || !range.text.starts_with(opening) || row.input.empty() ||
            row.reason.size() < 20 || !row.reason.ends_with('.'))
        {
            bad.push_back(told(row));
        }
    }
    return bad;
}

TEST(SimulationInputRanges, EveryRowHasARangeItsWordsAndAReason)
{
    EXPECT_EQ(badRows(), std::vector<std::string>{});
    // The ranges that are OpenRocket's own limits are those limits exactly.
    EXPECT_EQ(Ranges::kLaunchRodAngle.max, std::numbers::pi / 3);
    EXPECT_EQ(Ranges::kLaunchRodAngle.min, -std::numbers::pi / 3);
    EXPECT_EQ(Ranges::kMaximumStepAngle.min, 1 * std::numbers::pi / 180);
    EXPECT_EQ(Ranges::kMaximumStepAngle.max, 20 * std::numbers::pi / 180);
    EXPECT_EQ(Ranges::kLaunchPressure.min, 0.001);
    // The one range whose lower bound is outside it.
    EXPECT_TRUE(Ranges::kTimeStep.minExcluded);
    EXPECT_FALSE(Ranges::kMaxSimulationTime.minExcluded);
}

/// A number of a lookup table of kind @p kind in the column @p column.
[[nodiscard]] MachAoALookup::Number numberOf(MachAoALookup::Number::Kind kind,
                                             std::string_view            column)
{
    return {.kind = kind, .column = std::string(column), .mach = 1.0, .aoa = 2.0, .value = 3.0};
}

TEST(SimulationInputRanges, ANumberOfALookupTableHasTheRangeOfItsKindAndItsColumn)
{
    using Kind = MachAoALookup::Number::Kind;
    EXPECT_EQ(&Ranges::ofLookupNumber(numberOf(Kind::MACH, "")), &Ranges::kLookupMach);
    EXPECT_EQ(&Ranges::ofLookupNumber(numberOf(Kind::AOA, "")), &Ranges::kLookupAngleOfAttack);
    EXPECT_EQ(&Ranges::ofLookupNumber(numberOf(Kind::VALUE, "cd")), &Ranges::kLookupCoefficient);
    EXPECT_EQ(&Ranges::ofLookupNumber(numberOf(Kind::VALUE, "cn")), &Ranges::kLookupCoefficient);
    EXPECT_EQ(&Ranges::ofLookupNumber(numberOf(Kind::VALUE, "cm")), &Ranges::kLookupCoefficient);
    EXPECT_EQ(&Ranges::ofLookupNumber(numberOf(Kind::VALUE, "cp")),
              &Ranges::kLookupCentreOfPressure);
    // Any other column is a coefficient, and the column of a Mach number or an angle is of no
    // account.
    EXPECT_EQ(&Ranges::ofLookupNumber(numberOf(Kind::VALUE, "other")), &Ranges::kLookupCoefficient);
    EXPECT_EQ(&Ranges::ofLookupNumber(numberOf(Kind::MACH, "cp")), &Ranges::kLookupMach);
}

/// findRefusedLookupNumber() of @p table as "<column or kind> <number>"; "none" for a table a
/// run may read.
[[nodiscard]] std::string refusedOf(const Result<MachAoALookup>& table)
{
    if (!table.has_value())
    {
        return "no table: " + table.error().message;
    }
    const std::optional<MachAoALookup::Number> number = Ranges::findRefusedLookupNumber(*table);
    if (!number.has_value())
    {
        return "none";
    }
    std::string_view what = number->column;
    if (number->kind == MachAoALookup::Number::Kind::MACH)
    {
        what = "mach";
    }
    else if (number->kind == MachAoALookup::Number::Kind::AOA)
    {
        what = "aoa";
    }
    return std::format("{} {}", what, QtRocket::Strings::javaDoubleToString(number->value));
}

TEST(SimulationInputRanges, FindsTheFirstNumberOfALookupTableThatARunMayNotRead)
{
    EXPECT_EQ(refusedOf(MachAoALookup::dragBuilder()
                            .addDragData(0, -180, -1000000)
                            .addDragData(100, 180, 1000000)
                            .build()),
              "none");
    EXPECT_EQ(refusedOf(MachAoALookup::stabilityBuilder()
                            .addStabilityData(0, -1000000, 1000000, -1000)
                            .addStabilityData(100, 1000000, -1000000, 1000)
                            .build()),
              "none");
    // Outside a range, or not finite; the first in the order of the table.
    EXPECT_EQ(refusedOf(MachAoALookup::dragBuilder()
                            .addDragData(2, kNaN)
                            .addDragData(1, 1e300)
                            .addDragData(0, 0.3)
                            .build()),
              "cd 1.0E300");
    EXPECT_EQ(refusedOf(MachAoALookup::dragBuilder().addDragData(-0.5, 0.3).build()), "mach -0.5");
    EXPECT_EQ(refusedOf(MachAoALookup::dragBuilder().addDragData(100.5, kInf).build()),
              "mach 100.5");
    EXPECT_EQ(refusedOf(MachAoALookup::dragBuilder().addDragData(1, 181, kNaN).build()),
              "aoa 181.0");
    EXPECT_EQ(refusedOf(MachAoALookup::dragBuilder().addDragData(1, 90, kNaN).build()), "cd NaN");
    // A centre of pressure has the narrower range: what is a coefficient is no place.
    EXPECT_EQ(
        refusedOf(MachAoALookup::stabilityBuilder().addStabilityData(1, 5000, -5000, 5000).build()),
        "cp 5000.0");
    EXPECT_EQ(
        refusedOf(
            MachAoALookup::stabilityBuilder().addStabilityData(5, 1e300, -1e300, 1e300).build()),
        "cn 1.0E300");
}

// The table for whoever reviews it: the rows with their reasons, one line each.
//   build/clang-debug/bin/QtRocket_core_tests --gtest_also_run_disabled_tests
//       --gtest_filter=SimulationInputRanges.DISABLED_PrintsTheTable
TEST(SimulationInputRanges, DISABLED_PrintsTheTable)
{
    for (const Ranges::Row& row : Ranges::kTable)
    {
        std::cout << std::format("{} | {} | {}\n", row.input, row.range.text, row.reason);
    }
    std::cout << std::flush;
}

}  // namespace
