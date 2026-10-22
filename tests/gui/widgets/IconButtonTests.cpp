#include "QtRocket/gui/widgets/IconButton.h"

#include <cmath>
#include <memory>

#include <QAction>
#include <QIcon>
#include <QPixmap>
#include <QPoint>
#include <QPushButton>
#include <QSignalSpy>
#include <QSize>
#include <QString>
#include <QStyle>
#include <QTest>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/Icons.h"

namespace QtRocket::Gui
{

namespace
{

using IconButtonTest = Test::GuiTest;

/// An action as RocketActions makes one: a text with its mnemonic, an icon and a tooltip.
[[nodiscard]] std::unique_ptr<QAction> editAction()
{
    auto action = std::make_unique<QAction>(QStringLiteral("&Edit"));
    action->setIcon(Icons::icon(Icons::Id::EDIT_EDIT));
    action->setToolTip(QStringLiteral("Edit the selected component."));
    return action;
}

TEST_F(IconButtonTest, ShowsTheActionsTextIconAndTooltip)
{
    const std::unique_ptr<QAction> action = editAction();
    IconButton                     button(nullptr);
    EXPECT_EQ(button.action(), nullptr);
    button.setAction(action.get());
    EXPECT_EQ(button.action(), action.get());
    // Without the mnemonic (DesignPanel's setMnemonic(0)).
    EXPECT_EQ(button.text(), QStringLiteral("Edit"));
    EXPECT_FALSE(button.icon().isNull());
    EXPECT_EQ(button.toolTip(), QStringLiteral("Edit the selected component."));
    EXPECT_TRUE(button.isEnabled());
}

TEST_F(IconButtonTest, FollowsTheActionsChanges)
{
    const std::unique_ptr<QAction> action = editAction();
    IconButton                     button(nullptr);
    button.setAction(action.get());
    action->setEnabled(false);
    EXPECT_FALSE(button.isEnabled());
    action->setEnabled(true);
    EXPECT_TRUE(button.isEnabled());
    action->setText(QStringLiteral("Hide &selected"));
    action->setToolTip(QStringLiteral("Hide selected component(s)."));
    action->setIcon(QIcon());
    EXPECT_EQ(button.text(), QStringLiteral("Hide selected"));
    EXPECT_EQ(button.toolTip(), QStringLiteral("Hide selected component(s)."));
    EXPECT_TRUE(button.icon().isNull());
}

TEST_F(IconButtonTest, ShowsALiteralAmpersandAsItIs)
{
    QAction    action(QStringLiteral("Nose && tail"));
    IconButton button(nullptr);
    button.setAction(&action);
    // The text "Nose & tail", escaped for the button, which then has no mnemonic either.
    EXPECT_EQ(button.text(), QStringLiteral("Nose && tail"));
}

TEST_F(IconButtonTest, AClickTriggersTheEnabledAction)
{
    const std::unique_ptr<QAction> action = editAction();
    IconButton                     button(nullptr);
    button.setAction(action.get());
    button.resize(button.sizeHint());
    const QSignalSpy triggered(action.get(), &QAction::triggered);
    QTest::mouseClick(&button, Qt::LeftButton, Qt::NoModifier, QPoint(4, 4));
    EXPECT_EQ(triggered.count(), 1);
    action->setEnabled(false);
    QTest::mouseClick(&button, Qt::LeftButton, Qt::NoModifier, QPoint(4, 4));
    EXPECT_EQ(triggered.count(), 1);
}

TEST_F(IconButtonTest, LetsGoOfAnActionAndSurvivesOne)
{
    auto       action = editAction();
    IconButton button(nullptr);
    button.setAction(action.get());
    button.setAction(nullptr);
    EXPECT_EQ(button.action(), nullptr);
    action->setText(QStringLiteral("Other"));
    EXPECT_EQ(button.text(), QStringLiteral("Edit"));

    button.setAction(action.get());
    action = nullptr;
    // The button keeps what it showed and does nothing.
    EXPECT_EQ(button.action(), nullptr);
    EXPECT_EQ(button.text(), QStringLiteral("Other"));
    button.resize(button.sizeHint());
    QTest::mouseClick(&button, Qt::LeftButton, Qt::NoModifier, QPoint(4, 4));
}

TEST_F(IconButtonTest, ScalesTheStylesIconSize)
{
    const IconButton button(nullptr);
    const int styleSize = button.style()->pixelMetric(QStyle::PM_ButtonIconSize, nullptr, &button);
    const int expected  = static_cast<int>(std::lround(styleSize * IconButton::kIconScale));
    EXPECT_EQ(button.iconSize(), QSize(expected, expected));
}

TEST_F(IconButtonTest, LeavesItsIconGapBetweenIconAndText)
{
    // A text long enough for no style's smallest button width to matter.
    const QString text = QStringLiteral("A text that is long enough");
    IconButton    withIcon(nullptr);
    withIcon.setText(text);
    withIcon.setIcon(Icons::icon(Icons::Id::UP));
    IconButton withoutIcon(nullptr);
    withoutIcon.setText(text);
    EXPECT_EQ(withIcon.sizeHint().width() - withoutIcon.sizeHint().width(),
              withIcon.iconSize().width() + IconButton::kIconGap);
    EXPECT_EQ(withIcon.minimumSizeHint(), withIcon.sizeHint());

    // QPushButton leaves 4 pixels.
    QPushButton plain(text);
    plain.setIcon(Icons::icon(Icons::Id::UP));
    plain.setIconSize(withIcon.iconSize());
    EXPECT_EQ(withIcon.sizeHint().width() - plain.sizeHint().width(), IconButton::kIconGap - 4);
    EXPECT_EQ(withIcon.sizeHint().height(), plain.sizeHint().height());
}

TEST_F(IconButtonTest, PaintsInEveryState)
{
    const std::unique_ptr<QAction> action = editAction();
    IconButton                     button(nullptr);
    button.setAction(action.get());
    button.resize(button.sizeHint() + QSize(60, 0));
    EXPECT_FALSE(button.grab().isNull());
    button.setDown(true);
    EXPECT_FALSE(button.grab().isNull());
    button.setDown(false);
    action->setEnabled(false);
    EXPECT_FALSE(button.grab().isNull());
    button.setLayoutDirection(Qt::RightToLeft);
    EXPECT_FALSE(button.grab().isNull());
}

}  // namespace

}  // namespace QtRocket::Gui
