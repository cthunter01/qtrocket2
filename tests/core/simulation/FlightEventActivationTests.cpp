#include "QtRocket/simulation/FlightEventActivation.h"

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/mass/MotorClusterState.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentAssembly.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "rocket/TestRockets.h"
#include "simulation/SimulationTestSupport.h"

namespace
{

using QtRocket::AxialStage;
using QtRocket::BugError;
using QtRocket::ComponentAssembly;
using QtRocket::DeploymentConfiguration;
using QtRocket::FlightConfiguration;
using QtRocket::FlightConfigurationId;
using QtRocket::FlightEvent;
using QtRocket::IgnitionEvent;
using QtRocket::MotorClusterState;
using QtRocket::MotorConfiguration;
using QtRocket::MotorMount;
using QtRocket::RecoveryDevice;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::SimulationAbort;
using QtRocket::StageSeparationConfiguration;
using QtRocket::Warning;
using QtRocket::Test::EventTestRocket;
using DeployEvent     = QtRocket::DeploymentConfiguration::DeployEvent;
using SeparationEvent = QtRocket::StageSeparationConfiguration::SeparationEvent;
using Type            = QtRocket::FlightEvent::Type;
using Altitudes       = QtRocket::FlightEvent::AltitudeChange;

namespace Activation = QtRocket::FlightEventActivation;

// ============================================================================ the rules

/// The configuration of the test rocket with every stage active.
[[nodiscard]] FlightConfiguration& configOf(EventTestRocket& r)
{
    return r.rocket.getFlightConfiguration(r.fcid);
}

TEST(IgnitionActivation, LaunchFiresOnALaunchEventWhateverItsSource)
{
    EventTestRocket            r;
    const FlightConfiguration& config = configOf(r);
    EXPECT_TRUE(Activation::isActivationEvent(IgnitionEvent::LAUNCH, config,
                                              {Type::LAUNCH, 0.0, &r.rocket}, *r.boosterBody));
    EXPECT_TRUE(Activation::isActivationEvent(IgnitionEvent::LAUNCH, config, {Type::LAUNCH, 0.0},
                                              *r.sustainerMount));
    EXPECT_TRUE(
        Activation::isActivationEvent(IgnitionEvent::LAUNCH, config, {Type::LAUNCH, 0.0}, r.rocket))
        << "the target is not looked at";
    EXPECT_FALSE(Activation::isActivationEvent(IgnitionEvent::LAUNCH, config, {Type::LIFTOFF, 0.0},
                                               *r.boosterBody));
}

TEST(IgnitionActivation, EjectionChargeAndBurnoutFireFromTheStageRightBelow)
{
    EventTestRocket            r;
    const FlightConfiguration& config = configOf(r);
    // The booster is right below the sustainer; the strap-ons are right below the booster.
    const FlightEvent boosterCharge{Type::EJECTION_CHARGE, 1.0, r.booster};
    const FlightEvent strapOnCharge{Type::EJECTION_CHARGE, 1.0, r.strapOns};
    const FlightEvent ownCharge{Type::EJECTION_CHARGE, 1.0, r.sustainer};

    EXPECT_TRUE(Activation::isActivationEvent(IgnitionEvent::EJECTION_CHARGE, config, boosterCharge,
                                              *r.sustainerMount));
    EXPECT_FALSE(Activation::isActivationEvent(IgnitionEvent::EJECTION_CHARGE, config,
                                               strapOnCharge, *r.sustainerMount))
        << "two stages below";
    EXPECT_FALSE(Activation::isActivationEvent(IgnitionEvent::EJECTION_CHARGE, config, ownCharge,
                                               *r.sustainerMount))
        << "the motor's own stage";
    EXPECT_TRUE(Activation::isActivationEvent(IgnitionEvent::EJECTION_CHARGE, config, strapOnCharge,
                                              *r.boosterBody));
    EXPECT_FALSE(Activation::isActivationEvent(IgnitionEvent::EJECTION_CHARGE, config,
                                               boosterCharge, *r.boosterBody));
    EXPECT_FALSE(Activation::isActivationEvent(IgnitionEvent::EJECTION_CHARGE, config,
                                               boosterCharge, *r.strapOnBody))
        << "a stage above never ignites a stage below";

    const FlightEvent boosterBurnout{Type::BURNOUT, 1.0, r.boosterBody};
    EXPECT_TRUE(Activation::isActivationEvent(IgnitionEvent::BURNOUT, config, boosterBurnout,
                                              *r.sustainerMount));
    EXPECT_FALSE(Activation::isActivationEvent(IgnitionEvent::BURNOUT, config, boosterCharge,
                                               *r.sustainerMount))
        << "the other type";
    EXPECT_FALSE(Activation::isActivationEvent(IgnitionEvent::EJECTION_CHARGE, config,
                                               boosterBurnout, *r.sustainerMount));
}

TEST(IgnitionActivation, AutomaticIsLaunchInTheLaunchStageAndEjectionChargeAbove)
{
    EventTestRocket      r;
    FlightConfiguration& config = configOf(r);
    const FlightEvent    launch{Type::LAUNCH, 0.0, &r.rocket};
    const FlightEvent    boosterCharge{Type::EJECTION_CHARGE, 1.0, r.booster};

    // Every stage active: the booster is the bottom core stage.
    EXPECT_TRUE(
        Activation::isActivationEvent(IgnitionEvent::AUTOMATIC, config, launch, *r.boosterBody));
    EXPECT_FALSE(
        Activation::isActivationEvent(IgnitionEvent::AUTOMATIC, config, launch, *r.sustainerMount));
    EXPECT_TRUE(Activation::isActivationEvent(IgnitionEvent::AUTOMATIC, config, boosterCharge,
                                              *r.sustainerMount));
    EXPECT_TRUE(
        Activation::isActivationEvent(IgnitionEvent::AUTOMATIC, config, launch, *r.strapOnBody))
        << "an active parallel stage is a launch stage";

    // Only the sustainer active: now it is the launch stage.
    config.setOnlyStage(0);
    EXPECT_TRUE(
        Activation::isActivationEvent(IgnitionEvent::AUTOMATIC, config, launch, *r.sustainerMount));
    EXPECT_FALSE(Activation::isActivationEvent(IgnitionEvent::AUTOMATIC, config, boosterCharge,
                                               *r.sustainerMount));
    EXPECT_FALSE(
        Activation::isActivationEvent(IgnitionEvent::AUTOMATIC, config, launch, *r.strapOnBody));
}

TEST(IgnitionActivation, NeverNeverFires)
{
    EventTestRocket            r;
    const FlightConfiguration& config = configOf(r);
    for (const Type type : FlightEvent::kAllTypes)
    {
        const Result<FlightEvent> event = FlightEvent::create(type, 1.0, nullptr);
        if (event.has_value())
        {
            EXPECT_FALSE(Activation::isActivationEvent(IgnitionEvent::NEVER, config, *event,
                                                       *r.sustainerMount))
                << name(type);
        }
    }
}

TEST(IgnitionActivation, WhatJavaThrowsIsABug)
{
    EventTestRocket            r;
    const FlightConfiguration& config = configOf(r);
    // An ejection charge without a source (Java: NullPointerException).
    EXPECT_THROW(static_cast<void>(Activation::isActivationEvent(
                     IgnitionEvent::EJECTION_CHARGE, config, {Type::EJECTION_CHARGE, 1.0},
                     *r.sustainerMount)),
                 BugError);
    // A target that is in no stage (Java: IllegalStateException).
    EXPECT_THROW(
        static_cast<void>(Activation::isActivationEvent(
            IgnitionEvent::BURNOUT, config, {Type::BURNOUT, 1.0, r.boosterBody}, r.rocket)),
        BugError);
    EXPECT_THROW(static_cast<void>(Activation::isActivationEvent(IgnitionEvent::AUTOMATIC, config,
                                                                 {Type::LAUNCH, 0.0}, r.rocket)),
                 BugError);
    // Another type never gets that far.
    EXPECT_FALSE(Activation::isActivationEvent(IgnitionEvent::BURNOUT, config, {Type::LAUNCH, 1.0},
                                               r.rocket));
}

TEST(IgnitionActivation, TestForIgnitionAsksTheStatesIgnitionEventAndMount)
{
    EventTestRocket      r;
    FlightConfiguration& config = configOf(r);
    const FlightEvent    launch{Type::LAUNCH, 0.0, &r.rocket};
    const FlightEvent    boosterCharge{Type::EJECTION_CHARGE, 1.0, r.booster};
    const FlightEvent    boosterBurnout{Type::BURNOUT, 1.0, r.boosterBody};
    MotorConfiguration&  motor = r.sustainerMount->getMotorConfig(r.fcid);

    // The state of the fixture was made with the default ignition event, AUTOMATIC.
    ASSERT_EQ(r.state->getIgnitionEvent(), IgnitionEvent::AUTOMATIC);
    EXPECT_FALSE(Activation::testForIgnition(*r.state, config, launch));
    EXPECT_TRUE(Activation::testForIgnition(*r.state, config, boosterCharge));

    motor.setIgnitionEvent(IgnitionEvent::BURNOUT);
    const MotorClusterState onBurnout(motor);
    EXPECT_TRUE(Activation::testForIgnition(onBurnout, config, boosterBurnout));
    EXPECT_FALSE(Activation::testForIgnition(onBurnout, config, boosterCharge));

    motor.setIgnitionEvent(IgnitionEvent::NEVER);
    const MotorClusterState never(motor);
    EXPECT_FALSE(Activation::testForIgnition(never, config, launch));
    EXPECT_FALSE(Activation::testForIgnition(never, config, boosterBurnout));
}

TEST(DeployActivation, TheSimpleEvents)
{
    const EventTestRocket   r;
    DeploymentConfiguration config;
    const FlightEvent       launch{Type::LAUNCH, 0.0, &r.rocket};
    const FlightEvent       apogee{Type::APOGEE, 3.0, &r.rocket};

    EXPECT_TRUE(Activation::isActivationEvent(DeployEvent::LAUNCH, config, launch, *r.chute));
    EXPECT_FALSE(Activation::isActivationEvent(DeployEvent::LAUNCH, config, apogee, *r.chute));
    EXPECT_TRUE(Activation::isActivationEvent(DeployEvent::APOGEE, config, apogee, *r.chute));
    EXPECT_FALSE(Activation::isActivationEvent(DeployEvent::APOGEE, config, launch, *r.chute));
    EXPECT_FALSE(Activation::isActivationEvent(DeployEvent::NEVER, config, launch, *r.chute));
    EXPECT_FALSE(Activation::isActivationEvent(DeployEvent::NEVER, config, apogee, *r.chute));

    // The overload that reads the event from the configuration.
    config.setDeployEvent(DeployEvent::APOGEE);
    EXPECT_TRUE(Activation::isActivationEvent(config, apogee, *r.chute));
    EXPECT_FALSE(Activation::isActivationEvent(config, launch, *r.chute));
    config.setDeployEvent(DeployEvent::NEVER);
    EXPECT_FALSE(Activation::isActivationEvent(config, apogee, *r.chute));
}

TEST(DeployActivation, AnEjectionChargeDeploysTheDevicesOfItsStage)
{
    const EventTestRocket         r;
    const DeploymentConfiguration config;
    // Without the motor's state: the stage number of the source decides.
    EXPECT_TRUE(Activation::isActivationEvent(DeployEvent::EJECTION, config,
                                              {Type::EJECTION_CHARGE, 1.0, r.sustainer}, *r.chute));
    EXPECT_FALSE(Activation::isActivationEvent(DeployEvent::EJECTION, config,
                                               {Type::EJECTION_CHARGE, 1.0, r.booster}, *r.chute));
    EXPECT_FALSE(Activation::isActivationEvent(DeployEvent::EJECTION, config,
                                               {Type::BURNOUT, 1.0, r.sustainerBody}, *r.chute));
    EXPECT_THROW(static_cast<void>(Activation::isActivationEvent(
                     DeployEvent::EJECTION, config, {Type::EJECTION_CHARGE, 1.0}, *r.chute)),
                 BugError)
        << "no state and no source (Java: NullPointerException)";
}

TEST(DeployActivation, AnEjectionChargeWithItsMotorDeploysTheDevicesOfTheMotorsAssembly)
{
    // The Alpha III with motor pods: one motor in the body, one in a pod set on it, the
    // parachute in the body.
    const QtRocket::Test::TestEstesAlphaIIIWithMotorPods r;
    const FlightConfigurationId                          fcid = QtRocket::Test::testFcid(0);
    const DeploymentConfiguration                        config;
    const auto bodyMotor = std::make_shared<MotorClusterState>(r.inner->getMotorConfig(fcid));
    const auto podMotor  = std::make_shared<MotorClusterState>(r.podMount->getMotorConfig(fcid));

    // The source is the stage in both events; the motor's assembly decides.
    EXPECT_TRUE(Activation::isActivationEvent(
        DeployEvent::EJECTION, config, {Type::EJECTION_CHARGE, 1.0, r.stage, bodyMotor}, *r.chute));
    EXPECT_FALSE(Activation::isActivationEvent(
        DeployEvent::EJECTION, config, {Type::EJECTION_CHARGE, 1.0, r.stage, podMotor}, *r.chute))
        << "a charge in a pod does not pressurise the body";
    // Without the state the pod's charge would deploy it: same stage number.
    EXPECT_TRUE(Activation::isActivationEvent(DeployEvent::EJECTION, config,
                                              {Type::EJECTION_CHARGE, 1.0, r.stage}, *r.chute));
    // The state decides even without a source.
    EXPECT_TRUE(Activation::isActivationEvent(
        DeployEvent::EJECTION, config, {Type::EJECTION_CHARGE, 1.0, nullptr, bodyMotor}, *r.chute));
}

/// Whether an altitude event from @p previous to @p current deploys the parachute of @p r at a
/// deploy altitude of 200 m.
[[nodiscard]] bool deploys(const EventTestRocket& r, double previous, double current)
{
    DeploymentConfiguration config;
    config.setDeployAltitude(200.0);
    return Activation::isActivationEvent(
        DeployEvent::ALTITUDE, config,
        {Type::ALTITUDE, 1.0, &r.rocket, Altitudes{.previous = previous, .current = current}},
        *r.chute);
}

TEST(DeployActivation, AnAltitudeEventDeploysWhenItGoesDownThroughTheDeployAltitude)
{
    const EventTestRocket r;
    EXPECT_TRUE(deploys(r, 250.0, 150.0));
    EXPECT_FALSE(deploys(r, 150.0, 250.0)) << "going up";
    EXPECT_FALSE(deploys(r, 300.0, 250.0)) << "still above";
    EXPECT_FALSE(deploys(r, 150.0, 100.0)) << "already below";
    EXPECT_TRUE(deploys(r, 200.0, 150.0)) << "u >= altitude";
    EXPECT_TRUE(deploys(r, 250.0, 200.0)) << "v <= altitude";
    EXPECT_TRUE(deploys(r, 200.0, 200.0));
}

TEST(DeployActivation, AnAltitudeEventNeedsItsAltitudes)
{
    const EventTestRocket   r;
    DeploymentConfiguration config;
    config.setDeployAltitude(200.0);
    EXPECT_FALSE(Activation::isActivationEvent(DeployEvent::ALTITUDE, config,
                                               {Type::ALTITUDE, 1.0, &r.rocket}, *r.chute))
        << "an altitude event without data (the thrust curve's time points)";
    EXPECT_FALSE(Activation::isActivationEvent(DeployEvent::ALTITUDE, config,
                                               {Type::APOGEE, 1.0, &r.rocket}, *r.chute));
    EXPECT_THROW(static_cast<void>(Activation::isActivationEvent(
                     DeployEvent::ALTITUDE, config,
                     {Type::ALTITUDE, 1.0, &r.rocket, std::string{"not altitudes"}}, *r.chute)),
                 BugError)
        << "Java: ClassCastException";
}

TEST(DeployActivation, TheSeparationOfTheStageBelowDeploys)
{
    const EventTestRocket         r;
    const DeploymentConfiguration config;
    EXPECT_TRUE(Activation::isActivationEvent(DeployEvent::LOWER_STAGE_SEPARATION, config,
                                              {Type::STAGE_SEPARATION, 2.0, r.booster}, *r.chute));
    EXPECT_FALSE(Activation::isActivationEvent(DeployEvent::LOWER_STAGE_SEPARATION, config,
                                               {Type::STAGE_SEPARATION, 2.0, r.strapOns}, *r.chute))
        << "two stages below";
    EXPECT_FALSE(Activation::isActivationEvent(DeployEvent::LOWER_STAGE_SEPARATION, config,
                                               {Type::STAGE_SEPARATION, 2.0, r.sustainer},
                                               *r.chute));
    EXPECT_THROW(
        static_cast<void>(Activation::isActivationEvent(DeployEvent::LOWER_STAGE_SEPARATION, config,
                                                        {Type::STAGE_SEPARATION, 2.0}, *r.chute)),
        BugError);
}

TEST(SeparationActivation, TheEventsOfTheStageItself)
{
    const EventTestRocket              r;
    const StageSeparationConfiguration config;
    const FlightEvent                  ownIgnition{Type::IGNITION, 1.0, r.boosterBody};
    const FlightEvent                  ownBurnout{Type::BURNOUT, 1.0, r.boosterBody};
    const FlightEvent                  ownCharge{Type::EJECTION_CHARGE, 1.0, r.booster};
    const FlightEvent                  upperIgnition{Type::IGNITION, 1.0, r.sustainerMount};

    EXPECT_TRUE(
        Activation::isSeparationEvent(SeparationEvent::IGNITION, config, ownIgnition, *r.booster));
    EXPECT_FALSE(Activation::isSeparationEvent(SeparationEvent::IGNITION, config, upperIgnition,
                                               *r.booster));
    EXPECT_FALSE(
        Activation::isSeparationEvent(SeparationEvent::IGNITION, config, ownBurnout, *r.booster));
    EXPECT_TRUE(
        Activation::isSeparationEvent(SeparationEvent::BURNOUT, config, ownBurnout, *r.booster));
    EXPECT_FALSE(
        Activation::isSeparationEvent(SeparationEvent::BURNOUT, config, ownBurnout, *r.strapOns));
    EXPECT_TRUE(
        Activation::isSeparationEvent(SeparationEvent::EJECTION, config, ownCharge, *r.booster));
    EXPECT_FALSE(
        Activation::isSeparationEvent(SeparationEvent::EJECTION, config, ownCharge, *r.sustainer));

    EXPECT_TRUE(Activation::isSeparationEvent(SeparationEvent::UPPER_IGNITION, config,
                                              upperIgnition, *r.booster));
    EXPECT_FALSE(Activation::isSeparationEvent(SeparationEvent::UPPER_IGNITION, config, ownIgnition,
                                               *r.booster));
    EXPECT_TRUE(Activation::isSeparationEvent(SeparationEvent::UPPER_IGNITION, config, ownIgnition,
                                              *r.strapOns))
        << "by stage number: the strap-ons are stage 2, the booster stage 1";
}

TEST(SeparationActivation, TheSimpleEvents)
{
    const EventTestRocket              r;
    const StageSeparationConfiguration config;
    const FlightEvent                  launch{Type::LAUNCH, 0.0, &r.rocket};
    const FlightEvent                  apogee{Type::APOGEE, 3.0, &r.rocket};
    EXPECT_TRUE(Activation::isSeparationEvent(SeparationEvent::LAUNCH, config, launch, *r.booster));
    EXPECT_FALSE(
        Activation::isSeparationEvent(SeparationEvent::LAUNCH, config, apogee, *r.booster));
    EXPECT_TRUE(Activation::isSeparationEvent(SeparationEvent::APOGEE, config, apogee, *r.booster));
    EXPECT_FALSE(
        Activation::isSeparationEvent(SeparationEvent::APOGEE, config, launch, *r.booster));
    EXPECT_FALSE(Activation::isSeparationEvent(SeparationEvent::NEVER, config, launch, *r.booster));
    EXPECT_THROW(static_cast<void>(Activation::isSeparationEvent(
                     SeparationEvent::IGNITION, config, {Type::IGNITION, 1.0}, *r.booster)),
                 BugError)
        << "an ignition without a source (Java: NullPointerException)";
}

/// Whether an altitude event from @p previous to @p current separates the booster of @p r when
/// its separation is set to @p event at 200 m.
[[nodiscard]] bool separates(const EventTestRocket& r, SeparationEvent event, double previous,
                             double current)
{
    StageSeparationConfiguration config;
    config.setSeparationAltitude(200.0);
    return Activation::isSeparationEvent(
        event, config,
        {Type::ALTITUDE, 1.0, &r.rocket, Altitudes{.previous = previous, .current = current}},
        *r.booster);
}

TEST(SeparationActivation, TheAltitudeEvents)
{
    const EventTestRocket r;
    EXPECT_TRUE(separates(r, SeparationEvent::ALTITUDE_ASCENDING, 150.0, 250.0));
    EXPECT_FALSE(separates(r, SeparationEvent::ALTITUDE_ASCENDING, 250.0, 150.0));
    EXPECT_TRUE(separates(r, SeparationEvent::ALTITUDE_ASCENDING, 200.0, 200.0));
    EXPECT_TRUE(separates(r, SeparationEvent::ALTITUDE_ASCENDING, 200.0, 250.0));
    EXPECT_FALSE(separates(r, SeparationEvent::ALTITUDE_ASCENDING, 100.0, 150.0));
    EXPECT_TRUE(separates(r, SeparationEvent::ALTITUDE_DESCENDING, 250.0, 150.0));
    EXPECT_FALSE(separates(r, SeparationEvent::ALTITUDE_DESCENDING, 150.0, 250.0));
    EXPECT_TRUE(separates(r, SeparationEvent::ALTITUDE_DESCENDING, 200.0, 200.0));
    EXPECT_FALSE(separates(r, SeparationEvent::ALTITUDE_DESCENDING, 300.0, 250.0));
}

TEST(SeparationActivation, AnAltitudeEventNeedsItsAltitudes)
{
    const EventTestRocket        r;
    StageSeparationConfiguration config;
    config.setSeparationAltitude(200.0);
    EXPECT_FALSE(Activation::isSeparationEvent(SeparationEvent::ALTITUDE_ASCENDING, config,
                                               {Type::ALTITUDE, 1.0, &r.rocket}, *r.booster))
        << "an altitude event without data";
    EXPECT_FALSE(Activation::isSeparationEvent(SeparationEvent::ALTITUDE_DESCENDING, config,
                                               {Type::ALTITUDE, 1.0, &r.rocket}, *r.booster));
}

// ====================================================================== the tables of Java

/// One truth table of the Java probe (probes/events-data-impl/ActivationProbe.java): which
/// function and constant it is of, its number of entries, and the entries, run-length encoded
/// ("3F2TE" is FFFTTE): T and F for the answers, E where Java throws.
struct JavaTable
{
    std::string_view key;
    std::size_t      length;
    std::string_view rle;
};

/// What the probe printed for one test rocket.
struct JavaRocket
{
    /// The maker of TestRockets ("makeBeta").
    std::string_view maker;
    /// The test configuration to select first (0 for testFcid(0)), -1 to keep the maker's.
    int selectFcid;
    /// The components the tables range over, in tree order: class name and stage number, and
    /// "*" for a mount with a motor in the selected configuration.
    std::string_view       components;
    std::size_t            representatives;
    std::size_t            stages;
    std::size_t            states;
    std::size_t            events;
    std::vector<JavaTable> tables;
};

[[nodiscard]] const std::vector<JavaRocket>& javaRockets()
{
    static const std::vector<JavaRocket> kRockets{
        JavaRocket{
            .maker      = "makeBeta",
            .selectFcid = -1,
            .components =
                "Rocket-1,AxialStage0,BodyTube0,InnerTube0*,Parachute0,AxialStage1,BodyTube1,Inne"
                "rTube1*",
            .representatives = 8,
            .stages          = 2,
            .states          = 2,
            .events          = 252,
            .tables =
                {
                    {.key    = "events",
                     .length = 252,
                     .rle    = "12.6X6.6X27.6X6.6X9.3X3.9X3.6X99.X.17X.X.X.X.X.X.X.X.X10."},
                    {.key    = "ignition AUTOMATIC 0",
                     .length = 1472,
                     .rle    = "184E57F3E3F3T175F3E3F3T175F3E3F3T175F3E3F3T118F9T175F9T175F9T175F"},
                    {.key    = "ignition AUTOMATIC 1",
                     .length = 1472,
                     .rle    = "184E9T175F9T175F9T175F9T232F3E181F3E181F3E124F"},
                    {.key    = "ignition LAUNCH 0",
                     .length = 1472,
                     .rle    = "9T175F9T175F9T175F9T175F9T175F9T175F9T175F9T175F"},
                    {.key    = "ignition EJECTION_CHARGE 0",
                     .length = 1472,
                     .rle = "57F9E175F3E3F3T175F3E3F3T175F3E3F3T175F3E3F3T175F3E181F3E181F3E124F"},
                    {.key    = "ignition BURNOUT 0",
                     .length = 1472,
                     .rle = "42F15E169F3E6F6T169F3E6F6T169F3E6F6T169F3E6F6T169F3E181F3E181F3E139F"},
                    {.key = "ignition NEVER 0", .length = 1472, .rle = "1472F"},
                    {.key    = "deploy LAUNCH",
                     .length = 1472,
                     .rle    = "9T175F9T175F9T175F9T175F9T175F9T175F9T175F9T175F"},
                    {.key    = "deploy EJECTION",
                     .length = 1472,
                     .rle = "57FE183FETF2T2FT176FETF2T2FT176FETF2T2FT176FETF2T2FT176FEFT2F2TFT175FE"
                            "FT2F2TFT175FEFT2F2TFT118F"},
                    {.key    = "deploy APOGEE",
                     .length = 1472,
                     .rle    = "75F9T175F9T175F9T175F9T175F9T175F9T175F9T175F9T100F"},
                    {.key    = "deploy ALTITUDE",
                     .length = 1472,
                     .rle = "112FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT141FTFT2FTFT2FTFT2FTFT2"
                            "FTFT2FTFT2FTFT2FTFT2FTFT141FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTF"
                            "T141FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT141FTFT2FTFT2FTFT2FTFT"
                            "2FTFT2FTFT2FTFT2FTFT2FTFT141FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT"
                            "FT141FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT141FTFT2FTFT2FTFT2FTF"
                            "T2FTFT2FTFT2FTFT2FTFT2FTFT29F"},
                    {.key    = "deploy LOWER_STAGE_SEPARATION",
                     .length = 1472,
                     .rle    = "66FEF4T178FE5F3T175FE5F3T175FE5F3T175FE5F3T175FE183FE183FE117F"},
                    {.key = "deploy NEVER", .length = 1472, .rle = "1472F"},
                    {.key = "separation LAUNCH", .length = 368, .rle = "9T175F9T175F"},
                    {.key = "separation IGNITION", .length = 368, .rle = "9F3E6T175F3E6F6T160F"},
                    {.key = "separation BURNOUT", .length = 368, .rle = "42F3E6T175F3E6F6T127F"},
                    {.key = "separation EJECTION", .length = 368, .rle = "57F3E3T178F3E3F3T118F"},
                    {.key = "separation UPPER_IGNITION", .length = 368, .rle = "9F3E181F3E6T166F"},
                    {.key    = "separation ALTITUDE_ASCENDING",
                     .length = 368,
                     .rle = "113F2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T142F2T3F2T3F2T3F2T3F2T3F2T3F2T3F"
                            "2T3F2T29F"},
                    {.key = "separation APOGEE", .length = 368, .rle = "75F9T175F9T100F"},
                    {.key    = "separation ALTITUDE_DESCENDING",
                     .length = 368,
                     .rle = "112FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT141FTFT2FTFT2FTFT2FTFT2"
                            "FTFT2FTFT2FTFT2FTFT2FTFT29F"},
                    {.key = "separation NEVER", .length = 368, .rle = "368F"},
                    {.key = "testForIgnition 0", .length = 368, .rle = "57F3E3F3T118F9T175F"},
                    {.key = "testForIgnition 1", .length = 368, .rle = "9T232F3E124F"},
                }},
        JavaRocket{
            .maker      = "makeFalcon9Heavy",
            .selectFcid = -1,
            .components =
                "Rocket-1,AxialStage0,BodyTube0,BodyTube0,Parachute0,BodyTube0,AxialStage1,BodyTu"
                "be1*,ParallelStage2,BodyTube2,InnerTube2*",
            .representatives = 11,
            .stages          = 3,
            .states          = 2,
            .events          = 336,
            .tables =
                {
                    {.key    = "events",
                     .length = 336,
                     .rle = "15.6X6.3X3.3X3.3X33.6X6.3X3.3X3.3X9.3X3.12X3.3X3.6X132.X.23X.X.X.X.X.X"
                            ".X.X.X.X.X.X13."},
                    {.key    = "ignition AUTOMATIC 0",
                     .length = 2717,
                     .rle = "247E78F3E3F3T238F3E3F3T238F3E3F3T238F3E3F3T238F3E3F3T160F12T235F12T235"
                            "F12T235F12T235F12T235F"},
                    {.key    = "ignition AUTOMATIC 1",
                     .length = 2717,
                     .rle = "247E12T235F12T235F12T235F12T235F12T313F3E6F3T235F3E6F3T235F3E244F3E244"
                            "F3E166F"},
                    {.key    = "ignition LAUNCH 0",
                     .length = 2717,
                     .rle = "12T235F12T235F12T235F12T235F12T235F12T235F12T235F12T235F12T235F12T235F"
                            "12T235F"},
                    {.key    = "ignition EJECTION_CHARGE 0",
                     .length = 2717,
                     .rle = "78F12E235F3E3F3T238F3E3F3T238F3E3F3T238F3E3F3T238F3E3F3T238F3E6F3T235F"
                            "3E6F3T235F3E244F3E244F3E166F"},
                    {.key    = "ignition BURNOUT 0",
                     .length = 2717,
                     .rle = "57F21E226F3E9F3T232F3E9F3T232F3E9F3T232F3E9F3T232F3E9F3T232F3E12F6T226"
                            "F3E12F6T226F3E244F3E244F3E187F"},
                    {.key = "ignition NEVER 0", .length = 2717, .rle = "2717F"},
                    {.key    = "deploy LAUNCH",
                     .length = 2717,
                     .rle = "12T235F12T235F12T235F12T235F12T235F12T235F12T235F12T235F12T235F12T235F"
                            "12T235F"},
                    {.key    = "deploy EJECTION",
                     .length = 2717,
                     .rle = "78FE246FE2FT243FE2FT243FE2FT243FE2FT243FE2FT243FET2FTF2T2FT236FET2FTF2"
                            "T2FT236FEFT2FT2F2TFT235FEFT2FT2F2TFT235FEFT2FT2F2TFT157F"},
                    {.key    = "deploy APOGEE",
                     .length = 2717,
                     .rle = "102F12T235F12T235F12T235F12T235F12T235F12T235F12T235F12T235F12T235F12T"
                            "235F12T133F"},
                    {.key    = "deploy ALTITUDE",
                     .length = 2717,
                     .rle = "151FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT189FTFT2"
                            "FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT189FTFT2FTFT2FTF"
                            "T2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT189FTFT2FTFT2FTFT2FTFT2F"
                            "TFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT189FTFT2FTFT2FTFT2FTFT2FTFT2FTFT"
                            "2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT189FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT"
                            "FT2FTFT2FTFT2FTFT2FTFT189FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2"
                            "FTFT2FTFT2FTFT189FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTF"
                            "T2FTFT189FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT18"
                            "9FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT189FTFT2FT"
                            "FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT38F"},
                    {.key    = "deploy LOWER_STAGE_SEPARATION",
                     .length = 2717,
                     .rle = "90FEF5T240FE6F2T238FE6F2T238FE6F2T238FE6F2T238FE6F2T238FE8F3T235FE8F3T"
                            "235FE246FE246FE156F"},
                    {.key = "deploy NEVER", .length = 2717, .rle = "2717F"},
                    {.key = "separation LAUNCH", .length = 741, .rle = "12T235F12T235F12T235F"},
                    {.key    = "separation IGNITION",
                     .length = 741,
                     .rle    = "12F3E9T235F3E9F3T232F3E12F6T214F"},
                    {.key    = "separation BURNOUT",
                     .length = 741,
                     .rle    = "57F3E9T235F3E9F3T232F3E12F6T169F"},
                    {.key    = "separation EJECTION",
                     .length = 741,
                     .rle    = "78F3E3T241F3E3F3T238F3E6F3T157F"},
                    {.key    = "separation UPPER_IGNITION",
                     .length = 741,
                     .rle    = "12F3E244F3E9T235F3E9F3T220F"},
                    {.key    = "separation ALTITUDE_ASCENDING",
                     .length = 741,
                     .rle = "152F2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T190F2T3F2T3F2T3F2T3F"
                            "2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T190F2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T3F"
                            "2T3F2T3F2T38F"},
                    {.key = "separation APOGEE", .length = 741, .rle = "102F12T235F12T235F12T133F"},
                    {.key    = "separation ALTITUDE_DESCENDING",
                     .length = 741,
                     .rle = "151FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT189FTFT2"
                            "FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT189FTFT2FTFT2FTF"
                            "T2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT38F"},
                    {.key = "separation NEVER", .length = 741, .rle = "741F"},
                    {.key = "testForIgnition 0", .length = 494, .rle = "12T235F12T235F"},
                    {.key = "testForIgnition 1", .length = 494, .rle = "78F3E6F3T235F3E166F"},
                }},
        JavaRocket{
            .maker      = "makeMultiStageEventTestRocket",
            .selectFcid = -1,
            .components =
                "Rocket-1,AxialStage0,BodyTube0*,Parachute0,AxialStage1,BodyTube1*,ParallelStage2"
                ",BodyTube2*,Parachute2",
            .representatives = 9,
            .stages          = 3,
            .states          = 3,
            .events          = 310,
            .tables =
                {
                    {.key    = "events",
                     .length = 310,
                     .rle = "14.8X4.8X4.4X4.4X24.8X4.8X4.4X4.4X4.4X4.8X4.4X4.8X110.X.19X.X.X.X.X.X."
                            "X.X.X.X11."},
                    {.key    = "ignition AUTOMATIC 0",
                     .length = 1881,
                     .rle = "209E62F4E4F4T197F4E4F4T197F4E4F4T135F10T199F10T199F10T199F10T199F10T19"
                            "9F"},
                    {.key    = "ignition AUTOMATIC 1",
                     .length = 1881,
                     .rle    = "209E10T199F10T199F10T261F4E8F4T193F4E8F4T193F4E205F4E205F4E143F"},
                    {.key    = "ignition LAUNCH 0",
                     .length = 1881,
                     .rle    = "10T199F10T199F10T199F10T199F10T199F10T199F10T199F10T199F10T199F"},
                    {.key    = "ignition EJECTION_CHARGE 0",
                     .length = 1881,
                     .rle = "62F16E193F4E4F4T197F4E4F4T197F4E4F4T197F4E8F4T193F4E8F4T193F4E205F4E20"
                            "5F4E143F"},
                    {.key    = "ignition BURNOUT 0",
                     .length = 1881,
                     .rle = "46F16E193F4E4F4T197F4E4F4T197F4E4F4T197F4E8F4T193F4E8F4T193F4E205F4E20"
                            "5F4E159F"},
                    {.key = "ignition NEVER 0", .length = 1881, .rle = "1881F"},
                    {.key    = "deploy LAUNCH",
                     .length = 1881,
                     .rle    = "10T199F10T199F10T199F10T199F10T199F10T199F10T199F10T199F10T199F"},
                    {.key    = "deploy EJECTION",
                     .length = 1881,
                     .rle = "62FE208FET2F2T3FT3FT195FET2F2T3FT3FT195FET2F2T3FT3FT195FEFT3FTFTFT3FT1"
                            "94FEFT3FTFTFT3FT194FE2FT3FT3F2T2FT193FE2FT3FT3F2T2FT193FE2FT3FT3F2T2FT"
                            "131F"},
                    {.key    = "deploy APOGEE",
                     .length = 1881,
                     .rle = "88F10T199F10T199F10T199F10T199F10T199F10T199F10T199F10T199F10T111F"},
                    {.key    = "deploy ALTITUDE",
                     .length = 1881,
                     .rle = "129FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT161FTFT2FTFT2FTFT2"
                            "FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT161FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTF"
                            "T2FTFT2FTFT2FTFT161FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT16"
                            "1FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT161FTFT2FTFT2FTFT2FT"
                            "FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT161FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2"
                            "FTFT2FTFT2FTFT161FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT161F"
                            "TFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT32F"},
                    {.key    = "deploy LOWER_STAGE_SEPARATION",
                     .length = 1881,
                     .rle = "78FEF3T204FE4F2T202FE4F2T202FE4F2T202FE6F3T199FE6F3T199FE208FE208FE130"
                            "F"},
                    {.key = "deploy NEVER", .length = 1881, .rle = "1881F"},
                    {.key = "separation LAUNCH", .length = 627, .rle = "10T199F10T199F10T199F"},
                    {.key    = "separation IGNITION",
                     .length = 627,
                     .rle    = "10F4E4T201F4E4F4T197F4E8F4T183F"},
                    {.key    = "separation BURNOUT",
                     .length = 627,
                     .rle    = "46F4E4T201F4E4F4T197F4E8F4T147F"},
                    {.key    = "separation EJECTION",
                     .length = 627,
                     .rle    = "62F4E4T201F4E4F4T197F4E8F4T131F"},
                    {.key    = "separation UPPER_IGNITION",
                     .length = 627,
                     .rle    = "10F4E205F4E4T201F4E4F4T187F"},
                    {.key    = "separation ALTITUDE_ASCENDING",
                     .length = 627,
                     .rle = "130F2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T162F2T3F2T3F2T3F2T3F2T3F2T3F"
                            "2T3F2T3F2T3F2T162F2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T32F"},
                    {.key = "separation APOGEE", .length = 627, .rle = "88F10T199F10T199F10T111F"},
                    {.key    = "separation ALTITUDE_DESCENDING",
                     .length = 627,
                     .rle = "129FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT161FTFT2FTFT2FTFT2"
                            "FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT161FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTF"
                            "T2FTFT2FTFT2FTFT32F"},
                    {.key = "separation NEVER", .length = 627, .rle = "627F"},
                    {.key    = "testForIgnition 0",
                     .length = 627,
                     .rle    = "46F4E4F4T151F10T199F10T199F"},
                    {.key    = "testForIgnition 1",
                     .length = 627,
                     .rle    = "46F4E4F4T151F10T199F10T199F"},
                }},
        JavaRocket{
            .maker      = "makeEstesAlphaIIIWithMotorPods",
            .selectFcid = 0,
            .components =
                "Rocket-1,AxialStage0,BodyTube0,InnerTube0*,Parachute0,PodSet0,BodyTube0,InnerTub"
                "e0*",
            .representatives = 8,
            .stages          = 1,
            .states          = 2,
            .events          = 252,
            .tables =
                {
                    {.key    = "events",
                     .length = 252,
                     .rle    = "12.6X6.6X27.6X6.6X9.3X3.18X99.X.17X.X.X.X.X.X.X.X.X10."},
                    {.key    = "ignition AUTOMATIC 0",
                     .length = 1448,
                     .rle    = "181E9T172F9T172F9T172F9T172F9T172F9T172F9T172F"},
                    {.key    = "ignition AUTOMATIC 1",
                     .length = 1448,
                     .rle    = "181E9T172F9T172F9T172F9T172F9T172F9T172F9T172F"},
                    {.key    = "ignition LAUNCH 0",
                     .length = 1448,
                     .rle    = "9T172F9T172F9T172F9T172F9T172F9T172F9T172F9T172F"},
                    {.key    = "ignition EJECTION_CHARGE 0",
                     .length = 1448,
                     .rle    = "57F6E175F3E178F3E178F3E178F3E178F3E178F3E178F3E121F"},
                    {.key    = "ignition BURNOUT 0",
                     .length = 1448,
                     .rle    = "42F15E166F3E178F3E178F3E178F3E178F3E178F3E178F3E136F"},
                    {.key = "ignition NEVER 0", .length = 1448, .rle = "1448F"},
                    {.key    = "deploy LAUNCH",
                     .length = 1448,
                     .rle    = "9T172F9T172F9T172F9T172F9T172F9T172F9T172F9T172F"},
                    {.key    = "deploy EJECTION",
                     .length = 1448,
                     .rle = "57FE180FETF2T176FETF2T176FETF2T176FETF2T176FEF2TFT175FEF2TFT175FEF2TFT"
                            "118F"},
                    {.key    = "deploy APOGEE",
                     .length = 1448,
                     .rle    = "72F9T172F9T172F9T172F9T172F9T172F9T172F9T172F9T100F"},
                    {.key    = "deploy ALTITUDE",
                     .length = 1448,
                     .rle = "109FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT138FTFT2FTFT2FTFT2FTFT2"
                            "FTFT2FTFT2FTFT2FTFT2FTFT138FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTF"
                            "T138FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT138FTFT2FTFT2FTFT2FTFT"
                            "2FTFT2FTFT2FTFT2FTFT2FTFT138FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT"
                            "FT138FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT138FTFT2FTFT2FTFT2FTF"
                            "T2FTFT2FTFT2FTFT2FTFT2FTFT29F"},
                    {.key    = "deploy LOWER_STAGE_SEPARATION",
                     .length = 1448,
                     .rle    = "63FEF7T172FE180FE180FE180FE180FE180FE180FE117F"},
                    {.key = "deploy NEVER", .length = 1448, .rle = "1448F"},
                    {.key = "separation LAUNCH", .length = 181, .rle = "9T172F"},
                    {.key = "separation IGNITION", .length = 181, .rle = "9F3E12T157F"},
                    {.key = "separation BURNOUT", .length = 181, .rle = "42F3E12T124F"},
                    {.key = "separation EJECTION", .length = 181, .rle = "57F3E3T118F"},
                    {.key = "separation UPPER_IGNITION", .length = 181, .rle = "9F3E169F"},
                    {.key    = "separation ALTITUDE_ASCENDING",
                     .length = 181,
                     .rle    = "110F2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T29F"},
                    {.key = "separation APOGEE", .length = 181, .rle = "72F9T100F"},
                    {.key    = "separation ALTITUDE_DESCENDING",
                     .length = 181,
                     .rle    = "109FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT29F"},
                    {.key = "separation NEVER", .length = 181, .rle = "181F"},
                    {.key = "testForIgnition 0", .length = 362, .rle = "9T172F9T172F"},
                    {.key = "testForIgnition 1", .length = 362, .rle = "9T172F9T172F"},
                }},
        JavaRocket{
            .maker      = "makeClusterPods",
            .selectFcid = -1,
            .components =
                "Rocket-1,AxialStage0,BodyTube0,InnerTube0*,ParallelStage1,BodyTube1,InnerTube1*",
            .representatives = 7,
            .stages          = 2,
            .states          = 2,
            .events          = 224,
            .tables =
                {
                    {.key    = "events",
                     .length = 224,
                     .rle    = "11.6X6.3X25.6X6.3X9.3X3.6X3.6X88.X.15X.X.X.X.X.X.X.X9."},
                    {.key    = "ignition AUTOMATIC 0",
                     .length = 1176,
                     .rle    = "168E8T160F8T160F8T160F8T160F8T160F8T160F"},
                    {.key    = "ignition AUTOMATIC 1",
                     .length = 1176,
                     .rle    = "168E8T160F8T160F8T214F3E165F3E165F3E111F"},
                    {.key    = "ignition LAUNCH 0",
                     .length = 1176,
                     .rle    = "8T160F8T160F8T160F8T160F8T160F8T160F8T160F"},
                    {.key    = "ignition EJECTION_CHARGE 0",
                     .length = 1176,
                     .rle    = "54F9E159F3E3F3T159F3E3F3T159F3E3F3T159F3E165F3E165F3E111F"},
                    {.key    = "ignition BURNOUT 0",
                     .length = 1176,
                     .rle    = "39F15E153F3E6F6T153F3E6F6T153F3E6F6T153F3E165F3E165F3E126F"},
                    {.key = "ignition NEVER 0", .length = 1176, .rle = "1176F"},
                    {.key    = "deploy LAUNCH",
                     .length = 1176,
                     .rle    = "8T160F8T160F8T160F8T160F8T160F8T160F8T160F"},
                    {.key    = "deploy EJECTION",
                     .length = 1176,
                     .rle = "54FE167FETF2T2FT160FETF2T2FT160FETF2T2FT160FEFT2F2TFT159FEFT2F2TFT159F"
                            "EFT2F2TFT105F"},
                    {.key    = "deploy APOGEE",
                     .length = 1176,
                     .rle    = "71F8T160F8T160F8T160F8T160F8T160F8T160F8T89F"},
                    {.key    = "deploy ALTITUDE",
                     .length = 1176,
                     .rle = "104FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT130FTFT2FTFT2FTFT2FTFT2FTFT2"
                            "FTFT2FTFT2FTFT130FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT130FTFT2FTFT2F"
                            "TFT2FTFT2FTFT2FTFT2FTFT2FTFT130FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT"
                            "130FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT130FTFT2FTFT2FTFT2FTFT2FTFT2"
                            "FTFT2FTFT2FTFT26F"},
                    {.key    = "deploy LOWER_STAGE_SEPARATION",
                     .length = 1176,
                     .rle    = "63FEF3T163FE4F3T160FE4F3T160FE4F3T160FE167FE167FE104F"},
                    {.key = "deploy NEVER", .length = 1176, .rle = "1176F"},
                    {.key = "separation LAUNCH", .length = 336, .rle = "8T160F8T160F"},
                    {.key = "separation IGNITION", .length = 336, .rle = "8F3E6T159F3E6F6T145F"},
                    {.key = "separation BURNOUT", .length = 336, .rle = "39F3E6T159F3E6F6T114F"},
                    {.key = "separation EJECTION", .length = 336, .rle = "54F3E3T162F3E3F3T105F"},
                    {.key = "separation UPPER_IGNITION", .length = 336, .rle = "8F3E165F3E6T151F"},
                    {.key    = "separation ALTITUDE_ASCENDING",
                     .length = 336,
                     .rle = "105F2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T131F2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T26"
                            "F"},
                    {.key = "separation APOGEE", .length = 336, .rle = "71F8T160F8T89F"},
                    {.key    = "separation ALTITUDE_DESCENDING",
                     .length = 336,
                     .rle = "104FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT130FTFT2FTFT2FTFT2FTFT2FTFT2"
                            "FTFT2FTFT2FTFT26F"},
                    {.key = "separation NEVER", .length = 336, .rle = "336F"},
                    {.key = "testForIgnition 0", .length = 336, .rle = "8T160F8T160F"},
                    {.key = "testForIgnition 1", .length = 336, .rle = "8T160F8T160F"},
                }},
    };
    return kRockets;
}

/// The run-length encoding of the probe: a count (omitted for 1) before each character.
[[nodiscard]] std::string runLengthEncoded(std::string_view text)
{
    std::string encoded;
    std::size_t i = 0;
    while (i < text.size())
    {
        std::size_t j = i;
        while (j < text.size() && text.at(j) == text.at(i))
        {
            j++;
        }
        if (j - i > 1)
        {
            encoded += std::to_string(j - i);
        }
        encoded += text.at(i);
        i = j;
    }
    return encoded;
}

/// 'T' or 'F' for the answer of @p test, 'E' when it throws BugError (where Java throws).
template <class Test>
[[nodiscard]] char answerOf(const Test& test)
{
    try
    {
        return test() ? 'T' : 'F';
    }
    catch (const BugError&)
    {
        return 'E';
    }
}

/// The tables of the probe, computed here: every enum constant against every event type from
/// every representative source, with and without data, on one test rocket.
class ActivationTables
{
public:
    explicit ActivationTables(const JavaRocket& java) : m_rocket(makeRocket(java.maker))
    {
        if (java.selectFcid >= 0)
        {
            m_rocket->setSelectedConfiguration(QtRocket::Test::testFcid(java.selectFcid));
        }
        collectComponents();
        makeEvents();
    }

