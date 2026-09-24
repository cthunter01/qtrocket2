#include "QtRocket/unit/FractionalUnit.h"

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

using QtRocket::FractionalUnit;
using QtRocket::Tick;
using QtRocket::Unit;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

// The expected strings were produced by OpenRocket's FractionalUnit on JDK 17 (Locale.US).

const FractionalUnit testUnit(1, "unit", "unit", 4, 0.5);
const FractionalUnit testUnitApprox(1, "unit", "unit", 16, 0.5, 0.02);
const FractionalUnit inchUnit(0.0254, "in/64", "in", 64, 1.0 / 16.0);

// ---- Ported from FractionalUnitTest.java ----

TEST(FractionalUnit, Round)
{
    EXPECT_EQ(-1.0, testUnit.round(-1.125));  // rounds to -1 since mod is even
    EXPECT_EQ(-1.0, testUnit.round(-1.0));
    EXPECT_EQ(-1.0, testUnit.round(-0.875));  // rounds to -1 since mod is even

    EXPECT_EQ(-0.75, testUnit.round(-0.874));
    EXPECT_EQ(-0.75, testUnit.round(-0.75));
    EXPECT_EQ(-0.75, testUnit.round(-0.626));

    EXPECT_EQ(-0.5, testUnit.round(-0.625));  // rounds to -.5 since mod is even
    EXPECT_EQ(-0.5, testUnit.round(-0.5));
    EXPECT_EQ(-0.5, testUnit.round(-0.375));  // rounds to -.5 since mod is even

    EXPECT_EQ(-0.25, testUnit.round(-0.374));
    EXPECT_EQ(-0.25, testUnit.round(-0.25));
    EXPECT_EQ(-0.25, testUnit.round(-0.126));

    EXPECT_EQ(0.0, testUnit.round(-0.125));
    EXPECT_EQ(0.0, testUnit.round(0));
    EXPECT_EQ(0.0, testUnit.round(0.125));

    EXPECT_EQ(0.25, testUnit.round(0.126));
    EXPECT_EQ(0.25, testUnit.round(0.25));
    EXPECT_EQ(0.25, testUnit.round(0.374));

    EXPECT_EQ(0.5, testUnit.round(0.375));  // rounds to .5 since mod is even
    EXPECT_EQ(0.5, testUnit.round(0.5));
    EXPECT_EQ(0.5, testUnit.round(0.625));  // rounds to .5 since mod is even

    EXPECT_EQ(0.75, testUnit.round(0.626));
    EXPECT_EQ(0.75, testUnit.round(0.75));
    EXPECT_EQ(0.75, testUnit.round(0.874));

    EXPECT_EQ(1.0, testUnit.round(0.875));  // rounds to 1 since mod is even
    EXPECT_EQ(1.0, testUnit.round(1.0));
    EXPECT_EQ(1.0, testUnit.round(1.125));  // rounds to 1 since mod is even
}

TEST(FractionalUnit, Increment)
{
    EXPECT_EQ(-1.0, testUnit.getNextValue(-1.2));
    EXPECT_EQ(-1.0, testUnit.getNextValue(-1.4));

    EXPECT_EQ(-0.5, testUnit.getNextValue(-0.7));
    EXPECT_EQ(-0.5, testUnit.getNextValue(-0.9));
    EXPECT_EQ(-0.5, testUnit.getNextValue(-1.0));

    EXPECT_EQ(0.0, testUnit.getNextValue(-0.05));
    EXPECT_EQ(0.0, testUnit.getNextValue(-0.062));
    EXPECT_EQ(0.0, testUnit.getNextValue(-0.07));
    EXPECT_EQ(0.0, testUnit.getNextValue(-0.11));

    EXPECT_EQ(0.5, testUnit.getNextValue(0));
    EXPECT_EQ(0.5, testUnit.getNextValue(0.01));
    EXPECT_EQ(0.5, testUnit.getNextValue(0.062));
    EXPECT_EQ(0.5, testUnit.getNextValue(0.0625));

    EXPECT_EQ(1.0, testUnit.getNextValue(0.51));
    EXPECT_EQ(1.0, testUnit.getNextValue(0.7));
}

