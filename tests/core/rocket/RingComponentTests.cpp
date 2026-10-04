// RingComponent, through its concrete classes: the length, the radial position, the bounds, the
// mass and CG of the instances, the automatic outer radius and copies; and the golden geometry of
// the ring components of OpenRocket's test rockets (the RingComponentGolden suite, labelled
// "golden"). OpenRocket has no JUnit test of the class of its own. InternalComponent,
// StructuralComponent, ThicknessRingComponent and RadiusRingComponent have their own files.

#include "QtRocket/rocket/RingComponent.h"

#include <cmath>
#include <cstddef>
#include <format>
#include <memory>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/material/Material.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/Bulkhead.h"
#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/ClusterConfiguration.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/EngineBlock.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/LineInstanceable.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/Coordinate.h"
#include "goldens/GoldenData.h"
#include "rocket/InternalTestSupport.h"
#include "rocket/TestComponent.h"
#include "rocket/TestRockets.h"

namespace
{

using nlohmann::json;
using QtRocket::AxialMethod;
using QtRocket::BoundingBox;
using QtRocket::Bulkhead;
using QtRocket::CenteringRing;
using QtRocket::ComponentChangeEvent;
using QtRocket::ComponentKind;
using QtRocket::ComponentPreset;
using QtRocket::ComponentPresetType;
using QtRocket::Coordinate;
using QtRocket::EngineBlock;
using QtRocket::InnerTube;
using QtRocket::Manufacturer;
using QtRocket::RocketComponent;
using QtRocket::Transition;
using QtRocket::TransitionShape;
using QtRocket::TypedPropertyMap;
using QtRocket::Test::GoldenCheck;
using QtRocket::Test::goldenGeometryComponentOrFail;
using QtRocket::Test::makeFactoryPreset;
using QtRocket::Test::noGoldenMismatches;
using QtRocket::Test::OneStage;
using QtRocket::Test::TestBeta;
using QtRocket::Test::TestClusterPods;
using QtRocket::Test::TestComponent;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::TestFalcon9Heavy;
using QtRocket::Test::TestIsoHaisu;

constexpr double kEpsilon = 1e-12;

/// A body whose inner radius changes linearly along its length, from @p fore at x = 0 to @p aft
/// at its end, so that the automatic outer radius has to take the smaller of the two ends: a
/// conical transition with a wall of 2 mm.
[[nodiscard]] std::unique_ptr<Transition> taperedBody(double length, double fore, double aft)
{
    constexpr double kWall = 0.002;
    auto             body  = std::make_unique<Transition>();
    body->setShapeType(TransitionShape::CONICAL);
    body->setLength(length);
    body->setForeRadius(fore + kWall);
    body->setAftRadius(aft + kWall);
    body->setThickness(kWall);
    return body;
}

class RingComponentEvents : public QtRocket::Test::RingEventsFixture
{ };

// ======================================================================= RingComponent

TEST(RingComponent, SetLengthClampsAndClearsThePreset)
{
    TypedPropertyMap props;
    props.put(ComponentPreset::kType, ComponentPresetType::ENGINE_BLOCK);
    props.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("manufacturer"));
    props.put(ComponentPreset::kPartNo, "partno");
    props.put(ComponentPreset::kLength, 0.01);
    props.put(ComponentPreset::kOuterDiameter, 0.02);
    props.put(ComponentPreset::kInnerDiameter, 0.01);
    const ComponentPreset preset = makeFactoryPreset(props);

    EngineBlock block;
    block.loadPreset(&preset);
    block.setLength(0.01);  // unchanged: the preset stays
    EXPECT_EQ(block.getPresetComponent(), &preset);
    block.setLength(-1.0);
    EXPECT_EQ(block.getLength(), 0.0);
    EXPECT_EQ(block.getPresetComponent(), nullptr);
}

