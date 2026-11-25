#pragma once

#include <array>
#include <compare>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include <QCoreApplication>
#include <QString>

#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/unit/Value.h"

class QPalette;

namespace QtRocket
{
class Preferences;
class UnitGroup;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// The fourteen columns of OpenRocket's simulations table (the "Flight simulations" tab: the
/// columns SimulationPanel.java:1487-1760 gives its SimulationTableModel, with the column
/// adaptors of swing gui/adaptors/: Column, ValueColumn, ColumnTableModel, ColumnTable and
/// ColumnTableRowSorter), what each shows of a simulation and how each sorts. Qt-free but for the
/// texts; SimulationTableModel shows them, SimulationTableView (Milestone 4, run 2's T2) lays them
/// out, and SimulationTableCsvExport (T3) writes them.
///
/// The columns, in Java's order (their ids are SimulationPanel's COLUMN_ID_*, which the column
/// chooser stores in the document preference simulation.table.hiddenColumns):
/// - STATUS ("status"): no header text ("Status" in the column chooser, simpanel.col.Status) and
///   an empty header tooltip; the status icon (util/SimulationIcons); sorted by the text of its
///   label (statusSortText()) with the Java collator;
/// - WARNINGS ("warnings", "Warnings"): the warning counts by priority, drawn by
///   SimulationWarningsDelegate; sorted by the critical, then the normal, then the informational
///   count (bug 23, below);
/// - NAME ("name", "Name"): the simulation's name, sorted by AlphanumComparator;
/// - CONFIGURATION ("configuration", "Configuration"): the name of the simulation's flight
///   configuration as the rocket gives it (FlightConfiguration::getName() with the preferences'
///   motor name column: Java's RocketDescriptor.format()), sorted with the Java collator;
/// - SIMULATION_STEPPER ("simulationStepper", "Sim. Stepper", tooltip "Simulation Stepper
///   Method"): the short name of the options' stepper ("RK4", "RK6"), sorted with the Java
///   collator; hidden by default;
/// - nine value columns (Java's ValueColumn): LAUNCH_ROD_VELOCITY ("launchRodVelocity", "Velocity
///   off rod", velocity), APOGEE ("apogee", "Apogee", distance), DEPLOYMENT_VELOCITY
///   ("deploymentVelocity", "Velocity at deployment", velocity), OPTIMUM_COAST_TIME
///   ("optimumCoastTime", "Optimum delay", short time, with its own tooltip), MAX_VELOCITY
///   ("maxVelocity", "Max. velocity", velocity), MAX_ACCELERATION ("maxAcceleration", "Max.
///   acceleration", acceleration), TIME_TO_APOGEE ("timeToApogee", "Time to apogee", long
///   time), FLIGHT_TIME ("flightTime", "Flight time", long time) and GROUND_HIT_VELOCITY
///   ("groundHitVelocity", "Ground hit velocity", velocity): the summary value of the
///   simulation's flight data as a Value in the default unit of the column's unit group, its
///   text Value::toString() ("38.7 m", "N/A" for NaN), nothing without data; sorted by
///   Value::compareTo() (ValueComparator), a missing value first.
///
/// The order (ColumnTableRowSorter over JDK's TableRowSorter): a column with a comparator of its
/// own (NAME's AlphanumComparator, the value columns' ValueComparator) sorts its values with it;
/// any other sorts the toString() of its values with Collator.getInstance(), the tertiary Java
/// collator of en_US (JavaCollator). A missing value (Java's null) comes first ascending
/// (TableSortKeys::compareMissingFirst()). compare() is a column's comparison, ascending;
/// TableSortKeys applies the keys and the model's order of equal rows.
///
/// Texts: the headers and tooltips are OpenRocket's messages (tr() with the key); the cells'
/// texts are the core's, through the default units of the moment the cell is made, and the
/// statuses' descriptions and the abort causes are translated (EnumText) where OpenRocket
/// translates them. The tooltips are OpenRocket's HTML, their colours ThemeColors' for the
/// palette given (SimulationPanel's getSimulationStatusToolTip() and
/// getSimulationWarningsToolTip()); the warnings keep the core's English Warning::toString(), as
/// OpenRocket shows its untranslated messages.
///
/// Deviation (OpenRocket bug 23, fixed): OpenRocket's warnings column does not sort, since its
/// sorter compares the toString() of one Box shared by every row (the same text for all of them),
/// so a click on its header keeps the model's order. Here it sorts by the warning counts:
/// compare() orders two counts by the critical, then the normal, then the informational count,
/// and a simulation without data has no counts (a missing value, first ascending).
class SimulationColumns final
{
    Q_DECLARE_TR_FUNCTIONS(SimulationColumns)

public:
    /// The columns, in OpenRocket's order (the model's column indices).
    enum class Column
    {
        STATUS,
        WARNINGS,
        NAME,
        CONFIGURATION,
        SIMULATION_STEPPER,
        LAUNCH_ROD_VELOCITY,
        APOGEE,
        DEPLOYMENT_VELOCITY,
        OPTIMUM_COAST_TIME,
        MAX_VELOCITY,
        MAX_ACCELERATION,
        TIME_TO_APOGEE,
        FLIGHT_TIME,
        GROUND_HIT_VELOCITY,
    };