    [[nodiscard]] const std::string& components() const noexcept { return m_signature; }
    [[nodiscard]] std::size_t        representatives() const noexcept { return m_reps.size(); }
    [[nodiscard]] std::size_t        stages() const noexcept { return m_stages.size(); }
    [[nodiscard]] std::size_t        states() const noexcept { return m_states.size(); }
    [[nodiscard]] std::size_t        events() const noexcept { return m_events.size(); }

    /// Every table, by the key the probe prints.
    [[nodiscard]] std::map<std::string, std::string, std::less<>> all()
    {
        std::map<std::string, std::string, std::less<>> tables;
        tables.emplace("events", eventsMade());
        addIgnitionTables(tables);
        for (const DeployEvent deploy : DeploymentConfiguration::kAllDeployEvents)
        {
            tables.emplace("deploy " + std::string{deployEventName(deploy)}, deployTable(deploy));
        }
        for (const SeparationEvent separation : StageSeparationConfiguration::kAllSeparationEvents)
        {
            tables.emplace("separation " + std::string{separationEventName(separation)},
                           separationTable(separation));
        }
        tables.emplace("testForIgnition 0", testForIgnitionTable(false));
        tables.emplace("testForIgnition 1", testForIgnitionTable(true));
        return tables;
    }

private:
    [[nodiscard]] static std::unique_ptr<Rocket> makeRocket(std::string_view maker)
    {
        for (const QtRocket::Test::TestRocketMaker& candidate : QtRocket::Test::testRocketMakers())
        {
            if (candidate.method == maker)
            {
                return candidate.make();
            }
        }
        QtRocket::bug("no test rocket maker " + std::string{maker});
    }

