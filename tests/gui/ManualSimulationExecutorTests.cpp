#include "ManualSimulationExecutor.h"

#include <exception>
#include <memory>
#include <span>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

#include <QCoreApplication>
#include <QObject>

#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/util/SimulationExecutor.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/simulation/RunSample.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationRun.h"
#include "QtRocket/simulation/listeners/RunSampleListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/BugError.h"
#include "RecordingGuardHook.h"
#include "SimulationRunTestSupport.h"

// The tests' executor of the simulations: it runs a job's run on the calling thread, in the next
// turn of the event loop, when the test says, or never, and tells the callbacks as
// SimulationExecutor's contract says.

namespace QtRocket::Gui::Test
{

namespace
{

using ManualSimulationExecutorTest = GuiTest;
using Mode                         = ManualSimulationExecutor::Mode;
using JobId                        = SimulationExecutor::JobId;
using Outcome                      = SimulationRun::Outcome;

/// What the jobs did, in order.
using Log = std::vector<std::string>;

/// A simulation of a new design, without a motor, whose runs end at once (an aborted flight:
/// SUCCEEDED); the source of runs for the tests that look at the callbacks only.
class MotorlessSimulation
{
public:
    MotorlessSimulation()
      : m_document(OpenRocketDocumentFactory::createNewRocket()),
        m_simulation(std::make_shared<Simulation>(m_document->getRocket(), m_preferences))
    {
    }

    /// A new run of the simulation.
    [[nodiscard]] std::shared_ptr<SimulationRun> newRun() const
    {
        return std::make_shared<SimulationRun>(*m_simulation);
    }

private:
    InMemoryPreferences                 m_preferences;
    std::unique_ptr<OpenRocketDocument> m_document;
    std::shared_ptr<Simulation>         m_simulation;
};

/// A job's progressed() that logs "progressed <name>".
struct LoggingProgress
{
    Log*        log;
    std::string name;

    void operator()(std::span<const RunSample> /*samples*/) const
    {
        log->push_back("progressed " + name);
    }
};

/// A job's finished() that logs "finished <name> <the run's outcome>", with " exception" when
/// the job ended with one.
struct LoggingFinish
{
    Log*                                 log;
    std::string                          name;
    std::shared_ptr<const SimulationRun> run;

    void operator()(const std::exception_ptr& exception) const
    {
        log->push_back("finished " + name + " " + std::string(QtRocket::name(run->getOutcome())) +
                       (exception != nullptr ? " exception" : ""));
    }
};

/// Submits a run of @p simulation as the job @p name of @p window to @p executor, logged into
/// @p log, without a sample listener, with @p extraListeners.
JobId submitLogged(ManualSimulationExecutor& executor, const MotorlessSimulation& simulation,
                   Log& log, const std::string& name, const QObject* window = nullptr,
                   std::vector<std::shared_ptr<SimulationListener>> extraListeners = {})
{
    const std::shared_ptr<SimulationRun> run = simulation.newRun();
    return executor.submit(window, run, nullptr, std::move(extraListeners), std::stop_source{},
                           {.progressed = LoggingProgress{.log = &log, .name = name},
                            .finished   = LoggingFinish{.log = &log, .name = name, .run = run}});
}

/// Submits a run of @p simulation to @p executor with @p sampler and @p extraListeners, without
/// callbacks.
JobId submitWithListeners(ManualSimulationExecutor& executor, const MotorlessSimulation& simulation,
                          std::shared_ptr<RunSampleListener>               sampler,
                          std::vector<std::shared_ptr<SimulationListener>> extraListeners)
{
    return executor.submit(nullptr, simulation.newRun(), std::move(sampler),
                           std::move(extraListeners), std::stop_source{}, {});
}

/// What a listener of a run does as it runs: it submits another job with @p listener (one of the
/// running job's) and notes whether the executor refused it.
struct SubmitWithTheRunsListener
{
    ManualSimulationExecutor*           executor;
    const MotorlessSimulation*          simulation;
    std::shared_ptr<SimulationListener> listener;
    bool*                               refused;

