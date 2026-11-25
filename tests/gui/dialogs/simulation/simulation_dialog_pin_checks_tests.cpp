// The simulation-dialog pins (SimulationDialogPins.h: OpenRocket's settings dialog of every
// simulation of the exit suite's designs) as the suite reads them, and the deliberate differences
// SimulationDialogPinChecks.h applies to them before the suite compares QtRocket's dialog.

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/ColorConversion.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/SimulationStatusColors.h"
#include "QtRocket/simulation/Simulation.h"
#include "dialogs/simulation/SimulationDialogPinChecks.h"
#include "dialogs/simulation/SimulationDialogPins.h"

namespace QtRocket::Gui
{

namespace
{

using SimulationDialogPinChecks = Gui::Test::GuiTest;
using Status                    = Simulation::Status;

/// Pins with OpenRocket's FlatLaf Light status colours (the table's).
[[nodiscard]] Gui::Test::SimulationDialogPins lightThemePins()
{
    Gui::Test::SimulationDialogPins pins;
    pins.colors = {{"text", "#000000"}, {"warning", "#d99800"}, {"error", "#ff0000"}};
    return pins;
}

/// A simulation's pins whose walk is @p walk and whose extensions' buttons are @p extensions.
[[nodiscard]] Gui::Test::SimulationDialogPinSimulation simulationPins(
    std::vector<std::string> walk, std::vector<std::string> extensions = {})
{
    Gui::Test::SimulationDialogPinSimulation simulation;
    simulation.walk       = std::move(walk);
    simulation.extensions = std::move(extensions);
    simulation.tabs       = {"Launch conditions enabled", "Simulation options enabled",
                             "Warnings enabled", "Plot data enabled", "Export data enabled"};
    return simulation;
}

/// QtRocket's status line of @p status with OpenRocket's text @p text.
[[nodiscard]] std::string qtStatusLine(Status status, const char* text)
{
    return "LABEL <html>" +
           QtStrings::toUtf8(ColorConversion::formatHtmlColor(SimulationStatusColors::color(status),
                                                              QtStrings::fromUtf8(text))) +
           "</html>";
}

TEST_F(SimulationDialogPinChecks, TheExpectedWalkTakesQtRocketsDeliberateDifferences)
{
    // The status in QtRocket's colour; no "Add extension" (U-M4-3); no Configure button where an
    // extension's panel has one first (the second extension's panel has none); the collapsible
    // sections' headers without their icon, the description a word-wrapped label.
    const Gui::Test::SimulationDialogPinSimulation simulation = simulationPins(
        {"LABEL Status:",
         R"(LABEL <html><font color="#000000"><i>Loaded from file</i></font></html>)",
         "TAB Simulation options", "  BORDER Simulation extensions", "    BUTTON Add extension",
         "    LABEL Roll Control", "    BUTTON  icon", "    BUTTON  icon", "    BUTTON  icon",
         "    LABEL Java code: Listener", "    BUTTON  icon", "    BUTTON  icon", "TAB Warnings",
         "    BUTTON Single Deployment Warnings icon",
         "    LABEL <html><body style='margin:0;'>Warn</body></html>",
         "    BUTTON Dual Deployment Warnings icon"},
        {"CONFIGURE HELP DELETE", "HELP DELETE"});
    const std::vector<std::string> expected{
        "LABEL Status:",
        qtStatusLine(Status::LOADED, "<i>Loaded from file</i>"),
        "TAB Simulation options",
        "  BORDER Simulation extensions",
        "    LABEL Roll Control",
        "    BUTTON  icon",
        "    BUTTON  icon",
        "    LABEL Java code: Listener",
        "    BUTTON  icon",
        "    BUTTON  icon",
        "TAB Warnings",
        "    BUTTON Single Deployment Warnings",
        "    TEXTAREA <html><body style='margin:0;'>Warn</body></html>",
        "    BUTTON Dual Deployment Warnings"};
    EXPECT_EQ(Gui::Test::expectedSettingsDialogWalk(lightThemePins(), simulation, Status::LOADED),
              expected);
}

TEST_F(SimulationDialogPinChecks, AnErrorStatusTakesQtRocketsErrorColour)
{
    const Gui::Test::SimulationDialogPinSimulation simulation = simulationPins(
        {"LABEL Status:", R"(LABEL <html><font color="#ff0000"><b>Cannot run</b></font></html>)",
         "    BUTTON Add extension"});
    const std::vector<std::string> expected{"LABEL Status:",
                                            qtStatusLine(Status::CANT_RUN, "<b>Cannot run</b>")};
    EXPECT_EQ(Gui::Test::expectedSettingsDialogWalk(lightThemePins(), simulation, Status::CANT_RUN),
              expected);
}

TEST_F(SimulationDialogPinChecks, AStatusInAnotherColourThanTheThemesFails)
{
    // OpenRocket's status line must be in its theme's colour of the status (the text's for a
    // simulation loaded from its file).
    const Gui::Test::SimulationDialogPinSimulation simulation = simulationPins(
        {"LABEL Status:", R"(LABEL <html><font color="#ff0000">Loaded</font></html>)",
         "    BUTTON Add extension"});
    const Gui::Test::SimulationDialogPins pins = lightThemePins();
    EXPECT_NONFATAL_FAILURE(
        static_cast<void>(Gui::Test::expectedSettingsDialogWalk(pins, simulation, Status::LOADED)),
        "is not its theme's text of LOADED");
}

TEST_F(SimulationDialogPinChecks, AnExtensionOfThePinsMissingFromTheWalkFails)
{
    const Gui::Test::SimulationDialogPinSimulation simulation = simulationPins(
        {"LABEL Status:", R"(LABEL <html><font color="#000000">Loaded</font></html>)",
         "  BORDER Simulation extensions", "    BUTTON Add extension",
         "    LABEL No simulation extensions defined"},
        {"HELP DELETE"});
    const Gui::Test::SimulationDialogPins pins = lightThemePins();
    EXPECT_NONFATAL_FAILURE(
        static_cast<void>(Gui::Test::expectedSettingsDialogWalk(pins, simulation, Status::LOADED)),
        "extensions' names in the walk");
}

TEST_F(SimulationDialogPinChecks, TheExportPlaceholderIsDisabledWhereOpenRocketsTabIsEnabled)
{
    // Until the export panel comes, the dialog's Export data tab is a disabled placeholder
    // (SimulationConfigDialog's documented deviation); OpenRocket's disabled tab is kept.
    Gui::Test::SimulationDialogPinSimulation simulation = simulationPins({});
    EXPECT_EQ(Gui::Test::expectedSettingsDialogTabStates(simulation, true).back(),
              "Export data disabled");
    EXPECT_EQ(Gui::Test::expectedSettingsDialogTabStates(simulation, false).back(),
              "Export data enabled");
    simulation.tabs.back() = "Export data disabled tip=Simulation has no data to export";
    EXPECT_EQ(Gui::Test::expectedSettingsDialogTabStates(simulation, true).back(),
              "Export data disabled tip=Simulation has no data to export");
}

/// The number of the pins' simulations, and of those with a multi-level wind's levels.
struct PinnedSimulationCounts
{
    std::size_t simulations = 0;
    std::size_t winds       = 0;
};

[[nodiscard]] PinnedSimulationCounts pinnedSimulationCounts(
    const Gui::Test::SimulationDialogPins& pins)
{
    PinnedSimulationCounts counts;
    for (const Gui::Test::SimulationDialogPinDesign& design : pins.designs)
    {
        for (const Gui::Test::SimulationDialogPinSimulation& simulation : design.simulations)
        {
            ++counts.simulations;
            counts.winds += simulation.wind.empty() ? 0U : 1U;
        }
    }
    return counts;
}

/// The simulations of @p pins whose walk does not start with the name's label or that have not
/// five tabs, as "<design> <index>".
[[nodiscard]] std::vector<std::string> malformedSimulations(
    const Gui::Test::SimulationDialogPins& pins)
{
    std::vector<std::string> malformed;
    for (const Gui::Test::SimulationDialogPinDesign& design : pins.designs)
    {
        for (const Gui::Test::SimulationDialogPinSimulation& simulation : design.simulations)
        {
            if (simulation.walk.empty() || simulation.walk.front() != "LABEL Name: " ||
                simulation.tabs.size() != 5U ||
                simulation.changes.size() != design.simulations.size())
            {
                malformed.push_back(design.name + ' ' + std::to_string(simulation.index));
            }
        }
    }
    return malformed;
}

// The table holds the 66 designs' 196 simulations, two of them with a multi-level wind (the ninth
// of the test rocket "Estes Alpha III" and the saver coverage's kitchen sink), each with its walk,
// its five tabs and a change count per simulation of its design.
TEST(SimulationDialogPinsGolden, HoldEverySimulationOfTheSixtySixDesigns)
{
    const Gui::Test::SimulationDialogPins* const pins = Gui::Test::simulationDialogPinsOrFail();
    ASSERT_NE(pins, nullptr);
    EXPECT_EQ(pins->designs.size(), 66U);
    const PinnedSimulationCounts counts = pinnedSimulationCounts(*pins);
    EXPECT_EQ(counts.simulations, 196U);
    EXPECT_EQ(counts.winds, 2U);
    EXPECT_EQ(malformedSimulations(*pins), std::vector<std::string>{});
    EXPECT_EQ(pins->colors.size(), 3U);
}

}  // namespace

}  // namespace QtRocket::Gui
