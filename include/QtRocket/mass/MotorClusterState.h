#pragma once

#include <memory>
#include <string>

#include "QtRocket/mass/ThrustState.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/MotorConfigurationId.h"
#include "QtRocket/rocket/MotorConfiguration.h"

namespace QtRocket
{

class MotorMount;

/// The state of one motor cluster during a simulation (OpenRocket's
/// simulation/MotorClusterState): the motor configuration of one mount, the number of physical
/// motors it stands for, and when the motors ignite, burn out and fire their ejection charges.
/// The simulation makes one per motor of its flight configuration (SimulationStatus), drives it
/// through ignite(), burnOut() and expend() from its flight events, and asks it for the thrust;
/// MassCalculation asks it for the time since ignition (getMotorTime()).
///
/// States (ThrustState): ARMED until ignite(), THRUSTING until burnOut(), then DELAYING until
/// expend(), then SPENT; a plugged motor (no ejection charge) goes straight to SPENT on
/// burnOut(), whatever state it was in. A transition requested from any other state is ignored
/// (Java had a commented-out warning there). The times are +infinity until the event happens
/// (reset()).
///
/// Identity: Java shares each state object among its holders. SimulationStatus's copy
/// constructor (a new branch at stage separation) adds the parent's states to the new branch, and
/// the copied event queue's IGNITION, BURNOUT and EJECTION_CHARGE events carry those same
/// objects, so burnOut() and expend() on a state are seen by every branch and every queued event.
/// A state here is a copyable value, but the simulation must not copy states per branch: it keeps
/// each one at a stable address shared between the branches as Java shares it, by
/// std::shared_ptr<MotorClusterState>: in SimulationStatus's list of motor states and as the
/// data of the flight events (FlightEvent::Data), which keep the state alive after the status
/// is gone (the events of a finished simulation still carry it). MassCalculator is passed
/// pointers to them.
///
/// Placement: OpenRocket keeps this class in its simulation package. It lives in mass/ here
/// because MassCalculation needs it and simulation/ builds on mass/ (simulation/ includes mass/,
/// never the reverse); it needs only motor/ and rocket/.
///
/// Deviations from OpenRocket:
/// - The motor configuration is copied (Java keeps a reference to the mount's object):
///   FlightConfiguration::getAllMotors() hands out copies that its next update replaces. The
///   copy refers to the same mount, which must outlive this state. A later edit of the mount's
///   configuration (by a simulation listener, say) is therefore not seen by isPlugged(),
///   getEjectionDelay() or getIgnitionEvent(), where Java reads the live object.
/// - testForIgnition(FlightConfiguration, FlightEvent) is not here: it needs the simulation's
///   FlightEvent, which lives above mass/. It is FlightEventActivation::testForIgnition(state,
///   config, event) in simulation/FlightEventActivation.h, made from getIgnitionEvent() and
///   getMount() (the rules are in IgnitionEvent.h).
/// - A configuration without a motor is a BugError (Java: NullPointerException).
/// - getThrustDuration(), getEjectionTime() and getState() are additions: Java reads its
///   protected fields directly.
class MotorClusterState
{
public:
    /// The state of @p config's motor cluster, ARMED: the motor count is the number of absolute
    /// locations of the mount (its own instances times its parents'), the thrust duration the
    /// motor's burn time estimate.
    /// @throws BugError when @p config has no motor.
    explicit MotorClusterState(const MotorConfiguration& config);

    /// The ignition time (simulation time, s); +infinity until ignite().
    [[nodiscard]] double getIgnitionTime() const noexcept { return m_ignitionTime; }

    /// The configuration's ignition event.
    [[nodiscard]] IgnitionEvent getIgnitionEvent() const noexcept
    {
        return m_config.getIgnitionEvent();
    }

    /// Lights the motors at @p ignitionTime: ARMED becomes THRUSTING; ignored in any other state.
    void ignite(double ignitionTime) noexcept;

    /// Burns the motors out at @p burnOutTime: THRUSTING becomes DELAYING (ignored in any other
    /// state); then a plugged motor becomes SPENT, from any state.
    void burnOut(double burnOutTime) noexcept;

    /// Fires the ejection charges at @p ejectionTime: DELAYING becomes SPENT; ignored in any
    /// other state.
    void expend(double ejectionTime) noexcept;

    /// The motor's burn time (Motor::getBurnTime(), the time of the last thrust sample).
    [[nodiscard]] double getBurnTime() const { return m_motor->getBurnTime(); }

