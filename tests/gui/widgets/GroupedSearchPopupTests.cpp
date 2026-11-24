// The popup of a grouped, searchable combo box (Java: GroupableAndSearchableComboBox's groups and
// search popups), over items of the test's own. MaterialComboBoxTests and DataTypeComboBoxTests
// compare it, through its two owners, with OpenRocket's.

#include "QtRocket/gui/widgets/GroupedSearchPopup.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <QAction>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QString>
#include <QTest>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/util/BugError.h"
#include "RecordingGuardHook.h"
#include "WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using GroupedSearchPopupTest = Test::GuiTest;
using Test::OffscreenPopupWarnings;
using Test::RecordingGuardHook;
using Test::showActive;

/// An item of the test: its text, the key it sorts by and its group.
struct Item
{
    QString     text;
    std::string sortKey;
    std::size_t group{0};
};

/// The owner of a popup: a window with the popup as its child, over items of the test's own.
class Owner
{
public:
    Owner()
      : m_popup(makeChild<GroupedSearchPopup>(
            &m_window, QStringLiteral("Search here"),
            std::vector<QString>{QStringLiteral("Fruit"), QStringLiteral("Stone"),
                                 QStringLiteral("Empty")},
            content()))
    {
        m_window.setObjectName(QStringLiteral("owner"));
    }

    [[nodiscard]] GroupedSearchPopup& popup() const noexcept { return *m_popup; }
    [[nodiscard]] QWidget&            window() noexcept { return m_window; }

    std::vector<Item> items{{.text = QStringLiteral("Banana"), .sortKey = "banana", .group = 0},
                            {.text = QStringLiteral("apple"), .sortKey = "apple", .group = 0},
                            {.text = QStringLiteral("(x) Cherry"), .sortKey = "cherry", .group = 1},
                            {.text = QStringLiteral("Avocado"), .sortKey = "avocado", .group = 1}};
    int               current{1};
    std::optional<std::size_t> currentGroup{0};
    std::vector<int>           chosen;
    /// Whether the popup was visible when an item was chosen.
    std::vector<bool> visibleWhenChosen;
    /// What a choice does besides being recorded.
    std::function<void()> onChoice;

private:
    [[nodiscard]] GroupedSearchPopup::Content content()
    {
        return {
            .itemCount = [this] { return static_cast<int>(items.size()); },
            .text      = [this](int item) { return items.at(static_cast<std::size_t>(item)).text; },
            .sortKey =
                [this](int item) { return items.at(static_cast<std::size_t>(item)).sortKey; },
            .groups =
                [this] {
                    std::vector<GroupedSearchPopup::Group> groups;
                    for (std::size_t group = 0; group < 3; ++group)
                    {
                        GroupedSearchPopup::Group entry{.group = group, .items = {}};
                        for (std::size_t item = 0; item < items.size(); ++item)
                        {
                            if (items.at(item).group == group)
                            {
                                entry.items.push_back(static_cast<int>(item));
                            }
                        }
                        if (!entry.items.empty())
                        {
                            groups.push_back(std::move(entry));
                        }
                    }
                    return groups;
                },
            .current      = [this] { return current; },
            .currentGroup = [this] { return currentGroup; },
            .choose =
                [this](int item) {
                    chosen.push_back(item);
                    visibleWhenChosen.push_back(m_popup->isVisible());
                    // A copy: the action may destroy this owner, and its own copy with it.
                    const std::function<void()> action = onChoice;
                    if (action)
                    {
                        action();
                    }
                },
        };
    }

    QWidget             m_window;
    GroupedSearchPopup* m_popup{nullptr};
};

/// Each visible submenu of @p popup: its title, " [checked]" when checked, and its items, "[x] "
/// before a checked one.
[[nodiscard]] std::vector<std::string> groupLines(const GroupedSearchPopup& popup)
{
    std::vector<std::string> lines;
    for (const QAction* action : popup.actions())
    {
        const QMenu* menu = action->menu();
        if (menu == nullptr || !action->isVisible())
        {
            continue;
        }
        QString line =
            action->text() + (action->isChecked() ? QStringLiteral(" [checked]") : QString());
        QString separator = QStringLiteral(": ");
        for (const QAction* item : menu->actions())
        {
            line +=
                separator + (item->isChecked() ? QStringLiteral("[x] ") : QString()) + item->text();
            separator = QStringLiteral(" | ");
        }
        lines.push_back(QtStrings::toUtf8(line));
    }
    return lines;
}

