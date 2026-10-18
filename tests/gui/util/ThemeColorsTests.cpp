#include "QtRocket/gui/util/ThemeColors.h"

#include <algorithm>
#include <cstdlib>
#include <string_view>

#include <QColor>
#include <QPalette>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/Preferences.h"
#include "ScopedApplicationPalette.h"

namespace QtRocket::Gui
{

namespace
{

using ThemeColorsTest = Test::GuiTest;
using Key             = ThemeColors::Key;
using Theme           = ThemeColors::Theme;
using Palette         = Test::ScopedApplicationPalette;

/// The largest difference between the channels of @p a and @p b.
int channelDistance(const QColor& a, const QColor& b)
{
    return std::max({std::abs(a.red() - b.red()), std::abs(a.green() - b.green()),
                     std::abs(a.blue() - b.blue()), std::abs(a.alpha() - b.alpha())});
}

TEST_F(ThemeColorsTest, ALightPaletteIsTheLightThemeAndADarkOneTheDarkTheme)
{
    EXPECT_EQ(ThemeColors::theme(Palette::light()), Theme::LIGHT);
    EXPECT_EQ(ThemeColors::theme(Palette::dark()), Theme::DARK);
    {
        const Palette dark(Palette::dark());
        EXPECT_EQ(ThemeColors::currentTheme(), Theme::DARK);
    }
    {
        const Palette light(Palette::light());
        EXPECT_EQ(ThemeColors::currentTheme(), Theme::LIGHT);
    }
}

TEST_F(ThemeColorsTest, TakesTheValuesOfOpenRocketsThemeFiles)
{
    // themes/FlatLightLaf.properties and FlatDarculaLaf.properties.
    EXPECT_EQ(ThemeColors::themeColor(Key::CG, Theme::LIGHT), QColor(0x00, 0x00, 0xff));
    EXPECT_EQ(ThemeColors::themeColor(Key::CG, Theme::DARK), QColor(0x55, 0x85, 0xfd));
    EXPECT_EQ(ThemeColors::themeColor(Key::CP, Theme::LIGHT), QColor(0xff, 0x00, 0x00));
    EXPECT_EQ(ThemeColors::themeColor(Key::CP, Theme::DARK), QColor(0xff, 0x48, 0x6a));
    EXPECT_EQ(ThemeColors::themeColor(Key::BACKGROUND, Theme::DARK), QColor(0x49, 0x4c, 0x4f));
    // #RRGGBBAA in FlatLaf: the motor's border #000000c8 and fill #00000064.
    EXPECT_EQ(ThemeColors::themeColor(Key::MOTOR_BORDER, Theme::LIGHT), QColor(0, 0, 0, 0xc8));
    EXPECT_EQ(ThemeColors::themeColor(Key::MOTOR_FILL, Theme::LIGHT), QColor(0, 0, 0, 0x64));
    EXPECT_EQ(ThemeColors::themeColor(Key::FIN_GRID_MAJOR, Theme::LIGHT),
              QColor(0x00, 0x00, 0xff, 0x50));
    // References resolved: OR.icons.zoomOut = $OR.colors.darkRed, OR.icons.delete differs.
    EXPECT_EQ(ThemeColors::themeColor(Key::ICONS_ZOOM_OUT, Theme::LIGHT), QColor(0xc8, 0x00, 0x00));
    EXPECT_EQ(ThemeColors::themeColor(Key::ICONS_DELETE, Theme::DARK), QColor(0xf6, 0x8f, 0x8f));
    EXPECT_EQ(ThemeColors::themeColor(Key::RULER, Theme::DARK),
              ThemeColors::themeColor(Key::YELLOW, Theme::DARK));
}

TEST_F(ThemeColorsTest, EveryKeyHasOpenRocketsName)
{
    EXPECT_EQ(ThemeColors::keyName(Key::CG), "OR.colors.cg");
    EXPECT_EQ(ThemeColors::keyName(Key::ICONS_TINT_COLOR), "OR.icons.tintColor");
    EXPECT_EQ(ThemeColors::keyName(Key::VISIBILITY_HIDDEN_FOREGROUND),
              "OR.colors.visibility.hidden.foreground");
    for (const Key key : ThemeColors::kAllKeys)
    {
        EXPECT_TRUE(ThemeColors::keyName(key).starts_with("OR.")) << ThemeColors::keyName(key);
    }
}

TEST_F(ThemeColorsTest, ThePalettesColourIsTheIconsDefault)
{
    const QPalette light = Palette::light();
    const QPalette dark  = Palette::dark();
    EXPECT_EQ(ThemeColors::color(Key::ICONS_DEFAULT, light),
              light.color(QPalette::Active, QPalette::WindowText));
    EXPECT_EQ(ThemeColors::color(Key::ICONS_DEFAULT, dark),
              dark.color(QPalette::Active, QPalette::WindowText));
    // The others are the theme's.
    EXPECT_EQ(ThemeColors::color(Key::CG, dark), ThemeColors::themeColor(Key::CG, Theme::DARK));
    EXPECT_EQ(ThemeColors::color(Key::CG, light), ThemeColors::themeColor(Key::CG, Theme::LIGHT));
}

TEST_F(ThemeColorsTest, HiddenTextIsThePalettesTextLightenedOrDarkened)
{
    // FlatLaf's lighten(black, 25%) is #404040.
    QPalette light = Palette::light();
    light.setColor(QPalette::Text, QColor(0, 0, 0));
    EXPECT_LE(channelDistance(ThemeColors::color(Key::VISIBILITY_HIDDEN_FOREGROUND, light),
                              QColor(0x40, 0x40, 0x40)),
              1);
    // darken(#bbbbbb, 25%) is #7b7b7b.
    QPalette dark = Palette::dark();
    dark.setColor(QPalette::Text, QColor(0xbb, 0xbb, 0xbb));
    EXPECT_LE(channelDistance(ThemeColors::color(Key::VISIBILITY_HIDDEN_FOREGROUND, dark),
                              QColor(0x7b, 0x7b, 0x7b)),
              1);
}

TEST_F(ThemeColorsTest, TheLightComponentColoursAreTheCoresDefaults)
{
    const InMemoryPreferences preferences;
    const ComponentDefaults   core  = preferences.defaultComponentColors();
    const ComponentDefaults   light = ThemeColors::defaultComponentColors(Theme::LIGHT);
    EXPECT_TRUE(std::ranges::equal(core, light));
    const ComponentDefaults dark = ThemeColors::defaultComponentColors(Theme::DARK);
    ASSERT_EQ(dark.size(), light.size());
    EXPECT_EQ(dark.front().first, "BodyComponent");
    EXPECT_EQ(dark.front().second, "150,162,255");
}

}  // namespace

}  // namespace QtRocket::Gui
