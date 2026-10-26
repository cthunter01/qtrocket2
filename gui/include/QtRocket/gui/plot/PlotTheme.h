#pragma once

#include <rocketplot/PlotWidget.h>
#include <rocketplot/Theme.h>

class QPalette;

namespace QtRocket::Gui
{

/// The light or dark look of QtRocket's plots, decided by the rule that decides the plots' series
/// colours (PlotColors::curveColor(): ThemeColors::theme() of the palette), so that a plot and
/// its curves always agree, as OpenRocket takes both from one flag (UITheme.isLightTheme():
/// ThrustCurveMotorSelectionPanel.java:740-747, MotorInformationPanel.java:275-288).
/// rocketplot's own ThemeMode::SYSTEM decides by the lightness of the palette's Base colour
/// instead, which a palette whose window and view colours disagree (a light window around dark
/// views, or the reverse) sets apart from ThemeColors' window text against window rule.
///
/// forPalette() is rocketplot's light or dark theme as ThemeColors::theme() says, drawn on the
/// palette's Base colour (rocketplot's SYSTEM look: the plot sits on Base like a view, its legend
/// on Base with the theme's translucency) while Base is light in a light theme and dark in a dark
/// one (rocketplot's lightness 0.5); a Base that is not keeps the theme's own background, so that
/// the theme's text stays readable on it.
class PlotTheme
{
public:
    PlotTheme() = delete;

    /// The theme of a plot in @p palette (its Active colours): see the class comment.
    [[nodiscard]] static rocketplot::Theme forPalette(const QPalette& palette);

    /// Gives @p plot forPalette(@p palette) (ThemeMode::CUSTOM: rocketplot no longer follows the
    /// palette itself). Call it when the plot is made and on every PaletteChange of the widget
    /// that holds it, with that widget's palette (the plot's own palette follows its parent's only
    /// after the parent's PaletteChange).
    static void apply(rocketplot::PlotWidget& plot, const QPalette& palette);
};

}  // namespace QtRocket::Gui
