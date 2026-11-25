#pragma once

// The pins of OpenRocket's simulations table (the "Flight simulations" tab):
// tests/data/goldens/gui/simulation-table/simulation-table.json, which
// tools/openrocket-gui-pins/generate.sh writes from its SimulationTableProbe (family
// simulation-table; its README.md and the probe's class comment say what each value is). The
// table holds, for each design the probe was given (the 66 designs of the exit suite
// OpenDesignsExitGolden, open-designs/designs.py's list: the sixteen examples, the 49 re-saves and
// the GUI reference; and a new design): every cell's text, the status column's icon, sort text,
// description and tooltip, the warnings' counts, box and tooltip, the orders of a click and a
// second click on each column's header, the states of the panel's twelve actions, and the texts of
// the CSV export and the Copy; then two scenarios of edits with every simulation's status after
// each step, and a Copy whose rows include one without data. Where QtRocket fixes one of
// OpenRocket's bugs the table holds QtRocket's answer, with OpenRocket's own beside it: the
// warnings column's orders (bug 23), the pairing Copy (bug 25). A simulation a scenario ran fresh
// has no cells, warnings or counts (CLAUDE.md: no value of a fresh flight is pinned). Read once per
// test process, by the first test that compares with them.

#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/util/Error.h"

namespace QtRocket::Gui::Test
{

/// The schema version of the table (gen_simulation_table_pins.py's SCHEMA_VERSION).
inline constexpr int kSimulationTablePinsSchemaVersion = 1;

/// A column of OpenRocket's table: its id, header text, header tooltip and the default unit of
/// its unit group (UNITS_NONE's for a column without units).
struct SimulationTablePinColumn
{
    std::string id;
    std::string header;
    std::string toolTip;
    std::string unit;
};

/// The warnings column of a row: the counts (critical, normal, informational; nullopt without
/// data), the box's labels ("1 [NORMAL]": a count's text, an icon by its level), the tooltip, and
/// each warning as its priority's name and text.
struct SimulationTablePinWarnings
{
    std::optional<std::array<std::size_t, 3>>        counts;
    std::string                                      box;
    std::string                                      toolTip;
    std::vector<std::pair<std::string, std::string>> messages;
};

/// A row (a simulation, in the document's order).
struct SimulationTablePinRow
{
    int row = 0;
    /// The status's constant name ("LOADED").
    std::string status;
    /// The description of OpenRocket's status icon ("Loaded from File").
    std::string statusIcon;
    /// What the status column sorts by (StatusLabel.toString()).
    std::string statusSortText;
    /// getStatusDescription().
    std::string description;
    /// The tooltip of every cell but the warnings'.
    std::string statusToolTip;
    /// Whether a scenario ran the simulation fresh: then warnings, toolTip and cells are empty.
    bool fresh = false;
    /// The warnings column; nullopt for a fresh row.
    std::optional<SimulationTablePinWarnings> warnings;
    /// The cells of the model columns 2 to 13 (name to ground hit velocity): nullopt for Java's
    /// null (a value column without data). Empty for a fresh row.
    std::vector<std::optional<std::string>> cells;
};

/// A column's view order after one click on its header (ascending) and after a second click
/// (descending): the model rows (document indices) in the view's order.
struct SimulationTablePinOrder
{
    std::vector<int> ascending;
    std::vector<int> descending;
};

/// The width of a view column of OpenRocket's table: its id, preferred, minimum and maximum width.
struct SimulationTablePinWidth
{
    std::string id;
    int         preferred = 0;
    int         minimum   = 0;
    int         maximum   = 0;
};

/// A design's table.
struct SimulationTablePinDesign
{
    /// The name of the design in designs.py's list ("example_a_simple_model_rocket"), which is
    /// that of its OpenDesign of exit/OpenDesigns.h, or "new_design".
    std::string name;
    /// Its kind: designs.py's ("example", "example-resave", "testrocket", "legacy", "coverage",
    /// "gui": exit/OpenDesigns.h's openDesignKind()), or "new".
    std::string kind;
    /// Its file's name ("A simple model rocket.ork" in data/examples, "rocket.ork" for every
    /// re-save and the GUI reference); empty for the new design.
    std::string file;
    bool        hasValidConfig = false;
    std::string nextSimulationName;
    std::size_t simulationCount = 0;
    /// The view's columns (OpenRocket's widths, after setupAutoSizeColumns()).
    std::vector<SimulationTablePinWidth> widths;
    /// The states of the panel's actions ("newSimulation" ... "selectedSimsExport") for the
    /// selections "none", "row0" and "rows01" (those the table has rows for).
    std::map<std::string, std::map<std::string, bool>> actions;
    /// The CSV export (",", 3 decimals, plain), the Copy of every row and the CSV export (";", 2
    /// decimals, exponential); nullopt without simulations.
    std::optional<std::string>         csv;
    std::optional<std::string>         copy;
    std::optional<std::string>         csvExponential;
    std::vector<SimulationTablePinRow> simulations;
    /// The orders by column id; "openRocket:<id>" holds OpenRocket's own where QtRocket sorts
    /// otherwise (the warnings column, bug 23).
    std::map<std::string, SimulationTablePinOrder> orders;

