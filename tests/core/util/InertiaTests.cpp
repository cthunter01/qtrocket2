#include "QtRocket/util/Inertia.h"

#include <cmath>
#include <limits>

#include <gtest/gtest.h>

namespace
{

namespace Inertia = QtRocket::Inertia;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

TEST(Inertia, FilledCylinderRotationalIsHalfTheRadiusSquared)
{
    static_assert(Inertia::filledCylinderRotational(2.0) == 2.0);
    EXPECT_DOUBLE_EQ(Inertia::filledCylinderRotational(0.0), 0.0);
    EXPECT_DOUBLE_EQ(Inertia::filledCylinderRotational(1.0), 0.5);
    EXPECT_DOUBLE_EQ(Inertia::filledCylinderRotational(0.012), 0.000072);
}

TEST(Inertia, FilledCylinderLongitudinalIsThreeRadiusSquaredPlusLengthSquaredOverTwelve)
{
    static_assert(Inertia::filledCylinderLongitudinal(1.0, 2.0) == 7.0 / 12.0);
    EXPECT_DOUBLE_EQ(Inertia::filledCylinderLongitudinal(0.0, 0.0), 0.0);
    EXPECT_DOUBLE_EQ(Inertia::filledCylinderLongitudinal(0.0, 6.0), 3.0);
    EXPECT_DOUBLE_EQ(Inertia::filledCylinderLongitudinal(2.0, 0.0), 1.0);
    // A 24 mm x 70 mm motor, as ThrustCurveMotor computes it from diameter / 2 and length.
    EXPECT_DOUBLE_EQ(Inertia::filledCylinderLongitudinal(0.024 / 2, 0.07),
                     ((3 * 0.012 * 0.012) + (0.07 * 0.07)) / 12);
}

TEST(Inertia, ShiftAddsTheDistanceSquared)
{
    static_assert(Inertia::shift(1.5, 2.0) == 5.5);
    EXPECT_DOUBLE_EQ(Inertia::shift(0.0, 0.0), 0.0);
    EXPECT_DOUBLE_EQ(Inertia::shift(3.0, 0.0), 3.0);
    EXPECT_DOUBLE_EQ(Inertia::shift(3.0, -0.5), 3.25);
    EXPECT_DOUBLE_EQ(Inertia::shift(3.0, 0.5), Inertia::shift(3.0, -0.5));
}

TEST(Inertia, NanPropagates)
{
    EXPECT_TRUE(std::isnan(Inertia::filledCylinderRotational(kNaN)));
    EXPECT_TRUE(std::isnan(Inertia::filledCylinderLongitudinal(kNaN, 1.0)));
    EXPECT_TRUE(std::isnan(Inertia::filledCylinderLongitudinal(1.0, kNaN)));
    EXPECT_TRUE(std::isnan(Inertia::shift(kNaN, 1.0)));
    EXPECT_TRUE(std::isnan(Inertia::shift(1.0, kNaN)));
}

}  // namespace
