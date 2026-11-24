// The core's plot model against OpenRocket's own plots of the stored simulations of its example
// designs (tests/data/goldens/plots, written by tools/openrocket-plots; SimulationPlotGoldens.h
// says what the tables hold): every stored simulation of the sixteen examples with each of the 9
// preset plot configurations, chosen as OpenRocket's plot panel chooses one (a copy in the default
// units), through SimulationPlotConfiguration and SimulationPlotModel; ten of the simulations in
// detail. Everything is compared exactly, to the bit: OpenRocket computes the ranges, the
// goodness of the automatic axes, the series, the event line widening and the icons' positions
// with + - * / and sqrt only (the build turns floating-point contraction off), and the tool tips'
// digits of the stored values count alike with every libm (the probe checks it).
//
// Where QtRocket fixes OpenRocket's bugs the tables hold QtRocket's answers (bugs 41 and 45, see
// SimulationPlotGoldens.h), and the colours are the plot panel's (bug 42: the series and the
// legend take the colour the panel shows for their row).

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/plot/PlotAxisBounds.h"
#include "QtRocket/plot/PlotUtil.h"
#include "QtRocket/plot/SimulationPlotConfiguration.h"
#include "QtRocket/plot/SimulationPlotModel.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/MathUtil.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenExamples.h"
#include "plot/SimulationPlotGoldens.h"
#include "unit/DefaultUnitsGuard.h"

namespace
{

using QtRocket::FlightData;
using QtRocket::FlightDataBranch;
using QtRocket::FlightEvent;
using QtRocket::OpenRocketDocument;
using QtRocket::Simulation;
using QtRocket::SimulationPlotConfiguration;
using QtRocket::SimulationPlotModel;
using QtRocket::Test::DefaultUnitsGuard;
using QtRocket::Test::GoldenExample;
using QtRocket::Test::GoldenPlot;
using QtRocket::Test::GoldenPlotAxes;
using QtRocket::Test::GoldenPlotDetail;
using QtRocket::Test::GoldenPlotDetails;
using QtRocket::Test::GoldenPlotDetailSimulation;
using QtRocket::Test::GoldenPlotEvent;
using QtRocket::Test::GoldenPlotGroup;
using QtRocket::Test::GoldenPlotPair;
using QtRocket::Test::GoldenPlotSimulation;
using QtRocket::Test::GoldenPlotTable;
using QtRocket::Test::javaDoubleBits;
using QtRocket::Test::sameDoubleBits;

using Base = SimulationPlotConfiguration::Base;

/// The plots of a test: the tables, read once per process, or a failure when one cannot be read.
[[nodiscard]] const GoldenPlotTable* plotTableOrFail()
{
    const auto& table = QtRocket::Test::goldenPlotTable();
    if (!table.has_value())
    {
        ADD_FAILURE() << table.error().toString();
        return nullptr;
    }
    return &*table;
}

[[nodiscard]] const GoldenPlotDetails* plotDetailsOrFail()
{
    const auto& details = QtRocket::Test::goldenPlotDetails();
    if (!details.has_value())
    {
        ADD_FAILURE() << details.error().toString();
        return nullptr;
    }
    return &*details;
}

/// "[a, b, ...]" of @p values.
template <class T>
[[nodiscard]] std::string listText(const std::vector<T>& values)
{
    std::string text = "[";
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        text += std::format("{}{}", i == 0 ? "" : ", ", values[i]);
    }
    return text + "]";
}

/// A double as the mismatches show it: its shortest text and its bits.
[[nodiscard]] std::string doubleText(double value)
{
    return std::format("{} ({:016x})", value, javaDoubleBits(value));
}

/// The mismatches of a comparison, one line each, prefixed with where they were found.
class Mismatches
{
public:
    explicit Mismatches(std::vector<std::string>& lines) noexcept : m_lines(&lines) { }

    /// A comparison of @p context within this one.
    [[nodiscard]] Mismatches within(std::string_view context) const
    {
        Mismatches nested(*m_lines);
        nested.m_context =
            m_context.empty() ? std::string(context) : std::format("{} {}", m_context, context);
        return nested;
    }

    void fail(std::string_view what) const
    {
        m_lines->push_back(m_context.empty() ? std::string(what)
                                             : std::format("{}: {}", m_context, what));
    }

    void text(std::string_view what, std::string_view actual, std::string_view golden) const
    {
        if (actual != golden)
        {
            fail(std::format(R"({}: "{}" differs from OpenRocket's "{}")", what, actual, golden));
        }
    }

    void number(std::string_view what, double actual, double golden) const
    {
        if (!sameDoubleBits(actual, golden))
        {
            fail(std::format("{}: {} differs from OpenRocket's {}", what, doubleText(actual),
                             doubleText(golden)));
        }
    }

    void pair(std::string_view what, double first, double second,
              const GoldenPlotPair& golden) const
    {
        number(std::format("{}[0]", what), first, golden[0]);
        number(std::format("{}[1]", what), second, golden[1]);
    }

    void integer(std::string_view what, long long actual, long long golden) const
    {
        if (actual != golden)
        {
            fail(std::format("{}: {} differs from OpenRocket's {}", what, actual, golden));
        }
    }

    /// Whether @p actual and @p golden have the same size (a mismatch otherwise).
    [[nodiscard]] bool sameCount(std::string_view what, std::size_t actual,
                                 std::size_t golden) const
    {
        integer(std::format("the number of {}", what), static_cast<long long>(actual),
                static_cast<long long>(golden));
        return actual == golden;
    }

private:
    std::vector<std::string>* m_lines;
    std::string               m_context;
};

