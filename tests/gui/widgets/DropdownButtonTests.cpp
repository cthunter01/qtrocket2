#include "QtRocket/gui/widgets/DropdownButton.h"

#include <QAction>
#include <QMenu>
#include <QPoint>
#include <QPointer>
#include <QSignalSpy>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using DropdownButtonTest = Test::GuiTest;
using Test::OffscreenPopupWarnings;
using Test::showActive;

TEST_F(DropdownButtonTest, ShowsItsTextAndTheIndicator)
{
    // DROPDOWN_INDICATOR "  | \u25BC ".
    QWidget               window;
    const DropdownButton& button = *makeChild<DropdownButton>(&window, QStringLiteral("Actions"));
    EXPECT_EQ(DropdownButton::indicator(), QStringLiteral("  | \u25BC "));
    EXPECT_EQ(button.text(), QStringLiteral("Actions  | \u25BC "));
    EXPECT_TRUE(button.popupMenu().actions().isEmpty());
}

TEST_F(DropdownButtonTest, HoldsTheMenuItems)
{
    QWidget         window;
    DropdownButton& button = *makeChild<DropdownButton>(&window, QStringLiteral("Actions"));
    QAction*        first  = button.addMenuItem(QStringLiteral("First"));
    auto*           other  = makeChild<QAction>(&window, QStringLiteral("Other"));
    button.addMenuItem(other);
    ASSERT_EQ(button.popupMenu().actions().size(), 2);
    EXPECT_EQ(button.popupMenu().actions().front(), first);
    EXPECT_EQ(first->text(), QStringLiteral("First"));
    const QSignalSpy triggered(first, &QAction::triggered);
    first->trigger();
    EXPECT_EQ(triggered.count(), 1);

    // The menu's own items go with them; the other stays its owner's.
    const QPointer<QAction> own(first);
    button.removeAllMenuItems();
    EXPECT_TRUE(button.popupMenu().actions().isEmpty());
    EXPECT_TRUE(own.isNull());
    EXPECT_EQ(other->parent(), &window);
}

TEST_F(DropdownButtonTest, AClickShowsTheMenuBelowTheButton)
{
    QWidget         window;
    auto*           layout = makeChild<QVBoxLayout>(&window);
    DropdownButton& button = *makeChild<DropdownButton>(&window, QStringLiteral("Actions"));
    layout->addWidget(&button);
    static_cast<void>(button.addMenuItem(QStringLiteral("First")));
    ASSERT_TRUE(showActive(window));
    {
        const OffscreenPopupWarnings warnings;
        button.click();
        EXPECT_TRUE(button.popupMenu().isVisible());
        EXPECT_EQ(button.popupMenu().pos(), button.mapToGlobal(QPoint(0, button.height())));
        button.popupMenu().hide();
    }
}

}  // namespace

}  // namespace QtRocket::Gui
