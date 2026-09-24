#include "QtRocket/rocket/MotorConfiguration.h"

#include <cmath>
#include <format>
#include <memory>
#include <string>
#include <utility>

#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/MotorConfigurationId.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Inertia.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

MotorConfiguration::MotorConfiguration(MotorMount& mount, const FlightConfigurationId& fcid)
  : m_mount(&mount), m_fcid(fcid), m_mid(asComponent(mount).getId(), fcid.key())
{
}

MotorConfiguration::MotorConfiguration(MotorMount& mount, const FlightConfigurationId& fcid,
                                       const MotorConfiguration& source)
  : MotorConfiguration(mount, fcid)
{
    m_motor              = source.m_motor;
    m_ejectionDelay      = source.m_ejectionDelay;
    m_nozzleExitDiameter = source.m_nozzleExitDiameter;
    m_ignitionOverride   = source.m_ignitionOverride;
    m_ignitionEvent      = source.getIgnitionEvent();
    m_ignitionDelay      = source.getIgnitionDelay();
}

std::string MotorConfiguration::toMotorName(const Preferences& preferences) const
{
    if (m_motor == nullptr)
    {
        return std::string{kEmptyMotorName};
    }
    return m_motor->getMotorName(preferences, m_ejectionDelay);
}

void MotorConfiguration::setMotor(std::shared_ptr<const Motor> motor)
{
    if (m_motor != motor)
    {
        // The nozzle geometry belongs to the selected motor, so a replacement motor must not
        // inherit a diameter entered for the previous one.
        m_nozzleExitDiameter = 0.0;
    }
    m_motor = std::move(motor);
}

Result<void> MotorConfiguration::setNozzleExitDiameter(double diameter)
{
    if (!std::isfinite(diameter) || diameter < 0)
    {
        return fail(ErrorCode::INVALID_ARGUMENT,
                    "Nozzle exit diameter must be finite and non-negative");
    }
    if (m_motor != nullptr && diameter > m_motor->getDiameter())
    {
        return fail(ErrorCode::INVALID_ARGUMENT,
                    "Nozzle exit diameter must not exceed the motor diameter");
    }
    m_nozzleExitDiameter = diameter;
    return {};
}

Coordinate MotorConfiguration::getPosition() const
{
    return Coordinate{getX(), 0, 0};
}

double MotorConfiguration::getX() const
{
    if (isEmpty())
    {
        return 0.0;
    }
    return m_mount->getLength() - m_motor->getLength() + m_mount->getMotorOverhang();
}

void MotorConfiguration::useDefaultIgnition()
{
    m_ignitionOverride = false;
    // As in Java, through the setters, which mark the ignition as overridden again.
    setIgnitionDelay(0);
    setIgnitionEvent(IgnitionEvent::AUTOMATIC);
}

void MotorConfiguration::setIgnitionDelay(double delay) noexcept
{
    m_ignitionDelay    = delay;
    m_ignitionOverride = true;
}

void MotorConfiguration::setIgnitionEvent(IgnitionEvent event) noexcept
{
    m_ignitionEvent    = event;
    m_ignitionOverride = true;
}

int MotorConfiguration::getMotorCount() const
{
    // Java: an InnerTube's cluster configuration count, which is what its getMotorCount() gives.
    if (asComponent(*m_mount).kind() == ComponentKind::INNER_TUBE)
    {
        return m_mount->getMotorCount();
    }
    return 1;
}

Coordinate MotorConfiguration::getOffset() const
{
    if (m_motor == nullptr)
    {
        bug("MotorConfiguration::getOffset() without a motor");
    }
    const double deltaX =
        asComponent(*m_mount).getLength() + m_mount->getMotorOverhang() - m_motor->getLength();
    return Coordinate{deltaX, 0, 0};
}

double MotorConfiguration::getUnitLongitudinalInertia() const
{
    if (m_motor != nullptr)
    {
        return Inertia::filledCylinderLongitudinal(m_motor->getDiameter() / 2,
                                                   m_motor->getLength());
    }
    return 0.0;
}

double MotorConfiguration::getUnitRotationalInertia() const
{
    if (m_motor != nullptr)
    {
        return Inertia::filledCylinderRotational(m_motor->getDiameter() / 2);
    }
    return 0.0;
}

double MotorConfiguration::getPropellantMass() const
{
    if (m_motor != nullptr)
    {
        return m_motor->getLaunchMass() - m_motor->getBurnoutMass();
    }
    return 0.0;
}

MotorConfiguration MotorConfiguration::clone() const
{
    return copy(m_fcid);
}

MotorConfiguration MotorConfiguration::copy(const FlightConfigurationId& copyId) const
{
    MotorConfiguration clone{*m_mount, copyId};
    clone.m_motor              = m_motor;
    clone.m_ejectionDelay      = m_ejectionDelay;
    clone.m_nozzleExitDiameter = m_nozzleExitDiameter;
    clone.m_ignitionOverride   = m_ignitionOverride;
    clone.m_ignitionDelay      = m_ignitionDelay;
    clone.m_ignitionEvent      = m_ignitionEvent;
    return clone;
}

void MotorConfiguration::copyFrom(const MotorConfiguration& configuration)
{
    m_motor              = configuration.m_motor;
    m_ejectionDelay      = configuration.m_ejectionDelay;
    m_nozzleExitDiameter = configuration.m_nozzleExitDiameter;
    m_ignitionOverride   = configuration.m_ignitionOverride;
    m_ignitionDelay      = configuration.m_ignitionDelay;
    m_ignitionEvent      = configuration.m_ignitionEvent;
}

std::string MotorConfiguration::toDescription(const Preferences& preferences) const
{
    return toMotorName(preferences) + " in: " + asComponent(*m_mount).getDebugName() +
           " ign@: " + toIgnitionDescription();
}

std::string MotorConfiguration::toIgnitionDescription() const
{
    return std::string{name(m_ignitionEvent)} + " + " +
           Strings::javaDoubleToString(m_ignitionDelay) + "s ";
}

std::string MotorConfiguration::toDebugDetail(const Preferences& preferences) const
{
    return std::format("[in: {:>28}][fcid {:>10}][mid {:>10}][    {:>8} ign@: {:>12}]",
                       asComponent(*m_mount).getDebugName(), m_fcid.toShortKey(), m_mid.toDebug(),
                       toMotorName(preferences), toIgnitionDescription());
}

}  // namespace QtRocket
