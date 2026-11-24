#pragma once

#include <array>
#include <optional>

#include <QPointer>

#include <rocketplot/IconAnnotation.h>
#include <rocketplot/PlotWidget.h>
#include <rocketplot/Range.h>

namespace QtRocket::Gui
{

/// The view a plot comes back to: the ranges OpenRocket's axis fitting computed
/// (PlotConfiguration.fitAxes(), the domain's data range), which OpenRocket's charts show at first
/// and return to on Reset zoom (Plot.PresetNumberAxis: its autoAdjustRange() sets them). The
/// workaround of rocketplot's N16 (handoff/rocketplot-m4-points.md, verified by the M4 plot scout):
/// rocketplot's resetView(), its double-click and its menu's Reset view turn autoscale on, which
/// fits the data by rules of its own (a margin, zero, aligned zeros); there is no home range to
/// give it yet (the proposed Axis::setHomeRange()).
///
/// So the plot's three axes autoscale without a margin, without going to zero and with the y
/// axes' zeros left alone (Axis::setAutoscaleMargin(0), setAutoscaleIncludesZero(0),
/// PlotWidget::setYAxesZeroAligned(false)), and for each y axis with a home range two annotations
/// mark the corners of the home view on that axis, (x min, y min) and (x max, y max), included in
/// autoscale (Annotation::setIncludedInAutoscale()). They are icon annotations without an icon,
/// which draw nothing (on screen, in images and in SVG or PDF exports), take no room, cannot be
/// pointed at and have no legend entry, and whose autoscale extent is their point exactly. The
/// ranges given hold all the data, so autoscale fits exactly these points: the home ranges, bit
/// for bit, at first, after a zoom and a reset, after a zoom of x alone and a reset, and with
/// series hidden. (The scout's probe used a transparent rectangle per axis; a rectangle keeps its
/// width, and its right edge, its left one plus that width, can miss the home range's end by its
/// last bit.) Once rocketplot has a home range, this class sets it instead and the corners go.
///
/// The plot owns the corners; this object only refers to them. One per plot, made after the plot.
class PlotHomeRange
{
public:
    /// Prepares the axes of @p plot as the class comment says, without home ranges yet.
    explicit PlotHomeRange(rocketplot::PlotWidget& plot);

    /// The home ranges: @p x of the x axis, @p y of the y axis and @p y2 of the secondary one
    /// (nullopt for an axis without data: no corners, and autoscale fits it as rocketplot does).
    /// A range that is not valid (rocketplot::Range::isValid(): finite, its minimum at most its
    /// maximum) counts as none; without an x range there are no corners at all. Replaces the
    /// ranges given before; while an axis autoscales (no zoom since the last reset) it shows its
    /// new range at once.
    void setRanges(std::optional<rocketplot::Range> x, std::optional<rocketplot::Range> y,
                   std::optional<rocketplot::Range> y2);

    /// The home ranges as last given, a range that is not valid as none.
    [[nodiscard]] std::optional<rocketplot::Range> x() const noexcept { return m_x; }
    [[nodiscard]] std::optional<rocketplot::Range> y() const noexcept { return m_y; }
    [[nodiscard]] std::optional<rocketplot::Range> y2() const noexcept { return m_y2; }

    /// The corners of the y axis or of the secondary one (both null without a home range), for
    /// the tests: the lower left one, then the upper right one.
    [[nodiscard]] std::array<rocketplot::IconAnnotation*, 2> corners(bool secondary) const noexcept;

private:
    using Corners = std::array<QPointer<rocketplot::IconAnnotation>, 2>;

    /// Places @p corners of the y axis @p secondary or not at the corners of @p x and @p y, or
    /// removes them when either is none.
    void place(Corners& corners, bool secondary, std::optional<rocketplot::Range> x,
               std::optional<rocketplot::Range> y);

    QPointer<rocketplot::PlotWidget> m_plot;
    Corners                          m_corners;
    Corners                          m_secondaryCorners;
    std::optional<rocketplot::Range> m_x;
    std::optional<rocketplot::Range> m_y;
    std::optional<rocketplot::Range> m_y2;
};

}  // namespace QtRocket::Gui