    /// Every column, in OpenRocket's order.
    static constexpr std::array kAllColumns{
        Column::STATUS,
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

    /// The number of columns.
    static constexpr int kColumnCount = static_cast<int>(kAllColumns.size());

    /// The exact width of the status column (its Column.getExactWidth()).
    static constexpr int kStatusColumnWidth = 26;
    /// What ColumnTable.setupAutoSizeColumns() adds to the widest cell of an auto-sized column.
    static constexpr int kAutoSizePadding = 10;

    /// The numbers of a simulation's warnings by priority: HIGH ("critical"), NORMAL and LOW
    /// ("informational"), as WarningSet counts them. Ordered by the critical, then the normal,
    /// then the informational count (the warnings column's comparison).
    struct WarningCounts
    {
        std::size_t critical      = 0;
        std::size_t normal        = 0;
        std::size_t informational = 0;

        [[nodiscard]] auto operator<=>(const WarningCounts&) const = default;

        /// Whether the simulation has no warning at all.
        [[nodiscard]] bool isEmpty() const noexcept
        {
            return critical == 0 && normal == 0 && informational == 0;
        }
    };

    /// A simulation's value in a column (Java: Column.getValueAt()), which the table shows and
    /// sorts by: nothing (Java's null: a value column without data, the warnings without data),
    /// a text (STATUS: its sort text, statusSortText(); NAME, CONFIGURATION, SIMULATION_STEPPER:
    /// the cell's text, UTF-8), a Value (the value columns), or the warning counts (WARNINGS).
    using CellValue = std::variant<std::monostate, std::string, Value, WarningCounts>;

    SimulationColumns() = delete;

    // ------------------------------------------------------------------------------ the columns

    /// The column of the model column @p column.
    /// @throws BugError for a column outside the table
    [[nodiscard]] static Column columnAt(int column);

    /// The model column of @p column.
    [[nodiscard]] static int indexOf(Column column) noexcept;

    /// The column's id, as OpenRocket's column chooser stores it ("status", "launchRodVelocity").
    [[nodiscard]] static std::string_view id(Column column) noexcept;

    /// The column whose id() is @p id; nullopt for an unknown id (the chooser ignores those).
    [[nodiscard]] static std::optional<Column> fromId(std::string_view id) noexcept;

    /// The header's text (Column.toString()): empty for the status column.
    [[nodiscard]] static QString headerText(Column column);

    /// The column's name in the column chooser: "Status" (simpanel.col.Status) for the status
    /// column, headerText() for the others (ColumnVisibilityController's display names).
    [[nodiscard]] static QString chooserText(Column column);

    /// The header's tooltip (Column.getToolTip()): the header's text unless the column has a
    /// tooltip of its own (SIMULATION_STEPPER, OPTIMUM_COAST_TIME); empty for the status column.
    [[nodiscard]] static QString headerToolTip(Column column);

    /// Whether the column is hidden in a table whose design and application name no hidden
    /// columns (SimulationPanel's default: the stepper column).
    [[nodiscard]] static bool isHiddenByDefault(Column column) noexcept;

    /// Whether the column shows a Value (Java's ValueColumn).
    [[nodiscard]] static bool isValueColumn(Column column) noexcept;

    /// The column's unit group (Column.getUnits()): the value column's group, the group without
    /// units (UnitGroupId::NONE) for the other columns.
    [[nodiscard]] static const UnitGroup& units(Column column);

    /// Column.getDefaultWidth(): 40 for the warnings column (its minimum width: it is auto-sized),
    /// 125 for the name and configuration columns, 100 for the others.
    [[nodiscard]] static int defaultWidth(Column column) noexcept;

    /// Column.getExactWidth(): kStatusColumnWidth for the status column (its minimum, maximum and
    /// preferred width, not resizable), 0 (none) for the others.
    [[nodiscard]] static int exactWidth(Column column) noexcept;

    /// Column.isAutoSize(): the warnings column, whose preferred width is its widest cell plus
    /// kAutoSizePadding (ColumnTable.setupAutoSizeColumns()).
    [[nodiscard]] static bool isAutoSize(Column column) noexcept;

    /// Column.includeHeaderInAutoSize(): whether the header counts among the cells of an
    /// auto-sized column (false for the warnings column).
    [[nodiscard]] static bool includesHeaderInAutoSize(Column column) noexcept;

    // ------------------------------------------------------------------------------ the cells

    /// The summary value of the value column @p column of @p simulation's flight data (Java:
    /// ValueColumn.valueAt(), in SI units; NaN when the data has none); nullopt without data.
    /// @throws BugError for a column that is no value column
    [[nodiscard]] static std::optional<double> summaryValue(Column            column,
                                                            const Simulation& simulation);

    /// @p simulation's value in @p column (see CellValue), with @p status as its status (the
    /// one getStatus() gave for this document event: the table asks it once) and the
    /// configuration's name made with @p preferences' motor name column. A value is made in its
    /// group's default unit of this moment.
    [[nodiscard]] static CellValue cellValue(Column column, const Simulation& simulation,
                                             Simulation::Status status,
                                             const Preferences& preferences);

    /// The text a cell of @p column with @p value shows: the text, or Value::toString(); empty
    /// for nothing, for the status column (an icon) and the warnings column (its delegate draws
    /// it).
    [[nodiscard]] static QString cellText(Column column, const CellValue& value);

    /// The order of @p a and @p b in @p column, ascending (see the class comment): less when @p a
    /// comes first.
    /// @throws BugError when a value is not of the column's kind
    [[nodiscard]] static std::weak_ordering compare(Column column, const CellValue& a,
                                                    const CellValue& b);

    // ------------------------------------------------------------------------------ the status

    /// Status.getDescription(simulation) for @p status, translated: the description of the
    /// status (Simulation.Status.Description.<status>); for ABORTED, for each branch of the
    /// data whose first SIM_ABORT event has an abort, the description, "<i>: ", the abort's
    /// text (its cause, translated, with its sources) and "</i><br>".
    [[nodiscard]] static QString statusDescription(Simulation::Status status,
                                                   const Simulation&  simulation);

    /// What the status column sorts by (Java: StatusLabel.toString()): statusToolTip() without
    /// the name, each "<br>" replaced by "-" and the HTML tags removed: "Loaded from file-",
    /// "No simulation data available." for a simulation without data.
    [[nodiscard]] static QString statusSortText(Simulation::Status status,
                                                const Simulation&  simulation);

    /// The tooltip of every cell but the warnings' (SimulationPanel.getSimulationStatusToolTip()):
    /// "<html>", with @p includeName "<b>name</b><br>", then "No simulation data available."
    /// (simpanel.ttip.noData) without data, else statusDescription() in the status's colour
    /// (util/SimulationStatusColors for @p palette) and "<br>".
    [[nodiscard]] static QString statusToolTip(Simulation::Status status,
                                               const Simulation& simulation, bool includeName,
                                               const QPalette& palette);

    /// The warnings cell's tooltip (SimulationPanel.getSimulationWarningsToolTip()): "<html>",
    /// with @p includeName "<b>name</b>", then "<br>No simulation data available." without data,
    /// "<br>" and "No warnings." (simpanel.ttip.noWarnings) in TEXT_DIM without warnings, else for
    /// each priority that has warnings, critical (ERROR), normal (WARNING) and informational
    /// (INFO) ones, "<br><b>" and its title in its colour ("Critical(s):", "Warning(s):",
    /// "Informational(s):"), "</b>", and "<br>" with Warning::toString() of each of them.
    [[nodiscard]] static QString warningsToolTip(const Simulation& simulation, bool includeName,
                                                 const QPalette& palette);

    /// The counts of @p simulation's warnings; nullopt without data.
    [[nodiscard]] static std::optional<WarningCounts> warningCounts(const Simulation& simulation);

    /// The description OpenRocket gives the icon of @p status (Icons.java's
    /// SIMULATION_STATUS_ICON_MAP, not translated there): the status cell's accessible text.
    [[nodiscard]] static QString statusIconText(Simulation::Status status);
};

}  // namespace QtRocket::Gui
