#include "QtRocket/simulation/listeners/RunSampleListener.h"

// Tests of the listener that samples a running simulation for the run dialog (a port of the
// SimulationProgressListener of OpenRocket's SimulationRunDialog.java:496-530, which no JUnit
// test covers): the events and the steps that publish, the 200 ms between the samples of the
// steps on an injected clock (no sleeps, no timing assertions), the clones, whole flights, and
// the samples taken on another thread while the simulation runs (the tsan preset's tests).

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <thread>
#include <typeinfo>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/RunSample.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationProgressEstimator.h"
#include "QtRocket/simulation/SimulationRun.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/listeners/ProgressListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/simulation/listeners/SimulationListenerHelper.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "rocket/TestRockets.h"
#include "simulation/HeldRunSupport.h"
#include "simulation/SimulationRunSupport.h"
#include "simulation/SimulationStatusSupport.h"
#include "simulation/SimulationTestSupport.h"

namespace
{

using QtRocket::Coordinate;
using QtRocket::FlightConfigurationId;
using QtRocket::FlightData;
using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::FlightDataTypeId;
using QtRocket::FlightEvent;
using QtRocket::ProgressListener;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::RunSample;
using QtRocket::RunSampleListener;
using QtRocket::Simulation;
using QtRocket::SimulationAbort;
using QtRocket::SimulationListener;
using QtRocket::SimulationListenerHelper;
using QtRocket::SimulationProgressEstimator;
using QtRocket::SimulationRun;
using QtRocket::SimulationStatus;
using QtRocket::Warning;
using QtRocket::Test::bugText;
using QtRocket::Test::JavaTestPreferences;
using QtRocket::Test::kStepsBeforeTheHold;
using QtRocket::Test::runHolding;
using QtRocket::Test::simulatedData;
using QtRocket::Test::TestBeta;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::testFcid;
using QtRocket::Test::TestStatus;
using Type = QtRocket::FlightEvent::Type;
using std::chrono::milliseconds;
using TimePoint = std::chrono::steady_clock::time_point;

// ===================================================================== clocks and events

/// A clock whose time the test sets: *now after the clock's origin.
struct ManualClock
{
    std::shared_ptr<milliseconds> now = std::make_shared<milliseconds>(0);