/// The look of a group or an icon as the tables write it: the type's name, and for a warning
/// "/" and its priority.
[[nodiscard]] std::string lookText(FlightEvent::Type                               type,
                                   const std::optional<QtRocket::MessagePriority>& priority)
{
    std::string text(QtRocket::name(type));
    if (priority.has_value())
    {
        text += std::format("/{}", QtRocket::exportLabel(*priority));
    }
    return text;
}

// ---------------------------------------------------------------------------------------------
// The configuration and its axes

/// A copy of a configuration whose goodness of a branch a test can ask for (Java's protected
/// getGoodnessValue(), which the probe calls on each candidate).
class CandidateConfiguration : public SimulationPlotConfiguration
{
public:
    explicit CandidateConfiguration(const SimulationPlotConfiguration& source)
      : SimulationPlotConfiguration(source)
    {
    }

    [[nodiscard]] double goodness(const FlightDataBranch& branch)
    {
        return getGoodnessValue(branch);
    }
};

/// The rows' axes and the ranges of @p configuration against @p golden.
void compareFit(const Mismatches& mismatches, const Base& configuration,
                const GoldenPlotAxes& golden)
{
    std::vector<int> axes;
    axes.reserve(configuration.getDataCount());
    for (std::size_t row = 0; row < configuration.getDataCount(); ++row)
    {
        axes.push_back(configuration.getAxis(row));
    }
    if (axes != golden.axes)
    {
        mismatches.fail(std::format("axes {} differ from OpenRocket's {}", listText(axes),
                                    listText(golden.axes)));
    }
    for (std::size_t axis = 0; axis < 2; ++axis)
    {
        const QtRocket::PlotAxisBounds& bounds = configuration.getAllAxes().at(axis);
        mismatches.pair(axis == 0 ? "the left range" : "the right range", bounds.getMinValue(),
                        bounds.getMaxValue(), golden.ranges.at(axis));
    }
}

/// The preset as the plot panel takes it (rows, name and domain) against the table's.
void compareConfiguration(const Mismatches& mismatches, const SimulationPlotConfiguration& preset,
                          const GoldenPlot& golden)
{
    mismatches.text("the name", preset.getName(), golden.name);
    mismatches.text("the domain", preset.getDomainAxisType().getName(), golden.domain[0]);
    mismatches.text("the domain's unit", preset.getDomainAxisUnit().getUnit(), golden.domain[1]);
    if (!mismatches.sameCount("rows", preset.getDataCount(), golden.rows.size()))
    {
        return;
    }
    for (std::size_t row = 0; row < golden.rows.size(); ++row)
    {
        const Mismatches here = mismatches.within(std::format("row {}", row));
        here.text("type", preset.getType(row).getName(), golden.rows[row].type);
        here.text("unit", preset.getUnit(row).getUnit(), golden.rows[row].unit);
        here.integer("axis", preset.getAxis(row), golden.rows[row].axis);
    }
}

/// Every candidate fillAutoAxes() scores on @p main, in the order it scores them (the first
/// automatic row the most significant, the left axis before the right one), against the table's.
void compareCandidates(const Mismatches& mismatches, const SimulationPlotConfiguration& preset,
                       const FlightDataBranch& main, const GoldenPlot& golden)
{
    std::vector<std::size_t> automaticRows;
    for (std::size_t row = 0; row < preset.getDataCount(); ++row)
    {
        if (preset.getAxis(row) < 0)
        {
            automaticRows.push_back(row);
        }
    }
    const std::size_t candidates = std::size_t{1} << automaticRows.size();
    if (!mismatches.sameCount("candidates", candidates, golden.candidates.size()))
    {
        return;
    }
    for (std::size_t mask = 0; mask < candidates; ++mask)
    {
        CandidateConfiguration candidate(preset);
        for (std::size_t j = 0; j < automaticRows.size(); ++j)
        {
            const std::size_t shift = automaticRows.size() - 1 - j;
            candidate.setPlotDataAxis(automaticRows[j], static_cast<int>((mask >> shift) & 1U));
        }
        const Mismatches here     = mismatches.within(std::format("candidate {}", mask));
        const double     goodness = candidate.goodness(main);
        here.number("goodness", goodness, golden.candidates[mask].goodness);
        compareFit(here, candidate, golden.candidates[mask].fit);
    }
}

// ---------------------------------------------------------------------------------------------
// The chart

void compareTitlesAndDomain(const Mismatches& mismatches, const SimulationPlotModel& model,
                            const GoldenPlot& golden)
{
    mismatches.text("the title", model.getTitle(), golden.title);
    mismatches.text("the subtitle", model.getSubtitle(), golden.subtitle);
    mismatches.text("the domain label", model.getDomainLabel(), golden.domainLabel);
    mismatches.pair("the domain range", model.getDomainRange().min, model.getDomainRange().max,
                    golden.domainRange);
    // OpenRocket draws the events of a plot over time as lines, for which it widens the domain.
    if (golden.domainRangeWithEventLines.has_value())
    {
        mismatches.integer("the time domain", model.hasTimeDomain() ? 1 : 0, 1);
        mismatches.pair(
            "the domain range with the event lines", model.getDomainRangeWithEventLines().min,
            model.getDomainRangeWithEventLines().max, *golden.domainRangeWithEventLines);
    }
    else
    {
        mismatches.integer("the time domain", model.hasTimeDomain() ? 1 : 0, 0);
    }
}

void compareYAxes(const Mismatches& mismatches, const SimulationPlotModel& model,
                  const GoldenPlot& golden)
{
    for (std::size_t axis = 0; axis < 2; ++axis)
    {
        const Mismatches                  here  = mismatches.within(std::format("y axis {}", axis));
        const SimulationPlotModel::YAxis& yAxis = model.getYAxes().at(axis);
        const auto&                       wanted = golden.yAxes.at(axis);
        std::size_t                       series = 0;
        for (const SimulationPlotModel::Series& each : model.getSeries())
        {
            if (std::cmp_equal(each.axis, axis))
            {
                ++series;
            }
        }
        here.integer("series", static_cast<long long>(series),
                     static_cast<long long>(wanted.series));
        here.integer("has series", yAxis.hasSeries ? 1 : 0, wanted.series > 0 ? 1 : 0);
        // OpenRocket labels only an axis with series; QtRocket's label of an axis without rows is
        // empty.
        here.text("label", yAxis.label, wanted.label.value_or(std::string()));
        here.pair("range", yAxis.range.min, yAxis.range.max, wanted.range);
    }
}

