#include "QtRocket/gui/simulation/SimulationTableCsvExport.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "QtRocket/gui/simulation/SimulationColumns.h"
#include "QtRocket/gui/simulation/SimulationTableModel.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/unit/Value.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket::Gui
{

namespace
{

using Column = SimulationColumns::Column;

static_assert(SimulationTableCsvExport::kColumns.size() + 1 ==
                  SimulationColumns::kAllColumns.size(),
              "the text has every column but the status column");

/// The row of @p simulation in @p model; a BugError when it is none.
[[nodiscard]] int rowIn(const SimulationTableModel& model, const Simulation& simulation)
{
    const int row = model.rowOf(simulation);
    if (row < 0)
    {
        bug("The simulation " + simulation.getName() + " is no row of the simulations table");
    }
    return row;
}

/// @p simulation, a BugError when it is null.
[[nodiscard]] const Simulation& simulationOf(const std::shared_ptr<Simulation>& simulation)
{
    if (simulation == nullptr)
    {
        bug("A simulation of the simulations table's text is null");
    }
    return *simulation;
}

/// The field of @p column in @p simulation's line, the row @p row of @p model (Java:
/// formatCellValue() of getValueAt(), escaped); nullopt for a value Java's model gives as null.
[[nodiscard]] std::optional<std::string> field(const SimulationTableModel& model, int row,
                                               const Simulation& simulation, Column column,
                                               int decimalPlaces, bool exponentialNotation)
{
    if (SimulationColumns::isValueColumn(column))
    {
        // ValueColumn.getValueAt(): a Value in the default unit of now, written as
        // TextUtil.doubleToString() of getUnitValue().
        const std::optional<double> value = SimulationColumns::summaryValue(column, simulation);
        if (!value.has_value())
        {
            return std::nullopt;
        }
        const Value inUnit(*value, SimulationColumns::units(column));
        return Strings::escapeCsv(
            Strings::doubleToString(inUnit.getUnitValue(), decimalPlaces, exponentialNotation));
    }
    // The name, the configuration and the stepper: their toString(), the table's text.
    const auto* text = std::get_if<std::string>(&model.cellValue(row, column));
    if (text == nullptr)
    {
        return std::nullopt;
    }
    return Strings::escapeCsv(*text);
}

}  // namespace

std::string SimulationTableCsvExport::tableText(
    const SimulationTableModel& model, const std::vector<std::shared_ptr<Simulation>>& simulations,
    std::string_view separator, int decimalPlaces, bool exponentialNotation)
{
    std::string text = headerLine(separator);
    for (const std::shared_ptr<Simulation>& simulation : simulations)
    {
        const std::optional<std::string> line =
            rowLine(model, simulationOf(simulation), separator, decimalPlaces, exponentialNotation);
        if (line.has_value())
        {
            text += '\n';
            text += *line;
        }
    }
    return text;
}

std::string SimulationTableCsvExport::copyText(
    const SimulationTableModel& model, const std::vector<std::shared_ptr<Simulation>>& simulations)
{
    //: simpanel.col.Status
    std::string text = QtStrings::toUtf8(tr("Status"));
    text += kCopySeparator;
    text += headerLine(kCopySeparator);
    for (const std::shared_ptr<Simulation>& simulation : simulations)
    {
        const Simulation&                sim = simulationOf(simulation);
        const std::optional<std::string> line =
            rowLine(model, sim, kCopySeparator, kCopyDecimalPlaces, kCopyExponentialNotation);
        if (!line.has_value())
        {
            continue;
        }
        // OpenRocket bug 25, fixed: the status of this line's own simulation (see the header).
        const Simulation::Status status = model.status(rowIn(model, sim));
        text += '\n';
        text += Strings::removeHtmlTags(
            QtStrings::toUtf8(SimulationColumns::statusDescription(status, sim)));
        text += kCopySeparator;
        text += *line;
    }
    return text;
}

std::string SimulationTableCsvExport::headerLine(std::string_view separator)
{
    std::string line;
    for (const Column column : kColumns)
    {
        if (column != kColumns.front())
        {
            line += separator;
        }
        line += QtStrings::toUtf8(SimulationColumns::headerText(column));
        if (SimulationColumns::isValueColumn(column))
        {
            // populateColumnNameToUnitsHashTable(): getUnits().getDefaultUnit().getUnit().
            line += " (";
            line += SimulationColumns::units(column).getDefaultUnit().getUnit();
            line += ')';
        }
    }
    return line;
}

std::optional<std::string> SimulationTableCsvExport::rowLine(const SimulationTableModel& model,
                                                             const Simulation&           simulation,
                                                             std::string_view            separator,
                                                             int  decimalPlaces,
                                                             bool exponentialNotation)
{
    const int row = rowIn(model, simulation);
    // generateCSVData(): a simulation without summary data has no line.
    if (!simulation.hasSummaryData())
    {
        return std::nullopt;
    }
    std::string line = warningsText(simulation, separator);
    for (const Column column : kColumns)
    {
        if (column == Column::WARNINGS)
        {
            continue;
        }
        const std::optional<std::string> text =
            field(model, row, simulation, column, decimalPlaces, exponentialNotation);
        // buildRowData(): a row with a missing value has no line.
        if (!text.has_value())
        {
            return std::nullopt;
        }
        line += separator;
        line += *text;
    }
    return line;
}

std::string SimulationTableCsvExport::warningsText(const Simulation& simulation,
                                                   std::string_view  separator)
{
    const WarningSet* warnings = simulation.getSimulatedWarnings();
    if (warnings == nullptr || warnings->empty())
    {
        return {};
    }
    const std::string_view delimiter = separator == "|" ? " ; " : " | ";
    std::string            text;
    bool                   first = true;
    for (const Warning& warning : *warnings)
    {
        if (!first)
        {
            text += delimiter;
        }
        first = false;
        text += warning.toString();
    }
    return Strings::escapeCsv(text);
}

}  // namespace QtRocket::Gui
