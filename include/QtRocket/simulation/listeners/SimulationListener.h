#pragma once

#include <memory>

namespace QtRocket
{

class SimulationException;
class SimulationStatus;

/// Listens to a simulation and may act on it (OpenRocket's simulation/listeners/
/// SimulationListener): the hooks every listener has, around the simulation, around each of its
/// branches and around each step. The two other interfaces add hooks for those who want them:
/// SimulationEventListener for the flight events and SimulationComputationListener for the
/// computations of a step. The simulation asks each listener which of them it implements
/// (dynamic_cast, Java: instanceof) and calls only those hooks. AbstractSimulationListener
/// implements all three with hooks that do nothing; a listener normally derives from it,
/// through CloneableSimulationListener.
///
/// Ownership: the listeners of a simulation are held by std::shared_ptr<SimulationListener>
/// (the caller of a simulation keeps its listener and reads what it recorded afterwards; Java:
/// a reference). Each simulation run works on clones of the listeners it was given (clone()),
/// so a listener that keeps state must copy it in its copy constructor.
///
/// A hook that changes the simulation status it is given makes the simulation add the
/// "listeners affected the simulation" warning, unless the listener is a system listener.
///
/// Deviations from OpenRocket:
/// - The status is passed by reference; the exception of endSimulation() and
///   endSimulationBranch() is a pointer that is null for a normal ending (Java: null).
/// - A hook that Java declares `throws SimulationException` may throw one here; the two end
///   hooks must not, as in Java.
class SimulationListener
{
public:
    virtual ~SimulationListener() = default;

    SimulationListener& operator=(const SimulationListener&) = delete;
    SimulationListener& operator=(SimulationListener&&)      = delete;

    /// Called when a simulation starts.
    /// @throws SimulationException to stop the simulation
    virtual void startSimulation(SimulationStatus& status) = 0;

    /// Called when a simulation ends: normally (through an end-of-simulation event, with
    /// @p exception null) or because of a SimulationException (@p exception, valid during the
    /// call). It must not throw a SimulationException: the simulation is already ending.
    virtual void endSimulation(SimulationStatus& status, const SimulationException* exception) = 0;

    /// Called when a branch of the simulation starts: for the sustainer's branch and for the
    /// branch of every stage that separates.
    /// @throws SimulationException to stop the simulation
    virtual void startSimulationBranch(SimulationStatus& status) = 0;

    /// Called when a branch of the simulation ends: normally (@p exception null) or because of
    /// a SimulationException thrown while it ran (@p exception, valid during the call). It must
    /// not throw a SimulationException: the branch is already ending.
    virtual void endSimulationBranch(SimulationStatus&          status,
                                     const SimulationException* exception) = 0;

    /// Called before a simulation step. True to take the step normally, false to skip it.
    /// @throws SimulationException to stop the simulation
    [[nodiscard]] virtual bool preStep(SimulationStatus& status) = 0;

    /// Called right after a simulation step, whether preStep() let the step be taken or not.
    /// @throws SimulationException to stop the simulation
    virtual void postStep(SimulationStatus& status) = 0;

    /// Whether this is a system listener: one the application uses internally. A listener
    /// written by a user returns false. System listeners do not add a warning to the results
    /// when they affect the simulation.
    [[nodiscard]] virtual bool isSystemListener() const = 0;

    /// A copy of this listener with its state: an object of the same dynamic type (Java:
    /// clone()).
    [[nodiscard]] virtual std::shared_ptr<SimulationListener> clone() const = 0;

protected:
    SimulationListener()                          = default;
    SimulationListener(const SimulationListener&) = default;
    SimulationListener(SimulationListener&&)      = default;
};

}  // namespace QtRocket
