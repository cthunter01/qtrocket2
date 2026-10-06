#pragma once

namespace QtRocket
{

class SimulationStatus;

/// What advances a simulation by one time step (OpenRocket's simulation/SimulationStepper). The
/// engine has one stepper per phase of a flight (powered and coasting flight, tumbling, descent
/// under a recovery device, lying on the ground); when the phase changes it hands the status to
/// the stepper of the new phase with initialize() and then calls step() until the next change.
///
/// Deviations from OpenRocket:
/// - initialize() takes the status by value and gives the status to go on with back: Java's
///   steppers return either the object they were given or a new one made from it, and the
///   engine drops the old one either way (`status = stepper.initialize(status)`); here that is
///   `status = stepper.initialize(std::move(status))`.
class SimulationStepper
{
public:
    virtual ~SimulationStepper() = default;

    SimulationStepper& operator=(const SimulationStepper&) = delete;
    SimulationStepper& operator=(SimulationStepper&&)      = delete;

    /// Initializes a simulation with this stepper from the current simulation status and its
    /// launch conditions. Returns the status that is suitable for simulating with this stepper:
    /// @p status itself, or a new status made from it.
    [[nodiscard]] virtual SimulationStatus initialize(SimulationStatus status) = 0;

    /// Performs one simulation time step on @p status, a status that initialize() returned.
    ///
    /// @p maxTimeStep is the maximum time step to take: an upper bound that keeps a stepper from
    /// stepping over upcoming flight events (motor ignition etc.).
    ///
    /// When step() is called, a new point is added to the flight data branch and the current
    /// simulation status is saved to that point. The simulation parameters with which the status
    /// is updated are calculated and saved to the new point too (only those at the start of a
    /// Runge-Kutta step are saved, although they vary through the step).
    ///
    /// Upon ground hit one extra step is taken, with a NaN @p maxTimeStep, to save the
    /// simulation status and the parameters at the moment of impact. The status at the end of
    /// that step is not saved, as the rocket stops moving at this point.
    ///
    /// @throws SimulationException from a simulation listener, or when the computation fails
    virtual void step(SimulationStatus& status, double maxTimeStep) = 0;

protected:
    SimulationStepper()                         = default;
    SimulationStepper(const SimulationStepper&) = default;
    SimulationStepper(SimulationStepper&&)      = default;
};

}  // namespace QtRocket
