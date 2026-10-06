#pragma once

#include "QtRocket/simulation/AbstractRkSimulationStepper.h"

namespace QtRocket
{

class SimulationStatus;

/// The fourth-order Runge-Kutta flight stepper (OpenRocket's simulation/RK4SimulationStepper),
/// the default: four evaluations of the acceleration per step.
///
///     k1 = f(t, y)
///     k2 = f(t + h/2, y + k1*h/2)
///     k3 = f(t + h/2, y + k2*h/2)
///     k4 = f(t + h, y + k3*h)
///     y(n+1) = y(n) + h*(k1 + 2*k2 + 2*k3 + k4)/6
///
/// The step length h is decided after k1 (AbstractRkSimulationStepper::computeTimeStep()). The
/// data of the step (the store) are written to the flight data branch right after that, so a
/// row holds the forces at the start of its step. The sum is formed as Java forms it:
/// (2 k2 + 2 k3 + k1 + k4) * (h / 6).
///
/// See AbstractRkSimulationStepper for the deviations from OpenRocket.
class Rk4SimulationStepper : public AbstractRkSimulationStepper
{
public:
    Rk4SimulationStepper()                                       = default;
    Rk4SimulationStepper(const Rk4SimulationStepper&)            = default;
    Rk4SimulationStepper(Rk4SimulationStepper&&)                 = default;
    Rk4SimulationStepper& operator=(const Rk4SimulationStepper&) = delete;
    Rk4SimulationStepper& operator=(Rk4SimulationStepper&&)      = delete;
    ~Rk4SimulationStepper() override                             = default;

    /// One Runge-Kutta 4 step of at most @p maxTimeStep; a NaN @p maxTimeStep records the
    /// status and the step's data and sets the landed values (see SimulationStepper::step()).
    /// @throws SimulationException from a simulation listener
    /// @throws SimulationCalculationException when the values run out of range
    void step(SimulationStatus& status, double maxTimeStep) override;
};

}  // namespace QtRocket
