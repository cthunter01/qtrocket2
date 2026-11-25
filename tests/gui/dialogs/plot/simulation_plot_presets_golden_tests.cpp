// The exit suite E1 of Milestone 4 (plan 11.6, "every default plot preset renders"): every stored
// simulation of OpenRocket's example designs (the 54 of the core's plot goldens) with each of
// OpenRocket's 9 preset plot configurations, plotted through the plot dialog as the plot tab's
// Plot opens it (SimulationPlotDialog over the simulation and the preset chosen in the tab's
// preset box: a copy in the default units, Gui::Test::presetConfiguration()), with the preferences
// as they are at first ("Show events" on, "Show data points" off, the marker style "Line"), drawn
// (QWidget::grab()) and read back without pixels against OpenRocket's own plot (the core's plot
// goldens, tests/data/goldens/plots, compared as plot/SimulationPlotGoldenChecks.h says: the
// titles, the axes and their ranges to the last bit, the series and their colours, the legend, the
// event lines and the event icons). A plot over time is drawn a second time in the marker style
// "Icon" (the preference set before the dialog is made, as the plot tab's radio button sets it
// before Plot): icons in place of the lines, and the x range not widened for lines (OpenRocket bug
// 43 widens it only for them). What the dialog adds to the plot is compared too: its title (the
// simulation's name), the warning over another x axis than the time, "Show errors" hidden (no
// stored flight of the examples has an abort) and the stage box ("All Stages" and the stages,
// shown for two or more).
//
// Each test is one simulation with one preset (486), so that a failure names both; ctest runs
// them all in one process (tests/gui/CMakeLists.txt: the test "StoredSimulations/
// SimulationPlotPresetsGolden"), in which each example is loaded once (the fixture's
// storedSimulation()) and the motor database and the plot goldens are read once: a process of its
// own for each test would spend most of its time on them (about 0.6 s of a test's 0.8 s in a Debug
// build, 3 s of 3.5 s under ASan; a plot itself takes 0.01-0.03 s, 0.05-0.1 s under ASan).

#include <array>
#include <cstddef>
#include <format>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include <QCheckBox>
#include <QComboBox>
#include <QPixmap>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/dialogs/plot/SimulationPlotDialog.h"
#include "QtRocket/gui/plot/SimulationPlot.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/plot/PlotPreferences.h"
#include "QtRocket/plot/SimulationPlotConfiguration.h"
#include "QtRocket/simulation/Simulation.h"
#include "TestGuiServices.h"
#include "dialogs/plot/PlotDialogTestSupport.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenExamples.h"
#include "plot/SimulationPlotGoldenChecks.h"
#include "plot/SimulationPlotGoldens.h"

