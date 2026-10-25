#include "QtRocket/gui/util/Icons.h"

#include <algorithm>
#include <array>
#include <format>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <QByteArray>
#include <QColor>
#include <QFile>
#include <QFont>
#include <QIODevice>
#include <QIcon>
#include <QImage>
#include <QList>
#include <QPalette>
#include <QRgb>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "ScopedApplicationPalette.h"

namespace QtRocket::Gui
{

namespace
{

using IconsTest = Test::GuiTest;
using Id        = Icons::Id;
using Key       = ThemeColors::Key;
using Theme     = ThemeColors::Theme;
using Palette   = Test::ScopedApplicationPalette;

constexpr QRgb kRgbMask = 0x00ffffff;

/// The RGB of every opaque pixel of @p id drawn at 64 pixels.
std::set<QRgb> opaqueColors(Id id)
{
    const QImage image =
        Icons::icon(id).pixmap(QSize(64, 64)).toImage().convertToFormat(QImage::Format_ARGB32);
    std::set<QRgb> colors;
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            const QRgb pixel = image.pixel(x, y);
            if (qAlpha(pixel) == 255)
            {
                colors.insert(pixel & kRgbMask);
            }
        }
    }
    return colors;
}

QRgb rgbOf(const QColor& color)
{
    return color.rgb() & kRgbMask;
}

QRgb themeRgb(Key key, Theme theme)
{
    return rgbOf(ThemeColors::themeColor(key, theme));
}

/// The icons whose resource is missing, or which draw nothing at 32 pixels.
QStringList brokenIcons()
{
    QStringList broken;
    for (const Id id : Icons::kAllIds)
    {
        const QIcon icon = Icons::icon(id);
        if (!QFile::exists(Icons::resourcePath(id)) || icon.isNull() || opaqueColors(id).empty())
        {
            broken.append(Icons::name(id));
        }
    }
    return broken;
}

using ColorMap = std::vector<std::pair<QRgb, Key>>;

/// An icon's colour map as Icons.java writes it (SVG_THEME_COLOR_RGB is 0x000000, and
/// SVG_DEFAULT_COLOR_KEY OR.icons.default): loadIcon(svg, png, name) maps black to the default,
/// loadIcon(..., colorKey) black to colorKey, and a Map.of() what it names.
struct IconColors
{
    Id       id;
    ColorMap colors;
};

