#include "QtRocket/gui/plot/PlotHomeRange.h"

#include <array>
#include <cstddef>
#include <optional>

#include <QIcon>
#include <QPointF>
#include <QPointer>

#include <rocketplot/Axis.h>
#include <rocketplot/IconAnnotation.h>
#include <rocketplot/PlotWidget.h>
#include <rocketplot/Range.h>

namespace QtRocket::Gui
{

namespace
{

/// @p range if it is a range an axis can be given, else nullopt.
[[nodiscard]] std::optional<rocketplot::Range> validRange(std::optional<rocketplot::Range> range)
{
    if (range.has_value() && range->isValid())
    {
        return range;
    }
    return std::nullopt;
}

}  // namespace

PlotHomeRange::PlotHomeRange(rocketplot::PlotWidget& plot) : m_plot(&plot)
{
    for (rocketplot::Axis* const axis : {plot.xAxis(), plot.yAxis(), plot.yAxis2()})
    {
        axis->setAutoscaleMargin(0.0);
        axis->setAutoscaleIncludesZero(0.0);
    }
    plot.setYAxesZeroAligned(false);
}

void PlotHomeRange::setRanges(std::optional<rocketplot::Range> x,
                              std::optional<rocketplot::Range> y,
                              std::optional<rocketplot::Range> y2)
{
    m_x  = validRange(x);
    m_y  = validRange(y);
    m_y2 = validRange(y2);
    place(m_corners, false, m_x, m_y);
    place(m_secondaryCorners, true, m_x, m_y2);
}

std::array<rocketplot::IconAnnotation*, 2> PlotHomeRange::corners(bool secondary) const noexcept
{
    const Corners& corners = secondary ? m_secondaryCorners : m_corners;
    return {corners[0].data(), corners[1].data()};
}

void PlotHomeRange::place(Corners& corners, bool secondary, std::optional<rocketplot::Range> x,
                          std::optional<rocketplot::Range> y)
{
    rocketplot::PlotWidget* const plot = m_plot.data();
    if (plot == nullptr)
    {
        return;
    }
    // Made anew each time: IconAnnotation::setPosition() takes a point that QPointF's fuzzy
    // comparison calls equal to the old one for the old one, which would keep a home range that
    // differs from the new one in its last bits.
    for (QPointer<rocketplot::IconAnnotation>& corner : corners)
    {
        if (!corner.isNull())
        {
            plot->removeAnnotation(corner.data());
        }
        corner = nullptr;
    }
    if (!x.has_value() || !y.has_value())
    {
        return;
    }
    const std::array<QPointF, 2> points{QPointF(x->min, y->min), QPointF(x->max, y->max)};
    for (std::size_t i = 0; i < points.size(); ++i)
    {
        // A null icon: it draws nothing and cannot be pointed at.
        rocketplot::IconAnnotation* const corner =
            plot->addIcon(points.at(i).x(), points.at(i).y(), QIcon());
        corner->setYAxis(secondary ? plot->yAxis2() : plot->yAxis());
        corner->setIncludedInAutoscale(true);
        corners.at(i) = corner;
    }
}

}  // namespace QtRocket::Gui
