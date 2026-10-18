#include "QtRocket/figure/FigurePath.h"

#include <cmath>
#include <cstddef>
#include <ranges>
#include <span>
#include <type_traits>
#include <vector>

#include "QtRocket/figure/FigureRect.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/figure/WindingRule.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Geometry2D.h"

namespace QtRocket
{

namespace
{

// The point-crossing counts of sun.awt.geom.Curve (JDK 17): how often, and in which direction,
// a segment crosses the ray from (px, py) towards +x. Upward crossings count +1, downward -1;
// the segment's lower end is included and its upper end excluded, so a vertex shared by two
// segments is counted once.

[[nodiscard]] int pointCrossingsForLine(double px, double py, double x0, double y0, double x1,
                                        double y1) noexcept
{
    if (py < y0 && py < y1)
    {
        return 0;
    }
    if (py >= y0 && py >= y1)
    {
        return 0;
    }
    // assert(y0 != y1);
    if (px >= x0 && px >= x1)
    {
        return 0;
    }
    if (px < x0 && px < x1)
    {
        return (y0 < y1) ? 1 : -1;
    }
    const double xIntercept = x0 + ((py - y0) * (x1 - x0) / (y1 - y0));
    if (px >= xIntercept)
    {
        return 0;
    }
    return (y0 < y1) ? 1 : -1;
}

// Curve.pointCrossingsForQuad: subdivides until the curve is entirely on one side of the point
// or the recursion is 52 levels deep (double precision has only 52 bits of mantissa).
[[nodiscard]] int pointCrossingsForQuad(double px, double py, double x0, double y0, double xc,
                                        double yc, double x1, double y1, int level) noexcept
{
    if (py < y0 && py < yc && py < y1)
    {
        return 0;
    }
    if (py >= y0 && py >= yc && py >= y1)
    {
        return 0;
    }
    // Note y0 could equal y1...
    if (px >= x0 && px >= xc && px >= x1)
    {
        return 0;
    }
    if (px < x0 && px < xc && px < x1)
    {
        if (py >= y0)
        {
            if (py < y1)
            {
                return 1;
            }
        }
        else if (py >= y1)
        {
            // py < y0
            return -1;
        }
        // py outside of y01 range, and/or y0==y1
        return 0;
    }
    if (level > 52)
    {
        return pointCrossingsForLine(px, py, x0, y0, x1, y1);
    }
    const double x0c = (x0 + xc) / 2;
    const double y0c = (y0 + yc) / 2;
    const double xc1 = (xc + x1) / 2;
    const double yc1 = (yc + y1) / 2;
    xc               = (x0c + xc1) / 2;
    yc               = (y0c + yc1) / 2;
    if (std::isnan(xc) || std::isnan(yc))
    {
        // [xy]c are NaN if any of [xy]0c or [xy]c1 are NaN, which they are if any of [xy][0c1]
        // are NaN or opposing infinities are added.
        return 0;
    }
    return pointCrossingsForQuad(px, py, x0, y0, x0c, y0c, xc, yc, level + 1) +
           pointCrossingsForQuad(px, py, xc, yc, xc1, yc1, x1, y1, level + 1);
}

// Curve.pointCrossingsForCubic, subdividing as pointCrossingsForQuad does.
[[nodiscard]] int pointCrossingsForCubic(double px, double py, double x0, double y0, double xc0,
                                         double yc0, double xc1, double yc1, double x1, double y1,
                                         int level) noexcept
{
    if (py < y0 && py < yc0 && py < yc1 && py < y1)
    {
        return 0;
    }
    if (py >= y0 && py >= yc0 && py >= yc1 && py >= y1)
    {
        return 0;
    }
    // Note y0 could equal yc0...
    if (px >= x0 && px >= xc0 && px >= xc1 && px >= x1)
    {
        return 0;
    }
    if (px < x0 && px < xc0 && px < xc1 && px < x1)
    {
        if (py >= y0)
        {
            if (py < y1)
            {
                return 1;
            }
        }
        else if (py >= y1)
        {
            // py < y0
            return -1;
        }
        // py outside of y01 range, and/or y0==yc0
        return 0;
    }
    if (level > 52)
    {
        return pointCrossingsForLine(px, py, x0, y0, x1, y1);
    }
    double xMid       = (xc0 + xc1) / 2;
    double yMid       = (yc0 + yc1) / 2;
    xc0               = (x0 + xc0) / 2;
    yc0               = (y0 + yc0) / 2;
    xc1               = (xc1 + x1) / 2;
    yc1               = (yc1 + y1) / 2;
    const double xc0m = (xc0 + xMid) / 2;
    const double yc0m = (yc0 + yMid) / 2;
    const double xmc1 = (xMid + xc1) / 2;
    const double ymc1 = (yMid + yc1) / 2;
    xMid              = (xc0m + xmc1) / 2;
    yMid              = (yc0m + ymc1) / 2;
    if (std::isnan(xMid) || std::isnan(yMid))
    {
        // [xy]mid are NaN if any of [xy]c0m or [xy]mc1 are NaN, which they are if any of
        // [xy][c][01] are NaN or opposing infinities are added.
        return 0;
    }
    return pointCrossingsForCubic(px, py, x0, y0, xc0, yc0, xc0m, yc0m, xMid, yMid, level + 1) +
           pointCrossingsForCubic(px, py, xMid, yMid, xmc1, ymc1, xc1, yc1, x1, y1, level + 1);
}

/// Path2D.pointCrossings over @p segments, the first of which is a MOVE_TO: the crossings of
/// every segment, each subpath closed by a line back to its start.
[[nodiscard]] int pointCrossings(std::span<const PathSegment> segments, Point2D p) noexcept
{
    if (segments.empty())
    {
        return 0;
    }
    Point2D move    = segments.front().points[0];
    Point2D current = move;
    int     crossings{0};
    for (const PathSegment& segment : segments.subspan(1))
    {
        const auto& pts = segment.points;
        switch (segment.type)
        {
            case PathSegment::Type::MOVE_TO:
                if (current.y != move.y)
                {
                    crossings +=
                        pointCrossingsForLine(p.x, p.y, current.x, current.y, move.x, move.y);
                }
                move    = pts[0];
                current = move;
                break;
            case PathSegment::Type::LINE_TO:
                crossings +=
                    pointCrossingsForLine(p.x, p.y, current.x, current.y, pts[0].x, pts[0].y);
                current = pts[0];
                break;
            case PathSegment::Type::QUAD_TO:
                crossings += pointCrossingsForQuad(p.x, p.y, current.x, current.y, pts[0].x,
                                                   pts[0].y, pts[1].x, pts[1].y, 0);
                current = pts[1];
                break;
            case PathSegment::Type::CUBIC_TO:
                crossings +=
                    pointCrossingsForCubic(p.x, p.y, current.x, current.y, pts[0].x, pts[0].y,
                                           pts[1].x, pts[1].y, pts[2].x, pts[2].y, 0);
                current = pts[2];
                break;
            case PathSegment::Type::CLOSE:
                if (current.y != move.y)
                {
                    crossings +=
                        pointCrossingsForLine(p.x, p.y, current.x, current.y, move.x, move.y);
                }
                current = move;
                break;
        }
    }
    if (current.y != move.y)
    {
        crossings += pointCrossingsForLine(p.x, p.y, current.x, current.y, move.x, move.y);
    }
    return crossings;
}

/// The last stored point of @p segments, whose last segment has points.
[[nodiscard]] Point2D lastPoint(const std::vector<PathSegment>& segments) noexcept
{
    const PathSegment& last = segments.back();
    return last.points.at(static_cast<std::size_t>(last.pointCount() - 1));
}

/// @p v in the arithmetic of @p T: rounded to a float for float.
template <typename T>
[[nodiscard]] T inArithmetic(double v) noexcept
{
    if constexpr (std::is_same_v<T, float>)
    {
        return static_cast<float>(v);
    }
    else
    {
        return v;
    }
}

[[nodiscard]] double toDouble(float v) noexcept
{
    return static_cast<double>(v);
}

[[nodiscard]] double toDouble(double v) noexcept
{
    return v;
}

/// The box of every point of @p segments in the arithmetic of @p T, scanned from the last point
/// to the first as Path2D.getBounds2D scans its coordinates (which decides the sign of a zero and
/// where a NaN sticks).
template <typename T>
[[nodiscard]] Rectangle2D boundsOf(const std::vector<PathSegment>& segments) noexcept
{
    bool first = true;
    T    x1{};
    T    y1{};
    T    x2{};
    T    y2{};
    for (const PathSegment& segment : std::views::reverse(segments))
    {
        for (int i = segment.pointCount() - 1; i >= 0; --i)
        {
            const Point2D& point = segment.points.at(static_cast<std::size_t>(i));
            const T        x     = inArithmetic<T>(point.x);
            const T        y     = inArithmetic<T>(point.y);
            if (first)
            {
                x1    = x;
                x2    = x;
                y1    = y;
                y2    = y;
                first = false;
                continue;
            }
            if (x < x1)
            {
                x1 = x;
            }
            if (y < y1)
            {
                y1 = y;
            }
            if (x > x2)
            {
                x2 = x;
            }
            if (y > y2)
            {
                y2 = y;
            }
        }
    }
    const T width  = x2 - x1;
    const T height = y2 - y1;
    return Rectangle2D{
        .x = toDouble(x1), .y = toDouble(y1), .width = toDouble(width), .height = toDouble(height)};
}

}  // namespace

double FigurePath::stored(double v) const noexcept
{
    if (m_precision == Precision::FLOAT)
    {
        // Java's (float) narrowing: round to nearest, overflowing to an infinity.
        return static_cast<double>(static_cast<float>(v));
    }
    return v;
}

Point2D FigurePath::stored(Point2D p) const noexcept
{
    return Point2D{stored(p.x), stored(p.y)};
}

void FigurePath::requireMoveTo() const
{
    if (m_segments.empty())
    {
        bug("missing initial moveto in path definition");
    }
}

void FigurePath::moveTo(double x, double y)
{
    const Point2D p = stored(Point2D{x, y});
    if (!m_segments.empty() && m_segments.back().type == PathSegment::Type::MOVE_TO)
    {
        m_segments.back().points[0] = p;
        return;
    }
    m_segments.push_back(PathSegment::moveTo(p));
}

void FigurePath::lineTo(double x, double y)
{
    requireMoveTo();
    m_segments.push_back(PathSegment::lineTo(stored(Point2D{x, y})));
}

void FigurePath::quadTo(double x1, double y1, double x2, double y2)
{
    requireMoveTo();
    m_segments.push_back(PathSegment::quadTo(stored(Point2D{x1, y1}), stored(Point2D{x2, y2})));
}

void FigurePath::curveTo(double x1, double y1, double x2, double y2, double x3, double y3)
{
    requireMoveTo();
    m_segments.push_back(PathSegment::cubicTo(stored(Point2D{x1, y1}), stored(Point2D{x2, y2}),
                                              stored(Point2D{x3, y3})));
}

void FigurePath::closePath()
{
    if (m_segments.empty() || m_segments.back().type != PathSegment::Type::CLOSE)
    {
        requireMoveTo();
        m_segments.push_back(PathSegment::close());
    }
}

void FigurePath::append(std::span<const PathSegment> segments, bool connect)
{
    for (const PathSegment& segment : segments)
    {
        const auto& pts = segment.points;
        switch (segment.type)
        {
            case PathSegment::Type::MOVE_TO:
            {
                if (!connect || m_segments.empty())
                {
                    moveTo(pts[0].x, pts[0].y);
                    break;
                }
                // Java compares the stored coordinates with the iterator's, which for a FLOAT
                // path are floats.
                const Point2D target = stored(pts[0]);
                if (m_segments.back().type != PathSegment::Type::CLOSE &&
                    lastPoint(m_segments) == target)
                {
                    // Collapse out initial moveto/lineto
                    break;
                }
                lineTo(pts[0].x, pts[0].y);
                break;
            }
            case PathSegment::Type::LINE_TO:
                lineTo(pts[0].x, pts[0].y);
                break;
            case PathSegment::Type::QUAD_TO:
                quadTo(pts[0].x, pts[0].y, pts[1].x, pts[1].y);
                break;
            case PathSegment::Type::CUBIC_TO:
                curveTo(pts[0].x, pts[0].y, pts[1].x, pts[1].y, pts[2].x, pts[2].y);
                break;
            case PathSegment::Type::CLOSE:
                closePath();
                break;
        }
        connect = false;
    }
}

void FigurePath::append(const FigureShape& shape, bool connect)
{
    if (m_precision == Precision::FLOAT)
    {
        if (const auto* rect = shape.getIf<FigureRect>())
        {
            append(rect->floatSegments(), connect);
            return;
        }
    }
    append(shape.segments(), connect);
}

Rectangle2D FigurePath::bounds() const noexcept
{
    if (m_precision == Precision::FLOAT)
    {
        return boundsOf<float>(m_segments);
    }
    return boundsOf<double>(m_segments);
}

bool FigurePath::contains(Point2D p) const noexcept
{
    // N * 0.0 is 0.0 only if N is finite.
    if ((p.x * 0.0) + (p.y * 0.0) != 0.0)
    {
        return false;
    }
    if (m_segments.size() < 2)
    {
        return false;
    }
    const int crossings = pointCrossings(m_segments, p);
    if (m_windingRule == WindingRule::NON_ZERO)
    {
        return crossings != 0;
    }
    return (crossings & 1) != 0;
}

}  // namespace QtRocket
