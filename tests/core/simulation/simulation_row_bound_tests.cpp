// The rows of a flight data branch are bounded (SimulationInputRanges::kMaxBranchRows, a million;
// BasicEventSimulationEngine). QtRocket's own: OpenRocket simulates a flight that never lands
// until the maximum simulation time and keeps every row. The inputs of a run each have a range
// (SimulationInputRanges), but together they can still ask for any number of steps: the maximum
// time may be 1000000 s, a stepper may cut its steps to a twentieth of the time step, and a
// constant gravity of 0, which is inside its range, makes a flight that never comes down. So
// the engine ends a run whose branch holds the bound's rows and is not at its end, with a
// SimulationException, which Simulation::simulate() returns as an ordinary error.
//
// The bound of a run cannot be reached in a test (a million rows are a minute and 550 MB in a
// release build), so the tests give an engine a small bound of its own (setMaxBranchRows()) and
// pin the bound and one beyond it there; the measurement at the end runs into the real one.
//
// How far real flights are from the bound: the longest branch any test of the suite simulates
// has 4680 rows (a two-stage flight with roll control at the stable step of 0.01 s,
// ExtensionFlightStable; the longest of the golden comparisons has 1777), measured with a log
// line in the engine, removed again, over the 1170 branches the whole suite simulated in a
// debug build on 2026-10-10. The longest branch OpenRocket stored in the 16 example designs
// has 1139 rows (pinned below).

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <format>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/models/GravityModelType.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/simulation/BasicEventSimulationEngine.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationInputRanges.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/exception/SimulationException.h"
#include "QtRocket/simulation/listeners/AbstractSimulationListener.h"
#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/Error.h"
#include "goldens/GoldenExamples.h"
#include "rocket/TestRockets.h"
#include "simulation/SimulationRunSupport.h"

namespace
{

using QtRocket::BasicEventSimulationEngine;
using QtRocket::CloneableSimulationListener;
using QtRocket::DeploymentConfiguration;
using QtRocket::ErrorCode;
using QtRocket::FlightData;
using QtRocket::GravityModelType;
using QtRocket::LoadedDocument;
using QtRocket::RecoveryDevice;
using QtRocket::Result;
using QtRocket::Simulation;
using QtRocket::SimulationConditions;
using QtRocket::SimulationException;
using QtRocket::SimulationInputRanges;
using QtRocket::SimulationListener;
using QtRocket::SimulationOptions;
using QtRocket::SimulationStatus;
using QtRocket::Test::eventNames;
using QtRocket::Test::GoldenExample;
using QtRocket::Test::goldenExamples;
using QtRocket::Test::JavaTestPreferences;
using QtRocket::Test::loadGoldenExample;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::testFcid;

using Texts = std::vector<std::string>;

/// A simulation of the Estes Alpha III in its first test configuration (an A8 whose ejection
/// charge opens the parachute before the apogee) under the preferences of OpenRocket's tests:
/// calm air, no launch rod, the ISA atmosphere, a time step of 0.05 s and the random seed 0.
struct Flight
{
    JavaTestPreferences preferences;
    TestEstesAlphaIII   alpha;
    Simulation          simulation;

    Flight() : simulation(*alpha.rocket, preferences.store)
    {
        simulation.setFlightConfigurationId(testFcid(0));
        SimulationOptions& options = simulation.getOptions();
        options.setIsaAtmosphere(true);
        options.setTimeStep(0.05);
        options.setRandomSeed(0);
    }

