// MassObjectTest.java (core/src/test/.../rocketcomponent), ported, the MassObject behaviour
// OpenRocket does not test, and the golden geometry of the mass objects of OpenRocket's test
// rockets (the MassObjectGolden suite, labelled "golden").

#include "QtRocket/rocket/MassObject.h"

#include <filesystem>
#include <memory>
#include <numbers>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/ShockCord.h"
#include "QtRocket/rocket/Streamer.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/MathUtil.h"
#include "goldens/GoldenData.h"
#include "rocket/TestBodyComponent.h"
#include "rocket/TestComponent.h"

namespace
{

using nlohmann::json;
using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::ComponentChangeEvent;
using QtRocket::ComponentChangeSignal;
using QtRocket::ComponentKind;
using QtRocket::Coordinate;
using QtRocket::InnerTube;
using QtRocket::MassComponent;
using QtRocket::MassObject;
using QtRocket::Parachute;
using QtRocket::Rocket;
using QtRocket::ShockCord;
using QtRocket::Streamer;
using QtRocket::Test::GoldenCheck;
using QtRocket::Test::TestBodyComponent;
using QtRocket::Test::TestComponent;

constexpr double kEpsilon = QtRocket::MathUtil::kEpsilon;

/// A new mass object of each concrete class, as MassObjectTest's array: a shock cord, a mass
/// component, a parachute and a streamer, by index.
[[nodiscard]] std::unique_ptr<MassObject> newMassObject(int index)
{
    switch (index)
    {
        case 0:
            return std::make_unique<ShockCord>();
        case 1:
            return std::make_unique<MassComponent>();
        case 2:
            return std::make_unique<Parachute>();
        default:
            return std::make_unique<Streamer>();
    }
}

class MassObjectAutoRadiusTest : public ::testing::TestWithParam<int>
{ };

// Java: MassObjectTest.testAutoRadius, one mass object class per parameter.
TEST_P(MassObjectAutoRadiusTest, AutoRadius)
{
    std::unique_ptr<MassObject> owned = newMassObject(GetParam());
    MassObject&                 mo    = *owned;
    SCOPED_TRACE(QtRocket::className(mo.kind()));

    // Test no auto
    mo.setRadiusAutomatic(false);
    mo.setRadius(0.1);
    mo.setLength(0.1);
    EXPECT_NEAR(0.1, mo.getRadius(), kEpsilon) << " No auto incorrect radius";
    EXPECT_NEAR(0.1, mo.getLength(), kEpsilon) << " No auto incorrect length";
    EXPECT_NEAR(0.05, mo.getComponentCG().x, kEpsilon) << " No auto incorrect CG";

    mo.setLength(0.1);
    EXPECT_NEAR(0.1, mo.getLength(), kEpsilon) << " No auto 2 incorrect length";
    EXPECT_NEAR(0.05, mo.getComponentCG().x, kEpsilon) << " No auto incorrect CG";

    // Test auto
    auto parent = TestBodyComponent::make(0.0, 0.05);  // a BodyTube stand-in
    parent->setInnerRadius(0.05);
    parent->addChild(std::move(owned));
    mo.setRadiusAutomatic(true);
    EXPECT_NEAR(0.05, mo.getRadius(), kEpsilon) << " Auto 1 incorrect radius";
    EXPECT_NEAR(0.4, mo.getLength(), kEpsilon) << " Auto 1 incorrect length";
    EXPECT_NEAR(0.2, mo.getComponentCG().x, kEpsilon) << " Auto 1 incorrect CG";

    parent->setOuterRadius(0.1);
    parent->setInnerRadius(0.1);
    EXPECT_NEAR(0.1, mo.getRadius(), kEpsilon) << " Auto 2 incorrect radius";
    EXPECT_NEAR(0.1, mo.getLength(), kEpsilon) << " Auto 2 incorrect length";
    EXPECT_NEAR(0.05, mo.getComponentCG().x, kEpsilon) << " Auto 2 incorrect CG";

    parent->setOuterRadius(0.075);
    parent->setInnerRadius(0.075);
    mo.setLength(0.075);
    EXPECT_NEAR(0.075, mo.getRadius(), kEpsilon) << " Auto 3 incorrect radius";
    EXPECT_NEAR(0.075, mo.getLength(), kEpsilon) << " Auto 3 incorrect length";
    EXPECT_NEAR(0.0375, mo.getComponentCG().x, kEpsilon) << " Auto 3 incorrect CG";

    mo.setLength(0.05);
    EXPECT_NEAR(0.075, mo.getRadius(), kEpsilon) << " Auto 4 incorrect radius";
    EXPECT_NEAR(0.05, mo.getLength(), 0.001) << " Auto 4 incorrect length";
    EXPECT_NEAR(0.025, mo.getComponentCG().x, kEpsilon) << " Auto 4 incorrect CG";
}

INSTANTIATE_TEST_SUITE_P(MassObject, MassObjectAutoRadiusTest, ::testing::Values(0, 1, 2, 3));

TEST(MassObject, DefaultsAndPosition)
{
    const MassComponent mc;
    EXPECT_EQ(mc.getLength(), 0.025);
    EXPECT_EQ(mc.getRadius(), 0.0125);
    EXPECT_FALSE(mc.isRadiusAutomatic());
    EXPECT_EQ(mc.getAxialMethod(), AxialMethod::TOP);
    EXPECT_EQ(mc.getAxialOffset(), 0.0);
    EXPECT_FALSE(mc.isAfter());
    EXPECT_FALSE(mc.isAerodynamic());
    EXPECT_TRUE(mc.isMassive());
    EXPECT_EQ(mc.getRadialPosition(), 0.0);
    EXPECT_EQ(mc.getRadialDirection(), 0.0);
}

TEST(MassObject, SettersClampAndKeepTheVolume)
{
    MassComponent mc;
    mc.setLength(-1.0);
    EXPECT_EQ(mc.getLength(), 0.0);
    mc.setRadius(-1.0);
    EXPECT_EQ(mc.getRadius(), 0.0);

    // A stored radius of 0: the automatic length keeps the stored length.
    mc.setLength(0.2);
    auto parent = TestBodyComponent::make(0.5, 0.03);
    parent->setInnerRadius(0.0);
    auto& added = parent->addChild(std::make_unique<MassComponent>());
    added.setRadius(0.0);
    added.setLength(0.2);
    added.setRadiusAutomatic(true);
    EXPECT_EQ(added.getAutoRadius(), 0.0);  // the parent offers 0, so the stored 0
    EXPECT_EQ(added.getLength(), 0.2);
}

TEST(MassObject, AutomaticRadiusOfEachParentKind)
{
    // A body tube: its inner radius (Coaxial).
    auto        body = TestBodyComponent::make(0.5, 0.03);
    const auto& mc   = body->addChild(std::make_unique<MassComponent>());
    body->setInnerRadius(0.028);
    EXPECT_EQ(mc.getMaxParentRadius(), 0.028);

    // A transition: the larger of the fore and aft radii.
    auto transition = TestBodyComponent::make(0.1, 0.03, ComponentKind::TRANSITION);
    transition->setForeAftRadii(0.02, 0.035);
    const auto& inTransition = transition->addChild(std::make_unique<MassComponent>());
    EXPECT_EQ(inTransition.getMaxParentRadius(), 0.035);
    transition->setForeAftRadii(0.04, 0.035);
    EXPECT_EQ(inTransition.getMaxParentRadius(), 0.04);

    // A nose cone: the base radius.
    auto nose = TestBodyComponent::make(0.1, 0.03, ComponentKind::NOSE_CONE);
    nose->setForeAftRadii(0.0, 0.03);
    const auto& inNose = nose->addChild(std::make_unique<MassComponent>());
    EXPECT_EQ(inNose.getMaxParentRadius(), 0.03);
    nose->setForeAftRadii(0.03, 0.0);  // a flipped nose cone
    EXPECT_EQ(inNose.getMaxParentRadius(), 0.03);

    // A ring component: its inner radius.
    auto        tube   = std::make_unique<InnerTube>();
    const auto& inTube = tube->addChild(std::make_unique<MassComponent>());
    EXPECT_EQ(inTube.getMaxParentRadius(), tube->getInnerRadius());

    // Anything else: 0, and the automatic radius keeps the stored one.
    auto        other   = TestComponent::make(0.1, ComponentKind::POD_SET);
    const auto& inOther = other->addChild(std::make_unique<MassComponent>());
    EXPECT_EQ(inOther.getMaxParentRadius(), 0.0);
    EXPECT_EQ(inOther.getAutoRadius(), 0.0125);
    const MassComponent detached;
    EXPECT_EQ(detached.getMaxParentRadius(), 0.0);
    EXPECT_EQ(detached.getAutoRadius(), 0.0125);
}

TEST(MassObject, GetLengthReadsTheStoredRadiusUntilGetRadiusRefreshesIt)
{
    // As in Java, getLength() divides the volume by the stored radius; getRadius() stores the
    // automatic radius first.
    auto  body = TestBodyComponent::make(0.5, 0.05);
    auto& mc   = body->addChild(std::make_unique<MassComponent>(0.1, 0.1, 1.0));
    body->setInnerRadius(0.05);
    mc.setRadiusAutomatic(true);
    EXPECT_NEAR(mc.getLength(), 0.1, kEpsilon);  // still the stored radius 0.1
    EXPECT_NEAR(mc.getRadius(), 0.05, kEpsilon);
    EXPECT_NEAR(mc.getLength(), 0.4, kEpsilon);

    // Inertias and bounds read the radius first.
    body->setInnerRadius(0.1);
    EXPECT_NEAR(mc.getLongitudinalUnitInertia(), ((3 * 0.01) + 0.01) / 12, kEpsilon);
    EXPECT_NEAR(mc.getRotationalUnitInertia(), 0.01 / 2, kEpsilon);

    // Switching the automatic radius off keeps what was computed last.
    mc.setRadiusAutomatic(false);
    EXPECT_NEAR(mc.getRadius(), 0.1, kEpsilon);
    EXPECT_NEAR(mc.getLength(), 0.1, kEpsilon);
}

TEST(MassObject, RadialPositionShiftsTheCg)
{
    MassComponent mc{0.1, 0.02, 0.5};
    mc.setRadialPosition(0.01);
    mc.setRadialDirection(std::numbers::pi / 2);
    Coordinate cg = mc.getComponentCG();
    EXPECT_EQ(cg.x, 0.05);
    EXPECT_NEAR(cg.y, 0.0, 1e-15);
    EXPECT_NEAR(cg.z, 0.01, 1e-15);
    EXPECT_EQ(cg.weight, 0.5);

    mc.setRadialDirection(3 * std::numbers::pi / 2);  // reduced to -pi/2
    EXPECT_NEAR(mc.getRadialDirection(), -std::numbers::pi / 2, 1e-15);
    cg = mc.getComponentCG();
    EXPECT_NEAR(cg.z, -0.01, 1e-15);

    mc.setRadialPosition(-1.0);
    EXPECT_EQ(mc.getRadialPosition(), 0.0);
    EXPECT_EQ(mc.getComponentCG().z, 0.0);
}

TEST(MassObject, InertiasAndBoundsOfASolidCylinder)
{
    const MassComponent mc{0.2, 0.03, 1.0};
    EXPECT_NEAR(mc.getLongitudinalUnitInertia(), ((3 * 0.03 * 0.03) + (0.2 * 0.2)) / 12, 1e-15);
    EXPECT_NEAR(mc.getRotationalUnitInertia(), (0.03 * 0.03) / 2, 1e-15);
    const std::vector<Coordinate> bounds = mc.getComponentBounds();
    ASSERT_EQ(bounds.size(), 8U);
    EXPECT_TRUE(bounds[0].exactlyEquals(Coordinate{0, -0.03, -0.03}));
    EXPECT_TRUE(bounds[2].exactlyEquals(Coordinate{0, 0.03, 0.03}));
    EXPECT_TRUE(bounds[4].exactlyEquals(Coordinate{0.2, -0.03, -0.03}));
    EXPECT_TRUE(bounds[6].exactlyEquals(Coordinate{0.2, 0.03, 0.03}));
}

TEST(MassObject, SettersFireOnChange)
{
    Rocket rocket;
    auto&  stage = rocket.addChild(std::make_unique<AxialStage>());
    auto&  body  = stage.addChild(TestBodyComponent::make(0.5, 0.03));
    auto&  mc    = body.addChild(std::make_unique<MassComponent>());
    rocket.enableEvents();
    std::vector<int>                              types;
    const ComponentChangeSignal::ScopedConnection connection{rocket.addComponentChangeListener(
        [&types](const ComponentChangeEvent& e) { types.push_back(e.getType()); })};

    mc.setLength(0.025);
    mc.setRadius(0.0125);
    mc.setRadiusAutomatic(false);
    mc.setRadialPosition(0.0);
    mc.setRadialDirection(0.0);
    EXPECT_TRUE(types.empty());

    mc.setLength(0.03);
    mc.setRadius(0.01);
    mc.setRadialPosition(0.001);
    mc.setRadialDirection(0.5);
    EXPECT_EQ(types, std::vector<int>(4, ComponentChangeEvent::kMassChange));
    types.clear();
    mc.setRadiusAutomatic(true);
    EXPECT_EQ(types, std::vector<int>{ComponentChangeEvent::kAeromassChange});
    types.clear();
    mc.setRadius(0.01);  // equal, but it makes the radius manual
    EXPECT_EQ(types, std::vector<int>{ComponentChangeEvent::kMassChange});
    EXPECT_FALSE(mc.isRadiusAutomatic());
}

// ===================================================================== goldens

/// The component at @p path of the golden geometry of test rocket @p input.
[[nodiscard]] json goldenComponent(std::string_view input, std::string_view path)
{
    const QtRocket::Result<json> geometry =
        QtRocket::Test::loadGoldenJson(std::filesystem::path{std::string{input}} / "geometry.json");
    if (!geometry)
    {
        ADD_FAILURE() << "cannot read the geometry of " << input;
        return json{};
    }
    for (const json& component : geometry->at("components"))
    {
        if (component.at("path").get<std::string>() == path)
        {
            return component;
        }
    }
    ADD_FAILURE() << "no component " << path << " in " << input;
    return json{};
}

/// The mismatches between @p object and its golden entry; the radius is read first, as the
/// harness settles it.
[[nodiscard]] std::vector<std::string> goldenMismatches(const MassObject& object,
                                                        const json&       golden)
{
    GoldenCheck check{golden};
    check.number("/details/radius", object.getRadius());
    check.string("/name", object.getName());
    check.number("/length", object.getLength());
    check.number("/componentMass", object.getComponentMass());
    check.coordinate("/componentCG", object.getComponentCG());
    check.number("/longitudinalUnitInertia", object.getLongitudinalUnitInertia());
    check.number("/rotationalUnitInertia", object.getRotationalUnitInertia());
    check.number("/mass", object.getMass());
    check.coordinate("/cg", object.getCG());
    check.number("/longitudinalInertia", object.getLongitudinalInertia());
    check.number("/rotationalInertia", object.getRotationalInertia());
    check.number("/details/radialPosition", object.getRadialPosition());
    check.number("/details/radialDirection", object.getRadialDirection());
    check.boolean("/isAerodynamic", object.isAerodynamic());
    check.boolean("/isMassive", object.isMassive());
    check.coordinates("/componentBounds", object.getComponentBounds());
    check.integer("/instanceCount", object.getInstanceCount());
    check.coordinates("/instanceOffsets", object.getInstanceOffsets());
    return check.failures();
}

/// No mismatch.
[[nodiscard]] std::vector<std::string> none()
{
    return {};
}

TEST(MassObjectGolden, EstesAlphaIIIParachute)
{
    // TestRockets.makeEstesAlphaIII(): a default parachute at TOP 0.028 m in the body tube,
    // with an override mass of 2 g.
    Rocket rocket;
    auto&  stage = rocket.addChild(std::make_unique<AxialStage>());
    stage.addChild(TestBodyComponent::make(0.07, 0.012, ComponentKind::NOSE_CONE));
    auto& body = stage.addChild(TestBodyComponent::make(0.20, 0.012));
    body.setInnerRadius(0.012 - 0.0003);
    auto chute = std::make_unique<Parachute>();
    chute->setAxialMethod(AxialMethod::TOP);
    chute->setName("Parachute");
    chute->setAxialOffset(0.028);
    chute->setOverrideMass(0.002);
    chute->setMassOverridden(true);
    const Parachute& added = body.addChild(std::move(chute));
    rocket.enableEvents();

    const json golden = goldenComponent("testrocket-estes-alpha-iii", "/0/1/3");
    EXPECT_EQ(goldenMismatches(added, golden), none());
    GoldenCheck check{golden};
    check.number("/details/diameter", added.getDiameter());
    check.number("/details/cd", added.getCD());
    check.integer("/details/lineCount", added.getLineCount());
    check.number("/details/lineLength", added.getLineLength());
    check.string("/details/material/name", added.getMaterial().getName());
    check.number("/details/material/density", added.getMaterial().getDensity());
    check.coordinates("/componentLocations", added.getComponentLocations());
    EXPECT_EQ(check.failures(), none());
}

TEST(MassObjectGolden, Falcon9HeavyShockCord)
{
    // TestRockets.makeFalcon9Heavy(): a shock cord at BOTTOM 0 with a cord length of 0.4 m.
    auto cord = std::make_unique<ShockCord>();
    cord->setName("Shock Cord");
    cord->setAxialMethod(AxialMethod::BOTTOM);
    cord->setAxialOffset(0.0);
    cord->setCordLength(0.4);

    const json golden = goldenComponent("testrocket-falcon-9-heavy", "/0/3/1");
    EXPECT_EQ(goldenMismatches(*cord, golden), none());
    GoldenCheck check{golden};
    check.number("/details/cordLength", cord->getCordLength());
    check.string("/details/material/name", cord->getMaterial().getName());
    check.number("/details/material/density", cord->getMaterial().getDensity());
    EXPECT_EQ(check.failures(), none());
}

TEST(MassObjectGolden, IsoHaisuMassComponents)
{
    // TestRockets.makeIsoHaisu(): mass components made with (length, radius, mass).
    const MassComponent parachute{0.05, 0.05, 0.280};
    EXPECT_EQ(goldenMismatches(parachute, goldenComponent("testrocket-iso-haisu", "/0/1/2")),
              none());
    const MassComponent cord{0.05, 0.05, 0.125};
    EXPECT_EQ(goldenMismatches(cord, goldenComponent("testrocket-iso-haisu", "/0/1/3")), none());
    const MassComponent payload{0.40, 0.07, 1.500};
    EXPECT_EQ(goldenMismatches(payload, goldenComponent("testrocket-iso-haisu", "/0/1/4")), none());
    const MassComponent parachute2{0.1, 0.05, 0.028};
    EXPECT_EQ(goldenMismatches(parachute2, goldenComponent("testrocket-iso-haisu", "/0/2/1")),
              none());
    const MassComponent cord2{0.1, 0.05, 0.125};
    EXPECT_EQ(goldenMismatches(cord2, goldenComponent("testrocket-iso-haisu", "/0/2/3")), none());
}

}  // namespace
