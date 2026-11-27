#include "QtRocket/gui/plot/SimulationPlot.h"

#include <cstddef>
#include <format>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <QChar>
#include <QColor>
#include <QCoreApplication>
#include <QIcon>
#include <QList>
#include <QPoint>
#include <QPointF>
#include <QRgb>
#include <QString>
#include <QStringList>
#include <QTest>
#include <Qt>
#include <QtContainerFwd>

#include <gtest/gtest.h>
#include <rocketplot/Axis.h>
#include <rocketplot/Legend.h>
#include <rocketplot/LineSeries.h>
#include <rocketplot/PlotWidget.h>
#include <rocketplot/Series.h>
#include <rocketplot/TextAnnotation.h>
#include <rocketplot/enums.h>
#include <rocketplot/text.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/plot/DataPlot.h"
#include "QtRocket/gui/plot/PlotColors.h"
#include "QtRocket/gui/plot/PlotEventGraphics.h"
#include "QtRocket/gui/plot/PlotInputBindings.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/plot/PlotPreferences.h"
#include "QtRocket/plot/SimulationPlotConfiguration.h"
#include "QtRocket/plot/SimulationPlotModel.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/LineStyle.h"
#include "ScopedApplicationPalette.h"
#include "SimulationRunTestSupport.h"
#include "TestGuiServices.h"
#include "plot/PlotReadback.h"

// The simulation plot (OpenRocket's SimulationPlot and Plot, swing gui/plot) over hand-made
// flight data: what the core's SimulationPlotModel computes (tests/core/plot) drawn on a
// rocketplot plot, read back without pixels (PlotReadback.h). The comparisons with OpenRocket's
// own plots of the examples are in simulation_plot_chart_golden_tests.cpp.

namespace QtRocket::Gui
{

namespace
{

using SimulationPlotTest = Gui::Test::GuiTest;
using Id                 = FlightDataTypeId;
using Event              = FlightEvent::Type;
using Model              = SimulationPlotModel;
using Palette            = Gui::Test::ScopedApplicationPalette;

constexpr int kPlotWidth  = 800;
constexpr int kPlotHeight = 600;

[[nodiscard]] const FlightDataType& dataType(Id id)
{
    return FlightDataType::builtin(id);
}

/// A column of a hand-made branch: a type and its value at each time.
struct PlotColumn
{
    PlotColumn(Id columnType, std::vector<double> columnValues)
      : type(columnType), values(std::move(columnValues))
    {
    }

    Id                  type;
    std::vector<double> values;
};

/// A branch named @p name with the times @p times, @p columns and @p events.
[[nodiscard]] std::shared_ptr<FlightDataBranch> plotBranch(std::string                    name,
                                                           const std::vector<double>&     times,
                                                           const std::vector<PlotColumn>& columns,
                                                           std::vector<FlightEvent> events = {})
{
    std::vector<const FlightDataType*> types{&dataType(Id::TYPE_TIME)};
    for (const PlotColumn& column : columns)
    {
        types.push_back(&dataType(column.type));
    }
    auto branch = std::make_shared<FlightDataBranch>(std::move(name), types);
    for (std::size_t i = 0; i < times.size(); ++i)
    {
        branch->addPoint();
        branch->setValue(dataType(Id::TYPE_TIME), times[i]);
        for (const PlotColumn& column : columns)
        {
            branch->setValue(dataType(column.type), column.values.at(i));
        }
    }
    for (FlightEvent& event : events)
    {
        branch->addEvent(std::move(event));
    }
    return branch;
}

/// A row of a hand-made configuration: its type and axis.
struct PlotRow
{
    PlotRow(Id rowType, int rowAxis) : type(rowType), axis(rowAxis) { }

    Id  type;
    int axis;
};

/// A configuration named "Test" over @p domain with @p rows, marking @p events.
[[nodiscard]] SimulationPlotConfiguration plotConfiguration(
    Id domain, std::initializer_list<PlotRow> rows, std::initializer_list<Event> events = {})
{
    SimulationPlotConfiguration configuration("Test", dataType(domain));
    for (const PlotRow& row : rows)
    {
        configuration.addPlotDataType(dataType(row.type), row.axis);
    }
    for (const Event event : events)
    {
        configuration.setEvent(event, true);
    }
    return configuration;
}

/// A sustainer of four points and a booster of three, with altitude and vertical velocity.
[[nodiscard]] std::shared_ptr<const FlightData> twoStages()
{
    return std::make_shared<const FlightData>(
        std::initializer_list{plotBranch("Sustainer", {0, 1, 2, 3},
                                         {PlotColumn(Id::TYPE_ALTITUDE, {0, 10, 20, 15}),
                                          PlotColumn(Id::TYPE_VELOCITY_Z, {0, 10, 5, -5})}),
                              plotBranch("Booster", {0, 1, 2},
                                         {PlotColumn(Id::TYPE_ALTITUDE, {0, 5, 2}),
                                          PlotColumn(Id::TYPE_VELOCITY_Z, {0, 4, -3})})});
}

/// The altitude on the left axis and the vertical velocity on the right one of twoStages().
[[nodiscard]] Model twoStagesModel()
{
    return {"Simulation 1", twoStages(),
            plotConfiguration(Id::TYPE_TIME,
                              {PlotRow(Id::TYPE_ALTITUDE, 0), PlotRow(Id::TYPE_VELOCITY_Z, 1)}),
            SimulationPlot::translator()};
}

/// A flight of 60 s (one point a second) with the events of OpenRocket's preset plots: ignition
/// and burnout at the start, apogee and the deployment 0.03 s apart (one group), a warning, and
/// the ground hit at 59.9 s, near enough to the end to widen the domain for its line (OpenRocket
/// bug 43).
[[nodiscard]] std::shared_ptr<const FlightData> longFlight()
{
    std::vector<double> times;
    std::vector<double> altitudes;
    std::vector<double> velocities;
    for (int i = 0; i <= 60; ++i)
    {
        const double t = i;
        times.push_back(t);
        altitudes.push_back((40.0 * t) - (t * t / 1.5));
        velocities.push_back(40.0 - (t / 0.75));
    }
    std::vector<FlightEvent> events{
        FlightEvent(Event::LAUNCH, 0.0),
        FlightEvent(Event::IGNITION, 0.0),
        FlightEvent(Event::BURNOUT, 1.5),
        FlightEvent(Event::APOGEE, 30.0),
        FlightEvent(Event::RECOVERY_DEVICE_DEPLOYMENT, 30.03),
        FlightEvent(
            Event::SIM_WARN, 40.0, nullptr,
            FlightEvent::warningData(Warning::fromString("A warning", MessagePriority::HIGH))),
        FlightEvent(Event::GROUND_HIT, 59.9),
    };
    return std::make_shared<const FlightData>(std::initializer_list{plotBranch(
        "Sustainer", times,
        {PlotColumn(Id::TYPE_ALTITUDE, altitudes), PlotColumn(Id::TYPE_VELOCITY_Z, velocities)},
        std::move(events))});
}

/// longFlight() over time with the events of the "Vertical motion" preset.
[[nodiscard]] Model longFlightModel(Id domain = Id::TYPE_TIME)
{
    return {"Long flight", longFlight(),
            plotConfiguration(
                domain, {PlotRow(Id::TYPE_ALTITUDE, 0), PlotRow(Id::TYPE_VELOCITY_Z, 1)},
                {Event::IGNITION, Event::BURNOUT, Event::APOGEE, Event::RECOVERY_DEVICE_DEPLOYMENT,
                 Event::GROUND_HIT, Event::SIM_WARN, Event::SIM_ABORT}),
            SimulationPlot::translator()};
}

/// Three stages, the first and the last aborted.
[[nodiscard]] std::shared_ptr<const FlightData> abortedStages()
{
    const std::vector<double>     times{0, 1, 2, 3};
    const std::vector<PlotColumn> columns{PlotColumn(Id::TYPE_ALTITUDE, {0, 10, 20, 15})};
    return std::make_shared<const FlightData>(std::initializer_list{
        plotBranch("Sustainer", times, columns,
                   {FlightEvent(Event::SIM_ABORT, 2.5, nullptr,
                                SimulationAbort(SimulationAbort::Cause::NO_CP))}),
        plotBranch("Booster", times, columns),
        plotBranch("Payload", times, columns,
                   {FlightEvent(Event::SIM_ABORT, 1.25, nullptr,
                                SimulationAbort(SimulationAbort::Cause::NO_LIFTOFF))})});
}

[[nodiscard]] Model abortedModel()
{
    return {"Aborted", abortedStages(),
            plotConfiguration(Id::TYPE_TIME, {PlotRow(Id::TYPE_ALTITUDE, 0)}, {Event::SIM_ABORT}),
            SimulationPlot::translator()};
}

/// What a readout hook was given: each text, and where.
struct ReadoutRecorder
{
    void operator()(const QPoint& position, const QString& text) const
    {
        texts->push_back(text);
        positions->push_back(position);
    }