    /// The same without gravity, for @p maxTime seconds, and with a parachute that never
    /// opens: a flight that never comes down and ends when its time is up. Every step of it is
    /// one of the flight stepper's, which makes one row, and nothing starts the nested run for
    /// the optimum altitude, so the rows of its one branch are the steps of the run.
    [[nodiscard]] static std::unique_ptr<Flight> withoutGravity(double maxTime)
    {
        auto                    flight = std::make_unique<Flight>();
        DeploymentConfiguration never;
        never.setDeployEvent(DeploymentConfiguration::DeployEvent::NEVER);
        flight->alpha.chute->getDeploymentConfigurations().setDefault(never);
        SimulationOptions& options = flight->simulation.getOptions();
        options.setGravityModelType(GravityModelType::CONSTANT);
        options.setConstantGravity(0.0);
        options.setMaxSimulationTime(maxTime);
        return flight;
    }
};

/// What a run of an engine gave.
struct Outcome
{
    /// The message of the SimulationException the run ended with; "<none>" for a run that ended
    /// by itself, "<no conditions>" when the options made none.
    std::string error{"<none>"};
    /// The rows of the first branch.
    std::size_t rows{0};
    /// The names of the events of the first branch, in order.
    Texts events;
};

/// Runs @p flight in a new engine, with @p bound as the most rows of a branch when it is given
/// and with @p listener, when there is one, listening.
[[nodiscard]] Outcome fly(Flight& flight, std::optional<std::size_t> bound,
                          const std::shared_ptr<SimulationListener>& listener = nullptr)
{
    Outcome                      outcome;
    Result<SimulationConditions> made = flight.simulation.getOptions().toSimulationConditions();
    if (!made.has_value())
    {
        outcome.error = "<no conditions>";
        return outcome;
    }
    const auto conditions = std::make_shared<SimulationConditions>(std::move(*made));
    conditions->setSimulation(&flight.simulation);
    if (listener != nullptr)
    {
        conditions->getSimulationListenerList().push_back(listener);
    }

    BasicEventSimulationEngine engine;
    if (bound.has_value())
    {
        engine.setMaxBranchRows(*bound);
    }
    try
    {
        engine.simulate(conditions);
    }
    catch (const SimulationException& e)
    {
        outcome.error = e.getMessage().value_or("<no message>");
    }
    const std::shared_ptr<FlightData>& data = engine.getFlightData();
    if (data != nullptr && data->getBranchCount() > 0)
    {
        outcome.rows   = data->getBranch(0).getLength();
        outcome.events = eventNames(*data, 0);
    }
    return outcome;
}

/// Whether @p events has the event named @p name.
[[nodiscard]] bool has(const Texts& events, const std::string& name)
{
    return std::ranges::find(events, name) != events.end();
}

TEST(SimulationRowBound, AnEngineStartsWithTheBoundOfTheRanges)
{
    BasicEventSimulationEngine engine;
    EXPECT_EQ(engine.getMaxBranchRows(), SimulationInputRanges::kMaxBranchRows);
    EXPECT_EQ(engine.getMaxBranchRows(), 1000000U);
    engine.setMaxBranchRows(7);
    EXPECT_EQ(engine.getMaxBranchRows(), 7U);

    EXPECT_EQ(BasicEventSimulationEngine::tooManyRows(SimulationInputRanges::kMaxBranchRows),
              "Simulation needs more than 1000000 steps for one flight data branch. Try "
              "selecting a longer time step or a shorter maximum simulation time.");
}

// Without gravity the rocket never comes down: the branch ends when the time is up, and every
// step until then is a row.
TEST(SimulationRowBound, AFlightThatNeverLandsRunsUntilItsTimeIsUp)
{
    const std::unique_ptr<Flight> flight    = Flight::withoutGravity(60.0);
    const Outcome                 unbounded = fly(*flight, std::nullopt);
    EXPECT_EQ(unbounded.error, "<none>");
    EXPECT_GT(unbounded.rows, 100U);
    ASSERT_FALSE(unbounded.events.empty());
    EXPECT_EQ(unbounded.events.back(), "SIMULATION_END");
    EXPECT_FALSE(has(unbounded.events, "GROUND_HIT"));
    EXPECT_FALSE(has(unbounded.events, "RECOVERY_DEVICE_DEPLOYMENT"));

    // Twice the time, and more rows with it: only the maximum time ends such a flight.
    const std::unique_ptr<Flight> longer = Flight::withoutGravity(120.0);
    EXPECT_GT(fly(*longer, std::nullopt).rows, unbounded.rows + 50);
}

// At the bound: a branch that needs exactly as many rows as the bound allows is simulated to
// its end, and is the flight it is without a bound.
TEST(SimulationRowBound, ABranchOfAsManyRowsAsTheBoundIsSimulatedToItsEnd)
{
    const std::unique_ptr<Flight> flight    = Flight::withoutGravity(60.0);
    const Outcome                 unbounded = fly(*flight, std::nullopt);
    ASSERT_GT(unbounded.rows, 100U);

    const Outcome atTheBound = fly(*flight, unbounded.rows);
    EXPECT_EQ(atTheBound.error, "<none>");
    EXPECT_EQ(atTheBound.rows, unbounded.rows);
    EXPECT_EQ(atTheBound.events, unbounded.events);
}

// One above: a branch that needs one row more than the bound allows ends the run with an
// ordinary SimulationException that names the number, where the step that would make the
// row would be taken. The flight data hold the rows up to there and the EXCEPTION event, as
// for every SimulationException.
TEST(SimulationRowBound, ABranchOfOneRowMoreEndsTheRunWithAnOrdinaryError)
{
    const std::unique_ptr<Flight> flight    = Flight::withoutGravity(60.0);
    const Outcome                 unbounded = fly(*flight, std::nullopt);
    ASSERT_GT(unbounded.rows, 100U);

    const Outcome oneBeyond = fly(*flight, unbounded.rows - 1);
    EXPECT_EQ(oneBeyond.error, BasicEventSimulationEngine::tooManyRows(unbounded.rows - 1));
    EXPECT_EQ(oneBeyond.rows, unbounded.rows - 1);
    ASSERT_FALSE(oneBeyond.events.empty());
    EXPECT_EQ(oneBeyond.events.back(), "EXCEPTION");
    EXPECT_FALSE(has(oneBeyond.events, "SIMULATION_END"));

    // Far beyond, with the number in the text.
    const Outcome short100 = fly(*flight, 100);
    EXPECT_EQ(short100.error,
              "Simulation needs more than 100 steps for one flight data branch. Try selecting a "
              "longer time step or a shorter maximum simulation time.");
    EXPECT_EQ(short100.rows, 100U);

    // A bound of none: no step is taken at all.
    const Outcome none = fly(*flight, 0);
    EXPECT_EQ(none.error, BasicEventSimulationEngine::tooManyRows(0));
    EXPECT_EQ(none.rows, 0U);
}

// An ordinary flight is far below any bound worth the name and does not notice one, also not
// one of exactly its rows: where a branch is at its end no step is asked for. (One row fewer
// need not end such a flight: its last step, in which the rocket comes to rest on the ground,
// makes two rows.)
TEST(SimulationRowBound, AFlightThatLandsDoesNotNoticeTheBound)
{
    Flight        flight;
    const Outcome unbounded = fly(flight, std::nullopt);
    EXPECT_EQ(unbounded.error, "<none>");
    ASSERT_FALSE(unbounded.events.empty());
    EXPECT_EQ(unbounded.events.back(), "SIMULATION_END");
    EXPECT_TRUE(has(unbounded.events, "GROUND_HIT"));
    EXPECT_LT(unbounded.rows, 2000U);

    const Outcome bounded = fly(flight, unbounded.rows);
    EXPECT_EQ(bounded.error, "<none>");
    EXPECT_EQ(bounded.rows, unbounded.rows);
    EXPECT_EQ(bounded.events, unbounded.events);
}

/// Notes how many rows the branch holds when a recovery device is deployed.
class RowsAtDeployment final : public CloneableSimulationListener<RowsAtDeployment>
{
public:
    [[nodiscard]] bool recoveryDeviceDeployment(SimulationStatus& status,
                                                const RecoveryDevice& /*device*/) override
    {
        if (status.getFlightDataBranch() != nullptr)
        {
            *m_rows = status.getFlightDataBranch()->getLength();
        }
        return true;
    }

