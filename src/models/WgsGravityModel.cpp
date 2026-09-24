#include "QtRocket/models/WgsGravityModel.h"

#include <cmath>

#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/WorldCoordinate.h"

namespace QtRocket
{

double WgsGravityModel::getGravity(const WorldCoordinate& wc) const
{
    const double sin2lat = MathUtil::pow2(std::sin(wc.getLatitudeRad()));
    const double g0      = 9.7803267714 * ((1.0 + (0.00193185138639 * sin2lat)) /
                                           std::sqrt(1.0 - (0.00669437999013 * sin2lat)));

    // The correction for altitude assumes a spherical Earth; it is small, so that does not
    // matter much. The gravity of the atmosphere itself is not taken into account either.
    return g0 *
           MathUtil::pow2(WorldCoordinate::kRearth / (WorldCoordinate::kRearth + wc.getAltitude()));
}

}  // namespace QtRocket
