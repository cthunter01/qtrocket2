#include "QtRocket/gui/plot/PlotColors.h"

#include <array>
#include <cstddef>

#include <QColor>
#include <QPalette>
#include <QRgb>
#include <QString>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/util/BugError.h"
#include "ScopedApplicationPalette.h"

// OpenRocket has no JUnit test of its plot colours. The expected values are Util.PLOT_COLORS
// (swing gui/plot/Util.java:44-55) and what OpenRocket's compiled classes print under JDK 17:
// ThrustCurveMotorSelectionPanel.getColor(i) with its dark theme (FlatDarcula) set, and
// new java.awt.Color(r, g, b, a).brighter().

namespace QtRocket::Gui
{

namespace
{

using PlotColorsTest = Test::GuiTest;
using Theme          = ThemeColors::Theme;
using Palette        = Test::ScopedApplicationPalette;

/// A colour for each of the indexes 0 to PlotColors::kCount - 1 (0xAARRGGBB).
using Colors = std::array<QRgb, PlotColors::kCount>;

/// Util.PLOT_COLORS.
constexpr Colors kPlotColors{
    0xff0072bd, 0xffd95319, 0xffedb120, 0xff7e318e, 0xff77ac30,
    0xff4dbeee, 0xffa2142f, 0xffc56a7a, 0xffff7f50, 0xff556b2f,
};

/// ThrustCurveMotorSelectionPanel.getColor(0) to getColor(9) on OpenRocket's dark theme.
constexpr Colors kDarkCurveColors{
    0xff00e7ff, 0xffffa832, 0xffffff40, 0xffff64ff, 0xfff2ff61,
    0xff9dffff, 0xffff285f, 0xffffd7f8, 0xffffffa2, 0xffacd95f,
};

/// The colours of @p actual that differ from @p expected, as "index: #aarrggbb" lines (empty when
/// they all agree).
QString differences(const Colors& expected, const Colors& actual)
{
    QString lines;
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        if (actual.at(index) != expected.at(index))
        {
            lines += QStringLiteral("%1: %2\n")
                         .arg(index)
                         .arg(QColor::fromRgba(actual.at(index)).name(QColor::HexArgb));
        }
    }
    return lines;
}

/// plotColor() of @p first to @p first + kCount - 1.
Colors plotColors(int first)
{
    Colors colors{};
    for (std::size_t index = 0; index < colors.size(); ++index)
    {
        colors.at(index) = PlotColors::plotColor(first + static_cast<int>(index)).rgba();
    }
    return colors;
}

/// curveColor() of 0 to kCount - 1 in @p theme.
Colors curveColors(Theme theme)
{
    Colors colors{};
    for (std::size_t index = 0; index < colors.size(); ++index)
    {
        colors.at(index) = PlotColors::curveColor(static_cast<int>(index), theme).rgba();
    }
    return colors;
}

/// curveColor() of 0 to kCount - 1 for @p palette.
Colors curveColors(const QPalette& palette)
{
    Colors colors{};
    for (std::size_t index = 0; index < colors.size(); ++index)
    {
        colors.at(index) = PlotColors::curveColor(static_cast<int>(index), palette).rgba();
    }
    return colors;
}

/// curveColor() of 0 to kCount - 1 for the application's palette.
Colors applicationCurveColors()
{
    Colors colors{};
    for (std::size_t index = 0; index < colors.size(); ++index)
    {
        colors.at(index) = PlotColors::curveColor(static_cast<int>(index)).rgba();
    }
    return colors;
}

/// The red, green, blue and alpha of a colour.
struct Rgba
{
    int red;
    int green;
    int blue;
    int alpha;
};

/// A colour, its brighter() and its brighter().brighter() in JDK 17.
struct BrighterCase
{
    Rgba color;
    Rgba brighter;
    Rgba twiceBrighter;
};

constexpr Rgba rgba(int red, int green, int blue, int alpha)
{
    return {.red = red, .green = green, .blue = blue, .alpha = alpha};
}

constexpr BrighterCase brighterCase(Rgba color, Rgba brighter, Rgba twiceBrighter)
{
    return {.color = color, .brighter = brighter, .twiceBrighter = twiceBrighter};
}

// JDK 17's new java.awt.Color(r, g, b, a), its brighter() and its brighter().brighter().
constexpr std::array kBrighterCases{
    brighterCase(rgba(0, 0, 0, 255), rgba(3, 3, 3, 255), rgba(4, 4, 4, 255)),
    brighterCase(rgba(0, 0, 0, 0), rgba(3, 3, 3, 0), rgba(4, 4, 4, 0)),
    brighterCase(rgba(0, 0, 0, 77), rgba(3, 3, 3, 77), rgba(4, 4, 4, 77)),
    brighterCase(rgba(1, 0, 0, 255), rgba(4, 0, 0, 255), rgba(5, 0, 0, 255)),
    brighterCase(rgba(0, 2, 0, 255), rgba(0, 4, 0, 255), rgba(0, 5, 0, 255)),
    brighterCase(rgba(0, 0, 3, 255), rgba(0, 0, 4, 255), rgba(0, 0, 5, 255)),
    brighterCase(rgba(1, 2, 3, 10), rgba(4, 4, 4, 10), rgba(5, 5, 5, 10)),
    brighterCase(rgba(4, 5, 6, 255), rgba(5, 7, 8, 255), rgba(7, 10, 11, 255)),
    brighterCase(rgba(178, 179, 180, 255), rgba(254, 255, 255, 255), rgba(255, 255, 255, 255)),
    brighterCase(rgba(255, 255, 255, 255), rgba(255, 255, 255, 255), rgba(255, 255, 255, 255)),
    brighterCase(rgba(0, 114, 189, 255), rgba(0, 162, 255, 255), rgba(0, 231, 255, 255)),
    brighterCase(rgba(7, 0, 254, 200), rgba(10, 0, 255, 200), rgba(14, 0, 255, 200)),
    brighterCase(rgba(100, 0, 0, 255), rgba(142, 0, 0, 255), rgba(202, 0, 0, 255)),
    brighterCase(rgba(2, 255, 1, 1), rgba(4, 255, 4, 1), rgba(5, 255, 5, 1)),
};

QColor toQColor(const Rgba& color)
{
    return QColor::fromRgb(color.red, color.green, color.blue, color.alpha);
}

/// The cases of kBrighterCases that javaBrighter() gets wrong, as "#aarrggbb -> once, twice"
/// lines (empty when it gets them all right).
QString brighterDifferences()
{
    QString lines;
    for (const BrighterCase& expected : kBrighterCases)
    {
        const QColor color = toQColor(expected.color);
        const QColor once  = PlotColors::javaBrighter(color);
        const QColor twice = PlotColors::javaBrighter(once);
        if (once.rgba() != toQColor(expected.brighter).rgba() ||
            twice.rgba() != toQColor(expected.twiceBrighter).rgba())
        {
            lines += QStringLiteral("%1 -> %2, %3\n")
                         .arg(color.name(QColor::HexArgb), once.name(QColor::HexArgb),
                              twice.name(QColor::HexArgb));
        }
    }
    return lines;
}

TEST_F(PlotColorsTest, ThePlotColorsAreOpenRocketsTenInTurn)
{
    EXPECT_EQ(differences(kPlotColors, plotColors(0)), QString());
    // Util.getPlotColor(index) takes index % 10.
    EXPECT_EQ(differences(kPlotColors, plotColors(PlotColors::kCount)), QString());
    EXPECT_EQ(PlotColors::plotColor(1234), PlotColors::plotColor(4));
}

TEST_F(PlotColorsTest, ANegativeIndexIsABugError)
{
    // Java's PLOT_COLORS[index % 10] throws an ArrayIndexOutOfBoundsException.
    EXPECT_THROW(static_cast<void>(PlotColors::plotColor(-1)), BugError);
    EXPECT_THROW(static_cast<void>(PlotColors::curveColor(-10, Theme::LIGHT)), BugError);
}

TEST_F(PlotColorsTest, TheCurveColorsAreThePlotColorsOnTheLightTheme)
{
    EXPECT_EQ(differences(kPlotColors, curveColors(Theme::LIGHT)), QString());
}

TEST_F(PlotColorsTest, TheCurveColorsAreOpenRocketsBrightenedOnTheDarkTheme)
{
    EXPECT_EQ(differences(kDarkCurveColors, curveColors(Theme::DARK)), QString());
    EXPECT_EQ(PlotColors::curveColor(17, Theme::DARK).rgba(), kDarkCurveColors.at(7));
}

TEST_F(PlotColorsTest, TheCurveColorsFollowThePalettesTheme)
{
    EXPECT_EQ(differences(kPlotColors, curveColors(Palette::light())), QString());
    EXPECT_EQ(differences(kDarkCurveColors, curveColors(Palette::dark())), QString());
    {
        const Palette dark(Palette::dark());
        EXPECT_EQ(differences(kDarkCurveColors, applicationCurveColors()), QString());
    }
    const Palette light(Palette::light());
    EXPECT_EQ(differences(kPlotColors, applicationCurveColors()), QString());
}

TEST_F(PlotColorsTest, JavaBrighterIsJavasColorBrighter)
{
    EXPECT_EQ(brighterDifferences(), QString());
}

TEST_F(PlotColorsTest, JavaBrighterKeepsTheAlphaAndRefusesAnInvalidColor)
{
    EXPECT_EQ(PlotColors::javaBrighter(QColor::fromRgb(100, 0, 0, 33)).alpha(), 33);
    EXPECT_EQ(PlotColors::javaBrighter(QColor::fromHsv(0, 255, 100)), QColor::fromRgb(142, 0, 0));
    EXPECT_THROW(static_cast<void>(PlotColors::javaBrighter(QColor())), BugError);
}

}  // namespace

}  // namespace QtRocket::Gui
