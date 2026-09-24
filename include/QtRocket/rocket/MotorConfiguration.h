#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/MotorConfigurationId.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/ModId.h"

namespace QtRocket
{

class MotorMount;
class Preferences;

/// The motor of one motor mount in one flight configuration (OpenRocket's
/// motor.MotorConfiguration): the motor, its ejection charge delay, the nozzle exit diameter, and
/// when it ignites. The value type of a mount's MotorConfigurationSet.
///
/// The mount is held by a non-owning pointer (the mount owns its set, and the set its
/// configurations); the motor is shared, since motors are immutable and the database, the
/// document and every configuration using one hold the same instance. The MotorConfigurationId
/// is derived from the mount's id and the flight configuration's key when the configuration is
/// made, as in Java (a configuration copied into a copied mount gets the copy's id).
///
/// Ignition: a new configuration ignites AUTOMATIC with no delay and without an ignition
/// override. A configuration made from a source (the three-argument constructor, which the .ork
/// loader calls with the mount's default configuration) starts from the source's ignition
/// settings: that is how a mount's default ignition reaches the configurations. setIgnitionEvent()
/// and setIgnitionDelay() mark the ignition as overridden; useDefaultIgnition() restores AUTOMATIC
/// with no delay, and, since it does so through those setters as Java does, leaves the ignition
/// marked as overridden. The motor configuration table in the GUI shows the default
/// configuration's ignition for a configuration without an override (OpenRocket's
/// MotorConfigurationPanel); nothing in the model falls back on it.
///
/// Change notification goes through the owning mount: the setters notify nobody, as in Java, and
/// whoever edits a mount's motors fires MotorConfigurationSet::kDefaultMotorEventType on the mount
/// (asComponent(MotorMount&)). Java's config listeners (addConfigListener() and friends, which
/// repeat every setter on the configurations of other selected mounts) are not ported, by decision
/// (see RocketComponent).
///
/// Deviations from OpenRocket:
/// - setNozzleExitDiameter() returns an Error (ErrorCode::INVALID_ARGUMENT) where Java throws
///   IllegalArgumentException, and leaves the diameter unchanged.
/// - getOffset() throws BugError without a motor (Java: NullPointerException).
/// - toMotorName(), toDescription() and toDebugDetail() take the Preferences that choose between
///   the designation and the common name (Motor::getMotorName()).
/// - getMotorCount() gives the mount's getMotorCount() for an inner tube (Java: its cluster
///   configuration's count, the same number) and 1 otherwise.
/// - Java's hashCode() is the mid's; equality is operator==.
class MotorConfiguration
{
public:
    /// The class name FlightConfigurableParameterSet::toDebug() prints.
    static constexpr std::string_view kTypeName = "MotorConfiguration";

    /// toMotorName() without a motor: Java looks up the translation key "empty", which does not
    /// exist, and gets the key back.
    static constexpr std::string_view kEmptyMotorName = "empty";

    /// An empty configuration (no motor) of @p mount for @p fcid.
    MotorConfiguration(MotorMount& mount, const FlightConfigurationId& fcid);

    /// A configuration of @p mount for @p fcid with @p source's motor, ejection delay, nozzle exit
    /// diameter and ignition settings (Java: a non-null source; pass none for a null one).
    MotorConfiguration(MotorMount& mount, const FlightConfigurationId& fcid,
                       const MotorConfiguration& source);

    /// Whether the ignition event or delay has been set on this configuration.
    [[nodiscard]] bool hasIgnitionOverride() const noexcept { return m_ignitionOverride; }

    /// The motor's name with the ejection delay ("M1350-0"), or kEmptyMotorName without a motor.
    [[nodiscard]] std::string toMotorName(const Preferences& preferences) const;

    /// The identifier of this configuration (Java: getID() and getMID()).
    [[nodiscard]] const MotorConfigurationId& getId() const noexcept { return m_mid; }
    [[nodiscard]] const MotorConfigurationId& getMid() const noexcept { return m_mid; }

    /// The flight configuration this configuration belongs to (Java: getFCID()).
    [[nodiscard]] const FlightConfigurationId& getFcid() const noexcept { return m_fcid; }

    /// Sets the motor (nullptr for none). A different motor resets the nozzle exit diameter,
    /// which belongs to the motor (motors compare by identity).
    void setMotor(std::shared_ptr<const Motor> motor);

    /// The motor, or nullptr.
    [[nodiscard]] const std::shared_ptr<const Motor>& getMotor() const noexcept { return m_motor; }

    /// The mount this configuration belongs to.
    [[nodiscard]] MotorMount& getMount() const noexcept { return *m_mount; }

    /// The ejection charge delay in s (Motor::kPluggedDelay for a plugged motor).
    [[nodiscard]] double getEjectionDelay() const noexcept { return m_ejectionDelay; }
    void                 setEjectionDelay(double delay) noexcept { m_ejectionDelay = delay; }

