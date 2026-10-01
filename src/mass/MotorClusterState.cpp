#include "QtRocket/mass/MotorClusterState.h"

#include <format>
#include <limits>
#include <memory>
#include <string>

#include "QtRocket/mass/ThrustState.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// The motor of @p config.
/// @throws BugError when there is none (Java: NullPointerException).
std::shared_ptr<const Motor> requireMotor(const MotorConfiguration& config)
{
    if (config.isEmpty())
    {
        bug("MotorClusterState of a motor configuration without a motor");
    }
    return config.getMotor();
}

}  // namespace

MotorClusterState::MotorClusterState(const MotorConfiguration& config)
  : m_config(config),
    m_motor(requireMotor(config)),
    m_motorCount(static_cast<int>(asComponent(config.getMount()).getComponentLocations().size())),
    m_thrustDuration(m_motor->getBurnTimeEstimate())
{
    reset();
}

void MotorClusterState::ignite(double ignitionTime) noexcept
{
    if (ThrustState::ARMED == m_currentState)
    {
        m_ignitionTime = ignitionTime;
        m_currentState = ThrustState::THRUSTING;  // ARMED.getNext()
    }
}

void MotorClusterState::burnOut(double burnOutTime) noexcept
{
    if (ThrustState::THRUSTING == m_currentState)
    {
        m_cutoffTime   = burnOutTime;
        m_currentState = ThrustState::DELAYING;  // THRUSTING.getNext()
    }
    if (!hasEjectionCharge())
    {
        m_currentState = ThrustState::SPENT;
    }
}

void MotorClusterState::expend(double ejectionTime) noexcept
{
    if (ThrustState::DELAYING == m_currentState)
    {
        m_ejectionTime = ejectionTime;
        m_currentState = ThrustState::SPENT;  // DELAYING.getNext()
    }
}

double MotorClusterState::getPropellantMass() const
{
    return m_motor->getLaunchMass() - m_motor->getBurnoutMass();
}

double MotorClusterState::getPropellantMass(double motorTime) const
{
    return m_motor->getPropellantMass(motorTime) - m_motor->getBurnoutMass();
}

double MotorClusterState::getMotorTime(double simulationTime) const noexcept
{
    return MathUtil::javaMax(simulationTime - getIgnitionTime(), 0.0);
}

double MotorClusterState::getThrust(double simulationTime) const
{
    if (QtRocket::isThrusting(m_currentState))
    {
        const double motorTime = getMotorTime(simulationTime);
        return m_motorCount * m_motor->getThrust(motorTime);
    }
    return 0.0;
}

bool MotorClusterState::isPlugged() const noexcept
{
    return m_config.getEjectionDelay() == Motor::kPluggedDelay;
}

void MotorClusterState::reset() noexcept
{
    // i.e. in the "future"
    m_ignitionTime = std::numeric_limits<double>::infinity();
    m_cutoffTime   = std::numeric_limits<double>::infinity();
    m_ejectionTime = std::numeric_limits<double>::infinity();

    m_currentState = ThrustState::ARMED;
}

std::string MotorClusterState::toDescription() const
{
    // Java's "%32s / %4s - %s" pads to UTF-16 code units, not to std::format's display width.
    return std::format("{} / {} - {}",
                       Strings::javaPadLeft(asComponent(getMount()).getDebugName(), 32),
                       Strings::javaPadLeft(m_motor->getDesignation(), 4), name(m_currentState));
}

}  // namespace QtRocket
