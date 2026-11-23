#pragma once

#include <rocketplot/Theme.h>

class QPalette;

namespace QtRocket::Gui
{

/// The light or dark look of QtRocket's plots, decided by the rule that decides the plots' series
/// colours (PlotColors::curveColor(): ThemeColors::theme() of the palette), so that a plot and
/// its curves always agree, as OpenRocket takes both from one flag (UITheme.isLightTheme():
/// ThrustCurveMotorSelectionPanel.java:740-747, MotorInformationPanel.java:275-288).
/// rocketplot's own default rule decides by the lightness of the palette's Base colour instead,
/// which a palette whose window and view colours disagree (a light window around dark views, or
/// the reverse) sets apart from ThemeColors' window text against window rule.
///
/// installRule() makes isDark() rocketplot's rule (PlotWidget::setDarkPaletteRule()) for every
/// plot of the process; Application's constructor calls it, in the program and in the GUI tests.
/// So a plot stays in rocketplot's ThemeMode::SYSTEM (its default) and follows its palette by
/// itself, also when the palette changes: it shows forPalette(), rocketplot's light or dark theme
/// as isDark() says, drawn on the palette's Base colour (the plot sits on Base like a view, its
/// legend on Base with the theme's translucency) while Base is light in a light theme and dark in
/// a dark one (lightness 0.5); a Base that is not keeps the theme's own background, so that the
/// theme's text stays readable on it (rocketplot::Theme::forPalette()). The rule reads only the
/// palette it is given: it holds no state of its own.
class PlotTheme
{
public:
    PlotTheme() = delete;

    /// Whether a plot in @p palette (its Active colours) is dark: ThemeColors::theme() of it is
    /// DARK.
    [[nodiscard]] static bool isDark(const QPalette& palette);

    /// The theme of a plot in @p palette once installRule() has run: rocketplot::Theme::
    /// forPalette(@p palette, isDark(@p palette)).
    [[nodiscard]] static rocketplot::Theme forPalette(const QPalette& palette);

    /// Makes isDark() the rule of every plot in ThemeMode::SYSTEM, those there are and those to
    /// come, until the application object is destroyed (rocketplot::PlotWidget::
    /// setDarkPaletteRule()). Call it on the GUI thread once the QApplication exists.
    static void installRule();
};

}  // namespace QtRocket::Gui
