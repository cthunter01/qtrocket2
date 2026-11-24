#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <vector>

#include "QtRocket/simulation/RunSample.h"
#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"

namespace QtRocket
{

class FlightEvent;
class SimulationStatus;

/// The listener that samples a running simulation for its progress display: a port of the
/// SimulationProgressListener of OpenRocket's run dialog (swing gui/simulation/
/// SimulationRunDialog.java:496-530, UPDATE_MS :62). It publishes a sample of the status
/// (RunSample::of()) when the simulation handles a LAUNCH, an APOGEE or a SIMULATION_END event
/// (of every branch), and after a step when kUpdateInterval has passed on its clock since the
/// last sample a step published (the first step always publishes). The samples wait in the
/// listener until takeSamples() takes them, in the order they were published: what the owning
/// thread feeds to a SimulationProgressEstimator (Java: SwingWorker.publish(), whose chunks
/// reach process() on the event thread). It changes nothing of the simulation and is not a
/// system listener (Java's is not), so the nested coast run that finds the optimum altitude does
/// not reach it, and it never draws Warning::kListenersAffected.
///
/// Threads: the hooks are called on the thread that runs the simulation, and so is the clock;
/// takeSamples() may be called on any thread at any time, also while the simulation runs (the
/// samples are kept under a mutex). The listener reaches the owning thread through the runner
/// that runs the simulation (the owning thread keeps the shared_ptr it handed to the run, and
/// takes the samples when it polls the run), never through a queued call.
///
/// Clones: the simulation runs on clones of its listeners (see SimulationListener, "Clones").
/// The clones share everything with the listener they were made from (one state behind a
/// shared_ptr): the samples, the clock and the time of the last sample a step published, so
/// whatever clone a hook runs on, the listener the caller holds gets the sample.
///
/// Deviations from OpenRocket:
/// - The clock is a steady (monotonic) clock, injectable for tests; Java reads the wall clock
///   (System.currentTimeMillis()), twice when it publishes, so a change of the system time can
///   stop or speed up its samples.
/// - The time of the last sample a step published is shared by the clones. In Java each clone
///   has its own copy of the field (a long, copied when the clone is made: when a stepper
///   starts, when a stage separates), so a clone goes on from the time its source had then, and
///   the branch of a stage that separated may publish at its first step although the
///   sustainer's branch published less than 200 ms before. Which steps publish depends on the
///   wall clock in either case.
/// - A sample keeps four values of the status, not a copy of it (see RunSample).
class RunSampleListener final : public CloneableSimulationListener<RunSampleListener>
{
public:
    /// What the listener reads the time from: called once per step, on the simulation's thread.
    using Clock = std::function<std::chrono::steady_clock::time_point()>;

    /// The least time between two samples that steps publish (Java: UPDATE_MS, 200 ms).
    static constexpr std::chrono::milliseconds kUpdateInterval{200};

    /// A listener on std::chrono::steady_clock.
    RunSampleListener();

    /// A listener on @p clock (a test's clock).
    /// @throws BugError when @p clock is empty
    explicit RunSampleListener(Clock clock);

    /// Publishes a sample of @p status for a LAUNCH, APOGEE or SIMULATION_END event; always true
    /// (the event is handled).
    [[nodiscard]] bool handleFlightEvent(SimulationStatus&  status,
                                         const FlightEvent& event) override;

    /// Publishes a sample of @p status when no step has published one yet or kUpdateInterval
    /// has passed since the last one a step published (Java: now >= time + UPDATE_MS).
    void postStep(SimulationStatus& status) override;

    /// The samples published since the last call (by this listener or any of its clones), in
    /// the order they were published; they are not kept. Thread-safe.
    [[nodiscard]] std::vector<RunSample> takeSamples();

private:
    struct State;

    /// Adds the sample of @p status (under the lock).
    void publish(const SimulationStatus& status);

    /// Shared by the clones.
    std::shared_ptr<State> m_state;
};

}  // namespace QtRocket
