#include "QtRocket/rocket/StageSeparationConfiguration.h"

#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

using SeparationEvent = StageSeparationConfiguration::SeparationEvent;

void StageSeparationConfiguration::setSeparationAltitude(double separationAltitude) noexcept
{
    if (MathUtil::equals(m_separationAltitude, separationAltitude))
    {
        return;
    }
    m_separationAltitude = separationAltitude;
}

void StageSeparationConfiguration::setSeparationDelay(double separationDelay) noexcept
{
    if (MathUtil::equals(m_separationDelay, separationDelay))
    {
        return;
    }
    m_separationDelay = separationDelay;
}

std::string StageSeparationConfiguration::toString() const
{
    if (m_separationDelay > 0)
    {
        return std::string{displayName(m_separationEvent)} + " + " +
               Strings::javaDoubleToString(m_separationDelay) + "s";
    }
    return std::string{displayName(m_separationEvent)};
}

StageSeparationConfiguration StageSeparationConfiguration::copy(
    const FlightConfigurationId& /*copyId*/) const
{
    StageSeparationConfiguration clone;
    clone.m_separationEvent    = m_separationEvent;
    clone.m_separationAltitude = m_separationAltitude;
    clone.m_separationDelay    = m_separationDelay;
    return clone;
}

bool StageSeparationConfiguration::operator==(
    const StageSeparationConfiguration& other) const noexcept
{
    return MathUtil::javaDoubleCompare(other.m_separationDelay, m_separationDelay) == 0 &&
           m_separationEvent == other.m_separationEvent;
}

std::string_view separationEventName(SeparationEvent event) noexcept
{
    switch (event)
    {
        case SeparationEvent::LAUNCH:
            return "LAUNCH";
        case SeparationEvent::IGNITION:
            return "IGNITION";
        case SeparationEvent::BURNOUT:
            return "BURNOUT";
        case SeparationEvent::EJECTION:
            return "EJECTION";
        case SeparationEvent::UPPER_IGNITION:
            return "UPPER_IGNITION";
        case SeparationEvent::ALTITUDE_ASCENDING:
            return "ALTITUDE_ASCENDING";
        case SeparationEvent::APOGEE:
            return "APOGEE";
        case SeparationEvent::ALTITUDE_DESCENDING:
            return "ALTITUDE_DESCENDING";
        case SeparationEvent::NEVER:
            return "NEVER";
    }
    return "EJECTION";
}

std::string_view orkName(SeparationEvent event) noexcept
{
    switch (event)
    {
        case SeparationEvent::LAUNCH:
            return "launch";
        case SeparationEvent::IGNITION:
            return "ignition";
        case SeparationEvent::BURNOUT:
            return "burnout";
        case SeparationEvent::EJECTION:
            return "ejection";
        case SeparationEvent::UPPER_IGNITION:
            return "upperignition";
        case SeparationEvent::ALTITUDE_ASCENDING:
            return "altitudeascending";
        case SeparationEvent::APOGEE:
            return "apogee";
        case SeparationEvent::ALTITUDE_DESCENDING:
            return "altitudedescending";
        case SeparationEvent::NEVER:
            return "never";
    }
    return "ejection";
}

std::optional<SeparationEvent> separationEventFromOrkName(std::string_view text)
{
    for (const SeparationEvent event : StageSeparationConfiguration::kAllSeparationEvents)
    {
        if (Strings::orkEnumNameMatches(text, separationEventName(event)))
        {
            return event;
        }
    }
    return std::nullopt;
}

std::string_view displayKey(SeparationEvent event) noexcept
{
    switch (event)
    {
        case SeparationEvent::LAUNCH:
            return "Stage.SeparationEvent.LAUNCH";
        case SeparationEvent::IGNITION:
            return "Stage.SeparationEvent.IGNITION";
        case SeparationEvent::BURNOUT:
            return "Stage.SeparationEvent.BURNOUT";
        case SeparationEvent::EJECTION:
            return "Stage.SeparationEvent.EJECTION";
        case SeparationEvent::UPPER_IGNITION:
            return "Stage.SeparationEvent.UPPER_IGNITION";
        case SeparationEvent::ALTITUDE_ASCENDING:
            return "Stage.SeparationEvent.ALTITUDE_ASCENDING";
        case SeparationEvent::APOGEE:
            return "Stage.SeparationEvent.APOGEE";
        case SeparationEvent::ALTITUDE_DESCENDING:
            return "Stage.SeparationEvent.ALTITUDE_DESCENDING";
        case SeparationEvent::NEVER:
            return "Stage.SeparationEvent.NEVER";
    }
    return "Stage.SeparationEvent.EJECTION";
}

std::string_view displayName(SeparationEvent event) noexcept
{
    switch (event)
    {
        case SeparationEvent::LAUNCH:
            return "Launch";
        case SeparationEvent::IGNITION:
            return "Current stage motor ignition";
        case SeparationEvent::BURNOUT:
            return "Current stage motor burnout";
        case SeparationEvent::EJECTION:
            return "Current stage ejection charge";
        case SeparationEvent::UPPER_IGNITION:
            return "Upper stage motor ignition";
        case SeparationEvent::ALTITUDE_ASCENDING:
            return "Specific altitude during ascent";
        case SeparationEvent::APOGEE:
            return "Apogee";
        case SeparationEvent::ALTITUDE_DESCENDING:
            return "Specific altitude during descent";
        case SeparationEvent::NEVER:
            return "Never";
    }
    return "Current stage ejection charge";
}

}  // namespace QtRocket
