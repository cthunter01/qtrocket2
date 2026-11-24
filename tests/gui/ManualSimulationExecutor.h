#pragma once

#include <cstddef>
#include <deque>
#include <exception>
#include <memory>
#include <stop_token>
#include <vector>

#include <QObject>

#include "QtRocket/gui/util/SimulationExecutor.h"

namespace QtRocket
{
class RunSampleListener;
class SimulationListener;
class SimulationRun;
}  // namespace QtRocket

namespace QtRocket::Gui::Test
{

/// The SimulationExecutor of the tests (TestGuiServices'): it runs a job's run on the thread
/// that asks, the GUI thread, never on a thread of its own, so a test needs no sleep and no wait
/// for a run (O-M4-12) and stays clear of ThreadSanitizer's view of Qt. Running a job is what a
/// thread of the core's SimulationRunner does with it, and what the application's executor
/// (PollingSimulationExecutor) then hands over: SimulationRun::run() with the job's stop token
/// and its listeners (the sample listener, then the extra ones; whatever leaves run() itself is
/// kept as the job's exception), then progressed() with the samples the sample listener published
/// (when it published any), then finished() with the exception. Its mode says when a job runs:
/// - IMMEDIATE (the default): in the next turn of the event loop after submit() returned (a
///   QCoreApplication::processEvents(), QTest::qWaitFor(), a dialog's event loop), as the
///   application's executor reports at its next poll, or before it when the test says (runNext(),
///   runAll()). A job submitted in this mode runs in that turn whatever the mode is by then, and
///   so does one submitted in it from inside the turn;
/// - STEPPED: when the test says (runNext(), runAll()); until then the job waits, oldest first;
/// - NEVER: never; the job waits until it is cancelled (a test of the run dialog's Cancel, or one
///   that must not run anything), and a test that tries to run it fails.
/// A job cancelled while it waits has its run run at once, inside cancel(), with a stop that is
/// requested (not through its stop source), so that nothing of it runs and it ends CANCELLED, as
/// the core runner ends it; its finished() comes in the next turn of the event loop, whatever the
/// mode (or from runNext(), runAll(), when the test is first). A job cancelled while its run runs
/// (from inside the run: a listener of the test that cancels its own job, or another's) gets its
/// stop requested through its stop source, and its run ends CANCELLED after that step. An
/// abandoned job (abandon(), abandonAll()) gets no callback again: a waiting one is dropped
/// unrun, a running one runs to its end with a stop requested, and nothing is called after.
/// Everything is recorded: the jobs submitted, cancelled and abandoned. Destroying the executor
/// drops the waiting jobs without a callback, as the application's executor calls none once it
/// is being destroyed.
///
/// As the application's executor, it calls no callback from inside submit(), cancel(),
/// cancelAll(), abandon() or abandonAll(), and none while another of its callbacks runs: a turn
/// of the event loop that comes while a callback runs (the event loop of a modal message the
/// callback shows) waits until the callback has returned. Callbacks may call the executor (submit
/// the next job, cancel or abandon one); so may the listeners of a run. A callback that throws is
/// reported through Guard, as the application's executor reports it.
class ManualSimulationExecutor final : public SimulationExecutor
{
public:
    /// When a submitted job runs (see the class comment).
    enum class Mode
    {
        IMMEDIATE,
        STEPPED,
        NEVER,
    };

    /// A job as it was submitted.
    struct SubmittedJob
    {
        JobId          id     = kNoJob;
        const QObject* window = nullptr;
    };

    explicit ManualSimulationExecutor(Mode mode = Mode::IMMEDIATE) noexcept : m_mode(mode) { }
    ~ManualSimulationExecutor() override;
    ManualSimulationExecutor(const ManualSimulationExecutor&)            = delete;
    ManualSimulationExecutor& operator=(const ManualSimulationExecutor&) = delete;
    ManualSimulationExecutor(ManualSimulationExecutor&&)                 = delete;
    ManualSimulationExecutor& operator=(ManualSimulationExecutor&&)      = delete;

    [[nodiscard]] Mode mode() const noexcept { return m_mode; }
    /// The mode from now on, of the jobs submitted next and of runNext() (which runs none in the
    /// NEVER mode); the jobs that wait go on waiting (runAll() runs them in the other modes), and
    /// those submitted in the IMMEDIATE mode still run in the next turn.
    void setMode(Mode mode) noexcept { m_mode = mode; }

    /// @throws BugError when @p run is null, has run or is the run of a job that waits or runs,
    ///         when @p stopSource has no stop state, or when an extra listener is null (as the
    ///         core runner throws)
    [[nodiscard]] JobId submit(const QObject* window, std::shared_ptr<SimulationRun> run,
                               std::shared_ptr<RunSampleListener>               sampler,
                               std::vector<std::shared_ptr<SimulationListener>> extraListeners,
                               std::stop_source stopSource, Callbacks callbacks) override;
    void                cancel(JobId job) override;
    void                cancelAll(const QObject* window) override;
    void                abandon(JobId job) override;
    void                abandonAll(const QObject* window) override;
    [[nodiscard]] bool  isPending(JobId job) const override;

