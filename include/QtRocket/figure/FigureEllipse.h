#pragma once

#include <vector>

#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/util/Geometry2D.h"

namespace QtRocket
{

/// An ellipse of the figure, given by its framing rectangle: java.awt.geom.Ellipse2D.Double as
/// OpenRocket's shape providers use it, with its path iterator, bounds and containment test.
class FigureEllipse
{
public:
    /// The control value of Java's four-curve approximation of a quarter circle
    /// (EllipseIterator.CtrlVal, ArcIterator.btan(PI / 2)). Qt's QPainterPath::addEllipse uses
    /// the shorter 0.5522847498, so a painter that wants Java's outline draws segments().
    static constexpr double kControlValue = 0.5522847498307933;

    constexpr FigureEllipse() noexcept = default;

    constexpr FigureEllipse(double x, double y, double width, double height) noexcept
      : m_x(x), m_y(y), m_width(width), m_height(height)
    {
    }

    [[nodiscard]] constexpr double x() const noexcept { return m_x; }
    [[nodiscard]] constexpr double y() const noexcept { return m_y; }
    [[nodiscard]] constexpr double width() const noexcept { return m_width; }
    [[nodiscard]] constexpr double height() const noexcept { return m_height; }

    /// True when the width or the height is not positive (Ellipse2D.Double.isEmpty).
    [[nodiscard]] bool isEmpty() const noexcept;

    /// The framing rectangle (Ellipse2D.Double.getBounds2D).
    [[nodiscard]] Rectangle2D bounds() const noexcept;

    /// True when @p p lies strictly inside the ellipse, tested on the coordinates normalised to
    /// the unit frame as Ellipse2D.contains(double, double) does; never true for a non-positive
    /// width or height or a NaN coordinate.
    [[nodiscard]] bool contains(Point2D p) const noexcept;

    /// The outline as EllipseIterator.currentSegment(double[]) gives it: a move to (x + w,
    /// y + h/2), four cubic curves to (x + w/2, y + h), (x, y + h/2), (x + w/2, y) and back to
    /// (x + w, y + h/2), and a close; every coordinate is x + c*w or y + c*h with Java's
    /// constants c (0, 1/2, 1 and 1/2 +- kControlValue/2). Nothing when the width or the height
    /// is negative.
    [[nodiscard]] std::vector<PathSegment> segments() const;

    /// Equal frames.
    [[nodiscard]] constexpr bool operator==(const FigureEllipse&) const noexcept = default;

private:
    double m_x{0.0};
    double m_y{0.0};
    double m_width{0.0};
    double m_height{0.0};
};

}  // namespace QtRocket
