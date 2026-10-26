#include "QtRocket/gui/plot/PlotTheme.h"

#include <QColor>
#include <QPalette>
#include <QRgb>
#include <Qt>

#include <gtest/gtest.h>
#include <rocketplot/PlotWidget.h>
#include <rocketplot/Theme.h>
#include <rocketplot/enums.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "ScopedApplicationPalette.h"

// OpenRocket has no JUnit test of its chart colours: the expectations are rocketplot v0.6.0's
// built-in themes and its SYSTEM look (the plot on the palette's Base, the legend on Base with
// the theme's translucency), chosen by ThemeColors' rule.

namespace QtRocket::Gui
{

namespace
{

using PlotThemeTest = Gui::Test::GuiTest;
using Palette       = Gui::Test::ScopedApplicationPalette;

constexpr QRgb kDarkBase  = 0xff202020;
constexpr QRgb kLightBase = 0xfff8f8f8;

/// @p theme on @p base, as rocketplot's ThemeMode::SYSTEM puts it there.
[[nodiscard]] rocketplot::Theme onBase(rocketplot::Theme theme, const QColor& base)
{
    theme.background = base;
    theme.legendBackground =
        QColor(base.red(), base.green(), base.blue(), theme.legendBackground.alpha());
    return theme;
}

/// @p palette with its Base colour @p base in every colour group.
[[nodiscard]] QPalette withBase(QPalette palette, QRgb base)
{
    palette.setColor(QPalette::Base, QColor::fromRgb(base));
    return palette;
}

TEST_F(PlotThemeTest, ALightPaletteGivesRocketplotsLightThemeOnItsBase)
{
    const QPalette light = Palette::light();
    ASSERT_EQ(ThemeColors::theme(light), ThemeColors::Theme::LIGHT);
    const rocketplot::Theme theme = PlotTheme::forPalette(light);
    EXPECT_TRUE(theme ==
                onBase(rocketplot::Theme::light(), light.color(QPalette::Active, QPalette::Base)));
    EXPECT_EQ(theme.background.rgba(), QColor(Qt::white).rgba());
    EXPECT_EQ(theme.legendBackground.alpha(), rocketplot::Theme::light().legendBackground.alpha());
}

TEST_F(PlotThemeTest, ADarkPaletteGivesTheDarkThemeOnItsBase)
{
    const QPalette dark = Palette::dark();
    ASSERT_EQ(ThemeColors::theme(dark), ThemeColors::Theme::DARK);
    const rocketplot::Theme theme = PlotTheme::forPalette(dark);
    EXPECT_TRUE(theme ==
                onBase(rocketplot::Theme::dark(), dark.color(QPalette::Active, QPalette::Base)));
    EXPECT_EQ(theme.background.rgba(), QColor(Qt::black).rgba());
    // Another dark Base, kept as it is.
    EXPECT_TRUE(PlotTheme::forPalette(withBase(dark, kDarkBase)) ==
                onBase(rocketplot::Theme::dark(), QColor::fromRgb(kDarkBase)));
}

TEST_F(PlotThemeTest, ABaseThatTheThemeCannotBeReadOnKeepsTheThemesOwnBackground)
{
    // A light window around dark views: ThemeColors' light theme (rocketplot's SYSTEM rule would
    // pick its dark one), on the light theme's own background.
    const QPalette lightWindow = withBase(Palette::light(), kDarkBase);
    ASSERT_EQ(ThemeColors::theme(lightWindow), ThemeColors::Theme::LIGHT);
    EXPECT_TRUE(PlotTheme::forPalette(lightWindow) == rocketplot::Theme::light());
    // And the reverse.
    const QPalette darkWindow = withBase(Palette::dark(), kLightBase);
    ASSERT_EQ(ThemeColors::theme(darkWindow), ThemeColors::Theme::DARK);
    EXPECT_TRUE(PlotTheme::forPalette(darkWindow) == rocketplot::Theme::dark());
}

TEST_F(PlotThemeTest, ApplyGivesThePlotTheThemeForGood)
{
    rocketplot::PlotWidget plot;
    const QPalette         dark = Palette::dark();
    PlotTheme::apply(plot, dark);
    EXPECT_EQ(plot.themeMode(), rocketplot::ThemeMode::CUSTOM);
    EXPECT_TRUE(plot.theme() == PlotTheme::forPalette(dark));
    // rocketplot no longer follows the plot's palette itself.
    plot.setPalette(Palette::light());
    EXPECT_TRUE(plot.theme() == PlotTheme::forPalette(dark));
    PlotTheme::apply(plot, plot.palette());
    EXPECT_TRUE(plot.theme() == PlotTheme::forPalette(Palette::light()));
}

}  // namespace

}  // namespace QtRocket::Gui
