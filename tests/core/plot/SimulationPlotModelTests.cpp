#include "QtRocket/plot/SimulationPlotModel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/plot/PlotAxisBounds.h"
#include "QtRocket/plot/SimulationPlotConfiguration.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/LineStyle.h"
#include "QtRocket/util/Strings.h"
#include "goldens/GoldenExamples.h"
#include "unit/DefaultUnitsGuard.h"

// The first tests build small flight data by hand; the last ones plot stored simulations of the
// examples and pin what OpenRocket's SimulationPlot gives for them (the plot sections of
// tools/openrocket-probes/plot-model's expected.txt: the series, the readout tool tips, the
// event groups and the icons replayed in eventList order, each group at its first event, which is
// QtRocket's fix of bug 41; the colours the panel shows, which are QtRocket's series colours, its
// fix of bug 42).

namespace
{

using QtRocket::BugError;
using QtRocket::Color;
using QtRocket::FlightData;
using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::FlightEvent;
using QtRocket::LineStyle;
using QtRocket::MessagePriority;
using QtRocket::SimulationAbort;
using QtRocket::SimulationPlotConfiguration;
using QtRocket::Unit;
using QtRocket::Warning;
using QtRocket::Test::DefaultUnitsGuard;
using Id    = QtRocket::FlightDataTypeId;
using Event = QtRocket::FlightEvent::Type;
using Model = QtRocket::SimulationPlotModel;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

[[nodiscard]] const FlightDataType& type(Id id)
{
    return FlightDataType::builtin(id);
}

/// A column of a hand-made branch: a type and its value at each time.
struct Column
{
    Column(Id columnType, std::vector<double> columnValues)
      : type(columnType), values(std::move(columnValues))
    {
    }

    Id                  type;
    std::vector<double> values;
};

/// A branch named @p name with the times @p times and @p columns (each as long as @p times), and
/// @p events.
[[nodiscard]] std::shared_ptr<FlightDataBranch> makeBranch(std::string                name,
                                                           const std::vector<double>& times,
                                                           const std::vector<Column>& columns,
                                                           std::vector<FlightEvent>   events = {})
{
    std::vector<const FlightDataType*> types{&type(Id::TYPE_TIME)};
    for (const Column& column : columns)
    {
        types.push_back(&type(column.type));
    }
    auto branch = std::make_shared<FlightDataBranch>(std::move(name), types);
    for (std::size_t i = 0; i < times.size(); ++i)
    {
        branch->addPoint();
        branch->setValue(type(Id::TYPE_TIME), times[i]);
        for (const Column& column : columns)
        {
            branch->setValue(type(column.type), column.values.at(i));
        }
    }
    for (FlightEvent& event : events)
    {
        branch->addEvent(std::move(event));
    }
    return branch;
}

[[nodiscard]] std::shared_ptr<const FlightData> flightData(
    std::initializer_list<std::shared_ptr<FlightDataBranch>> branches)
{
    return std::make_shared<FlightData>(branches);
}

/// A row of a hand-made configuration.
struct RowSpec
{
    RowSpec(Id rowType, int rowAxis) : type(rowType), axis(rowAxis) { }

    Id  type;
    int axis;
};

/// A configuration named "Test" over @p domain with @p rows and the events @p events.
[[nodiscard]] SimulationPlotConfiguration configuration(Id                             domain,
                                                        std::initializer_list<RowSpec> rows,
                                                        std::initializer_list<Event>   events = {})
{
    SimulationPlotConfiguration config("Test", type(domain));
    for (const RowSpec& row : rows)
    {
        config.addPlotDataType(type(row.type), row.axis);
    }
    for (const Event event : events)
    {
        config.setEvent(event, true);
    }
    return config;
}

/// Two branches of four and three points with altitude and vertical velocity.
[[nodiscard]] std::shared_ptr<const FlightData> twoBranches()
{
    return flightData({makeBranch("Sustainer", {0, 1, 2, 3},
                                  {Column(Id::TYPE_ALTITUDE, {0, 10, 20, 15}),
                                   Column(Id::TYPE_VELOCITY_Z, {0, 10, 5, -5})}),
                       makeBranch("Booster", {0, 1, 2},
                                  {Column(Id::TYPE_ALTITUDE, {0, 5, 2}),
                                   Column(Id::TYPE_VELOCITY_Z, {0, 4, -3})})});
}

/// The series' names in order.
[[nodiscard]] std::vector<std::string> seriesNames(const Model& model)
{
    std::vector<std::string> names;
    names.reserve(model.getSeries().size());
    for (const Model::Series& series : model.getSeries())
    {
        names.push_back(series.name);
    }
    return names;
}

/// The colour index of each series.
[[nodiscard]] std::vector<int> seriesColors(const Model& model)
{
    std::vector<int> colors;
    colors.reserve(model.getSeries().size());
    for (const Model::Series& series : model.getSeries())
    {
        colors.push_back(series.colorIndex);
    }
    return colors;
}

/// "label/colour index" of each legend entry, joined by " | ".
[[nodiscard]] std::string legendText(const Model& model)
{
    std::string text;
    for (const Model::LegendEntry& entry : model.getLegend())
    {
        text += std::format("{}{}/c{}", text.empty() ? "" : " | ", entry.label, entry.colorIndex);
    }
    return text;
}

/// "type@time/b<branch>" of each event of @p group, joined by ' ', as the probe prints them.
[[nodiscard]] std::string groupEvents(const Model::EventGroup& group)
{
    std::string text;
    for (const Model::PlotEvent& info : group.events)
    {
        text += std::format(
            "{}{}@{}/b{}", text.empty() ? "" : " ", QtRocket::name(info.event.getType()),
            QtRocket::Strings::javaDoubleToString(info.event.getTime()), info.branch);
    }
    return text;
}

/// The same double, or both NaN.
[[nodiscard]] bool sameDouble(double a, double b)
{
    return (std::isnan(a) && std::isnan(b)) || a == b;
}

TEST(SimulationPlotModel, SeriesGoAxisByAxisRowByRowBranchByBranch)
{
    const DefaultUnitsGuard units;
    const Model             model("Simulation", twoBranches(),
                                  configuration(Id::TYPE_TIME, {RowSpec(Id::TYPE_ALTITUDE, 1),
                                                                RowSpec(Id::TYPE_VELOCITY_Z, 0)}));

    EXPECT_EQ(seriesNames(model), (std::vector<std::string>{
                                      "Vertical velocity (m/s)", "Booster: Vertical velocity (m/s)",
                                      "Altitude (m)", "Booster: Altitude (m)"}));
    const std::vector<Model::Series>& series = model.getSeries();
    ASSERT_EQ(series.size(), 4U);
    EXPECT_EQ(series[0].row, 1U);
    EXPECT_EQ(series[0].branch, 0U);
    EXPECT_EQ(series[0].axis, 0);
    EXPECT_EQ(series[0].label, "Vertical velocity (m/s)");
    EXPECT_EQ(series[0].unit, "m/s");
    EXPECT_EQ(series[1].branch, 1U);
    EXPECT_EQ(series[1].label, "Vertical velocity (m/s)");
    EXPECT_EQ(series[2].row, 0U);
    EXPECT_EQ(series[2].axis, 1);
    EXPECT_EQ(series[2].unit, "m");
    EXPECT_EQ(series[3].x, (std::vector<double>{0, 1, 2}));
    EXPECT_EQ(series[3].y, (std::vector<double>{0, 5, 2}));
    EXPECT_EQ(model.getTitle(), "Simulation");
    EXPECT_EQ(model.getSubtitle(), "Test");
    EXPECT_EQ(model.getBranchCount(), 2U);
    EXPECT_EQ(model.getSeriesLabels(), (std::vector<std::string>{"Sustainer", "Booster"}));
}

TEST(SimulationPlotModel, SeriesAreInTheUnitsOfTheConfiguration)
{
    const DefaultUnitsGuard     units;
    SimulationPlotConfiguration config =
        configuration(Id::TYPE_VELOCITY_Z, {RowSpec(Id::TYPE_ALTITUDE, 0)});
    const Unit* feet = type(Id::TYPE_ALTITUDE).getUnitGroup().getUnit("ft");
    const Unit* kmh  = type(Id::TYPE_VELOCITY_Z).getUnitGroup().getUnit("km/h");
    ASSERT_NE(feet, nullptr);
    ASSERT_NE(kmh, nullptr);
    config.setPlotDataUnit(0, *feet);
    config.setDomainAxisUnit(*kmh);
    const Model model("Simulation", twoBranches(), config);

    const Model::Series& series = model.getSeries().at(0);
    EXPECT_EQ(series.label, "Altitude (ft)");
    EXPECT_EQ(series.unit, "ft");
    EXPECT_EQ(series.y.at(1), feet->toUnit(10));
    EXPECT_EQ(series.x.at(1), kmh->toUnit(10));
    EXPECT_EQ(model.getDomainLabel(), "Vertical velocity (km/h)");
}

TEST(SimulationPlotModel, EachRowHasTheColourThePanelShows)
{
    // Bug 42: Java draws every row of an axis in its first row's colour when there are several
    // branches; the panel shows each row its own: left rows 0, 1, ..., right rows after the left
    // rows' series, 2 rows x 3 branches = 6.
    const DefaultUnitsGuard   units;
    const std::vector<double> times{0, 1};
    const std::vector<Column> columns{Column(Id::TYPE_ALTITUDE, {0, 1}),
                                      Column(Id::TYPE_VELOCITY_Z, {0, 2}),
                                      Column(Id::TYPE_ACCELERATION_Z, {0, 3})};
    const auto data = flightData({makeBranch("A", times, columns), makeBranch("B", times, columns),
                                  makeBranch("C", times, columns)});
    SimulationPlotConfiguration config = configuration(
        Id::TYPE_TIME, {RowSpec(Id::TYPE_ALTITUDE, 0), RowSpec(Id::TYPE_VELOCITY_Z, 1),
                        RowSpec(Id::TYPE_ACCELERATION_Z, 0)});
    config.setPlotDataColor(1, Color(1, 2, 3));
    config.setPlotDataLineStyle(2, LineStyle::DOTTED);
    const Model model("Simulation", data, config);

    EXPECT_EQ(seriesColors(model), (std::vector<int>{0, 0, 0, 1, 1, 1, 6, 6, 6}));
    EXPECT_EQ(Model::computeDefaultColorIndices(config, *data), (std::vector<int>{0, 6, 1}));
    EXPECT_EQ(legendText(model),
              "Altitude (m)/c0 | Vertical acceleration (m/s\xC2\xB2)/c1 | "
              "Vertical velocity (m/s)/c6");
    EXPECT_EQ(model.getSeries().at(6).color, Color(1, 2, 3));
    EXPECT_EQ(model.getSeries().at(0).color, std::nullopt);
    EXPECT_EQ(model.getSeries().at(4).lineStyle, LineStyle::DOTTED);
    EXPECT_EQ(model.getLegend().at(2).color, Color(1, 2, 3));
    EXPECT_EQ(model.getLegend().at(1).lineStyle, LineStyle::DOTTED);
}

TEST(SimulationPlotModel, AnEmptyBranchMisalignsNeitherTheLegendNorTheColours)
{
    // Bug 48: an empty branch has no series; Java's stepping by the branch count then pairs the
    // legend and the colours with the wrong series.
    const DefaultUnitsGuard   units;
    const std::vector<Column> columns{Column(Id::TYPE_ALTITUDE, {0, 1}),
                                      Column(Id::TYPE_VELOCITY_Z, {0, 2})};
    const std::vector<Column> noRows{Column(Id::TYPE_ALTITUDE, {}),
                                     Column(Id::TYPE_VELOCITY_Z, {})};
    const auto  data = flightData({makeBranch("A", {0, 1}, columns), makeBranch("B", {}, noRows),
                                   makeBranch("C", {0, 1}, columns)});
    const Model model("Simulation", data,
                      configuration(Id::TYPE_TIME, {RowSpec(Id::TYPE_ALTITUDE, 0),
                                                    RowSpec(Id::TYPE_VELOCITY_Z, 1)}));

    EXPECT_EQ(seriesNames(model),
              (std::vector<std::string>{"Altitude (m)", "C: Altitude (m)",
                                        "Vertical velocity (m/s)", "C: Vertical velocity (m/s)"}));
    EXPECT_EQ(model.getSeries().at(1).branch, 2U);
    EXPECT_EQ(model.getSeries().at(3).branch, 2U);
    EXPECT_EQ(seriesColors(model), (std::vector<int>{0, 0, 3, 3}));
    EXPECT_EQ(legendText(model), "Altitude (m)/c0 | Vertical velocity (m/s)/c3");
}

TEST(SimulationPlotModel, TheLegendNamesARowByItsLabelWhenTheFirstBranchIsEmpty)
{
    const DefaultUnitsGuard units;
    const auto  data = flightData({makeBranch("A", {}, {Column(Id::TYPE_ALTITUDE, {})}),
                                   makeBranch("B", {0, 1}, {Column(Id::TYPE_ALTITUDE, {0, 1})})});
    const Model model("Simulation", data,
                      configuration(Id::TYPE_TIME, {RowSpec(Id::TYPE_ALTITUDE, 0)}));

    EXPECT_EQ(seriesNames(model), (std::vector<std::string>{"B: Altitude (m)"}));
    EXPECT_EQ(legendText(model), "Altitude (m)/c0");
}

TEST(SimulationPlotModel, ABranchWithoutARowsTypeHasNoSeriesOfIt)
{
    // Bug 46: Java's plot fails there (NullPointerException).
    const DefaultUnitsGuard units;
    const auto              data = flightData(
        {makeBranch("A", {0, 1, 2},
                    {Column(Id::TYPE_ALTITUDE, {0, 4, 1}), Column(Id::TYPE_VELOCITY_Z, {0, 3, -2}),
                     Column(Id::TYPE_POSITION_X, {0, 1, 2})}),
         makeBranch("B", {0, 1}, {Column(Id::TYPE_ALTITUDE, {0, 2})})});
    const Model model("Simulation", data,
                      configuration(Id::TYPE_TIME, {RowSpec(Id::TYPE_ALTITUDE, 0),
                                                    RowSpec(Id::TYPE_VELOCITY_Z, 1)}));
    EXPECT_EQ(seriesNames(model), (std::vector<std::string>{"Altitude (m)", "B: Altitude (m)",
                                                            "Vertical velocity (m/s)"}));
    EXPECT_EQ(legendText(model), "Altitude (m)/c0 | Vertical velocity (m/s)/c2");

    // Without the domain's type the branch has no series at all.
    const Model overPosition("Simulation", data,
                             configuration(Id::TYPE_POSITION_X, {RowSpec(Id::TYPE_ALTITUDE, 0)}));
    EXPECT_EQ(seriesNames(overPosition), (std::vector<std::string>{"Altitude (m)"}));
}

TEST(SimulationPlotModel, TheDomainRangesOverTheSeriesWithHalfAUnitOnEitherSide)
{
    const DefaultUnitsGuard units;
    const auto data = flightData({makeBranch("A", {0, 1, 2},
                                             {Column(Id::TYPE_MACH_NUMBER, {kNaN, 0.25, 0.125}),
                                              Column(Id::TYPE_ALTITUDE, {0, 1, 2})}),
                                  makeBranch("B", {0, 1},
                                             {Column(Id::TYPE_MACH_NUMBER, {0.75, 0.0625}),
                                              Column(Id::TYPE_ALTITUDE, {0, 1})})});

    const Model overTime("Simulation", data,
                         configuration(Id::TYPE_TIME, {RowSpec(Id::TYPE_ALTITUDE, 0)}));
    EXPECT_EQ(overTime.getDomainLabel(), "Time (s)");
    EXPECT_EQ(overTime.getDomainRange().min, -0.5);
    EXPECT_EQ(overTime.getDomainRange().max, 2.5);
    EXPECT_TRUE(overTime.hasTimeDomain());

    // A dimensionless unit is left out of the label; a NaN value out of the range.
    const Model overMach("Simulation", data,
                         configuration(Id::TYPE_MACH_NUMBER, {RowSpec(Id::TYPE_ALTITUDE, 0)}));
    EXPECT_EQ(overMach.getDomainLabel(), "Mach number");
    EXPECT_EQ(overMach.getDomainRange().min, 0.0625 - 0.5);
    EXPECT_EQ(overMach.getDomainRange().max, 0.75 + 0.5);
    EXPECT_FALSE(overMach.hasTimeDomain());
    EXPECT_TRUE(std::isnan(overMach.getSeries().at(0).x.at(0)));
}

TEST(SimulationPlotModel, WithoutPointsNothingIsPlotted)
{
    const DefaultUnitsGuard units;
    const Model model("Simulation",
                      flightData({makeBranch("A", {}, {Column(Id::TYPE_ALTITUDE, {})})}),
                      configuration(Id::TYPE_TIME, {RowSpec(Id::TYPE_ALTITUDE, 0)}));
    EXPECT_TRUE(model.getSeries().empty());
    EXPECT_TRUE(model.getLegend().empty());
    EXPECT_TRUE(std::isnan(model.getDomainRange().min));
    EXPECT_TRUE(std::isnan(model.getDomainRangeWithEventLines().max));
    EXPECT_FALSE(model.getYAxes()[0].hasSeries);
    EXPECT_EQ(model.getYAxes()[0].label, "Altitude");
    EXPECT_TRUE(std::isnan(model.getYAxes()[0].range.min));
}

TEST(SimulationPlotModel, TheYAxesAreLabelledByTheirRowsTypesAndRangeAsTheFilledConfiguration)
{
    const DefaultUnitsGuard units;
    const Model             model("Simulation", twoBranches(),
                                  configuration(Id::TYPE_TIME, {RowSpec(Id::TYPE_ALTITUDE, 0),
                                                                RowSpec(Id::TYPE_VELOCITY_Z, 0)}));

    const std::array<Model::YAxis, 2>& axes = model.getYAxes();
    EXPECT_EQ(axes[0].label, "Altitude; Vertical velocity");
    EXPECT_TRUE(axes[0].hasSeries);
    EXPECT_EQ(axes[1].label, "");
    EXPECT_FALSE(axes[1].hasSeries);
    const QtRocket::PlotAxisBounds& left = model.getFilledConfiguration().getAllAxes()[0];
    EXPECT_EQ(axes[0].range.min, left.getMinValue());
    EXPECT_EQ(axes[0].range.max, left.getMaxValue());
    EXPECT_TRUE(std::isnan(axes[1].range.min));
    // The ranges over both branches with 3 % on either side (PlotConfiguration).
    EXPECT_EQ(left.getMinValue(), -5 - (25 * 0.03));
    EXPECT_EQ(left.getMaxValue(), 20 + (25 * 0.03));
}

/// A branch of six points (time 0 to 5 s) with altitude and vertical velocity and @p events.
[[nodiscard]] std::shared_ptr<FlightDataBranch> eventBranch(std::string              name,
                                                            std::vector<FlightEvent> events)
{
    return makeBranch(std::move(name), {0, 1, 2, 3, 4, 5},
                      {Column(Id::TYPE_ALTITUDE, {0, 2, 4, 6, 8, 10}),
                       Column(Id::TYPE_VELOCITY_Z, {0, 1, 2, 3, 4, 5})},
                      std::move(events));
}

/// "time label (look)" of each group.
[[nodiscard]] std::vector<std::string> groupTexts(const std::vector<Model::EventGroup>& groups)
{
    std::vector<std::string> texts;
    texts.reserve(groups.size());
    for (const Model::EventGroup& group : groups)
    {
        texts.push_back(
            std::format("{} {} ({})", group.time, group.label, QtRocket::name(group.lookType)));
    }
    return texts;
}

TEST(SimulationPlotModel, EventsCloseTogetherFormAGroupAtItsFirstEvent)
{
    // Bug 41: Java keeps a group's events in a HashSet; here the group is drawn at its first event
    // and lists its events in time order. Events within 0.05 s of a group's first event join it,
    // unless the group has their type; the group looks like its event of the highest ordinal.
    const DefaultUnitsGuard units;
    const auto              data = flightData(
        {eventBranch("Sustainer",
                     {FlightEvent(Event::IGNITION, 0), FlightEvent(Event::BURNOUT, 1.0),
                      FlightEvent(Event::BURNOUT, 1.015625), FlightEvent(Event::APOGEE, 1.03125),
                      FlightEvent(Event::ALTITUDE, 1.04), FlightEvent(Event::IGNITION, 1.046875),
                      FlightEvent(Event::GROUND_HIT, 1.0625), FlightEvent(Event::TUMBLE, 3)}),
         eventBranch("Booster",
                     {FlightEvent(Event::IGNITION, 1.0), FlightEvent(Event::GROUND_HIT, 4)})});
    const Model model("Simulation", data,
                      configuration(Id::TYPE_TIME, {RowSpec(Id::TYPE_ALTITUDE, 0)},
                                    {Event::IGNITION, Event::BURNOUT, Event::APOGEE,
                                     Event::GROUND_HIT, Event::ALTITUDE}));

    // TUMBLE is not marked, ALTITUDE never; at the same time the first branch's events first.
    EXPECT_EQ(model.getEvents().size(), 8U);
    EXPECT_EQ(model.getEvents().at(2).branch, 1U);
    EXPECT_EQ(model.getEvents().at(2).event.getType(), Event::IGNITION);
    EXPECT_EQ(groupTexts(model.getEventGroups()),
              (std::vector<std::string>{
                  "0 Motor ignition (IGNITION)",
                  "1 Motor burnout, Motor ignition, Apogee (APOGEE)",
                  "1.0625 Ground hit (GROUND_HIT)",
                  "4 Ground hit (GROUND_HIT)",
              }));
    EXPECT_EQ(groupEvents(model.getEventGroups().at(1)),
              "BURNOUT@1.0/b0 IGNITION@1.0/b1 APOGEE@1.03125/b0");
    EXPECT_EQ(groupTexts(model.getEventGroups(0)),
              (std::vector<std::string>{"0 Motor ignition (IGNITION)",
                                        "1 Motor burnout, Apogee, Motor ignition (APOGEE)",
                                        "1.0625 Ground hit (GROUND_HIT)"}));
    EXPECT_EQ(
        groupTexts(model.getEventGroups(1)),
        (std::vector<std::string>{"1 Motor ignition (IGNITION)", "4 Ground hit (GROUND_HIT)"}));
}

TEST(SimulationPlotModel, AWarningsGroupLooksLikeItsWarningsPriority)
{
    const DefaultUnitsGuard units;
    const Warning::Other    warning = Warning::fromString("Too fast", MessagePriority::HIGH);
    const auto              data =
        flightData({eventBranch("Sustainer", {FlightEvent(Event::RECOVERY_DEVICE_DEPLOYMENT, 2),
                                              FlightEvent(Event::SIM_WARN, 2.03125, nullptr,
                                                          FlightEvent::warningData(warning))})});
    const Model model("Simulation", data,
                      configuration(Id::TYPE_POSITION_X, {RowSpec(Id::TYPE_ALTITUDE, 0)},
                                    {Event::RECOVERY_DEVICE_DEPLOYMENT, Event::SIM_WARN}));

    const Model::EventGroup& group = model.getEventGroups().at(0);
    EXPECT_EQ(group.label, "Recovery device deployment, Warning");
    EXPECT_EQ(group.lookType, Event::SIM_WARN);
    EXPECT_EQ(group.lookWarningPriority, MessagePriority::HIGH);
    // The branch has no east position: no series, no icons.
    EXPECT_TRUE(model.getSeries().empty());
    EXPECT_TRUE(model.getIconMarkers().empty());
}

TEST(SimulationPlotModel, TheEventLinesWidenTheDomainOnce)
{
    // Bug 43: Java widens the domain at every redraw and loses its label; the label stays here.
    const DefaultUnitsGuard   units;
    const std::vector<double> times{0, 30, 60};
    const auto                data = flightData({makeBranch(
        "Sustainer", times,
        {Column(Id::TYPE_ALTITUDE, {0, 100, 0}), Column(Id::TYPE_POSITION_X, {0, 1, 2})},
        {FlightEvent(Event::APOGEE, 30), FlightEvent(Event::GROUND_HIT, 60),
         FlightEvent(Event::SIMULATION_END, 60.25)})});
    const Model overTime("Simulation", data,
                         configuration(Id::TYPE_TIME, {RowSpec(Id::TYPE_ALTITUDE, 0)},
                                       {Event::APOGEE, Event::GROUND_HIT, Event::SIMULATION_END}));

    EXPECT_EQ(overTime.getDomainRange().max, 60.5);
    // 1 % of the upper bound: 60 is within it of 60.5, then 60.25 within it of the new bound.
    const double markerWidth = 0.01 * 60.5;
    EXPECT_EQ(overTime.getDomainRangeWithEventLines().min, -0.5);
    EXPECT_EQ(overTime.getDomainRangeWithEventLines().max, 60.25 + markerWidth);
    EXPECT_EQ(overTime.getDomainLabel(), "Time (s)");

    // Not over time: no lines, no widening.
    const Model overPosition("Simulation", data,
                             configuration(Id::TYPE_POSITION_X, {RowSpec(Id::TYPE_ALTITUDE, 0)},
                                           {Event::APOGEE, Event::GROUND_HIT}));
    EXPECT_EQ(overPosition.getDomainRangeWithEventLines().max, 2.5);
}

TEST(SimulationPlotModel, ToolTipsAreOpenRocketsHtml)
{
    const DefaultUnitsGuard units;
    const auto              data = flightData({makeBranch(
        "Sustainer", {0, 1.5, 2},
        {Column(Id::TYPE_ALTITUDE, {0, 12.3456, kNaN}), Column(Id::TYPE_POSITION_X, {0, 1.5, 2}),
         Column(Id::TYPE_STABILITY, {kNaN, 2.25, 1})})});
    const Model model("Simulation", data,
                      configuration(Id::TYPE_POSITION_X, {RowSpec(Id::TYPE_ALTITUDE, 0),
                                                          RowSpec(Id::TYPE_STABILITY, 1)}));

    EXPECT_EQ(model.getToolTip(0, 1).toHtml(),
              "<html><b><i>Altitude (m)</i></b><br>Altitude: 12.35 m<br>Position East of launch: "
              "1.5 m<br>Time: 1.5 s<br></html>");
    // A NaN y has no line; a dimensionless unit is its zero-width space.
    EXPECT_EQ(model.getToolTip(0, 2).toHtml(),
              "<html><b><i>Altitude (m)</i></b><br>Position East of launch: 2 m<br>Time: 2 "
              "s<br></html>");
    EXPECT_EQ(model.getToolTip(1, 1).toHtml(),
              "<html><b><i>Stability margin calibers</i></b><br>Stability margin calibers: 2.25 "
              "\xE2\x80\x8B<br>Position East of launch: 1.5 m<br>Time: 1.5 s<br></html>");

    const Model::ToolTip parts = model.getToolTip(0, 1);
    EXPECT_EQ(parts.name, "Altitude (m)");
    EXPECT_EQ(parts.y.value_or(Model::ToolTipValue{}).value, "12.35");
    EXPECT_EQ(parts.x.name, "Position East of launch");
    EXPECT_EQ(parts.time.value_or(Model::ToolTipValue{}).unit, "s");

    // Over time, no time line.
    const Model overTime("Simulation", data,
                         configuration(Id::TYPE_TIME, {RowSpec(Id::TYPE_ALTITUDE, 0)}));
    EXPECT_EQ(overTime.getToolTip(0, 1).toHtml(),
              "<html><b><i>Altitude (m)</i></b><br>Altitude: 12.35 m<br>Time: 1.5 s<br></html>");
}

TEST(SimulationPlotModel, AToolTipsWarningsComeBeforeItsEvents)
{
    Model::ToolTip toolTip;
    toolTip.name     = "Altitude (m)";
    toolTip.warnings = {"Too fast"};
    toolTip.events   = {"Apogee", "Ground hit"};
    toolTip.x        = Model::ToolTipValue{.name = "Time", .value = "3", .unit = "s"};
    EXPECT_EQ(toolTip.toHtml(),
              "<html><b><i>Altitude (m)</i></b><br><b><i>Warning:  Too fast</b></i><br>Apogee, "
              "Ground hit<br>Time: 3 s<br></html>");
}

TEST(SimulationPlotModel, IconsMarkTheGroupsOnEverySeriesOfTheirBranch)
{
    const DefaultUnitsGuard units;
    const Warning::Other    warning = Warning::fromString("Too fast");
    const auto              data    = flightData(
        {makeBranch("Sustainer", {0, 1, 2, 3, 4},
                    {Column(Id::TYPE_POSITION_X, {0, 2, 4, 6, 8}),
                     Column(Id::TYPE_ALTITUDE, {0, 10, 20, 10, 0}),
                     Column(Id::TYPE_VELOCITY_Z, {10, 5, kNaN, -5, -10})},
                    {FlightEvent(Event::APOGEE, 1.5),
                     FlightEvent(Event::SIM_WARN, 1.5, nullptr, FlightEvent::warningData(warning)),
                     FlightEvent(Event::GROUND_HIT, 4)}),
         makeBranch("Booster", {0, 1},
                    {Column(Id::TYPE_POSITION_X, {0, 1}), Column(Id::TYPE_ALTITUDE, {0, 3}),
                     Column(Id::TYPE_VELOCITY_Z, {0, 1})},
                    {FlightEvent(Event::GROUND_HIT, 0.5)})});
    const Model model(
        "Simulation", data,
        configuration(Id::TYPE_POSITION_X,
                      {RowSpec(Id::TYPE_ALTITUDE, 0), RowSpec(Id::TYPE_VELOCITY_Z, 1)},
                      {Event::APOGEE, Event::SIM_WARN, Event::GROUND_HIT}));

    // The last branch first; then axis by axis, series by series, group by group. The velocity
    // at 1.5 s is NaN (interpolated towards the NaN at 2 s): no icon there.
    const std::vector<Model::EventIcon> icons = model.getIconMarkers();
    ASSERT_EQ(icons.size(), 5U);
    EXPECT_EQ(icons[0].series, 1U);
    EXPECT_EQ(icons[0].x, 0.5);
    EXPECT_EQ(icons[0].y, 1.5);
    EXPECT_EQ(icons[1].series, 3U);
    EXPECT_EQ(icons[2].series, 0U);
    EXPECT_EQ(icons[2].group, 0U);
    EXPECT_EQ(icons[2].time, 1.5);
    EXPECT_EQ(icons[2].x, 3);
    EXPECT_EQ(icons[2].y, 15);
    EXPECT_EQ(icons[2].yOnLeftAxis, 15);
    EXPECT_EQ(icons[2].lookType, Event::SIM_WARN);
    EXPECT_EQ(icons[2].lookWarningPriority, MessagePriority::NORMAL);
    EXPECT_EQ(icons[2].toolTip.toHtml(),
              "<html><b><i>Sustainer: Altitude (m)</i></b><br><b><i>Warning:  Too "
              "fast</b></i><br>Apogee<br>Altitude: 15 m<br>Position East of launch: 3 m<br>Time: "
              "1.5 s<br></html>");
    EXPECT_EQ(icons[3].group, 1U);
    EXPECT_EQ(icons[3].x, 8);
    EXPECT_EQ(icons[4].series, 2U);
    EXPECT_EQ(icons[4].y, -10);
    // The right axis's value mapped onto the left axis, as Java computes it.
    const auto&  axes      = model.getFilledConfiguration().getAllAxes();
    const double slope     = (axes[0].getMaxValue() - axes[0].getMinValue()) /
                             (axes[1].getMaxValue() - axes[1].getMinValue());
    const double intercept = ((axes[1].getMaxValue() * axes[0].getMinValue()) -
                              (axes[0].getMaxValue() * axes[1].getMinValue())) /
                             (axes[1].getMaxValue() - axes[1].getMinValue());
    EXPECT_EQ(icons[4].yOnLeftAxis, (slope * -10) + intercept);

    EXPECT_EQ(model.getIconMarkers(1).size(), 2U);
    EXPECT_EQ(model.getIconMarkers(0).size(), 3U);
    EXPECT_EQ(model.getIconMarkers(0).at(0).toolTip.toHtml(), icons[2].toolTip.toHtml());
}

TEST(SimulationPlotModel, AMultiStageSeriesIsNamedWithTheStagesAttachedAtThePoint)
{
    const DefaultUnitsGuard   units;
    const std::vector<double> times{0, 1, 2, 3, 4, 5};
    const std::vector<Column> columns{Column(Id::TYPE_ALTITUDE, {0, 1, 2, 3, 4, 5})};
    const auto                data = flightData(
        {makeBranch("Sustainer", times, columns, {FlightEvent(Event::STAGE_SEPARATION, 2)}),
         makeBranch("Booster", times, columns, {FlightEvent(Event::STAGE_SEPARATION, 2)}),
         makeBranch("Booster 2", times, columns, {FlightEvent(Event::STAGE_SEPARATION, 0.5)}),
         makeBranch("Payload", times, columns)});
    const Model model("Simulation", data,
                      configuration(Id::TYPE_TIME, {RowSpec(Id::TYPE_ALTITUDE, 0)}));

    // The sustainer at point 0: the boosters are attached (their separations are at points 2 and
    // 1 of their own data), the payload has no separation.
    EXPECT_EQ(model.getSeriesNameAt(0, 0), "Sustainer + Booster + Booster 2: Altitude (m)");
    EXPECT_EQ(model.getSeriesNameAt(0, 1), "Sustainer + Booster + Booster 2: Altitude (m)");
    EXPECT_EQ(model.getSeriesNameAt(0, 2), "Sustainer + Booster: Altitude (m)");
    EXPECT_EQ(model.getSeriesNameAt(0, 3), "Sustainer: Altitude (m)");
    EXPECT_EQ(model.getSeriesNameAt(1, 1), "Booster + Booster 2: Altitude (m)");
    EXPECT_EQ(model.getSeriesNameAt(2, 0), "Booster 2: Altitude (m)");
    // The last branch keeps its series name.
    EXPECT_EQ(model.getSeriesNameAt(3, 0), "Payload: Altitude (m)");
    EXPECT_EQ(model.getToolTip(0, 2).name, "Sustainer + Booster: Altitude (m)");
}

TEST(SimulationPlotModel, TheAbortAnnotationListsTheFirstAbortOfEachBranch)
{
    const DefaultUnitsGuard units;
    const auto              data = flightData(
        {eventBranch("Sustainer",
                     {FlightEvent(Event::SIM_ABORT, 1.125, nullptr,
                                  SimulationAbort(SimulationAbort::Cause::NO_MOTORS_DEFINED))}),
         eventBranch("Booster", {}),
         eventBranch("Booster",
                     {FlightEvent(Event::SIM_ABORT, 2.5, nullptr,
                                  SimulationAbort(SimulationAbort::Cause::NO_CP)),
                      FlightEvent(Event::SIM_ABORT, 3, nullptr,
                                  SimulationAbort(SimulationAbort::Cause::NO_LIFTOFF))})});
    const Model model("Simulation", data,
                      configuration(Id::TYPE_TIME, {RowSpec(Id::TYPE_ALTITUDE, 0)}));

    const std::vector<Model::AbortLine> lines = model.getAbortLines();
    ASSERT_EQ(lines.size(), 2U);
    EXPECT_EQ(lines[0].branch, 0U);
    EXPECT_EQ(lines[0].branchName, "Sustainer");
    EXPECT_EQ(lines[0].time, 1.125);
    EXPECT_EQ(lines[0].cause, SimulationAbort::Cause::NO_MOTORS_DEFINED);
    EXPECT_EQ(lines[1].branch, 2U);
    EXPECT_EQ(lines[1].cause, SimulationAbort::Cause::NO_CP);
    // Java's String.format("%.2f") rounds the shortest digits half up: 1.125 is "1.13".
    const std::string sustainer =
        std::format("Stage: Sustainer; Time: 1.13 s; Cause: {}",
                    QtRocket::causeText(SimulationAbort::Cause::NO_MOTORS_DEFINED));
    const std::string booster = std::format("Stage: Booster; Time: 2.50 s; Cause: {}",
                                            QtRocket::causeText(SimulationAbort::Cause::NO_CP));
    EXPECT_EQ(model.getAbortText(), "Simulation Abort\n" + sustainer + "\n" + booster);
    EXPECT_EQ(model.getAbortText(0), "Simulation Abort\n" + sustainer);
    EXPECT_EQ(model.getAbortText(1), "");
    EXPECT_TRUE(model.getAbortLines(1).empty());
    EXPECT_EQ(model.getAbortText(2), "Simulation Abort\n" + booster);
}

/// A Translator that writes "[key|English text]" and keeps the keys it was asked for.
struct RecordingTranslator
{
    [[nodiscard]] std::string operator()(std::string_view key, std::string_view englishText) const
    {
        keys->emplace_back(key);
        return std::format("[{}|{}]", key, englishText);
    }

