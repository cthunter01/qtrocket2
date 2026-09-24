#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/rocket/FlightConfigurationId.h"

namespace QtRocket
{

/// When a stage separates in one flight configuration (OpenRocket's
/// StageSeparationConfiguration): the event, the delay after it and the altitude of the
/// altitude events. The value type of AxialStage's FlightConfigurableParameterSet.
///
/// Change notification goes through the owning stage: Java's fireChangeEvent() here is an empty
/// method, so a setter notifies nobody, and whoever edits a stage's separation (the GUI) fires
/// EVENT_CHANGE on the AxialStage. The multi-edit config listeners are not ported, by decision.
///
/// Deferred to simulation/: SeparationEvent.isSeparationEvent(config, FlightEvent, stage), the
/// test the simulation engine applies to every flight event. Its logic, for the simulation group
/// to implement over FlightEvent: LAUNCH on a LAUNCH event; IGNITION, BURNOUT and EJECTION on an
/// IGNITION, BURNOUT or EJECTION_CHARGE event whose source is in the stage (source stage number
/// equal to the stage's); UPPER_IGNITION on an IGNITION event of the stage above (source stage
/// number + 1 equal to the stage's); ALTITUDE_ASCENDING on an ALTITUDE event whose data (the
/// previous and current altitude, u and v) has u <= separation altitude <= v, and
/// ALTITUDE_DESCENDING with u >= altitude >= v (an ALTITUDE event without data never matches);
/// APOGEE on an APOGEE event; NEVER never.
class StageSeparationConfiguration
{
public:
    /// The event that triggers the separation (Java: SeparationEvent), in OpenRocket's order.
    enum class SeparationEvent
    {
        LAUNCH,               ///< launch
        IGNITION,             ///< current stage motor ignition
        BURNOUT,              ///< current stage motor burnout
        EJECTION,             ///< current stage ejection charge
        UPPER_IGNITION,       ///< upper stage motor ignition
        ALTITUDE_ASCENDING,   ///< a fixed altitude during ascent
        APOGEE,               ///< apogee
        ALTITUDE_DESCENDING,  ///< a fixed altitude during descent
        NEVER,                ///< never
    };

    /// Every event, in declaration order (SeparationEvent.values()).
    static constexpr std::array<SeparationEvent, 9> kAllSeparationEvents{
        SeparationEvent::LAUNCH,         SeparationEvent::IGNITION,
        SeparationEvent::BURNOUT,        SeparationEvent::EJECTION,
        SeparationEvent::UPPER_IGNITION, SeparationEvent::ALTITUDE_ASCENDING,
        SeparationEvent::APOGEE,         SeparationEvent::ALTITUDE_DESCENDING,
        SeparationEvent::NEVER};

    /// The class name FlightConfigurableParameterSet::toDebug() prints.
    static constexpr std::string_view kTypeName = "StageSeparationConfiguration";

    /// EJECTION, altitude 200 m, no delay.
    StageSeparationConfiguration() = default;

    [[nodiscard]] SeparationEvent getSeparationEvent() const noexcept { return m_separationEvent; }
    void                          setSeparationEvent(SeparationEvent separationEvent) noexcept
    {
        m_separationEvent = separationEvent;
    }

    /// The altitude of the ALTITUDE_ASCENDING / ALTITUDE_DESCENDING events, in m.
    [[nodiscard]] double getSeparationAltitude() const noexcept { return m_separationAltitude; }
    /// Sets the altitude; a value within MathUtil::equals() of the current one is ignored.
    void setSeparationAltitude(double separationAltitude) noexcept;

    /// The delay after the event, in s.
    [[nodiscard]] double getSeparationDelay() const noexcept { return m_separationDelay; }
    /// Sets the delay; a value within MathUtil::equals() of the current one is ignored.
    void setSeparationDelay(double separationDelay) noexcept;

    /// The event's English description, followed by " + <delay>s" when the delay is positive,
    /// the delay as Java prints a double ("Current stage ejection charge + 1.5s").
    [[nodiscard]] std::string toString() const;

    /// An exact copy (Java: clone(), which is copy(null)).
    [[nodiscard]] StageSeparationConfiguration clone() const { return *this; }

    /// A copy for the configuration @p copyId (the id is not stored).
    [[nodiscard]] StageSeparationConfiguration copy(const FlightConfigurationId& copyId) const;

    /// Nothing to update.
    void update() noexcept { }

    /// Java's equals(): the same event and the same delay (Double.compare); the altitude does not
    /// take part, as in OpenRocket.
    [[nodiscard]] bool operator==(const StageSeparationConfiguration& other) const noexcept;

private:
    SeparationEvent m_separationEvent{SeparationEvent::EJECTION};
    double          m_separationAltitude{200};
    double          m_separationDelay{0};
};

/// The constant's name, e.g. "UPPER_IGNITION" (Java: name()).
[[nodiscard]] std::string_view separationEventName(
    StageSeparationConfiguration::SeparationEvent event) noexcept;

/// The .ork spelling the stage saver writes in <separationevent>: the name lower-cased without
/// underscores ("upperignition").
[[nodiscard]] std::string_view orkName(
    StageSeparationConfiguration::SeparationEvent event) noexcept;

/// The event @p text names, matched as DocumentConfig.findEnum() does; nullopt for anything
/// else.
[[nodiscard]] std::optional<StageSeparationConfiguration::SeparationEvent>
separationEventFromOrkName(std::string_view text);

/// The translation key of the description, e.g. "Stage.SeparationEvent.EJECTION".
[[nodiscard]] std::string_view displayKey(
    StageSeparationConfiguration::SeparationEvent event) noexcept;

/// The English description (Java: toString()), e.g. "Current stage ejection charge".
[[nodiscard]] std::string_view displayName(
    StageSeparationConfiguration::SeparationEvent event) noexcept;

}  // namespace QtRocket
