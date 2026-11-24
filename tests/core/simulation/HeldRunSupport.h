#pragma once

// A run of a simulation (SimulationRun) on a thread of its own, held after a known step while
// the test's thread does something: what the tests that hand runs between threads share (the
// threading tests of the runs and of the run sample listener). Test-only.

#include <atomic>
#include <functional>
#include <memory>
#include <span>
#include <stop_token>
#include <thread>
#include <vector>

#include "QtRocket/simulation/SimulationRun.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/listeners/ProgressListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"

namespace QtRocket::Test
{

/// The number of steps a held run takes before it waits for the test's thread.
inline constexpr int kStepsBeforeTheHold = 30;

/// What the thread of a held run (SimulationRun) and the test's thread share.
struct Hold
{
    /// Set by the run's thread when it has taken kStepsBeforeTheHold steps; it then waits for
    /// released.
    std::atomic<bool> reached{false};
    /// Set by the test's thread when the run may go on.
    std::atomic<bool> released{false};
    /// Set by the run's thread when run() has returned.
    std::atomic<bool> finished{false};
};

/// A listener that, after step kStepsBeforeTheHold, tells the test's thread that the run is
/// under way and holds it until the test's thread releases it.
[[nodiscard]] inline std::shared_ptr<ProgressListener> holdingListener(Hold& hold)
{
    const auto steps = std::make_shared<int>(0);
    return std::make_shared<ProgressListener>([&hold, steps](const SimulationStatus& /*status*/) {
        (*steps)++;
        if (*steps != kStepsBeforeTheHold)
        {
            return;
        }
        hold.reached.store(true);
        while (!hold.released.load())
        {
            std::this_thread::yield();
        }
    });
}

/// The body of the thread of a held run: SimulationRun::run() with @p stopToken, @p extraListeners
/// and, after them, the holding listener (so they have seen the step at which the run waits).
inline void runHeld(SimulationRun& run, Hold& hold, const std::stop_token& stopToken,
                    std::span<const std::shared_ptr<SimulationListener>> extraListeners)
{
    std::vector<std::shared_ptr<SimulationListener>> listeners(extraListeners.begin(),
                                                               extraListeners.end());
    listeners.push_back(holdingListener(hold));
    run.run(stopToken, listeners);
    hold.finished.store(true);
}

/// Runs @p run on a thread of its own with @p stopToken and @p extraListeners, does @p whileHeld
/// on this thread (the one that owns the simulation the run was made from) while the run is held
/// after kStepsBeforeTheHold steps, then lets the run go on and waits for it. Returns whether the
/// run got to that step (else it ended before, and @p whileHeld ran after it).
[[nodiscard]] inline bool runHolding(
    SimulationRun& run, const std::stop_token& stopToken, const std::function<void()>& whileHeld,
    std::span<const std::shared_ptr<SimulationListener>> extraListeners = {})
{
    Hold hold;
    {
        // Not the stop token of the std::jthread, whose destructor requests a stop.
        const std::jthread thread(runHeld, std::ref(run), std::ref(hold), stopToken,
                                  extraListeners);
        while (!hold.reached.load() && !hold.finished.load())
        {
            std::this_thread::yield();
        }
        whileHeld();
        hold.released.store(true);
    }  // joins
    return hold.reached.load();
}

}  // namespace QtRocket::Test