TEST(RingComponent, RadialPositionAndDirectionGiveTheShift)
{
    CenteringRing ring;
    EXPECT_EQ(ring.getRadialPosition(), 0.0);
    EXPECT_EQ(ring.getRadialDirection(), 0.0);

    ring.setRadialDirection(3 * std::numbers::pi / 2);  // reduced to -pi/2
    EXPECT_NEAR(ring.getRadialDirection(), -std::numbers::pi / 2, kEpsilon);
    EXPECT_EQ(ring.getRadialShiftY(), 0.0);
    EXPECT_EQ(ring.getRadialShiftZ(), 0.0);

    ring.setRadialPosition(0.01);
    EXPECT_EQ(ring.getRadialPosition(), 0.01);
    EXPECT_NEAR(ring.getRadialShiftY(), 0.0, kEpsilon);
    EXPECT_NEAR(ring.getRadialShiftZ(), -0.01, kEpsilon);

    ring.setRadialPosition(-1.0);
    EXPECT_EQ(ring.getRadialPosition(), 0.0);
    EXPECT_EQ(ring.getRadialShiftZ(), 0.0);

    ring.setRadialShift(0.003, 0.004);
    EXPECT_NEAR(ring.getRadialPosition(), 0.005, kEpsilon);
    EXPECT_NEAR(ring.getRadialDirection(), std::atan2(0.004, 0.003), kEpsilon);
    EXPECT_NEAR(ring.getRadialShiftY(), 0.003, kEpsilon);
    EXPECT_NEAR(ring.getRadialShiftZ(), 0.004, kEpsilon);
}

TEST_F(RingComponentEvents, RadialSettersFireMassChangesOnChangeOnly)
{
    auto& ring = m_body->addChild(std::make_unique<CenteringRing>());
    m_types.clear();
    ring.setRadialPosition(0.0);   // unchanged
    ring.setRadialDirection(0.0);  // unchanged
    EXPECT_TRUE(m_types.empty());
    ring.setRadialPosition(0.002);
    ring.setRadialDirection(1.0);
    ring.setRadialShift(ring.getRadialShiftY(), ring.getRadialShiftZ());  // fires always
    EXPECT_EQ(m_types, std::vector<int>(3, ComponentChangeEvent::kMassChange));
}

TEST(RingComponent, BoundsAndInstanceBoundingBox)
{
    EngineBlock block;
    block.setOuterRadius(0.02);
    block.setLength(0.05);
    const BoundingBox box = block.getInstanceBoundingBox();
    EXPECT_TRUE(box.min().exactlyEquals(Coordinate{0, -0.02, -0.02}));
    EXPECT_TRUE(box.max().exactlyEquals(Coordinate{0.05, 0.02, 0.02}));

    const std::vector<Coordinate> bounds = block.getComponentBounds();
    ASSERT_EQ(bounds.size(), 8U);
    EXPECT_TRUE(bounds[0].exactlyEquals(Coordinate{0, -0.02, -0.02}));
    EXPECT_TRUE(bounds[2].exactlyEquals(Coordinate{0, 0.02, 0.02}));
    EXPECT_TRUE(bounds[4].exactlyEquals(Coordinate{0.05, -0.02, -0.02}));
    EXPECT_TRUE(bounds[6].exactlyEquals(Coordinate{0.05, 0.02, 0.02}));
}

TEST(RingComponent, MassCgAndInertiaOfOneRing)
{
    EngineBlock block;
    block.setOuterRadius(0.02);
    block.setThickness(0.005);
    block.setLength(0.01);
    const double density = block.getMaterial().getDensity();
    const double mass    = std::numbers::pi * ((0.02 * 0.02) - (0.015 * 0.015)) * 0.01 * density;
    EXPECT_NEAR(block.getComponentMass(), mass, mass * kEpsilon);
    const Coordinate cg = block.getComponentCG();
    EXPECT_EQ(cg.x, 0.005);
    EXPECT_EQ(cg.y, 0.0);
    EXPECT_EQ(cg.z, 0.0);
    EXPECT_NEAR(cg.weight, mass, mass * kEpsilon);
    EXPECT_NEAR(block.getLongitudinalUnitInertia(),
                ((3 * ((0.015 * 0.015) + (0.02 * 0.02))) + (0.01 * 0.01)) / 12, kEpsilon);
    EXPECT_NEAR(block.getRotationalUnitInertia(), ((0.015 * 0.015) + (0.02 * 0.02)) / 2, kEpsilon);
}

TEST(RingComponent, MassAndCgOfSeveralInstances)
{
    CenteringRing ring;
    ring.setOuterRadius(0.02);
    ring.setInnerRadius(0.01);
    ring.setLength(0.002);
    ring.setInstanceCount(3);
    ring.setInstanceSeparation(0.01);
    const double one = std::numbers::pi * ((0.02 * 0.02) - (0.01 * 0.01)) * 0.002 *
                       ring.getMaterial().getDensity();
    EXPECT_NEAR(ring.getComponentMass(), 3 * one, one * kEpsilon);
    const Coordinate cg = ring.getComponentCG();
    EXPECT_NEAR(cg.x, 0.011, kEpsilon);  // the middle instance's center
    EXPECT_EQ(cg.y, 0.0);
    EXPECT_EQ(cg.z, 0.0);
    EXPECT_NEAR(cg.weight, 3 * one, one * kEpsilon);
}