TEST(FractionalUnit, Decrement)
{
    EXPECT_EQ(-1.5, testUnit.getPreviousValue(-1.2));
    EXPECT_EQ(-1.5, testUnit.getPreviousValue(-1.4));
    EXPECT_EQ(-1.5, testUnit.getPreviousValue(-1.0));

    EXPECT_EQ(-1.0, testUnit.getPreviousValue(-0.7));
    EXPECT_EQ(-1.0, testUnit.getPreviousValue(-0.9));

    EXPECT_EQ(-0.5, testUnit.getPreviousValue(-0.01));
    EXPECT_EQ(-0.5, testUnit.getPreviousValue(-0.05));
    EXPECT_EQ(-0.5, testUnit.getPreviousValue(-0.062));
    EXPECT_EQ(-0.5, testUnit.getPreviousValue(-0.07));
    EXPECT_EQ(-0.5, testUnit.getPreviousValue(0));

    EXPECT_EQ(0.0, testUnit.getPreviousValue(0.49));
    EXPECT_EQ(0.0, testUnit.getPreviousValue(0.262));
    EXPECT_EQ(0.0, testUnit.getPreviousValue(0.51));

    EXPECT_EQ(0.5, testUnit.getPreviousValue(0.7));

    EXPECT_EQ(1.0, testUnit.getPreviousValue(1.2));
}

TEST(FractionalUnit, ToStringDefaultPrecision)
{
    // The point is always the decimal separator here (OpenRocket follows the locale).
    EXPECT_EQ(testUnit.toString(-1.2), "-1.2");
    EXPECT_EQ(testUnit.toString(-1.3), "-1.3");

    EXPECT_EQ(testUnit.toString(-0.2), "-0.2");
    EXPECT_EQ(testUnit.toString(-0.3), "-0.3");

    EXPECT_EQ(testUnit.toString(-0.1), "-0.1");
    EXPECT_EQ(testUnit.toString(0.1), "0.1");

    EXPECT_EQ(testUnit.toString(0.2), "0.2");
    EXPECT_EQ(testUnit.toString(0.3), "0.3");

    EXPECT_EQ(testUnit.toString(1.2), "1.2");
    EXPECT_EQ(testUnit.toString(1.3), "1.3");

    // default epsilon is 0.025

    EXPECT_EQ(testUnit.toString(-1.225), "-1 ¹⁄₄");
    EXPECT_EQ(testUnit.toString(-1.227), "-1 ¹⁄₄");
    EXPECT_EQ(testUnit.toString(-1.25), "-1 ¹⁄₄");
    EXPECT_EQ(testUnit.toString(-1.25), "-1 ¹⁄₄");
    EXPECT_EQ(testUnit.toString(-1.275), "-1 ¹⁄₄");

    EXPECT_EQ(testUnit.toString(-0.225), "-¹⁄₄");
    EXPECT_EQ(testUnit.toString(-0.25), "-¹⁄₄");
    EXPECT_EQ(testUnit.toString(-0.274), "-¹⁄₄");
    // testUnit.toString(-.275) is "-0.275": the round-off error pushes it over epsilon.

    EXPECT_EQ(testUnit.toString(-0.024), "0");
    EXPECT_EQ(testUnit.toString(0), "0");
    EXPECT_EQ(testUnit.toString(0.024), "0");

    EXPECT_EQ(testUnit.toString(0.225), "¹⁄₄");
    EXPECT_EQ(testUnit.toString(0.25), "¹⁄₄");
    EXPECT_EQ(testUnit.toString(0.274), "¹⁄₄");

    EXPECT_EQ(testUnit.toString(1.225), "1 ¹⁄₄");
    EXPECT_EQ(testUnit.toString(1.25), "1 ¹⁄₄");
    EXPECT_EQ(testUnit.toString(1.275), "1 ¹⁄₄");
}

