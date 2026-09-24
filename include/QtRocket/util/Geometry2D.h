#pragma once

namespace QtRocket
{

/// A point in the plane (java.awt.geom.Point2D.Double).
struct Point2D
{
    constexpr Point2D() noexcept = default;
    constexpr Point2D(double px, double py) noexcept : x(px), y(py) { }

    double x{0.0};
    double y{0.0};

    [[nodiscard]] constexpr bool operator==(const Point2D&) const noexcept = default;
};

/// |p - q| (java.awt.geom.Point2D.distance).
[[nodiscard]] double distance(Point2D p, Point2D q) noexcept;

/// |p - q|^2 (java.awt.geom.Point2D.distanceSq).
[[nodiscard]] double distanceSquared(Point2D p, Point2D q) noexcept;

/// Where @p p lies relative to the directed segment a -> b, as java.awt.geom.Line2D.relativeCCW:
/// -1 when the segment has to pivot around a from +x towards +y to point at p, +1 when it has to
/// pivot the other way, and 0 when p is on the segment. A point collinear with the segment but
/// outside it gives -1 beyond a and +1 beyond b. Any NaN gives 0.
[[nodiscard]] int relativeCcw(Point2D a, Point2D b, Point2D p) noexcept;

/// True when the closed segments p1-p2 and p3-p4 share at least one point, exactly as
/// java.awt.geom.Line2D.linesIntersect: touching end points, T junctions and collinear
/// overlapping segments intersect, collinear segments with a gap between them do not, a
/// zero-length segment intersects whatever it lies on, and a NaN coordinate counts as
/// intersecting.
[[nodiscard]] bool segmentsIntersect(Point2D p1, Point2D p2, Point2D p3, Point2D p4) noexcept;

/// The distance from @p p to the closed segment a-b (java.awt.geom.Line2D.ptSegDist).
[[nodiscard]] double pointToSegmentDistance(Point2D p, Point2D a, Point2D b) noexcept;

/// The square of pointToSegmentDistance (java.awt.geom.Line2D.ptSegDistSq).
[[nodiscard]] double pointToSegmentDistanceSquared(Point2D p, Point2D a, Point2D b) noexcept;

}  // namespace QtRocket
