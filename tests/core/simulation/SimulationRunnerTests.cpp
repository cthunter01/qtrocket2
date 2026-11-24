#include "QtRocket/simulation/SimulationRunner.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <exception>
#include <format>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <ratio>
#include <stop_token>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/events/DocumentChangeEvent.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/RunSample.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationBatchCompletion.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationProgressEstimator.h"
#include "QtRocket/simulation/SimulationRun.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/exception/SimulationException.h"
#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"
#include "QtRocket/simulation/listeners/ProgressListener.h"
#include "QtRocket/simulation/listeners/RunSampleListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Signal.h"
#include "document/DocumentTestSupport.h"
#include "goldens/GoldenExamples.h"
#include "rocket/TestRockets.h"
#include "simulation/FlightDifferences.h"
#include "simulation/SimulationRunSupport.h"
#include "simulation/SimulationRunnerSupport.h"
#include "simulation/SimulationTestSupport.h"

// SimulationRunner: the pool of threads that runs simulations (SimulationRun) for the owning
// thread, which polls it for the news of its jobs. Every test drives it from the test's thread
// as the GUI's timer will (RunnerPoller), holds jobs at known steps with stop-aware gates and
// meetings (SimulationRunnerSupport.h), and compares flights with the lone flights of the same
// simulations, never with pinned values (a fresh flight is chaotic). These are tests the tsan
// preset is there for, with simulation_threading_tests.cpp (cmake --workflow --preset tsan):
// the runner's mailboxes must give no report.

namespace
{

using QtRocket::CloneableSimulationListener;
using QtRocket::DocumentChangeEvent;
using QtRocket::ErrorCode;
using QtRocket::FlightData;
using QtRocket::LoadedDocument;
using QtRocket::OpenRocketDocument;
using QtRocket::ProgressListener;
using QtRocket::RunSample;
using QtRocket::RunSampleListener;
using QtRocket::Simulation;
using QtRocket::SimulationBatchCompletion;
using QtRocket::SimulationException;
using QtRocket::SimulationListener;
using QtRocket::SimulationOptions;
using QtRocket::SimulationProgressEstimator;
using QtRocket::SimulationRun;
using QtRocket::SimulationRunner;
using QtRocket::SimulationStatus;
using QtRocket::Test::AlphaDocument;
using QtRocket::Test::bugText;
using QtRocket::Test::flightDifferences;
using QtRocket::Test::JavaTestPreferences;
using QtRocket::Test::RunnerPoller;
using QtRocket::Test::StepGate;
using QtRocket::Test::StepMeeting;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::testFcid;

using GroupId   = SimulationRunner::GroupId;
using JobId     = SimulationRunner::JobId;
using JobState  = SimulationRunner::JobState;
using Listeners = std::vector<std::shared_ptr<SimulationListener>>;
using News      = SimulationRunner::News;
using Outcome   = SimulationRun::Outcome;
using Status    = Simulation::Status;

static_assert(!std::is_copy_constructible_v<SimulationRunner>);
static_assert(!std::is_move_constructible_v<SimulationRunner>);

/// The step after which the gate of a run that is cancelled holds it (60, as in the cancel test
/// of simulation_threading_tests.cpp and the scouts' probe).
constexpr int kStepsBeforeTheGate = 60;

/// The step after which the runs of a meeting wait for each other, and other gates hold.
constexpr int kStepsBeforeTheMeeting = 30;

// ============================================================================ test support

/// Prepares @p options as the tests of OpenRocket set up a simulation (the ISA atmosphere, a
/// time step of 0.05 s) with the random seed @p seed, fixed (so a seed drawn for a batch keeps
/// it, and the test stays deterministic).
void prepareOptions(SimulationOptions& options, int seed)
{
    options.setIsaAtmosphere(true);
    options.setTimeStep(0.05);
    options.setRandomSeed(seed);
    options.setRandomSeedFixed(true);
}

/// Two simulations of the Estes Alpha III [A8-0] under the preferences of OpenRocket's tests,
/// with the random seeds 1 and 2: short flights of one branch, several hundred steps each.
struct Alphas
{
    JavaTestPreferences preferences;
    TestEstesAlphaIII   alpha;
    Simulation          first;
    Simulation          second;

