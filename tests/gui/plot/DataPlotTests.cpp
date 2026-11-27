#include "QtRocket/gui/plot/DataPlot.h"

#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <QColor>
#include <QList>
#include <QPoint>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QWidget>
#include <Qt>
#include <QtTypes>

#include <gtest/gtest.h>
#include <rocketplot/Axis.h>
#include <rocketplot/LineSeries.h>
#include <rocketplot/PlotWidget.h>
#include <rocketplot/Range.h>
#include <rocketplot/enums.h>
#include <rocketplot/text.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/plot/PlotColors.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/LineStyle.h"
#include "TestGuiServices.h"
#include "plot/PlotReadback.h"

// The chart that OpenRocket's data plots share (Plot.java, SimulationChart.java), with a plot of
// its own as Milestone 7's component analysis plot will be one; SimulationPlotTests.cpp tests it
// through the simulation plot. The home ranges (setHomeRanges(), rocketplot's
// Axis::setHomeRange()): the ranges of OpenRocket's "Vertical motion vs. time" of "A simple model
// rocket" #1 (the M4 plot scout's probe: PlotConfiguration.fitAxes() and the domain), given as
// home ranges, are what the axes show, bit for bit, at first and after every way back to them.

namespace QtRocket::Gui
{

namespace
{

using DataPlotTest = Gui::Test::GuiTest;
using rocketplot::Range;

constexpr Range kHomeX{.min = -0.5, .max = 16.388};
constexpr Range kHomeY{.min = -5.81486, .max = 52.23286};
constexpr Range kHomeY2{.min = -16.94345, .max = 152.197103931479};
/// A range whose end is not its start plus its width (the domain of the same rocket's drag
/// coefficients over the Mach number): x + (max - x) is not max here (nor for kHomeY2), so a range
/// kept as a start and a width (a QRectF, as the M4 scout's transparent rectangle) would miss it.
constexpr Range kAwkwardX{.min = -0.494, .max = 0.586};

/// Two lines, one on each axis, whose tool tips name the series and the point.
class TwoLinePlot : public DataPlot
{
public:
    explicit TwoLinePlot(const GuiServices& services) : DataPlot(services, false, nullptr)
    {
        setTitles(QStringLiteral("Two lines"), QStringLiteral("A test"));
        setAxisLabels(QStringLiteral("X (s)"), QStringLiteral("Left"), QStringLiteral("Right"));
        addSeries({0.0, 1.0, 2.0}, {0.0, 10.0, 5.0},
                  {.name        = QStringLiteral("Left line"),
                   .legendGroup = QStringLiteral("left"),
                   .axis        = 0,
                   .color       = PlotColors::plotColor(0),
                   .lineStyle   = LineStyle::DOTTED,
                   .shape       = 0});
        addSeries({0.0, 1.0, 2.0}, {100.0, 50.0, 0.0},
                  {.name        = QStringLiteral("Right line"),
                   .legendGroup = QStringLiteral("right"),
                   .axis        = 1,
                   .color       = PlotColors::plotColor(1),
                   .lineStyle   = LineStyle::SOLID,
                   .shape       = 2});
        setHomeRanges(rocketplot::Range{.min = -0.5, .max = 2.5},
                      rocketplot::Range{.min = -1.0, .max = 11.0},
                      rocketplot::Range{.min = -10.0, .max = 110.0});
        resize(600, 400);
    }

    /// An axis other than 0 and 1.
    void addOnAThirdAxis()
    {
        addSeries({0.0}, {0.0},
                  {.name        = QStringLiteral("Third"),
                   .legendGroup = {},
                   .axis        = 2,
                   .color       = PlotColors::plotColor(2),
                   .lineStyle   = LineStyle::SOLID,
                   .shape       = 0});
    }

protected:
    [[nodiscard]] QString toolTipText(std::size_t series, std::size_t point) const override
    {
        return QStringLiteral("series %1 point %2").arg(series).arg(point);
    }
};

/// A plot with altitude-like data on the left axis and acceleration-like data on the right one,
/// within the home ranges, which a test gives it (setHomeRanges()).
class HomeRangePlot : public DataPlot
{
public:
    explicit HomeRangePlot(const GuiServices& services) : DataPlot(services, false, nullptr)
    {
        std::vector<double> time;
        std::vector<double> altitude;
        std::vector<double> acceleration;
        for (int i = 0; i <= 15; ++i)
        {
            time.push_back(i);
            altitude.push_back((10.0 * i) - (0.6 * i * i));
            acceleration.push_back(i < 2 ? 140.0 : -9.8);
        }
        std::vector<double> sameTime = time;
        addSeries(std::move(time), std::move(altitude),
                  {.name        = QStringLiteral("Altitude"),
                   .legendGroup = {},
                   .axis        = 0,
                   .color       = PlotColors::plotColor(0),
                   .lineStyle   = LineStyle::SOLID,
                   .shape       = 0});
        addSeries(std::move(sameTime), std::move(acceleration),
                  {.name        = QStringLiteral("Acceleration"),
                   .legendGroup = {},
                   .axis        = 1,
                   .color       = PlotColors::plotColor(1),
                   .lineStyle   = LineStyle::SOLID,
                   .shape       = 0});
        resize(800, 600);
    }

