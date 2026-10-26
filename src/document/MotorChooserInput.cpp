#include "QtRocket/document/MotorChooserInput.h"

#include <memory>
#include <utility>

#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/chooser/ThrustCurveMotorSelection.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorConfigurationSet.h"
#include "QtRocket/rocket/MotorMount.h"

namespace QtRocket::MotorChooserInput
{

ThrustCurveMotorSelection::MountConfiguration fromMount(const MotorMount&            mount,
                                                        const FlightConfigurationId& fcid)
{
    ThrustCurveMotorSelection::MountConfiguration configuration;
    for (const MotorConfiguration& each : mount.getMotorConfigurationSet().values())
    {
        if (auto motor = std::dynamic_pointer_cast<const ThrustCurveMotor>(each.getMotor()))
        {
            configuration.mountMotors.push_back(std::move(motor));
        }
    }
    const MotorConfiguration& current = mount.getMotorConfig(fcid);
    configuration.motor = std::dynamic_pointer_cast<const ThrustCurveMotor>(current.getMotor());
    configuration.ejectionDelay      = current.getEjectionDelay();
    configuration.nozzleExitDiameter = current.getNozzleExitDiameter();
    return configuration;
}

}  // namespace QtRocket::MotorChooserInput