    std::shared_ptr<std::vector<QString>> texts     = std::make_shared<std::vector<QString>>();
    std::shared_ptr<std::vector<QPoint>>  positions = std::make_shared<std::vector<QPoint>>();
};

/// A plot of @p model over the test's services, laid out at the plot's size.
class PlotUnderTest
{
public:
    explicit PlotUnderTest(Model model, bool showPoints = false)
      : m_plot(m_services.services(), std::move(model), showPoints)
    {
        m_plot.resize(kPlotWidth, kPlotHeight);
        m_plot.setReadoutHook(m_readout);
    }

    [[nodiscard]] Gui::Test::TestGuiServices& services() noexcept { return m_services; }
    [[nodiscard]] SimulationPlot&             plot() noexcept { return m_plot; }
    [[nodiscard]] rocketplot::PlotWidget& widget() const noexcept { return m_plot.plotWidget(); }
    [[nodiscard]] const ReadoutRecorder&  readout() const noexcept { return m_readout; }

private:
    Gui::Test::TestGuiServices m_services;
    ReadoutRecorder            m_readout;
    SimulationPlot             m_plot;
};

/// The names of the series of @p plot, as given to rocketplot.
[[nodiscard]] QStringList givenNames(std::initializer_list<const char*> names)
{
    QStringList given;
    for (const char* const name : names)
    {
        given.append(rocketplot::literalText(QString::fromUtf8(name)));
    }
    return given;
}

/// Whether each series of @p plot is drawn against the right axis.
[[nodiscard]] std::vector<bool> secondaryAxes(const rocketplot::PlotWidget& plot)
{
    std::vector<bool>                secondary;
    const QList<rocketplot::Series*> all = plot.series();
    for (const rocketplot::Series* const series : all)
    {
        secondary.push_back(series->isOnSecondaryYAxis());
    }
    return secondary;
}

/// The colour of each series of @p plot.
[[nodiscard]] std::vector<QColor> seriesColors(const rocketplot::PlotWidget& plot)
{
    std::vector<QColor> colors;
    for (const Gui::Test::SeriesReadback& series : Gui::Test::readAllSeries(plot))
    {
        colors.push_back(series.color);
    }
    return colors;
}

/// The visibility of each series of @p plot.
[[nodiscard]] std::vector<bool> seriesVisibility(const rocketplot::PlotWidget& plot)
{
    std::vector<bool> visible;
    for (const Gui::Test::SeriesReadback& series : Gui::Test::readAllSeries(plot))
    {
        visible.push_back(series.visible);
    }
    return visible;
}

/// The marker of each series of @p plot.
[[nodiscard]] std::vector<rocketplot::Marker> seriesMarkers(const rocketplot::PlotWidget& plot)
{
    std::vector<rocketplot::Marker> markers;
    for (const Gui::Test::SeriesReadback& series : Gui::Test::readAllSeries(plot))
    {
        markers.push_back(series.marker);
    }
    return markers;
}

/// The x of each event line of @p plot.
[[nodiscard]] std::vector<double> eventTimes(const rocketplot::PlotWidget& plot)
{
    std::vector<double> times;
    for (const Gui::Test::EventMarkerReadback& marker : Gui::Test::readAnnotations(plot).events)
    {
        times.push_back(marker.x);
    }
    return times;
}

/// The event lines @p plot draws for its model's groups of every stage: at each group's time,
/// with its label, in its look's colour, with its look's image.
[[nodiscard]] std::vector<Gui::Test::EventMarkerReadback> expectedEventLines(
    const SimulationPlot& plot)
{
    std::vector<Gui::Test::EventMarkerReadback> lines;
    for (const Model::EventGroup& group : plot.model().getEventGroups())
    {
        lines.push_back({.x       = group.time,
                         .label   = rocketplot::literalText(QtStrings::fromUtf8(group.label)),
                         .color   = PlotEventGraphics::markerColor(group.lookType, plot.palette()),
                         .icon    = Gui::Test::iconImage(PlotEventGraphics::eventIcon(
                             group.lookType, group.lookWarningPriority)),
                         .visible = true});
    }
    return lines;
}

/// The icons @p plot draws for its model's icons of every stage: at each icon's point on its
/// series' axis, with its look's image and its tool tip.
[[nodiscard]] std::vector<Gui::Test::IconReadback> expectedEventIcons(const SimulationPlot& plot)
{
    std::vector<Gui::Test::IconReadback> icons;
    const Model&                         model = plot.model();
    for (const Model::EventIcon& icon : model.getIconMarkers())
    {
        icons.push_back({.position       = QPointF(icon.x, icon.y),
                         .secondaryYAxis = model.getSeries().at(icon.series).axis == 1,
                         .icon           = Gui::Test::iconImage(
                             PlotEventGraphics::eventIcon(icon.lookType, icon.lookWarningPriority)),
                         .toolTip             = QtStrings::fromUtf8(icon.toolTip.toHtml()),
                         .visible             = true,
                         .includedInAutoscale = false});
    }
    return icons;
}

/// Whether @p plot's abort box is visible; nullopt without one.
[[nodiscard]] std::optional<bool> abortBoxVisible(const SimulationPlot& plot)
{
    const rocketplot::TextAnnotation* const box = plot.abortBox();
    if (box == nullptr)
    {
        return std::nullopt;
    }
    return box->isVisible();
}

/// The text of @p plot's abort box, as given; nullopt without one.
[[nodiscard]] std::optional<QString> abortBoxText(const SimulationPlot& plot)
{
    const rocketplot::TextAnnotation* const box = plot.abortBox();
    if (box == nullptr)
    {
        return std::nullopt;
    }
    return box->text();
}

/// A flight of one stage named @p stage that aborts at 1.5 s, over time.
[[nodiscard]] Model abortedStageModel(std::string stage)
{
    const auto data = std::make_shared<const FlightData>(std::initializer_list{
        plotBranch(std::move(stage), {0, 1, 2}, {PlotColumn(Id::TYPE_ALTITUDE, {0, 10, 5})},
                   {FlightEvent(Event::SIM_ABORT, 1.5, nullptr,
                                SimulationAbort(SimulationAbort::Cause::NO_CP))})});
    return {"Aborted", data,
            plotConfiguration(Id::TYPE_TIME, {PlotRow(Id::TYPE_ALTITUDE, 0)}, {Event::SIM_ABORT}),
            SimulationPlot::translator()};
}

/// The lines of @p text that @p plot's SVG export does not draw as texts of their own, in order,
/// then, if there are any, every text the export draws (joined by '|'). The export is made at
/// kPlotWidth x kPlotHeight, which @p plot is given first (the plot of a SimulationPlot never
/// shown has not been laid out: a child widget's 100 x 30 pixels).
[[nodiscard]] std::vector<std::string> linesNotDrawn(rocketplot::PlotWidget& plot,
                                                     const QString&          text)
{
    plot.resize(kPlotWidth, kPlotHeight);
    QStringList drawn;
    for (const Gui::Test::SvgText& svg : Gui::Test::svgTexts(plot))
    {
        drawn.append(svg.text);
    }
    std::vector<std::string> missing;
    for (const QString& line : text.split(QLatin1Char('\n')))
    {
        if (!drawn.contains(line))
        {
            missing.push_back(QtStrings::toUtf8(line));
        }
    }
    if (!missing.empty())
    {
        missing.push_back("drawn: " + QtStrings::toUtf8(drawn.join(u'|')));
    }
    return missing;
}

/// A flight of @p count stages, "Stage 1" to "Stage <count>", each with two points of altitude.
[[nodiscard]] Model manyStagesModel(std::size_t count)
{
    std::vector<std::shared_ptr<FlightDataBranch>> branches;
    branches.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        branches.push_back(plotBranch(std::format("Stage {}", i + 1), {0, 1},
                                      {PlotColumn(Id::TYPE_ALTITUDE, {0, 10})}));
    }
    return {"Stages", std::make_shared<const FlightData>(branches),
            plotConfiguration(Id::TYPE_TIME, {PlotRow(Id::TYPE_ALTITUDE, 0)}),
            SimulationPlot::translator()};
}

/// A flight of three points at the altitude @p value, over time.
[[nodiscard]] Model flatModel(double value)
{
    return {"Flat",
            std::make_shared<const FlightData>(std::initializer_list{plotBranch(
                "Sustainer", {0, 1, 2}, {PlotColumn(Id::TYPE_ALTITUDE, {value, value, value})})}),
            plotConfiguration(Id::TYPE_TIME, {PlotRow(Id::TYPE_ALTITUDE, 0)}),
            SimulationPlot::translator()};
}

/// The marker size of each series of @p plot.
[[nodiscard]] std::vector<double> seriesMarkerSizes(const rocketplot::PlotWidget& plot)
{
    std::vector<double> sizes;
    for (const Gui::Test::SeriesReadback& series : Gui::Test::readAllSeries(plot))
    {
        sizes.push_back(series.markerSize);
    }
    return sizes;
}

/// Whether @p axis shows @p range exactly.
[[nodiscard]] bool showsRange(const rocketplot::Axis& axis, const Model::Range& range)
{
    return axis.min() == range.min && axis.max() == range.max;
}

}  // namespace

