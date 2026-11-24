#pragma once

#include <cstdint>
#include <functional>
#include <stop_token>

class QObject;

namespace QtRocket::Gui
{

/// Runs the simulations of the GUI off the GUI thread: what OpenRocket's run dialog does with its
/// static ThreadPoolExecutor of availableProcessors() threads (SimulationRunDialog.java:74-89) and
/// its design view with a worker thread (RocketPanel's background estimate), for every window of
/// the application. One object serves the application (app/ApplicationContext owns it,
/// GuiServices::simulationExecutor() hands it out); each job carries the window it belongs to,
/// which cancels them (cancelAll(): the run dialog's Cancel) or abandons them (abandonAll(): as it
/// goes).
///
/// A job is work that the executor runs once, on a thread of its choosing, with a stop token: in
/// the application the run of a duplicate of a document's simulation, prepared on the GUI thread
/// (the core's SimulationRun, Milestone 4: the run dialog's batch and the background estimate
/// submit work such as `[run, listeners](const std::stop_token& stop) { run->run(stop,
/// listeners); }`, with the run and its listeners held by shared pointers they keep too). The
/// executor never looks into the work: what came of it (the run's outcome, the progress its
/// listeners gathered) the caller reads from its own objects, which the work shares with it,
/// when a callback tells it to. (Provisional, until the core's SimulationRunner is there: then a
/// job becomes a SimulationRun with its sample listener and extra listeners, as the runner takes
/// them; progressed() hands over the samples the runner's poll took from the sample listener,
/// finished() the exception that left the run, and a job cancelled while it waits ends with its
/// run CANCELLED, as the runner ends it.)
///
/// Threads (U-M4-2): every function of the executor is called on the GUI thread, and every
/// callback is called there. The work runs on a worker thread (the application's executor: the
/// core's SimulationRunner of std::jthreads, whose mailboxes a QTimer of the GUI thread polls) or
/// on the GUI thread itself (the tests' ManualSimulationExecutor). So the work touches nothing
/// that the GUI thread uses without a lock the two share: never a Qt object, Guard, or a
/// Simulation, Rocket or OpenRocketDocument it does not own; and it does not throw (SimulationRun::
/// run() catches whatever its run throws, for the GUI thread to report). Whatever the work wrote
/// happens before its finished() callback (the executor's join or mailbox orders them): from
/// finished() on, the objects the work shared are the caller's alone. No data is sent from a
/// worker in a queued Qt call (QThreadPool, QtConcurrent and QFuture are not used).
///
/// When the callbacks come: never from inside submit(), so the caller knows a job's id (the one
/// submit() returned) before any callback of the job comes (a caller that keeps only its current
/// job compares the ids); from inside cancel() and cancelAll() (ManualSimulationExecutor drops a
/// waiting job there); or later, in the executor's own turn of the GUI thread's event loop (the
/// application's executor: its poll, about every 20 ms while a job is pending; the tests': the
/// turn after submit(), or when the test says). So a caller is ready for the job's end before
/// cancel() returns. A callback may call the executor (submit the next job, cancel or abandon
/// another, or its own). After finished(), no callback of the job comes again; nor after the job
/// was abandoned (abandon(), abandonAll()), nor once the executor is being destroyed (its
/// destructor cancels the jobs, waits for the running ones and calls no callback).
///
/// Lifetimes: the executor outlives every window (it is the application's), and a running job
/// that is cancelled calls back once its work has returned, a poll or more later. So callbacks
/// that capture an object of a window (a design view's background estimate, a run dialog's
/// batch) come only while that object lives because its jobs are abandoned before it goes: a
/// window abandons its jobs (abandonAll(window)) as it is destroyed, before the objects it owns
/// go; an object that may go before its window abandons its own jobs (abandon(job)) in its
/// destructor. cancel() and cancelAll() are for a run whose caller waits for its finished()
/// (Java's SwingWorker.done() after cancel(true), which the run dialog counts before it closes).
class SimulationExecutor
{
public:
    /// The id of a job, unique among the jobs of one executor; never kNoJob.
    using JobId = std::uint64_t;
    /// The id of no job.
    static constexpr JobId kNoJob = 0;

