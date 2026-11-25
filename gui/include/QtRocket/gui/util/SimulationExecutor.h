#pragma once

#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <span>
#include <stop_token>
#include <vector>

#include "QtRocket/simulation/RunSample.h"

class QObject;

namespace QtRocket
{
class RunSampleListener;
class SimulationListener;
class SimulationRun;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// Runs the simulations of the GUI off the GUI thread: what OpenRocket's run dialog does with its
/// static ThreadPoolExecutor of availableProcessors() threads (SimulationRunDialog.java:74-89) and
/// its design view with a worker thread (RocketPanel's background estimate), for every window of
/// the application. One object serves the application (app/ApplicationContext owns it,
/// GuiServices::simulationExecutor() hands it out: util/PollingSimulationExecutor, the GUI's
/// adapter of the core's SimulationRunner); each job carries the window it belongs to, which
/// cancels them (cancelAll()) or abandons them (abandonAll(): as it goes).
///
/// A job is the run of a duplicate of a document's simulation, prepared on the GUI thread (the
/// core's SimulationRun), with the listeners it runs with, as the core's SimulationRunner takes
/// them: a RunSampleListener for the progress (may be null), then the extra listeners (M7: the
/// custom expressions; the design view's estimate: its own), then the InterruptListener that
/// SimulationRun::run() adds for the job's stop. The job is stopped through the stop source it was
/// submitted with (a listener of the caller may watch its token: a test's hold). From submit()
/// until the job's finished() the run and its listeners are the job's (the executor's thread runs
/// them): the caller does not touch them (it keeps its shared pointers, and reads the run once
/// finished() came). The callbacks hand over what came of the job: progressed() the samples the
/// sample listener published (the core runner's poll takes them from it; the caller never calls
/// takeSamples() itself), finished() the exception that ended the job, and the run's outcome tells
/// the rest (SimulationRun::getOutcome(): SUCCEEDED, FAILED, EXCEPTION, or CANCELLED for a job
/// cancelled while it ran or while it waited: a job cancelled before it started has its run run
/// at once with a stop that is requested, so that nothing of it runs, as the core runner ends it).
/// The caller adopts the run (SimulationRun::adoptInto()) in finished(), on the GUI thread, if
/// its simulation is still in its document and the job is still the one it wants.
///
/// Threads (U-M4-2): every function of the executor is called on the GUI thread, and every
/// callback is called there. The run runs on a worker thread (the application's executor: the
/// core's SimulationRunner of std::jthreads, whose mailboxes a QTimer of the GUI thread polls) or
/// on the GUI thread itself (the tests' ManualSimulationExecutor). A run touches nothing that the
/// GUI thread uses without a lock the two share: the duplicate it owns, and the preference store,
/// which is thread-safe. Whatever the run wrote happens before the job's finished() (the runner's
/// mailbox orders them): from finished() on, the run and the listeners are the caller's again. No
/// data is sent from a worker in a queued Qt call (QThreadPool, QtConcurrent and QFuture are not
/// used).
///
/// When the callbacks come: only in the executor's own turns of the GUI thread's event loop (the
/// application's executor: its poll, about every 20 ms while a job is pending; the tests': the
/// turn after submit() or cancel(), or when the test says), never from inside submit(), cancel(),
/// cancelAll(), abandon() or abandonAll(). So the caller knows a job's id (the one submit()
/// returned) before any callback of the job comes (a caller that keeps only its current job
/// compares the ids), and its bookkeeping stays as it is while it cancels. A callback may call the
/// executor (submit the next job, cancel or abandon another, or its own). While a callback runs,
/// no other callback of the executor comes (the poll is paused): a callback that shows a modal
/// message (the run dialog's failure, OpenRocket's DetailDialog) sees no other job end during the
/// message's event loop, so the messages come one at a time. So a callback, and whatever it calls
/// synchronously (the document's listeners of a change it fires included), must never wait for a
/// job's callback: a run dialog opened there (SimulationRunDialog::run(), which refuses it: a
/// BugError) or any other wait for runs would wait for good, as no news comes until the callback
/// has returned. Such work is queued to a later turn of the event loop (a zero-time QTimer with a
/// context object); isHandingOver() tells whether a callback runs. A callback that throws is
/// reported through Guard (Guard::run()), and the turn goes on with the next news. After
/// finished(), no callback of the job comes again; nor after the job was abandoned (abandon(),
/// abandonAll()), nor once the executor is being destroyed (its destructor cancels the jobs, waits
/// for the running ones and calls no callback).
///
/// Lifetimes: the executor outlives every window (it is the application's), and a job that is
/// cancelled calls back once its run has returned, a poll or more later. So callbacks that
/// capture an object of a window (a design view's background estimate, a run dialog's batch)
/// come only while that object lives because its jobs are abandoned before it goes: an object
/// whose callbacks capture it abandons its jobs (abandon(job)) in its destructor, and a window
/// abandons the jobs of its own (abandonAll(window)) as it is destroyed, before the objects it owns
/// go. cancel() and cancelAll() are for a run whose caller waits for its finished() (Java's
/// SwingWorker.done() after cancel(true), which the run dialog counts before it closes).
class SimulationExecutor
{
public:
    /// The id of a job, unique among the jobs of one executor; never kNoJob.
    using JobId = std::uint64_t;
    /// The id of no job.
    static constexpr JobId kNoJob = 0;

