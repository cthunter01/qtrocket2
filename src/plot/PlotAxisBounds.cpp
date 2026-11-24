#include "QtRocket/plot/PlotAxisBounds.h"

#include <cmath>
#include <limits>

namespace QtRocket
{

void PlotAxisBounds::addBound(double value) noexcept
{
    if (value < m_minValue || std::isnan(m_minValue))
    {
        m_minValue = value;
    }
    if (value > m_maxValue || std::isnan(m_maxValue))
    {
        m_maxValue = value;
    }
}

void PlotAxisBounds::reset() noexcept
{
    m_minValue = std::numeric_limits<double>::quiet_NaN();
    m_maxValue = std::numeric_limits<double>::quiet_NaN();
}

}  // namespace QtRocket