    Alphas() : first(*alpha.rocket, preferences.store), second(*alpha.rocket, preferences.store)
    {
        alpha.rocket->setSelectedConfiguration(testFcid(0));
        first.setFlightConfigurationId(testFcid(0));
        second.setFlightConfigurationId(testFcid(0));
        prepareOptions(first.getOptions(), 1);
        prepareOptions(second.getOptions(), 2);
    }
};

/// A run of @p simulation, prepared on the test's thread.
[[nodiscard]] std::shared_ptr<SimulationRun> runOf(const Simulation& simulation)
{
    return std::make_shared<SimulationRun>(simulation);
}

/// The listeners of a job held at @p gate, cancelled through @p stop.
[[nodiscard]] Listeners heldAt(const StepGate& gate, const std::stop_source& stop)
{
    return {gate.listener(stop.get_token())};
}

/// The flight of @p simulation as a duplicate of it gives it when it runs alone, on this thread.
[[nodiscard]] std::shared_ptr<FlightData> loneFlight(const Simulation& simulation)
{
    const std::unique_ptr<Simulation> alone  = simulation.duplicateForIndependentSimulation();
    const QtRocket::Result<void>      result = alone->simulate();
    EXPECT_TRUE(result.has_value()) << (result.has_value() ? "" : result.error().message);
    return alone->getSimulatedData();
}

/// The differences between the lone flight @p expected and @p actual; a text when either is
/// missing.
[[nodiscard]] std::string flightProblem(const std::shared_ptr<FlightData>& expected,
                                        const std::shared_ptr<FlightData>& actual)
{
    if (expected == nullptr || actual == nullptr)
    {
        return "a flight is missing\n";
    }
    return flightDifferences(*expected, *actual);
}

/// The differences between the lone flight @p expected and the flight of @p run's duplicate.
[[nodiscard]] std::string flightProblem(const std::shared_ptr<FlightData>& expected,
                                        const SimulationRun&               run)
{
    return flightProblem(expected, run.getDuplicate().getSimulatedData());
}

/// The rows of the first branch of the flight of @p run's duplicate; 0 without a flight.
[[nodiscard]] std::size_t rowsOf(const SimulationRun& run)
{
    const std::shared_ptr<FlightData>& data = run.getDuplicate().getSimulatedData();
    return data != nullptr && data->getBranchCount() > 0 ? data->getBranch(0).getLength() : 0U;
}

/// The message of @p run's Error; "" without one.
[[nodiscard]] std::string errorMessage(const SimulationRun& run)
{
    return run.getError().value_or(QtRocket::Error{}).message;
}

/// The code of @p run's Error; ErrorCode{} without one.
[[nodiscard]] ErrorCode errorCode(const SimulationRun& run)
{
    return run.getError().value_or(QtRocket::Error{}).code;
}

/// The states of @p news, in order: "RUNNING ENDED".
[[nodiscard]] std::string statesOf(const std::vector<News>& news)
{
    std::string states;
    for (const News& item : news)
    {
        states += (states.empty() ? "" : " ") + std::string{name(item.state)};
    }
    return states;
}

/// The values of @p jobs, in order: "1 2 2".
[[nodiscard]] std::string idsText(std::initializer_list<JobId> jobs)
{
    std::string text;
    for (const JobId job : jobs)
    {
        text += std::format("{}{}", text.empty() ? "" : " ", job.value);
    }
    return text;
}

/// The jobs whose news told that they ended, in the order of that news (as idsText()).
[[nodiscard]] std::string endingsOf(const std::vector<News>& news)
{
    std::string text;
    for (const News& item : news)
    {
        if (item.state == JobState::ENDED)
        {
            text += std::format("{}{}", text.empty() ? "" : " ", item.job.value);
        }
    }
    return text;
}

/// The last news of @p job that @p poller has; a default one, and a failure of the test, when
/// there is none.
[[nodiscard]] News lastNews(const RunnerPoller& poller, JobId job)
{
    const std::vector<News> news = poller.newsOf(job);
    if (news.empty())
    {
        ADD_FAILURE() << "no news of job " << job.value;
        return {};
    }
    return news.back();
}

/// The number of news of @p job that @p poller has that tell that it ended.
[[nodiscard]] std::ptrdiff_t endingNewsCount(const RunnerPoller& poller, JobId job)
{
    return std::ranges::count(poller.newsOf(job), JobState::ENDED, &News::state);
}

/// A listener that counts the steps of a run in @p steps.
[[nodiscard]] std::shared_ptr<ProgressListener> stepCounter(
    const std::shared_ptr<std::atomic<int>>& steps)
{
    return std::make_shared<ProgressListener>(
        [steps](const SimulationStatus& /*status*/) { steps->fetch_add(1); });
}

/// The order in which runs took their first step.
struct StartLog
{
    std::mutex       mutex;
    std::vector<int> order;
};

/// A listener that writes @p number into @p log when its run takes its first step.
[[nodiscard]] std::shared_ptr<ProgressListener> startRecorder(const std::shared_ptr<StartLog>& log,
                                                              int number)
{
    const auto started = std::make_shared<bool>(false);
    return std::make_shared<ProgressListener>(
        [log, number, started](const SimulationStatus& /*status*/) {
            if (*started)
            {
                return;
            }
            *started = true;
            const std::scoped_lock lock(log->mutex);
            log->order.push_back(number);
        });
}

/// The numbers of @p log, in order: "2 3 4".
[[nodiscard]] std::string startOrder(StartLog& log)
{
    const std::scoped_lock lock(log.mutex);
    std::string            order;
    for (const int number : log.order)
    {
        order += std::format("{}{}", order.empty() ? "" : " ", number);
    }
    return order;
}

/// A clock that is 200 ms later at every call: every step of a RunSampleListener on it
/// publishes a sample (Java's UPDATE_MS has passed at every step), so its samples depend only
/// on the flight. One run's: called on the run's thread only.
[[nodiscard]] RunSampleListener::Clock everyStepClock()
{
    const auto calls = std::make_shared<int>(0);
    return [calls] {
        (*calls)++;
        return std::chrono::steady_clock::time_point{} +
               (*calls * RunSampleListener::kUpdateInterval);
    };
}

/// A RunSampleListener on everyStepClock().
[[nodiscard]] std::shared_ptr<RunSampleListener> everyStepSampler()
{
    return std::make_shared<RunSampleListener>(everyStepClock());
}

/// The samples of a lone run of @p simulation with a RunSampleListener on everyStepClock(), on
/// this thread.
[[nodiscard]] std::vector<RunSample> loneSamples(const Simulation& simulation)
{
    SimulationRun                                          run(simulation);
    const std::shared_ptr<RunSampleListener>               sampler = everyStepSampler();
    const std::vector<std::shared_ptr<SimulationListener>> listeners{sampler};
    run.run(std::stop_token{}, listeners);
    EXPECT_EQ(run.getOutcome(), Outcome::SUCCEEDED);
    return sampler->takeSamples();
}

/// A listener that fails its run at its first step: with a SimulationException (the run FAILS)
/// or with a BugError (an EXCEPTION).
class FailAtFirstStep final : public CloneableSimulationListener<FailAtFirstStep>
{
public:
    explicit FailAtFirstStep(bool withBug) : m_withBug(withBug) { }

