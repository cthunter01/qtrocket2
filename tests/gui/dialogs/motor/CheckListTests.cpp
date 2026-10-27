// The check list of the motor chooser's manufacturers (Java: CheckList, DefaultCheckListModel,
// CheckListEditor and CheckListRenderer of swing gui/util). OpenRocket has no JUnit test of them;
// these tests follow the Java source: a click of the left button anywhere on an item toggles it
// (CheckListEditor.mouseClicked() with a click count of 1), Space toggles the selected item, and
// every change fires the model's contentsChanged.

#include "QtRocket/gui/dialogs/motor/CheckList.h"

#include <string>
#include <vector>

#include <QAbstractItemView>
#include <QListWidgetItem>
#include <QPoint>
#include <QRect>
#include <QSignalSpy>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/util/BugError.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using CheckListTest = Test::GuiTest;
using Test::showActiveWith;

/// Three items, none checked.
[[nodiscard]] QStringList threeItems()
{
    return {QStringLiteral("AeroTech"), QStringLiteral("Cesaroni Technology Inc."),
            QStringLiteral("Estes")};
}

/// A point on the text of the item @p row, right of the check box.
[[nodiscard]] QPoint onText(const CheckList& list, int row)
{
    const QRect rect = list.visualItemRect(list.item(row));
    return {rect.right() - (rect.width() / 4), rect.center().y()};
}

/// A point at the left end of the item @p row, where the style draws the check box.
[[nodiscard]] QPoint onCheckBox(const CheckList& list, int row)
{
    const QRect rect = list.visualItemRect(list.item(row));
    return {rect.left() + (rect.height() / 2), rect.center().y()};
}

/// The check state of each item and whether it is user-checkable, joined by ", ".
[[nodiscard]] std::string itemStates(const CheckList& list)
{
    std::string states;
    for (int row = 0; row < list.count(); ++row)
    {
        const QListWidgetItem* item = list.item(row);
        if (!states.empty())
        {
            states += ", ";
        }
        states += list.isChecked(row) ? "checked" : "unchecked";
        states += item->checkState() == (list.isChecked(row) ? Qt::Checked : Qt::Unchecked)
                      ? ""
                      : " (another state)";
        states += item->flags().testFlag(Qt::ItemIsUserCheckable) ? " checkable" : "";
    }
    return states;
}

TEST_F(CheckListTest, StartsWithUncheckedItemsAndSingleSelection)
{
    QWidget          window;
    auto*            list = makeChild<CheckList>(&window);
    const QSignalSpy changes(list, &CheckList::checksChanged);
    list->setItems(threeItems());
    EXPECT_EQ(list->count(), 3);
    EXPECT_EQ(list->selectionMode(), QAbstractItemView::SingleSelection);
    EXPECT_EQ(itemStates(*list), "unchecked checkable, unchecked checkable, unchecked checkable");
    EXPECT_EQ(list->item(1)->text(), QStringLiteral("Cesaroni Technology Inc."));
    EXPECT_EQ(list->checkedIndices(), std::vector<int>{});
    // getUncheckedItems(): from the last item to the first.
    EXPECT_EQ(list->uncheckedIndices(), (std::vector<int>{2, 1, 0}));
    EXPECT_EQ(list->selectedIndex(), -1);
    // A new model fires nothing.
    EXPECT_EQ(changes.count(), 0);
}

