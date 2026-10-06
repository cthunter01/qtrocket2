#pragma once

#include <format>
#include <limits>
#include <optional>
#include <string_view>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/mass/RigidBody.h"
#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/simulation/AccelerationData.h"
#include "QtRocket/simulation/SimulationStepper.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Quaternion.h"
#include "QtRocket/util/Rotation2D.h"

namespace QtRocket
{

class FlightDataBranch;
class SimulationConditions;
class SimulationStatus;

/// What the simulation steppers share (OpenRocket's simulation/AbstractSimulationStepper): the
/// flight conditions of a status, the atmosphere, the wind and the gravity it flies in, the
/// mass of the rocket, each of which the simulation listeners may override or adjust, the
/// values of a rocket lying on the ground, and the DataStore, which holds what a step computed
/// and writes it to the flight data branch.
///
/// The hooks: every model is asked through SimulationListenerHelper, a `pre` hook that may
/// answer instead of the model and a `post` hook that may change the model's answer. The order
/// of the calls is Java's, since a listener can see it.
///
/// The structure mass is cached: it does not change between stage separations, so it is
/// recomputed only when the modification id of the status's flight configuration changes. The
/// cache belongs to the stepper, not to the status: the engine uses one stepper for every
/// branch of a flight. A listener's `pre` answer bypasses the cache and is not stored in it.
///
/// Threads: a stepper belongs to one simulation, on one thread. It has no static state.
///
/// Deviations from OpenRocket:
/// - What Java reaches through protected or package-private access is public here, because the
///   tests (OpenRocket's and this port's) call it: the DataStore and its fields,
///   calculateAcceleration(), calculateFlightConditions(), the model and mass functions,
///   checkNaN() and landedValues(), and MIN_TIME_STEP.
/// - The fields of the DataStore that Java leaves null are empty optionals. Reading an empty one
///   where Java would throw a NullPointerException is a BugError, and so are a status without
///   simulation conditions and conditions without the model or calculator that is asked.
/// - checkNaN() throws BugError (Java: BugException) with Java's message; a coordinate and a
///   quaternion are printed by this port's toString() (see Quaternion::toString() for how it
///   differs from Java's).
/// - Java's scratch MutableCoordinates are local Coordinates: the arithmetic of the mutable
///   operations is that of the immutable ones (the weights included).
/// - The thrusting nozzle exit areas are summed per assembly in the order of the status's
///   active motors (Java: a HashMap); see FlightConditions for the order of the areas.
class AbstractSimulationStepper : public SimulationStepper
{
public:
    ~AbstractSimulationStepper() override = default;

    AbstractSimulationStepper& operator=(const AbstractSimulationStepper&) = delete;
    AbstractSimulationStepper& operator=(AbstractSimulationStepper&&)      = delete;

    /// The smallest time step a stepper takes, s (MIN_TIME_STEP).
    static constexpr double kMinTimeStep = 0.001;

    /// The calculated data that is used in computing a simulation step (Java: DataStore). It is
    /// saved to the flight data branch at the beginning of the time step, and one extra time
    /// following the final simulation step, so that there is a full set of data for the final
    /// step.
    ///
    /// As OpenRocket notes, it is a little shady to save this data only at the start of a
    /// Runge-Kutta step, since the contents change over the course of a step.
    struct DataStore
    {
        /// The damping moment coefficient with its two parts (Java: the record
        /// DampingMomentComponents).
        struct DampingMomentComponents
        {
            double total;        ///< aerodynamic + propulsive
            double aerodynamic;  ///< the aerodynamic component
            double propulsive;   ///< the propulsive (jet damping) component
        };

        double timeStep{kNaN};

        std::optional<AccelerationData> accelerationData;

        std::optional<FlightConditions> flightConditions;

        std::optional<RigidBody> rocketMass;

        std::optional<RigidBody> motorMass;

        std::optional<Coordinate> coriolisAcceleration;

        std::optional<Coordinate> launchRodDirection;

