#include "QtRocket/gui/plot/PlotTheme.h"

#include <QPalette>

#include <rocketplot/PlotWidget.h>
#include <rocketplot/Theme.h>

#include "QtRocket/gui/util/ThemeColors.h"

namespace QtRocket::Gui
{

bool PlotTheme::isDark(const QPalette& palette)
{
    return ThemeColors::theme(palette) == ThemeColors::Theme::DARK;
}

rocketplot::Theme PlotTheme::forPalette(const QPalette& palette)
{
    return rocketplot::Theme::forPalette(palette, isDark(palette));
}

void PlotTheme::installRule()
{
    rocketplot::PlotWidget::setDarkPaletteRule(&PlotTheme::isDark);
}

}  // namespace QtRocket::Gui
