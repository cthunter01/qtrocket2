#pragma once

#include <vector>

#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/util/Geometry2D.h"

namespace QtRocket
{

/// A rectangle with rounded corners: java.awt.geom.RoundRectangle2D.Double as OpenRocket's shape
/// providers use it, with its path iterator, bounds and containment test. The arc width and
/// height are the full diameters of the corner ellipses (not radii) and are kept as given; the
/// iterator and contains() use their magnitudes clamped to the width and the height, as Java's do.
class FigureRoundRect
{
public:
    constexpr FigureRoundRect() noexcept = default;

    constexpr FigureRoundRect(double x, double y, double width, double height, double arcWidth,
                              double arcHeight) noexcept
      : m_x(x),
        m_y(y),
        m_width(width),
        m_height(height),
        m_arcWidth(arcWidth),
        m_arcHeight(arcHeight)
    {
    }

    [[nodiscard]] constexpr double x() const noexcept { return m_x; }
    [[nodiscard]] constexpr double y() const noexcept { return m_y; }
    [[nodiscard]] constexpr double width() const noexcept { return m_width; }
    [[nodiscard]] constexpr double height() const noexcept { return m_height; }
    [[nodiscard]] constexpr double arcWidth() const noexcept { return m_arcWidth; }
    [[nodiscard]] constexpr double arcHeight() const noexcept { return m_arcHeight; }

    /// True when the width or the height is not positive (RoundRectangle2D.Double.isEmpty).
    [[nodiscard]] bool isEmpty() const noexcept;

    /// The framing rectangle (RoundRectangle2D.Double.getBounds2D).
    [[nodiscard]] Rectangle2D bounds() const noexcept;

    /// RoundRectangle2D.contains(double, double): false when empty or outside the frame (whose
    /// right and upper edges are outside); true in the cross between the corners; in a corner,
    /// true when the point is inside or on the corner ellipse (half-axes min(w, |arcw|)/2 and
    /// min(h, |arch|)/2).
    [[nodiscard]] bool contains(Point2D p) const noexcept;

    /// The outline as RoundRectIterator.currentSegment(double[]) gives it: a move to (x, y +
    /// ah/2), then alternately a line and a cubic corner curve (four each) round the frame, and a
    /// close, with aw = min(w, |arcw|) and ah = min(h, |arch|) and every coordinate computed as
    /// x + v0*w + v1*aw (y + v2*h + v3*ah) with Java's constants. Nothing when aw or ah is
    /// negative (a negative width or height).
    [[nodiscard]] std::vector<PathSegment> segments() const;

    /// Equal frames and arcs.
    [[nodiscard]] constexpr bool operator==(const FigureRoundRect&) const noexcept = default;

private:
    double m_x{0.0};
    double m_y{0.0};
    double m_width{0.0};
    double m_height{0.0};
    double m_arcWidth{0.0};
    double m_arcHeight{0.0};
};

}  // namespace QtRocket
