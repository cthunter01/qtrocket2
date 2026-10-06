// The threading invariant of the simulation (plan section 7): simulations of independent copies
// of a design (Simulation::duplicateForIndependentSimulation()) run on several threads at the
// same time, each giving the flight the same simulation gives when it runs alone, to the last
// bit; and a stop requested from another thread cancels a simulation that is running, and only
// that one.
//
// The library has no state that two runs share: no static scratch objects, no singletons with
// state, and the counters behind ModId and FlightConfiguration are atomic. These tests are the
// ones the tsan preset is there for (cmake --workflow --preset tsan).
//
// Nothing is asserted on the worker threads: they write what they find into objects the test's
// thread reads after joining.

#include <array>
#include <atomic>
#include <cstddef>
#include <exception>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/models/WindModelType.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/SimulationStepperMethod.h"
#include "QtRocket/simulation/listeners/ProgressListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/ModId.h"
#include "rocket/TestRockets.h"
#include "simulation/SimulationRunSupport.h"

namespace
{

using QtRocket::ErrorCode;
using QtRocket::FlightConfigurationId;
using QtRocket::FlightData;
using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::FlightDataTypeId;
using QtRocket::FlightEvent;
using QtRocket::ModId;
using QtRocket::MotorConfiguration;
using QtRocket::MultiLevelPinkNoiseWindModel;
using QtRocket::Preferences;
using QtRocket::ProgressListener;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::Simulation;
using QtRocket::SimulationListener;
using QtRocket::SimulationOptions;
using QtRocket::SimulationStatus;
using QtRocket::SimulationStepperMethod;
using QtRocket::WindModelType;
using QtRocket::Test::addCoreFins;
using QtRocket::Test::JavaTestPreferences;
using QtRocket::Test::junitEquals;
using QtRocket::Test::TestBeta;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::TestFalcon9Heavy;
using QtRocket::Test::testFcid;
using QtRocket::Test::TestMultiStageEventTestRocket;

// ================================================================= the same flight, bit by bit

/// The first row in which the columns @p expected and @p actual differ in a bit (a NaN equals a
/// NaN), as a text; "" when they are the same column.
[[nodiscard]] std::string columnDifference(const std::vector<double>& expected,
                                           const std::vector<double>& actual)
{
    if (expected.size() != actual.size())
    {
        return std::format("{} rows, not {}", actual.size(), expected.size());
    }
    for (std::size_t row = 0; row < expected.size(); row++)
    {
        if (!junitEquals(expected[row], actual[row]))
        {
            return std::format("row {}: {}, not {}", row, actual[row], expected[row]);
        }
    }
    return {};
}

/// The events of @p branch, each as FlightEvent::toString() prints it (type, time, source and
/// data), one per line.
[[nodiscard]] std::string eventsText(const FlightDataBranch& branch)
{
    std::string text;
    for (const FlightEvent& event : branch.getEvents())
    {
        text += event.toString() + "\n";
    }
    return text;
}

/// What differs between the columns of the branches @p expected and @p actual, which have the
/// same types: every column but the wall clock (TYPE_COMPUTATION_TIME), bit by bit.
[[nodiscard]] std::string columnDifferences(const FlightDataBranch& expected,
                                            const FlightDataBranch& actual)
{
    const FlightDataType& wallClock =
        FlightDataType::builtin(FlightDataTypeId::TYPE_COMPUTATION_TIME);
    std::string differences;
    for (const FlightDataType* type : expected.getTypes())
    {
        const std::vector<double>* expectedColumn = expected.getView(*type);
        const std::vector<double>* actualColumn   = actual.getView(*type);
        if (type == &wallClock || expectedColumn == nullptr || actualColumn == nullptr)
        {
            continue;
        }
        const std::string difference = columnDifference(*expectedColumn, *actualColumn);
        if (!difference.empty())
        {
            differences += std::format("  {}: {}\n", type->getName(), difference);
        }
    }
    return differences;
}

/// What differs between the branches @p expected and @p actual: the name, the events, the data
/// types and every value; "" when they are the same branch.
[[nodiscard]] std::string branchDifferences(const FlightDataBranch& expected,
                                            const FlightDataBranch& actual)
{
    std::string differences;
    if (expected.getName() != actual.getName())
    {
        differences += std::format("  name: {}, not {}\n", actual.getName(), expected.getName());
    }
    if (eventsText(expected) != eventsText(actual))
    {
        differences +=
            std::format("  events:\n{}  not\n{}", eventsText(actual), eventsText(expected));
    }
    if (expected.getTypes() != actual.getTypes())
    {
        differences += "  other data types\n";
        return differences;
    }
    return differences + columnDifferences(expected, actual);
}

/// One summary value of a flight.
struct SummaryValue
{
    std::string_view name;
    double (FlightData::*get)() const noexcept;
};

/// What differs between the summary values of @p expected and @p actual, bit by bit.
[[nodiscard]] std::string summaryDifferences(const FlightData& expected, const FlightData& actual)
{
    static constexpr std::array<SummaryValue, 10> kValues{{
        {.name = "maxAltitude", .get = &FlightData::getMaxAltitude},
        {.name = "maxVelocity", .get = &FlightData::getMaxVelocity},
        {.name = "maxAcceleration", .get = &FlightData::getMaxAcceleration},
        {.name = "maxMachNumber", .get = &FlightData::getMaxMachNumber},
        {.name = "timeToApogee", .get = &FlightData::getTimeToApogee},
        {.name = "flightTime", .get = &FlightData::getFlightTime},
        {.name = "groundHitVelocity", .get = &FlightData::getGroundHitVelocity},
        {.name = "launchRodVelocity", .get = &FlightData::getLaunchRodVelocity},
        {.name = "deploymentVelocity", .get = &FlightData::getDeploymentVelocity},
        {.name = "optimumDelay", .get = &FlightData::getOptimumDelay},
    }};
    std::string                                   differences;
    for (const SummaryValue& value : kValues)
    {
        const double e = (expected.*value.get)();
        const double a = (actual.*value.get)();
        if (!junitEquals(e, a))
        {
            differences += std::format("  {}: {}, not {}\n", value.name, a, e);
        }
    }
    return differences;
}

/// What differs between the flights @p expected and @p actual: the summary values, the
/// warnings, the branches with their events and every value of every column (but the wall
/// clock), all to the last bit; "" when they are the same flight.
[[nodiscard]] std::string flightDifferences(const FlightData& expected, const FlightData& actual)
{
    std::string differences = summaryDifferences(expected, actual);
    if (expected.getWarningSet().toString() != actual.getWarningSet().toString())
    {
        differences += std::format("  warnings: {}, not {}\n", actual.getWarningSet().toString(),
                                   expected.getWarningSet().toString());
    }
    if (expected.getBranchCount() != actual.getBranchCount())
    {
        differences += std::format("  {} branches, not {}\n", actual.getBranchCount(),
                                   expected.getBranchCount());
        return differences;
    }
    for (std::size_t i = 0; i < expected.getBranchCount(); i++)
    {
        const std::string branch = branchDifferences(expected.getBranch(i), actual.getBranch(i));
        if (!branch.empty())
        {
            differences += std::format(" branch {}:\n{}", i, branch);
        }
    }
    return differences;
}

// ======================================================================== the simulations

/// A simulation the threads run copies of, with the rocket it refers to.
struct Original
{
    std::string                 name;
    std::unique_ptr<Rocket>     rocket;
    std::unique_ptr<Simulation> simulation;
};

/// A simulation of @p rocket for @p fcid as the tests of OpenRocket set one up (the ISA
/// atmosphere, a time step of 0.05 s) with @p method and the random seed @p seed.
[[nodiscard]] Original makeOriginal(std::string_view name, std::unique_ptr<Rocket> rocket,
                                    Preferences& preferences, const FlightConfigurationId& fcid,
                                    SimulationStepperMethod method, int seed)
{
    Original original{
        .name = std::string{name}, .rocket = std::move(rocket), .simulation = nullptr};
    original.simulation = std::make_unique<Simulation>(*original.rocket, preferences);
    original.simulation->setFlightConfigurationId(fcid);
    SimulationOptions& options = original.simulation->getOptions();
    options.setIsaAtmosphere(true);
    options.setTimeStep(0.05);
    options.setSimulationStepperMethodChoice(method);
    options.setRandomSeed(seed);
    return original;
}

/// The Estes Alpha III in turbulent wind, from a guide of 3 m (the flights of
/// WindModelSeedReproducibilityTest): the wind model draws random numbers during the run.
[[nodiscard]] Original turbulentOriginal(std::string_view name, Preferences& preferences,
                                         WindModelType type)
{
    Original original = makeOriginal(name, TestEstesAlphaIII().rocket, preferences, testFcid(0),
                                     SimulationStepperMethod::RK4, 12345);
    SimulationOptions& options = original.simulation->getOptions();
    options.setLaunchRodLength(3.0);
    options.setWindModelType(type);
    if (type == WindModelType::AVERAGE)
    {
        options.setWindSpeedAverage(5.0);
        options.setWindTurbulenceIntensity(0.2);
        return original;
    }
    MultiLevelPinkNoiseWindModel& model = options.getMultiLevelWindModel();
    model.clearLevels();
    EXPECT_TRUE(model.addWindLevel(0, 5.0, 0, 1.0).has_value());
    EXPECT_TRUE(model.addWindLevel(200, 7.5, 0, 1.0).has_value());
    return original;
}

/// The simulations of the test: one, two and three stages, a parallel stage, clustered motors,
/// both steppers, calm air and the two wind models with turbulence.
[[nodiscard]] std::vector<Original> makeOriginals(Preferences& preferences)
{
    std::vector<Original> originals;
    originals.push_back(makeOriginal("Estes Alpha III [A8-0], RK4", TestEstesAlphaIII().rocket,
                                     preferences, testFcid(0), SimulationStepperMethod::RK4, 1));
    originals.push_back(makeOriginal("Estes Alpha III [C6-5], RK6", TestEstesAlphaIII().rocket,
                                     preferences, testFcid(3), SimulationStepperMethod::RK6, 2));
    originals.push_back(makeOriginal("Beta, two stages", TestBeta().rocket, preferences,
                                     testFcid(1), SimulationStepperMethod::RK4, 3));

    // The simulation of FlightEventsTest.testMultiStage: three branches.
    TestMultiStageEventTestRocket multi;
    const FlightConfigurationId   multiFcid = multi.fcid;
    originals.push_back(makeOriginal("three stages with side boosters", std::move(multi.rocket),
                                     preferences, multiFcid, SimulationStepperMethod::RK4, 0));
    originals.back().simulation->getOptions().getAverageWindModel().setAverage(0.1);
    originals.back().simulation->getOptions().getAverageWindModel().setSeed(0);

    TestFalcon9Heavy falcon;
    addCoreFins(*falcon.rocket);
    const FlightConfigurationId falconFcid = falcon.fcid;
    originals.push_back(makeOriginal("Falcon 9 Heavy with core fins", std::move(falcon.rocket),
                                     preferences, falconFcid, SimulationStepperMethod::RK6, 4));

    originals.push_back(turbulentOriginal("Estes Alpha III in turbulent wind", preferences,
                                          WindModelType::AVERAGE));
    originals.push_back(turbulentOriginal("Estes Alpha III in turbulent multi-level wind",
                                          preferences, WindModelType::MULTI_LEVEL));
    return originals;
}

/// How a run on a worker thread ended: nullopt for a simulation that succeeded, else the
/// message of the error or of the exception.
using Failure = std::optional<std::string>;

/// Runs @p simulation and writes down how that ended. The body of a worker thread.
void simulateInto(Simulation& simulation, Failure& failure)
{
    try
    {
        const Result<void> result = simulation.simulate();
        failure = result.has_value() ? Failure{} : Failure{result.error().message};
    }
    catch (const std::exception& e)
    {
        failure = std::format("exception: {}", e.what());
    }
}

/// simulateInto() with a simulation that @p stopToken can cancel.
void simulateWithTokenInto(Simulation& simulation, Failure& failure,
                           const std::stop_token& stopToken)
{
    try
    {
        const Result<void> result = simulation.simulate(stopToken);
        failure = result.has_value() ? Failure{} : Failure{result.error().message};
    }
    catch (const std::exception& e)
    {
        failure = std::format("exception: {}", e.what());
    }
}

/// What a thread of the test runs: a copy of an original, and what came of it.
struct Copy
{
    std::size_t                 original;  ///< the index of the original
    std::unique_ptr<Simulation> simulation;
    Failure                     failure;
};

/// What is wrong with the flight of @p copy compared with the flight @p alone of the same
/// simulation run on its own; "" when nothing is.
[[nodiscard]] std::string copyProblem(const Copy& copy, const Simulation& alone)
{
    if (copy.failure.has_value())
    {
        return std::format("the simulation failed: {}\n", *copy.failure);
    }
    if (copy.simulation->getSimulatedData() == nullptr || alone.getSimulatedData() == nullptr)
    {
        return "no simulated data\n";
    }
    return flightDifferences(*alone.getSimulatedData(), *copy.simulation->getSimulatedData());
}

/// The modification ids of the rockets of @p originals.
[[nodiscard]] std::vector<ModId> rocketModIds(const std::vector<Original>& originals)
{
    std::vector<ModId> ids;
    ids.reserve(originals.size());
    for (const Original& original : originals)
    {
        ids.push_back(original.rocket->getModId());
    }
    return ids;
}

/// The originals that have simulated data, or whose stored status is not NOT_SIMULATED: a copy
/// that ran must not touch the simulation it was made from.
[[nodiscard]] std::string touchedOriginals(const std::vector<Original>& originals)
{
    std::string touched;
    for (const Original& original : originals)
    {
        if (original.simulation->getSimulatedData() != nullptr ||
            original.simulation->getStoredStatus() != Simulation::Status::NOT_SIMULATED)
        {
            touched += original.name + "\n";
        }
    }
    return touched;
}

/// Each simulation of @p originals run alone, on the calling thread, as an independent copy.
/// @p problems receives the simulations that failed or have no data.
[[nodiscard]] std::vector<std::unique_ptr<Simulation>> simulateAlone(
    const std::vector<Original>& originals, std::string& problems)
{
    std::vector<std::unique_ptr<Simulation>> alone;
    alone.reserve(originals.size());
    for (const Original& original : originals)
    {
        alone.push_back(original.simulation->duplicateForIndependentSimulation());
        const Result<void> result = alone.back()->simulate();
        if (!result.has_value() || alone.back()->getSimulatedData() == nullptr)
        {
            problems += std::format("{}: {}\n", original.name,
                                    result.has_value() ? "no data" : result.error().message);
        }
    }
    return alone;
}

/// @p perOriginal independent copies of each simulation of @p originals, made on the calling
/// thread (the one that owns the originals); a worker thread only runs its copy.
[[nodiscard]] std::vector<Copy> makeCopies(const std::vector<Original>& originals,
                                           std::size_t                  perOriginal)
{
    std::vector<Copy> copies;
    copies.reserve(originals.size() * perOriginal);
    for (std::size_t n = 0; n < perOriginal; n++)
    {
        for (std::size_t i = 0; i < originals.size(); i++)
        {
            copies.push_back(
                {.original   = i,
                 .simulation = originals[i].simulation->duplicateForIndependentSimulation(),
                 .failure    = std::nullopt});
        }
    }
    return copies;
}

/// Runs every simulation of @p copies on a thread of its own, all at the same time, and waits
/// for them.
void simulateAtTheSameTime(std::vector<Copy>& copies)
{
    std::vector<std::jthread> threads;
    threads.reserve(copies.size());
    for (Copy& copy : copies)
    {
        threads.emplace_back(simulateInto, std::ref(*copy.simulation), std::ref(copy.failure));
    }
}  // joins

/// What is wrong with the flights of @p copies, each compared with the flight of the same
/// simulation run alone (@p alone, by the index of the original); "" when nothing is.
[[nodiscard]] std::string copyProblems(const std::vector<Copy>&                        copies,
                                       const std::vector<std::unique_ptr<Simulation>>& alone,
                                       const std::vector<Original>&                    originals)
{
    std::string problems;
    for (const Copy& copy : copies)
    {
        const std::string problem = copyProblem(copy, *alone[copy.original]);
        if (!problem.empty())
        {
            problems += std::format("{}:\n{}", originals[copy.original].name, problem);
        }
    }
    return problems;
}

// Several simulations run at the same time on their own copies of their rockets, two threads per
// design, and each gives the flight the same simulation gives alone.
TEST(SimulationThreading, SimulationsOfIndependentCopiesRunAtTheSameTime)
{
    constexpr std::size_t kThreadsPerOriginal = 2;

    JavaTestPreferences         preferences;
    const std::vector<Original> originals = makeOriginals(preferences.store);

    std::string                                    problemsAlone;
    const std::vector<std::unique_ptr<Simulation>> alone = simulateAlone(originals, problemsAlone);
    ASSERT_EQ(problemsAlone, "");

    std::vector<Copy>        copies       = makeCopies(originals, kThreadsPerOriginal);
    const std::vector<ModId> modIdsBefore = rocketModIds(originals);
    ASSERT_EQ(copies.size(), 14U);

    simulateAtTheSameTime(copies);

    EXPECT_EQ(copyProblems(copies, alone, originals), "");

    // The copies left what they were made from alone.
    EXPECT_EQ(touchedOriginals(originals), "");
    EXPECT_EQ(rocketModIds(originals), modIdsBefore);
}

// The comparison above is not vacuous: the flights of the test are flights (they leave the
// launch rod and come down again), two runs of one simulation are the same flight, and the
// flights of two seeds are not.
TEST(SimulationThreading, TheComparisonTellsTwoFlightsApart)
{
    JavaTestPreferences preferences;
    Original            original =
        makeOriginal("Estes Alpha III", TestEstesAlphaIII().rocket, preferences.store, testFcid(0),
                     SimulationStepperMethod::RK4, 1);

    const std::unique_ptr<Simulation> first =
        original.simulation->duplicateForIndependentSimulation();
    const std::unique_ptr<Simulation> second =
        original.simulation->duplicateForIndependentSimulation();
    original.simulation->getOptions().setRandomSeed(2);
    const std::unique_ptr<Simulation> other =
        original.simulation->duplicateForIndependentSimulation();
    ASSERT_TRUE(first->simulate().has_value());
    ASSERT_TRUE(second->simulate().has_value());
    ASSERT_TRUE(other->simulate().has_value());
    ASSERT_NE(first->getSimulatedData(), nullptr);
    ASSERT_NE(second->getSimulatedData(), nullptr);
    ASSERT_NE(other->getSimulatedData(), nullptr);

    const FlightData& flight = *first->getSimulatedData();
    ASSERT_EQ(flight.getBranchCount(), 1U);
    EXPECT_NE(flight.getBranch(0).getFirstEvent(FlightEvent::Type::LAUNCHROD), nullptr);
    EXPECT_NE(flight.getBranch(0).getFirstEvent(FlightEvent::Type::GROUND_HIT), nullptr);
    EXPECT_GT(flight.getBranch(0).getLength(), 100U);

    EXPECT_EQ(flightDifferences(flight, *second->getSimulatedData()), "");
    EXPECT_NE(flightDifferences(flight, *other->getSimulatedData()), "");
}

// ================================================================ editing while copies run

/// simulateInto(), and one more in @p finished when the run is over.
void simulateAndCount(Simulation& simulation, Failure& failure, std::atomic<std::size_t>& finished)
{
    simulateInto(simulation, failure);
    finished.fetch_add(1);
}

/// Edit number @p edit of the design and the simulation the copies were made from: the fins,
/// the body, the parachute, the motor of the flight configuration, the stages that are active,
/// and the options and the name of the simulation. Each fires the change events of the rocket
/// or of the simulation.
void editTheOriginal(const TestEstesAlphaIII& alpha, Simulation& simulation, int edit)
{
    const FlightConfigurationId fcid = testFcid(0);
    alpha.fins->setTipChord(0.03 + (0.001 * (edit % 5)));
    alpha.fins->setSweep(0.02 + (0.001 * (edit % 3)));
    alpha.body->setLength(0.2 + (0.01 * (edit % 3)));
    alpha.chute->setDiameter(0.3 + (0.01 * (edit % 4)));
    alpha.chute->setCD(0.7 + (0.01 * (edit % 3)));

    MotorConfiguration motorConfig = alpha.inner->getMotorConfig(fcid);
    motorConfig.setIgnitionDelay(0.1 * (edit % 3));
    alpha.inner->setMotorConfig(motorConfig, fcid);
    alpha.rocket->getFlightConfiguration(fcid).setStageActive(0, edit % 2 == 0);

    SimulationOptions& options = simulation.getOptions();
    options.setLaunchRodLength(1.0 + (edit % 4));
    options.setWindSpeedAverage(edit % 6);
    options.getAverageWindModel().setDirection(0.1 * (edit % 10));
    options.setLaunchAltitude(100.0 * (edit % 3));
    options.setRandomSeed(edit);
    simulation.setName(std::format("edit {}", edit));
}

// A copy shares nothing that changes with the simulation and the rocket it was made from: while
// copies run on other threads, the thread that owns the original goes on changing the design and
// the options (what a user does while a simulation runs in the background), and each copy gives
// the flight of the design as it was when the copy was made. Under ThreadSanitizer this is the
// test that would find an object the copy still shares with its original.
TEST(SimulationThreading, TheOriginalCanBeChangedWhileItsCopiesRun)
{
    constexpr std::size_t kCopies   = 4;
    constexpr int         kMinEdits = 20;

    JavaTestPreferences     preferences;
    const TestEstesAlphaIII alpha;
    Simulation              simulation(*alpha.rocket, preferences.store);
    simulation.setFlightConfigurationId(testFcid(0));
    simulation.getOptions().setIsaAtmosphere(true);
    simulation.getOptions().setTimeStep(0.05);
    simulation.getOptions().setRandomSeed(1);

    const std::unique_ptr<Simulation> alone = simulation.duplicateForIndependentSimulation();
    ASSERT_TRUE(alone->simulate().has_value());
    ASSERT_NE(alone->getSimulatedData(), nullptr);

    std::vector<Copy> copies;
    copies.reserve(kCopies);
    for (std::size_t i = 0; i < kCopies; i++)
    {
        copies.push_back({.original   = 0,
                          .simulation = simulation.duplicateForIndependentSimulation(),
                          .failure    = std::nullopt});
    }

    std::atomic<std::size_t> finished{0};
    int                      edits = 0;
    {
        std::vector<std::jthread> threads;
        threads.reserve(copies.size());
        for (Copy& copy : copies)
        {
            threads.emplace_back(simulateAndCount, std::ref(*copy.simulation),
                                 std::ref(copy.failure), std::ref(finished));
        }
        // This thread owns the original: it edits while the copies fly.
        while (finished.load() < copies.size() || edits < kMinEdits)
        {
            editTheOriginal(alpha, simulation, edits);
            edits++;
        }
    }  // joins

    EXPECT_GE(edits, kMinEdits);
    for (const Copy& copy : copies)
    {
        EXPECT_EQ(copyProblem(copy, *alone), "");
    }
    // The original was changed, and its own flight is now another one.
    ASSERT_TRUE(simulation.simulate().has_value());
    ASSERT_NE(simulation.getSimulatedData(), nullptr);
    EXPECT_NE(flightDifferences(*alone->getSimulatedData(), *simulation.getSimulatedData()), "");
}

// ============================================== writing the preferences while copies run

/// Sets the stepper method of @p simulation's options to the one they have, which writes it to
/// the preference store the simulation shares with its original (as OpenRocket's options do),
/// and runs the simulation. The body of a worker thread.
void chooseTheStepperAndSimulate(Simulation& simulation, Failure& failure,
                                 std::atomic<std::size_t>& finished)
{
    SimulationOptions& options = simulation.getOptions();
    options.setSimulationStepperMethodChoice(options.getSimulationStepperMethodChoice());
    simulateAndCount(simulation, failure, finished);
}

/// Write number @p write of what an application stores all the time while simulations run in
/// the background (the size of a window, the last directory): a key comes and goes, in the root
/// node and in a child node, which changes the structure of the store's maps.
void writeToTheStore(Preferences& store, int write)
{
    const std::string key = std::format("A.WindowSize.{}", write % 7);
    store.putString(key, "800x600");
    store.getNode("recent").putString(key, "/tmp");
    store.remove(key);
    store.getNode("recent").remove(key);
}

// The regression test of the one object an independent copy still shares with the simulation
// it was made from: the preference store. simulate() reads the naming of the flight
// configuration from it on the worker thread when the run ends, and the options write the
// stepper method to it; the store was an unsynchronised map, so a write to any key by the
// thread that owns it (what a GUI does all the time) raced with every run (ThreadSanitizer:
// InMemoryPreferences::get() against InMemoryPreferences::put()). The store locks now. The
// keys written here are not the ones a simulation reads, so every copy still gives the flight
// and the description of the simulation run alone.
TEST(SimulationThreading, ThePreferenceStoreCanBeWrittenWhileCopiesRun)
{
    constexpr std::size_t kCopies    = 4;
    constexpr int         kMinWrites = 200;

    JavaTestPreferences     preferences;
    const TestEstesAlphaIII alpha;
    Simulation              simulation(*alpha.rocket, preferences.store);
    // The parachute opens before apogee: the nested coast run reads the store too.
    simulation.setFlightConfigurationId(testFcid(1));
    simulation.getOptions().setIsaAtmosphere(true);
    simulation.getOptions().setTimeStep(0.05);
    simulation.getOptions().setRandomSeed(1);

    const std::unique_ptr<Simulation> alone = simulation.duplicateForIndependentSimulation();
    ASSERT_TRUE(alone->simulate().has_value());
    ASSERT_NE(alone->getSimulatedData(), nullptr);

    std::vector<Copy> copies;
    copies.reserve(kCopies);
    for (std::size_t i = 0; i < kCopies; i++)
    {
        copies.push_back({.original   = 0,
                          .simulation = simulation.duplicateForIndependentSimulation(),
                          .failure    = std::nullopt});
    }

    std::atomic<std::size_t> finished{0};
    int                      writes = 0;
    {
        std::vector<std::jthread> threads;
        threads.reserve(copies.size());
        for (Copy& copy : copies)
        {
            threads.emplace_back(chooseTheStepperAndSimulate, std::ref(*copy.simulation),
                                 std::ref(copy.failure), std::ref(finished));
        }
        // This thread owns the store: it writes while the copies fly.
        while (finished.load() < copies.size() || writes < kMinWrites)
        {
            writeToTheStore(preferences.store, writes);
            writes++;
        }
    }  // joins

    EXPECT_GE(writes, kMinWrites);
    for (const Copy& copy : copies)
    {
        EXPECT_EQ(copyProblem(copy, *alone), "");
        EXPECT_EQ(copy.simulation->getSimulatedConfigurationDescription(),
                  alone->getSimulatedConfigurationDescription());
    }
    // What the workers wrote is in the store, and nothing of the comings and goings is left.
    EXPECT_EQ(preferences.store.getSimulationStepperMethodName(), "RK4");
    EXPECT_EQ(preferences.store.get("A.WindowSize.0"), std::nullopt);
    ASSERT_NE(preferences.store.findNode("recent"), nullptr);
    EXPECT_TRUE(preferences.store.findNode("recent")->keys().empty());
}

// ============================================================================ cancellation

/// The number of steps the simulation that is cancelled takes before the stop is requested.
constexpr int kStepsBeforeTheStop = 60;

/// What the thread of the simulation that is cancelled and the test's thread share.
struct Gate
{
    /// Set by the simulation thread when it has taken kStepsBeforeTheStop steps; it then waits
    /// for the stop request.
    std::atomic<bool> reached{false};
    /// Set by the simulation thread when simulate() has returned.
    std::atomic<bool> finished{false};
    /// The steps taken; counted on the simulation thread and read after it was joined.
    int steps{0};
    /// How simulate() ended; written by the simulation thread and read after it was joined.
    std::optional<QtRocket::Error> error;
    /// The message of an exception that left simulate().
    Failure exception;
};

/// The body of the thread whose simulation is cancelled: runs @p simulation with @p stopToken
/// (the token of the std::jthread it runs on) and a progress listener that, after step
/// kStepsBeforeTheStop, tells the test's thread that the flight is under way and holds the
/// simulation until the stop was requested. So the request reaches a simulation that is
/// running, on another thread, at a known step.
void simulateUntilStopped(const std::stop_token& stopToken, Simulation& simulation, Gate& gate)
{
    const auto progress =
        std::make_shared<ProgressListener>([&gate, stopToken](const SimulationStatus& /*status*/) {
            gate.steps++;
            if (gate.steps != kStepsBeforeTheStop)
            {
                return;
            }
            gate.reached.store(true);
            while (!stopToken.stop_requested())
            {
                std::this_thread::yield();
            }
        });
    const std::vector<std::shared_ptr<SimulationListener>> listeners{progress};
    try
    {
        const Result<void> result = simulation.simulate(stopToken, listeners);
        if (!result.has_value())
        {
            gate.error = result.error();
        }
    }
    catch (const std::exception& e)
    {
        gate.exception = std::format("exception: {}", e.what());
    }
    gate.finished.store(true);
}

/// Waits until the simulation thread of @p gate has reached the step at which it waits for the
/// stop request, or has ended without getting there.
void waitUntilTheFlightIsUnderWay(const Gate& gate)
{
    while (!gate.reached.load() && !gate.finished.load())
    {
        std::this_thread::yield();
    }
}

// A stop requested from another thread cancels a simulation that is running there: it ends after
// the step it was taking, with the flight so far as its data. Another simulation, which runs at
// the same time on a third thread with a stop token of its own that nobody stops, is not
// affected and gives the flight it gives alone.
TEST(SimulationThreading, AStopRequestedFromAnotherThreadCancelsTheRunningSimulation)
{
    JavaTestPreferences preferences;
    const Original      original =
        makeOriginal("Estes Alpha III", TestEstesAlphaIII().rocket, preferences.store, testFcid(0),
                     SimulationStepperMethod::RK4, 1);

    const std::unique_ptr<Simulation> alone =
        original.simulation->duplicateForIndependentSimulation();
    ASSERT_TRUE(alone->simulate().has_value());
    ASSERT_NE(alone->getSimulatedData(), nullptr);

    const std::unique_ptr<Simulation> cancelled =
        original.simulation->duplicateForIndependentSimulation();
    Copy bystander{.original   = 0,
                   .simulation = original.simulation->duplicateForIndependentSimulation(),
                   .failure    = std::nullopt};
    const std::stop_source nobodyStops;
    Gate                   gate;
    {
        // The simulation to cancel runs with the stop token of its std::jthread. (The other
        // thread must not: the destructor of a std::jthread requests a stop before it joins.)
        std::jthread       flight(simulateUntilStopped, std::ref(*cancelled), std::ref(gate));
        const std::jthread other(simulateWithTokenInto, std::ref(*bystander.simulation),
                                 std::ref(bystander.failure), nobodyStops.get_token());
        waitUntilTheFlightIsUnderWay(gate);
        // The request comes from this thread, while the simulation is held after that step.
        flight.request_stop();
    }  // joins

    ASSERT_TRUE(gate.reached.load()) << "the simulation ended before the stop was requested";
    EXPECT_EQ(gate.exception, Failure{});
    ASSERT_TRUE(gate.error.has_value()) << "the simulation was not cancelled";
    EXPECT_EQ(gate.error.value_or(QtRocket::Error{}).code, ErrorCode::CANCELLED);
    EXPECT_EQ(gate.error.value_or(QtRocket::Error{}).message, "The simulation was interrupted.");

    // It ended with the step in which the request arrived: no later step was taken.
    EXPECT_EQ(gate.steps, kStepsBeforeTheStop);
    const std::shared_ptr<FlightData>& data = cancelled->getSimulatedData();
    ASSERT_NE(data, nullptr);
    ASSERT_EQ(data->getBranchCount(), 1U);
    const FlightDataBranch& branch = data->getBranch(0);
    EXPECT_EQ(branch.getLength(), static_cast<std::size_t>(kStepsBeforeTheStop));
    EXPECT_NE(branch.getLastEvent(FlightEvent::Type::EXCEPTION), nullptr);
    EXPECT_NE(branch.getFirstEvent(FlightEvent::Type::LIFTOFF), nullptr) << "it was in flight";
    EXPECT_EQ(branch.getFirstEvent(FlightEvent::Type::GROUND_HIT), nullptr);
    EXPECT_EQ(branch.getFirstEvent(FlightEvent::Type::SIMULATION_END), nullptr);
    EXPECT_EQ(cancelled->getStoredStatus(), Simulation::Status::UPTODATE);

    // The flight so far is the beginning of the flight the simulation gives alone.
    const std::vector<double>* altitude =
        branch.getView(FlightDataType::builtin(FlightDataTypeId::TYPE_ALTITUDE));
    const std::vector<double>* altitudeAlone = alone->getSimulatedData()->getBranch(0).getView(
        FlightDataType::builtin(FlightDataTypeId::TYPE_ALTITUDE));
    ASSERT_NE(altitude, nullptr);
    ASSERT_NE(altitudeAlone, nullptr);
    ASSERT_GE(altitudeAlone->size(), altitude->size());
    EXPECT_EQ(*altitude, std::vector<double>(altitudeAlone->begin(),
                                             altitudeAlone->begin() +
                                                 static_cast<std::ptrdiff_t>(altitude->size())));

    // The simulation next to it went on to the ground.
    EXPECT_EQ(copyProblem(bystander, *alone), "");
}

}  // namespace
