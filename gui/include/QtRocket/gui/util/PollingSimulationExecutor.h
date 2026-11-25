#pragma once

#include <chrono>
#include <cstddef>
#include <memory>
#include <stop_token>
#include <vector>

#include <QObject>
#include <QTimer>

#include "QtRocket/gui/util/SimulationExecutor.h"
#include "QtRocket/simulation/SimulationRunner.h"

namespace QtRocket
{
class RunSampleListener;
class SimulationListener;
class SimulationRun;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// The application's SimulationExecutor: an adapter of the core's SimulationRunner (its pool of
/// std::jthreads, max(1, hardware_concurrency()) by default: Java's availableProcessors()) to the
/// GUI thread, as OpenRocket's run dialog and design view use their thread pools
/// (SimulationRunDialog.java:74-89, SwingWorker). Every job goes to the runner (its group is not
/// used: the jobs of a window are cancelled one by one, in their order), and a QTimer of the GUI
/// thread polls the runner's mailboxes, every pollInterval() while a job is pending (the runner
/// is busy) and not at all when none is: each news of a job is handed to its callbacks there
/// (its samples to progressed(), its end to finished()), in the order the runner reports them.
/// Nothing is sent from the runner's threads to the GUI thread but through those mailboxes, which
/// the GUI thread reads under their mutexes (U-M4-2: no queued call carries data; ThreadSanitizer
/// sees every lock).
///
/// The poll is paused while it hands the news over: the timer is stopped before the first
/// callback of a poll and started again after the last (when a job is still pending), so a
/// callback that runs an event loop (a modal message) sees no other callback of the executor
/// until it returns; a job submitted from inside a callback is polled once the poll is over.
/// So a callback, and whatever it calls (a document's listeners included), never waits for a
/// job's callback (a run dialog's modal loop: it would wait for good); such work is queued to
/// a later turn of the event loop (isHandingOver() tells, SimulationExecutor's class comment).
/// A callback that throws is reported through Guard, and the poll goes on with the next news.
///
/// The destructor calls no callback: it drops the callbacks, and the runner's destructor
/// requests a stop of the running jobs, drops the queued ones and joins its threads. So the
/// executor belongs to the thread that made it (the runner's thread contract), and what a job's
/// run refers to (the preference store of its simulation; what an extra listener refers to) must
/// outlive it: app/ApplicationContext declares it after the preferences and the databases.
///
/// Not copyable and not movable (SimulationExecutor).
class PollingSimulationExecutor final : public SimulationExecutor
{
public:
    /// The time between two polls while a job is pending (O-M4-2: about 20 ms).
    static constexpr std::chrono::milliseconds kPollInterval{20};

    /// An executor of SimulationRunner::defaultThreadCount() threads, polled every kPollInterval.
    PollingSimulationExecutor();

    /// An executor of at most @p threadCount threads, polled every @p pollInterval (a test's).
    /// @throws BugError when @p threadCount is 0 or @p pollInterval is negative
    explicit PollingSimulationExecutor(std::size_t               threadCount,
                                       std::chrono::milliseconds pollInterval = kPollInterval);

    ~PollingSimulationExecutor() override;
    PollingSimulationExecutor(const PollingSimulationExecutor&)            = delete;
    PollingSimulationExecutor& operator=(const PollingSimulationExecutor&) = delete;
    PollingSimulationExecutor(PollingSimulationExecutor&&)                 = delete;
    PollingSimulationExecutor& operator=(PollingSimulationExecutor&&)      = delete;

    [[nodiscard]] JobId submit(const QObject* window, std::shared_ptr<SimulationRun> run,
                               std::shared_ptr<RunSampleListener>               sampler,
                               std::vector<std::shared_ptr<SimulationListener>> extraListeners,
                               std::stop_source stopSource, Callbacks callbacks) override;
    void                cancel(JobId job) override;
    void                cancelAll(const QObject* window) override;
    void                abandon(JobId job) override;
    void                abandonAll(const QObject* window) override;
    [[nodiscard]] bool  isPending(JobId job) const override;
    /// Whether the news is being handed over (the poll is paused: a callback runs).
    [[nodiscard]] bool isHandingOver() const override { return m_handingOver; }

    /// The most threads the jobs run on.
    [[nodiscard]] std::size_t threadCount() const noexcept { return m_runner.getThreadCount(); }

    /// The time between two polls.
    [[nodiscard]] std::chrono::milliseconds pollInterval() const noexcept
    {
        return m_timer.intervalAsDuration();
    }

    /// Whether the poll's timer runs: a job is pending, and the executor is not handing news over
    /// (the poll is paused while a callback runs).
    [[nodiscard]] bool isPolling() const noexcept { return m_timer.isActive(); }

private:
    /// A job until the runner reported its end.
    struct Job
    {
        JobId                   id{kNoJob};
        const QObject*          window{nullptr};
        SimulationRunner::JobId runnerJob;
        Callbacks               callbacks;
        /// Abandoned: its callbacks are gone, and its news is dropped.
        bool abandoned{false};
    };

    /// The timer's slot: hands the runner's news over (see the class comment).
    void poll();
    /// Hands @p item, a news of the runner, to its job's callbacks.
    void handOver(const SimulationRunner::News& item);
    /// Starts the timer, unless it runs or the news is being handed over (the poll then starts it
    /// when it is done).
    void startPolling();
    /// The job @p job of the executor or of the runner (@p runnerJob), or null.
    [[nodiscard]] Job*       findJob(JobId job);
    [[nodiscard]] const Job* findJob(JobId job) const;
    [[nodiscard]] Job*       findRunnerJob(SimulationRunner::JobId runnerJob);
    /// Removes the job @p job, whose end the runner reported.
    void eraseJob(JobId job);
    /// The ids of the pending jobs of @p window, in the order they were submitted.
    [[nodiscard]] std::vector<JobId> pendingJobsOf(const QObject* window) const;

    JobId m_lastJob{kNoJob};
    /// The jobs whose end the runner has not reported, in the order they were submitted.
    std::vector<Job> m_jobs;
    /// Whether the news is being handed over (poll() is running).
    bool   m_handingOver{false};
    QTimer m_timer;
    /// Declared last, so destroyed first: its destructor joins the threads before the callbacks
    /// and the timer go.
    SimulationRunner m_runner;
};

}  // namespace QtRocket::Gui
