#include "QtRocket/rocket/InnerTube.h"

#include <cmath>
#include <cstddef>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/Bulkhead.h"
#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/ClusterConfiguration.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/EngineBlock.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/RadialParent.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetFactory.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Strings.h"
#include "rocket/TestBodyComponent.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::BugError;
using QtRocket::Bulkhead;
using QtRocket::CenteringRing;
using QtRocket::ClusterConfiguration;
using QtRocket::ComponentChangeEvent;
using QtRocket::ComponentChangeSignal;
using QtRocket::ComponentKind;
using QtRocket::ComponentPreset;
using QtRocket::ComponentPresetFactory;
using QtRocket::ComponentPresetType;
using QtRocket::Coordinate;
using QtRocket::EngineBlock;
using QtRocket::FlightConfigurationId;
using QtRocket::InnerTube;
using QtRocket::Manufacturer;
using QtRocket::MassComponent;
using QtRocket::MotorConfiguration;
using QtRocket::MotorMount;
using QtRocket::PodSet;
using QtRocket::RadialParent;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::TypedPropertyMap;
using QtRocket::Test::TestBodyComponent;

constexpr double kEpsilon = 1e-12;

/// A layout by its xml name.
[[nodiscard]] const ClusterConfiguration& layout(std::string_view name)
{
    const ClusterConfiguration* found = ClusterConfiguration::fromXmlName(name);
    if (found == nullptr)
    {
        ADD_FAILURE() << "no cluster layout " << name;
        return ClusterConfiguration::single();
    }
    return *found;
}

/// A rocket with a stage holding a body tube stand-in (0.3 m long, radius 0.025 m, inner radius
/// 0.024 m) with an inner tube; events enabled and recorded.
class InnerTubeTest : public ::testing::Test
{
protected:
    InnerTubeTest()
    {
        auto& stage = m_rocket.addChild(std::make_unique<AxialStage>());
        m_body      = &stage.addChild(TestBodyComponent::make(0.3, 0.025));
        m_body->setInnerRadius(0.024);
        m_tube = &m_body->addChild(std::make_unique<InnerTube>());
        m_rocket.enableEvents();
        m_connection = m_rocket.addComponentChangeListener(
            [this](const ComponentChangeEvent& e) { m_types.push_back(e.getType()); });
    }

    Rocket                                  m_rocket;
    TestBodyComponent*                      m_body{nullptr};
    InnerTube*                              m_tube{nullptr};
    std::vector<int>                        m_types;
    ComponentChangeSignal::ScopedConnection m_connection;
};

TEST(InnerTube, Defaults)
{
    const InnerTube tube;
    EXPECT_EQ(tube.kind(), ComponentKind::INNER_TUBE);
    EXPECT_EQ(tube.getComponentName(), "Inner Tube");
    EXPECT_EQ(tube.getName(), "Inner Tube");
    EXPECT_EQ(tube.getOuterRadius(), 0.019 / 2);
    EXPECT_NEAR(tube.getInnerRadius(), 0.018 / 2, kEpsilon);
    EXPECT_NEAR(tube.getThickness(), 0.0005, kEpsilon);
    EXPECT_EQ(tube.getLength(), 0.070);
    EXPECT_FALSE(tube.isOuterRadiusAutomatic());
    EXPECT_FALSE(tube.isInnerRadiusAutomatic());
    EXPECT_EQ(tube.getAxialMethod(), AxialMethod::BOTTOM);
    EXPECT_FALSE(tube.isAfter());
    EXPECT_EQ(tube.getDisplayOrderSide(), 5);
    EXPECT_EQ(tube.getDisplayOrderBack(), 14);
    EXPECT_EQ(&tube.getClusterConfiguration(), &ClusterConfiguration::single());
    EXPECT_EQ(tube.getClusterScale(), 1.0);
    EXPECT_EQ(tube.getClusterRotation(), 0.0);
    EXPECT_EQ(tube.getInstanceCount(), 1);
    EXPECT_EQ(tube.getPatternName(), "single");
    EXPECT_FALSE(tube.isMotorMount());
    EXPECT_FALSE(tube.hasMotor());
    EXPECT_EQ(tube.getMotorOverhang(), 0.0);
    EXPECT_EQ(tube.getMotorCount(), 1);
    EXPECT_EQ(tube.getPresetType(), ComponentPresetType::BODY_TUBE);
    EXPECT_FALSE(tube.isAerodynamic());
    EXPECT_TRUE(tube.isMassive());
    EXPECT_EQ(tube.getMaterial().getName(), "Cardboard");
}

