#include "QtRocket/aero/ComponentDrag.h"

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/BarrowmanCalculator.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/aero/ForceMap.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"

namespace QtRocket::ComponentDrag
{

double getComponentCD(const RocketComponent& component, double aoa, double theta, double mach,
                      double rollRate)
{
    const Rocket* const rocket = component.findRocket();
    if (rocket == nullptr)
    {
        // Java: getRocket() throws an IllegalStateException, which is caught here. It can happen
        // while a rocket is loaded (after an undo), before the sustainer has the rocket as its
        // parent; a later call then succeeds.
        return 0;
    }
    const FlightConfiguration& configuration = rocket->getSelectedConfiguration();
    FlightConditions           conditions{configuration};
    WarningSet                 warnings;
    BarrowmanCalculator        aerodynamicCalculator;

    conditions.setAOA(aoa);
    conditions.setTheta(theta);
    conditions.setMach(mach);
    conditions.setRollRate(rollRate);

    const ForceMap aeroData =
        aerodynamicCalculator.getForceAnalysis(configuration, conditions, &warnings);
    const AerodynamicForces* const forces = aeroData.get(&component);
    if (forces != nullptr)
    {
        return forces->getCD();
    }
    return 0;
}

double getOverrideCD(const RocketComponent& component, double defaultMach)
{
    if (!component.isCDOverridden())
    {
        return getComponentCD(component, 0, 0, defaultMach, 0);
    }
    return component.getOverrideCD();
}

}  // namespace QtRocket::ComponentDrag