    std::shared_ptr<std::vector<std::string>> keys = std::make_shared<std::vector<std::string>>();
};

TEST(SimulationPlotModel, ATranslatorTranslatesOpenRocketsTexts)
{
    const DefaultUnitsGuard   units;
    const RecordingTranslator translator;
    const auto                data = flightData(
        {eventBranch("Sustainer", {FlightEvent(Event::APOGEE, 2),
                                   FlightEvent(Event::SIM_ABORT, 3, nullptr,
                                               SimulationAbort(SimulationAbort::Cause::NO_CP))})});
    // The first preset's name and key, over altitude and vertical velocity on the left.
    SimulationPlotConfiguration config("Vertical motion vs. time",
                                       "PlotConfiguration.Verticalmotion", type(Id::TYPE_TIME));
    config.addPlotDataType(type(Id::TYPE_ALTITUDE), 0);
    config.addPlotDataType(type(Id::TYPE_VELOCITY_Z), 0);
    config.setEvent(Event::APOGEE, true);
    const Model model("Simulation", data, config, translator);

    EXPECT_EQ(model.getSubtitle(), "[PlotConfiguration.Verticalmotion|Vertical motion vs. time]");
    EXPECT_EQ(model.getSeries().at(0).label, "[FlightDataType.TYPE_ALTITUDE|Altitude] (m)");
    EXPECT_EQ(model.getYAxes()[0].label,
              "[FlightDataType.TYPE_ALTITUDE|Altitude]; [FlightDataType.TYPE_VELOCITY_Z|Vertical "
              "velocity]");
    EXPECT_EQ(model.getDomainLabel(), "[FlightDataType.TYPE_TIME|Time] (s)");
    EXPECT_EQ(model.getEventGroups().at(0).label, "[FlightEvent.Type.APOGEE|Apogee]");
    EXPECT_EQ(model.getAbortText(),
              std::format("[simulationplot.abort.title|Simulation Abort]\n"
                          "[simulationplot.abort.stage|Stage]: Sustainer; "
                          "[simulationplot.abort.time|Time]: 3.00 s; "
                          "[simulationplot.abort.cause|Cause]: {}",
                          QtRocket::causeText(SimulationAbort::Cause::NO_CP)));
    EXPECT_EQ(model.getToolTip(0, 1).toHtml(),
              "<html><b><i>[FlightDataType.TYPE_ALTITUDE|Altitude] (m)</i></b><br>"
              "[FlightDataType.TYPE_ALTITUDE|Altitude]: 2 m<br>"
              "[FlightDataType.TYPE_TIME|Time]: 1 s<br></html>");
    // Never asked for an empty key (the cause has none yet).
    EXPECT_EQ(std::ranges::count(*translator.keys, std::string()), 0);
    EXPECT_FALSE(translator.keys->empty());

    // A name without a key is shown as it is.
    config.setName("Mine");
    EXPECT_EQ(Model("Simulation", data, config, translator).getSubtitle(), "Mine");
}

TEST(SimulationPlotModel, TheModelSharesTheFlightData)
{
    const DefaultUnitsGuard     units;
    std::shared_ptr<FlightData> data = std::make_shared<FlightData>(
        std::initializer_list<std::shared_ptr<FlightDataBranch>>{eventBranch("Sustainer", {})});
    const std::weak_ptr<FlightData> watch = data;
    Model model("Simulation", data, configuration(Id::TYPE_TIME, {RowSpec(Id::TYPE_ALTITUDE, 0)}));
    data.reset();
    ASSERT_FALSE(watch.expired());
    EXPECT_EQ(model.getData().getBranch(0).getName(), "Sustainer");
    const Model moved(std::move(model));
    EXPECT_EQ(moved.getToolTip(0, 5).toHtml(),
              "<html><b><i>Altitude (m)</i></b><br>Altitude: 10 m<br>Time: 5 s<br></html>");
}

TEST(SimulationPlotModel, TheDefaultColourIndicesWithoutABranchAreTheRows)
{
    const DefaultUnitsGuard units;
    EXPECT_EQ(Model::computeDefaultColorIndices(
                  SimulationPlotConfiguration::defaultConfigurations().at(0), FlightData{}),
              (std::vector<int>{0, 1, 2}));
}

TEST(SimulationPlotModel, MisusesAreBugs)
{
    const DefaultUnitsGuard           units;
    const SimulationPlotConfiguration config =
        configuration(Id::TYPE_TIME, {RowSpec(Id::TYPE_ALTITUDE, 0)});
    EXPECT_THROW(Model("Simulation", nullptr, config), BugError);
    EXPECT_THROW(Model("Simulation", std::make_shared<const FlightData>(), config), BugError);

    const Model model("Simulation", twoBranches(), config);
    EXPECT_THROW(static_cast<void>(model.getSeriesNameAt(2, 0)), BugError);
    EXPECT_THROW(static_cast<void>(model.getToolTip(2, 0)), BugError);
    EXPECT_THROW(static_cast<void>(model.getToolTip(0, 4)), BugError);
    EXPECT_THROW(static_cast<void>(model.getEventGroups(2)), BugError);
    EXPECT_THROW(static_cast<void>(model.getIconMarkers(2)), BugError);
    EXPECT_THROW(static_cast<void>(model.getAbortLines(2)), BugError);
    EXPECT_THROW(static_cast<void>(model.getAbortText(2)), BugError);
}

// ---------------------------------------------------------------------------------------------
// The examples, pinned to OpenRocket's plots

/// A series as the probe printed it: its index in getSeries(), name, branch, row, unit, number of
/// points, colour index, and its first, middle (number / 2) and last points.
struct SeriesPin
{
    constexpr SeriesPin(std::size_t pinIndex, std::string_view pinName, std::size_t pinBranch,
                        std::size_t pinRow, std::string_view pinUnit, std::size_t pinPoints,
                        int pinColorIndex, std::array<double, 6> pinXy)
      : index(pinIndex),
        name(pinName),
        branch(pinBranch),
        row(pinRow),
        unit(pinUnit),
        points(pinPoints),
        colorIndex(pinColorIndex),
        xy(pinXy)
    {
    }