    [[nodiscard]] RunSampleListener::Clock clock() const
    {
        return [time = now] { return TimePoint{} + *time; };
    }
};

/// A clock that starts at its origin and goes on by @p step at every reading, counted in
/// *readings. A simulation reads it once per step, so which steps publish depends on the steps
/// alone; it is read on the simulation's thread only.
[[nodiscard]] RunSampleListener::Clock steppingClock(milliseconds                         step,
                                                     const std::shared_ptr<std::int64_t>& readings)
{
    return [step, readings] {
        const TimePoint now = TimePoint{} + (step * *readings);
        (*readings)++;
        return now;
    };
}

/// An event of type @p type at @p time, with the data its type needs.
[[nodiscard]] FlightEvent eventOf(Type type, double time)
{
    switch (type)
    {
        case Type::SIM_WARN:
            return FlightEvent{type, time, nullptr,
                               FlightEvent::warningData(Warning::kListenersAffected)};
        case Type::SIM_ABORT:
            return FlightEvent{type, time, nullptr,
                               SimulationAbort(SimulationAbort::Cause::NO_MOTORS_FIRED)};
        default:
            return FlightEvent{type, time};
    }
}

/// The times of @p samples, in order.
[[nodiscard]] std::vector<double> timesOf(const std::vector<RunSample>& samples)
{
    std::vector<double> times;
    times.reserve(samples.size());
    for (const RunSample& sample : samples)
    {
        times.push_back(sample.simulationTime);
    }
    return times;
}

/// Calls the listener's postStep() for @p status at @p time on @p clock.
void stepAt(RunSampleListener& listener, SimulationStatus& status, const ManualClock& clock,
            milliseconds time, double simulationTime)
{
    *clock.now = time;
    status.setSimulationTime(simulationTime);
    listener.postStep(status);
}

// ============================================================================ the hooks

// Java's handleFlightEvent(): a sample for LAUNCH, APOGEE and SIMULATION_END, nothing for the
// other events, and every event is handled (true). The events do not read the clock.
TEST(RunSampleListener, PublishesASampleForTheLaunchTheApogeeAndTheEnd)
{
    TestStatus        fixture;
    const auto        readings = std::make_shared<std::int64_t>(0);
    RunSampleListener listener(steppingClock(milliseconds(50), readings));
    double            time = 0.0;
    for (const Type type : FlightEvent::kAllTypes)
    {
        time += 1.0;
        fixture.status.setSimulationTime(time);
        EXPECT_TRUE(listener.handleFlightEvent(fixture.status, eventOf(type, time))) << name(type);
    }
    // LAUNCH, APOGEE and SIMULATION_END are the 1st, the 8th and the 11th type.
    EXPECT_EQ(timesOf(listener.takeSamples()), (std::vector<double>{1.0, 8.0, 11.0}));
    EXPECT_EQ(*readings, 0);
}

// A sample keeps the status's time, altitude, vertical velocity and sustainer (RunSample::of()).
TEST(RunSampleListener, ASampleIsTheStatusAsItWasThen)
{
    TestStatus        fixture;
    const ManualClock clock;
    RunSampleListener listener(clock.clock());
    fixture.status.setSimulationTime(2.5);
    fixture.status.setRocketPosition(Coordinate(1.0, 2.0, 80.0));
    fixture.status.setRocketVelocity(Coordinate(3.0, 4.0, 25.5));
    EXPECT_TRUE(listener.handleFlightEvent(fixture.status, eventOf(Type::APOGEE, 2.5)));
    fixture.status.setRocketPosition(Coordinate(0.0, 0.0, 90.0));
    listener.postStep(fixture.status);

    EXPECT_EQ(listener.takeSamples(), (std::vector<RunSample>{
                                          RunSample{.simulationTime   = 2.5,
                                                    .altitude         = 80.0,
                                                    .verticalVelocity = 25.5,
                                                    .sustainerActive  = true},
                                          RunSample{.simulationTime   = 2.5,
                                                    .altitude         = 90.0,
                                                    .verticalVelocity = 25.5,
                                                    .sustainerActive  = true},
                                      }));
}

// Java's postStep(): the first step publishes, then a step publishes when 200 ms have passed
// since the last step that published (now >= time + UPDATE_MS).
TEST(RunSampleListener, TheStepsPublishASampleEvery200Milliseconds)
{
    EXPECT_EQ(RunSampleListener::kUpdateInterval, milliseconds(200));
    TestStatus        fixture;
    const ManualClock clock;
    RunSampleListener listener(clock.clock());
    stepAt(listener, fixture.status, clock, milliseconds(0), 0.01);
    stepAt(listener, fixture.status, clock, milliseconds(199), 0.02);
    stepAt(listener, fixture.status, clock, milliseconds(200), 0.03);
    stepAt(listener, fixture.status, clock, milliseconds(399), 0.04);
    stepAt(listener, fixture.status, clock, milliseconds(400), 0.05);
    stepAt(listener, fixture.status, clock, milliseconds(1000), 0.06);
    stepAt(listener, fixture.status, clock, milliseconds(1199), 0.07);
    EXPECT_EQ(timesOf(listener.takeSamples()), (std::vector<double>{0.01, 0.03, 0.05, 0.06}));

    // A clock far from its origin: the first step publishes all the same.
    TestStatus        other;
    const ManualClock late;
    *late.now = milliseconds(123456);
    RunSampleListener first(late.clock());
    first.postStep(other.status);
    EXPECT_EQ(first.takeSamples().size(), 1U);
}

// The samples of the events do not count for the steps (Java sets its time in postStep() only).
TEST(RunSampleListener, TheEventsDoNotDelayTheSteps)
{
    TestStatus        fixture;
    const ManualClock clock;
    RunSampleListener listener(clock.clock());
    stepAt(listener, fixture.status, clock, milliseconds(0), 0.0);
    *clock.now = milliseconds(100);
    fixture.status.setSimulationTime(0.5);
    EXPECT_TRUE(listener.handleFlightEvent(fixture.status, eventOf(Type::APOGEE, 0.5)));
    stepAt(listener, fixture.status, clock, milliseconds(200), 1.0);
    stepAt(listener, fixture.status, clock, milliseconds(300), 1.5);
    EXPECT_EQ(timesOf(listener.takeSamples()), (std::vector<double>{0.0, 0.5, 1.0}));
}

// takeSamples() takes the samples once, in the order they were published.
TEST(RunSampleListener, TakesTheSamplesOnceInOrder)
{
    TestStatus        fixture;
    const ManualClock clock;
    RunSampleListener listener(clock.clock());
    EXPECT_TRUE(listener.takeSamples().empty());
    stepAt(listener, fixture.status, clock, milliseconds(0), 0.25);
    fixture.status.setSimulationTime(0.5);
    EXPECT_TRUE(listener.handleFlightEvent(fixture.status, eventOf(Type::LAUNCH, 0.5)));
    EXPECT_EQ(timesOf(listener.takeSamples()), (std::vector<double>{0.25, 0.5}));
    EXPECT_TRUE(listener.takeSamples().empty());
    stepAt(listener, fixture.status, clock, milliseconds(250), 0.75);
    EXPECT_EQ(timesOf(listener.takeSamples()), (std::vector<double>{0.75}));
}

// The simulation runs on clones of its listeners: they share the samples and the time of the
// last step that published, so the listener the caller holds gets every sample.
TEST(RunSampleListener, ItsClonesShareTheSamplesAndTheClock)
{
    TestStatus        fixture;
    const ManualClock clock;
    const auto        listener = std::make_shared<RunSampleListener>(clock.clock());
    const std::shared_ptr<SimulationListener> clone        = listener->clone();
    const std::shared_ptr<SimulationListener> cloneOfClone = clone->clone();
    const SimulationListener&                 cloned       = *cloneOfClone;
    ASSERT_TRUE(typeid(cloned) == typeid(RunSampleListener));

    *clock.now = milliseconds(0);
    fixture.status.setSimulationTime(0.0);
    listener->postStep(fixture.status);
    // 100 ms after the original published: not yet, also on a clone.
    *clock.now = milliseconds(100);
    fixture.status.setSimulationTime(0.1);
    clone->postStep(fixture.status);
    *clock.now = milliseconds(200);
    fixture.status.setSimulationTime(0.2);
    cloneOfClone->postStep(fixture.status);
    auto& clonedSampler = dynamic_cast<RunSampleListener&>(*clone);
    fixture.status.setSimulationTime(0.3);
    EXPECT_TRUE(clonedSampler.handleFlightEvent(fixture.status, eventOf(Type::APOGEE, 0.3)));

    EXPECT_EQ(timesOf(listener->takeSamples()), (std::vector<double>{0.0, 0.2, 0.3}));
    EXPECT_TRUE(clonedSampler.takeSamples().empty());
}

// Not a system listener (Java's is not), so the nested optimum-coast run does not reach it; and
// through the listener helper it draws no "listeners affected the simulation" warning.
TEST(RunSampleListener, ChangesNothingAndIsNotASystemListener)
{
    TestStatus        fixture;
    const ManualClock clock;
    const auto        listener = std::make_shared<RunSampleListener>(clock.clock());
    EXPECT_FALSE(listener->isSystemListener());
    fixture.conditions->getSimulationListenerList().push_back(listener);

    fixture.status.setSimulationTime(0.0);
    EXPECT_TRUE(SimulationListenerHelper::fireHandleFlightEvent(fixture.status,
                                                                eventOf(Type::LAUNCH, 0.0)));
    fixture.status.setSimulationTime(0.05);
    SimulationListenerHelper::firePostStep(fixture.status);
    EXPECT_EQ(timesOf(listener->takeSamples()), (std::vector<double>{0.0, 0.05}));
    EXPECT_TRUE(fixture.status.getWarnings()->empty());
}

TEST(RunSampleListener, NeedsAClock)
{
    EXPECT_EQ(bugText([] { const RunSampleListener listener{RunSampleListener::Clock{}}; }),
              "A run sample listener needs a clock");
}

// The default clock is the steady clock; its first step publishes, as every first step does.
TEST(RunSampleListener, ByDefaultItReadsTheSteadyClock)
{
    TestStatus        fixture;
    RunSampleListener listener;
    listener.postStep(fixture.status);
    EXPECT_EQ(listener.takeSamples().size(), 1U);
}

// ======================================================================== whole flights

/// A simulation of @p rocket for @p fcid as the Java tests set one up (the ISA atmosphere, a
/// time step of 0.05 s, the random seed 0).
struct SampledFlight
{
    JavaTestPreferences preferences;
    Simulation          simulation;
    /// The readings of the sampler's clock.
    std::shared_ptr<std::int64_t> readings = std::make_shared<std::int64_t>(0);
    /// 50 ms a step, so every fourth step publishes.
    std::shared_ptr<RunSampleListener> sampler =
        std::make_shared<RunSampleListener>(steppingClock(milliseconds(50), readings));
    /// The number of steps, from a progress listener next to the sampler.
    std::shared_ptr<std::size_t> steps = std::make_shared<std::size_t>(0);

