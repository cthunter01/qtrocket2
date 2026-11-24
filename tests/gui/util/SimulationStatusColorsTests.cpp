#include "QtRocket/gui/util/SimulationStatusColors.h"

#include <QColor>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/simulation/Simulation.h"
#include "ScopedApplicationPalette.h"

namespace QtRocket::Gui
{

namespace
{

using SimulationStatusColorsTest = Test::GuiTest;
using Key                        = ThemeColors::Key;
using Status                     = Simulation::Status;
using Theme                      = ThemeColors::Theme;
using Palette                    = Test::ScopedApplicationPalette;

TEST_F(SimulationStatusColorsTest, AreUiThemesStatusColours)
{
    // UITheme.getStatusColor(): ABORTED and CANT_RUN in the error colour, OUTDATED in the
    // warning colour, every other status in the text colour.
    EXPECT_EQ(SimulationStatusColors::key(Status::ABORTED), Key::ERROR);
    EXPECT_EQ(SimulationStatusColors::key(Status::CANT_RUN), Key::ERROR);
    EXPECT_EQ(SimulationStatusColors::key(Status::OUTDATED), Key::WARNING);
    EXPECT_EQ(SimulationStatusColors::key(Status::UPTODATE), Key::TEXT);
    EXPECT_EQ(SimulationStatusColors::key(Status::LOADED), Key::TEXT);
    EXPECT_EQ(SimulationStatusColors::key(Status::EXTERNAL), Key::TEXT);
    EXPECT_EQ(SimulationStatusColors::key(Status::NOT_SIMULATED), Key::TEXT);
}

TEST_F(SimulationStatusColorsTest, TakeTheColoursOfThePalettesTheme)
{
    // The light theme's #ff0000, #d99800 and #000000; the dark one's #f68f8f, #ffb066, #adadad.
    EXPECT_EQ(SimulationStatusColors::color(Status::ABORTED, Palette::light()),
              QColor(0xff, 0x00, 0x00));
    EXPECT_EQ(SimulationStatusColors::color(Status::OUTDATED, Palette::light()),
              QColor(0xd9, 0x98, 0x00));
    EXPECT_EQ(SimulationStatusColors::color(Status::UPTODATE, Palette::light()),
              QColor(0x00, 0x00, 0x00));
    EXPECT_EQ(SimulationStatusColors::color(Status::CANT_RUN, Palette::dark()),
              QColor(0xf6, 0x8f, 0x8f));
    EXPECT_EQ(SimulationStatusColors::color(Status::OUTDATED, Palette::dark()),
              QColor(0xff, 0xb0, 0x66));
    EXPECT_EQ(SimulationStatusColors::color(Status::LOADED, Palette::dark()),
              QColor(0xad, 0xad, 0xad));
}

TEST_F(SimulationStatusColorsTest, ReadTheApplicationsPaletteWhenAsked)
{
    {
        const Palette dark(Palette::dark());
        EXPECT_EQ(SimulationStatusColors::color(Status::ABORTED),
                  ThemeColors::themeColor(Key::ERROR, Theme::DARK));
    }
    const Palette light(Palette::light());
    EXPECT_EQ(SimulationStatusColors::color(Status::ABORTED),
              ThemeColors::themeColor(Key::ERROR, Theme::LIGHT));
}

}  // namespace

}  // namespace QtRocket::Gui
