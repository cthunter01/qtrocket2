#pragma once

#include "QtRocket/simulation/AbstractSimulationStepper.h"

namespace QtRocket
{

class SimulationStatus;

/// The base of the steppers of a rocket that falls without attitude dynamics (OpenRocket's
/// simulation/AbstractEulerStepper): under a recovery device (BasicLandingStepper) or tumbling
/// (BasicTumbleStepper). The rocket is a point mass with a drag coefficient, computeCD(), which
/// the subclass gives; the step is an Euler step whose length adapts to the motion.
///
/// The time step: kRecoveryTimeStep, at most 1 / |acceleration|; when the step given to step()
/// is shorter, just short of it (by kMinTimeStep, to capture discontinuities such as a chute
/// opening better), or all of it when it is not above kMinTimeStep; never below kMinTimeStep.
/// Then, with the position and velocity that step would give:
/// - below the ground: the step that ends on the ground, the solution of
///   1/2 a t^2 + v t + z = 0;
/// - the vertical velocity changes sign (apogee): the step to it, |v / a|;
/// - otherwise the acceleration at the end of the step is estimated from the jerk (the change
///   of the drag with the velocity), and when it changes sign appreciably (an oscillation of
///   the descent rate is building up) the step is cut to where it would be zero, |a / jerk|.
/// A step changed in this way is at least kMinTimeStep, becomes the whole given step when it
/// ends within kMinTimeStep of it, and a position within MathUtil::kEpsilon of the ground is
/// set on it.
///
/// The data of the step are written to the flight data branch at the end of step(), to the
/// point that was opened at its start.
///
/// Deviations from OpenRocket:
/// - What Java reaches through protected or package-private access is public here, because the
///   tests call it: the store and computeCD().
/// - Normalizing an airspeed between MathUtil::kEpsilon and 1e-7 m/s is a BugError (Java: an
///   IllegalStateException from Coordinate.normalize()).
/// - Logging is not ported.
class AbstractEulerStepper : public AbstractSimulationStepper
{
public:
    ~AbstractEulerStepper() override = default;

    AbstractEulerStepper& operator=(const AbstractEulerStepper&) = delete;
    AbstractEulerStepper& operator=(AbstractEulerStepper&&)      = delete;

    /// The tentative time step of a descent, s (RECOVERY_TIME_STEP).
    static constexpr double kRecoveryTimeStep = 0.5;

    /// Returns @p status as it is.
    [[nodiscard]] SimulationStatus initialize(SimulationStatus status) override;

    /// The drag coefficient of the falling rocket, relative to the reference area of the
    /// status's configuration.
    [[nodiscard]] virtual double computeCD(const SimulationStatus& status) = 0;

    /// One Euler step of at most @p maxTimeStep (see the class comment); a NaN @p maxTimeStep
    /// records the status and the step's data and sets the landed values.
    /// @throws SimulationException from a simulation listener
    void step(SimulationStatus& status, double maxTimeStep) override;

    /// The acceleration of @p status into @p store: forces with the drag coefficient
    /// computeCD() (as the total, axial and pressure drag; friction and base drag 0; nothing
    /// else set), which the `post` aerodynamic listeners may adjust as for the Runge-Kutta
    /// steppers; the drag force against the airspeed; the masses (an ACTIVE_MASS_ZERO abort
    /// when the rocket weighs less than MathUtil::kEpsilon); gravity and the Coriolis
    /// acceleration. There is no rotational acceleration and no thrust.
    /// @throws BugError when the store has no flight conditions
    void calculateAcceleration(SimulationStatus& status, DataStore& store) override;

    /// The store of the stepper (Java: the package-private field `store`).
    [[nodiscard]] DataStore&       getStore() noexcept { return m_store; }
    [[nodiscard]] const DataStore& getStore() const noexcept { return m_store; }

protected:
    AbstractEulerStepper()                            = default;
    AbstractEulerStepper(const AbstractEulerStepper&) = default;
    AbstractEulerStepper(AbstractEulerStepper&&)      = default;

private:
    /// The position and the velocity an Euler step gives (Java: EulerValues).
    struct EulerValues
    {
        /** linear velocity */
        Coordinate vel;
        /** position */
        Coordinate pos;
    };

    /// vel = v + a t and pos = pos + v t + a t^2 / 2.
    [[nodiscard]] static EulerValues eulerIntegrate(const Coordinate& pos, const Coordinate& v,
                                                    const Coordinate& a, double timeStep);

    /// The time step that ends where z or one of its first two derivatives changes sign, when
    /// the step of the store's length to @p newVals would cross such a point; else the store's
    /// time step.
    [[nodiscard]] double timeToSignChange(const SimulationStatus& status,
                                          const Coordinate&       linearAcceleration,
                                          const EulerValues&      newVals);

    DataStore m_store;
};

}  // namespace QtRocket
