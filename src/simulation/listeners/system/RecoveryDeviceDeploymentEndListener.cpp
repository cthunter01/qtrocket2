#include "QtRocket/simulation/listeners/system/RecoveryDeviceDeploymentEndListener.h"

#include <memory>

#include "QtRocket/simulation/EventQueue.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/SimulationStatus.h"

namespace QtRocket
{

std::shared_ptr<RecoveryDeviceDeploymentEndListener> RecoveryDeviceDeploymentEndListener::instance()
{
    static const std::shared_ptr<RecoveryDeviceDeploymentEndListener> kInstance =
        std::make_shared<RecoveryDeviceDeploymentEndListener>();
    return kInstance;
}

bool RecoveryDeviceDeploymentEndListener::recoveryDeviceDeployment(
    SimulationStatus& status, const RecoveryDevice& /*recoveryDevice*/)
{
    status.getEventQueue().add(
        FlightEvent(FlightEvent::Type::SIMULATION_END, status.getSimulationTime()));
    return true;
}

bool RecoveryDeviceDeploymentEndListener::isSystemListener() const
{
    return true;
}

}  // namespace QtRocket
