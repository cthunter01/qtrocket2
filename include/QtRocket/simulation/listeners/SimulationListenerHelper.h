#pragma once

#include <optional>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/mass/RigidBody.h"
#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/simulation/AccelerationData.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket
{

class FlightEvent;
class MotorClusterState;
class MotorConfigurationId;
class MotorMount;
class RecoveryDevice;
class SimulationException;
class SimulationStatus;

/// Fires the hooks of the simulation listeners (OpenRocket's
/// simulation/listeners/SimulationListenerHelper): the engine and the steppers call one of
/// these functions wherever the listeners of a simulation have a say.
///
/// Which listeners: those of the status's own conditions,
/// status.getSimulationConditions()->getSimulationListenerList(), in the order of the list. The
/// conditions are looked up once, when the function is called: a listener that gives the status
/// other conditions does not change which listeners the call in progress reaches. The hooks of
/// SimulationListener go to every listener; those of SimulationEventListener and of
/// SimulationComputationListener only to the listeners that implement that interface
/// (dynamic_cast; Java: instanceof).
///
/// How the answers are combined:
/// - the start, end and postStep() hooks: every listener is called;
/// - firePreStep(), fireAddFlightEvent(), fireHandleFlightEvent(), fireMotorIgnition() and
///   fireRecoveryDeviceDeployment(): the listeners are called until one returns false, which
///   is the result (the listeners after it are not called); true when none does;
/// - the `pre` computation hooks: the listeners are called until one returns a value (not
///   nullopt; for gravity and thrust: not NaN), which is the result; nullopt or NaN when none
///   does;
/// - the `post` computation hooks: every listener is called with the value as the listeners
///   before it left it, and replaces it when it returns a value (not nullopt; not NaN) that
///   differs from it: by operator== of the type, and for gravity and thrust by
///   MathUtil::equals(). The result is the last value.
///
/// The "listeners affected the simulation" warning: the modification id of the status is
/// compared around every hook; a listener that changed the status (through one of the setters
/// that draw an id, see SimulationStatus) gets Warning::kListenersAffected added to the status
/// (SimulationStatus::addWarning()), unless it is a system listener. So does a listener whose
/// answer changes the course of the simulation: a false, a `pre` value, a `post` value that
/// differs. A listener that does both is warned about twice, which the warning set holds once.
///
/// Exceptions: a SimulationException thrown by a hook leaves the function at once; the
/// listeners after it are not called. fireEndSimulation() and fireEndSimulationBranch() pass
/// the exception that ended the simulation to the listeners (null: a normal ending).
///
/// A listener must not change the listener list it is being called from. Java's loop then
/// throws a ConcurrentModificationException, and ends quietly when the change happens to leave
/// the list as long as the number of listeners already called; here the first is a BugError
/// and the second is reproduced. A change that keeps the length of the list goes unnoticed
/// (Java notices an add and a remove).
///
/// Deviations from OpenRocket:
/// - BugError where Java throws a NullPointerException: for a status without conditions, a
///   null listener in the list, and a warning that is to be added to a status without a flight
///   data branch (see SimulationStatus::addWarning()).
/// - The `post` hooks are given the current value by reference to const and return a changed
///   copy (Java hands them a clone they may change and return), so the clones Java makes for
///   them are not needed.
class SimulationListenerHelper
{
public:
    //////// SimulationListener methods ////////

    /// Fires startSimulation().
    /// @throws SimulationException from a listener
    static void fireStartSimulation(SimulationStatus& status);

    /// Fires endSimulation() with @p exception (null: a normal ending).
    static void fireEndSimulation(SimulationStatus& status, const SimulationException* exception);

    /// Fires startSimulationBranch().
    /// @throws SimulationException from a listener
    static void fireStartSimulationBranch(SimulationStatus& status);

    /// Fires endSimulationBranch() with @p exception (null: a normal ending).
    static void fireEndSimulationBranch(SimulationStatus&          status,
                                        const SimulationException* exception);

    /// Fires preStep(). True to handle the step normally, false to skip the step.
    /// @throws SimulationException from a listener
    [[nodiscard]] static bool firePreStep(SimulationStatus& status);

    /// Fires postStep().
    /// @throws SimulationException from a listener
    static void firePostStep(SimulationStatus& status);

    //////// SimulationEventListener methods ////////

    /// Fires addFlightEvent(). True to add the event normally, false to skip adding it.
    /// @throws SimulationException from a listener
    [[nodiscard]] static bool fireAddFlightEvent(SimulationStatus&  status,
                                                 const FlightEvent& event);

    /// Fires handleFlightEvent(). True to handle the event normally, false to skip it.
    /// @throws SimulationException from a listener
    [[nodiscard]] static bool fireHandleFlightEvent(SimulationStatus&  status,
                                                    const FlightEvent& event);

    /// Fires motorIgnition(). True to ignite the motor, false to skip the ignition.
    /// @throws SimulationException from a listener
    [[nodiscard]] static bool fireMotorIgnition(SimulationStatus&           status,
                                                const MotorConfigurationId& motorId,
                                                const MotorMount&           mount,
                                                MotorClusterState&          instance);

    /// Fires recoveryDeviceDeployment(). True to deploy the device, false to skip it.
    /// @throws SimulationException from a listener
    [[nodiscard]] static bool fireRecoveryDeviceDeployment(SimulationStatus&     status,
                                                           const RecoveryDevice& device);

    //////// SimulationComputationListener methods ////////

    /// Fires preAtmosphericModel(). nullopt normally, or the overriding atmospheric conditions.
    /// @throws SimulationException from a listener (as every function below)
    [[nodiscard]] static std::optional<AtmosphericConditions> firePreAtmosphericModel(
        SimulationStatus& status);

    /// Fires postAtmosphericModel(). The atmospheric conditions to use.
    [[nodiscard]] static AtmosphericConditions firePostAtmosphericModel(
        SimulationStatus& status, const AtmosphericConditions& conditions);

    /// Fires preWindModel(). nullopt normally, or the overriding wind.
    [[nodiscard]] static std::optional<Coordinate> firePreWindModel(SimulationStatus& status);

    /// Fires postWindModel(). The wind to use.
    [[nodiscard]] static Coordinate firePostWindModel(SimulationStatus& status,
                                                      const Coordinate& wind);

    /// Fires preGravityModel(). NaN normally, or the overriding gravity.
    [[nodiscard]] static double firePreGravityModel(SimulationStatus& status);

    /// Fires postGravityModel(). The gravity to use.
    [[nodiscard]] static double firePostGravityModel(SimulationStatus& status, double gravity);

    /// Fires preFlightConditions(). nullopt normally, or the overriding flight conditions.
    [[nodiscard]] static std::optional<FlightConditions> firePreFlightConditions(
        SimulationStatus& status);

    /// Fires postFlightConditions(). The flight conditions to use: @p conditions, or the
    /// modified conditions of a listener. Taken by value, so that a caller can move its
    /// conditions in and take them back (the other values are cheap to copy).
    [[nodiscard]] static FlightConditions firePostFlightConditions(SimulationStatus& status,
                                                                   FlightConditions  conditions);

    /// firePostFlightConditions(status, conditions), which also says in @p replaced whether a
    /// listener's conditions replaced the given ones. Java's caller sees that by comparing the
    /// returned object with the one it passed (AbstractSimulationStepper: `c !=
    /// store.flightConditions`), which is true as soon as one listener's conditions were taken,
    /// even if a later listener's then equal the original ones; a comparison of the values
    /// could not tell that case.
    [[nodiscard]] static FlightConditions firePostFlightConditions(SimulationStatus& status,
                                                                   FlightConditions  conditions,
                                                                   bool&             replaced);

    /// Fires preAerodynamicCalculation(). nullopt normally, or the overriding aerodynamic
    /// forces.
    [[nodiscard]] static std::optional<AerodynamicForces> firePreAerodynamicCalculation(
        SimulationStatus& status);

    /// Fires postAerodynamicCalculation(). The aerodynamic forces to use.
    [[nodiscard]] static AerodynamicForces firePostAerodynamicCalculation(
        SimulationStatus& status, const AerodynamicForces& forces);

    /// Fires preMassCalculation(). nullopt normally, or the overriding mass data.
    [[nodiscard]] static std::optional<RigidBody> firePreMassCalculation(SimulationStatus& status);

    /// Fires postMassCalculation(). The resultant mass data.
    [[nodiscard]] static RigidBody firePostMassCalculation(SimulationStatus& status,
                                                           const RigidBody&  mass);

    /// Fires preSimpleThrustCalculation(). NaN normally, or the overriding thrust.
    [[nodiscard]] static double firePreThrustCalculation(SimulationStatus& status);

    /// Fires postSimpleThrustCalculation(). The thrust to use.
    [[nodiscard]] static double firePostThrustCalculation(SimulationStatus& status, double thrust);

    /// Fires preAccelerationCalculation(). nullopt normally, or the overriding acceleration.
    [[nodiscard]] static std::optional<AccelerationData> firePreAccelerationCalculation(
        SimulationStatus& status);

    /// Fires postAccelerationCalculation(). The acceleration to use.
    [[nodiscard]] static AccelerationData firePostAccelerationCalculation(
        SimulationStatus& status, const AccelerationData& acceleration);
};

}  // namespace QtRocket
