#include "QtRocket/gui/main/WindowManager.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QAction>
#include <QCoreApplication>
#include <QEvent>
#include <QMenuBar>
#include <QPoint>
#include <QPointer>
#include <QSignalSpy>
#include <QString>
#include <QStringList>

#include <gtest/gtest.h>

#include "ExpectedQtMessage.h"
#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/main/MainMenus.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/settings/WindowGeometry.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/rocket/Rocket.h"
#include "ScriptedUserPrompts.h"
#include "TestGuiServices.h"

namespace QtRocket::Gui
{

namespace
{

using Texts               = std::vector<std::string>;
using LastWindowBehaviour = WindowManager::LastWindowBehaviour;
using SaveChangesAnswer   = UserPrompts::SaveChangesAnswer;
using PromptKind          = Gui::Test::ScriptedUserPrompts::Kind;

/// A window manager over test services, made by the test with the behaviour it needs.
class WindowManagerTest : public Test::GuiTest
{
protected:
    void TearDown() override
    {
        // Its windows (and those it still has to delete) before the fixture looks for windows.
        m_manager.reset();
        GuiTest::TearDown();
    }

    /// The manager, made the first time with @p behaviour.
    [[nodiscard]] WindowManager& manager(LastWindowBehaviour behaviour = LastWindowBehaviour::EXIT)
    {
        if (m_manager == nullptr)
        {
            m_manager = std::make_unique<WindowManager>(m_services.services(), behaviour);
        }
        return *m_manager;
    }

    [[nodiscard]] Gui::Test::ScriptedUserPrompts& prompts() { return m_services.prompts(); }

    /// Destroys the manager.
    void destroyManager() { m_manager.reset(); }

    /// The designs whose saving was asked, in order.
    [[nodiscard]] Texts askedToSave() const
    {
        Texts names;
        for (const Gui::Test::ScriptedUserPrompts::Prompt& prompt : m_services.prompts().prompts())
        {
            if (prompt.kind == PromptKind::SAVE_CHANGES && !prompt.texts.isEmpty())
            {
                names.push_back(QtStrings::toUtf8(prompt.texts.front()));
            }
        }
        return names;
    }

    /// Opens a window on a new document whose rocket is named @p name, so that it is not saved.
    MainWindow& openChanged(std::string_view name)
    {
        std::unique_ptr<OpenRocketDocument> document = OpenRocketDocumentFactory::createNewRocket();
        document->getRocket().setName(name);
        return manager().addDocument(std::move(document));
    }

    /// Deletes what was given to deleteLater().
    static void deleteLaterNow()
    {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    /// The rocket names of the manager's windows, in order.
    [[nodiscard]] Texts windowNames()
    {
        Texts names;
        for (const MainWindow* window : manager().windows())
        {
            names.push_back(window->document().getRocket().getName());
        }
        return names;
    }

private:
    Gui::Test::TestGuiServices     m_services;
    std::unique_ptr<WindowManager> m_manager;
};

/// A save hook: records the windows it saves and answers @p saved.
struct RecordingSave
{
    std::vector<MainWindow*>* windows;
    bool                      saved;

    bool operator()(MainWindow& window) const
    {
        windows->push_back(&window);
        return saved;
    }
};

/// A save hook that tries to close and to quit while it runs, and records the answers.
struct ReentrantSave
{
    WindowManager*     manager;
    std::vector<bool>* answers;

