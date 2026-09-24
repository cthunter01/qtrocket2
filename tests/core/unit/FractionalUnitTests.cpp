#include "QtRocket/unit/FractionalUnit.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/unit/Tick.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/util/BugError.h"

namespace
{

using QtRocket::BugError;
using QtRocket::FractionalUnit;
using QtRocket::Tick;
using QtRocket::Unit;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

// The expected strings were produced by OpenRocket's FractionalUnit on JDK 17 (Locale.US).

/// Steps @p count times down from @p value with getPreviousValue, each step strictly lower.
void expectStrictlyDecreasing(const FractionalUnit& unit, double value, int count)
{
    for (int i = 0; i < count; i++)
    {
        EXPECT_GT(value, unit.getPreviousValue(value));
        value = unit.getPreviousValue(value);
    }
}

/// Steps down from @p value with getPreviousValue, expecting each of @p expected in turn.
void expectPreviousValues(const FractionalUnit& unit, double value,
                          std::span<const double> expected)
{
    for (const double next : expected)
    {
        value = unit.getPreviousValue(value);
        EXPECT_DOUBLE_EQ(value, next);
    }
}

// ---- Ported from FractionalUnitTest.java ----

TEST(FractionalUnit, Round)
{
    const FractionalUnit testUnit(1, "unit", "unit", 4, 0.5);
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
    const FractionalUnit testUnit(1, "unit", "unit", 4, 0.5);
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
    const FractionalUnit testUnit(1, "unit", "unit", 4, 0.5);
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
    const FractionalUnit testUnit(1, "unit", "unit", 4, 0.5);
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
    const FractionalUnit testUnitApprox(1, "unit", "unit", 16, 0.5, 0.02);
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
    const FractionalUnit inchUnit(0.0254, "in/64", "in", 64, 1.0 / 16.0);
    // Just some random test points.
    EXPECT_EQ(inchUnit.toString(1.0 / 64.0 * 0.0254), "¹⁄₆₄");

    EXPECT_EQ(inchUnit.toString(-5.0 / 64.0 * 0.0254), "-⁵⁄₆₄");

    EXPECT_EQ(inchUnit.toString(9.0 / 2.0 * 0.0254), "4 ¹⁄₂");

    EXPECT_EQ(inchUnit.toString(0.002 * 0.0254), "0.002");

    // default body tube length:
    const double length = 8.0 * 0.025;

    EXPECT_EQ(inchUnit.toString(length), "7 ⁷⁄₈");

    // had problems with round-off in decrement.
    expectStrictlyDecreasing(inchUnit, inchUnit.toUnit(length), 15);
}

// ---- QtRocket additions ----

TEST(FractionalUnit, Properties)
{
    const FractionalUnit testUnit(1, "unit", "unit", 4, 0.5);
    const FractionalUnit testUnitApprox(1, "unit", "unit", 16, 0.5, 0.02);
    const FractionalUnit inchUnit(0.0254, "in/64", "in", 64, 1.0 / 16.0);
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
    const FractionalUnit inchUnit(0.0254, "in/64", "in", 64, 1.0 / 16.0);
    EXPECT_EQ(inchUnit.toStringUnit(1.0 / 64.0 * 0.0254), "¹⁄₆₄ in");
    EXPECT_EQ(inchUnit.toStringUnit(-5.0 / 64.0 * 0.0254), "-⁵⁄₆₄ in");
    EXPECT_EQ(inchUnit.toStringUnit(9.0 / 2.0 * 0.0254), "4 ¹⁄₂ in");
    EXPECT_EQ(inchUnit.toStringUnit(0.002 * 0.0254), "0.002 in");
    EXPECT_EQ(inchUnit.toStringUnit(0.0254), "1 in");
    EXPECT_EQ(inchUnit.toStringUnit(kNaN), "N/A");
}

TEST(FractionalUnit, MoreInchStrings)
{
    const FractionalUnit inchUnit(0.0254, "in/64", "in", 64, 1.0 / 16.0);
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
    constexpr std::array kExpected{7.8125, 7.75, 7.6875, 7.625, 7.5625, 7.5, 7.4375, 7.375,
                                   7.3125, 7.25, 7.1875, 7.125, 7.0625, 7.0, 6.9375};
    const double         start = inchUnit.toUnit(8.0 * 0.025);
    EXPECT_DOUBLE_EQ(start, 7.874015748031497);
    expectPreviousValues(inchUnit, start, kExpected);
}

TEST(FractionalUnit, ReducedFractionsAndDecimals)
{
    const FractionalUnit testUnit(1, "unit", "unit", 4, 0.5);
    const FractionalUnit testUnitApprox(1, "unit", "unit", 16, 0.5, 0.02);
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
    const FractionalUnit testUnit(1, "unit", "unit", 4, 0.5);
    const FractionalUnit testUnitApprox(1, "unit", "unit", 16, 0.5, 0.02);
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

TEST(FractionalUnit, LargeValuesTiesAndZerosMatchOpenRocket)
{
    const FractionalUnit inch64(0.0254, "in/64", "in", 64, 1.0 / 16.0, 0.5 / 64.0);
    const FractionalUnit quarters(1, "unit", "unit", 4, 0.5);
    EXPECT_EQ(inch64.toString(368502855063204480.0), "14507986419811201000");
    EXPECT_EQ(inch64.toString(-748727788667643520.0), "-29477471994789118000");
    EXPECT_EQ(inch64.toString(5.873443313069121e+23), "23123792571138272000000000");
    EXPECT_EQ(inch64.toString(-0.0), "0");
    EXPECT_EQ(inch64.toString(-1e-9), "0");
    EXPECT_EQ(inch64.toString(0.0254 * ((3.0 / 64) + (0.5 / 64) + 1e-9)), "¹⁄₁₆");
    EXPECT_EQ(quarters.toString(0x1p69), "590295810358705650000");
    EXPECT_EQ(quarters.toString(1e23), "99999999999999990000000");
    EXPECT_EQ(quarters.toString(2.0625), "2.062");
    EXPECT_EQ(quarters.toString(1.1875), "1.188");
    EXPECT_EQ(quarters.toString(0.1125), "0.113");
    EXPECT_EQ(quarters.toString(-0.3375), "-0.338");
    EXPECT_EQ(quarters.toString(-0.0), "0");
    EXPECT_EQ(quarters.round(0.125), 0.0);
    EXPECT_EQ(quarters.round(0.375), 0.5);
    EXPECT_EQ(quarters.getNextValue(0.75 - 0.025), 1.0);
    EXPECT_EQ(quarters.getPreviousValue(0.25 + 0.025), 0.0);
}

TEST(FractionalUnit, RoundNextAndPreviousSweep)
{
    const FractionalUnit testUnit(1, "unit", "unit", 4, 0.5);
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

/// testUnit.getTicks(0, 1, 1/32, 0.5): mod3 is 16, mod4 32, and mod2 becomes 5 because it clashed
/// with mod3 (both 16).
void expectThirtySecondTick(const Tick& tick, std::size_t i)
{
    EXPECT_DOUBLE_EQ(tick.value, static_cast<double>(i) / 32.0) << i;
    EXPECT_DOUBLE_EQ(tick.unitValue, static_cast<double>(i) / 32.0) << i;
    EXPECT_EQ(tick.major, i % 16 == 0) << i;
    EXPECT_EQ(tick.notable, i % 32 == 0 || (i % 16 != 0 && i % 5 == 0)) << i;
}

void expectThirtySecondsTicks(const std::vector<Tick>& ticks)
{
    ASSERT_EQ(ticks.size(), 33U);
    for (std::size_t i = 0; i < ticks.size(); i++)
    {
        expectThirtySecondTick(ticks[i], i);
    }
}

/// testUnit.getTicks(0, 3, 0.3, 1.2): steps of 0.5, only the origin major and notable.
void expectHalvesTicks(const std::vector<Tick>& ticks)
{
    ASSERT_EQ(ticks.size(), 7U);
    for (std::size_t i = 0; i < ticks.size(); i++)
    {
        EXPECT_DOUBLE_EQ(ticks[i].value, 0.5 * static_cast<double>(i)) << i;
        EXPECT_EQ(ticks[i].major, i == 0) << i;
        EXPECT_EQ(ticks[i].notable, i == 0) << i;
    }
}

/// testUnit.getTicks(-0.6, 0.6, 0.125, 0.25): major every 0.5, notable at 0.
void expectEighthsAroundZeroTicks(const std::vector<Tick>& ticks)
{
    constexpr std::array kMajor{true, false, false, false, true, false, false, false, true};
    constexpr std::array kNotable{false, false, false, false, true, false, false, false, false};
    ASSERT_EQ(ticks.size(), kMajor.size());
    for (std::size_t i = 0; i < ticks.size(); i++)
    {
        EXPECT_DOUBLE_EQ(ticks[i].value, -0.5 + (0.125 * static_cast<double>(i))) << i;
        EXPECT_EQ(ticks[i].major, kMajor.at(i)) << i;
        EXPECT_EQ(ticks[i].notable, kNotable.at(i)) << i;
    }
}

TEST(FractionalUnit, TicksStepByHalvings)
{
    const FractionalUnit testUnit(1, "unit", "unit", 4, 0.5);
    expectThirtySecondsTicks(testUnit.getTicks(0, 1, 1.0 / 32.0, 0.5));
    expectHalvesTicks(testUnit.getTicks(0, 3, 0.3, 1.2));
    expectEighthsAroundZeroTicks(testUnit.getTicks(-0.6, 0.6, 0.125, 0.25));
    EXPECT_THROW(static_cast<void>(testUnit.getTicks(0, 1, 0, 1)), BugError);
    EXPECT_THROW(static_cast<void>(testUnit.getTicks(0, 1, 0.5, 0.25)), BugError);
}

TEST(FractionalUnit, InchTicksAreSiValuesOfTheInchPositions)
{
    // The SI values are pos * minstep * 0.0254 as Java computes them.
    const FractionalUnit    inchUnit(0.0254, "in/64", "in", 64, 1.0 / 16.0);
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
}

TEST(FractionalUnit, CloneAndEquality)
{
    const FractionalUnit        testUnit(1, "unit", "unit", 4, 0.5);
    const FractionalUnit        testUnitApprox(1, "unit", "unit", 16, 0.5, 0.02);
    const FractionalUnit        inchUnit(0.0254, "in/64", "in", 64, 1.0 / 16.0);
    const std::unique_ptr<Unit> copy = inchUnit.clone();
    ASSERT_NE(dynamic_cast<const FractionalUnit*>(copy.get()), nullptr);
    EXPECT_EQ(copy->toStringUnit(0.0254), "1 in");
    EXPECT_TRUE(copy->equals(inchUnit));
    EXPECT_FALSE(testUnit.equals(inchUnit));
    EXPECT_TRUE(testUnit.equals(testUnitApprox));  // same class, multiplier and name
}

}  // namespace