// --------------------------------------------------------------------------------------- series

TEST_F(SimulationPlotTest, TheSeriesGoAxisByAxisRowByRowBranchByBranch)
{
    PlotUnderTest test(twoStagesModel());
    // Plot.java's dataset order; each series named by its branch after the first.
    EXPECT_EQ(Gui::Test::seriesNames(test.widget()),
              givenNames({"Altitude (m)", "Booster: Altitude (m)", "Vertical velocity (m/s)",
                          "Booster: Vertical velocity (m/s)"}));
    EXPECT_EQ(secondaryAxes(test.widget()), (std::vector<bool>{false, false, true, true}));
    // The points in the rows' units.
    const Gui::Test::SeriesReadback booster = Gui::Test::readSeries(*test.widget().series().at(1));
    EXPECT_EQ(booster.x, (std::vector<double>{0, 1, 2}));
    EXPECT_EQ(booster.y, (std::vector<double>{0, 5, 2}));
    EXPECT_EQ(test.plot().seriesCount(), 4U);
    EXPECT_EQ(test.plot().series(2), test.widget().series().at(2));
    EXPECT_THROW(static_cast<void>(test.plot().series(4)), BugError);
}

TEST_F(SimulationPlotTest, EachRowHasTheColourThePanelShows)
{
    const PlotUnderTest test(twoStagesModel());
    // OpenRocket bug 42: the right axis' row has its own colour (the left rows times the branches),
    // both branches of a row the same.
    EXPECT_EQ(seriesColors(test.widget()),
              (std::vector<QColor>{PlotColors::plotColor(0), PlotColors::plotColor(0),
                                   PlotColors::plotColor(2), PlotColors::plotColor(2)}));
}

TEST_F(SimulationPlotTest, TheLinesAreOpenRocketsStrokes)
{
    SimulationPlotConfiguration configuration = plotConfiguration(
        Id::TYPE_TIME, {PlotRow(Id::TYPE_ALTITUDE, 0), PlotRow(Id::TYPE_VELOCITY_Z, 1)});
    configuration.setPlotDataColor(1, Color(10, 20, 30));
    configuration.setPlotDataLineStyle(1, LineStyle::DASHDOT);
    const PlotUnderTest             test(Model("Simulation 1", twoStages(), configuration));
    const Gui::Test::SeriesReadback altitude = Gui::Test::readSeries(*test.widget().series().at(0));
    EXPECT_EQ(altitude.lineWidth, 1.5);
    EXPECT_EQ(altitude.marker, rocketplot::Marker::NONE);
    EXPECT_EQ(altitude.legendGroup, test.widget().series().at(1)->legendGroup());
    // BasicStroke(1.5f, CAP_BUTT, ...): no dashes for SOLID, the style's otherwise.
    EXPECT_EQ(Gui::Test::readSeriesLine(*test.widget().series().at(0)),
              (Gui::Test::SeriesLineReadback{
                  .secondaryYAxis = false, .dashPattern = {}, .capStyle = Qt::FlatCap}));
    EXPECT_EQ(
        Gui::Test::readSeriesLine(*test.widget().series().at(3)),
        (Gui::Test::SeriesLineReadback{
            .secondaryYAxis = true, .dashPattern = {8.0, 3.0, 2.0, 3.0}, .capStyle = Qt::FlatCap}));
    EXPECT_EQ(test.widget().series().at(3)->color(), QColor::fromRgb(10, 20, 30));
    // JFreeChart's XYPlot draws the first series on top.
    EXPECT_EQ(test.widget().seriesDrawOrder(), rocketplot::SeriesDrawOrder::REVERSE);
}

