#include "QtRocket/gui/widgets/IconToggleButton.h"

#include <QIcon>
#include <QImage>
#include <QPoint>
#include <QSize>
#include <QSizeF>
#include <QTest>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/Icons.h"

namespace QtRocket::Gui
{

namespace
{

using IconToggleButtonTest = Test::GuiTest;

TEST_F(IconToggleButtonTest, IsACheckableButtonWithoutFocus)
{
    IconToggleButton button(nullptr);
    EXPECT_TRUE(button.isCheckable());
    EXPECT_FALSE(button.isChecked());
    EXPECT_EQ(button.focusPolicy(), Qt::NoFocus);
    button.setSelectedIcon(Icons::icon(Icons::Id::HELP_ABOUT));
    EXPECT_TRUE(button.greysUnselectedIcon());
    button.resize(button.sizeHint());
    QTest::mouseClick(&button, Qt::LeftButton, Qt::NoModifier, QPoint(4, 4));
    EXPECT_TRUE(button.isChecked());
    QTest::mouseClick(&button, Qt::LeftButton, Qt::NoModifier, QPoint(4, 4));
    EXPECT_FALSE(button.isChecked());
}

TEST_F(IconToggleButtonTest, IsAsLargeAsItsScaledIcon)
{
    IconToggleButton button(Icons::icon(Icons::Id::HELP), nullptr);
    EXPECT_FALSE(button.greysUnselectedIcon());
    const QSize icon = button.iconSize();
    EXPECT_EQ(button.sizeHint(), icon);
    button.setIconScale(1.5);
    EXPECT_DOUBLE_EQ(button.iconScale(), 1.5);
    EXPECT_EQ(button.sizeHint(), (QSizeF(icon) * 1.5).toSize());
    EXPECT_EQ(button.minimumSizeHint(), button.sizeHint());
}

TEST_F(IconToggleButtonTest, GreysTheSelectedIconOutWhenUnselected)
{
    IconToggleButton button(nullptr);
    button.setSelectedIcon(Icons::icon(Icons::Id::HELP_ABOUT));
    button.resize(32, 32);
    const QImage unselected = button.grab().toImage();
    button.setChecked(true);
    const QImage selected = button.grab().toImage();
    EXPECT_NE(selected, unselected);

    // Shown in both states: the icon alone.
    IconToggleButton plain(Icons::icon(Icons::Id::HELP_ABOUT), true, nullptr);
    plain.resize(32, 32);
    const QImage checked = plain.grab().toImage();
    plain.setChecked(false);
    EXPECT_EQ(plain.grab().toImage(), checked);
    EXPECT_EQ(checked, selected);
}

}  // namespace

}  // namespace QtRocket::Gui
