// LaunchLugTest.java (core/src/test/.../rocketcomponent) and LaunchLugComponentTests.java
// (core/src/test/.../preset), ported, and the launch lug's own behaviour.
//
// LaunchLugPresetTests.java (the LAUNCH_LUG presets the factory makes) is ported, case by case, by
// the TubePresetTest suite of preset/ComponentPresetFactoryTests.cpp, which runs the same cases
// for the five tube-like preset types: testManufacturerRequired, testPartNoRequired,
// testLengthRequired, testOnlyOuterDiameter, testOnlyInnerDiameter, testOnlyThicknessDiameter,
// testComputeInnerDiameter, testComputeOuterDiameter, testComputeThickness,
// testComputeThicknessLooses, testMaterial and the two density tests that are commented out in
// Java.
//
// The values of the suites that have no JUnit counterpart (the Pins below and the numbers in
// those tests) were computed with OpenRocket itself: a small Java program built the same
// components with the same calls and printed what OpenRocket's LaunchLug answers (commit
// 5f164fd0e on JDK 17). Values that went through sin() or cos() are compared with a relative
// tolerance of 1e-12.

#include "QtRocket/rocket/LaunchLug.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <memory>
#include <numbers>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialPreferences.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/BoxBounded.h"
#include "QtRocket/rocket/Coaxial.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/ExternalComponent.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InsideColorComponent.h"
#include "QtRocket/rocket/LineInstanceable.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/Tube.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AnglePositionable.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetFactory.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"

namespace
{

using QtRocket::AngleMethod;
using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::BoundingBox;
using QtRocket::BugError;
using QtRocket::ComponentChangeEvent;
using QtRocket::ComponentChangeSignal;
using QtRocket::ComponentKind;
using QtRocket::ComponentPreset;
using QtRocket::ComponentPresetFactory;
using QtRocket::ComponentPresetType;
using QtRocket::Coordinate;
using QtRocket::Finish;
using QtRocket::FlightConfigurationId;
using QtRocket::LaunchLug;
using QtRocket::Manufacturer;
using QtRocket::Material;
using QtRocket::NoseCone;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Transition;
using QtRocket::TransitionShape;
using QtRocket::TypedPropertyMap;

/// LaunchLugTest.EPSILON (MathUtil.EPSILON).
constexpr double kEpsilon = QtRocket::MathUtil::kEpsilon;

/// The event types of the setters: AEROMASS_CHANGE, AERODYNAMIC_CHANGE and NONFUNCTIONAL_CHANGE.
constexpr int kBoth          = ComponentChangeEvent::kBothChange;
constexpr int kAerodynamic   = ComponentChangeEvent::kAerodynamicChange;
constexpr int kNonFunctional = ComponentChangeEvent::kNonFunctionalChange;

using Events = std::vector<int>;

/// The nose cone, body tube and launch lug of TestRockets.makeEstesAlphaIII(), built in the
/// order and with the calls of the Java factory (its fin set, internal components and motors are
/// left out: the lug does not read them).
struct EstesAlphaIII
{
    Rocket      rocket;
    AxialStage* stage{nullptr};
    NoseCone*   nose{nullptr};
    BodyTube*   body{nullptr};
    LaunchLug*  lug{nullptr};

