#pragma once

// The simulation settings dialog (dialogs/simulation/SimulationConfigDialog) as the
// simulation-dialog pins (SimulationDialogPins.h) hold OpenRocket's, and those pins with
// QtRocket's deliberate differences: what the exit suite OpenDesignsExitGolden compares for every
// simulation of its designs (ShowsEverySimulationInItsDialogAndCancels).

#include <string>
#include <vector>

#include "QtRocket/simulation/Simulation.h"

class QTabWidget;

namespace QtRocket::Gui
{
class SimulationConfigDialog;
}  // namespace QtRocket::Gui

namespace QtRocket::Gui::Test
{

struct SimulationDialogPins;
struct SimulationDialogPinSimulation;

/// @p dialog as the probe walks OpenRocket's (its W lines): dialogWalk() of the top rows, the line
/// of the tabs ("TABS [<title>]... selected=<index>"), each tab's title ("TAB <title>") with, for
/// the Launch conditions, Simulation options and Warnings tabs, dialogWalk() of its page indented
/// by two spaces, then dialogWalk() of the buttons' row.
[[nodiscard]] std::vector<std::string> settingsDialogWalk(const SimulationConfigDialog& dialog);

/// Each tab of @p tabs as "<title> enabled|disabled[ tip=<tooltip>]".
[[nodiscard]] std::vector<std::string> settingsDialogTabStates(const QTabWidget& tabs);

/// The pinned walk of @p simulation's dialog with QtRocket's deliberate differences:
/// - the status line (the dialog shows the status @p status the simulation had when it was made)
///   in QtRocket's colour of the status (SimulationStatusColors, the application's palette), its
///   text OpenRocket's; OpenRocket's colour must be the theme's colour of the status's key
///   (@p pins' colors: a test failure otherwise);
/// - no "Add extension" button (U-M4-3: Milestone 7), and no Configure button in an extension's
///   panel (the first of its icon buttons where the pins name CONFIGURE first);
/// - the Warnings tab's collapsible sections' headers without an icon (CollapsiblePanel: the
///   style's arrow) and their descriptions as word-wrapped labels (TEXTAREA); each of these
///   edits must apply to exactly one line of the walk (a test failure otherwise), so that it
///   cannot outlive the pin it was made for.
[[nodiscard]] std::vector<std::string> expectedSettingsDialogWalk(
    const SimulationDialogPins& pins, const SimulationDialogPinSimulation& simulation,
    Simulation::Status status);

/// The buttons of each extension's panel in @p dialog's Simulation options tab
/// (SimulationExtensionList), in the order of their row, by their icons as the probe's EXT lines
/// name them, space separated ("HELP DELETE"): CONFIGURE, HELP or DELETE for a button that draws
/// as Icons::Id::CONFIGURE, HELP or EDIT_DELETE, "other" for another; one line per extension.
[[nodiscard]] std::vector<std::string> settingsDialogExtensionButtons(
    const SimulationConfigDialog& dialog);

/// The pinned buttons of @p simulation's extensions' panels (its EXT lines) without a first
/// CONFIGURE (U-M4-3: the Configure button comes with Milestone 7).
[[nodiscard]] std::vector<std::string> expectedSettingsDialogExtensionButtons(
    const SimulationDialogPinSimulation& simulation);

}  // namespace QtRocket::Gui::Test
