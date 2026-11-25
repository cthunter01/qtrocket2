#pragma once

// What the tests of the plot dialog (dialogs/plot/PlotDialog, SimulationPlotDialog) share: the
// plot of a stored simulation of an example design under one of OpenRocket's preset plot
// configurations, as the plot tab's preset box makes it; a copy of a flight with aborts added, as
// tools/openrocket-gui-pins' plot-dialog probe (PlotDialogProbe) adds them; and a readout hook
// that keeps the plot's tool tips out of the test (DataPlot::setReadoutHook(): QToolTip leaves a
// window behind, which GuiTest refuses).

#include <cstddef>
#include <memory>
#include <span>
#include <string>

#include "QtRocket/gui/plot/DataPlot.h"
#include "QtRocket/plot/SimulationPlotConfiguration.h"
#include "QtRocket/plot/SimulationPlotModel.h"

namespace QtRocket
{
class FlightData;
class Simulation;
}  // namespace QtRocket

namespace QtRocket::Gui::Test
{

/// OpenRocket's preset plot configuration @p preset (SimulationPlotConfiguration::
/// defaultConfigurations(), 0 "Vertical motion vs. time" to 8) as the plot tab's preset box
/// chooses it: a copy in the default units (clone().resetUnits()).
[[nodiscard]] SimulationPlotConfiguration presetConfiguration(std::size_t preset);

/// The plot model of @p simulation's flight data under presetConfiguration(@p preset), its texts
/// translated as the GUI's (SimulationPlot::translator()).
[[nodiscard]] SimulationPlotModel presetPlotModel(const Simulation& simulation, std::size_t preset);

/// The plot model of @p data, named @p name, under presetConfiguration(@p preset).
[[nodiscard]] SimulationPlotModel presetPlotModelOf(std::string                       name,
                                                    std::shared_ptr<const FlightData> data,
                                                    std::size_t                       preset);

/// A copy of @p data (its branches cloned, FlightDataBranch::clone()) in which each branch of
/// @p aborted has a SIM_ABORT at 60 % of its last time, of the cause SimulationAbort::kAllCauses[
/// its index % 10] (as the probe adds them).
[[nodiscard]] std::shared_ptr<const FlightData> withAborts(const FlightData&            data,
                                                           std::span<const std::size_t> aborted);

/// A readout hook (DataPlot::ReadoutHook) that shows nothing.
[[nodiscard]] DataPlot::ReadoutHook silentReadout();

}  // namespace QtRocket::Gui::Test