    /// The order of the column @p id; a test failure and an empty order when there is none.
    [[nodiscard]] const SimulationTablePinOrder& order(std::string_view id) const;
};

/// A step of a scenario: what it did, then every simulation's status, the undo and redo
/// descriptions (nullopt: none) and whether the document was saved.
struct SimulationTablePinStep
{
    std::string                step;
    std::vector<std::string>   statuses;
    std::optional<std::string> undo;
    std::optional<std::string> redo;
    bool                       saved = false;
};

/// A simulation of a scenario's design as loaded: its name, the first 8 characters of its
/// configuration's id and the configuration's name.
struct SimulationTablePinInitial
{
    std::string name;
    std::string configurationId;
    std::string configuration;
};

/// A status scenario: the design (a file of data/examples), its simulations as loaded, its steps,
/// the rows after the last step and the status column's orders then.
struct SimulationTablePinScenario
{
    std::string                                    name;
    std::string                                    file;
    std::vector<SimulationTablePinInitial>         initial;
    std::vector<SimulationTablePinStep>            steps;
    std::vector<SimulationTablePinRow>             simulations;
    std::map<std::string, SimulationTablePinOrder> orders;
};

/// The Copy of every row of a table whose rows include one without data (bug 25): its design,
/// the step that made it, QtRocket's text (each line with its own simulation's status) and
/// OpenRocket's (the statuses paired with the lines by position).
struct SimulationTablePinPairing
{
    std::string                         name;
    std::string                         file;
    std::vector<SimulationTablePinStep> steps;
    std::string                         copy;
    std::string                         openRocketCopy;
};

/// The whole table.
struct SimulationTablePins
{
    std::string commit;
    /// The theme's colours of the tooltips (#rrggbb): text, textDim, warning, error, info.
    std::map<std::string, std::string>      colors;
    std::vector<SimulationTablePinColumn>   columns;
    std::vector<SimulationTablePinDesign>   designs;
    std::vector<SimulationTablePinScenario> scenarios;
    std::vector<SimulationTablePinPairing>  pairings;

    /// The design named @p name, the scenario named @p name, the pairing named @p name; null
    /// when there is none.
    [[nodiscard]] const SimulationTablePinDesign*   findDesign(std::string_view name) const;
    [[nodiscard]] const SimulationTablePinScenario* findScenario(std::string_view name) const;
    [[nodiscard]] const SimulationTablePinPairing*  findPairing(std::string_view name) const;
};

/// tests/data/goldens/gui/simulation-table/simulation-table.json, read and interpreted once per
/// test process (or the Error of the read or of its interpretation).
[[nodiscard]] const Result<SimulationTablePins>& simulationTablePins();

/// simulationTablePins(), or null and a test failure when they cannot be read.
[[nodiscard]] const SimulationTablePins* simulationTablePinsOrFail();

/// The name the pins give the new design, their last.
inline constexpr std::string_view kNewDesignPinName = "new_design";

/// The names of the designs the pins hold, in their order, for INSTANTIATE_TEST_SUITE_P: those of
/// OpenDesignsExitGolden's list (exit/OpenDesigns.h's openDesigns()), then kNewDesignPinName.
/// Made without reading the pins, which GoogleTest's generators would read in every test process
/// (a process per test under ctest): only the tests that compare with them read them.
/// SimulationTablePinsGolden and OpenDesignsExitGoldenDesigns check that the pins hold exactly
/// these designs; a design the pins lack fails its tests.
[[nodiscard]] std::vector<std::string> simulationTablePinnedDesigns();

/// The names of simulationTablePinnedDesigns() of OpenRocket's sixteen example designs (the
/// pins' kind "example", which loadSimulationExample() loads by their files' names) and of the
/// new design, in their order: for the suites that compare the tables of the examples alone (the
/// exit suite OpenDesignsExitGolden compares every design's in its windows).
[[nodiscard]] std::vector<std::string> simulationTablePinnedExampleDesigns();

/// The name of the tests of a design of simulationTablePinnedDesigns(): the design's name in the
/// pins ("example_a_simple_model_rocket").
[[nodiscard]] std::string simulationTablePinTestName(
    const ::testing::TestParamInfo<std::string>& param);

}  // namespace QtRocket::Gui::Test
