// Tests to verify that simulations contain all the expected flight events: OpenRocket's
// FlightEventsTest (core/src/test/java/info/openrocket/core/simulation/FlightEventsTest.java),
// with the same expectations and tolerances.
//
// The simulations run as the Java tests run them: under the preferences of OpenRocket's test
// set-up (SimulationRunSupport.h: a launch rod of length 0, no wind, the launch site at 0/0).
// Where the Java test leaves the random seed of the simulation to chance (testSingleStage,
// which JUnit repeats 50 times with 50 seeds), the test here runs the seeds 0 ... 49.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/logging/Message.h"
#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/AbstractRkSimulationStepper.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "rocket/TestRockets.h"
#include "simulation/SimulationRunSupport.h"
#include "unit/DefaultUnitsGuard.h"

namespace
{

using QtRocket::AbstractRkSimulationStepper;
using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::DeploymentConfiguration;
using QtRocket::FlightConfigurationId;
using QtRocket::FlightData;
using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::FlightDataTypeId;
using QtRocket::FlightEvent;
using QtRocket::IgnitionEvent;
using QtRocket::InnerTube;
using QtRocket::Message;
using QtRocket::MessageSource;
using QtRocket::MotorConfiguration;
using QtRocket::Parachute;
using QtRocket::ParallelStage;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Simulation;
using QtRocket::SimulationAbort;
using QtRocket::Warning;
using QtRocket::Test::DefaultUnitsGuard;
using QtRocket::Test::JavaTestPreferences;
using QtRocket::Test::simulatedData;
using QtRocket::Test::simulateOrFail;
using QtRocket::Test::TestBeta;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::testFcid;
using QtRocket::Test::TestMultiStageEventTestRocket;

using Type = FlightEvent::Type;

/// FlightEventsTest.EPSILON
constexpr double kEpsilon = 0.005;

/// RK4SimulationStepper.RECOMMENDED_MAX_TIME: the time the Java test gives an expected event
/// whose time is too variable to check.
constexpr double kNoTime = AbstractRkSimulationStepper::kRecommendedMaxTime;

/// An event the Java test expects (Java: a FlightEvent made for the comparison): its type, its
/// time, its source (a component of the test's own rocket, or null) and, for a SIM_WARN or a
/// SIM_ABORT event, the warning or the abort it carries (null otherwise).
struct ExpectedEvent
{
    Type                           type;
    double                         time;
    const RocketComponent*         source;
    std::shared_ptr<const Message> data;