/// The rows of the search list of @p popup.
[[nodiscard]] std::vector<std::string> searchRows(const GroupedSearchPopup& popup)
{
    std::vector<std::string> rows;
    rows.reserve(static_cast<std::size_t>(popup.searchList().count()));
    for (int row = 0; row < popup.searchList().count(); ++row)
    {
        rows.push_back(QtStrings::toUtf8(popup.searchList().item(row)->text()));
    }
    return rows;
}

TEST_F(GroupedSearchPopupTest, NeedsEveryFunctionOfItsContent)
{
    const GroupedSearchPopup::Content content{
        .itemCount    = [] { return 0; },
        .text         = [](int) { return QString(); },
        .sortKey      = [](int) { return std::string(); },
        .groups       = [] { return std::vector<GroupedSearchPopup::Group>(); },
        .current      = [] { return -1; },
        .currentGroup = [] { return std::optional<std::size_t>(); },
        .choose       = nullptr,
    };
    EXPECT_THROW(
        static_cast<void>(GroupedSearchPopup(QString(), std::vector<QString>{}, content, nullptr)),
        BugError);
}

TEST_F(GroupedSearchPopupTest, MakesASubmenuPerGroupAndShowsThoseWithItems)
{
    Owner owner;
    ASSERT_TRUE(showActive(owner.window()));
    const OffscreenPopupWarnings warnings;
    owner.popup().popUpBelow(owner.window());
    EXPECT_TRUE(owner.popup().isVisible());
    EXPECT_EQ(owner.popup().searchField().placeholderText(), QStringLiteral("Search here"));
    EXPECT_TRUE(owner.popup().searchField().hasFocus());
    EXPECT_FALSE(owner.popup().isSearching());
    EXPECT_EQ(groupLines(owner.popup()),
              (std::vector<std::string>{"Fruit [checked]: Banana | [x] apple",
                                        "Stone: (x) Cherry | Avocado"}));
    EXPECT_EQ(owner.popup().groupMenu(2).title(), QStringLiteral("Empty"));
    EXPECT_THROW(std::ignore = owner.popup().groupMenu(3), BugError);

    // The value's group is checked even when the value is none of the items.
    owner.current      = -1;
    owner.currentGroup = 2;
    owner.popup().refill();
    EXPECT_EQ(groupLines(owner.popup()),
              (std::vector<std::string>{"Fruit: Banana | apple", "Stone: (x) Cherry | Avocado"}));
    EXPECT_TRUE(owner.popup().groupMenu(2).menuAction()->isChecked());
    EXPECT_FALSE(owner.popup().groupMenu(2).menuAction()->isVisible());
}

TEST_F(GroupedSearchPopupTest, TheSearchSortsByTheKeyIgnoringCase)
{
    Owner owner;
    ASSERT_TRUE(showActive(owner.window()));
    const OffscreenPopupWarnings warnings;
    owner.popup().popUpBelow(owner.window());
    QTest::keyClicks(&owner.popup().searchField(), QStringLiteral("A"));
    EXPECT_TRUE(owner.popup().isSearching());
    EXPECT_TRUE(groupLines(owner.popup()).empty());
    // Containing "a" ignoring case, sorted by the key: apple, avocado, banana.
    EXPECT_EQ(searchRows(owner.popup()),
              (std::vector<std::string>{"\u2713 apple", "Avocado", "Banana"}));
    EXPECT_EQ(owner.popup().searchRows(), (std::vector<int>{1, 3, 0}));
    EXPECT_EQ(owner.popup().searchList().currentRow(), 0);
    // The text is searched, the key sorts: "(x)" is in the text only.
    owner.popup().searchField().clear();
    QTest::keyClicks(&owner.popup().searchField(), QStringLiteral("(X)"));
    EXPECT_EQ(searchRows(owner.popup()), (std::vector<std::string>{"(x) Cherry"}));
    QTest::keyClicks(&owner.popup().searchField(), QStringLiteral("zz"));
    EXPECT_TRUE(searchRows(owner.popup()).empty());
    EXPECT_EQ(owner.popup().searchList().currentRow(), -1);
}