TEST(RingComponent, SingleInstanceCgIgnoresTheRadialShift)
{
    // As in Java: a single instance's CG is on the axis whatever its radial position.
    InnerTube tube;
    tube.setRadialPosition(0.01);
    EXPECT_EQ(tube.getInstanceOffsets().front().y, 0.01);
    EXPECT_EQ(tube.getComponentCG().y, 0.0);
}

// ================================================================ automatic outer radius

TEST(RingComponent, AutomaticOuterRadiusTakesTheSmallerParentRadius)
{
    // In a rocket, so that every move reaches componentChanged() (which clears the cached
    // locations toRelative() reads).
    OneStage    rocket;
    Transition& body = rocket.stage->addChild(taperedBody(1.0, 0.01, 0.02));
    rocket.rocket.enableEvents();
    auto& block = body.addChild(std::make_unique<EngineBlock>());
    block.setLength(0.1);
    block.setAxialMethod(AxialMethod::TOP);
    block.setAxialOffset(0.25);
    ASSERT_TRUE(block.isOuterRadiusAutomatic());
    EXPECT_NEAR(block.getOuterRadius(), 0.0125, kEpsilon);  // at x = 0.25 (0.35 is wider)

    // A ring sticking out of either end of the parent is clamped to it.
    block.setAxialOffset(-0.05);
    EXPECT_NEAR(block.getOuterRadius(), 0.01, kEpsilon);
    block.setAxialOffset(0.95);
    EXPECT_NEAR(block.getOuterRadius(), 0.0195, kEpsilon);

    // The computed radius is stored, so it stays when the radius becomes manual.
    block.setOuterRadiusAutomatic(false);
    EXPECT_NEAR(block.getOuterRadius(), 0.0195, kEpsilon);
}

TEST(RingComponent, AutomaticOuterRadiusNeedsARadialParent)
{
    // A mass component is not a RadialParent, in Java as here: the stored radius (0 for a new
    // block) stays.
    auto        parent = TestComponent::make(0.5, ComponentKind::MASS_COMPONENT);
    const auto& block  = parent->addChild(std::make_unique<EngineBlock>());
    EXPECT_TRUE(block.isOuterRadiusAutomatic());
    EXPECT_EQ(block.getOuterRadius(), 0.0);
    const EngineBlock detached;
    EXPECT_EQ(detached.getOuterRadius(), 0.0);
}

/// The automatic outer radius of a new @p Ring in a TestComponent parent of @p kind with an
/// inner radius of 0.04 m.
template <class Ring>
[[nodiscard]] double automaticOuterRadiusIn(ComponentKind kind)
{
    auto parent = TestComponent::make(0.5, kind);
    parent->setOuterRadius(0.05);
    parent->setInnerRadius(0.04);
    const auto& ring = parent->addChild(std::make_unique<Ring>());
    EXPECT_TRUE(ring.isOuterRadiusAutomatic());
    return ring.getOuterRadius();
}

TEST(RingComponent, TheParentClassDecidesNotItsKind)
{
    // Java: getParent() instanceof RadialParent. A parent that only reports the kind of a body
    // tube, nose cone, transition, inner tube or coupler (a TestComponent, a Coaxial with radii
    // of its own) is not a RadialParent: the stored radius (0 for a new ring) stays.
    for (const ComponentKind kind :
         {ComponentKind::BODY_TUBE, ComponentKind::NOSE_CONE, ComponentKind::TRANSITION,
          ComponentKind::INNER_TUBE, ComponentKind::TUBE_COUPLER})
    {
        SCOPED_TRACE(QtRocket::componentKindName(kind));
        EXPECT_EQ(automaticOuterRadiusIn<EngineBlock>(kind), 0.0);  // ThicknessRingComponent
        EXPECT_EQ(automaticOuterRadiusIn<Bulkhead>(kind), 0.0);     // RadiusRingComponent
    }
}