namespace QtRocket::Gui
{

namespace
{

using QtRocket::Test::GoldenPlot;
using QtRocket::Test::GoldenPlotDetail;
using QtRocket::Test::GoldenPlotSimulation;

/// An example design and how many stored simulations it has (all of them in the plot goldens).
struct StoredExample
{
    const char* file;
    /// Its part of a test's name.
    const char* name;
    std::size_t simulations;
};

/// The examples, in the order of the plot goldens (that of the file names), with their stored
/// simulations: 54 (SimulationPlotPresetsGoldenCases checks them against the goldens).
constexpr std::array kStoredExamples{
    StoredExample{.file        = "3D printable nose cone and fins.ork",
                  .name        = "printable_nose_cone",
                  .simulations = 5},
    StoredExample{.file = "A simple model rocket.ork", .name = "simple_rocket", .simulations = 5},
    StoredExample{.file = "ARC payload rocket.ork", .name = "arc_payload", .simulations = 1},
    StoredExample{.file = "Airstart timing.ork", .name = "airstart_timing", .simulations = 5},
    StoredExample{.file = "Chute release.ork", .name = "chute_release", .simulations = 2},
    StoredExample{.file = "Clustered motors.ork", .name = "clustered_motors", .simulations = 5},
    StoredExample{.file = "Deployable payload.ork", .name = "deployable_payload", .simulations = 5},
    StoredExample{
        .file = "Dual parachute deployment.ork", .name = "dual_parachute", .simulations = 6},
    StoredExample{
        .file = "Parallel booster staging.ork", .name = "parallel_boosters", .simulations = 2},
    StoredExample{
        .file = "Pods--airframes and winglets.ork", .name = "pods_winglets", .simulations = 5},
    StoredExample{.file        = "Pods--powered with recovery deployment.ork",
                  .name        = "pods_powered",
                  .simulations = 1},
    StoredExample{
        .file = "Simulation extensions.ork", .name = "simulation_extensions", .simulations = 3},
    StoredExample{
        .file = "Simulation scripting.ork", .name = "simulation_scripting", .simulations = 3},
    StoredExample{
        .file = "Three stage low power rocket.ork", .name = "three_stage", .simulations = 3},
    StoredExample{.file = "Tube fin rocket.ork", .name = "tube_fin", .simulations = 1},
    StoredExample{.file = "Two stage high power rocket.ork", .name = "two_stage", .simulations = 2},
};

/// The presets' parts of a test's name, in the order of SimulationPlotConfiguration::
/// defaultConfigurations(): "Vertical motion vs. time", "Total motion vs. time", "Flight side
/// profile", "Ground track", "Stability vs. time", "Drag coefficients vs. Mach number", "Roll
/// characteristics", "Angle of attack and orientation vs. time", "Simulation time step and
/// computation time".
constexpr std::array kPresetNames{
    "vertical_motion",   "total_motion", "side_profile",    "ground_track", "stability",
    "drag_coefficients", "roll",         "angle_of_attack", "time_step",
};

/// A test: stored simulation @p simulation (its index in the document) of the example
/// kStoredExamples[@p example], with the preset @p preset.
struct PresetCase
{
    std::size_t example    = 0;
    std::size_t simulation = 0;
    std::size_t preset     = 0;
};

/// "<example>_<simulation from 1>_<preset>": "simple_rocket_1_vertical_motion".
[[nodiscard]] std::string presetCaseName(const PresetCase& presetCase)
{
    return std::format("{}_{}_{}", kStoredExamples.at(presetCase.example).name,
                       presetCase.simulation + 1, kPresetNames.at(presetCase.preset));
}

// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
void PrintTo(const PresetCase& presetCase, std::ostream* out)
{
    *out << presetCaseName(presetCase);
}

/// Every stored simulation with every preset, example after example, simulation after
/// simulation.
[[nodiscard]] std::vector<PresetCase> presetCases()
{
    std::vector<PresetCase> cases;
    for (std::size_t e = 0; e < kStoredExamples.size(); ++e)
    {
        for (std::size_t s = 0; s < kStoredExamples.at(e).simulations; ++s)
        {
            for (std::size_t p = 0; p < kPresetNames.size(); ++p)
            {
                cases.push_back({.example = e, .simulation = s, .preset = p});
            }
        }
    }
    return cases;
}

/// "<file> #<simulation index>" of each stored simulation of kStoredExamples.
[[nodiscard]] std::vector<std::string> storedSimulationNames()
{
    std::vector<std::string> names;
    for (const StoredExample& example : kStoredExamples)
    {
        for (std::size_t s = 0; s < example.simulations; ++s)
        {
            names.push_back(std::format("{} #{}", example.file, s));
        }
    }
    return names;
}

/// "<file> #<simulation index>" of each simulation of @p table.
[[nodiscard]] std::vector<std::string> goldenSimulationNames(
    const QtRocket::Test::GoldenPlotTable& table)
{
    std::vector<std::string> names;
    names.reserve(table.simulations.size());
    for (const GoldenPlotSimulation& simulation : table.simulations)
    {
        names.push_back(std::format("{} #{}", simulation.file, simulation.simulation));
    }
    return names;
}

/// The presets of each plot of each simulation of @p table, as "<preset>,<preset>,...".
[[nodiscard]] std::vector<std::string> goldenPresetLists(
    const QtRocket::Test::GoldenPlotTable& table)
{
    std::vector<std::string> lists;
    lists.reserve(table.simulations.size());
    for (const GoldenPlotSimulation& simulation : table.simulations)
    {
        std::string list;
        for (const GoldenPlot& plot : simulation.plots)
        {
            list += std::format("{}{}", list.empty() ? "" : ",", plot.preset);
        }
        lists.push_back(std::move(list));
    }
    return lists;
}

/// The plot of preset @p preset of @p simulation, or null.
[[nodiscard]] const GoldenPlot* goldenPlotOf(const GoldenPlotSimulation& simulation,
                                             std::size_t                 preset)
{
    for (const GoldenPlot& plot : simulation.plots)
    {
        if (plot.preset == preset)
        {
            return &plot;
        }
    }
    return nullptr;
}

/// The stage box's items OpenRocket's dialog has for @p golden: "All Stages", then each stage by
/// its label.
[[nodiscard]] QStringList expectedStageItems(const GoldenPlotSimulation& golden)
{
    QStringList items{QStringLiteral("All Stages")};
    for (const std::string& label : golden.seriesLabels)
    {
        items.append(QtStrings::fromUtf8(label));
    }
    return items;
}

/// The texts of @p box's items.
[[nodiscard]] QStringList stageItemsOf(const QComboBox& box)
{
    QStringList items;
    for (int i = 0; i < box.count(); ++i)
    {
        items.append(box.itemText(i));
    }
    return items;
}

/// What @p dialog adds to its plot against OpenRocket's dialog of @p golden with the preset of
/// @p plot.
void compareDialog(const SimulationPlotDialog& dialog, const GoldenPlotSimulation& golden,
                   const GoldenPlot& plot, const Gui::Test::SimulationPlotMismatches& mismatches)
{
    if (dialog.windowTitle() != QtStrings::fromUtf8(golden.name))
    {
        mismatches.fail(std::format(R"(the dialog's title "{}" is not the simulation's name)",
                                    QtStrings::toUtf8(dialog.windowTitle())));
    }
    if (!dialog.showEventsBox().isChecked() || dialog.showPointsBox().isChecked())
    {
        mismatches.fail("the dialog does not open with the events shown and the points hidden");
    }
    const bool overTime = plot.domainRangeWithEventLines.has_value();
    if ((dialog.timeSeriesWarning() == nullptr) != overTime)
    {
        mismatches.fail("the time series warning is there otherwise than over another x axis");
    }
    if (dialog.hasErrors() || !dialog.showErrorsBox().isHidden())
    {
        mismatches.fail("the flight has an abort, or \"Show errors\" is shown without one");
    }
    if (stageItemsOf(dialog.stageBox()) != expectedStageItems(golden))
    {
        mismatches.fail(std::format(
            "the stage box's items are {}",
            QtStrings::toUtf8(stageItemsOf(dialog.stageBox()).join(QStringLiteral(", ")))));
    }
    if (dialog.stageBox().isHidden() != (golden.branches.size() < 2))
    {
        mismatches.fail("the stage box is shown otherwise than for two stages or more");
    }
}

/// The plot dialog of @p simulation with the preset of @p plot, opened over @p services (whose
/// preferences say the marker style: @p markerStyleIcon) and drawn, against OpenRocket's
/// (@p golden, @p plot, and @p detail when the details table holds the plot).
[[nodiscard]] std::vector<std::string> dialogMismatches(
    const GuiServices& services, const Simulation& simulation, const GoldenPlotSimulation& golden,
    const GoldenPlot& plot, const GoldenPlotDetail* detail, bool markerStyleIcon)
{
    std::vector<std::string>                  lines;
    const Gui::Test::SimulationPlotMismatches mismatches(lines, plot.preset);
    SimulationPlotDialog dialog(services, simulation, Gui::Test::presetConfiguration(plot.preset));
    dialog.plot().setReadoutHook(Gui::Test::silentReadout());
    // Drawn as it is shown: laid out at its size and painted, the plot with it.
    const QPixmap drawn = dialog.grab();
    if (drawn.isNull() || drawn.deviceIndependentSize().toSize() != dialog.size())
    {
        mismatches.fail("the dialog is not drawn at its size");
    }
    compareDialog(dialog, golden, plot, mismatches);
    const SimulationPlot& chart = dialog.plot();
    const bool eventLineStyle   = plot.domainRangeWithEventLines.has_value() && !markerStyleIcon;
    Gui::Test::compareSimulationPlotAxes(
        chart, plot, Gui::Test::goldenDomainRange(plot, eventLineStyle), mismatches);
    Gui::Test::compareSimulationPlotSeries(chart, plot, mismatches);
    Gui::Test::compareSimulationPlotLegend(chart, plot, mismatches);
    if (eventLineStyle)
    {
        Gui::Test::compareSimulationPlotEventLines(chart, plot, mismatches);
    }
    else
    {
        Gui::Test::compareSimulationPlotEventIcons(chart, plot, detail, mismatches);
    }
    return lines;
}

/// What the goldens hold for a test: the stored simulation, its plot with the test's preset and,
/// when the details table holds the simulation, that plot in detail.
struct PresetGoldens
{
    const GoldenPlotSimulation* simulation = nullptr;
    const GoldenPlot*           plot       = nullptr;
    const GoldenPlotDetail*     detail     = nullptr;
};

/// The goldens of @p presetCase; a simulation or plot of null, with a test failure where a table
/// cannot be read, when the goldens lack it.
[[nodiscard]] PresetGoldens presetGoldensOf(const PresetCase& presetCase)
{
    const QtRocket::Test::GoldenPlotTable*   table   = Gui::Test::goldenPlotTableOrFail();
    const QtRocket::Test::GoldenPlotDetails* details = Gui::Test::goldenPlotDetailsOrFail();
    if (table == nullptr || details == nullptr)
    {
        return {};
    }
    const char* const           file       = kStoredExamples.at(presetCase.example).file;
    const GoldenPlotSimulation* simulation = table->find(file, presetCase.simulation);
    if (simulation == nullptr)
    {
        return {};
    }
    return {.simulation = simulation,
            .plot       = goldenPlotOf(*simulation, presetCase.preset),
            .detail     = Gui::Test::goldenPlotDetailOf(details->find(file, presetCase.simulation),
                                                        presetCase.preset)};
}

/// The plot dialogs of @p simulation with the preset of @p goldens.plot against OpenRocket's:
/// with the preferences as they are at first, then, over time, in the marker style "Icon" (its
/// mismatches marked so).
[[nodiscard]] std::vector<std::string> presetMismatches(const Simulation&    simulation,
                                                        const PresetGoldens& goldens)
{
    Gui::Test::TestGuiServices services;
    std::vector<std::string>   lines = dialogMismatches(
        services.services(), simulation, *goldens.simulation, *goldens.plot, goldens.detail, false);
    if (goldens.plot->domainRangeWithEventLines.has_value())
    {
        // Over time also in the marker style "Icon", chosen in the plot tab before Plot.
        PlotPreferences::setMarkerStyleIcon(services.preferences(), true);
        for (const std::string& line :
             dialogMismatches(services.services(), simulation, *goldens.simulation, *goldens.plot,
                              goldens.detail, true))
        {
            lines.push_back("marker style Icon, " + line);
        }
    }
    return lines;
}

/// The example a test of the suite loaded last: its file and its document.
struct LoadedExample
{
    std::string                         file;
    std::unique_ptr<OpenRocketDocument> document;
};

class SimulationPlotPresetsGolden : public Gui::Test::GuiTest,
                                    public ::testing::WithParamInterface<PresetCase>
{
public:
    /// The example loaded last goes when the suite ends (before the environment of the golden
    /// examples, whose preference store its simulations keep, goes at the process' end).
    static void TearDownTestSuite() { lastExample() = {}; }

protected:
    /// The stored simulation of @p presetCase, its example loaded (QtRocket::Test::
    /// loadGoldenExample(), as loaded) unless the test before loaded it: the tests go example
    /// after example, so that a run of the suite loads each once. Null, with a test failure, when
    /// it cannot be loaded or has no such simulation.
    [[nodiscard]] static const Simulation* storedSimulation(const PresetCase& presetCase)
    {
        const StoredExample& example = kStoredExamples.at(presetCase.example);
        LoadedExample&       loaded  = lastExample();
        if (loaded.document == nullptr || loaded.file != example.file)
        {
            loaded = {};
            const QtRocket::Test::GoldenExample* const golden =
                QtRocket::Test::findGoldenExample(example.file);
            if (golden == nullptr)
            {
                ADD_FAILURE() << "no golden example " << example.file;
                return nullptr;
            }
            auto load = QtRocket::Test::loadGoldenExample(*golden);
            if (!load.has_value())
            {
                ADD_FAILURE() << example.file << ": " << load.error().toString();
                return nullptr;
            }
            loaded = {.file = example.file, .document = std::move(load->document)};
        }
        const auto& simulations = loaded.document->getSimulations();
        if (presetCase.simulation >= simulations.size())
        {
            ADD_FAILURE() << example.file << " has " << simulations.size() << " simulations";
            return nullptr;
        }
        return simulations.at(presetCase.simulation).get();
    }

private:
    [[nodiscard]] static LoadedExample& lastExample()
    {
        static LoadedExample s_last;
        return s_last;
    }
};

using SimulationPlotPresetsGoldenCases = Gui::Test::GuiTest;

}  // namespace

// The suite's cases are every stored simulation of the plot goldens, in their order, each with
// every preset of SimulationPlotConfiguration, of which the goldens hold a plot each.
TEST_F(SimulationPlotPresetsGoldenCases, AreEveryStoredSimulationWithEveryPreset)
{
    const QtRocket::Test::GoldenPlotTable* table = Gui::Test::goldenPlotTableOrFail();
    ASSERT_NE(table, nullptr);
    EXPECT_EQ(storedSimulationNames(), goldenSimulationNames(*table));
    EXPECT_EQ(kPresetNames.size(), SimulationPlotConfiguration::defaultConfigurations().size());
    EXPECT_EQ(goldenPresetLists(*table),
              std::vector<std::string>(table->simulations.size(), "0,1,2,3,4,5,6,7,8"));
    EXPECT_EQ(presetCases().size(), 54U * 9U);
}

TEST_P(SimulationPlotPresetsGolden, IsDrawnAsOpenRocketsPlot)
{
    const PresetGoldens goldens = presetGoldensOf(GetParam());
    ASSERT_NE(goldens.simulation, nullptr);
    ASSERT_NE(goldens.plot, nullptr);
    const Simulation* const simulation = storedSimulation(GetParam());
    ASSERT_NE(simulation, nullptr);
    EXPECT_EQ(simulation->getName(), goldens.simulation->name);
    EXPECT_EQ(presetMismatches(*simulation, goldens), QtRocket::Test::noGoldenMismatches());
}

INSTANTIATE_TEST_SUITE_P(StoredSimulations, SimulationPlotPresetsGolden,
                         ::testing::ValuesIn(presetCases()),
                         [](const ::testing::TestParamInfo<PresetCase>& presetCase) {
                             return presetCaseName(presetCase.param);
                         });

}  // namespace QtRocket::Gui