/// Icons.java's colour map of every icon of the GUI.
[[nodiscard]] std::vector<IconColors> iconsJavaColorMaps()
{
    const auto plain = [](Id id) {
        return IconColors{.id = id, .colors = {{0x000000, Key::ICONS_DEFAULT}}};
    };
    const auto single = [](Id id, Key key) {
        return IconColors{.id = id, .colors = {{0x000000, key}}};
    };
    const auto tinted = [](Id id, QRgb rgb, Key key) {
        return IconColors{.id = id, .colors = {{0x000000, Key::ICONS_DEFAULT}, {rgb, key}}};
    };
    const ColorMap magnet{{0xE86B55, Key::ICONS_MAGNET_RED}, {0xE1E3E2, Key::ICONS_MAGNET_WHITE}};
    return {
        tinted(Id::FILE_NEW, 0x499C54, Key::ICONS_PLUS),
        plain(Id::FILE_OPEN),
        tinted(Id::FILE_OPEN_RECENT, 0x2D2DBD, Key::ICONS_TINT_COLOR),
        tinted(Id::FILE_OPEN_EXAMPLE, 0x2D2DBD, Key::ICONS_TINT_COLOR),
        plain(Id::FILE_SAVE),
        tinted(Id::FILE_SAVE_AS, 0xFAC132, Key::ICONS_PENCIL),
        tinted(Id::FILE_CLOSE, 0xC80000, Key::RED),
        single(Id::FILE_QUIT, Key::RED),
        single(Id::EDIT_UNDO, Key::ICONS_UNDO),
        single(Id::EDIT_REDO, Key::ICONS_REDO),
        tinted(Id::EDIT_EDIT, 0xFAC132, Key::ICONS_PENCIL),
        tinted(Id::EDIT_RENAME, 0x2D2DBD, Key::ICONS_TINT_COLOR),
        plain(Id::EDIT_CUT),
        plain(Id::EDIT_COPY),
        tinted(Id::EDIT_PASTE, 0xC80000, Key::ICONS_IMPORT),
        tinted(Id::EDIT_DUPLICATE, 0x2D2DBD, Key::ICONS_DUPLICATE),
        single(Id::EDIT_DELETE, Key::ICONS_DELETE),
        single(Id::HELP_ABOUT, Key::ICONS_HELP),
        tinted(Id::ZOOM_IN, 0x499C54, Key::ICONS_ZOOM_IN),
        tinted(Id::ZOOM_OUT, 0xC80000, Key::ICONS_ZOOM_OUT),
        tinted(Id::ZOOM_RESET, 0x2D2DBD, Key::ICONS_ZOOM_FIT),
        single(Id::PAN_VIEW, Key::PURPLE),
        tinted(Id::RULER, 0xFAC132, Key::RULER),
        IconColors{.id = Id::SNAP_CLICK, .colors = magnet},
        IconColors{.id = Id::SNAP, .colors = magnet},
        plain(Id::PREFERENCES),
        tinted(Id::CONFIGURE, 0x2D2DBD, Key::ICONS_TINT_COLOR),
        plain(Id::HELP),
        single(Id::UP, Key::ICONS_MOVE_UP),
        single(Id::DOWN, Key::ICONS_MOVE_DOWN),
        plain(Id::RESET),
        single(Id::MASS_OVERRIDE, Key::ICONS_OVERRIDE),
        single(Id::MASS_OVERRIDE_SUBCOMPONENT, Key::ICONS_OVERRIDE_SUBCOMPONENT),
        single(Id::CG_OVERRIDE, Key::ICONS_OVERRIDE),
        single(Id::CG_OVERRIDE_SUBCOMPONENT, Key::ICONS_OVERRIDE_SUBCOMPONENT),
        single(Id::CD_OVERRIDE, Key::ICONS_OVERRIDE),
        single(Id::CD_OVERRIDE_SUBCOMPONENT, Key::ICONS_OVERRIDE_SUBCOMPONENT),
        plain(Id::COMPONENT_HIDDEN),
        plain(Id::COMPONENT_SHOWING),
        plain(Id::COMPONENT_DISABLED),
        tinted(Id::COMPONENT_ENABLED, 0x499C54, Key::GREEN),
        tinted(Id::LOCKED, 0xF1C066, Key::ICONS_LOCKED),
        plain(Id::UNLOCKED),
    };
}

/// @p colors in a fixed order (Icons.java's Map.of() has none).
[[nodiscard]] ColorMap sorted(ColorMap colors)
{
    std::ranges::sort(colors);
    return colors;
}

/// The icons whose colour map is not Icons.java's, by name.
[[nodiscard]] QStringList iconsWithAnotherColorMap()
{
    QStringList different;
    for (const IconColors& expected : iconsJavaColorMaps())
    {
        if (sorted(Icons::colorMap(expected.id)) != sorted(expected.colors))
        {
            different.append(Icons::name(expected.id));
        }
    }
    return different;
}

/// Whether the SVG @p svg writes the colour @p rgb, as #rrggbb (in either case) or rgb(r,g,b).
[[nodiscard]] bool writesColor(const QByteArray& svg, QRgb rgb)
{
    const std::string hex      = std::format("#{:06x}", rgb & kRgbMask);
    const std::string function = std::format("rgb({},{},{})", qRed(rgb), qGreen(rgb), qBlue(rgb));
    QByteArray        compact  = svg.toLower();
    compact.replace(' ', QByteArray());
    return compact.contains(QByteArrayView(hex)) || compact.contains(QByteArrayView(function));
}

/// "<icon> <rgb>" for each placeholder colour of a colour map (other than black, which an SVG
/// may also write as black, currentColor or no fill at all) that its SVG does not write, which
/// would leave the colour as it is drawn.
[[nodiscard]] QStringList placeholdersMissingFromTheirSvg()
{
    QStringList missing;
    for (const Id id : Icons::kAllIds)
    {
        QFile file(Icons::resourcePath(id));
        if (!file.open(QIODevice::ReadOnly))
        {
            missing.append(Icons::name(id) + QStringLiteral(" (no SVG)"));
            continue;
        }
        const QByteArray svg = file.readAll();
        for (const std::pair<QRgb, Key>& placeholder : Icons::colorMap(id))
        {
            const QRgb rgb = placeholder.first;
            if (rgb != 0x000000 && !writesColor(svg, rgb))
            {
                missing.append(Icons::name(id) + QStringLiteral(" ") + QString::number(rgb, 16));
            }
        }
    }
    return missing;
}