    EstesAlphaIII()
    {
        rocket.setName("Estes Alpha III / Code Verification Rocket");
        stage = &rocket.addChild(std::make_unique<AxialStage>());
        stage->setName("Stage");

        const double noseconeLength = 0.07;
        const double noseconeRadius = 0.012;
        auto         nosecone =
            std::make_unique<NoseCone>(TransitionShape::OGIVE, noseconeLength, noseconeRadius);
        nosecone->setAftShoulderLength(0.02);
        nosecone->setAftShoulderThickness(0);
        nosecone->setAftShoulderRadius(0.011);
        nosecone->setName("Nose Cone");
        nose = &stage->addChild(std::move(nosecone));

        const double bodytubeLength    = 0.20;
        const double bodytubeRadius    = 0.012;
        const double bodytubeThickness = 0.0003;
        body                           = &stage->addChild(
            std::make_unique<BodyTube>(bodytubeLength, bodytubeRadius, bodytubeThickness));
        body->setName("Body Tube");

        auto newLug = std::make_unique<LaunchLug>();
        newLug->setName("Launch Lugs");
        newLug->setAxialMethod(AxialMethod::TOP);
        newLug->setAxialOffset(0.111);
        newLug->setLength(0.050);
        newLug->setOuterRadius(0.0022);
        newLug->setInnerRadius(0.0020);
        lug = &body->addChild(std::move(newLug));

        rocket.enableEvents();
    }
};

/// The four assertions LaunchLugTest makes on a CG: x, y, z and the weight within EPSILON.
void expectCG(const Coordinate& cg, double x, double y, double z, double weight)
{
    EXPECT_NEAR(x, cg.x, kEpsilon) << " LaunchLug CG has the wrong x value: ";
    EXPECT_NEAR(y, cg.y, kEpsilon) << " LaunchLug CG has the wrong y value: ";
    EXPECT_NEAR(z, cg.z, kEpsilon) << " LaunchLug CG has the wrong z value: ";
    EXPECT_NEAR(weight, cg.weight, kEpsilon) << " LaunchLug CM has the wrong value: ";
}

// ============================================================================ LaunchLugTest

// Java: testLaunchLugLocationZeroAngle
TEST(LaunchLugTest, LaunchLugLocationZeroAngle)
{
    const EstesAlphaIII alpha;
    LaunchLug&          lug = *alpha.lug;
    lug.setInstanceSeparation(0.05);
    lug.setInstanceCount(2);

    const double                  expX = 0.111 + alpha.body->getComponentLocations().at(0).x;
    const double                  expR = -(alpha.body->getOuterRadius() + lug.getOuterRadius());
    Coordinate                    expPos{expX, expR, 0, 0};
    const std::vector<Coordinate> actPos = lug.getComponentLocations();
    ASSERT_EQ(actPos.size(), 2U);
    EXPECT_NEAR(expPos.x, actPos[0].x, kEpsilon) << " LaunchLug has the wrong x value: ";
    EXPECT_NEAR(expPos.y, actPos[0].y, kEpsilon) << " LaunchLug has the wrong y value: ";
    EXPECT_NEAR(expPos.z, actPos[0].z, kEpsilon) << " LaunchLug has the wrong z value: ";
    EXPECT_NEAR(0, actPos[0].weight, kEpsilon) << " LaunchLug has the wrong weight: ";
    EXPECT_EQ(expPos, actPos[0]) << " LaunchLug #1 is in the wrong place: ";

    expPos = expPos.setX(expX + 0.05);
    EXPECT_EQ(expPos, actPos[1]) << " LaunchLug #2 is in the wrong place: ";
}

// Java: testLaunchLugLocationAtAngles
TEST(LaunchLugTest, LaunchLugLocationAtAngles)
{
    const EstesAlphaIII alpha;
    LaunchLug&          lug        = *alpha.lug;
    const double        startAngle = std::numbers::pi / 2;
    lug.setAngleOffset(startAngle);
    lug.setInstanceSeparation(0.05);
    lug.setInstanceCount(2);

    const double                  expX = 0.111 + alpha.body->getComponentLocations().at(0).x;
    const double                  expR = 0.015;
    const double                  expY = std::cos(startAngle) * expR;
    const double                  expZ = std::sin(startAngle) * expR;
    Coordinate                    expPos{expX, expY, expZ, 0};
    const std::vector<Coordinate> actPos = lug.getComponentLocations();
    ASSERT_EQ(actPos.size(), 2U);
    EXPECT_NEAR(expPos.x, actPos[0].x, kEpsilon) << " LaunchLug has the wrong x value: ";
    EXPECT_NEAR(expPos.y, actPos[0].y, kEpsilon) << " LaunchLug has the wrong y value: ";
    EXPECT_NEAR(expPos.z, actPos[0].z, kEpsilon) << " LaunchLug has the wrong z value: ";
    EXPECT_NEAR(0, actPos[0].weight, kEpsilon) << " LaunchLug has the wrong weight: ";
    EXPECT_EQ(expPos, actPos[0]) << " LaunchLug is in the wrong place: ";

    expPos = expPos.setX(expX + 0.05);
    EXPECT_EQ(expPos, actPos[1]) << " LaunchLug #2 is in the wrong place: ";
}

// Java: testCMSingleInstance
TEST(LaunchLugTest, CMSingleInstance)
{
    BodyTube bodyTube;
    bodyTube.setOuterRadius(0.025);
    auto newLug = std::make_unique<LaunchLug>();
    newLug->setLength(0.1);
    newLug->setOuterRadius(0.02);
    LaunchLug& lug = bodyTube.addChild(std::move(newLug));

    // Test normal CG
    expectCG(lug.getCG(), 0.05, -0.045, 0, 0.008331504);

    // Test rotated CG
    lug.setAngleOffset(std::numbers::pi / 2);
    expectCG(lug.getCG(), 0.05, 0, 0.045, 0.008331504);

    lug.setAngleOffset(-std::numbers::pi / 3);
    expectCG(lug.getCG(), 0.05, 0.0225, -0.03897114, 0.008331504);

    // Change dimensions
    lug.setLength(0.05);
    lug.setOuterRadius(0.015);
    lug.setAngleOffset(0);
    expectCG(lug.getCG(), 0.025, 0.04, 0, 0.00309761);

    // Test rotated CG
    lug.setAngleOffset(std::numbers::pi / 2);
    expectCG(lug.getCG(), 0.025, 0, 0.04, 0.00309761);

    lug.setAngleOffset(-std::numbers::pi / 3);
    expectCG(lug.getCG(), 0.025, 0.02, -0.034641016, 0.00309761);
}

// Java: testCMSingleInstanceOverride
TEST(LaunchLugTest, CMSingleInstanceOverride)
{
    BodyTube bodyTube;
    bodyTube.setOuterRadius(0.025);
    auto newLug = std::make_unique<LaunchLug>();
    newLug->setLength(0.1);
    newLug->setOuterRadius(0.02);
    newLug->setCGOverridden(true);
    newLug->setOverrideCGX(0.0123);
    LaunchLug& lug = bodyTube.addChild(std::move(newLug));

    // Test normal CG
    expectCG(lug.getCG(), 0.0123, -0.045, 0, 0.008331504);

    // Test rotated CG
    lug.setAngleOffset(std::numbers::pi / 2);
    expectCG(lug.getCG(), 0.0123, 0, 0.045, 0.008331504);

    lug.setAngleOffset(-std::numbers::pi / 3);
    expectCG(lug.getCG(), 0.0123, 0.0225, -0.03897114, 0.008331504);

    // Change dimensions
    lug.setLength(0.05);
    lug.setOuterRadius(0.015);
    lug.setAngleOffset(0);
    lug.setOverrideCGX(0.0321);
    lug.setMassOverridden(true);
    lug.setOverrideMass(0.1);
    expectCG(lug.getCG(), 0.0321, 0.04, 0, 0.1);

    // Test rotated CG
    lug.setAngleOffset(std::numbers::pi / 2);
    expectCG(lug.getCG(), 0.0321, 0, 0.04, 0.1);

    lug.setAngleOffset(-std::numbers::pi / 3);
    expectCG(lug.getCG(), 0.0321, 0.02, -0.034641016, 0.1);
}

// Java: testCMMultipleInstances
TEST(LaunchLugTest, CMMultipleInstances)
{
    BodyTube bodyTube;
    bodyTube.setOuterRadius(0.025);
    auto newLug = std::make_unique<LaunchLug>();
    newLug->setLength(0.1);
    newLug->setOuterRadius(0.02);
    newLug->setInstanceCount(3);
    newLug->setInstanceSeparation(0.2);
    LaunchLug& lug = bodyTube.addChild(std::move(newLug));

    // Test normal CG
    expectCG(lug.getCG(), 0.25, -0.045, 0, 0.024994512);

    // Test rotated CG
    lug.setAngleOffset(std::numbers::pi / 2);
    expectCG(lug.getCG(), 0.25, 0, 0.045, 0.024994512);

    lug.setAngleOffset(-std::numbers::pi / 3);
    expectCG(lug.getCG(), 0.25, 0.0225, -0.03897114, 0.024994512);

    // Change dimensions
    lug.setLength(0.05);
    lug.setOuterRadius(0.015);
    lug.setAngleOffset(0);
    lug.setInstanceCount(2);
    lug.setInstanceSeparation(0.15);
    expectCG(lug.getCG(), 0.1, 0.04, 0, 0.00619522);

    // Test rotated CG
    lug.setAngleOffset(std::numbers::pi / 2);
    expectCG(lug.getCG(), 0.1, 0, 0.04, 0.00619522);

    lug.setAngleOffset(-std::numbers::pi / 3);
    expectCG(lug.getCG(), 0.1, 0.02, -0.034641016, 0.00619522);
}

// Java: testCMMultipleInstancesOverride
TEST(LaunchLugTest, CMMultipleInstancesOverride)
{
    BodyTube bodyTube;
    bodyTube.setOuterRadius(0.025);
    auto newLug = std::make_unique<LaunchLug>();
    newLug->setLength(0.1);
    newLug->setOuterRadius(0.02);
    newLug->setInstanceCount(3);
    newLug->setInstanceSeparation(0.2);
    newLug->setCGOverridden(true);
    newLug->setOverrideCGX(0.0123);
    LaunchLug& lug = bodyTube.addChild(std::move(newLug));

    // Test normal CG
    expectCG(lug.getCG(), 0.0123, -0.045, 0, 0.024994512);

    // Test rotated CG
    lug.setAngleOffset(std::numbers::pi / 2);
    expectCG(lug.getCG(), 0.0123, 0, 0.045, 0.024994512);

    lug.setAngleOffset(-std::numbers::pi / 3);
    expectCG(lug.getCG(), 0.0123, 0.0225, -0.03897114, 0.024994512);

    // Change dimensions
    lug.setLength(0.05);
    lug.setOuterRadius(0.015);
    lug.setAngleOffset(0);
    lug.setInstanceCount(2);
    lug.setInstanceSeparation(0.15);
    lug.setOverrideCGX(0.0321);
    lug.setMassOverridden(true);
    lug.setOverrideMass(0.2);
    expectCG(lug.getCG(), 0.0321, 0.04, 0, 0.2);

    // Test rotated CG
    lug.setAngleOffset(std::numbers::pi / 2);
    expectCG(lug.getCG(), 0.0321, 0, 0.04, 0.2);

    lug.setAngleOffset(-std::numbers::pi / 3);
    expectCG(lug.getCG(), 0.0321, 0.02, -0.034641016, 0.2);
}

// ================================================================== LaunchLugComponentTests

/// LaunchLugComponentTests.createPreset(): a launch lug preset 2 m long, 2 m outer and 1 m inner
/// diameter, of mass 100 kg.
[[nodiscard]] ComponentPreset lugPreset()
{
    TypedPropertyMap presetspec;
    presetspec.put(ComponentPreset::kType, ComponentPresetType::LAUNCH_LUG);
    presetspec.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("manufacturer"));
    presetspec.put(ComponentPreset::kPartNo, "partno");
    presetspec.put(ComponentPreset::kLength, 2.0);
    presetspec.put(ComponentPreset::kOuterDiameter, 2.0);
    presetspec.put(ComponentPreset::kInnerDiameter, 1.0);
    presetspec.put(ComponentPreset::kMass, 100.0);
    const QtRocket::MaterialStorage materials;
    return ComponentPresetFactory::create(presetspec, materials).value();
}

