#pragma once

// The comparison of a simulations table with the simulation-table pins (SimulationTablePins.h):
// each row's status, warnings and cells as OpenRocket's own table shows them. Shared by the
// model's golden suites (SimulationTableModelTests.cpp) and the table of a design window's
// "Flight simulations" tab (main/simulations_window_tests.cpp).

#include <vector>

#include "QtRocket/gui/simulation/SimulationTableModel.h"
#include "simulation/SimulationTablePins.h"

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

}  // namespace QtRocket::Gui::Test