    [[nodiscard]] FlightConfiguration& config() { return m_rocket->getSelectedConfiguration(); }

    /// The stages, pod sets, motor mounts and recovery devices of the rocket, in tree order,
    /// and the motor states of the mounts that hold a motor in the selected configuration.
    void collectComponents()
    {
        const FlightConfigurationId fcid = config().getFlightConfigurationId();
        for (const RocketComponent& component : std::as_const(*m_rocket).subtree(true))
        {
            const auto* mount = dynamic_cast<const MotorMount*>(&component);
            if (dynamic_cast<const ComponentAssembly*>(&component) == nullptr && mount == nullptr &&
                dynamic_cast<const RecoveryDevice*>(&component) == nullptr)
            {
                continue;
            }
            m_reps.push_back(&component);
            if (!m_signature.empty())
            {
                m_signature += ',';
            }
            m_signature += className(component.kind());
            m_signature += std::to_string(component.getStageNumber());
            if (const auto* stage = dynamic_cast<const AxialStage*>(&component))
            {
                m_stages.push_back(stage);
            }
            if (mount != nullptr && !mount->getMotorConfig(fcid).isEmpty())
            {
                m_states.push_back(
                    std::make_shared<MotorClusterState>(mount->getMotorConfig(fcid)));
                m_signature += '*';
            }
        }
    }