TEST_F(GroupedSearchPopupTest, AnEmptySearchBringsTheGroupsBack)
{
    Owner owner;
    ASSERT_TRUE(showActive(owner.window()));
    const OffscreenPopupWarnings warnings;
    owner.popup().popUpBelow(owner.window());
    QTest::keyClicks(&owner.popup().searchField(), QStringLiteral("b"));
    EXPECT_TRUE(owner.popup().searchList().isVisible());
    QTest::keyClick(&owner.popup().searchField(), Qt::Key_Backspace);
    EXPECT_FALSE(owner.popup().isSearching());
    EXPECT_FALSE(owner.popup().searchList().isVisible());
    EXPECT_EQ(groupLines(owner.popup()).size(), 2U);
}

TEST_F(GroupedSearchPopupTest, AChoiceClosesThePopupFirst)
{
    Owner owner;
    ASSERT_TRUE(showActive(owner.window()));
    const OffscreenPopupWarnings warnings;
    owner.popup().popUpBelow(owner.window());
    owner.popup().groupMenu(1).actions().at(1)->trigger();  // Avocado
    owner.popup().popUpBelow(owner.window());
    QTest::keyClicks(&owner.popup().searchField(), QStringLiteral("an"));
    QTest::keyClick(&owner.popup().searchField(), Qt::Key_Return);  // Banana
    owner.popup().popUpBelow(owner.window());
    QTest::keyClicks(&owner.popup().searchField(), QStringLiteral("p"));
    Q_EMIT owner.popup().searchList().itemClicked(owner.popup().searchList().item(0));  // apple
    EXPECT_EQ(owner.chosen, (std::vector<int>{3, 0, 1}));
    EXPECT_EQ(owner.visibleWhenChosen, (std::vector<bool>{false, false, false}));
    EXPECT_FALSE(owner.popup().isVisible());
}

TEST_F(GroupedSearchPopupTest, AnErrorOfAChoiceIsReportedWithTheOwnersName)
{
    Owner owner;
    ASSERT_TRUE(showActive(owner.window()));
    owner.onChoice = [] { bug("a broken choice"); };
    const RecordingGuardHook     hook;
    const OffscreenPopupWarnings warnings;
    owner.popup().popUpBelow(owner.window());
    owner.popup().groupMenu(0).actions().at(0)->trigger();
    ASSERT_EQ(hook.failures().size(), 1U);
    EXPECT_EQ(hook.failures().front().context, QStringLiteral("owner"));
    EXPECT_EQ(owner.chosen, std::vector<int>{0});
}

TEST_F(GroupedSearchPopupTest, AChoiceMayDestroyThePopup)
{
    // The popup uses nothing of itself after it handed the item over (the owner's dialog may go
    // with the choice).
    auto owner = std::make_unique<Owner>();
    ASSERT_TRUE(showActive(owner->window()));
    const OffscreenPopupWarnings warnings;
    owner->onChoice = [&owner] { owner.reset(); };
    owner->popup().popUpBelow(owner->window());
    QTest::keyClicks(&owner->popup().searchField(), QStringLiteral("che"));
    Q_EMIT owner->popup().searchList().itemClicked(owner->popup().searchList().item(0));
    EXPECT_EQ(owner, nullptr);
}

TEST_F(GroupedSearchPopupTest, RefillsOnlyWhileOpen)
{
    Owner owner;
    ASSERT_TRUE(showActive(owner.window()));
    owner.popup().refill();  // closed: nothing filled
    EXPECT_TRUE(owner.popup().groupMenu(0).isEmpty());
    const OffscreenPopupWarnings warnings;
    owner.popup().popUpBelow(owner.window());
    QTest::keyClicks(&owner.popup().searchField(), QStringLiteral("a"));
    owner.items.push_back({.text = QStringLiteral("Apricot"), .sortKey = "apricot", .group = 1});
    owner.popup().refill();
    EXPECT_EQ(searchRows(owner.popup()),
              (std::vector<std::string>{"\u2713 apple", "Apricot", "Avocado", "Banana"}));
    EXPECT_EQ(owner.popup().groupMenu(1).actions().size(), 3);
}

}  // namespace

}  // namespace QtRocket::Gui
