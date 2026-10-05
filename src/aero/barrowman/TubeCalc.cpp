#include "QtRocket/aero/barrowman/TubeCalc.h"

#include <cmath>
#include <numbers>

#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/aero/barrowman/RocketComponentCalc.h"
#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/Tube.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

TubeCalc::TubeCalc(const Tube& tube)
  : RocketComponentCalc(tube),
    m_length(tube.getLength()),
    m_diameter(2 * tube.getInnerRadius()),
    m_innerArea(std::numbers::pi * MathUtil::pow2(tube.getInnerRadius())),
    // Java: totalArea - innerArea
    m_frontalArea((std::numbers::pi * MathUtil::pow2(tube.getOuterRadius())) - m_innerArea),
    // roughness; note we don't maintain surface roughness of interior separately from exterior.
    m_epsilon(roughnessSize(tube.getFinish()))
{
}

double TubeCalc::calculatePressureCD(const FlightConditions& conditions, double stagnationCD,
                                     double baseCD, WarningSet& /*warnings*/)
{
    // If we aren't moving, treat CD as 0
    const double v = conditions.getVelocity();
    if (v < MathUtil::kEpsilon)
    {
        return 0;
    }

    // Need to check for tube inner area 0 in case of rockets using launch lugs with an inner
    // radius of 0 to emulate rail guides (or just weird rockets, of course)
    double tubeCD = 0.0;
    if (m_innerArea > MathUtil::kEpsilon)
    {
        // Current atmospheric conditions (Java also reads the pressure and the temperature,
        // which it does not use)
        const AtmosphericConditions& atmosphere = conditions.getAtmosphericConditions();
        const double                 rho        = atmosphere.getDensity();

        // Reynolds number (note Reynolds number for the interior of a pipe is based on
        // diameter, not length (t))
        const double re = v * m_diameter / atmosphere.getKinematicViscosity();

        // friction coefficient using Swamee-Jain equation
        const double f = 0.25 / MathUtil::pow2(std::log10((m_epsilon / (3.7 * m_diameter)) +
                                                          (5.74 / MathUtil::javaPow(re, 0.9))));

        // pressure drop using Darcy-Weissbach equation
        const double deltap = f * (m_length * rho * MathUtil::pow2(v)) / (2 * m_diameter);

        // drag coefficient of tube interior from pressure drop
        tubeCD = 2 * (deltap * m_innerArea) / (rho * MathUtil::pow2(v) * m_innerArea);
    }

    // convert to CD and return
    return ((tubeCD * m_innerArea) + (0.7 * (stagnationCD + baseCD) * m_frontalArea)) /
           conditions.getRefArea();
}

}  // namespace QtRocket
