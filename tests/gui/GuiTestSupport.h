#pragma once

// The fixture of the GUI tests (CLAUDE.md, section GUI, and tests/gui/GuiTestMain.cpp for what the
// whole test run sets up).
//
// The test services (TestGuiServices.h) turn the parts library's auto-open off: the application
// opens the parts library for every new component of a kind that takes presets (the preference
// AutoOpenPartsLibrary, true by default, as in OpenRocket: ComponentConfigDialog), which would ask
// every test that adds a body tube, a nose cone, ... through an add button for a preset it did not
// script. The tests of the auto-open turn it on. Of the exit replays (tests/gui/exit), only the
// from-scratch one (FromScratchScenario) answers the parts library, through FromScratchPrompts: it
// runs with the application's own preferences, where the auto-open is on. The open-designs replay
// (open_designs_exit_tests.cpp) uses DesignFileEnvironment's preferences, where it is on too, and
// a plain ScriptedUserPrompts: a step of it that adds a component of a kind that takes presets must
// script the question (answerPreset()) or turn AutoOpenPartsLibrary off first.
//
// No test runs a simulation that it did not ask for (O-M4-12). The design view's background
// estimate (the preference backgroundFlight, true by default, as in OpenRocket) is off in the test
// services and in the tests of the application over a preferences file (ApplicationSessionTests,
// design_window_integration_tests.cpp, FromScratchScenario and so FlightConfigurationScenario),
// which turn it off before the application starts; the tests of the estimate turn it on. The
// services a test makes of its own objects (the open-designs replay's, ...) have no simulation
// executor (GuiServices::simulationExecutor() is null), and without one nothing runs a simulation.
// The test services' executor runs a job on the GUI thread in the next turn of the event loop
// (ManualSimulationExecutor, IMMEDIATE: QCoreApplication::processEvents(), or its runAll()), so a
// test that runs simulations needs no sleep and no thread; fresh runs use only the
// short flights of "Clustered motors" #1, "3D printable" #1 or "A simple model rocket" #1, and pin
// no late value of a fresh flight (CLAUDE.md, Portability).

#include <array>
#include <filesystem>

#include <QPalette>

#include <gtest/gtest.h>

#include "QtRocket/unit/UnitGroup.h"
#include "unit/DefaultUnitsGuard.h"

namespace QtRocket::Gui::Test
{

/// The fixture of every GUI test (TEST_F, through an alias named after the class under test:
/// using MainWindow = GuiTest;). A test starts from the default units, and its TearDown() checks
/// that it left nothing of the process behind: no top-level widget (after the objects it gave to
/// deleteLater() are deleted), nothing on the clipboard nor on the application's clipboard of
/// components and simulations (OpenRocketClipboard::instance()), the default units, the current
/// directory and the application's palette as the test found them. Each check that fails is a
/// test failure; the clipboards, the units, the directory and the palette are put back all the
/// same, so that the next test of the same process starts clean (ScopedApplicationPalette changes
/// the palette for a scope). A top-level widget left over is reported, never deleted (whoever
/// made it may still own it).
class GuiTest : public ::testing::Test
{
protected:
    void SetUp() override;
    void TearDown() override;

private:
    QtRocket::Test::DefaultUnitsGuard m_units;
    std::array<int, kUnitGroupCount>  m_defaultUnits{};
    std::filesystem::path             m_currentDirectory;
    QPalette                          m_palette;
};

}  // namespace QtRocket::Gui::Test
