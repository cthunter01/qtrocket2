// The wind editor's profile (OpenRocket's WindProfilePanel and WindProfileVisualization) on
// rocketplot, read back without pixels: its series, axes and annotations (PlotReadback, the
// getters of rocketplot), the tool tips of the levels, which are the TIP lines of the probe
// tools/openrocket-probes/wind-editor (WindEditorPins.h: Java's formatTooltipText()), and its
// arrows as they are painted (RecordingPaintDevice).

#include "QtRocket/gui/plot/WindProfilePlot.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <numbers>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QCheckBox>
#include <QColor>
#include <QIcon>
#include <QLineF>
#include <QList>
#include <QObject>
#include <QPainter>
#include <QPointF>
#include <QPolygonF>
#include <QRect>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <Qt>

#include <gtest/gtest.h>
#include <rocketplot/Annotation.h>
#include <rocketplot/IconAnnotation.h>
#include <rocketplot/PlotWidget.h>
#include <rocketplot/ScatterSeries.h>
#include <rocketplot/Series.h>
#include <rocketplot/enums.h>
#include <rocketplot/text.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/WindModel.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "dialogs/simulation/WindEditorPins.h"
#include "dialogs/simulation/WindEditorTranscript.h"
#include "figure/RecordingPaintDevice.h"
#include "plot/PlotReadback.h"

namespace QtRocket::Gui
{

namespace
{

using WindProfilePlotTest = Gui::Test::GuiTest;
using Level               = WindProfilePlot::Level;
namespace Pins            = Gui::Test::WindEditorPins;

/// The levels 0, 500 and 1250.5 m of the probe.
void threeLevels(MultiLevelPinkNoiseWindModel& model)
{
    model.clearLevels();
    EXPECT_TRUE(model.addWindLevel(0, 3, std::numbers::pi / 2, 0.3).has_value());
    EXPECT_TRUE(model.addWindLevel(500, 7.5, std::numbers::pi, 1.0).has_value());
    EXPECT_TRUE(model.addWindLevel(1250.5, 12.25, 0.0, 3.0).has_value());
}

/// The profile of three levels.
class Profile
{
public:
    Profile() { threeLevels(m_model); }

    [[nodiscard]] MultiLevelPinkNoiseWindModel& model() noexcept { return m_model; }
    [[nodiscard]] WindProfilePlot&              plot() noexcept { return m_plot; }

    /// The plot's icons, in the order added.
    [[nodiscard]] std::vector<rocketplot::IconAnnotation*> icons() const
    {
        std::vector<rocketplot::IconAnnotation*> found;
        for (rocketplot::Annotation* const annotation : m_plot.plotWidget().annotations())
        {
            if (auto* const icon = qobject_cast<rocketplot::IconAnnotation*>(annotation))
            {
                found.push_back(icon);
            }
        }
        return found;
    }

