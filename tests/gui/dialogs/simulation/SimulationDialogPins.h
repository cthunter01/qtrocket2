#pragma once

// The pins of OpenRocket's simulation settings dialog (SimulationConfigDialog) over every
// simulation of the designs of the exit suite OpenDesignsExitGolden:
// tests/data/goldens/gui/simulation-dialog/simulation-dialog.json, which
// tools/openrocket-gui-pins/generate.sh writes from its SimulationDialogProbe (family
// simulation-dialog; its README.md and the probe's class comment say what each value is). For each
// design (open-designs/designs.py's list: the sixteen examples, the 49 re-saves and the GUI
// reference) and each of its simulations, in the document's order, on one document: the dialog as
// the tab's Edit opens it (its title; its walk, in the form of SimulationDialogWalk.h's
// dialogWalk(): the top rows, the line of the tabs, each tab's title with the pages of the first
// three, the buttons; the tabs' states; the buttons of the extensions' panels; a multi-level
// wind's levels in the wind editor's table, in WindEditorTranscript.h's form), whether it was
// modified before its Cancel, the change events every simulation of the document fired while it
// was open, and the document's undo state, saved state and statuses before the dialog and after
// its Cancel. Read once per test process.

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/util/Error.h"

namespace QtRocket::Gui::Test
{

/// The schema version of the table (gen_simulation_dialog_pins.py's SCHEMA_VERSION).
inline constexpr int kSimulationDialogPinsSchemaVersion = 1;

/// The document's undo state (OpenRocketDocument::UndoDetail's fields, Java's private ones) and
/// whether it was saved.
struct SimulationDialogPinUndo
{
    std::size_t                             position = 0;
    std::vector<std::optional<std::string>> descriptions;
    std::optional<std::string>              next;
    std::optional<std::string>              stored;
    bool                                    clean = true;
    bool                                    saved = false;
};

/// A simulation's dialog.
struct SimulationDialogPinSimulation
{
    /// The simulation's index in the document and its name.
    std::size_t index = 0;
    std::string name;
    /// The title when the dialog was made ("Edit simulation").
    std::string title;
    /// Whether the dialog was modified (isModified) right before its Cancel, and its title then.
    bool        modified = false;
    std::string modifiedTitle;
    /// The walk's lines (the probe's W lines).
    std::vector<std::string> walk;
    /// Each tab's state, "<title> enabled|disabled[ tip=<tooltip>]".
    std::vector<std::string> tabs;
    /// The buttons of each extension's panel of the Simulation options tab, in order, by their
    /// icons: "CONFIGURE HELP DELETE".
    std::vector<std::string> extensions;
    /// A multi-level wind's levels in the wind editor's table (its HEADER, ROW and MODEL lines);
    /// empty for an average wind.
    std::vector<std::string> wind;
    /// The change events each simulation of the document fired while the dialog was open.
    std::vector<int> changes;
    /// The undo state before the dialog was made and after its Cancel.
    SimulationDialogPinUndo undoBefore;
    SimulationDialogPinUndo undoAfter;
    /// The statuses of the document's simulations (the constants' names, "LOADED") before the
    /// dialog and after its Cancel.
    std::vector<std::string> statusesBefore;
    std::vector<std::string> statusesAfter;
};

/// A design: its name in designs.py's list (that of its OpenDesign of exit/OpenDesigns.h), its
/// kind ("example", ...: openDesignKind()), its file's name, its simulations.
struct SimulationDialogPinDesign
{
    std::string                                name;
    std::string                                kind;
    std::string                                file;
    std::vector<SimulationDialogPinSimulation> simulations;
};

/// The whole table.
struct SimulationDialogPins
{
    std::string commit;
    /// The theme's colours of a simulation's status (UITheme.getStatusColor(), #rrggbb): "text",
    /// "warning", "error".
    std::map<std::string, std::string>     colors;
    std::vector<SimulationDialogPinDesign> designs;

    /// The design named @p name; null when there is none.
    [[nodiscard]] const SimulationDialogPinDesign* findDesign(std::string_view name) const;
};

/// tests/data/goldens/gui/simulation-dialog/simulation-dialog.json, read and interpreted once per
/// test process (or the Error of the read or of its interpretation).
[[nodiscard]] const Result<SimulationDialogPins>& simulationDialogPins();

/// simulationDialogPins(), or null and a test failure when they cannot be read.
[[nodiscard]] const SimulationDialogPins* simulationDialogPinsOrFail();

}  // namespace QtRocket::Gui::Test
