#include "ManualSimulationExecutor.h"

#include <stop_token>
#include <string>
#include <vector>

#include <QObject>

#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/SimulationExecutor.h"

// The tests' executor of the simulations: it runs a job's work on the calling thread, at once, when
// the test says, or never, and tells the callbacks as SimulationExecutor's contract says.

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

TEST_F(ManualSimulationExecutorTest, TheImmediateModeRunsAJobInsideSubmit)
{
    ManualSimulationExecutor executor;
    EXPECT_EQ(executor.mode(), Mode::IMMEDIATE);
    const QObject    window;
    SeenWhileRunning seen;
    Log              log;
    const JobId      job = executor.submit(
        &window, NoteWhileRunning{.executor = &executor, .seen = &seen}, logging(log, "a"));
    // The work, then progressed(), then finished(RAN), all before submit() returned.
    EXPECT_EQ(log, (Log{"progressed a", "finished a RAN"}));
    EXPECT_NE(job, SimulationExecutor::kNoJob);
    EXPECT_EQ(seen.running, job);
    EXPECT_TRUE(seen.pending);
    EXPECT_FALSE(executor.isPending(job));
    EXPECT_EQ(executor.runningJob(), SimulationExecutor::kNoJob);
    ASSERT_EQ(executor.submittedJobs().size(), 1U);
    EXPECT_EQ(executor.submittedJobs().front().id, job);
    EXPECT_EQ(executor.submittedJobs().front().window, &window);
    EXPECT_TRUE(executor.cancelledJobs().empty());
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
    EXPECT_EQ(log, (Log{"work b", "progressed b", "finished b RAN"}));
    EXPECT_EQ(executor.waitingJobs(), std::vector<JobId>{waiting});
    EXPECT_EQ(executor.runAll(), 1U);
}

TEST_F(ManualSimulationExecutorTest, DestroyingItDropsTheWaitingJobsWithoutACallback)
{
    bool finished = false;
    {
        ManualSimulationExecutor executor(Mode::STEPPED);
        static_cast<void>(executor.submit(
            nullptr, {}, {.progressed = {}, .finished = SetWhenFinished{.called = &finished}}));
    }
    EXPECT_FALSE(finished);
}

}  // namespace

}  // namespace QtRocket::Gui::Test
