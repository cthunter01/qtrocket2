#include "rocket/TestRockets.h"

#include <array>
#include <concepts>
#include <cstddef>
#include <format>
#include <memory>
#include <numbers>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialPreferences.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Bulkhead.h"
#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/ClusterConfiguration.h"
#include "QtRocket/rocket/EngineBlock.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/InstanceMap.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/ShockCord.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/TubeCoupler.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket::Test
{

// ============================================================================ test motors

std::shared_ptr<const ThrustCurveMotor> makeTestMotor(
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

std::shared_ptr<const ThrustCurveMotor> motorM1350()
{
    return makeTestMotor("AeroTech", "M1350", "Desc", "SU 75/512", {}, 0.075, 0.622, {0, 1, 2},
                         {0, 1357, 0}, {4.808, 3.389, 1.970});
}

std::shared_ptr<const ThrustCurveMotor> motorG77()
{
    return makeTestMotor("AeroTech", "G77", "Desc", "SU 29/180", {4, 7, 10}, 0.029, 0.124,
                         {0, 1, 2}, {0, 20, 0}, {0.123, 0.0935, 0.064});
}

std::shared_ptr<const ThrustCurveMotor> motorA8()
{
    return makeTestMotor("Estes", "A8", " SU Black Powder", "SU 18.0x70.0", {0, 3, 5}, 0.018, 0.070,
                         {0, 1, 2}, {0, 9, 0}, {0.0164, 0.0145, 0.0131});
}

std::shared_ptr<const ThrustCurveMotor> motorA10()
{
    return makeTestMotor("Estes", "A10", " SU Black Powder", "SU 13.0x45.0", {0, 3}, 0.013, 0.045,
                         {0.0, 0.2, 0.3, 1.04, 1.05}, {0.0, 10, 1, 1, 0},
                         {0.011, 0.009, 0.008, 0.003, 0.003});
}

std::shared_ptr<const ThrustCurveMotor> motorB4()
{
    return makeTestMotor("Estes", "B4", " SU Black Powder", "SU 18.0x70.0", {0, 3, 5}, 0.018, 0.070,
                         {0, 1, 2}, {0, 11.4, 0}, {0.0195, 0.0155, 0.013});
}

std::shared_ptr<const ThrustCurveMotor> motorC6()
{
    return makeTestMotor("Estes", "C6", " SU Black Powder", "SU 18.0x70.0", {0, 3, 5, 7}, 0.018,
                         0.070, {0, 0.2, 0.4, 2.0, 2.1}, {0, 12, 5, 5, 0},
                         {0.0227, 0.0165, 0.0165, 0.013, 0.012});
}

std::shared_ptr<const ThrustCurveMotor> motorD21()
{
    return makeTestMotor("AeroTech", "D21", "Desc", "SU 18.0x70.0", {}, 0.018, 0.070, {0, 1, 2},
                         {0, 32, 0}, {0.025, 0.020, 0.0154});
}

MotorConfiguration& addMotor(MotorMount& mount, const FlightConfigurationId& fcid,
                             std::shared_ptr<const Motor> motor, double ejectionDelay)
{
    MotorConfiguration config{mount, fcid};
    config.setMotor(std::move(motor));
    config.setEjectionDelay(ejectionDelay);
    mount.setMotorConfig(std::move(config), fcid);
    return mount.getMotorConfig(fcid);
}

// ============================================================================ test rockets

namespace
{

/// Removes the child @p child from @p parent and hands it to the caller, with its own type: the
/// C++ form of Java's removeChild(), after which the maker goes on using its reference.
template <std::derived_from<RocketComponent> Component>
[[nodiscard]] std::unique_ptr<Component> takeChild(RocketComponent& parent, Component& child)
{
    std::unique_ptr<RocketComponent> removed = parent.removeChild(&child);
    QTROCKET_ASSERT(removed.get() == &child);
    return std::unique_ptr<Component>{dynamic_cast<Component*>(removed.release())};
}

}  // namespace

FlightConfigurationId testFcid(int n)
{
    static constexpr std::array<std::string_view, 5> kKeys{
        "d010716e-ce0e-469d-ae46-190f3653ebbf", "f41bee5b-ebb8-4d92-bce7-53001577a313",
        "3e8d1280-53c2-4234-89a7-de215ef5cd69", "415a5485-f2da-4c2a-8803-394220ae58b8",
        "5abc18ec-a200-46f1-90c4-60b6995fc933"};
    return FlightConfigurationId::fromString(kKeys.at(static_cast<std::size_t>(n)));
}

// ---------------------------------------------------------------------- makeEstesAlphaIII

TestEstesAlphaIII::TestEstesAlphaIII()
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
    auto bodytube = std::make_unique<BodyTube>(bodytubeLength, bodytubeRadius, bodytubeThickness);
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

void TestEstesAlphaIII::addFinsLugAndMotorMount()
{
    const int    finCount     = 3;
    const double finRootChord = 0.05;
    const double finTipChord  = 0.03;
    const double finSweep     = 0.02;
    const double finHeight    = 0.05;
    auto         finset =
        std::make_unique<TrapezoidFinSet>(finCount, finRootChord, finTipChord, finSweep, finHeight);
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

void TestEstesAlphaIII::addC6(const FlightConfigurationId& fcid, double ejectionDelay) const
{
    MotorConfiguration motorConfig{*inner, fcid};
    motorConfig.setEjectionDelay(ejectionDelay);
    motorConfig.setMotor(motorC6());
    inner->setMotorConfig(std::move(motorConfig), fcid);
}

void TestEstesAlphaIII::addRecoveryAndRings()
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

// ------------------------------------------------------------------------ splitRocketFins

std::unique_ptr<TrapezoidFinSet> splitRocketFins(BodyTube& body, TrapezoidFinSet& fins,
                                                 int finCount)
{
    // actually remove the fins
    std::unique_ptr<TrapezoidFinSet> templateFins = takeChild(body, fins);

    templateFins->setFinCount(1);
    // and manually add in the equivalent the others
    for (int finNumber = 1; finNumber < finCount; ++finNumber)
    {
        const double rootChord = templateFins->getRootChord();
        const double tipChord  = templateFins->getTipChord();
        const double sweep     = templateFins->getSweep();
        const double height    = templateFins->getHeight();
        auto singleFin = std::make_unique<TrapezoidFinSet>(1, rootChord, tipChord, sweep, height);
        singleFin->setAngleOffset(((finNumber * std::numbers::pi) * 2.0) / finCount);
        singleFin->setThickness(templateFins->getThickness());
        singleFin->setAxialMethod(templateFins->getAxialMethod());
        singleFin->setName(std::format("Single Fin #{}", finNumber));
        body.addChild(std::move(singleFin));
    }
    return templateFins;
}

// ------------------------------------------------------------------------------- makeBeta

TestBeta::TestBeta()
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

void TestBeta::addBoosterInternals(double sustainerThickness)
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
    auto         finset =
        std::make_unique<TrapezoidFinSet>(finCount, finRootChord, finTipChord, finSweep, finHeight);
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

// ----------------------------------------------------------------------- makeSimple2Stage

TestSimple2Stage::TestSimple2Stage()
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

// ---------------------------------------------------------------------------- makeBigBlue

TestBigBlue::TestBigBlue()
{
    auto newStage = std::make_unique<AxialStage>();
    newStage->setName("Stage1");

    auto nosecone = std::make_unique<NoseCone>(TransitionShape::ELLIPSOID, 0.105, 0.033);
    nosecone->setThickness(0.001);
    auto bodytube = std::make_unique<BodyTube>(0.69, 0.033, 0.001);

    auto                            finset = std::make_unique<FreeformFinSet>();
    const std::array<Coordinate, 5> finPoints{Coordinate{0, 0}, Coordinate{0.115, 0.072},
                                              Coordinate{0.255, 0.072}, Coordinate{0.255, 0.037},
                                              Coordinate{0.150, 0}};
    finset->setPoints(finPoints);

    finset->setThickness(0.003);
    finset->setFinCount(4);

    finset->setCantAngle((0 * std::numbers::pi) / 180);

    auto mcomp = std::make_unique<MassComponent>(0.2, 0.03, 0.045 + 0.060);
    mcomp->setAxialMethod(AxialMethod::TOP);
    mcomp->setAxialOffset(0);

    // Stage construction
    stage = &rocket->addChild(std::move(newStage));
    rocket->setPerfectFinish(false);

    // Component construction
    nose = &stage->addChild(std::move(nosecone));
    body = &stage->addChild(std::move(bodytube));

    fins = &body->addChild(std::move(finset));

    mass = &body->addChild(std::move(mcomp));

    rocket->enableEvents();
}

// --------------------------------------------------------------------------- makeIsoHaisu

namespace
{

/// The radius of the Iso-Haisu (Java: R).
constexpr double kIsoHaisuRadius = 0.07;

/// A mass component (@p length, @p radius, @p mass) at the TOP of its parent, @p offset aft.
[[nodiscard]] std::unique_ptr<MassComponent> isoHaisuMass(double length, double radius, double mass,
                                                          double offset)
{
    auto component = std::make_unique<MassComponent>(length, radius, mass);
    component->setAxialMethod(AxialMethod::TOP);
    component->setAxialOffset(offset);
    return component;
}

/// A centering ring of the Iso-Haisu's third tube (automatic radii, 5 mm, its mass overridden),
/// positioned by @p method at @p offset.
[[nodiscard]] std::unique_ptr<CenteringRing> isoHaisuCenter(AxialMethod method, double offset)
{
    auto center = std::make_unique<CenteringRing>();
    center->setInnerRadiusAutomatic(true);
    center->setOuterRadiusAutomatic(true);
    center->setLength(0.005);
    center->setMassOverridden(true);
    center->setOverrideMass(0.038);
    center->setAxialMethod(method);
    center->setAxialOffset(offset);
    return center;
}

/// A body tube of the Iso-Haisu (@p length, wall 5 mm) with its mass overridden to @p mass.
[[nodiscard]] std::unique_ptr<BodyTube> isoHaisuTube(double length, double mass)
{
    auto tube = std::make_unique<BodyTube>(length, kIsoHaisuRadius, 0.005);
    tube->setMassOverridden(true);
    tube->setOverrideMass(mass);
    return tube;
}

}  // namespace

TestIsoHaisu::TestIsoHaisu()
{
    // The stage is added to the rocket at the end: until then `stage` is a detached tree.
    auto newStage = std::make_unique<AxialStage>();
    newStage->setName("Stage1");
    stage = newStage.get();

    auto nosecone = std::make_unique<NoseCone>(TransitionShape::OGIVE, 0.53, kIsoHaisuRadius);
    nosecone->setThickness(0.005);
    nosecone->setMassOverridden(true);
    nosecone->setOverrideMass(0.588);
    nose = &stage->addChild(std::move(nosecone));

    tube1 = &stage->addChild(isoHaisuTube(0.505, 0.366));
    tube2 = &stage->addChild(isoHaisuTube(0.605, 0.427));
    tube3 = &stage->addChild(isoHaisuTube(1.065, 0.730));

    fillTube1();
    fillTube2();
    fillTube3();

    fins->setCantAngle((0 * std::numbers::pi) / 180);

    // Stage construction
    rocket->addChild(std::move(newStage));
    rocket->setPerfectFinish(false);

    FlightConfiguration& config = rocket->getSelectedConfiguration();

    config.setAllStages();
    rocket->enableEvents();
}

void TestIsoHaisu::fillTube1()
{
    lug = &tube1->addChild(std::make_unique<LaunchLug>());

    auto coupler = std::make_unique<TubeCoupler>();
    coupler->setOuterRadiusAutomatic(true);
    coupler->setThickness(0.005);
    coupler->setLength(0.28);
    coupler->setMassOverridden(true);
    coupler->setOverrideMass(0.360);
    coupler->setAxialMethod(AxialMethod::BOTTOM);
    coupler->setAxialOffset(-0.14);
    tube1Coupler = &tube1->addChild(std::move(coupler));

    // Parachute
    tube1Masses[0] = &tube1->addChild(isoHaisuMass(0.05, 0.05, 0.280, 0.2));

    // Cord
    tube1Masses[1] = &tube1->addChild(isoHaisuMass(0.05, 0.05, 0.125, 0.2));

    // Payload
    tube1Masses[2] = &tube1->addChild(isoHaisuMass(0.40, kIsoHaisuRadius, 1.500, 0.25));

    auto auxfinset = std::make_unique<TrapezoidFinSet>();
    auxfinset->setName("CONTROL");
    auxfinset->setFinCount(2);
    auxfinset->setRootChord(0.05);
    auxfinset->setTipChord(0.05);
    auxfinset->setHeight(0.10);
    auxfinset->setSweep(0);
    auxfinset->setThickness(0.008);
    auxfinset->setCrossSection(FinSet::CrossSection::AIRFOIL);
    auxfinset->setAxialMethod(AxialMethod::TOP);
    auxfinset->setAxialOffset(0.28);
    auxfinset->setAngleOffset(std::numbers::pi / 2);
    controlFins = &tube1->addChild(std::move(auxfinset));
}

void TestIsoHaisu::fillTube2()
{
    auto coupler = std::make_unique<TubeCoupler>();
    coupler->setOuterRadiusAutomatic(true);
    coupler->setLength(0.28);
    coupler->setAxialMethod(AxialMethod::TOP);
    coupler->setAxialOffset(0.47);
    coupler->setMassOverridden(true);
    coupler->setOverrideMass(0.360);
    tube2Coupler = &tube2->addChild(std::move(coupler));

    // Parachute
    tube2Masses[0] = &tube2->addChild(isoHaisuMass(0.1, 0.05, 0.028, 0.14));

    auto bulkhead = std::make_unique<Bulkhead>();
    bulkhead->setOuterRadiusAutomatic(true);
    bulkhead->setMassOverridden(true);
    bulkhead->setOverrideMass(0.050);
    bulkhead->setAxialMethod(AxialMethod::TOP);
    bulkhead->setAxialOffset(0.27);
    bulk = &tube2->addChild(std::move(bulkhead));

    // Chord
    tube2Masses[1] = &tube2->addChild(isoHaisuMass(0.1, 0.05, 0.125, 0.19));
}

void TestIsoHaisu::fillTube3()
{
    auto innerTube = std::make_unique<InnerTube>();
    innerTube->setOuterRadius(0.08 / 2);
    innerTube->setInnerRadius(0.0762 / 2);
    innerTube->setLength(0.86);
    innerTube->setMassOverridden(true);
    innerTube->setOverrideMass(0.388);
    inner = &tube3->addChild(std::move(innerTube));

    centers[0] = &tube3->addChild(isoHaisuCenter(AxialMethod::BOTTOM, 0));
    centers[1] = &tube3->addChild(isoHaisuCenter(AxialMethod::TOP, 0.28));
    centers[2] = &tube3->addChild(isoHaisuCenter(AxialMethod::TOP, 0.83));

    auto finset = std::make_unique<TrapezoidFinSet>();
    finset->setRootChord(0.495);
    finset->setTipChord(0.1);
    finset->setHeight(0.185);
    finset->setThickness(0.005);
    finset->setSweep(0.3);
    finset->setAxialMethod(AxialMethod::BOTTOM);
    finset->setAxialOffset(-0.03);
    finset->setAngleOffset(std::numbers::pi / 2);
    fins = &tube3->addChild(std::move(finset));
}

// ----------------------------------------------------------------------- makeFalcon9Heavy

TestFalcon9Heavy::TestFalcon9Heavy()
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

// ====== Payload Stage ======
void TestFalcon9Heavy::addPayloadStage()
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
void TestFalcon9Heavy::addCoreStage()
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
void TestFalcon9Heavy::addBoosterStage()
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

// ---------------------------------------------------------------------------- addCoreFins

TrapezoidFinSet& addCoreFins(Rocket& rocket)
{
    const int         bodyFinCount       = 4;
    const double      bodyFinRootChord   = 0.05;
    const double      bodyFinTipChord    = bodyFinRootChord;
    const double      bodyFinHeight      = 0.025;
    const double      bodyFinSweep       = 0.0;
    const AxialMethod bodyFinAxialMethod = AxialMethod::BOTTOM;

    auto finSet = std::make_unique<TrapezoidFinSet>(bodyFinCount, bodyFinRootChord, bodyFinTipChord,
                                                    bodyFinSweep, bodyFinHeight);
    finSet->setName("Body Tube FinSet");
    finSet->setAxialMethod(bodyFinAxialMethod);

    return rocket.getChild(1).getChild(0).addChild(std::move(finSet));
}

// ---------------------------------------------------------- makeMultiStageEventTestRocket

namespace
{

/// The dimensions makeMultiStageEventTestRocket() shares between its stages.
constexpr double kEventRocketThickness  = 0.002;
constexpr double kEventRocketCoreRadius = 0.01;

}  // namespace

TestMultiStageEventTestRocket::TestMultiStageEventTestRocket()
  : fcid(rocket->createFlightConfiguration(FlightConfigurationId{}).getId())
{
    addSustainer();
    addCenterBooster();
    addSideBoosters();

    rocket->enableEvents();
    rocket->setSelectedConfiguration(fcid);
}

// Sustainer
void TestMultiStageEventTestRocket::addSustainer()
{
    auto stage = std::make_unique<AxialStage>();
    stage->setName("Sustainer");
    sustainer = &rocket->addChild(std::move(stage));

    const double ncLength = 0.1;
    auto         noseCone =
        std::make_unique<NoseCone>(TransitionShape::OGIVE, ncLength, kEventRocketCoreRadius);
    noseCone->setName("Sustainer Nose Cone");
    sustainerNose = &sustainer->addChild(std::move(noseCone));

    const double btLength = 0.2;
    auto         bodyTube =
        std::make_unique<BodyTube>(btLength, kEventRocketCoreRadius, kEventRocketThickness);
    bodyTube->setName("Sustainer Body Tube");
    bodyTube->setMotorMount(true);
    sustainerBody = &sustainer->addChild(std::move(bodyTube));

    MotorConfiguration motorConfig{*sustainerBody, fcid};
    motorConfig.setMotor(motorC6());
    motorConfig.setEjectionDelay(5.0);
    motorConfig.setIgnitionEvent(IgnitionEvent::BURNOUT);
    sustainerBody->setMotorConfig(std::move(motorConfig), fcid);

    const double chuteDiam = 0.3;
    auto         parachute = std::make_unique<Parachute>();
    parachute->setName("Sustainer Parachute");
    parachute->setDiameter(chuteDiam);
    sustainerChute = &sustainerBody->addChild(std::move(parachute));
}

// Center Booster
void TestMultiStageEventTestRocket::addCenterBooster()
{
    const int    numFins   = 4;
    const double rootChord = 0.04;
    const double tipChord  = 0.02;
    const double sweep     = 0.015;
    const double height    = 0.03;

    auto stage = std::make_unique<AxialStage>();
    stage->setName("Center Booster");
    centerBooster = &rocket->addChild(std::move(stage));

    const double btLength = 0.09;

    auto bodyTube =
        std::make_unique<BodyTube>(btLength, kEventRocketCoreRadius, kEventRocketThickness);
    bodyTube->setName("Center Booster Body Tube");
    bodyTube->setMotorMount(true);
    centerBody = &centerBooster->addChild(std::move(bodyTube));

    MotorConfiguration motorConfig{*centerBody, fcid};
    motorConfig.setMotor(motorC6());
    motorConfig.setEjectionDelay(0.0);
    motorConfig.setIgnitionEvent(IgnitionEvent::LAUNCH);
    motorConfig.setIgnitionDelay(0.01);
    centerBody->setMotorConfig(std::move(motorConfig), fcid);

    const double position = -0.015;
    auto finSet = std::make_unique<TrapezoidFinSet>(numFins, rootChord, tipChord, sweep, height);
    finSet->setName("Center Booster Fin Set");
    finSet->setAxialMethod(AxialMethod::BOTTOM);
    finSet->setAxialOffset(position);
    centerFins = &centerBody->addChild(std::move(finSet));
}

// Side Boosters
void TestMultiStageEventTestRocket::addSideBoosters()
{
    const double sideRadius = 0.007;
    auto         boosters   = std::make_unique<ParallelStage>(2);
    boosters->setName("Side boosters");
    sideBoosters = &rocket->getChild(1).getChild(0).addChild(std::move(boosters));
    sideBoosters->setAngleOffset(std::numbers::pi / 4);

    const double ncLength = 0.05;
    auto noseCone = std::make_unique<NoseCone>(TransitionShape::OGIVE, ncLength, sideRadius);
    noseCone->setName("Side Booster Nose Cones");
    sideNose = &sideBoosters->addChild(std::move(noseCone));

    const double btLength = 0.09;
    auto         bodyTube = std::make_unique<BodyTube>(btLength, sideRadius, kEventRocketThickness);
    bodyTube->setName("Side Booster Body Tubes");
    bodyTube->setMotorMount(true);
    sideBody = &sideBoosters->addChild(std::move(bodyTube));

    const double chuteDiam = 0.3;
    auto         parachute = std::make_unique<Parachute>();
    parachute->setName("Side Chutes");
    parachute->setDiameter(chuteDiam);
    sideChute = &sideBody->addChild(std::move(parachute));

    MotorConfiguration motorConfig{*sideBody, fcid};
    motorConfig.setMotor(motorA10());
    motorConfig.setEjectionDelay(0.0);
    motorConfig.setIgnitionEvent(IgnitionEvent::LAUNCH);
    sideBody->setMotorConfig(std::move(motorConfig), fcid);

    StageSeparationConfiguration stageSepConfig;
    stageSepConfig.setSeparationEvent(StageSeparationConfiguration::SeparationEvent::BURNOUT);
    sideBoosters->getSeparationConfigurations().set(fcid, stageSepConfig);
}

// --------------------------------------------------------------------- makeEndPlateRocket

TestEndPlateRocket::TestEndPlateRocket()
{
    // rocket design parameters
    const double radius = 0.01;  // note this is diameter/2!

    // nose cone
    const double noseLength = 0.05;

    // body tube
    const double bodyWallThick = 0.002;
    const double bodyLength    = 0.254;

    // main body tube fins
    const int         bodyFinCount       = 4;
    const double      bodyFinRootChord   = 0.05;
    const double      bodyFinTipChord    = bodyFinRootChord;
    const double      bodyFinHeight      = 0.025;
    const double      bodyFinSweep       = 0.0;
    const AxialMethod bodyFinAxialMethod = AxialMethod::BOTTOM;
    const double      bodyFinAxialOffset = 0.0;
    const double      bodyFinThickness   = 0.003;

    // pods for end plates
    const int         podSetCount       = bodyFinCount;
    const double      podSetOffset      = bodyFinHeight;
    const AxialMethod podSetAxialMethod = bodyFinAxialMethod;
    const double      podSetAxialOffset = 0.0;

    // "phantom" tube on pods to give us somewhere to connect end plates
    const double phantomLength        = bodyFinTipChord;
    const double phantomRadius        = 0;
    const double phantomWallThickness = 0;

    // end plates
    const int         endPlateCount       = 2;
    const double      endPlateRootChord   = bodyFinTipChord;
    const double      endPlateTipChord    = endPlateRootChord;
    const double      endPlateSweep       = 0.0;
    const double      endPlateThickness   = bodyFinThickness;
    const double      endPlateHeight      = bodyFinHeight;
    const double      endPlateRotation    = std::numbers::pi / 2.0;
    const AxialMethod endPlateAxialMethod = AxialMethod::BOTTOM;
    const double      endPlateAxialOffset = 0;

    // create bare rocket
    rocket->enableEvents();
    rocket->setName("End Plate Test");

    // rocket has one stage
    sustainer = &rocket->addChild(std::make_unique<AxialStage>());
    sustainer->setName("Sustainer");

    // nose cone
    nose = &sustainer->addChild(
        std::make_unique<NoseCone>(TransitionShape::OGIVE, noseLength, radius));
    nose->setName("Nose Cone");

    // body tube
    body = &sustainer->addChild(std::make_unique<BodyTube>(bodyLength, radius, bodyWallThick));
    body->setName("Body tube");

    // Trapezoidal fin set on body tube
    bodyFins = &body->addChild(std::make_unique<TrapezoidFinSet>(
        bodyFinCount, bodyFinRootChord, bodyFinTipChord, bodyFinSweep, bodyFinHeight));
    bodyFins->setName("Body Tube FinSet");
    bodyFins->setAxialMethod(bodyFinAxialMethod);
    bodyFins->setAxialOffset(bodyFinAxialOffset);
    bodyFins->setThickness(bodyFinThickness);

    // Pod set to put an end plate on each fin
    podSet = &body->addChild(std::make_unique<PodSet>());
    podSet->setName("Pod Set");
    podSet->setInstanceCount(podSetCount);
    podSet->setRadiusOffset(podSetOffset);
    podSet->setAxialMethod(podSetAxialMethod);
    podSet->setAxialOffset(podSetAxialOffset);

    // 0-diameter "body tube" to give us something to hook the endplates to. Note that this
    // causes a "thick fins" warning.
    phantom = &podSet->addChild(
        std::make_unique<BodyTube>(phantomLength, phantomRadius, phantomWallThickness));
    phantom->setName("Phantom");

    // end plates
    endPlates = &phantom->addChild(std::make_unique<TrapezoidFinSet>(
        endPlateCount, endPlateRootChord, endPlateTipChord, endPlateSweep, endPlateHeight));
    endPlates->setName("End plates");
    endPlates->setAngleOffset(endPlateRotation);
    endPlates->setAxialMethod(endPlateAxialMethod);
    endPlates->setAxialOffset(endPlateAxialOffset);
    endPlates->setThickness(endPlateThickness);
}

// -------------------------------------------------------------- makeEstesAlphaIIIWithPods

TestEstesAlphaIIIWithPods::TestEstesAlphaIIIWithPods()
{
    // find the body and fins: the fin set among the active components (the Alpha III has one)
    const InstanceMap&     imap  = rocket->getSelectedConfiguration().getActiveInstances();
    const TrapezoidFinSet* found = nullptr;
    for (const RocketComponent* const component : imap.keys())
    {
        found = dynamic_cast<const TrapezoidFinSet*>(component);
        if (found != nullptr)
        {
            break;
        }
    }
    QTROCKET_ASSERT(found == fins);
    auto& finBody = dynamic_cast<BodyTube&>(*fins->getParent());
    // Java keeps its reference to the removed fin set; here the maker owns it until it is added
    // to the pod body.
    std::unique_ptr<TrapezoidFinSet> detached = takeChild(finBody, *fins);

    // create a PodSet to hook the fins to
    auto podset = std::make_unique<PodSet>();
    podset->setInstanceCount(detached->getFinCount());

    podSet = &finBody.addChild(std::move(podset));

    // put a phantom body tube on the pods
    auto newPodBody = std::make_unique<BodyTube>(detached->getRootChord(), 0);
    newPodBody->setName("Pod Body");
    podBody = &podSet->addChild(std::move(newPodBody));

    // change the number of fins to 1 and put the revised finset on the podbody
    detached->setFinCount(1);
    fins = &podBody->addChild(std::move(detached));
}

// --------------------------------------------------------- makeEstesAlphaIIIWithMotorPods

TestEstesAlphaIIIWithMotorPods::TestEstesAlphaIIIWithMotorPods()
{
    rocket->setName("Alpha III with motor pods");

    AxialStage* const stage0   = rocket->getStage(0);
    auto&             bodyTube = dynamic_cast<BodyTube&>(stage0->getChild(1));

    auto newPodSet = std::make_unique<PodSet>();
    newPodSet->setName("Motor Pods");
    podSet = &bodyTube.addChild(std::move(newPodSet));
    podSet->setInstanceCount(2);
    podSet->setAxialMethod(AxialMethod::BOTTOM);
    podSet->setAxialOffset(0.0);
    podSet->setRadiusMethod(RadiusMethod::RELATIVE);
    podSet->setRadiusOffset(0.0);

    auto newPodBody = std::make_unique<BodyTube>(0.06, 0.0075, 0.0003);
    newPodBody->setName("Pod Body Tube");
    podBody = &podSet->addChild(std::move(newPodBody));

    auto newPodMount = std::make_unique<InnerTube>();
    newPodMount->setName("Pod Motor Mount");
    newPodMount->setAxialMethod(AxialMethod::BOTTOM);
    newPodMount->setAxialOffset(0.0);
    newPodMount->setLength(0.045);
    newPodMount->setOuterRadius(0.0065);
    newPodMount->setThickness(0.0003);
    newPodMount->setMotorMount(true);
    podMount = &podBody->addChild(std::move(newPodMount));

    // The pod motors have no ejection delay, so their charges fire the instant they burn out.
    for (int i = 0; i < 5; i++)
    {
        const FlightConfigurationId fcid = testFcid(i);
        MotorConfiguration          motorConfig{*podMount, fcid};
        motorConfig.setMotor(motorA10());
        motorConfig.setEjectionDelay(0.0);
        podMount->setMotorConfig(std::move(motorConfig), fcid);
    }
}

// ------------------------------------------------------- makeEstesAlphaIIIWithSecondMotor

TestEstesAlphaIIIWithSecondMotor::TestEstesAlphaIIIWithSecondMotor()
{
    rocket->setName("Alpha III with a second motor");

    AxialStage* const stage0   = rocket->getStage(0);
    auto&             bodyTube = dynamic_cast<BodyTube&>(stage0->getChild(1));

    auto newMount = std::make_unique<InnerTube>();
    newMount->setName("Second Motor Mount Tube");
    secondMount = &bodyTube.addChild(std::move(newMount));
    secondMount->setAxialMethod(AxialMethod::TOP);
    secondMount->setAxialOffset(0.06);
    secondMount->setLength(0.045);
    secondMount->setOuterRadius(0.0065);
    secondMount->setThickness(0.0003);
    secondMount->setMotorMount(true);

    for (int i = 0; i < 5; i++)
    {
        const FlightConfigurationId fcid = testFcid(i);
        MotorConfiguration          motorConfig{*secondMount, fcid};
        motorConfig.setMotor(motorA10());
        motorConfig.setEjectionDelay(5.0);
        secondMount->setMotorConfig(std::move(motorConfig), fcid);
    }
}

// --------------------------------------------------------- makeEstesAlphaIIIwithInlinePod

TestEstesAlphaIIIWithInlinePod::TestEstesAlphaIIIWithInlinePod()
{
    // Find rocket components to manipulate. Java walks the entries of the instance map while
    // the events its loop fires rebuild that map (a ConcurrentHashMap, which allows it); here
    // the loop walks a copy of the keys.
    const InstanceMap& imap = rocket->getSelectedConfiguration().getActiveInstances();
    const std::vector<RocketComponent*> components = imap.keys();
    const AxialStage*                   foundStage = nullptr;
    const BodyTube*                     foundBody  = nullptr;
    for (RocketComponent* const component : components)
    {
        // reference everything to the bottom
        component->setAxialMethod(AxialMethod::BOTTOM);

        if (const auto* const asStage = dynamic_cast<const AxialStage*>(component))
        {
            foundStage = asStage;
        }

        if (const auto* const asBody = dynamic_cast<const BodyTube*>(component))
        {
            foundBody = asBody;
        }
    }
    QTROCKET_ASSERT(foundStage == stage && foundBody == body);

    // disconnect the body from the stage (Java keeps its reference; here the maker owns the
    // body until it is added to the pod)
    std::unique_ptr<BodyTube> detached = takeChild(*stage, *body);

    // Make a shorter copy of the body tube and connect it the Stage
    // Notice -- total lengths of the short tubes must add up to match the original
    auto newFrontTube = std::make_unique<BodyTube>(
        detached->getLength() / 2.0, detached->getOuterRadius(), detached->getThickness());
    newFrontTube->setName("Front Body Tube");
    frontTube = &stage->addChild(std::move(newFrontTube));

    // Add a PodSet to the front body tube.
    auto newPod = std::make_unique<PodSet>();
    newPod->setInstanceCount(1);
    newPod->setRadiusMethod(RadiusMethod::COAXIAL);
    pod = &frontTube->addChild(std::move(newPod));
    pod->setAxialMethod(AxialMethod::TOP);
    pod->setAxialOffset(frontTube->getLength());

    // Add another even shorter tube to the pod
    auto newMiddleTube = std::make_unique<BodyTube>(
        detached->getLength() / 4.0, detached->getOuterRadius(), detached->getThickness());
    newMiddleTube->setName("Middle Body Tube");
    middleTube = &pod->addChild(std::move(newMiddleTube));

    // Shorten the original body tube, rename it, and put it on the pod
    detached->setName("Aft body tube");
    detached->setLength(detached->getLength() / 4.0);
    body = &pod->addChild(std::move(detached));
}

// ------------------------------------------------------------------------ makeClusterPods

TestClusterPods::TestClusterPods()
  : fcid(rocket->createFlightConfiguration(FlightConfigurationId{}).getId())
{
    // Sustainer
    auto newSustainer = std::make_unique<AxialStage>();
    newSustainer->setName("Sustainer");
    sustainer = &rocket->addChild(std::move(newSustainer));

    // Sustainer body tube
    auto sustainerBodyTube = std::make_unique<BodyTube>();
    sustainerBodyTube->setName("Sustainer Body Tube");
    sustainerBody = &sustainer->addChild(std::move(sustainerBodyTube));

    // Inner tubes for motor mount cluster
    auto sustainerMotorMount = std::make_unique<InnerTube>();
    sustainerMotorMount->setMotorMount(true);
    // two motors
    sustainerMotorMount->setClusterConfiguration(ClusterConfiguration::configurations()[1]);
    sustainerMount = &sustainerBody->addChild(std::move(sustainerMotorMount));

    MotorConfiguration sustainerMotorConfig{*sustainerMount, fcid};
    sustainerMotorConfig.setMotor(motorC6());
    sustainerMotorConfig.setIgnitionEvent(IgnitionEvent::LAUNCH);
    sustainerMount->setMotorConfig(std::move(sustainerMotorConfig), fcid);

    // Three Side Boosters
    sideBoosters = &sustainerBody->addChild(std::make_unique<ParallelStage>(3));

    auto sideBodyTubes = std::make_unique<BodyTube>();
    sideBodyTubes->setName("Side Booster Body Tubes");
    sideBody = &sideBoosters->addChild(std::move(sideBodyTubes));

    // Each side booster has a four-motor cluster
    auto sideBoosterMounts = std::make_unique<InnerTube>();
    sideBoosterMounts->setMotorMount(true);
    // four motors
    sideBoosterMounts->setClusterConfiguration(ClusterConfiguration::configurations()[3]);
    sideMounts = &sideBody->addChild(std::move(sideBoosterMounts));

    MotorConfiguration sideMotorConfig{*sideMounts, fcid};
    sideMotorConfig.setMotor(motorC6());
    sideMotorConfig.setIgnitionEvent(IgnitionEvent::LAUNCH);
    sideMounts->setMotorConfig(std::move(sideMotorConfig), fcid);

    rocket->enableEvents();
    rocket->setSelectedConfiguration(fcid);
}

// ============================================================================ the makers

namespace
{

/// The rocket of a new @p Fixture.
template <class Fixture>
[[nodiscard]] std::unique_ptr<Rocket> rocketOf()
{
    Fixture fixture;
    return std::move(fixture.rocket);
}

}  // namespace

std::ostream& operator<<(std::ostream& out, const TestRocketMaker& maker)
{
    return out << maker.method << " (" << maker.input << ')';
}

std::span<const TestRocketMaker> testRocketMakers()
{
    static constexpr std::array<TestRocketMaker, 13> kMakers{{
        {.input                 = "testrocket-estes-alpha-iii",
         .method                = "makeEstesAlphaIII",
         .make                  = &rocketOf<TestEstesAlphaIII>,
         .randomConfigurationId = false},
        {.input                 = "testrocket-beta",
         .method                = "makeBeta",
         .make                  = &rocketOf<TestBeta>,
         .randomConfigurationId = false},
        {.input                 = "testrocket-simple-2-stage",
         .method                = "makeSimple2Stage",
         .make                  = &rocketOf<TestSimple2Stage>,
         .randomConfigurationId = false},
        {.input                 = "testrocket-big-blue",
         .method                = "makeBigBlue",
         .make                  = &rocketOf<TestBigBlue>,
         .randomConfigurationId = false},
        {.input                 = "testrocket-iso-haisu",
         .method                = "makeIsoHaisu",
         .make                  = &rocketOf<TestIsoHaisu>,
         .randomConfigurationId = false},
        {.input                 = "testrocket-falcon-9-heavy",
         .method                = "makeFalcon9Heavy",
         .make                  = &rocketOf<TestFalcon9Heavy>,
         .randomConfigurationId = false},
        {.input                 = "testrocket-multi-stage-event-test-rocket",
         .method                = "makeMultiStageEventTestRocket",
         .make                  = &rocketOf<TestMultiStageEventTestRocket>,
         .randomConfigurationId = true},
        {.input                 = "testrocket-end-plate-rocket",
         .method                = "makeEndPlateRocket",
         .make                  = &rocketOf<TestEndPlateRocket>,
         .randomConfigurationId = false},
        {.input                 = "testrocket-estes-alpha-iii-with-pods",
         .method                = "makeEstesAlphaIIIWithPods",
         .make                  = &rocketOf<TestEstesAlphaIIIWithPods>,
         .randomConfigurationId = false},
        {.input                 = "testrocket-estes-alpha-iii-with-motor-pods",
         .method                = "makeEstesAlphaIIIWithMotorPods",
         .make                  = &rocketOf<TestEstesAlphaIIIWithMotorPods>,
         .randomConfigurationId = false},
        {.input                 = "testrocket-estes-alpha-iii-with-second-motor",
         .method                = "makeEstesAlphaIIIWithSecondMotor",
         .make                  = &rocketOf<TestEstesAlphaIIIWithSecondMotor>,
         .randomConfigurationId = false},
        {.input                 = "testrocket-estes-alpha-iii-with-inline-pod",
         .method                = "makeEstesAlphaIIIwithInlinePod",
         .make                  = &rocketOf<TestEstesAlphaIIIWithInlinePod>,
         .randomConfigurationId = false},
        {.input                 = "testrocket-cluster-pods",
         .method                = "makeClusterPods",
         .make                  = &rocketOf<TestClusterPods>,
         .randomConfigurationId = true},
    }};
    return kMakers;
}

}  // namespace QtRocket::Test
