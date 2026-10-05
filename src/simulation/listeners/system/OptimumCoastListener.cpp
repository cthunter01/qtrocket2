#include "QtRocket/simulation/listeners/system/OptimumCoastListener.h"

#include <memory>

#include "QtRocket/simulation/EventQueue.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/SimulationStatus.h"

namespace QtRocket
{

std::shared_ptr<OptimumCoastListener> OptimumCoastListener::instance()
{
    static const std::shared_ptr<OptimumCoastListener> kInstance =
        std::make_shared<OptimumCoastListener>();
    return kInstance;
}

bool OptimumCoastListener::handleFlightEvent(SimulationStatus& status, const FlightEvent& event)
{
    if (event.getType() == FlightEvent::Type::APOGEE)
    {
        status.getEventQueue().add(
            FlightEvent(FlightEvent::Type::SIMULATION_END, status.getSimulationTime()));
    }
    return true;
}

bool OptimumCoastListener::recoveryDeviceDeployment(SimulationStatus& /*status*/,
                                                    const RecoveryDevice& /*recoveryDevice*/)
{
    return false;
}

bool OptimumCoastListener::isSystemListener() const
{
    return true;
}

}  // namespace QtRocket
