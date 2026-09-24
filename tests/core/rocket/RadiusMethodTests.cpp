#include "QtRocket/rocket/position/RadiusMethod.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include <gtest/gtest.h>

#include "QtRocket/rocket/ComponentKind.h"
#include "rocket/TestComponent.h"

namespace
{

using QtRocket::ComponentKind;
using QtRocket::RadiusMethod;
using QtRocket::Test::TestComponent;

class RadiusMethodTest : public ::testing::Test
{
protected:
    RadiusMethodTest()
    {
        m_bodyTube.setOuterRadius(0.05);
        m_innerTube.setOuterRadius(0.02);
        m_pod.setBoundingRadius(0.01);
    }

    TestComponent m_bodyTube{ComponentKind::BODY_TUBE};
    TestComponent m_innerTube{ComponentKind::INNER_TUBE};  // a Coaxial, but not a body tube
    TestComponent m_pod{ComponentKind::POD_SET};
};

TEST_F(RadiusMethodTest, CoaxialIsAlwaysOnTheAxis)
{
    EXPECT_EQ(getRadius(RadiusMethod::COAXIAL, &m_bodyTube, &m_pod, 0.3), 0.0);
    EXPECT_EQ(getAsOffset(RadiusMethod::COAXIAL, &m_bodyTube, &m_pod, 0.3), 0.0);
}

TEST_F(RadiusMethodTest, FreeIsTheOffsetItself)
{
    EXPECT_EQ(getRadius(RadiusMethod::FREE, &m_bodyTube, &m_pod, 0.3), 0.3);
    EXPECT_EQ(getAsOffset(RadiusMethod::FREE, &m_bodyTube, &m_pod, 0.3), 0.3);
}

TEST_F(RadiusMethodTest, RelativeAddsTheTubeAndBoundingRadii)
{
    EXPECT_DOUBLE_EQ(getRadius(RadiusMethod::RELATIVE, &m_bodyTube, &m_pod, 0.1), 0.16);
    EXPECT_DOUBLE_EQ(getAsOffset(RadiusMethod::RELATIVE, &m_bodyTube, &m_pod, 0.16), 0.1);
    // Only a body tube parent counts; a non-RadiusPositionable (here a zero bounding radius)
    // adds nothing.
    EXPECT_DOUBLE_EQ(getRadius(RadiusMethod::RELATIVE, &m_innerTube, &m_pod, 0.1), 0.11);
    EXPECT_DOUBLE_EQ(getRadius(RadiusMethod::RELATIVE, nullptr, &m_pod, 0.1), 0.11);
    EXPECT_DOUBLE_EQ(getRadius(RadiusMethod::RELATIVE, &m_bodyTube, nullptr, 0.1), 0.15);
    // Exactly Java's sums, in order.
    EXPECT_EQ(getRadius(RadiusMethod::RELATIVE, &m_bodyTube, &m_pod, 0.1), (0.1 + 0.05) + 0.01);
    EXPECT_EQ(getAsOffset(RadiusMethod::RELATIVE, &m_bodyTube, &m_pod, 0.16), (0.16 - 0.05) - 0.01);
}

TEST_F(RadiusMethodTest, SurfaceIgnoresTheOffset)
{
    EXPECT_DOUBLE_EQ(getRadius(RadiusMethod::SURFACE, &m_bodyTube, &m_pod, 0.3), 0.06);
    EXPECT_DOUBLE_EQ(getRadius(RadiusMethod::SURFACE, nullptr, nullptr, 0.3), 0.0);
    EXPECT_EQ(getAsOffset(RadiusMethod::SURFACE, &m_bodyTube, &m_pod, 0.06), 0.0);
}

TEST_F(RadiusMethodTest, RelativeKeepsANegativeZeroOffset)
{
    // Java adds a radius only when the parent is a body tube: -0.0 stays -0.0 otherwise.
    const double radius = getRadius(RadiusMethod::RELATIVE, &m_innerTube, nullptr, -0.0);
    EXPECT_TRUE(std::signbit(radius));
}

TEST(RadiusMethod, ClampToZero)
{
    EXPECT_TRUE(clampToZero(RadiusMethod::COAXIAL));
    EXPECT_FALSE(clampToZero(RadiusMethod::FREE));
    EXPECT_FALSE(clampToZero(RadiusMethod::RELATIVE));
    EXPECT_TRUE(clampToZero(RadiusMethod::SURFACE));
}

TEST(RadiusMethod, ChoicesAreFreeAndRelative)
{
    ASSERT_EQ(QtRocket::kRadiusMethodChoices.size(), 2U);
    EXPECT_EQ(QtRocket::kRadiusMethodChoices[0], RadiusMethod::FREE);
    EXPECT_EQ(QtRocket::kRadiusMethodChoices[1], RadiusMethod::RELATIVE);
}

TEST(RadiusMethod, NamesAndOrkSpelling)
{
    EXPECT_EQ(radiusMethodName(RadiusMethod::SURFACE), "SURFACE");
    EXPECT_EQ(orkName(RadiusMethod::COAXIAL), "coaxial");
    EXPECT_EQ(orkName(RadiusMethod::RELATIVE), "relative");
    EXPECT_TRUE(std::ranges::all_of(QtRocket::kAllRadiusMethods, [](RadiusMethod method) {
        return QtRocket::radiusMethodFromOrkName(orkName(method)) == method;
    }));
    EXPECT_EQ(QtRocket::radiusMethodFromOrkName("nowhere"), std::nullopt);
}

TEST(RadiusMethod, DisplayNames)
{
    EXPECT_EQ(displayKey(RadiusMethod::FREE), "RocketComponent.Position.Method.Radius.FREE");
    EXPECT_EQ(displayName(RadiusMethod::SURFACE),
              "Surface of the parent component (without offset)");
}

}  // namespace
