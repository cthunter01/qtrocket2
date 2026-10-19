#include "QtRocket/gui/widgets/StyledLabel.h"

#include <QColor>
#include <QFont>
#include <QPalette>
#include <QString>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using StyledLabelTest = Test::GuiTest;
using Style           = StyledLabel::Style;

/// The point size of the labels' default font.
double defaultPointSize()
{
    const StyledLabel plain(nullptr);
    return plain.font().pointSizeF();
}

TEST_F(StyledLabelTest, ResizesTheFontByPoints)
{
    const StyledLabel small(QStringLiteral("Note"), -2.0, nullptr);
    EXPECT_EQ(small.text(), QStringLiteral("Note"));
    EXPECT_DOUBLE_EQ(small.font().pointSizeF(), defaultPointSize() - 2.0);

    StyledLabel label(QStringLiteral("Note"), nullptr);
    label.resizeFont(1.5);
    label.resizeFont(1.5);
    EXPECT_DOUBLE_EQ(label.font().pointSizeF(), defaultPointSize() + 3.0);
}

TEST_F(StyledLabelTest, SetsTheFontStyle)
{
    const StyledLabel bold(QStringLiteral("Title"), Style::BOLD, nullptr);
    EXPECT_TRUE(bold.font().bold());
    EXPECT_FALSE(bold.font().italic());
    EXPECT_DOUBLE_EQ(bold.font().pointSizeF(), defaultPointSize());

    StyledLabel label(QStringLiteral("Title"), 1.0, Style::BOLD_ITALIC, nullptr);
    EXPECT_TRUE(label.font().bold());
    EXPECT_TRUE(label.font().italic());
    label.setFontStyle(Style::PLAIN);
    EXPECT_FALSE(label.font().bold());
    EXPECT_FALSE(label.font().italic());
    label.setFontStyle(Style::ITALIC);
    EXPECT_FALSE(label.font().bold());
    EXPECT_TRUE(label.font().italic());
}

TEST_F(StyledLabelTest, AlignsAndColoursItsText)
{
    StyledLabel label(QStringLiteral("Right"), Qt::AlignRight, -1.0, Style::PLAIN, nullptr);
    EXPECT_EQ(label.alignment(), Qt::Alignment(Qt::AlignRight));
    label.setFontColor(QColor(200, 0, 0));
    EXPECT_EQ(label.palette().color(QPalette::WindowText), QColor(200, 0, 0));
}

}  // namespace

}  // namespace QtRocket::Gui