TEST(InnerTube, AcceptsInternalComponents)
{
    const InnerTube tube;
    EXPECT_TRUE(tube.allowsChildren());
    for (const ComponentKind kind : QtRocket::kAllComponentKinds)
    {
        EXPECT_EQ(tube.isCompatible(kind), QtRocket::isInternal(kind))
            << QtRocket::componentKindName(kind);
    }
    InnerTube parent;
    parent.addChild(std::make_unique<Bulkhead>());
    parent.addChild(std::make_unique<MassComponent>());
    parent.addChild(std::make_unique<InnerTube>());
    EXPECT_EQ(parent.getChildCount(), 3U);
    EXPECT_THROW(parent.addChild(std::make_unique<PodSet>()), BugError);
}

TEST(InnerTube, IsARadialParentOfConstantRadius)
{
    InnerTube tube;
    tube.setOuterRadius(0.02);
    tube.setThickness(0.001);
    const RadialParent& radial = tube;
    EXPECT_EQ(radial.getOuterRadius(0.0), 0.02);
    EXPECT_EQ(radial.getOuterRadius(0.05), 0.02);
    EXPECT_EQ(radial.getInnerRadius(0.05), tube.getInnerRadius());
    EXPECT_EQ(radial.getLength(), tube.getLength());
    EXPECT_EQ(tube.getMotorMountDiameter(), 2 * tube.getInnerRadius());
}

TEST(InnerTube, ARingInsideTakesItsInnerRadius)
{
    Rocket      rocket;
    auto&       stage = rocket.addChild(std::make_unique<AxialStage>());
    auto&       body  = stage.addChild(TestBodyComponent::make(0.3, 0.025));
    auto&       tube  = body.addChild(std::make_unique<InnerTube>());
    const auto& block = tube.addChild(std::make_unique<EngineBlock>());
    rocket.enableEvents();
    EXPECT_NEAR(block.getOuterRadius(), 0.018 / 2, kEpsilon);
    tube.setThickness(0.001);
    EXPECT_NEAR(block.getOuterRadius(), (0.019 / 2) - 0.001, kEpsilon);
}

// ================================================================================ cluster

TEST_F(InnerTubeTest, ClusterConfigurationSetsTheInstances)
{
    m_types.clear();
    m_tube->setClusterConfiguration(ClusterConfiguration::single());  // the same layout
    EXPECT_TRUE(m_types.empty());

    m_tube->setClusterConfiguration(layout("3-ring"));
    EXPECT_EQ(m_types, std::vector<int>{ComponentChangeEvent::kMassChange});
    EXPECT_EQ(m_tube->getInstanceCount(), 3);
    EXPECT_EQ(m_tube->getMotorCount(), 3);
    EXPECT_EQ(m_tube->getPatternName(), "3-ring");
    EXPECT_EQ(m_tube->getInstanceOffsets().size(), 3U);
    EXPECT_EQ(m_tube->getInstanceLocations().size(), 3U);
    EXPECT_EQ(m_tube->getComponentLocations().size(), 3U);

    // The count follows the layout only.
    m_tube->setInstanceCount(7);
    EXPECT_EQ(m_tube->getInstanceCount(), 3);
}

TEST(InnerTube, ClusterPointsAreScaledRotatedAndShifted)
{
    InnerTube tube;
    tube.setOuterRadius(0.01);
    tube.setClusterConfiguration(layout("double"));
    // The double layout is (-0.5, 0) and (0.5, 0); the separation is 2 * r * scale.
    EXPECT_EQ(tube.getClusterSeparation(), 0.02);
    std::vector<Coordinate> points = tube.getClusterPoints();
    ASSERT_EQ(points.size(), 2U);
    EXPECT_TRUE(points[0].exactlyEquals(Coordinate{0, -0.01, 0}));
    EXPECT_TRUE(points[1].exactlyEquals(Coordinate{0, 0.01, 0}));

    tube.setClusterScale(2.0);
    EXPECT_EQ(tube.getClusterSeparation(), 0.04);
    EXPECT_NEAR(tube.getClusterScaleAbsolute(), 0.02, kEpsilon);  // the gap between the tubes
    tube.setClusterScaleAbsolute(0.0);
    EXPECT_EQ(tube.getClusterScale(), 1.0);
    tube.setClusterScale(-3.0);
    EXPECT_EQ(tube.getClusterScale(), 0.0);
    tube.setClusterScale(1.0);

    // A rotation of pi/2: each (x, y) becomes (x cos + y sin, -x sin + y cos).
    tube.setClusterRotation(std::numbers::pi / 2);
    points = tube.getClusterPoints();
    EXPECT_NEAR(points[0].y, 0.0, kEpsilon);
    EXPECT_NEAR(points[0].z, 0.01, kEpsilon);
    EXPECT_NEAR(points[1].z, -0.01, kEpsilon);

    // The radial position shifts the cluster, and the radial direction turns it back.
    tube.setRadialDirection(std::numbers::pi / 2);
    tube.setRadialPosition(0.05);
    points = tube.getClusterPoints();
    EXPECT_NEAR(points[0].y, -0.01, kEpsilon);
    EXPECT_NEAR(points[0].z, 0.05, kEpsilon);
    EXPECT_NEAR(points[1].y, 0.01, kEpsilon);
    EXPECT_NEAR(points[1].z, 0.05, kEpsilon);
    EXPECT_EQ(tube.getInstanceOffsets().size(), 2U);
}