        // set by calculateFlightConditions and calculateAcceleration:
        std::optional<AerodynamicForces> forces;
        Coordinate                       windVelocity{kNaN, kNaN, kNaN};
        double                           gravity{kNaN};
        double                           thrustForce{kNaN};
        double                           thrustCorrection{kNaN};
        double                           dragForce{kNaN};
        double                           lateralPitchRate{kNaN};

        std::optional<Rotation2D> thetaRotation;

        /// Writes the data to the last point of @p status's flight data branch (the point
        /// SimulationStatus::storeData() opened), in this order, which is the order of the
        /// columns of a new branch after those of the status:
        /// - the thrust, the thrust correction, the gravity and the drag force;
        /// - the wind speed and direction (getWindDirection()) and the time step;
        /// - with a Coriolis acceleration: its magnitude;
        /// - with acceleration data: the lateral, total, z, x and y acceleration in world
        ///   coordinates and the z, x and y acceleration in rocket coordinates;
        /// - with a rocket mass: the thrust-to-weight ratio (thrust / (mass * gravity)), the CG,
        ///   the mass and the longitudinal and rotational inertia;
        /// - with a motor mass: it;
        /// - with flight conditions: the Reynolds number (velocity * aerodynamic length of the
        ///   configuration / kinematic viscosity), the Mach number, the reference length and
        ///   area, the pitch, yaw and roll rate, the angle of attack, and the temperature,
        ///   pressure, density and speed of sound of the air;
        /// - the damping moment coefficient with its aerodynamic and propulsive part, the
        ///   corrective moment coefficient, the damping ratio and the natural frequency (see
        ///   the compute functions below);
        /// - with forces: the total, axial, friction, pressure and base drag coefficient;
        /// - with forces, once the launch rod is cleared: the CP and CNa, the normal force, side
        ///   force, roll moment, roll forcing, roll damping and pitch damping moment
        ///   coefficient, and, with a rocket mass and flight conditions, the stability margin
        ///   in calibres and the pitch and yaw moment coefficient about the CG.
        /// @throws BugError when @p status has no flight data branch, or the branch is immutable
        void storeData(SimulationStatus& status) const;

        /// The damping moment coefficient (Cdm) and its parts: all NaN without flight conditions
        /// or a rocket mass; all 0 until the launch rod is cleared; all NaN when the angle of
        /// attack is NaN (on the ground, where landedValues() sets it so); otherwise the
        /// aerodynamic and the propulsive part and their sum. Despite its name the quantity has
        /// the units of an angular momentum per angular velocity; it is meant for analysis (Peak
        /// of Flight issue 195).
        [[nodiscard]] DampingMomentComponents computeDampingMomentCoefficientComponents(
            const SimulationStatus& status, const FlightDataBranch& dataBranch) const;

        /// The aerodynamic part of the damping moment coefficient: 0.5 * density * velocity *
        /// reference area * the sum, over the aerodynamic components of a force analysis in the
        /// flight conditions, of CNa * (CP - CG)^2.
        /// @throws BugError without flight conditions or a rocket mass
        [[nodiscard]] double computeAerodynamicDampingMomentCoefficient(
            const SimulationStatus& status) const;

        /// The propulsive (jet damping) part of the damping moment coefficient: the mass
        /// expulsion rate, -computeMotorMassDerivative(), times the square of the distance from
        /// the CG to the furthest-aft nozzle of the configuration's active motors (the largest
        /// MotorConfiguration::getX() + Motor::getLaunchCGx(), at least 0); 0 when the
        /// derivative is NaN.
        /// @throws BugError without a rocket mass
        [[nodiscard]] double computePropulsiveDampingMomentCoefficient(
            const SimulationStatus& status, const FlightDataBranch& dataBranch) const;

        /// The time derivative of the motor mass, from the last two points of @p dataBranch:
        /// NaN when the branch lacks the motor mass or the time, has fewer than two points, or
        /// the time between the two is not positive.
        [[nodiscard]] static double computeMotorMassDerivative(const FlightDataBranch& dataBranch);