class LaunchLugComponentTest : public ::testing::Test
{
protected:
    ComponentPreset m_preset{lugPreset()};
};

// Java: testComponentType
TEST_F(LaunchLugComponentTest, ComponentType)
{
    const LaunchLug bt;
    EXPECT_EQ(bt.getPresetType(), ComponentPresetType::LAUNCH_LUG);
}

// Java: testLoadFromPresetIsSane
TEST_F(LaunchLugComponentTest, LoadFromPresetIsSane)
{
    LaunchLug bt;
    bt.loadPreset(&m_preset);

    EXPECT_EQ(bt.getLength(), 2.0);
    EXPECT_EQ(bt.getOuterRadius(), 1.0);
    EXPECT_EQ(bt.getInnerRadius(), 0.5);

    EXPECT_EQ(bt.getMaterial(), m_preset.get(ComponentPreset::kMaterial));
    EXPECT_NEAR(bt.getMass(), 100.0, 0.05);
}

// Java: changeLengthLeavesPreset
TEST_F(LaunchLugComponentTest, ChangeLengthLeavesPreset)
{
    LaunchLug bt;
    bt.loadPreset(&m_preset);
    bt.setLength(1.0);
    EXPECT_EQ(bt.getPresetComponent(), &m_preset);
}

// Java: changeODClearsPreset
TEST_F(LaunchLugComponentTest, ChangeODClearsPreset)
{
    LaunchLug bt;
    bt.loadPreset(&m_preset);
    bt.setOuterRadius(2.0);
    EXPECT_EQ(bt.getPresetComponent(), nullptr);
}

// Java: changeIDClearsPreset
TEST_F(LaunchLugComponentTest, ChangeIDClearsPreset)
{
    LaunchLug bt;
    bt.loadPreset(&m_preset);
    bt.setInnerRadius(0.75);
    EXPECT_EQ(bt.getPresetComponent(), nullptr);
}

// Java: changeThicknessClearsPreset
TEST_F(LaunchLugComponentTest, ChangeThicknessClearsPreset)
{
    LaunchLug bt;
    bt.loadPreset(&m_preset);
    bt.setThickness(0.1);
    EXPECT_EQ(bt.getPresetComponent(), nullptr);
}

// Java: changeMaterialClearsPreset
TEST_F(LaunchLugComponentTest, ChangeMaterialClearsPreset)
{
    LaunchLug bt;
    bt.loadPreset(&m_preset);
    bt.setMaterial(Material::newMaterial(Material::Type::BULK, "new", 1.0, true));
    EXPECT_EQ(bt.getPresetComponent(), nullptr);
}

// Java: changeFinishLeavesPreset
TEST_F(LaunchLugComponentTest, ChangeFinishLeavesPreset)
{
    LaunchLug bt;
    bt.loadPreset(&m_preset);
    bt.setFinish(Finish::POLISHED);
    EXPECT_EQ(bt.getPresetComponent(), &m_preset);
}

// ========================================================================= Java's own values

/// Whether @p actual is the Java value @p expected: both NaN, equal, or within 1e-12 relative,
/// plus 1e-15 absolute for values near zero such as sin(pi) * r.
[[nodiscard]] bool matches(double expected, double actual)
{
    if (std::isnan(expected) || std::isnan(actual))
    {
        return std::isnan(expected) && std::isnan(actual);
    }
    if (expected == actual)
    {
        return true;
    }
    return std::abs(actual - expected) <=
           (1e-12 * std::max(std::abs(expected), std::abs(actual))) + 1e-15;
}

/// Collects the differences between Java's values and the computed ones, one line each.
class Differences
{
public:
    void number(std::string_view field, double expected, double actual)
    {
        if (!matches(expected, actual))
        {
            m_text += std::format("  {}: expected {}, got {}\n", field, expected, actual);
        }
    }

    void coordinate(std::string_view field, const Coordinate& expected, const Coordinate& actual)
    {
        number(std::format("{}.x", field), expected.x, actual.x);
        number(std::format("{}.y", field), expected.y, actual.y);
        number(std::format("{}.z", field), expected.z, actual.z);
        number(std::format("{}.weight", field), expected.weight, actual.weight);
    }

    void coordinates(std::string_view field, const std::vector<Coordinate>& expected,
                     const std::vector<Coordinate>& actual)
    {
        if (expected.size() != actual.size())
        {
            m_text += std::format("  {}: expected {} points, got {}\n", field, expected.size(),
                                  actual.size());
            return;
        }
        for (std::size_t i = 0; i < expected.size(); i++)
        {
            coordinate(std::format("{}[{}]", field, i), expected[i], actual[i]);
        }
    }

    /// Empty when everything matched.
    [[nodiscard]] const std::string& text() const noexcept { return m_text; }

private:
    std::string m_text;
};

/// What OpenRocket's LaunchLug answers in one state.
struct Pins
{
    int                     instanceCount{};
    double                  outerRadius{};
    double                  innerRadius{};
    double                  thickness{};
    double                  angleOffset{};
    double                  instanceSeparation{};
    double                  length{};
    double                  axialOffset{};
    Coordinate              position;
    double                  componentVolume{};
    double                  componentMass{};
    Coordinate              componentCG;
    double                  longitudinalUnitInertia{};
    double                  rotationalUnitInertia{};
    std::vector<Coordinate> componentBounds;
    Coordinate              boxMin;
    Coordinate              boxMax;
    std::vector<Coordinate> instanceOffsets;
    std::vector<Coordinate> componentLocations;
};

/// The differences between @p lug and Java's @p expected, empty when there are none.
[[nodiscard]] std::string differences(const Pins& expected, const LaunchLug& lug)
{
    Differences d;
    d.number("instanceCount", expected.instanceCount, lug.getInstanceCount());
    d.number("outerRadius", expected.outerRadius, lug.getOuterRadius());
    d.number("innerRadius", expected.innerRadius, lug.getInnerRadius());
    d.number("thickness", expected.thickness, lug.getThickness());
    d.number("angleOffset", expected.angleOffset, lug.getAngleOffset());
    d.number("instanceSeparation", expected.instanceSeparation, lug.getInstanceSeparation());
    d.number("length", expected.length, lug.getLength());
    d.number("axialOffset", expected.axialOffset, lug.getAxialOffset());
    d.coordinate("position", expected.position, lug.getPosition());
    d.number("componentVolume", expected.componentVolume, lug.getComponentVolume());
    d.number("componentMass", expected.componentMass, lug.getComponentMass());
    d.coordinate("componentCG", expected.componentCG, lug.getComponentCG());
    d.number("longitudinalUnitInertia", expected.longitudinalUnitInertia,
             lug.getLongitudinalUnitInertia());
    d.number("rotationalUnitInertia", expected.rotationalUnitInertia,
             lug.getRotationalUnitInertia());
    d.coordinates("componentBounds", expected.componentBounds, lug.getComponentBounds());
    const BoundingBox box = lug.getInstanceBoundingBox();
    d.coordinate("instanceBoundingBox.min", expected.boxMin, box.min());
    d.coordinate("instanceBoundingBox.max", expected.boxMax, box.max());
    d.coordinates("instanceOffsets", expected.instanceOffsets, lug.getInstanceOffsets());
    d.coordinates("componentLocations", expected.componentLocations, lug.getComponentLocations());
    return d.text();
}

// ---- Java's values, one function per state (printed by the Java program)

