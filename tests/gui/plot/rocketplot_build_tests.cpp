// rocketplot as QtRocket builds it (cmake/Dependencies.cmake, section GUI): its headers compile
// with QtRocket's warnings and Qt definitions, its library links into the GUI's tests, and a plot
// with a curve and its texts paints without a Qt message (the fixture fails a test on one) on every
// platform CI builds. Texts go through rocketplot::literalText() (CLAUDE.md, section GUI): a '$'
// would start math, of which rocketplot, built without its math font, warns once per process.

#include <vector>

#include <QPixmap>
#include <QString>

#include <gtest/gtest.h>
#include <rocketplot/Axis.h>
#include <rocketplot/LineSeries.h>
#include <rocketplot/PlotWidget.h>
#include <rocketplot/Series.h>
#include <rocketplot/text.h>

#include "GuiTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using RocketplotBuild = Test::GuiTest;

constexpr int kPlotWidth  = 400;
constexpr int kPlotHeight = 300;

/// Adds a thrust curve of four points named @p name to @p plot.
rocketplot::LineSeries* addThrustCurve(rocketplot::PlotWidget& plot, const QString& name)
{
    const std::vector<double> time{0.0, 0.1, 0.5, 1.0};
    const std::vector<double> thrust{0.0, 12.0, 5.0, 0.0};
    return plot.addLine(time, thrust, name);
}

TEST_F(RocketplotBuild, APlotTakesACurveAndItsTexts)
{
    rocketplot::PlotWidget plot;
    plot.setTitle(rocketplot::literalText(QStringLiteral("Thrust curve:")));
    plot.xAxis()->setLabel(rocketplot::literalText(QStringLiteral("Time / s")));
    plot.yAxis()->setLabel(rocketplot::literalText(QStringLiteral("Thrust / N")));
    rocketplot::LineSeries* const curve =
        addThrustCurve(plot, rocketplot::literalText(QStringLiteral("Thrust (0)")));
    ASSERT_NE(curve, nullptr);
    curve->setLineWidth(3.0);

    ASSERT_EQ(plot.series().size(), 1);
    EXPECT_EQ(plot.series().constFirst(), curve);
    // A text that shows as it is written anyway comes back from literalText() unchanged.
    EXPECT_EQ(curve->name(), QStringLiteral("Thrust (0)"));
    EXPECT_EQ(plot.title(), QStringLiteral("Thrust curve:"));
    EXPECT_EQ(plot.xAxis()->label(), QStringLiteral("Time / s"));
    EXPECT_EQ(plot.yAxis()->label(), QStringLiteral("Thrust / N"));

    plot.resize(kPlotWidth, kPlotHeight);
    EXPECT_FALSE(plot.grab().isNull());
}

TEST_F(RocketplotBuild, ATextThroughLiteralTextShowsAsWritten)
{
    const QString text    = QStringLiteral("Thrust ($5) <b> & more");
    const QString literal = rocketplot::literalText(text);
    // Rich text that shows the characters, never markup or math.
    EXPECT_NE(literal, text);

    rocketplot::PlotWidget plot;
    plot.setTitle(literal);
    plot.xAxis()->setLabel(rocketplot::literalText(QStringLiteral("Time / s")));
    plot.yAxis()->setLabel(literal);
    ASSERT_NE(addThrustCurve(plot, literal), nullptr);
    plot.resize(kPlotWidth, kPlotHeight);
    // Painted without a message: no math was typeset.
    EXPECT_FALSE(plot.grab().isNull());
    // CSV has the characters of the text: its header row is the x axis's label and the names.
    EXPECT_TRUE(plot.toCsv().startsWith(QStringLiteral("Time / s,Thrust ($5) <b> & more\n")))
        << plot.toCsv().toStdString();
}

}  // namespace

}  // namespace QtRocket::Gui
