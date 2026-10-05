#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/rocket/FlightConfigurationId.h"

namespace QtRocket
{

/// When a recovery device deploys in one flight configuration (OpenRocket's
/// DeploymentConfiguration): the event, the delay after it and the altitude of the ALTITUDE
/// event. The value type of a recovery device's FlightConfigurableParameterSet.
///
/// Change notification goes through the owning recovery device: the setters notify nobody, as in
/// Java, and whoever edits a device's deployment fires EVENT_CHANGE on the device. The multi-edit
/// config listeners are not ported, by decision (see RocketComponent).
///
/// In simulation/ (rocket/ does not include it): DeployEvent.isActivationEvent(config,
/// FlightEvent, source) and isActivationEvent(FlightEvent, source), the test the simulation
/// engine applies to every flight event for a recovery device @p source, are
/// FlightEventActivation::isActivationEvent(DeployEvent, config, event, source) and
/// isActivationEvent(config, event, source) in simulation/FlightEventActivation.h. Its rules:
/// - LAUNCH: a LAUNCH event.
/// - EJECTION: an EJECTION_CHARGE event; when the event's data is the MotorClusterState of the
///   motor that fired, only when that motor's mount has the same assembly as the device
///   (mount.getAssembly() equals source.getAssembly(): a charge only pressurises the airframe it
///   sits in, so a motor in a pod cannot deploy a device of the parent airframe, and vice versa);
///   without that data, when the event's source has the device's stage number.
/// - APOGEE: an APOGEE event.
/// - ALTITUDE: an ALTITUDE event with data (the previous and current altitude, u and v) such that
///   u >= deploy altitude and v <= deploy altitude (crossing it downwards); an ALTITUDE event
///   without data never matches.
/// - LOWER_STAGE_SEPARATION: a STAGE_SEPARATION event whose source's stage number is the device's
///   stage number + 1.
/// - NEVER: never.
///
/// Java's hashCode() (Objects.hash over the enum's identity hash) is not ported: it is not stable
/// between runs, and nothing hashes deployment configurations.
class DeploymentConfiguration
{
public:
    /// The event that deploys the device (Java: DeployEvent), in OpenRocket's order.
    enum class DeployEvent
    {
        LAUNCH,                  ///< launch (plus the delay)
        EJECTION,                ///< the first ejection charge of the device's stage
        APOGEE,                  ///< apogee
        ALTITUDE,                ///< a given altitude during descent
        LOWER_STAGE_SEPARATION,  ///< separation of the stage below
        NEVER,                   ///< never
    };

    /// Every event, in declaration order (DeployEvent.values()).
    static constexpr std::array<DeployEvent, 6> kAllDeployEvents{
        DeployEvent::LAUNCH,
        DeployEvent::EJECTION,
        DeployEvent::APOGEE,
        DeployEvent::ALTITUDE,
        DeployEvent::LOWER_STAGE_SEPARATION,
        DeployEvent::NEVER};

    /// The class name FlightConfigurableParameterSet::toDebug() prints.
    static constexpr std::string_view kTypeName = "DeploymentConfiguration";

    /// EJECTION, altitude 200 m, no delay.
    DeploymentConfiguration() = default;

    [[nodiscard]] DeployEvent getDeployEvent() const noexcept { return m_deployEvent; }
    void setDeployEvent(DeployEvent deployEvent) noexcept { m_deployEvent = deployEvent; }

    /// The altitude of the ALTITUDE event, in m.
    [[nodiscard]] double getDeployAltitude() const noexcept { return m_deployAltitude; }
    /// Sets the altitude; a value within MathUtil::equals() of the current one is ignored.
    void setDeployAltitude(double deployAltitude) noexcept;

    /// The delay after the event, in s.
    [[nodiscard]] double getDeployDelay() const noexcept { return m_deployDelay; }
    /// Sets the delay; a value within MathUtil::equals() of the current one is ignored.
    void setDeployDelay(double deployDelay) noexcept;

    /// The event's English description, followed by " + <delay>s" when the delay is positive (the
    /// delay as Java prints a double) and, for ALTITUDE with a non-zero altitude, by " " and the
    /// altitude in the distance group's default unit (UnitGroup::toString(), the number only):
    /// "Specific altitude during descent + 1.5s 200".
    [[nodiscard]] std::string toString() const;

    /// An exact copy (Java: clone(), which is copy(null)).
    [[nodiscard]] DeploymentConfiguration clone() const { return *this; }

    /// A copy for the configuration @p copyId (the id is not stored).
    [[nodiscard]] DeploymentConfiguration copy(const FlightConfigurationId& copyId) const;

    /// Nothing to update.
    void update() noexcept { }

    /// Java's equals(): the same event and the same altitude and delay (Double.compare, so NaN
    /// equals NaN and 0.0 differs from -0.0).
    [[nodiscard]] bool operator==(const DeploymentConfiguration& other) const noexcept;

private:
    DeployEvent m_deployEvent{DeployEvent::EJECTION};
    double      m_deployAltitude{200};
    double      m_deployDelay{0};
};

/// The constant's name, e.g. "LOWER_STAGE_SEPARATION" (Java: name()).
[[nodiscard]] std::string_view deployEventName(DeploymentConfiguration::DeployEvent event) noexcept;

/// The .ork spelling the recovery device saver writes in <deployevent>: the name lower-cased
/// without underscores ("lowerstageseparation").
[[nodiscard]] std::string_view orkName(DeploymentConfiguration::DeployEvent event) noexcept;

/// The event @p text names, matched as DocumentConfig.findEnum() does; nullopt for anything
/// else.
[[nodiscard]] std::optional<DeploymentConfiguration::DeployEvent> deployEventFromOrkName(
    std::string_view text);

/// The translation key of the description, e.g. "RecoveryDevice.DeployEvent.APOGEE".
[[nodiscard]] std::string_view displayKey(DeploymentConfiguration::DeployEvent event) noexcept;

/// The English description (Java: toString()), e.g. "First ejection charge of this stage".
[[nodiscard]] std::string_view displayName(DeploymentConfiguration::DeployEvent event) noexcept;

/// The translation key of the short name the GUI's tables show, e.g.
/// "RecoveryDevice.DeployEvent.short.EJECTION".
[[nodiscard]] std::string_view shortDisplayKey(DeploymentConfiguration::DeployEvent event) noexcept;

/// The English short name, e.g. "Ejection charge".
[[nodiscard]] std::string_view shortDisplayName(
    DeploymentConfiguration::DeployEvent event) noexcept;

}  // namespace QtRocket