TEST(FractionalUnit, ToStringWithPrecision)
{
    EXPECT_EQ(testUnitApprox.toString(-1.225), "-1.225");
    EXPECT_EQ(testUnitApprox.toString(-1.275), "-1.275");

    EXPECT_EQ(testUnitApprox.toString(-0.225), "-0.225");
    EXPECT_EQ(testUnitApprox.toString(-0.275), "-0.275");

    EXPECT_EQ(testUnitApprox.toString(-0.1), "-0.1");

    EXPECT_EQ(testUnitApprox.toString(-0.024), "-0.024");
    EXPECT_EQ(testUnitApprox.toString(0.024), "0.024");

    EXPECT_EQ(testUnitApprox.toString(0.1), "0.1");

    EXPECT_EQ(testUnitApprox.toString(0.275), "0.275");
    EXPECT_EQ(testUnitApprox.toString(0.225), "0.225");

    EXPECT_EQ(testUnitApprox.toString(1.225), "1.225");
    EXPECT_EQ(testUnitApprox.toString(1.275), "1.275");

    // epsilon is .02

    EXPECT_EQ(testUnitApprox.toString(-1.2), "-1 ³⁄₁₆");
    EXPECT_EQ(testUnitApprox.toString(-1.25), "-1 ¹⁄₄");
    EXPECT_EQ(testUnitApprox.toString(-1.3), "-1 ⁵⁄₁₆");

    EXPECT_EQ(testUnitApprox.toString(-0.2), "-³⁄₁₆");
    EXPECT_EQ(testUnitApprox.toString(-0.25), "-¹⁄₄");
    EXPECT_EQ(testUnitApprox.toString(-0.3), "-⁵⁄₁₆");

    EXPECT_EQ(testUnitApprox.toString(0), "0");

    EXPECT_EQ(testUnitApprox.toString(0.2), "³⁄₁₆");
    EXPECT_EQ(testUnitApprox.toString(0.25), "¹⁄₄");
    EXPECT_EQ(testUnitApprox.toString(0.3), "⁵⁄₁₆");

    EXPECT_EQ(testUnitApprox.toString(1.2), "1 ³⁄₁₆");
    EXPECT_EQ(testUnitApprox.toString(1.25), "1 ¹⁄₄");
    EXPECT_EQ(testUnitApprox.toString(1.3), "1 ⁵⁄₁₆");
}

TEST(FractionalUnit, InchToString)
{
    // Just some random test points.
    EXPECT_EQ(inchUnit.toString(1.0 / 64.0 * 0.0254), "¹⁄₆₄");

    EXPECT_EQ(inchUnit.toString(-5.0 / 64.0 * 0.0254), "-⁵⁄₆₄");

    EXPECT_EQ(inchUnit.toString(9.0 / 2.0 * 0.0254), "4 ¹⁄₂");

    EXPECT_EQ(inchUnit.toString(0.002 * 0.0254), "0.002");

    // default body tube length:
    const double length = 8.0 * 0.025;

    EXPECT_EQ(inchUnit.toString(length), "7 ⁷⁄₈");

    // had problems with round-off in decrement.

    double v = inchUnit.toUnit(length);
    for (int i = 0; i < 15; i++)
    {
        EXPECT_GT(v, inchUnit.getPreviousValue(v));
        v = inchUnit.getPreviousValue(v);
    }
}

// ---- QtRocket additions ----

TEST(FractionalUnit, Properties)
{
    EXPECT_EQ(inchUnit.getUnit(), "in/64");
    EXPECT_EQ(inchUnit.getUnitLabel(), "in");
    EXPECT_EQ(inchUnit.getFractionBase(), 64);
    EXPECT_DOUBLE_EQ(inchUnit.getIncrementValue(), 1.0 / 16.0);
    EXPECT_DOUBLE_EQ(inchUnit.getEpsilon(), 0.1 / 64.0);  // the default epsilon
    EXPECT_DOUBLE_EQ(testUnit.getEpsilon(), 0.025);
    EXPECT_DOUBLE_EQ(testUnitApprox.getEpsilon(), 0.02);
    EXPECT_EQ(inchUnit.getMultiplier(), 0.0254);
}

TEST(FractionalUnit, ToStringUnitUsesTheLabel)
{
    EXPECT_EQ(inchUnit.toStringUnit(1.0 / 64.0 * 0.0254), "¹⁄₆₄ in");
    EXPECT_EQ(inchUnit.toStringUnit(-5.0 / 64.0 * 0.0254), "-⁵⁄₆₄ in");
    EXPECT_EQ(inchUnit.toStringUnit(9.0 / 2.0 * 0.0254), "4 ¹⁄₂ in");
    EXPECT_EQ(inchUnit.toStringUnit(0.002 * 0.0254), "0.002 in");
    EXPECT_EQ(inchUnit.toStringUnit(0.0254), "1 in");
    EXPECT_EQ(inchUnit.toStringUnit(kNaN), "N/A");
}

