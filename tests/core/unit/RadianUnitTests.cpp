#include "QtRocket/unit/RadianUnit.h"

#include <cmath>
#include <limits>
#include <numbers>

#include <gtest/gtest.h>

namespace
{

using QtRocket::RadianUnit;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

// ---- QtRocket additions ----

TEST(RadianUnit, FormatsWithExactlyOneDecimal)
{
    const RadianUnit radian;
    constexpr double kPi = std::numbers::pi;
    EXPECT_TRUE(radian.hasSpace());
    EXPECT_EQ(radian.getUnit(), "rad");
    EXPECT_EQ(radian.getMultiplier(), 1.0);

    EXPECT_EQ(radian.toString(0.0), "0.0");
    EXPECT_EQ(radian.toStringUnit(0.0), "0.0 rad");
    EXPECT_EQ(radian.toString(kPi / 4), "0.8");
    EXPECT_EQ(radian.toString(kPi / 2), "1.6");
    EXPECT_EQ(radian.toString(kPi), "3.1");
    EXPECT_EQ(radian.toString(-6.981317007977319E-4), "-0.0");
    EXPECT_EQ(radian.toStringUnit(-6.981317007977319E-4), "-0.0 rad");
    EXPECT_EQ(radian.toString(kNaN), "NaN");
    EXPECT_EQ(radian.toStringUnit(kNaN), "N/A");
    EXPECT_EQ(radian.toString(174532.92519943297), "174532.9");
    EXPECT_EQ(radian.toString(kInf), "∞");
    EXPECT_EQ(radian.toStringUnit(kInf), "∞ rad");
    EXPECT_EQ(radian.toString(-0.0), "-0.0");
    EXPECT_EQ(radian.toString(1.0), "1.0");
    EXPECT_EQ(radian.toString(2.5), "2.5");
    EXPECT_EQ(radian.toString(0.05), "0.1");
    EXPECT_EQ(radian.toString(0.15), "0.1");
    EXPECT_EQ(radian.toString(0.25), "0.2");
    EXPECT_EQ(radian.toString(-0.04), "-0.0");
    EXPECT_EQ(radian.toString(123.456), "123.5");

    EXPECT_DOUBLE_EQ(radian.round(0.7853981633974483), 0.8);
    EXPECT_DOUBLE_EQ(radian.round(1.5707963267948966), 1.6);
    EXPECT_DOUBLE_EQ(radian.round(0.05), 0.0);
    EXPECT_DOUBLE_EQ(radian.round(0.15), 0.2);
    EXPECT_DOUBLE_EQ(radian.round(0.25), 0.2);
    EXPECT_DOUBLE_EQ(radian.round(123.456), 123.5);
    EXPECT_TRUE(std::isnan(radian.round(kNaN)));
}

TEST(RadianUnit, LargeMagnitudesTiesAndZerosMatchOpenRocket)
{
    const RadianUnit radian;
    EXPECT_EQ(radian.toString(0x1p69), "590295810358705650000.0");
    EXPECT_EQ(radian.toString(1e23), "99999999999999990000000.0");
    EXPECT_EQ(radian.toString(8.41e21), "8409999999999999000000.0");
    EXPECT_EQ(radian.toString(0.35), "0.3");
    EXPECT_EQ(radian.toString(0.45), "0.5");
    EXPECT_EQ(radian.toString(-0.05), "-0.1");
    EXPECT_EQ(radian.toString(-1e-300), "-0.0");
    EXPECT_EQ(radian.toString(9.95), "9.9");
    EXPECT_EQ(radian.toString(-99.95), "-100.0");
}

}  // namespace
