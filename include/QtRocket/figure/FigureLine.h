#pragma once

#include <vector>

#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/util/Geometry2D.h"

namespace QtRocket
{

/// A straight line segment: java.awt.geom.Line2D.Double as OpenRocket's shape providers use it,
/// with its path iterator and bounds. A line encloses no area, so contains() is always false
/// (Line2D.contains), and a hit test never finds one.
class FigureLine
{
public:
    constexpr FigureLine() noexcept = default;

    constexpr FigureLine(double x1, double y1, double x2, double y2) noexcept
      : m_x1(x1), m_y1(y1), m_x2(x2), m_y2(y2)
    {
    }

    [[nodiscard]] constexpr double x1() const noexcept { return m_x1; }
    [[nodiscard]] constexpr double y1() const noexcept { return m_y1; }
    [[nodiscard]] constexpr double x2() const noexcept { return m_x2; }
    [[nodiscard]] constexpr double y2() const noexcept { return m_y2; }

    /// Line2D.Double.getBounds2D: the corner with the smaller coordinates (the second end's when
    /// they are not smaller, NaN included) and the absolute differences.
    [[nodiscard]] Rectangle2D bounds() const noexcept;

    /// Always false: a line has no interior (Line2D.contains(double, double)).
    [[nodiscard]] static constexpr bool contains(Point2D /*p*/) noexcept { return false; }

    /// The outline as LineIterator.currentSegment(double[]) gives it: a move to the first end
    /// and a line to the second.
    [[nodiscard]] std::vector<PathSegment> segments() const;

    /// Equal end points.
    [[nodiscard]] constexpr bool operator==(const FigureLine&) const noexcept = default;

private:
    double m_x1{0.0};
    double m_y1{0.0};
    double m_x2{0.0};
    double m_y2{0.0};
};

}  // namespace QtRocket