TEST_F(CheckListTest, EveryChangeFiresAlsoWhenItChangesNothing)
{
    QWidget window;
    auto*   list = makeChild<CheckList>(&window);
    list->setItems(threeItems());
    const QSignalSpy changes(list, &CheckList::checksChanged);

    list->setChecked(1, true);
    EXPECT_TRUE(list->isChecked(1));
    EXPECT_EQ(changes.count(), 1);
    list->setChecked(1, true);
    EXPECT_EQ(changes.count(), 2);
    EXPECT_EQ(list->checkedIndices(), std::vector<int>{1});
    EXPECT_EQ(list->uncheckedIndices(), (std::vector<int>{2, 0}));

    list->checkAll();
    EXPECT_EQ(list->checkedIndices(), (std::vector<int>{0, 1, 2}));
    EXPECT_EQ(changes.count(), 3);
    list->clearAll();
    EXPECT_EQ(list->checkedIndices(), std::vector<int>{});
    EXPECT_EQ(changes.count(), 4);
    list->clearAll();
    EXPECT_EQ(changes.count(), 5);

    list->setUncheckedIndices({0, 7});
    EXPECT_EQ(list->checkedIndices(), (std::vector<int>{1, 2}));
    EXPECT_EQ(changes.count(), 6);
    list->setCheckedIndices({2, -1});
    EXPECT_EQ(list->checkedIndices(), std::vector<int>{2});
    EXPECT_EQ(changes.count(), 7);

    list->toggleIndex(0);
    EXPECT_EQ(list->checkedIndices(), (std::vector<int>{0, 2}));
    EXPECT_EQ(changes.count(), 8);
    // toggleIndex() of no item (the selected index of no selection) does nothing.
    list->toggleIndex(-1);
    list->toggleIndex(3);
    EXPECT_EQ(changes.count(), 8);
}

TEST_F(CheckListTest, RefusesAnIndexOfNoItem)
{
    QWidget window;
    auto*   list = makeChild<CheckList>(&window);
    list->setItems(threeItems());
    EXPECT_THROW(static_cast<void>(list->isChecked(3)), BugError);
    EXPECT_THROW(list->setChecked(-1, true), BugError);
}

TEST_F(CheckListTest, AClickAnywhereOnAnItemTogglesIt)
{
    QWidget window;
    auto*   list = makeChild<CheckList>(&window);
    list->setItems(threeItems());
    ASSERT_TRUE(showActiveWith(window, {list}));
    const QSignalSpy changes(list, &CheckList::checksChanged);

    QTest::mouseClick(list->viewport(), Qt::LeftButton, {}, onText(*list, 1));
    EXPECT_TRUE(list->isChecked(1));
    EXPECT_EQ(list->selectedIndex(), 1);
    EXPECT_EQ(changes.count(), 1);
    // On the check box, once (Qt's own toggle of the check box is not added to it).
    QTest::mouseClick(list->viewport(), Qt::LeftButton, {}, onCheckBox(*list, 1));
    EXPECT_FALSE(list->isChecked(1));
    EXPECT_EQ(changes.count(), 2);
    QTest::mouseClick(list->viewport(), Qt::LeftButton, {}, onCheckBox(*list, 2));
    EXPECT_EQ(list->checkedIndices(), std::vector<int>{2});
    EXPECT_EQ(list->selectedIndex(), 2);
    EXPECT_EQ(changes.count(), 3);
}

TEST_F(CheckListTest, OtherButtonsAndClicksBesideTheItemsToggleNothing)
{
    QWidget window;
    auto*   list = makeChild<CheckList>(&window);
    list->setItems(threeItems());
    ASSERT_TRUE(showActiveWith(window, {list}));
    const QSignalSpy changes(list, &CheckList::checksChanged);

    QTest::mouseClick(list->viewport(), Qt::RightButton, {}, onText(*list, 0));
    QTest::mouseClick(list->viewport(), Qt::MiddleButton, {}, onText(*list, 0));
    // Below the last item.
    const QRect last = list->visualItemRect(list->item(2));
    ASSERT_LT(last.bottom() + 5, list->viewport()->height());
    QTest::mouseClick(list->viewport(), Qt::LeftButton, {},
                      QPoint(last.center().x(), last.bottom() + 5));
    // Pressed on one item, released on another.
    QTest::mousePress(list->viewport(), Qt::LeftButton, {}, onText(*list, 0));
    QTest::mouseRelease(list->viewport(), Qt::LeftButton, {}, onText(*list, 1));
    EXPECT_EQ(list->checkedIndices(), std::vector<int>{});
    EXPECT_EQ(changes.count(), 0);
}