[[nodiscard]] Pins pinsDetached()
{
    return Pins{
        .instanceCount      = 1,
        .outerRadius        = 0.005,
        .innerRadius        = 0.004,
        .thickness          = 0.001,
        .angleOffset        = std::numbers::pi,
        .instanceSeparation = 0.06,
        .length             = 0.03,
        .axialOffset        = 0.0,
        .position           = Coordinate{0.0, 0.0, 0.0, 0.0},
        .componentVolume    = 8.482300164692443E-7,
        .componentMass      = 5.767964111990861E-4,
        .componentCG = Coordinate{0.015, -0.005, 6.123233995736766E-19, 5.767964111990861E-4},
        .longitudinalUnitInertia = 8.525E-5,
        .rotationalUnitInertia   = 2.05E-5,
        .componentBounds = {Coordinate{0.0, -0.005, -0.005, 0.0},
                            Coordinate{0.0, 0.005, -0.005, 0.0}, Coordinate{0.0, 0.005, 0.005, 0.0},
                            Coordinate{0.0, -0.005, 0.005, 0.0},
                            Coordinate{0.03, -0.005, -0.005, 0.0},
                            Coordinate{0.03, 0.005, -0.005, 0.0},
                            Coordinate{0.03, 0.005, 0.005, 0.0},
                            Coordinate{0.03, -0.005, 0.005, 0.0}},
        .boxMin          = Coordinate{0.0, -0.005, -0.005, 0.0},
        .boxMax          = Coordinate{0.03, 0.005, 0.005, 0.0},
        .instanceOffsets = {Coordinate{0.0, -0.0, 0.0, 0.0}},
        .componentLocations = {Coordinate{0.0, -0.0, 0.0, 0.0}}};
}

[[nodiscard]] Pins pinsNoRocket()
{
    return Pins{.instanceCount      = 1,
                .outerRadius        = 0.005,
                .innerRadius        = 0.004,
                .thickness          = 0.001,
                .angleOffset        = std::numbers::pi,
                .instanceSeparation = 0.06,
                .length             = 0.03,
                .axialOffset        = 0.0,
                .position           = Coordinate{0.0, 0.0, 0.0, 0.0},
                .componentVolume    = 8.482300164692443E-7,
                .componentMass      = 5.767964111990861E-4,
                .componentCG        = Coordinate{0.015, -0.030000000000000002, 3.67394039744206E-18,
                                                 5.767964111990861E-4},
                .longitudinalUnitInertia = 8.525E-5,
                .rotationalUnitInertia   = 2.05E-5,
                .componentBounds =
                    {Coordinate{0.0, -0.005, -0.005, 0.0}, Coordinate{0.0, 0.005, -0.005, 0.0},
                     Coordinate{0.0, 0.005, 0.005, 0.0}, Coordinate{0.0, -0.005, 0.005, 0.0},
                     Coordinate{0.03, -0.005, -0.005, 0.0}, Coordinate{0.03, 0.005, -0.005, 0.0},
                     Coordinate{0.03, 0.005, 0.005, 0.0}, Coordinate{0.03, -0.005, 0.005, 0.0}},
                .boxMin             = Coordinate{0.0, -0.005, -0.005, 0.0},
                .boxMax             = Coordinate{0.03, 0.005, 0.005, 0.0},
                .instanceOffsets    = {Coordinate{0.0, -0.0, 0.0, 0.0}},
                .componentLocations = {Coordinate{0.0, 0.0, 0.0, 0.0}}};
}

[[nodiscard]] Pins pinsAlpha()
{
    return Pins{
        .instanceCount      = 1,
        .outerRadius        = 0.003,
        .innerRadius        = 0.002,
        .thickness          = 0.001,
        .angleOffset        = std::numbers::pi,
        .instanceSeparation = 0.06,
        .length             = 0.05,
        .axialOffset        = 0.111,
        .position           = Coordinate{0.111, 0.0, 0.0, 0.0},
        .componentVolume    = 7.853981633974483E-7,
        .componentMass      = 5.340707511102649E-4,
        .componentCG = Coordinate{0.025, -0.015, 1.8369701987210296E-18, 5.340707511102649E-4},
        .longitudinalUnitInertia = 2.1158333333333337E-4,
        .rotationalUnitInertia   = 6.5000000000000004E-6,
        .componentBounds = {Coordinate{0.0, -0.003, -0.003, 0.0},
                            Coordinate{0.0, 0.003, -0.003, 0.0}, Coordinate{0.0, 0.003, 0.003, 0.0},
                            Coordinate{0.0, -0.003, 0.003, 0.0},
                            Coordinate{0.05, -0.003, -0.003, 0.0},
                            Coordinate{0.05, 0.003, -0.003, 0.0},
                            Coordinate{0.05, 0.003, 0.003, 0.0},
                            Coordinate{0.05, -0.003, 0.003, 0.0}},
        .boxMin          = Coordinate{0.0, -0.003, -0.003, 0.0},
        .boxMax          = Coordinate{0.05, 0.003, 0.003, 0.0},
        .instanceOffsets = {Coordinate{0.0, -0.015, 1.8369701987210296E-18, 0.0}},
        .componentLocations = {Coordinate{0.181, -0.015, 1.8369701987210296E-18, 0.0}}};
}

[[nodiscard]] Pins pinsThree()
{
    return Pins{.instanceCount      = 3,
                .outerRadius        = 0.003,
                .innerRadius        = 0.002,
                .thickness          = 0.001,
                .angleOffset        = std::numbers::pi,
                .instanceSeparation = 0.05,
                .length             = 0.05,
                .axialOffset        = 0.111,
                .position           = Coordinate{0.111, 0.0, 0.0, 0.0},
                .componentVolume    = 2.3561944901923448E-6,
                .componentMass      = 0.0016022122533307945,
                .componentCG = Coordinate{0.07500000000000001, -0.015, 1.8369701987210296E-18,
                                          0.0016022122533307945},
                .longitudinalUnitInertia = 2.1158333333333337E-4,
                .rotationalUnitInertia   = 6.5000000000000004E-6,
                .componentBounds =
                    {Coordinate{0.0, -0.003, -0.003, 0.0}, Coordinate{0.0, 0.003, -0.003, 0.0},
                     Coordinate{0.0, 0.003, 0.003, 0.0}, Coordinate{0.0, -0.003, 0.003, 0.0},
                     Coordinate{0.05, -0.003, -0.003, 0.0}, Coordinate{0.05, 0.003, -0.003, 0.0},
                     Coordinate{0.05, 0.003, 0.003, 0.0}, Coordinate{0.05, -0.003, 0.003, 0.0}},
                .boxMin             = Coordinate{0.0, -0.003, -0.003, 0.0},
                .boxMax             = Coordinate{0.05, 0.003, 0.003, 0.0},
                .instanceOffsets    = {Coordinate{0.0, -0.015, 1.8369701987210296E-18, 0.0},
                                       Coordinate{0.05, -0.015, 1.8369701987210296E-18, 0.0},
                                       Coordinate{0.1, -0.015, 1.8369701987210296E-18, 0.0}},
                .componentLocations = {Coordinate{0.181, -0.015, 1.8369701987210296E-18, 0.0},
                                       Coordinate{0.231, -0.015, 1.8369701987210296E-18, 0.0},
                                       Coordinate{0.281, -0.015, 1.8369701987210296E-18, 0.0}}};
}

