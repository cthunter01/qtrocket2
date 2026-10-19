#include "QtRocket/gui/main/MainMenus.h"

#include <array>
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <QAction>
#include <QIcon>
#include <QImage>
#include <QKeySequence>
#include <QList>
#include <QMenu>
#include <QMenuBar>
#include <QSize>
#include <QString>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "TestGuiServices.h"

namespace QtRocket::Gui
{

namespace
{

using MainMenusTest = Test::GuiTest;
using Texts         = std::vector<std::string>;

/// The name of the item @p action: its object name, or its submenu's.
[[nodiscard]] std::string itemName(const QAction& action)
{
    const QMenu* const submenu = action.menu();
    return QtStrings::toUtf8(submenu != nullptr ? submenu->objectName() : action.objectName());
}

/// The name of each item of @p menu (itemName()), "---" for a separator.
[[nodiscard]] Texts itemNames(const QMenu& menu)
{
    Texts names;
    for (const QAction* action : menu.actions())
    {
        names.push_back(action->isSeparator() ? std::string("---") : itemName(*action));
    }
    return names;
}

/// The text of each item of @p menu, "---" for a separator.
[[nodiscard]] Texts itemTexts(const QMenu& menu)
{
    Texts texts;
    for (const QAction* action : menu.actions())
    {
        texts.push_back(action->isSeparator() ? std::string("---")
                                              : QtStrings::toUtf8(action->text()));
    }
    return texts;
}

/// The object names of the menus of @p menuBar that are shown.
[[nodiscard]] Texts shownMenus(const QMenuBar& menuBar)
{
    Texts names;
    for (const QAction* action : menuBar.actions())
    {
        if (action->isVisible() && action->menu() != nullptr)
        {
            names.push_back(QtStrings::toUtf8(action->menu()->objectName()));
        }
    }
    return names;
}

/// The keys of @p action, as text.
[[nodiscard]] std::string keys(const QAction& action)
{
    return QtStrings::toUtf8(QKeySequence::listToString(action.shortcuts()));
}

/// The platform's keys of @p key, as text.
[[nodiscard]] std::string standardKeys(QKeySequence::StandardKey key)
{
    return QtStrings::toUtf8(QKeySequence::listToString(QKeySequence::keyBindings(key)));
}

/// Ctrl (Cmd on macOS) and @p key, as text.
[[nodiscard]] std::string menuKey(Qt::Key key)
{
    return QtStrings::toUtf8(QKeySequence(Qt::CTRL | key).toString());
}

/// Every action of @p menu and its submenus, the submenus' own actions included.
void collectActions(const QMenu& menu, std::vector<const QAction*>& actions)
{
    for (const QAction* action : menu.actions())
    {
        if (action->isSeparator())
        {
            continue;
        }
        actions.push_back(action);
        if (const QMenu* submenu = action->menu(); submenu != nullptr)
        {
            collectActions(*submenu, actions);
        }
    }
}

/// The names of those of @p actions that are not window shortcuts.
[[nodiscard]] Texts notWindowShortcuts(const std::vector<const QAction*>& actions)
{
    Texts names;
    for (const QAction* action : actions)
    {
        if (action->shortcutContext() != Qt::WindowShortcut)
        {
            names.push_back(itemName(*action));
        }
    }
    return names;
}

/// "<name> <role>" of those of @p actions that have a menu role.
[[nodiscard]] Texts menuRoles(const std::vector<const QAction*>& actions)
{
    Texts roles;
    for (const QAction* action : actions)
    {
        if (action->menuRole() != QAction::NoRole)
        {
            roles.push_back(itemName(*action) + " " + std::to_string(action->menuRole()));
        }
    }
    return roles;
}

/// The names of those of @p actions that are enabled.
[[nodiscard]] Texts enabledItems(const std::vector<const QAction*>& actions)
{
    Texts names;
    for (const QAction* action : actions)
    {
        if (action->isEnabled())
        {
            names.push_back(itemName(*action));
        }
    }
    return names;
}

/// The names of those of @p actions that have no icon.
[[nodiscard]] Texts withoutIcon(const std::vector<const QAction*>& actions)
{
    Texts names;
    for (const QAction* action : actions)
    {
        if (action->icon().isNull())
        {
            names.push_back(itemName(*action));
        }
    }
    return names;
}

/// The keys that more than one of @p actions has, each as "<key>: <name> <name> ...".
[[nodiscard]] Texts sharedKeys(const std::vector<const QAction*>& actions)
{
    std::map<std::string, std::string> owners;
    std::map<std::string, int>         counts;
    for (const QAction* action : actions)
    {
        for (const QKeySequence& key : action->shortcuts())
        {
            const std::string text = QtStrings::toUtf8(key.toString());
            owners[text] += " " + itemName(*action);
            ++counts[text];
        }
    }
    Texts shared;
    for (const auto& [key, count] : counts)
    {
        if (count > 1)
        {
            shared.push_back(key + ":" + owners[key]);
        }
    }
    return shared;
}

/// An item as OpenRocket has it (BasicFrame, RocketActions, UndoRedoAction, messages.properties):
/// its text with the mnemonic marked, its accessible description (here the status tip; empty for
/// none), its tooltip (empty for none of its own: Qt's of the text) and its icon.
struct Item
{
    std::string_view         name;
    std::string_view         text;
    std::string_view         statusTip;
    std::string_view         toolTip;
    std::optional<Icons::Id> icon;
};

constexpr Item item(std::string_view name, std::string_view text, std::string_view statusTip,
                    std::string_view toolTip, std::optional<Icons::Id> icon)
{
    return {.name = name, .text = text, .statusTip = statusTip, .toolTip = toolTip, .icon = icon};
}

using Id = Icons::Id;

// Every item of a design window's File and Edit menus, in their order.
constexpr std::array kDesignWindowItems{
    item("newAction", "&New", "Create a new rocket design", "", Id::FILE_NEW),
    item("openAction", "&Open\u2026", "Open a rocket design", "", Id::FILE_OPEN),
    item("openRecentMenu", "Open recent", "Open a recent rocket design", "", Id::FILE_OPEN_RECENT),
    item("openExampleMenu", "Open example", "Open an example rocket design", "",
         Id::FILE_OPEN_EXAMPLE),
    item("saveAction", "&Save", "Save the current rocket design", "", Id::FILE_SAVE),
    item("saveAsAction", "S&ave as\u2026", "Save the current rocket design to a new file", "",
         Id::FILE_SAVE_AS),
    item("propertiesAction", "Propert&ies", "View the file properties", "", Id::CONFIGURE),
    item("closeAction", "&Close design", "Close the current rocket design", "", Id::FILE_CLOSE),
    item("quitAction", "&Quit", "Quit the program", "", Id::FILE_QUIT),
    item("undoAction", "&Undo", "Undo the previous operation", "", Id::EDIT_UNDO),
    item("redoAction", "&Redo", "Redo the previously undone operation", "", Id::EDIT_REDO),
    item("editAction", "&Edit", "", "Edit the selected component.", Id::EDIT_EDIT),
    item("cutAction", "Cu&t", "",
         "Cut these components or simulations to the clipboard and delete from this design",
         Id::EDIT_CUT),
    item("copyAction", "&Copy", "", "Copy these components (and subcomponents) to the clipboard.",
         Id::EDIT_COPY),
    item("pasteAction", "Paste", "",
         "Paste the components or simulations on the clipboard to the design.", Id::EDIT_PASTE),
    item("duplicateAction", "&Duplicate", "", "Duplicate this component (and subcomponents).",
         Id::EDIT_DUPLICATE),
    item("deleteAction", "Delete", "", "Delete the selected components or simulations.",
         Id::EDIT_DELETE),
    item("selectMenu", "Select", "", "", std::nullopt),
    item("deselectAllAction", "Deselect all", "", "Deselect all currently selected components.",
         std::nullopt),
    item("visibilityMenu", "Visibility", "", "", std::nullopt),
    item("toggleVisibilityAction", "Hide selected", "", "Hide selected component(s).",
         Id::COMPONENT_HIDDEN),
    item("showAllComponentsAction", "Show all", "", "Show all components.", Id::COMPONENT_SHOWING),
    item("preferencesAction", "Preferences", "Setup the application preferences", "",
         Id::PREFERENCES),
};

/// @p icon drawn at 16 pixels (two icons are the same when they draw the same).
[[nodiscard]] QImage drawnIcon(const QIcon& icon)
{
    return icon.pixmap(QSize(16, 16)).toImage();
}

/// What of @p action differs from @p expected, one text each.
[[nodiscard]] Texts differences(const QAction& action, const Item& expected)
{
    Texts      found;
    const auto name = std::string(expected.name);
    const auto text = QtStrings::fromUtf8(expected.text);
    // An action without a tooltip of its own shows Qt's of its text.
    const QAction plain(text);
    const QString toolTip =
        expected.toolTip.empty() ? plain.toolTip() : QtStrings::fromUtf8(expected.toolTip);
    if (action.text() != text)
    {
        found.push_back(name + " text: " + QtStrings::toUtf8(action.text()));
    }
    if (action.statusTip() != QtStrings::fromUtf8(expected.statusTip))
    {
        found.push_back(name + " status tip: " + QtStrings::toUtf8(action.statusTip()));
    }
    if (action.toolTip() != toolTip)
    {
        found.push_back(name + " tooltip: " + QtStrings::toUtf8(action.toolTip()));
    }
    const bool sameIcon = expected.icon.has_value()
                              ? drawnIcon(action.icon()) == drawnIcon(Icons::icon(*expected.icon))
                              : action.icon().isNull();
    if (!sameIcon)
    {
        found.push_back(name + " icon");
    }
    return found;
}

/// What of @p actions differs from @p expected, item by item and in order.
[[nodiscard]] Texts itemDifferences(const std::vector<const QAction*>& actions,
                                    const std::vector<Item>&           expected)
{
    Texts found;
    if (actions.size() != expected.size())
    {
        found.push_back(std::to_string(actions.size()) + " items");
    }
    for (std::size_t index = 0; index < actions.size() && index < expected.size(); ++index)
    {
        const QAction& action = *actions.at(index);
        const Item&    wanted = expected.at(index);
        if (itemName(action) != wanted.name)
        {
            found.push_back(itemName(action) + " in the place of " + std::string(wanted.name));
            continue;
        }
        const Texts more = differences(action, wanted);
        found.insert(found.end(), more.begin(), more.end());
    }
    return found;
}

/// A design window's menus: those of a window of a new design.
class DesignWindowMenusTest : public Test::GuiTest
{
protected:
    void SetUp() override
    {
        GuiTest::SetUp();
        m_document = OpenRocketDocumentFactory::createNewRocket();
        m_window   = std::make_unique<MainWindow>(*m_document, m_services.services());
    }

