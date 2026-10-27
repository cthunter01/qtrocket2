#pragma once

// The fixture of the GUI tests (CLAUDE.md, section GUI, and tests/gui/GuiTestMain.cpp for what the
// whole test run sets up).
//
// The test services (TestGuiServices.h) turn the parts library's auto-open off: the application
// opens the parts library for every new component of a kind that takes presets (the preference
// AutoOpenPartsLibrary, true by default, as in OpenRocket: ComponentConfigDialog), which would ask
// every test that adds a body tube, a nose cone, ... through an add button for a preset it did not
// script. The tests of the auto-open turn it on; the exit replays (tests/gui/exit) run with the
// application's own preferences, where FromScratchPrompts answers it.

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
/// deleteLater() are deleted), nothing on the clipboard nor on the application's component
/// clipboard (OpenRocketClipboard::instance()), the default units, the current directory and the
/// application's palette as the test found them. Each check that fails is a test failure; the
/// clipboards, the units, the directory and the palette are put back all the same,
/// so that the next test of the same process starts clean (ScopedApplicationPalette changes the
/// palette for a scope). A top-level widget left over is reported, never deleted (whoever made it
/// may still own it).
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
