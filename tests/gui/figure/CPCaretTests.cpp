#include "QtRocket/gui/figure/CPCaret.h"

#include <QColor>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "ScopedApplicationPalette.h"

namespace QtRocket::Gui
{

namespace
{

using CPCaretTest = Test::GuiTest;
using Test::ScopedApplicationPalette;

TEST_F(CPCaretTest, IsADiscInsideARing)
{
    // Port of CPCaret.getCaret(): the ring between the radius 7 and 6.3 plus the disc of 5.25.
    const CPCaret      caret(0, 0);
    const QPainterPath area = caret.shape();
    EXPECT_EQ(area.fillRule(), Qt::WindingFill);
    EXPECT_EQ(area.controlPointRect(), QRectF(-7, -7, 14, 14));
    // The disc.
    EXPECT_TRUE(area.contains(QPointF(0, 0)));
    EXPECT_TRUE(area.contains(QPointF(5, 0)));
    EXPECT_TRUE(area.contains(QPointF(-3.5, 3.5)));
    // The gap between the disc and the ring.
    EXPECT_FALSE(area.contains(QPointF(5.8, 0)));
    EXPECT_FALSE(area.contains(QPointF(0, -5.8)));
    EXPECT_FALSE(area.contains(QPointF(4, 4)));
    // The ring.
    EXPECT_TRUE(area.contains(QPointF(6.6, 0)));
    EXPECT_TRUE(area.contains(QPointF(-4.7, -4.7)));
    // Outside.
    EXPECT_FALSE(area.contains(QPointF(7.5, 0)));
    EXPECT_FALSE(area.contains(QPointF(5.2, -5.2)));
}

TEST_F(CPCaretTest, IsTheThemesCpColour)
{
    const CPCaret caret(0, 0);
    EXPECT_EQ(caret.colorKey(), ThemeColors::Key::CP);
    {
        const ScopedApplicationPalette light(ScopedApplicationPalette::light());
        EXPECT_EQ(caret.color(), QColor(255, 0, 0));
    }
    const ScopedApplicationPalette dark(ScopedApplicationPalette::dark());
    EXPECT_EQ(caret.color(), QColor(0xff, 0x48, 0x6a));
}

}  // namespace

}  // namespace QtRocket::Gui
