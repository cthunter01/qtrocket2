#include "ManualSimulationExecutor.h"

#include <stop_token>
#include <string>
#include <vector>

#include <QCoreApplication>
#include <QObject>

#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/SimulationExecutor.h"

// The tests' executor of the simulations: it runs a job's work on the calling thread, in the next
// turn of the event loop, when the test says, or never, and tells the callbacks as
// SimulationExecutor's contract says.

namespace QtRocket::Gui::Test
{

namespace
{

using ManualSimulationExecutorTest = GuiTest;
using Mode                         = ManualSimulationExecutor::Mode;
using Ending                       = SimulationExecutor::Ending;
using JobId                        = SimulationExecutor::JobId;

/// What the jobs did, in order.
using Log = std::vector<std::string>;

/// A job's work that logs "work <name>", with " stopped" when a stop was requested.
struct LoggingWork
{
    Log*        log;
    std::string name;

    void operator()(const std::stop_token& stop) const
    {
        log->push_back("work " + name + (stop.stop_requested() ? " stopped" : ""));
    }
};

/// A job's progressed() that logs "progressed <name>".
struct LoggingProgress
{
    Log*        log;
    std::string name;

    void operator()() const { log->push_back("progressed " + name); }
};

/// A job's finished() that logs "finished <name> RAN" or "... DROPPED".
struct LoggingFinish
{
    Log*        log;
    std::string name;

    void operator()(Ending ending) const
    {
        log->push_back("finished " + name + (ending == Ending::RAN ? " RAN" : " DROPPED"));
    }
};

/// The callbacks that log the job @p name into @p log.
[[nodiscard]] SimulationExecutor::Callbacks logging(Log& log, const std::string& name)
{
    return {.progressed = LoggingProgress{.log = &log, .name = name},
            .finished   = LoggingFinish{.log = &log, .name = name}};
}

/// Submits the job @p name of @p window to @p executor, logged into @p log.
JobId submitLogged(ManualSimulationExecutor& executor, Log& log, const std::string& name,
                   const QObject* window = nullptr)
{
    return executor.submit(window, LoggingWork{.log = &log, .name = name}, logging(log, name));
}

/// What a work sees of its executor while it runs: the running job and whether it is pending.
struct SeenWhileRunning
{
    JobId running = SimulationExecutor::kNoJob;
    bool  pending = false;
};

/// A job's work that notes what it sees of @p executor.
struct NoteWhileRunning
{
    const ManualSimulationExecutor* executor;
    SeenWhileRunning*               seen;

    void operator()(const std::stop_token& /*stop*/) const
    {
        seen->running = executor->runningJob();
        seen->pending = executor->isPending(seen->running);
    }
};

/// A job's work that cancels its own job while it runs, logging the stop before and after.
struct SelfCancellingWork
{
    ManualSimulationExecutor* executor;
    Log*                      log;

    void operator()(const std::stop_token& stop) const
    {
        log->emplace_back(stop.stop_requested() ? "stopped" : "running");
        executor->cancel(executor->runningJob());
        log->emplace_back(stop.stop_requested() ? "stopped" : "running");
    }
};

/// A job's finished() that submits the job "second" (as the background estimate goes on to the
/// next simulation).
struct SubmitTheNext
{
    ManualSimulationExecutor* executor;
    Log*                      log;

    void operator()(Ending ending) const
    {
        LoggingFinish{.log = log, .name = "first"}(ending);
        static_cast<void>(submitLogged(*executor, *log, "second"));
    }
};

/// A job's finished() that sets @p called.
struct SetWhenFinished
{
    bool* called;

    void operator()(Ending /*ending*/) const { *called = true; }
};

/// A job's finished() that notes the job its caller had as its current one by then (as the
/// design view's background estimate adopts only its current job's run).
struct NoteTheCurrentJob
{
    const JobId* current;
    JobId*       seen;

    void operator()(Ending /*ending*/) const { *seen = *current; }
};

/// A job's work that logs the stop before and after it abandons the jobs of @p window (as its
/// window goes while the run goes on).
struct WindowClosingWork
{
    ManualSimulationExecutor* executor;
    const QObject*            window;
    Log*                      log;

