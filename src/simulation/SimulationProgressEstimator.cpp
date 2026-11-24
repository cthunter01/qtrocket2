#include "QtRocket/simulation/SimulationProgressEstimator.h"

#include <algorithm>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/simulation/RunSample.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

namespace
{

/// "<value> (max. <maximum>)" in the default unit of @p group (SimulationRunDialog.java:256-261).
[[nodiscard]] std::string valueWithMaximum(UnitGroupId group, double value, double maximum)
{
    const UnitGroup& units = unitGroup(group);
    return units.toStringUnit(value) + " (max. " + units.toStringUnit(maximum) + ")";
}

}  // namespace

SimulationProgressEstimator::SimulationProgressEstimator(const FlightConfiguration& configuration)
  : SimulationProgressEstimator(estimateBurnoutTime(configuration))
{
}

SimulationProgressEstimator::SimulationProgressEstimator(double burnoutTimeEstimate) noexcept
  : m_burnoutTimeEstimate(MathUtil::javaMax(burnoutTimeEstimate, kMinimumBurnoutTime))
{
}

double SimulationProgressEstimator::estimateBurnoutTime(const FlightConfiguration& configuration)
{
    // SimulationRunDialog.java:335-349.
    double launchBurn = 0.0;
    double otherBurn  = 0.0;
    for (const MotorConfiguration& curInstance : configuration.getActiveMotors())
    {
        const std::shared_ptr<const Motor>& motor = curInstance.getMotor();
        if (motor == nullptr)
        {
            bug("An active motor configuration has no motor");
        }
        if (curInstance.getIgnitionEvent() == IgnitionEvent::LAUNCH)
        {
            launchBurn = MathUtil::max(launchBurn, motor->getBurnTimeEstimate());
        }
        else
        {
            otherBurn = otherBurn + motor->getBurnTimeEstimate();
        }
    }
    // Java: Math.max, so a NaN stays NaN.
    return MathUtil::javaMax(launchBurn + otherBurn, kMinimumBurnoutTime);
}

void SimulationProgressEstimator::process(std::span<const RunSample> samples)
{
    if (samples.empty())
    {
        return;
    }

    // Update max. altitude and velocity of sustainer. Because parts of the simulation may be run
    // more than once in order to calculate things like optimal coast time, we'll keep updating
    // max altitude whenever we see that the rocket is going upwards. The last apogee found is
    // the real one. (Java's comment, :369-371.)
    for (const RunSample& s : samples)
    {
        if (s.sustainerActive && ((s.verticalVelocity > 0.0) || (s.altitude > m_maxAltitude)))
        {
            m_maxAltitude = s.altitude;
            m_maxVelocity = MathUtil::javaMax(m_maxVelocity, s.verticalVelocity);
        }
    }

    // Calculate the progress
    const RunSample& status = samples.back();
    m_lastSample            = status;

    // -2: Boost. time = 0 ... burnoutTimeEstimate
    if (m_stage == Stage::BOOST && status.simulationTime < m_burnoutTimeEstimate)
    {
        setProgress(MathUtil::map(status.simulationTime, 0.0, m_burnoutTimeEstimate, 0.0,
                                  kBurnoutProgress));
        return;
    }

    // Past burnout time estimate, switch to coast.
    if (m_stage == Stage::BOOST)
    {
        m_stage           = Stage::COAST;
        m_burnoutVelocity = MathUtil::max(status.verticalVelocity, kMinimumBurnoutVelocity);
    }

    // -1: Coast. z-velocity from burnout velocity to zero
    if (m_stage == Stage::COAST && status.verticalVelocity >= 0.0)
    {
        setProgress(MathUtil::map(status.verticalVelocity, m_burnoutVelocity, 0.0, kBurnoutProgress,
                                  kApogeeProgress));
        return;
    }

    // Past apogee, switch to landing
    if (m_stage == Stage::COAST && status.verticalVelocity < 0.0)
    {
        m_stage          = Stage::LANDING;
        m_apogeeAltitude = MathUtil::max(m_maxAltitude, kMinimumApogeeAltitude);
    }

    if (m_stage == Stage::COAST)
    {
        // Deviation (see the class comment): a NaN velocity in COAST. Java goes on to the
        // landing estimate with its apogee altitude still 0, and MathUtil.map throws for the
        // singular range.
        return;
    }

    // >= 0 Landing. z-position from apogee to zero
    setProgress(MathUtil::map(status.altitude, m_apogeeAltitude, 0.0, kApogeeProgress, 1.0));
}

void SimulationProgressEstimator::finish()
{
    // Java: simulationDone()'s setSimulationProgress(1.0).
    setProgress(1.0);
}

std::string SimulationProgressEstimator::getTimeText() const
{
    if (!m_lastSample.has_value())
    {
        return {};
    }
    return unitGroup(UnitGroupId::LONG_TIME).toStringUnit(m_lastSample->simulationTime);
}

std::string SimulationProgressEstimator::getAltitudeText() const
{
    if (!m_lastSample.has_value())
    {
        return {};
    }
    return valueWithMaximum(UnitGroupId::DISTANCE, m_lastSample->altitude, m_maxAltitude);
}

std::string SimulationProgressEstimator::getVelocityText() const
{
    if (!m_lastSample.has_value())
    {
        return {};
    }
    return valueWithMaximum(UnitGroupId::VELOCITY, m_lastSample->verticalVelocity, m_maxVelocity);
}

void SimulationProgressEstimator::setProgress(double fraction) noexcept
{
    // Java: int exact = Math.max(progress, (int) (100 * p + 0.5));
    //       progress = MathUtil.clamp(exact, 0, 100);
    const int exact = std::max(m_progress, MathUtil::javaIntCast((100.0 * fraction) + 0.5));
    m_progress      = MathUtil::clamp(exact, 0, 100);
}

std::string_view name(SimulationProgressEstimator::Stage stage) noexcept
{
    switch (stage)
    {
        case SimulationProgressEstimator::Stage::BOOST:
            return "BOOST";
        case SimulationProgressEstimator::Stage::COAST:
            return "COAST";
        case SimulationProgressEstimator::Stage::LANDING:
            return "LANDING";
    }
    return "BOOST";  // not reached: the switch covers every stage
}

}  // namespace QtRocket
