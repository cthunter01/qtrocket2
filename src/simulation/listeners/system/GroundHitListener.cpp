#include "QtRocket/simulation/listeners/system/GroundHitListener.h"

#include <memory>

#include "QtRocket/simulation/EventQueue.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/SimulationStatus.h"

namespace QtRocket
{

std::shared_ptr<GroundHitListener> GroundHitListener::instance()
{
    static const std::shared_ptr<GroundHitListener> kInstance =
        std::make_shared<GroundHitListener>();
    return kInstance;
}

bool GroundHitListener::handleFlightEvent(SimulationStatus& status, const FlightEvent& event)
{
    if (event.getType() == FlightEvent::Type::GROUND_HIT)
    {
        status.getEventQueue().add(
            FlightEvent(FlightEvent::Type::SIMULATION_END, status.getSimulationTime()));
    }
    return true;
}

bool GroundHitListener::isSystemListener() const
{
    return true;
}

}  // namespace QtRocket
