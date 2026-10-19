#include "ExpectedQtMessage.h"

#include <algorithm>
#include <format>
#include <mutex>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#include <QGuiApplication>
#include <QMessageLogContext>
#include <QRegularExpression>
#include <QString>

#include <gtest/gtest.h>

namespace QtRocket::Gui::Test
{

namespace
{

/// The guards alive and the handler Qt had before installHandler(). Qt may log from any thread.
struct Registry
{
    std::mutex                      mutex;
    std::vector<ExpectedQtMessage*> guards;
    QtMessageHandler                previous                 = nullptr;
    int                             droppedOffscreenWarnings = 0;
};

Registry& registry()
{
    static Registry s_registry;
    return s_registry;
}

std::string_view typeName(QtMsgType type)
{
    switch (type)
    {
        case QtDebugMsg:
            return "debug message";
        case QtInfoMsg:
            return "info message";
        case QtWarningMsg:
            return "warning";
        case QtCriticalMsg:
            return "critical message";
        case QtFatalMsg:
            return "fatal message";
    }
    return "message";
}

/// A message counts as a test's while GoogleTest runs a test suite (a test, or the suite's own
/// set-up and tear-down); before and after, Qt's handler only prints it.
bool insideTest()
{
    return ::testing::UnitTest::GetInstance()->current_test_suite() != nullptr;
}

/// Whether @p message is one of the offscreen platform's warnings that it cannot do something
/// for a window that the other platforms do, which says nothing about the code under test. The
/// offscreen platform cannot pass a window's size limits on (QPlatformWindow's default
/// propagateSizeHints()), so every window with a layout warns when it is shown.
bool isOffscreenLimitation(QtMsgType type, const QString& message)
{
    return type == QtWarningMsg && QGuiApplication::platformName() == QStringLiteral("offscreen") &&
           message == QStringLiteral("This plugin does not support propagateSizeHints()");
}

void forward(QtMessageHandler previous, QtMsgType type, const QMessageLogContext& context,
             const QString& message)
{
    if (previous != nullptr)
    {
        previous(type, context, message);
        return;
    }
    std::println(stderr, "{}", qFormatLogMessage(type, context, message).toStdString());
}

}  // namespace

ExpectedQtMessage::ExpectedQtMessage(const QString& pattern, QtMsgType type)
  : m_pattern(pattern), m_type(type)
{
    EXPECT_TRUE(m_pattern.isValid()) << "not a regular expression: " << pattern.toStdString();
    Registry&              shared = registry();
    const std::scoped_lock lock(shared.mutex);
    shared.guards.push_back(this);
}

ExpectedQtMessage::~ExpectedQtMessage()
{
    Registry& shared = registry();
    int       seen   = 0;
    {
        const std::scoped_lock lock(shared.mutex);
        std::erase(shared.guards, this);
        seen = m_count;
    }
    if (m_pattern.isValid())  // else the constructor has failed the test
    {
        EXPECT_GT(seen, 0) << "no Qt " << typeName(m_type) << " matching '"
                           << m_pattern.pattern().toStdString() << "' came";
    }
}

int ExpectedQtMessage::count() const
{
    const std::scoped_lock lock(registry().mutex);
    return m_count;
}

int ExpectedQtMessage::droppedOffscreenWarnings()
{
    const std::scoped_lock lock(registry().mutex);
    return registry().droppedOffscreenWarnings;
}

void ExpectedQtMessage::installHandler()
{
    Registry&              shared = registry();
    const std::scoped_lock lock(shared.mutex);
    shared.previous = qInstallMessageHandler(&ExpectedQtMessage::handleMessage);
}

bool ExpectedQtMessage::matches(QtMsgType type, const QString& message) const
{
    // Not match() on an invalid pattern: it would log a warning, which would come back here while
    // the registry is locked.
    return type == m_type && m_pattern.isValid() && m_pattern.match(message).hasMatch();
}

void ExpectedQtMessage::handleMessage(QtMsgType type, const QMessageLogContext& context,
                                      const QString& message)
{
    Registry&        shared   = registry();
    QtMessageHandler previous = nullptr;
    {
        const std::scoped_lock lock(shared.mutex);
        const auto             expecting = std::ranges::find_if(
            shared.guards.rbegin(), shared.guards.rend(),
            [&](const ExpectedQtMessage* guard) { return guard->matches(type, message); });
        if (expecting != shared.guards.rend())
        {
            ++(*expecting)->m_count;
            return;
        }
        previous = shared.previous;
    }
    if (isOffscreenLimitation(type, message))
    {
        const std::scoped_lock lock(shared.mutex);
        ++shared.droppedOffscreenWarnings;
        return;
    }
    if (type == QtDebugMsg || type == QtInfoMsg || !insideTest())
    {
        forward(previous, type, context, message);
        return;
    }
    // The context names the file only where the code that logs was built with it (a debug build).
    const std::string where =
        context.file != nullptr ? std::format(" ({}:{})", context.file, context.line) : "";
    ADD_FAILURE() << "unexpected Qt " << typeName(type) << ": " << message.toStdString() << where;
    if (type == QtFatalMsg)
    {
        forward(previous, type, context, message);  // Qt aborts the program after this
    }
}

}  // namespace QtRocket::Gui::Test