    void operator()(const std::stop_token& stop) const
    {
        log->emplace_back(stop.stop_requested() ? "stopped" : "running");
        executor->abandonAll(window);
        log->emplace_back(stop.stop_requested() ? "stopped" : "running");
    }
};

/// A job's progressed() that logs "progressed" and abandons the job that runs (its own).
struct SelfAbandoningProgress
{
    ManualSimulationExecutor* executor;
    Log*                      log;

    void operator()() const
    {
        log->emplace_back("progressed");
        executor->abandon(executor->runningJob());
    }
};

TEST_F(ManualSimulationExecutorTest, TheImmediateModeRunsAJobInTheNextTurnOfTheEventLoop)
{
    ManualSimulationExecutor executor;
    EXPECT_EQ(executor.mode(), Mode::IMMEDIATE);
    const QObject    window;
    SeenWhileRunning seen;
    Log              log;
    const JobId      job = executor.submit(
        &window, NoteWhileRunning{.executor = &executor, .seen = &seen}, logging(log, "a"));
    // Nothing inside submit(): the job waits for the turn.
    EXPECT_TRUE(log.empty());
    EXPECT_NE(job, SimulationExecutor::kNoJob);
    EXPECT_TRUE(executor.isPending(job));
    EXPECT_EQ(executor.waitingJobs(), std::vector<JobId>{job});
    QCoreApplication::processEvents();
    // The work, then progressed(), then finished(RAN).
    EXPECT_EQ(log, (Log{"progressed a", "finished a RAN"}));
    EXPECT_EQ(seen.running, job);
    EXPECT_TRUE(seen.pending);
    EXPECT_FALSE(executor.isPending(job));
    EXPECT_EQ(executor.runningJob(), SimulationExecutor::kNoJob);
    ASSERT_EQ(executor.submittedJobs().size(), 1U);
    EXPECT_EQ(executor.submittedJobs().front().id, job);
    EXPECT_EQ(executor.submittedJobs().front().window, &window);
    EXPECT_TRUE(executor.cancelledJobs().empty());
    EXPECT_TRUE(executor.abandonedJobs().empty());
}

TEST_F(ManualSimulationExecutorTest, TheCallerKnowsTheJobBeforeItsCallbacks)
{
    // submit() returns before any callback, so a caller that keeps its current job finds it in
    // the callbacks (Java: RocketPanel's backgroundSimulationWorker == this).
    ManualSimulationExecutor executor;
    JobId                    current = SimulationExecutor::kNoJob;
    JobId                    seen    = SimulationExecutor::kNoJob;
    current                          = executor.submit(
        nullptr, {},
        {.progressed = {}, .finished = NoteTheCurrentJob{.current = &current, .seen = &seen}});
    QCoreApplication::processEvents();
    EXPECT_NE(current, SimulationExecutor::kNoJob);
    EXPECT_EQ(seen, current);

    // As before the turn, when the test runs the job itself.
    seen    = SimulationExecutor::kNoJob;
    current = executor.submit(
        nullptr, {},
        {.progressed = {}, .finished = NoteTheCurrentJob{.current = &current, .seen = &seen}});
    EXPECT_EQ(executor.runAll(), 1U);
    EXPECT_EQ(seen, current);
    // The turn finds nothing left to run.
    QCoreApplication::processEvents();
    EXPECT_EQ(executor.submittedJobs().size(), 2U);
}

TEST_F(ManualSimulationExecutorTest, SteppedJobsWaitAndRunOldestFirst)
{
    ManualSimulationExecutor executor(Mode::STEPPED);
    Log                      log;
    const JobId              first  = submitLogged(executor, log, "a");
    const JobId              second = submitLogged(executor, log, "b");
    const JobId              third  = submitLogged(executor, log, "c");
    EXPECT_TRUE(log.empty());
    EXPECT_EQ(executor.waitingJobs(), (std::vector<JobId>{first, second, third}));
    EXPECT_TRUE(executor.isPending(second));
    // Ids grow with every job.
    EXPECT_LT(first, second);
    EXPECT_LT(second, third);

    EXPECT_TRUE(executor.runNext());
    EXPECT_EQ(log, (Log{"work a", "progressed a", "finished a RAN"}));
    EXPECT_EQ(executor.runAll(), 2U);
    EXPECT_EQ(log, (Log{"work a", "progressed a", "finished a RAN", "work b", "progressed b",
                        "finished b RAN", "work c", "progressed c", "finished c RAN"}));
    EXPECT_FALSE(executor.runNext());
    EXPECT_TRUE(executor.waitingJobs().empty());
}

TEST_F(ManualSimulationExecutorTest, ACancelledWaitingJobIsDroppedAtOnce)
{
    ManualSimulationExecutor executor(Mode::STEPPED);
    Log                      log;
    const JobId              first  = submitLogged(executor, log, "a");
    const JobId              second = submitLogged(executor, log, "b");
    executor.cancel(first);
    // Its finished(DROPPED) came inside cancel(), and its work never runs.
    EXPECT_EQ(log, (Log{"finished a DROPPED"}));
    EXPECT_FALSE(executor.isPending(first));
    EXPECT_EQ(executor.waitingJobs(), std::vector<JobId>{second});
    // A job that is over, an unknown one and none are not cancelled again.
    executor.cancel(first);
    executor.cancel(second + 1);
    executor.cancel(SimulationExecutor::kNoJob);
    EXPECT_EQ(executor.cancelledJobs(), std::vector<JobId>{first});
    EXPECT_EQ(executor.runAll(), 1U);
    EXPECT_EQ(log, (Log{"finished a DROPPED", "work b", "progressed b", "finished b RAN"}));
}

TEST_F(ManualSimulationExecutorTest, ARunningJobThatIsCancelledSeesTheStopAndRunsToItsEnd)
{
    ManualSimulationExecutor executor;
    Log                      log;
    const JobId              job = executor.submit(
        nullptr, SelfCancellingWork{.executor = &executor, .log = &log}, logging(log, "a"));
    QCoreApplication::processEvents();
    EXPECT_EQ(log, (Log{"running", "stopped", "progressed a", "finished a RAN"}));
    EXPECT_EQ(executor.cancelledJobs(), std::vector<JobId>{job});
}

TEST_F(ManualSimulationExecutorTest, CancelAllCancelsTheJobsOfOneWindowInTheirOrder)
{
    ManualSimulationExecutor executor(Mode::STEPPED);
    const QObject            closing;
    const QObject            other;
    Log                      log;
    const JobId              first  = submitLogged(executor, log, "a1", &closing);
    const JobId              second = submitLogged(executor, log, "b1", &other);
    const JobId              third  = submitLogged(executor, log, "a2", &closing);
    executor.cancelAll(&closing);
    EXPECT_EQ(log, (Log{"finished a1 DROPPED", "finished a2 DROPPED"}));
    EXPECT_EQ(executor.cancelledJobs(), (std::vector<JobId>{first, third}));
    EXPECT_EQ(executor.waitingJobs(), std::vector<JobId>{second});
    EXPECT_EQ(executor.runAll(), 1U);
    EXPECT_EQ(log.back(), "finished b1 RAN");
}

TEST_F(ManualSimulationExecutorTest, AnAbandonedRunningJobCallsNothingAgain)
{
    // Its window goes while the job runs: the work runs to its end with the stop requested, and
    // neither progressed() nor finished() comes.
    ManualSimulationExecutor executor;
    const QObject            window;
    Log                      log;
    const JobId              job = executor.submit(
        &window, WindowClosingWork{.executor = &executor, .window = &window, .log = &log},
        logging(log, "a"));
    QCoreApplication::processEvents();
    EXPECT_EQ(log, (Log{"running", "stopped"}));
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
    ManualSimulationExecutor executor(Mode::STEPPED);
    Log                      log;
    const JobId              job =
        executor.submit(nullptr, LoggingWork{.log = &log, .name = "a"},
                        {.progressed = SelfAbandoningProgress{.executor = &executor, .log = &log},
                         .finished   = LoggingFinish{.log = &log, .name = "a"}});
    EXPECT_TRUE(executor.runNext());
    EXPECT_EQ(log, (Log{"work a", "progressed"}));
    EXPECT_EQ(executor.abandonedJobs(), std::vector<JobId>{job});
    EXPECT_FALSE(executor.isPending(job));
}

TEST_F(ManualSimulationExecutorTest, AbandonDropsTheWaitingJobsOfAWindowWithoutACallback)
{
    ManualSimulationExecutor executor(Mode::STEPPED);
    const QObject            closing;
    const QObject            other;
    Log                      log;
    const JobId              first  = submitLogged(executor, log, "a1", &closing);
    const JobId              second = submitLogged(executor, log, "b1", &other);
    const JobId              third  = submitLogged(executor, log, "a2", &closing);
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
    EXPECT_TRUE(log.empty());
}

TEST_F(ManualSimulationExecutorTest, TheNeverModeRunsNothingUntilTheJobIsCancelled)
{
    ManualSimulationExecutor executor(Mode::NEVER);
    Log                      log;
    const JobId              job = submitLogged(executor, log, "a");
    bool                     ran = true;
    EXPECT_NONFATAL_FAILURE(ran = executor.runNext(),
                            "the manual simulation executor runs no job in its NEVER mode");
    EXPECT_FALSE(ran);
    EXPECT_TRUE(log.empty());
    EXPECT_TRUE(executor.isPending(job));
    executor.cancel(job);
    EXPECT_EQ(log, (Log{"finished a DROPPED"}));
}

TEST_F(ManualSimulationExecutorTest, ProgressIsReportedWhenTheTestSays)
{
    ManualSimulationExecutor executor(Mode::STEPPED);
    Log                      log;
    const JobId              job = submitLogged(executor, log, "a");
    EXPECT_TRUE(executor.reportProgress(job));
    EXPECT_FALSE(executor.reportProgress(job + 1));
    EXPECT_EQ(log, (Log{"progressed a"}));
}

TEST_F(ManualSimulationExecutorTest, ACallbackMaySubmitTheNextJob)
{
    ManualSimulationExecutor executor;
    Log                      log;
    static_cast<void>(executor.submit(
        nullptr, LoggingWork{.log = &log, .name = "first"},
        {.progressed = {}, .finished = SubmitTheNext{.executor = &executor, .log = &log}}));
    EXPECT_TRUE(log.empty());
    // The one submitted in the turn runs in it, after submit() has returned.
    QCoreApplication::processEvents();
    EXPECT_EQ(log, (Log{"work first", "finished first RAN", "work second", "progressed second",
                        "finished second RAN"}));
    EXPECT_EQ(executor.submittedJobs().size(), 2U);
}

TEST_F(ManualSimulationExecutorTest, AModeChangeIsForTheJobsSubmittedAfterIt)
{
    ManualSimulationExecutor executor(Mode::STEPPED);
    Log                      log;
    const JobId              waiting = submitLogged(executor, log, "a");
    executor.setMode(Mode::IMMEDIATE);
    static_cast<void>(submitLogged(executor, log, "b"));
    // A job submitted in the IMMEDIATE mode runs in the turn, whatever the mode is by then.
    executor.setMode(Mode::NEVER);
    QCoreApplication::processEvents();
    EXPECT_EQ(log, (Log{"work b", "progressed b", "finished b RAN"}));
    EXPECT_EQ(executor.waitingJobs(), std::vector<JobId>{waiting});
    executor.setMode(Mode::STEPPED);
    EXPECT_EQ(executor.runAll(), 1U);
}

TEST_F(ManualSimulationExecutorTest, DestroyingItDropsTheWaitingJobsWithoutACallback)
{
    bool finished = false;
    {
        ManualSimulationExecutor executor(Mode::STEPPED);
        static_cast<void>(executor.submit(
            nullptr, {}, {.progressed = {}, .finished = SetWhenFinished{.called = &finished}}));
        // And the turn of a job of the IMMEDIATE mode goes with it.
        executor.setMode(Mode::IMMEDIATE);
        static_cast<void>(executor.submit(
            nullptr, {}, {.progressed = {}, .finished = SetWhenFinished{.called = &finished}}));
    }
    QCoreApplication::processEvents();
    EXPECT_FALSE(finished);
}

}  // namespace

}  // namespace QtRocket::Gui::Test
