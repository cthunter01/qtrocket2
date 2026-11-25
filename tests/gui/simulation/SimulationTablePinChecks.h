#pragma once

// The comparison of a simulations table with the simulation-table pins (SimulationTablePins.h):
// each row's status, warnings and cells as OpenRocket's own table shows them, the states of the
// tab's actions and of the window's menu items that follow them, and the rows' order. Shared by
// the model's golden suites (SimulationTableModelTests.cpp), the tab's
// (SimulationPanelTests.cpp, SimulationTableViewTests.cpp), the table of a design window's
// "Flight simulations" tab (main/simulations_window_tests.cpp) and the exit suite
// OpenDesignsExitGolden (exit/open_designs_exit_tests.cpp).

#include <array>
#include <map>
#include <string>
#include <vector>

#include "QtRocket/gui/simulation/SimulationTableModel.h"
#include "simulation/SimulationTablePins.h"

class QAction;

namespace QtRocket::Gui
{
class MainMenus;
class SimulationPanel;
}  // namespace QtRocket::Gui

namespace QtRocket::Gui::Test
{

/// The row @p row of @p model shows the pin's row @p pin (gtest expectations, traced with both
/// rows): its status (the constant's name, the icon's description, the sort text, the
/// description and the tooltip of the status and the other cells) and, unless the pin's row was
/// run fresh, its warnings (the counts, OpenRocket's box of labels, the tooltip and each warning's
/// priority and text) and the texts of the model columns 2 to 13 (none for Java's null).
void expectSimulationTablePinRow(const SimulationTableModel& model, int row,
                                 const SimulationTablePinRow& pin);

/// Every row of @p model, which must be in the document's order (no sort keys), shows its pin of
/// @p pins (expectSimulationTablePinRow()), as many rows as pins.
void expectSimulationTablePinRows(const SimulationTableModel&               model,
                                  const std::vector<SimulationTablePinRow>& pins);

/// The rows of @p model, from the top, as the indices of their simulations in its document (-1
/// for one that is not in it): the order the pins' "orders" hold.
[[nodiscard]] std::vector<int> simulationTableDocumentOrder(const SimulationTableModel& model);

/// The actions of @p panel whose states the pins hold, in the order of OpenRocket's probe
/// (SimulationTableProbe.ACTIONS), without its Monte Carlo action ("landingDispersion", Milestone
/// 7).
[[nodiscard]] std::array<QAction*, 11> simulationPanelPinActions(const SimulationPanel& panel);

/// The probe's name of the tab's action @p action: its objectName without "Action"
/// ("newSimulation").
[[nodiscard]] std::string simulationPanelActionName(const QAction& action);

/// The states of @p panel's actions as the pins write them, in the probe's order:
/// "newSimulation=1 editSimulation=0 ...".
[[nodiscard]] std::string simulationPanelActionStates(const SimulationPanel& panel);

/// The states @p pinned (a selection's of the pins' "actions") of @p panel's actions in the same
/// form, '?' for an action the pins lack: equal to simulationPanelActionStates() when the tab's
/// actions are OpenRocket's.
[[nodiscard]] std::string pinnedSimulationPanelActionStates(
    const SimulationPanel& panel, const std::map<std::string, bool>& pinned);

/// The items of the window's menus @p menus whose states differ from @p pinned, the states of
/// OpenRocket's tab's actions, by the names of those actions: the File menu's "Export simulation
/// table as CSV file" (the tab's "simTableExport") always, and the Edit menu's Edit, Cut, Copy,
/// Paste, Duplicate and Delete (RocketActions: the component rule or the tab's action of the
/// same name) when @p editItems, which a caller asks for while no component is selected.
[[nodiscard]] std::vector<std::string> simulationMenuStateDifferences(
    const MainMenus& menus, const std::map<std::string, bool>& pinned, bool editItems);

}  // namespace QtRocket::Gui::Test
