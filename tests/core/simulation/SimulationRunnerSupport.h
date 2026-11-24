#pragma once

// Driving a SimulationRunner from the test's thread as the GUI's timer drives it (poll() in a
// loop, every news kept), and holding its jobs at known steps: a gate that holds one run, a
// meeting at which several runs wait for each other. Their listeners run on the runner's
// threads, which wait by spinning on atomics (as HeldRunSupport.h's do) until the test's thread
// lets them go on or the job's stop is requested: every hold watches the token of the stop
// source the job was submitted with, so a cancel (and a runner's destructor) always ends it.
// No sleeps and no timing assertions: the only clock is the safety net of the test's waits,
// which only a defect reaches and which makes the test fail instead of hang. For the tests of
// the runner and for the GUI's tests of what drives one. Test-only.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <iterator>
#include <memory>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/simulation/RunSample.h"
#include "QtRocket/simulation/SimulationRunner.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/listeners/ProgressListener.h"

namespace QtRocket::Test
{

/// How long a wait of the test's thread goes on before it gives up: a safety net. A run gets to
/// its gate in milliseconds, under the sanitizers too; only a defect (a runner that runs the
/// jobs one after the other, a run that never ends) gets here.
inline constexpr std::chrono::seconds kRunnerSafetyNet{120};

/// Holds a run after its step number step() (counted over every branch: the clones of the
/// listener share the count) until the test's thread releases it or the job's stop is requested.
/// The run then goes on: after a stop request, the InterruptListener after the extra listeners
/// cancels it in that same step.
class StepGate
{
public:
    /// A gate after step @p step.
    explicit StepGate(int step) : m_state(std::make_shared<State>(step)) { }

    /// The step after which the gate holds the run.
    [[nodiscard]] int step() const noexcept { return m_state->step; }

    /// The listener of the gate, for a job whose stop source gives @p stopToken. One run only.
    [[nodiscard]] std::shared_ptr<ProgressListener> listener(std::stop_token stopToken) const
    {
        return std::make_shared<ProgressListener>(
            [state = m_state, stopToken = std::move(stopToken)](const SimulationStatus&) {
                const int steps = state->steps.fetch_add(1) + 1;
                if (steps != state->step)
                {
                    return;
                }
                state->reached.store(true);
                while (!state->released.load() && !stopToken.stop_requested())
                {
                    std::this_thread::yield();
                }
            });
    }

    /// Whether the run got to the gate (and is held there, unless it was let go).
    [[nodiscard]] bool reached() const noexcept { return m_state->reached.load(); }

    /// Lets the run go on.
    void release() const noexcept { m_state->released.store(true); }

    /// The steps the run has taken.
    [[nodiscard]] int steps() const noexcept { return m_state->steps.load(); }

private:
    struct State
    {
        explicit State(int gateStep) : step(gateStep) { }

        const int         step;
        std::atomic<int>  steps{0};
        std::atomic<bool> reached{false};
        std::atomic<bool> released{false};
    };

    std::shared_ptr<State> m_state;
};

/// Several runs that meet after their step number step(): each waits there until every one of
/// them has arrived, so none can go on before all are at that step at the same time. With runs
/// that are run one after the other, the first waits for good: it then gives up when its job's
/// stop is requested (a cancel of the test's safety net), or when the test's thread abandons
/// the meeting.
class StepMeeting
{
public:
    /// A meeting of @p parties runs after step @p step.
    StepMeeting(int parties, int step) : m_state(std::make_shared<State>(parties, step)) { }

    /// The listener of one run of the meeting, for a job whose stop source gives @p stopToken.
    [[nodiscard]] std::shared_ptr<ProgressListener> listener(std::stop_token stopToken) const
    {
        return std::make_shared<ProgressListener>(
            [state = m_state, stopToken = std::move(stopToken),
             steps = std::make_shared<int>(0)](const SimulationStatus&) {
                (*steps)++;
                if (*steps != state->step)
                {
                    return;
                }
                state->arrived.fetch_add(1);
                while (state->arrived.load() < state->parties)
                {
                    if (state->abandoned.load() || stopToken.stop_requested())
                    {
                        state->gaveUp.fetch_add(1);
                        return;
                    }
                    std::this_thread::yield();
                }
            });
    }