    using DataPlot::setHomeRanges;

protected:
    [[nodiscard]] QString toolTipText(std::size_t /*series*/, std::size_t /*point*/) const override
    {
        return {};
    }
};

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

/// What is wrong with the axes of @p plot fitting their data as they are: a margin, the zero rule,
/// a home range; and with its y axes' zeros lined up.
[[nodiscard]] std::vector<std::string> dataFitMismatches(const rocketplot::PlotWidget& plot)
{
    std::vector<std::string> wrong;
    for (const rocketplot::Axis* const axis : {plot.xAxis(), plot.yAxis(), plot.yAxis2()})
    {
        if (axis->autoscaleMargin() != 0.0 || axis->autoscaleIncludesZero() != 0.0 ||
            axis->homeRange().has_value())
        {
            wrong.push_back(std::format("an axis with margin {}, zero rule {}, home range {}",
                                        axis->autoscaleMargin(), axis->autoscaleIncludesZero(),
                                        axis->homeRange().has_value()));
        }
    }
    if (plot.areYAxesZeroAligned())
    {
        wrong.emplace_back("the y axes' zeros are lined up");
    }
    return wrong;
}

/// The outlines of the paths @p plot's SVG export draws, in its order.
[[nodiscard]] std::vector<QString> pathData(const rocketplot::PlotWidget& plot)
{
    std::vector<QString> data;
    for (const Gui::Test::SvgPath& path : Gui::Test::svgPaths(plot))
    {
        data.push_back(path.data);
    }
    return data;
}

/// The texts a readout hook was given.
struct ReadoutTexts
{
    void operator()(const QPoint& /*position*/, const QString& text) const
    {
        texts->push_back(text);
    }

