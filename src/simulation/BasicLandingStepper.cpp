#include "QtRocket/simulation/BasicLandingStepper.h"

#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/InstanceMap.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket
{

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): the override of a virtual
double BasicLandingStepper::computeCD(const SimulationStatus& status)
{
    // Accumulate CD for all recovery devices
    double             cd   = 0;
    const InstanceMap& imap = status.getConfiguration().getActiveInstances();
    for (const RecoveryDevice* c : status.getDeployedRecoveryDevices())
    {
        if (c == nullptr)
        {
            bug("The set of deployed recovery devices holds a null device");
        }
        cd += imap.count(*c) * c->getCD() * c->getArea() /
              status.getConfiguration().getReferenceArea();
    }
    return cd;
}

}  // namespace QtRocket