        /// The corrective moment coefficient (Ccm), a torque: NaN without flight conditions, a
        /// rocket mass or forces; 0 until the launch rod is cleared; NaN when the angle of
        /// attack is NaN; otherwise 0.5 * density * velocity^2 * reference area * CNa *
        /// (CP - CG) (Peak of Flight issue 193).
        [[nodiscard]] double computeCorrectiveMomentCoefficient(
            const SimulationStatus& status) const;

        /// The damping ratio (zeta): NaN without a rocket mass; 0 until the launch rod is
        /// cleared; NaN when a coefficient is NaN, the longitudinal inertia is not positive or
        /// the corrective coefficient times the inertia is not positive; otherwise
        /// @p dampingMomentCoefficient / (2 * sqrt(@p correctiveMomentCoefficient * inertia))
        /// (Peak of Flight issue 197).
        [[nodiscard]] double computeDampingRatio(const SimulationStatus& status,
                                                 double                  dampingMomentCoefficient,
                                                 double correctiveMomentCoefficient) const;

        /// The natural frequency (wn), rad/s: 0 until the launch rod is cleared; NaN without a
        /// rocket mass, for a NaN coefficient, a longitudinal inertia that is not positive or a
        /// negative ratio; otherwise sqrt(@p correctiveMomentCoefficient / inertia) (Peak of
        /// Flight issue 196).
        [[nodiscard]] double computeNaturalFrequency(const SimulationStatus& status,
                                                     double correctiveMomentCoefficient) const;

        /// The direction of @p windVector in the horizontal plane, rad: 0 is north, pi / 2 east
        /// (atan2(x, y), reduced to 0 ... 2 pi).
        [[nodiscard]] static double getWindDirection(const Coordinate& windVector);
    };

    /// Calculates the acceleration of @p status into @p store (Java: abstract, package-private).
    /// @throws SimulationException from a simulation listener
    virtual void calculateAcceleration(SimulationStatus& status, DataStore& store) = 0;

    /// Calculates the flight conditions of @p status into @p store; the listeners can override
    /// them. It also sets the store's thetaRotation and lateralPitchRate, which can be used
    /// after the call, and (unless a `pre` listener answered) its windVelocity.
    ///
    /// A `pre` listener's conditions are taken as they are, with thetaRotation made from their
    /// theta and the lateral pitch rate std::hypot(pitch rate, yaw rate). Otherwise: the
    /// atmospheric conditions (modelAtmosphericConditions()), the wind (modelWindVelocity()),
    /// and from the airspeed in rocket coordinates (the rocket velocity plus the wind, rotated
    /// back by the orientation) the lateral direction theta (0 below a lateral airspeed of
    /// 0.0001 m/s), the velocity, the angle of attack (acos(z / velocity) with the sine
    /// lateral / velocity; 0 at or below 0.01 m/s) and the roll, pitch and yaw rate from the
    /// rotation velocity in rocket coordinates (pitch and yaw 0 below a lateral airspeed of
    /// 0.001 m/s). Then the `post` listeners; when one replaces the conditions, thetaRotation
    /// and the lateral pitch rate are made from the new conditions as for a `pre` answer.
    ///
    /// The nozzle exit areas of the motors that are thrusting (per component assembly; an
    /// unknown nozzle diameter, 0, contributes nothing) are set before the `post` hook and
    /// again after it, so that no listener's conditions go without them.
    /// @throws SimulationException from a simulation listener
    void calculateFlightConditions(SimulationStatus& status, DataStore& store);

    /// The atmospheric conditions to use, which listeners may override: those of the
    /// conditions' atmospheric model at the altitude of the rocket above sea level (the z
    /// position plus the altitude of the launch site).
    /// @throws SimulationException from a simulation listener
    /// @throws BugError when the pressure or the temperature is NaN
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static): Java's instance method
    [[nodiscard]] AtmosphericConditions modelAtmosphericConditions(SimulationStatus& status);