    ExpectedEvent(Type eventType, double eventTime, const RocketComponent* eventSource,
                  std::shared_ptr<const Message> eventData = nullptr)
      : type(eventType), time(eventTime), source(eventSource), data(std::move(eventData))
    {
    }
};

/// FlightEventsTest.findComponent(): the last child of @p parent whose class is exactly T (the
/// Java loop does not stop at the first one); null when there is none.
template <class T>
[[nodiscard]] T* findComponent(RocketComponent& parent)
{
    T* ret = nullptr;
    for (RocketComponent* child : parent.getChildren())
    {
        if (typeid(*child) == typeid(T))
        {
            ret = dynamic_cast<T*>(child);
        }
    }
    return ret;
}

/// Java's Objects.equals() of two event sources: both null, or RocketComponent.equals().
[[nodiscard]] bool sameSource(const RocketComponent* expected, const RocketComponent* actual)
{
    if (expected == nullptr || actual == nullptr)
    {
        return expected == actual;
    }
    return expected->equals(*actual);
}

/// The text of a warning for a message, "null" for none.
[[nodiscard]] std::string warningText(const Warning* warning)
{
    return warning == nullptr ? "null" : warning->toString();
}

/// The comparison of one expected event with the actual one at the same place of a branch
/// (the body of the loop of FlightEventsTest.checkEvents()).
void checkEvent(std::vector<std::string>& mismatches, const std::string& where,
                const ExpectedEvent& expected, const FlightEvent& actual, const FlightData& data,
                double timeStep)
{
    const Warning* expectedWarning = nullptr;
    if (expected.type == Type::SIM_WARN)
    {
        expectedWarning = dynamic_cast<const Warning*>(expected.data.get());
    }

    // Java's event holds the warning as the warning set has it (FlightData::findWarning()).
    const Warning* actualWarning = nullptr;
    if (actual.getType() == Type::SIM_WARN)
    {
        actualWarning = data.findWarning(actual);
    }

    if (expected.type != actual.getType())
    {
        mismatches.push_back(std::format("{}: expected type {}, got {}", where, name(expected.type),
                                         name(actual.getType())));
    }

    const bool warningsMatch = ((expectedWarning == nullptr) && (actualWarning == nullptr)) ||
                               ((expectedWarning != nullptr) && (actualWarning != nullptr) &&
                                expectedWarning->equals(*actualWarning)) ||
                               ((actualWarning != nullptr) && (expectedWarning != nullptr) &&
                                actualWarning->equals(*expectedWarning));
    if (!warningsMatch)
    {
        mismatches.push_back(std::format("{}: {} not found; {} found instead", where,
                                         warningText(expectedWarning), warningText(actualWarning)));
    }

    if (expected.time != kNoTime)
    {
        // event times that are dependent on simulation step time shouldn't be held to
        // tighter bounds than that
        const bool stepDependent =
            (actual.getType() == Type::TUMBLE) ||
            // A large-AOA warning fires on the first step past the stall angle, so its time
            // is as step- and wind-sensitive as a tumble.
            (dynamic_cast<const Warning::LargeAOA*>(actualWarning) != nullptr) ||
            (actual.getType() == Type::APOGEE) || (actual.getType() == Type::GROUND_HIT) ||
            (actual.getType() == Type::SIMULATION_END);
        const double epsilon = stepDependent ? (5 * timeStep) : kEpsilon;
        if (!(std::abs(expected.time - actual.getTime()) <= epsilon))
        {
            mismatches.push_back(std::format("{} type {} has wrong time: expected {}, got {}",
                                             where, name(expected.type), expected.time,
                                             actual.getTime()));
        }
    }

    // Test that the event sources are correct
    if (!sameSource(expected.source, actual.getSource()))
    {
        mismatches.push_back(
            std::format("{} type {} has wrong source", where, name(expected.type)));
    }

    // If it's a warning event, make sure the warning types match
    if (expected.type == Type::SIM_WARN &&
        (actualWarning == nullptr || expectedWarning == nullptr ||
         !expectedWarning->equals(*actualWarning)))
    {
        mismatches.push_back(std::format("{}: Expected: {} but was: {}", where,
                                         warningText(expectedWarning), warningText(actualWarning)));
    }
}

/// FlightEventsTest.checkEvents(): makes sure the expected and the actual events of branch
/// @p branchNo match. Returns what does not; empty when all is well.
[[nodiscard]] std::vector<std::string> checkEvents(const std::vector<ExpectedEvent>& expectedEvents,
                                                   const Simulation& sim, std::size_t branchNo)
{
    std::vector<std::string> mismatches;
    const FlightData&        data = simulatedData(sim);
    if (branchNo >= data.getBranchCount())
    {
        mismatches.push_back(std::format("Branch {} does not exist", branchNo));
        return mismatches;
    }
    const std::vector<FlightEvent>& actualEvents = data.getBranch(branchNo).getEvents();

    // Test that all expected events are present, in the right order, at the right
    // time, from the right sources
    for (std::size_t i = 0; i < std::min(expectedEvents.size(), actualEvents.size()); i++)
    {
        checkEvent(mismatches, std::format("Branch {} FlightEvent {}", branchNo, i),
                   expectedEvents[i], actualEvents[i], data, sim.getOptions().getTimeStep());
    }

    // Test event count.  I don't think it's possible to fail here without having failed
    // earlier, but just in case.
    if (expectedEvents.size() != actualEvents.size())
    {
        mismatches.push_back(
            std::format("Branch {} incorrect number of events: expected {}, got {}", branchNo,
                        expectedEvents.size(), actualEvents.size()));
    }
    return mismatches;
}

/// FlightEventsTest.checkLastRecord(): makes sure no flight data variable is present in the
/// next-to-last flight record but not in the last record, except the simulation step time,
/// which should be NaN on the last step. Returns what is wrong; empty when all is well.
[[nodiscard]] std::vector<std::string> checkLastRecord(const Simulation& sim, std::size_t b)
{
    std::vector<std::string> mismatches;
    const FlightData&        data = simulatedData(sim);
    if (b >= data.getBranchCount())
    {
        mismatches.push_back(std::format("Branch {} does not exist", b));
        return mismatches;
    }
    const FlightDataBranch& branch   = data.getBranch(b);
    const FlightDataType&   timeStep = FlightDataType::builtin(FlightDataTypeId::TYPE_TIME_STEP);
    const std::size_t       length   = branch.getLength();
    if (length < 2)
    {
        mismatches.push_back(std::format("Sim branch {} has fewer than two records", b));
        return mismatches;
    }
    for (const FlightDataType* type : branch.getTypes())
    {
        const std::optional<double> nextToLast = branch.getByIndex(*type, length - 2);
        if (nextToLast.has_value() && !std::isnan(*nextToLast) &&
            std::isnan(branch.getLast(*type)) && (type != &timeStep))
        {
            mismatches.push_back(std::format("Sim branch {}: final flight data variable {} is NaN",
                                             b, type->getName()));
        }
    }
    if (!std::isnan(branch.getLast(timeStep)))
    {
        mismatches.push_back(
            std::format("Sim branch {} final FlightDataType.TYPE_TIME_STEP isn't NaN", b));
    }
    return mismatches;
}

/// The events and the last record of every branch of @p sim, checked against @p expected (one
/// list per branch); a branch whose expected events end with a SIM_ABORT is not checked for
/// its last record (the simulation step parameters are not saved on an abort, so the last
/// record's saved types will be missing a lot of them).
[[nodiscard]] std::vector<std::string> checkBranches(
    const std::vector<std::vector<ExpectedEvent>>& expected, const Simulation& sim)
{
    std::vector<std::string> mismatches;
    for (std::size_t b = 0; b < expected.size(); b++)
    {
        const std::vector<std::string> events = checkEvents(expected[b], sim, b);
        mismatches.insert(mismatches.end(), events.begin(), events.end());
        if (!expected[b].empty() && expected[b].back().type != Type::SIM_ABORT)
        {
            const std::vector<std::string> record = checkLastRecord(sim, b);
            mismatches.insert(mismatches.end(), record.begin(), record.end());
        }
    }
    return mismatches;
}

/// One repetition of FlightEventsTest.testSingleStage with the random seed @p seed: what does
/// not match, each line prefixed with the seed.
[[nodiscard]] std::vector<std::string> singleStageMismatches(int seed)
{
    const TestEstesAlphaIII  alpha;
    Rocket&                  rocket         = *alpha.rocket;
    const AxialStage*        stage          = rocket.getStage(0);
    const InnerTube*         motorMountTube = alpha.inner;
    const Parachute*         parachute      = alpha.chute;
    std::vector<std::string> mismatches;
    if (stage == nullptr || &stage->getChild(1).getChild(2) != motorMountTube ||
        &stage->getChild(1).getChild(3) != parachute)
    {
        mismatches.emplace_back("the rocket is not built as the Java test expects");
        return mismatches;
    }

    const std::shared_ptr<const Message> warn =
        std::make_shared<Warning::RecoveryHighSpeedDeployment>(
            80.6, QtRocket::MessageSources{MessageSource::of(*parachute)});
    JavaTestPreferences preferences;
    Simulation          sim(rocket, preferences.store);
    sim.getOptions().setIsaAtmosphere(true);
    sim.getOptions().setTimeStep(0.05);
    // Java: whatever seed the new options drew.
    sim.getOptions().setRandomSeed(seed);
    sim.setFlightConfigurationId(testFcid(0));

    simulateOrFail(sim);

    // Test branch count
    const std::size_t branchCount = simulatedData(sim).getBranchCount();
    if (branchCount != 1)
    {
        mismatches.push_back(
            std::format(" Single stage simulation invalid branch count: {}", branchCount));
    }

    const std::vector<ExpectedEvent> expectedEvents{
        {Type::LAUNCH, 0.0, &rocket},
        {Type::IGNITION, 0.0, motorMountTube},
        {Type::LIFTOFF, 0.1275, nullptr},
        {Type::LAUNCHROD, 0.13, nullptr},
        {Type::BURNOUT, 2.0, motorMountTube},
        {Type::EJECTION_CHARGE, 2.0, stage},
        {Type::SIM_WARN, 2.0, nullptr, warn},
        {Type::RECOVERY_DEVICE_DEPLOYMENT, 2.001, parachute},
        {Type::APOGEE, 2.48, &rocket},
        {Type::GROUND_HIT, 42.97, nullptr},
        {Type::SIMULATION_END, 42.97, nullptr},
    };

    const std::vector<std::string> found = checkBranches({expectedEvents}, sim);
    mismatches.insert(mismatches.end(), found.begin(), found.end());
    for (std::string& mismatch : mismatches)
    {
        mismatch = std::format("seed {}: {}", seed, mismatch);
    }
    return mismatches;
}

/// singleStageMismatches() of the seeds @p first ... @p first + @p count - 1.
[[nodiscard]] std::vector<std::string> singleStageMismatches(int first, int count)
{
    std::vector<std::string> mismatches;
    for (int seed = first; seed < first + count; seed++)
    {
        const std::vector<std::string> found = singleStageMismatches(seed);
        mismatches.insert(mismatches.end(), found.begin(), found.end());
    }
    return mismatches;
}

/// The units of the warnings' texts (the speed of a deployment warning) are the defaults.
class FlightEventsTest : public ::testing::Test
{
private:
    DefaultUnitsGuard m_units;
};

// FlightEventsTest.testSingleStage (@RepeatedTest(50)): Tests for a single stage design.
TEST_F(FlightEventsTest, SingleStage)
{
    EXPECT_EQ(singleStageMismatches(0, 50), std::vector<std::string>{});
}

// FlightEventsTest.testDeployNoMotorEnabled (@RepeatedTest(50); the Java test fixes both random
// seeds, so its repetitions are one and the same run): Should not get a sim abort if recovery
// device deploys when upper stage motor never fires
TEST_F(FlightEventsTest, DeployNoMotorEnabled)
{
    const TestBeta beta;
    Rocket&        rocket    = *beta.rocket;
    auto*          sustainer = dynamic_cast<AxialStage*>(&rocket.getChild(0));
    ASSERT_NE(sustainer, nullptr);

    auto* sustainerBody = findComponent<BodyTube>(*sustainer);
    ASSERT_NE(sustainerBody, nullptr) << "Failed to find sustainer body tube";

    auto* chute = findComponent<Parachute>(*sustainerBody);
    ASSERT_NE(chute, nullptr) << "Failed to find sustainer parachute";

    // Set parachute to deploy 1/2 second after stage separation
    DeploymentConfiguration deploymentConfig;
    deploymentConfig.setDeployEvent(DeploymentConfiguration::DeployEvent::LOWER_STAGE_SEPARATION);
    deploymentConfig.setDeployDelay(0.5);
    chute->getDeploymentConfigurations().setDefault(deploymentConfig);

    auto* sustainerMount = findComponent<InnerTube>(*sustainerBody);
    ASSERT_NE(sustainerMount, nullptr) << "Failed to find sustainer motor mount";

    auto* booster = dynamic_cast<AxialStage*>(&rocket.getChild(1));
    ASSERT_NE(booster, nullptr) << "failed to find booster axial stage";

    auto* boosterBody = findComponent<BodyTube>(*booster);
    ASSERT_NE(boosterBody, nullptr) << "failed to find booster body tube";

    auto* boosterMount = findComponent<InnerTube>(*boosterBody);
    ASSERT_NE(boosterMount, nullptr) << "failed to find booster motor mount";

    JavaTestPreferences preferences;
    Simulation          sim(rocket, preferences.store);
    sim.getOptions().setIsaAtmosphere(true);
    sim.getOptions().setTimeStep(0.05);
    sim.getOptions().getAverageWindModel().setAverage(0.1);
    // Pin both independent random sources so repeated event times test the design.
    sim.getOptions().setRandomSeed(0);
    sim.getOptions().getAverageWindModel().setSeed(0);
    rocket.getSelectedConfiguration().setAllStages();
    const FlightConfigurationId fcid = rocket.getSelectedConfiguration().getFlightConfigurationId();
    sim.setFlightConfigurationId(fcid);

    // Set sustainer motor to never fire
    MotorConfiguration motorConfig = sustainerMount->getDefaultMotorConfig();
    motorConfig.setIgnitionEvent(IgnitionEvent::NEVER);
    sustainerMount->setMotorConfig(motorConfig, fcid);

    const std::shared_ptr<const Message> warn =
        std::make_shared<Warning::RecoveryHighSpeedDeployment>(
            53.2, QtRocket::MessageSources{MessageSource::of(*chute)});

    simulateOrFail(sim);

    // Test branch count (the Java test only declares the expected count; a third branch
    // would end it with an IllegalStateException)
    EXPECT_EQ(simulatedData(sim).getBranchCount(), 2U);

    // events whose time is too variable to check are given a time of the max sim time
    const std::vector<std::vector<ExpectedEvent>> expected{
        // Sustainer
        {
            {Type::LAUNCH, 0.0, &rocket},
            {Type::IGNITION, 0.0, boosterMount},
            {Type::LIFTOFF, 0.09, nullptr},
            {Type::LAUNCHROD, 0.09, nullptr},
            {Type::BURNOUT, 2.0, boosterMount},
            {Type::EJECTION_CHARGE, 2.0, booster},
            {Type::STAGE_SEPARATION, 2.0, booster},
            {Type::SIM_WARN, 2.5, nullptr, warn},
            {Type::RECOVERY_DEVICE_DEPLOYMENT, 2.5, chute},
            {Type::APOGEE, 2.87, &rocket},
            {Type::GROUND_HIT, kNoTime, nullptr},
            {Type::SIMULATION_END, kNoTime, nullptr},
        },
        // Stage
        {
            {Type::IGNITION, 0.0, boosterMount},
            {Type::BURNOUT, 2.0, boosterMount},
            {Type::EJECTION_CHARGE, 2.0, booster},
            {Type::STAGE_SEPARATION, 2.0, booster},
            {Type::TUMBLE, 2.13, nullptr},
            {Type::APOGEE, 3.5, &rocket},
            {Type::GROUND_HIT, kNoTime, nullptr},
            {Type::SIMULATION_END, kNoTime, nullptr},
        },
    };

    EXPECT_EQ(checkBranches(expected, sim), std::vector<std::string>{});
}

// FlightEventsTest.testMultiStage (@RepeatedTest(50); the Java test fixes both random seeds,
// so its repetitions are one and the same run): Tests for a multi-stage design.
TEST_F(FlightEventsTest, MultiStage)
{
    const TestMultiStageEventTestRocket multi;
    Rocket&                             rocket = *multi.rocket;

    JavaTestPreferences preferences;
    Simulation          sim(rocket, preferences.store);
    sim.getOptions().setIsaAtmosphere(true);
    sim.getOptions().setTimeStep(0.05);
    sim.getOptions().getAverageWindModel().setAverage(0.1);
    // The simulation has two independent random sources: the wind (PinkNoiseWindModel) and
    // the stepper's small pitch/yaw perturbation (seeded from getRandomSeed()). Pin both so
    // the event sequence and times are a property of the design rather than of the run.
    sim.getOptions().setRandomSeed(0);
    sim.getOptions().getAverageWindModel().setSeed(0);
    rocket.getSelectedConfiguration().setAllStages();
    const FlightConfigurationId fcid = rocket.getSelectedConfiguration().getFlightConfigurationId();
    sim.setFlightConfigurationId(fcid);

    simulateOrFail(sim);

    // Test branch count
    ASSERT_EQ(simulatedData(sim).getBranchCount(), 3U)
        << " Multi-stage simulation invalid branch count ";

    const AxialStage* sustainer = rocket.getStage(0);
    ASSERT_NE(sustainer, nullptr);
    const auto* sustainerBody = dynamic_cast<const BodyTube*>(&sustainer->getChild(1));

    const AxialStage* centerBooster = rocket.getStage(1);
    ASSERT_NE(centerBooster, nullptr);
    const auto* centerBoosterBody = dynamic_cast<const BodyTube*>(&centerBooster->getChild(0));
    ASSERT_NE(centerBoosterBody, nullptr);

    const auto* sideBoosters = dynamic_cast<const ParallelStage*>(&centerBoosterBody->getChild(1));
    ASSERT_NE(sideBoosters, nullptr);
    const auto* sideBoosterBodies = dynamic_cast<const BodyTube*>(&sideBoosters->getChild(1));
    ASSERT_NE(sideBoosterBodies, nullptr);
    const auto* sideChutes = dynamic_cast<const Parachute*>(&sideBoosterBodies->getChild(0));
    ASSERT_NE(sideChutes, nullptr);

    const std::shared_ptr<const Message> simAbort =
        std::make_shared<SimulationAbort>(SimulationAbort::Cause::TUMBLE_UNDER_THRUST);

    const std::shared_ptr<const Message> warn =
        std::make_shared<Warning::RecoveryHighSpeedDeployment>(
            53.2, QtRocket::MessageSources{MessageSource::of(*sideChutes)});

    // LargeAOA.equals() does not compare the angle, so the value here is immaterial.
    const std::shared_ptr<const Message> largeAoa = std::make_shared<Warning::LargeAOA>(0);

    // events whose time is too variable to check are given a time of the max sim time
    const std::vector<std::vector<ExpectedEvent>> expected{
        // Sustainer
        {
            {Type::LAUNCH, 0.0, &rocket},
            {Type::IGNITION, 0.0, sideBoosterBodies},
            {Type::IGNITION, 0.01, centerBoosterBody},
            {Type::LIFTOFF, 0.06, nullptr},
            {Type::LAUNCHROD, 0.0625, nullptr},
            {Type::BURNOUT, 1.05, sideBoosterBodies},
            {Type::EJECTION_CHARGE, 1.05, sideBoosters},
            {Type::STAGE_SEPARATION, 1.05, sideBoosters},
            {Type::BURNOUT, 2.11, centerBoosterBody},
            {Type::EJECTION_CHARGE, 2.11, centerBooster},
            {Type::STAGE_SEPARATION, 2.11, centerBooster},
            {Type::IGNITION, 2.11, sustainerBody},
            // The sustainer hits a large angle of attack, then tumbles under thrust and
            // aborts.
            {Type::SIM_WARN, 2.36, nullptr, largeAoa},
            {Type::SIM_ABORT, kNoTime, nullptr, simAbort},
        },
        // Center Booster
        {
            {Type::IGNITION, 0.01, centerBoosterBody},
            {Type::BURNOUT, 2.11, centerBoosterBody},
            {Type::EJECTION_CHARGE, 2.11, centerBooster},
            {Type::STAGE_SEPARATION, 2.11, centerBooster},
            {Type::TUMBLE, 2.45, nullptr},
            {Type::APOGEE, 3.63, &rocket},
            {Type::GROUND_HIT, kNoTime, nullptr},
            {Type::SIMULATION_END, kNoTime, nullptr},
        },
        // Side Boosters
        {
            {Type::IGNITION, 0.0, sideBoosterBodies},
            {Type::BURNOUT, 1.05, sideBoosterBodies},
            {Type::EJECTION_CHARGE, 1.05, sideBoosters},
            {Type::STAGE_SEPARATION, 1.05, sideBoosters},
            {Type::SIM_WARN, 1.05, nullptr, warn},
            {Type::RECOVERY_DEVICE_DEPLOYMENT, 1.051, sideChutes},
            {Type::APOGEE, 1.35, &rocket},
            {Type::GROUND_HIT, kNoTime, nullptr},
            {Type::SIMULATION_END, kNoTime, nullptr},
        },
    };

    EXPECT_EQ(checkBranches(expected, sim), std::vector<std::string>{});
}

}  // namespace
