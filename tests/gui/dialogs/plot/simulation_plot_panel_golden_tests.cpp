// The plot tab (SimulationPlotPanel) of every stored simulation of OpenRocket's example designs,
// each of the 9 presets chosen in its preset box, against OpenRocket's own tab: the core's plot
// goldens (tests/data/goldens/plots, read through tests/core/plot/SimulationPlotGoldens.h) hold,
// for each plot, the configuration OpenRocket's SimulationPlotPanel takes when the preset is
// chosen (the X type and unit, each row's type, unit and axis: a copy in the default units) and
// the colour its row shows (panelColors: computeDefaultPlotColors(), which tools/openrocket-plots'
// probe reads from the panel). Here the preset box is driven as a user drives it, and the tab's
// configuration and rows are compared with them.

#include <array>
#include <cstddef>
#include <format>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QColor>
#include <QComboBox>
#include <QString>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/dialogs/plot/SimulationPlotPanel.h"
#include "QtRocket/gui/dialogs/plot/SimulationPlotTypeSelector.h"
#include "QtRocket/gui/plot/PlotColors.h"
#include "QtRocket/gui/widgets/DataTypeComboBox.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/plot/SimulationPlotConfiguration.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/unit/Unit.h"
#include "TestGuiServices.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenExamples.h"
#include "plot/SimulationPlotGoldens.h"

namespace QtRocket::Gui
{

namespace
{

using QtRocket::Test::GoldenPlot;
using QtRocket::Test::GoldenPlotRow;
using QtRocket::Test::GoldenPlotSimulation;

/// An example design with stored simulations.
struct PanelCase
{
    const char* file;
    /// The test's name (GoogleTest's, without spaces).
    const char* name;
};

// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
void PrintTo(const PanelCase& panelCase, std::ostream* out)
{
    *out << panelCase.name;
}

/// The examples whose simulations have stored flights (all 16 of them).
constexpr std::array kPanelCases{
    PanelCase{.file = "3D printable nose cone and fins.ork", .name = "printable_nose_cone"},
    PanelCase{.file = "A simple model rocket.ork", .name = "simple_rocket"},
    PanelCase{.file = "ARC payload rocket.ork", .name = "arc_payload"},
    PanelCase{.file = "Airstart timing.ork", .name = "airstart_timing"},
    PanelCase{.file = "Chute release.ork", .name = "chute_release"},
    PanelCase{.file = "Clustered motors.ork", .name = "clustered_motors"},
    PanelCase{.file = "Deployable payload.ork", .name = "deployable_payload"},
    PanelCase{.file = "Dual parachute deployment.ork", .name = "dual_parachute"},
    PanelCase{.file = "Parallel booster staging.ork", .name = "parallel_boosters"},
    PanelCase{.file = "Pods--airframes and winglets.ork", .name = "pods_winglets"},
    PanelCase{.file = "Pods--powered with recovery deployment.ork", .name = "pods_powered"},
    PanelCase{.file = "Simulation extensions.ork", .name = "simulation_extensions"},
    PanelCase{.file = "Simulation scripting.ork", .name = "simulation_scripting"},
    PanelCase{.file = "Three stage low power rocket.ork", .name = "three_stage"},
    PanelCase{.file = "Tube fin rocket.ork", .name = "tube_fin"},
    PanelCase{.file = "Two stage high power rocket.ork", .name = "two_stage"},
};

/// The mismatches of the tab of one simulation, each a line prefixed with the plot's preset.
class PanelMismatches
{
public:
    explicit PanelMismatches(std::string simulation) : m_simulation(std::move(simulation)) { }

    void text(std::size_t preset, std::string_view what, std::string_view actual,
              std::string_view golden)
    {
        if (actual != golden)
        {
            m_lines.push_back(
                std::format(R"({} preset {}: {}: "{}" differs from OpenRocket's "{}")",
                            m_simulation, preset, what, actual, golden));
        }
    }

    void number(std::size_t preset, std::string_view what, long long actual, long long golden)
    {
        if (actual != golden)
        {
            m_lines.push_back(std::format("{} preset {}: {}: {} differs from OpenRocket's {}",
                                          m_simulation, preset, what, actual, golden));
        }
    }