[[nodiscard]] Pins pinsRotated()
{
    return Pins{.instanceCount      = 3,
                .outerRadius        = 0.003,
                .innerRadius        = 0.002,
                .thickness          = 0.001,
                .angleOffset        = 1.5707963267948966,
                .instanceSeparation = 0.05,
                .length             = 0.05,
                .axialOffset        = 0.111,
                .position           = Coordinate{0.111, 0.0, 0.0, 0.0},
                .componentVolume    = 2.3561944901923448E-6,
                .componentMass      = 0.0016022122533307945,
                .componentCG        = Coordinate{0.07500000000000001, 9.184850993605148E-19, 0.015,
                                                 0.0016022122533307945},
                .longitudinalUnitInertia = 2.1158333333333337E-4,
                .rotationalUnitInertia   = 6.5000000000000004E-6,
                .componentBounds =
                    {Coordinate{0.0, -0.003, -0.003, 0.0}, Coordinate{0.0, 0.003, -0.003, 0.0},
                     Coordinate{0.0, 0.003, 0.003, 0.0}, Coordinate{0.0, -0.003, 0.003, 0.0},
                     Coordinate{0.05, -0.003, -0.003, 0.0}, Coordinate{0.05, 0.003, -0.003, 0.0},
                     Coordinate{0.05, 0.003, 0.003, 0.0}, Coordinate{0.05, -0.003, 0.003, 0.0}},
                .boxMin             = Coordinate{0.0, -0.003, -0.003, 0.0},
                .boxMax             = Coordinate{0.05, 0.003, 0.003, 0.0},
                .instanceOffsets    = {Coordinate{0.0, 9.184850993605148E-19, 0.015, 0.0},
                                       Coordinate{0.05, 9.184850993605148E-19, 0.015, 0.0},
                                       Coordinate{0.1, 9.184850993605148E-19, 0.015, 0.0}},
                .componentLocations = {Coordinate{0.181, 9.184850993605148E-19, 0.015, 0.0},
                                       Coordinate{0.231, 9.184850993605148E-19, 0.015, 0.0},
                                       Coordinate{0.281, 9.184850993605148E-19, 0.015, 0.0}}};
}

[[nodiscard]] Pins pinsFinal()
{
    return Pins{
        .instanceCount      = 3,
        .outerRadius        = 0.003,
        .innerRadius        = 0.0025,
        .thickness          = 5.0E-4,
        .angleOffset        = -1.0,
        .instanceSeparation = 0.05,
        .length             = 0.04,
        .axialOffset        = -0.01,
        .position           = Coordinate{0.15, 0.0, 0.0, 0.0},
        .componentVolume    = 1.0367255756846318E-6,
        .componentMass      = 7.049733914655497E-4,
        .componentCG =
            Coordinate{0.07, 0.008104534588022096, -0.012622064772118448, 7.049733914655497E-4},
        .longitudinalUnitInertia = 1.3714583333333333E-4,
        .rotationalUnitInertia   = 7.625000000000001E-6,
        .componentBounds = {Coordinate{0.0, -0.003, -0.003, 0.0},
                            Coordinate{0.0, 0.003, -0.003, 0.0}, Coordinate{0.0, 0.003, 0.003, 0.0},
                            Coordinate{0.0, -0.003, 0.003, 0.0},
                            Coordinate{0.04, -0.003, -0.003, 0.0},
                            Coordinate{0.04, 0.003, -0.003, 0.0},
                            Coordinate{0.04, 0.003, 0.003, 0.0},
                            Coordinate{0.04, -0.003, 0.003, 0.0}},
        .boxMin          = Coordinate{0.0, -0.003, -0.003, 0.0},
        .boxMax          = Coordinate{0.04, 0.003, 0.003, 0.0},
        .instanceOffsets = {Coordinate{0.0, 0.008104534588022096, -0.012622064772118448, 0.0},
                            Coordinate{0.05, 0.008104534588022096, -0.012622064772118448, 0.0},
                            Coordinate{0.1, 0.008104534588022096, -0.012622064772118448, 0.0}},
        .componentLocations = {Coordinate{0.22, 0.008104534588022096, -0.012622064772118448, 0.0},
                               Coordinate{0.27, 0.008104534588022096, -0.012622064772118448, 0.0},
                               Coordinate{0.32, 0.008104534588022096, -0.012622064772118448, 0.0}}};
}

[[nodiscard]] Pins pinsOverhang()
{
    return Pins{
        .instanceCount      = 3,
        .outerRadius        = 0.003,
        .innerRadius        = 0.0025,
        .thickness          = 5.0E-4,
        .angleOffset        = -1.0,
        .instanceSeparation = 0.05,
        .length             = 0.04,
        .axialOffset        = -0.02,
        .position           = Coordinate{-0.02, 0.0, 0.0, 0.0},
        .componentVolume    = 1.0367255756846318E-6,
        .componentMass      = 7.049733914655497E-4,
        .componentCG =
            Coordinate{0.07, 0.008104534588022096, -0.012622064772118448, 7.049733914655497E-4},
        .longitudinalUnitInertia = 1.3714583333333333E-4,
        .rotationalUnitInertia   = 7.625000000000001E-6,
        .componentBounds = {Coordinate{0.0, -0.003, -0.003, 0.0},
                            Coordinate{0.0, 0.003, -0.003, 0.0}, Coordinate{0.0, 0.003, 0.003, 0.0},
                            Coordinate{0.0, -0.003, 0.003, 0.0},
                            Coordinate{0.04, -0.003, -0.003, 0.0},
                            Coordinate{0.04, 0.003, -0.003, 0.0},
                            Coordinate{0.04, 0.003, 0.003, 0.0},
                            Coordinate{0.04, -0.003, 0.003, 0.0}},
        .boxMin          = Coordinate{0.0, -0.003, -0.003, 0.0},
        .boxMax          = Coordinate{0.04, 0.003, 0.003, 0.0},
        .instanceOffsets = {Coordinate{0.0, 0.008104534588022096, -0.012622064772118448, 0.0},
                            Coordinate{0.05, 0.008104534588022096, -0.012622064772118448, 0.0},
                            Coordinate{0.1, 0.008104534588022096, -0.012622064772118448, 0.0}},
        .componentLocations = {
            Coordinate{0.05, 0.008104534588022096, -0.012622064772118448, 0.0},
            Coordinate{0.1, 0.008104534588022096, -0.012622064772118448, 0.0},
            Coordinate{0.15000000000000002, 0.008104534588022096, -0.012622064772118448, 0.0}}};
}

[[nodiscard]] Pins pinsCopy()
{
    return Pins{
        .instanceCount      = 2,
        .outerRadius        = 0.006,
        .innerRadius        = 0.004,
        .thickness          = 0.002,
        .angleOffset        = 0.5,
        .instanceSeparation = 0.1,
        .length             = 0.03,
        .axialOffset        = 0.0,
        .position           = Coordinate{0.0, 0.0, 0.0, 0.0},
        .componentVolume    = 3.7699111843077517E-6,
        .componentMass      = 0.0037699111843077517,
        .componentCG =
            Coordinate{0.065, 0.005265495371342237, 0.002876553231625218, 0.0037699111843077517},
        .longitudinalUnitInertia = 8.800000000000001E-5,
        .rotationalUnitInertia   = 2.6000000000000002E-5,
        .componentBounds = {Coordinate{0.0, -0.006, -0.006, 0.0},
                            Coordinate{0.0, 0.006, -0.006, 0.0}, Coordinate{0.0, 0.006, 0.006, 0.0},
                            Coordinate{0.0, -0.006, 0.006, 0.0},
                            Coordinate{0.03, -0.006, -0.006, 0.0},
                            Coordinate{0.03, 0.006, -0.006, 0.0},
                            Coordinate{0.03, 0.006, 0.006, 0.0},
                            Coordinate{0.03, -0.006, 0.006, 0.0}},
        .boxMin          = Coordinate{0.0, -0.006, -0.006, 0.0},
        .boxMax          = Coordinate{0.03, 0.006, 0.006, 0.0},
        .instanceOffsets = {Coordinate{0.0, 0.0, 0.0, 0.0}, Coordinate{0.1, 0.0, 0.0, 0.0}},
        .componentLocations = {Coordinate{0.0, 0.0, 0.0, 0.0}, Coordinate{0.1, 0.0, 0.0, 0.0}}};
}

// ================================================================================ detached