    /// The kinds of data the probe gives an event of @p type, "no data" first.
    [[nodiscard]] std::vector<FlightEvent::Data> payloads(Type type) const
    {
        std::vector<FlightEvent::Data> payloads{FlightEvent::Data{}};
        switch (type)
        {
            case Type::IGNITION:
            case Type::BURNOUT:
            case Type::EJECTION_CHARGE:
                payloads.insert(payloads.end(), m_states.begin(), m_states.end());
                break;
            case Type::ALTITUDE:
                payloads.emplace_back(Altitudes{.previous = 250, .current = 150});
                payloads.emplace_back(Altitudes{.previous = 150, .current = 250});
                payloads.emplace_back(Altitudes{.previous = 200, .current = 200});
                payloads.emplace_back(Altitudes{.previous = 300, .current = 250});
                break;
            case Type::SIM_WARN:
                payloads.push_back(FlightEvent::warningData(Warning::kListenersAffected));
                break;
            case Type::SIM_ABORT:
                payloads.emplace_back(SimulationAbort{SimulationAbort::Cause::NO_LIFTOFF});
                break;
            default:
                break;
        }
        return payloads;
    }

    /// Every type from every source (none first) with every kind of data; an event the
    /// constructor refuses is recorded as such.
    void makeEvents()
    {
        std::vector<const RocketComponent*> sources{nullptr};
        sources.insert(sources.end(), m_reps.begin(), m_reps.end());
        for (const Type type : FlightEvent::kAllTypes)
        {
            for (const RocketComponent* source : sources)
            {
                for (const FlightEvent::Data& payload : payloads(type))
                {
                    Result<FlightEvent> event = FlightEvent::create(type, 1.0, source, payload);
                    m_events.push_back(event.has_value()
                                           ? std::optional<FlightEvent>{std::move(*event)}
                                           : std::nullopt);
                }
            }
        }
    }