void compareSeries(const Mismatches& mismatches, const SimulationPlotModel& model,
                   const GoldenPlot& golden)
{
    const std::vector<SimulationPlotModel::Series>& series = model.getSeries();
    if (!mismatches.sameCount("series", series.size(), golden.series.size()))
    {
        return;
    }
    for (std::size_t i = 0; i < series.size(); ++i)
    {
        const Mismatches                   here   = mismatches.within(std::format("series {}", i));
        const SimulationPlotModel::Series& actual = series[i];
        const auto&                        wanted = golden.series[i];
        here.text("name", actual.name, wanted.name);
        here.integer("axis", actual.axis, wanted.axis);
        here.integer("branch", static_cast<long long>(actual.branch),
                     static_cast<long long>(wanted.branch));
        here.integer("row", static_cast<long long>(actual.row), static_cast<long long>(wanted.row));
        here.text("unit", actual.unit, wanted.unit);
        here.integer("points", static_cast<long long>(actual.x.size()),
                     static_cast<long long>(wanted.points));
        here.integer("y points", static_cast<long long>(actual.y.size()),
                     static_cast<long long>(wanted.points));
        here.text("digest", QtRocket::Test::seriesDigest(actual.x, actual.y), wanted.digest);
        // Bug 42: the colour the panel shows for the row.
        if (actual.row < golden.panelColors.size())
        {
            here.integer("colour", actual.colorIndex, golden.panelColors[actual.row]);
        }
        here.integer("explicit colour", actual.color.has_value() ? 1 : 0, 0);
    }
}

void compareLegend(const Mismatches& mismatches, const SimulationPlotModel& model,
                   const GoldenPlot& golden)
{
    const std::vector<SimulationPlotModel::LegendEntry>& legend = model.getLegend();
    if (!mismatches.sameCount("legend entries", legend.size(), golden.legend.size()))
    {
        return;
    }
    for (std::size_t i = 0; i < legend.size(); ++i)
    {
        const Mismatches here = mismatches.within(std::format("legend entry {}", i));
        here.text("label", legend[i].label, golden.legend[i]);
        if (legend[i].row < golden.panelColors.size())
        {
            here.integer("colour", legend[i].colorIndex, golden.panelColors[legend[i].row]);
        }
    }
}

void compareColors(const Mismatches& mismatches, const SimulationPlotConfiguration& preset,
                   const FlightData& data, const GoldenPlot& golden)
{
    const std::vector<int> colors = SimulationPlotModel::computeDefaultColorIndices(preset, data);
    if (colors != golden.panelColors)
    {
        mismatches.fail(std::format("the panel's colours {} differ from OpenRocket's {}",
                                    listText(colors), listText(golden.panelColors)));
    }
}

// ---------------------------------------------------------------------------------------------
// The events

void compareEvent(const Mismatches& mismatches, const SimulationPlotModel::PlotEvent& actual,
                  const GoldenPlotEvent& golden)
{
    mismatches.text("type", QtRocket::name(actual.event.getType()), golden.type);
    mismatches.number("time", actual.event.getTime(), golden.time);
    mismatches.integer("branch", static_cast<long long>(actual.branch),
                       static_cast<long long>(golden.branch));
}

void compareEvents(const Mismatches&                                  mismatches,
                   const std::vector<SimulationPlotModel::PlotEvent>& actual,
                   const std::vector<GoldenPlotEvent>&                golden)
{
    if (!mismatches.sameCount("events", actual.size(), golden.size()))
    {
        return;
    }
    for (std::size_t i = 0; i < actual.size(); ++i)
    {
        compareEvent(mismatches.within(std::format("event {}", i)), actual[i], golden[i]);
    }
}

void compareGroups(const Mismatches&                                   mismatches,
                   const std::vector<SimulationPlotModel::EventGroup>& actual,
                   const std::vector<GoldenPlotGroup>&                 golden)
{
    if (!mismatches.sameCount("groups", actual.size(), golden.size()))
    {
        return;
    }
    for (std::size_t i = 0; i < actual.size(); ++i)
    {
        const Mismatches here = mismatches.within(std::format("group {}", i));
        here.number("time", actual[i].time, golden[i].time);
        here.text("look", lookText(actual[i].lookType, actual[i].lookWarningPriority),
                  golden[i].look);
        here.text("label", actual[i].label, golden[i].label);
        compareEvents(here, actual[i].events, golden[i].events);
    }
}

/// The digests of @p icons as the tables write them (SimulationPlotGoldens.h, GoldenPlotIcons).
[[nodiscard]] std::array<std::string, 2> iconDigests(
    const std::vector<SimulationPlotModel::EventIcon>& icons)
{
    std::string positions;
    std::string toolTips;
    for (const SimulationPlotModel::EventIcon& icon : icons)
    {
        positions += std::format("{} {} {:016x} {:016x} {:016x} {:016x} {}\n", icon.series,
                                 icon.group, javaDoubleBits(icon.time), javaDoubleBits(icon.x),
                                 javaDoubleBits(icon.y), javaDoubleBits(icon.yOnLeftAxis),
                                 lookText(icon.lookType, icon.lookWarningPriority));
        toolTips += icon.toolTip.toHtml() + "\n";
    }
    return {QtRocket::Test::shortDigest(positions), QtRocket::Test::shortDigest(toolTips)};
}

