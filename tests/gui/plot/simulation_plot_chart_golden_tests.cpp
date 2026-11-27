// The simulation plot of the GUI (SimulationPlot over rocketplot) against OpenRocket's own plots
// of stored simulations of its example designs (the core's plot goldens, tests/data/goldens/plots,
// read through tests/core/plot/SimulationPlotGoldens.h): each of the 9 preset plot configurations
// of a few simulations, chosen as OpenRocket's plot panel chooses one (a copy in the default
// units), drawn and read back without pixels (plot/PlotReadback.h) and compared as
// plot/SimulationPlotGoldenChecks.h says: the titles, the axes' labels and ranges (to the last
// bit: the axes' home ranges), the series (names, axes, colours and a digest of their
// points), the legend, the event lines over time and the event icons (also over time, in the
// marker style "Icon", on the same plot). The core's tests compare the model itself with every
// stored simulation; the exit suite SimulationPlotPresetsGolden (dialogs/plot/
// simulation_plot_presets_golden_tests.cpp, plan 11.6, E1) draws all of them through the plot
// dialog.
//
// Where QtRocket fixes OpenRocket's bugs the tables hold QtRocket's answers (bugs 41 and 45), and
// the colours are the plot panel's (bug 42); the x axis' range over time is the one widened once
// for the event lines (bug 43).

#include <array>
#include <cstddef>
#include <ostream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/plot/SimulationPlot.h"
#include "QtRocket/plot/PlotPreferences.h"
#include "QtRocket/plot/SimulationPlotConfiguration.h"
#include "QtRocket/simulation/Simulation.h"
#include "TestGuiServices.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenExamples.h"
#include "plot/SimulationPlotGoldenChecks.h"
#include "plot/SimulationPlotGoldens.h"

namespace QtRocket::Gui
{

namespace
{

using QtRocket::Test::GoldenPlot;
using QtRocket::Test::GoldenPlotDetailSimulation;
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
        const Gui::Test::SimulationPlotMismatches mismatches(lines, goldenPlot.preset);
        SimulationPlotConfiguration               preset = presets.at(goldenPlot.preset);
        preset.resetUnits();
        SimulationPlot plot(services.services(), simulation, preset, false);
        plot.resize(kPlotWidth, kPlotHeight);
        // As OpenRocket's plot dialog opens it, with "Show events" (as the preference is at first).
        PlotPreferences::setMarkerStyleIcon(services.preferences(), false);
        plot.setShowEvents(true);
        const bool overTime = goldenPlot.domainRangeWithEventLines.has_value();
        Gui::Test::compareSimulationPlotAxes(
            plot, goldenPlot, Gui::Test::goldenDomainRange(goldenPlot, overTime), mismatches);
        Gui::Test::compareSimulationPlotSeries(plot, goldenPlot, mismatches);
        Gui::Test::compareSimulationPlotLegend(plot, goldenPlot, mismatches);
        if (overTime)
        {
            Gui::Test::compareSimulationPlotEventLines(plot, goldenPlot, mismatches);
            // The marker style "Icon": icons over time too.
            PlotPreferences::setMarkerStyleIcon(services.preferences(), true);
            plot.setShowEvents(true);
        }
        Gui::Test::compareSimulationPlotEventIcons(
            plot, goldenPlot, Gui::Test::goldenPlotDetailOf(detail, goldenPlot.preset), mismatches);
    }
    return lines;
}

class SimulationPlotChartGolden : public Gui::Test::GuiTest,
                                  public ::testing::WithParamInterface<ChartCase>
{ };

}  // namespace

TEST_P(SimulationPlotChartGolden, EveryPresetIsDrawnAsOpenRocketsPlot)
{
    const QtRocket::Test::GoldenPlotTable*   table   = Gui::Test::goldenPlotTableOrFail();
    const QtRocket::Test::GoldenPlotDetails* details = Gui::Test::goldenPlotDetailsOrFail();
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