    bool operator()(MainWindow& window) const
    {
        answers->push_back(manager->close(window));
        answers->push_back(manager->quit());
        return true;
    }
};

TEST_F(WindowManagerTest, NewDesignOpensAShownReplaceableWindowOfANewRocket)
{
    const QSignalSpy opened(&manager(), &WindowManager::windowOpened);
    const QSignalSpy menusCreated(&manager(), &WindowManager::menusCreated);
    MainWindow&      window = manager().newDesign();

    EXPECT_EQ(manager().windows(), std::vector<MainWindow*>{&window});
    EXPECT_EQ(manager().windowCount(), 1U);
    EXPECT_TRUE(window.isVisible());
    EXPECT_TRUE(manager().isReplaceable(window));
    EXPECT_EQ(window.windowTitle(), QStringLiteral("[*]Rocket"));
    const OpenRocketDocument& document = window.document();
    EXPECT_TRUE(document.isSaved());
    EXPECT_FALSE(document.getFile().has_value());
    EXPECT_EQ(document.getRocket().getChildCount(), 1U);
    EXPECT_EQ(document.getRocket().getChild(0).getName(), "Sustainer");

    ASSERT_EQ(opened.count(), 1);
    EXPECT_EQ(opened.at(0).at(0).value<MainWindow*>(), &window);
    ASSERT_EQ(menusCreated.count(), 1);
    EXPECT_EQ(menusCreated.at(0).at(0).value<MainMenus*>(), &window.menus());
    EXPECT_EQ(menusCreated.at(0).at(1).value<MainWindow*>(), &window);
    EXPECT_TRUE(prompts().prompts().empty());
}

TEST_F(WindowManagerTest, OpensAWindowWithItsSizesSetBeforeItIsShown)
{
    // A window that gets its minimum size as it is shown makes the offscreen platform warn that
    // it cannot pass size limits on, which the program would print at every start.
    const int         before = Gui::Test::ExpectedQtMessage::droppedOffscreenWarnings();
    const MainWindow& window = manager().newDesign();
    EXPECT_TRUE(window.isVisible());
    static_cast<void>(manager().addDocument(OpenRocketDocumentFactory::createNewRocket()));
    EXPECT_EQ(manager().windowCount(), 2U);
    EXPECT_EQ(Gui::Test::ExpectedQtMessage::droppedOffscreenWarnings(), before);
}

TEST_F(WindowManagerTest, NewDesignFromASavedReplaceableWindowTakesItsPlace)
{
    MainWindow& first = manager().newDesign();
    first.move(40, 50);
    const QPoint               place = first.pos();
    const QPointer<MainWindow> replaced(&first);

    MainWindow& second = manager().newDesign(&first);
    EXPECT_EQ(manager().windows(), std::vector<MainWindow*>{&second});
    EXPECT_EQ(second.pos(), place);
    EXPECT_TRUE(manager().isReplaceable(second));
    EXPECT_TRUE(prompts().prompts().empty());

    // The replaced window is hidden at once and deleted from the event loop.
    ASSERT_FALSE(replaced.isNull());
    EXPECT_FALSE(first.isVisible());
    deleteLaterNow();
    EXPECT_TRUE(replaced.isNull());
}

TEST_F(WindowManagerTest, NewDesignFromAChangedWindowCascadesBesideIt)
{
    MainWindow& first = manager().newDesign();
    first.document().getRocket().setName("Mars");

    MainWindow& second = manager().newDesign(&first);
    EXPECT_EQ(manager().windows(), (std::vector<MainWindow*>{&first, &second}));
    EXPECT_EQ(second.pos(),
              first.pos() + QPoint(WindowGeometry::kCascadeOffset, WindowGeometry::kCascadeOffset));
    EXPECT_TRUE(prompts().prompts().empty());
}

TEST_F(WindowManagerTest, AReplaceableWindowSavedAgainIsReplaced)
{
    // As in Java: replaceable for good, replaced whenever its design is saved.
    MainWindow& first = manager().newDesign();
    first.document().getRocket().setName("Mars");
    first.document().setSaved(true);

    MainWindow& second = manager().newDesign(&first);
    EXPECT_EQ(manager().windows(), std::vector<MainWindow*>{&second});
}

TEST_F(WindowManagerTest, AddedDocumentsWindowIsNotReplaceable)
{
    std::unique_ptr<OpenRocketDocument> document = OpenRocketDocumentFactory::createNewRocket();
    MainWindow&                         loaded   = manager().addDocument(std::move(document));
    EXPECT_FALSE(manager().isReplaceable(loaded));
    EXPECT_TRUE(loaded.document().isSaved());

    MainWindow& next = manager().newDesign(&loaded);
    EXPECT_EQ(manager().windows(), (std::vector<MainWindow*>{&loaded, &next}));
}

TEST_F(WindowManagerTest, AddedDocumentTakesThePlaceOfASavedReplaceableWindow)
{
    MainWindow& first = manager().newDesign();
    MainWindow& added = manager().addDocument(OpenRocketDocumentFactory::createNewRocket(), &first);
    EXPECT_EQ(manager().windows(), std::vector<MainWindow*>{&added});
}

TEST_F(WindowManagerTest, ClosingASavedWindowAsksNothingAndEndsTheApplication)
{
    const QSignalSpy           exits(&manager(), &WindowManager::exitRequested);
    MainWindow&                window = manager().newDesign();
    const QPointer<MainWindow> closed(&window);

    EXPECT_TRUE(manager().close(window));
    EXPECT_EQ(manager().windowCount(), 0U);
    EXPECT_TRUE(prompts().prompts().empty());
    EXPECT_EQ(exits.count(), 1);
    EXPECT_EQ(manager().noWindowMenuBar(), nullptr);
    deleteLaterNow();
    EXPECT_TRUE(closed.isNull());
}

TEST_F(WindowManagerTest, ClosingAChangedWindowAsksAndDiscards)
{
    const QSignalSpy exits(&manager(), &WindowManager::exitRequested);
    MainWindow&      window = openChanged("Mars");
    prompts().answerSaveChanges(SaveChangesAnswer::DISCARD);

    EXPECT_TRUE(manager().close(window));
    EXPECT_EQ(askedToSave(), Texts{"Mars"});
    EXPECT_EQ(manager().windowCount(), 0U);
    EXPECT_EQ(exits.count(), 1);
}

TEST_F(WindowManagerTest, CancelKeepsTheWindow)
{
    const QSignalSpy exits(&manager(), &WindowManager::exitRequested);
    MainWindow&      window = openChanged("Mars");
    prompts().answerSaveChanges(SaveChangesAnswer::CANCEL);

    EXPECT_FALSE(manager().close(window));
    EXPECT_EQ(manager().windows(), std::vector<MainWindow*>{&window});
    EXPECT_TRUE(window.isVisible());
    EXPECT_EQ(exits.count(), 0);

    // It can be closed again.
    prompts().answerSaveChanges(SaveChangesAnswer::DISCARD);
    EXPECT_TRUE(manager().close(window));
    EXPECT_EQ(askedToSave(), (Texts{"Mars", "Mars"}));
}

TEST_F(WindowManagerTest, SaveWithoutASaveHookSaysSoAndKeepsTheWindow)
{
    MainWindow& window = openChanged("Mars");
    prompts().answerSaveChanges(SaveChangesAnswer::SAVE);

    EXPECT_FALSE(manager().close(window));
    EXPECT_EQ(manager().windowCount(), 1U);
    const std::vector<Gui::Test::ScriptedUserPrompts::Prompt>& asked = prompts().prompts();
    ASSERT_EQ(asked.size(), 2U);
    EXPECT_EQ(asked.at(1).kind, PromptKind::MESSAGE);
    EXPECT_EQ(asked.at(1).title, QStringLiteral("Design not saved"));
    EXPECT_EQ(asked.at(1).texts,
              (QStringList{
                  QStringLiteral("WARNING"),
                  QStringLiteral("Saving designs is not available in this version of QtRocket.")}));
}

TEST_F(WindowManagerTest, SaveGoesThroughTheSaveHook)
{
    std::vector<MainWindow*> saved;
    manager().setSaveHook(RecordingSave{.windows = &saved, .saved = false});
    MainWindow& window = openChanged("Mars");

    // A save that fails (or is cancelled) keeps the window.
    prompts().answerSaveChanges(SaveChangesAnswer::SAVE);
    EXPECT_FALSE(manager().close(window));
    EXPECT_EQ(saved, std::vector<MainWindow*>{&window});
    EXPECT_EQ(manager().windowCount(), 1U);

    manager().setSaveHook(RecordingSave{.windows = &saved, .saved = true});
    prompts().answerSaveChanges(SaveChangesAnswer::SAVE);
    EXPECT_TRUE(manager().close(window));
    EXPECT_EQ(saved.size(), 2U);
    EXPECT_EQ(manager().windowCount(), 0U);
}

TEST_F(WindowManagerTest, AWindowIsNotClosedTwiceWhileItsQuestionRuns)
{
    std::vector<bool> answers;
    manager().setSaveHook(ReentrantSave{.manager = &manager(), .answers = &answers});
    MainWindow& window = openChanged("Mars");
    prompts().answerSaveChanges(SaveChangesAnswer::SAVE);

    EXPECT_TRUE(manager().close(window));
    // Neither a close of the window being closed nor a quit, which would close it too.
    EXPECT_EQ(answers, (std::vector<bool>{false, false}));
    EXPECT_EQ(manager().windowCount(), 0U);
}

TEST_F(WindowManagerTest, TheCloseButtonAndCloseDesignClose)
{
    MainWindow& first  = manager().addDocument(OpenRocketDocumentFactory::createNewRocket());
    MainWindow& second = manager().addDocument(OpenRocketDocumentFactory::createNewRocket());

    EXPECT_FALSE(first.close());  // the window asks the manager instead
    EXPECT_EQ(manager().windows(), std::vector<MainWindow*>{&second});

    second.menus().closeAction()->trigger();
    EXPECT_EQ(manager().windowCount(), 0U);
}

TEST_F(WindowManagerTest, NewOpensANewDesignInThePlaceOfAReplaceableWindow)
{
    const MainWindow& first = manager().newDesign();
    first.menus().newAction()->trigger();
    ASSERT_EQ(manager().windowCount(), 1U);
    EXPECT_NE(manager().windows().front(), &first);
}

TEST_F(WindowManagerTest, QuitClosesTheWindowsLastFirst)
{
    const QSignalSpy exits(&manager(), &WindowManager::exitRequested);
    static_cast<void>(openChanged("A"));
    static_cast<void>(openChanged("B"));
    static_cast<void>(openChanged("C"));
    prompts().answerSaveChanges(SaveChangesAnswer::DISCARD);
    prompts().answerSaveChanges(SaveChangesAnswer::DISCARD);
    prompts().answerSaveChanges(SaveChangesAnswer::DISCARD);

    EXPECT_TRUE(manager().quit());
    EXPECT_EQ(askedToSave(), (Texts{"C", "B", "A"}));
    EXPECT_EQ(manager().windowCount(), 0U);
    EXPECT_EQ(exits.count(), 1);
}

TEST_F(WindowManagerTest, QuitStopsAtAWindowThatStaysOpen)
{
    const QSignalSpy exits(&manager(), &WindowManager::exitRequested);
    static_cast<void>(openChanged("A"));
    static_cast<void>(openChanged("B"));
    static_cast<void>(openChanged("C"));
    prompts().answerSaveChanges(SaveChangesAnswer::DISCARD);
    prompts().answerSaveChanges(SaveChangesAnswer::CANCEL);

    EXPECT_FALSE(manager().quit());
    EXPECT_EQ(askedToSave(), (Texts{"C", "B"}));
    // The windows closed before stay closed.
    EXPECT_EQ(windowNames(), (Texts{"A", "B"}));
    EXPECT_EQ(exits.count(), 0);
}

TEST_F(WindowManagerTest, QuitItemQuits)
{
    const QSignalSpy  exits(&manager(), &WindowManager::exitRequested);
    const MainWindow& first = manager().addDocument(OpenRocketDocumentFactory::createNewRocket());
    static_cast<void>(manager().addDocument(OpenRocketDocumentFactory::createNewRocket()));

    first.menus().quitAction()->trigger();
    EXPECT_EQ(manager().windowCount(), 0U);
    EXPECT_EQ(exits.count(), 1);
}

TEST_F(WindowManagerTest, KeepsRunningWithTheMenusOfNoWindow)
{
    WindowManager&   keeping = manager(LastWindowBehaviour::KEEP_RUNNING);
    const QSignalSpy exits(&keeping, &WindowManager::exitRequested);
    const QSignalSpy menusCreated(&keeping, &WindowManager::menusCreated);
    EXPECT_EQ(keeping.lastWindowBehaviour(), LastWindowBehaviour::KEEP_RUNNING);
    EXPECT_TRUE(keeping.close(keeping.newDesign()));
    EXPECT_EQ(exits.count(), 0);

    // DummyFrameMenuOSX.
    const QMenuBar* const  bar   = keeping.noWindowMenuBar();
    const MainMenus* const menus = keeping.noWindowMenus();
    ASSERT_NE(bar, nullptr);
    ASSERT_NE(menus, nullptr);
    EXPECT_EQ(bar->parent(), nullptr);
    EXPECT_FALSE(bar->isVisible());
    EXPECT_FALSE(menus->isDesignWindowMenus());
    ASSERT_EQ(menusCreated.count(), 2);
    EXPECT_EQ(menusCreated.at(1).at(0).value<MainMenus*>(), menus);
    EXPECT_EQ(menusCreated.at(1).at(1).value<MainWindow*>(), nullptr);

    // Its New opens a window; the menu bar stays as it is when that one closes too.
    menus->newAction()->trigger();
    ASSERT_EQ(keeping.windowCount(), 1U);
    EXPECT_TRUE(keeping.close(*keeping.windows().front()));
    EXPECT_EQ(keeping.noWindowMenuBar(), bar);
    EXPECT_EQ(menusCreated.count(), 3);

    // Its Quit quits.
    menus->quitAction()->trigger();
    EXPECT_EQ(exits.count(), 1);
}

TEST_F(WindowManagerTest, QuitEndsTheApplicationAlsoWhenItWouldKeepRunning)
{
    WindowManager&   keeping = manager(LastWindowBehaviour::KEEP_RUNNING);
    const QSignalSpy exits(&keeping, &WindowManager::exitRequested);
    static_cast<void>(keeping.newDesign());

    EXPECT_TRUE(keeping.quit());
    EXPECT_EQ(exits.count(), 1);
    EXPECT_EQ(keeping.noWindowMenuBar(), nullptr);
}

TEST_F(WindowManagerTest, FindsTheWindowOfARocket)
{
    const MainWindow& first  = manager().addDocument(OpenRocketDocumentFactory::createNewRocket());
    const MainWindow& second = manager().addDocument(OpenRocketDocumentFactory::createNewRocket());
    EXPECT_EQ(manager().findWindow(first.document().getRocket()), &first);
    EXPECT_EQ(manager().findWindow(second.document().getRocket()), &second);
    const std::unique_ptr<OpenRocketDocument> other = OpenRocketDocumentFactory::createNewRocket();
    EXPECT_EQ(manager().findWindow(other->getRocket()), nullptr);
}

TEST_F(WindowManagerTest, DestroysItsWindowsWithIt)
{
    MainWindow& stillOpen = manager().addDocument(OpenRocketDocumentFactory::createNewRocket());
    MainWindow& closing   = manager().addDocument(OpenRocketDocumentFactory::createNewRocket());
    const QPointer<MainWindow> openWindow(&stillOpen);
    const QPointer<MainWindow> closedWindow(&closing);
    EXPECT_TRUE(manager().close(closing));

    destroyManager();
    EXPECT_TRUE(openWindow.isNull());
    EXPECT_TRUE(closedWindow.isNull());
}

TEST_F(WindowManagerTest, PlatformKeepsRunningOnMacOsOnly)
{
#ifdef __APPLE__
    EXPECT_EQ(WindowManager::platformLastWindowBehaviour(), LastWindowBehaviour::KEEP_RUNNING);
#else
    EXPECT_EQ(WindowManager::platformLastWindowBehaviour(), LastWindowBehaviour::EXIT);
#endif
}

}  // namespace

}  // namespace QtRocket::Gui
