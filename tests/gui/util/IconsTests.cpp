#include "QtRocket/gui/util/Icons.h"

#include <set>

#include <QColor>
#include <QFile>
#include <QFont>
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

TEST_F(IconsTest, TheWindowIconHasOpenRocketsFiveSizes)
{
    EXPECT_EQ(
        Icons::windowIconResources(),
        (QStringList{
            QStringLiteral(":/icons/app/icon-016.png"), QStringLiteral(":/icons/app/icon-048.png"),
            QStringLiteral(":/icons/app/icon-064.png"), QStringLiteral(":/icons/app/icon-128.png"),
            QStringLiteral(":/icons/app/icon-256.png")}));
    const QIcon icon = Icons::windowIcon();
    EXPECT_FALSE(icon.isNull());
    EXPECT_EQ(icon.availableSizes(), (QList<QSize>{QSize(16, 16), QSize(48, 48), QSize(64, 64),
                                                   QSize(128, 128), QSize(256, 256)}));
}

TEST_F(IconsTest, NamesEachIconAsIconsJava)
{
    EXPECT_EQ(Icons::name(Id::FILE_NEW), QStringLiteral("FILE_NEW"));
    EXPECT_EQ(Icons::name(Id::CD_OVERRIDE_SUBCOMPONENT),
              QStringLiteral("CD_OVERRIDE_SUBCOMPONENT"));
    EXPECT_EQ(Icons::kAllIds.size(), 42U);
}

}  // namespace

}  // namespace QtRocket::Gui