    void operator()() const
    {
        try
        {
            static_cast<void>(submitWithListeners(*executor, *simulation, nullptr, {listener}));
        }
        catch (const BugError&)
        {
            *refused = true;
        }
    }
};

/// A job's finished() that notes the job its caller had as its current one by then (as the
/// design view's background estimate adopts only its current job's run).
struct NoteTheCurrentJob
{
    const JobId* current;
    JobId*       seen;

    void operator()(const std::exception_ptr& /*exception*/) const { *seen = *current; }
};

/// A job's finished() that submits the job "second" (as the background estimate goes on to the
/// next simulation), logged.
struct SubmitTheNext
{
    ManualSimulationExecutor*  executor;
    const MotorlessSimulation* simulation;
    Log*                       log;

    void operator()(const std::exception_ptr& /*exception*/) const
    {
        log->emplace_back("finished first");
        static_cast<void>(submitLogged(*executor, *simulation, *log, "second"));
    }
};

/// A job's finished() that submits the job "second" and then runs the event loop, as a modal
/// message would, before it logs that it returns.
struct SubmitAndRunTheEventLoop
{
    ManualSimulationExecutor*  executor;
    const MotorlessSimulation* simulation;
    Log*                       log;

    void operator()(const std::exception_ptr& /*exception*/) const
    {
        static_cast<void>(submitLogged(*executor, *simulation, *log, "second"));
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        log->emplace_back("first returns");
    }
};

/// A job's progressed() that logs "progressed" and abandons the job that runs (its own).
struct SelfAbandoningProgress
{
    ManualSimulationExecutor* executor;
    Log*                      log;

    void operator()(std::span<const RunSample> /*samples*/) const
    {
        log->emplace_back("progressed");
        executor->abandon(executor->runningJob());
    }
};

/// What a listener of a run notes as it runs: the job that runs.
struct NoteTheRunningJob
{
    const ManualSimulationExecutor* executor;
    JobId*                          running;

    void operator()() const { *running = executor->runningJob(); }
};

/// What a listener of a run does as it runs: it cancels the job that runs (its own).
struct CancelTheRunningJob
{
    ManualSimulationExecutor* executor;

    void operator()() const { executor->cancel(executor->runningJob()); }
};

/// What a listener of a run does as it runs: it abandons the jobs of @p window (as the window
/// goes while the run goes on).
struct AbandonTheWindow
{
    ManualSimulationExecutor* executor;
    const QObject*            window;

    void operator()() const { executor->abandonAll(window); }
};

/// What a listener of a run does as it runs: it reports the progress of the job that runs (as a
/// test does), and notes whether that job was pending.
struct ReportTheProgress
{
    ManualSimulationExecutor* executor;
    bool*                     reported;

    void operator()() const { *reported = executor->reportProgress(executor->runningJob()); }
};

/// A job's finished() that throws.
struct ThrowingFinish
{
    void operator()(const std::exception_ptr& /*exception*/) const
    {
        throw std::runtime_error("a callback with a defect");
    }
};

/// The samples a job's progressed() was given, call by call.
struct CollectSamples
{
    std::vector<std::vector<RunSample>>* calls;

    void operator()(std::span<const RunSample> samples) const
    {
        calls->emplace_back(samples.begin(), samples.end());
    }
};

/// A job's finished() that notes the exception it was given.
struct NoteException
{
    std::exception_ptr* exception;
    bool*               called;

