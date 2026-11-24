#include "ManualSimulationExecutor.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

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
    if (m_mode == Mode::IMMEDIATE)
    {
        run(std::move(job));
    }
    else
    {
        m_waiting.push_back(std::move(job));
    }
    return id;
}

void ManualSimulationExecutor::cancel(JobId job)
{
    if (Job* const running = findRunning(job); running != nullptr)
    {
        // Its work sees the stop through its token; finished(RAN) once it has returned.
        recordCancel(job);
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
    recordCancel(job);
    if (dropped.callbacks.finished)
    {
        dropped.callbacks.finished(Ending::DROPPED);
    }
}

void ManualSimulationExecutor::cancelAll(const QObject* window)
{
    std::vector<JobId> jobs;
    for (const Job* const running : m_running)
    {
        if (running->window == window)
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
    for (const JobId job : jobs)
    {
        cancel(job);
    }
}

bool ManualSimulationExecutor::isPending(JobId job) const
{
    return std::ranges::any_of(m_running,
                               [job](const Job* running) { return running->id == job; }) ||
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
    if (job.callbacks.progressed)
    {
        job.callbacks.progressed();
    }
    // No longer pending once finished() is called.
    running.leave();
    if (job.callbacks.finished)
    {
        job.callbacks.finished(Ending::RAN);
    }
}

ManualSimulationExecutor::Job* ManualSimulationExecutor::findWaiting(JobId job)
{
    const auto waiting = std::ranges::find(m_waiting, job, &Job::id);
    return waiting == m_waiting.end() ? nullptr : &*waiting;
}

ManualSimulationExecutor::Job* ManualSimulationExecutor::findRunning(JobId job)
{
    const auto running = std::ranges::find_if(
        m_running, [job](const Job* candidate) { return candidate->id == job; });
    return running == m_running.end() ? nullptr : *running;
}

void ManualSimulationExecutor::requestStop(Job& job) noexcept
{
    // Through a Job& and not a const one: std::stop_source::request_stop() is not const in the
    // standard (libstdc++ declares it const, libc++ and MSVC's library do not).
    static_cast<void>(job.stop.request_stop());
}

void ManualSimulationExecutor::recordCancel(JobId job)
{
    if (std::ranges::find(m_cancelled, job) == m_cancelled.end())
    {
        m_cancelled.push_back(job);
    }
}

}  // namespace QtRocket::Gui::Test
