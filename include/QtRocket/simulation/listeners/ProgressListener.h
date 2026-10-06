#pragma once

#include <functional>
#include <memory>

#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"

namespace QtRocket
{

class SimulationStatus;

/// A simulation listener that reports the progress of a simulation: it calls a function with
/// the simulation status after every step.
///
/// An addition: OpenRocket has no such listener in its core; its simulation dialog has a
/// private one (SimulationProgressListener in swing's SimulationRunDialog), which reads the
/// altitude, the velocity and the flags of the status after each step. This is that listener
/// for the command line and the later GUI.
///
/// Not a system listener, as OpenRocket's is not (it lives in simulation/listeners/, not in
/// simulation/listeners/system/). So the coast simulation that the engine runs to find the
/// optimum altitude, which keeps the system listeners only, does not report to it: the callback
/// is called after every step of every branch of the flight and of nothing else, with the
/// status of that branch, and the time it is given never goes back within a branch. Being an
/// ordinary listener costs nothing: it only overrides postStep(), which answers nothing, and
/// the callback is given the status by reference to const (a progress display must not affect
/// the simulation), so it never draws Warning::kListenersAffected.
///
/// The callback is called on the thread that runs the simulation. The simulation runs on
/// clones of its listeners (see SimulationListener, "Clones"); the clones of a ProgressListener
/// share one callback object, so a callback with state of its own keeps one state for the
/// whole run.
class ProgressListener final : public CloneableSimulationListener<ProgressListener>
{
public:
    /// What is called after every step.
    using Callback = std::function<void(const SimulationStatus&)>;

    /// A listener that calls @p callback after every step.
    /// @throws BugError when @p callback is empty
    explicit ProgressListener(Callback callback);

    /// Calls the callback with @p status.
    void postStep(SimulationStatus& status) override;

private:
    /// Shared by the clones.
    std::shared_ptr<const Callback> m_callback;
};

}  // namespace QtRocket
