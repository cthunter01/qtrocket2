#include "QtRocket/gui/util/Guard.h"

#include <exception>
#include <stdexcept>
#include <utility>
#include <vector>

#include <QCoreApplication>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUnhandledException>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

using GuardTest = Test::GuiTest;
using Kind      = Guard::Failure::Kind;

/// Records every report for as long as it lives, then puts the previous hook back.
class RecordingHook
{
public:
    RecordingHook()
      : m_previous(Guard::setHook(
            [this](const Guard::Failure& failure) { m_failures.push_back(failure); }))
    {
    }
    ~RecordingHook() { static_cast<void>(Guard::setHook(std::move(m_previous))); }
    RecordingHook(const RecordingHook&)            = delete;
    RecordingHook& operator=(const RecordingHook&) = delete;
    RecordingHook(RecordingHook&&)                 = delete;
    RecordingHook& operator=(RecordingHook&&)      = delete;

    [[nodiscard]] const std::vector<Guard::Failure>& failures() const noexcept
    {
        return m_failures;
    }

private:
    std::vector<Guard::Failure> m_failures;
    Guard::Hook                 m_previous;
};

// The guarded callables (out of the test bodies, which then stay simple).
void throwBug()
{
    bug("the slot broke");
}

void throwRuntimeError()
{
    throw std::runtime_error("boom");
}

void throwInt()
{
    throw 42;
}

/// Adds its step to a counter.
struct Increment
{
    int* calls;
    int  step = 1;
    void operator()() const { *calls += step; }
};

/// Adds the argument to a counter.
struct AddArgument
{
    int* calls;
    void operator()(int step) const { *calls += step; }
};

/// Keeps the name it is given.
struct RecordName
{
    QString* name;
    void     operator()(const QString& newName) const { *name = newName; }
};

TEST_F(GuardTest, ABugErrorInAGuardedSlotReachesTheHook)
{
    const RecordingHook hook;
    QObject             sender;
    QObject::connect(&sender, &QObject::objectNameChanged, &sender,
                     guarded(&throwBug, QStringLiteral("Delete")));
    sender.setObjectName(QStringLiteral("rocket"));
    ASSERT_EQ(hook.failures().size(), 1U);
    const Guard::Failure& failure = hook.failures().front();
    EXPECT_EQ(failure.kind, Kind::BUG);
    EXPECT_TRUE(failure.message.startsWith(QStringLiteral("BUG: the slot broke (")))
        << failure.message.toStdString();
    EXPECT_TRUE(failure.file.endsWith(QStringLiteral("GuardTests.cpp")))
        << failure.file.toStdString();
    EXPECT_GT(failure.line, 0);
    EXPECT_EQ(failure.context, QStringLiteral("Delete"));
}

TEST_F(GuardTest, NoExceptionLeavesTheEventLoop)
{
    const RecordingHook hook;
    const QObject       context;
    QTimer::singleShot(0, &context, guarded(&throwRuntimeError));
    QCoreApplication::processEvents();
    ASSERT_EQ(hook.failures().size(), 1U);
    EXPECT_EQ(hook.failures().front().kind, Kind::EXCEPTION);
    EXPECT_EQ(hook.failures().front().message, QStringLiteral("boom"));
}

TEST_F(GuardTest, ReportsWhatIsNoStdException)
{
    const RecordingHook hook;
    EXPECT_FALSE(Guard::run(&throwInt));
    ASSERT_EQ(hook.failures().size(), 1U);
    EXPECT_EQ(hook.failures().front().kind, Kind::UNKNOWN);
    EXPECT_EQ(hook.failures().front().message, QStringLiteral("unknown exception"));
}

TEST_F(GuardTest, ACallThatReturnsReportsNothing)
{
    const RecordingHook hook;
    int                 calls = 0;
    EXPECT_TRUE(Guard::run(Increment{.calls = &calls}));
    auto slot = guarded(Increment{.calls = &calls});
    slot();
    EXPECT_EQ(calls, 2);
    EXPECT_TRUE(hook.failures().empty());
}

