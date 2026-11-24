#pragma once

#include <compare>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string_view>
#include <thread>
#include <vector>

#include "QtRocket/simulation/RunSample.h"

namespace QtRocket
{

class RunSampleListener;
class SimulationListener;
class SimulationRun;

/// Runs simulations on threads of its own: what the thread pool of OpenRocket's run dialog
/// does (swing gui/simulation/SimulationRunDialog.java:70-89, a ThreadPoolExecutor of
/// SwingPreferences.getMaxThreadCount() = availableProcessors() threads with an unbounded
/// queue, and the cancel of :197-202: SwingWorker.cancel(true) and executor.purge()), without
/// Swing, without Qt and without calls from the workers to anyone. An addition.
///
/// A job is a SimulationRun (prepared on the owning thread, see there) with the listeners it
/// runs with: a RunSampleListener for the progress (Java: the worker's
/// SimulationProgressListener), then the extra listeners (M7: the custom expressions), then the
/// InterruptListener that SimulationRun::run() adds for the job's stop token (Java: the
/// worker's CancelListener), in Java's order (InteractiveSimulationWorker.getExtraListeners()
/// and SimulationWorker.doInBackground()). The jobs start in the order they were submitted
/// (FIFO), each on the first free thread; at most getThreadCount() run at the same time, by
/// default max(1, std::thread::hardware_concurrency()) (Java: availableProcessors(), which is
/// never less than 1). The threads are started as the jobs come, one more for each job
/// submitted while fewer are started (as a ThreadPoolExecutor starts its core threads), and
/// wait for further jobs until the runner is destroyed.
///
/// Each job has a mailbox: its state (QUEUED, RUNNING, ENDED) and the exception that ended it,
/// under a mutex of the job's own, and the samples of its RunSampleListener, which locks
/// itself. The owning thread reads the mailboxes when it calls poll(), which hands it the news
/// of every job that has some (its run started, new samples, its run ended), on the owning
/// thread: the GUI's timer calls it (about every 20 ms while isBusy()), a test calls it in a
/// loop. Nothing is ever sent from a worker to the owning thread: no callback, no queued call,
/// no signal.
///
/// The endings (SimulationRun::getOutcome() of the job's run once poll() reported it ENDED):
/// - a run that ran: SUCCEEDED, FAILED or EXCEPTION, as SimulationRun::run() tells them apart;
///   an exception the run carries (EXCEPTION) is the news' exception too;
/// - a job cancelled while it runs (cancel(), cancelGroup(), cancelAll(), the destructor, or a
///   stop requested through the stop source the job was submitted with): its stop is
///   requested, and its run ends CANCELLED after the step it is taking (or as it ends anyway
///   when the request comes too late), "The simulation was interrupted.";
/// - a job cancelled or purged while it is queued: it is taken from the queue and its run is run
///   at once on the owning thread with a stop that is requested (not through the job's stop
///   source, which other jobs may share), so it ends CANCELLED and nothing of it runs (Java: a
///   cancelled future that executor.purge() drops, whose worker never runs; its done() still
///   reaches the dialog, which counts it as done). poll() reports it like any job that ended.
/// The runner adopts nothing: the caller adopts what it wants on the owning thread
/// (SimulationRun::adoptInto(), after it checked that the simulation the run was made from is
/// still in its document and that the job is still wanted). A cancelled run is never adoptable
/// (OpenRocket bug 40, see SimulationRun).
///
/// THE THREAD CONTRACT.
/// - The runner belongs to the thread that made it (the owning thread: the GUI's, a test's):
///   every member function is called there (else a BugError), and so is the destructor.
/// - A job's SimulationRun, its RunSampleListener and its extra listeners are the job's from
///   submit() until poll() reports the job ENDED: the owning thread does not touch them in
///   between (the sample listener's takeSamples() aside, which poll() calls; do not call it
///   yourself, or the news miss those samples). From that report on they are the owning
///   thread's again: the worker's last access to the job is the unlock of the mutex under which
///   it marked the job ENDED, which poll() locks before it reads the state, so whatever the run
///   wrote is visible to the owning thread after the report.
/// - A worker touches the job it runs (its run and listeners), the queue and the job's mailbox,
///   and nothing else: no Qt object, no Guard, no Simulation, Rocket or OpenRocketDocument but
///   the duplicate the run owns. A run's duplicate shares nothing with the simulation it was
///   made from but the preference store, which is thread-safe (see
///   Simulation::duplicateForIndependentSimulation()), so the owning thread may change the
///   design, the simulations and the preferences while jobs run.
/// - Nothing leaves a worker: SimulationRun::run() carries what the simulation throws, and the
///   worker catches anything that left run() itself into the news' exception (an exception
///   that leaves the function of a std::jthread is std::terminate()). The owning thread reports
///   or rethrows it (the GUI through Guard::failureOf(), as Java's run dialog hands it to the
///   application's exception handler).
/// - The jobs and what they hold are destroyed on the owning thread (poll() forgets a job
///   when it reports it ENDED), never on a worker.
/// - The destructor cancels every job and joins the threads: the running jobs get their stop
///   requested and end after their current step (which validateInputs() keeps short), mostly
///   CANCELLED; the queued ones are dropped and never run (their runs stay NOT_RUN). It hands
///   out no news: a caller that still holds a run reads its outcome there.
/// Lock order: a worker takes a job's mutex while it holds the queue's (when it takes a job
/// from the queue); the owning thread never holds both.
///
/// What OpenRocket's run dialog does with this (run 2, the GUI's SimulationBatch), per
/// simulation, on the owning thread: getOptions().randomizeSeedIfNotFixed(); a SimulationRun of
/// it; a SimulationProgressEstimator of the duplicate's configuration; a RunSampleListener;
/// submit(run, group, sampler); on each poll the estimator processes the news' samples; when a
/// job ends, the estimator finishes, the run is adopted if the simulation is still in the
/// document, a failure is shown, and SimulationBatchCompletion::markSimulationComplete() tells
/// when the batch is complete (one SimulationChangeEvent then). Its Cancel button:
/// cancelGroup(group) or cancel() of each job.
///
/// Deviations from OpenRocket:
/// - The threads have no priority of their own (std::thread has none to give): the jobs of the
///   design view's flight estimate run at the priority of the others, where Java gives them a
///   pool of Thread.MIN_PRIORITY threads of their own (swing RocketPanel.java:302-312).
/// - A running job that is cancelled ends when its run notices the stop (after its current
///   step) and poll() reports it then; Java's SwingWorker.cancel() calls the worker's done() on
///   the event thread at once, while the worker thread still runs to its next step.
/// - The threads are the runner's, not the process's: two runners have two pools (Java's pool
///   is static and shared by every run dialog).
/// Not copyable and not movable.
class SimulationRunner
{
public:
    /// Names a job of the runner: unique among the jobs of the runner, never reused.
    struct JobId
    {
        std::uint64_t value{0};

