#include "QtRocket/gui/plot/DataPlot.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include <QColor>
#include <QList>
#include <QPoint>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QWidget>
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
#include "QtRocket/gui/plot/PlotHomeRange.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/LineStyle.h"
#include "TestGuiServices.h"
#include "plot/PlotReadback.h"

// The chart that OpenRocket's data plots share (Plot.java, SimulationChart.java), with a plot of
// its own as Milestone 7's component analysis plot will be one; SimulationPlotTests.cpp tests it
// through the simulation plot.

namespace QtRocket::Gui
{

namespace
{

using DataPlotTest = Gui::Test::GuiTest;

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

    [[nodiscard]] const PlotHomeRange& home() const noexcept { return homeRange(); }

protected:
    [[nodiscard]] QString toolTipText(std::size_t series, std::size_t point) const override
    {
        return QStringLiteral("series %1 point %2").arg(series).arg(point);
    }
};

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
    EXPECT_EQ(plot.home().x(), (rocketplot::Range{.min = -0.5, .max = 2.5}));
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

}  // namespace QtRocket::Gui