    /// What a job does, once, on the thread the executor runs it on: it watches @p stopToken
    /// (a stop is requested when the job is cancelled while it runs) and returns when it is done.
    /// It must not throw (see the class comment).
    using Work = std::function<void(const std::stop_token& stopToken)>;

    /// How a job ended, for its finished() callback.
    enum class Ending
    {
        /// The work was called and has returned, whether it ran to its end or stopped early on
        /// a stop it saw (which the work's own result tells: SimulationRun's outcome CANCELLED).
        RAN,
        /// The work was never called: the job was cancelled before it started (Java's purge()
        /// of the queued workers, SimulationRunDialog.cancelSimulations()), or the executor runs
        /// nothing.
        DROPPED,
    };

    /// What the executor tells the caller about a job, on the GUI thread (see the class comment
    /// for when). Either may be empty.
    struct Callbacks
    {
        /// The job may have news: called any number of times (none included) between submit()
        /// and finished(), while the job runs (the application's executor at each of its polls;
        /// ManualSimulationExecutor once just before finished(RAN), and when the test says). The
        /// caller reads the progress its work gathered (the run's sample listener keeps it
        /// behind a lock: the core's RunSampleListener).
        std::function<void()> progressed;
        /// The job is over: called exactly once, with how it ended, unless the job is abandoned
        /// first (or the executor destroyed).
        std::function<void(Ending ending)> finished;
    };

    SimulationExecutor()                                     = default;
    virtual ~SimulationExecutor()                            = default;
    SimulationExecutor(const SimulationExecutor&)            = delete;
    SimulationExecutor& operator=(const SimulationExecutor&) = delete;
    SimulationExecutor(SimulationExecutor&&)                 = delete;
    SimulationExecutor& operator=(SimulationExecutor&&)      = delete;

    /// Queues @p work as a job of @p window (the design window it runs for; null for none),
    /// after the jobs queued before it (first in, first out; as many run at once as the executor
    /// has threads), and returns the job's id. @p window is only compared, never used. No
    /// callback comes from inside submit(), not even when the executor runs nothing (it drops the
    /// job in a later turn).
    [[nodiscard]] virtual JobId submit(const QObject* window, Work work, Callbacks callbacks) = 0;

    /// Cancels the job @p job: a job that has not started is dropped (finished(DROPPED)), a
    /// running one gets a stop requested through its token (finished(RAN) once its work has
    /// returned). Nothing for a job that is over, abandoned or unknown, or kNoJob.
    virtual void cancel(JobId job) = 0;

    /// cancel() of every pending job of @p window, in the order they were submitted (Java's
    /// cancelSimulations(): cancel(true) of each worker, then purge()).
    virtual void cancelAll(const QObject* window) = 0;

    /// Abandons the job @p job: cancels it as cancel() does, but without a callback. Its callbacks
    /// are destroyed at once, on the GUI thread, and none of them is called after abandon()
    /// returns (nor from inside it; abandon() may be called from inside one of them). A running
    /// job's work still runs to its end on its thread, with a stop requested, which the executor
    /// waits for as for any job's, and whatever the work reports is dropped. From then on the job
    /// is not pending. Nothing for a job that is over, abandoned or unknown, or kNoJob. For an
    /// object whose callbacks capture it, in its destructor (see the class comment).
    virtual void abandon(JobId job) = 0;

    /// abandon() of every pending job of @p window: as the window is destroyed, before the objects
    /// it owns go (see the class comment).
    virtual void abandonAll(const QObject* window) = 0;

    /// Whether the job @p job is pending: submitted, its finished() not called yet, and not
    /// abandoned.
    [[nodiscard]] virtual bool isPending(JobId job) const = 0;
};

}  // namespace QtRocket::Gui
