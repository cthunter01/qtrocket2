#include "QtRocket/rocket/RocketUtils.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket::RocketUtils
{

double getLength(const Rocket& rocket)
{
    double                        length = 0;
    const std::vector<Coordinate> bounds = rocket.getSelectedConfiguration().getBounds();
    if (!bounds.empty())
    {
        double minX = std::numeric_limits<double>::infinity();
        double maxX = -std::numeric_limits<double>::infinity();
        // std::min() and std::max() keep the current value for a NaN, as Java's comparisons do.
        for (const Coordinate& c : bounds)
        {
            minX = std::min(minX, c.x);
            maxX = std::max(maxX, c.x);
        }
        length = maxX - minX;
    }
    return length;
}

}  // namespace QtRocket::RocketUtils