    std::size_t           index;
    std::string_view      name;
    std::size_t           branch;
    std::size_t           row;
    std::string_view      unit;
    std::size_t           points;
    int                   colorIndex;
    std::array<double, 6> xy;
};

/// A readout tool tip: of point @p point of series @p series.
struct ToolTipPin
{
    constexpr ToolTipPin(std::size_t pinSeries, std::size_t pinPoint, std::string_view pinHtml)
      : series(pinSeries), point(pinPoint), html(pinHtml)
    {
    }

    std::size_t      series;
    std::size_t      point;
    std::string_view html;
};

/// The branch of a group of every branch.
constexpr std::size_t kAllBranches = std::numeric_limits<std::size_t>::max();

/// An event group: of a branch (or kAllBranches), its time, its look's type and image, its events
/// ("type@time/b<branch>") and its label.
struct GroupPin
{
    constexpr GroupPin(std::size_t pinBranch, double pinTime, std::string_view pinLook,
                       std::string_view pinImage, std::string_view pinEvents,
                       std::string_view pinLabel)
      : branch(pinBranch),
        time(pinTime),
        look(pinLook),
        image(pinImage),
        events(pinEvents),
        label(pinLabel)
    {
    }

    std::size_t      branch;
    double           time;
    std::string_view look;
    std::string_view image;
    std::string_view events;
    std::string_view label;
};

/// An icon, in getIconMarkers() order: its series, time, x, y, y on the left axis, image and tool
/// tip.
struct IconPin
{
    constexpr IconPin(std::size_t pinSeries, double pinTime, double pinX, double pinY,
                      double pinYOnLeftAxis, std::string_view pinImage, std::string_view pinToolTip)
      : series(pinSeries),
        time(pinTime),
        x(pinX),
        y(pinY),
        yOnLeftAxis(pinYOnLeftAxis),
        image(pinImage),
        toolTip(pinToolTip)
    {
    }

    std::size_t      series;
    double           time;
    double           x;
    double           y;
    double           yOnLeftAxis;
    std::string_view image;
    std::string_view toolTip;
};

/// A y axis: its label, range and whether it has series.
struct AxisPin
{
    constexpr AxisPin(std::string_view pinLabel, double pinMin, double pinMax, bool pinHasSeries)
      : label(pinLabel), min(pinMin), max(pinMax), hasSeries(pinHasSeries)
    {
    }

