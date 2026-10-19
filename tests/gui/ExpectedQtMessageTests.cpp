#include "ExpectedQtMessage.h"

#include <QGuiApplication>
#include <QMessageLogContext>
#include <QString>

#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

// The message handler of the GUI tests and its guard, tested with GoogleTest's own failure capture
// (EXPECT_NONFATAL_FAILURE): a check of the tests that could never fail would pass every broken
// test, as QtTest's macros do outside QtTest.

namespace QtRocket::Gui::Test
{

namespace
{

TEST(ExpectedQtMessage, AnUnexpectedWarningFailsTheTest)
{
    EXPECT_NONFATAL_FAILURE(qWarning("a warning nobody expects"),
                            "unexpected Qt warning: a warning nobody expects");
}

TEST(ExpectedQtMessage, AnUnexpectedCriticalMessageFailsTheTest)
{
    EXPECT_NONFATAL_FAILURE(qCritical("a critical message"),
                            "unexpected Qt critical message: a critical message");
}

TEST(ExpectedQtMessage, DebugAndInfoMessagesPass)
{
    qDebug("a debug message");
    qInfo("an info message");
}

TEST(ExpectedQtMessage, DropsAndCountsTheOffscreenPlatformsSizeHintsWarning)
{
    ASSERT_EQ(QGuiApplication::platformName(), QStringLiteral("offscreen"));
    const int before = ExpectedQtMessage::droppedOffscreenWarnings();
    qWarning("This plugin does not support propagateSizeHints()");
    EXPECT_EQ(ExpectedQtMessage::droppedOffscreenWarnings(), before + 1);
}

TEST(ExpectedQtMessage, SwallowsTheMessagesItExpects)
{
    const ExpectedQtMessage expected(QStringLiteral("^cannot connect .* to slot [0-9]+$"));
    qWarning("cannot connect a signal to slot 7");
    qWarning("cannot connect another signal to slot 8");
    EXPECT_EQ(expected.count(), 2);
}

TEST(ExpectedQtMessage, LetsOtherMessagesFail)
{
    const ExpectedQtMessage expected(QStringLiteral("^expected$"));
    qWarning("expected");
    EXPECT_NONFATAL_FAILURE(qWarning("something else"), "unexpected Qt warning: something else");
    EXPECT_NONFATAL_FAILURE(qCritical("expected"), "unexpected Qt critical message: expected");
}

TEST(ExpectedQtMessage, OfItsOwnType)
{
    const ExpectedQtMessage expected(QStringLiteral("model"), QtCriticalMsg);
    qCritical("the model is broken");
    EXPECT_EQ(expected.count(), 1);
}

TEST(ExpectedQtMessage, TheInnermostGuardCountsTheMessage)
{
    const ExpectedQtMessage outer(QStringLiteral("message"));
    {
        const ExpectedQtMessage inner(QStringLiteral("message"));
        qWarning("a message");
        EXPECT_EQ(inner.count(), 1);
    }
    qWarning("another message");
    EXPECT_EQ(outer.count(), 1);
}

TEST(ExpectedQtMessage, FailsWhenTheMessageNeverCame)
{
    EXPECT_NONFATAL_FAILURE(
        { const ExpectedQtMessage expected(QStringLiteral("never")); },
        "no Qt warning matching 'never' came");
}

TEST(ExpectedQtMessage, FailsForAPatternThatIsNoRegularExpression)
{
    EXPECT_NONFATAL_FAILURE(
        { const ExpectedQtMessage expected(QStringLiteral("(unclosed")); },
        "not a regular expression: (unclosed");
}

}  // namespace

}  // namespace QtRocket::Gui::Test
