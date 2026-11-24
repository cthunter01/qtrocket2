#pragma once

#include <cstddef>
#include <deque>
#include <stop_token>
#include <vector>

#include <QObject>

#include "QtRocket/gui/util/SimulationExecutor.h"

namespace QtRocket::Gui::Test
{

/// The SimulationExecutor of the tests (TestGuiServices'): it runs a job's work on the thread
/// that asks, the GUI thread, never on a thread of its own, so a test needs no sleep and no wait
/// for a run (O-M4-12) and stays clear of ThreadSanitizer's view of Qt. Its mode says when a job
/// runs:
/// - IMMEDIATE (the default): in the next turn of the event loop after submit() returned (a
///   QCoreApplication::processEvents(), QTest::qWaitFor(), a dialog's event loop), as the
///   application's executor reports at its next poll, or before it when the test says (runNext(),
///   runAll()): the work, then progressed(), then finished(RAN). A job submitted in this mode
///   runs in that turn whatever the mode is by then, and so does one submitted in it from inside
///   the turn;
/// - STEPPED: when the test says (runNext(), runAll()); until then the job waits, oldest first;
/// - NEVER: never; the job waits until it is cancelled (a test of the run dialog's Cancel, or one
///   that must not run anything), and a test that tries to run it fails.
/// No callback comes from inside submit(). A job that runs gets a stop requested when it is
/// cancelled from inside its work (a test's work that cancels its own job, or another's: the token
/// then says so); a waiting job that is cancelled is dropped at once (finished(DROPPED), inside
/// cancel()). An abandoned job (abandon(), abandonAll()) gets no callback again: a waiting one is
/// dropped, a running one (abandoned from inside its work or its progressed()) runs its work to
/// its end, with a stop requested, and nothing is called after. Everything is recorded: the jobs
/// submitted, cancelled and abandoned. Destroying the executor drops the waiting jobs without a
/// callback, as the application's executor calls none once it is being destroyed.
///
/// Callbacks may call the executor (submit the next job, cancel or abandon one); a work may too.
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
    ~ManualSimulationExecutor() override                                 = default;
    ManualSimulationExecutor(const ManualSimulationExecutor&)            = delete;
    ManualSimulationExecutor& operator=(const ManualSimulationExecutor&) = delete;
    ManualSimulationExecutor(ManualSimulationExecutor&&)                 = delete;
    ManualSimulationExecutor& operator=(ManualSimulationExecutor&&)      = delete;

    [[nodiscard]] Mode mode() const noexcept { return m_mode; }
    /// The mode from now on, of the jobs submitted next and of runNext() (which runs none in the
    /// NEVER mode); the jobs that wait go on waiting (runAll() runs them in the other modes), and
    /// those submitted in the IMMEDIATE mode still run in the next turn.
    void setMode(Mode mode) noexcept { m_mode = mode; }

    [[nodiscard]] JobId submit(const QObject* window, Work work, Callbacks callbacks) override;
    void                cancel(JobId job) override;
    void                cancelAll(const QObject* window) override;
    void                abandon(JobId job) override;
    void                abandonAll(const QObject* window) override;
    [[nodiscard]] bool  isPending(JobId job) const override;

    /// Runs the job that has waited longest, to its end, on this thread: its work with its stop
    /// token, then progressed(), then finished(RAN). False when no job waits; in the NEVER mode a
    /// test failure, and nothing runs.
    bool runNext();
    /// runNext() until no job waits (also the jobs submitted meanwhile, by a callback); the
    /// number of jobs run.
    std::size_t runAll();
    /// Calls progressed() of @p job, which is pending (waiting, or running: from inside its
    /// work); false for any other job.
    bool reportProgress(JobId job);

    /// The jobs that wait, oldest first.
    [[nodiscard]] std::vector<JobId> waitingJobs() const;
    /// The job whose work is running (from inside a work: the innermost; an abandoned one too),
    /// or kNoJob.
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
        JobId            id     = kNoJob;
        const QObject*   window = nullptr;
        Work             work;
        Callbacks        callbacks;
        std::stop_source stop;
        /// Submitted in the IMMEDIATE mode: it runs in the next turn.
        bool immediate = false;
        /// Abandoned while it runs: its callbacks are gone.
        bool abandoned = false;
    };

    /// Runs @p job (taken out of the queue) on this thread, as runNext() says.
    void run(Job job);
    /// Runs the next turn of the IMMEDIATE mode in the event loop, once.
    void postTurn();
    /// The turn: runs the waiting jobs submitted in the IMMEDIATE mode, oldest first.
    void runImmediateJobs();
    /// The waiting job @p job, or null.
    [[nodiscard]] Job* findWaiting(JobId job);
    /// The running job @p job, not abandoned, or null.
    [[nodiscard]] Job* findRunning(JobId job);
    /// The ids of the pending jobs of @p window, in the order they were submitted.
    [[nodiscard]] std::vector<JobId> pendingJobsOf(const QObject* window) const;
    /// Requests a stop of the running @p job through its token.
    static void requestStop(Job& job) noexcept;
    /// Adds @p job to @p jobs, once.
    static void record(std::vector<JobId>& jobs, JobId job);

    Mode            m_mode;
    JobId           m_lastJob{kNoJob};
    std::deque<Job> m_waiting;
    /// The jobs whose work runs, outermost first (a work that runs another job nests).
    std::vector<Job*>         m_running;
    std::vector<SubmittedJob> m_submitted;
    std::vector<JobId>        m_cancelled;
    std::vector<JobId>        m_abandoned;
    /// Whether a turn is posted and has not begun.
    bool m_turnPosted = false;
    /// The context of the posted turn, which goes with the executor (and the turn with it).
    QObject m_turn;
};

}  // namespace QtRocket::Gui::Test