    std::shared_ptr<std::vector<QString>> texts = std::make_shared<std::vector<QString>>();
};

}  // namespace

TEST_F(DataPlotTest, ASubclassGivesTheTitlesTheSeriesAndTheHomeRanges)
{
    const Gui::Test::TestGuiServices services;
    const TwoLinePlot                plot(services.services());
    const Gui::Test::PlotReadback    read = Gui::Test::readPlot(plot.plotWidget());
    EXPECT_EQ(read.title, rocketplot::literalText(QStringLiteral("Two lines")));
    EXPECT_EQ(read.subtitle, rocketplot::literalText(QStringLiteral("A test")));
    EXPECT_EQ(read.xAxis.label, rocketplot::literalText(QStringLiteral("X (s)")));
    EXPECT_EQ(read.yAxis.label, rocketplot::literalText(QStringLiteral("Left")));
    EXPECT_EQ(read.yAxis2.label, rocketplot::literalText(QStringLiteral("Right")));
    EXPECT_EQ(read.xAxis.min, -0.5);
    EXPECT_EQ(read.xAxis.max, 2.5);
    EXPECT_EQ(read.yAxis2.min, -10.0);
    EXPECT_EQ(read.yAxis2.max, 110.0);
    ASSERT_EQ(read.series.size(), 2U);
    EXPECT_EQ(read.series[1].name, rocketplot::literalText(QStringLiteral("Right line")));
    EXPECT_EQ(read.series[1].legendGroup, QStringLiteral("right"));
    EXPECT_EQ(Gui::Test::readSeriesLine(*plot.series(0)).dashPattern, (QList<qreal>{2.0, 3.0}));
    EXPECT_TRUE(Gui::Test::readSeriesLine(*plot.series(1)).secondaryYAxis);
    EXPECT_EQ(plot.plotWidget().xAxis()->homeRange(), (Range{.min = -0.5, .max = 2.5}));
    EXPECT_EQ(plot.plotWidget().yAxis()->homeRange(), (Range{.min = -1.0, .max = 11.0}));
    EXPECT_EQ(plot.plotWidget().yAxis2()->homeRange(), (Range{.min = -10.0, .max = 110.0}));
    EXPECT_EQ(plot.seriesCount(), 2U);
}

TEST_F(DataPlotTest, ASeriesHasTheShapeItIsGiven)
{
    const Gui::Test::TestGuiServices services;
    TwoLinePlot                      plot(services.services());
    plot.setShowPoints(true);
    EXPECT_EQ(plot.series(0)->marker(), rocketplot::Marker::SQUARE);
    EXPECT_EQ(plot.series(1)->marker(), rocketplot::Marker::TRIANGLE);
}

TEST_F(DataPlotTest, ASeriesRemovedThroughTheLegendIsNoLongerThere)
{
    const Gui::Test::TestGuiServices services;
    TwoLinePlot                      plot(services.services());
    // What the legend entry's Remove does.
    plot.plotWidget().removeSeries(plot.series(0));
    EXPECT_EQ(plot.series(0), nullptr);
    EXPECT_NE(plot.series(1), nullptr);
    EXPECT_EQ(plot.seriesCount(), 2U);
    plot.setShowPoints(true);
    EXPECT_EQ(plot.series(1)->marker(), rocketplot::Marker::TRIANGLE);
    // The readout finds the series left by its index.
    const ReadoutTexts readout;
    plot.setReadoutHook(readout);
    static_cast<void>(Gui::Test::moveCrosshairToPoint(plot.plotWidget(), 0, 1));
    EXPECT_EQ(plot.readout(), QStringLiteral("series 1 point 1"));
}

TEST_F(DataPlotTest, TheReadoutIsTheSubclassesToolTip)
{
    const Gui::Test::TestGuiServices services;
    TwoLinePlot                      plot(services.services());
    const ReadoutTexts               readout;
    plot.setReadoutHook(readout);
    EXPECT_TRUE(plot.readout().isEmpty());
    static_cast<void>(Gui::Test::moveCrosshairToPoint(plot.plotWidget(), 1, 2));
    EXPECT_EQ(plot.readout(), QStringLiteral("series 1 point 2"));
    ASSERT_EQ(readout.texts->size(), 1U);
    EXPECT_EQ(readout.texts->front(), QStringLiteral("series 1 point 2"));
}

TEST_F(DataPlotTest, ItsPreferredSizeIsChartPanels)
{
    // ChartPanel.DEFAULT_WIDTH x DEFAULT_HEIGHT, SimulationChart's preferred size ("preferred=
    // 1024x768", tools/openrocket-gui-pins' plot-dialog probe), whatever size it has.
    const Gui::Test::TestGuiServices services;
    const TwoLinePlot                plot(services.services());
    EXPECT_EQ(plot.sizeHint(), QSize(1024, 768));
    EXPECT_EQ(plot.sizeHint(), QSize(DataPlot::kPreferredWidth, DataPlot::kPreferredHeight));
    EXPECT_EQ(plot.size(), QSize(600, 400));
}

TEST_F(DataPlotTest, APlotWithoutEventsDrawsNoneWhenShowEventsIsChanged)
{
    // Plot.setShowEvents(): "Default implementation does nothing".
    const Gui::Test::TestGuiServices services;
    TwoLinePlot                      plot(services.services());
    const qsizetype                  annotations = plot.plotWidget().annotations().size();
    plot.setShowEvents(true);
    EXPECT_EQ(plot.plotWidget().annotations().size(), annotations);
    EXPECT_TRUE(Gui::Test::readAnnotations(plot.plotWidget()).events.empty());
    plot.setShowEvents(false);
    EXPECT_EQ(plot.plotWidget().annotations().size(), annotations);
}

TEST_F(DataPlotTest, ASeriesOnAThirdAxisIsABug)
{
    const Gui::Test::TestGuiServices services;
    TwoLinePlot                      plot(services.services());
    EXPECT_THROW(plot.addOnAThirdAxis(), BugError);
    EXPECT_THROW(static_cast<void>(plot.series(2)), BugError);
}

// ----------------------------------------------------------------------------------- home ranges

TEST_F(DataPlotTest, WithoutHomeRangesTheAxesFitTheDataWithoutMarginsOrZero)
{
    const Gui::Test::TestGuiServices services;
    const HomeRangePlot              data(services.services());
    EXPECT_EQ(dataFitMismatches(data.plotWidget()), std::vector<std::string>());
    // The times as they are, 0 to 15 s.
    EXPECT_EQ(data.plotWidget().xAxis()->min(), 0.0);
    EXPECT_EQ(data.plotWidget().xAxis()->max(), 15.0);
}

TEST_F(DataPlotTest, TheAxesShowTheHomeRangesExactly)
{
    const Gui::Test::TestGuiServices services;
    HomeRangePlot                    data(services.services());
    data.setHomeRanges(kHomeX, kHomeY, kHomeY2);
    const rocketplot::PlotWidget& plot = data.plotWidget();
    EXPECT_TRUE(shows(*plot.xAxis(), kHomeX)) << rangeText(*plot.xAxis());
    EXPECT_TRUE(shows(*plot.yAxis(), kHomeY)) << rangeText(*plot.yAxis());
    EXPECT_TRUE(shows(*plot.yAxis2(), kHomeY2)) << rangeText(*plot.yAxis2());
    EXPECT_EQ(plot.xAxis()->homeRange(), kHomeX);
    EXPECT_EQ(plot.yAxis()->homeRange(), kHomeY);
    EXPECT_EQ(plot.yAxis2()->homeRange(), kHomeY2);
}

TEST_F(DataPlotTest, AResetComesBackToTheHomeRanges)
{
    const Gui::Test::TestGuiServices services;
    HomeRangePlot                    data(services.services());
    data.setHomeRanges(kHomeX, kHomeY, kHomeY2);
    rocketplot::PlotWidget& plot = data.plotWidget();
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

    // The plot dialog's buttons (ChartPanel's ZOOM_IN_BOTH, ZOOM_OUT_DOMAIN, ZOOM_RESET_BOTH).
    data.zoomIn(false);
    data.zoomOut(true);
    data.resetZoom();
    EXPECT_TRUE(shows(*plot.xAxis(), kHomeX)) << rangeText(*plot.xAxis());
    EXPECT_TRUE(shows(*plot.yAxis(), kHomeY)) << rangeText(*plot.yAxis());
    EXPECT_TRUE(shows(*plot.yAxis2(), kHomeY2)) << rangeText(*plot.yAxis2());
}

TEST_F(DataPlotTest, HiddenSeriesKeepTheHomeRanges)
{
    const Gui::Test::TestGuiServices services;
    HomeRangePlot                    data(services.services());
    data.setHomeRanges(kHomeX, kHomeY, kHomeY2);
    data.series(0)->setVisible(false);
    data.series(1)->setVisible(false);
    const rocketplot::PlotWidget& plot = data.plotWidget();
    EXPECT_TRUE(shows(*plot.xAxis(), kHomeX)) << rangeText(*plot.xAxis());
    EXPECT_TRUE(shows(*plot.yAxis(), kHomeY)) << rangeText(*plot.yAxis());
    EXPECT_TRUE(shows(*plot.yAxis2(), kHomeY2)) << rangeText(*plot.yAxis2());
}

TEST_F(DataPlotTest, NewHomeRangesReplaceTheOldOnesToTheirLastBit)
{
    const Gui::Test::TestGuiServices services;
    HomeRangePlot                    data(services.services());
    data.setHomeRanges(kHomeX, kHomeY, kHomeY2);
    // A wider x, as the event lines widen it, then one that differs in its last bits only.
    const Range wider{.min = kHomeX.min, .max = 16.55188};
    data.setHomeRanges(wider, kHomeY, kHomeY2);
    const rocketplot::PlotWidget& plot = data.plotWidget();
    EXPECT_TRUE(shows(*plot.xAxis(), wider)) << rangeText(*plot.xAxis());
    const Range nextBit{.min = kHomeX.min,
                        .max = std::nextafter(wider.max, std::numeric_limits<double>::infinity())};
    data.setHomeRanges(nextBit, kHomeY, kHomeY2);
    EXPECT_TRUE(shows(*plot.xAxis(), nextBit)) << rangeText(*plot.xAxis());
}

TEST_F(DataPlotTest, AwkwardHomeRangesAreShownExactly)
{
    const Gui::Test::TestGuiServices services;
    HomeRangePlot                    data(services.services());
    data.setHomeRanges(kAwkwardX, kHomeY2, std::nullopt);
    const rocketplot::PlotWidget& plot = data.plotWidget();
    EXPECT_TRUE(shows(*plot.xAxis(), kAwkwardX)) << rangeText(*plot.xAxis());
    EXPECT_TRUE(shows(*plot.yAxis(), kHomeY2)) << rangeText(*plot.yAxis());
}

TEST_F(DataPlotTest, TheHomeRangesAddNothingToThePlot)
{
    const Gui::Test::TestGuiServices services;
    HomeRangePlot                    data(services.services());
    const rocketplot::PlotWidget&    plot        = data.plotWidget();
    const qsizetype                  annotations = plot.annotations().size();
    data.setHomeRanges(kHomeX, kHomeY, kHomeY2);
    EXPECT_EQ(plot.annotations().size(), annotations);
    // The same drawing as a plot given those ranges as plain ones (Axis::setRange()).
    const HomeRangePlot plain(services.services());
    plain.plotWidget().xAxis()->setRange(kHomeX);
    plain.plotWidget().yAxis()->setRange(kHomeY);
    plain.plotWidget().yAxis2()->setRange(kHomeY2);
    EXPECT_EQ(pathData(plot), pathData(plain.plotWidget()));
}

TEST_F(DataPlotTest, AnAxisWithoutAHomeRangeFitsItsData)
{
    const Gui::Test::TestGuiServices services;
    HomeRangePlot                    data(services.services());
    data.setHomeRanges(kHomeX, kHomeY, std::nullopt);
    const rocketplot::PlotWidget& plot = data.plotWidget();
    EXPECT_FALSE(plot.yAxis2()->homeRange().has_value());
    // The accelerations as they are, -9.8 to 140.
    EXPECT_EQ(plot.yAxis2()->min(), -9.8);
    EXPECT_EQ(plot.yAxis2()->max(), 140.0);
    // A range that is not one (NaN, the minimum above the maximum) is none, without a warning.
    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    data.setHomeRanges(kHomeX, Range{.min = kNaN, .max = kNaN}, Range{.min = 2.0, .max = 1.0});
    EXPECT_FALSE(plot.yAxis()->homeRange().has_value());
    EXPECT_FALSE(plot.yAxis2()->homeRange().has_value());
    EXPECT_TRUE(shows(*plot.xAxis(), kHomeX)) << rangeText(*plot.xAxis());
    // Each axis has its own: without x, the y axes keep theirs.
    data.setHomeRanges(std::nullopt, kHomeY, kHomeY2);
    EXPECT_FALSE(plot.xAxis()->homeRange().has_value());
    EXPECT_EQ(plot.xAxis()->max(), 15.0);
    EXPECT_TRUE(shows(*plot.yAxis2(), kHomeY2)) << rangeText(*plot.yAxis2());
}

// rocketplot ignores a range too narrow to tell its ends apart, beside the magnitude of its ends,
// with a warning (which fails the test): such a range is none, and the axis fits its data.
TEST_F(DataPlotTest, AHomeRangeTooNarrowToShowIsNone)
{
    const Gui::Test::TestGuiServices services;
    HomeRangePlot                    data(services.services());
    const rocketplot::PlotWidget&    plot = data.plotWidget();
    data.setHomeRanges(kHomeX, Range{.min = 1e14 - 1.0625, .max = 1e14 + 1.0625},
                       Range{.min = 5.0, .max = 5.0});
    EXPECT_TRUE(plot.xAxis()->homeRange().has_value());
    EXPECT_FALSE(plot.yAxis()->homeRange().has_value());
    EXPECT_FALSE(plot.yAxis2()->homeRange().has_value());
    EXPECT_EQ(plot.yAxis2()->max(), 140.0);
    // One wider beside its magnitude is one; one wider than rocketplot's widest span is none.
    data.setHomeRanges(kHomeX, Range{.min = 1e12 - 1.0625, .max = 1e12 + 1.0625},
                       Range{.min = -1e300, .max = 1e300});
    EXPECT_EQ(plot.yAxis()->homeRange(), (Range{.min = 1e12 - 1.0625, .max = 1e12 + 1.0625}));
    EXPECT_FALSE(plot.yAxis2()->homeRange().has_value());
}

}  // namespace QtRocket::Gui