TEST_F(SimulationPlotTest, ThePointsHaveTheShapeOfTheirBranch)
{
    PlotUnderTest test(twoStagesModel(), true);
    EXPECT_TRUE(test.plot().showsPoints());
    using rocketplot::Marker;
    EXPECT_EQ(seriesMarkers(test.widget()), (std::vector<Marker>{Marker::SQUARE, Marker::CIRCLE,
                                                                 Marker::SQUARE, Marker::CIRCLE}));
    const Gui::Test::SeriesReadback square = Gui::Test::readSeries(*test.widget().series().at(0));
    // JFreeChart's 6 x 6 pixel shapes, filled, at every point, without rocketplot's ring.
    EXPECT_EQ(square.markerSize, 6.0 / 0.85);
    EXPECT_EQ(square.markerRingWidth, 0.0);
    EXPECT_TRUE(square.markersAlwaysVisible);
    EXPECT_EQ(Gui::Test::readSeries(*test.widget().series().at(1)).markerSize, 6.0);
    test.plot().setShowPoints(false);
    EXPECT_FALSE(test.plot().showsPoints());
    EXPECT_EQ(seriesMarkers(test.widget()), (std::vector<Marker>(4, Marker::NONE)));
}

// JFreeChart's DefaultDrawingSupplier.createStandardSeriesShapes() (1.5.6, OpenRocket's), in its
// order: Rectangle2D(-3, -3, 6, 6), Ellipse2D(-3, -3, 6, 6), the polygon (0, -3), (3, 3), (-3, 3)
// (y down: pointing up), the diamond (0, -3), (3, 0), (0, 3), (-3, 0), Rectangle2D(-3, -1.5, 6, 3),
// the polygon (-3, -3), (3, -3), (0, 3) (down), Ellipse2D(-3, -1.5, 6, 3), the polygon (-3, -3),
// (3, 0), (-3, 3) (right), Rectangle2D(-1.5, -3, 3, 6) and the polygon (-3, 0), (3, -3), (3, 3)
// (left); Plot's renderer takes DEFAULT_SHAPE_SEQUENCE[branch % 10].
TEST_F(SimulationPlotTest, PointShapesAreJFreeChartsTenThenRepeat)
{
    using rocketplot::Marker;
    EXPECT_EQ(DataPlot::pointShape(0), Marker::SQUARE);
    EXPECT_EQ(DataPlot::pointShape(1), Marker::CIRCLE);
    EXPECT_EQ(DataPlot::pointShape(2), Marker::TRIANGLE);
    EXPECT_EQ(DataPlot::pointShape(3), Marker::DIAMOND);
    EXPECT_EQ(DataPlot::pointShape(4), Marker::RECTANGLE_WIDE);
    EXPECT_EQ(DataPlot::pointShape(5), Marker::TRIANGLE_DOWN);
    EXPECT_EQ(DataPlot::pointShape(6), Marker::ELLIPSE_WIDE);
    EXPECT_EQ(DataPlot::pointShape(7), Marker::TRIANGLE_RIGHT);
    EXPECT_EQ(DataPlot::pointShape(8), Marker::RECTANGLE_TALL);
    EXPECT_EQ(DataPlot::pointShape(9), Marker::TRIANGLE_LEFT);
    EXPECT_EQ(DataPlot::pointShape(10), Marker::SQUARE);
    EXPECT_EQ(DataPlot::pointShape(13), Marker::DIAMOND);
    // 6 pixels across: the long side of the square and the rectangles is 0.85 of their size.
    EXPECT_EQ(DataPlot::pointSize(Marker::SQUARE), 6.0 / 0.85);
    EXPECT_EQ(DataPlot::pointSize(Marker::RECTANGLE_WIDE), 6.0 / 0.85);
    EXPECT_EQ(DataPlot::pointSize(Marker::RECTANGLE_TALL), 6.0 / 0.85);
    EXPECT_EQ(DataPlot::pointSize(Marker::CIRCLE), 6.0);
    EXPECT_EQ(DataPlot::pointSize(Marker::ELLIPSE_WIDE), 6.0);
    EXPECT_EQ(DataPlot::pointSize(Marker::TRIANGLE), 6.0);
    EXPECT_EQ(DataPlot::pointSize(Marker::TRIANGLE_LEFT), 6.0);
}

TEST_F(SimulationPlotTest, ElevenStagesHaveTheTenShapesThenTheFirstAgain)
{
    const PlotUnderTest test(manyStagesModel(11), true);
    using rocketplot::Marker;
    const std::vector<Marker> shapes{
        Marker::SQUARE,         Marker::CIRCLE,        Marker::TRIANGLE,     Marker::DIAMOND,
        Marker::RECTANGLE_WIDE, Marker::TRIANGLE_DOWN, Marker::ELLIPSE_WIDE, Marker::TRIANGLE_RIGHT,
        Marker::RECTANGLE_TALL, Marker::TRIANGLE_LEFT, Marker::SQUARE};
    EXPECT_EQ(seriesMarkers(test.widget()), shapes);
    constexpr double kLong = 6.0 / 0.85;
    EXPECT_EQ(seriesMarkerSizes(test.widget()),
              (std::vector<double>{kLong, 6.0, 6.0, 6.0, kLong, 6.0, 6.0, 6.0, kLong, 6.0, kLong}));
}

// --------------------------------------------------------------------------------------- legend

// Port of SimulationChartTest.simulationLegendHighlightsAllBranchesOfMatchingDataSeries: the
// branches of a data type share one legend entry, which acts on all of them. OpenRocket's
// highlight (the series three times as wide) is rocketplot's here (U-M4-6): pointing at the entry
// brings its series forward, clicking it hides or shows them, every branch's together.
TEST_F(SimulationPlotTest, ALegendEntryStandsForEveryBranchOfItsDataType)
{
    const auto          data = std::make_shared<const FlightData>(std::initializer_list{
        plotBranch("Sustainer", {0, 1}, {PlotColumn(Id::TYPE_ALTITUDE, {100, 110})}),
        plotBranch("Booster", {0, 1}, {PlotColumn(Id::TYPE_ALTITUDE, {50, 60})})});
    const PlotUnderTest test(
        Model("Test", data, plotConfiguration(Id::TYPE_TIME, {PlotRow(Id::TYPE_ALTITUDE, 0)})));
    rocketplot::PlotWidget& plot = test.widget();
    static_cast<void>(plot.grab());

    const std::vector<Gui::Test::LegendEntryReadback> entries = Gui::Test::readLegend(plot);
    ASSERT_EQ(entries.size(), 1U);
    EXPECT_EQ(entries[0].name, rocketplot::literalText(QStringLiteral("Altitude (m)")));
    EXPECT_EQ(entries[0].series, (std::vector<std::size_t>{0, 1}));
    const QList<rocketplot::Legend::Entry> shown = plot.legend()->entries();
    ASSERT_TRUE(shown.front().isShown());

    QTest::mouseClick(&plot, Qt::LeftButton, Qt::NoModifier, shown.front().rect.center().toPoint());
    EXPECT_EQ(seriesVisibility(plot), (std::vector<bool>{false, false}));
    QTest::mouseClick(&plot, Qt::LeftButton, Qt::NoModifier, shown.front().rect.center().toPoint());
    EXPECT_EQ(seriesVisibility(plot), (std::vector<bool>{true, true}));
}