TEST(RingComponent, RadiusRingAutomaticOuterRadius)
{
    OneStage    rocket;
    Transition& body = rocket.stage->addChild(taperedBody(1.0, 0.03, 0.02));
    rocket.rocket.enableEvents();
    auto& bulk = body.addChild(std::make_unique<Bulkhead>());
    bulk.setAxialMethod(AxialMethod::TOP);
    bulk.setAxialOffset(0.5);
    EXPECT_NEAR(bulk.getOuterRadius(), 0.03 - (0.01 * 0.502), kEpsilon);  // the aft end
    EXPECT_EQ(bulk.getInnerRadius(), 0.0);
    EXPECT_NEAR(bulk.getThickness(), bulk.getOuterRadius(), kEpsilon);
}

TEST_F(RingComponentEvents, SetOuterRadiusAutomaticFiresOnChange)
{
    auto& block = m_body->addChild(std::make_unique<EngineBlock>());
    EXPECT_EQ(block.getOuterRadius(), 0.024);  // the body's inner radius, stored
    m_types.clear();
    block.setOuterRadiusAutomatic(true);  // already
    EXPECT_TRUE(m_types.empty());
    block.setOuterRadiusAutomatic(false);
    EXPECT_EQ(m_types, std::vector<int>{ComponentChangeEvent::kMassChange});
    EXPECT_EQ(block.getOuterRadius(), 0.024);  // stored by the automatic read
}

// ======================================================================= copies

TEST(RingComponent, CopiesKeepTheRingFields)
{
    CenteringRing ring;
    ring.setOuterRadius(0.03);
    ring.setInnerRadius(0.01);
    ring.setRadialPosition(0.002);
    ring.setRadialDirection(0.5);
    ring.setInstanceCount(2);
    ring.setInstanceSeparation(0.02);
    ring.setLength(0.004);

    const std::unique_ptr<CenteringRing> copy =
        QtRocket::componentCast<CenteringRing>(ring.copyWithNewIds());
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->getOuterRadius(), 0.03);
    EXPECT_EQ(copy->getInnerRadius(), 0.01);
    EXPECT_EQ(copy->getRadialPosition(), 0.002);
    EXPECT_EQ(copy->getRadialDirection(), 0.5);
    EXPECT_EQ(copy->getRadialShiftY(), ring.getRadialShiftY());
    EXPECT_EQ(copy->getRadialShiftZ(), ring.getRadialShiftZ());
    EXPECT_EQ(copy->getInstanceCount(), 2);
    EXPECT_EQ(copy->getInstanceSeparation(), 0.02);
    EXPECT_EQ(copy->getLength(), 0.004);
    EXPECT_EQ(copy->getMaterial(), ring.getMaterial());
    EXPECT_FALSE(copy->getId() == ring.getId());
}

// ===================================================================== goldens

/// The mismatches between @p ring and its golden entry: the ring details (radii, thickness,
/// material, radial position, instance box) first, as the harness settles the automatic radii
/// that way, then the geometry, placement and mass properties every component has. The
/// placement is relative to the parent, which the tests build with OpenRocket's dimensions.
[[nodiscard]] std::vector<std::string> goldenMismatches(const QtRocket::RingComponent& ring,
                                                        const json&                    golden)
{
    GoldenCheck check{golden};
    check.number("/details/outerRadius", ring.getOuterRadius());
    check.number("/details/innerRadius", ring.getInnerRadius());
    check.number("/details/thickness", ring.getThickness());
    check.string("/details/material/name", ring.getMaterial().getName());
    check.number("/details/material/density", ring.getMaterial().getDensity());
    check.number("/details/radialPosition", ring.getRadialPosition());
    check.number("/details/radialDirection", ring.getRadialDirection());
    check.string("/details/radiusMethod", QtRocket::radiusMethodName(ring.getRadiusMethod()));
    check.number("/details/radiusOffset", ring.getRadiusOffset());
    check.number("/details/angleOffset", ring.getAngleOffset());
    check.boolean("/details/motorMount", ring.isMotorMount());
    if (const auto* line = dynamic_cast<const QtRocket::LineInstanceable*>(&ring))
    {
        check.number("/details/instanceSeparation", line->getInstanceSeparation());
    }
    const BoundingBox box = ring.getInstanceBoundingBox();
    check.coordinate("/details/instanceBoundingBox/min", box.min());
    check.coordinate("/details/instanceBoundingBox/max", box.max());

    check.string("/name", ring.getName());
    check.number("/length", ring.getLength());
    check.string("/axialMethod", QtRocket::axialMethodName(ring.getAxialMethod()));
    check.number("/axialOffset", ring.getAxialOffset());
    check.coordinate("/position", ring.getPosition());
    check.number("/componentMass", ring.getComponentMass());
    check.coordinate("/componentCG", ring.getComponentCG());
    check.number("/longitudinalUnitInertia", ring.getLongitudinalUnitInertia());
    check.number("/rotationalUnitInertia", ring.getRotationalUnitInertia());
    check.number("/mass", ring.getMass());
    check.number("/sectionMass", ring.getSectionMass());
    check.coordinate("/cg", ring.getCG());
    check.number("/longitudinalInertia", ring.getLongitudinalInertia());
    check.number("/rotationalInertia", ring.getRotationalInertia());
    check.boolean("/isAerodynamic", ring.isAerodynamic());
    check.boolean("/isMassive", ring.isMassive());
    check.coordinates("/componentBounds", ring.getComponentBounds());
    check.integer("/instanceCount", ring.getInstanceCount());
    check.coordinates("/instanceOffsets", ring.getInstanceOffsets());
    check.numbers("/instanceAngles", ring.getInstanceAngles());
    check.coordinates("/instanceLocations", ring.getInstanceLocations());
    return check.failures();
}

