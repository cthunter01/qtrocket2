#include "QtRocket/models/InterpolatingAtmosphericModel.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <mutex>
#include <vector>

#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

AtmosphericConditions InterpolatingAtmosphericModel::getConditions(double altitude) const
{
    std::call_once(m_levelsOnce, [this] { m_levels = computeLayers(); });
    if (m_levels.empty())
    {
        bug("InterpolatingAtmosphericModel: no levels below the maximum altitude");
    }

    if (altitude <= 0)
    {
        return m_levels.front();
    }

    const std::size_t maxIndex = m_levels.size() - 1;
    if (altitude >= kDelta * static_cast<double>(maxIndex) || maxIndex == 0)
    {
        // maxIndex == 0 only matters for a NaN altitude, where Java reads past a one-level table.
        return m_levels.back();
    }

    // (int) Math.floor(altitude / DELTA): 0 for a NaN altitude. The clamp only acts when the
    // quotient of an altitude just below the top rounds up to maxIndex (see the header).
    const auto lowerIndex =
        std::min(static_cast<std::size_t>(MathUtil::javaIntCast(std::floor(altitude / kDelta))),
                 maxIndex - 1);
    const double fraction = (altitude - (static_cast<double>(lowerIndex) * kDelta)) / kDelta;

    const AtmosphericConditions& lower = m_levels[lowerIndex];
    const AtmosphericConditions& upper = m_levels[lowerIndex + 1];

    return AtmosphericConditions{
        MathUtil::interpolate(lower.getTemperature(), upper.getTemperature(), fraction),
        MathUtil::interpolate(lower.getPressure(), upper.getPressure(), fraction),
        MathUtil::interpolate(lower.getRelativeHumidity(), upper.getRelativeHumidity(), fraction)};
}

std::vector<AtmosphericConditions> InterpolatingAtmosphericModel::computeLayers() const
{
    const double                       max  = getMaxAltitude();
    const int                          size = MathUtil::javaIntCast(std::ceil(max / kDelta));
    std::vector<AtmosphericConditions> newLevels;
    newLevels.reserve(static_cast<std::size_t>(std::max(size, 0)));

    for (int i = 0; i < size; ++i)
    {
        newLevels.push_back(getExactConditions(static_cast<double>(i) * kDelta));
    }
    return newLevels;
}

}  // namespace QtRocket