    void postStep(SimulationStatus& /*status*/) override
    {
        if (m_withBug)
        {
            QtRocket::bug("a listener with a bug");
        }
        throw SimulationException("the listener gave up");
    }

private:
    bool m_withBug;
};

/// The listeners of a job that fails at its first step (see FailAtFirstStep).
[[nodiscard]] Listeners failingAtFirstStep(bool withBug)
{
    return {std::make_shared<FailAtFirstStep>(withBug)};
}

/// The message of the BugError in @p exception; "" for none.
[[nodiscard]] std::string bugMessage(const std::exception_ptr& exception)
{
    if (exception == nullptr)
    {
        return "";
    }
    return bugText([&exception] { std::rethrow_exception(exception); });
}

/// The message of the BugError of a runner made with no thread.
[[nodiscard]] std::string bugOfARunnerWithoutThreads()
{
    return bugText([] { const SimulationRunner none(0); });
}

/// The message of the BugError that submit() of @p run with @p extraListeners and @p stop throws;
/// "<none>" when it takes the job.
[[nodiscard]] std::string submitRefusal(SimulationRunner&                     runner,
                                        const std::shared_ptr<SimulationRun>& run,
                                        const Listeners&                      extraListeners = {},
                                        const std::stop_source&               stop           = {})
{
    return bugText([&] { runner.submit(run, {}, nullptr, extraListeners, stop); });
}

/// The message of the BugError that @p body throws on a thread of its own; "<none>" without one.
[[nodiscard]] std::string bugTextOnAnotherThread(const std::function<void()>& body)
{
    std::string text;
    {
        const std::jthread other([&text, &body] { text = bugText(body); });
    }  // joins
    return text;
}

/// What poll(), submit(), cancelAll() and isBusy() of @p runner throw on another thread (a job
/// of @p simulation for submit()).
[[nodiscard]] std::vector<std::string> callsOnAnotherThread(SimulationRunner& runner,
                                                            const Simulation& simulation)
{
    const std::shared_ptr<SimulationRun> run = runOf(simulation);
    return {bugTextOnAnotherThread([&runner] { static_cast<void>(runner.poll()); }),
            bugTextOnAnotherThread([&runner, &run] { runner.submit(run); }),
            bugTextOnAnotherThread([&runner] { runner.cancelAll(); }),
            bugTextOnAnotherThread([&runner] { static_cast<void>(runner.isBusy()); })};
}

// ============================================================================ the pool

TEST(SimulationRunner, ByDefaultItHasAThreadForEachProcessor)
{
    const SimulationRunner runner;
    const SimulationRunner three(3);

    EXPECT_EQ(SimulationRunner::defaultThreadCount(),
              std::max<std::size_t>(1, std::thread::hardware_concurrency()));
    EXPECT_EQ(runner.getThreadCount(), SimulationRunner::defaultThreadCount());
    EXPECT_FALSE(runner.isBusy());
    EXPECT_EQ(three.getThreadCount(), 3U);
    EXPECT_EQ(bugOfARunnerWithoutThreads(), "A simulation runner needs a thread");
}

// A job runs on a thread of the runner and gives the flight its simulation gives alone; the
// news tells that it ended (its last news), and the simulation it was made from is left alone
// until the test's thread adopts the run.
TEST(SimulationRunner, AJobRunsItsRunAndTheNewsTellsItEnded)
{
    Alphas                               a;
    const std::shared_ptr<FlightData>    alone = loneFlight(a.first);
    const std::shared_ptr<SimulationRun> run   = runOf(a.first);
    SimulationRunner                     runner(2);
    RunnerPoller                         poller(runner);

    const JobId job = runner.submit(run);
    EXPECT_TRUE(runner.isBusy());
    ASSERT_TRUE(poller.pollUntilEnded(job));

    EXPECT_FALSE(runner.isBusy());
    EXPECT_EQ(runner.getState(job), std::nullopt) << "forgotten once reported";
    const News last = lastNews(poller, job);
    EXPECT_EQ(last.state, JobState::ENDED);
    EXPECT_EQ(last.run, run);
    EXPECT_EQ(last.group, GroupId{});
    EXPECT_EQ(last.exception, nullptr);
    EXPECT_TRUE(poller.samplesOf(job).empty()) << "a job without a sample listener";
    EXPECT_EQ(endingNewsCount(poller, job), 1);
    EXPECT_EQ(run->getOutcome(), Outcome::SUCCEEDED);
    EXPECT_EQ(flightProblem(alone, *run), "");

    EXPECT_EQ(a.first.getStoredStatus(), Status::NOT_SIMULATED);
    EXPECT_EQ(a.first.getSimulatedData(), nullptr);
    ASSERT_TRUE(run->adoptInto(a.first));
    EXPECT_EQ(a.first.getStatus(), Status::UPTODATE);
    EXPECT_EQ(a.first.getSimulatedData(), run->getDuplicate().getSimulatedData());

    // Nothing more comes.
    const std::size_t newsBefore = poller.news().size();
    poller.pollOnce();
    EXPECT_EQ(poller.news().size(), newsBefore);
}

// The exit criterion "simulations run concurrently": two jobs meet after step 30, each waiting
// there until the other has arrived, so neither can end before both run at the same time. (A
// runner that ran them one after the other would hold the first for good: the safety net then
// cancels it, and the test fails.) Both then end as they end alone.
TEST(SimulationRunner, TwoRunsProceedAtTheSameTime)
{
    const Alphas                         a;
    const std::shared_ptr<FlightData>    firstAlone  = loneFlight(a.first);
    const std::shared_ptr<FlightData>    secondAlone = loneFlight(a.second);
    const std::shared_ptr<SimulationRun> first       = runOf(a.first);
    const std::shared_ptr<SimulationRun> second      = runOf(a.second);
    const StepMeeting                    meeting(2, kStepsBeforeTheMeeting);
    const std::stop_source               firstStop;
    const std::stop_source               secondStop;
    SimulationRunner                     runner(2);
    RunnerPoller                         poller(runner);

    runner.submit(first, {}, nullptr, {meeting.listener(firstStop.get_token())}, firstStop);
    runner.submit(second, {}, nullptr, {meeting.listener(secondStop.get_token())}, secondStop);
    EXPECT_TRUE(poller.pollUntilIdle());

    EXPECT_TRUE(meeting.allMet());
    EXPECT_EQ(meeting.arrived(), 2);
    EXPECT_EQ(first->getOutcome(), Outcome::SUCCEEDED);
    EXPECT_EQ(second->getOutcome(), Outcome::SUCCEEDED);
    EXPECT_EQ(flightProblem(firstAlone, *first), "");
    EXPECT_EQ(flightProblem(secondAlone, *second), "");
}

/// The differences of every flight of @p runs from its lone flight: @p alone[i % 2] for
/// @p runs[i]; "" when there are none.
[[nodiscard]] std::string flightProblems(const std::vector<std::shared_ptr<SimulationRun>>& runs,
                                         const std::vector<std::shared_ptr<FlightData>>&    alone)
{
    std::string problems;
    for (std::size_t i = 0; i < runs.size(); i++)
    {
        const std::string problem = runs[i]->getOutcome() == Outcome::SUCCEEDED
                                        ? flightProblem(alone[i % 2], *runs[i])
                                        : std::string{name(runs[i]->getOutcome())};
        if (!problem.empty())
        {
            problems += std::format("run {}: {}\n", i, problem);
        }
    }
    return problems;
}

/// @p count runs, of @p a's first and second simulation in turn, submitted to @p runner.
[[nodiscard]] std::vector<std::shared_ptr<SimulationRun>> submitInTurn(SimulationRunner& runner,
                                                                       const Alphas&     a,
                                                                       std::size_t       count)
{
    std::vector<std::shared_ptr<SimulationRun>> runs;
    for (std::size_t i = 0; i < count; i++)
    {
        runs.push_back(runOf(i % 2 == 0 ? a.first : a.second));
        runner.submit(runs.back());
    }
    return runs;
}

// Many jobs on a few threads: every one ends as its simulation ends alone, to the last bit.
TEST(SimulationRunner, ManyJobsOnAFewThreadsEndAsTheyEndAlone)
{
    constexpr std::size_t                          kJobs = 12;
    const Alphas                                   a;
    const std::vector<std::shared_ptr<FlightData>> alone{loneFlight(a.first), loneFlight(a.second)};
    SimulationRunner                               runner(4);
    RunnerPoller                                   poller(runner);

    const std::vector<std::shared_ptr<SimulationRun>> runs = submitInTurn(runner, a, kJobs);
    EXPECT_TRUE(poller.pollUntilIdle());

    EXPECT_EQ(flightProblems(runs, alone), "");
    EXPECT_EQ(a.first.getStoredStatus(), Status::NOT_SIMULATED);
    EXPECT_EQ(a.second.getStoredStatus(), Status::NOT_SIMULATED);
}

// The other side of the meeting: a runner of one thread runs one job at a time. The first job
// arrives at the meeting and waits there while the second stays in the queue; only when the
// test's thread abandons the meeting does the first go on, and the second runs after it.
TEST(SimulationRunner, NoMoreJobsRunAtOnceThanItHasThreads)
{
    const Alphas                         a;
    const std::shared_ptr<SimulationRun> first  = runOf(a.first);
    const std::shared_ptr<SimulationRun> second = runOf(a.second);
    const StepMeeting                    meeting(2, kStepsBeforeTheMeeting);
    const std::stop_source               firstStop;
    const std::stop_source               secondStop;
    SimulationRunner                     runner(1);
    RunnerPoller                         poller(runner);

    const JobId firstJob =
        runner.submit(first, {}, nullptr, {meeting.listener(firstStop.get_token())}, firstStop);
    const JobId secondJob =
        runner.submit(second, {}, nullptr, {meeting.listener(secondStop.get_token())}, secondStop);
    ASSERT_TRUE(poller.pollUntilArrived(meeting, 1));

    EXPECT_EQ(runner.getState(firstJob), JobState::RUNNING);
    EXPECT_EQ(runner.getState(secondJob), JobState::QUEUED);
    meeting.abandon();
    EXPECT_TRUE(poller.pollUntilIdle());

    EXPECT_FALSE(meeting.allMet());
    EXPECT_EQ(first->getOutcome(), Outcome::SUCCEEDED);
    EXPECT_EQ(second->getOutcome(), Outcome::SUCCEEDED);
    EXPECT_EQ(endingsOf(poller.news()), idsText({firstJob, secondJob}));
}

// The jobs start in the order they were submitted (Java's executor: a FIFO queue).
TEST(SimulationRunner, TheJobsStartInTheOrderTheyWereSubmitted)
{
    const Alphas           a;
    const StepGate         gate(kStepsBeforeTheMeeting);
    const std::stop_source stop;
    const auto             log = std::make_shared<StartLog>();
    SimulationRunner       runner(1);
    RunnerPoller           poller(runner);

    const JobId held = runner.submit(runOf(a.first), {}, nullptr, heldAt(gate, stop), stop);
    ASSERT_TRUE(poller.pollUntilReached(gate));
    const JobId second = runner.submit(runOf(a.second), {}, nullptr, {startRecorder(log, 2)});
    const JobId third  = runner.submit(runOf(a.first), {}, nullptr, {startRecorder(log, 3)});
    const JobId fourth = runner.submit(runOf(a.second), {}, nullptr, {startRecorder(log, 4)});
    const JobId fifth  = runner.submit(runOf(a.first), {}, nullptr, {startRecorder(log, 5)});
    gate.release();
    EXPECT_TRUE(poller.pollUntilIdle());

    EXPECT_EQ(startOrder(*log), "2 3 4 5");
    EXPECT_EQ(endingsOf(poller.news()), idsText({held, second, third, fourth, fifth}));
}

// ======================================================================= the cancellation

// The exit criterion "simulations can be cancelled": a job held at its gate after step 60 is
// cancelled from the test's thread and ends CANCELLED at that step, with Java's message; its
// run is not adopted (bug 40), so its simulation keeps what it had. Another job, which runs at
// the same time, is not affected: it ends as it ends alone and is adopted.
TEST(SimulationRunner, ARunningJobThatIsCancelledEndsCancelledAndIsNotAdopted)
{
    Alphas                               a;
    const std::shared_ptr<FlightData>    secondAlone = loneFlight(a.second);
    const std::shared_ptr<SimulationRun> cancelled   = runOf(a.first);
    const std::shared_ptr<SimulationRun> bystander   = runOf(a.second);
    const StepGate                       gate(kStepsBeforeTheGate);
    const std::stop_source               stop;
    SimulationRunner                     runner(2);
    RunnerPoller                         poller(runner);

    const JobId job = runner.submit(cancelled, {}, nullptr, heldAt(gate, stop), stop);
    runner.submit(bystander);
    ASSERT_TRUE(poller.pollUntilReached(gate));
    EXPECT_EQ(runner.getState(job), JobState::RUNNING);
    runner.cancel(job);
    EXPECT_TRUE(stop.stop_requested()) << "through the stop source the job was submitted with";
    EXPECT_TRUE(poller.pollUntilIdle());

    EXPECT_EQ(cancelled->getOutcome(), Outcome::CANCELLED);
    EXPECT_EQ(errorCode(*cancelled), ErrorCode::CANCELLED);
    EXPECT_EQ(errorMessage(*cancelled), "The simulation was interrupted.");
    EXPECT_EQ(gate.steps(), kStepsBeforeTheGate) << "it ended in the step the stop came in";
    EXPECT_EQ(rowsOf(*cancelled), static_cast<std::size_t>(kStepsBeforeTheGate));
    EXPECT_FALSE(cancelled->isAdoptable());
    EXPECT_FALSE(cancelled->adoptInto(a.first));
    EXPECT_EQ(a.first.getStoredStatus(), Status::NOT_SIMULATED);
    EXPECT_EQ(a.first.getSimulatedData(), nullptr);

    EXPECT_EQ(bystander->getOutcome(), Outcome::SUCCEEDED);
    EXPECT_EQ(flightProblem(secondAlone, *bystander), "");
    EXPECT_TRUE(bystander->adoptInto(a.second));
    EXPECT_EQ(a.second.getStatus(), Status::UPTODATE);
}

// A stop requested through the stop source a job was submitted with cancels it as cancel()
// does.
TEST(SimulationRunner, AJobCanBeCancelledThroughItsStopSource)
{
    const Alphas                         a;
    const std::shared_ptr<SimulationRun> run = runOf(a.first);
    const StepGate                       gate(kStepsBeforeTheGate);
    const std::stop_source               stop;
    SimulationRunner                     runner(1);
    RunnerPoller                         poller(runner);

    const JobId job = runner.submit(run, {}, nullptr, heldAt(gate, stop), stop);
    ASSERT_TRUE(poller.pollUntilReached(gate));
    stop.request_stop();
    EXPECT_TRUE(poller.pollUntilEnded(job));

    EXPECT_EQ(run->getOutcome(), Outcome::CANCELLED);
    EXPECT_EQ(rowsOf(*run), static_cast<std::size_t>(kStepsBeforeTheGate));
}

// A queued job that is cancelled is taken from the queue and ends at once, CANCELLED, without
// having run a step (Java: a cancelled future the executor never runs); the job that holds the
// thread goes on.
TEST(SimulationRunner, AQueuedJobThatIsCancelledNeverRuns)
{
    const Alphas                         a;
    const std::shared_ptr<SimulationRun> held   = runOf(a.first);
    const std::shared_ptr<SimulationRun> queued = runOf(a.second);
    const StepGate                       gate(kStepsBeforeTheMeeting);
    const std::stop_source               stop;
    const auto                           steps = std::make_shared<std::atomic<int>>(0);
    SimulationRunner                     runner(1);
    RunnerPoller                         poller(runner);

    const JobId heldJob   = runner.submit(held, {}, nullptr, heldAt(gate, stop), stop);
    const JobId queuedJob = runner.submit(queued, {}, nullptr, {stepCounter(steps)});
    ASSERT_TRUE(poller.pollUntilReached(gate));
    EXPECT_EQ(runner.getState(queuedJob), JobState::QUEUED);
    runner.cancel(queuedJob);
    EXPECT_EQ(runner.getState(queuedJob), JobState::ENDED);
    poller.pollOnce();

    EXPECT_EQ(statesOf(poller.newsOf(queuedJob)), "ENDED");
    EXPECT_EQ(queued->getOutcome(), Outcome::CANCELLED);
    EXPECT_EQ(errorMessage(*queued), "The simulation was interrupted.");
    EXPECT_EQ(steps->load(), 0);
    EXPECT_EQ(queued->getDuplicate().getSimulatedData(), nullptr);
    EXPECT_FALSE(queued->isAdoptable());
    EXPECT_EQ(runner.getState(heldJob), JobState::RUNNING);
    EXPECT_FALSE(stop.stop_requested());

    gate.release();
    EXPECT_TRUE(poller.pollUntilIdle());
    EXPECT_EQ(held->getOutcome(), Outcome::SUCCEEDED);
    EXPECT_EQ(steps->load(), 0);
}

// purge() takes the queued jobs of a group from the queue (each ends CANCELLED without having
// run), and leaves the running jobs of the group and the jobs of other groups alone.
TEST(SimulationRunner, PurgeTakesTheQueuedJobsOfItsGroupOnly)
{
    const Alphas                         a;
    const std::shared_ptr<SimulationRun> running    = runOf(a.first);
    const std::shared_ptr<SimulationRun> purged     = runOf(a.second);
    const std::shared_ptr<SimulationRun> other      = runOf(a.first);
    const std::shared_ptr<SimulationRun> alsoPurged = runOf(a.second);
    const StepGate                       gate(kStepsBeforeTheMeeting);
    const std::stop_source               stop;
    SimulationRunner                     runner(1);
    RunnerPoller                         poller(runner);
    const GroupId                        window      = runner.newGroup();
    const GroupId                        otherWindow = runner.newGroup();

    const JobId runningJob    = runner.submit(running, window, nullptr, heldAt(gate, stop), stop);
    const JobId purgedJob     = runner.submit(purged, window);
    const JobId otherJob      = runner.submit(other, otherWindow);
    const JobId alsoPurgedJob = runner.submit(alsoPurged, window);
    ASSERT_TRUE(poller.pollUntilReached(gate));
    runner.purge(window);
    poller.pollOnce();

    EXPECT_EQ(endingsOf(poller.news()), idsText({purgedJob, alsoPurgedJob}));
    EXPECT_EQ(purged->getOutcome(), Outcome::CANCELLED);
    EXPECT_EQ(alsoPurged->getOutcome(), Outcome::CANCELLED);
    EXPECT_EQ(runner.getState(runningJob), JobState::RUNNING);
    EXPECT_EQ(runner.getState(otherJob), JobState::QUEUED);
    EXPECT_FALSE(stop.stop_requested());

    gate.release();
    EXPECT_TRUE(poller.pollUntilIdle());
    EXPECT_EQ(running->getOutcome(), Outcome::SUCCEEDED);
    EXPECT_EQ(other->getOutcome(), Outcome::SUCCEEDED);
    EXPECT_EQ(lastNews(poller, otherJob).group, otherWindow);
}

// A queued job that is purged ends without a stop request through its stop source: a running
// job that shares the source goes on.
TEST(SimulationRunner, APurgeLeavesAStopSourceThatJobsShareAlone)
{
    const Alphas                         a;
    const std::shared_ptr<SimulationRun> running = runOf(a.first);
    const std::shared_ptr<SimulationRun> purged  = runOf(a.second);
    const StepGate                       gate(kStepsBeforeTheMeeting);
    const std::stop_source               shared;
    SimulationRunner                     runner(1);
    RunnerPoller                         poller(runner);
    const GroupId                        batch = runner.newGroup();

    runner.submit(running, batch, nullptr, heldAt(gate, shared), shared);
    runner.submit(purged, batch, nullptr, {}, shared);
    ASSERT_TRUE(poller.pollUntilReached(gate));
    runner.purge(batch);
    EXPECT_FALSE(shared.stop_requested());
    gate.release();
    EXPECT_TRUE(poller.pollUntilIdle());

    EXPECT_EQ(purged->getOutcome(), Outcome::CANCELLED);
    EXPECT_EQ(running->getOutcome(), Outcome::SUCCEEDED);
}

// cancelGroup() (the run dialog's Cancel, a window that closes) stops the running jobs of the
// group and purges its queued ones; a running job of another group goes on.
TEST(SimulationRunner, CancellingAGroupStopsItsRunningJobsAndPurgesItsQueuedOnes)
{
    const Alphas                         a;
    const std::shared_ptr<SimulationRun> running = runOf(a.first);
    const std::shared_ptr<SimulationRun> other   = runOf(a.second);
    const std::shared_ptr<SimulationRun> queued  = runOf(a.first);
    const StepGate                       gate(kStepsBeforeTheGate);
    const StepGate                       otherGate(kStepsBeforeTheGate);
    const std::stop_source               stop;
    const std::stop_source               otherStop;
    SimulationRunner                     runner(2);
    RunnerPoller                         poller(runner);
    const GroupId                        window      = runner.newGroup();
    const GroupId                        otherWindow = runner.newGroup();

    runner.submit(running, window, nullptr, heldAt(gate, stop), stop);
    runner.submit(other, otherWindow, nullptr, heldAt(otherGate, otherStop), otherStop);
    ASSERT_TRUE(poller.pollUntilReached(gate));
    ASSERT_TRUE(poller.pollUntilReached(otherGate));
    const JobId queuedJob = runner.submit(queued, window);
    EXPECT_EQ(runner.getState(queuedJob), JobState::QUEUED);
    runner.cancelGroup(window);
    EXPECT_FALSE(otherStop.stop_requested());
    otherGate.release();
    EXPECT_TRUE(poller.pollUntilIdle());

    EXPECT_EQ(running->getOutcome(), Outcome::CANCELLED);
    EXPECT_EQ(rowsOf(*running), static_cast<std::size_t>(kStepsBeforeTheGate));
    EXPECT_EQ(queued->getOutcome(), Outcome::CANCELLED);
    EXPECT_EQ(queued->getDuplicate().getSimulatedData(), nullptr);
    EXPECT_EQ(other->getOutcome(), Outcome::SUCCEEDED);
}

// cancelAll() cancels every job, running or queued, whatever its group.
TEST(SimulationRunner, CancellingAllCancelsEveryJob)
{
    const Alphas                         a;
    const std::shared_ptr<SimulationRun> running = runOf(a.first);
    const std::shared_ptr<SimulationRun> queued  = runOf(a.second);
    const StepGate                       gate(kStepsBeforeTheGate);
    const std::stop_source               stop;
    SimulationRunner                     runner(1);
    RunnerPoller                         poller(runner);

    runner.submit(running, runner.newGroup(), nullptr, heldAt(gate, stop), stop);
    runner.submit(queued, runner.newGroup());
    ASSERT_TRUE(poller.pollUntilReached(gate));
    runner.cancelAll();
    EXPECT_TRUE(poller.pollUntilIdle());

    EXPECT_EQ(running->getOutcome(), Outcome::CANCELLED);
    EXPECT_EQ(rowsOf(*running), static_cast<std::size_t>(kStepsBeforeTheGate));
    EXPECT_EQ(queued->getOutcome(), Outcome::CANCELLED);
    EXPECT_EQ(queued->getDuplicate().getSimulatedData(), nullptr);
}

// A cancel of a job the runner does not know, or no longer knows, does nothing.
TEST(SimulationRunner, ACancelOfAJobThatEndedDoesNothing)
{
    const Alphas                         a;
    const std::shared_ptr<SimulationRun> run = runOf(a.first);
    const std::stop_source               stop;
    SimulationRunner                     runner(1);
    RunnerPoller                         poller(runner);

    runner.cancel(JobId{.value = 42});
    runner.purge(GroupId{.value = 42});
    runner.cancelGroup(GroupId{.value = 42});
    const JobId job = runner.submit(run, {}, nullptr, {}, stop);
    ASSERT_TRUE(poller.pollUntilEnded(job));
    runner.cancel(job);
    runner.cancelAll();

    EXPECT_FALSE(stop.stop_requested());
    EXPECT_EQ(run->getOutcome(), Outcome::SUCCEEDED);
    EXPECT_EQ(runner.getState(job), std::nullopt);
    EXPECT_FALSE(runner.isBusy());
}

/// Submits a job of @p running held at @p gate and a job of @p queued counted by @p steps to a
/// runner of one thread, and destroys the runner while the first is held.
void destroyWhileHeld(const std::shared_ptr<SimulationRun>& running,
                      const std::shared_ptr<SimulationRun>& queued, const StepGate& gate,
                      const std::stop_source& stop, const std::shared_ptr<std::atomic<int>>& steps)
{
    SimulationRunner runner(1);
    RunnerPoller     poller(runner);
    runner.submit(running, {}, nullptr, heldAt(gate, stop), stop);
    runner.submit(queued, {}, nullptr, {stepCounter(steps)});
    EXPECT_TRUE(poller.pollUntilReached(gate));
}  // destroyed while the job is held

// The destructor cancels the running jobs and joins the threads: the job held at its gate gets
// its stop and ends CANCELLED in that step; the queued job never runs.
TEST(SimulationRunner, TheDestructorCancelsTheRunningJobsAndJoinsItsThreads)
{
    const Alphas                         a;
    const std::shared_ptr<SimulationRun> running = runOf(a.first);
    const std::shared_ptr<SimulationRun> queued  = runOf(a.second);
    const StepGate                       gate(kStepsBeforeTheGate);
    const std::stop_source               stop;
    const auto                           steps = std::make_shared<std::atomic<int>>(0);

    destroyWhileHeld(running, queued, gate, stop, steps);

    EXPECT_TRUE(stop.stop_requested());
    EXPECT_EQ(running->getOutcome(), Outcome::CANCELLED);
    EXPECT_EQ(gate.steps(), kStepsBeforeTheGate);
    EXPECT_EQ(rowsOf(*running), static_cast<std::size_t>(kStepsBeforeTheGate));
    EXPECT_FALSE(running->isAdoptable());
    EXPECT_EQ(queued->getOutcome(), Outcome::NOT_RUN);
    EXPECT_EQ(steps->load(), 0);
    EXPECT_FALSE(queued->isAdoptable());
    EXPECT_EQ(a.first.getStoredStatus(), Status::NOT_SIMULATED);
}

// ========================================================================== the endings

// A run that fails ends FAILED with its Error, which the run carries; an exception of a run (a
// BugError of a listener) is carried to the owning thread in the news, and the thread goes on
// with the next job.
TEST(SimulationRunner, FailuresAndExceptionsAreCarriedAndTheThreadGoesOn)
{
    const Alphas                         a;
    const std::shared_ptr<SimulationRun> failing  = runOf(a.first);
    const std::shared_ptr<SimulationRun> throwing = runOf(a.second);
    const std::shared_ptr<SimulationRun> after    = runOf(a.first);
    SimulationRunner                     runner(1);
    RunnerPoller                         poller(runner);

    const JobId failingJob  = runner.submit(failing, {}, nullptr, failingAtFirstStep(false));
    const JobId throwingJob = runner.submit(throwing, {}, nullptr, failingAtFirstStep(true));
    const JobId afterJob    = runner.submit(after);
    EXPECT_TRUE(poller.pollUntilIdle());

    EXPECT_EQ(failing->getOutcome(), Outcome::FAILED);
    EXPECT_EQ(errorMessage(*failing), "the listener gave up");
    EXPECT_EQ(lastNews(poller, failingJob).exception, nullptr);

    EXPECT_EQ(throwing->getOutcome(), Outcome::EXCEPTION);
    const std::exception_ptr exception = lastNews(poller, throwingJob).exception;
    EXPECT_EQ(exception, throwing->getException());
    EXPECT_EQ(bugMessage(exception), "a listener with a bug");

    EXPECT_EQ(after->getOutcome(), Outcome::SUCCEEDED);
    EXPECT_TRUE(poller.hasEnded(afterJob));
}

// ============================================================================ the samples

// The news carries every sample of the job's RunSampleListener, once and in order: together
// they are the samples of the same simulation run alone. The last news of a job is the one that
// tells it ended.
TEST(SimulationRunner, TheNewsCarriesEverySampleInOrder)
{
    const Alphas                             a;
    const std::vector<RunSample>             alone   = loneSamples(a.first);
    const std::shared_ptr<SimulationRun>     run     = runOf(a.first);
    const std::shared_ptr<RunSampleListener> sampler = everyStepSampler();
    SimulationRunner                         runner(2);
    RunnerPoller                             poller(runner);

    const JobId job = runner.submit(run, {}, sampler);
    ASSERT_TRUE(poller.pollUntilEnded(job));

    EXPECT_GT(alone.size(), 100U);
    EXPECT_EQ(poller.samplesOf(job), alone);
    EXPECT_TRUE(sampler->takeSamples().empty()) << "the runner took them all";
    EXPECT_EQ(lastNews(poller, job).state, JobState::ENDED);
    EXPECT_EQ(endingNewsCount(poller, job), 1);
}

// The sample listener comes before the extra listeners (Java: the progress listener is the
// first of getExtraListeners()): when an extra listener fails the run at its first step, the
// sample listener has already sampled that step.
TEST(SimulationRunner, TheSampleListenerComesBeforeTheExtraListeners)
{
    const Alphas                             a;
    const std::shared_ptr<SimulationRun>     run     = runOf(a.first);
    const std::shared_ptr<RunSampleListener> sampler = everyStepSampler();
    SimulationRunner                         runner(1);
    RunnerPoller                             poller(runner);

    const JobId job = runner.submit(run, {}, sampler, failingAtFirstStep(false));
    ASSERT_TRUE(poller.pollUntilEnded(job));

    EXPECT_EQ(run->getOutcome(), Outcome::FAILED);
    const std::vector<RunSample> samples = poller.samplesOf(job);
    ASSERT_FALSE(samples.empty());
    EXPECT_GT(samples.back().simulationTime, 0.0) << "the sample of the first step";
}

// ================================================================== adoption and the batch

/// A simulation of a batch run as the GUI's run dialog will run it (run 2's SimulationBatch,
/// after SimulationRunDialog.java), with what the dialog keeps of it.
struct BatchEntry
{
    std::shared_ptr<Simulation>                  simulation;
    std::shared_ptr<SimulationRun>               run;
    std::shared_ptr<RunSampleListener>           sampler;
    std::unique_ptr<SimulationProgressEstimator> estimator;
    JobId                                        job;
};

/// What the test's thread saw of a batch.
struct BatchRun
{
    /// The progress of each simulation when the batch was complete.
    std::vector<int> progress;
    /// The calls of markSimulationComplete() that completed the batch.
    int completions{0};
    /// Whether every adoption was made on the test's thread.
    bool adoptedHere{true};
};

/// Prepares and submits the runs of @p simulations as one batch: per simulation a seed (if not
/// fixed), a run, a progress estimate of the duplicate's configuration and a sample listener.
[[nodiscard]] std::vector<BatchEntry> submitBatch(
    SimulationRunner& runner, const std::vector<std::shared_ptr<Simulation>>& simulations)
{
    std::vector<BatchEntry> entries;
    const GroupId           group = runner.newGroup();
    for (const std::shared_ptr<Simulation>& simulation : simulations)
    {
        simulation->getOptions().randomizeSeedIfNotFixed();
        BatchEntry entry{.simulation = simulation,
                         .run        = runOf(*simulation),
                         .sampler    = std::make_shared<RunSampleListener>(),
                         .estimator  = nullptr,
                         .job        = {}};
        entry.estimator = std::make_unique<SimulationProgressEstimator>(
            entry.run->getDuplicate().getActiveConfiguration());
        entry.job = runner.submit(entry.run, group, entry.sampler);
        entries.push_back(std::move(entry));
    }
    return entries;
}

/// Takes in @p news of @p entry: the samples, and at the end the estimate's finish, the adoption
/// (while the simulation is in @p document) and the mark in @p completion.
void takeBatchNews(const News& news, BatchEntry& entry, std::size_t index,
                   OpenRocketDocument& document, SimulationBatchCompletion& completion,
                   BatchRun& batch, std::thread::id owner)
{
    entry.estimator->process(news.samples);
    if (news.state != JobState::ENDED)
    {
        return;
    }
    entry.estimator->finish();
    if (document.getSimulationIndex(*entry.simulation).has_value())
    {
        batch.adoptedHere = batch.adoptedHere && std::this_thread::get_id() == owner;
        EXPECT_TRUE(news.run->adoptInto(*entry.simulation));
    }
    batch.completions += completion.markSimulationComplete(index) ? 1 : 0;
}

/// Runs @p simulations of @p document as a batch on @p runner, polling it on this thread.
[[nodiscard]] BatchRun runBatch(SimulationRunner& runner, OpenRocketDocument& document,
                                const std::vector<std::shared_ptr<Simulation>>& simulations)
{
    std::vector<BatchEntry>   entries = submitBatch(runner, simulations);
    BatchRun                  batch;
    SimulationBatchCompletion completion(entries.size());
    const std::thread::id     owner = std::this_thread::get_id();
    const auto giveUp = std::chrono::steady_clock::now() + QtRocket::Test::kRunnerSafetyNet;
    while (!completion.isBatchComplete() && std::chrono::steady_clock::now() < giveUp)
    {
        for (const News& news : runner.poll())
        {
            const auto entry = std::ranges::find(entries, news.job, &BatchEntry::job);
            if (entry == entries.end())
            {
                ADD_FAILURE() << "news of a job that is not the batch's";
                continue;
            }
            const auto index = static_cast<std::size_t>(std::distance(entries.begin(), entry));
            takeBatchNews(news, *entry, index, document, completion, batch, owner);
        }
        std::this_thread::yield();
    }
    EXPECT_TRUE(completion.isBatchComplete());
    for (const BatchEntry& entry : entries)
    {
        batch.progress.push_back(entry.estimator->getProgress());
    }
    return batch;
}

/// A document of the Estes Alpha III with three simulations of a fixed seed (two of the A8, one
/// of the B4), saved.
struct BatchDocument
{
    AlphaDocument                            alpha;
    std::vector<std::shared_ptr<Simulation>> simulations;