    [[nodiscard]] std::string eventsMade() const
    {
        std::string made;
        for (const std::optional<FlightEvent>& event : m_events)
        {
            made += event.has_value() ? '.' : 'X';
        }
        return made;
    }

    /// The answers of @p test for every made event, appended to @p table.
    template <class Test>
    void appendAnswers(std::string& table, const Test& test) const
    {
        for (const std::optional<FlightEvent>& event : m_events)
        {
            if (event.has_value())
            {
                table += answerOf([&] { return test(*event); });
            }
        }
    }

    [[nodiscard]] std::string ignitionTable(IgnitionEvent ignition, bool onlyTopStage)
    {
        if (onlyTopStage)
        {
            config().setOnlyStage(0);
        }
        std::string table;
        for (const RocketComponent* target : m_reps)
        {
            appendAnswers(table, [&](const FlightEvent& event) {
                return Activation::isActivationEvent(ignition, config(), event, *target);
            });
        }
        if (onlyTopStage)
        {
            config().setAllStages();
        }
        return table;
    }

    void addIgnitionTables(std::map<std::string, std::string, std::less<>>& tables)
    {
        for (const IgnitionEvent ignition : QtRocket::kAllIgnitionEvents)
        {
            const std::string key = "ignition " + std::string{QtRocket::name(ignition)};
            tables.emplace(key + " 0", ignitionTable(ignition, false));
            if (ignition == IgnitionEvent::AUTOMATIC)
            {
                tables.emplace(key + " 1", ignitionTable(ignition, true));
            }
        }
    }

