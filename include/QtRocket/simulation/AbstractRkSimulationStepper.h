#pragma once

#include <cstdint>
#include <numbers>
#include <optional>
#include <span>
#include <string_view>

#include "QtRocket/simulation/AbstractSimulationStepper.h"
#include "QtRocket/simulation/AccelerationData.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/JavaRandom.h"

namespace QtRocket
{

class SimulationStatus;

/// The base of the Runge-Kutta simulation steppers (OpenRocket's
/// simulation/AbstractRKSimulationStepper): the six-degrees-of-freedom flight from the launch
/// rod to the deployment of a recovery device. It computes the acceleration of the rocket from
/// the aerodynamic forces, the thrust, gravity and the Coriolis acceleration, chooses the time
/// step, and gives Rk4SimulationStepper and Rk6SimulationStepper the pieces of their step().
///
/// The pitch and yaw jitter: after every aerodynamic calculation a small random amount, up to
/// plus or minus kPitchYawRandom, is added to the pitch moment coefficient and then to the yaw
/// moment coefficient, "to prevent over-perfect flight". It is drawn from a JavaRandom (Java:
/// java.util.Random) seeded in initialize() with the conditions' random seed XOR
/// kSeedRandomization, two draws per calculation, Cm first: a run with a given seed has the
/// jitter OpenRocket's run with that seed has. The sequence starts anew with every
/// initialize(), as in Java.
///
/// Deviations from OpenRocket:
/// - What Java reaches through protected or package-private access is public here, because the
///   tests call it: the store, computeTimeStep(), computeParameters(), calculateThrust(),
///   computeAcceleration(), calculateForces(), RKParameters and the step limits.
/// - Java's step() of the two steppers repeats the set-up of every intermediate status and the
///   end of the step line by line; here the three shared pieces are functions of this class
///   (startStep(), computeStage() and finishStep()) with Java's operations in Java's order, and
///   each stepper's step() lists its stages with Java's coefficients.
/// - "Stepping backwards in time" (a negative or NaN time step at the end of a step) is a
///   BugError (Java: IllegalArgumentException, which is not a SimulationException either).
/// - Calculating forces before initialize() is a BugError (Java: NullPointerException, the
///   random source being null).
/// - Java's RK6 stepper has no translation of "error.valuesTooLarge" and shows that key as its
///   message; here both steppers throw kValuesTooLarge, the text of the RK4 stepper.
/// - Logging is not ported.
class AbstractRkSimulationStepper : public AbstractSimulationStepper
{
public:
    ~AbstractRkSimulationStepper() override = default;

    AbstractRkSimulationStepper& operator=(const AbstractRkSimulationStepper&) = delete;
    AbstractRkSimulationStepper& operator=(AbstractRkSimulationStepper&&)      = delete;

    /// Random value with which to XOR the random seed value (SEED_RANDOMIZATION).
    static constexpr std::int32_t kSeedRandomization = 0x23E3A01F;

    /// A recommended reasonably accurate time step, s (RECOMMENDED_TIME_STEP).
    static constexpr double kRecommendedTimeStep = 0.05;

    /// A recommended reasonable maximum simulation time, s (RECOMMENDED_MAX_TIME).
    static constexpr double kRecommendedMaxTime = 1200;

    /// A recommended maximum angle step value, rad: 3 degrees, written as in Java
    /// (RECOMMENDED_ANGLE_STEP).
    static constexpr double kRecommendedAngleStep = 3 * std::numbers::pi / 180;

    /// A random amount that is added to pitch and yaw coefficients, plus or minus
    /// (PITCH_YAW_RANDOM).
    static constexpr double kPitchYawRandom = 0.0005;

    /// Maximum roll step allowed, rad. This is selected as an uneven division of the full
    /// circle so that the simulation will sample the most wind directions
    /// (MAX_ROLL_STEP_ANGLE).
    static constexpr double kMaxRollStepAngle = 2 * 28.32 * std::numbers::pi / 180;

    /// The largest change of the roll rate in one step, rad/s (MAX_ROLL_RATE_CHANGE).
    static constexpr double kMaxRollRateChange = 2 * std::numbers::pi / 180;

    /// The largest change of the pitch and yaw rate in one step, rad/s (MAX_PITCH_YAW_CHANGE).
    static constexpr double kMaxPitchYawChange = 4 * std::numbers::pi / 180;

    /// The message of the SimulationCalculationException a step throws when the velocity, the
    /// position or the rotation velocity runs out of range
    /// (RK4SimulationStepper.error.valuesTooLarge).
    static constexpr std::string_view kValuesTooLarge =
        "Simulation values exceeded limits.  Try selecting a shorter time step.";

    /// The derivatives at one point of a Runge-Kutta step (Java: RKParameters).
    struct RkParameters
    {
        /** Linear acceleration */
        Coordinate a;
        /** Linear velocity */
        Coordinate v;
        /** Rotational acceleration */
        Coordinate ra;
        /** Rotational velocity */
        Coordinate rv;
    };

    /// One term of the status of an intermediate point: the derivatives @p k of an earlier
    /// point, scaled by @p scale (the time step times the coefficient of the method).
    struct StageTerm
    {
        const RkParameters* k;
        double              scale;

        constexpr StageTerm(const RkParameters& parameters, double scaleValue) noexcept
          : k(&parameters), scale(scaleValue)
        {
        }
    };

    /// A status to integrate from: Java's copy of @p original (SimulationStatus's copy
    /// constructor: its own conditions with clones of the listeners, its own configuration)
    /// that keeps the original's warning set. Also sets the launch rod direction of the store
    /// from the conditions' rod angle and direction, and seeds the jitter's random source.
    [[nodiscard]] SimulationStatus initialize(SimulationStatus original) override;

