#pragma once

#include <array>
#include <iosfwd>

#include "QtRocket/util/Geometry2D.h"

namespace QtRocket
{

/// One segment of a shape's outline, as java.awt.geom.PathIterator.currentSegment(double[])
/// returns it: its type and the points that follow the current point. The figure's shapes give
/// their outlines as these segments (FigureShape::segments()), in Java's order and with Java's
/// coordinates, so that a painter can build any shape as a path and a test can compare shapes
/// segment by segment.
struct PathSegment
{
    /// PathIterator's SEG_MOVETO, SEG_LINETO, SEG_QUADTO, SEG_CUBICTO and SEG_CLOSE.
    enum class Type
    {
        /// Starts a new subpath at points[0].
        MOVE_TO,
        /// A straight line to points[0].
        LINE_TO,
        /// A quadratic Bezier curve through the control point points[0] to points[1].
        QUAD_TO,
        /// A cubic Bezier curve through the control points points[0] and points[1] to points[2].
        CUBIC_TO,
        /// A straight line back to the start of the subpath, which it closes; no points.
        CLOSE,
    };

    [[nodiscard]] static constexpr PathSegment moveTo(Point2D p) noexcept
    {
        return {.type = Type::MOVE_TO, .points = {p, Point2D{}, Point2D{}}};
    }

    [[nodiscard]] static constexpr PathSegment lineTo(Point2D p) noexcept
    {
        return {.type = Type::LINE_TO, .points = {p, Point2D{}, Point2D{}}};
    }

    [[nodiscard]] static constexpr PathSegment quadTo(Point2D control, Point2D end) noexcept
    {
        return {.type = Type::QUAD_TO, .points = {control, end, Point2D{}}};
    }

    [[nodiscard]] static constexpr PathSegment cubicTo(Point2D control1, Point2D control2,
                                                       Point2D end) noexcept
    {
        return {.type = Type::CUBIC_TO, .points = {control1, control2, end}};
    }

    [[nodiscard]] static constexpr PathSegment close() noexcept
    {
        return {.type = Type::CLOSE, .points = {}};
    }

    /// How many of points the type uses: 1 for MOVE_TO and LINE_TO, 2 for QUAD_TO, 3 for
    /// CUBIC_TO and 0 for CLOSE. The others are (0, 0).
    [[nodiscard]] static constexpr int pointCount(Type type) noexcept
    {
        switch (type)
        {
            case Type::MOVE_TO:
            case Type::LINE_TO:
                return 1;
            case Type::QUAD_TO:
                return 2;
            case Type::CUBIC_TO:
                return 3;
            case Type::CLOSE:
                return 0;
        }
        return 0;
    }

    /// pointCount(type).
    [[nodiscard]] constexpr int pointCount() const noexcept { return pointCount(type); }

    /// Equal types and all three points equal (so a NaN coordinate is unequal to everything).
    [[nodiscard]] constexpr bool operator==(const PathSegment&) const noexcept = default;

    Type                   type{Type::MOVE_TO};
    std::array<Point2D, 3> points{};
};

/// The letter of the type, as SVG path data spells it: M, L, Q, C or Z.
[[nodiscard]] char pathSegmentLetter(PathSegment::Type type) noexcept;

/// Writes the letter and the used points, e.g. "C 1,2 3,4 5,6" or "Z" (shortest round-trip
/// digits), so that GoogleTest prints a readable value when two segments differ.
std::ostream& operator<<(std::ostream& os, const PathSegment& segment);

}  // namespace QtRocket