    BatchDocument()
      : simulations{alpha.newSimulation("First", testFcid(0)),
                    alpha.newSimulation("Second", testFcid(0)),
                    alpha.newSimulation("Third", testFcid(1))}
    {
        for (const std::shared_ptr<Simulation>& simulation : simulations)
        {
            prepareOptions(simulation->getOptions(), 7);
            alpha.document().addSimulation(simulation);
        }
        alpha.document().setSaved(true);
    }
};

/// The threads on which the document's events came.
class EventThreads
{
public:
    explicit EventThreads(OpenRocketDocument& document)
      : m_connection(document.documentChanged().connect([this](const DocumentChangeEvent&) {
            m_threads.push_back(std::this_thread::get_id());
        }))
    {
    }

    EventThreads(const EventThreads&)            = delete;
    EventThreads& operator=(const EventThreads&) = delete;
    EventThreads(EventThreads&&)                 = delete;
    EventThreads& operator=(EventThreads&&)      = delete;
    ~EventThreads()                              = default;

    /// Whether there were events, every one on this thread.
    [[nodiscard]] bool allHere() const
    {
        return !m_threads.empty() && std::ranges::all_of(m_threads, [](std::thread::id thread) {
            return thread == std::this_thread::get_id();
        });
    }

private:
    /// Declared before the connection, which is so disconnected before the list goes.
    mutable std::vector<std::thread::id>                           m_threads;
    QtRocket::Signal<const DocumentChangeEvent&>::ScopedConnection m_connection;
};

// The results are adopted on the owning thread, as the run dialog will adopt them: each
// simulation of the document becomes up to date with the flight it gives alone, the document
// hears it there (and is no longer saved), the progress estimates end at 100, and the batch is
// complete once, after the last job.
TEST(SimulationRunner, TheResultsAreAdoptedOnTheOwningThread)
{
    const BatchDocument                             batchDocument;
    const std::vector<std::shared_ptr<Simulation>>& simulations = batchDocument.simulations;
    OpenRocketDocument&                             document    = batchDocument.alpha.document();
    const std::shared_ptr<FlightData>               firstAlone  = loneFlight(*simulations[0]);
    const std::shared_ptr<FlightData>               thirdAlone  = loneFlight(*simulations[2]);
    const EventThreads                              events(document);
    SimulationRunner                                runner(2);

    const BatchRun batch = runBatch(runner, document, simulations);

    EXPECT_TRUE(batch.adoptedHere);
    EXPECT_TRUE(events.allHere());
    EXPECT_EQ(batch.completions, 1);
    EXPECT_EQ(batch.progress, (std::vector<int>{100, 100, 100}));
    EXPECT_FALSE(document.isSaved());
    EXPECT_EQ(simulations[0]->getStatus(), Status::UPTODATE);
    EXPECT_EQ(simulations[1]->getStatus(), Status::UPTODATE);
    EXPECT_EQ(simulations[2]->getStatus(), Status::UPTODATE);
    EXPECT_EQ(flightProblem(firstAlone, simulations[0]->getSimulatedData()), "");
    EXPECT_EQ(flightProblem(thirdAlone, simulations[2]->getSimulatedData()), "");
    EXPECT_FALSE(runner.isBusy());
}

// An edit of the design while a job runs leaves the adopted simulation OUTDATED: the run
// records the modification id its configuration had when the run was prepared.
TEST(SimulationRunner, AnEditWhileTheJobRunsLeavesTheAdoptedSimulationOutdated)
{
    Alphas                               a;
    const std::shared_ptr<SimulationRun> run = runOf(a.first);
    const StepGate                       gate(kStepsBeforeTheMeeting);
    const std::stop_source               stop;
    SimulationRunner                     runner(1);
    RunnerPoller                         poller(runner);

    const JobId job = runner.submit(run, {}, nullptr, heldAt(gate, stop), stop);
    ASSERT_TRUE(poller.pollUntilReached(gate));
    a.alpha.body->setLength(a.alpha.body->getLength() + 0.01);
    gate.release();
    ASSERT_TRUE(poller.pollUntilEnded(job));

    EXPECT_EQ(run->getOutcome(), Outcome::SUCCEEDED);
    ASSERT_TRUE(run->adoptInto(a.first));
    EXPECT_EQ(a.first.getStatus(), Status::OUTDATED);
}

// ============================================================================== the rules

// submit() refuses what cannot be a job, on the owning thread, and queues nothing then.
TEST(SimulationRunner, SubmitRefusesWhatCannotBeAJob)
{
    const Alphas                         a;
    const std::shared_ptr<SimulationRun> ran = runOf(a.first);
    ran->run(std::stop_token{});
    const std::shared_ptr<SimulationRun> held = runOf(a.first);
    const StepGate                       gate(kStepsBeforeTheMeeting);
    const std::stop_source               stop;
    SimulationRunner                     runner(1);
    RunnerPoller                         poller(runner);
    const JobId job = runner.submit(held, {}, nullptr, heldAt(gate, stop), stop);

    EXPECT_EQ(submitRefusal(runner, nullptr), "A job of a simulation runner needs a run");
    EXPECT_EQ(submitRefusal(runner, ran), "A simulation run that has run cannot be submitted");
    EXPECT_EQ(submitRefusal(runner, held), "A simulation run is the run of one job");
    EXPECT_EQ(submitRefusal(runner, runOf(a.second), {nullptr}),
              "An extra listener of a job is null");
    EXPECT_EQ(submitRefusal(runner, runOf(a.second), {}, std::stop_source(std::nostopstate)),
              "A job needs a stop source that can stop it");

    gate.release();
    EXPECT_TRUE(poller.pollUntilIdle());
    EXPECT_EQ(endingsOf(poller.news()), idsText({job})) << "nothing else was queued";
    EXPECT_EQ(held->getOutcome(), Outcome::SUCCEEDED);
}

// The runner belongs to the thread that made it.
TEST(SimulationRunner, ItIsUsedOnTheThreadThatMadeIt)
{
    const Alphas      a;
    SimulationRunner  runner(1);
    const std::string expected = "A simulation runner is used on the thread that made it";

    EXPECT_EQ(callsOnAnotherThread(runner, a.first),
              (std::vector<std::string>{expected, expected, expected, expected}));
    EXPECT_FALSE(runner.isBusy());
}

TEST(SimulationRunner, EveryGroupIsANewOne)
{
    SimulationRunner runner(1);
    const GroupId    first  = runner.newGroup();
    const GroupId    second = runner.newGroup();

    EXPECT_NE(first, GroupId{});
    EXPECT_NE(second, GroupId{});
    EXPECT_NE(first, second);
}

TEST(SimulationRunnerJobState, TheNamesAreTheConstants)
{
    EXPECT_EQ(name(JobState::QUEUED), "QUEUED");
    EXPECT_EQ(name(JobState::RUNNING), "RUNNING");
    EXPECT_EQ(name(JobState::ENDED), "ENDED");
}

// ========================================================================== a measurement

/// The median of @p values, which it sorts.
[[nodiscard]] double medianOf(std::vector<double>& values)
{
    std::ranges::sort(values);
    return values.empty() ? 0.0 : values[values.size() / 2];
}

/// The milliseconds of @p count runs of @p simulation, each of a new duplicate: with simulate()
/// on this thread when @p runner is null, else as a SimulationRun on @p runner, from the
/// preparation to the news that it ended (the poll spinning).
[[nodiscard]] std::vector<double> runMilliseconds(const Simulation& simulation,
                                                  SimulationRunner* runner, int count)
{
    std::vector<double> times;
    for (int i = 0; i < count; i++)
    {
        const auto start = std::chrono::steady_clock::now();
        if (runner == nullptr)
        {
            const std::unique_ptr<Simulation> duplicate =
                simulation.duplicateForIndependentSimulation();
            const QtRocket::Result<void> result = duplicate->simulate();
            EXPECT_TRUE(result.has_value());
        }
        else
        {
            RunnerPoller poller(*runner);
            static_cast<void>(poller.pollUntilEnded(runner->submit(runOf(simulation))));
        }
        const std::chrono::duration<double, std::milli> taken =
            std::chrono::steady_clock::now() - start;
        times.push_back(taken.count());
    }
    return times;
}

// A measurement, not a test: what running the examples' fastest flight ("Clustered motors",
// simulation 1) as a job of the runner costs over a plain simulate() of a duplicate on the
// calling thread (the duplicate, the hand-over to a thread, the mailbox and the polling). Run
// it by name in a release build.
TEST(SimulationRunner, DISABLED_PrintsTheOverheadOfARunOnTheRunner)
{
    constexpr int                        kRuns = 200;
    const QtRocket::Test::GoldenExample* example =
        QtRocket::Test::findGoldenExample("Clustered motors.ork");
    ASSERT_NE(example, nullptr);
    const QtRocket::Result<LoadedDocument> loaded = QtRocket::Test::loadGoldenExample(*example);
    ASSERT_TRUE(loaded.has_value());
    const std::shared_ptr<Simulation> simulation = loaded->document->getSimulation(0);
    SimulationRunner                  runner(2);

    // The first round warms up; the plain runs are measured before and after the runner's.
    std::vector<double> plain      = runMilliseconds(*simulation, nullptr, kRuns);
    plain                          = runMilliseconds(*simulation, nullptr, kRuns);
    std::vector<double> onRunner   = runMilliseconds(*simulation, &runner, kRuns);
    std::vector<double> plainAfter = runMilliseconds(*simulation, nullptr, kRuns);

    RecordProperty("median_ms_simulate", std::format("{:.3f}", medianOf(plain)));
    RecordProperty("median_ms_runner", std::format("{:.3f}", medianOf(onRunner)));
    RecordProperty("median_ms_simulate_after", std::format("{:.3f}", medianOf(plainAfter)));
    SUCCEED();
}

}  // namespace
