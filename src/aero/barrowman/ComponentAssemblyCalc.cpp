#include "QtRocket/aero/barrowman/ComponentAssemblyCalc.h"

#include "QtRocket/aero/barrowman/RocketComponentCalc.h"

namespace QtRocket
{

ComponentAssemblyCalc::ComponentAssemblyCalc(const RocketComponent& component)
  : RocketComponentCalc(component)
{
}

void ComponentAssemblyCalc::calculateNonaxialForces(const FlightConditions& /*conditions*/,
                                                    const Transformation& /*transform*/,
                                                    AerodynamicForces& /*forces*/,
                                                    WarningSet& /*warnings*/)
{
    // empty
}

double ComponentAssemblyCalc::calculateFrictionCD(const FlightConditions& /*conditions*/,
                                                  double /*componentCf*/, WarningSet& /*warnings*/)
{
    return 0;
}

double ComponentAssemblyCalc::calculatePressureCD(const FlightConditions& /*conditions*/,
                                                  double /*stagnationCD*/, double /*baseCD*/,
                                                  WarningSet& /*warnings*/)
{
    return 0;
}

}  // namespace QtRocket