    void TearDown() override
    {
        m_window.reset();
        m_document.reset();
        GuiTest::TearDown();
    }

    [[nodiscard]] const MainMenus&  menus() const { return m_window->menus(); }
    [[nodiscard]] const MainWindow& window() const { return *m_window; }

    /// Every action of the menus, in order.
    [[nodiscard]] std::vector<const QAction*> allActions() const
    {
        std::vector<const QAction*> actions;
        collectActions(*menus().fileMenu(), actions);
        collectActions(*menus().editMenu(), actions);
        return actions;
    }

private:
    Gui::Test::TestGuiServices          m_services;
    std::unique_ptr<OpenRocketDocument> m_document;
    std::unique_ptr<MainWindow>         m_window;
};

/// The menus of no window, in their own menu bar.
struct NoWindowMenuBar
{
    QMenuBar   menuBar;
    MainMenus* menus = makeChild<MainMenus>(&menuBar);
};

TEST_F(DesignWindowMenusTest, HasFileEditAndAnEmptyHiddenHelpMenu)
{
    EXPECT_TRUE(menus().isDesignWindowMenus());
    EXPECT_EQ(shownMenus(*menus().menuBar()), (Texts{"fileMenu", "editMenu"}));
    ASSERT_NE(menus().helpMenu(), nullptr);
    EXPECT_TRUE(menus().helpMenu()->isEmpty());
    EXPECT_FALSE(menus().helpMenu()->menuAction()->isVisible());
    EXPECT_EQ(QtStrings::toUtf8(menus().fileMenu()->title()), "&File");
    EXPECT_EQ(QtStrings::toUtf8(menus().editMenu()->title()), "&Edit");
    EXPECT_EQ(QtStrings::toUtf8(menus().helpMenu()->title()), "&Help");
    EXPECT_EQ(menus().fileMenu()->accessibleDescription(),
              QStringLiteral("File-handling related tasks"));
    EXPECT_EQ(menus().editMenu()->accessibleDescription(), QStringLiteral("Rocket editing"));
    EXPECT_EQ(menus().helpMenu()->accessibleDescription(),
              QStringLiteral("Information about QtRocket"));
}

TEST_F(DesignWindowMenusTest, FileMenuHasOpenRocketsItemsInItsOrder)
{
    EXPECT_EQ(
        itemNames(*menus().fileMenu()),
        (Texts{"newAction", "openAction", "openRecentMenu", "openExampleMenu", "---", "saveAction",
               "saveAsAction", "---", "propertiesAction", "closeAction", "---", "quitAction"}));
    EXPECT_EQ(itemTexts(*menus().fileMenu()),
              (Texts{"&New", "&Open\u2026", "Open recent", "Open example", "---", "&Save",
                     "S&ave as\u2026", "---", "Propert&ies", "&Close design", "---", "&Quit"}));
}

TEST_F(DesignWindowMenusTest, EditMenuHasOpenRocketsItemsInItsOrder)
{
    EXPECT_EQ(itemNames(*menus().editMenu()),
              (Texts{"undoAction", "redoAction", "---", "editAction", "cutAction", "copyAction",
                     "pasteAction", "duplicateAction", "deleteAction", "---", "selectMenu", "---",
                     "visibilityMenu", "---", "preferencesAction"}));
    EXPECT_EQ(itemTexts(*menus().editMenu()),
              (Texts{"&Undo", "&Redo", "---", "&Edit", "Cu&t", "&Copy", "Paste", "&Duplicate",
                     "Delete", "---", "Select", "---", "Visibility", "---", "Preferences"}));
    // Components of same color comes with Milestone 5.
    EXPECT_EQ(itemNames(*menus().selectMenu()), (Texts{"deselectAllAction"}));
    EXPECT_EQ(itemTexts(*menus().selectMenu()), (Texts{"Deselect all"}));
    EXPECT_EQ(itemNames(*menus().visibilityMenu()),
              (Texts{"toggleVisibilityAction", "showAllComponentsAction"}));
    EXPECT_EQ(itemTexts(*menus().visibilityMenu()), (Texts{"Hide selected", "Show all"}));
}

TEST_F(DesignWindowMenusTest, AccessorsGiveTheItemsOfTheMenus)
{
    const MainMenus& m = menus();
    EXPECT_EQ(m.newAction()->objectName(), QStringLiteral("newAction"));
    EXPECT_EQ(m.openAction()->objectName(), QStringLiteral("openAction"));
    EXPECT_EQ(m.openRecentMenu()->objectName(), QStringLiteral("openRecentMenu"));
    EXPECT_EQ(m.openExampleMenu()->objectName(), QStringLiteral("openExampleMenu"));
    EXPECT_EQ(m.saveAction()->objectName(), QStringLiteral("saveAction"));
    EXPECT_EQ(m.saveAsAction()->objectName(), QStringLiteral("saveAsAction"));
    EXPECT_EQ(m.propertiesAction()->objectName(), QStringLiteral("propertiesAction"));
    EXPECT_EQ(m.closeAction()->objectName(), QStringLiteral("closeAction"));
    EXPECT_EQ(m.quitAction()->objectName(), QStringLiteral("quitAction"));
    EXPECT_EQ(m.undoAction()->objectName(), QStringLiteral("undoAction"));
    EXPECT_EQ(m.redoAction()->objectName(), QStringLiteral("redoAction"));
    EXPECT_EQ(m.editAction()->objectName(), QStringLiteral("editAction"));
    EXPECT_EQ(m.cutAction()->objectName(), QStringLiteral("cutAction"));
    EXPECT_EQ(m.copyAction()->objectName(), QStringLiteral("copyAction"));
    EXPECT_EQ(m.pasteAction()->objectName(), QStringLiteral("pasteAction"));
    EXPECT_EQ(m.duplicateAction()->objectName(), QStringLiteral("duplicateAction"));
    EXPECT_EQ(m.deleteAction()->objectName(), QStringLiteral("deleteAction"));
    EXPECT_EQ(m.selectMenu()->objectName(), QStringLiteral("selectMenu"));
    EXPECT_EQ(m.deselectAllAction()->objectName(), QStringLiteral("deselectAllAction"));
    EXPECT_EQ(m.visibilityMenu()->objectName(), QStringLiteral("visibilityMenu"));
    EXPECT_EQ(m.toggleVisibilityAction()->objectName(), QStringLiteral("toggleVisibilityAction"));
    EXPECT_EQ(m.showAllComponentsAction()->objectName(), QStringLiteral("showAllComponentsAction"));
    EXPECT_EQ(m.preferencesAction()->objectName(), QStringLiteral("preferencesAction"));
    EXPECT_EQ(m.helpMenu()->objectName(), QStringLiteral("helpMenu"));
    EXPECT_EQ(m.menuBar(), window().menuBar());
}

TEST_F(DesignWindowMenusTest, ItemsHaveTheStandardKeysOrMenuKeys)
{
    const MainMenus& m = menus();
    EXPECT_EQ(keys(*m.newAction()), standardKeys(QKeySequence::New));
    EXPECT_EQ(keys(*m.openAction()), standardKeys(QKeySequence::Open));
    EXPECT_EQ(keys(*m.saveAction()), standardKeys(QKeySequence::Save));
    EXPECT_EQ(keys(*m.saveAsAction()), standardKeys(QKeySequence::SaveAs));
    EXPECT_EQ(keys(*m.propertiesAction()), menuKey(Qt::Key_I));
    EXPECT_EQ(keys(*m.closeAction()), standardKeys(QKeySequence::Close));
    EXPECT_EQ(keys(*m.quitAction()), menuKey(Qt::Key_Q));
    EXPECT_EQ(keys(*m.undoAction()), standardKeys(QKeySequence::Undo));
    EXPECT_EQ(keys(*m.redoAction()), standardKeys(QKeySequence::Redo));
    EXPECT_EQ(keys(*m.editAction()), menuKey(Qt::Key_E));
    EXPECT_EQ(keys(*m.cutAction()), standardKeys(QKeySequence::Cut));
    EXPECT_EQ(keys(*m.copyAction()), standardKeys(QKeySequence::Copy));
    EXPECT_EQ(keys(*m.pasteAction()), standardKeys(QKeySequence::Paste));
    EXPECT_EQ(keys(*m.duplicateAction()), menuKey(Qt::Key_D));
    // RocketActions' Delete key alone, whatever the platform's key scheme.
    EXPECT_EQ(m.deleteAction()->shortcuts(), QList<QKeySequence>{QKeySequence(Qt::Key_Delete)});
    EXPECT_EQ(keys(*m.deselectAllAction()), "");
    EXPECT_EQ(keys(*m.toggleVisibilityAction()), "");
    EXPECT_EQ(keys(*m.showAllComponentsAction()), "");
    EXPECT_EQ(keys(*m.preferencesAction()), "");
    EXPECT_FALSE(m.newAction()->shortcuts().isEmpty());
    EXPECT_FALSE(m.closeAction()->shortcuts().isEmpty());
}

TEST_F(DesignWindowMenusTest, NoTwoItemsShareAKey)
{
    // An ambiguous window shortcut triggers neither item (Qt warns "Ambiguous shortcut overload").
    EXPECT_EQ(sharedKeys(allActions()), Texts{});
}

TEST_F(DesignWindowMenusTest, EveryItemHasOpenRocketsTextDescriptionTooltipAndIcon)
{
    const std::vector<Item> items(kDesignWindowItems.begin(), kDesignWindowItems.end());
    EXPECT_EQ(itemDifferences(allActions(), items), Texts{});
}

TEST_F(DesignWindowMenusTest, EveryItemIsAWindowShortcutWithoutAMenuRoleButPreferencesAndQuit)
{
    EXPECT_EQ(notWindowShortcuts(allActions()), Texts{});
    EXPECT_EQ(menuRoles(allActions()),
              (Texts{"quitAction " + std::to_string(QAction::QuitRole),
                     "preferencesAction " + std::to_string(QAction::PreferencesRole)}));
}

TEST_F(DesignWindowMenusTest, OnlyNewCloseAndQuitAreEnabledInANewDesign)
{
    EXPECT_EQ(enabledItems(allActions()),
              (Texts{"newAction", "closeAction", "quitAction", "selectMenu", "visibilityMenu"}));
    EXPECT_TRUE(menus().openRecentMenu()->isEmpty());
    EXPECT_TRUE(menus().openExampleMenu()->isEmpty());
}

TEST_F(DesignWindowMenusTest, ItemsHaveOpenRocketsIconsDescriptionsAndTooltips)
{
    const MainMenus& m = menus();
    // OpenRocket's items without an icon: the submenus Select and Visibility, and Select's items.
    EXPECT_EQ(withoutIcon(allActions()),
              (Texts{"selectMenu", "deselectAllAction", "visibilityMenu"}));
    EXPECT_EQ(m.newAction()->statusTip(), QStringLiteral("Create a new rocket design"));
    EXPECT_EQ(m.openRecentMenu()->menuAction()->statusTip(),
              QStringLiteral("Open a recent rocket design"));
    EXPECT_EQ(m.propertiesAction()->statusTip(), QStringLiteral("View the file properties"));
    EXPECT_EQ(m.quitAction()->statusTip(), QStringLiteral("Quit the program"));
    EXPECT_EQ(m.preferencesAction()->statusTip(),
              QStringLiteral("Setup the application preferences"));
    EXPECT_EQ(m.editAction()->toolTip(), QStringLiteral("Edit the selected component."));
    EXPECT_EQ(m.cutAction()->toolTip(),
              QStringLiteral("Cut these components or simulations to the clipboard and delete "
                             "from this design"));
    EXPECT_EQ(m.deleteAction()->toolTip(),
              QStringLiteral("Delete the selected components or simulations."));
    EXPECT_EQ(m.toggleVisibilityAction()->toolTip(), QStringLiteral("Hide selected component(s)."));
    EXPECT_TRUE(m.editMenu()->toolTipsVisible());
    EXPECT_TRUE(m.selectMenu()->toolTipsVisible());
    EXPECT_TRUE(m.visibilityMenu()->toolTipsVisible());
}

TEST_F(MainMenusTest, MenusOfNoWindowHaveFileAndHelp)
{
    const NoWindowMenuBar bar;
    const MainMenus&      m = *bar.menus;
    EXPECT_FALSE(m.isDesignWindowMenus());
    EXPECT_EQ(m.menuBar(), &bar.menuBar);
    EXPECT_EQ(shownMenus(bar.menuBar), (Texts{"fileMenu"}));
    EXPECT_FALSE(m.helpMenu()->menuAction()->isVisible());
    EXPECT_EQ(itemNames(*m.fileMenu()), (Texts{"newAction", "openAction", "openRecentMenu",
                                               "openExampleMenu", "---", "quitAction"}));
    EXPECT_TRUE(m.newAction()->isEnabled());
    EXPECT_TRUE(m.quitAction()->isEnabled());
    EXPECT_FALSE(m.openAction()->isEnabled());
    EXPECT_EQ(m.quitAction()->menuRole(), QAction::QuitRole);
    EXPECT_EQ(m.saveAction(), nullptr);
    EXPECT_EQ(m.closeAction(), nullptr);
    EXPECT_EQ(m.editMenu(), nullptr);
    EXPECT_EQ(m.undoAction(), nullptr);
    EXPECT_EQ(m.preferencesAction(), nullptr);
}

}  // namespace

}  // namespace QtRocket::Gui
