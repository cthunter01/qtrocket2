#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <QCoreApplication>

#include "QtRocket/gui/simulation/SimulationColumns.h"

namespace QtRocket
{
class Simulation;
}

namespace QtRocket::Gui
{

class SimulationTableModel;

/// The text of OpenRocket's simulations table (swing file/SimulationTableCSVExport.java's
/// generateCSVData(), and SimulationPanel's copySimulationValues() with its addStatusColumn()):
/// what the table's CSV export writes into its file (the File menu's "Export simulation table as
/// CSV file" and the popup's "Export simulation(s) as CSV file", Milestone 4's run 3) and what
/// Copy puts on the system clipboard beside the simulations (SimulationEditing). Not the file
/// export itself: the file, the options and the messages are the caller's.
///
/// The table text (tableText(), Java's generateCSVData()): the header line, then a line for each
/// simulation given that has summary data, in the order given (the table's: all its rows, or the
/// selected ones), joined by '\n', with nothing after the last line.
/// - The header (headerLine(), buildHeaderRow()): the header text of every column from the
///   warnings column on (kColumns: OpenRocket's model columns 1 to 13), shown or hidden, since the
///   column chooser hides a column of the view only; a value column's with " (" + the symbol of
///   its group's default unit + ")" ("Apogee (m)"); joined by the separator and not escaped.
/// - A line (rowLine(), buildRowData()): the simulation's warnings (warningsText()), then the
///   texts of the name, configuration and stepper columns (the table's) and the nine values in the
///   default units of their groups as Strings::doubleToString() writes them with the decimal places
///   and the notation given ("38.733"), each field escaped (Strings::escapeCsv()), joined by the
///   separator. A simulation without summary data has no line, nor would one with a value missing
///   (OpenRocket's null), which cannot be: every value is there with the data.
/// - The warnings (warningsText(), formatWarningsText()): Warning::toString() of each warning of
/// the
///   simulated data, in the set's order, joined by " | " (" ; " when the separator is "|"), the
///   whole escaped; nothing without warnings.
/// The units are the default ones of the moment the text is made, those of the header and the
/// values alike (OpenRocket reads both then), whatever units the table showed at its last reading.
/// The name, configuration and stepper texts and the statuses are those of the table
/// (SimulationTableModel's last reading, at the last document event: they change only with one),
/// so the simulations given must be rows of the table.
///
/// The Copy (copyText(), copySimulationValues("\t") with addStatusColumn()): the table text of the
/// simulations given with a tab between the fields, kCopyDecimalPlaces decimal places and no
/// exponential notation, with a status column before the others: the header line starts with
/// "Status" (simpanel.col.Status) and a tab, and every simulation's line with the description of
/// its status (SimulationColumns::statusDescription() of the table's status, its HTML tags removed:
/// Strings::removeHtmlTags(); "Loaded from file", "Out of date") and a tab.
///
/// Deviation (OpenRocket bug 25, fixed): OpenRocket's addStatusColumn() pairs the statuses of the
/// selected rows with the lines of the exporter's text by position, but the text has no line for
/// a row without summary data, so that every status after such a row lands on the next row's line
/// (the line of "Simulation 3 - too short delay" shows the status of the row without data before
/// it), and the last statuses are left out. Here every line carries its own simulation's status.
/// (Java's addStatusColumn() also splits the text at every '\n', so a warning text with a line
/// break, which escapeCsv() quotes, would make two lines of one; here it stays in its line.)
class SimulationTableCsvExport final
{
    Q_DECLARE_TR_FUNCTIONS(SimulationTableCsvExport)

public:
    using Column = SimulationColumns::Column;

    /// The columns of the text, in their order: every column of the table but the status column
    /// (OpenRocket's model columns 1 to 13), shown or hidden.
    static constexpr std::array kColumns{
        Column::WARNINGS,
        Column::NAME,
        Column::CONFIGURATION,
        Column::SIMULATION_STEPPER,
        Column::LAUNCH_ROD_VELOCITY,
        Column::APOGEE,
        Column::DEPLOYMENT_VELOCITY,
        Column::OPTIMUM_COAST_TIME,
        Column::MAX_VELOCITY,
        Column::MAX_ACCELERATION,
        Column::TIME_TO_APOGEE,
        Column::FLIGHT_TIME,
        Column::GROUND_HIT_VELOCITY,
    };

    /// The Copy's separator (copySimulationAction(): a tab, which spreadsheets take for a column).
    static constexpr std::string_view kCopySeparator = "\t";
    /// The Copy's decimal places (copySimulationValues()).
    static constexpr int kCopyDecimalPlaces = 6;
    /// The Copy's notation (copySimulationValues(): no exponential notation).
    static constexpr bool kCopyExponentialNotation = false;

    SimulationTableCsvExport() = delete;

    /// The table text of @p simulations, rows of @p model in the order the text gives them, with
    /// @p separator between the fields and the values written with @p decimalPlaces decimal places,
    /// in exponential notation when @p exponentialNotation (Java's generateCSVData(); see the class
    /// comment). UTF-8.
    /// @throws BugError when a simulation is null or no row of @p model
    [[nodiscard]] static std::string tableText(
        const SimulationTableModel&                     model,
        const std::vector<std::shared_ptr<Simulation>>& simulations, std::string_view separator,
        int decimalPlaces, bool exponentialNotation);

    /// The Copy's text of @p simulations, rows of @p model in the order the text gives them (see
    /// the class comment). UTF-8.
    /// @throws BugError when a simulation is null or no row of @p model
    [[nodiscard]] static std::string copyText(
        const SimulationTableModel&                     model,
        const std::vector<std::shared_ptr<Simulation>>& simulations);

    /// The header line (buildHeaderRow()) with @p separator between the columns, in the default
    /// units of now. UTF-8.
    [[nodiscard]] static std::string headerLine(std::string_view separator);

    /// The line of @p simulation, a row of @p model (buildRowData()): nullopt when it has no
    /// summary data. UTF-8.
    /// @throws BugError when @p simulation is no row of @p model
    [[nodiscard]] static std::optional<std::string> rowLine(const SimulationTableModel& model,
                                                            const Simulation&           simulation,
                                                            std::string_view            separator,
                                                            int  decimalPlaces,
                                                            bool exponentialNotation);

    /// The warnings field of @p simulation's line (formatWarningsText()), escaped, with the
    /// delimiter that goes with @p separator: empty without data or warnings. UTF-8.
    [[nodiscard]] static std::string warningsText(const Simulation& simulation,
                                                  std::string_view  separator);
};

}  // namespace QtRocket::Gui