void compareIcons(const Mismatches& mismatches, const SimulationPlotModel& model,
                  const GoldenPlot& golden)
{
    const std::vector<SimulationPlotModel::EventIcon> icons = model.getIconMarkers();
    if (!mismatches.sameCount("icons", icons.size(), golden.icons.count))
    {
        return;
    }
    const std::array<std::string, 2> digests = iconDigests(icons);
    mismatches.text("the icons' digest", digests[0], golden.icons.digest);
    mismatches.text("the icons' tool tips' digest", digests[1], golden.icons.toolTipDigest);
}

// ---------------------------------------------------------------------------------------------
// The details

/// JFreeChart's XYSeries bounds: the smaller of @p a and @p b with a NaN ignored (Java's
/// Math.min otherwise; minIgnoreNaN()).
[[nodiscard]] double minIgnoreNaN(double a, double b)
{
    if (std::isnan(a))
    {
        return b;
    }
    if (std::isnan(b))
    {
        return a;
    }
    return QtRocket::MathUtil::javaMin(a, b);
}

/// The larger of @p a and @p b with a NaN ignored (maxIgnoreNaN()).
[[nodiscard]] double maxIgnoreNaN(double a, double b)
{
    if (std::isnan(a))
    {
        return b;
    }
    if (std::isnan(b))
    {
        return a;
    }
    return QtRocket::MathUtil::javaMax(a, b);
}

/// The smallest of @p values as an XYSeries keeps it (NaN without a value).
[[nodiscard]] double seriesMinimum(const std::vector<double>& values)
{
    double minimum = std::numeric_limits<double>::quiet_NaN();
    for (const double value : values)
    {
        minimum = minIgnoreNaN(minimum, value);
    }
    return minimum;
}

/// The largest of @p values as an XYSeries keeps it (NaN without a value).
[[nodiscard]] double seriesMaximum(const std::vector<double>& values)
{
    double maximum = std::numeric_limits<double>::quiet_NaN();
    for (const double value : values)
    {
        maximum = maxIgnoreNaN(maximum, value);
    }
    return maximum;
}

void compareSeriesPoints(const Mismatches& mismatches, const SimulationPlotModel& model,
                         const GoldenPlotDetail& detail)
{
    const std::vector<SimulationPlotModel::Series>& series = model.getSeries();
    if (!mismatches.sameCount("series in detail", series.size(), detail.series.size()))
    {
        return;
    }
    for (std::size_t i = 0; i < series.size(); ++i)
    {
        const Mismatches here = mismatches.within(std::format("series {}", i));
        const auto&      x    = series[i].x;
        const auto&      y    = series[i].y;
        const auto&      want = detail.series[i];
        here.pair("minimum", seriesMinimum(x), seriesMinimum(y), want.min);
        here.pair("maximum", seriesMaximum(x), seriesMaximum(y), want.max);
        if (x.empty() || y.size() != x.size())
        {
            here.fail("no points");
            continue;
        }
        const std::size_t middle = x.size() / 2;
        here.pair("the first point", x.front(), y.front(), want.first);
        here.pair("the middle point", x[middle], y[middle], want.middle);
        here.pair("the last point", x.back(), y.back(), want.last);
    }
}

void compareToolTips(const Mismatches& mismatches, const SimulationPlotModel& model,
                     const GoldenPlotDetail& detail)
{
    for (const auto& toolTip : detail.toolTips)
    {
        const Mismatches here =
            mismatches.within(std::format("series {} point {}", toolTip.series, toolTip.point));
        if (toolTip.series >= model.getSeries().size() ||
            toolTip.point >= model.getSeries()[toolTip.series].x.size())
        {
            here.fail("no such point");
            continue;
        }
        here.text("tool tip", model.getToolTip(toolTip.series, toolTip.point).toHtml(),
                  toolTip.html);
        here.text("name", model.getSeriesNameAt(toolTip.series, toolTip.point), toolTip.name);
    }
}

void compareIconDetails(const Mismatches& mismatches, const SimulationPlotModel& model,
                        const GoldenPlotDetail& detail)
{
    const std::vector<SimulationPlotModel::EventIcon> icons = model.getIconMarkers();
    if (!mismatches.sameCount("icons in detail", icons.size(), detail.icons.size()))
    {
        return;
    }
    for (std::size_t i = 0; i < icons.size(); ++i)
    {
        const Mismatches here = mismatches.within(std::format("icon {}", i));
        const auto&      want = detail.icons[i];
        here.integer("series", static_cast<long long>(icons[i].series),
                     static_cast<long long>(want.series));
        here.integer("group", static_cast<long long>(icons[i].group),
                     static_cast<long long>(want.group));
        here.number("time", icons[i].time, want.time);
        here.number("x", icons[i].x, want.x);
        here.number("y", icons[i].y, want.y);
        here.number("y on the left axis", icons[i].yOnLeftAxis, want.yOnLeftAxis);
        here.text("look", lookText(icons[i].lookType, icons[i].lookWarningPriority), want.look);
        here.text("tool tip", icons[i].toolTip.toHtml(), want.toolTip);
    }
}

void compareDetail(const Mismatches& mismatches, const SimulationPlotModel& model,
                   const GoldenPlotDetail& detail)
{
    compareSeriesPoints(mismatches, model, detail);
    compareToolTips(mismatches, model, detail);
    compareEvents(mismatches.within("the event list"), model.getEvents(), detail.eventList);
    if (mismatches.sameCount("branches with groups", model.getBranchCount(),
                             detail.branchGroups.size()))
    {
        for (std::size_t branch = 0; branch < model.getBranchCount(); ++branch)
        {
            compareGroups(mismatches.within(std::format("branch {}", branch)),
                          model.getEventGroups(branch), detail.branchGroups[branch]);
        }
    }
    compareIconDetails(mismatches, model, detail);
}