TEST(FractionalUnit, MoreInchStrings)
{
    EXPECT_EQ(inchUnit.toString(0.0254), "1");
    EXPECT_EQ(inchUnit.toString(0.0254 * 3.0 / 128.0), "0.023");
    EXPECT_EQ(inchUnit.toString(0.0254 * 0.0254), "0.025");
    EXPECT_EQ(inchUnit.toString(0.0254 * 7.999), "8");
    EXPECT_EQ(inchUnit.toString(0.0254 * 7.9925), "7.992");
    EXPECT_EQ(inchUnit.toString(0.0254 * 7.992), "7.992");
    EXPECT_EQ(inchUnit.toString(0.0254 * 0.008), "0.008");
    EXPECT_EQ(inchUnit.toString(0.0254 * 0.0078), "0.008");
    EXPECT_EQ(inchUnit.toString(0.0254 * 0.007), "0.007");

    // The decrement chain of testInchToString, value for value.
    const double expected[] = {7.8125, 7.75, 7.6875, 7.625, 7.5625, 7.5, 7.4375, 7.375,
                               7.3125, 7.25, 7.1875, 7.125, 7.0625, 7.0, 6.9375};
    double       v          = inchUnit.toUnit(8.0 * 0.025);
    EXPECT_DOUBLE_EQ(v, 7.874015748031497);
    for (const double next : expected)
    {
        v = inchUnit.getPreviousValue(v);
        EXPECT_DOUBLE_EQ(v, next);
    }
}

TEST(FractionalUnit, ReducedFractionsAndDecimals)
{
    EXPECT_EQ(testUnit.toString(0.5), "¹⁄₂");
    EXPECT_EQ(testUnit.toString(1.5), "1 ¹⁄₂");
    EXPECT_EQ(testUnit.toString(2.75), "2 ³⁄₄");
    EXPECT_EQ(testUnit.toString(0.0625), "0.062");  // farther than epsilon from a quarter
    EXPECT_EQ(testUnit.toString(0.125), "0.125");
    EXPECT_EQ(testUnit.toString(0.1875), "0.188");
    EXPECT_EQ(testUnit.toString(3.0), "3");
    EXPECT_EQ(testUnit.toString(-3.0), "-3");
    EXPECT_EQ(testUnit.toString(0.984375), "1");
    EXPECT_EQ(testUnit.toString(1.015625), "1");
    EXPECT_EQ(testUnit.toString(0.015625), "0");
    EXPECT_EQ(testUnit.toString(100.5), "100 ¹⁄₂");
    EXPECT_EQ(testUnit.toString(0.02), "0");
    EXPECT_EQ(testUnit.toString(0.026), "0.026");
    EXPECT_EQ(testUnit.toString(0.021), "0");
    EXPECT_EQ(testUnit.toString(-0.0), "0");
    EXPECT_EQ(testUnit.toString(12.0), "12");
    EXPECT_EQ(testUnit.toString(1.0E-4), "0");
    EXPECT_EQ(testUnit.toString(0.3125), "0.312");
    EXPECT_EQ(testUnit.toString(0.9375), "0.938");
    EXPECT_EQ(testUnit.toString(1.9375), "1.938");
    EXPECT_EQ(testUnit.toString(2.0625), "2.062");
    EXPECT_EQ(testUnit.toString(0.4999), "¹⁄₂");
    EXPECT_EQ(testUnit.toString(0.505), "¹⁄₂");
    EXPECT_EQ(testUnit.toString(1234.567), "1234.567");
    EXPECT_EQ(testUnit.toString(1234.5), "1234 ¹⁄₂");
    EXPECT_EQ(testUnit.toString(999999.75), "999999 ³⁄₄");
    EXPECT_EQ(testUnit.toString(1000000.5), "1000000 ¹⁄₂");
    EXPECT_EQ(testUnit.toString(0.7), "0.7");
    EXPECT_EQ(testUnit.toString(-0.7), "-0.7");
    EXPECT_EQ(testUnit.toString(5.03), "5.03");
    EXPECT_EQ(testUnit.toString(5.02), "5");
    EXPECT_EQ(testUnit.toString(5.015), "5");
    EXPECT_EQ(testUnit.toString(0.0125), "0");
    EXPECT_EQ(testUnit.toString(0.0126), "0");

    EXPECT_EQ(testUnitApprox.toString(0.0625), "¹⁄₁₆");
    EXPECT_EQ(testUnitApprox.toString(0.125), "¹⁄₈");
    EXPECT_EQ(testUnitApprox.toString(0.1875), "³⁄₁₆");
    EXPECT_EQ(testUnitApprox.toString(0.3125), "⁵⁄₁₆");
    EXPECT_EQ(testUnitApprox.toString(0.9375), "¹⁵⁄₁₆");
    EXPECT_EQ(testUnitApprox.toString(1.9375), "1 ¹⁵⁄₁₆");
    EXPECT_EQ(testUnitApprox.toString(2.0625), "2 ¹⁄₁₆");
    EXPECT_EQ(testUnitApprox.toString(0.7), "¹¹⁄₁₆");
    EXPECT_EQ(testUnitApprox.toString(-0.7), "-¹¹⁄₁₆");
    EXPECT_EQ(testUnitApprox.toString(1234.567), "1234 ⁹⁄₁₆");
    EXPECT_EQ(testUnitApprox.toString(0.026), "0.026");
    EXPECT_EQ(testUnitApprox.toString(0.021), "0.021");
    EXPECT_EQ(testUnitApprox.toString(0.02), "0");
    EXPECT_EQ(testUnitApprox.toString(5.03), "5.03");
    EXPECT_EQ(testUnitApprox.toString(5.0125), "5");
}