TEST_F(SimulationPlotTest, TheLegendListsTheRowsAxisByAxisUnderThePlot)
{
    const PlotUnderTest test(
        Model("Simulation 1", twoStages(),
              plotConfiguration(Id::TYPE_TIME,
                                {PlotRow(Id::TYPE_VELOCITY_Z, 1), PlotRow(Id::TYPE_ALTITUDE, 0)})));
    const std::vector<Gui::Test::LegendEntryReadback> entries =
        Gui::Test::readLegend(test.widget());
    ASSERT_EQ(entries.size(), 2U);
    // The left axis' rows first (Plot.LegendItems), each entry its row's.
    EXPECT_EQ(entries[0].name, rocketplot::literalText(QStringLiteral("Altitude (m)")));
    EXPECT_EQ(entries[1].name, rocketplot::literalText(QStringLiteral("Vertical velocity (m/s)")));
    EXPECT_EQ(entries[1].series, (std::vector<std::size_t>{2, 3}));
    const rocketplot::Legend& legend = *test.widget().legend();
    EXPECT_TRUE(legend.isVisible());
    EXPECT_EQ(legend.anchor(), rocketplot::LegendAnchor::OUTSIDE_BOTTOM);
    EXPECT_FALSE(legend.areValuesVisible());
}

// ---------------------------------------------------------------------------------- titles, axes

TEST_F(SimulationPlotTest, TitlesAndLabelsAreTheModels)
{
    const PlotUnderTest           test(twoStagesModel());
    const Gui::Test::PlotReadback read = Gui::Test::readPlot(test.widget());
    EXPECT_EQ(read.title, rocketplot::literalText(QStringLiteral("Simulation 1")));
    EXPECT_EQ(read.subtitle, rocketplot::literalText(QStringLiteral("Test")));
    EXPECT_EQ(read.xAxis.label, rocketplot::literalText(QStringLiteral("Time (s)")));
    EXPECT_EQ(read.yAxis.label, rocketplot::literalText(QStringLiteral("Altitude")));
    EXPECT_EQ(read.yAxis2.label, rocketplot::literalText(QStringLiteral("Vertical velocity")));
    EXPECT_TRUE(read.yAxis2Shown);
    EXPECT_TRUE(read.xAxis.gridVisible);
    EXPECT_TRUE(read.yAxis.gridVisible);
    EXPECT_FALSE(read.yAxis2.gridVisible);
}

TEST_F(SimulationPlotTest, ARightAxisWithoutSeriesIsNotShown)
{
    PlotUnderTest test(Model("Simulation 1", twoStages(),
                             plotConfiguration(Id::TYPE_TIME, {PlotRow(Id::TYPE_ALTITUDE, 0),
                                                               PlotRow(Id::TYPE_VELOCITY_Z, 0)})));
    const Gui::Test::PlotReadback read = Gui::Test::readPlot(test.widget());
    EXPECT_FALSE(read.yAxis2Shown);
    EXPECT_EQ(read.yAxis.label,
              rocketplot::literalText(QStringLiteral("Altitude; Vertical velocity")));
    EXPECT_FALSE(test.plot().model().getYAxes()[1].hasSeries);
}

TEST_F(SimulationPlotTest, TheAxesShowTheFittedRanges)
{
    PlotUnderTest                 test(twoStagesModel());
    const Model&                  model = test.plot().model();
    const rocketplot::PlotWidget& plot  = test.widget();
    // Plot.PresetNumberAxis: the domain's data range with JFreeChart's half unit on either side,
    // the y axes' fitAxes() ranges, to the last bit.
    EXPECT_TRUE(showsRange(*plot.xAxis(), model.getDomainRange()));
    EXPECT_EQ(plot.xAxis()->min(), -0.5);
    EXPECT_EQ(plot.xAxis()->max(), 3.5);
    EXPECT_TRUE(showsRange(*plot.yAxis(), model.getYAxes()[0].range));
    EXPECT_TRUE(showsRange(*plot.yAxis2(), model.getYAxes()[1].range));
}

// The axis fitting widens a flat series' range by 1 on either side (PlotConfiguration.fitAxes():
// MathUtil.equals(min, max)), then by 3 %: for values beyond about 1e13 that range is too narrow
// for rocketplot to tell its ends apart (Axis::setHomeRange() would warn, which fails the test).
// The axis then has no home range and fits the data, as the plot did before rocketplot had home
// ranges; nearer to zero the range is the home range, to the last bit.
TEST_F(SimulationPlotTest, AFlatSeriesFarFromZeroFitsItsData)
{
    PlotUnderTest far(flatModel(1e14));
    const Model&  model = far.plot().model();
    EXPECT_EQ(model.getYAxes()[0].range.max - model.getYAxes()[0].range.min, 2.125);
    EXPECT_FALSE(far.widget().yAxis()->homeRange().has_value());
    EXPECT_LT(far.widget().yAxis()->min(), 1e14);
    EXPECT_GT(far.widget().yAxis()->max(), 1e14);
    EXPECT_TRUE(showsRange(*far.widget().xAxis(), model.getDomainRange()));

    PlotUnderTest nearer(flatModel(1e12));
    EXPECT_TRUE(nearer.widget().yAxis()->homeRange().has_value());
    EXPECT_TRUE(showsRange(*nearer.widget().yAxis(), nearer.plot().model().getYAxes()[0].range));
}

TEST_F(SimulationPlotTest, TheZoomButtonsZoomAboutTheMiddleAndResetComesHome)
{
    PlotUnderTest                 test(twoStagesModel());
    const rocketplot::PlotWidget& plot  = test.widget();
    const Model&                  model = test.plot().model();
    // ChartPanel's zoom in: half the ranges, about their middles.
    test.plot().zoomIn(false);
    EXPECT_DOUBLE_EQ(plot.xAxis()->min(), 0.5);
    EXPECT_DOUBLE_EQ(plot.xAxis()->max(), 2.5);
    EXPECT_FALSE(showsRange(*plot.yAxis(), model.getYAxes()[0].range));
    test.plot().resetZoom();
    EXPECT_TRUE(showsRange(*plot.xAxis(), model.getDomainRange()));
    EXPECT_TRUE(showsRange(*plot.yAxis(), model.getYAxes()[0].range));
    EXPECT_TRUE(showsRange(*plot.yAxis2(), model.getYAxes()[1].range));
    // With Alt: x alone. Zoomed out beyond the home ranges (rocketplot's N17: OpenRocket refuses).
    test.plot().zoomOut(true);
    EXPECT_DOUBLE_EQ(plot.xAxis()->min(), -2.5);
    EXPECT_DOUBLE_EQ(plot.xAxis()->max(), 5.5);
    EXPECT_TRUE(showsRange(*plot.yAxis(), model.getYAxes()[0].range));
    test.plot().zoomIn(true);
    test.plot().resetZoom();
    EXPECT_TRUE(showsRange(*plot.xAxis(), model.getDomainRange()));
}

