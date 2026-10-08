#include "QtRocket/simulation/extension/example/AirStart.h"

#include <format>
#include <memory>
#include <optional>
#include <string>

#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/extension/AbstractSimulationExtension.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Quaternion.h"

namespace QtRocket
{

namespace
{

/// Java's AirStart.AirStartListener, with the settings of the extension as they were when it
/// was made (see the class comment of AirStart).
class AirStartListener final : public CloneableSimulationListener<AirStartListener>
{
public:
    AirStartListener(double launchAltitude, double launchVelocity) noexcept
      : m_launchAltitude(launchAltitude), m_launchVelocity(launchVelocity)
    {
    }

    void startSimulation(SimulationStatus& status) override
    {
        status.setRocketPosition(Coordinate(0, 0, m_launchAltitude));
        status.setRocketVelocity(
            status.getRocketOrientationQuaternion().rotate(Coordinate(0, 0, m_launchVelocity)));
    }

private:
    double m_launchAltitude;
    double m_launchVelocity;
};

}  // namespace

AirStart::AirStart() : AbstractSimulationExtension(std::string(kId)) { }

bool AirStart::isMonteCarloSafe() const
{
    return true;
}

void AirStart::initialize(SimulationConditions& conditions)
{
    conditions.getSimulationListenerList().push_back(
        std::make_shared<AirStartListener>(getLaunchAltitude(), getLaunchVelocity()));
}

std::string AirStart::getName() const
{
    // Java replaces {alt} and {vel} in the text of SimulationExtension.airstart.name.altvel
    // ("Air-start ({alt}, {vel})") or .alt ("Air-start ({alt})").
    const std::string alt = unitGroup(UnitGroupId::DISTANCE).toStringUnit(getLaunchAltitude());
    if (getLaunchVelocity() > 0.01)
    {
        const std::string vel = unitGroup(UnitGroupId::VELOCITY).toStringUnit(getLaunchVelocity());
        return std::format("Air-start ({}, {})", alt, vel);
    }
    return std::format("Air-start ({})", alt);
}

std::optional<std::string> AirStart::getDescription() const
{
    return "Start simulation with a configurable altitude and velocity";
}

std::unique_ptr<SimulationExtension> AirStart::clone() const
{
    return std::make_unique<AirStart>(*this);
}

double AirStart::getLaunchAltitude() const
{
    return m_config.getDouble("launchAltitude", 100.0);
}

void AirStart::setLaunchAltitude(double launchAltitude)
{
    m_config.put("launchAltitude", launchAltitude);
    fireChangeEvent();
}

double AirStart::getLaunchVelocity() const
{
    return m_config.getDouble("launchVelocity", 50.0);
}

void AirStart::setLaunchVelocity(double launchVelocity)
{
    m_config.put("launchVelocity", launchVelocity);
    fireChangeEvent();
}

}  // namespace QtRocket
