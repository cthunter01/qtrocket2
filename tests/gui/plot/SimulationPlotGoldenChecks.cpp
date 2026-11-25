#include "plot/SimulationPlotGoldenChecks.h"

#include <array>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QIcon>
#include <QImage>
#include <QPalette>
#include <QString>

#include <gtest/gtest.h>
#include <rocketplot/Axis.h>
#include <rocketplot/LineSeries.h>
#include <rocketplot/PlotWidget.h>
#include <rocketplot/text.h>

#include "QtRocket/gui/plot/PlotColors.h"
#include "QtRocket/gui/plot/PlotEventGraphics.h"
#include "QtRocket/gui/plot/SimulationPlot.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/plot/SimulationPlotModel.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "plot/PlotReadback.h"
#include "plot/SimulationPlotGoldens.h"

namespace QtRocket::Gui::Test
{

namespace
{

using QtRocket::Test::GoldenPlot;
using QtRocket::Test::GoldenPlotDetail;
using QtRocket::Test::GoldenPlotDetailSimulation;
using QtRocket::Test::GoldenPlotPair;

/// "<double> (<its bits>)".
[[nodiscard]] std::string doubleText(double value)
{
    return std::format("{} ({:016x})", value, QtRocket::Test::javaDoubleBits(value));
}

/// The look of an event group or icon: the type whose colour and image it takes, and for a
/// warning its priority.
struct EventLook
{
    FlightEvent::Type              type = FlightEvent::Type::LAUNCH;
    std::optional<MessagePriority> priority;
};

/// The look @p look of the tables ("APOGEE", "SIM_WARN/HIGH"), nullopt for an unknown type.
[[nodiscard]] std::optional<EventLook> goldenLook(std::string_view look)
{
    const std::size_t      slash    = look.find('/');
    const std::string_view typeName = look.substr(0, slash);
    for (const FlightEvent::Type type : FlightEvent::kAllTypes)
    {
        if (QtRocket::name(type) == typeName)
        {
            EventLook found{.type = type, .priority = std::nullopt};
            if (slash != std::string_view::npos)
            {
                found.priority = priorityFromExportLabel(look.substr(slash + 1));
            }
            return found;
        }
    }
    return std::nullopt;
}

/// The look of @p icon as the tables write it: the name of its type, for a warning "/" and its
/// priority's export label.
[[nodiscard]] std::string lookTextOf(const SimulationPlotModel::EventIcon& icon)
{
    std::string text(QtRocket::name(icon.lookType));
    if (icon.lookWarningPriority.has_value())
    {
        text += std::format("/{}", exportLabel(*icon.lookWarningPriority));
    }
    return text;
}

/// The image a look's icon has at 16 pixels (a null image for a look without one).
[[nodiscard]] QImage lookImage(FlightEvent::Type type, std::optional<MessagePriority> priority)
{
    return iconImage(PlotEventGraphics::eventIcon(type, priority));
}

/// The icons of @p plot's model that the plot draws, in its order: those whose look has an image
/// (SimulationPlot::drawEventIcons(), as Java's plotIconMarkers()).
[[nodiscard]] std::vector<SimulationPlotModel::EventIcon> drawnModelIcons(
    const SimulationPlot& plot)
{
    std::vector<SimulationPlotModel::EventIcon> drawn;
    for (SimulationPlotModel::EventIcon& icon : plot.model().getIconMarkers())
    {
        if (!PlotEventGraphics::eventIcon(icon.lookType, icon.lookWarningPriority).isNull())
        {
            drawn.push_back(std::move(icon));
        }
    }
    return drawn;
}

/// Whether series @p series of @p plot is drawn on the right axis (false for no such series).
[[nodiscard]] bool onSecondaryAxis(const SimulationPlot& plot, std::size_t series)
{
    const rocketplot::LineSeries* const line =
        series < plot.seriesCount() ? plot.series(series) : nullptr;
    return line != nullptr && line->isOnSecondaryYAxis();
}

/// The icons drawn (@p read) against the model's icons they stand for (@p model) and the digests
/// of @p golden: the axis and the image of each, the digests of all.
void compareIconsWithModel(const SimulationPlot& plot, const std::vector<IconReadback>& read,
                           const std::vector<SimulationPlotModel::EventIcon>& model,
                           const GoldenPlot& golden, const SimulationPlotMismatches& mismatches)
{
    std::string places;
    std::string toolTips;
    for (std::size_t i = 0; i < read.size(); ++i)
    {
        const IconReadback&                   drawn = read[i];
        const SimulationPlotModel::EventIcon& icon  = model[i];
        places += std::format("{} {} {:016x} {:016x} {:016x} {:016x} {}\n", icon.series, icon.group,
                              QtRocket::Test::javaDoubleBits(icon.time),
                              QtRocket::Test::javaDoubleBits(drawn.position.x()),
                              QtRocket::Test::javaDoubleBits(drawn.position.y()),
                              QtRocket::Test::javaDoubleBits(icon.yOnLeftAxis), lookTextOf(icon));
        toolTips += QtStrings::toUtf8(drawn.toolTip) + "\n";
        if (drawn.secondaryYAxis != onSecondaryAxis(plot, icon.series))
        {
            mismatches.fail(std::format("icon {} is not on its series' axis", i));
        }
        if (drawn.icon != lookImage(icon.lookType, icon.lookWarningPriority))
        {
            mismatches.fail(std::format("icon {}'s image is not that of {}", i, lookTextOf(icon)));
        }
    }
    const std::string placesDigest = QtRocket::Test::shortDigest(places);
    if (placesDigest != golden.icons.digest)
    {
        mismatches.fail(std::format("the icons' places' digest {} differs from OpenRocket's {}",
                                    placesDigest, golden.icons.digest));
    }
    const std::string toolTipsDigest = QtRocket::Test::shortDigest(toolTips);
    if (toolTipsDigest != golden.icons.toolTipDigest)
    {
        mismatches.fail(std::format("the icons' tool tips' digest {} differs from OpenRocket's {}",
                                    toolTipsDigest, golden.icons.toolTipDigest));
    }
}

/// The icons drawn (@p read) against the details table's icons of the plot (@p detail).
void compareIconsWithDetail(const SimulationPlot& plot, const std::vector<IconReadback>& read,
                            const GoldenPlotDetail&         detail,
                            const SimulationPlotMismatches& mismatches)
{
    if (!mismatches.sameCount("detailed icons", read.size(), detail.icons.size()))
    {
        return;
    }
    for (std::size_t i = 0; i < read.size(); ++i)
    {
        const QtRocket::Test::GoldenPlotIcon& expected = detail.icons[i];
        mismatches.number(std::format("icon {}'s x", i), read[i].position.x(), expected.x);
        mismatches.number(std::format("icon {}'s y", i), read[i].position.y(), expected.y);
        if (read[i].secondaryYAxis != onSecondaryAxis(plot, expected.series))
        {
            mismatches.fail(
                std::format("icon {} is not on the axis of series {}", i, expected.series));
        }
        if (read[i].toolTip != QtStrings::fromUtf8(expected.toolTip))
        {
            mismatches.fail(std::format("icon {}'s tool tip differs", i));
        }
    }
}

}  // namespace

SimulationPlotMismatches::SimulationPlotMismatches(std::vector<std::string>& lines,
                                                   std::size_t               preset)
  : m_lines(&lines), m_preset(preset)
{
}

void SimulationPlotMismatches::fail(std::string_view what) const
{
    m_lines->push_back(std::format("preset {}: {}", m_preset, what));
}

void SimulationPlotMismatches::text(std::string_view what, const QString& actual,
                                    std::string_view golden) const
{
    const QString expected = rocketplot::literalText(QtStrings::fromUtf8(golden));
    if (actual != expected)
    {
        fail(std::format(R"({}: "{}" differs from OpenRocket's "{}")", what,
                         QtStrings::toUtf8(actual), QtStrings::toUtf8(expected)));
    }
}

void SimulationPlotMismatches::number(std::string_view what, double actual, double golden) const
{
    if (!QtRocket::Test::sameDoubleBits(actual, golden))
    {
        fail(std::format("{}: {} differs from OpenRocket's {}", what, doubleText(actual),
                         doubleText(golden)));
    }
}

void SimulationPlotMismatches::range(std::string_view what, const rocketplot::Axis& axis,
                                     const GoldenPlotPair& golden) const
{
    number(std::format("{} min", what), axis.min(), golden[0]);
    number(std::format("{} max", what), axis.max(), golden[1]);
}

bool SimulationPlotMismatches::sameCount(std::string_view what, std::size_t actual,
                                         std::size_t golden) const
{
    if (actual != golden)
    {
        fail(std::format("{} {} differ from OpenRocket's {}", actual, what, golden));
    }
    return actual == golden;
}

const GoldenPlotPair& goldenDomainRange(const GoldenPlot& golden, bool eventLinesDrawn)
{
    if (eventLinesDrawn && golden.domainRangeWithEventLines.has_value())
    {
        return *golden.domainRangeWithEventLines;
    }
    return golden.domainRange;
}

void compareSimulationPlotAxes(const SimulationPlot& plot, const GoldenPlot& golden,
                               const GoldenPlotPair&           domainRange,
                               const SimulationPlotMismatches& mismatches)
{
    const rocketplot::PlotWidget& widget = plot.plotWidget();
    mismatches.text("the title", widget.title(), golden.title);
    mismatches.text("the subtitle", widget.subtitle(), golden.subtitle);
    mismatches.text("the x label", widget.xAxis()->label(), golden.domainLabel);
    mismatches.range("x", *widget.xAxis(), domainRange);
    const std::array<const rocketplot::Axis*, 2> axes{widget.yAxis(), widget.yAxis2()};
    const PlotReadback                           read = readPlot(widget);
    if (read.yAxis2Shown != (golden.yAxes[1].series > 0))
    {
        mismatches.fail("the right axis is shown otherwise than OpenRocket's");
    }
    for (std::size_t a = 0; a < axes.size(); ++a)
    {
        if (golden.yAxes.at(a).series == 0)
        {
            continue;
        }
        mismatches.text(std::format("the label of y axis {}", a), axes.at(a)->label(),
                        golden.yAxes.at(a).label.value_or(""));
        mismatches.range(std::format("y axis {}", a), *axes.at(a), golden.yAxes.at(a).range);
    }
}

void compareSimulationPlotSeries(const SimulationPlot& plot, const GoldenPlot& golden,
                                 const SimulationPlotMismatches& mismatches)
{
    const std::vector<SeriesReadback> all = readAllSeries(plot.plotWidget());
    if (!mismatches.sameCount("series", all.size(), golden.series.size()))
    {
        return;
    }
    for (std::size_t i = 0; i < all.size(); ++i)
    {
        const QtRocket::Test::GoldenPlotSeries& expected = golden.series[i];
        mismatches.text(std::format("series {}'s name", i), all[i].name, expected.name);
        if (onSecondaryAxis(plot, i) != (expected.axis == 1))
        {
            mismatches.fail(std::format("series {} is on the other axis", i));
        }
        if (QtRocket::Test::seriesDigest(all[i].x, all[i].y) != expected.digest)
        {
            mismatches.fail(std::format("series {}'s points differ", i));
        }
        if (all[i].color != PlotColors::plotColor(golden.panelColors.at(expected.row)))
        {
            mismatches.fail(
                std::format("series {}'s colour is {}", i, QtStrings::toUtf8(all[i].color.name())));
        }
    }
}

void compareSimulationPlotLegend(const SimulationPlot& plot, const GoldenPlot& golden,
                                 const SimulationPlotMismatches& mismatches)
{
    const std::vector<LegendEntryReadback> entries = readLegend(plot.plotWidget());
    if (!mismatches.sameCount("legend entries", entries.size(), golden.legend.size()))
    {
        return;
    }
    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        mismatches.text(std::format("legend entry {}", i), entries[i].name, golden.legend[i]);
    }
}

