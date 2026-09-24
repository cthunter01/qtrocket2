#include "QtRocket/unit/FixedPrecisionUnit.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/unit/Tick.h"
#include "QtRocket/unit/Unit.h"

namespace
{

using QtRocket::FixedPrecisionUnit;
using QtRocket::Tick;
using QtRocket::Unit;

constexpr double kEpsilon = 1e-8;
constexpr double kNaN     = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf     = std::numeric_limits<double>::infinity();

// The expected strings and values were produced by OpenRocket's FixedPrecisionUnit on JDK 17.

class FixedPrecisionUnitTest : public ::testing::Test
{
protected:
    FixedPrecisionUnit unitPoint1{"unit", 0.1};
    FixedPrecisionUnit unitPoint25{"unit", 0.25};
    FixedPrecisionUnit unitPoint5{"unit", 0.5};
    FixedPrecisionUnit unitNoTrailing{"unit", 0.1, 1.0, false};
    FixedPrecisionUnit unitWithMultiplier{"unit", 0.1, 2.0};
};

// ---- Ported from FixedPrecisionUnitTest.java ----

TEST_F(FixedPrecisionUnitTest, Rounding)
{
    // Test rounding with 0.1 precision
    EXPECT_NEAR(1.0, unitPoint1.round(1.04), kEpsilon);
    EXPECT_NEAR(1.1, unitPoint1.round(1.05), kEpsilon);
    EXPECT_NEAR(1.1, unitPoint1.round(1.14), kEpsilon);
    EXPECT_NEAR(1.2, unitPoint1.round(1.15), kEpsilon);

    // Test rounding with 0.25 precision
    EXPECT_NEAR(1.0, unitPoint25.round(1.12), kEpsilon);
    EXPECT_NEAR(1.25, unitPoint25.round(1.13), kEpsilon);
    EXPECT_NEAR(1.25, unitPoint25.round(1.37), kEpsilon);
    EXPECT_NEAR(1.5, unitPoint25.round(1.38), kEpsilon);

    // Test rounding with 0.5 precision
    EXPECT_NEAR(1.0, unitPoint5.round(1.24), kEpsilon);
    EXPECT_NEAR(1.5, unitPoint5.round(1.25), kEpsilon);
    EXPECT_NEAR(1.5, unitPoint5.round(1.74), kEpsilon);
    EXPECT_NEAR(2.0, unitPoint5.round(1.75), kEpsilon);
}

TEST_F(FixedPrecisionUnitTest, NextValue)
{
    // Test with 0.1 precision
    EXPECT_NEAR(1.1, unitPoint1.getNextValue(1.0), kEpsilon);
    EXPECT_NEAR(1.2, unitPoint1.getNextValue(1.1), kEpsilon);
    EXPECT_NEAR(0.1, unitPoint1.getNextValue(0.0), kEpsilon);
    EXPECT_NEAR(-0.9, unitPoint1.getNextValue(-1.0), kEpsilon);

    // Test with 0.25 precision
    EXPECT_NEAR(1.25, unitPoint25.getNextValue(1.0), kEpsilon);
    EXPECT_NEAR(1.5, unitPoint25.getNextValue(1.25), kEpsilon);
    EXPECT_NEAR(0.25, unitPoint25.getNextValue(0.0), kEpsilon);

    // Test with 0.5 precision
    EXPECT_NEAR(1.5, unitPoint5.getNextValue(1.0), kEpsilon);
    EXPECT_NEAR(2.0, unitPoint5.getNextValue(1.5), kEpsilon);
    EXPECT_NEAR(0.5, unitPoint5.getNextValue(0.0), kEpsilon);
}

TEST_F(FixedPrecisionUnitTest, PreviousValue)
{
    // Test with 0.1 precision
    EXPECT_NEAR(0.9, unitPoint1.getPreviousValue(1.0), kEpsilon);
    EXPECT_NEAR(0.8, unitPoint1.getPreviousValue(0.9), kEpsilon);
    EXPECT_NEAR(-0.1, unitPoint1.getPreviousValue(0.0), kEpsilon);
    EXPECT_NEAR(-1.1, unitPoint1.getPreviousValue(-1.0), kEpsilon);

    // Test with 0.25 precision
    EXPECT_NEAR(0.75, unitPoint25.getPreviousValue(1.0), kEpsilon);
    EXPECT_NEAR(0.5, unitPoint25.getPreviousValue(0.75), kEpsilon);
    EXPECT_NEAR(-0.25, unitPoint25.getPreviousValue(0.0), kEpsilon);

    // Test with 0.5 precision
    EXPECT_NEAR(0.5, unitPoint5.getPreviousValue(1.0), kEpsilon);
    EXPECT_NEAR(0.0, unitPoint5.getPreviousValue(0.5), kEpsilon);
    EXPECT_NEAR(-0.5, unitPoint5.getPreviousValue(0.0), kEpsilon);
}

TEST_F(FixedPrecisionUnitTest, ToString)
{
    // Test with trailing zeros (default)
    EXPECT_EQ("1.0", unitPoint1.toString(1.0));
    EXPECT_EQ("1.1", unitPoint1.toString(1.1));
    EXPECT_EQ("-1.0", unitPoint1.toString(-1.0));
    EXPECT_EQ("0.0", unitPoint1.toString(0.0));

    // Test without trailing zeros
    EXPECT_EQ("1", unitNoTrailing.toString(1.0));
    EXPECT_EQ("1.1", unitNoTrailing.toString(1.1));
    EXPECT_EQ("-1", unitNoTrailing.toString(-1.0));
    EXPECT_EQ("0", unitNoTrailing.toString(0.0));

    // Test with multiplier
    EXPECT_EQ("0.5", unitWithMultiplier.toString(1.0));
    EXPECT_EQ("0.6", unitWithMultiplier.toString(1.1));
    EXPECT_EQ("-0.5", unitWithMultiplier.toString(-1.0));
    EXPECT_EQ("0.0", unitWithMultiplier.toString(0.0));
}

TEST_F(FixedPrecisionUnitTest, GetTicks)
{
    const FixedPrecisionUnit unit("unit", 0.5);
    const std::vector<Tick>  ticks = unit.getTicks(0.0, 5.0, 0.5, 1.0);

    // Verify we have the expected number of ticks
    ASSERT_FALSE(ticks.empty());

    // Test specific tick values
    for (std::size_t i = 0; i + 1 < ticks.size(); i++)
    {
        // Verify ticks are in ascending order
        EXPECT_LT(ticks[i].value, ticks[i + 1].value);

        // Verify tick spacing matches precision
        EXPECT_NEAR(0.5, ticks[i + 1].value - ticks[i].value, 0.001);
    }

    // Test major/minor tick marking
    for (const Tick& tick : ticks)
    {
        if (std::abs(std::fmod(tick.value, 1.0)) < 0.001)
        {
            // Whole numbers should be major ticks
            EXPECT_TRUE(tick.major);
        }
    }
}

// ---- QtRocket additions ----

TEST_F(FixedPrecisionUnitTest, DecimalsFollowThePrecision)
{
    EXPECT_EQ(unitPoint1.getDecimals(), 1);
    EXPECT_EQ(unitPoint25.getDecimals(), 2);
    EXPECT_EQ(unitPoint5.getDecimals(), 1);
    EXPECT_EQ(FixedPrecisionUnit("ms", 1, 0.001).getDecimals(), 0);
    EXPECT_EQ(FixedPrecisionUnit("K", 0.01).getDecimals(), 2);
    EXPECT_EQ(FixedPrecisionUnit("x", 0.001).getDecimals(), 3);
    EXPECT_EQ(FixedPrecisionUnit("° N", 10E-6, 1, false).getDecimals(), 5);
    EXPECT_EQ(unitPoint1.getPrecision(), 0.1);
    EXPECT_TRUE(unitPoint1.displaysTrailingZeros());
    EXPECT_FALSE(unitNoTrailing.displaysTrailingZeros());
    EXPECT_EQ(unitWithMultiplier.getMultiplier(), 2.0);
    EXPECT_TRUE(unitPoint1.hasSpace());
}

TEST_F(FixedPrecisionUnitTest, ToStringWithTrailingZerosIsJavasFormatter)
{
    // Half-up on the decimal digits, as String.format("%.1f") does.
    EXPECT_EQ(unitPoint1.toString(0.05), "0.1");
    EXPECT_EQ(unitPoint1.toString(0.15), "0.2");
    EXPECT_EQ(unitPoint1.toString(0.25), "0.3");
    EXPECT_EQ(unitPoint1.toString(0.35), "0.4");
    EXPECT_EQ(unitPoint1.toString(1.005), "1.0");
    EXPECT_EQ(unitPoint1.toString(1.015), "1.0");
    EXPECT_EQ(unitPoint1.toString(1.05), "1.1");
    EXPECT_EQ(unitPoint1.toString(1.15), "1.2");
    EXPECT_EQ(unitPoint1.toString(1.25), "1.3");
    EXPECT_EQ(unitPoint1.toString(1.75), "1.8");
    EXPECT_EQ(unitPoint1.toString(2.675), "2.7");
    EXPECT_EQ(unitPoint1.toString(12.345678), "12.3");
    EXPECT_EQ(unitPoint1.toString(1234.5678), "1234.6");
    EXPECT_EQ(unitPoint1.toString(-12.3456789), "-12.3");
    EXPECT_EQ(unitPoint1.toString(101325.0), "101325.0");
    EXPECT_EQ(unitPoint1.toString(1.0E7), "10000000.0");
    EXPECT_EQ(unitPoint1.toString(1.0E20), "100000000000000000000.0");
    EXPECT_EQ(unitPoint1.toString(1.0E-7), "0.0");
    EXPECT_EQ(unitPoint1.toString(-1.0E-7), "-0.0");
    EXPECT_EQ(unitPoint1.toString(-0.04), "-0.0");
    EXPECT_EQ(unitPoint1.toString(-0.0), "-0.0");
    // NaN and the infinities are formatted, not "N/A"; toStringUnit still says "N/A" for NaN.
    EXPECT_EQ(unitPoint1.toString(kNaN), "NaN");
    EXPECT_EQ(unitPoint1.toString(kInf), "Infinity");
    EXPECT_EQ(unitPoint1.toString(-kInf), "-Infinity");
    EXPECT_EQ(unitPoint1.toStringUnit(1.5), "1.5 unit");
    EXPECT_EQ(unitPoint1.toStringUnit(kNaN), "N/A");
    EXPECT_EQ(unitPoint1.toStringUnit(kInf), "Infinity unit");

    EXPECT_EQ(unitPoint25.toString(0.0), "0.00");
    EXPECT_EQ(unitPoint25.toString(1.005), "1.01");
    EXPECT_EQ(unitPoint25.toString(1.015), "1.02");
    EXPECT_EQ(unitPoint25.toString(12.345678), "12.35");
    EXPECT_EQ(unitPoint25.toString(0.123456789), "0.12");
    EXPECT_EQ(unitPoint25.toString(2.675), "2.68");
    EXPECT_EQ(unitPoint25.toString(-0.04), "-0.04");
    EXPECT_EQ(unitPoint25.toString(1234.5678), "1234.57");
    EXPECT_EQ(unitPoint25.toStringUnit(1.5), "1.50 unit");

    EXPECT_EQ(unitWithMultiplier.toString(0.05), "0.0");
    EXPECT_EQ(unitWithMultiplier.toString(0.15), "0.1");
    EXPECT_EQ(unitWithMultiplier.toString(0.25), "0.1");
    EXPECT_EQ(unitWithMultiplier.toString(0.35), "0.2");
    EXPECT_EQ(unitWithMultiplier.toString(0.5), "0.3");
    EXPECT_EQ(unitWithMultiplier.toString(1.5), "0.8");
    EXPECT_EQ(unitWithMultiplier.toString(2.5), "1.3");
    EXPECT_EQ(unitWithMultiplier.toString(12.345678), "6.2");
    EXPECT_EQ(unitWithMultiplier.toString(101325.0), "50662.5");
    EXPECT_EQ(unitWithMultiplier.toString(1.0E20), "50000000000000000000.0");
    EXPECT_EQ(unitWithMultiplier.toStringUnit(1.5), "0.8 unit");
}

TEST_F(FixedPrecisionUnitTest, ToStringWithoutTrailingZerosIsDecimalFormat)
{
    // Half to even on the exact binary value, as DecimalFormat("0.#") does.
    EXPECT_EQ(unitNoTrailing.toString(0.05), "0.1");
    EXPECT_EQ(unitNoTrailing.toString(0.15), "0.1");
    EXPECT_EQ(unitNoTrailing.toString(0.25), "0.2");
    EXPECT_EQ(unitNoTrailing.toString(0.35), "0.3");
    EXPECT_EQ(unitNoTrailing.toString(1.005), "1");
    EXPECT_EQ(unitNoTrailing.toString(1.05), "1.1");
    EXPECT_EQ(unitNoTrailing.toString(1.15), "1.1");
    EXPECT_EQ(unitNoTrailing.toString(1.25), "1.2");
    EXPECT_EQ(unitNoTrailing.toString(1.75), "1.8");
    EXPECT_EQ(unitNoTrailing.toString(2.675), "2.7");
    EXPECT_EQ(unitNoTrailing.toString(12.345678), "12.3");
    EXPECT_EQ(unitNoTrailing.toString(1234.5678), "1234.6");
    EXPECT_EQ(unitNoTrailing.toString(101325.0), "101325");
    EXPECT_EQ(unitNoTrailing.toString(1.0E7), "10000000");
    EXPECT_EQ(unitNoTrailing.toString(1.0E20), "100000000000000000000");
    EXPECT_EQ(unitNoTrailing.toString(1.0E-7), "0");
    EXPECT_EQ(unitNoTrailing.toString(-1.0E-7), "-0");
    EXPECT_EQ(unitNoTrailing.toString(-0.04), "-0");
    EXPECT_EQ(unitNoTrailing.toString(-0.0), "-0");
    EXPECT_EQ(unitNoTrailing.toString(kNaN), "NaN");
    EXPECT_EQ(unitNoTrailing.toString(kInf), "∞");
    EXPECT_EQ(unitNoTrailing.toString(-kInf), "-∞");
    EXPECT_EQ(unitNoTrailing.toStringUnit(1.5), "1.5 unit");
    EXPECT_EQ(unitNoTrailing.toStringUnit(kNaN), "N/A");

    // The latitude units of UNITS_LATITUDE: five decimals, no trailing zeros, one negated.
    const FixedPrecisionUnit north("° N", 10E-6, 1, false);
    const FixedPrecisionUnit south("° S", 10E-6, -1, false);
    EXPECT_EQ(north.toString(1.1), "1.1");
    EXPECT_EQ(north.toString(1.005), "1.005");
    EXPECT_EQ(north.toString(12.345678), "12.34568");
    EXPECT_EQ(north.toString(0.123456789), "0.12346");
    EXPECT_EQ(north.toString(1234.5678), "1234.5678");
    EXPECT_EQ(north.toString(60.123456), "60.12346");
    EXPECT_EQ(north.toString(-12.3456789), "-12.34568");
    EXPECT_EQ(north.toString(2.675), "2.675");
    EXPECT_EQ(north.toString(1.0E-7), "0");
    EXPECT_EQ(north.toString(-1.0E-7), "-0");
    EXPECT_EQ(north.toStringUnit(1.5), "1.5 ° N");
    EXPECT_EQ(south.toString(0.0), "-0");
    EXPECT_EQ(south.toString(1.0), "-1");
    EXPECT_EQ(south.toString(-1.0), "1");
    EXPECT_EQ(south.toString(12.345678), "-12.34568");
    EXPECT_EQ(south.toString(-0.04), "0.04");
    EXPECT_EQ(south.toString(-0.0), "0");
    EXPECT_EQ(south.toString(kInf), "-∞");
    EXPECT_EQ(south.toString(-kInf), "∞");
    EXPECT_EQ(south.toStringUnit(1.5), "-1.5 ° S");
}

TEST_F(FixedPrecisionUnitTest, OtherOpenRocketUnits)
{
    const FixedPrecisionUnit ms("ms", 1, 0.001);
    EXPECT_EQ(ms.toString(0.0), "0");
    EXPECT_EQ(ms.toString(1.0), "1000");
    EXPECT_EQ(ms.toString(-1.0), "-1000");
    EXPECT_EQ(ms.toString(0.35), "350");
    EXPECT_EQ(ms.toString(1.005), "1005");
    EXPECT_EQ(ms.toString(12.345678), "12346");
    EXPECT_EQ(ms.toString(0.123456789), "123");
    EXPECT_EQ(ms.toString(-0.04), "-40");
    EXPECT_EQ(ms.toString(-0.0), "-0");
    EXPECT_EQ(ms.toString(1.0E-7), "0");
    EXPECT_EQ(ms.toString(-1.0E-7), "-0");
    EXPECT_EQ(ms.toString(60.123456), "60123");
    EXPECT_EQ(ms.toString(1.0E7), "10000000000");
    // 1e20 / 0.001 is the double 1e23, printed with its shortest digits as JDK 19+ does
    // (JDK 17 prints "99999999999999990000000", JDK-4511638; see Strings::javaDoubleToString).
    EXPECT_EQ(ms.toString(1.0E20), "100000000000000000000000");
    EXPECT_EQ(ms.toStringUnit(1.5), "1500 ms");
    EXPECT_EQ(ms.toString(kInf), "Infinity");

    const FixedPrecisionUnit kelvin("K", 0.01);
    EXPECT_EQ(kelvin.toString(0.0), "0.00");
    EXPECT_EQ(kelvin.toString(1.005), "1.01");
    EXPECT_EQ(kelvin.toString(1.015), "1.02");
    EXPECT_EQ(kelvin.toString(283.15), "283.15");
    EXPECT_EQ(kelvin.toStringUnit(1.5), "1.50 K");

    const FixedPrecisionUnit mbar("mbar", 0.01, 1.0e2);
    EXPECT_EQ(mbar.toString(101325.0), "1013.25");
    EXPECT_EQ(mbar.toString(1.0), "0.01");
    EXPECT_EQ(mbar.toString(1.5), "0.02");
    EXPECT_EQ(mbar.toString(2.5), "0.03");
    EXPECT_EQ(mbar.toString(0.5), "0.01");
    EXPECT_EQ(mbar.toString(60.123456), "0.60");
    EXPECT_EQ(mbar.toString(1.0E20), "1000000000000000000.00");
    EXPECT_EQ(mbar.toStringUnit(1.5), "0.02 mbar");

    const FixedPrecisionUnit permille("‰", 1, 0.001);
    EXPECT_EQ(permille.toString(1.0), "1000");
    EXPECT_EQ(permille.toString(12.345678), "12346");
    EXPECT_EQ(permille.toStringUnit(1.5), "1500 ‰");

    const FixedPrecisionUnit coefficient("​", 0.001);
    EXPECT_EQ(coefficient.toString(0.0), "0.000");
    EXPECT_EQ(coefficient.toString(1.005), "1.005");
    EXPECT_EQ(coefficient.toString(12.345678), "12.346");
    EXPECT_EQ(coefficient.toString(0.123456789), "0.123");
    EXPECT_EQ(coefficient.toString(-1.0E-7), "-0.000");
    EXPECT_EQ(coefficient.toStringUnit(1.5), "1.500 ​");

    const FixedPrecisionUnit scaling("​", 0.1);
    EXPECT_EQ(scaling.toString(0.05), "0.1");
    EXPECT_EQ(scaling.toString(1.75), "1.8");
    EXPECT_EQ(scaling.toStringUnit(1.5), "1.5 ​");
}

TEST_F(FixedPrecisionUnitTest, RoundIsJavasMathRound)
{
    // Ties go up (toward positive infinity), even for values that floor(x + 0.5) would misround.
    EXPECT_DOUBLE_EQ(unitPoint1.round(1.25), 1.3);
    EXPECT_DOUBLE_EQ(unitPoint1.round(0.75), 0.8);
    EXPECT_DOUBLE_EQ(unitPoint1.round(-0.05), 0.0);
    EXPECT_DOUBLE_EQ(unitPoint1.round(0.05), 0.1);
    EXPECT_DOUBLE_EQ(unitPoint1.round(0.15), 0.2);
    EXPECT_DOUBLE_EQ(unitPoint1.round(2.5), 2.5);
    EXPECT_DOUBLE_EQ(unitPoint1.round(-2.5), -2.5);
    EXPECT_DOUBLE_EQ(unitPoint1.round(0.49999999999999994), 0.5);
    EXPECT_DOUBLE_EQ(unitPoint1.round(1.0E17), 1.0E17);
    EXPECT_DOUBLE_EQ(unitPoint1.getNextValue(1.25), 1.4);
    EXPECT_DOUBLE_EQ(unitPoint1.getPreviousValue(1.25), 1.2);
    EXPECT_DOUBLE_EQ(unitPoint1.getNextValue(-0.05), 0.1);
    EXPECT_DOUBLE_EQ(unitPoint1.getPreviousValue(-0.05), -0.2);
    EXPECT_DOUBLE_EQ(unitPoint1.getNextValue(1.0E17), 1.0E17);

    EXPECT_DOUBLE_EQ(unitPoint25.round(1.14), 1.25);
    EXPECT_DOUBLE_EQ(unitPoint25.round(1.24), 1.25);
    EXPECT_DOUBLE_EQ(unitPoint25.round(0.9), 1.0);
    EXPECT_DOUBLE_EQ(unitPoint25.round(0.05), 0.0);
    EXPECT_DOUBLE_EQ(unitPoint25.round(0.15), 0.25);
    EXPECT_DOUBLE_EQ(unitPoint25.round(-2.5), -2.5);
    EXPECT_DOUBLE_EQ(unitPoint25.getNextValue(2.5), 2.75);
    EXPECT_DOUBLE_EQ(unitPoint25.getPreviousValue(-2.5), -2.75);
    EXPECT_DOUBLE_EQ(unitPoint25.round(0.49999999999999994), 0.5);

    EXPECT_DOUBLE_EQ(unitPoint5.round(1.24), 1.0);
    EXPECT_DOUBLE_EQ(unitPoint5.round(1.25), 1.5);
    EXPECT_DOUBLE_EQ(unitPoint5.round(0.75), 1.0);
    EXPECT_DOUBLE_EQ(unitPoint5.round(0.15), 0.0);
    EXPECT_DOUBLE_EQ(unitPoint5.round(-0.05), 0.0);
    EXPECT_DOUBLE_EQ(unitPoint5.getNextValue(1.25), 2.0);
    EXPECT_DOUBLE_EQ(unitPoint5.getPreviousValue(1.25), 1.0);
    EXPECT_DOUBLE_EQ(unitPoint5.round(0.49999999999999994), 0.5);

    // NaN rounds to 0 (Math.round(NaN) is 0).
    EXPECT_DOUBLE_EQ(unitPoint1.round(kNaN), 0.0);
    EXPECT_DOUBLE_EQ(unitPoint1.getNextValue(kNaN), 0.0);
    EXPECT_DOUBLE_EQ(unitPoint25.getPreviousValue(kNaN), 0.0);

    // The multiplier does not take part in rounding (the value is already in the unit).
    EXPECT_DOUBLE_EQ(unitWithMultiplier.round(1.05), 1.1);
    EXPECT_DOUBLE_EQ(unitWithMultiplier.getNextValue(1.0), 1.1);
    EXPECT_DOUBLE_EQ(unitWithMultiplier.getPreviousValue(1.0), 0.9);
}

TEST_F(FixedPrecisionUnitTest, TicksAreGeneralUnitsTicks)
{
    const FixedPrecisionUnit unit("unit", 0.5);
    const std::vector<Tick>  ticks = unit.getTicks(0.7, 3.2, 0.2, 0.9);
    ASSERT_EQ(ticks.size(), 5U);
    const double expectedValues[]  = {1.0, 1.5, 2.0, 2.5, 3.0};
    const bool   expectedMajor[]   = {true, false, true, false, true};
    const bool   expectedNotable[] = {false, true, false, true, false};
    for (std::size_t i = 0; i < ticks.size(); i++)
    {
        EXPECT_DOUBLE_EQ(ticks[i].value, expectedValues[i]);
        EXPECT_DOUBLE_EQ(ticks[i].unitValue, expectedValues[i]);
        EXPECT_EQ(ticks[i].major, expectedMajor[i]);
        EXPECT_EQ(ticks[i].notable, expectedNotable[i]);
    }
    EXPECT_THROW(static_cast<void>(unit.getTicks(0, 1, 0.5, 0.25)), std::invalid_argument);
}

TEST_F(FixedPrecisionUnitTest, CloneAndEquality)
{
    const std::unique_ptr<Unit> copy = unitNoTrailing.clone();
    ASSERT_NE(dynamic_cast<const FixedPrecisionUnit*>(copy.get()), nullptr);
    EXPECT_EQ(copy->toString(1.0), "1");
    EXPECT_TRUE(copy->equals(unitNoTrailing));
    // Only the class, multiplier and name count, as in Unit.equals.
    EXPECT_TRUE(unitPoint1.equals(unitPoint25));
    EXPECT_FALSE(unitPoint1.equals(unitWithMultiplier));
}

}  // namespace
