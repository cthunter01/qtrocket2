#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/mass/MotorClusterState.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentAssembly.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/FlightEventActivation.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "rocket/TestRockets.h"
#include "simulation/SimulationTestSupport.h"

namespace
{

using QtRocket::AxialStage;
using QtRocket::BodyTube;
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
using QtRocket::Parachute;
using QtRocket::ParallelStage;
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

/// The component of @p copy that has the id of @p original (a copy made with
/// copyRocketWithOriginalId() keeps the ids).
[[nodiscard]] const RocketComponent& inCopy(const Rocket& copy, const RocketComponent& original)
{
    const RocketComponent* found = copy.findComponent(original.getId());
    if (found == nullptr)
    {
        QtRocket::bug("the copy has no component with that id");
    }
    return *found;
}

/// Whether @p event ignites a motor in @p target whose ignition is set to EJECTION_CHARGE.
[[nodiscard]] bool chargeIgnites(const FlightConfiguration& config, const FlightEvent& event,
                                 const RocketComponent& target)
{
    return Activation::isActivationEvent(IgnitionEvent::EJECTION_CHARGE, config, event, target);
}

/// Whether @p event ignites a motor in @p target whose ignition is set to BURNOUT.
[[nodiscard]] bool burnoutIgnites(const FlightConfiguration& config, const FlightEvent& event,
                                  const RocketComponent& target)
{
    return Activation::isActivationEvent(IgnitionEvent::BURNOUT, config, event, target);
}

TEST(IgnitionActivation, TheStageRightBelowIsFoundByStructureNotByStageNumber)
{
    // Java: probes/events-data-fix/FixProbe.java, "structure". A booster set P added to the body
    // of the first stage before the second core stage S1 is: the stage numbers are S0 = 0,
    // P = 1, S1 = 2, and both P and S1 are right below S0. "The stage with the next number"
    // would ignite P's motor on S1's charge.
    Rocket         rocket;
    AxialStage&    s0 = rocket.addChild(std::make_unique<AxialStage>());
    BodyTube&      b0 = s0.addChild(std::make_unique<BodyTube>(0.2, 0.02));
    ParallelStage& p  = b0.addChild(std::make_unique<ParallelStage>());
    BodyTube&      pb = p.addChild(std::make_unique<BodyTube>(0.1, 0.01));
    AxialStage&    s1 = rocket.addChild(std::make_unique<AxialStage>());
    BodyTube&      b1 = s1.addChild(std::make_unique<BodyTube>(0.2, 0.02));
    rocket.enableEvents();
    rocket.getSelectedConfiguration().setAllStages();
    const FlightConfiguration& config = rocket.getSelectedConfiguration();

    ASSERT_EQ(s0.getStageNumber(), 0);
    ASSERT_EQ(p.getStageNumber(), 1);
    ASSERT_EQ(s1.getStageNumber(), 2);
    ASSERT_EQ(p.getUpperStage(), &s0);
    ASSERT_EQ(s1.getUpperStage(), &s0);
    ASSERT_EQ(s0.getUpperStage(), nullptr);

    const FlightEvent chargeOfS1{Type::EJECTION_CHARGE, 1.0, &s1};
    const FlightEvent chargeOfP{Type::EJECTION_CHARGE, 1.0, &p};
    const FlightEvent chargeOfS0{Type::EJECTION_CHARGE, 1.0, &s0};
    const FlightEvent burnoutInS1{Type::BURNOUT, 1.0, &b1};
    const FlightEvent burnoutInP{Type::BURNOUT, 1.0, &pb};

    EXPECT_TRUE(chargeIgnites(config, chargeOfS1, b0));
    EXPECT_FALSE(chargeIgnites(config, chargeOfS1, pb))
        << "P has the stage number before S1's, but S1 is not below it";
    EXPECT_FALSE(chargeIgnites(config, chargeOfS1, b1));

    EXPECT_TRUE(chargeIgnites(config, chargeOfP, b0));
    EXPECT_FALSE(chargeIgnites(config, chargeOfP, pb));
    EXPECT_FALSE(chargeIgnites(config, chargeOfP, b1));

    EXPECT_FALSE(chargeIgnites(config, chargeOfS0, b0));
    EXPECT_FALSE(chargeIgnites(config, chargeOfS0, pb))
        << "P has the stage number after S0's, but S0 is above it";
    EXPECT_FALSE(chargeIgnites(config, chargeOfS0, b1));

    EXPECT_TRUE(burnoutIgnites(config, burnoutInS1, b0));
    EXPECT_FALSE(burnoutIgnites(config, burnoutInS1, pb));
    EXPECT_FALSE(burnoutIgnites(config, burnoutInS1, b1));

    EXPECT_TRUE(burnoutIgnites(config, burnoutInP, b0));
    EXPECT_FALSE(burnoutIgnites(config, burnoutInP, pb));
    EXPECT_FALSE(burnoutIgnites(config, burnoutInP, b1));
}

TEST(IgnitionActivation, ASourceInACopyOfTheRocketIsMatchedAsItsOriginal)
{
    // Java: FixProbe.java, "sources in a copy of the rocket". The stages are compared with
    // equals() (the class and the id), so the events of a copy find the targets of the original.
    EventTestRocket               r;
    const FlightConfiguration&    config = configOf(r);
    const std::unique_ptr<Rocket> copy   = r.rocket.copyRocketWithOriginalId();
    ASSERT_NE(&inCopy(*copy, *r.booster), r.booster);
    ASSERT_TRUE(inCopy(*copy, *r.booster).equals(*r.booster));

    const FlightEvent boosterCharge{Type::EJECTION_CHARGE, 1.0, &inCopy(*copy, *r.booster)};
    const FlightEvent strapOnCharge{Type::EJECTION_CHARGE, 1.0, &inCopy(*copy, *r.strapOns)};
    const FlightEvent ownCharge{Type::EJECTION_CHARGE, 1.0, &inCopy(*copy, *r.sustainer)};
    const FlightEvent boosterBurnout{Type::BURNOUT, 1.0, &inCopy(*copy, *r.boosterBody)};

    EXPECT_TRUE(Activation::isActivationEvent(IgnitionEvent::EJECTION_CHARGE, config, boosterCharge,
                                              *r.sustainerMount));
    EXPECT_FALSE(Activation::isActivationEvent(IgnitionEvent::EJECTION_CHARGE, config,
                                               strapOnCharge, *r.sustainerMount));
    EXPECT_FALSE(Activation::isActivationEvent(IgnitionEvent::EJECTION_CHARGE, config, ownCharge,
                                               *r.sustainerMount));
    EXPECT_TRUE(Activation::isActivationEvent(IgnitionEvent::EJECTION_CHARGE, config, strapOnCharge,
                                              *r.boosterBody));
    EXPECT_FALSE(Activation::isActivationEvent(IgnitionEvent::EJECTION_CHARGE, config,
                                               boosterCharge, *r.boosterBody));
    EXPECT_FALSE(Activation::isActivationEvent(IgnitionEvent::EJECTION_CHARGE, config,
                                               boosterCharge, *r.strapOnBody));
    EXPECT_TRUE(Activation::isActivationEvent(IgnitionEvent::BURNOUT, config, boosterBurnout,
                                              *r.sustainerMount));
    EXPECT_FALSE(Activation::isActivationEvent(IgnitionEvent::BURNOUT, config, boosterBurnout,
                                               *r.boosterBody));
    EXPECT_TRUE(Activation::isActivationEvent(IgnitionEvent::AUTOMATIC, config, boosterCharge,
                                              *r.sustainerMount));
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

TEST(IgnitionActivation, AutomaticWithoutAnActiveCoreStageIsNotInTheLaunchStage)
{
    // The documented deviation (see AxialStage::isLaunchStage()): with only the strap-ons
    // active no core stage is active, and Java throws NullPointerException for a target in a
    // core stage (FixProbe.java, "AUTOMATIC without an active core stage"). Here such a target
    // is not in the launch stage, and AUTOMATIC is EJECTION_CHARGE for it.
    EventTestRocket      r;
    FlightConfiguration& config = configOf(r);
    config.setOnlyStage(2);
    ASSERT_FALSE(config.isStageActive(0));
    ASSERT_FALSE(config.isStageActive(1));
    ASSERT_TRUE(config.isStageActive(2));
    const FlightEvent launch{Type::LAUNCH, 0.0, &r.rocket};
    const FlightEvent boosterCharge{Type::EJECTION_CHARGE, 1.0, r.booster};

    EXPECT_FALSE(
        Activation::isActivationEvent(IgnitionEvent::AUTOMATIC, config, launch, *r.sustainerMount))
        << "Java: NullPointerException";
    EXPECT_TRUE(Activation::isActivationEvent(IgnitionEvent::AUTOMATIC, config, boosterCharge,
                                              *r.sustainerMount))
        << "Java: NullPointerException";
    // A parallel stage asks only whether it is active itself: as in Java.
    EXPECT_TRUE(
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

TEST(DeployActivation, AnEjectionChargeWithAMotorOfACopyDeploysTheDevicesOfTheSameAssembly)
{
    // Java: FixProbe.java, "pods". The assemblies are compared with equals(), so the motor of a
    // copy of the rocket is in the assembly of the original's parachute, or is not.
    const QtRocket::Test::TestEstesAlphaIIIWithMotorPods r;
    const FlightConfigurationId                          fcid = QtRocket::Test::testFcid(0);
    const DeploymentConfiguration                        config;
    const std::unique_ptr<Rocket> copy = r.rocket->copyRocketWithOriginalId();
    const auto* innerOfCopy            = dynamic_cast<const MotorMount*>(&inCopy(*copy, *r.inner));
    const auto* podMountOfCopy = dynamic_cast<const MotorMount*>(&inCopy(*copy, *r.podMount));
    ASSERT_NE(innerOfCopy, nullptr);
    ASSERT_NE(podMountOfCopy, nullptr);
    const RocketComponent* stageOfCopy = &inCopy(*copy, *r.stage);
    const auto bodyMotor = std::make_shared<MotorClusterState>(innerOfCopy->getMotorConfig(fcid));
    const auto podMotor = std::make_shared<MotorClusterState>(podMountOfCopy->getMotorConfig(fcid));
    ASSERT_NE(&asComponent(bodyMotor->getMount()), r.inner) << "the mount of the copy";

    EXPECT_TRUE(Activation::isActivationEvent(DeployEvent::EJECTION, config,
                                              {Type::EJECTION_CHARGE, 1.0, stageOfCopy, bodyMotor},
                                              *r.chute));
    EXPECT_FALSE(Activation::isActivationEvent(DeployEvent::EJECTION, config,
                                               {Type::EJECTION_CHARGE, 1.0, stageOfCopy, podMotor},
                                               *r.chute));
    EXPECT_TRUE(Activation::isActivationEvent(DeployEvent::EJECTION, config,
                                              {Type::EJECTION_CHARGE, 1.0, stageOfCopy}, *r.chute))
        << "without the state: the stage number of the copy's stage";
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

/// The answers ("T" or "F" each) of DeployEvent::ALTITUDE at the deploy altitude @p altitude
/// for altitude events with the changes @p changes, through both overloads.
[[nodiscard]] std::string deployAnswers(const EventTestRocket& r, double altitude,
                                        std::span<const Altitudes> changes)
{
    DeploymentConfiguration config;
    config.setDeployEvent(DeployEvent::ALTITUDE);
    config.setDeployAltitude(altitude);
    std::string answers;
    for (const Altitudes& change : changes)
    {
        const FlightEvent event{Type::ALTITUDE, 1.0, &r.rocket, change};
        const bool        viaEnum =
            Activation::isActivationEvent(DeployEvent::ALTITUDE, config, event, *r.chute);
        EXPECT_EQ(Activation::isActivationEvent(config, event, *r.chute), viaEnum);
        answers += viaEnum ? 'T' : 'F';
    }
    return answers;
}

TEST(DeployActivation, TheDeployAltitudeIsTheConfigurations)
{
    // Java: FixProbe.java, "deploy ALTITUDE 123". 200 m is only the default.
    const EventTestRocket r;
    ASSERT_EQ(DeploymentConfiguration{}.getDeployAltitude(), 200.0);
    const std::array<Altitudes, 7> changes{
        Altitudes{.previous = 250, .current = 150}, Altitudes{.previous = 130, .current = 120},
        Altitudes{.previous = 123, .current = 123}, Altitudes{.previous = 123, .current = 100},
        Altitudes{.previous = 150, .current = 123}, Altitudes{.previous = 122, .current = 100},
        Altitudes{.previous = 120, .current = 130}};
    EXPECT_EQ(deployAnswers(r, 123.0, changes), "FTTTTFF");
    // The same events at the default altitude: only the first goes through 200 m.
    EXPECT_EQ(deployAnswers(r, 200.0, changes), "TFFFFFF");
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

/// The answers ("T" or "F" each) of @p separation at the separation altitude @p altitude for
/// altitude events with the changes @p changes.
[[nodiscard]] std::string separationAnswers(const EventTestRocket& r, SeparationEvent separation,
                                            double altitude, std::span<const Altitudes> changes)
{
    StageSeparationConfiguration config;
    config.setSeparationAltitude(altitude);
    std::string answers;
    for (const Altitudes& change : changes)
    {
        const FlightEvent event{Type::ALTITUDE, 1.0, &r.rocket, change};
        answers += Activation::isSeparationEvent(separation, config, event, *r.booster) ? 'T' : 'F';
    }
    return answers;
}

TEST(SeparationActivation, TheSeparationAltitudeIsTheConfigurations)
{
    // Java: FixProbe.java, "separation ALTITUDE_ASCENDING 77" and "... DESCENDING 77". 200 m is
    // only the default.
    const EventTestRocket r;
    ASSERT_EQ(StageSeparationConfiguration{}.getSeparationAltitude(), 200.0);
    const std::array<Altitudes, 8> changes{
        Altitudes{.previous = 150, .current = 250}, Altitudes{.previous = 70, .current = 80},
        Altitudes{.previous = 77, .current = 77},   Altitudes{.previous = 77, .current = 100},
        Altitudes{.previous = 50, .current = 77},   Altitudes{.previous = 78, .current = 100},
        Altitudes{.previous = 80, .current = 70},   Altitudes{.previous = 250, .current = 150}};
    EXPECT_EQ(separationAnswers(r, SeparationEvent::ALTITUDE_ASCENDING, 77.0, changes), "FTTTTFFF");
    EXPECT_EQ(separationAnswers(r, SeparationEvent::ALTITUDE_DESCENDING, 77.0, changes),
              "FFTFFFTF");
    // The same events at the default altitude.
    EXPECT_EQ(separationAnswers(r, SeparationEvent::ALTITUDE_ASCENDING, 200.0, changes),
              "TFFFFFFF");
    EXPECT_EQ(separationAnswers(r, SeparationEvent::ALTITUDE_DESCENDING, 200.0, changes),
              "FFFFFFFT");
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
    EXPECT_FALSE(Activation::isSeparationEvent(SeparationEvent::ALTITUDE_ASCENDING, config,
                                               {Type::APOGEE, 1.0, &r.rocket}, *r.booster));
    // Data of another kind (Java: ClassCastException; FixProbe.java, "with text").
    const FlightEvent text{Type::ALTITUDE, 1.0, &r.rocket, std::string{"not altitudes"}};
    EXPECT_THROW(static_cast<void>(Activation::isSeparationEvent(
                     SeparationEvent::ALTITUDE_ASCENDING, config, text, *r.booster)),
                 BugError);
    EXPECT_THROW(static_cast<void>(Activation::isSeparationEvent(
                     SeparationEvent::ALTITUDE_DESCENDING, config, text, *r.booster)),
                 BugError);
}

// ====================================================================== the tables of Java

/// One truth table of the Java probe (probes/events-data-fix/ActivationProbe.java, which extends
/// probes/events-data-impl/ActivationProbe.java): which function and constant it is of, its
/// number of entries, and the entries, run-length encoded ("3F2TE" is FFFTTE): T and F for the
/// answers, E where Java throws. A key that ends in " 250" is the table at a deploy or
/// separation altitude of 250 m in place of the default 200 m.
struct JavaTable
{
    std::string_view key;
    std::size_t      length;
    std::string_view rle;
};

/// What the probe printed for one test rocket. The probe computes every table twice: with the
/// event sources and the motor states taken from the rocket itself, and taken from a copy of it
/// (copyWithOriginalID()) while the targets and the flight configuration stay the original's.
/// Java gives the same tables both ways for each of these rockets ("copy identical" in
/// activation.out): where it compares components with equals(), a component of a copy matches.
struct JavaRocket
{
    /// The maker of TestRockets ("makeBeta"), or of the two rockets built below.
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
                    {.key    = "deploy ALTITUDE 250",
                     .length = 1472,
                     .rle = "112FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT140FT2FTFT2FTFT2FTFT2F"
                            "TFT2FTFT2FTFT2FTFT2FTFT2FT140FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTF"
                            "T2FT140FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT140FT2FTFT2FTFT2FT"
                            "FT2FTFT2FTFT2FTFT2FTFT2FTFT2FT140FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT"
                            "2FTFT2FT140FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT140FT2FTFT2FTF"
                            "T2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT28F"},
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
                    {.key    = "separation ALTITUDE_ASCENDING 250",
                     .length = 368,
                     .rle    = "113FT4FT4FT4FT4FT4FT4FT4FT4FT143FT4FT4FT4FT4FT4FT4FT4FT4FT30F"},
                    {.key = "separation APOGEE", .length = 368, .rle = "75F9T175F9T100F"},
                    {.key    = "separation ALTITUDE_DESCENDING",
                     .length = 368,
                     .rle = "112FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT141FTFT2FTFT2FTFT2FTFT2"
                            "FTFT2FTFT2FTFT2FTFT2FTFT29F"},
                    {.key    = "separation ALTITUDE_DESCENDING 250",
                     .length = 368,
                     .rle = "112FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT140FT2FTFT2FTFT2FTFT2F"
                            "TFT2FTFT2FTFT2FTFT2FTFT2FT28F"},
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
                    {.key    = "deploy ALTITUDE 250",
                     .length = 2717,
                     .rle = "151FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT188FT2F"
                            "TFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT188FT2FTFT2FTF"
                            "T2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT188FT2FTFT2FTFT2FTFT2"
                            "FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT188FT2FTFT2FTFT2FTFT2FTFT2FT"
                            "FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT188FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT"
                            "2FTFT2FTFT2FTFT2FTFT2FTFT2FT188FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2F"
                            "TFT2FTFT2FTFT2FTFT2FT188FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTF"
                            "T2FTFT2FTFT2FT188FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2"
                            "FTFT2FT188FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT"
                            "188FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT37F"},
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
                    {.key    = "separation ALTITUDE_ASCENDING 250",
                     .length = 741,
                     .rle = "152FT4FT4FT4FT4FT4FT4FT4FT4FT4FT4FT4FT191FT4FT4FT4FT4FT4FT4FT4FT4FT4FT"
                            "4FT4FT191FT4FT4FT4FT4FT4FT4FT4FT4FT4FT4FT4FT39F"},
                    {.key = "separation APOGEE", .length = 741, .rle = "102F12T235F12T235F12T133F"},
                    {.key    = "separation ALTITUDE_DESCENDING",
                     .length = 741,
                     .rle = "151FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT189FTFT2"
                            "FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT189FTFT2FTFT2FTF"
                            "T2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT38F"},
                    {.key    = "separation ALTITUDE_DESCENDING 250",
                     .length = 741,
                     .rle = "151FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT188FT2F"
                            "TFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT188FT2FTFT2FTF"
                            "T2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT37F"},
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
                    {.key    = "deploy ALTITUDE 250",
                     .length = 1881,
                     .rle = "129FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT160FT2FTFT2FTFT2F"
                            "TFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT160FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTF"
                            "T2FTFT2FTFT2FTFT2FT160FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2"
                            "FT160FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT160FT2FTFT2FTFT"
                            "2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT160FT2FTFT2FTFT2FTFT2FTFT2FTFT2F"
                            "TFT2FTFT2FTFT2FTFT2FT160FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTF"
                            "T2FT160FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT31F"},
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
                    {.key    = "separation ALTITUDE_ASCENDING 250",
                     .length = 627,
                     .rle = "130FT4FT4FT4FT4FT4FT4FT4FT4FT4FT163FT4FT4FT4FT4FT4FT4FT4FT4FT4FT163FT4"
                            "FT4FT4FT4FT4FT4FT4FT4FT4FT33F"},
                    {.key = "separation APOGEE", .length = 627, .rle = "88F10T199F10T199F10T111F"},
                    {.key    = "separation ALTITUDE_DESCENDING",
                     .length = 627,
                     .rle = "129FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT161FTFT2FTFT2FTFT2"
                            "FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT161FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTF"
                            "T2FTFT2FTFT2FTFT32F"},
                    {.key    = "separation ALTITUDE_DESCENDING 250",
                     .length = 627,
                     .rle = "129FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT160FT2FTFT2FTFT2F"
                            "TFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT160FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTF"
                            "T2FTFT2FTFT2FTFT2FT31F"},
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
                    {.key    = "deploy ALTITUDE 250",
                     .length = 1448,
                     .rle = "109FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT137FT2FTFT2FTFT2FTFT2F"
                            "TFT2FTFT2FTFT2FTFT2FTFT2FT137FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTF"
                            "T2FT137FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT137FT2FTFT2FTFT2FT"
                            "FT2FTFT2FTFT2FTFT2FTFT2FTFT2FT137FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT"
                            "2FTFT2FT137FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT137FT2FTFT2FTF"
                            "T2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT28F"},
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
                    {.key    = "separation ALTITUDE_ASCENDING 250",
                     .length = 181,
                     .rle    = "110FT4FT4FT4FT4FT4FT4FT4FT4FT30F"},
                    {.key = "separation APOGEE", .length = 181, .rle = "72F9T100F"},
                    {.key    = "separation ALTITUDE_DESCENDING",
                     .length = 181,
                     .rle    = "109FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT29F"},
                    {.key    = "separation ALTITUDE_DESCENDING 250",
                     .length = 181,
                     .rle    = "109FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT28F"},
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
                    {.key    = "deploy ALTITUDE 250",
                     .length = 1176,
                     .rle = "104FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT129FT2FTFT2FTFT2FTFT2FTFT2F"
                            "TFT2FTFT2FTFT2FT129FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT129FT2FTFT2"
                            "FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT129FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT"
                            "FT2FT129FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT129FT2FTFT2FTFT2FTFT2F"
                            "TFT2FTFT2FTFT2FTFT2FT25F"},
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
                    {.key    = "separation ALTITUDE_ASCENDING 250",
                     .length = 336,
                     .rle    = "105FT4FT4FT4FT4FT4FT4FT4FT132FT4FT4FT4FT4FT4FT4FT4FT27F"},
                    {.key = "separation APOGEE", .length = 336, .rle = "71F8T160F8T89F"},
                    {.key    = "separation ALTITUDE_DESCENDING",
                     .length = 336,
                     .rle = "104FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT130FTFT2FTFT2FTFT2FTFT2FTFT2"
                            "FTFT2FTFT2FTFT26F"},
                    {.key    = "separation ALTITUDE_DESCENDING 250",
                     .length = 336,
                     .rle = "104FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT129FT2FTFT2FTFT2FTFT2FTFT2F"
                            "TFT2FTFT2FTFT2FT25F"},
                    {.key = "separation NEVER", .length = 336, .rle = "336F"},
                    {.key = "testForIgnition 0", .length = 336, .rle = "8T160F8T160F"},
                    {.key = "testForIgnition 1", .length = 336, .rle = "8T160F8T160F"},
                }},
        JavaRocket{
            .maker      = "makeBoosterSetBeforeSecondStage",
            .selectFcid = 0,
            .components =
                "Rocket-1,AxialStage0,BodyTube0*,Parachute0,ParallelStage1,BodyTube1*,Parachute1,"
                "AxialStage2,BodyTube2*",
            .representatives = 9,
            .stages          = 3,
            .states          = 3,
            .events          = 310,
            .tables =
                {
                    {.key    = "events",
                     .length = 310,
                     .rle = "14.8X4.8X4.8X28.8X4.8X4.8X8.4X4.8X4.8X4.4X110.X.19X.X.X.X.X.X.X.X.X.X1"
                            "1."},
                    {.key    = "ignition AUTOMATIC 0",
                     .length = 1881,
                     .rle = "209E62F4E4F8T193F4E4F8T193F4E4F8T131F10T199F10T199F10T199F10T199F10T19"
                            "9F"},
                    {.key    = "ignition AUTOMATIC 1",
                     .length = 1881,
                     .rle    = "209E10T199F10T199F10T261F4E205F4E205F4E205F4E205F4E143F"},
                    {.key    = "ignition LAUNCH 0",
                     .length = 1881,
                     .rle    = "10T199F10T199F10T199F10T199F10T199F10T199F10T199F10T199F10T199F"},
                    {.key    = "ignition EJECTION_CHARGE 0",
                     .length = 1881,
                     .rle =
                         "62F16E193F4E4F8T193F4E4F8T193F4E4F8T193F4E205F4E205F4E205F4E205F4E143F"},
                    {.key    = "ignition BURNOUT 0",
                     .length = 1881,
                     .rle =
                         "46F16E193F4E4F8T193F4E4F8T193F4E4F8T193F4E205F4E205F4E205F4E205F4E159F"},
                    {.key = "ignition NEVER 0", .length = 1881, .rle = "1881F"},
                    {.key    = "deploy LAUNCH",
                     .length = 1881,
                     .rle    = "10T199F10T199F10T199F10T199F10T199F10T199F10T199F10T199F10T199F"},
                    {.key    = "deploy EJECTION",
                     .length = 1881,
                     .rle = "62FE208FET2F2T3FT3FT195FET2F2T3FT3FT195FET2F2T3FT3FT195FEFT3FTFTFT3FT1"
                            "94FEFT3FTFTFT3FT194FEFT3FTFTFT3FT194FE2FT3FT3F2T2FT193FE2FT3FT3F2T2FT1"
                            "31F"},
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
                    {.key    = "deploy ALTITUDE 250",
                     .length = 1881,
                     .rle = "129FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT160FT2FTFT2FTFT2F"
                            "TFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT160FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTF"
                            "T2FTFT2FTFT2FTFT2FT160FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2"
                            "FT160FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT160FT2FTFT2FTFT"
                            "2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT160FT2FTFT2FTFT2FTFT2FTFT2FTFT2F"
                            "TFT2FTFT2FTFT2FTFT2FT160FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTF"
                            "T2FT160FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT31F"},
                    {.key    = "deploy LOWER_STAGE_SEPARATION",
                     .length = 1881,
                     .rle = "78FEF3T204FE4F3T201FE4F3T201FE4F3T201FE7F2T199FE7F2T199FE7F2T199FE208F"
                            "E130F"},
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
                    {.key    = "separation ALTITUDE_ASCENDING 250",
                     .length = 627,
                     .rle = "130FT4FT4FT4FT4FT4FT4FT4FT4FT4FT163FT4FT4FT4FT4FT4FT4FT4FT4FT4FT163FT4"
                            "FT4FT4FT4FT4FT4FT4FT4FT4FT33F"},
                    {.key = "separation APOGEE", .length = 627, .rle = "88F10T199F10T199F10T111F"},
                    {.key    = "separation ALTITUDE_DESCENDING",
                     .length = 627,
                     .rle = "129FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT161FTFT2FTFT2FTFT2"
                            "FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT161FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTF"
                            "T2FTFT2FTFT2FTFT32F"},
                    {.key    = "separation ALTITUDE_DESCENDING 250",
                     .length = 627,
                     .rle = "129FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT160FT2FTFT2FTFT2F"
                            "TFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT160FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTF"
                            "T2FTFT2FTFT2FTFT2FT31F"},
                    {.key = "separation NEVER", .length = 627, .rle = "627F"},
                    {.key    = "testForIgnition 0",
                     .length = 627,
                     .rle    = "62F4E4F8T177F4E159F10T199F"},
                    {.key = "testForIgnition 1", .length = 627, .rle = "62F4E4F8T177F4E221F4E143F"},
                }},
        JavaRocket{
            .maker      = "makeTwoBoosterSetsOnOneCore",
            .selectFcid = 0,
            .components =
                "Rocket-1,AxialStage0,BodyTube0*,Parachute0,AxialStage1,BodyTube1*,ParallelStage2"
                ",BodyTube2*,ParallelStage3,BodyTube3*,Parachute3",
            .representatives = 11,
            .stages          = 4,
            .states          = 4,
            .events          = 408,
            .tables =
                {
                    {.key    = "events",
                     .length = 408,
                     .rle = "17.10X5.10X5.5X5.5X5.5X29.10X5.10X5.5X5.5X5.5X5.5X5.10X5.5X5.5X5.10X13"
                            "2.X.23X.X.X.X.X.X.X.X.X.X.X.X13."},
                    {.key    = "ignition AUTOMATIC 0",
                     .length = 2948,
                     .rle = "268E86F5E5F5T253F5E5F5T253F5E5F5T167F12T256F12T256F12T256F12T256F12T25"
                            "6F12T256F12T256F"},
                    {.key    = "ignition AUTOMATIC 1",
                     .length = 2948,
                     .rle = "268E12T256F12T256F12T342F5E10F10T243F5E10F10T243F5E263F5E263F5E263F5E2"
                            "63F5E177F"},
                    {.key    = "ignition LAUNCH 0",
                     .length = 2948,
                     .rle = "12T256F12T256F12T256F12T256F12T256F12T256F12T256F12T256F12T256F12T256F"
                            "12T256F"},
                    {.key    = "ignition EJECTION_CHARGE 0",
                     .length = 2948,
                     .rle = "86F25E243F5E5F5T253F5E5F5T253F5E5F5T253F5E10F10T243F5E10F10T243F5E263F"
                            "5E263F5E263F5E263F5E177F"},
                    {.key    = "ignition BURNOUT 0",
                     .length = 2948,
                     .rle = "61F25E243F5E5F5T253F5E5F5T253F5E5F5T253F5E10F10T243F5E10F10T243F5E263F"
                            "5E263F5E263F5E263F5E202F"},
                    {.key = "ignition NEVER 0", .length = 2948, .rle = "2948F"},
                    {.key    = "deploy LAUNCH",
                     .length = 2948,
                     .rle = "12T256F12T256F12T256F12T256F12T256F12T256F12T256F12T256F12T256F12T256F"
                            "12T256F"},
                    {.key    = "deploy EJECTION",
                     .length = 2948,
                     .rle = "86FE267FET3F2T4FT4FT4FT246FET3F2T4FT4FT4FT246FET3F2T4FT4FT4FT246FEFT4F"
                            "T2FTFT4FT4FT245FEFT4FT2FTFT4FT4FT245FE2FT4FT4FTFT2FT4FT244FE2FT4FT4FTF"
                            "T2FT4FT244FE3FT4FT4FT4F2T3FT243FE3FT4FT4FT4F2T3FT243FE3FT4FT4FT4F2T3FT"
                            "157F"},
                    {.key    = "deploy APOGEE",
                     .length = 2948,
                     .rle = "123F12T256F12T256F12T256F12T256F12T256F12T256F12T256F12T256F12T256F12T"
                            "256F12T133F"},
                    {.key    = "deploy ALTITUDE",
                     .length = 2948,
                     .rle = "172FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT210FTFT2"
                            "FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT210FTFT2FTFT2FTF"
                            "T2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT210FTFT2FTFT2FTFT2FTFT2F"
                            "TFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT210FTFT2FTFT2FTFT2FTFT2FTFT2FTFT"
                            "2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT210FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT"
                            "FT2FTFT2FTFT2FTFT2FTFT210FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2"
                            "FTFT2FTFT2FTFT210FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTF"
                            "T2FTFT210FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT21"
                            "0FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT210FTFT2FT"
                            "FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT38F"},
                    {.key    = "deploy ALTITUDE 250",
                     .length = 2948,
                     .rle = "172FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT209FT2F"
                            "TFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT209FT2FTFT2FTF"
                            "T2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT209FT2FTFT2FTFT2FTFT2"
                            "FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT209FT2FTFT2FTFT2FTFT2FTFT2FT"
                            "FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT209FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT"
                            "2FTFT2FTFT2FTFT2FTFT2FTFT2FT209FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2F"
                            "TFT2FTFT2FTFT2FTFT2FT209FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTF"
                            "T2FTFT2FTFT2FT209FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2"
                            "FTFT2FT209FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT"
                            "209FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT37F"},
                    {.key    = "deploy LOWER_STAGE_SEPARATION",
                     .length = 2948,
                     .rle = "111FEF3T263FE4F2T261FE4F2T261FE4F2T261FE6F2T259FE6F2T259FE8F3T256FE8F3"
                            "T256FE267FE267FE156F"},
                    {.key = "deploy NEVER", .length = 2948, .rle = "2948F"},
                    {.key    = "separation LAUNCH",
                     .length = 1072,
                     .rle    = "12T256F12T256F12T256F12T256F"},
                    {.key    = "separation IGNITION",
                     .length = 1072,
                     .rle    = "12F5E5T258F5E5F5T253F5E10F5T248F5E15F5T231F"},
                    {.key    = "separation BURNOUT",
                     .length = 1072,
                     .rle    = "61F5E5T258F5E5F5T253F5E10F5T248F5E15F5T182F"},
                    {.key    = "separation EJECTION",
                     .length = 1072,
                     .rle    = "86F5E5T258F5E5F5T253F5E10F5T248F5E15F5T157F"},
                    {.key    = "separation UPPER_IGNITION",
                     .length = 1072,
                     .rle    = "12F5E263F5E5T258F5E5F5T253F5E10F5T236F"},
                    {.key    = "separation ALTITUDE_ASCENDING",
                     .length = 1072,
                     .rle = "173F2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T211F2T3F2T3F2T3F2T3F"
                            "2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T211F2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T3F"
                            "2T3F2T3F2T211F2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T3F2T38F"},
                    {.key    = "separation ALTITUDE_ASCENDING 250",
                     .length = 1072,
                     .rle = "173FT4FT4FT4FT4FT4FT4FT4FT4FT4FT4FT4FT212FT4FT4FT4FT4FT4FT4FT4FT4FT4FT"
                            "4FT4FT212FT4FT4FT4FT4FT4FT4FT4FT4FT4FT4FT4FT212FT4FT4FT4FT4FT4FT4FT4FT"
                            "4FT4FT4FT4FT39F"},
                    {.key    = "separation APOGEE",
                     .length = 1072,
                     .rle    = "123F12T256F12T256F12T256F12T133F"},
                    {.key    = "separation ALTITUDE_DESCENDING",
                     .length = 1072,
                     .rle = "172FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT210FTFT2"
                            "FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT210FTFT2FTFT2FTF"
                            "T2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT210FTFT2FTFT2FTFT2FTFT2F"
                            "TFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT38F"},
                    {.key    = "separation ALTITUDE_DESCENDING 250",
                     .length = 1072,
                     .rle = "172FT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT209FT2F"
                            "TFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT209FT2FTFT2FTF"
                            "T2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT209FT2FTFT2FTFT2FTFT2"
                            "FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FTFT2FT37F"},
                    {.key = "separation NEVER", .length = 1072, .rle = "1072F"},
                    {.key    = "testForIgnition 0",
                     .length = 1072,
                     .rle    = "86F5E5F5T228F5E10F10T268F5E177F12T256F"},
                    {.key    = "testForIgnition 1",
                     .length = 1072,
                     .rle    = "12T317F5E10F10T268F5E263F5E177F"},
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

/// The rocket of the probe's makeBoosterSetBeforeSecondStage(): a booster set that is added
/// before the second core stage, so that the stages are numbered S0 = 0, P = 1, S1 = 2 while S1
/// and P both have S0 as their upper stage. The stage "right below" another is then not the one
/// with the next number.
///
///     Rocket
///       S0 (stage 0)
///         body (a motor ignited by EJECTION_CHARGE)
///           parachute
///           P (a parallel stage, stage 1)
///             body (a motor ignited by BURNOUT)
///               parachute
///       S1 (stage 2)
///         body (a motor ignited by AUTOMATIC)
[[nodiscard]] std::unique_ptr<Rocket> makeBoosterSetBeforeSecondStage()
{
    auto rocket = std::make_unique<Rocket>();
    rocket->setName("Booster set first");
    AxialStage& s0 = rocket->addChild(std::make_unique<AxialStage>());
    BodyTube&   b0 = s0.addChild(std::make_unique<BodyTube>(0.2, 0.02));
    b0.addChild(std::make_unique<Parachute>());
    ParallelStage& p  = b0.addChild(std::make_unique<ParallelStage>());
    BodyTube&      pb = p.addChild(std::make_unique<BodyTube>(0.1, 0.01));
    pb.addChild(std::make_unique<Parachute>());
    AxialStage& s1 = rocket->addChild(std::make_unique<AxialStage>());
    BodyTube&   b1 = s1.addChild(std::make_unique<BodyTube>(0.2, 0.02));

    const FlightConfigurationId fcid = QtRocket::Test::testFcid(0);
    rocket->createFlightConfiguration(fcid);
    QtRocket::Test::addMotor(b0, fcid, QtRocket::Test::motorA8())
        .setIgnitionEvent(IgnitionEvent::EJECTION_CHARGE);
    QtRocket::Test::addMotor(pb, fcid, QtRocket::Test::motorA8())
        .setIgnitionEvent(IgnitionEvent::BURNOUT);
    QtRocket::Test::addMotor(b1, fcid, QtRocket::Test::motorA8())
        .setIgnitionEvent(IgnitionEvent::AUTOMATIC);
    rocket->enableEvents();
    return rocket;
}

/// The rocket of the probe's makeTwoBoosterSetsOnOneCore(): two booster sets on the body of the
/// second core stage.
///
///     Rocket
///       S0 (stage 0)
///         body (a motor ignited by AUTOMATIC)
///           parachute
///       S1 (stage 1)
///         body (a motor ignited by BURNOUT)
///           P1 (a parallel stage, stage 2)
///             body (a motor ignited by EJECTION_CHARGE)
///           P2 (a parallel stage, stage 3)
///             body (a motor ignited by AUTOMATIC)
///               parachute
[[nodiscard]] std::unique_ptr<Rocket> makeTwoBoosterSetsOnOneCore()
{
    auto rocket = std::make_unique<Rocket>();
    rocket->setName("Two booster sets");
    AxialStage& s0 = rocket->addChild(std::make_unique<AxialStage>());
    BodyTube&   b0 = s0.addChild(std::make_unique<BodyTube>(0.2, 0.02));
    b0.addChild(std::make_unique<Parachute>());
    AxialStage&    s1  = rocket->addChild(std::make_unique<AxialStage>());
    BodyTube&      b1  = s1.addChild(std::make_unique<BodyTube>(0.2, 0.02));
    ParallelStage& p1  = b1.addChild(std::make_unique<ParallelStage>());
    BodyTube&      pb1 = p1.addChild(std::make_unique<BodyTube>(0.1, 0.01));
    ParallelStage& p2  = b1.addChild(std::make_unique<ParallelStage>());
    BodyTube&      pb2 = p2.addChild(std::make_unique<BodyTube>(0.1, 0.01));
    pb2.addChild(std::make_unique<Parachute>());

    const FlightConfigurationId fcid = QtRocket::Test::testFcid(0);
    rocket->createFlightConfiguration(fcid);
    QtRocket::Test::addMotor(b0, fcid, QtRocket::Test::motorA8())
        .setIgnitionEvent(IgnitionEvent::AUTOMATIC);
    QtRocket::Test::addMotor(b1, fcid, QtRocket::Test::motorA8())
        .setIgnitionEvent(IgnitionEvent::BURNOUT);
    QtRocket::Test::addMotor(pb1, fcid, QtRocket::Test::motorA8())
        .setIgnitionEvent(IgnitionEvent::EJECTION_CHARGE);
    QtRocket::Test::addMotor(pb2, fcid, QtRocket::Test::motorA8())
        .setIgnitionEvent(IgnitionEvent::AUTOMATIC);
    rocket->enableEvents();
    return rocket;
}

/// The stages, pod sets, motor mounts and recovery devices of @p rocket, the rocket itself
/// first, in tree order: the components the tables range over.
[[nodiscard]] std::vector<const RocketComponent*> representativesOf(const Rocket& rocket)
{
    std::vector<const RocketComponent*> representatives;
    for (const RocketComponent& component : rocket.subtree(true))
    {
        if (dynamic_cast<const ComponentAssembly*>(&component) != nullptr ||
            dynamic_cast<const MotorMount*>(&component) != nullptr ||
            dynamic_cast<const RecoveryDevice*>(&component) != nullptr)
        {
            representatives.push_back(&component);
        }
    }
    return representatives;
}

/// The altitude of the tables that do not use the default (the probe's OTHER_ALTITUDE).
constexpr double kOtherAltitude = 250.0;

/// The tables of the probe, computed here: every enum constant against every event type from
/// every representative source, with and without data, on one test rocket. The targets and the
/// flight configuration are the rocket's; the event sources and the motor states are the
/// rocket's too, or those of a copy of it.
class ActivationTables
{
public:
    ActivationTables(const JavaRocket& java, bool sourcesFromCopy)
      : m_rocket(makeRocket(java.maker))
    {
        if (java.selectFcid >= 0)
        {
            m_rocket->setSelectedConfiguration(QtRocket::Test::testFcid(java.selectFcid));
        }
        if (sourcesFromCopy)
        {
            m_copy = m_rocket->copyRocketWithOriginalId();
        }
        collectComponents();
        makeEvents();
    }

    [[nodiscard]] const std::string& components() const noexcept { return m_signature; }
    [[nodiscard]] std::size_t        representatives() const noexcept { return m_reps.size(); }
    [[nodiscard]] std::size_t        stages() const noexcept { return m_stages.size(); }
    [[nodiscard]] std::size_t        states() const noexcept { return m_states.size(); }
    [[nodiscard]] std::size_t        events() const noexcept { return m_events.size(); }

    /// Whether every event source and every mount of a motor state is in another rocket than
    /// the targets.
    [[nodiscard]] bool sourcesAreInAnotherRocket() const
    {
        const Rocket* rocket = m_rocket.get();
        return std::ranges::none_of(m_sources,
                                    [rocket](const RocketComponent* source) {
                                        return source->findRocket() == rocket;
                                    }) &&
               std::ranges::none_of(
                   m_states, [rocket](const std::shared_ptr<MotorClusterState>& state) {
                       return asComponent(state->getMount()).findRocket() == rocket;
                   });
    }

    /// Every table, by the key the probe prints.
    [[nodiscard]] std::map<std::string, std::string, std::less<>> all()
    {
        std::map<std::string, std::string, std::less<>> tables;
        tables.emplace("events", eventsMade());
        addIgnitionTables(tables);
        for (const DeployEvent deploy : DeploymentConfiguration::kAllDeployEvents)
        {
            tables.emplace("deploy " + std::string{deployEventName(deploy)},
                           deployTable(deploy, std::nullopt));
        }
        tables.emplace("deploy ALTITUDE 250", deployTable(DeployEvent::ALTITUDE, kOtherAltitude));
        for (const SeparationEvent separation : StageSeparationConfiguration::kAllSeparationEvents)
        {
            tables.emplace("separation " + std::string{separationEventName(separation)},
                           separationTable(separation, std::nullopt));
        }
        tables.emplace("separation ALTITUDE_ASCENDING 250",
                       separationTable(SeparationEvent::ALTITUDE_ASCENDING, kOtherAltitude));
        tables.emplace("separation ALTITUDE_DESCENDING 250",
                       separationTable(SeparationEvent::ALTITUDE_DESCENDING, kOtherAltitude));
        tables.emplace("testForIgnition 0", testForIgnitionTable(false));
        tables.emplace("testForIgnition 1", testForIgnitionTable(true));
        return tables;
    }

private:
    [[nodiscard]] static std::unique_ptr<Rocket> makeRocket(std::string_view maker)
    {
        if (maker == "makeBoosterSetBeforeSecondStage")
        {
            return makeBoosterSetBeforeSecondStage();
        }
        if (maker == "makeTwoBoosterSetsOnOneCore")
        {
            return makeTwoBoosterSetsOnOneCore();
        }
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

    /// The targets (the representatives of the rocket and its stages), the event sources (the
    /// representatives of the rocket or of its copy), and the motor states of the source mounts
    /// that hold a motor in the selected configuration.
    void collectComponents()
    {
        const FlightConfigurationId fcid = config().getFlightConfigurationId();
        m_reps                           = representativesOf(*m_rocket);
        for (const RocketComponent* component : m_reps)
        {
            if (const auto* stage = dynamic_cast<const AxialStage*>(component))
            {
                m_stages.push_back(stage);
            }
        }

        m_sources = representativesOf(m_copy != nullptr ? *m_copy : *m_rocket);
        for (const RocketComponent* component : m_sources)
        {
            if (!m_signature.empty())
            {
                m_signature += ',';
            }
            m_signature += className(component->kind());
            m_signature += std::to_string(component->getStageNumber());
            const auto* mount = dynamic_cast<const MotorMount*>(component);
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
        sources.insert(sources.end(), m_sources.begin(), m_sources.end());
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

    /// The table of @p deploy at the deploy altitude @p altitude, the default without one.
    [[nodiscard]] std::string deployTable(DeployEvent deploy, std::optional<double> altitude) const
    {
        DeploymentConfiguration deployment;
        deployment.setDeployEvent(deploy);
        if (altitude.has_value())
        {
            deployment.setDeployAltitude(*altitude);
        }
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

    /// The table of @p separation at the separation altitude @p altitude, the default without
    /// one.
    [[nodiscard]] std::string separationTable(SeparationEvent       separation,
                                              std::optional<double> altitude) const
    {
        StageSeparationConfiguration configuration;
        configuration.setSeparationEvent(separation);
        if (altitude.has_value())
        {
            configuration.setSeparationAltitude(*altitude);
        }
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

    std::unique_ptr<Rocket> m_rocket;
    /// The copy the event sources and the motor states are taken from, or null: the rocket's own.
    std::unique_ptr<Rocket>                         m_copy;
    std::vector<const RocketComponent*>             m_reps;
    std::vector<const AxialStage*>                  m_stages;
    std::vector<const RocketComponent*>             m_sources;
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

/// The parameter: the index of the rocket in javaRockets(), and whether the event sources and
/// the motor states come from a copy of the rocket.
class FlightEventActivationAgainstJava
  : public ::testing::TestWithParam<std::tuple<std::size_t, bool>>
{ };

TEST_P(FlightEventActivationAgainstJava, TheTruthTablesAreJavas)
{
    const JavaRocket& java            = javaRockets().at(std::get<0>(GetParam()));
    const bool        sourcesFromCopy = std::get<1>(GetParam());
    SCOPED_TRACE(java.maker);
    ActivationTables tables(java, sourcesFromCopy);

    // The same components, in the same order, as the probe saw.
    ASSERT_EQ(tables.components(), java.components);
    ASSERT_EQ(tables.representatives(), java.representatives);
    ASSERT_EQ(tables.stages(), java.stages);
    ASSERT_EQ(tables.states(), java.states);
    ASSERT_EQ(tables.events(), java.events);
    EXPECT_EQ(tables.sourcesAreInAnotherRocket(), sourcesFromCopy);
    EXPECT_EQ(java.tables.size(), 27U)
        << "1 of the events, 6 of ignition, 7 of deployment, 11 of separation, 2 of the states";
    // Java's tables are the same for the sources of a copy (see JavaRocket).
    expectTables(tables.all(), java.tables);
}

INSTANTIATE_TEST_SUITE_P(Rockets, FlightEventActivationAgainstJava,
                         ::testing::Combine(::testing::Range<std::size_t>(0, 7),
                                            ::testing::Bool()));

TEST(FlightEventActivationAgainstJava, TheTablesCoverSevenRocketsFiveOfThemWithParallelStages)
{
    ASSERT_EQ(javaRockets().size(), 7U);
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
    EXPECT_EQ(withParallelStages, 5U);
    EXPECT_GT(entries, 200000U);
}

TEST(FlightEventActivationAgainstJava, TheHandBuiltRocketsAreNumberedAsInJava)
{
    // The probe's "components" lines: the class and the stage number of each representative.
    const std::unique_ptr<Rocket> first = makeBoosterSetBeforeSecondStage();
    std::string                   signature;
    for (const RocketComponent* component : representativesOf(*first))
    {
        signature += std::string{className(component->kind())} +
                     std::to_string(component->getStageNumber()) + " ";
    }
    EXPECT_EQ(signature,
              "Rocket-1 AxialStage0 BodyTube0 Parachute0 ParallelStage1 BodyTube1 "
              "Parachute1 AxialStage2 BodyTube2 ");
    EXPECT_EQ(representativesOf(*makeTwoBoosterSetsOnOneCore()).size(), 11U);
}

TEST(FlightEventActivationAgainstJava, TheRunLengthEncodingIsTheProbes)
{
    EXPECT_EQ(runLengthEncoded(""), "");
    EXPECT_EQ(runLengthEncoded("T"), "T");
    EXPECT_EQ(runLengthEncoded("FFFTTE"), "3F2TE");
    EXPECT_EQ(runLengthEncoded("FTFTFFFFFFFFFFFF"), "FTFT12F");
}

}  // namespace
