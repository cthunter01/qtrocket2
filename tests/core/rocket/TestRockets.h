#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <numbers>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "rocket/TestBodyComponent.h"
#include "rocket/TestComponent.h"
#include "rocket/TestMotorMount.h"

/// The test motors and rockets of OpenRocket's TestRockets, built from the test doubles
/// (TestComponent, TestBodyComponent, TestMotorMount) until the concrete components are ported.
namespace QtRocket::Test
{

// ============================================================================ test motors

/// A single-use motor with a triangular thrust curve over {0, 1, 2} s, built as TestRockets'
/// generateMotor_*() do.
[[nodiscard]] inline std::shared_ptr<const ThrustCurveMotor> makeTestMotor(
    const std::string& manufacturer, const std::string& designation, const std::string& caseInfo,
    std::vector<double> delays, double diameter, double length, double peakThrust,
    double launchMass, double midMass, double burnoutMass)
{
    ThrustCurveMotor::Builder builder;
    builder.setManufacturer(Manufacturer::getManufacturer(manufacturer))
        .setDesignation(designation)
        .setDescription("Desc")
        .setCaseInfo(caseInfo)
        .setMotorType(Motor::Type::SINGLE)
        .setStandardDelays(std::move(delays))
        .setDiameter(diameter)
        .setLength(length)
        .setTimePoints({0, 1, 2})
        .setThrustPoints({0, peakThrust, 0})
        .setCGPoints({Coordinate{length / 2, 0, 0, launchMass},
                      Coordinate{length / 2, 0, 0, midMass},
                      Coordinate{length / 2, 0, 0, burnoutMass}})
        .setDigest("digest " + designation + " test");
    auto built = builder.build();
    if (!built)
    {
        bug("test motor " + designation + ": " + built.error().toString());
    }
    return std::make_shared<const ThrustCurveMotor>(std::move(*built));
}

/// TestRockets.generateMotor_M1350_75mm().
[[nodiscard]] inline std::shared_ptr<const ThrustCurveMotor> motorM1350()
{
    return makeTestMotor("AeroTech", "M1350", "SU 75/512", {}, 0.075, 0.622, 1357, 4.808, 3.389,
                         1.970);
}

/// TestRockets.generateMotor_G77_29mm().
[[nodiscard]] inline std::shared_ptr<const ThrustCurveMotor> motorG77()
{
    return makeTestMotor("AeroTech", "G77", "SU 29/180", {4, 7, 10}, 0.029, 0.124, 20, 0.123,
                         0.0935, 0.064);
}

/// TestRockets.generateMotor_A8_18mm() (triangular curve).
[[nodiscard]] inline std::shared_ptr<const ThrustCurveMotor> motorA8()
{
    return makeTestMotor("Estes", "A8", "SU 18.0x70.0", {0, 3, 5}, 0.018, 0.070, 9, 0.0164, 0.0145,
                         0.0131);
}

/// TestRockets.generateMotor_B4_18mm().
[[nodiscard]] inline std::shared_ptr<const ThrustCurveMotor> motorB4()
{
    return makeTestMotor("Estes", "B4", "SU 18.0x70.0", {0, 3, 5}, 0.018, 0.070, 11.4, 0.0195,
                         0.0155, 0.013);
}

/// TestRockets.generateMotor_C6_18mm() (with a triangular curve instead of the five points).
[[nodiscard]] inline std::shared_ptr<const ThrustCurveMotor> motorC6()
{
    return makeTestMotor("Estes", "C6", "SU 18.0x70.0", {0, 3, 5, 7}, 0.018, 0.070, 12, 0.0227,
                         0.0165, 0.0102);
}

/// TestRockets.generateMotor_D21_18mm().
[[nodiscard]] inline std::shared_ptr<const ThrustCurveMotor> motorD21()
{
    return makeTestMotor("AeroTech", "D21", "SU 18.0x70.0", {}, 0.018, 0.070, 32, 0.025, 0.020,
                         0.0154);
}

// ============================================================================ test rockets

/// TestRockets' TEST_FCID_0 ... TEST_FCID_4.
[[nodiscard]] inline FlightConfigurationId testFcid(int n)
{
    static constexpr std::array<std::string_view, 5> kKeys{
        "d010716e-ce0e-469d-ae46-190f3653ebbf", "f41bee5b-ebb8-4d92-bce7-53001577a313",
        "3e8d1280-53c2-4234-89a7-de215ef5cd69", "415a5485-f2da-4c2a-8803-394220ae58b8",
        "5abc18ec-a200-46f1-90c4-60b6995fc933"};
    return FlightConfigurationId::fromString(kKeys.at(static_cast<std::size_t>(n)));
}

/// A component that is not aerodynamic (an internal one), of @p kind, @p length, positioned by
/// @p method at @p offset.
[[nodiscard]] inline std::unique_ptr<TestComponent> makeInternal(ComponentKind kind, double length,
                                                                 AxialMethod method, double offset)
{
    auto component = TestComponent::make(length, kind, method);
    component->setAerodynamic(false);
    component->setAxialOffset(method, offset);
    return component;
}

/// A fin set of @p count fins of root chord @p rootChord on a body of radius @p bodyRadius,
/// positioned BOTTOM (the fins' instance offsets are on the body's surface, at 2 pi i / count).
[[nodiscard]] inline std::unique_ptr<TestComponent> makeFins(int count, double rootChord,
                                                             double bodyRadius)
{
    auto fins =
        TestComponent::make(rootChord, ComponentKind::TRAPEZOID_FIN_SET, AxialMethod::BOTTOM);
    std::vector<Coordinate> offsets;
    std::vector<double>     angles;
    for (int i = 0; i < count; i++)
    {
        const double angle = 2 * std::numbers::pi * i / count;
        offsets.emplace_back(0, bodyRadius * std::cos(angle), bodyRadius * std::sin(angle));
        angles.push_back(angle);
    }
    fins->setInstances(std::move(offsets), std::move(angles));
    return fins;
}

/// TestRockets.makeEstesAlphaIII() from test doubles, with the same dimensions: a stage with a
/// nose cone (0.07 m, radius 0.012 m) and a body tube (0.2 m) holding fins, a launch lug, a motor
/// mount inner tube (with an engine block, and a motor in each of the five test
/// configurations), a parachute and two centering rings. Events are enabled.
struct TestEstesAlphaIII
{
    std::unique_ptr<Rocket> rocket = std::make_unique<Rocket>();
    AxialStage*             stage{nullptr};
    TestBodyComponent*      nose{nullptr};
    TestBodyComponent*      body{nullptr};
    TestComponent*          fins{nullptr};
    TestComponent*          lug{nullptr};
    TestMotorMount*         inner{nullptr};
    TestComponent*          chute{nullptr};
    TestComponent*          rings{nullptr};

