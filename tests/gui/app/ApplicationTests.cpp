#include "QtRocket/gui/app/Application.h"

#include <filesystem>
#include <vector>

#include <QCoreApplication>
#include <QEvent>
#include <QFileOpenEvent>
#include <QGuiApplication>
#include <QIcon>
#include <QObject>
#include <QString>
#include <QUrl>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/BuildInfo.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/util/BugError.h"
#include "RecordingGuardHook.h"

// The application object of the tests is the application's own (tests/gui/GuiTestMain.cpp).

namespace QtRocket::Gui
{

namespace
{

using Files = std::vector<std::filesystem::path>;

/// The tests' Application, whose hooks a test sets and TearDown() empties again.
class ApplicationTest : public Test::GuiTest
{
protected:
    void SetUp() override
    {
        GuiTest::SetUp();
        // No file an earlier test asked to open waits.
        static_cast<void>(application().setOpenFilesHook([](const Files& /*files*/) { }));
        static_cast<void>(application().setOpenFilesHook({}));
    }

    void TearDown() override
    {
        static_cast<void>(application().setQuitHook({}));
        static_cast<void>(application().setOpenFilesHook({}));
        GuiTest::TearDown();
    }

    [[nodiscard]] static Application& application()
    {
        Application* const current = Application::current();
        // The tests' main() makes it; nothing works without it.
        QTROCKET_ASSERT(current != nullptr);
        return *current;
    }

    /// Sends the platform's request to open @p file to the application.
    static void sendFileOpen(const QString& file)
    {
        QFileOpenEvent event(file);
        QCoreApplication::sendEvent(&application(), &event);
    }
};

/// An object whose event handler throws a BugError for QEvent::User.
class ThrowingObject : public QObject
{
public:
    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::User)
        {
            bug("the event handler broke");
        }
        return QObject::event(event);
    }
};

/// A quit hook that counts its calls and answers as it is told.
struct CountingQuit
{
    int* calls;
    bool quit;

    bool operator()() const
    {
        ++*calls;
        return quit;
    }
};

/// An open-files hook that keeps what it is given.
struct RecordingOpen
{
    std::vector<Files>* calls;

    void operator()(const Files& files) const { calls->push_back(files); }
};

TEST_F(ApplicationTest, IsTheApplicationOfTheTests)
{
    EXPECT_NE(Application::current(), nullptr);
    EXPECT_EQ(QCoreApplication::applicationName(), BuildInfo::applicationName());
    EXPECT_EQ(QCoreApplication::organizationName(), BuildInfo::organizationName());
    EXPECT_EQ(QCoreApplication::applicationVersion(), BuildInfo::applicationVersion());
    EXPECT_FALSE(QGuiApplication::windowIcon().isNull());
    // The WindowManager decides what the last window does.
    EXPECT_FALSE(QGuiApplication::quitOnLastWindowClosed());
}

TEST_F(ApplicationTest, NotifyReportsWhatAnEventHandlerThrows)
{
    // On macOS an exception thrown in an event handler cannot unwind through Qt's own frames:
    // the program terminates before notify() can catch it (CI, Qt 6.8.3). Only the guarded
    // connections (Guard) protect there (Application.h).
#ifdef __APPLE__
    GTEST_SKIP() << "macOS: an exception cannot cross Qt's frames to reach notify()";
#endif
    const Gui::Test::RecordingGuardHook hook;
    ThrowingObject                      object;
    QEvent                              event(QEvent::User);
    EXPECT_FALSE(QCoreApplication::sendEvent(&object, &event));
    ASSERT_EQ(hook.failures().size(), 1U);
    EXPECT_EQ(hook.failures().front().kind, Guard::Failure::Kind::BUG);
    EXPECT_TRUE(
        hook.failures().front().message.contains(QStringLiteral("the event handler broke")));
    EXPECT_EQ(hook.failures().front().context, QStringLiteral("the User event of QObject"));
    EXPECT_FALSE(hook.failures().front().file.isEmpty());

    // The application goes on: the next event is delivered, and a second failure is reported.
    QEvent other(QEvent::None);
    EXPECT_FALSE(QCoreApplication::sendEvent(&object, &other));
    EXPECT_EQ(hook.failures().size(), 1U);
    EXPECT_FALSE(QCoreApplication::sendEvent(&object, &event));
    EXPECT_EQ(hook.failures().size(), 2U);
}

TEST_F(ApplicationTest, AQuitEventAsksTheQuitHook)
{
    int calls = 0;
    EXPECT_FALSE(application().setQuitHook(CountingQuit{.calls = &calls, .quit = false}));
    QEvent refused(QEvent::Quit);
    QCoreApplication::sendEvent(&application(), &refused);
    EXPECT_EQ(calls, 1);
    EXPECT_FALSE(refused.isAccepted());

    static_cast<void>(application().setQuitHook(CountingQuit{.calls = &calls, .quit = true}));
    QEvent accepted(QEvent::Quit);
    accepted.ignore();
    QCoreApplication::sendEvent(&application(), &accepted);
    EXPECT_EQ(calls, 2);
    EXPECT_TRUE(accepted.isAccepted());
}

TEST_F(ApplicationTest, FilesToOpenWaitForTheHook)
{
    const QString first  = QStringLiteral("/designs/first.ork");
    const QString second = QStringLiteral("/designs/second.ork");
    sendFileOpen(first);
    EXPECT_EQ(application().pendingFiles(), Files{QtStrings::toPath(first)});

    std::vector<Files> calls;
    EXPECT_FALSE(application().setOpenFilesHook(RecordingOpen{.calls = &calls}));
    ASSERT_EQ(calls.size(), 1U);
    EXPECT_EQ(calls.front(), Files{QtStrings::toPath(first)});
    EXPECT_TRUE(application().pendingFiles().empty());

    sendFileOpen(second);
    ASSERT_EQ(calls.size(), 2U);
    EXPECT_EQ(calls.back(), Files{QtStrings::toPath(second)});
}

TEST_F(ApplicationTest, OnlyALocalFileIsOpened)
{
    std::vector<Files> calls;
    static_cast<void>(application().setOpenFilesHook(RecordingOpen{.calls = &calls}));
    QFileOpenEvent event(QUrl(QStringLiteral("https://openrocket.info/rocket.ork")));
    QCoreApplication::sendEvent(&application(), &event);
    EXPECT_TRUE(calls.empty());
    EXPECT_TRUE(application().pendingFiles().empty());
}

}  // namespace

}  // namespace QtRocket::Gui