    [[nodiscard]] std::string deployTable(DeployEvent deploy) const
    {
        DeploymentConfiguration deployment;
        deployment.setDeployEvent(deploy);
        std::string table;
        for (const RocketComponent* target : m_reps)
        {
            appendAnswers(table, [&](const FlightEvent& event) {
                return Activation::isActivationEvent(deploy, deployment, event, *target);
            });
        }
        // The overload that takes the event from the configuration gives the same table.
        std::string viaConfiguration;
        for (const RocketComponent* target : m_reps)
        {
            appendAnswers(viaConfiguration, [&](const FlightEvent& event) {
                return Activation::isActivationEvent(deployment, event, *target);
            });
        }
        EXPECT_EQ(viaConfiguration, table) << deployEventName(deploy);
        return table;
    }

    [[nodiscard]] std::string separationTable(SeparationEvent separation) const
    {
        StageSeparationConfiguration configuration;
        configuration.setSeparationEvent(separation);
        std::string table;
        for (const AxialStage* stage : m_stages)
        {
            appendAnswers(table, [&](const FlightEvent& event) {
                return Activation::isSeparationEvent(separation, configuration, event, *stage);
            });
        }
        return table;
    }

    [[nodiscard]] std::string testForIgnitionTable(bool onlyTopStage)
    {
        if (onlyTopStage)
        {
            config().setOnlyStage(0);
        }
        std::string table;
        for (const std::shared_ptr<MotorClusterState>& state : m_states)
        {
            appendAnswers(table, [&](const FlightEvent& event) {
                return Activation::testForIgnition(*state, config(), event);
            });
        }
        if (onlyTopStage)
        {
            config().setAllStages();
        }
        return table;
    }

