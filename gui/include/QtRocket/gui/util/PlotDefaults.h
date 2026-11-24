#pragma once

#include <memory>

namespace QtRocket
{
class SimulationPlotConfiguration;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// The plot configuration that a simulation's "Plot data" tab starts from: the one last plotted,
/// in any window, of any simulation (OpenRocket's SimulationPlotPanel.DEFAULT_CONFIGURATION, a
/// process-wide static that doPlot() replaces through setDefaultConfiguration(), with the
/// simulation's own plot appearances stripped first: clearPlotAppearances()). One object serves
/// the application (app/ApplicationContext owns it, GuiServices::plotDefaults() hands it out); a
/// test has its own (TestGuiServices). Called on the GUI thread only.
///
/// Before the first plot there is none, and the tab starts from the first preset in the default
/// units of the moment (Java's initial value, DEFAULT_CONFIGURATIONS[0].resetUnits():
/// SimulationPlotConfiguration::defaultConfigurations().front()).
class PlotDefaults
{
public:
    PlotDefaults()                               = default;
    virtual ~PlotDefaults()                      = default;
    PlotDefaults(const PlotDefaults&)            = delete;
    PlotDefaults& operator=(const PlotDefaults&) = delete;
    PlotDefaults(PlotDefaults&&)                 = delete;
    PlotDefaults& operator=(PlotDefaults&&)      = delete;

    /// The configuration last plotted (setLastPlotted()), or null before the first plot. Shared,
    /// never changed: the tab works on a copy of it.
    [[nodiscard]] virtual std::shared_ptr<const SimulationPlotConfiguration> lastPlotted()
        const = 0;

    /// Keeps @p configuration as the one last plotted, from now on (null forgets it): the plot
    /// tab's doPlot() gives a copy of its configuration without the simulation's plot
    /// appearances, so that they do not reach other simulations.
    virtual void setLastPlotted(
        std::shared_ptr<const SimulationPlotConfiguration> configuration) = 0;
};

}  // namespace QtRocket::Gui
