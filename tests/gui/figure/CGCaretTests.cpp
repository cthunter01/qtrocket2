#include "QtRocket/gui/figure/CGCaret.h"

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

using CGCaretTest = Test::GuiTest;
using Test::ScopedApplicationPalette;

TEST_F(CGCaretTest, IsTwoOppositeQuartersInsideARing)
{
    // Port of CGCaret.getCaret(): the circle of radius 7 less the squares (-7, -7, 7, 7) and
    // (0, 0, 7, 7), plus the ring between 7 and 6.3; in the caret's own coordinates (y up in the
    // figure).
    const CGCaret      caret(0, 0);
    const QPainterPath area = caret.shape();
    EXPECT_EQ(area.fillRule(), Qt::WindingFill);
    EXPECT_EQ(area.controlPointRect(), QRectF(-7, -7, 14, 14));
    // The quarters that stay: x > 0 with y < 0, and x < 0 with y > 0.
    EXPECT_TRUE(area.contains(QPointF(3, -3)));
    EXPECT_TRUE(area.contains(QPointF(-3, 3)));
    EXPECT_TRUE(area.contains(QPointF(5, -1)));
    EXPECT_TRUE(area.contains(QPointF(-1, 5)));
    // The squares taken out.
    EXPECT_FALSE(area.contains(QPointF(3, 3)));
    EXPECT_FALSE(area.contains(QPointF(-3, -3)));
    EXPECT_FALSE(area.contains(QPointF(5, 1)));
    EXPECT_FALSE(area.contains(QPointF(-1, -5)));
    // The ring, all around.
    EXPECT_TRUE(area.contains(QPointF(4.7, 4.7)));
    EXPECT_TRUE(area.contains(QPointF(-4.7, -4.7)));
    EXPECT_TRUE(area.contains(QPointF(0.1, 6.6)));
    // Outside.
    EXPECT_FALSE(area.contains(QPointF(5.2, 5.2)));
    EXPECT_FALSE(area.contains(QPointF(7.5, 0.1)));
}

TEST_F(CGCaretTest, IsTheThemesCgColour)
{
    const CGCaret caret(0, 0);
    EXPECT_EQ(caret.colorKey(), ThemeColors::Key::CG);
    {
        const ScopedApplicationPalette light(ScopedApplicationPalette::light());
        EXPECT_EQ(caret.color(), QColor(0, 0, 255));
    }
    const ScopedApplicationPalette dark(ScopedApplicationPalette::dark());
    EXPECT_EQ(caret.color(), QColor(0x55, 0x85, 0xfd));
}

}  // namespace

}  // namespace QtRocket::Gui
