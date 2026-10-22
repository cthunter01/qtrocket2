#include "QtRocket/gui/main/ExampleDesignsMenu.h"

#include <filesystem>
#include <optional>
#include <vector>

#include <QAction>
#include <QMenu>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QtContainerFwd>
#include <QtCore/qtestsupport_core.h>

#include <gtest/gtest.h>

#include "FileMenusTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/main/DocumentFileOperations.h"
#include "QtRocket/gui/main/ExampleDesigns.h"
#include "QtRocket/gui/main/MainMenus.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/main/WindowManager.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "ScriptedUserPrompts.h"
#include "TestPaths.h"

// File > Open example (ExampleDesignFileAction) in the windows of a manager with its file
// operations, over the shipped examples (data/examples) and directories of the tests' own.

namespace QtRocket::Gui
{

namespace
{

using ExampleDesignsMenuTest = Gui::Test::FileMenusTest;

/// The shipped examples.
[[nodiscard]] std::filesystem::path shippedExamples()
{
    return QtRocket::Test::dataDir() / "examples";
}

/// Fills the Open example menu of @p menus with the examples of @p directory, opened by
/// @p operations from @p window; the menu object (a pointer: GCC's -Wdangling-reference takes a
/// reference returned for a temporary path as dangling).
ExampleDesignsMenu* fillExampleMenu(MainMenus& menus, const std::filesystem::path& directory,
                                    DocumentFileOperations& operations, MainWindow* window)
{
    auto* filler =
        makeChild<ExampleDesignsMenu>(menus.openExampleMenu(), directory, operations, window);
    filler->setObjectName(QStringLiteral("exampleDesignsMenu"));
    return filler;
}

/// Records the components of the configuration dialog requests of a window.
struct RecordDialogRequests
{
    std::vector<RocketComponent*>* components;

    void operator()(RocketComponent* component) const { components->push_back(component); }
};

/// Whether @p components holds a component.
struct NotEmpty
{
    const std::vector<RocketComponent*>* components;

    [[nodiscard]] bool operator()() const { return !components->empty(); }
};

/// Records the files a signal tells.
struct RecordFiles
{
    QStringList* files;