    TestEstesAlphaIII()
    {
        for (int i = 0; i < 5; i++)
        {
            rocket->createFlightConfiguration(testFcid(i));
        }
        rocket->setName("Estes Alpha III / Code Verification Rocket");
        stage = &rocket->addChild(std::make_unique<AxialStage>());
        stage->setName("Stage");

        nose = &stage->addChild(TestBodyComponent::make(0.07, 0.012, ComponentKind::NOSE_CONE));
        nose->setForeAftRadii(0, 0.012);
        nose->setName("Nose Cone");

        body = &stage->addChild(TestBodyComponent::make(0.20, 0.012));
        body->setInnerRadius(0.012 - 0.0003);
        body->setName("Body Tube");

        fins = &body->addChild(makeFins(3, 0.05, 0.012));
        fins->setName("3 Fin Set");

        lug =
            &body->addChild(TestComponent::make(0.05, ComponentKind::LAUNCH_LUG, AxialMethod::TOP));
        lug->setAxialOffset(AxialMethod::TOP, 0.111);
        lug->setName("Launch Lugs");

        inner = &body->addChild(
            TestMotorMount::make(0.07, 0.009, ComponentKind::INNER_TUBE, AxialMethod::TOP));
        inner->setAxialOffset(AxialMethod::TOP, 0.133);
        inner->setInnerRadius(0.009 - 0.0003);
        inner->setAerodynamic(false);
        inner->setMotorMount(true);
        inner->setName("Motor Mount Tube");
        inner->addChild(makeInternal(ComponentKind::ENGINE_BLOCK, 0.005, AxialMethod::TOP, 0.0))
            .setName("Engine Block");
        inner->addMotor(testFcid(0), motorA8(), 0.0);
        inner->addMotor(testFcid(1), motorB4(), 3.0);
        inner->addMotor(testFcid(2), motorC6(), 3.0);
        inner->addMotor(testFcid(3), motorC6(), 5.0);
        inner->addMotor(testFcid(4), motorC6(), 7.0);

        chute =
            &body->addChild(makeInternal(ComponentKind::PARACHUTE, 0.025, AxialMethod::TOP, 0.028));
        chute->setName("Parachute");

        rings = &body->addChild(
            makeInternal(ComponentKind::CENTERING_RING, 0.006, AxialMethod::TOP, 0.14));
        rings->setInstances({Coordinate{0, 0, 0}, Coordinate{0.035, 0, 0}}, {0, 0});
        rings->setName("Centering Rings");

        rocket->enableEvents();
    }
};

/// TestRockets.makeBeta() from test doubles: the Estes Alpha III as the sustainer, plus a booster
/// stage with a body tube (0.06 m, radius 0.012 m) holding a coupler, fins, a motor mount (a
/// motor in TEST_FCID_1) and a launch lug, and a tail cone (0.005 m, radii 0.012 and 0.01 m).
/// TEST_FCID_1 is selected, with every stage active.
struct TestBeta : TestEstesAlphaIII
{
    AxialStage*        boosterStage{nullptr};
    TestBodyComponent* boosterBody{nullptr};
    TestMotorMount*    boosterMmt{nullptr};
    TestBodyComponent* boosterTail{nullptr};

