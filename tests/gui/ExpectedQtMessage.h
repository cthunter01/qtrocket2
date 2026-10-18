#pragma once

#include <QMessageLogContext>
#include <QRegularExpression>
#include <QString>

namespace QtRocket::Gui::Test
{

/// A Qt message (qWarning(), qCritical(), a model tester's report, a failed connect(), ...) that
/// a test expects, for as long as the guard lives: a message of its type that matches its pattern
/// is counted and does not fail the test, and the guard fails the test when it is destroyed
/// without having seen one. It takes the place of QTest::ignoreMessage(), which only works inside
/// QtTest's own runner.
///
/// The message handler that GuiTestMain.cpp installs (installHandler()) turns every other message
/// of warning level or above during a test into a GoogleTest failure; debug and info messages,
/// and messages outside a test, go on to Qt's handler, which prints them. Guards may be nested:
/// the most recent one that matches counts the message.
class ExpectedQtMessage
{
public:
    /// Expects a message of @p type whose text matches the regular expression @p pattern.
    explicit ExpectedQtMessage(const QString& pattern, QtMsgType type = QtWarningMsg);
    ~ExpectedQtMessage();
    ExpectedQtMessage(const ExpectedQtMessage&)            = delete;
    ExpectedQtMessage& operator=(const ExpectedQtMessage&) = delete;
    ExpectedQtMessage(ExpectedQtMessage&&)                 = delete;
    ExpectedQtMessage& operator=(ExpectedQtMessage&&)      = delete;

    /// How many expected messages have come so far.
    [[nodiscard]] int count() const;

    /// Installs the message handler for the whole test run (once, in main(), after
    /// InitGoogleTest()).
    static void installHandler();

private:
    static void        handleMessage(QtMsgType type, const QMessageLogContext& context,
                                     const QString& message);
    [[nodiscard]] bool matches(QtMsgType type, const QString& message) const;

    QRegularExpression m_pattern;
    QtMsgType          m_type;
    int                m_count = 0;
};

}  // namespace QtRocket::Gui::Test
