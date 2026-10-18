#pragma once

#include <vector>

#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/util/Geometry2D.h"

namespace QtRocket
{

/// An elliptical arc: java.awt.geom.Arc2D.Double as OpenRocket's shape providers use it, with its
/// path iterator, bounds and containment test. The ellipse is given by its framing rectangle; the
/// start and the extent are in degrees, counter-clockwise in Java2D's y-down frame (so clockwise
/// in the figure's y-up frame), with 0 degrees at the right-hand end of the horizontal axis, and
/// are kept as given (no normalisation).
class FigureArc
{
public:
    /// How the arc is closed (Arc2D.OPEN, CHORD and PIE).
    enum class Type
    {
        /// Not closed: only the curve.
        OPEN,
        /// Closed by the straight chord from the end back to the start.
        CHORD,
        /// Closed by lines from the end to the centre and from the centre back to the start.
        PIE,
    };

    constexpr FigureArc() noexcept = default;

    constexpr FigureArc(double x, double y, double width, double height, double angleStart,
                        double angleExtent, Type type) noexcept
      : m_x(x),
        m_y(y),
        m_width(width),
        m_height(height),
        m_angleStart(angleStart),
        m_angleExtent(angleExtent),
        m_type(type)
    {
    }

    [[nodiscard]] constexpr double x() const noexcept { return m_x; }
    [[nodiscard]] constexpr double y() const noexcept { return m_y; }
    [[nodiscard]] constexpr double width() const noexcept { return m_width; }
    [[nodiscard]] constexpr double height() const noexcept { return m_height; }
    /// The start angle in degrees (Arc2D.getAngleStart).
    [[nodiscard]] constexpr double angleStart() const noexcept { return m_angleStart; }
    /// The angular extent in degrees (Arc2D.getAngleExtent).
    [[nodiscard]] constexpr double angleExtent() const noexcept { return m_angleExtent; }
    /// The closure (Arc2D.getArcType).
    [[nodiscard]] constexpr Type type() const noexcept { return m_type; }

    /// True when the width or the height of the frame is not positive (Arc2D.Double.isEmpty).
    [[nodiscard]] bool isEmpty() const noexcept;

    /// Arc2D.getBounds2D: for an empty arc the frame; otherwise the tight box of the arc's end
    /// points and the quadrant points (0, 90, 180, 270 degrees) it passes, plus the centre of
    /// the frame for a PIE.
    [[nodiscard]] Rectangle2D bounds() const noexcept;

    /// Arc2D.containsAngle: true when @p angle (degrees) lies within the arc's angular range,
    /// the start included and the end excluded; always for an extent of at least 360 degrees in
    /// magnitude.
    [[nodiscard]] bool containsAngle(double angle) const noexcept;

    /// Arc2D.contains(double, double): the point is strictly inside the full ellipse and, for a
    /// PIE, within the angular range; for a CHORD or an OPEN arc (both closed by the chord, as
    /// Java tests them), inside the region the chord cuts off.
    [[nodiscard]] bool contains(Point2D p) const noexcept;

    /// The outline as ArcIterator.currentSegment(double[]) gives it: a move to the start point,
    /// ceil(|extent| / 90) cubic curves (4 for 360 degrees or more, none for a zero extent), then
    /// for a PIE a line to the centre and a close, for a CHORD a close, for an OPEN arc nothing.
    /// Nothing when the width or the height is negative.
    [[nodiscard]] std::vector<PathSegment> segments() const;

    /// Equal frames, angles and types.
    [[nodiscard]] constexpr bool operator==(const FigureArc&) const noexcept = default;

private:
    double m_x{0.0};
    double m_y{0.0};
    double m_width{0.0};
    double m_height{0.0};
    double m_angleStart{0.0};
    double m_angleExtent{0.0};
    Type   m_type{Type::OPEN};
};

}  // namespace QtRocket
