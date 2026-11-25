#pragma once

// What the tests of the plot tab (dialogs/plot/PlotPanel, PlotTypeSelector, SimulationPlotPanel,
// SimulationPlotTypeSelector, FlightEventTableModel) share: a simulation over a small flight of
// its own, SimulationChartTest's flight, and the panel's state as text in the format of
// tools/openrocket-gui-pins' PlotTabProbe (its hand-pinned.txt, after PlotDialogProbe's lines), so
// that a test compares a line of OpenRocket's transcript with QtRocket's as it is.

#include <memory>
#include <string>

namespace QtRocket
{
class FlightData;
class OpenRocketDocument;
class Simulation;
class SimulationPlotConfiguration;
}  // namespace QtRocket

namespace QtRocket::Gui
{
class FlightEventTableModel;
class SimulationPlotPanel;
class SimulationPlotTypeSelector;
}  // namespace QtRocket::Gui

namespace QtRocket::Gui::Test
{

/// A simulation of a new design (OpenRocketDocumentFactory::createNewRocket()) whose stored flight
/// is @p data, up to date, named @p name, added to the design's simulations (Java: new
/// Simulation(document, rocket, UPTODATE, name, options, extensions, data)).
class TestFlight
{
public:
    explicit TestFlight(std::shared_ptr<FlightData> data, const std::string& name = "Test");
    ~TestFlight();
    TestFlight(const TestFlight&)            = delete;
    TestFlight& operator=(const TestFlight&) = delete;
    TestFlight(TestFlight&&)                 = delete;
    TestFlight& operator=(TestFlight&&)      = delete;

    [[nodiscard]] OpenRocketDocument& document() const noexcept { return *m_document; }
    [[nodiscard]] const std::shared_ptr<Simulation>& simulation() const noexcept
    {
        return m_simulation;
    }

private:
    std::unique_ptr<OpenRocketDocument> m_document;
    std::shared_ptr<Simulation>         m_simulation;
};

/// SimulationChartTest.createPositionFlightDataBranch(): a stage "Sustainer" of 3 points, time
/// i, altitude 100 i, vertical velocity 20 i, vertical acceleration 1000 i, the positions east and
/// north 10 i.
[[nodiscard]] std::shared_ptr<FlightData> positionFlight();

/// PlotTabProbe's config(): "name='<name>' X=<domain type> [<unit>] rows=<n>: <type> [<unit>]
/// @<axis> color=<#rrggbb or none> <line style>; ... events=[<event names>]", the names in
/// English (util/EnumText's texts without a translator).
[[nodiscard]] std::string configurationText(const SimulationPlotConfiguration& configuration);

/// PlotTabProbe's rowsOf() of one row: "<index> <type> [<unit>] <axis box's text> <colour shown>
/// <line style> setDefault=on|off reset=on|off".
[[nodiscard]] std::string rowText(const SimulationPlotTypeSelector& row);

/// Every row of @p panel (rowText()), joined by " | ".
[[nodiscard]] std::string rowsText(const SimulationPlotPanel& panel);

/// PlotTabProbe's eventsOf(): "<rows>: <name>=<true|false>; ...".
[[nodiscard]] std::string eventsText(const FlightEventTableModel& model);

/// The first line of PlotTabProbe's state(): "preset box=<index> '<its text>' X box='<domain
/// type>' X unit=<unit>".
[[nodiscard]] std::string stateText(const SimulationPlotPanel& panel);

/// PlotTabProbe's extras(): "description visible=<shown> marker style enabled=<b> tooltip='...'
/// Vertical line enabled=<b> selected=<b> tooltip=none|'...' Icon enabled=<b> selected=<b>
/// tooltip=none|'...'" (shown: not hidden, as Java's isVisible()).
[[nodiscard]] std::string extrasText(const SimulationPlotPanel& panel);

}  // namespace QtRocket::Gui::Test
