#include "QtRocket/gui/util/PollingSimulationExecutor.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

#include <QScopeGuard>
#include <QTimer>

#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/SimulationExecutor.h"
#include "QtRocket/simulation/RunSample.h"
#include "QtRocket/simulation/SimulationRunner.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// @p interval, which must not be negative.
[[nodiscard]] std::chrono::milliseconds checkedInterval(std::chrono::milliseconds interval)
{
    if (interval.count() < 0)
    {
        bug("A simulation executor's poll interval is not negative");
    }
    return interval;
}

}  // namespace

PollingSimulationExecutor::PollingSimulationExecutor()
  : PollingSimulationExecutor(SimulationRunner::defaultThreadCount())
{
}

PollingSimulationExecutor::PollingSimulationExecutor(std::size_t               threadCount,
                                                     std::chrono::milliseconds pollInterval)
  : m_runner(threadCount)
{
    m_timer.setInterval(checkedInterval(pollInterval));
    // The poll runs on the GUI thread, the timer's.
    QObject::connect(&m_timer, &QTimer::timeout, &m_timer, [this] { poll(); });
}

PollingSimulationExecutor::~PollingSimulationExecutor()
{
    // No callback from here on: the timer stops, and the callbacks go with m_jobs once the
    // runner (declared last) has cancelled its jobs and joined its threads.
    m_timer.stop();
}

SimulationExecutor::JobId PollingSimulationExecutor::submit(
    const QObject* window, std::shared_ptr<SimulationRun> run,
    std::shared_ptr<RunSampleListener>               sampler,
    std::vector<std::shared_ptr<SimulationListener>> extraListeners, std::stop_source stopSource,
    Callbacks callbacks)
{
    // The runner checks the run, the listeners and the stop source, and throws before it queues
    // anything.
    const SimulationRunner::JobId runnerJob =
        m_runner.submit(std::move(run), SimulationRunner::GroupId{}, std::move(sampler),
                        std::move(extraListeners), std::move(stopSource));
    const JobId id = ++m_lastJob;
    m_jobs.push_back({.id        = id,
                      .window    = window,
                      .runnerJob = runnerJob,
                      .callbacks = std::move(callbacks),
                      .abandoned = false});
    // Never a callback from here: the first comes at the poll.
    startPolling();
    return id;
}

void PollingSimulationExecutor::cancel(JobId job)
{
    const Job* const found = findJob(job);
    if (found == nullptr || found->abandoned)
    {
        return;
    }
    // A queued job ends CANCELLED at once, a running one after its step; the poll reports it.
    m_runner.cancel(found->runnerJob);
    startPolling();
}

void PollingSimulationExecutor::cancelAll(const QObject* window)
{
    for (const JobId job : pendingJobsOf(window))
    {
        cancel(job);
    }
}

void PollingSimulationExecutor::abandon(JobId job)
{
    Job* const found = findJob(job);
    if (found == nullptr || found->abandoned)
    {
        return;
    }
    found->abandoned = true;
    // The callback that may be running is a copy (poll()).
    found->callbacks = {};
    // The run ends at its next step; the poll drops its news and forgets the job then.
    m_runner.cancel(found->runnerJob);
    startPolling();
}

void PollingSimulationExecutor::abandonAll(const QObject* window)
{
    for (const JobId job : pendingJobsOf(window))
    {
        abandon(job);
    }
}

bool PollingSimulationExecutor::isPending(JobId job) const
{
    const Job* const found = findJob(job);
    return found != nullptr && !found->abandoned;
}

void PollingSimulationExecutor::poll()
{
    // Paused while the news is handed over: a callback's event loop (a modal message) gets no
    // other callback of the executor. Whatever happens, the poll goes on afterwards while a job
    // is pending.
    m_timer.stop();
    m_handingOver                                  = true;
    const auto                                over = qScopeGuard([this] {
        m_handingOver = false;
        if (m_runner.isBusy())
        {
            m_timer.start();
        }
    });
    const std::vector<SimulationRunner::News> news = m_runner.poll();
    for (const SimulationRunner::News& item : news)
    {
        handOver(item);
    }
}

void PollingSimulationExecutor::handOver(const SimulationRunner::News& item)
{
    // Looked up again before each callback: one may have abandoned the job (or another), or
    // submitted one (which moves the jobs).
    if (const Job* const job = findRunnerJob(item.job);
        job != nullptr && !job->abandoned && !item.samples.empty())
    {
        // A copy: the callback may abandon its job, which destroys the job's callbacks.
        const std::function<void(std::span<const RunSample>)> progressed =
            job->callbacks.progressed;
        if (progressed)
        {
            Guard::run([&progressed, &item] { progressed(item.samples); });
        }
    }
    if (item.state != SimulationRunner::JobState::ENDED)
    {
        return;
    }
    Job* const job = findRunnerJob(item.job);
    if (job == nullptr)
    {
        return;
    }
    // No longer pending once its finished() is called.
    const bool                                           abandoned = job->abandoned;
    const std::function<void(const std::exception_ptr&)> finished =
        std::move(job->callbacks.finished);
    eraseJob(job->id);
    if (!abandoned && finished)
    {
        Guard::run([&finished, &item] { finished(item.exception); });
    }
}

void PollingSimulationExecutor::startPolling()
{
    if (!m_handingOver && !m_timer.isActive())
    {
        m_timer.start();
    }
}

PollingSimulationExecutor::Job* PollingSimulationExecutor::findJob(JobId job)
{
    const auto found = std::ranges::find(m_jobs, job, &Job::id);
    return found == m_jobs.end() || job == kNoJob ? nullptr : &*found;
}

const PollingSimulationExecutor::Job* PollingSimulationExecutor::findJob(JobId job) const
{
    const auto found = std::ranges::find(m_jobs, job, &Job::id);
    return found == m_jobs.end() || job == kNoJob ? nullptr : &*found;
}

PollingSimulationExecutor::Job* PollingSimulationExecutor::findRunnerJob(
    SimulationRunner::JobId runnerJob)
{
    const auto found = std::ranges::find(m_jobs, runnerJob, &Job::runnerJob);
    return found == m_jobs.end() ? nullptr : &*found;
}

void PollingSimulationExecutor::eraseJob(JobId job)
{
    std::erase_if(m_jobs, [job](const Job& candidate) { return candidate.id == job; });
}

std::vector<SimulationExecutor::JobId> PollingSimulationExecutor::pendingJobsOf(
    const QObject* window) const
{
    std::vector<JobId> jobs;
    for (const Job& job : m_jobs)
    {
        if (job.window == window && !job.abandoned)
        {
            jobs.push_back(job.id);
        }
    }
    return jobs;
}

}  // namespace QtRocket::Gui