TEST(LaunchLug, Defaults)
{
    const LaunchLug lug;
    EXPECT_EQ(lug.kind(), ComponentKind::LAUNCH_LUG);
    EXPECT_EQ(lug.getName(), "Launch Lug");
    EXPECT_EQ(lug.getMaterial(), QtRocket::ExternalComponent::defaultMaterial());
    EXPECT_EQ(lug.getMaterial().getName(), "Cardboard");
    EXPECT_EQ(lug.getPresetType(), ComponentPresetType::LAUNCH_LUG);
    EXPECT_FALSE(lug.isAfter());
    EXPECT_TRUE(lug.isAerodynamic());
    EXPECT_TRUE(lug.isMassive());
    EXPECT_EQ(lug.getAxialMethod(), AxialMethod::MIDDLE);
    EXPECT_EQ(lug.getAngleMethod(), AngleMethod::RELATIVE);
    EXPECT_EQ(lug.getAngleOffset(), std::numbers::pi);
    EXPECT_EQ(lug.getPatternName(), "1-Line");
    EXPECT_EQ(lug.getInstanceAngles(), std::vector<double>{0.0});
    EXPECT_EQ(lug.getDisplayOrderSide(), 15);
    EXPECT_EQ(lug.getDisplayOrderBack(), 12);
    EXPECT_EQ(differences(pinsDetached(), lug), "");
}

TEST(LaunchLug, IsATubeWithTheInterfacesOfJava)
{
    LaunchLug lug;
    EXPECT_NE(dynamic_cast<QtRocket::Tube*>(&lug), nullptr);
    EXPECT_NE(dynamic_cast<QtRocket::Coaxial*>(&lug), nullptr);
    EXPECT_NE(dynamic_cast<QtRocket::AnglePositionable*>(&lug), nullptr);
    EXPECT_NE(dynamic_cast<QtRocket::BoxBounded*>(&lug), nullptr);
    EXPECT_NE(dynamic_cast<QtRocket::LineInstanceable*>(&lug), nullptr);
    EXPECT_NE(dynamic_cast<QtRocket::InsideColorComponent*>(&lug), nullptr);

    // Through the interfaces the overrides answer, not RocketComponent's defaults.
    lug.setInstanceCount(2);
    const RocketComponent& component = lug;
    EXPECT_EQ(component.getInstanceCount(), 2);
    EXPECT_EQ(component.getAngleOffset(), std::numbers::pi);
    EXPECT_EQ(component.getInstanceOffsets().size(), 2U);
    EXPECT_EQ(component.getInstanceLocations().size(), 2U);
    EXPECT_EQ(component.getInstanceAngles(), (std::vector<double>{0.0, 0.0}));
}

TEST(LaunchLug, AcceptsNoChildren)
{
    LaunchLug lug;
    EXPECT_FALSE(lug.allowsChildren());
    for (const ComponentKind kind : QtRocket::kAllComponentKinds)
    {
        EXPECT_FALSE(lug.isCompatible(kind)) << QtRocket::componentKindName(kind);
    }
    EXPECT_THROW(lug.addChild(std::make_unique<QtRocket::MassComponent>()), BugError);
}

TEST(LaunchLug, WithoutAnEventTheInstancesStayOnTheParentsAxis)
{
    // A tree that is not in a Rocket delivers no events, so componentChanged() never stores the
    // radial distance; the CG reads the parent directly and is on the surface.
    BodyTube         body(0.3, 0.025, 0.002);
    const LaunchLug& lug = body.addChild(std::make_unique<LaunchLug>());
    EXPECT_EQ(differences(pinsNoRocket(), lug), "");
    EXPECT_EQ(lug.getInstanceOffsets().at(0).y, 0.0);
}

TEST(LaunchLug, CopyKeepsEveryField)
{
    LaunchLug original;
    original.setMaterial(Material::newMaterial(Material::Type::BULK, "x", 1000, true));
    original.setOuterRadius(0.006);
    original.setThickness(0.002);
    original.setInstanceCount(2);
    original.setInstanceSeparation(0.1);
    original.setAngleOffset(0.5);
    original.getInsideColorComponentHandler().setEdgesSameAsInside(true);

    const std::unique_ptr<LaunchLug> copy =
        QtRocket::componentCast<LaunchLug>(original.copyWithNewIds());
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(differences(pinsCopy(), *copy), "");
    EXPECT_EQ(copy->getMaterial(), original.getMaterial());
    EXPECT_TRUE(copy->getInsideColorComponentHandler().isEdgesSameAsInside());
    EXPECT_NE(copy->getId(), original.getId());

    const std::unique_ptr<RocketComponent> sameId = original.copyWithOriginalId();
    EXPECT_EQ(sameId->getId(), original.getId());
    EXPECT_EQ(sameId->kind(), ComponentKind::LAUNCH_LUG);

    original.setInstanceCount(5);
    EXPECT_EQ(copy->getInstanceCount(), 2);
}

TEST(LaunchLug, ApplyDefaultMaterialTakesThePreferencesDefaultForItsClass)
{
    QtRocket::MaterialStorage     storage;
    QtRocket::InMemoryPreferences prefs;
    QtRocket::addBuiltinMaterials(storage);

    LaunchLug lug;
    lug.applyDefaultMaterial(prefs, storage);
    EXPECT_EQ(lug.getMaterial().getName(), "Cardboard") << "the built-in fallback";

    const Material balsa =
        storage.findMaterial(Material::Type::BULK, "Balsa")
            .value_or(Material::newMaterial(Material::Type::BULK, "<not found>", 0, true));
    ASSERT_EQ(balsa.getName(), "Balsa");
    QtRocket::setDefaultComponentMaterial(prefs, "LaunchLug", balsa);
    lug.applyDefaultMaterial(prefs, storage);
    EXPECT_EQ(lug.getMaterial(), balsa);
}

// ====================================================================== on the Alpha III body

/// The Alpha III parts with the events recorded.
class LaunchLugOnBody : public ::testing::Test
{
protected:
    LaunchLugOnBody()
    {
        m_connection = m_alpha.rocket.addComponentChangeListener(
            [this](const ComponentChangeEvent& e) { m_types.push_back(e.getType()); });
    }

    /// The types of the events fired since the last call.
    [[nodiscard]] Events takeEvents() { return std::exchange(m_types, {}); }

    [[nodiscard]] LaunchLug& lug() const noexcept { return *m_alpha.lug; }

    EstesAlphaIII                           m_alpha;
    Events                                  m_types;
    ComponentChangeSignal::ScopedConnection m_connection;
};

TEST(LaunchLug, BeforeEventsAreEnabledTheInstancesAreOnTheAxis)
{
    Rocket      rocket;
    auto&       stage = rocket.addChild(std::make_unique<AxialStage>());
    auto&       body  = stage.addChild(std::make_unique<BodyTube>(0.20, 0.012, 0.0003));
    const auto& lug   = body.addChild(std::make_unique<LaunchLug>());
    EXPECT_EQ(lug.getInstanceOffsets().at(0).y, 0.0);

    rocket.enableEvents();
    EXPECT_TRUE(matches(-0.017, lug.getInstanceOffsets().at(0).y))
        << "the body radius plus the lug's";
}

TEST_F(LaunchLugOnBody, SitsOnTheBodysSurface)
{
    // setInnerRadius(0.0020) kept the 1 mm wall of a new lug, so the outer radius is 3 mm.
    EXPECT_EQ(differences(pinsAlpha(), lug()), "");
    EXPECT_EQ(lug().getAxialMethod(), AxialMethod::TOP);

    // The launch lug assertions of RocketTest.testEstesAlphaIII.
    EXPECT_EQ(lug().getInstanceCount(), 1) << lug().getName() << " have incorrect count: ";
    EXPECT_EQ(lug().getComponentLocations().at(0), (Coordinate{0.181, -0.015, 0}))
        << lug().getName() << " not positioned correctly: ";
}

