#include "QtRocket/rocket/DeploymentConfiguration.h"

#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

using DeployEvent = DeploymentConfiguration::DeployEvent;

void DeploymentConfiguration::setDeployAltitude(double deployAltitude) noexcept
{
    if (MathUtil::equals(m_deployAltitude, deployAltitude))
    {
        return;
    }
    m_deployAltitude = deployAltitude;
}

void DeploymentConfiguration::setDeployDelay(double deployDelay) noexcept
{
    if (MathUtil::equals(m_deployDelay, deployDelay))
    {
        return;
    }
    m_deployDelay = deployDelay;
}

std::string DeploymentConfiguration::toString() const
{
    std::string description{displayName(m_deployEvent)};
    if (m_deployDelay > 0)
    {
        description += " + " + Strings::javaDoubleToString(m_deployDelay) + "s";
    }
    if (m_deployEvent == DeployEvent::ALTITUDE && m_deployAltitude != 0)
    {
        description += " " + unitGroup(UnitGroupId::DISTANCE).toString(m_deployAltitude);
    }
    return description;
}

DeploymentConfiguration DeploymentConfiguration::copy(const FlightConfigurationId& /*copyId*/) const
{
    DeploymentConfiguration that;
    that.m_deployAltitude = m_deployAltitude;
    that.m_deployDelay    = m_deployDelay;
    that.m_deployEvent    = m_deployEvent;
    return that;
}

bool DeploymentConfiguration::operator==(const DeploymentConfiguration& other) const noexcept
{
    return MathUtil::javaDoubleCompare(other.m_deployAltitude, m_deployAltitude) == 0 &&
           MathUtil::javaDoubleCompare(other.m_deployDelay, m_deployDelay) == 0 &&
           m_deployEvent == other.m_deployEvent;
}

std::string_view deployEventName(DeployEvent event) noexcept
{
    switch (event)
    {
        case DeployEvent::LAUNCH:
            return "LAUNCH";
        case DeployEvent::EJECTION:
            return "EJECTION";
        case DeployEvent::APOGEE:
            return "APOGEE";
        case DeployEvent::ALTITUDE:
            return "ALTITUDE";
        case DeployEvent::LOWER_STAGE_SEPARATION:
            return "LOWER_STAGE_SEPARATION";
        case DeployEvent::NEVER:
            return "NEVER";
    }
    return "EJECTION";
}

std::string_view orkName(DeployEvent event) noexcept
{
    switch (event)
    {
        case DeployEvent::LAUNCH:
            return "launch";
        case DeployEvent::EJECTION:
            return "ejection";
        case DeployEvent::APOGEE:
            return "apogee";
        case DeployEvent::ALTITUDE:
            return "altitude";
        case DeployEvent::LOWER_STAGE_SEPARATION:
            return "lowerstageseparation";
        case DeployEvent::NEVER:
            return "never";
    }
    return "ejection";
}

std::optional<DeployEvent> deployEventFromOrkName(std::string_view text)
{
    for (const DeployEvent event : DeploymentConfiguration::kAllDeployEvents)
    {
        if (Strings::orkEnumNameMatches(text, deployEventName(event)))
        {
            return event;
        }
    }
    return std::nullopt;
}

std::string_view displayKey(DeployEvent event) noexcept
{
    switch (event)
    {
        case DeployEvent::LAUNCH:
            return "RecoveryDevice.DeployEvent.LAUNCH";
        case DeployEvent::EJECTION:
            return "RecoveryDevice.DeployEvent.EJECTION";
        case DeployEvent::APOGEE:
            return "RecoveryDevice.DeployEvent.APOGEE";
        case DeployEvent::ALTITUDE:
            return "RecoveryDevice.DeployEvent.ALTITUDE";
        case DeployEvent::LOWER_STAGE_SEPARATION:
            return "RecoveryDevice.DeployEvent.LOWER_STAGE_SEPARATION";
        case DeployEvent::NEVER:
            return "RecoveryDevice.DeployEvent.NEVER";
    }
    return "RecoveryDevice.DeployEvent.EJECTION";
}

std::string_view displayName(DeployEvent event) noexcept
{
    // OpenRocket's English messages.properties.
    switch (event)
    {
        case DeployEvent::LAUNCH:
            return "Launch (plus NN seconds)";
        case DeployEvent::EJECTION:
            return "First ejection charge of this stage";
        case DeployEvent::APOGEE:
            return "Apogee";
        case DeployEvent::ALTITUDE:
            return "Specific altitude during descent";
        case DeployEvent::LOWER_STAGE_SEPARATION:
            return "Lower stage separation";
        case DeployEvent::NEVER:
            return "Never";
    }
    return "First ejection charge of this stage";
}

std::string_view shortDisplayKey(DeployEvent event) noexcept
{
    switch (event)
    {
        case DeployEvent::LAUNCH:
            return "RecoveryDevice.DeployEvent.short.LAUNCH";
        case DeployEvent::EJECTION:
            return "RecoveryDevice.DeployEvent.short.EJECTION";
        case DeployEvent::APOGEE:
            return "RecoveryDevice.DeployEvent.short.APOGEE";
        case DeployEvent::ALTITUDE:
            return "RecoveryDevice.DeployEvent.short.ALTITUDE";
        case DeployEvent::LOWER_STAGE_SEPARATION:
            return "RecoveryDevice.DeployEvent.short.LOWER_STAGE_SEPARATION";
        case DeployEvent::NEVER:
            return "RecoveryDevice.DeployEvent.short.NEVER";
    }
    return "RecoveryDevice.DeployEvent.short.EJECTION";
}

std::string_view shortDisplayName(DeployEvent event) noexcept
{
    switch (event)
    {
        case DeployEvent::LAUNCH:
            return "Launch";
        case DeployEvent::EJECTION:
            return "Ejection charge";
        case DeployEvent::APOGEE:
            return "Apogee";
        case DeployEvent::ALTITUDE:
            return "Altitude";
        case DeployEvent::LOWER_STAGE_SEPARATION:
            return "Lower stage separation";
        case DeployEvent::NEVER:
            return "Never";
    }
    return "Ejection charge";
}

}  // namespace QtRocket