    /// burnOut(@p cutoffTime).
    void cutOff(double cutoffTime) noexcept { burnOut(cutoffTime); }

    /// The motor configuration (this state's copy).
    [[nodiscard]] const MotorConfiguration& getConfig() const noexcept { return m_config; }

    /// The configuration's ejection delay (Motor::kPluggedDelay for a plugged motor).
    [[nodiscard]] double getEjectionDelay() const noexcept { return m_config.getEjectionDelay(); }

    /// The configuration's id (Java: getID()).
    [[nodiscard]] const MotorConfigurationId& getId() const noexcept { return m_config.getId(); }

    /// The propellant mass: launch mass minus burnout mass.
    [[nodiscard]] double getPropellantMass() const;

    /// Motor::getPropellantMass(@p motorTime) minus the burnout mass, as Java computes it.
    [[nodiscard]] double getPropellantMass(double motorTime) const;

    /// The mount of the motors.
    [[nodiscard]] MotorMount&       getMount() noexcept { return m_config.getMount(); }
    [[nodiscard]] const MotorMount& getMount() const noexcept { return m_config.getMount(); }

    /// The motor (never null).
    [[nodiscard]] const std::shared_ptr<const Motor>& getMotor() const noexcept { return m_motor; }

    /// The number of physical motors this state stands for, the copies made by the mount's
    /// parent assemblies included.
    [[nodiscard]] int getMotorCount() const noexcept { return m_motorCount; }

    /// The configuration's nozzle exit diameter in m, 0 when it is unknown.
    [[nodiscard]] double getNozzleExitDiameter() const noexcept
    {
        return m_config.getNozzleExitDiameter();
    }

    /// The burnout (cutoff) time (simulation time, s); +infinity until burnOut().
    [[nodiscard]] double getCutOffTime() const noexcept { return m_cutoffTime; }

    /// The ejection time (simulation time, s); +infinity until expend().
    [[nodiscard]] double getEjectionTime() const noexcept { return m_ejectionTime; }

    /// The motor's burn time estimate, taken when this state was made (Java's thrustDuration,
    /// which nothing reads there).
    [[nodiscard]] double getThrustDuration() const noexcept { return m_thrustDuration; }

    /// The current state.
    [[nodiscard]] ThrustState getState() const noexcept { return m_currentState; }

    /// The time since ignition at @p simulationTime: simulationTime - getIgnitionTime(), and 0
    /// before the ignition (Java's Math.max(.., 0.0), so a NaN time gives NaN). Before ignite()
    /// the ignition time is +infinity, so this is 0.
    [[nodiscard]] double getMotorTime(double simulationTime) const noexcept;

    /// The thrust of the whole cluster at @p simulationTime (getMotorCount() times the motor's
    /// thrust at getMotorTime()) while THRUSTING, else 0.
    [[nodiscard]] double getThrust(double simulationTime) const;

    /// Whether the motors are between ignition and burnout. Preferable to a test of the thrust,
    /// since thrust curves normally start and end with zero.
    [[nodiscard]] bool isThrusting() const noexcept
    {
        return QtRocket::isThrusting(m_currentState);
    }

    /// Whether the motor is plugged: its ejection delay is Motor::kPluggedDelay.
    [[nodiscard]] bool isPlugged() const noexcept;

    /// !isPlugged().
    [[nodiscard]] bool hasEjectionCharge() const noexcept { return !isPlugged(); }

    [[nodiscard]] bool isDelaying() const noexcept
    {
        return m_currentState == ThrustState::DELAYING;
    }

    [[nodiscard]] bool isSpent() const noexcept { return m_currentState == ThrustState::SPENT; }

    /// Back to before the flight (Java: also "resetToPreflight"): every time +infinity, ARMED.
    void reset() noexcept;

    /// "<mount debug name, right-aligned in 32> / <designation, right-aligned in 4> - <state>"
    /// (Java's "%32s / %4s - %s"; the widths count UTF-16 code units, as Java's do).
    [[nodiscard]] std::string toDescription() const;

    /// The motor's designation (Java: toString()).
    [[nodiscard]] const std::string& toString() const { return m_motor->getDesignation(); }

private:
    MotorConfiguration           m_config;
    std::shared_ptr<const Motor> m_motor;
    int                          m_motorCount;
    double                       m_thrustDuration;

    double      m_ignitionTime{0.0};
    double      m_cutoffTime{0.0};
    double      m_ejectionTime{0.0};
    ThrustState m_currentState{ThrustState::ARMED};
};

}  // namespace QtRocket
