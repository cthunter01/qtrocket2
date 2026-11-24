// The button of a colour (Java: ColorChooserButton with its ColorIcon).
//
// Expectations marked "probe" are OpenRocket's own results, printed by the Java probe
// tools/openrocket-gui-pins/plot-widgets/probe/PlotWidgetsProbe.java (OpenRocket 5f164fd0e, JDK 17;
// its transcript is plot-widgets/hand-pinned.txt): a ColorChooserButton of new Color(1, 2, 3), its
// icon painted at (3, 4) into a recording Graphics2D, enabled and disabled, and its property
// change events.

#include "QtRocket/gui/widgets/ColorChooserButton.h"

#include <optional>
#include <vector>

#include <QColor>
#include <QIcon>
#include <QPainter>
#include <QPoint>
#include <QRect>
#include <QRectF>
#include <QSignalSpy>
#include <QSize>
#include <QString>
#include <QTest>
#include <QVariant>
#include <Qt>
#include <QtTypes>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "ScriptedUserPrompts.h"
#include "figure/RecordingPaintDevice.h"

namespace QtRocket::Gui
{

namespace
{

using ColorChooserButtonTest = Test::GuiTest;
using Test::RecordedPaint;
using Test::RecordingPaintDevice;
using Test::ScriptedUserPrompts;

/// What @p icon paints into a 80 x 30 device at (3, 4) in its size 25 x 15, in @p mode.
[[nodiscard]] std::vector<RecordedPaint> paintedIcon(const QIcon& icon, QIcon::Mode mode)
{
    RecordingPaintDevice device(QSize(80, 30));
    {
        QPainter painter(&device);
        icon.paint(&painter, QRect(QPoint(3, 4), ColorChooserButton::kIconSize), Qt::AlignCenter,
                   mode);
    }
    return device.records();
}

TEST_F(ColorChooserButtonTest, ShowsTheColourAsAnIconWithoutText)
{
    // probe: selected=1,2,3,255 icon=ColorIcon 25x15 text='' tooltip=null
    ScriptedUserPrompts      prompts;
    const ColorChooserButton button(QColor(1, 2, 3), prompts, nullptr);
    EXPECT_EQ(button.selectedColor(), QColor(1, 2, 3));
    EXPECT_FALSE(button.icon().isNull());
    EXPECT_EQ(button.iconSize(), QSize(25, 15));
    EXPECT_TRUE(button.text().isEmpty());
    EXPECT_TRUE(button.toolTip().isEmpty());
}

TEST_F(ColorChooserButtonTest, TheIconIsFilledAndOnlyOutlinedWhileDisabled)
{
    // probe: enabled: setColor(1,2,3,255) fillRect(3, 4, 25, 15); disabled: setColor(1,2,3,255)
    // drawRect(3, 4, 25, 15). Deviation: the outline stays inside the icon (Java's, 1 px wide, is
    // 26 x 16 px).
    const QIcon                      icon   = ColorChooserButton::colorIcon(QColor(1, 2, 3));
    const std::vector<RecordedPaint> filled = paintedIcon(icon, QIcon::Normal);
    ASSERT_EQ(filled.size(), 1U);
    EXPECT_EQ(filled.front().kind, RecordedPaint::Kind::RECT);
    EXPECT_EQ(filled.front().devicePath().boundingRect(), QRectF(3, 4, 25, 15));
    EXPECT_EQ(filled.front().brush.color(), QColor(1, 2, 3));
    EXPECT_EQ(filled.front().brush.style(), Qt::SolidPattern);

    const std::vector<RecordedPaint> outlined = paintedIcon(icon, QIcon::Disabled);
    ASSERT_EQ(outlined.size(), 1U);
    EXPECT_EQ(outlined.front().kind, RecordedPaint::Kind::RECT);
    EXPECT_EQ(outlined.front().devicePath().boundingRect(), QRectF(3.5, 4.5, 24, 14));
    EXPECT_EQ(outlined.front().pen.color(), QColor(1, 2, 3));
    EXPECT_EQ(outlined.front().pen.widthF(), 1.0);
    EXPECT_EQ(outlined.front().brush.style(), Qt::NoBrush);
}

TEST_F(ColorChooserButtonTest, ANewColourIsShownAndToldOfOnce)
{
    // probe: setSelectedColor(1,2,3), equal: no event, the same icon; null: no event, the colour
    // kept; (200,100,50,128): another icon and the event (Java's listeners hear it twice).
    ScriptedUserPrompts prompts;
    ColorChooserButton  button(QColor(1, 2, 3), prompts, nullptr);
    const QSignalSpy    changes(&button, &ColorChooserButton::selectedColorChanged);
    const qint64        icon = button.icon().cacheKey();
    button.setSelectedColor(QColor(1, 2, 3));
    EXPECT_EQ(changes.count(), 0);
    EXPECT_EQ(button.icon().cacheKey(), icon);
    button.setSelectedColor(QColor::fromRgbF(1.0F / 255, 2.0F / 255, 3.0F / 255));  // equal RGB
    EXPECT_EQ(changes.count(), 0);
    button.setSelectedColor(QColor());
    EXPECT_EQ(changes.count(), 0);
    EXPECT_EQ(button.selectedColor(), QColor(1, 2, 3));
    button.setSelectedColor(QColor(200, 100, 50, 128));
    ASSERT_EQ(changes.count(), 1);
    EXPECT_EQ(changes.at(0).at(0).value<QColor>(), QColor(200, 100, 50, 128));
    EXPECT_EQ(button.selectedColor(), QColor(200, 100, 50, 128));
    EXPECT_NE(button.icon().cacheKey(), icon);
}

TEST_F(ColorChooserButtonTest, WithoutAColourThereIsNoIcon)
{
    // probe: ColorChooserButton(null): selected=null icon=null
    ScriptedUserPrompts prompts;
    ColorChooserButton  button(QColor(), prompts, nullptr);
    EXPECT_FALSE(button.selectedColor().isValid());
    EXPECT_TRUE(button.icon().isNull());
    button.setSelectedColor(QColor(Qt::red));
    EXPECT_EQ(button.selectedColor(), QColor(Qt::red));
    EXPECT_FALSE(button.icon().isNull());
}

TEST_F(ColorChooserButtonTest, AClickAsksForAColour)
{
    // Java: JColorChooser.createDialog(button, "Select color", ...) with the selected colour; OK
    // takes the chosen one.
    ScriptedUserPrompts prompts;
    ColorChooserButton  button(QColor(1, 2, 3), prompts, nullptr);
    const QSignalSpy    changes(&button, &ColorChooserButton::selectedColorChanged);
    prompts.answerColor(QColor(10, 20, 30));
    QTest::mouseClick(&button, Qt::LeftButton);
    EXPECT_EQ(button.selectedColor(), QColor(10, 20, 30));
    EXPECT_EQ(changes.count(), 1);
    ASSERT_EQ(prompts.colorQuestions().size(), 1U);
    EXPECT_EQ(prompts.colorQuestions().front().initial, QColor(1, 2, 3));
    EXPECT_EQ(prompts.colorQuestions().front().title, QStringLiteral("Select color"));
}

TEST_F(ColorChooserButtonTest, CancelKeepsTheColour)
{
    ScriptedUserPrompts prompts;
    ColorChooserButton  button(QColor(1, 2, 3), prompts, nullptr);
    const QSignalSpy    changes(&button, &ColorChooserButton::selectedColorChanged);
    prompts.answerColor(std::nullopt);
    button.chooseColor();
    EXPECT_EQ(button.selectedColor(), QColor(1, 2, 3));
    EXPECT_EQ(changes.count(), 0);
    // The colour it had chosen before is offered again.
    prompts.answerColor(QColor(1, 2, 3));
    button.click();
    EXPECT_EQ(changes.count(), 0);
    ASSERT_EQ(prompts.colorQuestions().size(), 2U);
    EXPECT_EQ(prompts.colorQuestions().back().initial, QColor(1, 2, 3));
}

}  // namespace

}  // namespace QtRocket::Gui
