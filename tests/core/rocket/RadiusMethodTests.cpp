#include "QtRocket/rocket/position/RadiusMethod.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>

#include <gtest/gtest.h>

#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/Transition.h"
#include "rocket/TestComponent.h"

namespace
{

using QtRocket::BodyTube;
using QtRocket::ComponentKind;
using QtRocket::InnerTube;
using QtRocket::PodSet;
using QtRocket::RadiusMethod;
using QtRocket::Transition;
using QtRocket::Test::TestComponent;

/// A body tube of radius 0.05 m, an inner tube of radius 0.02 m and a pod set whose bounding
/// radius is 0.01 m (the radius of the body tube it holds).
class RadiusMethodTest : public ::testing::Test
{
protected:
    RadiusMethodTest()
    {
        m_innerTube.setOuterRadius(0.02);
        m_pod.addChild(std::make_unique<BodyTube>(0.1, 0.01));
    }

    BodyTube  m_bodyTube{0.3, 0.05};
    InnerTube m_innerTube;  // a Coaxial with an outer radius, but not a body tube
    PodSet    m_pod;
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

TEST_F(RadiusMethodTest, OnlyABodyTubeGivesTheParentRadius)
{
    // Java: parentComponent instanceof BodyTube. A component that only reports the kind BODY_TUBE
    // (a TestComponent, a Coaxial with an outer radius) is not one, and neither is another
    // symmetric component.
    TestComponent lookalike{ComponentKind::BODY_TUBE};
    lookalike.setOuterRadius(0.05);
    EXPECT_DOUBLE_EQ(getRadius(RadiusMethod::RELATIVE, &lookalike, &m_pod, 0.1), 0.11);
    EXPECT_DOUBLE_EQ(getRadius(RadiusMethod::SURFACE, &lookalike, &m_pod, 0.3), 0.01);
    EXPECT_DOUBLE_EQ(getAsOffset(RadiusMethod::RELATIVE, &lookalike, &m_pod, 0.11), 0.1);

    Transition transition;
    transition.setForeRadius(0.05);
    transition.setAftRadius(0.05);
    EXPECT_DOUBLE_EQ(getRadius(RadiusMethod::RELATIVE, &transition, &m_pod, 0.1), 0.11);
    EXPECT_DOUBLE_EQ(getRadius(RadiusMethod::SURFACE, &transition, &m_pod, 0.3), 0.01);
}

TEST_F(RadiusMethodTest, AnAutomaticBodyTubeRadiusIsReadThroughItsGetter)
{
    // BodyTube.getOuterRadius() resolves an automatic radius; without a neighbour to take it
    // from, that is the default radius.
    m_bodyTube.setOuterRadiusAutomatic(true);
    const double automatic = m_bodyTube.getOuterRadius();
    EXPECT_EQ(getRadius(RadiusMethod::SURFACE, &m_bodyTube, nullptr, 0.0), automatic);
    EXPECT_EQ(getAsOffset(RadiusMethod::RELATIVE, &m_bodyTube, nullptr, 0.2), 0.2 - automatic);
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
