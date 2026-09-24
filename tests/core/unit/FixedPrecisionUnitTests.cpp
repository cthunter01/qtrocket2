#include "QtRocket/unit/FixedPrecisionUnit.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/unit/Tick.h"
#include "QtRocket/util/BugError.h"

namespace
{

using QtRocket::BugError;
using QtRocket::FixedPrecisionUnit;
using QtRocket::Tick;

constexpr double kEpsilon = 1e-8;
constexpr double kNaN     = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf     = std::numeric_limits<double>::infinity();

// The expected strings and values were produced by OpenRocket's FixedPrecisionUnit on JDK 17.

class FixedPrecisionUnitTest : public ::testing::Test
{
protected:
    FixedPrecisionUnit m_unitPoint1{"unit", 0.1};
    FixedPrecisionUnit m_unitPoint25{"unit", 0.25};
    FixedPrecisionUnit m_unitPoint5{"unit", 0.5};
    FixedPrecisionUnit m_unitNoTrailing{"unit", 0.1, 1.0, false};
    FixedPrecisionUnit m_unitWithMultiplier{"unit", 0.1, 2.0};
};

/// The ticks are in ascending order, spaced by the precision 0.5 (FixedPrecisionUnitTest).
void expectAscendingByHalves(const std::vector<Tick>& ticks)
{
    for (std::size_t i = 0; i + 1 < ticks.size(); i++)
    {
        EXPECT_LT(ticks[i].value, ticks[i + 1].value);
        EXPECT_NEAR(0.5, ticks[i + 1].value - ticks[i].value, 0.001);
    }
}

/// Whole numbers are major ticks.
void expectWholeNumbersMajor(const std::vector<Tick>& ticks)
{
    for (const Tick& tick : ticks)
    {
        if (std::abs(std::fmod(tick.value, 1.0)) < 0.001)
        {
            EXPECT_TRUE(tick.major);
        }
    }
}

// ---- Ported from FixedPrecisionUnitTest.java ----

TEST_F(FixedPrecisionUnitTest, Rounding)
{
    // Test rounding with 0.1 precision
    EXPECT_NEAR(1.0, m_unitPoint1.round(1.04), kEpsilon);
    EXPECT_NEAR(1.1, m_unitPoint1.round(1.05), kEpsilon);
    EXPECT_NEAR(1.1, m_unitPoint1.round(1.14), kEpsilon);
    EXPECT_NEAR(1.2, m_unitPoint1.round(1.15), kEpsilon);

    // Test rounding with 0.25 precision
    EXPECT_NEAR(1.0, m_unitPoint25.round(1.12), kEpsilon);
    EXPECT_NEAR(1.25, m_unitPoint25.round(1.13), kEpsilon);
    EXPECT_NEAR(1.25, m_unitPoint25.round(1.37), kEpsilon);
    EXPECT_NEAR(1.5, m_unitPoint25.round(1.38), kEpsilon);

    // Test rounding with 0.5 precision
    EXPECT_NEAR(1.0, m_unitPoint5.round(1.24), kEpsilon);
    EXPECT_NEAR(1.5, m_unitPoint5.round(1.25), kEpsilon);
    EXPECT_NEAR(1.5, m_unitPoint5.round(1.74), kEpsilon);
    EXPECT_NEAR(2.0, m_unitPoint5.round(1.75), kEpsilon);
}

TEST_F(FixedPrecisionUnitTest, NextValue)
{
    // Test with 0.1 precision
    EXPECT_NEAR(1.1, m_unitPoint1.getNextValue(1.0), kEpsilon);
    EXPECT_NEAR(1.2, m_unitPoint1.getNextValue(1.1), kEpsilon);
    EXPECT_NEAR(0.1, m_unitPoint1.getNextValue(0.0), kEpsilon);
    EXPECT_NEAR(-0.9, m_unitPoint1.getNextValue(-1.0), kEpsilon);

    // Test with 0.25 precision
    EXPECT_NEAR(1.25, m_unitPoint25.getNextValue(1.0), kEpsilon);
    EXPECT_NEAR(1.5, m_unitPoint25.getNextValue(1.25), kEpsilon);
    EXPECT_NEAR(0.25, m_unitPoint25.getNextValue(0.0), kEpsilon);

    // Test with 0.5 precision
    EXPECT_NEAR(1.5, m_unitPoint5.getNextValue(1.0), kEpsilon);
    EXPECT_NEAR(2.0, m_unitPoint5.getNextValue(1.5), kEpsilon);
    EXPECT_NEAR(0.5, m_unitPoint5.getNextValue(0.0), kEpsilon);
}

TEST_F(FixedPrecisionUnitTest, PreviousValue)
{
    // Test with 0.1 precision
    EXPECT_NEAR(0.9, m_unitPoint1.getPreviousValue(1.0), kEpsilon);
    EXPECT_NEAR(0.8, m_unitPoint1.getPreviousValue(0.9), kEpsilon);
    EXPECT_NEAR(-0.1, m_unitPoint1.getPreviousValue(0.0), kEpsilon);
    EXPECT_NEAR(-1.1, m_unitPoint1.getPreviousValue(-1.0), kEpsilon);

    // Test with 0.25 precision
    EXPECT_NEAR(0.75, m_unitPoint25.getPreviousValue(1.0), kEpsilon);
    EXPECT_NEAR(0.5, m_unitPoint25.getPreviousValue(0.75), kEpsilon);
    EXPECT_NEAR(-0.25, m_unitPoint25.getPreviousValue(0.0), kEpsilon);

    // Test with 0.5 precision
    EXPECT_NEAR(0.5, m_unitPoint5.getPreviousValue(1.0), kEpsilon);
    EXPECT_NEAR(0.0, m_unitPoint5.getPreviousValue(0.5), kEpsilon);
    EXPECT_NEAR(-0.5, m_unitPoint5.getPreviousValue(0.0), kEpsilon);
}

TEST_F(FixedPrecisionUnitTest, ToString)
{
    // Test with trailing zeros (default)
    EXPECT_EQ("1.0", m_unitPoint1.toString(1.0));
    EXPECT_EQ("1.1", m_unitPoint1.toString(1.1));
    EXPECT_EQ("-1.0", m_unitPoint1.toString(-1.0));
    EXPECT_EQ("0.0", m_unitPoint1.toString(0.0));

    // Test without trailing zeros
    EXPECT_EQ("1", m_unitNoTrailing.toString(1.0));
    EXPECT_EQ("1.1", m_unitNoTrailing.toString(1.1));
    EXPECT_EQ("-1", m_unitNoTrailing.toString(-1.0));
    EXPECT_EQ("0", m_unitNoTrailing.toString(0.0));

    // Test with multiplier
    EXPECT_EQ("0.5", m_unitWithMultiplier.toString(1.0));
    EXPECT_EQ("0.6", m_unitWithMultiplier.toString(1.1));
    EXPECT_EQ("-0.5", m_unitWithMultiplier.toString(-1.0));
    EXPECT_EQ("0.0", m_unitWithMultiplier.toString(0.0));
}

TEST_F(FixedPrecisionUnitTest, GetTicks)
{
    const FixedPrecisionUnit unit("unit", 0.5);
    const std::vector<Tick>  ticks = unit.getTicks(0.0, 5.0, 0.5, 1.0);

    // Verify we have the expected number of ticks
    ASSERT_FALSE(ticks.empty());

    expectAscendingByHalves(ticks);
    expectWholeNumbersMajor(ticks);
}

// ---- QtRocket additions ----

TEST_F(FixedPrecisionUnitTest, DecimalsFollowThePrecision)
{
    EXPECT_EQ(m_unitPoint1.getDecimals(), 1);
    EXPECT_EQ(m_unitPoint25.getDecimals(), 2);
    EXPECT_EQ(m_unitPoint5.getDecimals(), 1);
    EXPECT_EQ(FixedPrecisionUnit("ms", 1, 0.001).getDecimals(), 0);
    EXPECT_EQ(FixedPrecisionUnit("K", 0.01).getDecimals(), 2);
    EXPECT_EQ(FixedPrecisionUnit("x", 0.001).getDecimals(), 3);
    EXPECT_EQ(FixedPrecisionUnit("° N", 10E-6, 1, false).getDecimals(), 5);
    EXPECT_EQ(m_unitPoint1.getPrecision(), 0.1);
    EXPECT_TRUE(m_unitPoint1.displaysTrailingZeros());
    EXPECT_FALSE(m_unitNoTrailing.displaysTrailingZeros());
    EXPECT_EQ(m_unitWithMultiplier.getMultiplier(), 2.0);
    EXPECT_TRUE(m_unitPoint1.hasSpace());
}

TEST_F(FixedPrecisionUnitTest, ToStringWithTrailingZerosIsJavasFormatter)
{
    // Half-up on the decimal digits, as String.format("%.1f") does.
    EXPECT_EQ(m_unitPoint1.toString(0.05), "0.1");
    EXPECT_EQ(m_unitPoint1.toString(0.15), "0.2");
    EXPECT_EQ(m_unitPoint1.toString(0.25), "0.3");
    EXPECT_EQ(m_unitPoint1.toString(0.35), "0.4");
    EXPECT_EQ(m_unitPoint1.toString(1.005), "1.0");
    EXPECT_EQ(m_unitPoint1.toString(1.015), "1.0");
    EXPECT_EQ(m_unitPoint1.toString(1.05), "1.1");
    EXPECT_EQ(m_unitPoint1.toString(1.15), "1.2");
    EXPECT_EQ(m_unitPoint1.toString(1.25), "1.3");
    EXPECT_EQ(m_unitPoint1.toString(1.75), "1.8");
    EXPECT_EQ(m_unitPoint1.toString(2.675), "2.7");
    EXPECT_EQ(m_unitPoint1.toString(12.345678), "12.3");
    EXPECT_EQ(m_unitPoint1.toString(1234.5678), "1234.6");
    EXPECT_EQ(m_unitPoint1.toString(-12.3456789), "-12.3");
    EXPECT_EQ(m_unitPoint1.toString(101325.0), "101325.0");
    EXPECT_EQ(m_unitPoint1.toString(1.0E7), "10000000.0");
    EXPECT_EQ(m_unitPoint1.toString(1.0E20), "100000000000000000000.0");
    EXPECT_EQ(m_unitPoint1.toString(1.0E-7), "0.0");
    EXPECT_EQ(m_unitPoint1.toString(-1.0E-7), "-0.0");
    EXPECT_EQ(m_unitPoint1.toString(-0.04), "-0.0");
    EXPECT_EQ(m_unitPoint1.toString(-0.0), "-0.0");
    // NaN and the infinities are formatted, not "N/A"; toStringUnit still says "N/A" for NaN.
    EXPECT_EQ(m_unitPoint1.toString(kNaN), "NaN");
    EXPECT_EQ(m_unitPoint1.toString(kInf), "Infinity");
    EXPECT_EQ(m_unitPoint1.toString(-kInf), "-Infinity");
    EXPECT_EQ(m_unitPoint1.toStringUnit(1.5), "1.5 unit");
    EXPECT_EQ(m_unitPoint1.toStringUnit(kNaN), "N/A");
    EXPECT_EQ(m_unitPoint1.toStringUnit(kInf), "Infinity unit");

    EXPECT_EQ(m_unitPoint25.toString(0.0), "0.00");
    EXPECT_EQ(m_unitPoint25.toString(1.005), "1.01");
    EXPECT_EQ(m_unitPoint25.toString(1.015), "1.02");
    EXPECT_EQ(m_unitPoint25.toString(12.345678), "12.35");
    EXPECT_EQ(m_unitPoint25.toString(0.123456789), "0.12");
    EXPECT_EQ(m_unitPoint25.toString(2.675), "2.68");
    EXPECT_EQ(m_unitPoint25.toString(-0.04), "-0.04");
    EXPECT_EQ(m_unitPoint25.toString(1234.5678), "1234.57");
    EXPECT_EQ(m_unitPoint25.toStringUnit(1.5), "1.50 unit");

    EXPECT_EQ(m_unitWithMultiplier.toString(0.05), "0.0");
    EXPECT_EQ(m_unitWithMultiplier.toString(0.15), "0.1");
    EXPECT_EQ(m_unitWithMultiplier.toString(0.25), "0.1");
    EXPECT_EQ(m_unitWithMultiplier.toString(0.35), "0.2");
    EXPECT_EQ(m_unitWithMultiplier.toString(0.5), "0.3");
    EXPECT_EQ(m_unitWithMultiplier.toString(1.5), "0.8");
    EXPECT_EQ(m_unitWithMultiplier.toString(2.5), "1.3");
    EXPECT_EQ(m_unitWithMultiplier.toString(12.345678), "6.2");
    EXPECT_EQ(m_unitWithMultiplier.toString(101325.0), "50662.5");
    EXPECT_EQ(m_unitWithMultiplier.toString(1.0E20), "50000000000000000000.0");
    EXPECT_EQ(m_unitWithMultiplier.toStringUnit(1.5), "0.8 unit");
}

TEST_F(FixedPrecisionUnitTest, ToStringWithoutTrailingZerosIsDecimalFormat)
{
    // Half to even on the exact binary value, as DecimalFormat("0.#") does.
    EXPECT_EQ(m_unitNoTrailing.toString(0.05), "0.1");
    EXPECT_EQ(m_unitNoTrailing.toString(0.15), "0.1");
    EXPECT_EQ(m_unitNoTrailing.toString(0.25), "0.2");
    EXPECT_EQ(m_unitNoTrailing.toString(0.35), "0.3");
    EXPECT_EQ(m_unitNoTrailing.toString(1.005), "1");
    EXPECT_EQ(m_unitNoTrailing.toString(1.05), "1.1");
    EXPECT_EQ(m_unitNoTrailing.toString(1.15), "1.1");
    EXPECT_EQ(m_unitNoTrailing.toString(1.25), "1.2");
    EXPECT_EQ(m_unitNoTrailing.toString(1.75), "1.8");
    EXPECT_EQ(m_unitNoTrailing.toString(2.675), "2.7");
    EXPECT_EQ(m_unitNoTrailing.toString(12.345678), "12.3");
    EXPECT_EQ(m_unitNoTrailing.toString(1234.5678), "1234.6");
    EXPECT_EQ(m_unitNoTrailing.toString(101325.0), "101325");
    EXPECT_EQ(m_unitNoTrailing.toString(1.0E7), "10000000");
    EXPECT_EQ(m_unitNoTrailing.toString(1.0E20), "100000000000000000000");
    EXPECT_EQ(m_unitNoTrailing.toString(1.0E-7), "0");
    EXPECT_EQ(m_unitNoTrailing.toString(-1.0E-7), "-0");
    EXPECT_EQ(m_unitNoTrailing.toString(-0.04), "-0");
    EXPECT_EQ(m_unitNoTrailing.toString(-0.0), "-0");
    EXPECT_EQ(m_unitNoTrailing.toString(kNaN), "NaN");
    EXPECT_EQ(m_unitNoTrailing.toString(kInf), "∞");
    EXPECT_EQ(m_unitNoTrailing.toString(-kInf), "-∞");
    EXPECT_EQ(m_unitNoTrailing.toStringUnit(1.5), "1.5 unit");
    EXPECT_EQ(m_unitNoTrailing.toStringUnit(kNaN), "N/A");

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

TEST_F(FixedPrecisionUnitTest, WithoutTrailingZerosLargeValuesTiesAndZerosMatchOpenRocket)
{
    const FixedPrecisionUnit tenths("u", 0.1, 1.0, false);
    const FixedPrecisionUnit south("s", 10E-6, -1, false);
    EXPECT_EQ(tenths.toString(0x1p69), "590295810358705650000");
    EXPECT_EQ(tenths.toString(1e23), "99999999999999990000000");
    EXPECT_EQ(tenths.toString(0.05), "0.1");
    EXPECT_EQ(tenths.toString(0.15), "0.1");
    EXPECT_EQ(tenths.toString(0.25), "0.2");
    EXPECT_EQ(tenths.toString(0.45), "0.5");
    EXPECT_EQ(tenths.toString(-0.04), "-0");
    EXPECT_EQ(tenths.toString(-0.0), "-0");
    EXPECT_EQ(tenths.toString(-0.05), "-0.1");
    // Double.toString(5e-6) is "5.0E-6", whose '5' is followed by a zero: no tie, rounded down.
    EXPECT_EQ(south.toString(5e-6), "-0");
    EXPECT_EQ(south.toString(-5e-6), "0");
    EXPECT_EQ(south.toString(-4.9e-6), "0");
    EXPECT_EQ(south.toString(1.5e-5), "-0.00002");
    EXPECT_EQ(south.toString(2.5e-5), "-0.00003");
    EXPECT_EQ(south.toString(0.0), "-0");
    EXPECT_EQ(south.toString(1.23456785), "-1.23457");
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
    EXPECT_DOUBLE_EQ(m_unitPoint1.round(1.25), 1.3);
    EXPECT_DOUBLE_EQ(m_unitPoint1.round(0.75), 0.8);
    EXPECT_DOUBLE_EQ(m_unitPoint1.round(-0.05), 0.0);
    EXPECT_DOUBLE_EQ(m_unitPoint1.round(0.05), 0.1);
    EXPECT_DOUBLE_EQ(m_unitPoint1.round(0.15), 0.2);
    EXPECT_DOUBLE_EQ(m_unitPoint1.round(2.5), 2.5);
    EXPECT_DOUBLE_EQ(m_unitPoint1.round(-2.5), -2.5);
    EXPECT_DOUBLE_EQ(m_unitPoint1.round(0.49999999999999994), 0.5);
    EXPECT_DOUBLE_EQ(m_unitPoint1.round(1.0E17), 1.0E17);
    EXPECT_DOUBLE_EQ(m_unitPoint1.getNextValue(1.25), 1.4);
    EXPECT_DOUBLE_EQ(m_unitPoint1.getPreviousValue(1.25), 1.2);
    EXPECT_DOUBLE_EQ(m_unitPoint1.getNextValue(-0.05), 0.1);
    EXPECT_DOUBLE_EQ(m_unitPoint1.getPreviousValue(-0.05), -0.2);
    EXPECT_DOUBLE_EQ(m_unitPoint1.getNextValue(1.0E17), 1.0E17);

    EXPECT_DOUBLE_EQ(m_unitPoint25.round(1.14), 1.25);
    EXPECT_DOUBLE_EQ(m_unitPoint25.round(1.24), 1.25);
    EXPECT_DOUBLE_EQ(m_unitPoint25.round(0.9), 1.0);
    EXPECT_DOUBLE_EQ(m_unitPoint25.round(0.05), 0.0);
    EXPECT_DOUBLE_EQ(m_unitPoint25.round(0.15), 0.25);
    EXPECT_DOUBLE_EQ(m_unitPoint25.round(-2.5), -2.5);
    EXPECT_DOUBLE_EQ(m_unitPoint25.getNextValue(2.5), 2.75);
    EXPECT_DOUBLE_EQ(m_unitPoint25.getPreviousValue(-2.5), -2.75);
    EXPECT_DOUBLE_EQ(m_unitPoint25.round(0.49999999999999994), 0.5);

    EXPECT_DOUBLE_EQ(m_unitPoint5.round(1.24), 1.0);
    EXPECT_DOUBLE_EQ(m_unitPoint5.round(1.25), 1.5);
    EXPECT_DOUBLE_EQ(m_unitPoint5.round(0.75), 1.0);
    EXPECT_DOUBLE_EQ(m_unitPoint5.round(0.15), 0.0);
    EXPECT_DOUBLE_EQ(m_unitPoint5.round(-0.05), 0.0);
    EXPECT_DOUBLE_EQ(m_unitPoint5.getNextValue(1.25), 2.0);
    EXPECT_DOUBLE_EQ(m_unitPoint5.getPreviousValue(1.25), 1.0);
    EXPECT_DOUBLE_EQ(m_unitPoint5.round(0.49999999999999994), 0.5);

    // NaN rounds to 0 (Math.round(NaN) is 0).
    EXPECT_DOUBLE_EQ(m_unitPoint1.round(kNaN), 0.0);
    EXPECT_DOUBLE_EQ(m_unitPoint1.getNextValue(kNaN), 0.0);
    EXPECT_DOUBLE_EQ(m_unitPoint25.getPreviousValue(kNaN), 0.0);

    // The multiplier does not take part in rounding (the value is already in the unit).
    EXPECT_DOUBLE_EQ(m_unitWithMultiplier.round(1.05), 1.1);
    EXPECT_DOUBLE_EQ(m_unitWithMultiplier.getNextValue(1.0), 1.1);
    EXPECT_DOUBLE_EQ(m_unitWithMultiplier.getPreviousValue(1.0), 0.9);
}

void expectTick(const Tick& tick, double value, bool major, bool notable)
{
    EXPECT_EQ(tick.value, value);
    EXPECT_EQ(tick.unitValue, value);
    EXPECT_EQ(tick.major, major);
    EXPECT_EQ(tick.notable, notable);
}

TEST_F(FixedPrecisionUnitTest, TicksAreGeneralUnitsTicks)
{
    const FixedPrecisionUnit unit("unit", 0.5);
    const std::vector<Tick>  ticks = unit.getTicks(0.7, 3.2, 0.2, 0.9);
    ASSERT_EQ(ticks.size(), 5U);
    expectTick(ticks[0], 1.0, true, false);
    expectTick(ticks[1], 1.5, false, true);
    expectTick(ticks[2], 2.0, true, false);
    expectTick(ticks[3], 2.5, false, true);
    expectTick(ticks[4], 3.0, true, false);
    EXPECT_THROW(static_cast<void>(unit.getTicks(0, 1, 0.5, 0.25)), BugError);
}

TEST_F(FixedPrecisionUnitTest, Equality)
{
    // Only the class, multiplier and name count, as in Unit.equals.
    EXPECT_TRUE(m_unitPoint1.equals(m_unitPoint25));
    EXPECT_FALSE(m_unitPoint1.equals(m_unitWithMultiplier));
}

}  // namespace
