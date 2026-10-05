#include "QtRocket/aero/barrowman/LaunchLugCalc.h"

#include "QtRocket/aero/barrowman/TubeCalc.h"
#include "QtRocket/rocket/LaunchLug.h"

namespace QtRocket
{

LaunchLugCalc::LaunchLugCalc(const LaunchLug& lug) : TubeCalc(lug) { }

void LaunchLugCalc::calculateNonaxialForces(const FlightConditions& /*conditions*/,
                                            const Transformation& /*transform*/,
                                            AerodynamicForces& /*forces*/, WarningSet& /*warnings*/)
{
    // Nothing to be done
}

double LaunchLugCalc::calculateFrictionCD(const FlightConditions& /*conditions*/,
                                          double /*componentCf*/, WarningSet& /*warnings*/)
{
    // launch lug doesn't add enough area to worry about
    return 0;
}

}  // namespace QtRocket