TEST_F(InnerTubeTest, ClusterSettersFireOnChangeOnly)
{
    m_types.clear();
    m_tube->setClusterScale(1.0);
    m_tube->setClusterRotation(0.0);
    m_tube->setClusterRotation(2 * std::numbers::pi);  // reduced to 0
    EXPECT_TRUE(m_types.empty());
    m_types.clear();
    m_tube->setClusterScale(1.5);
    m_tube->setClusterRotation(1.0);
    EXPECT_EQ(m_types, std::vector<int>(2, ComponentChangeEvent::kMassChange));
    EXPECT_NEAR(m_tube->getClusterRotation(), 1.0, kEpsilon);
    m_tube->setClusterRotation(1.0 + (2 * std::numbers::pi));
    EXPECT_NEAR(m_tube->getClusterRotation(), 1.0, 1e-9);
}

TEST_F(InnerTubeTest, ClusterMassAndCg)
{
    const double single = m_tube->getComponentMass();
    m_tube->setClusterConfiguration(layout("4-ring"));
    EXPECT_NEAR(m_tube->getComponentMass(), 4 * single, single * kEpsilon);
    const Coordinate cg = m_tube->getComponentCG();
    EXPECT_NEAR(cg.x, 0.035, kEpsilon);
    EXPECT_NEAR(cg.y, 0.0, kEpsilon);
    EXPECT_NEAR(cg.z, 0.0, kEpsilon);
    EXPECT_NEAR(cg.weight, 4 * single, single * kEpsilon);
}

TEST_F(InnerTubeTest, CenteringRingTakesTheTubeAsItsInnerRadius)
{
    auto& ring = m_body->addChild(std::make_unique<CenteringRing>());
    ring.setAxialMethod(AxialMethod::BOTTOM);
    ring.setAxialOffset(-0.01);  // within the tube's 70 mm at the aft end
    EXPECT_NEAR(ring.getInnerRadius(), 0.019 / 2, kEpsilon);
    EXPECT_NEAR(ring.getOuterRadius(), 0.024, kEpsilon);
}

// ============================================================================= motor mount

TEST_F(InnerTubeTest, MotorMountState)
{
    m_types.clear();
    m_tube->setMotorMount(false);  // unchanged
    EXPECT_TRUE(m_types.empty());
    m_tube->setMotorMount(true);
    EXPECT_EQ(m_types, std::vector<int>{ComponentChangeEvent::kMotorChange});
    EXPECT_TRUE(m_tube->isMotorMount());
    EXPECT_TRUE(static_cast<const RocketComponent&>(*m_tube).isMotorMount());

    m_types.clear();
    m_tube->setMotorOverhang(0.0);  // unchanged
    m_tube->setMotorOverhang(0.005);
    EXPECT_EQ(m_types, std::vector<int>{ComponentChangeEvent::kAeromassChange});
    EXPECT_EQ(m_tube->getMotorOverhang(), 0.005);
}

