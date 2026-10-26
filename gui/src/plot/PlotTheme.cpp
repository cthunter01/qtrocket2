#include "QtRocket/gui/plot/PlotTheme.h"

#include <QColor>
#include <QPalette>

#include <rocketplot/PlotWidget.h>
#include <rocketplot/Theme.h>

#include "QtRocket/gui/util/ThemeColors.h"

namespace QtRocket::Gui
{

namespace
{

/// rocketplot's SYSTEM rule: a Base colour of lightness below this is a dark one.
constexpr float kDarkLightness = 0.5F;

}  // namespace

rocketplot::Theme PlotTheme::forPalette(const QPalette& palette)
{
    const bool        dark  = ThemeColors::theme(palette) == ThemeColors::Theme::DARK;
    rocketplot::Theme theme = dark ? rocketplot::Theme::dark() : rocketplot::Theme::light();
    const QColor      base  = palette.color(QPalette::Active, QPalette::Base);
    if ((base.lightnessF() < kDarkLightness) == dark)
    {
        theme.background = base;
        theme.legendBackground =
            QColor(base.red(), base.green(), base.blue(), theme.legendBackground.alpha());
    }
    return theme;
}

void PlotTheme::apply(rocketplot::PlotWidget& plot, const QPalette& palette)
{
    plot.setTheme(forPalette(palette));
}

}  // namespace QtRocket::Gui
