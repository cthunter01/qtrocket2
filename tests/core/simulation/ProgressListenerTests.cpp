#include "QtRocket/simulation/listeners/ProgressListener.h"

// Tests of the progress listener, an addition of QtRocket (OpenRocket's counterpart is private to
// its simulation dialog and has no test): the listener alone, through the listener helper, and
// in whole flights, where it must report the flight and nothing else.

#include <cstddef>
#include <memory>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/simulation/listeners/SimulationListenerHelper.h"
#include "QtRocket/util/Error.h"
#include "rocket/TestRockets.h"
#include "simulation/SimulationRunSupport.h"
#include "simulation/SimulationStatusSupport.h"
#include "simulation/SimulationTestSupport.h"

namespace
{

using QtRocket::FlightConfigurationId;
using QtRocket::FlightData;
using QtRocket::FlightDataBranch;
using QtRocket::FlightEvent;
using QtRocket::ProgressListener;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::Simulation;
using QtRocket::SimulationListener;
using QtRocket::SimulationListenerHelper;
using QtRocket::SimulationStatus;
using QtRocket::Warning;
using QtRocket::WarningSet;
using QtRocket::Test::bugText;
using QtRocket::Test::JavaTestPreferences;
using QtRocket::Test::simulatedData;
using QtRocket::Test::TestBeta;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::testFcid;
using QtRocket::Test::TestStatus;

/// What a progress display keeps of the steps it was told about.
struct Progress
{
    std::vector<double>                  times;
    std::vector<const SimulationStatus*> statuses;
    /// The flight data branch of the status of each step.
    std::vector<const FlightDataBranch*> branches;
};

/// A callback that records the time, the status and the branch of every step in @p progress.
[[nodiscard]] ProgressListener::Callback recordInto(const std::shared_ptr<Progress>& progress)
{
    return [progress](const SimulationStatus& status) {
        progress->times.push_back(status.getSimulationTime());
        progress->statuses.push_back(&status);
        progress->branches.push_back(status.getFlightDataBranch().get());
    };
}

TEST(ProgressListener, CallsTheCallbackWithTheStatusAfterEveryStep)
{
    TestStatus       fixture;
    const auto       progress = std::make_shared<Progress>();
    ProgressListener listener(recordInto(progress));

    fixture.status.setSimulationTime(0.5);
    listener.postStep(fixture.status);
    fixture.status.setSimulationTime(1.25);
    listener.postStep(fixture.status);

    EXPECT_EQ(progress->times, (std::vector<double>{0.5, 1.25}));
    EXPECT_EQ(progress->statuses,
              (std::vector<const SimulationStatus*>{&fixture.status, &fixture.status}));

    // Only after a step.
    EXPECT_TRUE(listener.preStep(fixture.status));
    listener.startSimulation(fixture.status);
    listener.endSimulation(fixture.status, nullptr);
    EXPECT_EQ(progress->times.size(), 2U);
}

// As OpenRocket's SimulationProgressListener, which does not override isSystemListener(): the
// nested optimum-coast run of the engine keeps the system listeners only, and a progress display
// must not be told about the steps of that run.
TEST(ProgressListener, IsNotASystemListener)
{
    const ProgressListener listener([](const SimulationStatus& /*status*/) { });
    EXPECT_FALSE(listener.isSystemListener());
    const std::shared_ptr<SimulationListener> clone = listener.clone();
    ASSERT_NE(clone, nullptr);
    EXPECT_FALSE(clone->isSystemListener());
}

/// A callback with state of its own: it counts its calls and reports the count to @p seen.
class CountingCallback
{
public:
    explicit CountingCallback(std::shared_ptr<std::vector<int>> seen) : m_seen(std::move(seen)) { }

    void operator()(const SimulationStatus& /*status*/)
    {
        m_calls++;
        m_seen->push_back(m_calls);
    }

private:
    int                               m_calls{0};
    std::shared_ptr<std::vector<int>> m_seen;
};

TEST(ProgressListener, ItsClonesShareTheOneCallback)
{
    // The simulation runs on clones of its listeners (and on clones of the clones): the
    // callback's own state goes on counting, where a copied callback would start again.
    TestStatus                                fixture;
    const auto                                seen = std::make_shared<std::vector<int>>();
    const std::shared_ptr<SimulationListener> listener =
        std::make_shared<ProgressListener>(CountingCallback(seen));
    listener->postStep(fixture.status);
    const std::shared_ptr<SimulationListener> clone = listener->clone();
    clone->postStep(fixture.status);
    clone->postStep(fixture.status);
    const std::shared_ptr<SimulationListener> cloneOfClone = clone->clone();
    cloneOfClone->postStep(fixture.status);
    listener->postStep(fixture.status);
    EXPECT_EQ(*seen, (std::vector<int>{1, 2, 3, 4, 5}));
    const SimulationListener& cloned = *cloneOfClone;
    EXPECT_TRUE(typeid(cloned) == typeid(ProgressListener));
}

TEST(ProgressListener, NeedsACallback)
{
    EXPECT_EQ(bugText([] { const ProgressListener listener{ProgressListener::Callback{}}; }),
              "A progress listener needs a callback");
}

// Not a system listener, and still no "listeners affected the simulation" warning: it answers
// nothing and cannot change the status.
TEST(ProgressListener, ThroughTheHelperItReportsEveryStepWithoutAWarning)
{
    TestStatus fixture;
    const auto progress = std::make_shared<Progress>();
    fixture.conditions->getSimulationListenerList().push_back(
        std::make_shared<ProgressListener>(recordInto(progress)));
    for (int step = 1; step <= 3; step++)
    {
        fixture.status.setSimulationTime(0.1 * step);
        SimulationListenerHelper::firePostStep(fixture.status);
    }
    ASSERT_EQ(progress->times.size(), 3U);
    EXPECT_EQ(progress->times.back(), 0.1 * 3);
    const WarningSet& warnings = *fixture.status.getWarnings();
    EXPECT_TRUE(warnings.empty());
}

// ------------------------------------------------------------------------- whole flights

/// A simulation of @p rocket as the Java tests set one up, with the random seed 0, run with a
/// progress listener.
struct ProgressRun
{
    JavaTestPreferences       preferences;
    Simulation                simulation;
    std::shared_ptr<Progress> progress = std::make_shared<Progress>();