void compareSimulationPlotEventLines(const SimulationPlot& plot, const GoldenPlot& golden,
                                     const SimulationPlotMismatches& mismatches)
{
    const AnnotationsReadback read = readAnnotations(plot.plotWidget());
    if (!mismatches.sameCount("event lines", read.events.size(), golden.groups.size()) ||
        !mismatches.sameCount("icons", read.icons.size(), 0U))
    {
        return;
    }
    for (std::size_t i = 0; i < read.events.size(); ++i)
    {
        const QtRocket::Test::GoldenPlotGroup& group = golden.groups[i];
        mismatches.number(std::format("event line {}", i), read.events[i].x, group.time);
        mismatches.text(std::format("event line {}'s label", i), read.events[i].label, group.label);
        const std::optional<EventLook> look = goldenLook(group.look);
        if (!look.has_value())
        {
            mismatches.fail(std::format("event line {}'s look {} is no event type", i, group.look));
            continue;
        }
        if (read.events[i].color != PlotEventGraphics::markerColor(look->type, plot.palette()))
        {
            mismatches.fail(std::format("event line {}'s colour is not {}'s", i, group.look));
        }
        if (read.events[i].icon != lookImage(look->type, look->priority))
        {
            mismatches.fail(std::format("event line {}'s icon is not {}'s", i, group.look));
        }
    }
}

