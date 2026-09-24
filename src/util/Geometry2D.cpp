#include "QtRocket/util/Geometry2D.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <ostream>

namespace QtRocket
{

std::ostream& operator<<(std::ostream& os, const Rectangle2D& rect)
{
    return os << std::format("Rectangle2D[x={}, y={}, w={}, h={}]", rect.x, rect.y, rect.width,
                             rect.height);
}

double distance(Point2D p, Point2D q) noexcept
{
    return std::sqrt(distanceSquared(p, q));
}

double distanceSquared(Point2D p, Point2D q) noexcept
{
    const double dx = p.x - q.x;
    const double dy = p.y - q.y;
    return (dx * dx) + (dy * dy);
}

int relativeCcw(Point2D a, Point2D b, Point2D p) noexcept
{
    // Everything relative to a, as Line2D.relativeCCW does.
    const double x2  = b.x - a.x;
    const double y2  = b.y - a.y;
    double       px  = p.x - a.x;
    double       py  = p.y - a.y;
    double       ccw = (px * y2) - (py * x2);
    if (ccw == 0.0)
    {
        // The point is collinear: classify it by its projection onto the segment. A negative
        // projection puts it beyond a; a positive projection from b's end puts it beyond b.
        ccw = (px * x2) + (py * y2);
        if (ccw > 0.0)
        {
            px -= x2;
            py -= y2;
            ccw = std::max((px * x2) + (py * y2), 0.0);
        }
    }
    if (ccw < 0.0)
    {
        return -1;
    }
    if (ccw > 0.0)
    {
        return 1;
    }
    return 0;
}

bool segmentsIntersect(Point2D p1, Point2D p2, Point2D p3, Point2D p4) noexcept
{
    return (relativeCcw(p1, p2, p3) * relativeCcw(p1, p2, p4) <= 0) &&
           (relativeCcw(p3, p4, p1) * relativeCcw(p3, p4, p2) <= 0);
}

double pointToSegmentDistance(Point2D p, Point2D a, Point2D b) noexcept
{
    return std::sqrt(pointToSegmentDistanceSquared(p, a, b));
}

double pointToSegmentDistanceSquared(Point2D p, Point2D a, Point2D b) noexcept
{
    // The segment and the point as vectors from a, as Line2D.ptSegDistSq does.
    const double x2 = b.x - a.x;
    const double y2 = b.y - a.y;
    double       px = p.x - a.x;
    double       py = p.y - a.y;

    double dotprod   = (px * x2) + (py * y2);
    double projlenSq = 0.0;  // the squared length of the point's projection clipped to the segment
    if (dotprod > 0.0)
    {
        // Not beyond a: measure from b's end, with both vectors reversed (same dot product).
        px      = x2 - px;
        py      = y2 - py;
        dotprod = (px * x2) + (py * y2);
        if (dotprod > 0.0)
        {
            // Between a and b: dotprod is the projection times the segment length.
            projlenSq = dotprod * dotprod / ((x2 * x2) + (y2 * y2));
        }
    }
    // The distance to the segment is the point vector minus its projection.
    return std::max(((px * px) + (py * py)) - projlenSq, 0.0);
}

}  // namespace QtRocket