TEST_F(InnerTubeTest, MotorConfigurations)
{
    const FlightConfigurationId fcid = QtRocket::Test::testFcid(0);
    EXPECT_FALSE(m_tube->hasMotor());
    EXPECT_EQ(&m_tube->getMotorConfig(fcid), &m_tube->getDefaultMotorConfig());
    EXPECT_THROW(static_cast<void>(m_tube->getMotorPosition(fcid)), BugError);

    MotorConfiguration config{*m_tube, fcid};
    config.setMotor(QtRocket::Test::motorA8());
    m_tube->setMotorConfig(std::move(config), fcid);
    EXPECT_TRUE(m_tube->isMotorMount());  // set without an event, as in Java
    EXPECT_TRUE(m_tube->hasMotor());
    EXPECT_EQ(m_tube->getMotorConfigurationSet().size(), 1U);
    EXPECT_NE(m_tube->getMotorConfig(fcid).getMotor(), nullptr);

    m_tube->setMotorOverhang(0.01);
    const Coordinate position = m_tube->getMotorPosition(fcid);
    EXPECT_NEAR(position.x, 0.07 - 0.07 + 0.01, kEpsilon);
    EXPECT_EQ(position.y, 0.0);

    // Copying and resetting configurations.
    const FlightConfigurationId other = QtRocket::Test::testFcid(1);
    m_tube->copyFlightConfiguration(fcid, other);
    EXPECT_EQ(m_tube->getMotorConfigurationSet().size(), 2U);
    EXPECT_EQ(m_tube->getMotorConfig(other).getMotor(), m_tube->getMotorConfig(fcid).getMotor());
    m_tube->reset(other);
    EXPECT_EQ(m_tube->getMotorConfigurationSet().size(), 1U);

    // nullopt forgets the configuration.
    m_tube->setMotorConfig(std::nullopt, fcid);
    EXPECT_FALSE(m_tube->hasMotor());

    // A configuration of another mount is a bug.
    InnerTube otherTube;
    EXPECT_THROW(m_tube->setMotorConfig(MotorConfiguration{otherTube, fcid}, fcid), BugError);

    const QtRocket::InMemoryPreferences preferences;
    EXPECT_NE(m_tube->toMotorDebug(preferences).find("MotorConfigurationSet"), std::string::npos);
}

TEST(InnerTube, MotorCountIncludesTheAssemblyCopies)
{
    Rocket rocket;
    auto&  stage = rocket.addChild(std::make_unique<AxialStage>());
    auto&  body  = stage.addChild(TestBodyComponent::make(0.3, 0.025));
    auto&  pods  = body.addChild(std::make_unique<PodSet>());
    pods.setInstanceCount(3);
    auto& podBody = pods.addChild(TestBodyComponent::make(0.2, 0.02));
    auto& tube    = podBody.addChild(std::make_unique<InnerTube>());
    tube.setClusterConfiguration(layout("double"));
    rocket.enableEvents();
    // 2 motors per pod, 3 pods, 1 stage and the rocket.
    EXPECT_EQ(tube.getMotorCount(), 2);
    EXPECT_EQ(tube.getMotorCountIncludingAssemblyCopies(), 6);
}

TEST(InnerTube, MotorConfigurationsCountTheCluster)
{
    InnerTube tube;
    tube.setClusterConfiguration(layout("5-ring"));
    const FlightConfigurationId fcid = QtRocket::Test::testFcid(2);
    tube.setMotorConfig(MotorConfiguration{tube, fcid}, fcid);
    EXPECT_EQ(tube.getMotorConfig(fcid).getMotorCount(), 5);
}

// ================================================================================= copies

TEST(InnerTube, CopyRebuildsTheMotorConfigurations)
{
    InnerTube tube;
    tube.setClusterConfiguration(layout("3-row"));
    tube.setClusterScale(1.5);
    tube.setClusterRotation(0.25);
    tube.setMotorOverhang(0.003);
    tube.getInsideColorComponentHandler().setEdgesSameAsInside(true);
    const FlightConfigurationId fcid = QtRocket::Test::testFcid(2);
    MotorConfiguration          config{tube, fcid};
    config.setMotor(QtRocket::Test::motorA8());
    config.setEjectionDelay(4.0);
    tube.setMotorConfig(std::move(config), fcid);

    const std::unique_ptr<InnerTube> copy =
        QtRocket::componentCast<InnerTube>(tube.copyWithOriginalId());
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->getId(), tube.getId());
    EXPECT_EQ(&copy->getClusterConfiguration(), &tube.getClusterConfiguration());
    EXPECT_EQ(copy->getClusterScale(), 1.5);
    EXPECT_EQ(copy->getClusterRotation(), 0.25);
    EXPECT_EQ(copy->getMotorOverhang(), 0.003);
    EXPECT_TRUE(copy->isMotorMount());
    EXPECT_TRUE(copy->getInsideColorComponentHandler().isEdgesSameAsInside());
    ASSERT_TRUE(copy->hasMotor());
    const MotorConfiguration& copied = copy->getMotorConfig(fcid);
    EXPECT_EQ(&copied.getMount(), static_cast<const MotorMount*>(copy.get()));
    EXPECT_EQ(copied.getMotor(), tube.getMotorConfig(fcid).getMotor());
    EXPECT_EQ(copied.getEjectionDelay(), 4.0);
    EXPECT_EQ(&copy->getDefaultMotorConfig().getMount(),
              static_cast<const MotorMount*>(copy.get()));
}

