// Tests the effect on the simulation results of activating/deactivating stages: OpenRocket's
// DisableStageTest (core/src/test/java/info/openrocket/core/simulation/DisableStageTest.java),
// with the same expectations and tolerances. (The Java class has one more case,
// testMultiStageFirstDisabled, which is commented out there.)
//
// The simulations run as the Java tests run them: under the preferences of OpenRocket's test
// set-up (SimulationRunSupport.h: a launch rod of length 0, no wind, the launch site at 0/0).
// Where the Java test leaves the random seed of a simulation to chance, the seed is fixed here:
// compareSims() gives the expected simulation the seed 0 before it hands that seed to the other
// one (Java: whatever seed the new options drew), and the cases that run one simulation set it.

#include <cmath>
#include <exception>
#include <format>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/SimulationStepperMethod.h"
#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"
#include "QtRocket/util/Error.h"
#include "rocket/TestRockets.h"
#include "simulation/SimulationRunSupport.h"

namespace
{

using QtRocket::AxialStage;
using QtRocket::CloneableSimulationListener;
using QtRocket::FlightConfiguration;
using QtRocket::FlightConfigurationId;
using QtRocket::FlightData;
using QtRocket::FlightEvent;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::Simulation;
using QtRocket::SimulationAbort;
using QtRocket::SimulationStatus;
using QtRocket::SimulationStepperMethod;
using QtRocket::Test::addCoreFins;
using QtRocket::Test::JavaTestPreferences;
using QtRocket::Test::junitEquals;
using QtRocket::Test::simulatedData;
using QtRocket::Test::simulateOrFail;
using QtRocket::Test::TestBeta;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::TestFalcon9Heavy;
using QtRocket::Test::testFcid;
using QtRocket::Test::TestSimple2Stage;

/// DisableStageTest.DELTA: 3 % error margin (simulations are not exact)
constexpr double kDelta = 0.05;
/// DisableStageTest.DELTA_COURSE: 10 % course error margin (simulations are not exact)
constexpr double kDeltaCourse = 0.1;
/// DisableStageTest.POSITION_EPSILON
constexpr double kPositionEpsilon = 1.0e-9;

/// The seed of the simulations whose seed the Java test leaves to chance.
constexpr int kRandomSeed = 0;

/// Writes down, when the simulation starts, where the booster of the rocket the simulation
/// runs on is (Java: the anonymous listener of getBoosterPositionDuringSimulation(), which
/// writes into an array of its method). The position is shared by the clones.
class BoosterPositionListener final : public CloneableSimulationListener<BoosterPositionListener>
{
public:
    void startSimulation(SimulationStatus& status) override
    {
        const AxialStage* booster = status.getConfiguration().getRocket().getStage(1);
        if (booster != nullptr)
        {
            *m_boosterPosition = booster->getPosition().x;
        }
    }

