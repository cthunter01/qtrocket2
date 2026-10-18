#include "QtRocket/figure/FigureEllipse.h"

#include <array>
#include <vector>

#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/util/Geometry2D.h"

namespace QtRocket
{

namespace
{

// EllipseIterator's control points of four cubic curves that approximate a circle of radius 0.5
// centred at (0.5, 0.5), each {x1, y1, x2, y2, x3, y3} as fractions of the frame.
constexpr double kPcv = 0.5 + (FigureEllipse::kControlValue * 0.5);
constexpr double kNcv = 0.5 - (FigureEllipse::kControlValue * 0.5);

constexpr std::array<std::array<double, 6>, 4> kControlPoints{{
    {1.0, kPcv, kPcv, 1.0, 0.5, 1.0},
    {kNcv, 1.0, 0.0, kPcv, 0.0, 0.5},
    {0.0, kNcv, kNcv, 0.0, 0.5, 0.0},
    {kPcv, 0.0, 1.0, kNcv, 1.0, 0.5},
}};

}  // namespace

bool FigureEllipse::isEmpty() const noexcept
{
    return m_width <= 0.0 || m_height <= 0.0;
}

Rectangle2D FigureEllipse::bounds() const noexcept
{
    return Rectangle2D{.x = m_x, .y = m_y, .width = m_width, .height = m_height};
}

bool FigureEllipse::contains(Point2D p) const noexcept
{
    // Normalised to an ellipse of radius 0.5 centred at the origin.
    if (m_width <= 0.0)
    {
        return false;
    }
    const double normX = ((p.x - m_x) / m_width) - 0.5;
    if (m_height <= 0.0)
    {
        return false;
    }
    const double normY = ((p.y - m_y) / m_height) - 0.5;
    return ((normX * normX) + (normY * normY)) < 0.25;
}

std::vector<PathSegment> FigureEllipse::segments() const
{
    if (m_width < 0 || m_height < 0)
    {
        return {};
    }
    const auto at = [this](double fx, double fy) {
        return Point2D{m_x + (fx * m_width), m_y + (fy * m_height)};
    };
    std::vector<PathSegment> result;
    result.reserve(6);
    const std::array<double, 6>& last = kControlPoints[3];
    result.push_back(PathSegment::moveTo(at(last[4], last[5])));
    for (const std::array<double, 6>& c : kControlPoints)
    {
        result.push_back(PathSegment::cubicTo(at(c[0], c[1]), at(c[2], c[3]), at(c[4], c[5])));
    }
    result.push_back(PathSegment::close());
    return result;
}

}  // namespace QtRocket