        friend bool operator==(const JobId&, const JobId&)  = default;
        friend auto operator<=>(const JobId&, const JobId&) = default;
    };

    /// A group of jobs that are cancelled or purged together (a window's, a batch's): from
    /// newGroup(). GroupId{} is the group of the jobs submitted without one.
    struct GroupId
    {
        std::uint64_t value{0};

        friend bool operator==(const GroupId&, const GroupId&)  = default;
        friend auto operator<=>(const GroupId&, const GroupId&) = default;
    };

    /// Where a job is.
    enum class JobState
    {
        QUEUED,   ///< waiting for a thread
        RUNNING,  ///< its run runs on a thread of the runner
        ENDED,    ///< its run has ended (or was cancelled while queued): it has its outcome
    };

    /// What poll() reports of a job.
    struct News
    {
        /// The job.
        JobId job;
        /// Its group.
        GroupId group;
        /// RUNNING: its run started since the last poll() or has new samples; ENDED: its run
        /// has ended, and this is the last news of the job (with the last samples).
        JobState state{JobState::RUNNING};
        /// The job's run (the one given to submit()). Read it on the owning thread only once
        /// the state is ENDED.
        std::shared_ptr<SimulationRun> run;
        /// The samples its RunSampleListener published since the last news of the job, in the
        /// order they were published; empty for a job without a sample listener.
        std::vector<RunSample> samples;
        /// The exception that ended the job (ENDED only): the one the run carries
        /// (SimulationRun::getException(), outcome EXCEPTION) or one that left
        /// SimulationRun::run() itself (the run's outcome is then NOT_RUN); null otherwise.
        std::exception_ptr exception;
    };

    /// max(1, std::thread::hardware_concurrency()): the default number of threads (Java:
    /// Runtime.availableProcessors()).
    [[nodiscard]] static std::size_t defaultThreadCount() noexcept;

    /// A runner of defaultThreadCount() threads. Starts no thread yet.
    SimulationRunner();

    /// A runner of at most @p threadCount threads (a test's count). Starts no thread yet.
    /// @throws BugError when @p threadCount is 0
    explicit SimulationRunner(std::size_t threadCount);

    SimulationRunner(const SimulationRunner&)            = delete;
    SimulationRunner& operator=(const SimulationRunner&) = delete;
    SimulationRunner(SimulationRunner&&)                 = delete;
    SimulationRunner& operator=(SimulationRunner&&)      = delete;

