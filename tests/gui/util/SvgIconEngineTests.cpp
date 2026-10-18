#include "QtRocket/gui/util/SvgIconEngine.h"

#include <set>
#include <utility>
#include <vector>

#include <QByteArray>
#include <QColor>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QRect>
#include <QRgb>
#include <QSize>
#include <QString>
#include <QSvgRenderer>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "ScopedApplicationPalette.h"

namespace QtRocket::Gui
{

namespace
{

using SvgIconEngineTest = Test::GuiTest;
using Key               = ThemeColors::Key;
using Theme             = ThemeColors::Theme;
using Palette           = Test::ScopedApplicationPalette;

constexpr QRgb kRgbMask = 0x00ffffff;

/// The RGB of every opaque pixel of @p image: the inside of the strokes and fills, where no
/// anti-aliasing mixes a colour with the background.
std::set<QRgb> opaqueColors(const QImage& image)
{
    const QImage   argb = image.convertToFormat(QImage::Format_ARGB32);
    std::set<QRgb> colors;
    for (int y = 0; y < argb.height(); ++y)
    {
        for (int x = 0; x < argb.width(); ++x)
        {
            const QRgb pixel = argb.pixel(x, y);
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

/// The undo icon: black is OR.icons.undo.
QIcon undoIcon()
{
    return QIcon(makeQtOwned<SvgIconEngine>(
        QStringLiteral(":/icons/actions/undo.svg"),
        std::vector<SvgIconEngine::Placeholder>{{0x000000, Key::ICONS_UNDO}}));
}

TEST_F(SvgIconEngineTest, RecoloursEveryWayAnSvgWritesAColour)
{
    const QByteArray svg(
        R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16">)svg"
        R"svg(<path d="M1 1h4" style="fill:none;stroke:#000;stroke-width:1px"/>)svg"
        R"svg(<path d="M1 2h4" stroke="#000000" fill="rgb(73,156,84)"/>)svg"
        R"svg(<path d="M1 3h4" stroke="black" fill="#499C54"/>)svg"
        R"svg(<path d="M1 4h4" stroke="currentColor" fill="white"/>)svg"
        R"svg(</svg>)svg");
    const std::vector<std::pair<QRgb, QColor>> colors{{0x000000, QColor(0xff, 0x00, 0x00)},
                                                      {0x499c54, QColor(0x00, 0x00, 0xff)}};
    const QString recolored = QString::fromUtf8(SvgIconEngine::recolored(svg, colors));
    EXPECT_TRUE(
        recolored.contains(QStringLiteral("style=\"fill:none;stroke:#ff0000;stroke-width:1px\"")))
        << recolored.toStdString();
    EXPECT_TRUE(recolored.contains(QStringLiteral("stroke=\"#ff0000\" fill=\"#0000ff\"")))
        << recolored.toStdString();
    EXPECT_EQ(recolored.count(QStringLiteral("#ff0000")), 4 + 1) << recolored.toStdString();
    EXPECT_EQ(recolored.count(QStringLiteral("#0000ff")), 2) << recolored.toStdString();
    // White is no placeholder; the root gets the black fill a shape without one has.
    EXPECT_TRUE(recolored.contains(QStringLiteral("fill=\"white\""))) << recolored.toStdString();
    EXPECT_TRUE(
        recolored.startsWith(QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" "
                                            "viewBox=\"0 0 16 16\" fill=\"#ff0000\">")))
        << recolored.toStdString();
}

TEST_F(SvgIconEngineTest, LeavesTheRootFillAloneWhenTheSvgSetsOne)
{
    const QByteArray svg(
        R"svg(<svg xmlns="http://www.w3.org/2000/svg" fill="none"><path/></svg>)svg");
    const QString recolored =
        QString::fromUtf8(SvgIconEngine::recolored(svg, {{0x000000, QColor(0xff, 0x00, 0x00)}}));
    EXPECT_EQ(recolored, QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" "
                                        "fill=\"none\"><path/></svg>"));
    // Without a black placeholder there is no fill to add.
    const QByteArray bare(R"svg(<svg xmlns="http://www.w3.org/2000/svg"><path/></svg>)svg");
    EXPECT_EQ(QString::fromUtf8(SvgIconEngine::recolored(bare, {{0x2d2dbd, QColor(Qt::red)}})),
              QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\"><path/></svg>"));
}

TEST_F(SvgIconEngineTest, LeavesABrokenSvgAsItIs)
{
    const QByteArray broken("<svg><path></svg>");
    EXPECT_EQ(SvgIconEngine::recolored(broken, {{0x000000, QColor(Qt::red)}}), broken);
}

TEST_F(SvgIconEngineTest, DrawsTheStrokesInTheColourOfTheKeyForThePalette)
{
    const QIcon icon = undoIcon();
    {
        const Palette light(Palette::light());
        EXPECT_EQ(opaqueColors(icon.pixmap(QSize(64, 64)).toImage()),
                  std::set<QRgb>{rgbOf(ThemeColors::themeColor(Key::ICONS_UNDO, Theme::LIGHT))});
    }
    {
        // The same icon, painted again with a dark palette: no new icon is needed.
        const Palette dark(Palette::dark());
        EXPECT_EQ(opaqueColors(icon.pixmap(QSize(64, 64)).toImage()),
                  std::set<QRgb>{rgbOf(ThemeColors::themeColor(Key::ICONS_UNDO, Theme::DARK))});
    }
}

TEST_F(SvgIconEngineTest, PaintsIntoAPainterAtTheDevicesPixelRatio)
{
    const Palette light(Palette::light());
    QImage        image(QSize(64, 64), QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(2.0);
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        undoIcon().paint(&painter, QRect(0, 0, 32, 32));
    }
    EXPECT_EQ(opaqueColors(image),
              std::set<QRgb>{rgbOf(ThemeColors::themeColor(Key::ICONS_UNDO, Theme::LIGHT))});
}

TEST_F(SvgIconEngineTest, RendersAtTheSizeTimesTheDevicePixelRatio)
{
    const QPixmap pixmap = undoIcon().pixmap(QSize(16, 16), 2.0);
    EXPECT_EQ(pixmap.size(), QSize(32, 32));
    EXPECT_DOUBLE_EQ(pixmap.devicePixelRatio(), 2.0);
    EXPECT_EQ(undoIcon().pixmap(QSize(16, 16)).size(), QSize(16, 16));
}

TEST_F(SvgIconEngineTest, TheDisabledIconIsTheStylesDisabledPixmap)
{
    const Palette light(Palette::light());
    const QIcon   icon     = undoIcon();
    const QImage  normal   = icon.pixmap(QSize(32, 32), QIcon::Normal).toImage();
    const QImage  disabled = icon.pixmap(QSize(32, 32), QIcon::Disabled).toImage();
    EXPECT_EQ(disabled.size(), normal.size());
    EXPECT_NE(disabled.convertToFormat(QImage::Format_ARGB32),
              normal.convertToFormat(QImage::Format_ARGB32));
}

TEST_F(SvgIconEngineTest, AMissingResourceIsANullIcon)
{
    SvgIconEngine engine(QStringLiteral(":/icons/actions/no-such-icon.svg"), {});
    EXPECT_TRUE(engine.isNull());
    EXPECT_TRUE(
        engine.pixmap(QSize(16, 16), QIcon::Normal, QIcon::Off).toImage().isNull() ||
        opaqueColors(engine.pixmap(QSize(16, 16), QIcon::Normal, QIcon::Off).toImage()).empty());
}

TEST_F(SvgIconEngineTest, AClonePaintsLikeTheOriginal)
{
    const Palette light(Palette::light());
    SvgIconEngine engine(QStringLiteral(":/icons/actions/undo.svg"), {{0x000000, Key::ICONS_UNDO}});
    const QIcon   clone(engine.clone());
    EXPECT_EQ(opaqueColors(clone.pixmap(QSize(48, 48)).toImage()),
              opaqueColors(engine.pixmap(QSize(48, 48), QIcon::Normal, QIcon::Off).toImage()));
    EXPECT_EQ(engine.key(), QStringLiteral("QtRocket::Gui::SvgIconEngine"));
    EXPECT_EQ(engine.resourcePath(), QStringLiteral(":/icons/actions/undo.svg"));
    EXPECT_EQ(engine.actualSize(QSize(24, 24), QIcon::Normal, QIcon::Off), QSize(24, 24));
}

}  // namespace

}  // namespace QtRocket::Gui
