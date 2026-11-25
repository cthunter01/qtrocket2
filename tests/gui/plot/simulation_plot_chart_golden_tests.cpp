// The simulation plot of the GUI (SimulationPlot over rocketplot) against OpenRocket's own plots
// of stored simulations of its example designs (the core's plot goldens, tests/data/goldens/plots,
// read through tests/core/plot/SimulationPlotGoldens.h): each of the 9 preset plot configurations
// of a few simulations, chosen as OpenRocket's plot panel chooses one (a copy in the default
// units), drawn and read back without pixels (plot/PlotReadback.h): the titles, the axes' labels
// and ranges (to the last bit: the home ranges, PlotHomeRange), the series (names, axes, colours
// and a digest of their points), the legend, the event lines over time and the event icons. The
// core's tests compare the model itself with every stored simulation; run 3 draws all of them
// through the plot dialog (plan 11.6, E1).
//
// Where QtRocket fixes OpenRocket's bugs the tables hold QtRocket's answers (bugs 41 and 45), and
// the colours are the plot panel's (bug 42); the x axis' range over time is the one widened once
// for the event lines (bug 43).

#include <array>
#include <cstddef>
#include <format>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include <QColor>
#include <QList>
#include <QPalette>
#include <QPointF>
#include <QString>

#include <gtest/gtest.h>
#include <rocketplot/Axis.h>
#include <rocketplot/PlotWidget.h>
#include <rocketplot/Series.h>
#include <rocketplot/text.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/plot/PlotColors.h"
#include "QtRocket/gui/plot/PlotEventGraphics.h"
#include "QtRocket/gui/plot/SimulationPlot.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/plot/PlotPreferences.h"
#include "QtRocket/plot/SimulationPlotConfiguration.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "TestGuiServices.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenExamples.h"
#include "plot/PlotReadback.h"
#include "plot/SimulationPlotGoldens.h"

namespace QtRocket::Gui
{

namespace
{

using QtRocket::Test::GoldenPlot;
using QtRocket::Test::GoldenPlotDetail;
using QtRocket::Test::GoldenPlotDetailSimulation;
using QtRocket::Test::GoldenPlotPair;
using QtRocket::Test::GoldenPlotSimulation;

/// A stored simulation of an example.
struct ChartCase
{
    const char* file;
    std::size_t simulation;
    /// The test's name (GoogleTest's, without spaces).
    const char* name;
};

// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
void PrintTo(const ChartCase& chartCase, std::ostream* out)
{
    *out << chartCase.name;
}

/// The simulations compared: one stage, two and three; one whose drag plot has a warning; all in
/// the details table, which holds their icons.
constexpr std::array kChartCases{
    ChartCase{.file = "A simple model rocket.ork", .simulation = 0, .name = "simple_rocket_1"},
    ChartCase{.file = "ARC payload rocket.ork", .simulation = 0, .name = "arc_payload_1"},
    ChartCase{.file = "Dual parachute deployment.ork", .simulation = 0, .name = "dual_parachute_1"},
    ChartCase{.file = "Three stage low power rocket.ork", .simulation = 0, .name = "three_stage_1"},
};

constexpr int kPlotWidth  = 900;
constexpr int kPlotHeight = 700;

/// "<double> (<its bits>)".
[[nodiscard]] std::string doubleText(double value)
{
    return std::format("{} ({:016x})", value, QtRocket::Test::javaDoubleBits(value));
}

/// The mismatches of one plot, each a line prefixed with the plot's preset.
class ChartMismatches
{
public:
    ChartMismatches(std::vector<std::string>& lines, std::size_t preset)
      : m_lines(&lines), m_preset(preset)
    {
    }

    void fail(std::string_view what) const
    {
        m_lines->push_back(std::format("preset {}: {}", m_preset, what));
    }

    /// A text the plot was given (@p actual) against the golden's, given as rocketplot's literal.
    void text(std::string_view what, const QString& actual, std::string_view golden) const
    {
        const QString expected = rocketplot::literalText(QtStrings::fromUtf8(golden));
        if (actual != expected)
        {
            fail(std::format(R"({}: "{}" differs from OpenRocket's "{}")", what,
                             QtStrings::toUtf8(actual), QtStrings::toUtf8(expected)));
        }
    }

    void number(std::string_view what, double actual, double golden) const
    {
        if (!QtRocket::Test::sameDoubleBits(actual, golden))
        {
            fail(std::format("{}: {} differs from OpenRocket's {}", what, doubleText(actual),
                             doubleText(golden)));
        }
    }

    void range(std::string_view what, const rocketplot::Axis& axis,
               const GoldenPlotPair& golden) const
    {
        number(std::format("{} min", what), axis.min(), golden[0]);
        number(std::format("{} max", what), axis.max(), golden[1]);
    }