// ---------------------------------------------------------------------------------------------
// A plot, a simulation, an example

/// The plot of @p simulation with the preset of @p golden, made as the plot panel and the plot
/// dialog make it, against @p golden (and @p detail, unless null).
void comparePlot(const Mismatches& mismatches, const Simulation& simulation,
                 const GoldenPlot& golden, const GoldenPlotDetail* detail)
{
    const std::vector<SimulationPlotConfiguration> presets =
        SimulationPlotConfiguration::defaultConfigurations();
    if (golden.preset >= presets.size())
    {
        mismatches.fail("no such preset");
        return;
    }
    // The plot panel's choice of a preset: a copy in the default units.
    SimulationPlotConfiguration preset = presets[golden.preset];
    preset.resetUnits();
    compareConfiguration(mismatches, preset, golden);

    const FlightData&       data = *simulation.getSimulatedData();
    const FlightDataBranch& main = data.getBranch(0);
    compareCandidates(mismatches, preset, main, golden);
    compareFit(mismatches.within("fillAutoAxes()"), *preset.fillAutoAxes(main), golden.automatic);
    compareColors(mismatches, preset, data, golden);

    const SimulationPlotModel model(simulation.getName(), simulation.getSimulatedData(), preset);
    compareFit(mismatches.within("the filled configuration"), model.getFilledConfiguration(),
               golden.filled);
    compareTitlesAndDomain(mismatches, model, golden);
    compareYAxes(mismatches, model, golden);
    compareSeries(mismatches, model, golden);
    compareLegend(mismatches, model, golden);
    compareGroups(mismatches.within("all stages"), model.getEventGroups(), golden.groups);
    compareIcons(mismatches, model, golden);
    if (detail != nullptr)
    {
        compareDetail(mismatches.within("in detail"), model, *detail);
    }
}

/// The stored simulation's branches and series labels against the table's.
void compareBranches(const Mismatches& mismatches, const FlightData& data,
                     const GoldenPlotSimulation& golden)
{
    if (mismatches.sameCount("branches", data.getBranchCount(), golden.branches.size()))
    {
        for (std::size_t b = 0; b < data.getBranchCount(); ++b)
        {
            const Mismatches        here   = mismatches.within(std::format("branch {}", b));
            const FlightDataBranch& branch = data.getBranch(b);
            const auto&             wanted = golden.branches[b];
            here.text("name", branch.getName(), wanted.name);
            here.integer("points", static_cast<long long>(branch.getLength()),
                         static_cast<long long>(wanted.points));
            here.number("separation time", branch.getSeparationTime(), wanted.separationTime);
            const std::optional<std::size_t> index =
                branch.getDataIndexOfTime(branch.getSeparationTime());
            here.integer("separation index",
                         index.has_value() ? static_cast<long long>(*index) : -1,
                         wanted.separationIndex);
        }
    }
    const std::vector<std::string> labels = QtRocket::PlotUtil::generateSeriesLabels(data);
    if (labels != golden.seriesLabels)
    {
        mismatches.fail(std::format("the series labels {} differ from OpenRocket's {}",
                                    listText(labels), listText(golden.seriesLabels)));
    }
}

/// Everything of @p simulation (stored simulation @p index of its document) that differs from
/// the tables.
void compareSimulation(const Mismatches& mismatches, const Simulation& simulation,
                       const GoldenPlotSimulation& golden, const GoldenPlotDetailSimulation* detail)
{
    mismatches.text("the name", simulation.getName(), golden.name);
    compareBranches(mismatches, *simulation.getSimulatedData(), golden);
    if (detail != nullptr &&
        !mismatches.sameCount("plots in detail", detail->plots.size(), golden.plots.size()))
    {
        detail = nullptr;
    }
    for (std::size_t i = 0; i < golden.plots.size(); ++i)
    {
        const GoldenPlot& plot = golden.plots[i];
        comparePlot(mismatches.within(std::format("preset {}", plot.preset)), simulation, plot,
                    detail != nullptr ? &detail->plots[i] : nullptr);
    }
}

/// Whether @p simulation has stored flight data (the probe plots only those).
[[nodiscard]] bool hasStoredData(const Simulation& simulation)
{
    const auto& data = simulation.getSimulatedData();
    return data != nullptr && data->getBranchCount() > 0;
}

/// Everything of the stored simulations of @p document, the example of the file @p file, that
/// differs from the tables, the stored simulations the tables lack and those they hold the
/// document has not; and in @p compared the number of plots compared and in @p detailed of those
/// in detail.
[[nodiscard]] std::vector<std::string> exampleMismatches(const OpenRocketDocument& document,
                                                         std::string_view          file,
                                                         std::size_t&              compared,
                                                         std::size_t&              detailed)
{
    std::vector<std::string> lines;
    const GoldenPlotTable*   table   = plotTableOrFail();
    const GoldenPlotDetails* details = plotDetailsOrFail();
    if (table == nullptr || details == nullptr)
    {
        return lines;
    }
    const auto  simulations = document.getSimulations();
    std::size_t stored      = 0;
    for (std::size_t index = 0; index < simulations.size(); ++index)
    {
        const Mismatches mismatches =
            Mismatches(lines).within(std::format("{} simulation {}", file, index));
        const GoldenPlotSimulation* golden = table->find(file, index);
        if (!hasStoredData(*simulations[index]))
        {
            if (golden != nullptr)
            {
                mismatches.fail("has no stored data, but the table has its plots");
            }
            continue;
        }
        ++stored;
        if (golden == nullptr)
        {
            mismatches.fail("has stored data, but the table has no plots of it");
            continue;
        }
        const GoldenPlotDetailSimulation* detail = details->find(file, index);
        compareSimulation(mismatches, *simulations[index], *golden, detail);
        compared += golden->plots.size();
        detailed += detail != nullptr ? detail->plots.size() : 0;
    }
    std::size_t inTable = 0;
    for (const GoldenPlotSimulation& simulation : table->simulations)
    {
        if (simulation.file == file)
        {
            ++inTable;
        }
    }
    if (inTable != stored)
    {
        Mismatches(lines).fail(std::format(
            "{}: the table has {} stored simulations, the example {}", file, inTable, stored));
    }
    return lines;
}