    void operator()(const std::exception_ptr& ended) const
    {
        *exception = ended;
        *called    = true;
    }
};

TEST_F(ManualSimulationExecutorTest, TheImmediateModeRunsAJobInTheNextTurnOfTheEventLoop)
{
    ManualSimulationExecutor executor;
    EXPECT_EQ(executor.mode(), Mode::IMMEDIATE);
    const MotorlessSimulation simulation;
    const QObject             window;
    Log                       log;
    const JobId               job = submitLogged(executor, simulation, log, "a", &window);
    // Nothing inside submit(): the job waits for the turn.
    EXPECT_TRUE(log.empty());
    EXPECT_NE(job, SimulationExecutor::kNoJob);
    EXPECT_TRUE(executor.isPending(job));
    EXPECT_EQ(executor.waitingJobs(), std::vector<JobId>{job});
    QCoreApplication::processEvents();
    // The run, then finished() (no sample listener: no progressed()).
    EXPECT_EQ(log, (Log{"finished a SUCCEEDED"}));
    EXPECT_FALSE(executor.isPending(job));
    EXPECT_EQ(executor.runningJob(), SimulationExecutor::kNoJob);
    ASSERT_EQ(executor.submittedJobs().size(), 1U);
    EXPECT_EQ(executor.submittedJobs().front().id, job);
    EXPECT_EQ(executor.submittedJobs().front().window, &window);
    EXPECT_TRUE(executor.cancelledJobs().empty());
    EXPECT_TRUE(executor.abandonedJobs().empty());
}

TEST_F(ManualSimulationExecutorTest, AJobRunsWithItsListenersAndHandsOverItsSamples)
{
    // As a thread of the core's runner: the run with the sample listener, then the extra
    // listeners; then the samples, then the end.
    ManualSimulationExecutor             executor;
    const ExampleDesign                  design(ExampleDesign::kSimpleModelRocket);
    const std::shared_ptr<SimulationRun> run =
        std::make_shared<SimulationRun>(*design.simulation(0));
    const auto sampler   = std::make_shared<RunSampleListener>(frozenSampleClock());
    JobId      runningAt = SimulationExecutor::kNoJob;
    std::vector<std::vector<RunSample>> calls;
    std::exception_ptr                  exception;
    bool                                finished = false;
    const JobId                         job      = executor.submit(
        nullptr, run, sampler,
        {actionAtStep(1, NoteTheRunningJob{.executor = &executor, .running = &runningAt})},
        std::stop_source{},
        {.progressed = CollectSamples{.calls = &calls},
         .finished   = NoteException{.exception = &exception, .called = &finished}});
    QCoreApplication::processEvents();
    // The extra listener ran, in the job's run.
    EXPECT_EQ(runningAt, job);
    EXPECT_TRUE(finished);
    EXPECT_EQ(exception, nullptr);
    EXPECT_EQ(run->getOutcome(), Outcome::SUCCEEDED);
    // One hand-over at the end, of every sample (the first step's and the events': the clock
    // never moves on), the first at the start of the flight; none is left in the listener.
    ASSERT_EQ(calls.size(), 1U);
    ASSERT_GE(calls.front().size(), 2U);
    EXPECT_EQ(calls.front().front().altitude, 0.0);
    EXPECT_TRUE(sampler->takeSamples().empty());
}

TEST_F(ManualSimulationExecutorTest, TheCallerKnowsTheJobBeforeItsCallbacks)
{
    // submit() returns before any callback, so a caller that keeps its current job finds it in
    // the callbacks (Java: RocketPanel's backgroundSimulationWorker == this).
    ManualSimulationExecutor  executor;
    const MotorlessSimulation simulation;
    JobId                     current = SimulationExecutor::kNoJob;
    JobId                     seen    = SimulationExecutor::kNoJob;
    current                           = executor.submit(
        nullptr, simulation.newRun(), nullptr, {}, std::stop_source{},
        {.progressed = {}, .finished = NoteTheCurrentJob{.current = &current, .seen = &seen}});
    QCoreApplication::processEvents();
    EXPECT_NE(current, SimulationExecutor::kNoJob);
    EXPECT_EQ(seen, current);

    // As before the turn, when the test runs the job itself.
    seen    = SimulationExecutor::kNoJob;
    current = executor.submit(
        nullptr, simulation.newRun(), nullptr, {}, std::stop_source{},
        {.progressed = {}, .finished = NoteTheCurrentJob{.current = &current, .seen = &seen}});
    EXPECT_EQ(executor.runAll(), 1U);
    EXPECT_EQ(seen, current);
    // The turn finds nothing left to run.
    QCoreApplication::processEvents();
    EXPECT_EQ(executor.submittedJobs().size(), 2U);
}

TEST_F(ManualSimulationExecutorTest, SteppedJobsWaitAndRunOldestFirst)
{
    ManualSimulationExecutor  executor(Mode::STEPPED);
    const MotorlessSimulation simulation;
    Log                       log;
    const JobId               first  = submitLogged(executor, simulation, log, "a");
    const JobId               second = submitLogged(executor, simulation, log, "b");
    const JobId               third  = submitLogged(executor, simulation, log, "c");
    QCoreApplication::processEvents();
    EXPECT_TRUE(log.empty());
    EXPECT_EQ(executor.waitingJobs(), (std::vector<JobId>{first, second, third}));
    EXPECT_TRUE(executor.isPending(second));
    // Ids grow with every job.
    EXPECT_LT(first, second);
    EXPECT_LT(second, third);

    EXPECT_TRUE(executor.runNext());
    EXPECT_EQ(log, (Log{"finished a SUCCEEDED"}));
    EXPECT_EQ(executor.runAll(), 2U);
    EXPECT_EQ(log, (Log{"finished a SUCCEEDED", "finished b SUCCEEDED", "finished c SUCCEEDED"}));
    EXPECT_FALSE(executor.runNext());
    EXPECT_TRUE(executor.waitingJobs().empty());
}

TEST_F(ManualSimulationExecutorTest, ACancelledWaitingJobEndsCancelledInTheNextTurn)
{
    // As the core runner ends a queued job: its run ends CANCELLED at once, without running, and
    // the end is reported later (the application's executor: at its next poll).
    ManualSimulationExecutor             executor(Mode::STEPPED);
    const MotorlessSimulation            simulation;
    Log                                  log;
    const std::shared_ptr<SimulationRun> run   = simulation.newRun();
    const JobId                          first = executor.submit(
        nullptr, run, nullptr, {}, std::stop_source{},
        {.progressed = {}, .finished = LoggingFinish{.log = &log, .name = "a", .run = run}});
    const JobId second = submitLogged(executor, simulation, log, "b");
    executor.cancel(first);
    // Nothing inside cancel(); the run has ended.
    EXPECT_TRUE(log.empty());
    EXPECT_EQ(run->getOutcome(), Outcome::CANCELLED);
    EXPECT_TRUE(run->wasStopped());
    EXPECT_TRUE(executor.isPending(first));
    // A job that is cancelled already, an unknown one and none are not cancelled again.
    executor.cancel(first);
    executor.cancel(second + 1);
    executor.cancel(SimulationExecutor::kNoJob);
    EXPECT_EQ(executor.cancelledJobs(), std::vector<JobId>{first});
    // The next turn reports it, whatever the mode; the other job goes on waiting.
    QCoreApplication::processEvents();
    EXPECT_EQ(log, (Log{"finished a CANCELLED"}));
    EXPECT_FALSE(executor.isPending(first));
    EXPECT_EQ(executor.waitingJobs(), std::vector<JobId>{second});
    EXPECT_EQ(executor.runAll(), 1U);
    EXPECT_EQ(log, (Log{"finished a CANCELLED", "finished b SUCCEEDED"}));
}

TEST_F(ManualSimulationExecutorTest, ARunningJobThatIsCancelledStopsAfterItsStep)
{
    ManualSimulationExecutor             executor;
    const ExampleDesign                  design(ExampleDesign::kSimpleModelRocket);
    const std::shared_ptr<SimulationRun> run =
        std::make_shared<SimulationRun>(*design.simulation(0));
    Log         log;
    const JobId job = executor.submit(
        nullptr, run, nullptr, {actionAtStep(5, CancelTheRunningJob{.executor = &executor})},
        std::stop_source{},
        {.progressed = {}, .finished = LoggingFinish{.log = &log, .name = "a", .run = run}});
    QCoreApplication::processEvents();
    EXPECT_EQ(log, (Log{"finished a CANCELLED"}));
    EXPECT_TRUE(run->wasStopped());
    EXPECT_FALSE(run->isAdoptable());
    EXPECT_EQ(executor.cancelledJobs(), std::vector<JobId>{job});
}

TEST_F(ManualSimulationExecutorTest, CancelAllCancelsTheJobsOfOneWindowInTheirOrder)
{
    ManualSimulationExecutor  executor(Mode::STEPPED);
    const MotorlessSimulation simulation;
    const QObject             closing;
    const QObject             other;
    Log                       log;
    const JobId               first  = submitLogged(executor, simulation, log, "a1", &closing);
    const JobId               second = submitLogged(executor, simulation, log, "b1", &other);
    const JobId               third  = submitLogged(executor, simulation, log, "a2", &closing);
    executor.cancelAll(&closing);
    EXPECT_TRUE(log.empty());
    EXPECT_EQ(executor.cancelledJobs(), (std::vector<JobId>{first, third}));
    QCoreApplication::processEvents();
    EXPECT_EQ(log, (Log{"finished a1 CANCELLED", "finished a2 CANCELLED"}));
    EXPECT_EQ(executor.waitingJobs(), std::vector<JobId>{second});
    EXPECT_EQ(executor.runAll(), 1U);
    EXPECT_EQ(log.back(), "finished b1 SUCCEEDED");
}

TEST_F(ManualSimulationExecutorTest, AnAbandonedRunningJobCallsNothingAgain)
{
    // Its window goes while the job runs: the run ends at its next step (the stop is requested),
    // and neither progressed() nor finished() comes.
    ManualSimulationExecutor             executor;
    const ExampleDesign                  design(ExampleDesign::kSimpleModelRocket);
    const std::shared_ptr<SimulationRun> run =
        std::make_shared<SimulationRun>(*design.simulation(0));
    const QObject window;
    Log           log;
    const JobId   job = executor.submit(
        &window, run, std::make_shared<RunSampleListener>(frozenSampleClock()),
        {actionAtStep(5, AbandonTheWindow{.executor = &executor, .window = &window})},
        std::stop_source{},
        {.progressed = LoggingProgress{.log = &log, .name = "a"},
         .finished   = LoggingFinish{.log = &log, .name = "a", .run = run}});
    QCoreApplication::processEvents();
    EXPECT_TRUE(log.empty());
    EXPECT_EQ(run->getOutcome(), Outcome::CANCELLED);
    EXPECT_FALSE(executor.isPending(job));
    EXPECT_EQ(executor.abandonedJobs(), std::vector<JobId>{job});
    EXPECT_TRUE(executor.cancelledJobs().empty());
    // An abandoned job is not cancelled or abandoned again.
    executor.cancel(job);
    executor.abandon(job);
    EXPECT_TRUE(executor.cancelledJobs().empty());
    EXPECT_EQ(executor.abandonedJobs(), std::vector<JobId>{job});
}

TEST_F(ManualSimulationExecutorTest, AJobThatAbandonsItselfInItsProgressIsNotFinished)
{
    ManualSimulationExecutor             executor(Mode::STEPPED);
    const ExampleDesign                  design(ExampleDesign::kSimpleModelRocket);
    const std::shared_ptr<SimulationRun> run =
        std::make_shared<SimulationRun>(*design.simulation(0));
    Log         log;
    const JobId job =
        executor.submit(nullptr, run, std::make_shared<RunSampleListener>(frozenSampleClock()), {},
                        std::stop_source{},
                        {.progressed = SelfAbandoningProgress{.executor = &executor, .log = &log},
                         .finished   = LoggingFinish{.log = &log, .name = "a", .run = run}});
    EXPECT_TRUE(executor.runNext());
    EXPECT_EQ(log, (Log{"progressed"}));
    EXPECT_EQ(executor.abandonedJobs(), std::vector<JobId>{job});
    EXPECT_FALSE(executor.isPending(job));
}

TEST_F(ManualSimulationExecutorTest, AbandonDropsTheWaitingJobsOfAWindowWithoutACallback)
{
    ManualSimulationExecutor  executor(Mode::STEPPED);
    const MotorlessSimulation simulation;
    const QObject             closing;
    const QObject             other;
    Log                       log;
    const JobId               first  = submitLogged(executor, simulation, log, "a1", &closing);
    const JobId               second = submitLogged(executor, simulation, log, "b1", &other);
    const JobId               third  = submitLogged(executor, simulation, log, "a2", &closing);
    executor.abandonAll(&closing);
    EXPECT_TRUE(log.empty());
    EXPECT_EQ(executor.abandonedJobs(), (std::vector<JobId>{first, third}));
    EXPECT_FALSE(executor.isPending(first));
    EXPECT_FALSE(executor.isPending(third));
    EXPECT_EQ(executor.waitingJobs(), std::vector<JobId>{second});
    executor.abandon(second);
    EXPECT_TRUE(executor.waitingJobs().empty());
    // Nothing for a job that is over, an unknown one or none.
    executor.abandon(second);
    executor.abandon(third + 1);
    executor.abandon(SimulationExecutor::kNoJob);
    EXPECT_EQ(executor.abandonedJobs(), (std::vector<JobId>{first, third, second}));
    EXPECT_EQ(executor.runAll(), 0U);
    QCoreApplication::processEvents();
    EXPECT_TRUE(log.empty());
}

TEST_F(ManualSimulationExecutorTest, TheNeverModeRunsNothingUntilTheJobIsCancelled)
{
    ManualSimulationExecutor  executor(Mode::NEVER);
    const MotorlessSimulation simulation;
    Log                       log;
    const JobId               job = submitLogged(executor, simulation, log, "a");
    bool                      ran = true;
    EXPECT_NONFATAL_FAILURE(ran = executor.runNext(),
                            "the manual simulation executor runs no job in its NEVER mode");
    EXPECT_FALSE(ran);
    QCoreApplication::processEvents();
    EXPECT_TRUE(log.empty());
    EXPECT_TRUE(executor.isPending(job));
    executor.cancel(job);
    QCoreApplication::processEvents();
    EXPECT_EQ(log, (Log{"finished a CANCELLED"}));
}

TEST_F(ManualSimulationExecutorTest, ProgressIsReportedWhenTheTestSays)
{
    // From inside the run (a listener of the test), the samples published so far; the rest at
    // the end.
    ManualSimulationExecutor             executor;
    const ExampleDesign                  design(ExampleDesign::kSimpleModelRocket);
    const std::shared_ptr<SimulationRun> run =
        std::make_shared<SimulationRun>(*design.simulation(0));
    std::vector<std::vector<RunSample>> calls;
    bool                                reported = false;
    const JobId                         job      = executor.submit(
        nullptr, run, std::make_shared<RunSampleListener>(frozenSampleClock()),
        {actionAtStep(3, ReportTheProgress{.executor = &executor, .reported = &reported})},
        std::stop_source{}, {.progressed = CollectSamples{.calls = &calls}, .finished = {}});
    EXPECT_FALSE(executor.reportProgress(job + 1));
    QCoreApplication::processEvents();
    EXPECT_TRUE(reported);
    ASSERT_EQ(calls.size(), 2U);
    // The first step's sample (and the launch's) came during the run, the rest at its end.
    EXPECT_FALSE(calls.front().empty());
    EXPECT_FALSE(calls.back().empty());
    EXPECT_EQ(run->getOutcome(), Outcome::SUCCEEDED);
}

TEST_F(ManualSimulationExecutorTest, ACallbackMaySubmitTheNextJob)
{
    ManualSimulationExecutor  executor;
    const MotorlessSimulation simulation;
    Log                       log;
    static_cast<void>(executor.submit(
        nullptr, simulation.newRun(), nullptr, {}, std::stop_source{},
        {.progressed = {},
         .finished =
             SubmitTheNext{.executor = &executor, .simulation = &simulation, .log = &log}}));
    EXPECT_TRUE(log.empty());
    // The one submitted in the turn runs in it, after submit() has returned.
    QCoreApplication::processEvents();
    EXPECT_EQ(log, (Log{"finished first", "finished second SUCCEEDED"}));
    EXPECT_EQ(executor.submittedJobs().size(), 2U);
}

TEST_F(ManualSimulationExecutorTest, NoTurnComesWhileACallbackRuns)
{
    // A callback that runs the event loop (a modal message) gets no other callback meanwhile: the
    // job it submitted runs once it has returned.
    ManualSimulationExecutor  executor;
    const MotorlessSimulation simulation;
    Log                       log;
    static_cast<void>(
        executor.submit(nullptr, simulation.newRun(), nullptr, {}, std::stop_source{},
                        {.progressed = {},
                         .finished   = SubmitAndRunTheEventLoop{
                             .executor = &executor, .simulation = &simulation, .log = &log}}));
    QCoreApplication::processEvents();
    EXPECT_EQ(log.front(), "first returns");
    QCoreApplication::processEvents();
    EXPECT_EQ(log, (Log{"first returns", "finished second SUCCEEDED"}));
}

TEST_F(ManualSimulationExecutorTest, ACallbackThatThrowsIsReportedAndTheTurnGoesOn)
{
    const RecordingGuardHook  hook;
    ManualSimulationExecutor  executor;
    const MotorlessSimulation simulation;
    Log                       log;
    static_cast<void>(executor.submit(nullptr, simulation.newRun(), nullptr, {}, std::stop_source{},
                                      {.progressed = {}, .finished = ThrowingFinish{}}));
    static_cast<void>(submitLogged(executor, simulation, log, "b"));
    QCoreApplication::processEvents();
    ASSERT_EQ(hook.failures().size(), 1U);
    EXPECT_TRUE(
        hook.failures().front().message.contains(QStringLiteral("a callback with a defect")));
    EXPECT_EQ(log, (Log{"finished b SUCCEEDED"}));
}

TEST_F(ManualSimulationExecutorTest, AJobThatCannotRunIsRefused)
{
    // As the core runner refuses it.
    ManualSimulationExecutor             executor(Mode::STEPPED);
    const MotorlessSimulation            simulation;
    const std::shared_ptr<SimulationRun> waiting = simulation.newRun();
    static_cast<void>(executor.submit(nullptr, waiting, nullptr, {}, std::stop_source{}, {}));
    const std::shared_ptr<SimulationRun> ran = simulation.newRun();
    ran->run(std::stop_source{}.get_token());
    EXPECT_THROW(
        static_cast<void>(executor.submit(nullptr, nullptr, nullptr, {}, std::stop_source{}, {})),
        BugError);
    EXPECT_THROW(
        static_cast<void>(executor.submit(nullptr, waiting, nullptr, {}, std::stop_source{}, {})),
        BugError);
    EXPECT_THROW(
        static_cast<void>(executor.submit(nullptr, ran, nullptr, {}, std::stop_source{}, {})),
        BugError);
    EXPECT_THROW(static_cast<void>(executor.submit(nullptr, simulation.newRun(), nullptr, {},
                                                   std::stop_source(std::nostopstate), {})),
                 BugError);
    EXPECT_THROW(static_cast<void>(executor.submit(nullptr, simulation.newRun(), nullptr, {nullptr},
                                                   std::stop_source{}, {})),
                 BugError);
    EXPECT_EQ(executor.submittedJobs().size(), 1U);
}

TEST_F(ManualSimulationExecutorTest, AListenerIsTheListenerOfOneLiveJob)
{
    // As the core runner refuses it: a sample or extra listener of a job that waits or runs,
    // until the job's end is reported; an abandoned waiting job's until the next turn (the
    // runner: until its next poll).
    ManualSimulationExecutor                  executor(Mode::STEPPED);
    const MotorlessSimulation                 simulation;
    const auto                                sampler = std::make_shared<RunSampleListener>();
    const std::shared_ptr<SimulationListener> extra   = std::make_shared<RunSampleListener>();
    const JobId first = submitWithListeners(executor, simulation, sampler, {extra});
    EXPECT_THROW(static_cast<void>(submitWithListeners(executor, simulation, sampler, {})),
                 BugError);
    EXPECT_THROW(static_cast<void>(submitWithListeners(executor, simulation, nullptr, {extra})),
                 BugError);
    EXPECT_THROW(static_cast<void>(submitWithListeners(executor, simulation, nullptr, {sampler})),
                 BugError);
    // Cancelled while it waits: live until its end is reported.
    executor.cancel(first);
    EXPECT_THROW(static_cast<void>(submitWithListeners(executor, simulation, nullptr, {extra})),
                 BugError);
    EXPECT_EQ(executor.runAll(), 1U);
    const JobId second = submitWithListeners(executor, simulation, sampler, {extra});
    EXPECT_NE(second, SimulationExecutor::kNoJob);
    executor.abandon(second);
    EXPECT_THROW(static_cast<void>(submitWithListeners(executor, simulation, sampler, {})),
                 BugError);
    QCoreApplication::processEvents();
    EXPECT_NE(submitWithListeners(executor, simulation, sampler, {extra}),
              SimulationExecutor::kNoJob);
    EXPECT_EQ(executor.submittedJobs().size(), 3U);
}

TEST_F(ManualSimulationExecutorTest, ARunningJobsListenerIsItsOwnUntilItsEnd)
{
    // From inside the run (a listener of the test submits another job with the running job's
    // listener): refused; once the job's finished() came, accepted.
    const RecordingGuardHook  hook;
    ManualSimulationExecutor  executor(Mode::STEPPED);
    const ExampleDesign       design(ExampleDesign::kSimpleModelRocket);
    const MotorlessSimulation simulation;
    const auto                shared  = std::make_shared<RunSampleListener>();
    bool                      refused = false;
    static_cast<void>(executor.submit(
        nullptr, std::make_shared<SimulationRun>(*design.simulation(0)), nullptr,
        {shared, actionAtStep(1, SubmitWithTheRunsListener{.executor   = &executor,
                                                           .simulation = &simulation,
                                                           .listener   = shared,
                                                           .refused    = &refused})},
        std::stop_source{}, {}));
    EXPECT_EQ(executor.runAll(), 1U);
    EXPECT_TRUE(refused);
    EXPECT_NE(submitWithListeners(executor, simulation, nullptr, {shared}),
              SimulationExecutor::kNoJob);
    EXPECT_TRUE(hook.failures().empty());
}

TEST_F(ManualSimulationExecutorTest, AModeChangeIsForTheJobsSubmittedAfterIt)
{
    ManualSimulationExecutor  executor(Mode::STEPPED);
    const MotorlessSimulation simulation;
    Log                       log;
    const JobId               waiting = submitLogged(executor, simulation, log, "a");
    executor.setMode(Mode::IMMEDIATE);
    static_cast<void>(submitLogged(executor, simulation, log, "b"));
    // A job submitted in the IMMEDIATE mode runs in the turn, whatever the mode is by then.
    executor.setMode(Mode::NEVER);
    QCoreApplication::processEvents();
    EXPECT_EQ(log, (Log{"finished b SUCCEEDED"}));
    EXPECT_EQ(executor.waitingJobs(), std::vector<JobId>{waiting});
    executor.setMode(Mode::STEPPED);
    EXPECT_EQ(executor.runAll(), 1U);
}

TEST_F(ManualSimulationExecutorTest, DestroyingItDropsTheWaitingJobsWithoutACallback)
{
    const MotorlessSimulation simulation;
    Log                       log;
    {
        ManualSimulationExecutor executor(Mode::STEPPED);
        static_cast<void>(submitLogged(executor, simulation, log, "a"));
        // And the turn of a job of the IMMEDIATE mode goes with it, as does that of a cancel.
        executor.setMode(Mode::IMMEDIATE);
        static_cast<void>(submitLogged(executor, simulation, log, "b"));
        executor.cancel(executor.waitingJobs().front());
    }
    QCoreApplication::processEvents();
    EXPECT_TRUE(log.empty());
}

}  // namespace

}  // namespace QtRocket::Gui::Test
