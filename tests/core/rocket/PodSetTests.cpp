#include "QtRocket/rocket/PodSet.h"

#include <cmath>
#include <memory>
#include <numbers>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/InstanceContext.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "rocket/TestBodyComponent.h"

namespace
{

using QtRocket::AngleMethod;
using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::BugError;
using QtRocket::ComponentChangeEvent;
using QtRocket::ComponentChangeSignal;
using QtRocket::ComponentKind;
using QtRocket::Coordinate;
using QtRocket::InstanceContext;
using QtRocket::PodSet;
using QtRocket::RadiusMethod;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Test::TestBodyComponent;

constexpr double kEpsilon = 1e-9;

/// A rocket with a stage holding a body tube (0.5 m long, radius 0.05 m) that carries a pod set
/// with one pod body (0.2 m, radius 0.02 m); events enabled.
class PodSetTest : public ::testing::Test
{
protected:
    PodSetTest()
    {
        m_stage   = &m_rocket.addChild(std::make_unique<AxialStage>());
        m_body    = &m_stage->addChild(TestBodyComponent::make(0.5, 0.05));
        m_pods    = &m_body->addChild(std::make_unique<PodSet>());
        m_podBody = &m_pods->addChild(TestBodyComponent::make(0.2, 0.02));
        m_rocket.enableEvents();
        m_connection = m_rocket.addComponentChangeListener(
            [this](const ComponentChangeEvent& e) { m_lastType = e.getType(); });
    }