TEST_F(SimulationPlotTest, AGrayLineMarksZeroOnTheLeftAxis)
{
    const PlotUnderTest                  test(twoStagesModel());
    const Gui::Test::AnnotationsReadback read = Gui::Test::readAnnotations(test.widget());
    ASSERT_EQ(read.lines.size(), 1U);
    QColor gray = QColor::fromRgb(128, 128, 128);
    gray.setAlphaF(0.8F);
    // JFreeChart's ValueMarker(0): Color.GRAY, a 0.5 pixel stroke, alpha 0.8.
    EXPECT_EQ(read.lines[0], (Gui::Test::ReferenceLineReadback{.orientation    = Qt::Horizontal,
                                                               .value          = 0.0,
                                                               .label          = {},
                                                               .color          = gray,
                                                               .lineWidth      = 0.5,
                                                               .lineStyle      = Qt::SolidLine,
                                                               .secondaryYAxis = false,
                                                               .visible        = true}));
}

TEST_F(SimulationPlotTest, TheGesturesAndTheReadoutAreSimulationChartsOnes)
{
    PlotUnderTest                 test(twoStagesModel());
    const rocketplot::PlotWidget& plot = test.widget();
    EXPECT_EQ(plot.inputBindings(), PlotInputBindings::simulationChart());
    EXPECT_TRUE(plot.isCrosshairEnabled());
    EXPECT_EQ(plot.crosshairMode(), rocketplot::CrosshairMode::SNAP);
    EXPECT_EQ(test.plot().objectName(), QStringLiteral("chartPanel"));
    EXPECT_EQ(plot.objectName(), QStringLiteral("chart"));
}

// --------------------------------------------------------------------------------------- events

TEST_F(SimulationPlotTest, NoEventIsDrawnUntilAskedFor)
{
    PlotUnderTest test(longFlightModel());
    // OpenRocket's plot dialog calls setShowEvents() at once; the plot alone draws none.
    EXPECT_TRUE(test.plot().eventLines().empty());
    EXPECT_TRUE(test.plot().eventIcons().empty());
    EXPECT_TRUE(showsRange(*test.widget().xAxis(), test.plot().model().getDomainRange()));
}

TEST_F(SimulationPlotTest, OverTimeEachGroupOfEventsIsALineWithAFlag)
{
    PlotUnderTest test(longFlightModel());
    test.plot().setShowEvents(true);
    const Model&                          model  = test.plot().model();
    const std::vector<Model::EventGroup>& groups = model.getEventGroups();
    const Gui::Test::AnnotationsReadback  read   = Gui::Test::readAnnotations(test.widget());
    ASSERT_EQ(groups.size(), 5U);
    ASSERT_EQ(read.events.size(), groups.size());
    EXPECT_EQ(read.events, expectedEventLines(test.plot()));
    // The apogee and the deployment 30 ms later: one group, at the apogee, looking like the
    // deployment (its type's ordinal is the higher).
    EXPECT_EQ(read.events[2].x, 30.0);
    EXPECT_EQ(read.events[2].label,
              rocketplot::literalText(QStringLiteral("Apogee, Recovery device deployment")));
    EXPECT_EQ(read.events[2].color.rgb(), qRgb(0, 0, 128));
    // A warning's flag carries its priority's image.
    EXPECT_EQ(read.events[3].icon, Gui::Test::iconImage(PlotEventGraphics::eventIcon(
                                       Event::SIM_WARN, MessagePriority::HIGH)));
    EXPECT_TRUE(read.icons.empty());
    EXPECT_EQ(test.widget().eventFlagPlacement(), rocketplot::FlagPlacement::ABOVE_PLOT_AREA);
}

TEST_F(SimulationPlotTest, TheEventLinesWidenTheDomainOnceAndKeepItsLabel)
{
    PlotUnderTest                 test(longFlightModel());
    const rocketplot::PlotWidget& plot  = test.widget();
    const Model&                  model = test.plot().model();
    const QString                 label = plot.xAxis()->label();
    test.plot().setShowEvents(true);
    // The ground hit at 59.9 s is within 1 % of the upper bound 60.5: the domain goes to 59.9 s
    // plus 1 % of 60.5 s, and the axis keeps its label (OpenRocket bug 43).
    EXPECT_TRUE(showsRange(*plot.xAxis(), model.getDomainRangeWithEventLines()));
    EXPECT_GT(plot.xAxis()->max(), model.getDomainRange().max);
    EXPECT_EQ(plot.xAxis()->label(), label);
    EXPECT_EQ(label, rocketplot::literalText(QStringLiteral("Time (s)")));
    // Hidden and drawn again, of a stage and of all: never widened again.
    test.plot().setShowEvents(false);
    test.plot().setShowBranch(0);
    test.plot().setShowBranch(std::nullopt);
    test.plot().setShowEvents(true);
    test.plot().resetZoom();
    EXPECT_TRUE(showsRange(*plot.xAxis(), model.getDomainRangeWithEventLines()));
}

TEST_F(SimulationPlotTest, HiddenEventsGoAndComeBack)
{
    PlotUnderTest test(longFlightModel());
    test.plot().setShowEvents(true);
    test.plot().setShowEvents(false);
    EXPECT_TRUE(test.plot().eventLines().empty());
    EXPECT_TRUE(Gui::Test::readAnnotations(test.widget()).events.empty());
    test.plot().setShowEvents(true);
    EXPECT_EQ(eventTimes(test.widget()), (std::vector<double>{0.0, 1.5, 30.0, 40.0, 59.9}));
    EXPECT_EQ(test.plot().eventLines().size(), 5U);
}

TEST_F(SimulationPlotTest, OverAnotherDomainTheEventsAreIconsOnTheCurves)
{
    PlotUnderTest test(longFlightModel(Id::TYPE_ALTITUDE));
    test.plot().setShowEvents(true);
    const std::vector<Model::EventIcon>  icons = test.plot().model().getIconMarkers();
    const Gui::Test::AnnotationsReadback read  = Gui::Test::readAnnotations(test.widget());
    EXPECT_TRUE(read.events.empty());
    ASSERT_FALSE(icons.empty());
    ASSERT_EQ(read.icons.size(), icons.size());
    EXPECT_EQ(read.icons, expectedEventIcons(test.plot()));
    EXPECT_EQ(test.plot().eventIcons().size(), icons.size());
}

TEST_F(SimulationPlotTest, TheIconMarkerStyleDrawsIconsOverTimeToo)
{
    PlotUnderTest test(longFlightModel());
    PlotPreferences::setMarkerStyleIcon(test.services().preferences(), true);
    test.plot().setShowEvents(true);
    EXPECT_TRUE(test.plot().eventLines().empty());
    EXPECT_EQ(test.plot().eventIcons().size(), test.plot().model().getIconMarkers().size());
    // No line, no widening.
    EXPECT_TRUE(showsRange(*test.widget().xAxis(), test.plot().model().getDomainRange()));
    // The preference is read at each drawing, as Java reads it.
    PlotPreferences::setMarkerStyleIcon(test.services().preferences(), false);
    test.plot().setShowEvents(true);
    EXPECT_TRUE(test.plot().eventIcons().empty());
    EXPECT_EQ(test.plot().eventLines().size(), 5U);
}