    void operator()(const QString& file) const { files->append(file); }
};

TEST_F(ExampleDesignsMenuTest, ListsTheExamplesInOpenRocketsOrderWithItsSeparators)
{
    MainWindow&        window = manager().newDesign();
    const QMenu* const menu   = window.menus().openExampleMenu();
    EXPECT_FALSE(menu->menuAction()->isEnabled());

    const ExampleDesignsMenu& filler =
        *fillExampleMenu(window.menus(), shippedExamples(), operations(), &window);
    EXPECT_EQ(filler.menu(), menu);
    EXPECT_EQ(filler.parent(), menu);
    EXPECT_TRUE(menu->menuAction()->isEnabled());
    EXPECT_EQ(filler.designs().designs(), ExampleDesigns(shippedExamples()).designs());
    // ExampleDesignsTest pins the order (decision U4 included).
    EXPECT_EQ(
        Gui::Test::menuTexts(*menu),
        (QStringList{
            QStringLiteral("A simple model rocket"), QStringLiteral("Two stage high power rocket"),
            QStringLiteral("Three stage low power rocket"), QStringLiteral("ARC payload rocket"),
            QStringLiteral("Tube fin rocket"), QStringLiteral("3D printable nose cone and fins"),
            QStringLiteral("----"), QStringLiteral("Deployable payload"),
            QStringLiteral("Airstart timing"), QStringLiteral("Chute release"),
            QStringLiteral("Dual parachute deployment"), QStringLiteral("Clustered motors"),
            QStringLiteral("Parallel booster staging"),
            QStringLiteral("Pods--airframes and winglets"),
            QStringLiteral("Pods--powered with recovery deployment"), QStringLiteral("----"),
            QStringLiteral("Simulation extensions"), QStringLiteral("Simulation scripting")}));
    const QAction* const first = menu->actions().front();
    EXPECT_TRUE(first->icon().isNull());
    EXPECT_EQ(first->menuRole(), QAction::NoRole);
    EXPECT_TRUE(first->isEnabled());
}

TEST_F(ExampleDesignsMenuTest, ChoosingAnExampleOpensItWithoutAFileAndAsksForTheRocketsDialog)
{
    // ExampleDesignFileAction: BasicFrame.open(example.getURL(), parent): no file, saved, not a
    // recent file, the rocket's dialog once the window is shown; the New design it was chosen
    // from is replaced (closeIfReplaceable()) while its item is being triggered.
    MainWindow& origin = manager().newDesign();
    static_cast<void>(fillExampleMenu(origin.menus(), shippedExamples(), operations(), &origin));
    QStringList opened;
    QObject::connect(&operations(), &DocumentFileOperations::designFileOpened, &operations(),
                     RecordFiles{.files = &opened});
    QAction* const item =
        Gui::Test::menuItem(*origin.menus().openExampleMenu(), QStringLiteral("Tube fin rocket"));
    ASSERT_NE(item, nullptr);
    item->trigger();
    Gui::Test::FileMenusTest::deleteLaterNow();
    // The test services hold no motor: the load warns of the example's motor, naming the file.
    ASSERT_EQ(asked().size(), 1U);
    EXPECT_EQ(asked().front().kind, Gui::Test::ScriptedUserPrompts::Kind::WARNINGS);
    EXPECT_EQ(asked().front().texts.value(0),
              QStringLiteral(
                  "The following problems were encountered while opening Tube fin rocket.ork."));
    ASSERT_EQ(manager().windowCount(), 1U);
    MainWindow& window = *manager().windows().front();
    EXPECT_EQ(window.document().getRocket().getName(), "Tube fin rocket");
    EXPECT_EQ(window.document().getFile(), std::nullopt);
    EXPECT_TRUE(window.document().isSaved());
    EXPECT_TRUE(opened.isEmpty());
    std::vector<RocketComponent*> requested;
    QObject::connect(&window, &MainWindow::configDialogRequested, &window,
                     RecordDialogRequests{.components = &requested});
    EXPECT_TRUE(QTest::qWaitFor(NotEmpty{.components = &requested}));
    EXPECT_EQ(requested, std::vector<RocketComponent*>{&window.document().getRocket()});
}

TEST_F(ExampleDesignsMenuTest, AnExampleOfAnotherWindowOpensBesideIt)
{
    MainWindow& origin = manager().addDocument(OpenRocketDocumentFactory::createNewRocket());
    ExampleDesignsMenu& filler =
        *fillExampleMenu(origin.menus(), shippedExamples(), operations(), &origin);
    ASSERT_FALSE(filler.designs().designs().empty());
    const MainWindow* const window = filler.open(filler.designs().designs().front());
    ASSERT_NE(window, nullptr);
    EXPECT_EQ(manager().windowCount(), 2U);
    EXPECT_EQ(window->document().getRocket().getName(), "3D printable nose cone and fins");
    Gui::Test::FileMenusTest::deleteLaterNow();
}

TEST_F(ExampleDesignsMenuTest, AnExampleWhoseMenuGoesWithItsWindowDuringTheOpenStillOpens)
{
    // The window of the menu is closed and destroyed while the example's warnings are shown (the
    // test services hold no motor): the menu goes with it, and the example opens in a window of
    // its own, as from no window (ASan checks that nothing of the menu or of the window is used
    // afterwards).
    MainWindow& origin = manager().addDocument(OpenRocketDocumentFactory::createNewRocket());
    const QPointer<ExampleDesignsMenu> filler(
        fillExampleMenu(origin.menus(), shippedExamples(), operations(), &origin));
    closeBeforeNextMessage(origin);
    QAction* const item =
        Gui::Test::menuItem(*origin.menus().openExampleMenu(), QStringLiteral("Tube fin rocket"));
    ASSERT_NE(item, nullptr);
    item->trigger();
    EXPECT_TRUE(filler.isNull());
    ASSERT_EQ(asked().size(), 1U);
    EXPECT_EQ(asked().front().kind, Gui::Test::ScriptedUserPrompts::Kind::WARNINGS);
    ASSERT_EQ(manager().windowCount(), 1U);
    EXPECT_EQ(manager().windows().front()->document().getRocket().getName(), "Tube fin rocket");
    EXPECT_EQ(manager().windows().front()->document().getFile(), std::nullopt);
    Gui::Test::FileMenusTest::deleteLaterNow();
}

TEST_F(ExampleDesignsMenuTest, AnExampleThatCannotBeOpenedSaysSo)
{
    // A design file of the directory that is no design: the loader's error.
    static_cast<void>(directory().write("Broken & bent.ork", "not a design\n"));
    MainWindow& origin = manager().addDocument(OpenRocketDocumentFactory::createNewRocket());
    const ExampleDesignsMenu& filler =
        *fillExampleMenu(origin.menus(), directory().path(), operations(), &origin);
    // The name as it is (an '&' shows as one), no separator before it: it is no example of the
    // order list.
    EXPECT_EQ(Gui::Test::menuTexts(*filler.menu()),
              (QStringList{QStringLiteral("----"), QStringLiteral("----"), QStringLiteral("----"),
                           QStringLiteral("Broken && bent")}));
    QAction* const item = Gui::Test::menuItem(*filler.menu(), QStringLiteral("Broken && bent"));
    ASSERT_NE(item, nullptr);
    item->trigger();
    EXPECT_EQ(manager().windowCount(), 1U);
    ASSERT_EQ(asked().size(), 1U);
    EXPECT_EQ(asked().front().title, QStringLiteral("Error opening file"));
    ASSERT_EQ(asked().front().texts.size(), 2);
    EXPECT_TRUE(asked().front().texts.at(1).startsWith(
        QStringLiteral("Unable to open file 'Broken & bent.ork': ")));
}

TEST_F(ExampleDesignsMenuTest, TheMenuOfNoWindowOpensFromNoWindow)
{
    // macOS without a window: ExampleDesignFileAction of DummyFrameMenuOSX (parent null).
    MainWindow& window = manager(WindowManager::LastWindowBehaviour::KEEP_RUNNING).newDesign();
    static_cast<void>(operations());
    EXPECT_TRUE(manager().close(window));
    Gui::Test::FileMenusTest::deleteLaterNow();
    MainMenus* const menus = manager().noWindowMenus();
    ASSERT_NE(menus, nullptr);
    const ExampleDesignsMenu& filler =
        *fillExampleMenu(*menus, shippedExamples(), operations(), nullptr);
    EXPECT_TRUE(menus->openExampleMenu()->menuAction()->isEnabled());
    QAction* const item =
        Gui::Test::menuItem(*filler.menu(), QStringLiteral("A simple model rocket"));
    ASSERT_NE(item, nullptr);
    item->trigger();
    ASSERT_EQ(manager().windowCount(), 1U);
    EXPECT_EQ(manager().windows().front()->document().getRocket().getName(),
              "A simple model rocket");
    Gui::Test::FileMenusTest::deleteLaterNow();
}

TEST_F(ExampleDesignsMenuTest, WithoutItsFileOperationsAnItemDoesNothing)
{
    MainWindow& origin = manager().addDocument(OpenRocketDocumentFactory::createNewRocket());
    ExampleDesignsMenu& filler =
        *fillExampleMenu(origin.menus(), shippedExamples(), operations(), &origin);
    destroyOperations();
    ASSERT_FALSE(filler.designs().designs().empty());
    EXPECT_EQ(filler.open(filler.designs().designs().front()), nullptr);
    EXPECT_EQ(manager().windowCount(), 1U);
}

}  // namespace

}  // namespace QtRocket::Gui
