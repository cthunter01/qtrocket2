#pragma once

#include "QtRocket/simulation/AbstractSimulationStepper.h"

namespace QtRocket
{

class SimulationStatus;

/// The stepper of a rocket that lies on the ground (OpenRocket's simulation/GroundStepper):
/// after landing the engine goes on stepping while events remain (a late ejection charge, the
/// separation of a stage), and each step only records that the rocket sits at altitude 0
/// without moving.
///
/// A step first sets the landed values (AbstractSimulationStepper::landedValues()). A time step
/// above twice kMinTimeStep is split in two: a first point kMinTimeStep later, so that the
/// record shows the rocket at rest immediately, and a second one at the end of the step; a
/// shorter step has the one point at its end. Before each point is opened, the time step that
/// leads to it is written to the previous point of the branch, replacing the NaN it was stored
/// with; a new point's own time step is NaN until the next step writes it.
///
/// Deviations from OpenRocket:
/// - The store is reached through getStore() (Java: the package-private field `store`).
/// - A status without a flight data branch is a BugError (Java: NullPointerException).
/// - Logging is not ported.
class GroundStepper : public AbstractSimulationStepper
{
public:
    GroundStepper()                                = default;
    GroundStepper(const GroundStepper&)            = default;
    GroundStepper(GroundStepper&&)                 = default;
    GroundStepper& operator=(const GroundStepper&) = delete;
    GroundStepper& operator=(GroundStepper&&)      = delete;
    ~GroundStepper() override                      = default;

    /// Returns @p status as it is.
    [[nodiscard]] SimulationStatus initialize(SimulationStatus status) override;

    /// Records the rocket lying on the ground for @p timeStep seconds (see the class comment).
    /// @throws SimulationException from a simulation listener
    void step(SimulationStatus& status, double timeStep) override;

    /// Does nothing: a rocket on the ground does not accelerate.
    void calculateAcceleration(SimulationStatus& status, DataStore& store) override;

    /// The store of the stepper (Java: the package-private field `store`).
    [[nodiscard]] DataStore&       getStore() noexcept { return m_store; }
    [[nodiscard]] const DataStore& getStore() const noexcept { return m_store; }

private:
    DataStore m_store;
};

}  // namespace QtRocket