    [[nodiscard]] const std::vector<std::string>& lines() const noexcept { return m_lines; }

private:
    std::string              m_simulation;
    std::vector<std::string> m_lines;
};

/// Compares the row @p index of @p panel (the configuration's and the selector's) with @p golden.
void compareRow(PanelMismatches& mismatches, std::size_t preset, const SimulationPlotPanel& panel,
                std::size_t index, const GoldenPlotRow& golden, int goldenColor)
{
    const SimulationPlotConfiguration& configuration = panel.getConfiguration();
    const std::string                  row           = std::format("row {}", index);
    mismatches.text(preset, row + " type", configuration.getType(index).getName(), golden.type);
    mismatches.text(preset, row + " unit", configuration.getUnit(index).getUnit(), golden.unit);
    mismatches.number(preset, row + " axis", configuration.getAxis(index), golden.axis);
    const SimulationPlotTypeSelector& selector = panel.plotTypeSelector(index);
    mismatches.text(preset, row + " shown type", selector.selectedType()->getName(), golden.type);
    mismatches.text(preset, row + " shown unit", selector.selectedUnit().getUnit(), golden.unit);
    mismatches.number(preset, row + " shown axis", selector.selectedAxis(),
                      golden.axis < 0 ? -1 : golden.axis);
    mismatches.text(preset, row + " colour", selector.selectedColor().name().toStdString(),
                    PlotColors::plotColor(goldenColor).name().toStdString());
}

/// Compares @p panel, its preset @p plot.preset chosen, with @p plot.
void comparePlot(PanelMismatches& mismatches, const SimulationPlotPanel& panel,
                 const GoldenPlot& plot)
{
    const std::size_t                  preset        = plot.preset;
    const SimulationPlotConfiguration& configuration = panel.getConfiguration();
    mismatches.number(preset, "preset box", panel.configurationSelector().currentIndex(),
                      static_cast<long long>(preset));
    mismatches.text(preset, "name", configuration.getName(), plot.name);
    mismatches.text(preset, "X type", configuration.getDomainAxisType().getName(), plot.domain[0]);
    mismatches.text(preset, "X unit", configuration.getDomainAxisUnit().getUnit(), plot.domain[1]);
    mismatches.text(preset, "X shown", panel.domainTypeSelector().selectedType()->getName(),
                    plot.domain[0]);
    mismatches.text(preset, "X unit shown", panel.domainUnitSelector().getSelectedUnit().getUnit(),
                    plot.domain[1]);
    mismatches.number(preset, "rows", static_cast<long long>(configuration.getDataCount()),
                      static_cast<long long>(plot.rows.size()));
    mismatches.number(preset, "rows shown", static_cast<long long>(panel.selectorCount()),
                      static_cast<long long>(plot.rows.size()));
    mismatches.number(preset, "colours", static_cast<long long>(plot.panelColors.size()),
                      static_cast<long long>(plot.rows.size()));
    for (std::size_t i = 0; i < plot.rows.size() && i < configuration.getDataCount() &&
                            i < panel.selectorCount() && i < plot.panelColors.size();
         ++i)
    {
        compareRow(mismatches, preset, panel, i, plot.rows[i], plot.panelColors[i]);
    }
}

/// The mismatches of @p simulation's tab, every preset chosen in turn, with @p golden.
[[nodiscard]] std::vector<std::string> panelMismatches(
    const std::shared_ptr<Simulation>& simulation, const GoldenPlotSimulation& golden)
{
    const Gui::Test::TestGuiServices services;
    const SimulationPlotPanel        panel(services.services(), simulation);
    PanelMismatches                  mismatches(golden.name);
    for (const GoldenPlot& plot : golden.plots)
    {
        // The tab opens at the first preset (no configuration plotted yet).
        panel.configurationSelector().setCurrentIndex(static_cast<int>(plot.preset));
        comparePlot(mismatches, panel, plot);
    }
    return mismatches.lines();
}

[[nodiscard]] const QtRocket::Test::GoldenPlotTable* panelTableOrFail()
{
    const auto& table = QtRocket::Test::goldenPlotTable();
    if (!table.has_value())
    {
        ADD_FAILURE() << table.error().toString();
        return nullptr;
    }
    return &*table;
}

class SimulationPlotPanelGolden : public Gui::Test::GuiTest,
                                  public ::testing::WithParamInterface<PanelCase>
{ };

}  // namespace

TEST_P(SimulationPlotPanelGolden, EveryPresetShowsOpenRocketsRowsAndColours)
{
    const QtRocket::Test::GoldenPlotTable* table = panelTableOrFail();
    ASSERT_NE(table, nullptr);
    const QtRocket::Test::GoldenExample* const example =
        QtRocket::Test::findGoldenExample(GetParam().file);
    ASSERT_NE(example, nullptr);
    auto loaded = QtRocket::Test::loadGoldenExample(*example);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    const auto& simulations = loaded->document->getSimulations();
    std::size_t compared    = 0;
    for (const GoldenPlotSimulation& golden : table->simulations)
    {
        if (golden.file != GetParam().file)
        {
            continue;
        }
        ASSERT_LT(golden.simulation, simulations.size());
        EXPECT_EQ(golden.plots.size(), 9U);
        EXPECT_EQ(panelMismatches(simulations.at(golden.simulation), golden),
                  QtRocket::Test::noGoldenMismatches());
        ++compared;
    }
    EXPECT_GT(compared, 0U) << "the goldens hold no simulation of " << GetParam().file;
}

INSTANTIATE_TEST_SUITE_P(StoredSimulations, SimulationPlotPanelGolden,
                         ::testing::ValuesIn(kPanelCases),
                         [](const ::testing::TestParamInfo<PanelCase>& panelCase) {
                             return std::string(panelCase.param.name);
                         });

}  // namespace QtRocket::Gui
