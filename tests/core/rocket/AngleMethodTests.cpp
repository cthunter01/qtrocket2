#include "QtRocket/rocket/position/AngleMethod.h"

#include <numbers>
#include <optional>

#include <gtest/gtest.h>

#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"
#include "rocket/TestComponent.h"

namespace
{

using QtRocket::AngleMethod;
using QtRocket::BugError;
using QtRocket::Test::TestComponent;

constexpr double kPi = std::numbers::pi;

TEST(AngleMethod, RelativeAddsTheParentsAngle)
{
    TestComponent parent;
    parent.setAngleOffset(0.5);
    EXPECT_DOUBLE_EQ(getAngle(AngleMethod::RELATIVE, &parent, nullptr, 0.25), 0.75);
    EXPECT_EQ(getAngle(AngleMethod::RELATIVE, &parent, nullptr, 0.25), 0.5 + 0.25);
}

TEST(AngleMethod, FixedIsAlwaysZero)
{
    TestComponent parent;
    parent.setAngleOffset(0.5);
    EXPECT_EQ(getAngle(AngleMethod::FIXED, &parent, nullptr, 1.25), 0.0);
    EXPECT_EQ(getAngle(AngleMethod::FIXED, nullptr, nullptr, 1.25), 0.0);
}

TEST(AngleMethod, MirrorXyReflectsAnglesBelowPi)
{
    TestComponent parent;
    // 0.5 + 0.25 = 0.75 < pi: pi - 0.75.
    parent.setAngleOffset(0.5);
    EXPECT_EQ(getAngle(AngleMethod::MIRROR_XY, &parent, nullptr, 0.25),
              -(QtRocket::MathUtil::reduce2Pi(0.75) - kPi));
    EXPECT_NEAR(getAngle(AngleMethod::MIRROR_XY, &parent, nullptr, 0.25), kPi - 0.75, 1e-15);
    // 4 is above pi: kept.
    parent.setAngleOffset(0.0);
    EXPECT_DOUBLE_EQ(getAngle(AngleMethod::MIRROR_XY, &parent, nullptr, 4.0), 4.0);
    // Reduced to [0, 2 pi) first: -1 is 2 pi - 1, above pi.
    EXPECT_NEAR(getAngle(AngleMethod::MIRROR_XY, &parent, nullptr, -1.0), (2 * kPi) - 1.0, 1e-12);
}

TEST(AngleMethod, TheParentIsRequiredWhereJavaDereferencesIt)
{
    EXPECT_THROW(static_cast<void>(getAngle(AngleMethod::RELATIVE, nullptr, nullptr, 0.0)),
                 BugError);
    EXPECT_THROW(static_cast<void>(getAngle(AngleMethod::MIRROR_XY, nullptr, nullptr, 0.0)),
                 BugError);
}

TEST(AngleMethod, ClampToZeroAndChoices)
{
    EXPECT_TRUE(clampToZero(AngleMethod::RELATIVE));
    EXPECT_TRUE(clampToZero(AngleMethod::FIXED));
    EXPECT_FALSE(clampToZero(AngleMethod::MIRROR_XY));
    ASSERT_EQ(QtRocket::kAngleMethodChoices.size(), 1U);
    EXPECT_EQ(QtRocket::kAngleMethodChoices[0], AngleMethod::RELATIVE);
}

TEST(AngleMethod, NamesAndOrkSpelling)
{
    EXPECT_EQ(angleMethodName(AngleMethod::MIRROR_XY), "MIRROR_XY");
    EXPECT_EQ(orkName(AngleMethod::RELATIVE), "relative");
    EXPECT_EQ(orkName(AngleMethod::FIXED), "fixed");
    EXPECT_EQ(orkName(AngleMethod::MIRROR_XY), "mirror_xy");
    EXPECT_EQ(QtRocket::angleMethodFromOrkName("relative"), AngleMethod::RELATIVE);
    EXPECT_EQ(QtRocket::angleMethodFromOrkName("fixed"), AngleMethod::FIXED);
    // OpenRocket's findEnum compares against "mirrorxy", so the saved spelling does not load.
    EXPECT_EQ(QtRocket::angleMethodFromOrkName("mirror_xy"), std::nullopt);
    EXPECT_EQ(QtRocket::angleMethodFromOrkName("mirrorxy"), AngleMethod::MIRROR_XY);
    EXPECT_EQ(displayKey(AngleMethod::FIXED), "RocketComponent.Position.Method.Angle.FIXED");
    EXPECT_EQ(displayName(AngleMethod::MIRROR_XY), "Mirror relative to the rocket's x-y plane");
}

}  // namespace