TEST(FractionalUnit, NaNAndInfinityFallThroughAsInOpenRocket)
{
    // Math.signum(NaN) is NaN, the numerator becomes 0 and the integer part prints as "NaN".
    EXPECT_EQ(testUnit.toString(kNaN), "NaN 0⁄₄");
    EXPECT_EQ(testUnitApprox.toString(kNaN), "NaN 0⁄₁₆");
    EXPECT_EQ(testUnit.toString(kInf), "NaN 0⁄₄");
    EXPECT_EQ(testUnit.toStringUnit(kNaN), "N/A");
    EXPECT_TRUE(std::isnan(testUnit.round(kNaN)));
    EXPECT_TRUE(std::isnan(testUnit.round(kInf)));
    EXPECT_TRUE(std::isnan(testUnit.getNextValue(kNaN)));
    EXPECT_TRUE(std::isnan(testUnit.getPreviousValue(kInf)));
}

TEST(FractionalUnit, RoundNextAndPreviousSweep)
{
    EXPECT_DOUBLE_EQ(testUnit.round(-1.2), -1.25);
    EXPECT_DOUBLE_EQ(testUnit.round(-0.1), 0.0);
    EXPECT_DOUBLE_EQ(testUnit.round(0.7), 0.75);
    EXPECT_DOUBLE_EQ(testUnit.round(-0.7), -0.75);
    EXPECT_DOUBLE_EQ(testUnit.round(0.9375), 1.0);
    EXPECT_DOUBLE_EQ(testUnit.round(1234.567), 1234.5);
    EXPECT_DOUBLE_EQ(testUnit.round(-0.0), 0.0);
    EXPECT_DOUBLE_EQ(testUnit.round(5.03), 5.0);

    EXPECT_DOUBLE_EQ(testUnit.getNextValue(-0.024), 0.5);
    EXPECT_DOUBLE_EQ(testUnit.getPreviousValue(-0.024), -0.5);
    EXPECT_DOUBLE_EQ(testUnit.getNextValue(0.024), 0.5);
    EXPECT_DOUBLE_EQ(testUnit.getPreviousValue(0.024), -0.5);
    EXPECT_DOUBLE_EQ(testUnit.getNextValue(0.026), 0.5);
    EXPECT_DOUBLE_EQ(testUnit.getPreviousValue(0.026), 0.0);
    EXPECT_DOUBLE_EQ(testUnit.getNextValue(2.75), 3.0);
    EXPECT_DOUBLE_EQ(testUnit.getPreviousValue(2.75), 2.5);
    EXPECT_DOUBLE_EQ(testUnit.getNextValue(0.9375), 1.0);
    EXPECT_DOUBLE_EQ(testUnit.getPreviousValue(0.9375), 0.5);
    EXPECT_DOUBLE_EQ(testUnit.getNextValue(2.0625), 2.5);
    EXPECT_DOUBLE_EQ(testUnit.getPreviousValue(2.0625), 2.0);
    EXPECT_DOUBLE_EQ(testUnit.getNextValue(1234.567), 1235.0);
    EXPECT_DOUBLE_EQ(testUnit.getPreviousValue(1234.567), 1234.5);
    EXPECT_DOUBLE_EQ(testUnit.getNextValue(999999.75), 1000000.0);
    EXPECT_DOUBLE_EQ(testUnit.getPreviousValue(1000000.5), 1000000.0);
    EXPECT_DOUBLE_EQ(testUnit.getNextValue(5.02), 5.5);
    EXPECT_DOUBLE_EQ(testUnit.getPreviousValue(5.02), 4.5);
    EXPECT_DOUBLE_EQ(testUnit.getPreviousValue(5.03), 5.0);
}