    /// The runs that have arrived.
    [[nodiscard]] int arrived() const noexcept { return m_state->arrived.load(); }

    /// Whether every run arrived and none gave up waiting for the others.
    [[nodiscard]] bool allMet() const noexcept
    {
        return m_state->arrived.load() == m_state->parties && m_state->gaveUp.load() == 0;
    }

    /// Lets the runs that wait give up.
    void abandon() const noexcept { m_state->abandoned.store(true); }

private:
    struct State
    {
        State(int meetingParties, int meetingStep) : parties(meetingParties), step(meetingStep) { }

        const int         parties;
        const int         step;
        std::atomic<int>  arrived{0};
        std::atomic<int>  gaveUp{0};
        std::atomic<bool> abandoned{false};
    };

    std::shared_ptr<State> m_state;
};

/// Polls a runner on the test's thread (the runner's owning thread) and keeps every news it
/// hands out, in order: what the GUI's timer does.
class RunnerPoller
{
public:
    using JobId = SimulationRunner::JobId;
    using News  = SimulationRunner::News;

    explicit RunnerPoller(SimulationRunner& runner) : m_runner(&runner) { }

    /// One poll().
    void pollOnce()
    {
        std::vector<News> news = m_runner->poll();
        std::ranges::move(news, std::back_inserter(m_news));
    }

    /// Polls until @p done holds. When the safety net is reached first, the test fails, every
    /// job is cancelled, and the runner is polled until its jobs have ended (every hold of this
    /// header gives up on a cancel), so that the test ends. Returns whether @p done held.
    bool pollUntil(const std::function<bool()>& done)
    {
        const auto giveUp = std::chrono::steady_clock::now() + kRunnerSafetyNet;
        while (true)
        {
            pollOnce();
            if (done())
            {
                return true;
            }
            if (std::chrono::steady_clock::now() > giveUp)
            {
                break;
            }
            std::this_thread::yield();
        }
        ADD_FAILURE() << "the runner did not get there: its jobs are cancelled";
        m_runner->cancelAll();
        while (m_runner->isBusy())
        {
            pollOnce();
            std::this_thread::yield();
        }
        return false;
    }

    /// Polls until the news has told that @p job ended.
    bool pollUntilEnded(JobId job)
    {
        return pollUntil([this, job] { return hasEnded(job); });
    }

    /// Polls until the runner has no job left (every job ended, and the news told it).
    bool pollUntilIdle()
    {
        return pollUntil([this] { return !m_runner->isBusy(); });
    }

    /// Polls until the run of @p gate has got to it.
    bool pollUntilReached(const StepGate& gate)
    {
        return pollUntil([&gate] { return gate.reached(); });
    }

    /// Polls until @p count runs have arrived at @p meeting.
    bool pollUntilArrived(const StepMeeting& meeting, int count)
    {
        return pollUntil([&meeting, count] { return meeting.arrived() == count; });
    }

    /// Every news so far, in the order poll() handed them out.
    [[nodiscard]] const std::vector<News>& news() const noexcept { return m_news; }

    /// The news of @p job so far, in order.
    [[nodiscard]] std::vector<News> newsOf(JobId job) const
    {
        std::vector<News> news;
        std::ranges::copy_if(m_news, std::back_inserter(news),
                             [job](const News& item) { return item.job == job; });
        return news;
    }

    /// Whether the news has told that @p job ended.
    [[nodiscard]] bool hasEnded(JobId job) const
    {
        return std::ranges::any_of(m_news, [job](const News& item) {
            return item.job == job && item.state == SimulationRunner::JobState::ENDED;
        });
    }

    /// The samples of @p job so far, in order.
    [[nodiscard]] std::vector<RunSample> samplesOf(JobId job) const
    {
        std::vector<RunSample> samples;
        for (const News& item : m_news)
        {
            if (item.job == job)
            {
                samples.insert(samples.end(), item.samples.begin(), item.samples.end());
            }
        }
        return samples;
    }

private:
    SimulationRunner* m_runner;
    std::vector<News> m_news;
};

}  // namespace QtRocket::Test