/// Whether @p split is the single tube makeIndividualClusterComponent() makes of @p cluster's
/// member at @p location, named @p name (plain checks, collected into one result).
[[nodiscard]] ::testing::AssertionResult isSplitTube(const InnerTube&   split,
                                                     const InnerTube&   cluster,
                                                     const Coordinate&  location,
                                                     const std::string& name)
{
    const auto near = [](double a, double b) { return std::abs(a - b) <= kEpsilon; };
    // The single tube sits where the cluster member was.
    const Coordinate         offset = split.getInstanceOffsets().front();
    std::vector<std::string> problems;
    const std::vector<std::pair<bool, std::string_view>> checks{
        {split.getName() == name, "the name"},
        {!(split.getId() == cluster.getId()), "a new id"},
        {&split.getClusterConfiguration() == &ClusterConfiguration::single(), "a single tube"},
        {split.getClusterRotation() == 0.0, "no rotation"},
        {split.getClusterScale() == 1.0, "scale 1"},
        {split.getChildCount() == 1U, "the children copied"},
        {near(split.getRadialShiftY(), location.y), "the y shift"},
        {near(split.getRadialShiftZ(), location.z), "the z shift"},
        {near(split.getRadialPosition(), std::hypot(location.y, location.z)), "the position"},
        {near(offset.y, location.y) && near(offset.z, location.z), "the instance offset"},
    };
    for (const auto& [ok, what] : checks)
    {
        if (!ok)
        {
            problems.emplace_back(what);
        }
    }
    if (problems.empty())
    {
        return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure()
           << name << " fails: " << QtRocket::Strings::join(", ", problems);
}

TEST(InnerTube, MakeIndividualClusterComponent)
{
    Rocket rocket;
    auto&  stage = rocket.addChild(std::make_unique<AxialStage>());
    auto&  body  = stage.addChild(TestBodyComponent::make(0.3, 0.025));
    auto&  tube  = body.addChild(std::make_unique<InnerTube>());
    tube.setName("MMT");
    tube.setOuterRadius(0.009);
    tube.setClusterConfiguration(layout("3-ring"));
    tube.setClusterRotation(0.3);
    tube.setClusterScale(1.2);
    tube.addChild(std::make_unique<EngineBlock>());
    rocket.enableEvents();

    const std::vector<Coordinate> locations = tube.getComponentLocations();
    ASSERT_EQ(locations.size(), 3U);
    for (std::size_t i = 0; i < locations.size(); ++i)
    {
        const std::string                name = "MMT #" + std::to_string(i + 1);
        const std::unique_ptr<InnerTube> split =
            InnerTube::makeIndividualClusterComponent(locations[i], name, tube);
        ASSERT_NE(split, nullptr);
        EXPECT_TRUE(isSplitTube(*split, tube, locations[i], name));
    }
}

// ================================================================================= presets

/// A BT-20 body tube preset (0.2 m long, outer diameter 24.7 mm, inner 23.7 mm).
[[nodiscard]] ComponentPreset bodyTubePreset()
{
    TypedPropertyMap props;
    props.put(ComponentPreset::kType, ComponentPresetType::BODY_TUBE);
    props.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("Estes"));
    props.put(ComponentPreset::kPartNo, "BT-20");
    props.put(ComponentPreset::kLength, 0.2);
    props.put(ComponentPreset::kOuterDiameter, 0.0247);
    props.put(ComponentPreset::kInnerDiameter, 0.0237);
    const QtRocket::MaterialStorage materials;
    return ComponentPresetFactory::create(props, materials).value();
}

TEST(InnerTube, LoadFromABodyTubePreset)
{
    const ComponentPreset preset = bodyTubePreset();
    InnerTube             tube;
    tube.loadPreset(&preset);
    EXPECT_EQ(tube.getPresetComponent(), &preset);
    EXPECT_EQ(tube.getLength(), 0.2);
    EXPECT_EQ(tube.getOuterRadius(), 0.0247 / 2.0);
    EXPECT_EQ(tube.getThickness(), (0.0247 - 0.0237) / 2.0);
    EXPECT_FALSE(tube.isOuterRadiusAutomatic());

    tube.setOuterRadius(0.02);
    EXPECT_EQ(tube.getPresetComponent(), nullptr);
}

TEST_F(InnerTubeTest, LoadPresetEndsWithItsOwnEvent)
{
    const ComponentPreset preset = bodyTubePreset();
    m_types.clear();
    m_tube->loadPreset(&preset);
    // The frozen rocket fires the load's changes as one, then loadPreset()'s own event.
    ASSERT_EQ(m_types.size(), 2U);
    EXPECT_EQ(m_types.back(), ComponentChangeEvent::kNonFunctionalChange);
}

}  // namespace
