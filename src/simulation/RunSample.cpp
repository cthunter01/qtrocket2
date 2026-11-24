#include "QtRocket/simulation/RunSample.h"

#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/simulation/SimulationStatus.h"

namespace QtRocket
{

RunSample RunSample::of(const SimulationStatus& status)
{
    // SimulationRunDialog.java: status.getSimulationTime(), getRocketPosition().getZ(),
    // getRocketVelocity().getZ() and getConfiguration().isStageActive(0).
    return RunSample{
        .simulationTime   = status.getSimulationTime(),
        .altitude         = status.getRocketPosition().z,
        .verticalVelocity = status.getRocketVelocity().z,
        .sustainerActive  = status.getConfiguration().isStageActive(0),
    };
}

}  // namespace QtRocket