TEST_F(LaunchLugOnBody, InstancesAreSpacedAlongTheAxis)
{
    lug().setInstanceSeparation(0.05);
    EXPECT_EQ(takeEvents(), Events{kAerodynamic});
    lug().setInstanceSeparation(0.05);
    EXPECT_EQ(takeEvents(), Events{});
    lug().setInstanceCount(3);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    lug().setInstanceCount(3);
    EXPECT_EQ(takeEvents(), Events{});
    lug().setInstanceCount(0);
    EXPECT_EQ(takeEvents(), Events{});
    lug().setInstanceCount(-2);
    EXPECT_EQ(takeEvents(), Events{});
    EXPECT_EQ(lug().getInstanceCount(), 3);
    EXPECT_EQ(lug().getPatternName(), "3-Line");

    // The volume and the mass count the three lugs; the unit inertias are those of one.
    EXPECT_EQ(differences(pinsThree(), lug()), "");
}

TEST_F(LaunchLugOnBody, AngleOffsetIsClampedToPlusMinusPi)
{
    lug().setInstanceSeparation(0.05);
    lug().setInstanceCount(3);
    static_cast<void>(takeEvents());

    lug().setAngleOffset(std::numbers::pi / 2);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    lug().setAngleOffset(std::numbers::pi / 2);
    EXPECT_EQ(takeEvents(), Events{});
    EXPECT_EQ(differences(pinsRotated(), lug()), "");

    lug().setAngleOffset(4);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    EXPECT_EQ(lug().getAngleOffset(), std::numbers::pi) << "clamped, not reduced";
    lug().setAngleOffset(7);
    EXPECT_EQ(takeEvents(), Events{}) << "clamped to the current angle";
    lug().setAngleOffset(-4);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    EXPECT_EQ(lug().getAngleOffset(), -std::numbers::pi);
    lug().setAngleOffset(-1);
    EXPECT_EQ(takeEvents(), Events{kBoth});

    lug().setAngleMethod(AngleMethod::FIXED);
    EXPECT_EQ(takeEvents(), Events{});
    EXPECT_EQ(lug().getAngleMethod(), AngleMethod::RELATIVE);
}

TEST_F(LaunchLugOnBody, RadiusAndThicknessSetters)
{
    lug().setOuterRadius(0.003);
    EXPECT_EQ(takeEvents(), Events{}) << "the current radius";
    lug().setOuterRadius(0.0022);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    lug().setOuterRadius(0.003);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    EXPECT_EQ(lug().getThickness(), 0.001);
    EXPECT_TRUE(matches(0.002, lug().getInnerRadius()));

    lug().setOuterRadius(0.0001);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    EXPECT_EQ(lug().getThickness(), 1.0E-4) << "a wall thicker than the radius becomes it";
    EXPECT_EQ(lug().getInnerRadius(), 0.0);
    lug().setThickness(0.0001);
    EXPECT_EQ(takeEvents(), Events{}) << "the current thickness";

    lug().setOuterRadius(0.004);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    lug().setThickness(1);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    EXPECT_EQ(lug().getThickness(), 0.004) << "at most the outer radius";
    lug().setThickness(-1);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    EXPECT_EQ(lug().getThickness(), 0.0);
    lug().setThickness(0.0005);
    EXPECT_EQ(takeEvents(), Events{kBoth});

    // The inner radius keeps the wall and moves the outer radius.
    lug().setInnerRadius(0.002);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    EXPECT_EQ(lug().getOuterRadius(), 0.0025);
    EXPECT_EQ(lug().getThickness(), 5.0E-4);
    EXPECT_TRUE(matches(0.002, lug().getInnerRadius()));
    lug().setInnerRadius(0.002);
    EXPECT_EQ(takeEvents(), Events{});

    // As in OpenRocket, a negative outer radius is accepted and takes the wall with it.
    lug().setInstanceCount(3);
    static_cast<void>(takeEvents());
    lug().setOuterRadius(-0.001);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    EXPECT_EQ(lug().getOuterRadius(), -0.001);
    EXPECT_EQ(lug().getThickness(), -0.001);
    EXPECT_EQ(lug().getInnerRadius(), 0.0);
    EXPECT_TRUE(matches(4.7123889803846896E-7, lug().getComponentVolume()))
        << lug().getComponentVolume();
}

TEST_F(LaunchLugOnBody, LengthAndAxialPosition)
{
    lug().setInstanceSeparation(0.05);
    lug().setInstanceCount(3);
    lug().setAngleOffset(-1);
    lug().setThickness(0.0005);
    static_cast<void>(takeEvents());

    lug().setLength(0.05);
    EXPECT_EQ(takeEvents(), Events{}) << "the current length";
    lug().setLength(0.04);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    lug().setLength(-0.04);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    EXPECT_EQ(lug().getLength(), -0.04) << "not clamped, as in OpenRocket";
    lug().setLength(0.04);
    static_cast<void>(takeEvents());

    // Unlike a tube fin set or a rail button, a launch lug's setAxialMethod() fires nothing.
    lug().setAxialMethod(AxialMethod::BOTTOM);
    EXPECT_EQ(takeEvents(), Events{});
    EXPECT_TRUE(matches(-0.049, lug().getAxialOffset())) << lug().getAxialOffset();
    lug().setAxialOffset(-0.01);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    EXPECT_EQ(differences(pinsFinal(), lug()), "");

    // A lug that starts ahead of the body: its ends are clamped onto the body for the radius.
    lug().setAxialMethod(AxialMethod::TOP);
    lug().setAxialOffset(-0.02);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    EXPECT_EQ(differences(pinsOverhang(), lug()), "");
}

TEST_F(LaunchLugOnBody, PresetEvents)
{
    const ComponentPreset preset = lugPreset();
    lug().loadPreset(&preset);
    // loadFromPreset()'s own change (as the rocket thaws), then loadPreset()'s.
    EXPECT_EQ(takeEvents(), (Events{kBoth, kNonFunctional}));

    lug().setLength(1.0);
    EXPECT_EQ(takeEvents(), Events{kBoth});
    lug().setInstanceCount(2);
    lug().setInstanceSeparation(0.3);
    lug().setAngleOffset(1.0);
    EXPECT_EQ(lug().getPresetComponent(), &preset);
    static_cast<void>(takeEvents());

    // The preset is cleared before the setter's own change fires.
    lug().setOuterRadius(2.0);
    EXPECT_EQ(takeEvents(), (Events{kNonFunctional, kBoth}));
    EXPECT_EQ(lug().getPresetComponent(), nullptr);
}

TEST(LaunchLugPreset, LoadedValues)
{
    const ComponentPreset preset = lugPreset();
    LaunchLug             lug;
    lug.loadPreset(&preset);
    EXPECT_EQ(lug.getThickness(), 0.5);
    EXPECT_EQ(lug.getMaterial().getName(), "TubeCustom");
    EXPECT_TRUE(matches(21.22065907891938, lug.getMaterial().getDensity()));
    EXPECT_NEAR(lug.getMass(), 100.0, 1e-9);
    EXPECT_EQ(lug.getInstanceSeparation(), 0.06) << "not the preset's business";
}