    std::string_view label;
    double           min;
    double           max;
    bool             hasSeries;
};

/// A plot of a stored simulation of an example with a preset, as the probe printed it.
struct PlotPin
{
    std::string_view      file;
    std::size_t           simulation;
    std::size_t           preset;
    std::string_view      title;
    std::string_view      subtitle;
    std::string_view      domainLabel;
    std::array<double, 2> domain;
    std::array<double, 2> domainWithEventLines;
    /// "row->axis ..." of the filled configuration.
    std::string_view       axes;
    std::array<AxisPin, 2> yAxes;
    /// "row->colour index ..." of the plot panel (computeDefaultPlotColors()).
    std::string_view colors;
    /// The legend's labels joined by " | ".
    std::string_view            legend;
    std::span<const SeriesPin>  series;
    std::span<const ToolTipPin> toolTips;
    std::span<const GroupPin>   groups;
    std::span<const IconPin>    icons;
};

// clang-format off
// A simple model rocket.ork, simulation 0, preset 0 "Vertical motion vs. time".
constexpr auto kSimpleVerticalSeries = std::to_array<SeriesPin>({
    SeriesPin(0, "Altitude (m)", 0, 0, "m", 233, 0, {0.0, 0.0, 0x1.3ae147ae147aep0, 0x1.9a0c49ba5e354p4, 0x1.fc6a7ef9db22dp3, 0.0}),
    SeriesPin(1, "Vertical velocity (m/s)", 0, 1, "m/s", 233, 1, {0.0, 0.0, 0x1.3ae147ae147aep0, 0x1.70189374bc6a8p4, 0x1.fc6a7ef9db22dp3, -0x1.09a9fbe76c8b4p2}),
    SeriesPin(2, "Vertical acceleration (m/s\xC2\xB2)", 0, 2, "m/s\xC2\xB2", 233, 2, {0.0, 0.0, 0x1.3ae147ae147aep0, -0x1.6ac8b43958106p3, 0x1.fc6a7ef9db22dp3, -0x1.999999999999ap-5}),
});
constexpr auto kSimpleVerticalToolTips = std::to_array<ToolTipPin>({
    ToolTipPin(0, 0, "<html><b><i>Altitude (m)</i></b><br>Altitude: 0 m<br>Time: 0 s<br></html>"),
    ToolTipPin(0, 116, "<html><b><i>Altitude (m)</i></b><br>Altitude: 25.63 m<br>Time: 1.23 s<br></html>"),
    ToolTipPin(0, 232, "<html><b><i>Altitude (m)</i></b><br>Altitude: 0 m<br>Time: 15.89 s<br></html>"),
    ToolTipPin(1, 0, "<html><b><i>Vertical velocity (m/s)</i></b><br>Vertical velocity: 0 m/s<br>Time: 0 s<br></html>"),
    ToolTipPin(1, 116, "<html><b><i>Vertical velocity (m/s)</i></b><br>Vertical velocity: 23.01 m/s<br>Time: 1.23 s<br></html>"),
    ToolTipPin(1, 232, "<html><b><i>Vertical velocity (m/s)</i></b><br>Vertical velocity: -4.15 m/s<br>Time: 15.89 s<br></html>"),
    ToolTipPin(2, 0, "<html><b><i>Vertical acceleration (m/s\xC2\xB2)</i></b><br>Vertical acceleration: 0 m/s\xC2\xB2<br>Time: 0 s<br></html>"),
    ToolTipPin(2, 116, "<html><b><i>Vertical acceleration (m/s\xC2\xB2)</i></b><br>Vertical acceleration: -11.34 m/s\xC2\xB2<br>Time: 1.23 s<br></html>"),
    ToolTipPin(2, 232, "<html><b><i>Vertical acceleration (m/s\xC2\xB2)</i></b><br>Vertical acceleration: -0.05 m/s\xC2\xB2<br>Time: 15.89 s<br></html>"),
});
constexpr auto kSimpleVerticalGroups = std::to_array<GroupPin>({
    GroupPin(kAllBranches, 0.0, "IGNITION", "event:IGNITION", "IGNITION@0.0/b0", "Motor ignition"),
    GroupPin(kAllBranches, 0x1.75c28f5c28f5cp-1, "BURNOUT", "event:BURNOUT", "BURNOUT@0.73/b0", "Motor burnout"),
    GroupPin(kAllBranches, 0x1.bd916872b020cp1, "APOGEE", "event:APOGEE", "APOGEE@3.481/b0", "Apogee"),
    GroupPin(kAllBranches, 0x1.dd916872b020cp1, "RECOVERY_DEVICE_DEPLOYMENT", "event:RECOVERY_DEVICE_DEPLOYMENT", "RECOVERY_DEVICE_DEPLOYMENT@3.731/b0", "Recovery device deployment"),
    GroupPin(kAllBranches, 0x1.fc6a7ef9db22dp3, "GROUND_HIT", "event:GROUND_HIT", "GROUND_HIT@15.888/b0", "Ground hit"),
    GroupPin(0, 0.0, "IGNITION", "event:IGNITION", "IGNITION@0.0/b0", "Motor ignition"),
    GroupPin(0, 0x1.75c28f5c28f5cp-1, "BURNOUT", "event:BURNOUT", "BURNOUT@0.73/b0", "Motor burnout"),
    GroupPin(0, 0x1.bd916872b020cp1, "APOGEE", "event:APOGEE", "APOGEE@3.481/b0", "Apogee"),
    GroupPin(0, 0x1.dd916872b020cp1, "RECOVERY_DEVICE_DEPLOYMENT", "event:RECOVERY_DEVICE_DEPLOYMENT", "RECOVERY_DEVICE_DEPLOYMENT@3.731/b0", "Recovery device deployment"),
    GroupPin(0, 0x1.fc6a7ef9db22dp3, "GROUND_HIT", "event:GROUND_HIT", "GROUND_HIT@15.888/b0", "Ground hit"),
});
constexpr auto kSimpleVerticalIcons = std::to_array<IconPin>({
    IconPin(0, 0.0, 0.0, 0.0, 0.0, "event:IGNITION", "<html><b><i>Altitude (m)</i></b><br>Motor ignition<br>Altitude: 0 m<br>Time: 0 s<br></html>"),
    IconPin(0, 0x1.75c28f5c28f5cp-1, 0x1.75c28f5c28f5cp-1, 0x1.94bc6a7ef9db2p3, 0x1.94bc6a7ef9db2p3, "event:BURNOUT", "<html><b><i>Altitude (m)</i></b><br>Motor burnout<br>Altitude: 12.65 m<br>Time: 0.73 s<br></html>"),
    IconPin(0, 0x1.bd916872b020cp1, 0x1.bd916872b020cp1, 0x1.94b851eb851ecp5, 0x1.94b851eb851ecp5, "event:APOGEE", "<html><b><i>Altitude (m)</i></b><br>Apogee<br>Altitude: 50.59 m<br>Time: 3.48 s<br></html>"),
    IconPin(0, 0x1.dd916872b020cp1, 0x1.dd916872b020cp1, 0x1.92083126e978dp5, 0x1.92083126e978dp5, "event:RECOVERY_DEVICE_DEPLOYMENT", "<html><b><i>Altitude (m)</i></b><br>Recovery device deployment<br>Altitude: 50.25 m<br>Time: 3.73 s<br></html>"),
    IconPin(0, 0x1.fc6a7ef9db22dp3, 0x1.fc6a7ef9db22dp3, 0.0, 0.0, "event:GROUND_HIT", "<html><b><i>Altitude (m)</i></b><br>Ground hit<br>Altitude: 0 m<br>Time: 15.89 s<br></html>"),
    IconPin(1, 0.0, 0.0, 0.0, 0.0, "event:IGNITION", "<html><b><i>Vertical velocity (m/s)</i></b><br>Motor ignition<br>Vertical velocity: 0 m/s<br>Time: 0 s<br></html>"),
    IconPin(1, 0x1.75c28f5c28f5cp-1, 0x1.75c28f5c28f5cp-1, 0x1.cf4bc6a7ef9dbp4, 0x1.cf4bc6a7ef9dbp4, "event:BURNOUT", "<html><b><i>Vertical velocity (m/s)</i></b><br>Motor burnout<br>Vertical velocity: 28.96 m/s<br>Time: 0.73 s<br></html>"),
    IconPin(1, 0x1.bd916872b020cp1, 0x1.bd916872b020cp1, -0x1.1cac083126e98p-3, -0x1.1cac083126e98p-3, "event:APOGEE", "<html><b><i>Vertical velocity (m/s)</i></b><br>Apogee<br>Vertical velocity: -0.14 m/s<br>Time: 3.48 s<br></html>"),
    IconPin(1, 0x1.dd916872b020cp1, 0x1.dd916872b020cp1, -0x1.44b17e4b17e48p1, -0x1.44b17e4b17e48p1, "event:RECOVERY_DEVICE_DEPLOYMENT", "<html><b><i>Vertical velocity (m/s)</i></b><br>Recovery device deployment<br>Vertical velocity: -2.54 m/s<br>Time: 3.73 s<br></html>"),
    IconPin(1, 0x1.fc6a7ef9db22dp3, 0x1.fc6a7ef9db22dp3, -0x1.09a9fbe76c8b4p2, -0x1.09a9fbe76c8b4p2, "event:GROUND_HIT", "<html><b><i>Vertical velocity (m/s)</i></b><br>Ground hit<br>Vertical velocity: -4.15 m/s<br>Time: 15.89 s<br></html>"),
    IconPin(2, 0.0, 0.0, 0.0, 0x1.8376f8e8b8934p-51, "event:IGNITION", "<html><b><i>Vertical acceleration (m/s\xC2\xB2)</i></b><br>Motor ignition<br>Vertical acceleration: 0 m/s\xC2\xB2<br>Time: 0 s<br></html>"),
    IconPin(2, 0x1.75c28f5c28f5cp-1, 0x1.75c28f5c28f5cp-1, -0x1.8883126e978d5p3, -0x1.0d6a03b5f63bcp2, "event:BURNOUT", "<html><b><i>Vertical acceleration (m/s\xC2\xB2)</i></b><br>Motor burnout<br>Vertical acceleration: -12.27 m/s\xC2\xB2<br>Time: 0.73 s<br></html>"),
    IconPin(2, 0x1.bd916872b020cp1, 0x1.bd916872b020cp1, -0x1.35c28f5c28f5cp3, -0x1.a93a975e0fb95p1, "event:APOGEE", "<html><b><i>Vertical acceleration (m/s\xC2\xB2)</i></b><br>Apogee<br>Vertical acceleration: -9.68 m/s\xC2\xB2<br>Time: 3.48 s<br></html>"),
    IconPin(2, 0x1.dd916872b020cp1, 0x1.dd916872b020cp1, -0x1.f16872b020d61p2, -0x1.5569c0d4b9863p1, "event:RECOVERY_DEVICE_DEPLOYMENT", "<html><b><i>Vertical acceleration (m/s\xC2\xB2)</i></b><br>Recovery device deployment<br>Vertical acceleration: -7.77 m/s\xC2\xB2<br>Time: 3.73 s<br></html>"),
    IconPin(2, 0x1.fc6a7ef9db22dp3, 0x1.fc6a7ef9db22dp3, -0x1.999999999999ap-5, -0x1.19249f513b029p-6, "event:GROUND_HIT", "<html><b><i>Vertical acceleration (m/s\xC2\xB2)</i></b><br>Ground hit<br>Vertical acceleration: -0.05 m/s\xC2\xB2<br>Time: 15.89 s<br></html>"),
});
constexpr PlotPin kSimpleVerticalPlot{
    .file = "A simple model rocket.ork",
    .simulation = 0,
    .preset = 0,
    .title = "Simulation 1",
    .subtitle = "Vertical motion vs. time",
    .domainLabel = "Time (s)",
    .domain = {-0x1.0p-1, 0x1.06353f7ced916p4},
    .domainWithEventLines = {-0x1.0p-1, 0x1.06353f7ced916p4},
    .axes = "0->0 1->0 2->1",
    .yAxes = {AxisPin("Altitude; Vertical velocity", -0x1.7426aa8eb4634p2, 0x1.a1dce5b4245f6p5, true), AxisPin("Vertical acceleration", -0x1.0f185f06f6944p4, 0x1.3064eace773b1p7, true)},
    .colors = "0->0 1->1 2->2",
    .legend = "Altitude (m) | Vertical velocity (m/s) | Vertical acceleration (m/s\xC2\xB2)",
    .series = kSimpleVerticalSeries,
    .toolTips = kSimpleVerticalToolTips,
    .groups = kSimpleVerticalGroups,
    .icons = kSimpleVerticalIcons,
};

// A simple model rocket.ork, simulation 0, preset 2 "Flight side profile".
constexpr auto kSimpleSideProfileSeries = std::to_array<SeriesPin>({
    SeriesPin(0, "Altitude (m)", 0, 0, "m", 233, 0, {0.0, 0.0, 0x1.f22d0e5604189p-1, 0x1.9a0c49ba5e354p4, -0x1.467ef9db22d0ep4, 0.0}),
});
constexpr auto kSimpleSideProfileToolTips = std::to_array<ToolTipPin>({
    ToolTipPin(0, 0, "<html><b><i>Altitude (m)</i></b><br>Altitude: 0 m<br>Position East of launch: 0 m<br>Time: 0 s<br></html>"),
    ToolTipPin(0, 116, "<html><b><i>Altitude (m)</i></b><br>Altitude: 25.63 m<br>Position East of launch: 0.97 m<br>Time: 1.23 s<br></html>"),
    ToolTipPin(0, 232, "<html><b><i>Altitude (m)</i></b><br>Altitude: 0 m<br>Position East of launch: -20.41 m<br>Time: 15.89 s<br></html>"),
});
constexpr auto kSimpleSideProfileGroups = std::to_array<GroupPin>({
    GroupPin(kAllBranches, 0.0, "IGNITION", "event:IGNITION", "IGNITION@0.0/b0", "Motor ignition"),
    GroupPin(kAllBranches, 0x1.75c28f5c28f5cp-1, "BURNOUT", "event:BURNOUT", "BURNOUT@0.73/b0", "Motor burnout"),
    GroupPin(kAllBranches, 0x1.bd916872b020cp1, "APOGEE", "event:APOGEE", "APOGEE@3.481/b0", "Apogee"),
    GroupPin(kAllBranches, 0x1.dd916872b020cp1, "RECOVERY_DEVICE_DEPLOYMENT", "event:RECOVERY_DEVICE_DEPLOYMENT", "RECOVERY_DEVICE_DEPLOYMENT@3.731/b0", "Recovery device deployment"),
    GroupPin(kAllBranches, 0x1.fc6a7ef9db22dp3, "GROUND_HIT", "event:GROUND_HIT", "GROUND_HIT@15.888/b0", "Ground hit"),
    GroupPin(0, 0.0, "IGNITION", "event:IGNITION", "IGNITION@0.0/b0", "Motor ignition"),
    GroupPin(0, 0x1.75c28f5c28f5cp-1, "BURNOUT", "event:BURNOUT", "BURNOUT@0.73/b0", "Motor burnout"),
    GroupPin(0, 0x1.bd916872b020cp1, "APOGEE", "event:APOGEE", "APOGEE@3.481/b0", "Apogee"),
    GroupPin(0, 0x1.dd916872b020cp1, "RECOVERY_DEVICE_DEPLOYMENT", "event:RECOVERY_DEVICE_DEPLOYMENT", "RECOVERY_DEVICE_DEPLOYMENT@3.731/b0", "Recovery device deployment"),
    GroupPin(0, 0x1.fc6a7ef9db22dp3, "GROUND_HIT", "event:GROUND_HIT", "GROUND_HIT@15.888/b0", "Ground hit"),
});
constexpr auto kSimpleSideProfileIcons = std::to_array<IconPin>({
    IconPin(0, 0.0, 0.0, 0.0, 0.0, "event:IGNITION", "<html><b><i>Altitude (m)</i></b><br>Motor ignition<br>Altitude: 0 m<br>Position East of launch: 0 m<br>Time: 0 s<br></html>"),
    IconPin(0, 0x1.75c28f5c28f5cp-1, 0x1.26e978d4fdf3bp-2, 0x1.94bc6a7ef9db2p3, 0x1.94bc6a7ef9db2p3, "event:BURNOUT", "<html><b><i>Altitude (m)</i></b><br>Motor burnout<br>Altitude: 12.65 m<br>Position East of launch: 0.29 m<br>Time: 0.73 s<br></html>"),
    IconPin(0, 0x1.bd916872b020cp1, 0x1.b0e5604189375p1, 0x1.94b851eb851ecp5, 0x1.94b851eb851ecp5, "event:APOGEE", "<html><b><i>Altitude (m)</i></b><br>Apogee<br>Altitude: 50.59 m<br>Position East of launch: 3.38 m<br>Time: 3.48 s<br></html>"),
    IconPin(0, 0x1.dd916872b020cp1, 0x1.c983c131d5acbp1, 0x1.92083126e978dp5, 0x1.92083126e978dp5, "event:RECOVERY_DEVICE_DEPLOYMENT", "<html><b><i>Altitude (m)</i></b><br>Recovery device deployment<br>Altitude: 50.25 m<br>Position East of launch: 3.57 m<br>Time: 3.73 s<br></html>"),
    IconPin(0, 0x1.fc6a7ef9db22dp3, -0x1.467ef9db22d0ep4, 0.0, 0.0, "event:GROUND_HIT", "<html><b><i>Altitude (m)</i></b><br>Ground hit<br>Altitude: 0 m<br>Position East of launch: -20.41 m<br>Time: 15.89 s<br></html>"),
});
constexpr PlotPin kSimpleSideProfilePlot{
    .file = "A simple model rocket.ork",
    .simulation = 0,
    .preset = 2,
    .title = "Simulation 1",
    .subtitle = "Flight side profile",
    .domainLabel = "Position East of launch (m)",
    .domain = {-0x1.4e7ef9db22d0ep4, 0x1.07df3b645a1cbp2},
    .domainWithEventLines = {-0x1.4e7ef9db22d0ep4, 0x1.07df3b645a1cbp2},
    .axes = "0->0",
    .yAxes = {AxisPin("Altitude", -0x1.8487fcb923a2ap0, 0x1.a0dc91d14e3bdp5, true), AxisPin("", kNaN, kNaN, false)},
    .colors = "0->0",
    .legend = "Altitude (m)",
    .series = kSimpleSideProfileSeries,
    .toolTips = kSimpleSideProfileToolTips,
    .groups = kSimpleSideProfileGroups,
    .icons = kSimpleSideProfileIcons,
};

// A simple model rocket.ork, simulation 0, preset 4 "Stability vs. time".
constexpr auto kSimpleStabilitySeries = std::to_array<SeriesPin>({
    SeriesPin(0, "Stability margin calibers", 0, 0, "\xE2\x80\x8B", 233, 0, {0.0, kNaN, 0x1.3ae147ae147aep0, 0x1.770a3d70a3d71p1, 0x1.fc6a7ef9db22dp3, -0x1.34c49ba5e353fp3}),
    SeriesPin(1, "CP location (cm)", 0, 1, "cm", 233, 1, {0.0, kNaN, 0x1.3ae147ae147aep0, 0x1.f666666666666p4, 0x1.fc6a7ef9db22dp3, 0.0}),
    SeriesPin(2, "CG location (cm)", 0, 2, "cm", 233, 2, {0.0, 0x1.8cccccccccccdp4, 0x1.3ae147ae147aep0, 0x1.8199999999999p4, 0x1.fc6a7ef9db22dp3, 0x1.8199999999999p4}),
});
constexpr auto kSimpleStabilityToolTips = std::to_array<ToolTipPin>({
    ToolTipPin(0, 0, "<html><b><i>Stability margin calibers</i></b><br>Time: 0 s<br></html>"),
    ToolTipPin(0, 116, "<html><b><i>Stability margin calibers</i></b><br>Stability margin calibers: 2.93 \xE2\x80\x8B<br>Time: 1.23 s<br></html>"),
    ToolTipPin(0, 232, "<html><b><i>Stability margin calibers</i></b><br>Stability margin calibers: -9.65 \xE2\x80\x8B<br>Time: 15.89 s<br></html>"),
    ToolTipPin(1, 0, "<html><b><i>CP location (cm)</i></b><br>Time: 0 s<br></html>"),
    ToolTipPin(1, 116, "<html><b><i>CP location (cm)</i></b><br>CP location: 31.4 cm<br>Time: 1.23 s<br></html>"),
    ToolTipPin(1, 232, "<html><b><i>CP location (cm)</i></b><br>CP location: 0 cm<br>Time: 15.89 s<br></html>"),
    ToolTipPin(2, 0, "<html><b><i>CG location (cm)</i></b><br>CG location: 24.8 cm<br>Time: 0 s<br></html>"),
    ToolTipPin(2, 116, "<html><b><i>CG location (cm)</i></b><br>CG location: 24.1 cm<br>Time: 1.23 s<br></html>"),
    ToolTipPin(2, 232, "<html><b><i>CG location (cm)</i></b><br>CG location: 24.1 cm<br>Time: 15.89 s<br></html>"),
});
constexpr auto kSimpleStabilityGroups = std::to_array<GroupPin>({
    GroupPin(kAllBranches, 0.0, "IGNITION", "event:IGNITION", "IGNITION@0.0/b0", "Motor ignition"),
    GroupPin(kAllBranches, 0x1.75c28f5c28f5cp-1, "BURNOUT", "event:BURNOUT", "BURNOUT@0.73/b0", "Motor burnout"),
    GroupPin(kAllBranches, 0x1.bd916872b020cp1, "APOGEE", "event:APOGEE", "APOGEE@3.481/b0", "Apogee"),
    GroupPin(kAllBranches, 0x1.dd916872b020cp1, "RECOVERY_DEVICE_DEPLOYMENT", "event:RECOVERY_DEVICE_DEPLOYMENT", "RECOVERY_DEVICE_DEPLOYMENT@3.731/b0", "Recovery device deployment"),
    GroupPin(kAllBranches, 0x1.fc6a7ef9db22dp3, "GROUND_HIT", "event:GROUND_HIT", "GROUND_HIT@15.888/b0", "Ground hit"),
    GroupPin(0, 0.0, "IGNITION", "event:IGNITION", "IGNITION@0.0/b0", "Motor ignition"),
    GroupPin(0, 0x1.75c28f5c28f5cp-1, "BURNOUT", "event:BURNOUT", "BURNOUT@0.73/b0", "Motor burnout"),
    GroupPin(0, 0x1.bd916872b020cp1, "APOGEE", "event:APOGEE", "APOGEE@3.481/b0", "Apogee"),
    GroupPin(0, 0x1.dd916872b020cp1, "RECOVERY_DEVICE_DEPLOYMENT", "event:RECOVERY_DEVICE_DEPLOYMENT", "RECOVERY_DEVICE_DEPLOYMENT@3.731/b0", "Recovery device deployment"),
    GroupPin(0, 0x1.fc6a7ef9db22dp3, "GROUND_HIT", "event:GROUND_HIT", "GROUND_HIT@15.888/b0", "Ground hit"),
});
constexpr auto kSimpleStabilityIcons = std::to_array<IconPin>({
    IconPin(0, 0x1.75c28f5c28f5cp-1, 0x1.75c28f5c28f5cp-1, 0x1.6fbe76c8b4396p1, 0x1.6fbe76c8b4396p1, "event:BURNOUT", "<html><b><i>Stability margin calibers</i></b><br>Motor burnout<br>Stability margin calibers: 2.87 \xE2\x80\x8B<br>Time: 0.73 s<br></html>"),
    IconPin(0, 0x1.bd916872b020cp1, 0x1.bd916872b020cp1, 0x1.99ba5e353f7cfp1, 0x1.99ba5e353f7cfp1, "event:APOGEE", "<html><b><i>Stability margin calibers</i></b><br>Apogee<br>Stability margin calibers: 3.2 \xE2\x80\x8B<br>Time: 3.48 s<br></html>"),
    IconPin(0, 0x1.dd916872b020cp1, 0x1.dd916872b020cp1, -0x1.ee147ae14644p-1, -0x1.ee147ae14644p-1, "event:RECOVERY_DEVICE_DEPLOYMENT", "<html><b><i>Stability margin calibers</i></b><br>Recovery device deployment<br>Stability margin calibers: -0.96 \xE2\x80\x8B<br>Time: 3.73 s<br></html>"),
    IconPin(0, 0x1.fc6a7ef9db22dp3, 0x1.fc6a7ef9db22dp3, -0x1.34c49ba5e353fp3, -0x1.34c49ba5e353fp3, "event:GROUND_HIT", "<html><b><i>Stability margin calibers</i></b><br>Ground hit<br>Stability margin calibers: -9.65 \xE2\x80\x8B<br>Time: 15.89 s<br></html>"),
    IconPin(1, 0x1.75c28f5c28f5cp-1, 0x1.75c28f5c28f5cp-1, 0x1.f4ccccccccccdp4, 0x1.c2e8bbd6f9806p1, "event:BURNOUT", "<html><b><i>CP location (cm)</i></b><br>Motor burnout<br>CP location: 31.3 cm<br>Time: 0.73 s<br></html>"),
    IconPin(1, 0x1.bd916872b020cp1, 0x1.bd916872b020cp1, 0x1.00ccccccccccdp5, 0x1.ce6f175655c1dp1, "event:APOGEE", "<html><b><i>CP location (cm)</i></b><br>Apogee<br>CP location: 32.1 cm<br>Time: 3.48 s<br></html>"),
    IconPin(1, 0x1.dd916872b020cp1, 0x1.dd916872b020cp1, 0x1.5bbbbbbbbbd8p4, 0x1.391760594a60ap1, "event:RECOVERY_DEVICE_DEPLOYMENT", "<html><b><i>CP location (cm)</i></b><br>Recovery device deployment<br>CP location: 21.73 cm<br>Time: 3.73 s<br></html>"),
    IconPin(1, 0x1.fc6a7ef9db22dp3, 0x1.fc6a7ef9db22dp3, 0.0, 0x1.0adeeef850839p-50, "event:GROUND_HIT", "<html><b><i>CP location (cm)</i></b><br>Ground hit<br>CP location: 0 cm<br>Time: 15.89 s<br></html>"),
    IconPin(2, 0.0, 0.0, 0x1.8cccccccccccdp4, 0x1.6545146c2becbp1, "event:IGNITION", "<html><b><i>CG location (cm)</i></b><br>Motor ignition<br>CG location: 24.8 cm<br>Time: 0 s<br></html>"),
    IconPin(2, 0x1.75c28f5c28f5cp-1, 0x1.75c28f5c28f5cp-1, 0x1.8199999999999p4, 0x1.5b2f845cbb336p1, "event:BURNOUT", "<html><b><i>CG location (cm)</i></b><br>Motor burnout<br>CG location: 24.1 cm<br>Time: 0.73 s<br></html>"),
    IconPin(2, 0x1.bd916872b020cp1, 0x1.bd916872b020cp1, 0x1.8199999999999p4, 0x1.5b2f845cbb336p1, "event:APOGEE", "<html><b><i>CG location (cm)</i></b><br>Apogee<br>CG location: 24.1 cm<br>Time: 3.48 s<br></html>"),
    IconPin(2, 0x1.dd916872b020cp1, 0x1.dd916872b020cp1, 0x1.8199999999999p4, 0x1.5b2f845cbb336p1, "event:RECOVERY_DEVICE_DEPLOYMENT", "<html><b><i>CG location (cm)</i></b><br>Recovery device deployment<br>CG location: 24.1 cm<br>Time: 3.73 s<br></html>"),
    IconPin(2, 0x1.fc6a7ef9db22dp3, 0x1.fc6a7ef9db22dp3, 0x1.8199999999999p4, 0x1.5b2f845cbb336p1, "event:GROUND_HIT", "<html><b><i>CG location (cm)</i></b><br>Ground hit<br>CG location: 24.1 cm<br>Time: 15.89 s<br></html>"),
});
constexpr PlotPin kSimpleStabilityPlot{
    .file = "A simple model rocket.ork",
    .simulation = 0,
    .preset = 4,
    .title = "Simulation 1",
    .subtitle = "Stability vs. time",
    .domainLabel = "Time (s)",
    .domain = {-0x1.0p-1, 0x1.06353f7ced916p4},
    .domainWithEventLines = {-0x1.0p-1, 0x1.06353f7ced916p4},
    .axes = "0->0 1->1 2->1",
    .yAxes = {AxisPin("Stability margin calibers", -0x1.414894c447c3p3, 0x1.e3b9e060fe479p1, true), AxisPin("CP location; CG location", -0x1.64d4fbc7e42e8p6, 0x1.0c9fbe76c8b44p5, true)},
    .colors = "0->0 1->1 2->2",
    .legend = "Stability margin calibers | CP location (cm) | CG location (cm)",
    .series = kSimpleStabilitySeries,
    .toolTips = kSimpleStabilityToolTips,
    .groups = kSimpleStabilityGroups,
    .icons = kSimpleStabilityIcons,
};

// A simple model rocket.ork, simulation 0, preset 5 "Drag coefficients vs. Mach number".
constexpr auto kSimpleDragSeries = std::to_array<SeriesPin>({
    SeriesPin(0, "Drag coefficient (CD)", 0, 0, "\xE2\x80\x8B", 233, 0, {0x1.89374bc6a7efap-8, 0x1.b6c8b43958106p-1, 0x1.16872b020c49cp-4, 0x1.3be76c8b43958p-1, 0x1.89374bc6a7efap-7, 0x1.ccccccccccccdp6}),
    SeriesPin(1, "Friction drag coefficient (CD_friction)", 0, 1, "\xE2\x80\x8B", 233, 1, {0x1.89374bc6a7efap-8, 0x1.547ae147ae148p-1, 0x1.16872b020c49cp-4, 0x1.bb645a1cac083p-2, 0x1.89374bc6a7efap-7, 0.0}),
    SeriesPin(2, "Base drag coefficient (CD_base)", 0, 2, "\xE2\x80\x8B", 233, 2, {0x1.89374bc6a7efap-8, 0x1.eb851eb851eb8p-4, 0x1.16872b020c49cp-4, 0x1.ef9db22d0e56p-4, 0x1.89374bc6a7efap-7, 0.0}),
    SeriesPin(3, "Pressure drag coefficient (CD_pressure)", 0, 3, "\xE2\x80\x8B", 233, 3, {0x1.89374bc6a7efap-8, 0x1.2b020c49ba5e3p-4, 0x1.16872b020c49cp-4, 0x1.020c49ba5e354p-4, 0x1.89374bc6a7efap-7, 0x1.ccccccccccccdp6}),
});
constexpr auto kSimpleDragToolTips = std::to_array<ToolTipPin>({
    ToolTipPin(0, 0, "<html><b><i>Drag coefficient (CD)</i></b><br>Drag coefficient (CD): 0.86 \xE2\x80\x8B<br>Mach number: 0.006 \xE2\x80\x8B<br>Time: 0 s<br></html>"),
    ToolTipPin(0, 116, "<html><b><i>Drag coefficient (CD)</i></b><br>Drag coefficient (CD): 0.62 \xE2\x80\x8B<br>Mach number: 0.068 \xE2\x80\x8B<br>Time: 1.23 s<br></html>"),
    ToolTipPin(0, 232, "<html><b><i>Drag coefficient (CD)</i></b><br>Drag coefficient (CD): 115.2 \xE2\x80\x8B<br>Mach number: 0.012 \xE2\x80\x8B<br>Time: 15.89 s<br></html>"),
    ToolTipPin(1, 0, "<html><b><i>Friction drag coefficient (CD_friction)</i></b><br>Friction drag coefficient (CD_friction): 0.67 \xE2\x80\x8B<br>Mach number: 0.006 \xE2\x80\x8B<br>Time: 0 s<br></html>"),
    ToolTipPin(1, 116, "<html><b><i>Friction drag coefficient (CD_friction)</i></b><br>Friction drag coefficient (CD_friction): 0.43 \xE2\x80\x8B<br>Mach number: 0.068 \xE2\x80\x8B<br>Time: 1.23 s<br></html>"),
    ToolTipPin(1, 232, "<html><b><i>Friction drag coefficient (CD_friction)</i></b><br>Friction drag coefficient (CD_friction): 0 \xE2\x80\x8B<br>Mach number: 0.012 \xE2\x80\x8B<br>Time: 15.89 s<br></html>"),
    ToolTipPin(2, 0, "<html><b><i>Base drag coefficient (CD_base)</i></b><br>Base drag coefficient (CD_base): 0.12 \xE2\x80\x8B<br>Mach number: 0.006 \xE2\x80\x8B<br>Time: 0 s<br></html>"),
    ToolTipPin(2, 116, "<html><b><i>Base drag coefficient (CD_base)</i></b><br>Base drag coefficient (CD_base): 0.12 \xE2\x80\x8B<br>Mach number: 0.068 \xE2\x80\x8B<br>Time: 1.23 s<br></html>"),
    ToolTipPin(2, 232, "<html><b><i>Base drag coefficient (CD_base)</i></b><br>Base drag coefficient (CD_base): 0 \xE2\x80\x8B<br>Mach number: 0.012 \xE2\x80\x8B<br>Time: 15.89 s<br></html>"),
    ToolTipPin(3, 0, "<html><b><i>Pressure drag coefficient (CD_pressure)</i></b><br>Pressure drag coefficient (CD_pressure): 0.073 \xE2\x80\x8B<br>Mach number: 0.006 \xE2\x80\x8B<br>Time: 0 s<br></html>"),
    ToolTipPin(3, 116, "<html><b><i>Pressure drag coefficient (CD_pressure)</i></b><br>Pressure drag coefficient (CD_pressure): 0.063 \xE2\x80\x8B<br>Mach number: 0.068 \xE2\x80\x8B<br>Time: 1.23 s<br></html>"),
    ToolTipPin(3, 232, "<html><b><i>Pressure drag coefficient (CD_pressure)</i></b><br>Pressure drag coefficient (CD_pressure): 115.2 \xE2\x80\x8B<br>Mach number: 0.012 \xE2\x80\x8B<br>Time: 15.89 s<br></html>"),
});
constexpr PlotPin kSimpleDragPlot{
    .file = "A simple model rocket.ork",
    .simulation = 0,
    .preset = 5,
    .title = "Simulation 1",
    .subtitle = "Drag coefficients vs. Mach number",
    .domainLabel = "Mach number",
    .domain = {-0x1.f9db22d0e5604p-2, 0x1.2c083126e978dp-1},
    .domainWithEventLines = {-0x1.f9db22d0e5604p-2, 0x1.2c083126e978dp-1},
    .axes = "0->0 1->0 2->0 3->0",
    .yAxes = {AxisPin("Drag coefficient (CD); Friction drag coefficient (CD_friction); Base drag coefficient (CD_base); Pressure drag coefficient (CD_pressure)", -0x1.ba5e353f7ced9p1, 0x1.da9fbe76c8b44p6, true), AxisPin("", kNaN, kNaN, false)},
    .colors = "0->0 1->1 2->2 3->3",
    .legend = "Drag coefficient (CD) | Friction drag coefficient (CD_friction) | Base drag coefficient (CD_base) | Pressure drag coefficient (CD_pressure)",
    .series = kSimpleDragSeries,
    .toolTips = kSimpleDragToolTips,
    .groups = {},
    .icons = {},
};

// Three stage low power rocket.ork, simulation 0, preset 0 "Vertical motion vs. time".
constexpr auto kThreeStageVerticalSeries = std::to_array<SeriesPin>({
    SeriesPin(0, "Altitude (m)", 0, 0, "m", 559, 0, {0.0, 0.0, 0x1.5353f7ced9168p3, 0x1.01283126e978dp8, 0x1.2048b43958106p6, 0.0}),
    SeriesPin(1, "Booster stage: Altitude (m)", 1, 0, "m", 262, 0, {0.0, 0.0, 0x1.bbe76c8b43958p0, 0x1.a2e76c8b43958p5, 0x1.8b6c8b439581p3, 0.0}),
    SeriesPin(2, "Booster stage: Altitude (m)", 2, 0, "m", 186, 0, {0.0, 0.0, 0x1.cp-1, 0x1.a9916872b020cp3, 0x1.746a7ef9db22dp2, 0.0}),
    SeriesPin(3, "Vertical acceleration (m/s\xC2\xB2)", 0, 2, "m/s\xC2\xB2", 559, 1, {0.0, 0.0, 0x1.5353f7ced9168p3, -0x1.89374bc6a7efap-9, 0x1.2048b43958106p6, -0x1.47ae147ae147bp-4}),
    SeriesPin(4, "Booster stage: Vertical acceleration (m/s\xC2\xB2)", 1, 2, "m/s\xC2\xB2", 262, 1, {0.0, 0.0, 0x1.bbe76c8b43958p0, -0x1.d745a1cac0831p5, 0x1.8b6c8b439581p3, 0x1.47ae147ae147bp-7}),
    SeriesPin(5, "Booster stage: Vertical acceleration (m/s\xC2\xB2)", 2, 2, "m/s\xC2\xB2", 186, 1, {0.0, 0.0, 0x1.cp-1, -0x1.4c28f5c28f5c3p4, 0x1.746a7ef9db22dp2, 0x1.89374bc6a7efap-9}),
    SeriesPin(6, "Vertical velocity (m/s)", 0, 1, "m/s", 559, 6, {0.0, 0.0, 0x1.5353f7ced9168p3, -0x1.0d604189374bcp2, 0x1.2048b43958106p6, -0x1.090624dd2f1aap2}),
    SeriesPin(7, "Booster stage: Vertical velocity (m/s)", 1, 1, "m/s", 262, 6, {0.0, 0.0, 0x1.bbe76c8b43958p0, 0x1.c572b020c49bap5, 0x1.8b6c8b439581p3, -0x1.03c6a7ef9db23p3}),
    SeriesPin(8, "Booster stage: Vertical velocity (m/s)", 2, 1, "m/s", 186, 6, {0.0, 0.0, 0x1.cp-1, 0x1.9ee147ae147aep4, 0x1.746a7ef9db22dp2, -0x1.11604189374bcp3}),
});
constexpr auto kThreeStageVerticalToolTips = std::to_array<ToolTipPin>({
    ToolTipPin(0, 0, "<html><b><i>Sustainer + Booster stage + Booster stage: Altitude (m)</i></b><br>Altitude: 0 m<br>Time: 0 s<br></html>"),
    ToolTipPin(0, 279, "<html><b><i>Sustainer: Altitude (m)</i></b><br>Altitude: 257.16 m<br>Time: 10.6 s<br></html>"),
    ToolTipPin(0, 558, "<html><b><i>Sustainer: Altitude (m)</i></b><br>Altitude: 0 m<br>Time: 72.07 s<br></html>"),
    ToolTipPin(1, 0, "<html><b><i>Booster stage + Booster stage: Altitude (m)</i></b><br>Altitude: 0 m<br>Time: 0 s<br></html>"),
    ToolTipPin(1, 131, "<html><b><i>Booster stage: Altitude (m)</i></b><br>Altitude: 52.36 m<br>Time: 1.73 s<br></html>"),
    ToolTipPin(1, 261, "<html><b><i>Booster stage: Altitude (m)</i></b><br>Altitude: 0 m<br>Time: 12.36 s<br></html>"),
    ToolTipPin(2, 0, "<html><b><i>Booster stage: Altitude (m)</i></b><br>Altitude: 0 m<br>Time: 0 s<br></html>"),
    ToolTipPin(2, 93, "<html><b><i>Booster stage: Altitude (m)</i></b><br>Altitude: 13.3 m<br>Time: 0.88 s<br></html>"),
    ToolTipPin(2, 185, "<html><b><i>Booster stage: Altitude (m)</i></b><br>Altitude: 0 m<br>Time: 5.82 s<br></html>"),
    ToolTipPin(3, 0, "<html><b><i>Sustainer + Booster stage + Booster stage: Vertical acceleration (m/s\xC2\xB2)</i></b><br>Vertical acceleration: 0 m/s\xC2\xB2<br>Time: 0 s<br></html>"),
    ToolTipPin(3, 279, "<html><b><i>Sustainer: Vertical acceleration (m/s\xC2\xB2)</i></b><br>Vertical acceleration: -0.003 m/s\xC2\xB2<br>Time: 10.6 s<br></html>"),
    ToolTipPin(3, 558, "<html><b><i>Sustainer: Vertical acceleration (m/s\xC2\xB2)</i></b><br>Vertical acceleration: -0.08 m/s\xC2\xB2<br>Time: 72.07 s<br></html>"),
    ToolTipPin(4, 0, "<html><b><i>Booster stage + Booster stage: Vertical acceleration (m/s\xC2\xB2)</i></b><br>Vertical acceleration: 0 m/s\xC2\xB2<br>Time: 0 s<br></html>"),
    ToolTipPin(4, 131, "<html><b><i>Booster stage: Vertical acceleration (m/s\xC2\xB2)</i></b><br>Vertical acceleration: -58.91 m/s\xC2\xB2<br>Time: 1.73 s<br></html>"),
    ToolTipPin(4, 261, "<html><b><i>Booster stage: Vertical acceleration (m/s\xC2\xB2)</i></b><br>Vertical acceleration: 0.01 m/s\xC2\xB2<br>Time: 12.36 s<br></html>"),
    ToolTipPin(5, 0, "<html><b><i>Booster stage: Vertical acceleration (m/s\xC2\xB2)</i></b><br>Vertical acceleration: 0 m/s\xC2\xB2<br>Time: 0 s<br></html>"),
    ToolTipPin(5, 93, "<html><b><i>Booster stage: Vertical acceleration (m/s\xC2\xB2)</i></b><br>Vertical acceleration: -20.76 m/s\xC2\xB2<br>Time: 0.88 s<br></html>"),
    ToolTipPin(5, 185, "<html><b><i>Booster stage: Vertical acceleration (m/s\xC2\xB2)</i></b><br>Vertical acceleration: 0.003 m/s\xC2\xB2<br>Time: 5.82 s<br></html>"),
    ToolTipPin(6, 0, "<html><b><i>Sustainer + Booster stage + Booster stage: Vertical velocity (m/s)</i></b><br>Vertical velocity: 0 m/s<br>Time: 0 s<br></html>"),
    ToolTipPin(6, 279, "<html><b><i>Sustainer: Vertical velocity (m/s)</i></b><br>Vertical velocity: -4.21 m/s<br>Time: 10.6 s<br></html>"),
    ToolTipPin(6, 558, "<html><b><i>Sustainer: Vertical velocity (m/s)</i></b><br>Vertical velocity: -4.14 m/s<br>Time: 72.07 s<br></html>"),
    ToolTipPin(7, 0, "<html><b><i>Booster stage + Booster stage: Vertical velocity (m/s)</i></b><br>Vertical velocity: 0 m/s<br>Time: 0 s<br></html>"),
    ToolTipPin(7, 131, "<html><b><i>Booster stage: Vertical velocity (m/s)</i></b><br>Vertical velocity: 56.68 m/s<br>Time: 1.73 s<br></html>"),
    ToolTipPin(7, 261, "<html><b><i>Booster stage: Vertical velocity (m/s)</i></b><br>Vertical velocity: -8.12 m/s<br>Time: 12.36 s<br></html>"),
    ToolTipPin(8, 0, "<html><b><i>Booster stage: Vertical velocity (m/s)</i></b><br>Vertical velocity: 0 m/s<br>Time: 0 s<br></html>"),
    ToolTipPin(8, 93, "<html><b><i>Booster stage: Vertical velocity (m/s)</i></b><br>Vertical velocity: 25.93 m/s<br>Time: 0.88 s<br></html>"),
    ToolTipPin(8, 185, "<html><b><i>Booster stage: Vertical velocity (m/s)</i></b><br>Vertical velocity: -8.54 m/s<br>Time: 5.82 s<br></html>"),
});
constexpr auto kThreeStageVerticalGroups = std::to_array<GroupPin>({
    GroupPin(kAllBranches, 0.0, "IGNITION", "event:IGNITION", "IGNITION@0.0/b0", "Motor ignition"),
    GroupPin(kAllBranches, 0x1.b6c8b43958106p-1, "STAGE_SEPARATION", "event:STAGE_SEPARATION", "BURNOUT@0.857/b0 IGNITION@0.857/b0 STAGE_SEPARATION@0.857/b0", "Motor burnout, Motor ignition, Stage separation"),
    GroupPin(kAllBranches, 0x1.2e147ae147ae1p0, "TUMBLE", "event:TUMBLE", "TUMBLE@1.18/b2", "Tumbling"),
    GroupPin(kAllBranches, 0x1.b6c8b43958106p0, "STAGE_SEPARATION", "event:STAGE_SEPARATION", "BURNOUT@1.714/b0 IGNITION@1.714/b0 STAGE_SEPARATION@1.714/b0", "Motor burnout, Motor ignition, Stage separation"),
    GroupPin(kAllBranches, 0x1.e395810624dd3p0, "TUMBLE", "event:TUMBLE", "TUMBLE@1.889/b1", "Tumbling"),
    GroupPin(kAllBranches, 0x1.12d0e56041893p1, "APOGEE", "event:APOGEE", "APOGEE@2.147/b2", "Apogee"),
    GroupPin(kAllBranches, 0x1.38d4fdf3b645ap1, "BURNOUT", "event:BURNOUT", "BURNOUT@2.444/b0", "Motor burnout"),
    GroupPin(kAllBranches, 0x1.8020c49ba5e35p1, "APOGEE", "event:APOGEE", "APOGEE@3.001/b1", "Apogee"),
    GroupPin(kAllBranches, 0x1.746a7ef9db22dp2, "GROUND_HIT", "event:GROUND_HIT", "GROUND_HIT@5.819/b2", "Ground hit"),
    GroupPin(kAllBranches, 0x1.dc7ae147ae148p2, "RECOVERY_DEVICE_DEPLOYMENT", "event:RECOVERY_DEVICE_DEPLOYMENT", "RECOVERY_DEVICE_DEPLOYMENT@7.445/b0", "Recovery device deployment"),
    GroupPin(kAllBranches, 0x1.e8f5c28f5c28fp2, "APOGEE", "event:APOGEE", "APOGEE@7.64/b0", "Apogee"),
    GroupPin(kAllBranches, 0x1.8b6c8b439581p3, "GROUND_HIT", "event:GROUND_HIT", "GROUND_HIT@12.357/b1", "Ground hit"),
    GroupPin(kAllBranches, 0x1.2048b43958106p6, "GROUND_HIT", "event:GROUND_HIT", "GROUND_HIT@72.071/b0", "Ground hit"),
    GroupPin(0, 0.0, "IGNITION", "event:IGNITION", "IGNITION@0.0/b0", "Motor ignition"),
    GroupPin(0, 0x1.b6c8b43958106p-1, "STAGE_SEPARATION", "event:STAGE_SEPARATION", "BURNOUT@0.857/b0 IGNITION@0.857/b0 STAGE_SEPARATION@0.857/b0", "Motor burnout, Motor ignition, Stage separation"),
    GroupPin(0, 0x1.b6c8b43958106p0, "STAGE_SEPARATION", "event:STAGE_SEPARATION", "BURNOUT@1.714/b0 IGNITION@1.714/b0 STAGE_SEPARATION@1.714/b0", "Motor burnout, Motor ignition, Stage separation"),
    GroupPin(0, 0x1.38d4fdf3b645ap1, "BURNOUT", "event:BURNOUT", "BURNOUT@2.444/b0", "Motor burnout"),
    GroupPin(0, 0x1.dc7ae147ae148p2, "RECOVERY_DEVICE_DEPLOYMENT", "event:RECOVERY_DEVICE_DEPLOYMENT", "RECOVERY_DEVICE_DEPLOYMENT@7.445/b0", "Recovery device deployment"),
    GroupPin(0, 0x1.e8f5c28f5c28fp2, "APOGEE", "event:APOGEE", "APOGEE@7.64/b0", "Apogee"),
    GroupPin(0, 0x1.2048b43958106p6, "GROUND_HIT", "event:GROUND_HIT", "GROUND_HIT@72.071/b0", "Ground hit"),
    GroupPin(1, 0x1.b6c8b43958106p-1, "IGNITION", "event:IGNITION", "IGNITION@0.857/b1", "Motor ignition"),
    GroupPin(1, 0x1.b6c8b43958106p0, "STAGE_SEPARATION", "event:STAGE_SEPARATION", "BURNOUT@1.714/b1 STAGE_SEPARATION@1.714/b1", "Motor burnout, Stage separation"),
    GroupPin(1, 0x1.e395810624dd3p0, "TUMBLE", "event:TUMBLE", "TUMBLE@1.889/b1", "Tumbling"),
    GroupPin(1, 0x1.8020c49ba5e35p1, "APOGEE", "event:APOGEE", "APOGEE@3.001/b1", "Apogee"),
    GroupPin(1, 0x1.8b6c8b439581p3, "GROUND_HIT", "event:GROUND_HIT", "GROUND_HIT@12.357/b1", "Ground hit"),
    GroupPin(2, 0.0, "IGNITION", "event:IGNITION", "IGNITION@0.0/b2", "Motor ignition"),
    GroupPin(2, 0x1.b6c8b43958106p-1, "STAGE_SEPARATION", "event:STAGE_SEPARATION", "BURNOUT@0.857/b2 STAGE_SEPARATION@0.857/b2", "Motor burnout, Stage separation"),
    GroupPin(2, 0x1.2e147ae147ae1p0, "TUMBLE", "event:TUMBLE", "TUMBLE@1.18/b2", "Tumbling"),
    GroupPin(2, 0x1.12d0e56041893p1, "APOGEE", "event:APOGEE", "APOGEE@2.147/b2", "Apogee"),
    GroupPin(2, 0x1.746a7ef9db22dp2, "GROUND_HIT", "event:GROUND_HIT", "GROUND_HIT@5.819/b2", "Ground hit"),
});
constexpr auto kThreeStageVerticalIcons = std::to_array<IconPin>({
    IconPin(2, 0.0, 0.0, 0.0, 0.0, "event:IGNITION", "<html><b><i>Booster stage: Altitude (m)</i></b><br>Motor ignition<br>Altitude: 0 m<br>Time: 0 s<br></html>"),
    IconPin(2, 0x1.b6c8b43958106p-1, 0x1.b6c8b43958106p-1, 0x1.9a6e978d4fdf4p3, 0x1.9a6e978d4fdf4p3, "event:STAGE_SEPARATION", "<html><b><i>Booster stage: Altitude (m)</i></b><br>Motor burnout, Stage separation<br>Altitude: 12.83 m<br>Time: 0.86 s<br></html>"),
    IconPin(2, 0x1.2e147ae147ae1p0, 0x1.2e147ae147ae1p0, 0x1.444dd2f1a9fbep4, 0x1.444dd2f1a9fbep4, "event:TUMBLE", "<html><b><i>Booster stage: Altitude (m)</i></b><br>Tumbling<br>Altitude: 20.27 m<br>Time: 1.18 s<br></html>"),
    IconPin(2, 0x1.12d0e56041893p1, 0x1.12d0e56041893p1, 0x1.abe353f7ced91p4, 0x1.abe353f7ced91p4, "event:APOGEE", "<html><b><i>Booster stage: Altitude (m)</i></b><br>Apogee<br>Altitude: 26.74 m<br>Time: 2.15 s<br></html>"),
    IconPin(2, 0x1.746a7ef9db22dp2, 0x1.746a7ef9db22dp2, 0.0, 0.0, "event:GROUND_HIT", "<html><b><i>Booster stage: Altitude (m)</i></b><br>Ground hit<br>Altitude: 0 m<br>Time: 5.82 s<br></html>"),
    IconPin(5, 0.0, 0.0, 0.0, 0.0, "event:IGNITION", "<html><b><i>Booster stage: Vertical acceleration (m/s\xC2\xB2)</i></b><br>Motor ignition<br>Vertical acceleration: 0 m/s\xC2\xB2<br>Time: 0 s<br></html>"),
    IconPin(5, 0x1.b6c8b43958106p-1, 0x1.b6c8b43958106p-1, -0x1.50b4395810625p4, -0x1.50b4395810625p4, "event:STAGE_SEPARATION", "<html><b><i>Booster stage: Vertical acceleration (m/s\xC2\xB2)</i></b><br>Motor burnout, Stage separation<br>Vertical acceleration: -21.04 m/s\xC2\xB2<br>Time: 0.86 s<br></html>"),
    IconPin(5, 0x1.2e147ae147ae1p0, 0x1.2e147ae147ae1p0, -0x1.e722d0e560419p5, -0x1.e722d0e560419p5, "event:TUMBLE", "<html><b><i>Booster stage: Vertical acceleration (m/s\xC2\xB2)</i></b><br>Tumbling<br>Vertical acceleration: -60.89 m/s\xC2\xB2<br>Time: 1.18 s<br></html>"),
    IconPin(5, 0x1.12d0e56041893p1, 0x1.12d0e56041893p1, -0x1.39cac083126e9p3, -0x1.39cac083126e9p3, "event:APOGEE", "<html><b><i>Booster stage: Vertical acceleration (m/s\xC2\xB2)</i></b><br>Apogee<br>Vertical acceleration: -9.81 m/s\xC2\xB2<br>Time: 2.15 s<br></html>"),
    IconPin(5, 0x1.746a7ef9db22dp2, 0x1.746a7ef9db22dp2, 0x1.89374bc6a7efap-9, 0x1.89374bc6a7efap-9, "event:GROUND_HIT", "<html><b><i>Booster stage: Vertical acceleration (m/s\xC2\xB2)</i></b><br>Ground hit<br>Vertical acceleration: 0.003 m/s\xC2\xB2<br>Time: 5.82 s<br></html>"),
    IconPin(8, 0.0, 0.0, 0.0, 0x1.712381c9e5ae9p-46, "event:IGNITION", "<html><b><i>Booster stage: Vertical velocity (m/s)</i></b><br>Motor ignition<br>Vertical velocity: 0 m/s<br>Time: 0 s<br></html>"),
    IconPin(8, 0x1.b6c8b43958106p-1, 0x1.b6c8b43958106p-1, 0x1.a4f1a9fbe76c9p4, 0x1.7bf9c19b6e20ap6, "event:STAGE_SEPARATION", "<html><b><i>Booster stage: Vertical velocity (m/s)</i></b><br>Motor burnout, Stage separation<br>Vertical velocity: 26.31 m/s<br>Time: 0.86 s<br></html>"),
    IconPin(8, 0x1.2e147ae147ae1p0, 0x1.2e147ae147ae1p0, 0x1.383d70a3d70a4p4, 0x1.19d9ea4779c23p6, "event:TUMBLE", "<html><b><i>Booster stage: Vertical velocity (m/s)</i></b><br>Tumbling<br>Vertical velocity: 19.52 m/s<br>Time: 1.18 s<br></html>"),
    IconPin(8, 0x1.12d0e56041893p1, 0x1.12d0e56041893p1, 0.0, 0x1.712381c9e5ae9p-46, "event:APOGEE", "<html><b><i>Booster stage: Vertical velocity (m/s)</i></b><br>Apogee<br>Vertical velocity: 0 m/s<br>Time: 2.15 s<br></html>"),
    IconPin(8, 0x1.746a7ef9db22dp2, 0x1.746a7ef9db22dp2, -0x1.11604189374bcp3, -0x1.ed8a1258dfa5ap4, "event:GROUND_HIT", "<html><b><i>Booster stage: Vertical velocity (m/s)</i></b><br>Ground hit<br>Vertical velocity: -8.54 m/s<br>Time: 5.82 s<br></html>"),
    IconPin(1, 0x1.b6c8b43958106p-1, 0x1.b6c8b43958106p-1, 0x1.9a6e978d4fdf4p3, 0x1.9a6e978d4fdf4p3, "event:IGNITION", "<html><b><i>Booster stage + Booster stage: Altitude (m)</i></b><br>Motor ignition<br>Altitude: 12.83 m<br>Time: 0.86 s<br></html>"),
    IconPin(1, 0x1.b6c8b43958106p0, 0x1.b6c8b43958106p0, 0x1.99bc6a7ef9db2p5, 0x1.99bc6a7ef9db2p5, "event:STAGE_SEPARATION", "<html><b><i>Booster stage: Altitude (m)</i></b><br>Motor burnout, Stage separation<br>Altitude: 51.22 m<br>Time: 1.71 s<br></html>"),
    IconPin(1, 0x1.e395810624dd3p0, 0x1.e395810624dd3p0, 0x1.e3810624dd2f2p5, 0x1.e3810624dd2f2p5, "event:TUMBLE", "<html><b><i>Booster stage: Altitude (m)</i></b><br>Tumbling<br>Altitude: 60.44 m<br>Time: 1.89 s<br></html>"),
    IconPin(1, 0x1.8020c49ba5e35p1, 0x1.8020c49ba5e35p1, 0x1.1f8a3d70a3d71p6, 0x1.1f8a3d70a3d71p6, "event:APOGEE", "<html><b><i>Booster stage: Altitude (m)</i></b><br>Apogee<br>Altitude: 71.89 m<br>Time: 3 s<br></html>"),
    IconPin(1, 0x1.8b6c8b439581p3, 0x1.8b6c8b439581p3, 0.0, 0.0, "event:GROUND_HIT", "<html><b><i>Booster stage: Altitude (m)</i></b><br>Ground hit<br>Altitude: 0 m<br>Time: 12.36 s<br></html>"),
    IconPin(4, 0x1.b6c8b43958106p-1, 0x1.b6c8b43958106p-1, -0x1.74624dd2f1aap3, -0x1.74624dd2f1aap3, "event:IGNITION", "<html><b><i>Booster stage + Booster stage: Vertical acceleration (m/s\xC2\xB2)</i></b><br>Motor ignition<br>Vertical acceleration: -11.64 m/s\xC2\xB2<br>Time: 0.86 s<br></html>"),
    IconPin(4, 0x1.b6c8b43958106p0, 0x1.b6c8b43958106p0, -0x1.ed916872b020cp5, -0x1.ed916872b020cp5, "event:STAGE_SEPARATION", "<html><b><i>Booster stage: Vertical acceleration (m/s\xC2\xB2)</i></b><br>Motor burnout, Stage separation<br>Vertical acceleration: -61.7 m/s\xC2\xB2<br>Time: 1.71 s<br></html>"),
    IconPin(4, 0x1.e395810624dd3p0, 0x1.e395810624dd3p0, -0x1.50672b020c49cp8, -0x1.50672b020c49cp8, "event:TUMBLE", "<html><b><i>Booster stage: Vertical acceleration (m/s\xC2\xB2)</i></b><br>Tumbling<br>Vertical acceleration: -336.4 m/s\xC2\xB2<br>Time: 1.89 s<br></html>"),
    IconPin(4, 0x1.8020c49ba5e35p1, 0x1.8020c49ba5e35p1, -0x1.39cac083126e9p3, -0x1.39cac083126e9p3, "event:APOGEE", "<html><b><i>Booster stage: Vertical acceleration (m/s\xC2\xB2)</i></b><br>Apogee<br>Vertical acceleration: -9.81 m/s\xC2\xB2<br>Time: 3 s<br></html>"),
    IconPin(4, 0x1.8b6c8b439581p3, 0x1.8b6c8b439581p3, 0x1.47ae147ae147bp-7, 0x1.47ae147ae147bp-7, "event:GROUND_HIT", "<html><b><i>Booster stage: Vertical acceleration (m/s\xC2\xB2)</i></b><br>Ground hit<br>Vertical acceleration: 0.01 m/s\xC2\xB2<br>Time: 12.36 s<br></html>"),
    IconPin(7, 0x1.b6c8b43958106p-1, 0x1.b6c8b43958106p-1, 0x1.a4f1a9fbe76c9p4, 0x1.7bf9c19b6e20ap6, "event:IGNITION", "<html><b><i>Booster stage + Booster stage: Vertical velocity (m/s)</i></b><br>Motor ignition<br>Vertical velocity: 26.31 m/s<br>Time: 0.86 s<br></html>"),
    IconPin(7, 0x1.b6c8b43958106p0, 0x1.b6c8b43958106p0, 0x1.cf1cac083126fp5, 0x1.a20a23c66a852p7, "event:STAGE_SEPARATION", "<html><b><i>Booster stage: Vertical velocity (m/s)</i></b><br>Motor burnout, Stage separation<br>Vertical velocity: 57.89 m/s<br>Time: 1.71 s<br></html>"),
    IconPin(7, 0x1.e395810624dd3p0, 0x1.e395810624dd3p0, 0x1.77bc6a7ef9db2p5, 0x1.532ae002ef022p7, "event:TUMBLE", "<html><b><i>Booster stage: Vertical velocity (m/s)</i></b><br>Tumbling<br>Vertical velocity: 46.97 m/s<br>Time: 1.89 s<br></html>"),
    IconPin(7, 0x1.8020c49ba5e35p1, 0x1.8020c49ba5e35p1, 0.0, 0x1.712381c9e5ae9p-46, "event:APOGEE", "<html><b><i>Booster stage: Vertical velocity (m/s)</i></b><br>Apogee<br>Vertical velocity: 0 m/s<br>Time: 3 s<br></html>"),
    IconPin(7, 0x1.8b6c8b439581p3, 0x1.8b6c8b439581p3, -0x1.03c6a7ef9db23p3, -0x1.d4fc90baefabap4, "event:GROUND_HIT", "<html><b><i>Booster stage: Vertical velocity (m/s)</i></b><br>Ground hit<br>Vertical velocity: -8.12 m/s<br>Time: 12.36 s<br></html>"),
    IconPin(0, 0.0, 0.0, 0.0, 0.0, "event:IGNITION", "<html><b><i>Sustainer + Booster stage + Booster stage: Altitude (m)</i></b><br>Motor ignition<br>Altitude: 0 m<br>Time: 0 s<br></html>"),
    IconPin(0, 0x1.b6c8b43958106p-1, 0x1.b6c8b43958106p-1, 0x1.9a6e978d4fdf4p3, 0x1.9a6e978d4fdf4p3, "event:STAGE_SEPARATION", "<html><b><i>Sustainer + Booster stage + Booster stage: Altitude (m)</i></b><br>Motor burnout, Motor ignition, Stage separation<br>Altitude: 12.83 m<br>Time: 0.86 s<br></html>"),
    IconPin(0, 0x1.b6c8b43958106p0, 0x1.b6c8b43958106p0, 0x1.99bc6a7ef9db2p5, 0x1.99bc6a7ef9db2p5, "event:STAGE_SEPARATION", "<html><b><i>Sustainer + Booster stage: Altitude (m)</i></b><br>Motor burnout, Motor ignition, Stage separation<br>Altitude: 51.22 m<br>Time: 1.71 s<br></html>"),
    IconPin(0, 0x1.38d4fdf3b645ap1, 0x1.38d4fdf3b645ap1, 0x1.990a3d70a3d71p6, 0x1.990a3d70a3d71p6, "event:BURNOUT", "<html><b><i>Sustainer: Altitude (m)</i></b><br>Motor burnout<br>Altitude: 102.26 m<br>Time: 2.44 s<br></html>"),
    IconPin(0, 0x1.dc7ae147ae148p2, 0x1.dc7ae147ae148p2, 0x1.0c14a6921735fp8, 0x1.0c14a6921735fp8, "event:RECOVERY_DEVICE_DEPLOYMENT", "<html><b><i>Sustainer: Altitude (m)</i></b><br>Recovery device deployment<br>Altitude: 268.08 m<br>Time: 7.45 s<br></html>"),
    IconPin(0, 0x1.e8f5c28f5c28fp2, 0x1.e8f5c28f5c28fp2, 0x1.0c58d4fdf3b64p8, 0x1.0c58d4fdf3b64p8, "event:APOGEE", "<html><b><i>Sustainer: Altitude (m)</i></b><br>Apogee<br>Altitude: 268.35 m<br>Time: 7.64 s<br></html>"),
    IconPin(0, 0x1.2048b43958106p6, 0x1.2048b43958106p6, 0.0, 0.0, "event:GROUND_HIT", "<html><b><i>Sustainer: Altitude (m)</i></b><br>Ground hit<br>Altitude: 0 m<br>Time: 72.07 s<br></html>"),
    IconPin(3, 0.0, 0.0, 0.0, 0.0, "event:IGNITION", "<html><b><i>Sustainer + Booster stage + Booster stage: Vertical acceleration (m/s\xC2\xB2)</i></b><br>Motor ignition<br>Vertical acceleration: 0 m/s\xC2\xB2<br>Time: 0 s<br></html>"),
    IconPin(3, 0x1.b6c8b43958106p-1, 0x1.b6c8b43958106p-1, -0x1.74624dd2f1aap3, -0x1.74624dd2f1aap3, "event:STAGE_SEPARATION", "<html><b><i>Sustainer + Booster stage + Booster stage: Vertical acceleration (m/s\xC2\xB2)</i></b><br>Motor burnout, Motor ignition, Stage separation<br>Vertical acceleration: -11.64 m/s\xC2\xB2<br>Time: 0.86 s<br></html>"),
    IconPin(3, 0x1.b6c8b43958106p0, 0x1.b6c8b43958106p0, -0x1.38d916872b021p4, -0x1.38d916872b021p4, "event:STAGE_SEPARATION", "<html><b><i>Sustainer + Booster stage: Vertical acceleration (m/s\xC2\xB2)</i></b><br>Motor burnout, Motor ignition, Stage separation<br>Vertical acceleration: -19.55 m/s\xC2\xB2<br>Time: 1.71 s<br></html>"),
    IconPin(3, 0x1.38d4fdf3b645ap1, 0x1.38d4fdf3b645ap1, -0x1.bb2b020c49ba6p4, -0x1.bb2b020c49ba6p4, "event:BURNOUT", "<html><b><i>Sustainer: Vertical acceleration (m/s\xC2\xB2)</i></b><br>Motor burnout<br>Vertical acceleration: -27.7 m/s\xC2\xB2<br>Time: 2.44 s<br></html>"),
    IconPin(3, 0x1.dc7ae147ae148p2, 0x1.dc7ae147ae148p2, -0x1.f5b4e81b4ebfcp3, -0x1.f5b4e81b4ebfcp3, "event:RECOVERY_DEVICE_DEPLOYMENT", "<html><b><i>Sustainer: Vertical acceleration (m/s\xC2\xB2)</i></b><br>Recovery device deployment<br>Vertical acceleration: -15.68 m/s\xC2\xB2<br>Time: 7.45 s<br></html>"),
    IconPin(3, 0x1.e8f5c28f5c28fp2, 0x1.e8f5c28f5c28fp2, -0x1.39c28f5c28f5cp3, -0x1.39c28f5c28f5cp3, "event:APOGEE", "<html><b><i>Sustainer: Vertical acceleration (m/s\xC2\xB2)</i></b><br>Apogee<br>Vertical acceleration: -9.8 m/s\xC2\xB2<br>Time: 7.64 s<br></html>"),
    IconPin(3, 0x1.2048b43958106p6, 0x1.2048b43958106p6, -0x1.47ae147ae147bp-4, -0x1.47ae147ae147bp-4, "event:GROUND_HIT", "<html><b><i>Sustainer: Vertical acceleration (m/s\xC2\xB2)</i></b><br>Ground hit<br>Vertical acceleration: -0.08 m/s\xC2\xB2<br>Time: 72.07 s<br></html>"),
    IconPin(6, 0.0, 0.0, 0.0, 0x1.712381c9e5ae9p-46, "event:IGNITION", "<html><b><i>Sustainer + Booster stage + Booster stage: Vertical velocity (m/s)</i></b><br>Motor ignition<br>Vertical velocity: 0 m/s<br>Time: 0 s<br></html>"),
    IconPin(6, 0x1.b6c8b43958106p-1, 0x1.b6c8b43958106p-1, 0x1.a4f1a9fbe76c9p4, 0x1.7bf9c19b6e20ap6, "event:STAGE_SEPARATION", "<html><b><i>Sustainer + Booster stage + Booster stage: Vertical velocity (m/s)</i></b><br>Motor burnout, Motor ignition, Stage separation<br>Vertical velocity: 26.31 m/s<br>Time: 0.86 s<br></html>"),
    IconPin(6, 0x1.b6c8b43958106p0, 0x1.b6c8b43958106p0, 0x1.cf1cac083126fp5, 0x1.a20a23c66a852p7, "event:STAGE_SEPARATION", "<html><b><i>Sustainer + Booster stage: Vertical velocity (m/s)</i></b><br>Motor burnout, Motor ignition, Stage separation<br>Vertical velocity: 57.89 m/s<br>Time: 1.71 s<br></html>"),
    IconPin(6, 0x1.38d4fdf3b645ap1, 0x1.38d4fdf3b645ap1, 0x1.2f04189374bc7p6, 0x1.118663982a693p8, "event:BURNOUT", "<html><b><i>Sustainer: Vertical velocity (m/s)</i></b><br>Motor burnout<br>Vertical velocity: 75.75 m/s<br>Time: 2.44 s<br></html>"),
    IconPin(6, 0x1.dc7ae147ae148p2, 0x1.dc7ae147ae148p2, 0x1.a189374bc6a7ap1, 0x1.78e63824b480fp3, "event:RECOVERY_DEVICE_DEPLOYMENT", "<html><b><i>Sustainer: Vertical velocity (m/s)</i></b><br>Recovery device deployment<br>Vertical velocity: 3.26 m/s<br>Time: 7.45 s<br></html>"),
    IconPin(6, 0x1.e8f5c28f5c28fp2, 0x1.e8f5c28f5c28fp2, 0.0, 0x1.712381c9e5ae9p-46, "event:APOGEE", "<html><b><i>Sustainer: Vertical velocity (m/s)</i></b><br>Apogee<br>Vertical velocity: 0 m/s<br>Time: 7.64 s<br></html>"),
    IconPin(6, 0x1.2048b43958106p6, 0x1.2048b43958106p6, -0x1.090624dd2f1aap2, -0x1.de76080487e79p3, "event:GROUND_HIT", "<html><b><i>Sustainer: Vertical velocity (m/s)</i></b><br>Ground hit<br>Vertical velocity: -4.14 m/s<br>Time: 72.07 s<br></html>"),
});
constexpr PlotPin kThreeStageVerticalPlot{
    .file = "Three stage low power rocket.ork",
    .simulation = 0,
    .preset = 0,
    .title = "Simulation 1",
    .subtitle = "Vertical motion vs. time",
    .domainLabel = "Time (s)",
    .domain = {-0x1.0p-1, 0x1.2248b43958106p6},
    .domainWithEventLines = {-0x1.0p-1, 0x1.232fd4bf0995bp6},
    .axes = "0->0 1->1 2->0",
    .yAxes = {AxisPin("Altitude; Vertical acceleration", -0x1.628ba5e353f7dp8, 0x1.1e7d4fdf3b645p8, true), AxisPin("Vertical velocity", -0x1.88c5a55305315p6, 0x1.3d60dc33721d6p6, true)},
    .colors = "0->0 1->6 2->1",
    .legend = "Altitude (m) | Vertical acceleration (m/s\xC2\xB2) | Vertical velocity (m/s)",
    .series = kThreeStageVerticalSeries,
    .toolTips = kThreeStageVerticalToolTips,
    .groups = kThreeStageVerticalGroups,
    .icons = kThreeStageVerticalIcons,
};

// Three stage low power rocket.ork, simulation 0, preset 2 "Flight side profile".
constexpr auto kThreeStageSideProfileSeries = std::to_array<SeriesPin>({
    SeriesPin(0, "Altitude (m)", 0, 0, "m", 559, 0, {0.0, 0.0, 0x1.05bb645a1cac1p6, 0x1.01283126e978dp8, -0x1.c99ba5e353f7dp5, 0.0}),
    SeriesPin(1, "Booster stage: Altitude (m)", 1, 0, "m", 262, 0, {0.0, 0.0, 0x1.9ac083126e979p2, 0x1.a2e76c8b43958p5, -0x1.978d4fdf3b646p3, 0.0}),
    SeriesPin(2, "Booster stage: Altitude (m)", 2, 0, "m", 186, 0, {0.0, 0.0, 0x1.178d4fdf3b646p-1, 0x1.a9916872b020cp3, -0x1.174395810624ep3, 0.0}),
});
constexpr auto kThreeStageSideProfileToolTips = std::to_array<ToolTipPin>({
    ToolTipPin(0, 0, "<html><b><i>Sustainer + Booster stage + Booster stage: Altitude (m)</i></b><br>Altitude: 0 m<br>Position East of launch: 0 m<br>Time: 0 s<br></html>"),
    ToolTipPin(0, 279, "<html><b><i>Sustainer: Altitude (m)</i></b><br>Altitude: 257.16 m<br>Position East of launch: 65.43 m<br>Time: 10.6 s<br></html>"),
    ToolTipPin(0, 558, "<html><b><i>Sustainer: Altitude (m)</i></b><br>Altitude: 0 m<br>Position East of launch: -57.2 m<br>Time: 72.07 s<br></html>"),
    ToolTipPin(1, 0, "<html><b><i>Booster stage + Booster stage: Altitude (m)</i></b><br>Altitude: 0 m<br>Position East of launch: 0 m<br>Time: 0 s<br></html>"),
    ToolTipPin(1, 131, "<html><b><i>Booster stage: Altitude (m)</i></b><br>Altitude: 52.36 m<br>Position East of launch: 6.42 m<br>Time: 1.73 s<br></html>"),
    ToolTipPin(1, 261, "<html><b><i>Booster stage: Altitude (m)</i></b><br>Altitude: 0 m<br>Position East of launch: -12.74 m<br>Time: 12.36 s<br></html>"),
    ToolTipPin(2, 0, "<html><b><i>Booster stage: Altitude (m)</i></b><br>Altitude: 0 m<br>Position East of launch: 0 m<br>Time: 0 s<br></html>"),
    ToolTipPin(2, 93, "<html><b><i>Booster stage: Altitude (m)</i></b><br>Altitude: 13.3 m<br>Position East of launch: 0.55 m<br>Time: 0.88 s<br></html>"),
    ToolTipPin(2, 185, "<html><b><i>Booster stage: Altitude (m)</i></b><br>Altitude: 0 m<br>Position East of launch: -8.73 m<br>Time: 5.82 s<br></html>"),
});
constexpr auto kThreeStageSideProfileGroups = std::to_array<GroupPin>({
    GroupPin(kAllBranches, 0.0, "IGNITION", "event:IGNITION", "IGNITION@0.0/b0", "Motor ignition"),
    GroupPin(kAllBranches, 0x1.b6c8b43958106p-1, "STAGE_SEPARATION", "event:STAGE_SEPARATION", "BURNOUT@0.857/b0 IGNITION@0.857/b0 STAGE_SEPARATION@0.857/b0", "Motor burnout, Motor ignition, Stage separation"),
    GroupPin(kAllBranches, 0x1.2e147ae147ae1p0, "TUMBLE", "event:TUMBLE", "TUMBLE@1.18/b2", "Tumbling"),
    GroupPin(kAllBranches, 0x1.b6c8b43958106p0, "STAGE_SEPARATION", "event:STAGE_SEPARATION", "BURNOUT@1.714/b0 IGNITION@1.714/b0 STAGE_SEPARATION@1.714/b0", "Motor burnout, Motor ignition, Stage separation"),
    GroupPin(kAllBranches, 0x1.e395810624dd3p0, "TUMBLE", "event:TUMBLE", "TUMBLE@1.889/b1", "Tumbling"),
    GroupPin(kAllBranches, 0x1.12d0e56041893p1, "APOGEE", "event:APOGEE", "APOGEE@2.147/b2", "Apogee"),
    GroupPin(kAllBranches, 0x1.38d4fdf3b645ap1, "BURNOUT", "event:BURNOUT", "BURNOUT@2.444/b0", "Motor burnout"),
    GroupPin(kAllBranches, 0x1.8020c49ba5e35p1, "APOGEE", "event:APOGEE", "APOGEE@3.001/b1", "Apogee"),
    GroupPin(kAllBranches, 0x1.746a7ef9db22dp2, "GROUND_HIT", "event:GROUND_HIT", "GROUND_HIT@5.819/b2", "Ground hit"),
    GroupPin(kAllBranches, 0x1.dc7ae147ae148p2, "RECOVERY_DEVICE_DEPLOYMENT", "event:RECOVERY_DEVICE_DEPLOYMENT", "RECOVERY_DEVICE_DEPLOYMENT@7.445/b0", "Recovery device deployment"),
    GroupPin(kAllBranches, 0x1.e8f5c28f5c28fp2, "APOGEE", "event:APOGEE", "APOGEE@7.64/b0", "Apogee"),
    GroupPin(kAllBranches, 0x1.8b6c8b439581p3, "GROUND_HIT", "event:GROUND_HIT", "GROUND_HIT@12.357/b1", "Ground hit"),
    GroupPin(kAllBranches, 0x1.2048b43958106p6, "GROUND_HIT", "event:GROUND_HIT", "GROUND_HIT@72.071/b0", "Ground hit"),
    GroupPin(0, 0.0, "IGNITION", "event:IGNITION", "IGNITION@0.0/b0", "Motor ignition"),
    GroupPin(0, 0x1.b6c8b43958106p-1, "STAGE_SEPARATION", "event:STAGE_SEPARATION", "BURNOUT@0.857/b0 IGNITION@0.857/b0 STAGE_SEPARATION@0.857/b0", "Motor burnout, Motor ignition, Stage separation"),
    GroupPin(0, 0x1.b6c8b43958106p0, "STAGE_SEPARATION", "event:STAGE_SEPARATION", "BURNOUT@1.714/b0 IGNITION@1.714/b0 STAGE_SEPARATION@1.714/b0", "Motor burnout, Motor ignition, Stage separation"),
    GroupPin(0, 0x1.38d4fdf3b645ap1, "BURNOUT", "event:BURNOUT", "BURNOUT@2.444/b0", "Motor burnout"),
    GroupPin(0, 0x1.dc7ae147ae148p2, "RECOVERY_DEVICE_DEPLOYMENT", "event:RECOVERY_DEVICE_DEPLOYMENT", "RECOVERY_DEVICE_DEPLOYMENT@7.445/b0", "Recovery device deployment"),
    GroupPin(0, 0x1.e8f5c28f5c28fp2, "APOGEE", "event:APOGEE", "APOGEE@7.64/b0", "Apogee"),
    GroupPin(0, 0x1.2048b43958106p6, "GROUND_HIT", "event:GROUND_HIT", "GROUND_HIT@72.071/b0", "Ground hit"),
    GroupPin(1, 0x1.b6c8b43958106p-1, "IGNITION", "event:IGNITION", "IGNITION@0.857/b1", "Motor ignition"),
    GroupPin(1, 0x1.b6c8b43958106p0, "STAGE_SEPARATION", "event:STAGE_SEPARATION", "BURNOUT@1.714/b1 STAGE_SEPARATION@1.714/b1", "Motor burnout, Stage separation"),
    GroupPin(1, 0x1.e395810624dd3p0, "TUMBLE", "event:TUMBLE", "TUMBLE@1.889/b1", "Tumbling"),
    GroupPin(1, 0x1.8020c49ba5e35p1, "APOGEE", "event:APOGEE", "APOGEE@3.001/b1", "Apogee"),
    GroupPin(1, 0x1.8b6c8b439581p3, "GROUND_HIT", "event:GROUND_HIT", "GROUND_HIT@12.357/b1", "Ground hit"),
    GroupPin(2, 0.0, "IGNITION", "event:IGNITION", "IGNITION@0.0/b2", "Motor ignition"),
    GroupPin(2, 0x1.b6c8b43958106p-1, "STAGE_SEPARATION", "event:STAGE_SEPARATION", "BURNOUT@0.857/b2 STAGE_SEPARATION@0.857/b2", "Motor burnout, Stage separation"),
    GroupPin(2, 0x1.2e147ae147ae1p0, "TUMBLE", "event:TUMBLE", "TUMBLE@1.18/b2", "Tumbling"),
    GroupPin(2, 0x1.12d0e56041893p1, "APOGEE", "event:APOGEE", "APOGEE@2.147/b2", "Apogee"),
    GroupPin(2, 0x1.746a7ef9db22dp2, "GROUND_HIT", "event:GROUND_HIT", "GROUND_HIT@5.819/b2", "Ground hit"),
});
constexpr auto kThreeStageSideProfileIcons = std::to_array<IconPin>({
    IconPin(2, 0.0, 0.0, 0.0, 0.0, "event:IGNITION", "<html><b><i>Booster stage: Altitude (m)</i></b><br>Motor ignition<br>Altitude: 0 m<br>Position East of launch: 0 m<br>Time: 0 s<br></html>"),
    IconPin(2, 0x1.b6c8b43958106p-1, 0x1.03126e978d4fep-1, 0x1.9a6e978d4fdf4p3, 0x1.9a6e978d4fdf4p3, "event:STAGE_SEPARATION", "<html><b><i>Booster stage: Altitude (m)</i></b><br>Motor burnout, Stage separation<br>Altitude: 12.83 m<br>Position East of launch: 0.51 m<br>Time: 0.86 s<br></html>"),
    IconPin(2, 0x1.2e147ae147ae1p0, 0x1.6e978d4fdf3b6p-1, 0x1.444dd2f1a9fbep4, 0x1.444dd2f1a9fbep4, "event:TUMBLE", "<html><b><i>Booster stage: Altitude (m)</i></b><br>Tumbling<br>Altitude: 20.27 m<br>Position East of launch: 0.72 m<br>Time: 1.18 s<br></html>"),
    IconPin(2, 0x1.12d0e56041893p1, -0x1.48b4395810625p0, 0x1.abe353f7ced91p4, 0x1.abe353f7ced91p4, "event:APOGEE", "<html><b><i>Booster stage: Altitude (m)</i></b><br>Apogee<br>Altitude: 26.74 m<br>Position East of launch: -1.28 m<br>Time: 2.15 s<br></html>"),
    IconPin(2, 0x1.746a7ef9db22dp2, -0x1.174395810624ep3, 0.0, 0.0, "event:GROUND_HIT", "<html><b><i>Booster stage: Altitude (m)</i></b><br>Ground hit<br>Altitude: 0 m<br>Position East of launch: -8.73 m<br>Time: 5.82 s<br></html>"),
    IconPin(1, 0x1.b6c8b43958106p-1, 0x1.03126e978d4fep-1, 0x1.9a6e978d4fdf4p3, 0x1.9a6e978d4fdf4p3, "event:IGNITION", "<html><b><i>Booster stage + Booster stage: Altitude (m)</i></b><br>Motor ignition<br>Altitude: 12.83 m<br>Position East of launch: 0.51 m<br>Time: 0.86 s<br></html>"),
    IconPin(1, 0x1.b6c8b43958106p0, 0x1.8e147ae147ae1p2, 0x1.99bc6a7ef9db2p5, 0x1.99bc6a7ef9db2p5, "event:STAGE_SEPARATION", "<html><b><i>Booster stage: Altitude (m)</i></b><br>Motor burnout, Stage separation<br>Altitude: 51.22 m<br>Position East of launch: 6.22 m<br>Time: 1.71 s<br></html>"),
    IconPin(1, 0x1.e395810624dd3p0, 0x1.d4ccccccccccdp2, 0x1.e3810624dd2f2p5, 0x1.e3810624dd2f2p5, "event:TUMBLE", "<html><b><i>Booster stage: Altitude (m)</i></b><br>Tumbling<br>Altitude: 60.44 m<br>Position East of launch: 7.33 m<br>Time: 1.89 s<br></html>"),
    IconPin(1, 0x1.8020c49ba5e35p1, 0x1.4ee978d4fdf3bp2, 0x1.1f8a3d70a3d71p6, 0x1.1f8a3d70a3d71p6, "event:APOGEE", "<html><b><i>Booster stage: Altitude (m)</i></b><br>Apogee<br>Altitude: 71.89 m<br>Position East of launch: 5.23 m<br>Time: 3 s<br></html>"),
    IconPin(1, 0x1.8b6c8b439581p3, -0x1.978d4fdf3b646p3, 0.0, 0.0, "event:GROUND_HIT", "<html><b><i>Booster stage: Altitude (m)</i></b><br>Ground hit<br>Altitude: 0 m<br>Position East of launch: -12.74 m<br>Time: 12.36 s<br></html>"),
    IconPin(0, 0.0, 0.0, 0.0, 0.0, "event:IGNITION", "<html><b><i>Sustainer + Booster stage + Booster stage: Altitude (m)</i></b><br>Motor ignition<br>Altitude: 0 m<br>Position East of launch: 0 m<br>Time: 0 s<br></html>"),
    IconPin(0, 0x1.b6c8b43958106p-1, 0x1.03126e978d4fep-1, 0x1.9a6e978d4fdf4p3, 0x1.9a6e978d4fdf4p3, "event:STAGE_SEPARATION", "<html><b><i>Sustainer + Booster stage + Booster stage: Altitude (m)</i></b><br>Motor burnout, Motor ignition, Stage separation<br>Altitude: 12.83 m<br>Position East of launch: 0.51 m<br>Time: 0.86 s<br></html>"),
    IconPin(0, 0x1.b6c8b43958106p0, 0x1.8e147ae147ae1p2, 0x1.99bc6a7ef9db2p5, 0x1.99bc6a7ef9db2p5, "event:STAGE_SEPARATION", "<html><b><i>Sustainer + Booster stage: Altitude (m)</i></b><br>Motor burnout, Motor ignition, Stage separation<br>Altitude: 51.22 m<br>Position East of launch: 6.22 m<br>Time: 1.71 s<br></html>"),
    IconPin(0, 0x1.38d4fdf3b645ap1, 0x1.010a3d70a3d71p4, 0x1.990a3d70a3d71p6, 0x1.990a3d70a3d71p6, "event:BURNOUT", "<html><b><i>Sustainer: Altitude (m)</i></b><br>Motor burnout<br>Altitude: 102.26 m<br>Position East of launch: 16.07 m<br>Time: 2.44 s<br></html>"),
    IconPin(0, 0x1.dc7ae147ae148p2, 0x1.12162fc962fc9p6, 0x1.0c14a6921735fp8, 0x1.0c14a6921735fp8, "event:RECOVERY_DEVICE_DEPLOYMENT", "<html><b><i>Sustainer: Altitude (m)</i></b><br>Recovery device deployment<br>Altitude: 268.08 m<br>Position East of launch: 68.52 m<br>Time: 7.45 s<br></html>"),
    IconPin(0, 0x1.e8f5c28f5c28fp2, 0x1.15e6666666666p6, 0x1.0c58d4fdf3b64p8, 0x1.0c58d4fdf3b64p8, "event:APOGEE", "<html><b><i>Sustainer: Altitude (m)</i></b><br>Apogee<br>Altitude: 268.35 m<br>Position East of launch: 69.47 m<br>Time: 7.64 s<br></html>"),
    IconPin(0, 0x1.2048b43958106p6, -0x1.c99ba5e353f7dp5, 0.0, 0.0, "event:GROUND_HIT", "<html><b><i>Sustainer: Altitude (m)</i></b><br>Ground hit<br>Altitude: 0 m<br>Position East of launch: -57.2 m<br>Time: 72.07 s<br></html>"),
});
constexpr PlotPin kThreeStageSideProfilePlot{
    .file = "Three stage low power rocket.ork",
    .simulation = 0,
    .preset = 2,
    .title = "Simulation 1",
    .subtitle = "Flight side profile",
    .domainLabel = "Position East of launch (m)",
    .domain = {-0x1.cd9ba5e353f7dp5, 0x1.19ccccccccccdp6},
    .domainWithEventLines = {-0x1.cd9ba5e353f7dp5, 0x1.19ccccccccccdp6},
    .axes = "0->0",
    .yAxes = {AxisPin("Altitude", -0x1.019cf56eac86p3, 0x1.1465bca9691a7p8, true), AxisPin("", kNaN, kNaN, false)},
    .colors = "0->0",
    .legend = "Altitude (m)",
    .series = kThreeStageSideProfileSeries,
    .toolTips = kThreeStageSideProfileToolTips,
    .groups = kThreeStageSideProfileGroups,
    .icons = kThreeStageSideProfileIcons,
};

// 3D printable nose cone and fins.ork, simulation 2, preset 0 "Vertical motion vs. time".
constexpr auto kPrintableVerticalSeries = std::to_array<SeriesPin>({
    SeriesPin(0, "Altitude (m)", 0, 0, "m", 487, 0, {0.0, 0.0, 0x1.d45a1cac08312p2, 0x1.d81a9fbe76c8bp7, 0x1.ef72b020c49bap5, 0.0}),
    SeriesPin(1, "Vertical acceleration (m/s\xC2\xB2)", 0, 2, "m/s\xC2\xB2", 487, 1, {0.0, 0.0, 0x1.d45a1cac08312p2, 0x1.89374bc6a7efap-9, 0x1.ef72b020c49bap5, -0x1.90a9300f92444p-11}),
    SeriesPin(2, "Vertical velocity (m/s)", 0, 1, "m/s", 487, 2, {0.0, 0.0, 0x1.d45a1cac08312p2, -0x1.16353f7ced917p2, 0x1.ef72b020c49bap5, -0x1.13126e978d4fep2}),
});
constexpr auto kPrintableVerticalToolTips = std::to_array<ToolTipPin>({
    ToolTipPin(0, 0, "<html><b><i>Altitude (m)</i></b><br>Altitude: 0 m<br>Time: 0 s<br></html>"),
    ToolTipPin(0, 243, "<html><b><i>Altitude (m)</i></b><br>Altitude: 236.05 m<br>Time: 7.32 s<br></html>"),
    ToolTipPin(0, 486, "<html><b><i>Altitude (m)</i></b><br>Altitude: 0 m<br>Time: 61.93 s<br></html>"),
    ToolTipPin(1, 0, "<html><b><i>Vertical acceleration (m/s\xC2\xB2)</i></b><br>Vertical acceleration: 0 m/s\xC2\xB2<br>Time: 0 s<br></html>"),
    ToolTipPin(1, 243, "<html><b><i>Vertical acceleration (m/s\xC2\xB2)</i></b><br>Vertical acceleration: 0.003 m/s\xC2\xB2<br>Time: 7.32 s<br></html>"),
    ToolTipPin(1, 486, "<html><b><i>Vertical acceleration (m/s\xC2\xB2)</i></b><br>Vertical acceleration: -0.00076 m/s\xC2\xB2<br>Time: 61.93 s<br></html>"),
    ToolTipPin(2, 0, "<html><b><i>Vertical velocity (m/s)</i></b><br>Vertical velocity: 0 m/s<br>Time: 0 s<br></html>"),
    ToolTipPin(2, 243, "<html><b><i>Vertical velocity (m/s)</i></b><br>Vertical velocity: -4.35 m/s<br>Time: 7.32 s<br></html>"),
    ToolTipPin(2, 486, "<html><b><i>Vertical velocity (m/s)</i></b><br>Vertical velocity: -4.3 m/s<br>Time: 61.93 s<br></html>"),
});
constexpr auto kPrintableVerticalGroups = std::to_array<GroupPin>({
    GroupPin(kAllBranches, 0.0, "IGNITION", "event:IGNITION", "IGNITION@0.0/b0", "Motor ignition"),
    GroupPin(kAllBranches, 0x1.dc28f5c28f5c3p0, "BURNOUT", "event:BURNOUT", "BURNOUT@1.86/b0", "Motor burnout"),
    GroupPin(kAllBranches, 0x1.371a9fbe76c8bp2, "SIM_WARN", "warning:NORMAL", "RECOVERY_DEVICE_DEPLOYMENT@4.861/b0 SIM_WARN@4.863/b0", "Recovery device deployment, Warning"),
    GroupPin(kAllBranches, 0x1.5b95810624dd3p2, "APOGEE", "event:APOGEE", "APOGEE@5.431/b0", "Apogee"),
    GroupPin(kAllBranches, 0x1.ef72b020c49bap5, "GROUND_HIT", "event:GROUND_HIT", "GROUND_HIT@61.931/b0", "Ground hit"),
    GroupPin(0, 0.0, "IGNITION", "event:IGNITION", "IGNITION@0.0/b0", "Motor ignition"),
    GroupPin(0, 0x1.dc28f5c28f5c3p0, "BURNOUT", "event:BURNOUT", "BURNOUT@1.86/b0", "Motor burnout"),
    GroupPin(0, 0x1.371a9fbe76c8bp2, "SIM_WARN", "warning:NORMAL", "RECOVERY_DEVICE_DEPLOYMENT@4.861/b0 SIM_WARN@4.863/b0", "Recovery device deployment, Warning"),
    GroupPin(0, 0x1.5b95810624dd3p2, "APOGEE", "event:APOGEE", "APOGEE@5.431/b0", "Apogee"),
    GroupPin(0, 0x1.ef72b020c49bap5, "GROUND_HIT", "event:GROUND_HIT", "GROUND_HIT@61.931/b0", "Ground hit"),
});
constexpr auto kPrintableVerticalIcons = std::to_array<IconPin>({
    IconPin(0, 0.0, 0.0, 0.0, 0.0, "event:IGNITION", "<html><b><i>Altitude (m)</i></b><br>Motor ignition<br>Altitude: 0 m<br>Time: 0 s<br></html>"),
    IconPin(0, 0x1.dc28f5c28f5c3p0, 0x1.dc28f5c28f5c3p0, 0x1.64fced916872bp6, 0x1.64fced916872bp6, "event:BURNOUT", "<html><b><i>Altitude (m)</i></b><br>Motor burnout<br>Altitude: 89.25 m<br>Time: 1.86 s<br></html>"),
    IconPin(0, 0x1.371a9fbe76c8bp2, 0x1.371a9fbe76c8bp2, 0x1.dfacb6f46508ep7, 0x1.dfacb6f46508ep7, "warning:NORMAL", "<html><b><i>Altitude (m)</i></b><br><b><i>Warning:  Recovery device deployment at high speed (26.9 m/s):  \"Parachute\"</b></i><br>Recovery device deployment<br>Altitude: 239.84 m<br>Time: 4.86 s<br></html>"),
    IconPin(0, 0x1.5b95810624dd3p2, 0x1.5b95810624dd3p2, 0x1.e632b020c49bap7, 0x1.e632b020c49bap7, "event:APOGEE", "<html><b><i>Altitude (m)</i></b><br>Apogee<br>Altitude: 243.1 m<br>Time: 5.43 s<br></html>"),
    IconPin(0, 0x1.ef72b020c49bap5, 0x1.ef72b020c49bap5, 0.0, 0.0, "event:GROUND_HIT", "<html><b><i>Altitude (m)</i></b><br>Ground hit<br>Altitude: 0 m<br>Time: 61.93 s<br></html>"),
    IconPin(1, 0.0, 0.0, 0.0, 0.0, "event:IGNITION", "<html><b><i>Vertical acceleration (m/s\xC2\xB2)</i></b><br>Motor ignition<br>Vertical acceleration: 0 m/s\xC2\xB2<br>Time: 0 s<br></html>"),
    IconPin(1, 0x1.dc28f5c28f5c3p0, 0x1.dc28f5c28f5c3p0, -0x1.fad0e56041893p4, -0x1.fad0e56041893p4, "event:BURNOUT", "<html><b><i>Vertical acceleration (m/s\xC2\xB2)</i></b><br>Motor burnout<br>Vertical acceleration: -31.68 m/s\xC2\xB2<br>Time: 1.86 s<br></html>"),
    IconPin(1, 0x1.371a9fbe76c8bp2, 0x1.371a9fbe76c8bp2, -0x1.12f0fb38a8a9dp7, -0x1.12f0fb38a8a9dp7, "warning:NORMAL", "<html><b><i>Vertical acceleration (m/s\xC2\xB2)</i></b><br><b><i>Warning:  Recovery device deployment at high speed (26.9 m/s):  \"Parachute\"</b></i><br>Recovery device deployment<br>Vertical acceleration: -137.47 m/s\xC2\xB2<br>Time: 4.86 s<br></html>"),
    IconPin(1, 0x1.5b95810624dd3p2, 0x1.5b95810624dd3p2, -0x1.39c28f5c28f5cp3, -0x1.39c28f5c28f5cp3, "event:APOGEE", "<html><b><i>Vertical acceleration (m/s\xC2\xB2)</i></b><br>Apogee<br>Vertical acceleration: -9.8 m/s\xC2\xB2<br>Time: 5.43 s<br></html>"),
    IconPin(1, 0x1.ef72b020c49bap5, 0x1.ef72b020c49bap5, -0x1.90a9300f92444p-11, -0x1.90a9300f92444p-11, "event:GROUND_HIT", "<html><b><i>Vertical acceleration (m/s\xC2\xB2)</i></b><br>Ground hit<br>Vertical acceleration: -0.00076 m/s\xC2\xB2<br>Time: 61.93 s<br></html>"),
    IconPin(2, 0.0, 0.0, 0.0, 0x1.2c0e889c4368fp-45, "event:IGNITION", "<html><b><i>Vertical velocity (m/s)</i></b><br>Motor ignition<br>Vertical velocity: 0 m/s<br>Time: 0 s<br></html>"),
    IconPin(2, 0x1.dc28f5c28f5c3p0, 0x1.dc28f5c28f5c3p0, 0x1.4a570a3d70a3dp6, 0x1.fa27b9ccccb7bp7, "event:BURNOUT", "<html><b><i>Vertical velocity (m/s)</i></b><br>Motor burnout<br>Vertical velocity: 82.58 m/s<br>Time: 1.86 s<br></html>"),
    IconPin(2, 0x1.371a9fbe76c8bp2, 0x1.371a9fbe76c8bp2, 0x1.ac6e978d4fdf5p4, 0x1.483a22be96712p6, "warning:NORMAL", "<html><b><i>Vertical velocity (m/s)</i></b><br><b><i>Warning:  Recovery device deployment at high speed (26.9 m/s):  \"Parachute\"</b></i><br>Recovery device deployment<br>Vertical velocity: 26.78 m/s<br>Time: 4.86 s<br></html>"),
    IconPin(2, 0x1.5b95810624dd3p2, 0x1.5b95810624dd3p2, 0.0, 0x1.2c0e889c4368fp-45, "event:APOGEE", "<html><b><i>Vertical velocity (m/s)</i></b><br>Apogee<br>Vertical velocity: 0 m/s<br>Time: 5.43 s<br></html>"),
    IconPin(2, 0x1.ef72b020c49bap5, 0x1.ef72b020c49bap5, -0x1.13126e978d4fep2, -0x1.a578e0465940bp3, "event:GROUND_HIT", "<html><b><i>Vertical velocity (m/s)</i></b><br>Ground hit<br>Vertical velocity: -4.3 m/s<br>Time: 61.93 s<br></html>"),
});
constexpr PlotPin kPrintableVerticalPlot{
    .file = "3D printable nose cone and fins.ork",
    .simulation = 2,
    .preset = 0,
    .title = "Simulation 3 - too short delay",
    .subtitle = "Vertical motion vs. time",
    .domainLabel = "Time (s)",
    .domain = {-0x1.0p-1, 0x1.f372b020c49bap5},
    .domainWithEventLines = {-0x1.0p-1, 0x1.f471465e89225p5},
    .axes = "0->0 1->1 2->0",
    .yAxes = {AxisPin("Altitude; Vertical acceleration", -0x1.974553cddd6ep8, 0x1.060ab40f66a55p8, true), AxisPin("Vertical velocity", -0x1.09cdb4ef05f12p7, 0x1.560a79bbadc0ap6, true)},
    .colors = "0->0 1->2 2->1",
    .legend = "Altitude (m) | Vertical acceleration (m/s\xC2\xB2) | Vertical velocity (m/s)",
    .series = kPrintableVerticalSeries,
    .toolTips = kPrintableVerticalToolTips,
    .groups = kPrintableVerticalGroups,
    .icons = kPrintableVerticalIcons,
};

// 3D printable nose cone and fins.ork, simulation 2, preset 2 "Flight side profile".
constexpr auto kPrintableSideProfileSeries = std::to_array<SeriesPin>({
    SeriesPin(0, "Altitude (m)", 0, 0, "m", 487, 0, {0.0, 0.0, 0x1.d010624dd2f1bp3, 0x1.d81a9fbe76c8bp7, -0x1.7b9fbe76c8b44p6, 0.0}),
});
constexpr auto kPrintableSideProfileToolTips = std::to_array<ToolTipPin>({
    ToolTipPin(0, 0, "<html><b><i>Altitude (m)</i></b><br>Altitude: 0 m<br>Position East of launch: 0 m<br>Time: 0 s<br></html>"),
    ToolTipPin(0, 243, "<html><b><i>Altitude (m)</i></b><br>Altitude: 236.05 m<br>Position East of launch: 14.5 m<br>Time: 7.32 s<br></html>"),
    ToolTipPin(0, 486, "<html><b><i>Altitude (m)</i></b><br>Altitude: 0 m<br>Position East of launch: -94.91 m<br>Time: 61.93 s<br></html>"),
});
constexpr auto kPrintableSideProfileGroups = std::to_array<GroupPin>({
    GroupPin(kAllBranches, 0.0, "IGNITION", "event:IGNITION", "IGNITION@0.0/b0", "Motor ignition"),
    GroupPin(kAllBranches, 0x1.dc28f5c28f5c3p0, "BURNOUT", "event:BURNOUT", "BURNOUT@1.86/b0", "Motor burnout"),
    GroupPin(kAllBranches, 0x1.371a9fbe76c8bp2, "SIM_WARN", "warning:NORMAL", "RECOVERY_DEVICE_DEPLOYMENT@4.861/b0 SIM_WARN@4.863/b0", "Recovery device deployment, Warning"),
    GroupPin(kAllBranches, 0x1.5b95810624dd3p2, "APOGEE", "event:APOGEE", "APOGEE@5.431/b0", "Apogee"),
    GroupPin(kAllBranches, 0x1.ef72b020c49bap5, "GROUND_HIT", "event:GROUND_HIT", "GROUND_HIT@61.931/b0", "Ground hit"),
    GroupPin(0, 0.0, "IGNITION", "event:IGNITION", "IGNITION@0.0/b0", "Motor ignition"),
    GroupPin(0, 0x1.dc28f5c28f5c3p0, "BURNOUT", "event:BURNOUT", "BURNOUT@1.86/b0", "Motor burnout"),
    GroupPin(0, 0x1.371a9fbe76c8bp2, "SIM_WARN", "warning:NORMAL", "RECOVERY_DEVICE_DEPLOYMENT@4.861/b0 SIM_WARN@4.863/b0", "Recovery device deployment, Warning"),
    GroupPin(0, 0x1.5b95810624dd3p2, "APOGEE", "event:APOGEE", "APOGEE@5.431/b0", "Apogee"),
    GroupPin(0, 0x1.ef72b020c49bap5, "GROUND_HIT", "event:GROUND_HIT", "GROUND_HIT@61.931/b0", "Ground hit"),
});
constexpr auto kPrintableSideProfileIcons = std::to_array<IconPin>({
    IconPin(0, 0.0, 0.0, 0.0, 0.0, "event:IGNITION", "<html><b><i>Altitude (m)</i></b><br>Motor ignition<br>Altitude: 0 m<br>Position East of launch: 0 m<br>Time: 0 s<br></html>"),
    IconPin(0, 0x1.dc28f5c28f5c3p0, 0x1.3ab020c49ba5ep2, 0x1.64fced916872bp6, 0x1.64fced916872bp6, "event:BURNOUT", "<html><b><i>Altitude (m)</i></b><br>Motor burnout<br>Altitude: 89.25 m<br>Position East of launch: 4.92 m<br>Time: 1.86 s<br></html>"),
    IconPin(0, 0x1.371a9fbe76c8bp2, 0x1.213490b9af72p4, 0x1.dfacb6f46508ep7, 0x1.dfacb6f46508ep7, "warning:NORMAL", "<html><b><i>Altitude (m)</i></b><br><b><i>Warning:  Recovery device deployment at high speed (26.9 m/s):  \"Parachute\"</b></i><br>Recovery device deployment<br>Altitude: 239.84 m<br>Position East of launch: 18.08 m<br>Time: 4.86 s<br></html>"),
    IconPin(0, 0x1.5b95810624dd3p2, 0x1.1c20c49ba5e35p4, 0x1.e632b020c49bap7, 0x1.e632b020c49bap7, "event:APOGEE", "<html><b><i>Altitude (m)</i></b><br>Apogee<br>Altitude: 243.1 m<br>Position East of launch: 17.76 m<br>Time: 5.43 s<br></html>"),
    IconPin(0, 0x1.ef72b020c49bap5, -0x1.7b9fbe76c8b44p6, 0.0, 0.0, "event:GROUND_HIT", "<html><b><i>Altitude (m)</i></b><br>Ground hit<br>Altitude: 0 m<br>Position East of launch: -94.91 m<br>Time: 61.93 s<br></html>"),
});
constexpr PlotPin kPrintableSideProfilePlot{
    .file = "3D printable nose cone and fins.ork",
    .simulation = 2,
    .preset = 2,
    .title = "Simulation 3 - too short delay",
    .subtitle = "Flight side profile",
    .domainLabel = "Position East of launch (m)",
    .domain = {-0x1.7d9fbe76c8b44p6, 0x1.2b16872b020c5p4},
    .domainWithEventLines = {-0x1.7d9fbe76c8b44p6, 0x1.2b16872b020c5p4},
    .axes = "0->0",
    .yAxes = {AxisPin("Altitude", -0x1.d2c0053e2d623p2, 0x1.f4c8b04ab606bp7, true), AxisPin("", kNaN, kNaN, false)},
    .colors = "0->0",
    .legend = "Altitude (m)",
    .series = kPrintableSideProfileSeries,
    .toolTips = kPrintableSideProfileToolTips,
    .groups = kPrintableSideProfileGroups,
    .icons = kPrintableSideProfileIcons,
};

// clang-format on

/// The image the probe names a group's look by: "warning:<priority>" for a warning, else
/// "event:<type>" (Java's EventGraphics images; SIM_ABORT and EXCEPTION share one).
[[nodiscard]] std::string imageName(Event lookType, std::optional<MessagePriority> priority)
{
    if (lookType == Event::SIM_WARN)
    {
        return "warning:" +
               std::string(priority.has_value() ? QtRocket::exportLabel(*priority) : "null");
    }
    return "event:" + std::string(QtRocket::name(lookType));
}

/// "row->axis ..." of @p filled.
[[nodiscard]] std::string axesText(const SimulationPlotConfiguration::Base& filled)
{
    std::string text;
    for (std::size_t row = 0; row < filled.getDataCount(); ++row)
    {
        text += std::format("{}{}->{}", row == 0 ? "" : " ", row, filled.getAxis(row));
    }
    return text;
}

/// "row->colour index ..." of @p colors.
[[nodiscard]] std::string colorsText(const std::vector<int>& colors)
{
    std::string text;
    for (std::size_t row = 0; row < colors.size(); ++row)
    {
        text += std::format("{}{}->{}", row == 0 ? "" : " ", row, colors[row]);
    }
    return text;
}

void expectAxes(const Model& model, const PlotPin& pin)
{
    EXPECT_EQ(model.getTitle(), pin.title);
    EXPECT_EQ(model.getSubtitle(), pin.subtitle);
    EXPECT_EQ(model.getDomainLabel(), pin.domainLabel);
    EXPECT_EQ((std::array{model.getDomainRange().min, model.getDomainRange().max,
                          model.getDomainRangeWithEventLines().min,
                          model.getDomainRangeWithEventLines().max}),
              (std::array{pin.domain[0], pin.domain[1], pin.domainWithEventLines[0],
                          pin.domainWithEventLines[1]}));
    EXPECT_EQ(axesText(model.getFilledConfiguration()), pin.axes);
}

void expectYAxis(const Model& model, const PlotPin& pin, std::size_t axis)
{
    SCOPED_TRACE(testing::Message() << "y axis " << axis);
    const Model::YAxis& yAxis   = model.getYAxes().at(axis);
    const AxisPin&      axisPin = pin.yAxes.at(axis);
    EXPECT_EQ(yAxis.hasSeries, axisPin.hasSeries);
    EXPECT_TRUE(sameDouble(yAxis.range.min, axisPin.min) &&
                sameDouble(yAxis.range.max, axisPin.max))
        << yAxis.range.min << " " << yAxis.range.max;
    EXPECT_EQ(yAxis.label, axisPin.label);
}

void expectLegend(const Model& model, const PlotPin& pin)
{
    const std::vector<int> colors =
        Model::computeDefaultColorIndices(model.getConfiguration(), model.getData());
    EXPECT_EQ(colorsText(colors), pin.colors);
    std::string labels;
    for (const Model::LegendEntry& entry : model.getLegend())
    {
        labels += (labels.empty() ? "" : " | ") + entry.label;
        // Bug 42: the legend shows each row's colour, the panel's.
        EXPECT_EQ(entry.colorIndex, colors.at(entry.row)) << entry.label;
    }
    EXPECT_EQ(labels, pin.legend);
}

void expectPoint(const Model::Series& series, std::size_t point, double x, double y)
{
    EXPECT_TRUE(sameDouble(series.x.at(point), x) && sameDouble(series.y.at(point), y))
        << "point " << point << ": " << series.x.at(point) << ", " << series.y.at(point);
}

void expectSeries(const Model& model, const SeriesPin& pin)
{
    SCOPED_TRACE(testing::Message() << "series " << pin.index);
    ASSERT_LT(pin.index, model.getSeries().size());
    const Model::Series& series = model.getSeries()[pin.index];
    EXPECT_EQ(std::tie(series.name, series.branch, series.row, series.unit, series.colorIndex),
              std::tie(pin.name, pin.branch, pin.row, pin.unit, pin.colorIndex));
    ASSERT_EQ(series.x.size(), pin.points);
    ASSERT_EQ(series.y.size(), pin.points);
    expectPoint(series, 0, pin.xy[0], pin.xy[1]);
    expectPoint(series, pin.points / 2, pin.xy[2], pin.xy[3]);
    expectPoint(series, pin.points - 1, pin.xy[4], pin.xy[5]);
}

void expectToolTips(const Model& model, const PlotPin& pin)
{
    for (const ToolTipPin& toolTip : pin.toolTips)
    {
        EXPECT_EQ(model.getToolTip(toolTip.series, toolTip.point).toHtml(), toolTip.html)
            << "series " << toolTip.series << " point " << toolTip.point;
    }
}

void expectGroup(const Model::EventGroup& group, const GroupPin& pin)
{
    EXPECT_EQ(group.time, pin.time);
    EXPECT_EQ(QtRocket::name(group.lookType), pin.look);
    EXPECT_EQ(imageName(group.lookType, group.lookWarningPriority), pin.image);
    EXPECT_EQ(groupEvents(group), pin.events);
    EXPECT_EQ(group.label, pin.label);
}

void expectGroups(const Model& model, const PlotPin& pin)
{
    // The pins list the groups of every branch, then those of each branch.
    std::vector<const Model::EventGroup*> groups;
    for (const Model::EventGroup& group : model.getEventGroups())
    {
        groups.push_back(&group);
    }
    for (std::size_t branch = 0; branch < model.getBranchCount(); ++branch)
    {
        for (const Model::EventGroup& group : model.getEventGroups(branch))
        {
            groups.push_back(&group);
        }
    }
    ASSERT_EQ(groups.size(), pin.groups.size());
    for (std::size_t i = 0; i < groups.size(); ++i)
    {
        SCOPED_TRACE(testing::Message() << "group " << i);
        expectGroup(*groups[i], pin.groups[i]);
    }
}

void expectIcon(const Model::EventIcon& icon, const IconPin& pin)
{
    EXPECT_EQ(std::tie(icon.series, icon.time, icon.x, icon.y, icon.yOnLeftAxis),
              std::tie(pin.series, pin.time, pin.x, pin.y, pin.yOnLeftAxis));
    EXPECT_EQ(imageName(icon.lookType, icon.lookWarningPriority), pin.image);
    EXPECT_EQ(icon.toolTip.toHtml(), pin.toolTip);
}

void expectIcons(const Model& model, const PlotPin& pin)
{
    const std::vector<Model::EventIcon> icons = model.getIconMarkers();
    ASSERT_EQ(icons.size(), pin.icons.size());
    for (std::size_t i = 0; i < icons.size(); ++i)
    {
        SCOPED_TRACE(testing::Message() << "icon " << i);
        expectIcon(icons[i], pin.icons[i]);
    }
}

/// The plot of @p simulation of @p document with the preset of @p pin, chosen as the plot panel
/// chooses it (a copy in the default units), compared with @p pin.
void expectPlot(const QtRocket::OpenRocketDocument& document, const PlotPin& pin)
{
    SCOPED_TRACE(testing::Message()
                 << pin.file << " simulation " << pin.simulation << " preset " << pin.preset);
    const auto simulations = document.getSimulations();
    ASSERT_LT(pin.simulation, simulations.size());
    const QtRocket::Simulation& simulation = *simulations[pin.simulation];
    ASSERT_NE(simulation.getSimulatedData(), nullptr);
    SimulationPlotConfiguration preset =
        SimulationPlotConfiguration::defaultConfigurations().at(pin.preset);
    preset.resetUnits();
    const Model model(simulation.getName(), simulation.getSimulatedData(), preset);

    expectAxes(model, pin);
    expectYAxis(model, pin, 0);
    expectYAxis(model, pin, 1);
    expectLegend(model, pin);
    EXPECT_EQ(model.getSeries().size(), pin.series.size());
    for (const SeriesPin& series : pin.series)
    {
        expectSeries(model, series);
    }
    expectToolTips(model, pin);
    expectGroups(model, pin);
    expectIcons(model, pin);
}

/// The document of the example @p file, as loaded.
[[nodiscard]] std::unique_ptr<QtRocket::OpenRocketDocument> loadExample(std::string_view file)
{
    const QtRocket::Test::GoldenExample* example = QtRocket::Test::findGoldenExample(file);
    if (example == nullptr)
    {
        ADD_FAILURE() << "no example " << file;
        return nullptr;
    }
    auto loaded = QtRocket::Test::loadGoldenExample(*example);
    if (!loaded.has_value())
    {
        ADD_FAILURE() << "cannot load " << file;
        return nullptr;
    }
    return std::move(loaded->document);
}

TEST(SimulationPlotModel, ASimpleModelRocketsPlotsAreOpenRocketsOwn)
{
    const DefaultUnitsGuard units;
    const auto              document = loadExample("A simple model rocket.ork");
    ASSERT_NE(document, nullptr);
    expectPlot(*document, kSimpleVerticalPlot);
    expectPlot(*document, kSimpleSideProfilePlot);
    expectPlot(*document, kSimpleStabilityPlot);
    expectPlot(*document, kSimpleDragPlot);
}

TEST(SimulationPlotModel, AThreeStageRocketsPlotsAreOpenRocketsOwn)
{
    const DefaultUnitsGuard units;
    const auto              document = loadExample("Three stage low power rocket.ork");
    ASSERT_NE(document, nullptr);
    expectPlot(*document, kThreeStageVerticalPlot);
    expectPlot(*document, kThreeStageSideProfilePlot);
}

TEST(SimulationPlotModel, APlotWithAWarningIsOpenRocketsOwn)
{
    const DefaultUnitsGuard units;
    const auto              document = loadExample("3D printable nose cone and fins.ork");
    ASSERT_NE(document, nullptr);
    expectPlot(*document, kPrintableVerticalPlot);
    expectPlot(*document, kPrintableSideProfilePlot);
}

/// A copy of the branches of @p stored, each with a SIM_ABORT at 0.6 times its last time, of the
/// cause (3 b + 4) mod 10, branch 1 with a second one, of NO_ACTIVE_STAGES, a second later (the
/// probe's aborts()).
[[nodiscard]] std::shared_ptr<const FlightData> withAborts(const FlightData& stored)
{
    std::vector<std::shared_ptr<FlightDataBranch>> copies;
    for (std::size_t b = 0; b < stored.getBranchCount(); ++b)
    {
        auto         copy = std::make_shared<FlightDataBranch>(stored.getBranch(b).clone());
        const double t    = copy->getLast(type(Id::TYPE_TIME)) * 0.6;
        copy->addEvent(
            FlightEvent(Event::SIM_ABORT, t, nullptr,
                        SimulationAbort(SimulationAbort::kAllCauses.at(((3 * b) + 4) % 10))));
        if (b == 1)
        {
            copy->addEvent(FlightEvent(Event::SIM_ABORT, t + 1, nullptr,
                                       SimulationAbort(SimulationAbort::Cause::NO_ACTIVE_STAGES)));
        }
        copies.push_back(std::move(copy));
    }
    return std::make_shared<const FlightData>(copies);
}

TEST(SimulationPlotModel, TheAbortsOfAThreeStageRocketAreAnnotatedAsOpenRocketAnnotatesThem)
{
    // The probe's "aborts" section: the branches of "Three stage low power rocket"'s first
    // simulation, each with a SIM_ABORT at 0.6 times its last time (cause (3 b + 4) mod 10),
    // branch 1 with a second one a second later.
    const DefaultUnitsGuard units;
    const auto              document = loadExample("Three stage low power rocket.ork");
    ASSERT_NE(document, nullptr);
    const Model model("Simulation 1",
                      withAborts(*document->getSimulations().at(0)->getSimulatedData()),
                      SimulationPlotConfiguration::defaultConfigurations().at(0));

    const std::vector<Model::AbortLine> lines = model.getAbortLines();
    ASSERT_EQ(lines.size(), 3U);
    EXPECT_EQ(lines[0].time, 0x1.59f0d844d013ap5);
    EXPECT_EQ(lines[0].cause, SimulationAbort::Cause::NO_LIFTOFF);
    EXPECT_EQ(lines[1].time, 0x1.da8240b780346p2);
    EXPECT_EQ(lines[1].cause, SimulationAbort::Cause::ACTIVE_MASS_ZERO);
    EXPECT_EQ(lines[2].time, 0x1.bee631f8a0903p1);
    EXPECT_EQ(lines[2].cause, SimulationAbort::Cause::NO_ACTIVE_STAGES);

    constexpr std::string_view kSustainer =
        "Stage: Sustainer; Time: 43.24 s; Cause: <html>Motor burnout without liftoff. <br>Use "
        "more (powerful) motors, or decrease the rocket mass.</html>";
    constexpr std::string_view kBooster =
        "Stage: Booster stage; Time: 7.41 s; Cause: Total mass of active stages is 0";
    constexpr std::string_view kSecondBooster =
        "Stage: Booster stage; Time: 3.49 s; Cause: No active stages";
    EXPECT_EQ(model.getAbortText(),
              std::format("Simulation Abort\n{}\n{}\n{}", kSustainer, kBooster, kSecondBooster));
    EXPECT_EQ(model.getAbortText(0), std::format("Simulation Abort\n{}", kSustainer));
    EXPECT_EQ(model.getAbortText(1), std::format("Simulation Abort\n{}", kBooster));
    EXPECT_EQ(model.getAbortText(2), std::format("Simulation Abort\n{}", kSecondBooster));
}

}  // namespace