    Rocket                                  m_rocket;
    AxialStage*                             m_stage{nullptr};
    TestBodyComponent*                      m_body{nullptr};
    PodSet*                                 m_pods{nullptr};
    TestBodyComponent*                      m_podBody{nullptr};
    int                                     m_lastType{0};
    ComponentChangeSignal::ScopedConnection m_connection;
};

TEST(PodSet, Defaults)
{
    const PodSet pods;
    EXPECT_EQ(pods.kind(), ComponentKind::POD_SET);
    EXPECT_EQ(pods.getName(), "Pod Set");
    EXPECT_EQ(pods.getInstanceCount(), 2);
    EXPECT_EQ(pods.getAxialMethod(), AxialMethod::BOTTOM);
    EXPECT_FALSE(pods.isAfter());
    EXPECT_EQ(pods.getRadiusMethod(), RadiusMethod::RELATIVE);
    EXPECT_EQ(pods.getRadiusOffset(), 0.0);
    EXPECT_EQ(pods.getAngleMethod(), AngleMethod::RELATIVE);
    EXPECT_EQ(pods.getAngleOffset(), 0.0);
    EXPECT_DOUBLE_EQ(pods.getInstanceAngleIncrement(), std::numbers::pi);
    EXPECT_EQ(pods.getPatternName(), "2-ring");
    EXPECT_TRUE(pods.allowsChildren());
    EXPECT_FALSE(pods.isAxisymmetric()) << "two instances";
    EXPECT_TRUE(pods.getComponentBounds().empty());
    EXPECT_EQ(pods.getRelativeToStage(), -1);
}

TEST(PodSet, AcceptsBodyComponentsOnly)
{
    const PodSet pods;
    EXPECT_TRUE(pods.isCompatible(ComponentKind::BODY_TUBE));
    EXPECT_TRUE(pods.isCompatible(ComponentKind::NOSE_CONE));
    EXPECT_TRUE(pods.isCompatible(ComponentKind::TRANSITION));
    EXPECT_FALSE(pods.isCompatible(ComponentKind::POD_SET));
    EXPECT_FALSE(pods.isCompatible(ComponentKind::TRAPEZOID_FIN_SET));
    EXPECT_FALSE(pods.isCompatible(ComponentKind::INNER_TUBE));
}

TEST_F(PodSetTest, InstanceCountAndAngles)
{
    m_pods->setInstanceCount(0);
    EXPECT_EQ(m_pods->getInstanceCount(), 2) << "at least one instance";
    m_pods->setInstanceCount(3);
    EXPECT_EQ(m_lastType, ComponentChangeEvent::kBothChange);
    EXPECT_EQ(m_pods->getInstanceCount(), 3);
    EXPECT_DOUBLE_EQ(m_pods->getInstanceAngleIncrement(), 2 * std::numbers::pi / 3);
    EXPECT_EQ(m_pods->getPatternName(), "3-ring");

    // As given, not reduced (ParallelStage reduces it).
    m_pods->setAngleOffset(4.0);
    EXPECT_EQ(m_pods->getAngleOffset(), 4.0);
    const std::vector<double> angles = m_pods->getInstanceAngles();
    ASSERT_EQ(angles.size(), 3U);
    EXPECT_DOUBLE_EQ(angles[0], 4.0);
    EXPECT_DOUBLE_EQ(angles[1], 4.0 + (2 * std::numbers::pi / 3));
    EXPECT_DOUBLE_EQ(angles[2], 4.0 + (4 * std::numbers::pi / 3));

    // The angle method cannot be changed (Java's setter is empty).
    m_lastType = 0;
    m_pods->setAngleMethod(AngleMethod::FIXED);
    EXPECT_EQ(m_pods->getAngleMethod(), AngleMethod::RELATIVE);
    EXPECT_EQ(m_lastType, 0) << "and fires nothing";
}

TEST_F(PodSetTest, RelativeRadiusPutsThePodsOnTheSurface)
{
    // RELATIVE with offset 0: the parent tube's radius plus the pods' own bounding radius.
    EXPECT_DOUBLE_EQ(m_pods->getBoundingRadius(), 0.02);
    const std::vector<Coordinate> offsets = m_pods->getInstanceOffsets();
    ASSERT_EQ(offsets.size(), 2U);
    EXPECT_NEAR(offsets[0].y, 0.07, kEpsilon);
    EXPECT_NEAR(offsets[0].z, 0.0, kEpsilon);
    EXPECT_NEAR(offsets[1].y, -0.07, kEpsilon);
    EXPECT_NEAR(offsets[1].z, 0.0, kEpsilon);

    m_pods->setRadiusOffset(0.01);
    EXPECT_NEAR(m_pods->getInstanceOffsets()[0].y, 0.08, kEpsilon);
    EXPECT_EQ(m_pods->getInstanceLocations().size(), 2U);
}

TEST_F(PodSetTest, SetRadiusClampsByThePreviousMethod)
{
    m_pods->setRadius(RadiusMethod::FREE, 0.3);
    EXPECT_EQ(m_pods->getRadiusMethod(), RadiusMethod::FREE);
    EXPECT_DOUBLE_EQ(m_pods->getRadiusOffset(), 0.3);

    m_pods->setRadius(RadiusMethod::SURFACE, 0.3);
    EXPECT_EQ(m_pods->getRadiusOffset(), 0.0);

    // From SURFACE (which clamps) the requested radius is dropped, as in Java's PodSet.
    m_pods->setRadius(RadiusMethod::FREE, 0.3);
    EXPECT_EQ(m_pods->getRadiusMethod(), RadiusMethod::FREE);
    EXPECT_EQ(m_pods->getRadiusOffset(), 0.0);

    // setRadiusOffset() under a clamping method stores zero.
    m_pods->setRadius(RadiusMethod::SURFACE, 0.0);
    m_pods->setRadiusOffset(0.5);
    EXPECT_EQ(m_pods->getRadiusOffset(), 0.0);
}

TEST_F(PodSetTest, SetRadiusMethodKeepsTheRadius)
{
    m_pods->setRadius(RadiusMethod::FREE, 0.3);
    m_pods->setRadiusMethod(RadiusMethod::RELATIVE);
    EXPECT_EQ(m_pods->getRadiusMethod(), RadiusMethod::RELATIVE);
    EXPECT_NEAR(m_pods->getRadiusOffset(), 0.3 - 0.05 - 0.02, kEpsilon);
    EXPECT_NEAR(m_pods->getInstanceOffsets()[0].y, 0.3, kEpsilon);

    m_lastType = 0;
    m_pods->setRadiusMethod(RadiusMethod::RELATIVE);
    EXPECT_EQ(m_lastType, 0) << "the same method changes nothing";
}

TEST_F(PodSetTest, AxialPositioning)
{
    // BOTTOM with offset 0: the pods' aft ends meet the body's.
    EXPECT_NEAR(m_pods->getPosition().x, 0.5 - 0.2, kEpsilon);
    EXPECT_NEAR(m_pods->getComponentLocations().at(0).x, 0.3, kEpsilon);

    m_pods->setAxialMethod(AxialMethod::AFTER);
    EXPECT_EQ(m_pods->getAxialMethod(), AxialMethod::TOP) << "AFTER is refused";
    EXPECT_EQ(m_lastType, ComponentChangeEvent::kNonFunctionalChange);

    m_pods->setAxialOffset(0.1);
    EXPECT_NEAR(m_pods->getPosition().x, 0.1, kEpsilon);
    EXPECT_NEAR(m_pods->getAxialOffset(), 0.1, kEpsilon);
    EXPECT_NEAR(m_pods->getAxialOffset(AxialMethod::BOTTOM), 0.1 + 0.2 - 0.5, kEpsilon);
    EXPECT_NEAR(m_pods->getAxialOffset(AxialMethod::ABSOLUTE), 0.1, kEpsilon);

    // An offset within MathUtil::kEpsilon of zero reads as zero.
    m_pods->setAxialMethod(AxialMethod::BOTTOM);
    m_pods->setAxialOffset(1e-10);
    EXPECT_NEAR(m_pods->getPosition().x, 0.3, kEpsilon);
    EXPECT_EQ(m_pods->getAxialOffset(), 0.0);
    EXPECT_EQ(m_pods->getAxialOffset(AxialMethod::BOTTOM), 0.0);

    PodSet detached;
    EXPECT_THROW(detached.setAxialMethod(AxialMethod::TOP), BugError);
}

TEST_F(PodSetTest, RelativeToStage)
{
    auto&   innerBody = m_podBody->addChild(TestBodyComponent::make(0.1, 0.01));
    PodSet& inner     = innerBody.addChild(std::make_unique<PodSet>());
    EXPECT_EQ(inner.getRelativeToStage(), -1) << "the parent is not a pod set";
    EXPECT_EQ(m_pods->getRelativeToStage(), -1);
    // A pod set is relative to a stage only inside a pod set, which accepts body components only.
    EXPECT_FALSE(m_pods->isCompatible(ComponentKind::POD_SET));
}

TEST_F(PodSetTest, CopiesKeepTheSettings)
{
    m_pods->setInstanceCount(4);
    m_pods->setAngleOffset(0.5);
    m_pods->setRadius(RadiusMethod::FREE, 0.25);
    m_pods->setAxialMethod(AxialMethod::TOP);
    m_pods->setAxialOffset(0.05);

    const std::unique_ptr<RocketComponent> copy = m_pods->copyWithNewIds();
    const auto&                            pods = dynamic_cast<const PodSet&>(*copy);
    EXPECT_EQ(pods.getInstanceCount(), 4);
    EXPECT_DOUBLE_EQ(pods.getInstanceAngleIncrement(), std::numbers::pi / 2);
    EXPECT_EQ(pods.getAngleOffset(), 0.5);
    EXPECT_EQ(pods.getRadiusMethod(), RadiusMethod::FREE);
    EXPECT_EQ(pods.getRadiusOffset(), 0.25);
    EXPECT_EQ(pods.getAxialMethod(), AxialMethod::TOP);
    EXPECT_EQ(pods.getChildCount(), 1U);
    EXPECT_NE(pods.getId(), m_pods->getId());
}

/// Whether @p context is instance 0 of a pod body at x = 0.3 on the ring of radius 0.1 at
/// @p angle.
::testing::AssertionResult isPodBodyAt(const InstanceContext& context, double angle)
{
    if (context.instanceNumber != 0)
    {
        return ::testing::AssertionFailure() << "each pod body is instance 0 of its pod";
    }
    const Coordinate location = context.getLocation();
    const Coordinate expected{0.3, 0.1 * std::cos(angle), 0.1 * std::sin(angle)};
    if (std::abs(location.x - expected.x) > kEpsilon ||
        std::abs(location.y - expected.y) > kEpsilon ||
        std::abs(location.z - expected.z) > kEpsilon)
    {
        return ::testing::AssertionFailure()
               << location.toPreciseString() << " is not " << expected.toPreciseString();
    }
    return ::testing::AssertionSuccess();
}

TEST_F(PodSetTest, PodsAreInstancedInTheConfiguration)
{
    m_pods->setInstanceCount(3);
    m_pods->setRadius(RadiusMethod::FREE, 0.1);
    const auto& instances = m_rocket.getSelectedConfiguration().getActiveInstances();
    ASSERT_EQ(instances.count(*m_pods), 3);
    ASSERT_EQ(instances.count(*m_podBody), 3);
    const std::span<const InstanceContext> bodies = instances.getInstanceContexts(*m_podBody);
    EXPECT_TRUE(isPodBodyAt(bodies[0], 0));
    EXPECT_TRUE(isPodBodyAt(bodies[1], 2 * std::numbers::pi / 3));
    EXPECT_TRUE(isPodBodyAt(bodies[2], 4 * std::numbers::pi / 3));
    // A pod set belongs to its parent's stage.
    EXPECT_EQ(m_pods->getStageNumber(), m_stage->getStageNumber());
    EXPECT_EQ(m_rocket.getStageCount(), 1U);
}

}  // namespace