TEST(LaunchLugPreset, APresetGivenByItsThickness)
{
    // The factory completes the inner diameter, and the lug takes its wall from the two diameters.
    TypedPropertyMap presetspec;
    presetspec.put(ComponentPreset::kType, ComponentPresetType::LAUNCH_LUG);
    presetspec.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("manufacturer"));
    presetspec.put(ComponentPreset::kPartNo, "thick");
    presetspec.put(ComponentPreset::kLength, 0.05);
    presetspec.put(ComponentPreset::kOuterDiameter, 0.01);
    presetspec.put(ComponentPreset::kThickness, 0.0015);
    const QtRocket::MaterialStorage materials;
    const ComponentPreset preset = ComponentPresetFactory::create(presetspec, materials).value();

    LaunchLug lug;
    lug.loadPreset(&preset);
    EXPECT_EQ(lug.getOuterRadius(), 0.005);
    EXPECT_TRUE(matches(0.0015, lug.getThickness())) << lug.getThickness();
    EXPECT_TRUE(matches(0.0035, lug.getInnerRadius())) << lug.getInnerRadius();
    EXPECT_EQ(lug.getLength(), 0.05);
}

// ========================================================================== automatic radii

/// The body components of TestRockets.makeBeta() (see BodyBeta in RocketTests.cpp), with its two
/// launch lugs when @p withLugs: a nose cone and a body tube in the sustainer stage, a body tube
/// and a tail cone in the booster stage; TEST_FCID_1 selected with every stage active.
struct Beta
{
    Rocket      rocket;
    NoseCone*   nose{nullptr};
    BodyTube*   body{nullptr};
    BodyTube*   boosterBody{nullptr};
    Transition* tailCone{nullptr};
    LaunchLug*  lug{nullptr};
    LaunchLug*  boosterLug{nullptr};

    explicit Beta(bool withLugs)
    {
        // TestRockets.TEST_FCID_0 ... TEST_FCID_4.
        for (const std::string_view key :
             {"d010716e-ce0e-469d-ae46-190f3653ebbf", "f41bee5b-ebb8-4d92-bce7-53001577a313",
              "3e8d1280-53c2-4234-89a7-de215ef5cd69", "415a5485-f2da-4c2a-8803-394220ae58b8",
              "5abc18ec-a200-46f1-90c4-60b6995fc933"})
        {
            rocket.createFlightConfiguration(FlightConfigurationId::fromString(key));
        }
        const FlightConfigurationId fcid1 =
            FlightConfigurationId::fromString("f41bee5b-ebb8-4d92-bce7-53001577a313");
        auto& sustainer = rocket.addChild(std::make_unique<AxialStage>());

        auto nosecone = std::make_unique<NoseCone>(TransitionShape::OGIVE, 0.07, 0.012);
        nosecone->setAftShoulderLength(0.02);
        nosecone->setAftShoulderThickness(0);
        nosecone->setAftShoulderRadius(0.011);
        nose = &sustainer.addChild(std::move(nosecone));
        body = &sustainer.addChild(std::make_unique<BodyTube>(0.20, 0.012, 0.0003));
        if (withLugs)
        {
            lug = &body->addChild(makeLug(0.111));
        }
        const double sustainerRadius    = body->getAftRadius();
        const double sustainerThickness = body->getThickness();

        auto& booster = rocket.addChild(std::make_unique<AxialStage>());
        boosterBody   = &booster.addChild(
            std::make_unique<BodyTube>(0.06, sustainerRadius, sustainerThickness));
        if (withLugs)
        {
            boosterLug = &boosterBody->addChild(makeLug(0.0));
        }
        auto tail = std::make_unique<Transition>();
        tail->setForeRadius(0.012);
        tail->setAftRadius(0.01);
        tail->setLength(0.005);
        tailCone = &booster.addChild(std::move(tail));

        rocket.setSelectedConfiguration(fcid1);
        rocket.getSelectedConfiguration().setAllStages();
        rocket.enableEvents();
    }

    /// A launch lug of makeBeta(), @p offset from the top of its body tube.
    [[nodiscard]] static std::unique_ptr<LaunchLug> makeLug(double offset)
    {
        auto newLug = std::make_unique<LaunchLug>();
        newLug->setName("Launch Lugs");
        newLug->setAxialMethod(AxialMethod::TOP);
        newLug->setAxialOffset(offset);
        newLug->setLength(0.050);
        newLug->setOuterRadius(0.0022);
        newLug->setInnerRadius(0.0020);
        return newLug;
    }
};

// The steps of RocketTest.testAutoSizeNextComponent on the real launch lugs. A lug's
// componentChanged() reads its body's radius at both of its ends, which refreshes an automatic
// body tube radius and the component it was taken from; the last step depends on that.
TEST(LaunchLugAutoRadius, NextComponentWithTheLugs)
{
    const Beta   beta(true);
    const double expRadius = 0.012;

    beta.nose->setAftRadiusAutomatic(true);
    EXPECT_NEAR(expRadius, beta.nose->getAftRadius(), kEpsilon) << " radius match: ";

    beta.boosterBody->setOuterRadiusAutomatic(true);
    EXPECT_NEAR(expRadius, beta.boosterBody->getOuterRadius(), kEpsilon)
        << " trailing transition match: ";

    beta.body->setOuterRadiusAutomatic(true);
    EXPECT_NEAR(expRadius, beta.body->getOuterRadius(), kEpsilon) << " radius match: ";
    EXPECT_EQ(beta.nose->getAftRadius(), 0.012);
    EXPECT_EQ(beta.boosterBody->getOuterRadius(), 0.012);
    EXPECT_EQ(beta.tailCone->getForeRadius(), 0.012);

    // What the lugs stored while those events went round (Java's values): the sustainer's lug
    // read its body's radius before the neighbours had settled it, the default 25 mm.
    Differences d;
    d.coordinates("lug.instanceOffsets", {Coordinate{0.0, -0.028, 3.429011037612589E-18, 0.0}},
                  beta.lug->getInstanceOffsets());
    d.coordinates("lug.componentLocations", {Coordinate{0.181, -0.028, 3.429011037612589E-18, 0.0}},
                  beta.lug->getComponentLocations());
    d.coordinates("boosterLug.instanceOffsets",
                  {Coordinate{0.0, -0.015, 1.8369701987210296E-18, 0.0}},
                  beta.boosterLug->getInstanceOffsets());
    d.coordinates("boosterLug.componentLocations",
                  {Coordinate{0.27, -0.015, 1.8369701987210296E-18, 0.0}},
                  beta.boosterLug->getComponentLocations());
    EXPECT_EQ(d.text(), "");
}

TEST(LaunchLugAutoRadius, NextComponentWithoutTheLugs)
{
    // The same steps without the lugs end differently in OpenRocket too: nothing reads the
    // sustainer body's radius in between, and it falls back on the default radius.
    const Beta beta(false);

    beta.nose->setAftRadiusAutomatic(true);
    EXPECT_EQ(beta.nose->getAftRadius(), 0.012);
    beta.boosterBody->setOuterRadiusAutomatic(true);
    EXPECT_EQ(beta.boosterBody->getOuterRadius(), 0.012);
    beta.body->setOuterRadiusAutomatic(true);
    EXPECT_EQ(beta.body->getOuterRadius(), 0.025);
    EXPECT_EQ(beta.nose->getAftRadius(), 0.012);
    EXPECT_EQ(beta.boosterBody->getOuterRadius(), 0.012);
    EXPECT_EQ(beta.tailCone->getForeRadius(), 0.012);
}

// The steps of RocketTest.testAutoSizePreviousComponent on the real launch lugs.
TEST(LaunchLugAutoRadius, PreviousComponent)
{
    for (const bool withLugs : {true, false})
    {
        const Beta beta(withLugs);
        beta.body->setOuterRadiusAutomatic(true);
        EXPECT_EQ(beta.body->getOuterRadius(), 0.012) << withLugs;
        beta.tailCone->setForeRadiusAutomatic(true);
        EXPECT_EQ(beta.tailCone->getForeRadius(), 0.012) << withLugs;
        beta.boosterBody->setOuterRadiusAutomatic(true);
        EXPECT_EQ(beta.boosterBody->getOuterRadius(), 0.012) << withLugs;
    }
}

}  // namespace
