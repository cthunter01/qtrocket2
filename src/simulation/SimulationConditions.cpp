#include "QtRocket/simulation/SimulationConditions.h"

#include <memory>
#include <optional>
#include <utility>

#include "QtRocket/mass/MassCalculator.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/WorldCoordinate.h"

namespace QtRocket
{

void SimulationConditions::setAerodynamicCalculator(
    std::shared_ptr<AerodynamicCalculator> aerodynamicCalculator)
{
    if (m_aerodynamicCalculator != nullptr)
    {
        m_modIdAdd = ModId{};
    }
    m_aerodynamicCalculator = std::move(aerodynamicCalculator);
}

void SimulationConditions::setMassCalculator(std::optional<MassCalculator> massCalculator)
{
    if (m_massCalculator.has_value())
    {
        m_modIdAdd = ModId{};
    }
    m_massCalculator = massCalculator;
}

Rocket& SimulationConditions::getRocket() const
{
    if (m_simulation == nullptr)
    {
        bug("The simulation conditions have no simulation");
    }
    return m_simulation->getRocket();
}

FlightConfigurationId SimulationConditions::getMotorConfigurationId() const
{
    if (m_simulation == nullptr)
    {
        bug("The simulation conditions have no simulation");
    }
    return m_simulation->getId();
}

FlightConfigurationId SimulationConditions::getFlightConfigurationId() const
{
    if (m_simulation == nullptr)
    {
        bug("The simulation conditions have no simulation");
    }
    return m_simulation->getId();
}

void SimulationConditions::setLaunchRodLength(double launchRodLength) noexcept
{
    m_launchRodLength = launchRodLength;
    m_modId           = ModId{};
}

void SimulationConditions::setLaunchRodAngle(double launchRodAngle) noexcept
{
    m_launchRodAngle = launchRodAngle;
    m_modId          = ModId{};
}

void SimulationConditions::setLaunchRodDirection(double launchRodDirection) noexcept
{
    m_launchRodDirection = launchRodDirection;
    m_modId              = ModId{};
}

void SimulationConditions::setLaunchSite(const WorldCoordinate& site) noexcept
{
    if (m_launchSite == site)
    {
        return;
    }
    m_launchSite = site;
    m_modId      = ModId{};
}

void SimulationConditions::setLaunchPosition(const Coordinate& launchPosition) noexcept
{
    if (m_launchPosition == launchPosition)
    {
        return;
    }
    m_launchPosition = launchPosition;
    m_modId          = ModId{};
}

void SimulationConditions::setLaunchVelocity(const Coordinate& launchVelocity) noexcept
{
    if (m_launchVelocity == launchVelocity)
    {
        return;
    }
    m_launchVelocity = launchVelocity;
    m_modId          = ModId{};
}

void SimulationConditions::setGeodeticComputation(
    GeodeticComputationStrategy geodeticComputation) noexcept
{
    if (m_geodeticComputation == geodeticComputation)
    {
        return;
    }
    m_geodeticComputation = geodeticComputation;
    m_modId               = ModId{};
}

void SimulationConditions::setWindModel(std::shared_ptr<WindModel> windModel)
{
    if (m_windModel != nullptr)
    {
        m_modIdAdd = ModId{};
    }
    m_windModel = std::move(windModel);
}

void SimulationConditions::setAtmosphericModel(
    std::shared_ptr<const AtmosphericModel> atmosphericModel)
{
    if (m_atmosphericModel != nullptr)
    {
        m_modIdAdd = ModId{};
    }
    m_atmosphericModel = std::move(atmosphericModel);
}

void SimulationConditions::setGravityModel(std::shared_ptr<const GravityModel> gravityModel)
{
    m_modId        = ModId{};
    m_gravityModel = std::move(gravityModel);
}

void SimulationConditions::setTimeStep(double timeStep) noexcept
{
    m_timeStep = timeStep;
    m_modId    = ModId{};
}

void SimulationConditions::setMaxSimulationTime(double maxSimulationTime) noexcept
{
    m_maxSimulationTime = maxSimulationTime;
    m_modId             = ModId{};
}

void SimulationConditions::setMaximumAngleStep(double maximumAngle) noexcept
{
    m_maximumAngleStep = maximumAngle;
    m_modId            = ModId{};
}

void SimulationConditions::setRecoverySpeedWarning(double recoverySpeedWarning) noexcept
{
    m_recoverySpeedWarning = recoverySpeedWarning;
    m_modId                = ModId{};
}

void SimulationConditions::setDrogueLowSpeedWarning(double drogueLowSpeedWarning) noexcept
{
    m_drogueLowSpeedWarning = drogueLowSpeedWarning;
    m_modId                 = ModId{};
}

void SimulationConditions::setRecoveryDrogueMainHighSpeedWarning(
    double recoveryDrogueMainHighSpeedWarning) noexcept
{
    m_recoveryDrogueMainHighSpeedWarning = recoveryDrogueMainHighSpeedWarning;
    m_modId                              = ModId{};
}

void SimulationConditions::setRecoveryDrogueMainLowSpeedWarning(
    double recoveryDrogueMainLowSpeedWarning) noexcept
{
    m_recoveryDrogueMainLowSpeedWarning = recoveryDrogueMainLowSpeedWarning;
    m_modId                             = ModId{};
}

void SimulationConditions::setRandomSeed(int randomSeed) noexcept
{
    m_randomSeed = randomSeed;
    m_modId      = ModId{};
}

SimulationConditions SimulationConditions::clone() const
{
    // The models and the calculators are shared, as in Java ("TODO: HIGH: Deep clone models").
    SimulationConditions clone(*this);
    clone.m_simulationListeners.clear();
    clone.m_simulationListeners.reserve(m_simulationListeners.size());
    for (const std::shared_ptr<SimulationListener>& listener : m_simulationListeners)
    {
        if (listener == nullptr)
        {
            bug("The simulation listener list holds a null listener");
        }
        clone.m_simulationListeners.push_back(listener->clone());
    }

    return clone;
}

}  // namespace QtRocket