    ProgressRun(Rocket& rocket, const FlightConfigurationId& fcid)
      : simulation(rocket, preferences.store)
    {
        simulation.setFlightConfigurationId(fcid);
        simulation.getOptions().setIsaAtmosphere(true);
        simulation.getOptions().setTimeStep(0.05);
        simulation.getOptions().setRandomSeed(0);
    }

    /// Runs the simulation with the progress listener; "" or why it failed.
    [[nodiscard]] std::string run()
    {
        const Result<void> result =
            simulation.simulate({std::make_shared<ProgressListener>(recordInto(progress))});
        return result.has_value() ? std::string{} : result.error().message;
    }
};

/// What is wrong with the steps @p progress was told about, as the progress of a flight whose
/// data are @p data: every step must belong to a branch of the flight data, the branches must
/// come one after the other in the order of the flight data, and the time must never go back
/// within a branch. One line per problem; "" when there is none.
[[nodiscard]] std::string progressProblems(const Progress& progress, const FlightData& data)
{
    std::string problems;
    std::size_t current = 0;
    for (std::size_t i = 0; i < progress.times.size(); i++)
    {
        while (current < data.getBranchCount() && &data.getBranch(current) != progress.branches[i])
        {
            current++;
        }
        if (current >= data.getBranchCount())
        {
            return problems + "step " + std::to_string(i) +
                   ": a branch that is not the next branch of the flight data\n";
        }
        if (i > 0 && progress.branches[i - 1] == progress.branches[i] &&
            progress.times[i] < progress.times[i - 1])
        {
            problems += "step " + std::to_string(i) + ": the time went back\n";
        }
    }
    return problems;
}

/// The number of steps @p progress was told about for @p branch.
[[nodiscard]] std::size_t stepsOf(const Progress& progress, const FlightDataBranch& branch)
{
    std::size_t steps = 0;
    for (const FlightDataBranch* seen : progress.branches)
    {
        if (seen == &branch)
        {
            steps++;
        }
    }
    return steps;
}

// The regression test of the progress listener as a system listener: in a flight whose parachute
// opens before apogee the engine runs the flight once more, in a nested engine, to find the
// optimum altitude. A system listener is told about the steps of that run too: the time jumped
// back to 0 in a branch that is not in the flight data, replayed the ascent and jumped back
// again. The progress listener is told about the flight only.
TEST(ProgressListener, ReportsTheFlightAndNotTheNestedCoastRun)
{
    TestEstesAlphaIII alpha;
    ProgressRun       flight(*alpha.rocket, testFcid(1));
    ASSERT_EQ(flight.run(), "");

    const FlightData& data = simulatedData(flight.simulation);
    ASSERT_EQ(data.getBranchCount(), 1U);
    const FlightDataBranch& branch = data.getBranch(0);

    // The nested run did take place: the deployment came before the apogee.
    const FlightEvent* deployment =
        branch.getFirstEvent(FlightEvent::Type::RECOVERY_DEVICE_DEPLOYMENT);
    const FlightEvent* apogee = branch.getFirstEvent(FlightEvent::Type::APOGEE);
    ASSERT_NE(deployment, nullptr);
    ASSERT_NE(apogee, nullptr);
    ASSERT_LT(deployment->getTime(), apogee->getTime());

    EXPECT_EQ(progressProblems(*flight.progress, data), "");
    // One call per record: the extra record of the ground hit, which has no post-step hook,
    // and the last pass of the engine's loop, which has the hook and takes no step, even out.
    EXPECT_EQ(flight.progress->times.size(), branch.getLength());
    ASSERT_FALSE(flight.progress->times.empty());
    EXPECT_EQ(flight.progress->times.back(), data.getFlightTime());

    // An ordinary listener that changes nothing draws no warning.
    EXPECT_FALSE(data.getWarningSet().contains(Warning::kListenersAffected));
}

// Every branch of a two-stage flight is reported, one after the other, each with its own clock.
TEST(ProgressListener, ReportsEveryBranchOfATwoStageFlight)
{
    TestBeta beta;
    beta.rocket->getSelectedConfiguration().setAllStages();
    ProgressRun flight(*beta.rocket,
                       beta.rocket->getSelectedConfiguration().getFlightConfigurationId());
    ASSERT_EQ(flight.run(), "");

    const FlightData& data = simulatedData(flight.simulation);
    ASSERT_EQ(data.getBranchCount(), 2U);
    EXPECT_EQ(progressProblems(*flight.progress, data), "");

    // The sustainer's branch as in a single-stage flight; the booster's branch starts with the
    // records it took over from the sustainer's, which no step of its own made.
    const std::size_t sustainerSteps = stepsOf(*flight.progress, data.getBranch(0));
    const std::size_t boosterSteps   = stepsOf(*flight.progress, data.getBranch(1));
    EXPECT_EQ(sustainerSteps, data.getBranch(0).getLength());
    EXPECT_GT(boosterSteps, 0U);
    EXPECT_LT(boosterSteps, data.getBranch(1).getLength());
    EXPECT_EQ(sustainerSteps + boosterSteps, flight.progress->times.size());
    EXPECT_FALSE(data.getWarningSet().contains(Warning::kListenersAffected));
}

}  // namespace