// ---------------------------------------------------------------------------------------- stages

TEST_F(SimulationPlotTest, AStageShowsItsSeriesAndItsEvents)
{
    PlotUnderTest test(twoStagesModel());
    test.plot().setShowBranch(1);
    EXPECT_EQ(test.plot().shownBranch(), std::optional<std::size_t>(1));
    EXPECT_EQ(seriesVisibility(test.widget()), (std::vector<bool>{false, true, false, true}));
    test.plot().setShowBranch(std::nullopt);
    EXPECT_EQ(seriesVisibility(test.widget()), (std::vector<bool>(4, true)));
    EXPECT_THROW(test.plot().setShowBranch(2), BugError);
}

// Kept as Java: SimulationPlotDialog's stage choice draws the stage's events whatever Show events
// says, and Show events draws every stage's.
TEST_F(SimulationPlotTest, AStageDrawsItsEventsAsOpenRocketDoes)
{
    const auto    data = std::make_shared<const FlightData>(std::initializer_list{
        plotBranch("Sustainer", {0, 1, 2}, {PlotColumn(Id::TYPE_ALTITUDE, {0, 10, 20})},
                   {FlightEvent(Event::APOGEE, 2.0)}),
        plotBranch(
            "Booster", {0, 1}, {PlotColumn(Id::TYPE_ALTITUDE, {0, 5})},
            {FlightEvent(Event::STAGE_SEPARATION, 0.5), FlightEvent(Event::GROUND_HIT, 1.0)})});
    PlotUnderTest test(
        Model("Stages", data,
              plotConfiguration(Id::TYPE_TIME, {PlotRow(Id::TYPE_ALTITUDE, 0)},
                                {Event::APOGEE, Event::STAGE_SEPARATION, Event::GROUND_HIT})));
    test.plot().setShowEvents(false);
    test.plot().setShowBranch(1);
    EXPECT_EQ(eventTimes(test.widget()), (std::vector<double>{0.5, 1.0}));
    test.plot().setShowEvents(true);
    EXPECT_EQ(eventTimes(test.widget()), (std::vector<double>{0.5, 1.0, 2.0}));
    test.plot().setShowBranch(0);
    EXPECT_EQ(eventTimes(test.widget()), (std::vector<double>{2.0}));
}

// ---------------------------------------------------------------------------------------- aborts

TEST_F(SimulationPlotTest, TheAbortBoxListsTheAbortsAtTheBottomLeft)
{
    PlotUnderTest                        test(abortedModel());
    const Gui::Test::AnnotationsReadback read = Gui::Test::readAnnotations(test.widget());
    ASSERT_EQ(read.texts.size(), 1U);
    const QString text = QtStrings::fromUtf8(test.plot().model().getAbortText());
    EXPECT_TRUE(text.startsWith(QStringLiteral("Simulation Abort\nStage: Sustainer; Time: 2.50 s")))
        << QtStrings::toUtf8(text);
    // The text and its frame in the error colour (TextTitle's paint, BlockBorder's).
    const QColor error = ThemeColors::color(ThemeColors::Key::ERROR, test.plot().palette());
    EXPECT_EQ(read.texts[0],
              (Gui::Test::TextReadback{.position     = QPointF(0.01, 0.01),
                                       .text         = rocketplot::literalText(text),
                                       .color        = error,
                                       .coordinates  = rocketplot::AnnotationCoordinates::PLOT_AREA,
                                       .alignment    = Qt::AlignLeft | Qt::AlignBottom,
                                       .arrowVisible = false,
                                       .frameVisible = true,
                                       .frameColor   = error,
                                       .visible      = true}));
    EXPECT_TRUE(test.plot().showsErrors());
}

// ErrorAnnotationSet's TextTitle has a line for each line of its text: the title, then one per
// aborted stage, each drawn as written.
TEST_F(SimulationPlotTest, TheAbortBoxHasALineForEachLineOfItsText)
{
    PlotUnderTest     test(abortedModel());
    const QString     text  = QtStrings::fromUtf8(test.plot().model().getAbortText());
    const QStringList lines = text.split(QLatin1Char('\n'));
    ASSERT_EQ(lines.size(), 3);
    EXPECT_EQ(lines[0], QStringLiteral("Simulation Abort"));
    EXPECT_TRUE(lines[2].startsWith(QStringLiteral("Stage: Payload; Time: 1.25 s")))
        << QtStrings::toUtf8(lines[2]);
    EXPECT_EQ(linesNotDrawn(test.widget(), text), std::vector<std::string>());
}

// A stage's name in the abort box is shown with the characters of markup and math it has.
TEST_F(SimulationPlotTest, TheAbortBoxShowsAStagesNameAsWritten)
{
    PlotUnderTest test(abortedStageModel("Stage <A> & $5$"));
    const QString text = QtStrings::fromUtf8(test.plot().model().getAbortText());
    EXPECT_EQ(abortBoxText(test.plot()), std::optional(rocketplot::literalText(text)));
    EXPECT_TRUE(
        text.startsWith(QStringLiteral("Simulation Abort\nStage: Stage <A> & $5$; Time: 1.50 s")))
        << QtStrings::toUtf8(text);
    EXPECT_EQ(linesNotDrawn(test.widget(), text), std::vector<std::string>());
}

TEST_F(SimulationPlotTest, TheAbortBoxStaysWithTheEventsOnOrOff)
{
    PlotUnderTest test(abortedModel());
    // OpenRocket bug 44: the plot dialog's setShowEvents() at its open cleared the box.
    test.plot().setShowEvents(true);
    EXPECT_EQ(abortBoxVisible(test.plot()), std::optional(true));
    test.plot().setShowEvents(false);
    EXPECT_EQ(abortBoxVisible(test.plot()), std::optional(true));
}

TEST_F(SimulationPlotTest, ShowErrorsShowsAndHidesTheAbortBox)
{
    PlotUnderTest test(abortedModel());
    test.plot().setShowErrors(false);
    EXPECT_FALSE(test.plot().showsErrors());
    EXPECT_EQ(abortBoxVisible(test.plot()), std::optional(false));
    // A stage chosen while hidden keeps it hidden.
    test.plot().setShowBranch(2);
    EXPECT_EQ(abortBoxVisible(test.plot()), std::optional(false));
    test.plot().setShowErrors(true);
    EXPECT_EQ(abortBoxVisible(test.plot()), std::optional(true));
}

TEST_F(SimulationPlotTest, AStageShowsItsOwnAbortBox)
{
    PlotUnderTest test(abortedModel());
    const Model&  model = test.plot().model();
    test.plot().setShowBranch(2);
    EXPECT_EQ(abortBoxText(test.plot()),
              std::optional(rocketplot::literalText(QtStrings::fromUtf8(model.getAbortText(2)))));
    // The booster has no abort: no box.
    test.plot().setShowBranch(1);
    EXPECT_EQ(test.plot().abortBox(), nullptr);
    EXPECT_TRUE(Gui::Test::readAnnotations(test.widget()).texts.empty());
    test.plot().setShowBranch(std::nullopt);
    EXPECT_EQ(abortBoxText(test.plot()),
              std::optional(rocketplot::literalText(QtStrings::fromUtf8(model.getAbortText()))));
}