    TestBeta()
    {
        rocket->setName("Kit-bash Beta");
        stage->setName("Sustainer Stage");
        body->setName("Sustainer Body Tube");

        boosterStage = &rocket->addChild(std::make_unique<AxialStage>());
        boosterStage->setName("Booster Stage");

        boosterBody = &boosterStage->addChild(TestBodyComponent::make(0.06, 0.012));
        boosterBody->setInnerRadius(0.012 - 0.0003);
        boosterBody->setName("Booster Body");
        boosterBody
            ->addChild(makeInternal(ComponentKind::TUBE_COUPLER, 0.03, AxialMethod::TOP, -0.015))
            .setName("Coupler");
        boosterBody->addChild(makeFins(3, 0.05, 0.012)).setName("Booster Fins");

        boosterMmt = &boosterBody->addChild(
            TestMotorMount::make(0.05, 0.019 / 2, ComponentKind::INNER_TUBE, AxialMethod::BOTTOM));
        boosterMmt->setAxialOffset(AxialMethod::BOTTOM, 0.005);
        boosterMmt->setInnerRadius(0.018 / 2);
        boosterMmt->setAerodynamic(false);
        boosterMmt->setMotorMount(true);
        boosterMmt->setName("Booster MMT");
        boosterMmt->addMotor(testFcid(1), motorD21());

        TestComponent& boosterLug = boosterBody->addChild(
            TestComponent::make(0.05, ComponentKind::LAUNCH_LUG, AxialMethod::TOP));
        boosterLug.setName("Launch Lugs");

        boosterTail = &boosterStage->addChild(
            TestBodyComponent::make(0.005, 0.012, ComponentKind::TRANSITION));
        boosterTail->setForeAftRadii(0.012, 0.01);
        boosterTail->setName("Booster Tail Cone");

        rocket->setSelectedConfiguration(testFcid(1));
        rocket->getSelectedConfiguration().setAllStages();
        rocket->enableEvents();
    }
};

/// TestRockets.makeFalcon9Heavy() from test doubles, with the same dimensions: a payload stage
/// (nose 0.118 m, fairing body 0.132 m, transition 0.014 m, upper stage body 0.18 m with a
/// parachute and a shock cord, interstage 0.12 m) and a core stage whose body (0.8 m, radius
/// 0.0385 m, an M1350 motor) holds a two-booster set on its surface; each booster has a nose
/// (0.08 m) and a body (0.8 m) holding a 4-motor cluster of inner tubes (G77 motors, overhang
/// 0.01234 m) and three fins. Its configuration is selected, with every stage active.
struct TestFalcon9Heavy
{
    std::unique_ptr<Rocket> rocket = std::make_unique<Rocket>();
    FlightConfigurationId fcid{FlightConfigurationId::fromString("test_config #1: [ M1350, G77]")};
    AxialStage*           payloadStage{nullptr};
    TestBodyComponent*    payloadNose{nullptr};
    TestBodyComponent*    payloadBody{nullptr};
    TestBodyComponent*    payloadTransition{nullptr};
    TestBodyComponent*    upperStageBody{nullptr};
    TestComponent*        parachute{nullptr};
    TestComponent*        shockCord{nullptr};
    TestBodyComponent*    interstage{nullptr};
    AxialStage*           coreStage{nullptr};
    TestMotorMount*       coreBody{nullptr};
    ParallelStage*        boosterStage{nullptr};
    TestBodyComponent*    boosterNose{nullptr};
    TestBodyComponent*    boosterBody{nullptr};
    TestMotorMount*       boosterMotorTubes{nullptr};
    TestComponent*        boosterFins{nullptr};

