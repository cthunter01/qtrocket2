#pragma once

#include <span>
#include <vector>

#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/figure/WindingRule.h"
#include "QtRocket/util/Geometry2D.h"

namespace QtRocket
{

class FigureShape;

/// A general outline of straight and curved segments: java.awt.geom.Path2D.Double, or
/// Path2D.Float with Precision::FLOAT, as OpenRocket's shape providers use them. The segments are
/// kept exactly as Java keeps them, so a path built by the same calls compares segment by segment
/// with Java's PathIterator: consecutive moveTo() calls collapse into the last one, closePath()
/// after a close adds nothing, and append() follows Path2D.append's rules. A FLOAT path rounds
/// every coordinate it stores to a float (and keeps it as that float's exact double value), and
/// computes its bounds in float arithmetic, as Path2D.Float does.
///
/// Every segment but the first needs a current point: lineTo(), quadTo(), curveTo() and
/// closePath() on an empty path are a programming error (a BugError where Java throws an
/// IllegalPathStateException), so a non-empty path always starts with a MOVE_TO.
class FigurePath
{
public:
    /// The coordinate type of Java's two path classes.
    enum class Precision
    {
        /// Path2D.Double: coordinates are doubles.
        DOUBLE,
        /// Path2D.Float: coordinates are rounded to floats.
        FLOAT,
    };

    /// An empty Path2D.Double with the NON_ZERO rule.
    FigurePath() noexcept = default;

    /// An empty path of @p precision with the winding rule @p rule (Path2D.Float() or
    /// Path2D.Double(rule)).
    explicit FigurePath(Precision precision, WindingRule rule = WindingRule::NON_ZERO) noexcept
      : m_precision(precision), m_windingRule(rule)
    {
    }

    [[nodiscard]] Precision   precision() const noexcept { return m_precision; }
    [[nodiscard]] WindingRule windingRule() const noexcept { return m_windingRule; }

    /// The segments in order, as the path's PathIterator returns them (a FLOAT path's
    /// coordinates are the exact doubles of its floats).
    [[nodiscard]] const std::vector<PathSegment>& segments() const noexcept { return m_segments; }

    /// True when no segment has been added (Path2D's numTypes == 0).
    [[nodiscard]] bool isEmpty() const noexcept { return m_segments.empty(); }

    /// Starts a new subpath at (x, y); right after another moveTo() it moves that one instead
    /// (Path2D.moveTo).
    void moveTo(double x, double y);

    /// A straight line from the current point to (x, y).
    void lineTo(double x, double y);

    /// A quadratic curve through the control point (x1, y1) to (x2, y2).
    void quadTo(double x1, double y1, double x2, double y2);

    /// A cubic curve through the control points (x1, y1) and (x2, y2) to (x3, y3)
    /// (Path2D.curveTo).
    void curveTo(double x1, double y1, double x2, double y2, double x3, double y3);

    /// Closes the current subpath, unless the last segment is a close already.
    void closePath();

    /// Appends @p segments as Path2D.append(PathIterator, connect) does: with @p connect, a
    /// leading MOVE_TO becomes a LINE_TO from the current point, or disappears when it lands on
    /// the last point of a path whose last segment is not a close (a path without segments just
    /// moves). The segments must form a path a PathIterator could return.
    void append(std::span<const PathSegment> segments, bool connect);

    /// Appends the outline of @p shape (Path2D.append(Shape, connect)): the shape's segments(),
    /// or, into a FLOAT path, the segments Java's iterator gives a float array (which differ
    /// from rounded doubles only for a FigureRect: FigureRect::floatSegments()).
    void append(const FigureShape& shape, bool connect);

    /// Path2D.getBounds2D as JDK 17 computes it: the box of every stored coordinate, control
    /// points included (so not always tight around curves), computed in float arithmetic for a
    /// FLOAT path; (0, 0, 0, 0) for an empty path.
    [[nodiscard]] Rectangle2D bounds() const noexcept;

    /// Path2D.contains(double, double): whether the outline, every subpath implicitly closed
    /// (so an open polyline encloses the polygon of its points), winds around @p p under the
    /// path's rule, counted by Java's crossing rules (a point on a left or lower edge tends to be
    /// inside, on a right or upper edge outside). False for a path of fewer than two segments or
    /// a non-finite coordinate.
    [[nodiscard]] bool contains(Point2D p) const noexcept;

    /// Equal precisions, rules and segments.
    [[nodiscard]] bool operator==(const FigurePath&) const noexcept = default;

private:
    /// @p v as the path stores it: rounded to a float for a FLOAT path.
    [[nodiscard]] double stored(double v) const noexcept;

    /// @p p as the path stores it.
    [[nodiscard]] Point2D stored(Point2D p) const noexcept;

    /// The bug() of a segment that needs a current point on an empty path.
    void requireMoveTo() const;

    Precision                m_precision{Precision::DOUBLE};
    WindingRule              m_windingRule{WindingRule::NON_ZERO};
    std::vector<PathSegment> m_segments;
};

}  // namespace QtRocket
