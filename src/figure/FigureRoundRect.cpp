#include "QtRocket/figure/FigureRoundRect.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

namespace
{

// RoundRectIterator's acv = (1 - cv) / 2 with cv = 4/3 a b / c, a = 1 - cos(PI/4), b = tan(PI/4)
// and c = sqrt(1 + b^2) - 1 + a, as Java's class initialisation computes it (JDK 17; the probe
// ConstProbe.java gives cv = 0.5522847498307933 and this acv). Pinned rather than computed: the
// last bits of cos and tan differ between Java and the C libraries.
// NOLINTNEXTLINE(modernize-use-std-numbers): Java's computed value, not an approximation
constexpr double kAcv = 0.22385762508460333;

// One step of the outline: its type and its points, each {v0, v1, v2, v3} for the point
// (x + v0*w + v1*aw, y + v2*h + v3*ah).
struct RoundRectStep
{
    PathSegment::Type                    type;
    std::array<std::array<double, 4>, 3> points;
};

constexpr std::array<RoundRectStep, 9> kSteps{{
    {.type = PathSegment::Type::MOVE_TO, .points = {{{0.0, 0.0, 0.0, 0.5}}}},
    {.type = PathSegment::Type::LINE_TO, .points = {{{0.0, 0.0, 1.0, -0.5}}}},
    {.type   = PathSegment::Type::CUBIC_TO,
     .points = {{{0.0, 0.0, 1.0, -kAcv}, {0.0, kAcv, 1.0, 0.0}, {0.0, 0.5, 1.0, 0.0}}}},
    {.type = PathSegment::Type::LINE_TO, .points = {{{1.0, -0.5, 1.0, 0.0}}}},
    {.type   = PathSegment::Type::CUBIC_TO,
     .points = {{{1.0, -kAcv, 1.0, 0.0}, {1.0, 0.0, 1.0, -kAcv}, {1.0, 0.0, 1.0, -0.5}}}},
    {.type = PathSegment::Type::LINE_TO, .points = {{{1.0, 0.0, 0.0, 0.5}}}},
    {.type   = PathSegment::Type::CUBIC_TO,
     .points = {{{1.0, 0.0, 0.0, kAcv}, {1.0, -kAcv, 0.0, 0.0}, {1.0, -0.5, 0.0, 0.0}}}},
    {.type = PathSegment::Type::LINE_TO, .points = {{{0.0, 0.5, 0.0, 0.0}}}},
    {.type   = PathSegment::Type::CUBIC_TO,
     .points = {{{0.0, kAcv, 0.0, 0.0}, {0.0, 0.0, 0.0, kAcv}, {0.0, 0.0, 0.0, 0.5}}}},
}};

}  // namespace

bool FigureRoundRect::isEmpty() const noexcept
{
    return m_width <= 0.0 || m_height <= 0.0;
}

Rectangle2D FigureRoundRect::bounds() const noexcept
{
    return Rectangle2D{.x = m_x, .y = m_y, .width = m_width, .height = m_height};
}

bool FigureRoundRect::contains(Point2D p) const noexcept
{
    if (isEmpty())
    {
        return false;
    }
    double       rrx0 = m_x;
    double       rry0 = m_y;
    const double rrx1 = rrx0 + m_width;
    const double rry1 = rry0 + m_height;
    // Trivial rejection: outside the frame.
    if (p.x < rrx0 || p.y < rry0 || p.x >= rrx1 || p.y >= rry1)
    {
        return false;
    }
    const double aw = MathUtil::javaMin(m_width, std::abs(m_arcWidth)) / 2.0;
    const double ah = MathUtil::javaMin(m_height, std::abs(m_arcHeight)) / 2.0;
    // Between the corners horizontally or vertically: inside. Otherwise rrx0 and rry0 end up as
    // the centre of the corner ellipse the point is in, as Java's chained assignments leave them.
    rrx0 += aw;
    if (p.x >= rrx0)
    {
        rrx0 = rrx1 - aw;
        if (p.x < rrx0)
        {
            return true;
        }
    }
    rry0 += ah;
    if (p.y >= rry0)
    {
        rry0 = rry1 - ah;
        if (p.y < rry0)
        {
            return true;
        }
    }
    const double nx = (p.x - rrx0) / aw;
    const double ny = (p.y - rry0) / ah;
    return ((nx * nx) + (ny * ny)) <= 1.0;
}

std::vector<PathSegment> FigureRoundRect::segments() const
{
    const double aw = MathUtil::javaMin(m_width, std::abs(m_arcWidth));
    const double ah = MathUtil::javaMin(m_height, std::abs(m_arcHeight));
    if (aw < 0 || ah < 0)
    {
        return {};
    }
    const auto at = [&](const std::array<double, 4>& v) {
        return Point2D{m_x + (v[0] * m_width) + (v[1] * aw), m_y + (v[2] * m_height) + (v[3] * ah)};
    };
    std::vector<PathSegment> result;
    result.reserve(kSteps.size() + 1);
    for (const RoundRectStep& step : kSteps)
    {
        PathSegment segment{.type = step.type, .points = {}};
        const auto  count = static_cast<std::size_t>(PathSegment::pointCount(step.type));
        for (std::size_t i = 0; i < count; ++i)
        {
            segment.points.at(i) = at(step.points.at(i));
        }
        result.push_back(segment);
    }
    result.push_back(PathSegment::close());
    return result;
}

}  // namespace QtRocket