// ===================================================================================== tests

/// One of OpenRocket's example designs: the plots of its stored simulations.
class SimulationPlotGolden : public ::testing::TestWithParam<GoldenExample>
{ };

// Every stored simulation of the example (as loaded) with each of the 9 presets, chosen as the
// plot panel chooses it, is plotted by SimulationPlotModel as OpenRocket plots it: the
// configuration, every candidate of the automatic axes, the filled ranges, the titles, the axes,
// the colours, the series, the legend, the event groups and the icons; and the simulations of
// the details table in detail.
TEST_P(SimulationPlotGolden, EveryPresetOfEveryStoredSimulation)
{
    const DefaultUnitsGuard units;
    auto                    loaded = QtRocket::Test::loadGoldenExample(GetParam());
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    std::size_t compared = 0;
    std::size_t detailed = 0;
    EXPECT_EQ(exampleMismatches(*loaded->document, GetParam().file, compared, detailed),
              QtRocket::Test::noGoldenMismatches());
    EXPECT_GT(compared, 0U) << "every example has stored simulations";
    EXPECT_EQ(compared % 9, 0U) << "9 presets per simulation";
    RecordProperty("plots", static_cast<int>(compared));
    RecordProperty("detailed", static_cast<int>(detailed));
}

INSTANTIATE_TEST_SUITE_P(Examples, SimulationPlotGolden,
                         ::testing::ValuesIn(QtRocket::Test::goldenExamples()),
                         QtRocket::Test::goldenExampleTestName);

// ---------------------------------------------------------------------------------------------
// The tables themselves

/// What of @p provenance differs from the manifest @p manifest and the schema this test reads.
[[nodiscard]] std::vector<std::string> provenanceMismatches(
    std::string_view table, const QtRocket::Test::GoldenPlotProvenance& provenance,
    const QtRocket::Test::GoldenManifest& manifest)
{
    std::vector<std::string> lines;
    const Mismatches         mismatches = Mismatches(lines).within(table);
    mismatches.integer("the schema version", provenance.schemaVersion,
                       QtRocket::Test::kGoldenPlotSchemaVersion);
    mismatches.text("the commit", provenance.openrocketCommit, manifest.openrocketCommit);
    mismatches.text("the version", provenance.openrocketVersion, manifest.openrocketVersion);
    mismatches.integer("dirty", provenance.dirty ? 1 : 0, 0);
    return lines;
}

// Both tables come from the OpenRocket commit of the goldens' manifest, unmodified, and are of
// the schema this test reads.
TEST(SimulationPlotGoldenTables, AreOfTheGoldensOpenRocketCommit)
{
    const auto manifest = QtRocket::Test::loadGoldenManifest();
    ASSERT_TRUE(manifest.has_value()) << manifest.error().toString();
    const GoldenPlotTable*   table   = plotTableOrFail();
    const GoldenPlotDetails* details = plotDetailsOrFail();
    ASSERT_NE(table, nullptr);
    ASSERT_NE(details, nullptr);
    EXPECT_EQ(provenanceMismatches("simulation-plots.json", table->provenance, *manifest),
              QtRocket::Test::noGoldenMismatches());
    EXPECT_EQ(provenanceMismatches("simulation-plot-details.json", details->provenance, *manifest),
              QtRocket::Test::noGoldenMismatches());
}

/// "<file> <simulation>" of every simulation of @p simulations.
template <class S>
[[nodiscard]] std::vector<std::string> simulationKeys(const std::vector<S>& simulations)
{
    std::vector<std::string> keys;
    for (const S& simulation : simulations)
    {
        keys.push_back(std::format("{} {}", simulation.file, simulation.simulation));
    }
    return keys;
}

/// The number of plots of @p table that are in their simulation's place of their preset (0 to 8).
[[nodiscard]] std::size_t plotsInPresetOrder(const GoldenPlotTable& table)
{
    std::size_t plots = 0;
    for (const GoldenPlotSimulation& simulation : table.simulations)
    {
        for (std::size_t i = 0; i < simulation.plots.size(); ++i)
        {
            if (simulation.plots[i].preset == i)
            {
                ++plots;
            }
        }
    }
    return plots;
}

/// The simulations of @p details that @p table lacks, and those whose number of plots or series
/// differs from the table's.
[[nodiscard]] std::vector<std::string> detailsUnlikeTheTable(const GoldenPlotTable&   table,
                                                             const GoldenPlotDetails& details)
{
    std::vector<std::string> lines;
    for (const GoldenPlotDetailSimulation& detail : details.simulations)
    {
        const Mismatches mismatches = Mismatches(lines).within(
            std::format("{} simulation {}", detail.file, detail.simulation));
        const GoldenPlotSimulation* simulation = table.find(detail.file, detail.simulation);
        if (simulation == nullptr)
        {
            mismatches.fail("is not in the table");
            continue;
        }
        mismatches.text("the name", detail.name, simulation->name);
        if (!mismatches.sameCount("plots", detail.plots.size(), simulation->plots.size()))
        {
            continue;
        }
        for (std::size_t i = 0; i < detail.plots.size(); ++i)
        {
            const Mismatches here = mismatches.within(std::format("plot {}", i));
            here.integer("preset", static_cast<long long>(detail.plots[i].preset),
                         static_cast<long long>(simulation->plots[i].preset));
            static_cast<void>(here.sameCount("series", detail.plots[i].series.size(),
                                             simulation->plots[i].series.size()));
            static_cast<void>(here.sameCount("icons", detail.plots[i].icons.size(),
                                             simulation->plots[i].icons.count));
        }
    }
    return lines;
}

