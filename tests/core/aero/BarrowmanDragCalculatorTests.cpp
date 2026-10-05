#include "QtRocket/aero/BarrowmanDragCalculator.h"

#include <array>
#include <cmath>
#include <limits>

#include <gtest/gtest.h>

namespace
{

using QtRocket::BarrowmanDragCalculator;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

struct PressureCase
{
    double mach;
    double stagnationCD;
    double baseCD;
};

TEST(BarrowmanDragCalculator, StagnationAndBaseCDAreOpenRockets)
{
    // BarrowmanDragCalculator.calculateStagnationCD() and calculateBaseCD() on JDK 17 (the probe
    // EdgeProbe.java); additions, multiplications and divisions only, so the same doubles. Mach
    // 1 belongs to the subsonic branch of both; the first double above it to the supersonic one.
    const std::array<PressureCase, 10> cases{{
        {.mach = 0.0, .stagnationCD = 0.85, .baseCD = 0.12},
        {.mach = 0.5, .stagnationCD = 0.9044531249999999, .baseCD = 0.1525},
        {.mach = 0.9, .stagnationCD = 1.0360671250000002, .baseCD = 0.2253},
        {.mach = 1.0, .stagnationCD = 1.08375, .baseCD = 0.25},
        {.mach         = std::nextafter(1.0, 2.0),
         .stagnationCD = 1.0888500000000003,
         .baseCD       = 0.24999999999999994},
        {.mach = 1.0000001, .stagnationCD = 1.088850054910001, .baseCD = 0.24999997500000248},
        {.mach = 1.5, .stagnationCD = 1.3073722908093282, .baseCD = 0.16666666666666666},
        {.mach = 2.0, .stagnationCD = 1.41178359375, .baseCD = 0.125},
        {.mach = 5.0, .stagnationCD = 1.538387664, .baseCD = 0.05},
        // A negative Mach number is not rejected: the formulas are even in it.
        {.mach = -0.5, .stagnationCD = 0.9044531249999999, .baseCD = 0.1525},
    }};
    for (const PressureCase& c : cases)
    {
        EXPECT_EQ(BarrowmanDragCalculator::calculateStagnationCD(c.mach), c.stagnationCD)
            << "Mach " << c.mach;
        EXPECT_EQ(BarrowmanDragCalculator::calculateBaseCD(c.mach), c.baseCD) << "Mach " << c.mach;
    }
}

TEST(BarrowmanDragCalculator, StagnationAndBaseCDOfNaNAndInfinity)
{
    // A NaN fails `m <= 1` and goes through the supersonic formulas.
    EXPECT_TRUE(std::isnan(BarrowmanDragCalculator::calculateStagnationCD(kNaN)));
    EXPECT_TRUE(std::isnan(BarrowmanDragCalculator::calculateBaseCD(kNaN)));
    // The supersonic limits: 0.85 * 1.84 and 0.
    EXPECT_EQ(BarrowmanDragCalculator::calculateStagnationCD(kInf), 1.564);
    EXPECT_EQ(BarrowmanDragCalculator::calculateBaseCD(kInf), 0.0);
}

TEST(BarrowmanDragCalculator, StagnationCDStepsAtMach1AsOpenRockets)
{
    // The two branches do not meet: 0.85 * 1.275 at Mach 1 and 0.85 * 1.281 just above it.
    EXPECT_GT(BarrowmanDragCalculator::calculateStagnationCD(std::nextafter(1.0, 2.0)),
              BarrowmanDragCalculator::calculateStagnationCD(1.0) + 0.005);
    // The base drag does meet (0.12 + 0.13 = 0.25 / 1).
    EXPECT_NEAR(BarrowmanDragCalculator::calculateBaseCD(std::nextafter(1.0, 2.0)),
                BarrowmanDragCalculator::calculateBaseCD(1.0), 1e-15);
}

}  // namespace