TEST_F(IconsTest, EveryIconHasTheColourMapOfIconsJava)
{
    EXPECT_EQ(iconsJavaColorMaps().size(), Icons::kAllIds.size());
    EXPECT_EQ(iconsWithAnotherColorMap(), QStringList());
}

TEST_F(IconsTest, EveryPlaceholderColourIsInItsSvg)
{
    EXPECT_EQ(placeholdersMissingFromTheirSvg(), QStringList());
}

TEST_F(IconsTest, EveryIconOfTheGuiLoadsAndDraws)
{
    EXPECT_EQ(brokenIcons(), QStringList());
}

TEST_F(IconsTest, NamesTheResourcesInLowerCase)
{
    // Icons.java asks for cd-override.svg; OpenRocket's file is CD-override.svg.
    EXPECT_EQ(Icons::resourcePath(Id::CD_OVERRIDE),
              QStringLiteral(":/icons/actions/cd-override.svg"));
    EXPECT_EQ(Icons::resourcePath(Id::FILE_NEW),
              QStringLiteral(":/icons/actions/file-plus-corner.svg"));
    for (const Id id : Icons::kAllIds)
    {
        EXPECT_EQ(Icons::resourcePath(id), Icons::resourcePath(id).toLower());
    }
}

TEST_F(IconsTest, BlackIsTheDefaultIconColourOfThePalette)
{
    const Palette light(Palette::light());
    const QRgb    text = rgbOf(Palette::light().color(QPalette::WindowText));
    EXPECT_EQ(opaqueColors(Id::EDIT_COPY), std::set<QRgb>{text});
    EXPECT_EQ(opaqueColors(Id::FILE_SAVE), std::set<QRgb>{text});
}

TEST_F(IconsTest, TheOtherPlaceholdersAreTheirThemeKeys)
{
    const Palette light(Palette::light());
    const QRgb    text = rgbOf(Palette::light().color(QPalette::WindowText));
    // file-plus-corner.svg: black and OpenRocket's green #499c54 (OR.icons.plus).
    EXPECT_EQ(opaqueColors(Id::FILE_NEW),
              (std::set<QRgb>{text, themeRgb(Key::ICONS_PLUS, Theme::LIGHT)}));
    // rename.svg: black and OpenRocket's blue #2d2dbd (OR.icons.tintColor).
    EXPECT_EQ(opaqueColors(Id::EDIT_RENAME),
              (std::set<QRgb>{text, themeRgb(Key::ICONS_TINT_COLOR, Theme::LIGHT)}));
    // zoom-fit.svg writes its blue as rgb(45,45,189) (OR.icons.zoomFit).
    EXPECT_EQ(opaqueColors(Id::ZOOM_RESET),
              (std::set<QRgb>{text, themeRgb(Key::ICONS_ZOOM_FIT, Theme::LIGHT)}));
    // power.svg: its black is OR.colors.red.
    EXPECT_EQ(opaqueColors(Id::FILE_QUIT), std::set<QRgb>{themeRgb(Key::RED, Theme::LIGHT)});
    // move.svg strokes in currentColor, which is black: OR.colors.purple.
    EXPECT_EQ(opaqueColors(Id::PAN_VIEW), std::set<QRgb>{themeRgb(Key::PURPLE, Theme::LIGHT)});
}

TEST_F(IconsTest, AShapeWithoutAFillIsFilledInTheBlacksColour)
{
    // cg-override.svg fills two quadrants without a fill attribute (SVG's initial black).
    const Palette light(Palette::light());
    EXPECT_EQ(opaqueColors(Id::CG_OVERRIDE),
              std::set<QRgb>{themeRgb(Key::ICONS_OVERRIDE, Theme::LIGHT)});
    EXPECT_EQ(opaqueColors(Id::CG_OVERRIDE_SUBCOMPONENT),
              std::set<QRgb>{themeRgb(Key::ICONS_OVERRIDE_SUBCOMPONENT, Theme::LIGHT)});
    EXPECT_EQ(opaqueColors(Id::CD_OVERRIDE),
              std::set<QRgb>{themeRgb(Key::ICONS_OVERRIDE, Theme::LIGHT)});
}