// The compact table holds the 54 stored simulations of the examples, each with the 9 presets in
// their order: 486 plots; the details hold ten of them, plot for plot.
TEST(SimulationPlotGoldenTables, HoldEveryStoredSimulationWithEveryPreset)
{
    const GoldenPlotTable*   table   = plotTableOrFail();
    const GoldenPlotDetails* details = plotDetailsOrFail();
    ASSERT_NE(table, nullptr);
    ASSERT_NE(details, nullptr);
    EXPECT_EQ(table->presets.size(), 9U);
    EXPECT_EQ(table->simulations.size(), 54U);
    EXPECT_EQ(plotsInPresetOrder(*table), 486U);
    EXPECT_EQ(details->simulations.size(), 10U);
    EXPECT_EQ(detailsUnlikeTheTable(*table, *details), QtRocket::Test::noGoldenMismatches());
}

/// What of QtRocket's SimulationPlotConfiguration::defaultConfigurations() differs from the
/// presets of @p table.
[[nodiscard]] std::vector<std::string> presetMismatches(const GoldenPlotTable& table)
{
    std::vector<std::string>                       lines;
    const std::vector<SimulationPlotConfiguration> presets =
        SimulationPlotConfiguration::defaultConfigurations();
    if (!Mismatches(lines).sameCount("presets", presets.size(), table.presets.size()))
    {
        return lines;
    }
    for (std::size_t k = 0; k < presets.size(); ++k)
    {
        const Mismatches mismatches = Mismatches(lines).within(std::format("preset {}", k));
        GoldenPlot       golden;
        golden.name   = table.presets[k].name;
        golden.domain = table.presets[k].domain;
        golden.rows   = table.presets[k].rows;
        compareConfiguration(mismatches, presets[k], golden);
        std::vector<std::string> events;
        for (const FlightEvent::Type type : presets[k].getActiveEvents())
        {
            events.emplace_back(QtRocket::name(type));
        }
        if (events != table.presets[k].events)
        {
            mismatches.fail(std::format("events {} differ from OpenRocket's {}", listText(events),
                                        listText(table.presets[k].events)));
        }
    }
    return lines;
}

// The presets the probe chose through OpenRocket's plot panel are QtRocket's
// SimulationPlotConfiguration::defaultConfigurations(): names, domains, rows and events.
TEST(SimulationPlotGoldenTables, ThePresetsAreQtRocketsDefaultConfigurations)
{
    const DefaultUnitsGuard units;
    const GoldenPlotTable*  table = plotTableOrFail();
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(presetMismatches(*table), QtRocket::Test::noGoldenMismatches());
}

/// The plots of @p table that record OpenRocket's own choice, "<file> <simulation> <preset>".
[[nodiscard]] std::vector<std::string> openRocketChoices(const GoldenPlotTable& table)
{
    std::vector<std::string> plots;
    for (const GoldenPlotSimulation& simulation : table.simulations)
    {
        for (const GoldenPlot& plot : simulation.plots)
        {
            if (plot.openRocket.has_value())
            {
                plots.push_back(
                    std::format("{} {} {}", simulation.file, simulation.simulation, plot.preset));
            }
        }
    }
    return plots;
}

/// OpenRocket's own choice recorded with @p plot; after a failure, an empty one.
[[nodiscard]] QtRocket::Test::GoldenPlotOpenRocket openRocketChoiceOf(const GoldenPlot& plot)
{
    if (!plot.openRocket.has_value())
    {
        ADD_FAILURE() << "no choice of OpenRocket's recorded";
        return {};
    }
    return *plot.openRocket;
}

/// Whether a range of @p candidates is infinite.
[[nodiscard]] bool hasInfiniteRange(
    const std::vector<QtRocket::Test::GoldenPlotCandidate>& candidates)
{
    return std::ranges::any_of(candidates, [](const QtRocket::Test::GoldenPlotCandidate& c) {
        return std::ranges::any_of(c.fit.ranges, [](const GoldenPlotPair& range) {
            return std::isinf(range[0]) || std::isinf(range[1]);
        });
    });
}

/// The plots of @p table with a candidate of an infinite range, "<file> <simulation> <preset>".
[[nodiscard]] std::vector<std::string> plotsWithInfiniteCandidates(const GoldenPlotTable& table)
{
    std::vector<std::string> plots;
    for (const GoldenPlotSimulation& simulation : table.simulations)
    {
        for (const GoldenPlot& plot : simulation.plots)
        {
            if (hasInfiniteRange(plot.candidates))
            {
                plots.push_back(
                    std::format("{} {} {}", simulation.file, simulation.simulation, plot.preset));
            }
        }
    }
    return plots;
}

// Bug 45 (PlotConfiguration.h): OpenRocket's common zero gives a candidate of one plot of the
// examples an infinite bound, "Dual parachute deployment"'s first simulation with the time step
// preset, whose automatic axes it therefore scores and chooses otherwise. The table records
// OpenRocket's own choice for that plot alone, and no candidate of the table (QtRocket's) has an
// infinite range.
TEST(SimulationPlotGoldenTables, OnlyBug45ChangesOpenRocketsChoice)
{
    const GoldenPlotTable* table = plotTableOrFail();
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(openRocketChoices(*table),
              (std::vector<std::string>{"Dual parachute deployment.ork 0 8"}));
    EXPECT_EQ(plotsWithInfiniteCandidates(*table), std::vector<std::string>{});
    const GoldenPlotSimulation* simulation = table->find("Dual parachute deployment.ork", 0);
    ASSERT_NE(simulation, nullptr);
    ASSERT_EQ(simulation->plots.size(), 9U);
    const GoldenPlot&                          plot = simulation->plots[8];
    const QtRocket::Test::GoldenPlotOpenRocket java = openRocketChoiceOf(plot);
    EXPECT_EQ(java.bug, 45);
    EXPECT_TRUE(hasInfiniteRange(java.candidates));
    EXPECT_EQ(java.automatic.axes, (std::vector<int>{0, 1}));
    EXPECT_EQ(java.filled.axes, (std::vector<int>{0, 1}));
    EXPECT_EQ(plot.automatic.axes, (std::vector<int>{1, 0}));
    EXPECT_EQ(plot.filled.axes, (std::vector<int>{1, 0}));
}

