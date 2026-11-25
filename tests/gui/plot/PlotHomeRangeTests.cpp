#include "QtRocket/gui/plot/PlotHomeRange.h"

#include <array>
#include <cmath>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <QPointF>
#include <Qt>

#include <gtest/gtest.h>
#include <rocketplot/Axis.h>
#include <rocketplot/IconAnnotation.h>
#include <rocketplot/LineSeries.h>
#include <rocketplot/PlotWidget.h>
#include <rocketplot/Range.h>

#include "GuiTestSupport.h"
#include "plot/PlotReadback.h"

// The workaround of rocketplot's N16 (handoff/rocketplot-m4-points.md): the ranges of OpenRocket's
// "Vertical motion vs. time" of "A simple model rocket" #1 (the M4 plot scout's probe:
// PlotConfiguration.fitAxes() and the domain), given as home ranges, are what the axes show, bit
// for bit, at first and after every way back to them.

namespace QtRocket::Gui
{

namespace
{

using PlotHomeRangeTest = Gui::Test::GuiTest;
using rocketplot::Range;

constexpr Range kHomeX{.min = -0.5, .max = 16.388};
constexpr Range kHomeY{.min = -5.81486, .max = 52.23286};
constexpr Range kHomeY2{.min = -16.94345, .max = 152.197103931479};
/// A range the scout's transparent rectangle would have missed (the domain of the same rocket's
/// drag coefficients over the Mach number): QRectF keeps x and the width, and x + (max - x) is not
/// max here (nor for kHomeY2).
constexpr Range kAwkwardX{.min = -0.494, .max = 0.586};

/// "[min, max]" of an axis' range, in hexadecimal.
[[nodiscard]] std::string rangeText(const rocketplot::Axis& axis)
{
    return std::format("[{:a}, {:a}]", axis.min(), axis.max());
}

/// Whether @p axis shows @p range exactly.
[[nodiscard]] bool shows(const rocketplot::Axis& axis, Range range)
{
    return axis.min() == range.min && axis.max() == range.max;
}

/// A plot with altitude-like data on the left axis and acceleration-like data on the right one,
/// within the home ranges.
class TwoAxisPlot
{
public:
    TwoAxisPlot()
    {
        m_plot.resize(800, 600);
        std::vector<double> time;
        std::vector<double> altitude;
        std::vector<double> acceleration;
        for (int i = 0; i <= 15; ++i)
        {
            time.push_back(i);
            altitude.push_back((10.0 * i) - (0.6 * i * i));
            acceleration.push_back(i < 2 ? 140.0 : -9.8);
        }
        m_left  = m_plot.addLine(time, altitude);
        m_right = m_plot.addLine(time, acceleration);
        m_right->setYAxis(m_plot.yAxis2());
    }