    /// The scatter series @p index of the plot's series (null with a test failure).
    [[nodiscard]] rocketplot::ScatterSeries* scatter(int index) const
    {
        const QList<rocketplot::Series*> series = m_plot.plotWidget().series();
        auto* const scatter = index < series.size()
                                  ? qobject_cast<rocketplot::ScatterSeries*>(series.at(index))
                                  : nullptr;
        if (scatter == nullptr)
        {
            ADD_FAILURE() << "no scatter series " << index;
        }
        return scatter;
    }

private:
    InMemoryPreferences          m_preferences;
    MultiLevelPinkNoiseWindModel m_model{m_preferences};
    WindProfilePlot              m_plot{m_model, nullptr};
};

/// The units of the table: altitude, speed, direction and deviation.
[[nodiscard]] WindProfilePlot::Units unitsNamed(std::string_view altitude, std::string_view speed,
                                                std::string_view direction,
                                                std::string_view deviation)
{
    return {.altitude  = unitGroup(UnitGroupId::DISTANCE).getUnit(altitude),
            .speed     = unitGroup(UnitGroupId::WINDSPEED).getUnit(speed),
            .direction = unitGroup(UnitGroupId::ANGLE).getUnit(direction),
            .deviation = unitGroup(UnitGroupId::WINDSPEED).getUnit(deviation)};
}

/// The probe's TIP lines of @p title.
[[nodiscard]] std::vector<std::string> tipPins(std::span<const std::string_view> pins,
                                               std::string_view                  title)
{
    const std::string                     prefix = "TIP " + std::string(title) + " ";
    const std::array<std::string_view, 1> prefixes{prefix};
    return Gui::Test::windPinLinesWith(pins, prefixes);
}

/// The colour of each point of @p series.
[[nodiscard]] std::vector<QColor> pointColors(const rocketplot::ScatterSeries& series)
{
    std::vector<QColor> colors;
    colors.reserve(series.size());
    for (std::size_t point = 0; point < series.size(); ++point)
    {
        colors.push_back(series.pointColor(point));
    }
    return colors;
}

/// The marker size of each point of @p series.
[[nodiscard]] std::vector<double> pointSizes(const rocketplot::ScatterSeries& series)
{
    std::vector<double> sizes;
    sizes.reserve(series.size());
    for (std::size_t point = 0; point < series.size(); ++point)
    {
        sizes.push_back(series.pointSize(point));
    }
    return sizes;
}

/// What @p icon paints in a square of @p side pixels.
[[nodiscard]] std::vector<Gui::Test::RecordedPaint> paintedIcon(const QIcon& icon, int side)
{
    Gui::Test::RecordingPaintDevice device(QSize(side, side));
    {
        QPainter painter(&device);
        icon.paint(&painter, QRect(0, 0, side, side));
    }
    return device.records();
}

/// @p arrow: at @p point, kArrowIconSide pixels across, without a tool tip.
void expectArrow(const rocketplot::IconAnnotation& arrow, QPointF point)
{
    EXPECT_EQ(arrow.position(), point);
    EXPECT_EQ(arrow.size(), QSizeF(50, 50));
    EXPECT_TRUE(arrow.toolTip().isEmpty());
}

/// @p tip: at @p point, twice the dot's 6 pixels across, an icon that draws nothing.
void expectTip(const rocketplot::IconAnnotation& tip, QPointF point)
{
    EXPECT_EQ(tip.position(), point);
    EXPECT_EQ(tip.size(), QSizeF(12, 12));
    EXPECT_FALSE(tip.icon().isNull());
}

/// The arrows (the first three icons) and the tool tips (the last three) of three levels at the
/// points of @p line.
void expectLevelIcons(const std::vector<rocketplot::IconAnnotation*>& icons,
                      const Gui::Test::SeriesReadback&                line)
{
    ASSERT_EQ(icons.size(), 6U);
    for (std::size_t i = 0; i < 3; ++i)
    {
        const QPointF point(line.x.at(i), line.y.at(i));
        expectArrow(*icons.at(i), point);
        expectTip(*icons.at(i + 3), point);
    }
}

/// The tool tips of @p icons.
[[nodiscard]] std::vector<std::string> toolTipsOf(
    const std::vector<rocketplot::IconAnnotation*>& icons)
{
    std::vector<std::string> tips;
    tips.reserve(icons.size());
    for (const rocketplot::IconAnnotation* const icon : icons)
    {
        tips.push_back(QtStrings::toUtf8(icon->toolTip()));
    }
    return tips;
}

}  // namespace

TEST_F(WindProfilePlotTest, TheProfileOfThreeLevels)
{
    Profile                       profile;
    const rocketplot::PlotWidget& plot = profile.plot().plotWidget();
    EXPECT_EQ(profile.plot().objectName(), QStringLiteral("visualization"));
    EXPECT_EQ(plot.objectName(), QStringLiteral("windProfileVisualization"));
    const std::vector<Gui::Test::SeriesReadback> series = Gui::Test::readAllSeries(plot);
    ASSERT_EQ(series.size(), 2U);
    // Java: the grey lines between the levels, then the blue dots.
    EXPECT_EQ(series.at(0).x, (std::vector<double>{3, 7.5, 12.25}));
    EXPECT_EQ(series.at(0).y, (std::vector<double>{0, 500, 1250.5}));
    EXPECT_EQ(series.at(0).color, QColor(128, 128, 128));
    EXPECT_EQ(series.at(0).lineWidth, 1.0);
    EXPECT_EQ(series.at(0).marker, rocketplot::Marker::NONE);
    EXPECT_TRUE(series.at(0).name.isEmpty());
    EXPECT_EQ(series.at(1).x, series.at(0).x);
    EXPECT_EQ(series.at(1).y, series.at(0).y);
    EXPECT_EQ(series.at(1).marker, rocketplot::Marker::CIRCLE);
    EXPECT_EQ(series.at(1).markerRingWidth, 0.0);
    const rocketplot::ScatterSeries* const dots = profile.scatter(1);
    ASSERT_NE(dots, nullptr);
    EXPECT_EQ(pointColors(*dots), (std::vector<QColor>(3, QColor(0, 0, 255))));
    EXPECT_EQ(pointSizes(*dots), (std::vector<double>(3, 6.0)));
    // Java: the axes reach 10 % beyond the highest values.
    const Gui::Test::AxisReadback x = Gui::Test::readAxis(*plot.xAxis());
    const Gui::Test::AxisReadback y = Gui::Test::readAxis(*plot.yAxis());
    EXPECT_EQ(x.min, 0.0);
    EXPECT_EQ(x.max, WindProfilePlot::kRangeExtension * 12.25);
    EXPECT_EQ(y.min, 0.0);
    EXPECT_EQ(y.max, WindProfilePlot::kRangeExtension * 1250.5);
    EXPECT_FALSE(x.autoscale);
    EXPECT_EQ(x.label, rocketplot::literalText(QStringLiteral("Wind speed (m/s)")));
    EXPECT_EQ(y.label, rocketplot::literalText(QStringLiteral("Altitude MSL (m)")));
    // An arrow per level, then a tool tip per level.
    const std::vector<rocketplot::IconAnnotation*> icons = profile.icons();
    ASSERT_EQ(icons.size(), 6U);
    expectLevelIcons(icons, series.at(0));
    EXPECT_EQ(profile.plot().showDirectionsCheckBox().text(),
              QStringLiteral("Show wind direction vectors"));
    EXPECT_TRUE(profile.plot().showDirectionsCheckBox().isChecked());
    EXPECT_EQ(plot.contextMenuPolicy(), Qt::NoContextMenu);
}

TEST_F(WindProfilePlotTest, TheToolTipsAreJavas)
{
    Profile profile;
    EXPECT_EQ(Gui::Test::windTipLines("MSL m", profile.model(), profile.plot().units()),
              tipPins(Pins::kThreeLevels, "MSL m"));
    const std::vector<std::string> shown = toolTipsOf(profile.icons());
    ASSERT_EQ(shown.size(), 6U);
    EXPECT_EQ(shown.at(3),
              "<html>Altitude MSL: 0 m<br>Speed: 3 m/s<br>Direction: 90\u00B0<br>Deviation: "
              "0.3 m/s</html>");
    profile.model().setAltitudeReference(WindModel::AltitudeReference::AGL);
    EXPECT_EQ(Gui::Test::windTipLines("AGL m", profile.model(), profile.plot().units()),
              tipPins(Pins::kThreeLevels, "AGL m"));
    profile.model().setAltitudeReference(WindModel::AltitudeReference::MSL);
    EXPECT_EQ(
        Gui::Test::windTipLines("MSL ft", profile.model(), unitsNamed("ft", "mph", "rad", "km/h")),
        tipPins(Pins::kUnits, "MSL ft"));
}

TEST_F(WindProfilePlotTest, TheProfileIsInTheTablesUnits)
{
    Profile profile;
    profile.plot().setUnits(unitsNamed("ft", "mph", "rad", "km/h"));
    const rocketplot::PlotWidget&                plot   = profile.plot().plotWidget();
    const Unit&                                  feet   = *profile.plot().units().altitude;
    const Unit&                                  mph    = *profile.plot().units().speed;
    const std::vector<Gui::Test::SeriesReadback> series = Gui::Test::readAllSeries(plot);
    ASSERT_EQ(series.size(), 2U);
    EXPECT_EQ(series.at(0).x,
              (std::vector<double>{mph.toUnit(3), mph.toUnit(7.5), mph.toUnit(12.25)}));
    EXPECT_EQ(series.at(0).y,
              (std::vector<double>{feet.toUnit(0), feet.toUnit(500), feet.toUnit(1250.5)}));
    EXPECT_EQ(plot.xAxis()->max(), mph.toUnit(WindProfilePlot::kRangeExtension * 12.25));
    EXPECT_EQ(plot.yAxis()->max(), feet.toUnit(WindProfilePlot::kRangeExtension * 1250.5));
    EXPECT_EQ(plot.xAxis()->label(), rocketplot::literalText(QStringLiteral("Wind speed (mph)")));
    EXPECT_EQ(plot.yAxis()->label(), rocketplot::literalText(QStringLiteral("Altitude MSL (ft)")));
    profile.model().setAltitudeReference(WindModel::AltitudeReference::AGL);
    EXPECT_EQ(plot.yAxis()->label(), rocketplot::literalText(QStringLiteral("Altitude AGL (ft)")));
    EXPECT_THROW(profile.plot().setUnits({}), BugError);
}

TEST_F(WindProfilePlotTest, TheSelectedLevelIsRedAndRinged)
{
    Profile      profile;
    const Level* level = profile.model().getLevels().at(1);
    profile.plot().setSelectedLevel(level->weak_from_this());
    EXPECT_EQ(profile.plot().selectedLevel(), level);
    const rocketplot::ScatterSeries* const dots = profile.scatter(1);
    ASSERT_NE(dots, nullptr);
    EXPECT_EQ(pointColors(*dots),
              (std::vector<QColor>{QColor(0, 0, 255), QColor(255, 0, 0), QColor(0, 0, 255)}));
    EXPECT_EQ(pointSizes(*dots), (std::vector<double>{6, 10, 6}));
    // Java: drawOval in black around it.
    const std::vector<Gui::Test::SeriesReadback> series =
        Gui::Test::readAllSeries(profile.plot().plotWidget());
    ASSERT_EQ(series.size(), 3U);
    EXPECT_EQ(series.at(2).x, (std::vector<double>{7.5}));
    EXPECT_EQ(series.at(2).y, (std::vector<double>{500}));
    EXPECT_EQ(series.at(2).color, QColor(Qt::black));
    EXPECT_EQ(series.at(2).markerSize, 10.0);
    const rocketplot::ScatterSeries* const ring = profile.scatter(2);
    ASSERT_NE(ring, nullptr);
    EXPECT_FALSE(ring->isMarkerFilled());
    // Its tool tip within twice its radius, its arrow in Java's SELECTED_DIRECTION_COLOR.
    const std::vector<rocketplot::IconAnnotation*> icons = profile.icons();
    ASSERT_EQ(icons.size(), 6U);
    EXPECT_EQ(icons.at(4)->size(), QSizeF(20, 20));
    const std::vector<Gui::Test::RecordedPaint> selected = paintedIcon(icons.at(1)->icon(), 50);
    const std::vector<Gui::Test::RecordedPaint> other    = paintedIcon(icons.at(0)->icon(), 50);
    ASSERT_EQ(selected.size(), 2U);
    ASSERT_EQ(other.size(), 2U);
    EXPECT_EQ(selected.at(0).pen.color(), QColor(199, 40, 40));
    EXPECT_EQ(selected.at(1).brush.color(), QColor(199, 40, 40));
    EXPECT_EQ(other.at(0).pen.color(), QColor(40, 40, 199));
    EXPECT_EQ(other.at(1).brush.color(), QColor(40, 40, 199));
    // The level goes: nothing is selected.
    profile.model().removeWindLevelIdx(1);
    EXPECT_EQ(profile.plot().selectedLevel(), nullptr);
    EXPECT_EQ(Gui::Test::readAllSeries(profile.plot().plotWidget()).size(), 2U);
}

TEST_F(WindProfilePlotTest, TheArrowsCanBeHidden)
{
    Profile profile;
    profile.plot().showDirectionsCheckBox().click();
    EXPECT_FALSE(profile.plot().showDirectionsCheckBox().isChecked());
    const std::vector<rocketplot::IconAnnotation*> icons = profile.icons();
    ASSERT_EQ(icons.size(), 3U);
    EXPECT_FALSE(icons.front()->toolTip().isEmpty());
    profile.plot().showDirectionsCheckBox().click();
    EXPECT_EQ(profile.icons().size(), 6U);
}

TEST_F(WindProfilePlotTest, TheArrowPointsDownwind)
{
    // Java: a wind from the north (0) blows south, down the screen; from the east (pi/2) to the
    // west, left; the line 15 pixels, the head's tip 24 pixels from the dot, its sides 10 pixels
    // long at 30 degrees from the arrow.
    const QPointF dot(100, 100);
    const QLineF  south = WindProfilePlot::arrowLine(0, dot, 1);
    EXPECT_EQ(south.p1(), dot);
    EXPECT_NEAR(south.p2().x(), 100, 1e-12);
    EXPECT_NEAR(south.p2().y(), 115, 1e-12);
    const QLineF west = WindProfilePlot::arrowLine(std::numbers::pi / 2, dot, 2);
    EXPECT_NEAR(west.p2().x(), 70, 1e-12);
    EXPECT_NEAR(west.p2().y(), 100, 1e-12);
    const QPolygonF head = WindProfilePlot::arrowHead(0, dot, 1);
    ASSERT_EQ(head.size(), 3);
    EXPECT_NEAR(head.at(0).x(), 100, 1e-12);
    EXPECT_NEAR(head.at(0).y(), 124, 1e-12);
    EXPECT_NEAR(head.at(1).x(), 95, 1e-12);
    EXPECT_NEAR(head.at(1).y(), 124 - (10 * std::sqrt(3.0) / 2), 1e-12);
    EXPECT_NEAR(head.at(2).x(), 105, 1e-12);
    EXPECT_NEAR(head.at(2).y(), head.at(1).y(), 1e-12);
}

TEST_F(WindProfilePlotTest, TheRangesShowEveryLevel)
{
    const Unit& metres = unitGroup(UnitGroupId::DISTANCE).getDefaultUnit();
    EXPECT_EQ(WindProfilePlot::axisRange(0, 1250.5, metres),
              std::make_pair(0.0, WindProfilePlot::kRangeExtension * 1250.5));
    // Below sea level (Java draws such levels below its axis, or upside down).
    EXPECT_EQ(WindProfilePlot::axisRange(-50, 100, metres),
              std::make_pair(WindProfilePlot::kRangeExtension * -50,
                             WindProfilePlot::kRangeExtension * 100));
    EXPECT_EQ(WindProfilePlot::axisRange(-50, -10, metres),
              std::make_pair(WindProfilePlot::kRangeExtension * -50, 0.0));
    // An empty range (Java divides by zero).
    EXPECT_EQ(WindProfilePlot::axisRange(0, 0, metres), std::make_pair(0.0, 1.0));
}

TEST_F(WindProfilePlotTest, TheProfileFollowsItsModel)
{
    Profile profile;
    ASSERT_TRUE(profile.model().addWindLevel(2000, 20, 0, 0).has_value());
    const rocketplot::PlotWidget& plot = profile.plot().plotWidget();
    EXPECT_EQ(Gui::Test::readAllSeries(plot).at(0).x, (std::vector<double>{3, 7.5, 12.25, 20}));
    EXPECT_EQ(plot.yAxis()->max(), WindProfilePlot::kRangeExtension * 2000);
    // Sorted by altitude, as Java draws them.
    profile.model().getLevels().front()->setAltitude(3000);
    EXPECT_EQ(Gui::Test::readAllSeries(plot).at(0).y,
              (std::vector<double>{500, 1250.5, 2000, 3000}));
    // No levels: the axes alone, [0, 1].
    profile.model().clearLevels();
    EXPECT_TRUE(Gui::Test::readAllSeries(plot).empty());
    EXPECT_TRUE(profile.icons().empty());
    EXPECT_EQ(plot.xAxis()->max(), 1.0);
    EXPECT_EQ(plot.yAxis()->max(), 1.0);
}

TEST_F(WindProfilePlotTest, TheProfileKeepsWhatItShowedWhenTheModelGoes)
{
    const InMemoryPreferences preferences;
    auto                      model = std::make_unique<MultiLevelPinkNoiseWindModel>(preferences);
    threeLevels(*model);
    WindProfilePlot plot(*model, nullptr);
    model.reset();
    EXPECT_FALSE(plot.hasModel());
    plot.refresh();
    plot.setUnits(unitsNamed("ft", "mph", "rad", "km/h"));
    EXPECT_EQ(Gui::Test::readAllSeries(plot.plotWidget()).at(0).y,
              (std::vector<double>{0, 500, 1250.5}));
}

}  // namespace QtRocket::Gui