TEST(FractionalUnit, TicksStepByHalvings)
{
    const std::vector<Tick> ticks = testUnit.getTicks(0, 1, 1.0 / 32.0, 0.5);
    ASSERT_EQ(ticks.size(), 33U);
    for (std::size_t i = 0; i < ticks.size(); i++)
    {
        EXPECT_DOUBLE_EQ(ticks[i].value, static_cast<double>(i) / 32.0) << i;
        EXPECT_DOUBLE_EQ(ticks[i].unitValue, static_cast<double>(i) / 32.0) << i;
        // mod3 is 16, mod4 32, mod2 becomes 5 because it clashed with mod3 (both 16).
        EXPECT_EQ(ticks[i].major, i % 16 == 0) << i;
        EXPECT_EQ(ticks[i].notable, i % 32 == 0 || (i % 16 != 0 && i % 5 == 0)) << i;
    }

    // (0, 3, 0.3, 1.2): steps of 0.5, only the origin major.
    const std::vector<Tick> coarse = testUnit.getTicks(0, 3, 0.3, 1.2);
    ASSERT_EQ(coarse.size(), 7U);
    for (std::size_t i = 0; i < coarse.size(); i++)
    {
        EXPECT_DOUBLE_EQ(coarse[i].value, 0.5 * static_cast<double>(i)) << i;
        EXPECT_EQ(coarse[i].major, i == 0) << i;
        EXPECT_EQ(coarse[i].notable, i == 0) << i;
    }

    // (-0.6, 0.6, 0.125, 0.25): major every 0.5, notable at 0.
    const std::vector<Tick> around = testUnit.getTicks(-0.6, 0.6, 0.125, 0.25);
    ASSERT_EQ(around.size(), 9U);
    const bool expectedMajor[]   = {true, false, false, false, true, false, false, false, true};
    const bool expectedNotable[] = {false, false, false, false, true, false, false, false, false};
    for (std::size_t i = 0; i < around.size(); i++)
    {
        EXPECT_DOUBLE_EQ(around[i].value, -0.5 + (0.125 * static_cast<double>(i))) << i;
        EXPECT_EQ(around[i].major, expectedMajor[i]) << i;
        EXPECT_EQ(around[i].notable, expectedNotable[i]) << i;
    }

    // In inches: the SI values are pos * minstep * 0.0254 as Java computes them.
    const std::vector<Tick> inches = inchUnit.getTicks(0, 0.0254 * 2, 0.0254 / 16, 0.0254 / 2);
    ASSERT_EQ(inches.size(), 33U);
    EXPECT_DOUBLE_EQ(inches[3].value, 0.004762499999999999);
    EXPECT_DOUBLE_EQ(inches[3].unitValue, 0.1875);
    EXPECT_DOUBLE_EQ(inches[8].value, 0.0127);
    EXPECT_TRUE(inches[8].major);
    EXPECT_FALSE(inches[8].notable);
    EXPECT_DOUBLE_EQ(inches[16].value, 0.0254);
    EXPECT_TRUE(inches[16].major);
    EXPECT_TRUE(inches[16].notable);
    EXPECT_DOUBLE_EQ(inches[24].value, 0.038099999999999995);
    EXPECT_FALSE(inches[5].major);
    EXPECT_FALSE(inches[5].notable);

    const std::vector<Tick> partial = inchUnit.getTicks(0.01, 0.05, 0.005, 0.02);
    ASSERT_EQ(partial.size(), 6U);
    EXPECT_DOUBLE_EQ(partial[0].unitValue, 0.5);
    EXPECT_DOUBLE_EQ(partial[5].unitValue, 1.75);
    EXPECT_TRUE(partial[2].major);
    EXPECT_FALSE(partial[2].notable);

    EXPECT_THROW(static_cast<void>(testUnit.getTicks(0, 1, 0, 1)), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(testUnit.getTicks(0, 1, 0.5, 0.25)), std::invalid_argument);
}

TEST(FractionalUnit, CloneAndEquality)
{
    const std::unique_ptr<Unit> copy = inchUnit.clone();
    ASSERT_NE(dynamic_cast<const FractionalUnit*>(copy.get()), nullptr);
    EXPECT_EQ(copy->toStringUnit(0.0254), "1 in");
    EXPECT_TRUE(copy->equals(inchUnit));
    EXPECT_FALSE(testUnit.equals(inchUnit));
    EXPECT_TRUE(testUnit.equals(testUnitApprox));  // same class, multiplier and name
}

}  // namespace