    /// The rows at the last deployment; nullopt when there was none.
    [[nodiscard]] std::optional<std::size_t> rows() const { return *m_rows; }

private:
    /// Shared with the clones a run makes of the listener.
    std::shared_ptr<std::optional<std::size_t>> m_rows =
        std::make_shared<std::optional<std::size_t>>();
};

// When the parachute opens before the apogee the engine flies the whole flight once more in
// a nested engine, to find the optimum altitude. That run has the bound of the engine that
// starts it: with a bound that the branch itself has not reached when the parachute opens, and
// that the nested run, which goes on to the apogee, needs more rows than, the run ends there.
TEST(SimulationRowBound, TheNestedRunForTheOptimumAltitudeHasTheSameBound)
{
    Flight        flight;
    const auto    counter   = std::make_shared<RowsAtDeployment>();
    const Outcome unbounded = fly(flight, std::nullopt, counter);
    ASSERT_EQ(unbounded.error, "<none>");
    const std::optional<std::size_t> atDeployment = counter->rows();
    ASSERT_TRUE(atDeployment.has_value());
    const std::size_t rows = atDeployment.value_or(0);
    ASSERT_GT(rows, 10U);
    ASSERT_LT(rows + 1, unbounded.rows);

    const Outcome bounded = fly(flight, rows + 1);
    EXPECT_EQ(bounded.error, BasicEventSimulationEngine::tooManyRows(rows + 1));
    // The branch itself holds fewer rows than the bound: it was the nested run that needed more.
    EXPECT_EQ(bounded.rows, rows);
    ASSERT_FALSE(bounded.events.empty());
    EXPECT_EQ(bounded.events.back(), "EXCEPTION");
}

/// The longest stored branch of the 16 example designs as their files load, and the problems
/// of loading them.
struct LongestBranch
{
    std::size_t rows{0};
    std::size_t branches{0};
    Texts       problems;
};

[[nodiscard]] LongestBranch longestStoredBranchOfTheExamples()
{
    LongestBranch longest;
    for (const GoldenExample& example : goldenExamples())
    {
        const Result<LoadedDocument> loaded = loadGoldenExample(example);
        if (!loaded.has_value())
        {
            longest.problems.push_back(example.name + ": " + loaded.error().message);
            continue;
        }
        for (std::size_t i = 0; i < loaded->document->getSimulationCount(); i++)
        {
            const std::shared_ptr<FlightData>& data =
                loaded->document->getSimulation(i)->getSimulatedData();
            for (std::size_t b = 0; data != nullptr && b < data->getBranchCount(); b++)
            {
                longest.branches++;
                longest.rows = std::max(longest.rows, data->getBranch(b).getLength());
            }
        }
    }
    return longest;
}

// What OpenRocket itself stored for its 16 examples, at their time steps of 0.02 s to 0.05 s:
// the longest of the 69 branches is about a thousandth of the bound.
TEST(SimulationRowBound, TheLongestBranchOfTheExamplesIsFarBelowTheBound)
{
    const LongestBranch longest = longestStoredBranchOfTheExamples();
    EXPECT_EQ(longest.problems, Texts{});
    EXPECT_EQ(longest.branches, 69U);
    EXPECT_EQ(longest.rows, 1139U);
    EXPECT_LT(longest.rows * 100, SimulationInputRanges::kMaxBranchRows);
}

// The real bound, through Simulation::simulate(): a flight without gravity at the default time
// step and a maximum time of 1000000 s, which the ranges allow, ends with an ordinary error
// when its branch holds a million rows. Measured on 2026-10-10 in the ci-gcc build: a million
// rows after 49994 s of flight and 53 s of run time, about 550 MB (and many times that in a
// debug build), so it is no test of the suite:
//   build/ci-gcc/bin/QtRocket_core_tests --gtest_also_run_disabled_tests
//       --gtest_filter=SimulationRowBound.DISABLED_PrintsHowAFlightWithoutGravityEndsAtTheBound
TEST(SimulationRowBound, DISABLED_PrintsHowAFlightWithoutGravityEndsAtTheBound)
{
    const std::unique_ptr<Flight> flight = Flight::withoutGravity(1000000.0);
    ASSERT_TRUE(flight->simulation.validateInputs().has_value());

    const auto         start  = std::chrono::steady_clock::now();
    const Result<void> result = flight->simulation.simulate();
    const double       seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::SIMULATION_ABORTED);
    EXPECT_EQ(result.error().message,
              BasicEventSimulationEngine::tooManyRows(SimulationInputRanges::kMaxBranchRows));
    const std::shared_ptr<FlightData>& data = flight->simulation.getSimulatedData();
    ASSERT_NE(data, nullptr);
    ASSERT_EQ(data->getBranchCount(), 1U);
    EXPECT_EQ(data->getBranch(0).getLength(), SimulationInputRanges::kMaxBranchRows);
    std::cout << std::format("{} rows, flight time {} s, {:.1f} s of run time: {}\n",
                             data->getBranch(0).getLength(), data->getFlightTime(), seconds,
                             result.error().message)
              << std::flush;
}

}  // namespace