void compareSimulationPlotEventIcons(const SimulationPlot& plot, const GoldenPlot& golden,
                                     const GoldenPlotDetail*         detail,
                                     const SimulationPlotMismatches& mismatches)
{
    const AnnotationsReadback read = readAnnotations(plot.plotWidget());
    if (!mismatches.sameCount("icons", read.icons.size(), golden.icons.count) ||
        !mismatches.sameCount("event lines", read.events.size(), 0U))
    {
        return;
    }
    const std::vector<SimulationPlotModel::EventIcon> model = drawnModelIcons(plot);
    if (mismatches.sameCount("icons of the plot's model", model.size(), read.icons.size()))
    {
        compareIconsWithModel(plot, read.icons, model, golden, mismatches);
    }
    if (detail != nullptr)
    {
        compareIconsWithDetail(plot, read.icons, *detail, mismatches);
    }
}

const GoldenPlotDetail* goldenPlotDetailOf(const GoldenPlotDetailSimulation* detail,
                                           std::size_t                       preset)
{
    if (detail == nullptr)
    {
        return nullptr;
    }
    for (const GoldenPlotDetail& plot : detail->plots)
    {
        if (plot.preset == preset)
        {
            return &plot;
        }
    }
    return nullptr;
}

const QtRocket::Test::GoldenPlotTable* goldenPlotTableOrFail()
{
    const auto& table = QtRocket::Test::goldenPlotTable();
    if (!table.has_value())
    {
        ADD_FAILURE() << table.error().toString();
        return nullptr;
    }
    return &*table;
}

const QtRocket::Test::GoldenPlotDetails* goldenPlotDetailsOrFail()
{
    const auto& details = QtRocket::Test::goldenPlotDetails();
    if (!details.has_value())
    {
        ADD_FAILURE() << details.error().toString();
        return nullptr;
    }
    return &*details;
}

}  // namespace QtRocket::Gui::Test
