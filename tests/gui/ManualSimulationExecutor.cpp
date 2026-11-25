#include "ManualSimulationExecutor.h"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

#include <QObject>
#include <QTimer>

#include <gtest/gtest.h>

#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/SimulationExecutor.h"
#include "QtRocket/simulation/RunSample.h"
#include "QtRocket/simulation/SimulationRun.h"
#include "QtRocket/simulation/listeners/RunSampleListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui::Test
{

namespace
{

/// Puts a job on the executor's running jobs while it lives, and takes it off when it goes (also
/// when an exception passes) or when leave() is called.
template <class Job>
class RunningScope
{
public:
    RunningScope(std::vector<Job*>& running, Job& job) : m_running(&running)
    {
        running.push_back(&job);
    }
    ~RunningScope() { leave(); }
    RunningScope(const RunningScope&)            = delete;
    RunningScope& operator=(const RunningScope&) = delete;
    RunningScope(RunningScope&&)                 = delete;
    RunningScope& operator=(RunningScope&&)      = delete;

    /// Takes the job off now (once): the innermost one, as the scopes nest.
    void leave() noexcept
    {
        if (m_running != nullptr)
        {
            m_running->pop_back();
            m_running = nullptr;
        }
    }

private:
    std::vector<Job*>* m_running;
};

/// Counts a callback of the executor while it lives (also when an exception passes).
class DeliveringScope
{
public:
    explicit DeliveringScope(int& delivering) noexcept : m_delivering(&delivering) { ++delivering; }
    ~DeliveringScope() { --*m_delivering; }
    DeliveringScope(const DeliveringScope&)            = delete;
    DeliveringScope& operator=(const DeliveringScope&) = delete;
    DeliveringScope(DeliveringScope&&)                 = delete;
    DeliveringScope& operator=(DeliveringScope&&)      = delete;

private:
    int* m_delivering;
};

/// The listeners a job's run runs with: its sample listener (when it has one), then its extra
/// listeners, as the core runner passes them.
template <class Job>
[[nodiscard]] std::vector<std::shared_ptr<SimulationListener>> listenersOf(const Job& job)
{
    std::vector<std::shared_ptr<SimulationListener>> listeners;
    listeners.reserve(job.extraListeners.size() + 1);
    if (job.sampler != nullptr)
    {
        listeners.push_back(job.sampler);
    }
    listeners.insert(listeners.end(), job.extraListeners.begin(), job.extraListeners.end());
    return listeners;
}

/// Runs @p run with @p stop and @p listeners, and returns the exception that ended it: the one
/// that left SimulationRun::run() itself, or the one the run carries (as the core runner's worker
/// and its news give it).
[[nodiscard]] std::exception_ptr runAndCatch(
    SimulationRun& run, const std::stop_token& stop,
    const std::vector<std::shared_ptr<SimulationListener>>& listeners)
{
    try
    {
        run.run(stop, listeners);
    }
    catch (...)
    {
        return std::current_exception();
    }
    return run.getException();
}

}  // namespace

ManualSimulationExecutor::~ManualSimulationExecutor() = default;

SimulationExecutor::JobId ManualSimulationExecutor::submit(
    const QObject* window, std::shared_ptr<SimulationRun> run,
    std::shared_ptr<RunSampleListener>               sampler,
    std::vector<std::shared_ptr<SimulationListener>> extraListeners, std::stop_source stopSource,
    Callbacks callbacks)
{
    checkSubmission(run.get(), sampler.get(), stopSource, extraListeners);
    const JobId id = ++m_lastJob;
    m_submitted.push_back({.id = id, .window = window});
    Job job;
    job.id             = id;
    job.window         = window;
    job.run            = std::move(run);
    job.sampler        = std::move(sampler);
    job.extraListeners = std::move(extraListeners);
    job.stop           = std::move(stopSource);
    job.callbacks      = std::move(callbacks);
    job.immediate      = m_mode == Mode::IMMEDIATE;
    m_waiting.push_back(std::move(job));
    if (m_mode == Mode::IMMEDIATE)
    {
        // Never inside submit(): the caller has the id before any callback.
        postTurn();
    }
    return id;
}

void ManualSimulationExecutor::cancel(JobId job)
{
    if (Job* const running = findRunning(job); running != nullptr)
    {
        // Its run sees the stop after its step; finished() once it has returned.
        record(m_cancelled, job);
        requestStop(running->stop);
        return;
    }
    Job* const waiting = findWaiting(job);
    if (waiting == nullptr || waiting->ended)
    {
        return;
    }
    record(m_cancelled, job);
    // As the core runner ends a queued job: its run is run now, with a stop that is requested
    // (not through its stop source, which other jobs may share), so nothing of it runs; the
    // report comes in the next turn, whatever the mode.
    std::stop_source stopped;
    requestStop(stopped);
    waiting->exception = runAndCatch(*waiting->run, stopped.get_token(), listenersOf(*waiting));
    waiting->ended     = true;
    postTurn();
}

void ManualSimulationExecutor::cancelAll(const QObject* window)
{
    for (const JobId job : pendingJobsOf(window))
    {
        cancel(job);
    }
}

void ManualSimulationExecutor::abandon(JobId job)
{
    if (Job* const running = findRunning(job); running != nullptr)
    {
        // Its run runs to its end with the stop requested; run() calls nothing after it. The
        // callback that may be running is a copy (report(), reportProgress()).
        record(m_abandoned, job);
        running->abandoned = true;
        running->callbacks = {};
        requestStop(running->stop);
        return;
    }
    const auto waiting = std::ranges::find(m_waiting, job, &Job::id);
    if (waiting == m_waiting.end())
    {
        return;
    }
    // Its callbacks go with it, here; its run never runs. Its listeners are a live job's until the
    // next turn, as the core runner keeps an abandoned queued job until its next poll.
    m_abandonedListeners.insert(m_abandonedListeners.end(), waiting->extraListeners.begin(),
                                waiting->extraListeners.end());
    if (waiting->sampler != nullptr)
    {
        m_abandonedListeners.push_back(waiting->sampler);
    }
    m_waiting.erase(waiting);
    record(m_abandoned, job);
    postTurn();
}

void ManualSimulationExecutor::abandonAll(const QObject* window)
{
    for (const JobId job : pendingJobsOf(window))
    {
        abandon(job);
    }
}

bool ManualSimulationExecutor::isPending(JobId job) const
{
    return std::ranges::any_of(
               m_running,
               [job](const Job* running) { return running->id == job && !running->abandoned; }) ||
           std::ranges::find(m_waiting, job, &Job::id) != m_waiting.end();
}

bool ManualSimulationExecutor::runNext()
{
    if (m_mode == Mode::NEVER)
    {
        ADD_FAILURE() << "the manual simulation executor runs no job in its NEVER mode";
        return false;
    }
    if (m_delivering == 0)
    {
        releaseAbandonedListeners();
    }
    if (m_waiting.empty())
    {
        return false;
    }
    Job job = std::move(m_waiting.front());
    m_waiting.pop_front();
    run(std::move(job));
    return true;
}

std::size_t ManualSimulationExecutor::runAll()
{
    std::size_t count = 0;
    while (runNext())
    {
        ++count;
    }
    return count;
}

bool ManualSimulationExecutor::reportProgress(JobId job)
{
    const Job* pending = findRunning(job);
    if (pending == nullptr)
    {
        pending = findWaiting(job);
    }
    if (pending == nullptr)
    {
        return false;
    }
    if (pending->sampler == nullptr)
    {
        return true;
    }
    const std::vector<RunSample> samples = pending->sampler->takeSamples();
    // A copy: the callback may change the queue or abandon the job.
    const std::function<void(std::span<const RunSample>)> progressed =
        pending->callbacks.progressed;
    if (progressed && !samples.empty())
    {
        deliver([&progressed, &samples] { progressed(samples); });
    }
    return true;
}

std::vector<SimulationExecutor::JobId> ManualSimulationExecutor::waitingJobs() const
{
    std::vector<JobId> jobs;
    jobs.reserve(m_waiting.size());
    for (const Job& waiting : m_waiting)
    {
        jobs.push_back(waiting.id);
    }
    return jobs;
}

SimulationExecutor::JobId ManualSimulationExecutor::runningJob() const noexcept
{
    return m_running.empty() ? kNoJob : m_running.back()->id;
}

void ManualSimulationExecutor::run(Job job)
{
    if (!job.ended)
    {
        const RunningScope running(m_running, job);
        job.exception = runAndCatch(*job.run, job.stop.get_token(), listenersOf(job));
        job.ended     = true;
    }
    report(job);
}

void ManualSimulationExecutor::report(Job& job)
{
    if (!job.abandoned && job.sampler != nullptr)
    {
        const std::vector<RunSample> samples = job.sampler->takeSamples();
        // A copy: the callback may abandon its job, which destroys the job's callbacks.
        const std::function<void(std::span<const RunSample>)> progressed = job.callbacks.progressed;
        if (progressed && !samples.empty())
        {
            // The job is still running for whoever asks meanwhile: it is pending until finished().
            const RunningScope running(m_running, job);
            deliver([&progressed, &samples] { progressed(samples); });
        }
    }
    if (job.abandoned)
    {
        return;
    }
    const std::function<void(const std::exception_ptr&)> finished =
        std::move(job.callbacks.finished);
    job.callbacks = {};
    if (finished)
    {
        deliver([&finished, &job] { finished(job.exception); });
    }
}

template <class Callback>
void ManualSimulationExecutor::deliver(const Callback& callback)
{
    {
        const DeliveringScope delivering(m_delivering);
        Guard::run(callback);
    }
    if (m_delivering == 0 && m_turnDeferred)
    {
        m_turnDeferred = false;
        postTurn();
    }
}

void ManualSimulationExecutor::postTurn()
{
    if (m_turnPosted)
    {
        return;
    }
    m_turnPosted = true;
    // A queued call of the GUI thread to itself, which goes with m_turn.
    QTimer::singleShot(0, &m_turn, [this] {
        m_turnPosted = false;
        runTurn();
    });
}

void ManualSimulationExecutor::runTurn()
{
    if (m_delivering > 0)
    {
        // A callback runs (its message's event loop is this one): the turn waits for it.
        m_turnDeferred = true;
        return;
    }
    releaseAbandonedListeners();
    // One at a time, oldest first: a callback may submit, cancel or abandon jobs meanwhile.
    const auto due = [](const Job& job) { return job.ended || job.immediate; };
    for (auto found = std::ranges::find_if(m_waiting, due); found != m_waiting.end();
         found      = std::ranges::find_if(m_waiting, due))
    {
        Job job = std::move(*found);
        m_waiting.erase(found);
        run(std::move(job));
    }
}

ManualSimulationExecutor::Job* ManualSimulationExecutor::findWaiting(JobId job)
{
    const auto waiting = std::ranges::find(m_waiting, job, &Job::id);
    return waiting == m_waiting.end() ? nullptr : &*waiting;
}

ManualSimulationExecutor::Job* ManualSimulationExecutor::findRunning(JobId job)
{
    const auto running = std::ranges::find_if(m_running, [job](const Job* candidate) {
        return candidate->id == job && !candidate->abandoned;
    });
    return running == m_running.end() ? nullptr : *running;
}

std::vector<SimulationExecutor::JobId> ManualSimulationExecutor::pendingJobsOf(
    const QObject* window) const
{
    std::vector<JobId> jobs;
    for (const Job* const running : m_running)
    {
        if (running->window == window && !running->abandoned)
        {
            jobs.push_back(running->id);
        }
    }
    for (const Job& waiting : m_waiting)
    {
        if (waiting.window == window)
        {
            jobs.push_back(waiting.id);
        }
    }
    // In the order they were submitted: their ids grow.
    std::ranges::sort(jobs);
    return jobs;
}

void ManualSimulationExecutor::checkSubmission(
    const SimulationRun* run, const RunSampleListener* sampler, const std::stop_source& stopSource,
    const std::vector<std::shared_ptr<SimulationListener>>& extraListeners) const
{
    if (run == nullptr)
    {
        bug("A simulation job needs a run");
    }
    const auto isTheRun = [run](const Job* job) { return job->run.get() == run; };
    if (std::ranges::any_of(m_running, isTheRun) ||
        std::ranges::any_of(m_waiting, [&isTheRun](const Job& job) { return isTheRun(&job); }))
    {
        bug("A simulation run is the run of one job");
    }
    if (run->getOutcome() != SimulationRun::Outcome::NOT_RUN)
    {
        bug("A simulation job's run has not run");
    }
    if (!stopSource.stop_possible())
    {
        bug("A simulation job needs a stop source with a stop state");
    }
    if (std::ranges::any_of(extraListeners,
                            [](const std::shared_ptr<SimulationListener>& listener) {
                                return listener == nullptr;
                            }))
    {
        bug("A simulation job's extra listener is not null");
    }
    // As the core runner: a listener of a live job is that job's.
    if (sampler != nullptr && isListenerOfALiveJob(*sampler))
    {
        bug("A simulation job's sample listener is the listener of one job");
    }
    if (std::ranges::any_of(extraListeners,
                            [this](const std::shared_ptr<SimulationListener>& listener) {
                                return isListenerOfALiveJob(*listener);
                            }))
    {
        bug("A simulation job's extra listener is the listener of one job");
    }
}

bool ManualSimulationExecutor::isListenerOfALiveJob(const SimulationListener& listener) const
{
    const auto isIt = [&listener](const std::shared_ptr<SimulationListener>& own) {
        return own.get() == &listener;
    };
    const auto hasIt = [&isIt, &listener](const Job& job) {
        return job.sampler.get() == &listener || std::ranges::any_of(job.extraListeners, isIt);
    };
    // A running job whose run has ended is reporting its end: the core runner has forgotten it
    // by then (its poll took the end). A waiting job is live, also one cancelled whose report
    // waits.
    return std::ranges::any_of(m_running,
                               [&hasIt](const Job* job) { return !job->ended && hasIt(*job); }) ||
           std::ranges::any_of(m_waiting, hasIt) || std::ranges::any_of(m_abandonedListeners, isIt);
}

void ManualSimulationExecutor::releaseAbandonedListeners() noexcept
{
    m_abandonedListeners.clear();
}

void ManualSimulationExecutor::requestStop(std::stop_source& source) noexcept
{
    // Through a stop_source& and not a const one: std::stop_source::request_stop() is not const
    // in the standard (libstdc++ declares it const, libc++ and MSVC's library do not).
    static_cast<void>(source.request_stop());
}

void ManualSimulationExecutor::record(std::vector<JobId>& jobs, JobId job)
{
    if (std::ranges::find(jobs, job) == jobs.end())
    {
        jobs.push_back(job);
    }
}

}  // namespace QtRocket::Gui::Test