    TestFalcon9Heavy()
    {
        rocket->setName("Falcon9H Scale Rocket");
        rocket->createFlightConfiguration(fcid);
        rocket->setSelectedConfiguration(fcid);

        // ====== Payload Stage ======
        payloadStage = &rocket->addChild(std::make_unique<AxialStage>());
        payloadStage->setName("Payload Fairing Stage");

        payloadNose = &payloadStage->addChild(
            TestBodyComponent::make(0.118, 0.052, ComponentKind::NOSE_CONE));
        payloadNose->setForeAftRadii(0, 0.052);
        payloadNose->setName("PL Fairing Nose");

        payloadBody = &payloadStage->addChild(TestBodyComponent::make(0.132, 0.052));
        payloadBody->setName("PL Fairing Body");

        payloadTransition = &payloadStage->addChild(
            TestBodyComponent::make(0.014, 0.052, ComponentKind::TRANSITION));
        payloadTransition->setForeAftRadii(0.052, 0.0385);
        payloadTransition->setName("PL Fairing Transition");

        upperStageBody = &payloadStage->addChild(TestBodyComponent::make(0.18, 0.0385));
        upperStageBody->setName("Upper Stage Body");
        parachute = &upperStageBody->addChild(
            makeInternal(ComponentKind::PARACHUTE, 0.025, AxialMethod::MIDDLE, 0.0));
        parachute->setName("Parachute");
        shockCord = &upperStageBody->addChild(
            makeInternal(ComponentKind::SHOCK_CORD, 0.025, AxialMethod::BOTTOM, 0.0));
        shockCord->setName("Shock Cord");

        interstage = &payloadStage->addChild(TestBodyComponent::make(0.12, 0.0385));
        interstage->setName("Interstage");

        // ====== Core Stage ======
        coreStage = &rocket->addChild(std::make_unique<AxialStage>());
        coreStage->setName("Core Stage");

        coreBody = &coreStage->addChild(TestMotorMount::make(0.8, 0.0385));
        coreBody->setInnerRadius(0.0385 - 0.001);
        coreBody->setName("Core Stage Body");
        coreBody->setMotorMount(true);
        coreBody->addMotor(fcid, motorM1350());

        // ====== Booster Stage Set ======
        boosterStage = &coreBody->addChild(std::make_unique<ParallelStage>());
        boosterStage->setName("Booster Stage");
        boosterStage->setAxialMethod(AxialMethod::BOTTOM);
        boosterStage->setAxialOffset(0.0);
        boosterStage->setInstanceCount(2);
        boosterStage->setRadius(RadiusMethod::SURFACE, 0.0);
        boosterStage->setAngleMethod(AngleMethod::RELATIVE);

        boosterNose = &boosterStage->addChild(
            TestBodyComponent::make(0.08, 0.0385, ComponentKind::NOSE_CONE));
        boosterNose->setForeAftRadii(0, 0.0385);
        boosterNose->setName("Booster Nose");

        boosterBody = &boosterStage->addChild(TestBodyComponent::make(0.8, 0.0385));
        boosterBody->setName("Booster Body");

        boosterMotorTubes = &boosterBody->addChild(
            TestMotorMount::make(0.15, 0.015, ComponentKind::INNER_TUBE, AxialMethod::BOTTOM));
        boosterMotorTubes->setName("Booster Motor Tubes");
        boosterMotorTubes->setInnerRadius(0.015 - 0.0005);
        boosterMotorTubes->setAerodynamic(false);
        // The 4-ring cluster of 29 mm tubes (ClusterConfiguration.CONFIGURATIONS[5], scale 1).
        boosterMotorTubes->setInstances(
            {Coordinate{0, -0.015, 0.015}, Coordinate{0, 0.015, 0.015},
             Coordinate{0, 0.015, -0.015}, Coordinate{0, -0.015, -0.015}},
            {0, 0, 0, 0});
        boosterMotorTubes->setMotorCount(4);
        boosterMotorTubes->addMotor(fcid, motorG77());
        boosterMotorTubes->setMotorOverhang(0.01234);

        boosterFins = &boosterBody->addChild(makeFins(3, 0.32, 0.0385));
        boosterFins->setName("Booster Fins");

        rocket->enableEvents();
        rocket->setSelectedConfiguration(fcid);
        rocket->getFlightConfiguration(fcid).setAllStages();
    }
};

}  // namespace QtRocket::Test