    [[nodiscard]] rocketplot::PlotWidget& plot() noexcept { return m_plot; }
    [[nodiscard]] rocketplot::LineSeries& left() const noexcept { return *m_left; }
    [[nodiscard]] rocketplot::LineSeries& right() const noexcept { return *m_right; }

private:
    rocketplot::PlotWidget  m_plot;
    rocketplot::LineSeries* m_left  = nullptr;
    rocketplot::LineSeries* m_right = nullptr;
};

/// What is wrong with the corners of the y axis @p secondary or not of @p home: each must be
/// there, without an icon, included in autoscale, on that axis and without a legend entry.
[[nodiscard]] std::vector<std::string> cornerMismatches(const PlotHomeRange& home, bool secondary)
{
    std::vector<std::string> wrong;
    for (const rocketplot::IconAnnotation* const corner : home.corners(secondary))
    {
        if (corner == nullptr)
        {
            wrong.emplace_back("a corner is missing");
        }
        else if (!corner->icon().isNull() || !corner->isIncludedInAutoscale() ||
                 corner->isOnSecondaryYAxis() != secondary || !corner->legendName().isEmpty())
        {
            wrong.emplace_back(std::format("the corner at ({}, {}) is not a blank one of its axis",
                                           corner->position().x(), corner->position().y()));
        }
    }
    return wrong;
}

}  // namespace

TEST_F(PlotHomeRangeTest, TheAxesAutoscaleWithoutMarginsOrZero)
{
    TwoAxisPlot         data;
    const PlotHomeRange home(data.plot());
    for (const rocketplot::Axis* axis :
         {data.plot().xAxis(), data.plot().yAxis(), data.plot().yAxis2()})
    {
        EXPECT_EQ(axis->autoscaleMargin(), 0.0);
        EXPECT_EQ(axis->autoscaleIncludesZero(), 0.0);
    }
    EXPECT_FALSE(data.plot().areYAxesZeroAligned());
    EXPECT_FALSE(home.x().has_value());
    EXPECT_EQ(home.corners(false), (std::array<rocketplot::IconAnnotation*, 2>{}));
}

TEST_F(PlotHomeRangeTest, TheAxesShowTheHomeRangesExactly)
{
    TwoAxisPlot   data;
    PlotHomeRange home(data.plot());
    home.setRanges(kHomeX, kHomeY, kHomeY2);
    const rocketplot::PlotWidget& plot = data.plot();
    EXPECT_TRUE(shows(*plot.xAxis(), kHomeX)) << rangeText(*plot.xAxis());
    EXPECT_TRUE(shows(*plot.yAxis(), kHomeY)) << rangeText(*plot.yAxis());
    EXPECT_TRUE(shows(*plot.yAxis2(), kHomeY2)) << rangeText(*plot.yAxis2());
    EXPECT_EQ(home.x(), kHomeX);
    EXPECT_EQ(home.y(), kHomeY);
    EXPECT_EQ(home.y2(), kHomeY2);
}

TEST_F(PlotHomeRangeTest, AResetComesBackToThem)
{
    TwoAxisPlot   data;
    PlotHomeRange home(data.plot());
    home.setRanges(kHomeX, kHomeY, kHomeY2);
    rocketplot::PlotWidget& plot = data.plot();
    plot.zoom(2.0);
    EXPECT_LT(plot.xAxis()->max(), kHomeX.max);
    EXPECT_LT(plot.yAxis2()->max(), kHomeY2.max);
    plot.resetView();
    EXPECT_TRUE(shows(*plot.xAxis(), kHomeX)) << rangeText(*plot.xAxis());
    EXPECT_TRUE(shows(*plot.yAxis(), kHomeY)) << rangeText(*plot.yAxis());
    EXPECT_TRUE(shows(*plot.yAxis2(), kHomeY2)) << rangeText(*plot.yAxis2());

    plot.zoom(0.5, Qt::Horizontal);
    EXPECT_LT(plot.xAxis()->min(), kHomeX.min);
    EXPECT_TRUE(shows(*plot.yAxis(), kHomeY)) << rangeText(*plot.yAxis());
    plot.resetView();
    EXPECT_TRUE(shows(*plot.xAxis(), kHomeX)) << rangeText(*plot.xAxis());
}

TEST_F(PlotHomeRangeTest, HiddenSeriesKeepThem)
{
    TwoAxisPlot   data;
    PlotHomeRange home(data.plot());
    home.setRanges(kHomeX, kHomeY, kHomeY2);
    data.left().setVisible(false);
    data.right().setVisible(false);
    const rocketplot::PlotWidget& plot = data.plot();
    EXPECT_TRUE(shows(*plot.xAxis(), kHomeX)) << rangeText(*plot.xAxis());
    EXPECT_TRUE(shows(*plot.yAxis(), kHomeY)) << rangeText(*plot.yAxis());
    EXPECT_TRUE(shows(*plot.yAxis2(), kHomeY2)) << rangeText(*plot.yAxis2());
}

TEST_F(PlotHomeRangeTest, NewRangesReplaceTheOldOnesToTheirLastBit)
{
    TwoAxisPlot   data;
    PlotHomeRange home(data.plot());
    home.setRanges(kHomeX, kHomeY, kHomeY2);
    // A wider x, as the event lines widen it, then one that differs in its last bits only.
    const Range wider{.min = kHomeX.min, .max = 16.55188};
    home.setRanges(wider, kHomeY, kHomeY2);
    const rocketplot::PlotWidget& plot = data.plot();
    EXPECT_TRUE(shows(*plot.xAxis(), wider)) << rangeText(*plot.xAxis());
    const Range nextBit{.min = kHomeX.min,
                        .max = std::nextafter(wider.max, std::numeric_limits<double>::infinity())};
    home.setRanges(nextBit, kHomeY, kHomeY2);
    EXPECT_TRUE(shows(*plot.xAxis(), nextBit)) << rangeText(*plot.xAxis());
}

TEST_F(PlotHomeRangeTest, RangesARectangleWouldMissAreShownExactly)
{
    rocketplot::PlotWidget plot;
    plot.resize(800, 600);
    plot.addLine(std::vector<double>{0.006, 0.046, 0.086}, std::vector<double>{0.0, 100.0, 50.0});
    PlotHomeRange home(plot);
    home.setRanges(kAwkwardX, kHomeY2, std::nullopt);
    EXPECT_TRUE(shows(*plot.xAxis(), kAwkwardX)) << rangeText(*plot.xAxis());
    EXPECT_TRUE(shows(*plot.yAxis(), kHomeY2)) << rangeText(*plot.yAxis());
}

TEST_F(PlotHomeRangeTest, TheCornersDrawNothingAndCannotBePointedAt)
{
    TwoAxisPlot   data;
    PlotHomeRange home(data.plot());
    home.setRanges(kHomeX, kHomeY, kHomeY2);
    EXPECT_EQ(cornerMismatches(home, false), std::vector<std::string>());
    EXPECT_EQ(cornerMismatches(home, true), std::vector<std::string>());
    ASSERT_NE(home.corners(false)[0], nullptr);
    ASSERT_NE(home.corners(true)[1], nullptr);
    EXPECT_EQ(home.corners(false)[0]->position(), QPointF(kHomeX.min, kHomeY.min));
    EXPECT_EQ(home.corners(true)[1]->position(), QPointF(kHomeX.max, kHomeY2.max));
    rocketplot::PlotWidget& plot = data.plot();
    static_cast<void>(plot.grab());
    EXPECT_EQ(plot.annotationAt(plot.mapFromData(QPointF(kHomeX.min, kHomeY.min))), nullptr);
    EXPECT_EQ(plot.annotationAt(plot.mapFromData(QPointF(kHomeX.max, kHomeY.max))), nullptr);
    // The same drawing as a plot without them.
    TwoAxisPlot bare;
    EXPECT_EQ(Gui::Test::svgPaths(plot).size(), Gui::Test::svgPaths(bare.plot()).size());
}

TEST_F(PlotHomeRangeTest, AnAxisWithoutARangeHasNoCorners)
{
    TwoAxisPlot   data;
    PlotHomeRange home(data.plot());
    home.setRanges(kHomeX, kHomeY, std::nullopt);
    EXPECT_NE(home.corners(false)[0], nullptr);
    EXPECT_EQ(home.corners(true), (std::array<rocketplot::IconAnnotation*, 2>{}));
    EXPECT_FALSE(home.y2().has_value());
    // A range that is not one (NaN, the minimum above the maximum) is none.
    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    home.setRanges(kHomeX, Range{.min = kNaN, .max = kNaN}, Range{.min = 2.0, .max = 1.0});
    EXPECT_FALSE(home.y().has_value());
    EXPECT_FALSE(home.y2().has_value());
    EXPECT_EQ(home.corners(false), (std::array<rocketplot::IconAnnotation*, 2>{}));
    // Without x, no corners at all.
    home.setRanges(std::nullopt, kHomeY, kHomeY2);
    EXPECT_EQ(home.corners(false), (std::array<rocketplot::IconAnnotation*, 2>{}));
    EXPECT_EQ(home.corners(true), (std::array<rocketplot::IconAnnotation*, 2>{}));
    EXPECT_TRUE(data.plot().annotations().isEmpty());
}

}  // namespace QtRocket::Gui
