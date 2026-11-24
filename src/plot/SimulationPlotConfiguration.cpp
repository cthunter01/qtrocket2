#include "QtRocket/plot/SimulationPlotConfiguration.h"

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "QtRocket/plot/PlotConfiguration.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"

namespace QtRocket
{

template class PlotConfiguration<FlightDataType, FlightDataBranch>;

namespace
{

using Type = FlightEvent::Type;

[[nodiscard]] const FlightDataType& builtinType(FlightDataTypeId id)
{
    return FlightDataType::builtin(id);
}

void setEvents(SimulationPlotConfiguration& config, std::initializer_list<Type> types)
{
    for (const Type each : types)
    {
        config.setEvent(each, true);
    }
}

[[nodiscard]] std::size_t eventIndex(Type type) noexcept
{
    return static_cast<std::size_t>(type);
}

}  // namespace

SimulationPlotConfiguration::SimulationPlotConfiguration()
  : SimulationPlotConfiguration(std::string{}, builtinType(FlightDataTypeId::TYPE_TIME))
{
}

SimulationPlotConfiguration::SimulationPlotConfiguration(std::string name)
  : SimulationPlotConfiguration(std::move(name), builtinType(FlightDataTypeId::TYPE_TIME))
{
}

SimulationPlotConfiguration::SimulationPlotConfiguration(std::string           name,
                                                         const FlightDataType& domainType)
  : Base(std::move(name), domainType)
{
}

SimulationPlotConfiguration::SimulationPlotConfiguration(std::string name, std::string nameKey,
                                                         const FlightDataType& domainType)
  : Base(std::move(name), std::move(nameKey), domainType)
{
}

std::vector<SimulationPlotConfiguration> SimulationPlotConfiguration::defaultConfigurations()
{
    using enum FlightDataTypeId;
    const FlightDataType& time = builtinType(TYPE_TIME);

    std::vector<SimulationPlotConfiguration> configs;
    configs.reserve(9);

    //// Vertical motion vs. time
    SimulationPlotConfiguration verticalMotion("Vertical motion vs. time",
                                               "PlotConfiguration.Verticalmotion", time);
    verticalMotion.addPlotDataType(builtinType(TYPE_ALTITUDE), 0);
    verticalMotion.addPlotDataType(builtinType(TYPE_VELOCITY_Z));
    verticalMotion.addPlotDataType(builtinType(TYPE_ACCELERATION_Z));
    setEvents(verticalMotion,
              {Type::IGNITION, Type::BURNOUT, Type::APOGEE, Type::RECOVERY_DEVICE_DEPLOYMENT,
               Type::STAGE_SEPARATION, Type::GROUND_HIT, Type::TUMBLE, Type::EXCEPTION,
               Type::SIM_WARN, Type::SIM_ABORT});
    configs.push_back(std::move(verticalMotion));

    //// Total motion vs. time
    SimulationPlotConfiguration totalMotion("Total motion vs. time",
                                            "PlotConfiguration.Totalmotion", time);
    totalMotion.addPlotDataType(builtinType(TYPE_ALTITUDE), 0);
    totalMotion.addPlotDataType(builtinType(TYPE_VELOCITY_TOTAL));
    totalMotion.addPlotDataType(builtinType(TYPE_ACCELERATION_TOTAL));
    setEvents(totalMotion,
              {Type::IGNITION, Type::BURNOUT, Type::APOGEE, Type::RECOVERY_DEVICE_DEPLOYMENT,
               Type::STAGE_SEPARATION, Type::GROUND_HIT, Type::TUMBLE, Type::EXCEPTION,
               Type::SIM_WARN, Type::SIM_ABORT});
    configs.push_back(std::move(totalMotion));

    //// Flight side profile
    SimulationPlotConfiguration flightSide("Flight side profile", "PlotConfiguration.Flightside",
                                           builtinType(TYPE_POSITION_X));
    flightSide.addPlotDataType(builtinType(TYPE_ALTITUDE));
    setEvents(flightSide,
              {Type::IGNITION, Type::BURNOUT, Type::APOGEE, Type::RECOVERY_DEVICE_DEPLOYMENT,
               Type::STAGE_SEPARATION, Type::GROUND_HIT, Type::TUMBLE, Type::EXCEPTION,
               Type::SIM_WARN, Type::SIM_ABORT});
    configs.push_back(std::move(flightSide));

    //// Ground track
    SimulationPlotConfiguration groundTrack("Ground track", "PlotConfiguration.Groundtrack",
                                            builtinType(TYPE_POSITION_X));
    groundTrack.addPlotDataType(builtinType(TYPE_POSITION_Y), 0);
    groundTrack.addPlotDataType(builtinType(TYPE_ALTITUDE), 1);
    setEvents(groundTrack,
              {Type::IGNITION, Type::BURNOUT, Type::APOGEE, Type::RECOVERY_DEVICE_DEPLOYMENT,
               Type::GROUND_HIT, Type::EXCEPTION, Type::SIM_WARN, Type::SIM_ABORT});
    configs.push_back(std::move(groundTrack));

    //// Stability vs. time
    SimulationPlotConfiguration stability("Stability vs. time", "PlotConfiguration.Stability",
                                          time);
    stability.addPlotDataType(builtinType(TYPE_STABILITY), 0);
    stability.addPlotDataType(builtinType(TYPE_CP_LOCATION), 1);
    stability.addPlotDataType(builtinType(TYPE_CG_LOCATION), 1);
    setEvents(stability,
              {Type::IGNITION, Type::BURNOUT, Type::APOGEE, Type::RECOVERY_DEVICE_DEPLOYMENT,
               Type::STAGE_SEPARATION, Type::GROUND_HIT, Type::TUMBLE, Type::EXCEPTION,
               Type::SIM_WARN, Type::SIM_ABORT});
    configs.push_back(std::move(stability));

    //// Drag coefficients vs. Mach number
    SimulationPlotConfiguration dragCoefficients("Drag coefficients vs. Mach number",
                                                 "PlotConfiguration.Dragcoef",
                                                 builtinType(TYPE_MACH_NUMBER));
    dragCoefficients.addPlotDataType(builtinType(TYPE_DRAG_COEFF), 0);
    dragCoefficients.addPlotDataType(builtinType(TYPE_FRICTION_DRAG_COEFF), 0);
    dragCoefficients.addPlotDataType(builtinType(TYPE_BASE_DRAG_COEFF), 0);
    dragCoefficients.addPlotDataType(builtinType(TYPE_PRESSURE_DRAG_COEFF), 0);
    setEvents(dragCoefficients, {Type::EXCEPTION, Type::SIM_WARN, Type::SIM_ABORT});
    configs.push_back(std::move(dragCoefficients));

    //// Roll characteristics
    SimulationPlotConfiguration roll("Roll characteristics",
                                     "PlotConfiguration.Rollcharacteristics", time);
    roll.addPlotDataType(builtinType(TYPE_ROLL_RATE), 0);
    roll.addPlotDataType(builtinType(TYPE_ROLL_MOMENT_COEFF), 1);
    roll.addPlotDataType(builtinType(TYPE_ROLL_FORCING_COEFF), 1);
    roll.addPlotDataType(builtinType(TYPE_ROLL_DAMPING_COEFF), 1);
    setEvents(roll, {Type::IGNITION, Type::LAUNCHROD, Type::BURNOUT, Type::APOGEE,
                     Type::RECOVERY_DEVICE_DEPLOYMENT, Type::STAGE_SEPARATION, Type::GROUND_HIT,
                     Type::TUMBLE, Type::EXCEPTION, Type::SIM_WARN, Type::SIM_ABORT});
    configs.push_back(std::move(roll));

    //// Angle of attack and orientation vs. time
    SimulationPlotConfiguration angleOfAttack("Angle of attack and orientation vs. time",
                                              "PlotConfiguration.Angleofattack", time);
    angleOfAttack.addPlotDataType(builtinType(TYPE_AOA), 0);
    angleOfAttack.addPlotDataType(builtinType(TYPE_ORIENTATION_PHI));
    angleOfAttack.addPlotDataType(builtinType(TYPE_ORIENTATION_THETA));
    setEvents(angleOfAttack,
              {Type::IGNITION, Type::BURNOUT, Type::APOGEE, Type::RECOVERY_DEVICE_DEPLOYMENT,
               Type::STAGE_SEPARATION, Type::GROUND_HIT, Type::TUMBLE, Type::EXCEPTION,
               Type::SIM_WARN, Type::SIM_ABORT});
    configs.push_back(std::move(angleOfAttack));

    //// Simulation time step and computation time
    SimulationPlotConfiguration simulationTime("Simulation time step and computation time",
                                               "PlotConfiguration.Simulationtime", time);
    simulationTime.addPlotDataType(builtinType(TYPE_TIME_STEP));
    simulationTime.addPlotDataType(builtinType(TYPE_COMPUTATION_TIME));
    setEvents(simulationTime,
              {Type::IGNITION, Type::BURNOUT, Type::APOGEE, Type::RECOVERY_DEVICE_DEPLOYMENT,
               Type::STAGE_SEPARATION, Type::GROUND_HIT, Type::TUMBLE, Type::EXCEPTION,
               Type::SIM_WARN, Type::SIM_ABORT});
    configs.push_back(std::move(simulationTime));

    return configs;
}

SimulationPlotConfiguration SimulationPlotConfiguration::customConfiguration()
{
    return {std::string{kCustomName}, std::string{kCustomNameKey},
            builtinType(FlightDataTypeId::TYPE_TIME)};
}

std::vector<FlightEvent::Type> SimulationPlotConfiguration::getActiveEvents() const
{
    std::vector<Type> active;
    for (const Type each : FlightEvent::kAllTypes)
    {
        if (m_events.test(eventIndex(each)))
        {
            active.push_back(each);
        }
    }
    return active;
}

void SimulationPlotConfiguration::setEvent(FlightEvent::Type type, bool active)
{
    m_events.set(eventIndex(type), active);
}

bool SimulationPlotConfiguration::isEventActive(FlightEvent::Type type) const
{
    return m_events.test(eventIndex(type));
}

std::unique_ptr<SimulationPlotConfiguration::Base> SimulationPlotConfiguration::cloneConfiguration()
    const
{
    return std::make_unique<SimulationPlotConfiguration>(*this);
}

}  // namespace QtRocket
