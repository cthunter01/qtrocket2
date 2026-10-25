#include "QtRocket/gui/plot/PlotColors.h"

#include <algorithm>
#include <array>
#include <cstddef>

#include <QColor>
#include <QPalette>

#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// A colour of Util.PLOT_COLORS (new Color(r, g, b): opaque).
struct Rgb
{
    int red;
    int green;
    int blue;
};

constexpr Rgb rgb(int red, int green, int blue)
{
    return {.red = red, .green = green, .blue = blue};
}

// Util.PLOT_COLORS (gui/plot/Util.java:44-55), in its order.
constexpr std::array<Rgb, PlotColors::kCount> kPlotColors{
    rgb(0, 114, 189),  rgb(217, 83, 25), rgb(237, 177, 32),  rgb(126, 49, 142), rgb(119, 172, 48),
    rgb(77, 190, 238), rgb(162, 20, 47), rgb(197, 106, 122), rgb(255, 127, 80), rgb(85, 107, 47),
};

// java.awt.Color's factor of brighter() and darker(), and the least value brighter() gives a
// channel that is not 0 (the integer part of 1 / (1 - 0.7)).
constexpr double kBrighterFactor  = 0.7;
constexpr int    kBrighterMinimum = 3;
constexpr int    kMaxChannel      = 255;

/// One channel of javaBrighter(): raised to kBrighterMinimum when it is above 0 but below it, then
/// divided by the factor, truncated and kept at most 255.
[[nodiscard]] int brighterChannel(int value)
{
    const int raised = value > 0 && value < kBrighterMinimum ? kBrighterMinimum : value;
    return std::min(static_cast<int>(raised / kBrighterFactor), kMaxChannel);
}

}  // namespace

QColor PlotColors::plotColor(int index)
{
    QTROCKET_ASSERT(index >= 0);
    const Rgb& color = kPlotColors.at(static_cast<std::size_t>(index % kCount));
    return QColor::fromRgb(color.red, color.green, color.blue);
}

QColor PlotColors::curveColor(int index, ThemeColors::Theme theme)
{
    const QColor color = plotColor(index);
    if (theme == ThemeColors::Theme::LIGHT)
    {
        return color;
    }
    return javaBrighter(javaBrighter(color));
}

QColor PlotColors::curveColor(int index, const QPalette& palette)
{
    return curveColor(index, ThemeColors::theme(palette));
}

QColor PlotColors::curveColor(int index)
{
    return curveColor(index, ThemeColors::currentTheme());
}

QColor PlotColors::javaBrighter(const QColor& color)
{
    QTROCKET_ASSERT(color.isValid());
    const QColor rgbColor = color.toRgb();
    if (rgbColor.red() == 0 && rgbColor.green() == 0 && rgbColor.blue() == 0)
    {
        return QColor::fromRgb(kBrighterMinimum, kBrighterMinimum, kBrighterMinimum,
                               rgbColor.alpha());
    }
    return QColor::fromRgb(brighterChannel(rgbColor.red()), brighterChannel(rgbColor.green()),
                           brighterChannel(rgbColor.blue()), rgbColor.alpha());
}

}  // namespace QtRocket::Gui