    /// Whether the counts agree (a mismatch otherwise).
    [[nodiscard]] bool sameCount(std::string_view what, std::size_t actual,
                                 std::size_t golden) const
    {
        if (actual != golden)
        {
            fail(std::format("{} {} differ from OpenRocket's {}", actual, what, golden));
        }
        return actual == golden;
    }

private:
    std::vector<std::string>* m_lines;
    std::size_t               m_preset;
};

/// The type of the look @p look of the tables ("APOGEE", "SIM_WARN/HIGH"), nullopt for none.
[[nodiscard]] std::optional<FlightEvent::Type> lookType(std::string_view look)
{
    const std::string_view typeName = look.substr(0, look.find('/'));
    for (const FlightEvent::Type type : FlightEvent::kAllTypes)
    {
        if (QtRocket::name(type) == typeName)
        {
            return type;
        }
    }
    return std::nullopt;
}

/// The titles and the axes of @p plot against @p golden.
void compareAxes(const SimulationPlot& plot, const GoldenPlot& golden,
                 const ChartMismatches& mismatches)
{
    const rocketplot::PlotWidget& widget = plot.plotWidget();
    mismatches.text("the title", widget.title(), golden.title);
    mismatches.text("the subtitle", widget.subtitle(), golden.subtitle);
    mismatches.text("the x label", widget.xAxis()->label(), golden.domainLabel);
    // Over time the event lines widen the domain once (bug 43); over anything else they are none.
    mismatches.range("x", *widget.xAxis(),
                     golden.domainRangeWithEventLines.value_or(golden.domainRange));
    const std::array<const rocketplot::Axis*, 2> axes{widget.yAxis(), widget.yAxis2()};
    const Gui::Test::PlotReadback                read = Gui::Test::readPlot(widget);
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

/// The series of @p plot against @p golden.
void compareSeries(const SimulationPlot& plot, const GoldenPlot& golden,
                   const ChartMismatches& mismatches)
{
    const std::vector<Gui::Test::SeriesReadback> all = Gui::Test::readAllSeries(plot.plotWidget());
    if (!mismatches.sameCount("series", all.size(), golden.series.size()))
    {
        return;
    }
    for (std::size_t i = 0; i < all.size(); ++i)
    {
        const QtRocket::Test::GoldenPlotSeries& expected = golden.series[i];
        mismatches.text(std::format("series {}'s name", i), all[i].name, expected.name);
        if (plot.series(i)->isOnSecondaryYAxis() != (expected.axis == 1))
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

/// The legend of @p plot against @p golden.
void compareLegend(const SimulationPlot& plot, const GoldenPlot& golden,
                   const ChartMismatches& mismatches)
{
    const std::vector<Gui::Test::LegendEntryReadback> entries =
        Gui::Test::readLegend(plot.plotWidget());
    if (!mismatches.sameCount("legend entries", entries.size(), golden.legend.size()))
    {
        return;
    }
    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        mismatches.text(std::format("legend entry {}", i), entries[i].name, golden.legend[i]);
    }
}

/// The event lines of @p plot (over time) against @p golden's groups.
void compareEventLines(const SimulationPlot& plot, const GoldenPlot& golden,
                       const ChartMismatches& mismatches)
{
    const Gui::Test::AnnotationsReadback read = Gui::Test::readAnnotations(plot.plotWidget());
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
        const std::optional<FlightEvent::Type> look = lookType(group.look);
        if (!look.has_value() ||
            read.events[i].color != PlotEventGraphics::markerColor(*look, plot.palette()))
        {
            mismatches.fail(std::format("event line {}'s colour is not {}'s", i, group.look));
        }
    }
}

/// The event icons of @p plot against @p golden (their number) and @p detail (each).
void compareEventIcons(const SimulationPlot& plot, const GoldenPlot& golden,
                       const GoldenPlotDetail* detail, const ChartMismatches& mismatches)
{
    const Gui::Test::AnnotationsReadback read = Gui::Test::readAnnotations(plot.plotWidget());
    if (!mismatches.sameCount("icons", read.icons.size(), golden.icons.count) ||
        !mismatches.sameCount("event lines", read.events.size(), 0U) || detail == nullptr ||
        !mismatches.sameCount("detailed icons", read.icons.size(), detail->icons.size()))
    {
        return;
    }
    for (std::size_t i = 0; i < read.icons.size(); ++i)
    {
        const QtRocket::Test::GoldenPlotIcon& expected = detail->icons[i];
        mismatches.number(std::format("icon {}'s x", i), read.icons[i].position.x(), expected.x);
        mismatches.number(std::format("icon {}'s y", i), read.icons[i].position.y(), expected.y);
        if (read.icons[i].secondaryYAxis != plot.series(expected.series)->isOnSecondaryYAxis())
        {
            mismatches.fail(std::format("icon {} is not on its series' axis", i));
        }
        if (read.icons[i].toolTip != QtStrings::fromUtf8(expected.toolTip))
        {
            mismatches.fail(std::format("icon {}'s tool tip differs", i));
        }
    }
}

/// The detail of preset @p preset of @p detail, or null.
[[nodiscard]] const GoldenPlotDetail* detailOf(const GoldenPlotDetailSimulation* detail,
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

/// The mismatches of @p simulation's plots with each preset against @p golden.
[[nodiscard]] std::vector<std::string> chartMismatches(const Simulation&                 simulation,
                                                       const GoldenPlotSimulation&       golden,
                                                       const GoldenPlotDetailSimulation* detail)
{
    std::vector<std::string>                       lines;
    Gui::Test::TestGuiServices                     services;
    const std::vector<SimulationPlotConfiguration> presets =
        SimulationPlotConfiguration::defaultConfigurations();
    for (const GoldenPlot& goldenPlot : golden.plots)
    {
        const ChartMismatches       mismatches(lines, goldenPlot.preset);
        SimulationPlotConfiguration preset = presets.at(goldenPlot.preset);
        preset.resetUnits();
        SimulationPlot plot(services.services(), simulation, preset, false);
        plot.resize(kPlotWidth, kPlotHeight);
        // As OpenRocket's plot dialog opens it, with "Show events" (as the preference is at first).
        PlotPreferences::setMarkerStyleIcon(services.preferences(), false);
        plot.setShowEvents(true);
        compareAxes(plot, goldenPlot, mismatches);
        compareSeries(plot, goldenPlot, mismatches);
        compareLegend(plot, goldenPlot, mismatches);
        if (goldenPlot.domainRangeWithEventLines.has_value())
        {
            compareEventLines(plot, goldenPlot, mismatches);
            // The marker style "Icon": icons over time too.
            PlotPreferences::setMarkerStyleIcon(services.preferences(), true);
            plot.setShowEvents(true);
        }
        compareEventIcons(plot, goldenPlot, detailOf(detail, goldenPlot.preset), mismatches);
    }
    return lines;
}

/// The plot goldens' table, or a failure.
[[nodiscard]] const QtRocket::Test::GoldenPlotTable* chartTableOrFail()
{
    const auto& table = QtRocket::Test::goldenPlotTable();
    if (!table.has_value())
    {
        ADD_FAILURE() << table.error().toString();
        return nullptr;
    }
    return &*table;
}

[[nodiscard]] const QtRocket::Test::GoldenPlotDetails* chartDetailsOrFail()
{
    const auto& details = QtRocket::Test::goldenPlotDetails();
    if (!details.has_value())
    {
        ADD_FAILURE() << details.error().toString();
        return nullptr;
    }
    return &*details;
}

class SimulationPlotChartGolden : public Gui::Test::GuiTest,
                                  public ::testing::WithParamInterface<ChartCase>
{ };

}  // namespace

TEST_P(SimulationPlotChartGolden, EveryPresetIsDrawnAsOpenRocketsPlot)
{
    const QtRocket::Test::GoldenPlotTable*   table   = chartTableOrFail();
    const QtRocket::Test::GoldenPlotDetails* details = chartDetailsOrFail();
    ASSERT_NE(table, nullptr);
    ASSERT_NE(details, nullptr);
    const GoldenPlotSimulation* const golden = table->find(GetParam().file, GetParam().simulation);
    ASSERT_NE(golden, nullptr);
    const GoldenPlotDetailSimulation* const detail =
        details->find(GetParam().file, GetParam().simulation);
    EXPECT_NE(detail, nullptr) << "every simulation here is in the details table";
    const QtRocket::Test::GoldenExample* const example =
        QtRocket::Test::findGoldenExample(GetParam().file);
    ASSERT_NE(example, nullptr);
    auto loaded = QtRocket::Test::loadGoldenExample(*example);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    const auto& simulations = loaded->document->getSimulations();
    ASSERT_LT(GetParam().simulation, simulations.size());
    EXPECT_EQ(golden->plots.size(), 9U);
    EXPECT_EQ(chartMismatches(*simulations.at(GetParam().simulation), *golden, detail),
              QtRocket::Test::noGoldenMismatches());
}

INSTANTIATE_TEST_SUITE_P(StoredSimulations, SimulationPlotChartGolden,
                         ::testing::ValuesIn(kChartCases),
                         [](const ::testing::TestParamInfo<ChartCase>& chartCase) {
                             return std::string(chartCase.param.name);
                         });

}  // namespace QtRocket::Gui