    /// The job that has waited longest, to its end, on this thread: the report of a job that was
    /// cancelled while it waited (finished()), or the run of a job and its report (see the class
    /// comment). False when no job waits; in the NEVER mode a test failure, and nothing runs.
    bool runNext();
    /// runNext() until no job waits (also the jobs submitted meanwhile, by a callback); the
    /// number of jobs ended.
    std::size_t runAll();
    /// Calls progressed() of @p job, which is pending (running: from inside its run, as a
    /// listener of the test calls it; or waiting), with the samples its sample listener has
    /// published so far (no call when there are none). False for any other job.
    bool reportProgress(JobId job);

    /// The jobs that wait (to run, or to report that they were cancelled), oldest first.
    [[nodiscard]] std::vector<JobId> waitingJobs() const;
    /// The job whose run is running (from inside a run: the innermost; an abandoned one too), or
    /// kNoJob.
    [[nodiscard]] JobId runningJob() const noexcept;
    /// Every job submitted so far, in order.
    [[nodiscard]] const std::vector<SubmittedJob>& submittedJobs() const noexcept
    {
        return m_submitted;
    }
    /// The pending jobs cancelled so far (cancel(), cancelAll()), in order, each once.
    [[nodiscard]] const std::vector<JobId>& cancelledJobs() const noexcept { return m_cancelled; }
    /// The pending jobs abandoned so far (abandon(), abandonAll()), in order, each once.
    [[nodiscard]] const std::vector<JobId>& abandonedJobs() const noexcept { return m_abandoned; }

private:
    /// A job, waiting or running.
    struct Job
    {
        JobId                                            id     = kNoJob;
        const QObject*                                   window = nullptr;
        std::shared_ptr<SimulationRun>                   run;
        std::shared_ptr<RunSampleListener>               sampler;
        std::vector<std::shared_ptr<SimulationListener>> extraListeners;
        std::stop_source                                 stop;
        Callbacks                                        callbacks;
        /// Submitted in the IMMEDIATE mode: it runs in the next turn.
        bool immediate = false;
        /// Cancelled while it waited: its run has ended, and only its report waits.
        bool ended = false;
        /// Abandoned while it runs: its callbacks are gone.
        bool abandoned = false;
        /// What ended it (see Callbacks::finished).
        std::exception_ptr exception;
    };

    /// Runs @p job (taken out of the queue) on this thread, and reports it.
    void run(Job job);
    /// Calls the callbacks of @p job, whose run has ended: progressed() with the samples left,
    /// then finished().
    void report(Job& job);
    /// Calls @p callback, as a callback of the executor (no turn comes meanwhile; what it throws
    /// is reported through Guard).
    template <class Callback>
    void deliver(const Callback& callback);
    /// Runs the next turn in the event loop, once.
    void postTurn();
    /// The turn: the reports of the cancelled jobs and the runs of the jobs submitted in the
    /// IMMEDIATE mode, oldest first.
    void runTurn();
    /// The waiting job @p job, or null.
    [[nodiscard]] Job* findWaiting(JobId job);
    /// The running job @p job, not abandoned, or null.
    [[nodiscard]] Job* findRunning(JobId job);
    /// The ids of the pending jobs of @p window, in the order they were submitted.
    [[nodiscard]] std::vector<JobId> pendingJobsOf(const QObject* window) const;
    /// Throws a BugError when @p run cannot be submitted (see submit()).
    void checkSubmission(
        const SimulationRun* run, const std::stop_source& stopSource,
        const std::vector<std::shared_ptr<SimulationListener>>& extraListeners) const;
    /// Requests a stop through @p source.
    static void requestStop(std::stop_source& source) noexcept;
    /// Adds @p job to @p jobs, once.
    static void record(std::vector<JobId>& jobs, JobId job);

    Mode            m_mode;
    JobId           m_lastJob{kNoJob};
    std::deque<Job> m_waiting;
    /// The jobs whose run runs, outermost first (a listener that runs another job nests).
    std::vector<Job*>         m_running;
    std::vector<SubmittedJob> m_submitted;
    std::vector<JobId>        m_cancelled;
    std::vector<JobId>        m_abandoned;
    /// How many callbacks of the executor run (nested ones counted).
    int m_delivering = 0;
    /// Whether a turn is posted and has not begun.
    bool m_turnPosted = false;
    /// Whether a turn came while a callback ran, and runs when the callback has returned.
    bool m_turnDeferred = false;
    /// The context of the posted turn, which goes with the executor (and the turn with it).
    QObject m_turn;
};

}  // namespace QtRocket::Gui::Test
