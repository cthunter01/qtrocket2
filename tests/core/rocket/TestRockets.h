#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialPreferences.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/ClusterConfiguration.h"
#include "QtRocket/rocket/EngineBlock.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/ShockCord.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/TubeCoupler.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"

/// The test motors and rockets of OpenRocket's TestRockets (core/src/main/java/.../util/
/// TestRockets.java), built call for call as the Java makers build them, from the real
/// components: NoseCone, BodyTube, Transition, TrapezoidFinSet, LaunchLug, InnerTube,
/// EngineBlock, CenteringRing, TubeCoupler, Parachute, ShockCord and the real MotorMount API.
///
/// Where Java asks the application preferences for a material (the default material of a new
/// component, and makeEstesAlphaIII()'s getDefaultComponentMaterial(null, BULK)), the rockets
/// have the built-in bulk default, "Cardboard": what OpenRocket's test preferences give, and so
/// what the values OpenRocket's tests pin are computed with.
namespace QtRocket::Test
{

// ============================================================================ test motors

/// A single-use motor whose CG stays at half its @p length, built as TestRockets'
/// generateMotor_*() do: the thrust curve @p thrust over @p time, with the masses @p masses.
[[nodiscard]] inline std::shared_ptr<const ThrustCurveMotor> makeTestMotor(
    const std::string& manufacturer, const std::string& designation, const std::string& description,
    const std::string& caseInfo, std::vector<double> delays, double diameter, double length,
    std::vector<double> time, std::vector<double> thrust, const std::vector<double>& masses)
{
    std::vector<Coordinate> cg;
    cg.reserve(masses.size());
    for (const double mass : masses)
    {
        cg.emplace_back(length / 2, 0, 0, mass);
    }
    ThrustCurveMotor::Builder builder;
    builder.setManufacturer(Manufacturer::getManufacturer(manufacturer))
        .setDesignation(designation)
        .setDescription(description)
        .setCaseInfo(caseInfo)
        .setMotorType(Motor::Type::SINGLE)
        .setStandardDelays(std::move(delays))
        .setDiameter(diameter)
        .setLength(length)
        .setTimePoints(std::move(time))
        .setThrustPoints(std::move(thrust))
        .setCGPoints(std::move(cg))
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
    return makeTestMotor("AeroTech", "M1350", "Desc", "SU 75/512", {}, 0.075, 0.622, {0, 1, 2},
                         {0, 1357, 0}, {4.808, 3.389, 1.970});
}

/// TestRockets.generateMotor_G77_29mm().
[[nodiscard]] inline std::shared_ptr<const ThrustCurveMotor> motorG77()
{
    return makeTestMotor("AeroTech", "G77", "Desc", "SU 29/180", {4, 7, 10}, 0.029, 0.124,
                         {0, 1, 2}, {0, 20, 0}, {0.123, 0.0935, 0.064});
}

/// TestRockets.generateMotor_A8_18mm().
[[nodiscard]] inline std::shared_ptr<const ThrustCurveMotor> motorA8()
{
    return makeTestMotor("Estes", "A8", " SU Black Powder", "SU 18.0x70.0", {0, 3, 5}, 0.018, 0.070,
                         {0, 1, 2}, {0, 9, 0}, {0.0164, 0.0145, 0.0131});
}

/// TestRockets.generateMotor_B4_18mm().
[[nodiscard]] inline std::shared_ptr<const ThrustCurveMotor> motorB4()
{
    return makeTestMotor("Estes", "B4", " SU Black Powder", "SU 18.0x70.0", {0, 3, 5}, 0.018, 0.070,
                         {0, 1, 2}, {0, 11.4, 0}, {0.0195, 0.0155, 0.013});
}

/// TestRockets.generateMotor_C6_18mm().
[[nodiscard]] inline std::shared_ptr<const ThrustCurveMotor> motorC6()
{
    return makeTestMotor("Estes", "C6", " SU Black Powder", "SU 18.0x70.0", {0, 3, 5, 7}, 0.018,
                         0.070, {0, 0.2, 0.4, 2.0, 2.1}, {0, 12, 5, 5, 0},
                         {0.0227, 0.0165, 0.0165, 0.013, 0.012});
}

/// TestRockets.generateMotor_D21_18mm().
[[nodiscard]] inline std::shared_ptr<const ThrustCurveMotor> motorD21()
{
    return makeTestMotor("AeroTech", "D21", "Desc", "SU 18.0x70.0", {}, 0.018, 0.070, {0, 1, 2},
                         {0, 32, 0}, {0.025, 0.020, 0.0154});
}

/// Gives @p mount a motor configuration for @p fcid with @p motor and @p ejectionDelay, as
/// TestRockets does (new MotorConfiguration(mount, fcid), setMotor(), setEjectionDelay(),
/// mount.setMotorConfig()), which also makes the mount act as one. Returns the stored
/// configuration.
inline MotorConfiguration& addMotor(MotorMount& mount, const FlightConfigurationId& fcid,
                                    std::shared_ptr<const Motor> motor, double ejectionDelay = 0.0)
{
    MotorConfiguration config{mount, fcid};
    config.setMotor(std::move(motor));
    config.setEjectionDelay(ejectionDelay);
    mount.setMotorConfig(std::move(config), fcid);
    return mount.getMotorConfig(fcid);
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

/// TestRockets.makeEstesAlphaIII(): a stage with an ogive nose cone (0.07 m, base radius
/// 0.012 m, an aft shoulder) and a body tube (0.2 m, wall 0.3 mm) holding three trapezoidal fins
/// (root chord 0.05 m, tip chord 0.03 m, sweep 0.02 m, height 0.05 m, 3.2 mm thick, at the
/// BOTTOM), a launch lug (0.05 m, radii 3 and 2 mm, TOP 0.111 m), a motor mount inner tube (with an
/// engine block, and a motor in each of the five test configurations), a parachute whose mass is
/// overridden and two centering rings. The default configuration stays selected; events are
/// enabled.
struct TestEstesAlphaIII
{
    std::unique_ptr<Rocket> rocket = std::make_unique<Rocket>();
    AxialStage*             stage{nullptr};
    NoseCone*               nose{nullptr};
    BodyTube*               body{nullptr};
    TrapezoidFinSet*        fins{nullptr};
    LaunchLug*              lug{nullptr};
    InnerTube*              inner{nullptr};
    EngineBlock*            block{nullptr};
    Parachute*              chute{nullptr};
    CenteringRing*          rings{nullptr};

    TestEstesAlphaIII()
    {
        for (int i = 0; i < 5; i++)
        {
            rocket->createFlightConfiguration(testFcid(i));
        }

        rocket->setName("Estes Alpha III / Code Verification Rocket");
        auto newStage = std::make_unique<AxialStage>();
        newStage->setName("Stage");
        stage = &rocket->addChild(std::move(newStage));

        const double noseconeLength = 0.07;
        const double noseconeRadius = 0.012;
        auto         nosecone =
            std::make_unique<NoseCone>(TransitionShape::OGIVE, noseconeLength, noseconeRadius);
        nosecone->setAftShoulderLength(0.02);
        // A thickness of 0 (setAftShoulderLength() changed it).
        nosecone->setAftShoulderThickness(0);
        nosecone->setAftShoulderRadius(0.011);
        nosecone->setName("Nose Cone");
        nose = &stage->addChild(std::move(nosecone));

        const double bodytubeLength    = 0.20;
        const double bodytubeRadius    = 0.012;
        const double bodytubeThickness = 0.0003;
        auto         bodytube =
            std::make_unique<BodyTube>(bodytubeLength, bodytubeRadius, bodytubeThickness);
        bodytube->setName("Body Tube");
        body = &stage->addChild(std::move(bodytube));

        addFinsLugAndMotorMount();
        addRecoveryAndRings();

        const Material material = builtinDefaultComponentMaterial(Material::Type::BULK);
        nose->setMaterial(material);
        body->setMaterial(material);
        fins->setMaterial(material);

        // The default configuration of the rocket stays as it was initialised.
        rocket->enableEvents();
    }

private:
    /// The fin set, the launch lug and the motor mount with its engine block and motors.
    void addFinsLugAndMotorMount()
    {
        const int    finCount     = 3;
        const double finRootChord = 0.05;
        const double finTipChord  = 0.03;
        const double finSweep     = 0.02;
        const double finHeight    = 0.05;
        auto         finset = std::make_unique<TrapezoidFinSet>(finCount, finRootChord, finTipChord,
                                                                finSweep, finHeight);
        finset->setThickness(0.0032);
        finset->setAxialMethod(AxialMethod::BOTTOM);
        finset->setName("3 Fin Set");
        fins = &body->addChild(std::move(finset));

        auto launchLug = std::make_unique<LaunchLug>();
        launchLug->setName("Launch Lugs");
        launchLug->setAxialMethod(AxialMethod::TOP);
        launchLug->setAxialOffset(0.111);
        launchLug->setLength(0.050);
        launchLug->setOuterRadius(0.0022);
        launchLug->setInnerRadius(0.0020);
        lug = &body->addChild(std::move(launchLug));

        auto innerTube = std::make_unique<InnerTube>();
        innerTube->setAxialMethod(AxialMethod::TOP);
        innerTube->setAxialOffset(0.133);
        innerTube->setLength(0.07);
        innerTube->setOuterRadius(0.009);
        innerTube->setThickness(0.0003);
        innerTube->setMotorMount(true);
        innerTube->setName("Motor Mount Tube");
        inner = &body->addChild(std::move(innerTube));

        auto thrustBlock = std::make_unique<EngineBlock>();
        thrustBlock->setAxialMethod(AxialMethod::TOP);
        thrustBlock->setAxialOffset(0.0);
        thrustBlock->setLength(0.005);
        thrustBlock->setOuterRadius(0.009);
        thrustBlock->setThickness(0.0008);
        thrustBlock->setName("Engine Block");
        block = &inner->addChild(std::move(thrustBlock));
        inner->setMotorMount(true);

        {
            MotorConfiguration motorConfig{*inner, testFcid(0)};
            motorConfig.setMotor(motorA8());
            motorConfig.setEjectionDelay(0.0);
            inner->setMotorConfig(std::move(motorConfig), testFcid(0));
        }
        {
            MotorConfiguration motorConfig{*inner, testFcid(1)};
            motorConfig.setMotor(motorB4());
            motorConfig.setEjectionDelay(3.0);
            inner->setMotorConfig(std::move(motorConfig), testFcid(1));
        }
        addC6(testFcid(2), 3.0);
        addC6(testFcid(3), 5.0);
        addC6(testFcid(4), 7.0);
    }

    /// A C6 with @p ejectionDelay in the motor mount for @p fcid (the delay is set first, as in
    /// Java).
    void addC6(const FlightConfigurationId& fcid, double ejectionDelay) const
    {
        MotorConfiguration motorConfig{*inner, fcid};
        motorConfig.setEjectionDelay(ejectionDelay);
        motorConfig.setMotor(motorC6());
        inner->setMotorConfig(std::move(motorConfig), fcid);
    }

    /// The parachute and the centering rings.
    void addRecoveryAndRings()
    {
        auto parachute = std::make_unique<Parachute>();
        parachute->setAxialMethod(AxialMethod::TOP);
        parachute->setName("Parachute");
        parachute->setAxialOffset(0.028);
        parachute->setOverrideMass(0.002);
        parachute->setMassOverridden(true);
        chute = &body->addChild(std::move(parachute));

        // "bulkhead x2"
        auto centerings = std::make_unique<CenteringRing>();
        centerings->setName("Centering Rings");
        centerings->setAxialMethod(AxialMethod::TOP);
        centerings->setAxialOffset(0.14);
        centerings->setLength(0.006);
        centerings->setInstanceCount(2);
        centerings->setInstanceSeparation(0.035);
        rings = &body->addChild(std::move(centerings));
    }
};

/// TestRockets.makeBeta(): the Estes Alpha III as the sustainer, plus a booster stage with a
/// body tube (0.06 m, the sustainer's radius and wall) holding a coupler, the booster fins, a
/// motor mount (a D21 in TEST_FCID_1) and a launch lug, and a tail cone (0.005 m, radii 0.012
/// and 0.01 m). TEST_FCID_1 is selected, with every stage active.
struct TestBeta : TestEstesAlphaIII
{
    AxialStage*      boosterStage{nullptr};
    BodyTube*        boosterBody{nullptr};
    TubeCoupler*     coupler{nullptr};
    TrapezoidFinSet* boosterFins{nullptr};
    InnerTube*       boosterMmt{nullptr};
    LaunchLug*       boosterLug{nullptr};
    Transition*      boosterTail{nullptr};

    TestBeta()
    {
        rocket->setName("Kit-bash Beta");

        stage->setName("Sustainer Stage");
        body->setName("Sustainer Body Tube");
        const double sustainerRadius    = body->getAftRadius();
        const double sustainerThickness = body->getThickness();

        auto newBoosterStage = std::make_unique<AxialStage>();
        newBoosterStage->setName("Booster Stage");
        boosterStage = &rocket->addChild(std::move(newBoosterStage));

        auto newBoosterBody = std::make_unique<BodyTube>(0.06, sustainerRadius, sustainerThickness);
        newBoosterBody->setName("Booster Body");
        boosterBody = &boosterStage->addChild(std::move(newBoosterBody));

        addBoosterInternals(sustainerThickness);

        // Tail Cone
        auto tail = std::make_unique<Transition>();
        tail->setForeRadius(0.012);
        tail->setAftRadius(0.01);
        tail->setLength(0.005);
        tail->setName("Booster Tail Cone");
        boosterTail = &boosterStage->addChild(std::move(tail));

        rocket->setSelectedConfiguration(testFcid(1));
        rocket->getSelectedConfiguration().setAllStages();
        rocket->enableEvents();
    }

private:
    /// The coupler, the fins, the motor mount and the launch lug of the booster body.
    void addBoosterInternals(double sustainerThickness)
    {
        auto newCoupler = std::make_unique<TubeCoupler>();
        newCoupler->setName("Coupler");
        newCoupler->setOuterRadiusAutomatic(true);
        newCoupler->setThickness(sustainerThickness);
        newCoupler->setLength(0.03);
        newCoupler->setAxialMethod(AxialMethod::TOP);
        newCoupler->setAxialOffset(-0.015);
        coupler = &boosterBody->addChild(std::move(newCoupler));

        const int    finCount     = 3;
        const double finRootChord = 0.05;
        const double finTipChord  = 0.03;
        const double finSweep     = 0.02;
        const double finHeight    = 0.05;
        auto         finset = std::make_unique<TrapezoidFinSet>(finCount, finRootChord, finTipChord,
                                                                finSweep, finHeight);
        finset->setName("Booster Fins");
        finset->setThickness(0.0032);
        finset->setAxialMethod(AxialMethod::BOTTOM);
        finset->setAxialOffset(0.0);
        boosterFins = &boosterBody->addChild(std::move(finset));

        // Motor mount
        auto mmt = std::make_unique<InnerTube>();
        mmt->setName("Booster MMT");
        mmt->setAxialOffset(0.005);
        mmt->setAxialMethod(AxialMethod::BOTTOM);
        mmt->setOuterRadius(0.019 / 2);
        mmt->setInnerRadius(0.018 / 2);
        mmt->setLength(0.05);
        mmt->setMotorMount(true);
        {
            MotorConfiguration motorConfig{*mmt, testFcid(1)};
            motorConfig.setMotor(motorD21());
            mmt->setMotorConfig(std::move(motorConfig), testFcid(1));
        }
        boosterMmt = &boosterBody->addChild(std::move(mmt));

        auto launchLug = std::make_unique<LaunchLug>();
        launchLug->setName("Launch Lugs");
        launchLug->setAxialMethod(AxialMethod::TOP);
        launchLug->setAxialOffset(0.0);
        launchLug->setLength(0.050);
        launchLug->setOuterRadius(0.0022);
        launchLug->setInnerRadius(0.0020);
        boosterLug = &boosterBody->addChild(std::move(launchLug));
    }
};

/// TestRockets.makeSimple2Stage(): two stages, each holding a body tube 0.1 m long with radius
/// 0.01 m and a 1 mm wall; TEST_FCID_0 selected with every stage active.
struct TestSimple2Stage
{
    std::unique_ptr<Rocket> rocket = std::make_unique<Rocket>();
    AxialStage*             sustainerStage{nullptr};
    BodyTube*               sustainerBody{nullptr};
    AxialStage*             boosterStage{nullptr};
    BodyTube*               boosterBody{nullptr};

    TestSimple2Stage()
    {
        rocket->createFlightConfiguration(testFcid(0));
        rocket->setName("Simple 2-Stage Rocket");

        const double bodytubeLength    = 0.10;
        const double bodytubeRadius    = 0.01;
        const double bodytubeThickness = 0.001;
        {
            auto stage = std::make_unique<AxialStage>();
            stage->setName("Sustainer Stage");
            sustainerStage = &rocket->addChild(std::move(stage));

            auto bodytube =
                std::make_unique<BodyTube>(bodytubeLength, bodytubeRadius, bodytubeThickness);
            bodytube->setName("Sustainer Body Tube");
            sustainerBody = &sustainerStage->addChild(std::move(bodytube));
        }
        {
            auto stage = std::make_unique<AxialStage>();
            stage->setName("Booster Stage");
            boosterStage = &rocket->addChild(std::move(stage));

            auto bodytube =
                std::make_unique<BodyTube>(bodytubeLength, bodytubeRadius, bodytubeThickness);
            bodytube->setName("Booster Body Tube");
            boosterBody = &boosterStage->addChild(std::move(bodytube));
        }

        rocket->setSelectedConfiguration(testFcid(0));
        rocket->getSelectedConfiguration().setAllStages();

        rocket->enableEvents();
    }
};

/// TestRockets.makeFalcon9Heavy(): a payload stage (a power-series nose 0.118 m, the fairing body
/// 0.132 m, a transition 0.014 m with automatic radii, the upper stage body 0.18 m with a
/// parachute and a shock cord, the interstage 0.12 m) and a core stage whose body (0.8 m, radius
/// 0.0385 m, an M1350 motor) holds a two-booster set on its surface; each booster has a nose
/// (0.08 m) and a body (0.8 m, automatic radius) holding a 4-ring cluster of inner tubes (G77
/// motors, overhang 0.01234 m) and three fins. Its configuration is selected, with every stage
/// active.
struct TestFalcon9Heavy
{
    std::unique_ptr<Rocket> rocket = std::make_unique<Rocket>();
    /// TestRockets.FALCON_9H_FCID_1.
    FlightConfigurationId fcid{FlightConfigurationId::fromString("test_config #1: [ M1350, G77]")};
    AxialStage*           payloadStage{nullptr};
    NoseCone*             payloadNose{nullptr};
    BodyTube*             payloadBody{nullptr};
    Transition*           payloadTransition{nullptr};
    BodyTube*             upperStageBody{nullptr};
    Parachute*            parachute{nullptr};
    ShockCord*            shockCord{nullptr};
    BodyTube*             interstage{nullptr};
    AxialStage*           coreStage{nullptr};
    BodyTube*             coreBody{nullptr};
    ParallelStage*        boosterStage{nullptr};
    NoseCone*             boosterNose{nullptr};
    BodyTube*             boosterBody{nullptr};
    InnerTube*            boosterMotorTubes{nullptr};
    TrapezoidFinSet*      boosterFins{nullptr};

    TestFalcon9Heavy()
    {
        rocket->setName("Falcon9H Scale Rocket");

        rocket->createFlightConfiguration(fcid);
        rocket->setSelectedConfiguration(fcid);

        addPayloadStage();
        addCoreStage();
        addBoosterStage();

        rocket->enableEvents();
        rocket->setSelectedConfiguration(fcid);
        rocket->getFlightConfiguration(fcid).setAllStages();
    }

private:
    // ====== Payload Stage ======
    void addPayloadStage()
    {
        auto stage = std::make_unique<AxialStage>();
        stage->setName("Payload Fairing Stage");
        payloadStage = &rocket->addChild(std::move(stage));

        auto fairingNose = std::make_unique<NoseCone>(TransitionShape::POWER, 0.118, 0.052);
        fairingNose->setName("PL Fairing Nose");
        fairingNose->setThickness(0.001);
        fairingNose->setShapeParameter(0.5);
        fairingNose->setAftShoulderRadius(0.051);
        fairingNose->setAftShoulderLength(0.02);
        fairingNose->setAftShoulderThickness(0.001);
        fairingNose->setAftShoulderCapped(false);
        payloadNose = &payloadStage->addChild(std::move(fairingNose));

        auto fairingBody = std::make_unique<BodyTube>(0.132, 0.052, 0.001);
        fairingBody->setName("PL Fairing Body");
        payloadBody = &payloadStage->addChild(std::move(fairingBody));

        auto fairingTail = std::make_unique<Transition>();
        fairingTail->setName("PL Fairing Transition");
        fairingTail->setLength(0.014);
        fairingTail->setThickness(0.002);
        fairingTail->setForeRadiusAutomatic(true);
        fairingTail->setAftRadiusAutomatic(true);
        payloadTransition = &payloadStage->addChild(std::move(fairingTail));

        auto upperBody = std::make_unique<BodyTube>(0.18, 0.0385, 0.001);
        upperBody->setName("Upper Stage Body");
        upperStageBody = &payloadStage->addChild(std::move(upperBody));

        // Parachute
        auto upperChute = std::make_unique<Parachute>();
        upperChute->setName("Parachute");
        upperChute->setAxialMethod(AxialMethod::MIDDLE);
        upperChute->setAxialOffset(0.0);
        upperChute->setDiameter(0.3);
        upperChute->setLineCount(6);
        upperChute->setLineLength(0.3);
        parachute = &upperStageBody->addChild(std::move(upperChute));

        // Cord
        auto cord = std::make_unique<ShockCord>();
        cord->setName("Shock Cord");
        cord->setAxialMethod(AxialMethod::BOTTOM);
        cord->setAxialOffset(0.0);
        cord->setCordLength(0.4);
        shockCord = &upperStageBody->addChild(std::move(cord));

        auto interstageBody = std::make_unique<BodyTube>(0.12, 0.0385, 0.001);
        interstageBody->setName("Interstage");
        interstage = &payloadStage->addChild(std::move(interstageBody));
    }

    // ====== Core Stage ======
    void addCoreStage()
    {
        auto stage = std::make_unique<AxialStage>();
        stage->setName("Core Stage");
        coreStage = &rocket->addChild(std::move(stage));

        // 74 mm inner dia
        auto body = std::make_unique<BodyTube>(0.8, 0.0385, 0.001);
        body->setName("Core Stage Body");
        body->setMotorMount(true);
        coreBody = &coreStage->addChild(std::move(body));

        MotorConfiguration coreMotorConfig{*coreBody, fcid};
        coreMotorConfig.setMotor(motorM1350());
        coreBody->setMotorMount(true);
        coreBody->setMotorConfig(std::move(coreMotorConfig), fcid);
    }

    // ====== Booster Stage Set ======
    void addBoosterStage()
    {
        auto boosters = std::make_unique<ParallelStage>();
        boosters->setName("Booster Stage");
        boosterStage = &coreBody->addChild(std::move(boosters));
        boosterStage->setAxialMethod(AxialMethod::BOTTOM);
        boosterStage->setAxialOffset(0.0);
        boosterStage->setInstanceCount(2);
        boosterStage->setRadius(RadiusMethod::SURFACE, 0.0);
        boosterStage->setAngleMethod(AngleMethod::RELATIVE);

        auto boosterCone = std::make_unique<NoseCone>(TransitionShape::POWER, 0.08, 0.0385);
        boosterCone->setShapeParameter(0.5);
        boosterCone->setName("Booster Nose");
        boosterCone->setThickness(0.002);
        boosterCone->setAftShoulderRadius(0.0375);
        boosterCone->setAftShoulderLength(0.02);
        boosterCone->setAftShoulderThickness(0.001);
        boosterCone->setAftShoulderCapped(false);
        boosterNose = &boosterStage->addChild(std::move(boosterCone));

        auto body = std::make_unique<BodyTube>(0.8, 0.0385, 0.001);
        body->setName("Booster Body");
        body->setOuterRadiusAutomatic(true);
        boosterBody = &boosterStage->addChild(std::move(body));

        auto motorTubes = std::make_unique<InnerTube>();
        motorTubes->setName("Booster Motor Tubes");
        motorTubes->setLength(0.15);
        motorTubes->setOuterRadius(0.015);  // => 29mm motors
        motorTubes->setThickness(0.0005);
        motorTubes->setClusterConfiguration(ClusterConfiguration::configurations()[5]);  // 4-ring
        motorTubes->setClusterScale(1.0);
        boosterMotorTubes = &boosterBody->addChild(std::move(motorTubes));

        MotorConfiguration boosterMotorConfig{*boosterMotorTubes, fcid};
        boosterMotorConfig.setMotor(motorG77());
        boosterMotorTubes->setMotorConfig(std::move(boosterMotorConfig), fcid);
        boosterMotorTubes->setMotorOverhang(0.01234);

        boosterFins = &boosterBody->addChild(std::make_unique<TrapezoidFinSet>());
        boosterFins->setName("Booster Fins");
        boosterFins->setFinCount(3);
        boosterFins->setThickness(0.003);
        boosterFins->setCrossSection(FinSet::CrossSection::ROUNDED);
        boosterFins->setRootChord(0.32);
        boosterFins->setTipChord(0.12);
        boosterFins->setHeight(0.10);
        boosterFins->setSweep(0.18);
        boosterFins->setAxialMethod(AxialMethod::BOTTOM);
        boosterFins->setAxialOffset(0.0);
    }
};

}  // namespace QtRocket::Test
