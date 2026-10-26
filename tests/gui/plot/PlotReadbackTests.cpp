#include "plot/PlotReadback.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include <QColor>
#include <QLocale>
#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include <gtest/gtest.h>
#include <rocketplot/Axis.h>
#include <rocketplot/Legend.h>
#include <rocketplot/LineSeries.h>
#include <rocketplot/PlotWidget.h>
#include <rocketplot/Range.h>
#include <rocketplot/Series.h>
#include <rocketplot/enums.h>
#include <rocketplot/text.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/plot/PlotColors.h"
#include "QtRocket/gui/util/QtStrings.h"

// The tests of the plot read-back (tests/gui/plot/PlotReadback.h), the test support of the plots:
// what it reads of plots made for the purpose. Tick labels are checked for what they must be (the
// numbers of the axis, in order), never for the numbers rocketplot picks.

namespace QtRocket::Gui
{

namespace
{

using PlotReadbackTest = Gui::Test::GuiTest;

constexpr int    kPlotWidth    = 600;
constexpr int    kPlotHeight   = 400;
constexpr double kSelectedSize = 6.0;
constexpr double kSelectedLine = 1.5;

/// Adds a thrust curve named @p name (given as it is, through literalText()) from (0, 0) to
/// (@p duration, 0) with its peak @p peak to @p plot.
rocketplot::LineSeries* addCurve(rocketplot::PlotWidget& plot, const QString& name, double duration,
                                 double peak)
{
    const std::vector<double> time{0.0, 0.1 * duration, 0.5 * duration, duration};
    const std::vector<double> thrust{0.0, peak, 0.4 * peak, 0.0};
    return plot.addLine(time, thrust, name.isEmpty() ? QString() : rocketplot::literalText(name));
}

/// A plot as the thrust-curve dialog shows one: a title, axis labels, two named curves, the
/// legend under the plot area.
void makeDialogPlot(rocketplot::PlotWidget& plot)
{
    plot.setTitle(rocketplot::literalText(QStringLiteral("Motor thrust curves")));
    plot.xAxis()->setLabel(rocketplot::literalText(QStringLiteral("Time / s")));
    plot.yAxis()->setLabel(rocketplot::literalText(QStringLiteral("Thrust / N")));
    static_cast<void>(addCurve(plot, QStringLiteral("Estes C6"), 2.0, 14.0));
    static_cast<void>(addCurve(plot, QStringLiteral("Estes C6 (1)"), 1.8, 12.0));
    plot.legend()->setVisible(true);
    plot.legend()->setAnchor(rocketplot::LegendAnchor::OUTSIDE_BOTTOM);
    plot.resize(kPlotWidth, kPlotHeight);
}

/// Whether @p labels are at least two numbers (in the test's locale, as the axis writes them),
/// increasing, within @p range.
[[nodiscard]] bool areAxisNumbers(const QStringList& labels, rocketplot::Range range)
{
    if (labels.size() < 2)
    {
        return false;
    }
    const QLocale         locale;
    std::optional<double> previous;
    for (const QString& label : labels)
    {
        bool         ok    = false;
        const double value = locale.toDouble(label, &ok);
        if (!ok || value < range.min || value > range.max ||
            (previous.has_value() && value <= *previous))
        {
            return false;
        }
        previous = value;
    }
    return true;
}

/// @p labels joined for a failure message.
[[nodiscard]] std::string joined(const QStringList& labels)
{
    return QtStrings::toUtf8(labels.join(QStringLiteral(" | ")));
}

/// The text @p text of @p texts; one without characters when there is none.
[[nodiscard]] Gui::Test::SvgText textNamed(const std::vector<Gui::Test::SvgText>& texts,
                                           const QString&                         text)
{
    const auto found = std::ranges::find(texts, text, &Gui::Test::SvgText::text);
    return found != texts.end() ? *found : Gui::Test::SvgText{};
}

TEST_F(PlotReadbackTest, ReadsASeriesAsThePlotShowsIt)
{
    rocketplot::PlotWidget        plot;
    rocketplot::LineSeries* const curve = addCurve(plot, QStringLiteral("Estes C6"), 2.0, 14.0);
    curve->setColor(PlotColors::plotColor(1));
    curve->setLineWidth(kSelectedLine);
    curve->setMarker(rocketplot::Marker::SQUARE);
    curve->setMarkerSize(kSelectedSize);
    curve->setMarkerRingWidth(0.0);
    curve->setMarkersAlwaysVisible(true);

    const Gui::Test::SeriesReadback expected{.name                 = QStringLiteral("Estes C6"),
                                             .legendGroup          = QString(),
                                             .visible              = true,
                                             .color                = PlotColors::plotColor(1),
                                             .marker               = rocketplot::Marker::SQUARE,
                                             .markerSize           = kSelectedSize,
                                             .markerRingWidth      = 0.0,
                                             .lineWidth            = kSelectedLine,
                                             .markersAlwaysVisible = true,
                                             .x                    = {0.0, 0.2, 1.0, 2.0},
                                             .y                    = {0.0, 14.0, 0.4 * 14.0, 0.0}};
    EXPECT_EQ(Gui::Test::readSeries(*curve), expected);
}

TEST_F(PlotReadbackTest, ReadsThePlotItsAxesAndItsSeriesInOrder)
{
    rocketplot::PlotWidget plot;
    makeDialogPlot(plot);
    static_cast<void>(addCurve(plot, QString(), 1.0, 1.0));
    plot.xAxis()->setAutoscaleMargin(0.05);
    plot.yAxis()->setRange(0.0, 20.0);

    const Gui::Test::PlotReadback read = Gui::Test::readPlot(plot);
    EXPECT_EQ(read.title, QStringLiteral("Motor thrust curves"));
    EXPECT_EQ(read.xAxis.label, QStringLiteral("Time / s"));
    EXPECT_TRUE(read.xAxis.autoscale);
    EXPECT_EQ(read.xAxis.autoscaleMargin, 0.05);
    EXPECT_EQ(read.xAxis.min, plot.xAxis()->min());
    EXPECT_EQ(read.xAxis.max, plot.xAxis()->max());
    EXPECT_EQ(read.yAxis.label, QStringLiteral("Thrust / N"));
    EXPECT_FALSE(read.yAxis.autoscale);
    EXPECT_EQ(read.yAxis.min, 0.0);
    EXPECT_EQ(read.yAxis.max, 20.0);
    EXPECT_TRUE(read.yAxis.tickLabelsVisible);
    ASSERT_EQ(read.series.size(), 3U);
    EXPECT_EQ(read.series.at(1).name, QStringLiteral("Estes C6 (1)"));
    // The order added, an unnamed series too.
    EXPECT_EQ(Gui::Test::seriesNames(plot),
              (QStringList{QStringLiteral("Estes C6"), QStringLiteral("Estes C6 (1)"), QString()}));
}

TEST_F(PlotReadbackTest, ReadsTheLegendsEntriesAndTheTickLabelsFromTheSvgExport)
{
    rocketplot::PlotWidget plot;
    makeDialogPlot(plot);
    const Gui::Test::PlotTexts texts = Gui::Test::readPlotTexts(plot);
    EXPECT_EQ(texts.legendEntries,
              (QStringList{QStringLiteral("Estes C6"), QStringLiteral("Estes C6 (1)")}));
    EXPECT_TRUE(areAxisNumbers(texts.xTickLabels, plot.xAxis()->range()))
        << joined(texts.xTickLabels);
    EXPECT_TRUE(areAxisNumbers(texts.yTickLabels, plot.yAxis()->range()))
        << joined(texts.yTickLabels);
    // Every text is in all, the title and the axis labels in none of the others.
    EXPECT_EQ(texts.all.size(),
              texts.legendEntries.size() + texts.xTickLabels.size() + texts.yTickLabels.size() + 3);
    EXPECT_TRUE(texts.all.contains(QStringLiteral("Motor thrust curves")));
    EXPECT_TRUE(texts.all.contains(QStringLiteral("Time / s")));
    EXPECT_TRUE(texts.all.contains(QStringLiteral("Thrust / N")));
}

TEST_F(PlotReadbackTest, ReadsALegendInsideThePlotArea)
{
    rocketplot::PlotWidget plot;
    makeDialogPlot(plot);
    plot.legend()->setAnchor(rocketplot::LegendAnchor::TOP_RIGHT);
    const Gui::Test::PlotTexts texts = Gui::Test::readPlotTexts(plot);
    EXPECT_EQ(texts.legendEntries,
              (QStringList{QStringLiteral("Estes C6"), QStringLiteral("Estes C6 (1)")}));
    EXPECT_TRUE(areAxisNumbers(texts.xTickLabels, plot.xAxis()->range()))
        << joined(texts.xTickLabels);
    EXPECT_TRUE(areAxisNumbers(texts.yTickLabels, plot.yAxis()->range()))
        << joined(texts.yTickLabels);
}

TEST_F(PlotReadbackTest, AnAxisWithoutTickLabelsHasNone)
{
    rocketplot::PlotWidget plot;
    makeDialogPlot(plot);
    plot.xAxis()->setTickLabelsVisible(false);
    plot.yAxis()->setTickLabelsVisible(false);
    const Gui::Test::PlotTexts texts = Gui::Test::readPlotTexts(plot);
    EXPECT_TRUE(texts.xTickLabels.isEmpty()) << joined(texts.xTickLabels);
    EXPECT_TRUE(texts.yTickLabels.isEmpty()) << joined(texts.yTickLabels);
    EXPECT_EQ(texts.legendEntries.size(), 2);
    EXPECT_EQ(texts.all.size(), 5);
}

TEST_F(PlotReadbackTest, ANameWithMarkupCharactersReadsAsWritten)
{
    const QString          name = QStringLiteral("Thrust <b> & $5");
    rocketplot::PlotWidget plot;
    static_cast<void>(addCurve(plot, name, 1.0, 10.0));
    plot.legend()->setVisible(true);
    plot.resize(kPlotWidth, kPlotHeight);
    EXPECT_EQ(Gui::Test::seriesNames(plot), QStringList{rocketplot::literalText(name)});
    EXPECT_EQ(Gui::Test::readPlotTexts(plot).legendEntries, QStringList{name});
}

TEST_F(PlotReadbackTest, TheYAxisLabelIsRotatedAndTheTitleIsNot)
{
    rocketplot::PlotWidget plot;
    makeDialogPlot(plot);
    const std::vector<Gui::Test::SvgText> texts = Gui::Test::svgTexts(plot);
    const Gui::Test::SvgText              label = textNamed(texts, QStringLiteral("Thrust / N"));
    const Gui::Test::SvgText title = textNamed(texts, QStringLiteral("Motor thrust curves"));
    ASSERT_EQ(label.text, QStringLiteral("Thrust / N"));
    ASSERT_EQ(title.text, QStringLiteral("Motor thrust curves"));
    EXPECT_TRUE(label.rotated);
    EXPECT_FALSE(title.rotated);
    // The title starts above the plot area, the y axis' label left of it.
    EXPECT_LT(title.position.y(), plot.plotArea().top());
    EXPECT_LT(label.position.x(), plot.plotArea().left());
}

}  // namespace

}  // namespace QtRocket::Gui
