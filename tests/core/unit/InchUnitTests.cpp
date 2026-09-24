#include "QtRocket/unit/InchUnit.h"

#include <limits>

#include <gtest/gtest.h>

namespace
{

using QtRocket::InchUnit;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// ---- QtRocket additions ----

TEST(InchUnit, KeepsThreeDecimals)
{
    const InchUnit inch(0.0254, "in", 1);
    EXPECT_EQ(inch.getPrecision(), 1.0);
    EXPECT_EQ(inch.toString(25.125 * 25.4 / 1000), "25.125");
    EXPECT_EQ(inch.toStringUnit(25.125 * 25.4 / 1000), "25.125 in");
    EXPECT_EQ(inch.toString(0.0254 * 0.0005), "0");
    EXPECT_EQ(inch.toString(0.0254 * 1.0005), "1");
    EXPECT_EQ(inch.toString(0.0254 * 2.00049), "2");
    EXPECT_EQ(inch.toString(0.0254 * 99.9995), "100");
    EXPECT_EQ(inch.toString(0.0254 * 100), "100");
    EXPECT_EQ(inch.toString(0.0254 * 0.0004), "0");
    EXPECT_EQ(inch.toString(1e-9), "0");
    EXPECT_EQ(inch.toString(0.0254 * 1.0004), "1");
    EXPECT_EQ(inch.toString(0.0254 * 1.00051), "1.001");
    EXPECT_EQ(inch.toString(0.0254 * 0.00051), "0.001");
    EXPECT_EQ(inch.toString(0.0254 * 0.0006), "0.001");
    EXPECT_EQ(inch.toString(0.0254 * 12.3456), "12.346");
    EXPECT_EQ(inch.toString(0.0254 * 12.3455), "12.346");
    EXPECT_EQ(inch.toString(0.0254 * 12.3445), "12.344");
    EXPECT_EQ(inch.toString(0.0254 * 1e7), "1.00E7");
    EXPECT_EQ(inch.toString(kNaN), "N/A");
    EXPECT_EQ(inch.toString(0.0254 * 1e-3), "0.001");
    EXPECT_EQ(inch.toString(0.0254 * 0.9995), "1");
    EXPECT_EQ(inch.toString(0.0254 * 0.99949), "0.999");
    EXPECT_EQ(inch.toStringUnit(0.0254 * 0.99949), "0.999 in");
}

TEST(InchUnit, StepsByPrecisionAndRoundsAsGeneralUnit)
{
    const InchUnit inch(0.0254, "in", 1);
    const InchUnit inchDefault(0.0254, "in");
    EXPECT_DOUBLE_EQ(inch.getNextValue(2.5), 3.5);
    EXPECT_DOUBLE_EQ(inch.getPreviousValue(2.5), 1.5);
    EXPECT_DOUBLE_EQ(inchDefault.getNextValue(2.5), 3.5);
    EXPECT_DOUBLE_EQ(InchUnit(0.0254, "in", 0.125).getNextValue(2.5), 2.625);
    EXPECT_DOUBLE_EQ(inch.round(2.55), 2.6);
    EXPECT_DOUBLE_EQ(inch.round(12.5), 12.0);
    EXPECT_DOUBLE_EQ(inch.round(125.0), 120.0);
}

TEST(InchUnit, TiesMatchOpenRocket)
{
    const InchUnit inch(0.0254, "in", 1);
    EXPECT_EQ(inch.toString(0.0254 * 2.0005), "2.001");
    EXPECT_EQ(inch.toString(0.0254 * 0.0015), "0.002");
    EXPECT_EQ(inch.toString(-0.0254 * 0.0005), "0");
    EXPECT_EQ(inch.round(2.25), 2.2);
    EXPECT_EQ(inch.round(2.35), 2.4);
    EXPECT_EQ(inch.getPreviousValue(-0.5), -1.5);
}

}  // namespace
