#pragma once

// The fixture of the GUI tests (CLAUDE.md, section GUI, and tests/gui/GuiTestMain.cpp for what the
// whole test run sets up).

#include <array>
#include <filesystem>

#include <gtest/gtest.h>

#include "QtRocket/unit/UnitGroup.h"
#include "unit/DefaultUnitsGuard.h"

namespace QtRocket::Gui::Test
{

/// The fixture of every GUI test (TEST_F, through an alias named after the class under test:
/// using MainWindow = GuiTest;). A test starts from the default units, and its TearDown() checks
/// that it left nothing of the process behind: no top-level widget (after the objects it gave to
/// deleteLater() are deleted), nothing on the clipboard, the default units and the current
/// directory as the test found them. Each check that fails is a test failure; the clipboard, the
/// units and the directory are put back all the same, so that the next test of the same process
/// starts clean. A top-level widget left over is reported, never deleted (whoever made it may
/// still own it).
class GuiTest : public ::testing::Test
{
protected:
    void SetUp() override;
    void TearDown() override;

private:
    QtRocket::Test::DefaultUnitsGuard m_units;
    std::array<int, kUnitGroupCount>  m_defaultUnits{};
    std::filesystem::path             m_currentDirectory;
};

}  // namespace QtRocket::Gui::Test