    SampledFlight(Rocket& rocket, const FlightConfigurationId& fcid)
      : simulation(rocket, preferences.store)
    {
        simulation.setFlightConfigurationId(fcid);
        simulation.getOptions().setIsaAtmosphere(true);
        simulation.getOptions().setTimeStep(0.05);
        simulation.getOptions().setRandomSeed(0);
    }

    /// Runs the simulation with the sampler and the step counter; "" or why it failed.
    [[nodiscard]] std::string run()
    {
        const auto counter = std::make_shared<ProgressListener>(
            [count = steps](const SimulationStatus& /*status*/) { (*count)++; });
        const Result<void> result = simulation.simulate({sampler, counter});
        return result.has_value() ? std::string{} : result.error().message;
    }
};

/// The largest value of column @p type of @p branch; NaN when the branch has no such column.
[[nodiscard]] double largestOf(const FlightDataBranch& branch, FlightDataTypeId type)
{
    const std::vector<double>* column = branch.getView(FlightDataType::builtin(type));
    if (column == nullptr || column->empty())
    {
        ADD_FAILURE() << "no column " << static_cast<int>(type);
        return std::numeric_limits<double>::quiet_NaN();
    }
    return std::ranges::max(*column);
}

/// The time of the first event of @p type in @p branch; NaN when it has none.
[[nodiscard]] double firstEventTime(const FlightDataBranch& branch, Type type)
{
    const FlightEvent* event = branch.getFirstEvent(type);
    if (event == nullptr)
    {
        ADD_FAILURE() << "no " << name(type) << " event";
        return std::numeric_limits<double>::quiet_NaN();
    }
    return event->getTime();
}

/// The time of the last event of @p type in @p branch; NaN when it has none.
[[nodiscard]] double lastEventTime(const FlightDataBranch& branch, Type type)
{
    const FlightEvent* event = branch.getLastEvent(type);
    if (event == nullptr)
    {
        ADD_FAILURE() << "no " << name(type) << " event";
        return std::numeric_limits<double>::quiet_NaN();
    }
    return event->getTime();
}

/// The first time of @p branch after @p time; NaN when there is none.
[[nodiscard]] double firstTimeAfter(const FlightDataBranch& branch, double time)
{
    const std::vector<double>* times =
        branch.getView(FlightDataType::builtin(FlightDataTypeId::TYPE_TIME));
    if (times == nullptr)
    {
        ADD_FAILURE() << "no time column";
        return std::numeric_limits<double>::quiet_NaN();
    }
    const auto after = std::ranges::upper_bound(*times, time);
    if (after == times->end())
    {
        ADD_FAILURE() << "no time after " << time;
        return std::numeric_limits<double>::quiet_NaN();
    }
    return *after;
}

/// The time of the first of @p samples; NaN when there is none.
[[nodiscard]] double firstSampleTime(const std::vector<RunSample>& samples)
{
    if (samples.empty())
    {
        ADD_FAILURE() << "no samples";
        return std::numeric_limits<double>::quiet_NaN();
    }
    return samples.front().simulationTime;
}

/// The time of the last of @p samples; NaN when there is none.
[[nodiscard]] double lastSampleTime(const std::vector<RunSample>& samples)
{
    if (samples.empty())
    {
        ADD_FAILURE() << "no samples";
        return std::numeric_limits<double>::quiet_NaN();
    }
    return samples.back().simulationTime;
}

/// Whether one of @p samples is at the time @p time.
[[nodiscard]] bool hasSampleAt(const std::vector<RunSample>& samples, double time)
{
    return std::ranges::find(samples, time, &RunSample::simulationTime) != samples.end();
}

/// Whether the times of @p samples never go back.
[[nodiscard]] bool inTimeOrder(const std::vector<RunSample>& samples)
{
    return std::ranges::is_sorted(samples, {}, &RunSample::simulationTime);
}

/// The samples of a flight with a stage that separates, split where the first sample that is
/// not of the sustainer comes.
struct SplitSamples
{
    std::vector<RunSample> sustainer;
    std::vector<RunSample> separated;
    /// Whether every sample after the split is not of the sustainer either.
    bool apart{false};
};

/// @p samples split at the first that is not of the sustainer.
[[nodiscard]] SplitSamples splitAtTheSeparatedStage(const std::vector<RunSample>& samples)
{
    const auto   split = std::ranges::find(samples, false, &RunSample::sustainerActive);
    SplitSamples result{.sustainer = std::vector<RunSample>(samples.begin(), split),
                        .separated = std::vector<RunSample>(split, samples.end()),
                        .apart     = false};
    result.apart = std::ranges::none_of(result.separated, &RunSample::sustainerActive);
    return result;
}

/// Feeds @p samples to @p estimator in batches of @p batch, as a run dialog that polls takes
/// them; whether the progress never went back.
[[nodiscard]] bool feedInBatches(SimulationProgressEstimator&  estimator,
                                 const std::vector<RunSample>& samples, std::size_t batch)
{
    const std::span<const RunSample> all(samples);
    int                              progress = estimator.getProgress();
    bool                             monotone = true;
    for (std::size_t i = 0; i < all.size(); i += batch)
    {
        estimator.process(all.subspan(i, std::min(batch, all.size() - i)));
        monotone = monotone && estimator.getProgress() >= progress;
        progress = estimator.getProgress();
    }
    return monotone;
}

// A flight's samples: one for each of its LAUNCH, APOGEE and SIMULATION_END events and one for
// every fourth step (the first included), the first at the launch, the last at the end.
TEST(RunSampleListener, SamplesAFlightAtItsEventsAndEvery200MillisecondsOfSteps)
{
    TestEstesAlphaIII alpha;
    SampledFlight     flight(*alpha.rocket, testFcid(1));
    ASSERT_EQ(flight.run(), "");

    const FlightData& data = simulatedData(flight.simulation);
    ASSERT_EQ(data.getBranchCount(), 1U);
    const FlightDataBranch& branch = data.getBranch(0);

    const std::vector<RunSample> samples = flight.sampler->takeSamples();
    const std::size_t            steps   = *flight.steps;
    ASSERT_GT(steps, 0U);
    EXPECT_EQ(*flight.readings, static_cast<std::int64_t>(steps));
    EXPECT_EQ(samples.size(), 3 + ((steps + 3) / 4));
    EXPECT_TRUE(inTimeOrder(samples));
    EXPECT_EQ(firstSampleTime(samples), firstEventTime(branch, Type::LAUNCH));
    // The APOGEE event has the time of the step before the one that found it, and is handled
    // after that step: its sample has the time of the next row.
    EXPECT_TRUE(hasSampleAt(samples, firstTimeAfter(branch, firstEventTime(branch, Type::APOGEE))));
    EXPECT_EQ(lastSampleTime(samples), data.getFlightTime());
    EXPECT_EQ(lastSampleTime(samples), lastEventTime(branch, Type::SIMULATION_END));
    EXPECT_TRUE(std::ranges::all_of(samples, &RunSample::sustainerActive));
    EXPECT_FALSE(data.getWarningSet().contains(Warning::kListenersAffected));
}

// The samples of a two-stage flight: the sustainer's branch first, to its end, then the
// booster's, whose samples are not of the sustainer.
TEST(RunSampleListener, SamplesTheBoosterBranchAfterTheSustainers)
{
    TestBeta beta;
    beta.rocket->getSelectedConfiguration().setAllStages();
    SampledFlight flight(*beta.rocket,
                         beta.rocket->getSelectedConfiguration().getFlightConfigurationId());
    ASSERT_EQ(flight.run(), "");
    const FlightData& data = simulatedData(flight.simulation);
    ASSERT_EQ(data.getBranchCount(), 2U);

    const SplitSamples split = splitAtTheSeparatedStage(flight.sampler->takeSamples());
    EXPECT_FALSE(split.sustainer.empty());
    EXPECT_FALSE(split.separated.empty());
    EXPECT_TRUE(split.apart);
    EXPECT_TRUE(inTimeOrder(split.sustainer));
    EXPECT_TRUE(inTimeOrder(split.separated));
    EXPECT_EQ(lastSampleTime(split.sustainer),
              lastEventTime(data.getBranch(0), Type::SIMULATION_END));
    EXPECT_EQ(lastSampleTime(split.separated),
              lastEventTime(data.getBranch(1), Type::SIMULATION_END));
}

// The samples of a flight, taken in batches as a run dialog takes them, drive the estimate up to
// the landing; the maxima are values the sustainer had.
TEST(RunSampleListener, TheSamplesOfAFlightDriveTheEstimateToTheLanding)
{
    TestEstesAlphaIII           alpha;
    SampledFlight               flight(*alpha.rocket, testFcid(0));
    SimulationProgressEstimator estimator(flight.simulation.getActiveConfiguration());
    ASSERT_EQ(flight.run(), "");
    const FlightData& data = simulatedData(flight.simulation);
    ASSERT_EQ(data.getBranchCount(), 1U);
    const FlightDataBranch& branch = data.getBranch(0);

    const std::vector<RunSample> samples = flight.sampler->takeSamples();
    ASSERT_GE(samples.size(), 6U);
    EXPECT_TRUE(feedInBatches(estimator, samples, 3));
    EXPECT_EQ(estimator.getStage(), SimulationProgressEstimator::Stage::LANDING);
    EXPECT_GE(estimator.getProgress(), 70);
    EXPECT_EQ(estimator.getLastSample(), std::optional<RunSample>(samples.back()));

    EXPECT_GT(estimator.getMaxAltitude(), 0.0);
    EXPECT_LE(estimator.getMaxAltitude(), largestOf(branch, FlightDataTypeId::TYPE_ALTITUDE));
    EXPECT_GT(estimator.getMaxVelocity(), 0.0);
    EXPECT_LE(estimator.getMaxVelocity(), largestOf(branch, FlightDataTypeId::TYPE_VELOCITY_Z));
    EXPECT_EQ(estimator.getApogeeAltitude(), std::max(estimator.getMaxAltitude(), 1.0));

    estimator.finish();
    EXPECT_EQ(estimator.getProgress(), 100);
}

// ============================================================================== threads

/// The original of the runs on other threads: the Estes Alpha III as the Java tests fly it, with
/// the random seed 1.
struct ThreadedOriginal
{
    JavaTestPreferences preferences;
    TestEstesAlphaIII   alpha;
    Simulation          simulation{*alpha.rocket, preferences.store};