/// What parseGoldenPlotTable() makes of @p text: "parsed", or its error's message.
[[nodiscard]] std::string plotTableParse(std::string_view text)
{
    const auto table = QtRocket::Test::parseGoldenPlotTableText(text);
    return table.has_value() ? std::string("parsed") : table.error().message;
}

// The reader refuses a table of another schema and one with a member missing or of the wrong
// type, naming the member.
TEST(SimulationPlotGoldenTables, AMalformedTableIsAParseError)
{
    constexpr std::string_view kHead =
        R"("schemaVersion": 1, "openrocket": {"commit": "c", "version": "v", "dirty": false})";
    EXPECT_EQ(plotTableParse(std::format(R"({{"schema": "other", {}}})", kHead)),
              R"(simulation-plots.json: the schema is not "simulation-plots")");
    EXPECT_EQ(plotTableParse(
                  std::format(R"({{"schema": "simulation-plots", {}, "presets": []}})", kHead)),
              R"(simulation-plots.json: missing "simulations")");
    EXPECT_EQ(
        plotTableParse(std::format(
            R"({{"schema": "simulation-plots", {}, "presets": [], "simulations": 3}})", kHead)),
        "simulation-plots.json: simulations is not an array");
    EXPECT_EQ(
        plotTableParse(std::format(
            R"({{"schema": "simulation-plots", {}, "presets": [], "simulations": []}})", kHead)),
        "parsed");
}

/// The separation times of the branches of the one simulation of the table @p text, or nothing
/// after a failure.
[[nodiscard]] std::vector<double> parsedSeparationTimes(std::string_view text)
{
    const auto table = QtRocket::Test::parseGoldenPlotTableText(text);
    if (!table.has_value() || table->simulations.size() != 1)
    {
        ADD_FAILURE() << (table.has_value() ? "not one simulation" : table.error().message);
        return {};
    }
    std::vector<double> times;
    for (const auto& branch : table->simulations.front().branches)
    {
        times.push_back(branch.separationTime);
    }
    return times;
}

// The reader keeps every double's bits: a negative zero, the smallest subnormal, NaN and the
// infinities (the strings of the goldens), as the exact comparisons need.
TEST(SimulationPlotGoldenTables, TheReaderKeepsTheBitsOfEveryDouble)
{
    const std::vector<double> times = parsedSeparationTimes(
        R"({"schema": "simulation-plots", "schemaVersion": 1,
            "openrocket": {"commit": "c", "version": "v", "dirty": false}, "presets": [],
            "simulations": [{"file": "f.ork", "simulation": 0, "name": "S", "seriesLabels": [],
              "plots": [], "branches": [
                {"name": "a", "points": 1, "separationTime": -0.0, "separationIndex": -1},
                {"name": "b", "points": 1, "separationTime": 5e-324, "separationIndex": -1},
                {"name": "c", "points": 1, "separationTime": 0.1, "separationIndex": -1},
                {"name": "d", "points": 1, "separationTime": "NaN", "separationIndex": -1},
                {"name": "e", "points": 1, "separationTime": "-Infinity", "separationIndex": -1}]}]})");
    ASSERT_EQ(times.size(), 5U);
    EXPECT_EQ(javaDoubleBits(times[0]), std::uint64_t{0x8000000000000000});
    EXPECT_EQ(javaDoubleBits(times[1]), std::uint64_t{1});
    EXPECT_EQ(javaDoubleBits(times[2]), std::uint64_t{0x3fb999999999999a});
    EXPECT_EQ(javaDoubleBits(times[3]), std::uint64_t{0x7ff8000000000000});
    EXPECT_EQ(javaDoubleBits(times[4]), std::uint64_t{0xfff0000000000000});
    EXPECT_TRUE(sameDoubleBits(times[3], std::numeric_limits<double>::quiet_NaN()));
    EXPECT_FALSE(sameDoubleBits(times[0], 0.0));
}

// The digest of a series is that of its points' bits, x before y (the tables' 1725 series check
// it against the probe's): a series without points has SHA-256's digest of nothing, a NaN is
// Java's canonical one whatever its sign and payload, and the order and the signs of zeros count.
TEST(SimulationPlotGoldenTables, ASeriesDigestIsThatOfItsPointsBits)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(QtRocket::Test::seriesDigest({}, {}), "e3b0c44298fc1c14");
    EXPECT_EQ(QtRocket::Test::seriesDigest({0.0, 1.0}, {nan, -2.5}),
              QtRocket::Test::seriesDigest({0.0, 1.0}, {-nan, -2.5}));
    EXPECT_NE(QtRocket::Test::seriesDigest({0.0, 1.0}, {2.0, 3.0}),
              QtRocket::Test::seriesDigest({2.0, 3.0}, {0.0, 1.0}));
    EXPECT_NE(QtRocket::Test::seriesDigest({0.0}, {1.0}),
              QtRocket::Test::seriesDigest({-0.0}, {1.0}));
    EXPECT_EQ(QtRocket::Test::shortDigest(""), "e3b0c44298fc1c14");
}

}  // namespace