/// The mismatches between the absolute placement of @p component (its locations and angles in
/// the rocket, its stage) and its golden one; only a test that builds the whole stage can
/// compare it.
[[nodiscard]] std::vector<std::string> goldenLocationMismatches(const RocketComponent& component,
                                                                const json&            golden)
{
    GoldenCheck check{golden};
    check.coordinates("/componentLocations", component.getComponentLocations());
    check.coordinates("/componentAngles", component.getComponentAngles());
    check.integer("/stageNumber", component.getStageNumber());
    return check.failures();
}

/// The mismatches between @p tube's motor mount and cluster settings and its golden entry.
[[nodiscard]] std::vector<std::string> goldenMountMismatches(const InnerTube& tube,
                                                             const json&      golden)
{
    GoldenCheck check{golden};
    check.boolean("/details/motorMount", tube.isMotorMount());
    check.number("/details/motorOverhang", tube.getMotorOverhang());
    check.string("/details/clusterConfiguration", tube.getClusterConfiguration().getXmlName());
    check.number("/details/clusterScale", tube.getClusterScale());
    check.number("/details/clusterRotation", tube.getClusterRotation());
    return check.failures();
}

// The ring components of TestRockets.makeEstesAlphaIII(), makeBeta() and makeFalcon9Heavy() are
// those of the shared fixtures (TestRockets.h), which build the whole rockets.

TEST(RingComponentGolden, EstesAlphaIIIMotorMountTube)
{
    const TestEstesAlphaIII alpha;
    const json& golden = goldenGeometryComponentOrFail("testrocket-estes-alpha-iii", "/0/1/2");
    EXPECT_EQ(goldenMismatches(*alpha.inner, golden), noGoldenMismatches());
    EXPECT_EQ(goldenLocationMismatches(*alpha.inner, golden), noGoldenMismatches());
    EXPECT_EQ(goldenMountMismatches(*alpha.inner, golden), noGoldenMismatches());
}

TEST(RingComponentGolden, EstesAlphaIIIEngineBlock)
{
    const TestEstesAlphaIII alpha;
    const json& golden = goldenGeometryComponentOrFail("testrocket-estes-alpha-iii", "/0/1/2/0");
    EXPECT_EQ(goldenMismatches(*alpha.block, golden), noGoldenMismatches());
    EXPECT_EQ(goldenLocationMismatches(*alpha.block, golden), noGoldenMismatches());
}

TEST(RingComponentGolden, EstesAlphaIIICenteringRings)
{
    // Automatic outer radius from the body tube, automatic inner radius from the inner tube.
    const TestEstesAlphaIII alpha;
    const json& golden = goldenGeometryComponentOrFail("testrocket-estes-alpha-iii", "/0/1/4");
    EXPECT_EQ(goldenMismatches(*alpha.rings, golden), noGoldenMismatches());
    EXPECT_EQ(goldenLocationMismatches(*alpha.rings, golden), noGoldenMismatches());
}

TEST(RingComponentGolden, BetaCoupler)
{
    // TestRockets.makeBeta(): the coupler of the booster body (BodyTube(0.06, 0.012, 0.0003)),
    // whose thickness the constructor-time clamp keeps at 0 (so it weighs nothing).
    const TestBeta beta;
    const json&    golden = goldenGeometryComponentOrFail("testrocket-beta", "/1/0/0");
    EXPECT_EQ(goldenMismatches(*beta.coupler, golden), noGoldenMismatches());
    EXPECT_EQ(goldenLocationMismatches(*beta.coupler, golden), noGoldenMismatches());
}