    std::unique_ptr<Rocket>                         m_rocket;
    std::vector<const RocketComponent*>             m_reps;
    std::vector<const AxialStage*>                  m_stages;
    std::vector<std::shared_ptr<MotorClusterState>> m_states;
    std::string                                     m_signature;
    /// nullopt for an event the constructor refuses.
    std::vector<std::optional<FlightEvent>> m_events;
};

/// Expects the tables computed here to be the probe's.
void expectTables(const std::map<std::string, std::string, std::less<>>& computed,
                  const std::vector<JavaTable>&                          expected)
{
    EXPECT_EQ(computed.size(), expected.size());
    for (const JavaTable& table : expected)
    {
        SCOPED_TRACE(table.key);
        const auto found = computed.find(table.key);
        ASSERT_NE(found, computed.end());
        EXPECT_EQ(found->second.size(), table.length);
        EXPECT_EQ(runLengthEncoded(found->second), table.rle);
    }
}

class FlightEventActivationAgainstJava : public ::testing::TestWithParam<std::size_t>
{ };

TEST_P(FlightEventActivationAgainstJava, TheTruthTablesAreJavas)
{
    const JavaRocket& java = javaRockets().at(GetParam());
    SCOPED_TRACE(java.maker);
    ActivationTables tables(java);

    // The same components, in the same order, as the probe saw.
    ASSERT_EQ(tables.components(), java.components);
    ASSERT_EQ(tables.representatives(), java.representatives);
    ASSERT_EQ(tables.stages(), java.stages);
    ASSERT_EQ(tables.states(), java.states);
    ASSERT_EQ(tables.events(), java.events);
    EXPECT_EQ(java.tables.size(), 24U)
        << "1 of the events, 6 of ignition, 6 of deployment, 9 of separation, 2 of the states";
    expectTables(tables.all(), java.tables);
}

INSTANTIATE_TEST_SUITE_P(Rockets, FlightEventActivationAgainstJava,
                         ::testing::Range<std::size_t>(0, 5));

TEST(FlightEventActivationAgainstJava, TheTablesCoverFiveRocketsThreeOfThemWithParallelStages)
{
    ASSERT_EQ(javaRockets().size(), 5U);
    std::size_t withParallelStages = 0;
    std::size_t entries            = 0;
    for (const JavaRocket& rocket : javaRockets())
    {
        withParallelStages += rocket.components.contains("ParallelStage") ? 1U : 0U;
        for (const JavaTable& table : rocket.tables)
        {
            entries += table.length;
        }
    }
    EXPECT_EQ(withParallelStages, 3U);
    EXPECT_GT(entries, 100000U);
}

TEST(FlightEventActivationAgainstJava, TheRunLengthEncodingIsTheProbes)
{
    EXPECT_EQ(runLengthEncoded(""), "");
    EXPECT_EQ(runLengthEncoded("T"), "T");
    EXPECT_EQ(runLengthEncoded("FFFTTE"), "3F2TE");
    EXPECT_EQ(runLengthEncoded("FTFTFFFFFFFFFFFF"), "FTFT12F");
}

}  // namespace