    /// The nozzle exit diameter in m, or 0 when it is unknown (which disables the powered base
    /// drag correction).
    [[nodiscard]] double getNozzleExitDiameter() const noexcept { return m_nozzleExitDiameter; }

    /// Sets the nozzle exit diameter; 0 means unknown.
    /// @return ErrorCode::INVALID_ARGUMENT, with Java's message, when @p diameter is not finite,
    ///         negative, or larger than the motor's diameter.
    [[nodiscard]] Result<void> setNozzleExitDiameter(double diameter);

    /// (getX(), 0, 0).
    [[nodiscard]] Coordinate getPosition() const;

    /// The x of the front of the motor relative to the mount: mount length - motor length +
    /// overhang; 0 without a motor.
    [[nodiscard]] double getX() const;

    /// AUTOMATIC with no delay, set through the setters (so the ignition stays overridden; see the
    /// class comment).
    void useDefaultIgnition();

    /// The ignition delay after the ignition event, in s.
    [[nodiscard]] double getIgnitionDelay() const noexcept { return m_ignitionDelay; }
    /// Sets the ignition delay and marks the ignition as overridden.
    void setIgnitionDelay(double delay) noexcept;

    [[nodiscard]] IgnitionEvent getIgnitionEvent() const noexcept { return m_ignitionEvent; }
    /// Sets the ignition event and marks the ignition as overridden.
    void setIgnitionEvent(IgnitionEvent event) noexcept;

    /// The number of motors: the cluster size for an inner tube, else 1.
    [[nodiscard]] int getMotorCount() const;

    /// (mount length + overhang - motor length, 0, 0).
    /// @throws BugError without a motor.
    [[nodiscard]] Coordinate getOffset() const;

    /// The motor's longitudinal unit inertia as a solid cylinder, or 0 without a motor.
    [[nodiscard]] double getUnitLongitudinalInertia() const;

    /// The motor's rotational unit inertia as a solid cylinder, or 0 without a motor.
    [[nodiscard]] double getUnitRotationalInertia() const;

    /// Launch mass minus burnout mass, or 0 without a motor.
    [[nodiscard]] double getPropellantMass() const;

    /// Whether there is no motor.
    [[nodiscard]] bool isEmpty() const noexcept { return m_motor == nullptr; }

    /// Whether there is a motor.
    [[nodiscard]] bool hasMotor() const noexcept { return !isEmpty(); }

    /// Java's equals(): the same MotorConfigurationId.
    [[nodiscard]] bool operator==(const MotorConfiguration& other) const noexcept
    {
        return m_mid == other.m_mid;
    }

    /// Java's hashCode(): the mid's.
    [[nodiscard]] std::int32_t hashCode() const noexcept { return m_mid.hashCode(); }

    /// An independent copy with the same mount and flight configuration (and a new modification
    /// id).
    [[nodiscard]] MotorConfiguration clone() const;

    /// A copy belonging to the flight configuration @p copyId (its mid follows).
    [[nodiscard]] MotorConfiguration copy(const FlightConfigurationId& copyId) const;

    /// Copies @p configuration's motor, ejection delay, nozzle exit diameter and ignition
    /// settings into this one (the mount, flight configuration and mid stay).
    void copyFrom(const MotorConfiguration& configuration);

    /// The modification id drawn when the configuration was made.
    [[nodiscard]] ModId getModId() const noexcept { return m_modId; }
    /// getModId() (the Monitorable concept).
    [[nodiscard]] ModId modId() const noexcept { return m_modId; }

    /// Nothing to update.
    void update() noexcept { }

    /// "<motor name> in: <mount debug name> ign@: <ignition description>".
    [[nodiscard]] std::string toDescription(const Preferences& preferences) const;

    /// "<event name> + <delay>s ", e.g. "AUTOMATIC + 0.0s " (the delay as Java prints a double).
    [[nodiscard]] std::string toIgnitionDescription() const;

    /// "[in: <mount>][fcid <key>][mid <key>][    <motor> ign@: <ignition>]" with Java's field
    /// widths.
    [[nodiscard]] std::string toDebugDetail(const Preferences& preferences) const;

private:
    MotorMount*                  m_mount;
    FlightConfigurationId        m_fcid;
    MotorConfigurationId         m_mid;
    std::shared_ptr<const Motor> m_motor;
    double                       m_ejectionDelay{0.0};
    double                       m_nozzleExitDiameter{0.0};
    double                       m_ignitionDelay{0.0};
    ModId                        m_modId;
    IgnitionEvent                m_ignitionEvent{IgnitionEvent::AUTOMATIC};
    bool                         m_ignitionOverride{false};
};

}  // namespace QtRocket