    /// Cancels every job and joins the threads (see the class comment).
    ~SimulationRunner();

    /// The most threads the runner runs jobs on (the count it was made with).
    [[nodiscard]] std::size_t getThreadCount() const noexcept { return m_threadCount; }

    /// A new group, not GroupId{} and not one this runner gave before.
    [[nodiscard]] GroupId newGroup();

    /// Queues the job of running @p run, in @p group, with @p sampler (may be null) and then
    /// @p extraListeners as its extra listeners, cancelled through @p stopSource (by default a
    /// stop source of its own; pass one to request the stop yourself, or to let a listener of
    /// yours see the request through its token; a source that several jobs share stops all of
    /// them that run when one is cancelled). The job starts when a thread is free (a new one is
    /// started while fewer than getThreadCount() are). From now on the run and the listeners are
    /// the job's (see the thread contract).
    /// @throws BugError when @p run is null, has run, or is the run of a job of this runner
    ///         that poll() has not reported ENDED, when an extra listener is null, or when
    ///         @p stopSource has no stop state (std::nostopstate)
    /// @throws std::system_error when no thread can be started and none is (the job is not
    ///         queued then)
    JobId submit(std::shared_ptr<SimulationRun> run, GroupId group = GroupId{.value = 0},
                 std::shared_ptr<RunSampleListener>               sampler        = nullptr,
                 std::vector<std::shared_ptr<SimulationListener>> extraListeners = {},
                 std::stop_source                                 stopSource     = {});

    /// Cancels @p job: a queued job is taken from the queue and ends CANCELLED without running;
    /// a running one gets its stop requested and ends after its current step. Nothing for a
    /// job that has ended or that the runner does not know (any more).
    void cancel(JobId job);

    /// Purges the queued jobs of @p group: each is taken from the queue and ends CANCELLED
    /// without running. The running jobs of the group go on. (Java: executor.purge(), which
    /// drops the queued workers that were cancelled; here the queued jobs of one group.)
    void purge(GroupId group);

    /// Cancels every job of @p group: purge(group), then a stop request to each of its running
    /// jobs (Java: cancelSimulations(), the run dialog's Cancel).
    void cancelGroup(GroupId group);

    /// Cancels every job of the runner.
    void cancelAll();

    /// The news of every job that has some since the last call, in the order the jobs were
    /// submitted: a job whose run started, that has new samples, or whose run ended (its last
    /// news; the runner forgets it then). Empty when nothing happened.
    [[nodiscard]] std::vector<News> poll();

    /// The state of @p job now (it may change at once, but for ENDED); nullopt for a job the
    /// runner does not know, never had or has reported ENDED.
    [[nodiscard]] std::optional<JobState> getState(JobId job) const;

    /// Whether the runner has a job that poll() has not reported ENDED: one that is queued,
    /// running, or ended since the last poll().
    [[nodiscard]] bool isBusy() const;

private:
    struct Job;

    /// Throws a BugError when called on another thread than the owning one.
    void checkThread() const;

    /// The job @p id, or null.
    [[nodiscard]] Job* findJob(JobId id) const;

    /// Starts one more thread while fewer than m_threadCount are started.
    void startThread();

    /// The body of a thread: runs jobs from the queue until the runner shuts down.
    void work();

    /// The next job of the queue, marked RUNNING; null when the runner shuts down.
    [[nodiscard]] Job* takeJob();

    /// Takes the queued jobs for which @p taken is true from the queue.
    template <typename Predicate>
    [[nodiscard]] std::vector<Job*> takeQueued(Predicate taken);

    /// Ends @p job, taken from the queue, without running it (see the class comment).
    static void endUnrun(Job& job);

    /// Requests the stop of @p job when it has not ended.
    static void requestStop(Job& job);

    // Shared with the threads, under m_queueMutex.
    mutable std::mutex      m_queueMutex;
    std::condition_variable m_queueChanged;
    std::deque<Job*>        m_queue;
    bool                    m_shuttingDown{false};

    // The owning thread's.
    std::thread::id                   m_owner;
    std::size_t                       m_threadCount;
    std::uint64_t                     m_lastJob{0};
    std::uint64_t                     m_lastGroup{0};
    std::vector<std::unique_ptr<Job>> m_jobs;  ///< not reported ENDED, in submission order
    /// Joined by the destructor, before anything else of the runner goes.
    std::vector<std::jthread> m_threads;
};

/// The constant's name, e.g. "RUNNING".
[[nodiscard]] std::string_view name(SimulationRunner::JobState state) noexcept;

}  // namespace QtRocket