    ThreadedOriginal()
    {
        simulation.setFlightConfigurationId(testFcid(0));
        simulation.getOptions().setIsaAtmosphere(true);
        simulation.getOptions().setTimeStep(0.05);
        simulation.getOptions().setRandomSeed(1);
    }

    /// The samples a sampler on a 50 ms stepping clock takes of a duplicate run on this thread.
    [[nodiscard]] std::vector<RunSample> samplesAlone() const
    {
        const std::unique_ptr<Simulation> alone   = simulation.duplicateForIndependentSimulation();
        const auto                        sampler = std::make_shared<RunSampleListener>(
            steppingClock(milliseconds(50), std::make_shared<std::int64_t>(0)));
        const Result<void> result = alone->simulate({sampler});
        EXPECT_TRUE(result.has_value());
        return sampler->takeSamples();
    }
};

/// Appends @p more to @p samples.
void appendSamples(std::vector<RunSample>& samples, const std::vector<RunSample>& more)
{
    samples.insert(samples.end(), more.begin(), more.end());
}

/// What a sampler on a 50 ms stepping clock gave of a run held on its thread after
/// kStepsBeforeTheHold steps.
struct HeldSamples
{
    /// Whether the run got to the hold.
    bool reached{false};
    /// The samples this thread took while the run was held.
    std::vector<RunSample> whileHeld;
    /// The samples it took after the run.
    std::vector<RunSample> after;
};

/// Runs @p run held on a thread of its own with a sampler, taking the samples while it is held
/// and after it.
[[nodiscard]] HeldSamples runHeldWithASampler(SimulationRun& run)
{
    HeldSamples result;
    const auto  sampler = std::make_shared<RunSampleListener>(
        steppingClock(milliseconds(50), std::make_shared<std::int64_t>(0)));
    const std::vector<std::shared_ptr<SimulationListener>> listeners{sampler};
    result.reached = runHolding(
        run, std::stop_token{}, [&result, &sampler] { result.whileHeld = sampler->takeSamples(); },
        listeners);
    result.after = sampler->takeSamples();
    return result;
}

// While a run is held on its thread after step 30, this thread takes the samples published so
// far: the launch's and those of the steps until then; the rest come after the run. Together
// they are the samples of the same flight run alone.
TEST(RunSampleListener, TheSamplesCanBeTakenWhileTheRunIsHeldOnItsThread)
{
    const ThreadedOriginal       original;
    const std::vector<RunSample> alone = original.samplesAlone();

    SimulationRun     run(original.simulation);
    const HeldSamples held = runHeldWithASampler(run);
    ASSERT_TRUE(held.reached);
    EXPECT_EQ(run.getOutcome(), SimulationRun::Outcome::SUCCEEDED);

    // The launch, then steps 1, 5, ..., 29 (a step publishes every 200 ms of 50 ms readings).
    const auto stepsBeforeTheHold = static_cast<std::size_t>(kStepsBeforeTheHold);
    EXPECT_EQ(held.whileHeld.size(), 1 + ((stepsBeforeTheHold + 3) / 4));
    std::vector<RunSample> all = held.whileHeld;
    appendSamples(all, held.after);
    EXPECT_EQ(all, alone);
}

/// The body of a thread that runs @p run with @p listeners and then sets @p finished.
void runAndFinish(SimulationRun&                                          run,
                  const std::vector<std::shared_ptr<SimulationListener>>& listeners,
                  std::atomic<bool>&                                      finished)
{
    run.run(std::stop_token{}, listeners);
    finished.store(true);
}

/// Runs @p run on a thread of its own with a sampler on a 50 ms stepping clock, taking its
/// samples on this thread as often as it can while the run goes on; the samples taken.
[[nodiscard]] std::vector<RunSample> takeSamplesWhileItRuns(SimulationRun& run)
{
    const auto sampler = std::make_shared<RunSampleListener>(
        steppingClock(milliseconds(50), std::make_shared<std::int64_t>(0)));
    const std::vector<std::shared_ptr<SimulationListener>> listeners{sampler};
    std::atomic<bool>                                      finished{false};
    std::vector<RunSample>                                 taken;
    {
        const std::jthread thread(runAndFinish, std::ref(run), std::cref(listeners),
                                  std::ref(finished));
        while (!finished.load())
        {
            appendSamples(taken, sampler->takeSamples());
            std::this_thread::yield();
        }
    }  // joins
    appendSamples(taken, sampler->takeSamples());
    return taken;
}

// This thread takes the samples as often as it can while the run goes on another thread: none
// is lost and none comes twice.
TEST(RunSampleListener, TheSamplesCanBeTakenOnAnotherThreadWhileTheSimulationRuns)
{
    const ThreadedOriginal       original;
    const std::vector<RunSample> alone = original.samplesAlone();

    SimulationRun                run(original.simulation);
    const std::vector<RunSample> taken = takeSamplesWhileItRuns(run);
    EXPECT_EQ(run.getOutcome(), SimulationRun::Outcome::SUCCEEDED);
    EXPECT_EQ(taken, alone);
}

}  // namespace
