#include "QtRocket/gui/main/RecentFilesMenu.h"

#include <filesystem>
#include <memory>
#include <optional>

#include <QAction>
#include <QMenu>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "FileMenusTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/main/DocumentFileOperations.h"
#include "QtRocket/gui/main/MainMenus.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/main/RecentFiles.h"
#include "QtRocket/gui/main/WindowManager.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/rocket/Rocket.h"
#include "ScriptedUserPrompts.h"

// File > Open recent (MRUDesignFileAction) in the windows of a manager with its file operations;
// the questions are answered by ScriptedUserPrompts.

namespace QtRocket::Gui
{

namespace
{

using RecentFilesMenuTest = Gui::Test::FileMenusTest;
using PromptKind          = Gui::Test::ScriptedUserPrompts::Kind;

/// Fills the Open recent menu of @p menus with @p recentFiles, opened by @p operations from
/// @p window.
RecentFilesMenu& fillRecentMenu(MainMenus const& menus, RecentFiles& recentFiles,
                                DocumentFileOperations& operations, MainWindow* window)
{
    auto* filler =
        makeChild<RecentFilesMenu>(menus.openRecentMenu(), recentFiles, operations, window);
    filler->setObjectName(QStringLiteral("recentFilesMenu"));
    return *filler;
}

/// A window of a design that no New or Open replaces.
[[nodiscard]] MainWindow& loadedWindow(WindowManager& manager)
{
    return manager.addDocument(OpenRocketDocumentFactory::createNewRocket());
}

TEST_F(RecentFilesMenuTest, ListsTheRecentFilesNewestFirstAndFollowsTheirChanges)
{
    RecentFiles recent(preferences());
    recent.addFile(QStringLiteral("/r/a.ork"));
    recent.addFile(QStringLiteral("/r/b & c.ork"));
    MainWindow&        window = manager().newDesign();
    const QMenu* const menu   = window.menus().openRecentMenu();
    EXPECT_FALSE(menu->menuAction()->isEnabled());

    const RecentFilesMenu& filler = fillRecentMenu(window.menus(), recent, operations(), &window);
    EXPECT_EQ(filler.menu(), menu);
    EXPECT_EQ(filler.parent(), menu);
    EXPECT_TRUE(menu->menuAction()->isEnabled());
    // The paths as they are (an '&' shows as one), no icon, no menu role.
    EXPECT_EQ(Gui::Test::menuTexts(*menu),
              (QStringList{QStringLiteral("/r/b && c.ork"), QStringLiteral("/r/a.ork")}));
    const QAction* const first = menu->actions().front();
    EXPECT_TRUE(first->icon().isNull());
    EXPECT_EQ(first->menuRole(), QAction::NoRole);
    EXPECT_TRUE(first->isEnabled());

    recent.addFile(QStringLiteral("/r/d.ork"));
    EXPECT_EQ(Gui::Test::menuTexts(*menu),
              (QStringList{QStringLiteral("/r/d.ork"), QStringLiteral("/r/b && c.ork"),
                           QStringLiteral("/r/a.ork")}));
    recent.removeFile(QStringLiteral("/r/b & c.ork"));
    EXPECT_EQ(Gui::Test::menuTexts(*menu),
              (QStringList{QStringLiteral("/r/d.ork"), QStringLiteral("/r/a.ork")}));
    // Empty, it stays enabled (a JMenu).
    recent.setFiles({});
    EXPECT_TRUE(menu->actions().isEmpty());
    EXPECT_TRUE(menu->menuAction()->isEnabled());
    Gui::Test::FileMenusTest::deleteLaterNow();
}

TEST_F(RecentFilesMenuTest, ChoosingAFileOpensItAndPutsItOnTop)
{
    // MRUDesignFileAction: BasicFrame.open(new File(path), parent), then addFile(path).
    const QString alpha = QtStrings::fromPath(writeDesign("alpha.ork", "Alpha"));
    const QString beta  = QtStrings::fromPath(writeDesign("beta.ork", "Beta"));
    RecentFiles   recent(preferences());
    recent.setFiles({alpha, beta});
    MainWindow&        origin = loadedWindow(manager());
    const QMenu* const menu   = origin.menus().openRecentMenu();
    static_cast<void>(fillRecentMenu(origin.menus(), recent, operations(), &origin));

    QAction* const item = Gui::Test::menuItem(*menu, beta);
    ASSERT_NE(item, nullptr);
    item->trigger();
    Gui::Test::FileMenusTest::deleteLaterNow();
    EXPECT_TRUE(asked().empty());
    ASSERT_EQ(manager().windowCount(), 2U);
    const MainWindow& opened = *manager().windows().back();
    EXPECT_EQ(opened.document().getRocket().getName(), "Beta");
    EXPECT_EQ(opened.document().getFile(), std::optional(QtStrings::toPath(beta)));
    EXPECT_EQ(recent.files(), (QStringList{beta, alpha}));
    EXPECT_EQ(Gui::Test::menuTexts(*menu), (QStringList{beta, alpha}));
}

TEST_F(RecentFilesMenuTest, AFileThatCannotBeOpenedIsTakenOutOfTheList)
{
    // MRUDesignFileAction: removeFile(path) when BasicFrame.open() gives null.
    const QString alpha   = QtStrings::fromPath(writeDesign("alpha.ork", "Alpha"));
    const QString missing = QtStrings::fromPath(directory().resolve("missing.ork"));
    RecentFiles   recent(preferences());
    recent.setFiles({missing, alpha});
    MainWindow&            origin = loadedWindow(manager());
    const RecentFilesMenu& filler = fillRecentMenu(origin.menus(), recent, operations(), &origin);

    QAction* const item = Gui::Test::menuItem(*filler.menu(), missing);
    ASSERT_NE(item, nullptr);
    item->trigger();
    Gui::Test::FileMenusTest::deleteLaterNow();
    ASSERT_EQ(asked().size(), 1U);
    EXPECT_EQ(asked().front().kind, PromptKind::MESSAGE);
    EXPECT_EQ(asked().front().title, QStringLiteral("Error opening file"));
    EXPECT_EQ(asked().front().texts, (QStringList{QStringLiteral("ERROR"),
                                                  QStringLiteral("File not found: missing.ork")}));
    EXPECT_EQ(manager().windowCount(), 1U);
    EXPECT_EQ(recent.files(), QStringList{alpha});
    EXPECT_EQ(Gui::Test::menuTexts(*filler.menu()), QStringList{alpha});
}

TEST_F(RecentFilesMenuTest, TheListIsUpdatedWhenTheMenuGoesWithItsWindowDuringTheOpen)
{
    // The window of the menu is closed and destroyed while the open's message is shown (another
    // window's event; on macOS a design file the Finder opens, or the Dock's Quit): the menu goes
    // with it, and the file is taken out of the list all the same, as MRUDesignFileAction updates
    // the list whatever became of the menu (ASan checks that nothing of the menu is used after
    // the open).
    const QString alpha   = QtStrings::fromPath(writeDesign("alpha.ork", "Alpha"));
    const QString missing = QtStrings::fromPath(directory().resolve("missing.ork"));
    RecentFiles   recent(preferences());
    recent.setFiles({missing, alpha});
    MainWindow&                     origin = loadedWindow(manager());
    const QPointer<RecentFilesMenu> filler(
        &fillRecentMenu(origin.menus(), recent, operations(), &origin));
    closeBeforeNextMessage(origin);
    QAction* const item = Gui::Test::menuItem(*origin.menus().openRecentMenu(), missing);
    ASSERT_NE(item, nullptr);
    item->trigger();
    EXPECT_TRUE(filler.isNull());
    EXPECT_EQ(manager().windowCount(), 0U);
    ASSERT_EQ(asked().size(), 1U);
    EXPECT_EQ(asked().front().texts.value(1), QStringLiteral("File not found: missing.ork"));
    EXPECT_EQ(recent.files(), QStringList{alpha});
}

TEST_F(RecentFilesMenuTest, AFileChosenInANewDesignTakesItsPlace)
{
    // BasicFrame.open(): closeIfReplaceable(). The window of the menu goes while its item is
    // being triggered (ASan checks that nothing of it is used afterwards).
    const QString alpha = QtStrings::fromPath(writeDesign("alpha.ork", "Alpha"));
    RecentFiles   recent(preferences());
    recent.setFiles({alpha});
    MainWindow& origin = manager().newDesign();
    static_cast<void>(fillRecentMenu(origin.menus(), recent, operations(), &origin));
    QAction* const item = Gui::Test::menuItem(*origin.menus().openRecentMenu(), alpha);
    ASSERT_NE(item, nullptr);
    item->trigger();
    Gui::Test::FileMenusTest::deleteLaterNow();
    ASSERT_EQ(manager().windowCount(), 1U);
    EXPECT_EQ(manager().windows().front()->document().getRocket().getName(), "Alpha");
    EXPECT_EQ(recent.files(), QStringList{alpha});
}

TEST_F(RecentFilesMenuTest, TheMenuOfNoWindowOpensFromNoWindow)
{
    // macOS without a window: MRUDesignFileAction of DummyFrameMenuOSX.
    const QString alpha = QtStrings::fromPath(writeDesign("alpha.ork", "Alpha"));
    RecentFiles   recent(preferences());
    recent.setFiles({alpha});
    MainWindow& window = manager(WindowManager::LastWindowBehaviour::KEEP_RUNNING).newDesign();
    static_cast<void>(operations());
    EXPECT_TRUE(manager().close(window));
    Gui::Test::FileMenusTest::deleteLaterNow();
    MainMenus const* const menus = manager().noWindowMenus();
    ASSERT_NE(menus, nullptr);
    RecentFilesMenu& filler = fillRecentMenu(*menus, recent, operations(), nullptr);
    EXPECT_TRUE(menus->openRecentMenu()->menuAction()->isEnabled());

    EXPECT_NE(filler.open(alpha), nullptr);
    ASSERT_EQ(manager().windowCount(), 1U);
    EXPECT_EQ(manager().windows().front()->document().getRocket().getName(), "Alpha");
    EXPECT_EQ(recent.files(), QStringList{alpha});
}

TEST_F(RecentFilesMenuTest, WithoutItsFileOperationsAnItemDoesNothing)
{
    const QString alpha = QtStrings::fromPath(writeDesign("alpha.ork", "Alpha"));
    RecentFiles   recent(preferences());
    recent.setFiles({alpha});
    MainWindow&      origin = loadedWindow(manager());
    RecentFilesMenu& filler = fillRecentMenu(origin.menus(), recent, operations(), &origin);
    destroyOperations();
    EXPECT_EQ(filler.open(alpha), nullptr);
    EXPECT_EQ(manager().windowCount(), 1U);
    EXPECT_EQ(recent.files(), QStringList{alpha});
    EXPECT_TRUE(asked().empty());
}

TEST_F(RecentFilesMenuTest, WithoutItsRecentFilesTheMenuStaysAndAnItemStillOpens)
{
    const QString alpha  = QtStrings::fromPath(writeDesign("alpha.ork", "Alpha"));
    auto          recent = std::make_unique<RecentFiles>(preferences());
    recent->setFiles({alpha});
    MainWindow&      origin = loadedWindow(manager());
    RecentFilesMenu& filler = fillRecentMenu(origin.menus(), *recent, operations(), &origin);
    recent.reset();
    EXPECT_EQ(Gui::Test::menuTexts(*filler.menu()), QStringList{alpha});
    EXPECT_NE(filler.open(alpha), nullptr);
    EXPECT_EQ(manager().windowCount(), 2U);
}

}  // namespace

}  // namespace QtRocket::Gui