    /// What the executor tells the caller about a job, on the GUI thread (see the class comment
    /// for when). Either may be empty.
    struct Callbacks
    {
        /// The samples the job's sample listener published since the last call, in the order they
        /// were published (Java: SwingWorker.process(chunks)): called any number of times (none
        /// included) between submit() and finished(), never with no samples and never for a job
        /// without a sample listener; the last ones of a job come right before its finished().
        std::function<void(std::span<const RunSample> samples)> progressed;
        /// The job is over: called exactly once, unless the job is abandoned first (or the
        /// executor destroyed), with the exception that ended it: the one its run carries
        /// (SimulationRun::getException(), outcome EXCEPTION) or one that left
        /// SimulationRun::run() itself (the run's outcome is NOT_RUN then); null otherwise. The
        /// run's outcome tells how it ended.
        std::function<void(const std::exception_ptr& exception)> finished;
    };

    SimulationExecutor()                                     = default;
    virtual ~SimulationExecutor()                            = default;
    SimulationExecutor(const SimulationExecutor&)            = delete;
    SimulationExecutor& operator=(const SimulationExecutor&) = delete;
    SimulationExecutor(SimulationExecutor&&)                 = delete;
    SimulationExecutor& operator=(SimulationExecutor&&)      = delete;

    /// Queues the run @p run, with the sample listener @p sampler (may be null) and then
    /// @p extraListeners, as a job of @p window (the design window it runs for; null for none),
    /// after the jobs queued before it (first in, first out; as many run at once as the executor
    /// has threads), stopped through @p stopSource, and returns the job's id. @p window is only
    /// compared, never used. No callback comes from inside submit().
    /// @throws BugError when @p run is null or has run, when @p stopSource has no stop state, or
    ///         when an extra listener is null; and the core runner refuses a run or a listener
    ///         (the sample listener, an extra one) that is a live job's too: a listener belongs to
    ///         one job from its submit() until its end has been reported (an abandoned job's until
    ///         its run has ended and the executor has seen it), so every job gets listeners of
    ///         its own (a stateless system listener included: never a shared instance())
    [[nodiscard]] virtual JobId submit(
        const QObject* window, std::shared_ptr<SimulationRun> run,
        std::shared_ptr<RunSampleListener>               sampler,
        std::vector<std::shared_ptr<SimulationListener>> extraListeners,
        std::stop_source stopSource, Callbacks callbacks) = 0;

    /// Cancels the job @p job: a job that has not started never runs (its run ends CANCELLED at
    /// once), a running one gets a stop requested through its stop source (its run ends
    /// CANCELLED after its current step, or as it ends anyway when the request comes too late);
    /// either way its finished() comes in a later turn. Nothing for a job that is over, abandoned
    /// or unknown, or kNoJob.
    virtual void cancel(JobId job) = 0;

    /// cancel() of every pending job of @p window, in the order they were submitted.
    virtual void cancelAll(const QObject* window) = 0;

    /// Forgets the job @p job: cancels it as cancel() does, but without a callback. Its callbacks
    /// are destroyed at once, on the GUI thread, and none of them is called after abandon()
    /// returns (nor from inside it; abandon() may be called from inside one of them). A running
    /// job's run still runs to its end on its thread, with a stop requested, which the executor
    /// waits for as for any job's, and whatever the job reports is dropped. From then on the job
    /// is not pending. Nothing for a job that is over, abandoned or unknown, or kNoJob. For an
    /// object whose callbacks capture it, in its destructor (see the class comment).
    virtual void abandon(JobId job) = 0;

    /// abandon() of every pending job of @p window: as the window is destroyed, before the objects
    /// it owns go (see the class comment).
    virtual void abandonAll(const QObject* window) = 0;

    /// Whether the job @p job is pending: submitted, its finished() not called yet, and not
    /// abandoned.
    [[nodiscard]] virtual bool isPending(JobId job) const = 0;

    /// Whether a callback of the executor runs now (its news is being handed over: no other comes
    /// meanwhile). Nothing that waits for a job's callback may be done then (see the class
    /// comment).
    [[nodiscard]] virtual bool isHandingOver() const = 0;
};

}  // namespace QtRocket::Gui
