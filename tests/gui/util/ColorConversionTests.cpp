#include "QtRocket/gui/util/ColorConversion.h"

#include <optional>

#include <QColor>
#include <QString>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"

// ColorConversionTest (swing gui/util) tests compositeColor(), which comes in Milestone 5 with
// the calipers; these are the other conversions.

namespace QtRocket::Gui
{

namespace
{

using ColorConversionTest = Test::GuiTest;

TEST_F(ColorConversionTest, ConvertsTheCoresColourToQColorAndBack)
{
    const Color  color(200, 10, 30, 128);
    const QColor qt = ColorConversion::toQColor(color);
    EXPECT_EQ(qt, QColor(200, 10, 30, 128));
    EXPECT_EQ(ColorConversion::fromQColor(qt), color);
    EXPECT_EQ(ColorConversion::fromQColor(QColor::fromHsv(0, 255, 255)), Color(255, 0, 0));
}

TEST_F(ColorConversionTest, AnUnsetColourStaysUnset)
{
    EXPECT_EQ(ColorConversion::toQColor(std::optional<Color>()), std::nullopt);
    EXPECT_EQ(ColorConversion::toQColor(std::optional<Color>(Color::darkRed())),
              std::optional<QColor>(QColor(200, 0, 0)));
}

TEST_F(ColorConversionTest, AChannelOutOfRangeIsABug)
{
    // java.awt.Color throws an IllegalArgumentException for it.
    EXPECT_THROW(static_cast<void>(ColorConversion::toQColor(Color(256, 0, 0))), BugError);
    EXPECT_THROW(static_cast<void>(ColorConversion::toQColor(Color(0, 0, 0, -1))), BugError);
    EXPECT_THROW(static_cast<void>(ColorConversion::fromQColor(QColor())), BugError);
}

TEST_F(ColorConversionTest, FormatsAnHtmlColourInLowerCase)
{
    EXPECT_EQ(
        ColorConversion::formatHtmlColor(QColor(0xAB, 0x00, 0x7F, 10), QStringLiteral("Warning")),
        QStringLiteral("<font color=\"#ab007f\">Warning</font>"));
}

TEST_F(ColorConversionTest, WritesTheShortHexColourWhenEveryChannelRepeatsItsDigit)
{
    EXPECT_EQ(ColorConversion::toHexColor(Color(0xAA, 0xBB, 0xCC)), QStringLiteral("#ABC"));
    EXPECT_EQ(ColorConversion::toHexColor(Color(0, 0, 0)), QStringLiteral("#000"));
    EXPECT_EQ(ColorConversion::toHexColor(Color(0xAA, 0xBB, 0xCD)), QStringLiteral("#AABBCD"));
    EXPECT_EQ(ColorConversion::toHexColor(Color(1, 2, 3, 4)), QStringLiteral("#010203"));
}

TEST_F(ColorConversionTest, ReadsAHexColour)
{
    EXPECT_EQ(ColorConversion::fromHexColor(u"#abc"), Color(0xAA, 0xBB, 0xCC));
    EXPECT_EQ(ColorConversion::fromHexColor(u"ABC"), Color(0xAA, 0xBB, 0xCC));
    EXPECT_EQ(ColorConversion::fromHexColor(u"#0a1B2c"), Color(0x0A, 0x1B, 0x2C));
    EXPECT_EQ(ColorConversion::fromHexColor(u"# 123456 "), Color(0x12, 0x34, 0x56));
    // Java: null for a blank text, an IllegalArgumentException for the others.
    EXPECT_EQ(ColorConversion::fromHexColor(u""), std::nullopt);
    EXPECT_EQ(ColorConversion::fromHexColor(u"   "), std::nullopt);
    EXPECT_EQ(ColorConversion::fromHexColor(u"#12345"), std::nullopt);
    EXPECT_EQ(ColorConversion::fromHexColor(u"#ggg"), std::nullopt);
    // Java removes the '#' before it trims.
    EXPECT_EQ(ColorConversion::fromHexColor(u" #abc"), std::nullopt);
}

TEST_F(ColorConversionTest, BrightensAColourUpToWhite)
{
    EXPECT_EQ(ColorConversion::brightenColor(QColor(210, 31, 0, 100), 50), QColor(255, 81, 50));
}

}  // namespace

}  // namespace QtRocket::Gui
