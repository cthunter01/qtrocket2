#pragma once

#include <vector>

#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/util/Geometry2D.h"

namespace QtRocket
{

/// An axis-aligned rectangle of the figure: java.awt.geom.Rectangle2D.Double as OpenRocket's
/// shape providers use it, with its path iterator, bounds and containment test. The corner
/// (x, y) has the smallest coordinates; the extents are kept as given, a negative one included.
class FigureRect
{
public:
    constexpr FigureRect() noexcept = default;

    constexpr FigureRect(double x, double y, double width, double height) noexcept
      : m_x(x), m_y(y), m_width(width), m_height(height)
    {
    }

    [[nodiscard]] constexpr double x() const noexcept { return m_x; }
    [[nodiscard]] constexpr double y() const noexcept { return m_y; }
    [[nodiscard]] constexpr double width() const noexcept { return m_width; }
    [[nodiscard]] constexpr double height() const noexcept { return m_height; }

    /// True when the width or the height is not positive (Rectangle2D.Double.isEmpty).
    [[nodiscard]] bool isEmpty() const noexcept;

    /// The rectangle itself (Rectangle2D.getBounds2D).
    [[nodiscard]] Rectangle2D bounds() const noexcept;

    /// True when x <= p.x < x + width and y <= p.y < y + height: the left and lower edges are
    /// inside, the right and upper ones outside (Rectangle2D.contains(double, double)). Never true
    /// for an empty rectangle or a NaN coordinate.
    [[nodiscard]] bool contains(Point2D p) const noexcept;

    /// The outline as RectIterator.currentSegment(double[]) gives it: a move to (x, y), lines to
    /// (x + w, y), (x + w, y + h), (x, y + h) and back to (x, y), then a close. Nothing when the
    /// width or the height is negative (a zero extent still gives the six segments).
    [[nodiscard]] std::vector<PathSegment> segments() const;

    /// The outline as RectIterator.currentSegment(float[]) gives it, which is what a
    /// Path2D.Float appends: the corners are float sums of the float-rounded x, y, w and h
    /// ((float) x + (float) w, not (float) (x + w)), returned as doubles.
    [[nodiscard]] std::vector<PathSegment> floatSegments() const;

    /// Equal corners and extents.
    [[nodiscard]] constexpr bool operator==(const FigureRect&) const noexcept = default;

private:
    double m_x{0.0};
    double m_y{0.0};
    double m_width{0.0};
    double m_height{0.0};
};

}  // namespace QtRocket
