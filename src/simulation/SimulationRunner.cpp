#include "QtRocket/simulation/SimulationRunner.h"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "QtRocket/simulation/RunSample.h"
#include "QtRocket/simulation/SimulationRun.h"
#include "QtRocket/simulation/listeners/RunSampleListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket
{

/// A job and its mailbox.
struct SimulationRunner::Job
{
    // Set by submit() before the job is queued and never changed: the queue's mutex hands them
    // to the thread that runs the job.
    JobId                          id;
    GroupId                        group;
    std::shared_ptr<SimulationRun> run;
    /// The sample listener, or null.
    std::shared_ptr<RunSampleListener> sampler;
    /// The sample listener (if any), then the extra listeners.
    std::vector<std::shared_ptr<SimulationListener>> listeners;
    std::stop_source                                 stopSource;
    std::stop_token                                  stopToken;

    // The mailbox, under the mutex.
    std::mutex         mutex;
    JobState           state{JobState::QUEUED};
    std::exception_ptr exception;

    // The owning thread's.
    /// Whether a news has told that the job runs (or has ended).
    bool startReported{false};
};

std::size_t SimulationRunner::defaultThreadCount() noexcept
{
    return std::max<std::size_t>(1, std::thread::hardware_concurrency());
}

SimulationRunner::SimulationRunner() : SimulationRunner(defaultThreadCount()) { }

SimulationRunner::SimulationRunner(std::size_t threadCount)
  : m_owner(std::this_thread::get_id()), m_threadCount(threadCount)
{
    if (threadCount == 0)
    {
        bug("A simulation runner needs a thread");
    }
}

SimulationRunner::~SimulationRunner()
{
    {
        // No thread takes a job from now on: the queued ones are dropped unrun.
        const std::scoped_lock lock(m_queueMutex);
        m_shuttingDown = true;
        m_queue.clear();
    }
    m_queueChanged.notify_all();
    for (const std::unique_ptr<Job>& job : m_jobs)
    {
        requestStop(*job);
    }
    // Each thread ends the job it runs (after the step it takes) and returns.
    m_threads.clear();
}

SimulationRunner::GroupId SimulationRunner::newGroup()
{
    checkThread();
    m_lastGroup++;
    return GroupId{.value = m_lastGroup};
}

SimulationRunner::JobId SimulationRunner::submit(
    std::shared_ptr<SimulationRun> run, GroupId group, std::shared_ptr<RunSampleListener> sampler,
    std::vector<std::shared_ptr<SimulationListener>> extraListeners, std::stop_source stopSource)
{
    checkThread();
    if (run == nullptr)
    {
        bug("A job of a simulation runner needs a run");
    }
    // Before the outcome is read: the run of a live job is its worker's, which may be writing
    // the outcome (see the thread contract).
    if (std::ranges::any_of(m_jobs,
                            [&run](const std::unique_ptr<Job>& job) { return job->run == run; }))
    {
        bug("A simulation run is the run of one job");
    }
    if (run->getOutcome() != SimulationRun::Outcome::NOT_RUN)
    {
        bug("A simulation run that has run cannot be submitted");
    }
    if (std::ranges::any_of(extraListeners,
                            [](const std::shared_ptr<SimulationListener>& listener) {
                                return listener == nullptr;
                            }))
    {
        bug("An extra listener of a job is null");
    }
    // A listener of a live job is its worker's: two workers would call it at the same time, and
    // a sample listener would hand the samples of one job to the news of the other.
    if (sampler != nullptr && isListenerOfAJob(*sampler))
    {
        bug("A sample listener is the listener of one job");
    }
    if (std::ranges::any_of(extraListeners,
                            [this](const std::shared_ptr<SimulationListener>& listener) {
                                return isListenerOfAJob(*listener);
                            }))
    {
        bug("An extra listener is the listener of one job");
    }
    if (!stopSource.stop_possible())
    {
        bug("A job needs a stop source that can stop it");
    }

    auto job        = std::make_unique<Job>();
    job->id         = JobId{.value = m_lastJob + 1};
    job->group      = group;
    job->run        = std::move(run);
    job->sampler    = std::move(sampler);
    job->stopSource = std::move(stopSource);
    job->stopToken  = job->stopSource.get_token();
    job->listeners.reserve(extraListeners.size() + 1);
    if (job->sampler != nullptr)
    {
        job->listeners.push_back(job->sampler);
    }
    std::ranges::move(extraListeners, std::back_inserter(job->listeners));

    startThread();
    Job& queued = *job;
    m_jobs.push_back(std::move(job));
    try
    {
        const std::scoped_lock lock(m_queueMutex);
        m_queue.push_back(&queued);
    }
    catch (...)
    {
        m_jobs.pop_back();
        throw;
    }
    m_lastJob = queued.id.value;
    m_queueChanged.notify_one();
    return queued.id;
}

void SimulationRunner::cancel(JobId job)
{
    checkThread();
    Job* const cancelled = findJob(job);
    if (cancelled == nullptr)
    {
        return;
    }
    for (Job* const queued :
         takeQueued([cancelled](const Job& candidate) { return &candidate == cancelled; }))
    {
        endUnrun(*queued);
    }
    requestStop(*cancelled);
}

void SimulationRunner::purge(GroupId group)
{
    checkThread();
    for (Job* const queued :
         takeQueued([group](const Job& candidate) { return candidate.group == group; }))
    {
        endUnrun(*queued);
    }
}

void SimulationRunner::cancelGroup(GroupId group)
{
    purge(group);
    for (const std::unique_ptr<Job>& job : m_jobs)
    {
        if (job->group == group)
        {
            requestStop(*job);
        }
    }
}

void SimulationRunner::cancelAll()
{
    checkThread();
    for (Job* const queued : takeQueued([](const Job& /*candidate*/) { return true; }))
    {
        endUnrun(*queued);
    }
    for (const std::unique_ptr<Job>& job : m_jobs)
    {
        requestStop(*job);
    }
}

std::vector<SimulationRunner::News> SimulationRunner::poll()
{
    checkThread();
    std::vector<News> news;
    for (auto it = m_jobs.begin(); it != m_jobs.end();)
    {
        Job&               job   = **it;
        JobState           state = JobState::QUEUED;
        std::exception_ptr exception;
        {
            const std::scoped_lock lock(job.mutex);
            state     = job.state;
            exception = job.exception;
        }
        // Taken after the state was read: a job that was ENDED then has published every sample.
        // (A queued job has published none.)
        std::vector<RunSample> samples = job.sampler != nullptr && state != JobState::QUEUED
                                             ? job.sampler->takeSamples()
                                             : std::vector<RunSample>{};
        const bool             started = state != JobState::QUEUED && !job.startReported;
        if (started || !samples.empty() || state == JobState::ENDED)
        {
            job.startReported = true;
            news.push_back(News{.job       = job.id,
                                .group     = job.group,
                                .state     = state,
                                .run       = job.run,
                                .samples   = std::move(samples),
                                .exception = std::move(exception)});
        }
        if (state == JobState::ENDED)
        {
            it = m_jobs.erase(it);
        }
        else
        {
            ++it;
        }
    }
    return news;
}

std::optional<SimulationRunner::JobState> SimulationRunner::getState(JobId job) const
{
    checkThread();
    Job* const found = findJob(job);
    if (found == nullptr)
    {
        return std::nullopt;
    }
    const std::scoped_lock lock(found->mutex);
    return found->state;
}

bool SimulationRunner::isBusy() const
{
    checkThread();
    return !m_jobs.empty();
}

void SimulationRunner::checkThread() const
{
    if (std::this_thread::get_id() != m_owner)
    {
        bug("A simulation runner is used on the thread that made it");
    }
}

SimulationRunner::Job* SimulationRunner::findJob(JobId id) const
{
    const auto found = std::ranges::find_if(
        m_jobs, [id](const std::unique_ptr<Job>& job) { return job->id == id; });
    return found != m_jobs.end() ? found->get() : nullptr;
}

bool SimulationRunner::isListenerOfAJob(const SimulationListener& listener) const
{
    return std::ranges::any_of(m_jobs, [&listener](const std::unique_ptr<Job>& job) {
        return std::ranges::any_of(job->listeners,
                                   [&listener](const std::shared_ptr<SimulationListener>& own) {
                                       return own.get() == &listener;
                                   });
    });
}

void SimulationRunner::startThread()
{
    if (m_threads.size() >= m_threadCount)
    {
        return;
    }
    try
    {
        m_threads.emplace_back([this] { work(); });
    }
    catch (const std::system_error&)
    {
        if (m_threads.empty())
        {
            throw;
        }
        // The job waits for a thread that is started.
    }
}

void SimulationRunner::work()
{
    while (Job* const job = takeJob())
    {
        std::exception_ptr exception;
        try
        {
            job->run->run(job->stopToken, job->listeners);
        }
        catch (...)
        {
            // Nothing leaves the thread (see the thread contract).
            exception = std::current_exception();
        }
        if (exception == nullptr)
        {
            exception = job->run->getException();
        }
        // The thread's last access to the job: after the unlock the owning thread may forget it.
        const std::scoped_lock lock(job->mutex);
        job->exception = std::move(exception);
        job->state     = JobState::ENDED;
    }
}

SimulationRunner::Job* SimulationRunner::takeJob()
{
    std::unique_lock lock(m_queueMutex);
    m_queueChanged.wait(lock, [this] { return m_shuttingDown || !m_queue.empty(); });
    if (m_shuttingDown)
    {
        return nullptr;
    }
    Job* const job = m_queue.front();
    m_queue.pop_front();
    // QUEUED while in the queue, RUNNING from the moment it leaves it.
    const std::scoped_lock jobLock(job->mutex);
    job->state = JobState::RUNNING;
    return job;
}

template <typename Predicate>
std::vector<SimulationRunner::Job*> SimulationRunner::takeQueued(Predicate taken)
{
    std::vector<Job*>      jobs;
    const std::scoped_lock lock(m_queueMutex);
    for (auto it = m_queue.begin(); it != m_queue.end();)
    {
        if (taken(**it))
        {
            jobs.push_back(*it);
            it = m_queue.erase(it);
        }
        else
        {
            ++it;
        }
    }
    return jobs;
}

void SimulationRunner::endUnrun(Job& job)
{
    // The run has not run (submit() made sure, and no thread took the job). With a stop that is
    // requested, run() ends it CANCELLED at once. A stop of its own: the job's stop source may
    // be the source of other jobs too, which go on.
    // NOLINTNEXTLINE(misc-const-correctness): request_stop() is non-const outside libstdc++
    std::stop_source stopped;
    stopped.request_stop();
    job.run->run(stopped.get_token());
    const std::scoped_lock lock(job.mutex);
    job.state = JobState::ENDED;
}

void SimulationRunner::requestStop(Job& job)
{
    {
        const std::scoped_lock lock(job.mutex);
        if (job.state == JobState::ENDED)
        {
            return;
        }
    }
    job.stopSource.request_stop();
}

std::string_view name(SimulationRunner::JobState state) noexcept
{
    switch (state)
    {
        case SimulationRunner::JobState::QUEUED:
            return "QUEUED";
        case SimulationRunner::JobState::RUNNING:
            return "RUNNING";
        case SimulationRunner::JobState::ENDED:
            return "ENDED";
    }
    return "QUEUED";  // not reached: the switch covers every state
}

}  // namespace QtRocket
