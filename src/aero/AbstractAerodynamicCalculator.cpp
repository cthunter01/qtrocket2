#include "QtRocket/aero/AbstractAerodynamicCalculator.h"

#include <limits>
#include <numbers>

#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

Coordinate AbstractAerodynamicCalculator::getWorstCP(const FlightConfiguration& configuration,
                                                     FlightConditions&          conditions,
                                                     WarningSet*                warnings)
{
    FlightConditions cond = conditions.clone();
    Coordinate       worst{std::numeric_limits<double>::max()};
    double           theta = 0;

    for (int i = 0; i < kDivisions; i++)
    {
        cond.setTheta(2 * std::numbers::pi * i / kDivisions);
        const Coordinate cp = getCP(configuration, cond, warnings);
        if ((cp.weight > MathUtil::kEpsilon) && (cp.x < worst.x))
        {
            worst = cp;
            theta = cond.getTheta();
        }
    }

    conditions.setTheta(theta);
    return worst;
}

void AbstractAerodynamicCalculator::checkCache(const FlightConfiguration& configuration)
{
    const Rocket& rocket = configuration.getRocket();
    if (m_rocketAeroModId != rocket.getAerodynamicModId() ||
        m_rocketTreeModId != rocket.getTreeModId())
    {
        m_rocketAeroModId = rocket.getAerodynamicModId();
        m_rocketTreeModId = rocket.getTreeModId();
        voidAerodynamicCache();
    }
}

void AbstractAerodynamicCalculator::voidAerodynamicCache()
{
    // No-op
}

}  // namespace QtRocket