TEST_F(GuardTest, PassesTheSignalsLeadingArgumentsTheSlotTakes)
{
    const RecordingHook hook;
    QObject             sender;
    QString             name;
    int                 calls = 0;
    QObject::connect(&sender, &QObject::objectNameChanged, &sender,
                     guarded(RecordName{.name = &name}));
    QObject::connect(&sender, &QObject::objectNameChanged, &sender,
                     guarded(Increment{.calls = &calls}));
    sender.setObjectName(QStringLiteral("stage"));
    EXPECT_EQ(name, QStringLiteral("stage"));
    EXPECT_EQ(calls, 1);
    // A direct call with more arguments than the callable takes drops the trailing ones.
    auto slot = guarded(AddArgument{.calls = &calls});
    slot(10, QStringLiteral("dropped"));
    EXPECT_EQ(calls, 11);
    EXPECT_TRUE(hook.failures().empty());
}

TEST_F(GuardTest, AnExceptionThatQtConcurrentCarriedIsTheExceptionItCarries)
{
    const Guard::Failure failure = Guard::failureOf(
        std::make_exception_ptr(QUnhandledException(std::make_exception_ptr(BugError("worker")))),
        QStringLiteral("Simulate"));
    EXPECT_EQ(failure.kind, Kind::BUG);
    EXPECT_TRUE(failure.message.startsWith(QStringLiteral("BUG: worker")))
        << failure.message.toStdString();
    EXPECT_EQ(failure.context, QStringLiteral("Simulate"));
    EXPECT_EQ(Guard::failureOf(std::make_exception_ptr(QUnhandledException()), {}).kind,
              Kind::EXCEPTION);
    EXPECT_EQ(Guard::failureOf(nullptr, {}).kind, Kind::UNKNOWN);
}

TEST_F(GuardTest, ReportsAnErrorCondition)
{
    const RecordingHook hook;
    Guard::reportErrorCondition(QStringLiteral("Image file x.svg not found, ignoring."));
    ASSERT_EQ(hook.failures().size(), 1U);
    EXPECT_EQ(hook.failures().front().kind, Kind::ERROR_CONDITION);
    EXPECT_EQ(hook.failures().front().message,
              QStringLiteral("Image file x.svg not found, ignoring."));
    EXPECT_TRUE(hook.failures().front().file.isEmpty());
}

TEST_F(GuardTest, AFailureWhileTheHookRunsDoesNotReachTheHookAgain)
{
    int         calls    = 0;
    Guard::Hook previous = Guard::setHook([&calls](const Guard::Failure& /*failure*/) {
        ++calls;
        // The dialog's own failure (OpenRocket ignores it while it handles one).
        Guard::reportErrorCondition(QStringLiteral("nested"));
    });
    Guard::reportErrorCondition(QStringLiteral("first"));
    static_cast<void>(Guard::setHook(std::move(previous)));
    EXPECT_EQ(calls, 1);
}

TEST_F(GuardTest, WhatTheHookThrowsStaysInTheReport)
{
    Guard::Hook previous =
        Guard::setHook([](const Guard::Failure& /*failure*/) { throw std::runtime_error("hook"); });
    EXPECT_NO_THROW(Guard::reportErrorCondition(QStringLiteral("first")));
    static_cast<void>(Guard::setHook(std::move(previous)));
}

TEST_F(GuardTest, SetHookReturnsTheHookItReplaces)
{
    int         calls    = 0;
    Guard::Hook previous = Guard::setHook([&calls](const Guard::Failure& /*failure*/) { ++calls; });
    const Guard::Hook mine = Guard::setHook(std::move(previous));
    ASSERT_TRUE(mine);
    mine(Guard::Failure{});
    EXPECT_EQ(calls, 1);
}

TEST_F(GuardTest, DescribesAFailureForTheStandardError)
{
    Guard::Failure failure;
    failure.message = QStringLiteral("BUG: broken (Rocket.cpp:12)");
    EXPECT_EQ(Guard::describe(failure),
              QStringLiteral("QtRocket: internal error: BUG: broken (Rocket.cpp:12)"));
    failure.context = QStringLiteral("Delete");
    EXPECT_EQ(Guard::describe(failure),
              QStringLiteral("QtRocket: internal error: BUG: broken (Rocket.cpp:12) (in Delete)"));
}

}  // namespace

}  // namespace QtRocket::Gui
