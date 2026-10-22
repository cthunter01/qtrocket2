#include "QtRocket/gui/dialogs/MessageList.h"

#include <QAbstractItemView>
#include <QBrush>
#include <QColor>
#include <QListWidgetItem>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QVariant>
#include <Qt>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using MessageListTest = Test::GuiTest;

TEST_F(MessageListTest, ListsTheMessagesInRowsOfAlternatingBackground)
{
    const QStringList messages{QStringLiteral("Unknown element foo, ignoring."),
                               QStringLiteral("Illegal motor specification")};
    const MessageList list(messages);
    EXPECT_EQ(list.messages(), messages);
    EXPECT_TRUE(list.alternatingRowColors());
    // JList's default selection: any rows.
    EXPECT_EQ(list.selectionMode(), QAbstractItemView::ExtendedSelection);
    EXPECT_EQ(list.item(0)->data(Qt::ForegroundRole), QVariant());
}

TEST_F(MessageListTest, TheTextsCanHaveAColourOfTheirOwn)
{
    // BetterListCellRenderer(Color).
    const QColor      red(0x80, 0, 0);
    const MessageList list({QStringLiteral("first"), QStringLiteral("second")}, red);
    ASSERT_EQ(list.count(), 2);
    EXPECT_EQ(list.item(0)->foreground().color(), red);
    EXPECT_EQ(list.item(1)->foreground().color(), red);
}

TEST_F(MessageListTest, APressBelowTheLastRowClearsTheSelection)
{
    // ErrorWarningDialog: "Deselect items if clicked on blank region".
    MessageList list({QStringLiteral("first"), QStringLiteral("second")});
    list.resize(300, 200);
    list.show();
    list.setCurrentRow(1);
    ASSERT_EQ(list.selectedItems().size(), 1);
    const QPoint below(10, list.viewport()->height() - 2);
    ASSERT_EQ(list.itemAt(below), nullptr);
    QTest::mousePress(list.viewport(), Qt::LeftButton, Qt::NoModifier, below);
    QTest::mouseRelease(list.viewport(), Qt::LeftButton, Qt::NoModifier, below);
    EXPECT_TRUE(list.selectedItems().isEmpty());

    // A press on a row selects it.
    const QPoint first = list.visualItemRect(list.item(0)).center();
    QTest::mouseClick(list.viewport(), Qt::LeftButton, Qt::NoModifier, first);
    ASSERT_EQ(list.selectedItems().size(), 1);
    EXPECT_EQ(list.selectedItems().front()->text(), QStringLiteral("first"));
}

}  // namespace

}  // namespace QtRocket::Gui