TEST_F(CheckListTest, ADoubleClickTogglesOnce)
{
    QWidget window;
    auto*   list = makeChild<CheckList>(&window);
    list->setItems(threeItems());
    ASSERT_TRUE(showActiveWith(window, {list}));
    const QSignalSpy changes(list, &CheckList::checksChanged);

    // What a widget gets of a double click: a press and a release, then the double click in
    // place of the second press, and its release (Swing: clicks of the counts 1 and 2).
    QTest::mouseClick(list->viewport(), Qt::LeftButton, {}, onText(*list, 0));
    QTest::mouseDClick(list->viewport(), Qt::LeftButton, {}, onText(*list, 0));
    QTest::mouseRelease(list->viewport(), Qt::LeftButton, {}, onText(*list, 0));
    EXPECT_EQ(list->checkedIndices(), std::vector<int>{0});
    EXPECT_EQ(changes.count(), 1);
    // The next click toggles again.
    QTest::mouseClick(list->viewport(), Qt::LeftButton, {}, onText(*list, 0));
    EXPECT_EQ(list->checkedIndices(), std::vector<int>{});
    EXPECT_EQ(changes.count(), 2);
}

TEST_F(CheckListTest, SpaceTogglesTheSelectedItem)
{
    QWidget window;
    auto*   list = makeChild<CheckList>(&window);
    list->setItems(threeItems());
    ASSERT_TRUE(showActiveWith(window, {list}));
    list->setFocus();
    const QSignalSpy changes(list, &CheckList::checksChanged);

    // No selection: nothing.
    QTest::keyClick(list, Qt::Key_Space);
    EXPECT_EQ(changes.count(), 0);

    list->setCurrentRow(2);
    ASSERT_EQ(list->selectedIndex(), 2);
    QTest::keyClick(list, Qt::Key_Space);
    EXPECT_EQ(list->checkedIndices(), std::vector<int>{2});
    EXPECT_EQ(changes.count(), 1);
    QTest::keyClick(list, Qt::Key_Space);
    EXPECT_EQ(list->checkedIndices(), std::vector<int>{});
    EXPECT_EQ(changes.count(), 2);
    // Only without modifiers (KeyStroke.getKeyStroke(VK_SPACE, 0)).
    QTest::keyClick(list, Qt::Key_Space, Qt::ShiftModifier);
    EXPECT_EQ(changes.count(), 2);
    // The arrows move the selection, as a JList's.
    QTest::keyClick(list, Qt::Key_Up);
    EXPECT_EQ(list->selectedIndex(), 1);
    QTest::keyClick(list, Qt::Key_Space);
    EXPECT_EQ(list->checkedIndices(), std::vector<int>{1});
}

TEST_F(CheckListTest, ACheckStateSetOnAnItemFiresToo)
{
    QWidget window;
    auto*   list = makeChild<CheckList>(&window);
    list->setItems(threeItems());
    const QSignalSpy changes(list, &CheckList::checksChanged);
    // As the accessibility's toggle action does it.
    list->item(0)->setCheckState(Qt::Checked);
    EXPECT_TRUE(list->isChecked(0));
    EXPECT_EQ(changes.count(), 1);
}

TEST_F(CheckListTest, TakesThePreferredAndMinimumSizesGiven)
{
    QWidget window;
    auto*   list = makeChild<CheckList>(&window);
    list->setItems(threeItems());
    EXPECT_NE(list->sizeHint(), QSize(60, 60));
    list->setSizeHints(QSize(60, 60), QSize(0, 0));
    EXPECT_EQ(list->sizeHint(), QSize(60, 60));
    EXPECT_EQ(list->minimumSizeHint(), QSize(0, 0));
}

}  // namespace

}  // namespace QtRocket::Gui
