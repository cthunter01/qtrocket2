#include "QtRocket/unit/FrequencyUnit.h"

#include <cmath>
#include <limits>

#include <gtest/gtest.h>

namespace
{

using QtRocket::FrequencyUnit;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

// ---- QtRocket additions ----

TEST(FrequencyUnit, InvertsThePeriod)
{
    const FrequencyUnit hz(1, "Hz");
    const FrequencyUnit mhz(0.001, "mHz");
    const FrequencyUnit khz(1000, "kHz");

    EXPECT_DOUBLE_EQ(hz.toUnit(0.5), 2.0);
    EXPECT_DOUBLE_EQ(hz.fromUnit(0.5), 2.0);
    EXPECT_EQ(hz.toString(0.5), "2");
    EXPECT_DOUBLE_EQ(mhz.toUnit(0.5), 2000.0);
    EXPECT_EQ(mhz.toString(0.5), "2000");
    EXPECT_DOUBLE_EQ(khz.toUnit(0.5), 0.002);
    EXPECT_EQ(khz.toString(0.5), "0.002");
    EXPECT_DOUBLE_EQ(hz.toUnit(2.0), 0.5);
    EXPECT_EQ(hz.toString(2.0), "0.5");
    EXPECT_DOUBLE_EQ(khz.toUnit(2.0), 5.0E-4);
    EXPECT_EQ(khz.toString(2.0), "0");
    EXPECT_EQ(hz.toUnit(0.0), kInf);
    EXPECT_EQ(hz.toString(0.0), "∞");
    EXPECT_EQ(hz.toUnit(kInf), 0.0);
    EXPECT_EQ(hz.toString(kInf), "0");
    EXPECT_TRUE(std::isnan(hz.toUnit(kNaN)));
    EXPECT_EQ(hz.toString(kNaN), "N/A");
    EXPECT_DOUBLE_EQ(hz.toUnit(0.001), 1000.0);
    EXPECT_EQ(mhz.toString(0.001), "1000000");
    EXPECT_EQ(khz.toString(0.001), "1");
    EXPECT_EQ(hz.toString(1000.0), "0.001");
    EXPECT_DOUBLE_EQ(khz.toUnit(1000.0), 1.0E-6);
    EXPECT_EQ(khz.toString(1000.0), "0");
    EXPECT_DOUBLE_EQ(hz.toUnit(-0.5), -2.0);
    EXPECT_EQ(hz.toString(-0.5), "-2");
    EXPECT_EQ(khz.toString(-0.5), "-0.002");
}

}  // namespace
