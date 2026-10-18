#include "QtRocket/figure/FigureLine.h"

#include <vector>

#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/util/Geometry2D.h"

namespace QtRocket
{

Rectangle2D FigureLine::bounds() const noexcept
{
    Rectangle2D result;
    if (m_x1 < m_x2)
    {
        result.x     = m_x1;
        result.width = m_x2 - m_x1;
    }
    else
    {
        result.x     = m_x2;
        result.width = m_x1 - m_x2;
    }
    if (m_y1 < m_y2)
    {
        result.y      = m_y1;
        result.height = m_y2 - m_y1;
    }
    else
    {
        result.y      = m_y2;
        result.height = m_y1 - m_y2;
    }
    return result;
}

std::vector<PathSegment> FigureLine::segments() const
{
    return {PathSegment::moveTo({m_x1, m_y1}), PathSegment::lineTo({m_x2, m_y2})};
}

}  // namespace QtRocket
