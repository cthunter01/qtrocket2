#include "QtRocket/figure/FigureRect.h"

#include <vector>

#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/util/Geometry2D.h"

namespace QtRocket
{

bool FigureRect::isEmpty() const noexcept
{
    return m_width <= 0.0 || m_height <= 0.0;
}

Rectangle2D FigureRect::bounds() const noexcept
{
    return Rectangle2D{.x = m_x, .y = m_y, .width = m_width, .height = m_height};
}

bool FigureRect::contains(Point2D p) const noexcept
{
    return p.x >= m_x && p.y >= m_y && p.x < m_x + m_width && p.y < m_y + m_height;
}

std::vector<PathSegment> FigureRect::segments() const
{
    if (m_width < 0 || m_height < 0)
    {
        return {};
    }
    // RectIterator: index 1 and 2 add the width, index 2 and 3 the height.
    const double right = m_x + m_width;
    const double top   = m_y + m_height;
    return {PathSegment::moveTo({m_x, m_y}),   PathSegment::lineTo({right, m_y}),
            PathSegment::lineTo({right, top}), PathSegment::lineTo({m_x, top}),
            PathSegment::lineTo({m_x, m_y}),   PathSegment::close()};
}

std::vector<PathSegment> FigureRect::floatSegments() const
{
    if (m_width < 0 || m_height < 0)
    {
        return {};
    }
    const auto  x     = static_cast<float>(m_x);
    const auto  y     = static_cast<float>(m_y);
    const float right = x + static_cast<float>(m_width);
    const float top   = y + static_cast<float>(m_height);
    const auto  point = [](float px, float py) {
        return Point2D{static_cast<double>(px), static_cast<double>(py)};
    };
    return {PathSegment::moveTo(point(x, y)),       PathSegment::lineTo(point(right, y)),
            PathSegment::lineTo(point(right, top)), PathSegment::lineTo(point(x, top)),
            PathSegment::lineTo(point(x, y)),       PathSegment::close()};
}

}  // namespace QtRocket
