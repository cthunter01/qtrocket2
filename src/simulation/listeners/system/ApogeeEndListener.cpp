#include "QtRocket/simulation/listeners/system/ApogeeEndListener.h"

#include <memory>

#include "QtRocket/simulation/EventQueue.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/SimulationStatus.h"

namespace QtRocket
{

std::shared_ptr<ApogeeEndListener> ApogeeEndListener::instance()
{
    static const std::shared_ptr<ApogeeEndListener> kInstance =
        std::make_shared<ApogeeEndListener>();
    return kInstance;
}

bool ApogeeEndListener::handleFlightEvent(SimulationStatus& status, const FlightEvent& event)
{
    if (event.getType() == FlightEvent::Type::APOGEE)
    {
        status.getEventQueue().add(
            FlightEvent(FlightEvent::Type::SIMULATION_END, status.getSimulationTime()));
    }
    return true;
}

bool ApogeeEndListener::isSystemListener() const
{
    return true;
}

}  // namespace QtRocket
