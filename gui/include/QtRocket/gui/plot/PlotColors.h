#pragma once

#include <QColor>

#include "QtRocket/gui/util/ThemeColors.h"

class QPalette;

namespace QtRocket::Gui
{

/// OpenRocket's plot colours, which its plots give out to their series in turn: Util.PLOT_COLORS
/// and Util.getPlotColor() (swing gui/plot/Util.java:44-55, 88-90), used as they are by the
/// simulation plots (Milestone 4), and ThrustCurveMotorSelectionPanel.getColor()
/// (ThrustCurveMotorSelectionPanel.java:740-747), which brightens them on a dark theme for the
/// thrust curves of the motor chooser: its curve combo, the preview and the thrust-curve dialog.
///
/// OpenRocket asks UITheme.isLightTheme() of its chosen theme; QtRocket follows the platform's
/// light or dark palette as ThemeColors::theme() tells them apart (the dark rule of every
/// OpenRocket colour the GUI paints with, so that the plots, the combo and the figure always
/// agree). Read the colours when painting or when the palette changes, never once for good.
class PlotColors
{
public:
    PlotColors() = delete;

    /// The number of plot colours (Util.PLOT_COLORS.length): plotColor() repeats after it.
    static constexpr int kCount = 10;

    /// Util.getPlotColor(@p index): colour @p index % kCount of Util.PLOT_COLORS, opaque. A
    /// negative @p index is a BugError (Java's array access throws for it).
    [[nodiscard]] static QColor plotColor(int index);

    /// ThrustCurveMotorSelectionPanel.getColor(@p index) in @p theme: plotColor(@p index), and on
    /// the DARK theme twice javaBrighter() of it (Color.brighter().brighter()).
    [[nodiscard]] static QColor curveColor(int index, ThemeColors::Theme theme);

    /// curveColor() in the theme of @p palette (ThemeColors::theme()).
    [[nodiscard]] static QColor curveColor(int index, const QPalette& palette);

    /// curveColor() in the theme of the application's palette (ThemeColors::currentTheme()), read
    /// now.
    [[nodiscard]] static QColor curveColor(int index);

    /// java.awt.Color.brighter() (JDK 17, as its documentation and its results describe it): each
    /// of red, green and blue divided by 0.7 and truncated, at most 255, after a channel between 0
    /// and 3 (both excluded) is raised to 3; black becomes (3, 3, 3). The alpha is kept. An invalid
    /// @p color (no Java colour is one) is a BugError.
    [[nodiscard]] static QColor javaBrighter(const QColor& color);
};

}  // namespace QtRocket::Gui
