#pragma once

#include "QtRocket/simulation/AbstractRkSimulationStepper.h"

namespace QtRocket
{

class SimulationStatus;

/// The sixth-order Runge-Kutta flight stepper (OpenRocket's simulation/RK6SimulationStepper):
/// seven evaluations of the acceleration per step, slower than RK4 but more accurate in some
/// cases. The coefficients are those of Mechee and Rajihy, "Generalized RK Integrators for
/// Solving Ordinary Differential Equations: A Survey & Comparison Study", Global Journal of Pure
/// and Applied Mathematics 13(7), 2017, table 2:
///
///     0   |
///     1/3 | 1/3
///     2/3 | 0      2/3
///     1/3 | 1/12   1/3   -1/12
///     1/2 | -1/16  9/8   -3/16  -3/8
///     1/2 | 0      9/8   -3/8   -3/4   1/2
///     1   | 9/44   -9/11 63/44  18/11  0      -16/11
///     ----+-----------------------------------------------------
///         | 11/120 0     27/40  27/40  -4/15  -4/15  11/120
///
///     k1 = f(t, y)
///     k2 = f(t + h/3, y + 1/3*h*k1)
///     k3 = f(t + h*2/3, y + 2/3*h*k2)
///     k4 = f(t + h*1/3, y + 1/12*h*k1 + 1/3*h*k2 - 1/12*h*k3)
///     k5 = f(t + h*1/2, y - 1/16*h*k1 + 9/8*h*k2 - 3/16*h*k3 - 3/8*h*k4)
///     k6 = f(t + h*1/2, y + 9/8*h*k2 - 3/8*h*k3 - 3/4*h*k4 + 1/2*h*k5)
///     k7 = f(t + h, y + 9/44*h*k1 - 9/11*h*k2 + 63/44*h*k3 + 18/11*h*k4 - 16/11*h*k6)
///     y(n+1) = y(n) + h*(11/120*k1 + 27/40*k3 + 27/40*k4 - 4/15*k5 - 4/15*k6 + 11/120*k7)
///
/// The step length h is decided after k1, as for Rk4SimulationStepper, and the store is written
/// at the same point. The orientation of an intermediate point is the product of one rotation
/// per term, applied in the order of the terms (rotations do not commute, so this is part of
/// the method as OpenRocket implements it).
///
/// See AbstractRkSimulationStepper for the deviations from OpenRocket.
class Rk6SimulationStepper : public AbstractRkSimulationStepper
{
public:
    Rk6SimulationStepper()                                       = default;
    Rk6SimulationStepper(const Rk6SimulationStepper&)            = default;
    Rk6SimulationStepper(Rk6SimulationStepper&&)                 = default;
    Rk6SimulationStepper& operator=(const Rk6SimulationStepper&) = delete;
    Rk6SimulationStepper& operator=(Rk6SimulationStepper&&)      = delete;
    ~Rk6SimulationStepper() override                             = default;

    /// One Runge-Kutta 6 step of at most @p maxTimeStep; a NaN @p maxTimeStep records the
    /// status and the step's data and sets the landed values (see SimulationStepper::step()).
    /// @throws SimulationException from a simulation listener
    /// @throws SimulationCalculationException when the values run out of range
    void step(SimulationStatus& status, double maxTimeStep) override;
};

}  // namespace QtRocket