TEST_F(IconsTest, TheMagnetsKeepTheirBlackAndMapTheirRedAndWhite)
{
    const Palette light(Palette::light());
    EXPECT_EQ(opaqueColors(Id::SNAP),
              (std::set<QRgb>{themeRgb(Key::ICONS_MAGNET_RED, Theme::LIGHT),
                              themeRgb(Key::ICONS_MAGNET_WHITE, Theme::LIGHT)}));
    const std::set<QRgb> click = opaqueColors(Id::SNAP_CLICK);
    EXPECT_TRUE(click.contains(0x000000));
    EXPECT_TRUE(click.contains(themeRgb(Key::ICONS_MAGNET_RED, Theme::LIGHT)));
}

TEST_F(IconsTest, ADarkPaletteTakesTheDarkThemesColours)
{
    const Palette dark(Palette::dark());
    const QRgb    text = rgbOf(Palette::dark().color(QPalette::WindowText));
    EXPECT_EQ(opaqueColors(Id::EDIT_UNDO), std::set<QRgb>{themeRgb(Key::ICONS_UNDO, Theme::DARK)});
    EXPECT_EQ(opaqueColors(Id::EDIT_DUPLICATE),
              (std::set<QRgb>{text, themeRgb(Key::ICONS_DUPLICATE, Theme::DARK)}));
    EXPECT_EQ(opaqueColors(Id::COMPONENT_ENABLED),
              (std::set<QRgb>{text, themeRgb(Key::GREEN, Theme::DARK)}));
}

TEST_F(IconsTest, TheSubcomponentMarkersAreThreeQuartersTheSize)
{
    EXPECT_DOUBLE_EQ(Icons::scaleMultiplier(Id::MASS_OVERRIDE), 1.0);
    EXPECT_DOUBLE_EQ(Icons::scaleMultiplier(Id::MASS_OVERRIDE_SUBCOMPONENT), 0.75);
    EXPECT_DOUBLE_EQ(Icons::scaleMultiplier(Id::CD_OVERRIDE_SUBCOMPONENT), 0.75);
    QFont font;
    font.setPixelSize(16);
    EXPECT_EQ(Icons::iconHeight(Id::MASS_OVERRIDE, font), 20);
    EXPECT_EQ(Icons::iconHeight(Id::MASS_OVERRIDE_SUBCOMPONENT, font), 15);
}

TEST_F(IconsTest, TheWindowIconHasTheSizesOfOpenRocketsFrames)
{
    // GUIUtil loads icon-256, -064, -048, -032 and -016.png; there is no icon-032.png.
    EXPECT_EQ(Icons::windowIconResources(),
              (QStringList{QStringLiteral(":/icons/app/icon-016.png"),
                           QStringLiteral(":/icons/app/icon-048.png"),
                           QStringLiteral(":/icons/app/icon-064.png"),
                           QStringLiteral(":/icons/app/icon-256.png")}));
    const QIcon icon = Icons::windowIcon();
    EXPECT_FALSE(icon.isNull());
    EXPECT_EQ(icon.availableSizes(),
              (QList<QSize>{QSize(16, 16), QSize(48, 48), QSize(64, 64), QSize(256, 256)}));
}

TEST_F(IconsTest, NamesEachIconAsIconsJava)
{
    EXPECT_EQ(Icons::name(Id::FILE_NEW), QStringLiteral("FILE_NEW"));
    EXPECT_EQ(Icons::name(Id::CD_OVERRIDE_SUBCOMPONENT),
              QStringLiteral("CD_OVERRIDE_SUBCOMPONENT"));
    EXPECT_EQ(Icons::name(Id::EDIT_RENAME), QStringLiteral("EDIT_RENAME"));
    EXPECT_EQ(Icons::kAllIds.size(), 43U);
}

}  // namespace

}  // namespace QtRocket::Gui