TEST_F(SimulationPlotTest, WithoutAnAbortThereIsNoBox)
{
    PlotUnderTest test(twoStagesModel());
    EXPECT_EQ(test.plot().abortBox(), nullptr);
    EXPECT_TRUE(Gui::Test::readAnnotations(test.widget()).texts.empty());
}

// ---------------------------------------------------------------------------------------- colours

TEST_F(SimulationPlotTest, ADarkPaletteBrightensTheEventsAndTheErrors)
{
    PlotUnderTest test(longFlightModel());
    test.plot().setShowEvents(true);
    const PlotUnderTest aborted(abortedModel());
    {
        const Palette dark(Palette::dark());
        QCoreApplication::sendPostedEvents();
        const Gui::Test::AnnotationsReadback read = Gui::Test::readAnnotations(test.widget());
        ASSERT_EQ(read.events.size(), 5U);
        // The apogee's group looks like the deployment: navy, twice brightened.
        EXPECT_EQ(read.events[2].color, PlotEventGraphics::markerColor(
                                            Event::RECOVERY_DEVICE_DEPLOYMENT, Palette::dark()));
        EXPECT_EQ(read.events[2].color.rgb(), qRgb(0, 0, 255));
        const Gui::Test::AnnotationsReadback abortRead =
            Gui::Test::readAnnotations(aborted.widget());
        ASSERT_EQ(abortRead.texts.size(), 1U);
        EXPECT_EQ(abortRead.texts.front().color,
                  ThemeColors::themeColor(ThemeColors::Key::ERROR, ThemeColors::Theme::DARK));
        EXPECT_EQ(abortRead.texts.front().frameColor, abortRead.texts.front().color);
    }
    QCoreApplication::sendPostedEvents();
    const Gui::Test::AnnotationsReadback read = Gui::Test::readAnnotations(test.widget());
    ASSERT_EQ(read.events.size(), 5U);
    EXPECT_EQ(read.events[2].color.rgb(), qRgb(0, 0, 128));
    const Gui::Test::AnnotationsReadback abortRead = Gui::Test::readAnnotations(aborted.widget());
    ASSERT_EQ(abortRead.texts.size(), 1U);
    EXPECT_EQ(abortRead.texts.front().color,
              ThemeColors::themeColor(ThemeColors::Key::ERROR, ThemeColors::Theme::LIGHT));
    EXPECT_EQ(abortRead.texts.front().frameColor, abortRead.texts.front().color);
}

// --------------------------------------------------------------------------------------- readout

TEST_F(SimulationPlotTest, TheReadoutShowsTheToolTipOfThePointUnderTheCrosshair)
{
    PlotUnderTest                      test(twoStagesModel());
    const Gui::Test::CrosshairReadback crosshair =
        Gui::Test::moveCrosshairToPoint(test.widget(), 0, 2, QPointF(2.0, 1.0));
    EXPECT_EQ(crosshair, (Gui::Test::CrosshairReadback{.series = 0, .point = 2}));
    const QString expected = QtStrings::fromUtf8(test.plot().model().getToolTip(0, 2).toHtml());
    EXPECT_EQ(test.plot().readout(), expected);
    ASSERT_EQ(test.readout().texts->size(), 1U);
    EXPECT_EQ(test.readout().texts->back(), expected);
    // The sustainer's series is named with its stage (getNameBasedOnIdxAndSeries()).
    EXPECT_EQ(expected,
              QStringLiteral("<html><b><i>Sustainer: Altitude (m)</i></b><br>Altitude: 20 m<br>"
                             "Time: 2 s<br></html>"));
}

TEST_F(SimulationPlotTest, TheReadoutGoesWhenTheCrosshairLeavesTheData)
{
    PlotUnderTest test(twoStagesModel());
    static_cast<void>(Gui::Test::moveCrosshairToPoint(test.widget(), 2, 1));
    ASSERT_FALSE(test.plot().readout().isEmpty());
    // Far from every point: the crosshair follows the pointer, the tool tip goes.
    const Gui::Test::CrosshairReadback away = Gui::Test::moveCrosshairTo(
        test.widget(), test.widget().plotArea().topLeft() + QPointF(1, 1));
    EXPECT_FALSE(away.series.has_value());
    EXPECT_TRUE(test.plot().readout().isEmpty());
    ASSERT_EQ(test.readout().texts->size(), 2U);
    EXPECT_TRUE(test.readout().texts->back().isEmpty());
    // Off the data again: nothing more to hide.
    static_cast<void>(Gui::Test::moveCrosshairTo(
        test.widget(), test.widget().plotArea().topLeft() + QPointF(2, 1)));
    EXPECT_EQ(test.readout().texts->size(), 2U);
}

// ------------------------------------------------------------------------------- the simulation

TEST_F(SimulationPlotTest, ASimulationIsPlottedUnderItsName)
{
    const Gui::Test::ExampleDesign    design(Gui::Test::ExampleDesign::kSimpleModelRocket);
    const std::shared_ptr<Simulation> simulation = design.simulation(0);
    ASSERT_NE(simulation, nullptr);
    const Gui::Test::TestGuiServices services;
    SimulationPlotConfiguration      preset =
        SimulationPlotConfiguration::defaultConfigurations().front();
    preset.resetUnits();
    const SimulationPlot plot(services.services(), *simulation, preset, false);
    EXPECT_EQ(plot.model().getTitle(), simulation->getName());
    EXPECT_EQ(plot.plotWidget().title(),
              rocketplot::literalText(QtStrings::fromUtf8(simulation->getName())));
    EXPECT_EQ(plot.plotWidget().subtitle(),
              rocketplot::literalText(QStringLiteral("Vertical motion vs. time")));
    // The model shares the simulation's data.
    EXPECT_EQ(&plot.model().getData(), simulation->getSimulatedData().get());
}

TEST_F(SimulationPlotTest, ASimulationWithoutDataIsABug)
{
    Gui::Test::ExampleDesign          design(Gui::Test::ExampleDesign::kSimpleModelRocket);
    const std::shared_ptr<Simulation> copy = design.addCopyOf(0, "Not simulated");
    ASSERT_NE(copy, nullptr);
    ASSERT_EQ(copy->getSimulatedData(), nullptr);
    const Gui::Test::TestGuiServices services;
    EXPECT_THROW(
        SimulationPlot(services.services(), *copy,
                       SimulationPlotConfiguration::defaultConfigurations().front(), false),
        BugError);
}

TEST_F(SimulationPlotTest, ExportImageAsksForTheFileAndWritesIt)
{
    PlotUnderTest test(twoStagesModel());
    test.services().prompts().answerSaveFile(std::nullopt);
    UserPrompts::SaveFileRequest request;
    request.title       = QStringLiteral("Export Image");
    request.nameFilters = QStringList{QStringLiteral("PNG image (*.png)")};
    EXPECT_FALSE(test.plot().exportImage(request));
    ASSERT_EQ(test.services().prompts().saveFileRequests().size(), 1U);
    EXPECT_EQ(test.services().prompts().saveFileRequests().front().title,
              QStringLiteral("Export Image"));
}

}  // namespace QtRocket::Gui