    /// The acceleration of @p status into @p store.accelerationData: a `pre` listener's
    /// answer, else computeAcceleration(); then the `post` listeners.
    void calculateAcceleration(SimulationStatus& status, DataStore& store) override;

    /// The time step to take from @p status, where @p k1 are the derivatives at the start of
    /// the step. It is the smallest of
    /// - the conditions' time step, at least kMinTimeStep (a fifth of that on the launch rod);
    /// - @p maxTimeStep;
    /// - the conditions' maximum angle step / the store's lateral pitch rate;
    /// - kMaxRollStepAngle / the roll rate;
    /// - kMaxRollRateChange / the roll acceleration;
    /// - kMaxPitchYawChange / the larger of the pitch and yaw acceleration (the rotational
    ///   accelerations in rocket coordinates);
    /// - on the launch rod: the launch rod length / the velocity / 10;
    /// - 1.5 times the previous time step (the store's timeStep),
    /// where a limit that is NaN does not count. A step within a twentieth of the conditions'
    /// time step of @p maxTimeStep becomes @p maxTimeStep (the next scheduled event), and a
    /// step below that twentieth is raised to it.
    /// @throws BugError when the store has no flight conditions or acceleration data
    [[nodiscard]] double computeTimeStep(const SimulationStatus& status, double maxTimeStep,
                                         const RkParameters& k1);

    /// The derivatives at @p status: calculateAcceleration() into @p store, then the linear
    /// and rotational acceleration in world coordinates and the velocity and rotation velocity
    /// of the status.
    /// @throws BugError when one of them has a NaN
    [[nodiscard]] RkParameters computeParameters(SimulationStatus& status, DataStore& store);

    /// The thrust of the motors of the status's active stages at the simulation time, which
    /// listeners may override, corrected for the air pressure: thrust curves give the thrust at
    /// the standard pressure, and the total thrust is F0 + (P0 - Pa) * A with A the nozzle exit
    /// area of the thrusting motors (FlightConditions::getThrustingNozzleExitArea()). The
    /// correction is stored in @p store.thrustCorrection (0 without thrust or without a known
    /// nozzle area). As OpenRocket notes, the moments of off-centre motors are not taken into
    /// account.
    /// @throws BugError when the thrust is NaN, or the store has no flight conditions while
    ///         there is thrust
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static): Java's instance method
    [[nodiscard]] double calculateThrust(SimulationStatus& status, DataStore& store);

    /// The linear and angular acceleration at @p status: the masses (an ACTIVE_MASS_ZERO abort
    /// when the rocket weighs less than MathUtil::kEpsilon), calculateForces(), the drag,
    /// normal and side force from the coefficients, the thrust, the conversion to world
    /// coordinates, gravity and the Coriolis acceleration; then, before lift-off, no rotation
    /// and no sinking into the ground; on the launch rod, the projection on the rod and no
    /// rotation; in free flight, the moments about the CG and the angular acceleration.
    [[nodiscard]] AccelerationData computeAcceleration(SimulationStatus& status, DataStore& store);

    /// The aerodynamic forces at @p status into @p store.forces: a `pre` listener's answer as
    /// it is (the flight conditions of the store are then not recalculated); else the flight
    /// conditions, the calculator's forces (with warnings only while the status records them:
    /// not on the launch rod, nor for SimulationStatus::kWarningsWait after it, nor below 20 %
    /// of the maximum velocity), the jitter, and the `post` listeners.
    /// @throws BugError before initialize()
    void calculateForces(SimulationStatus& status, DataStore& store);

    /// The store of the stepper (Java: the package-private field `store`).
    [[nodiscard]] DataStore&       getStore() noexcept { return m_store; }
    [[nodiscard]] const DataStore& getStore() const noexcept { return m_store; }

protected:
    AbstractRkSimulationStepper()                                   = default;
    AbstractRkSimulationStepper(const AbstractRkSimulationStepper&) = default;
    AbstractRkSimulationStepper(AbstractRkSimulationStepper&&)      = default;

    /// The start of a step: stores the status to a new point of its flight data branch,
    /// calculates the flight conditions and the derivatives k1 at the status, chooses the time
    /// step, stores the step's data and checks the time step. For a NaN @p maxTimeStep (the
    /// extra step upon ground hit) it stores the data with a NaN time step, sets the landed
    /// values and returns nullopt: the step is over.
    [[nodiscard]] std::optional<RkParameters> startStep(SimulationStatus& status,
                                                        double            maxTimeStep);

    /// The derivatives at an intermediate point: a clone() of @p status at @p time, moved by
    /// @p terms (the position by each term's velocity, the velocity by its acceleration, the
    /// orientation by a rotation of its rotation velocity, multiplied from the left term by
    /// term, and the rotation velocity by its rotational acceleration, each scaled).
    [[nodiscard]] RkParameters computeStage(const SimulationStatus& status, double time,
                                            std::span<const StageTerm> terms);

    /// The end of a step: adds the deltas to the velocity, the position and the rotation
    /// velocity of @p status, rotates the orientation by @p deltaO (normalized if necessary),
    /// sets the world position through the geodetic computation, advances the simulation time
    /// by the store's time step, and verifies that the values did not run out of range.
    /// @throws SimulationCalculationException (kValuesTooLarge) when the square of the
    ///         velocity, the position or the rotation velocity exceeds 1e18
    /// @throws BugError when the time step is negative or NaN
    void finishStep(SimulationStatus& status, const Coordinate& deltaV, const Coordinate& deltaP,
                    const Coordinate& deltaR, const Coordinate& deltaO) const;

private:
    /// The jitter's random source; empty until initialize() (Java: null).
    std::optional<JavaRandom> m_random;
    DataStore                 m_store;
};

}  // namespace QtRocket
