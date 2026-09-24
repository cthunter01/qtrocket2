#include "QtRocket/unit/DegreeUnit.h"

#include <cmath>
#include <limits>
#include <numbers>

#include <gtest/gtest.h>

namespace
{

using QtRocket::DegreeUnit;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

// ---- QtRocket additions ----

TEST(DegreeUnit, FormatsWithOneDecimalAndNoSpace)
{
    const DegreeUnit degree;
    constexpr double kPi = std::numbers::pi;
    EXPECT_FALSE(degree.hasSpace());
    EXPECT_EQ(degree.getUnit(), "°");
    EXPECT_DOUBLE_EQ(degree.getMultiplier(), 0.017453292519943295);

    EXPECT_EQ(degree.toString(0.0), "0");
    EXPECT_EQ(degree.toStringUnit(0.0), "0°");
    EXPECT_EQ(degree.toString(kPi / 4), "45");
    EXPECT_EQ(degree.toStringUnit(kPi / 2), "90°");
    EXPECT_EQ(degree.toString(kPi), "180");
    EXPECT_EQ(degree.toString(-kPi / 180 * 0.04), "-0");
    EXPECT_EQ(degree.toStringUnit(-kPi / 180 * 0.04), "-0°");
    EXPECT_EQ(degree.toString(kPi / 180 * 45.25), "45.2");
    EXPECT_EQ(degree.toString(kPi / 180 * 45.35), "45.4");
    EXPECT_EQ(degree.toString(kNaN), "NaN");
    EXPECT_EQ(degree.toStringUnit(kNaN), "N/A");
    EXPECT_EQ(degree.toString(kPi / 180 * 1e7), "10000000");
    EXPECT_EQ(degree.toString(kInf), "∞");
    EXPECT_EQ(degree.toStringUnit(kInf), "∞°");
    EXPECT_EQ(degree.toString(-0.0), "-0");
    EXPECT_EQ(degree.toString(1.0), "57.3");
    EXPECT_EQ(degree.toString(2.5), "143.2");
    EXPECT_EQ(degree.toString(0.05), "2.9");
    EXPECT_EQ(degree.toString(-0.04), "-2.3");
    EXPECT_EQ(degree.toString(kPi / 180 * 0.05), "0.1");
    EXPECT_EQ(degree.toString(kPi / 180 * 12.34), "12.3");
    EXPECT_EQ(degree.toString(1e-9), "0");
    EXPECT_EQ(degree.toString(123.456), "7073.5");

    EXPECT_DOUBLE_EQ(degree.round(0.7853981633974483), 1.0);
    EXPECT_DOUBLE_EQ(degree.round(2.5), 2.0);
    EXPECT_DOUBLE_EQ(degree.round(-0.04), -0.0);
    EXPECT_DOUBLE_EQ(degree.round(174532.92519943297), 174533.0);
    EXPECT_TRUE(std::isnan(degree.round(kNaN)));
    EXPECT_EQ(degree.round(kInf), kInf);
}

TEST(DegreeUnit, LargeMagnitudesTiesAndZerosMatchOpenRocket)
{
    const DegreeUnit degree;
    EXPECT_EQ(degree.toString(-2.594816859051859e+23), "-14867205463306412000000000");
    EXPECT_EQ(degree.toString(793017819599871232.0), "45436574141739516000");
    EXPECT_EQ(degree.toString(4.7636871895947546e+23), "27293917088431583000000000");
    EXPECT_EQ(degree.toString(-1e-300), "-0");
    EXPECT_EQ(degree.toString(0.0043633231299858239), "0.2");
    EXPECT_EQ(degree.toString(-0.0026179938779914941), "-0.1");
}

}  // namespace