    [[nodiscard]] double boosterPosition() const noexcept { return *m_boosterPosition; }

private:
    std::shared_ptr<double> m_boosterPosition =
        std::make_shared<double>(std::numeric_limits<double>::quiet_NaN());
};

/// DisableStageTest.getBoosterPositionDuringSimulation(): captures the booster position from
/// the simulation-owned rocket before its first branch starts.
[[nodiscard]] double getBoosterPositionDuringSimulation(Simulation& simulation)
{
    const auto         listener = std::make_shared<BoosterPositionListener>();
    const Result<void> result   = simulation.simulate({listener});
    EXPECT_TRUE(result.has_value()) << (result.has_value() ? "" : result.error().message);
    return listener->boosterPosition();
}

/// The x of the front of stage @p stageNumber of @p rocket in its parent's frame (NaN, and a
/// failure, for a stage the rocket does not have).
[[nodiscard]] double stagePosition(const Rocket& rocket, int stageNumber)
{
    const AxialStage* stage = rocket.getStage(stageNumber);
    if (stage == nullptr)
    {
        ADD_FAILURE() << "the rocket has no stage " << stageNumber;
        return std::numeric_limits<double>::quiet_NaN();
    }
    return stage->getPosition().x;
}

// DisableStageTest.testSimulationUsesItsOwnConfigurationForComponentPositions: Verifies that a
// simulation resolves component positions using its own flight configuration rather than the
// configuration selected in the user interface.
TEST(DisableStageSimulationTest, SimulationUsesItsOwnConfigurationForComponentPositions)
{
    const TestSimple2Stage      simple;
    Rocket&                     rocket         = *simple.rocket;
    const FlightConfigurationId activeConfigId = testFcid(0);
    const FlightConfigurationId disabledSustainerConfigId;
    ASSERT_NE(rocket.getStage(0), nullptr);
    const double sustainerLength = rocket.getStage(0)->getLength();

    rocket.createFlightConfiguration(disabledSustainerConfigId);
    rocket.getFlightConfiguration(disabledSustainerConfigId).clearStage(0);

    // Simulate the fully active configuration while the disabled configuration is selected.
    rocket.setSelectedConfiguration(disabledSustainerConfigId);
    EXPECT_NEAR(0.0, stagePosition(rocket, 1), kPositionEpsilon);

    JavaTestPreferences preferences;
    Simulation          activeSimulation(rocket, preferences.store);
    activeSimulation.setFlightConfigurationId(activeConfigId);
    EXPECT_NEAR(sustainerLength, getBoosterPositionDuringSimulation(activeSimulation),
                kPositionEpsilon);

    // Running a simulation must not alter the selected configuration or its geometry.
    EXPECT_EQ(disabledSustainerConfigId, rocket.getSelectedConfiguration().getId());
    EXPECT_NEAR(0.0, stagePosition(rocket, 1), kPositionEpsilon);

    // Also verify the inverse: simulate the disabled configuration while the active one is
    // selected.
    rocket.setSelectedConfiguration(activeConfigId);
    EXPECT_NEAR(sustainerLength, stagePosition(rocket, 1), kPositionEpsilon);

    Simulation disabledSustainerSimulation(rocket, preferences.store);
    disabledSustainerSimulation.setFlightConfigurationId(disabledSustainerConfigId);
    EXPECT_NEAR(0.0, getBoosterPositionDuringSimulation(disabledSustainerSimulation),
                kPositionEpsilon);

    EXPECT_EQ(activeConfigId, rocket.getSelectedConfiguration().getId());
    EXPECT_NEAR(sustainerLength, stagePosition(rocket, 1), kPositionEpsilon);
}

/// DisableStageTest.calculateDelta()
[[nodiscard]] double calculateDelta(double value, double delta)
{
    return std::isnan(value) ? 0 : value * delta;
}

/// One value of compareSims(): its name, the value of the expected and of the actual
/// simulation, and the delta JUnit's assertEquals() is given.
struct ComparedValue
{
    std::string_view name;
    double           expected;
    double           actual;
    double           delta;
};

/// DisableStageTest.compareSims(): compares simActual to simExpected, after running both. What
/// does not match (and a simulation that failed) is returned; empty when all is well. Tested
/// parameters: maxAltitude, maxVelocity, maxMachNumber, flightTime, timeToApogee,
/// launchRodVelocity and deploymentVelocity. The comparisons are JUnit's assertEquals() with a
/// delta: two NaN are equal.
[[nodiscard]] std::vector<std::string> compareSims(Simulation& simExpected, Simulation& simActual)
{
    std::vector<std::string> mismatches;

    // Java: whatever seed the options of simExpected drew.
    simExpected.getOptions().setRandomSeed(kRandomSeed);
    // Compare the rocket configurations under the same wind realization.
    simActual.getOptions().setRandomSeed(simExpected.getOptions().getRandomSeed());

    const Result<void> expectedRun = simExpected.simulate();
    if (!expectedRun.has_value())
    {
        mismatches.push_back(std::format("Simulation failed: {}", expectedRun.error().message));
        return mismatches;
    }
    const FlightData& expected = simulatedData(simExpected);

    const Result<void> actualRun = simActual.simulate();
    if (!actualRun.has_value())
    {
        mismatches.push_back(std::format("Simulation failed: {}", actualRun.error().message));
        return mismatches;
    }
    const FlightData& actual = simulatedData(simActual);

    const std::vector<ComparedValue> values{
        {.name     = "maxAltitude",
         .expected = expected.getMaxAltitude(),
         .actual   = actual.getMaxAltitude(),
         .delta    = 25},
        {.name     = "maxVelocity",
         .expected = expected.getMaxVelocity(),
         .actual   = actual.getMaxVelocity(),
         .delta    = 15},
        {.name     = "maxMachNumber",
         .expected = expected.getMaxMachNumber(),
         .actual   = actual.getMaxMachNumber(),
         .delta    = 0.05},
        {.name     = "flightTime",
         .expected = expected.getFlightTime(),
         .actual   = actual.getFlightTime(),
         .delta    = calculateDelta(expected.getFlightTime(), kDeltaCourse)},
        {.name     = "timeToApogee",
         .expected = expected.getTimeToApogee(),
         .actual   = actual.getTimeToApogee(),
         .delta    = calculateDelta(expected.getTimeToApogee(), kDeltaCourse)},
        {.name     = "launchRodVelocity",
         .expected = expected.getLaunchRodVelocity(),
         .actual   = actual.getLaunchRodVelocity(),
         .delta    = calculateDelta(expected.getLaunchRodVelocity(), kDelta)},
        {.name     = "deploymentVelocity",
         .expected = expected.getDeploymentVelocity(),
         .actual   = actual.getDeploymentVelocity(),
         .delta    = calculateDelta(expected.getDeploymentVelocity(), kDelta)},
    };
    for (const ComparedValue& value : values)
    {
        const ::testing::AssertionResult result =
            junitEquals(value.expected, value.actual, value.delta);
        if (!result)
        {
            mismatches.push_back(std::format("{}: {}", value.name, result.message()));
        }
    }
    return mismatches;
}

/// The abort of the last SIM_ABORT event of the first branch of the data @p simulation has just
/// simulated, or null (Java: getSimulatedData().getBranch(0).getLastEvent(SIM_ABORT) and its
/// data). Valid until the simulation is run again.
[[nodiscard]] const SimulationAbort* lastAbort(const Simulation& simulation)
{
    const FlightData& data = simulatedData(simulation);
    if (data.getBranchCount() == 0)
    {
        return nullptr;
    }
    const FlightEvent* abort = data.getBranch(0).getLastEvent(FlightEvent::Type::SIM_ABORT);
    return abort == nullptr ? nullptr : abort->getAbort();
}

/// Runs @p simulation as the cases of the Java class do that wrap simulate() in a try block and
/// fail the test for whatever it throws ("unexpected exception " and the exception): that text
/// for a run that failed or threw, "" for one that did neither.
[[nodiscard]] std::string unexpectedException(Simulation& simulation)
{
    try
    {
        const Result<void> run = simulation.simulate();
        return run.has_value() ? std::string{} : "unexpected exception " + run.error().message;
    }
    catch (const std::exception& e)
    {
        return std::string{"unexpected exception "} + e.what();
    }
}

/// A simulation of @p rocket for @p fcid as every case of the Java class sets one up: the ISA
/// atmosphere, a time step of 0.05 s and @p method (the Java cases without "_RK6" leave the
/// method at RK4, the default).
[[nodiscard]] std::unique_ptr<Simulation> makeSimulation(Rocket& rocket, JavaTestPreferences& prefs,
                                                         const FlightConfigurationId& fcid,
                                                         SimulationStepperMethod      method)
{
    auto simulation = std::make_unique<Simulation>(rocket, prefs.store);
    simulation->setFlightConfigurationId(fcid);
    simulation->getOptions().setIsaAtmosphere(true);
    simulation->getOptions().setTimeStep(0.05);
    simulation->getOptions().setSimulationStepperMethodChoice(method);
    return simulation;
}

/// The body of testSingleStage and testSingleStage_RK6.
void singleStage(SimulationStepperMethod method)
{
    //// Test disabling the stage
    const TestEstesAlphaIII alpha;
    Rocket&                 rocket = *alpha.rocket;

    JavaTestPreferences preferences;
    Simulation          simDisabled(rocket, preferences.store);
    simDisabled.setFlightConfigurationId(testFcid(0));
    simDisabled.getActiveConfiguration().setStageActive(0, false);
    simDisabled.getOptions().setIsaAtmosphere(true);
    simDisabled.getOptions().setTimeStep(0.05);
    simDisabled.getOptions().setSimulationStepperMethodChoice(method);
    simDisabled.getOptions().setRandomSeed(kRandomSeed);  // Java: left to chance

    simulateOrFail(simDisabled);

    // Since there are no stages, the simulation should abort
    const SimulationAbort* abort = lastAbort(simDisabled);
    ASSERT_NE(abort, nullptr) << "Empty simulation failed to abort";
    EXPECT_EQ(SimulationAbort::Cause::NO_ACTIVE_STAGES, abort->cause())
        << "Abort cause did not match";

    //// Test re-enabling the stage.
    const TestEstesAlphaIII           alphaOriginal;
    const std::unique_ptr<Simulation> simOriginal =
        makeSimulation(*alphaOriginal.rocket, preferences, testFcid(0), method);

    simDisabled.getActiveConfiguration().setAllStages();  // Re-enable all stages.

    EXPECT_EQ(compareSims(*simOriginal, simDisabled), std::vector<std::string>{});
}

// DisableStageTest.testSingleStage: Tests that the simulation results are correct when a single
// stage is deactivated and re-activated.
TEST(DisableStageSimulationTest, SingleStage)
{
    singleStage(SimulationStepperMethod::RK4);
}

// DisableStageTest.testSingleStage_RK6: the same, using the RK6 stepper.
TEST(DisableStageSimulationTest, SingleStageRk6)
{
    singleStage(SimulationStepperMethod::RK6);
}

/// The body of testMultiStageLastDisabled and testMultiStageLastDisabled_RK6.
void multiStageLastDisabled(SimulationStepperMethod method)
{
    //// Test disabling the stage
    const TestBeta removed;   // Rocket with the last stage removed
    const TestBeta disabled;  // Rocket with the last stage disabled
    Rocket&        rocketRemoved  = *removed.rocket;
    Rocket&        rocketDisabled = *disabled.rocket;

    const int stageNr = static_cast<int>(rocketRemoved.getChildCount()) - 1;
    static_cast<void>(rocketRemoved.removeChild(stageNr));
    FlightConfiguration& fc = rocketDisabled.getFlightConfiguration(testFcid(1));
    fc.setStageActive(stageNr, false);

    JavaTestPreferences               preferences;
    const std::unique_ptr<Simulation> simRemoved =
        makeSimulation(rocketRemoved, preferences, testFcid(1), method);
    const std::unique_ptr<Simulation> simDisabled =
        makeSimulation(rocketDisabled, preferences, testFcid(1), method);

    EXPECT_EQ(compareSims(*simRemoved, *simDisabled), std::vector<std::string>{});

    //// Test re-enabling the stage.
    const TestBeta                    original;
    const std::unique_ptr<Simulation> simOriginal =
        makeSimulation(*original.rocket, preferences, testFcid(1), method);

    simDisabled->getActiveConfiguration().setAllStages();
    // (The RK6 case of the Java class sets the method of simDisabled once more here.)
    simDisabled->getOptions().setSimulationStepperMethodChoice(method);

    EXPECT_EQ(compareSims(*simOriginal, *simDisabled), std::vector<std::string>{});
}

// DisableStageTest.testMultiStageLastDisabled: Tests that the simulation results are correct
// when the last stage of a multi-stage rocket is deactivated and re-activated.
TEST(DisableStageSimulationTest, MultiStageLastDisabled)
{
    multiStageLastDisabled(SimulationStepperMethod::RK4);
}

// DisableStageTest.testMultiStageLastDisabled_RK6: the same, but with the RK6 stepper.
TEST(DisableStageSimulationTest, MultiStageLastDisabledRk6)
{
    multiStageLastDisabled(SimulationStepperMethod::RK6);
}

// DisableStageTest.testBooster1: Tests that the simulation results are correct when a booster
// stage is deactivated and re-activated.
TEST(DisableStageSimulationTest, Booster1)
{
    //// Test disabling the stage
    const TestFalcon9Heavy removed;  // Rocket with the last stage removed
    Rocket&                rocketRemoved = *removed.rocket;
    addCoreFins(rocketRemoved);

    const TestFalcon9Heavy disabled;  // Rocket with the last stage disabled
    Rocket&                rocketDisabled = *disabled.rocket;
    addCoreFins(rocketDisabled);

    const FlightConfigurationId fcid = FlightConfigurationId::fromString(TestFalcon9Heavy::kFcid1);
    const int                   stageNr = 2;  // Stage 2 is the Parallel Booster Stage
    // Remove the Parallel Booster Stage
    static_cast<void>(rocketRemoved.getChild(1).getChild(0).removeChild(0));
    FlightConfiguration& fc = rocketDisabled.getFlightConfiguration(fcid);
    fc.setStageActive(stageNr, false);

    JavaTestPreferences               preferences;
    const std::unique_ptr<Simulation> simRemoved =
        makeSimulation(rocketRemoved, preferences, fcid, SimulationStepperMethod::RK4);
    simRemoved->getOptions().getAverageWindModel().setStandardDeviation(0.0);

    const std::unique_ptr<Simulation> simDisabled =
        makeSimulation(rocketDisabled, preferences, fcid, SimulationStepperMethod::RK4);
    simDisabled->getOptions().getAverageWindModel().setStandardDeviation(0.0);

    EXPECT_EQ(compareSims(*simRemoved, *simDisabled), std::vector<std::string>{});

    //// Test re-enabling the stage.
    const TestFalcon9Heavy original;
    addCoreFins(*original.rocket);

    const std::unique_ptr<Simulation> simOriginal =
        makeSimulation(*original.rocket, preferences, fcid, SimulationStepperMethod::RK4);

    simDisabled->getActiveConfiguration().setAllStages();

    EXPECT_EQ(compareSims(*simOriginal, *simDisabled), std::vector<std::string>{});
}

/// The body of testBooster2 and testBooster2_RK6.
void booster2(SimulationStepperMethod method)
{
    //// Test disabling the stage
    const TestFalcon9Heavy removed;  // Rocket with the last stage removed
    Rocket&                rocketRemoved = *removed.rocket;
    addCoreFins(rocketRemoved);

    const TestFalcon9Heavy disabled;  // Rocket with the last stage disabled
    Rocket&                rocketDisabled = *disabled.rocket;
    addCoreFins(rocketDisabled);

    const FlightConfigurationId fid = FlightConfigurationId::fromString(TestFalcon9Heavy::kFcid1);
    const int stageNr               = 1;  // Stage 1 is the Parallel Booster Stage's parent stage
    // Remove the Parallel Booster Stage's parent stage
    static_cast<void>(rocketRemoved.getChild(1).removeChild(0));
    FlightConfiguration& fc = rocketDisabled.getFlightConfiguration(fid);
    fc.setStageActive(stageNr, false);

    JavaTestPreferences               preferences;
    const std::unique_ptr<Simulation> simRemoved =
        makeSimulation(rocketRemoved, preferences, fid, method);
    simRemoved->getOptions().setRandomSeed(kRandomSeed);  // Java: left to chance

    ASSERT_EQ(unexpectedException(*simRemoved), "");

    // There should be no motors left at this point, so we should abort on no motors
    const SimulationAbort* abort = lastAbort(*simRemoved);
    ASSERT_NE(abort, nullptr) << "Empty simulation failed to abort";
    EXPECT_EQ(SimulationAbort::Cause::NO_MOTORS_DEFINED, abort->cause())
        << "Abort cause did not match";

    const std::unique_ptr<Simulation> simDisabled =
        makeSimulation(rocketDisabled, preferences, fid, method);

    //// Test re-enabling the stage.
    const TestFalcon9Heavy original;
    addCoreFins(*original.rocket);

    const std::unique_ptr<Simulation> simOriginal =
        makeSimulation(*original.rocket, preferences, fid, method);

    simDisabled->getActiveConfiguration().setAllStages();

    EXPECT_EQ(compareSims(*simOriginal, *simDisabled), std::vector<std::string>{});
}

// DisableStageTest.testBooster2: Tests that the simulation results are correct when the parent
// stage of a booster stage is deactivated and re-activated.
TEST(DisableStageSimulationTest, Booster2)
{
    booster2(SimulationStepperMethod::RK4);
}

// DisableStageTest.testBooster2_RK6: the same, but using the RK6 stepper.
TEST(DisableStageSimulationTest, Booster2Rk6)
{
    booster2(SimulationStepperMethod::RK6);
}

/// The body of testBooster3 and testBooster3_RK6.
void booster3(SimulationStepperMethod method)
{
    const TestFalcon9Heavy disabled;
    Rocket&                rocketDisabled = *disabled.rocket;

    const FlightConfigurationId fid = FlightConfigurationId::fromString(TestFalcon9Heavy::kFcid1);
    JavaTestPreferences         preferences;
    const std::unique_ptr<Simulation> simDisabled =
        makeSimulation(rocketDisabled, preferences, fid, method);
    simDisabled->getOptions().setRandomSeed(kRandomSeed);  // Java: left to chance

    //// Test only enabling the booster stage (test for GitHub issue #1848)
    simDisabled->getActiveConfiguration().setOnlyStage(2);

    //// Test that the top stage is the booster stage
    EXPECT_EQ(rocketDisabled.getTopmostStage(simDisabled->getActiveConfiguration()),
              rocketDisabled.getStage(2));

    ASSERT_EQ(unexpectedException(*simDisabled), "");

    // Sim will tumble under
    const SimulationAbort* abort = lastAbort(*simDisabled);
    ASSERT_NE(abort, nullptr) << "Unstable booster failed to abort";
    EXPECT_EQ(SimulationAbort::Cause::TUMBLE_UNDER_THRUST, abort->cause())
        << "Abort cause did not match";
}

// DisableStageTest.testBooster3: Test whether the simulations run when only the booster stage
// is active.
TEST(DisableStageSimulationTest, Booster3)
{
    booster3(SimulationStepperMethod::RK4);
}

// DisableStageTest.testBooster3_RK6: the same, but using the RK6 stepper.
TEST(DisableStageSimulationTest, Booster3Rk6)
{
    booster3(SimulationStepperMethod::RK6);
}

}  // namespace