TEST(RingComponentGolden, BetaBoosterMotorMount)
{
    // TestRockets.makeBeta(): the booster's motor mount, positioned BOTTOM 0.005 m.
    const TestBeta beta;
    const json&    golden = goldenGeometryComponentOrFail("testrocket-beta", "/1/0/2");
    EXPECT_EQ(goldenMismatches(*beta.boosterMmt, golden), noGoldenMismatches());
    EXPECT_EQ(goldenLocationMismatches(*beta.boosterMmt, golden), noGoldenMismatches());
    EXPECT_EQ(goldenMountMismatches(*beta.boosterMmt, golden), noGoldenMismatches());
}

TEST(RingComponentGolden, IsoHaisuCouplerAndBulkhead)
{
    // TestRockets.makeIsoHaisu(): the coupler and the bulkhead of the second body tube
    // (BodyTube(0.605, 0.07, 0.005)), both with an automatic outer radius.
    const TestIsoHaisu iso;
    const json&        coupler = goldenGeometryComponentOrFail("testrocket-iso-haisu", "/0/2/0");
    EXPECT_EQ(goldenMismatches(*iso.tube2Coupler, coupler), noGoldenMismatches());
    const json& bulkhead = goldenGeometryComponentOrFail("testrocket-iso-haisu", "/0/2/2");
    EXPECT_EQ(goldenMismatches(*iso.bulk, bulkhead), noGoldenMismatches());
}

TEST(RingComponentGolden, IsoHaisuInnerTubeAndCenteringRings)
{
    // The inner tube and the three centering rings of the third body tube.
    const TestIsoHaisu iso;
    const json&        inner = goldenGeometryComponentOrFail("testrocket-iso-haisu", "/0/3/0");
    EXPECT_EQ(goldenMismatches(*iso.inner, inner), noGoldenMismatches());
    for (std::size_t i = 0; i < iso.centers.size(); ++i)
    {
        const json& center =
            goldenGeometryComponentOrFail("testrocket-iso-haisu", std::format("/0/3/{}", i + 1));
        EXPECT_EQ(goldenMismatches(*iso.centers.at(i), center), noGoldenMismatches());
    }
}

TEST(RingComponentGolden, Falcon9HeavyBoosterMotorTubes)
{
    // A 4-ring cluster in each of the two booster bodies (BodyTube(0.8, 0.0385, 0.001)): eight
    // locations in the rocket.
    const TestFalcon9Heavy f9h;
    const InnerTube&       tubes = *f9h.boosterMotorTubes;

    const json& golden = goldenGeometryComponentOrFail("testrocket-falcon-9-heavy", "/1/0/0/1/0");
    EXPECT_EQ(goldenMismatches(tubes, golden), noGoldenMismatches());
    EXPECT_EQ(goldenLocationMismatches(tubes, golden), noGoldenMismatches());
    EXPECT_EQ(goldenMountMismatches(tubes, golden), noGoldenMismatches());
    EXPECT_EQ(tubes.getComponentLocations().size(), 8U);
}

TEST(RingComponentGolden, ClusterPodsInnerTubes)
{
    // TestRockets.makeClusterPods(): default inner tubes in default body tubes, a double
    // cluster in the sustainer and a 4-row cluster in each side booster.
    const TestClusterPods cluster;

    const json& sustainer = goldenGeometryComponentOrFail("testrocket-cluster-pods", "/0/0/0");
    EXPECT_EQ(goldenMismatches(*cluster.sustainerMount, sustainer), noGoldenMismatches());
    EXPECT_EQ(goldenLocationMismatches(*cluster.sustainerMount, sustainer), noGoldenMismatches());
    EXPECT_EQ(goldenMountMismatches(*cluster.sustainerMount, sustainer), noGoldenMismatches());

    const json& boosters = goldenGeometryComponentOrFail("testrocket-cluster-pods", "/0/0/1/0/0");
    EXPECT_EQ(goldenMismatches(*cluster.sideMounts, boosters), noGoldenMismatches());
    EXPECT_EQ(goldenLocationMismatches(*cluster.sideMounts, boosters), noGoldenMismatches());
    EXPECT_EQ(goldenMountMismatches(*cluster.sideMounts, boosters), noGoldenMismatches());
}

}  // namespace