    /// The wind to use, which listeners may override: that of the conditions' wind model at the
    /// simulation time and the altitude of the rocket (above sea level and above ground).
    /// @throws SimulationException from a simulation listener
    /// @throws BugError when the wind has a NaN
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static): Java's instance method
    [[nodiscard]] Coordinate modelWindVelocity(SimulationStatus& status);

    /// The gravitational acceleration to use, which listeners may override: that of the
    /// conditions' gravity model at the world position of the rocket.
    /// @throws SimulationException from a simulation listener
    /// @throws BugError when the gravity is NaN
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static): Java's instance method
    [[nodiscard]] double modelGravity(SimulationStatus& status);

    /// The mass data of the structure, which listeners may override: MassCalculator's
    /// structure of the status's configuration, cached by the configuration's modification id
    /// (see the class comment).
    /// @throws SimulationException from a simulation listener
    /// @throws BugError when the centre of mass or an inertia is NaN
    [[nodiscard]] RigidBody calculateStructureMass(SimulationStatus& status);

    /// The mass data of the motors at the simulation time, which listeners may override (the
    /// same hooks as for the structure mass).
    /// @throws SimulationException from a simulation listener
    /// @throws BugError when the centre of mass or an inertia is NaN
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static): Java's instance method
    [[nodiscard]] RigidBody calculateMotorMass(SimulationStatus& status);

    /// Checks that @p d is not NaN.
    /// @throws BugError "Simulation resulted in not-a-number (NaN) value for <var>, please
    ///         report a bug."
    static void checkNaN(double d, std::string_view var);

    /// Checks that no component of @p c (the weight included) is NaN.
    /// @throws BugError "Simulation resulted in not-a-number (NaN) value for <var>, please
    ///         report a bug, c=<c>"
    static void checkNaN(const Coordinate& c, std::string_view var);

    /// Checks that no component of @p q is NaN.
    /// @throws BugError "Simulation resulted in not-a-number (NaN) value for <var>, please
    ///         report a bug, q=<q>"
    static void checkNaN(const Quaternion& q, std::string_view var);

    /// Sets @p status and @p store to values consistent with sitting on the ground: the time
    /// step NaN; the flight conditions of the status with a NaN angle of attack and no
    /// rotation; forces whose drag coefficients are NaN (and the rest unset); the masses; the
    /// gravity; no thrust, thrust correction, drag or Coriolis acceleration; zero acceleration
    /// data; and the status at altitude 0 with zero velocity.
    /// @throws SimulationException from a simulation listener
    void landedValues(SimulationStatus& status, DataStore& store);

protected:
    AbstractSimulationStepper()                                 = default;
    AbstractSimulationStepper(const AbstractSimulationStepper&) = default;
    AbstractSimulationStepper(AbstractSimulationStepper&&)      = default;

    /// The simulation conditions of @p status.
    /// @throws BugError when the status has none (Java: NullPointerException)
    [[nodiscard]] static const SimulationConditions& conditionsOf(const SimulationStatus& status);

    /// The value of the data store field @p field, which a step needs at this point.
    /// @throws BugError "The data store has no <what>" when it is empty (Java:
    ///         NullPointerException)
    template <class T>
    [[nodiscard]] static T& required(std::optional<T>& field, std::string_view what)
    {
        if (!field.has_value())
        {
            bug(std::format("The data store has no {}", what));
        }
        return *field;
    }

private:
    static constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

    /// Sets the nozzle exit areas of the thrusting motors of @p status in @p conditions: the
    /// first-order powered base-drag correction of OpenRocket's technical documentation. A zero
    /// nozzle exit diameter means that the geometry is unknown and leaves the legacy base-drag
    /// calculation unchanged.
    static void setThrustingNozzleExitAreas(const SimulationStatus& status,
                                            FlightConditions&       conditions);

    // Structural mass does not change between stage-separation events, so cache it
    // and recompute only when the FlightConfiguration's ModID changes.
    std::optional<RigidBody> m_cachedStructureMass;
    ModId                    m_cachedStructureConfigModId{ModId::invalid()};
};

}  // namespace QtRocket
