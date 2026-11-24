#include "ManualSimulationExecutor.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

#include <QObject>
#include <QTimer>

#include <gtest/gtest.h>

#include "QtRocket/gui/util/SimulationExecutor.h"

namespace QtRocket::Gui::Test
{

namespace
{

/// Puts a job on the executor's running jobs while it lives, and takes it off when it goes (also
/// when a test's work throws, which it must not) or when leave() is called.
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

/// Whether one of @p jobs is @p job (std::ranges::find: Apple's libc++ lacks
/// std::ranges::contains).
template <class Jobs>
[[nodiscard]] bool findIn(const Jobs& jobs, SimulationExecutor::JobId job)
{
    return std::ranges::find_if(
               jobs, [job](const auto& candidate) { return candidate.id == job; }) != jobs.end();
}

}  // namespace

SimulationExecutor::JobId ManualSimulationExecutor::submit(const QObject* window, Work work,
                                                           Callbacks callbacks)
{
    const JobId id = ++m_lastJob;
    m_submitted.push_back({.id = id, .window = window});
    Job job;
    job.id        = id;
    job.window    = window;
    job.work      = std::move(work);
    job.callbacks = std::move(callbacks);
    job.immediate = m_mode == Mode::IMMEDIATE;
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
        // Its work sees the stop through its token; finished(RAN) once it has returned.
        record(m_cancelled, job);
        requestStop(*running);
        return;
    }
    const auto waiting = std::ranges::find(m_waiting, job, &Job::id);
    if (waiting == m_waiting.end())
    {
        return;
    }
    const Job dropped = std::move(*waiting);
    m_waiting.erase(waiting);
    record(m_cancelled, job);
    if (dropped.callbacks.finished)
    {
        dropped.callbacks.finished(Ending::DROPPED);
    }
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
        // Its work runs to its end with the stop requested; run() calls nothing after it. The
        // callback that may be running is a copy (run(), reportProgress()).
        record(m_abandoned, job);
        running->abandoned = true;
        running->callbacks = {};
        requestStop(*running);
        return;
    }
    const auto waiting = std::ranges::find(m_waiting, job, &Job::id);
    if (waiting == m_waiting.end())
    {
        return;
    }
    // Its callbacks go with it, here.
    m_waiting.erase(waiting);
    record(m_abandoned, job);
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
           findIn(m_waiting, job);
}

bool ManualSimulationExecutor::runNext()
{
    if (m_mode == Mode::NEVER)
    {
        ADD_FAILURE() << "the manual simulation executor runs no job in its NEVER mode";
        return false;
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
    // A copy: the callback may change the queue.
    const std::function<void()> progressed = pending->callbacks.progressed;
    if (progressed)
    {
        progressed();
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
    RunningScope running(m_running, job);
    if (job.work)
    {
        job.work(job.stop.get_token());
    }
    if (!job.abandoned)
    {
        // A copy: the callback may abandon its job, which destroys the job's callbacks.
        const std::function<void()> progressed = job.callbacks.progressed;
        if (progressed)
        {
            progressed();
        }
    }
    // No longer pending once finished() is called.
    running.leave();
    if (!job.abandoned)
    {
        const std::function<void(Ending)> finished = std::move(job.callbacks.finished);
        job.callbacks                              = {};
        if (finished)
        {
            finished(Ending::RAN);
        }
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
        runImmediateJobs();
    });
}

void ManualSimulationExecutor::runImmediateJobs()
{
    // One at a time, oldest first: a callback may submit, cancel or abandon jobs meanwhile.
    for (auto found = std::ranges::find(m_waiting, true, &Job::immediate); found != m_waiting.end();
         found      = std::ranges::find(m_waiting, true, &Job::immediate))
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

void ManualSimulationExecutor::requestStop(Job& job) noexcept
{
    // Through a Job& and not a const one: std::stop_source::request_stop() is not const in the
    // standard (libstdc++ declares it const, libc++ and MSVC's library do not).
    static_cast<void>(job.stop.request_stop());
}

void ManualSimulationExecutor::record(std::vector<JobId>& jobs, JobId job)
{
    if (std::ranges::find(jobs, job) == jobs.end())
    {
        jobs.push_back(job);
    }
}

}  // namespace QtRocket::Gui::Test
