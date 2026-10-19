#include "QtRocket/gui/util/Guard.h"

#include <cstdio>
#include <exception>
#include <mutex>
#include <utility>

#include <QByteArray>
#include <QString>
#include <QUnhandledException>

#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The hook and whether it is running, guarded by a mutex (the hook is called outside it).
struct HookState
{
    std::mutex  mutex;
    Guard::Hook hook;
    bool        handling = false;
};

HookState& hookState()
{
    static HookState s_state;
    return s_state;
}

/// Writes @p text and a line end to the standard error; never throws.
void writeToStandardError(const QString& text) noexcept
{
    try
    {
        const QByteArray bytes = (text + QStringLiteral("\n")).toUtf8();
        static_cast<void>(std::fputs(bytes.constData(), stderr));
    }
    catch (...)
    {
        static_cast<void>(std::fputs("QtRocket: internal error\n", stderr));
    }
}

/// Marks the hook as running for as long as it lives, if it was not already.
class HandlingScope
{
public:
    explicit HandlingScope(HookState& state) : m_state(state)
    {
        const std::scoped_lock lock(m_state.mutex);
        if (!m_state.handling)
        {
            m_entered        = true;
            m_state.handling = true;
            m_hook           = m_state.hook;
        }
    }
    ~HandlingScope()
    {
        if (m_entered)
        {
            const std::scoped_lock lock(m_state.mutex);
            m_state.handling = false;
        }
    }
    HandlingScope(const HandlingScope&)            = delete;
    HandlingScope& operator=(const HandlingScope&) = delete;
    HandlingScope(HandlingScope&&)                 = delete;
    HandlingScope& operator=(HandlingScope&&)      = delete;

    /// False when the hook was already running.
    [[nodiscard]] bool entered() const noexcept { return m_entered; }
    /// The hook to call (empty for the default).
    [[nodiscard]] const Guard::Hook& hook() const noexcept { return m_hook; }

private:
    HookState&  m_state;
    bool        m_entered = false;
    Guard::Hook m_hook;
};

}  // namespace

Guard::Hook Guard::setHook(Hook hook)
{
    HookState&             state = hookState();
    const std::scoped_lock lock(state.mutex);
    return std::exchange(state.hook, std::move(hook));
}

void Guard::report(const Failure& failure) noexcept
{
    try
    {
        const HandlingScope scope(hookState());
        if (!scope.entered())
        {
            // A failure while the hook handles another one (OpenRocket logs and ignores it).
            writeToStandardError(describe(failure));
            return;
        }
        if (scope.hook())
        {
            scope.hook()(failure);
        }
        else
        {
            writeToStandardError(describe(failure));
        }
    }
    catch (...)
    {
        static_cast<void>(
            std::fputs("QtRocket: internal error while reporting an internal error\n", stderr));
    }
}

void Guard::reportCurrentException(const QString& context) noexcept
{
    try
    {
        report(failureOf(std::current_exception(), context));
    }
    catch (...)
    {
        static_cast<void>(std::fputs("QtRocket: internal error\n", stderr));
    }
}

void Guard::reportErrorCondition(const QString& message, const QString& context) noexcept
{
    try
    {
        Failure failure;
        failure.kind    = Failure::Kind::ERROR_CONDITION;
        failure.message = message;
        failure.context = context;
        report(failure);
    }
    catch (...)
    {
        static_cast<void>(std::fputs("QtRocket: internal error\n", stderr));
    }
}

Guard::Failure Guard::failureOf(const std::exception_ptr& exception, const QString& context)
{
    Failure failure;
    failure.context = context;
    if (!exception)
    {
        failure.message = QStringLiteral("unknown exception");
        return failure;
    }
    try
    {
        std::rethrow_exception(exception);
    }
    catch (const BugError& error)
    {
        failure.kind     = Failure::Kind::BUG;
        failure.message  = QString::fromUtf8(error.what());
        failure.file     = QString::fromUtf8(error.where().file_name());
        failure.line     = static_cast<int>(error.where().line());
        failure.function = QString::fromUtf8(error.where().function_name());
    }
    catch (const QUnhandledException& error)
    {
        if (error.exception())
        {
            return failureOf(error.exception(), context);
        }
        failure.kind    = Failure::Kind::EXCEPTION;
        failure.message = QString::fromUtf8(error.what());
    }
    catch (const std::exception& error)
    {
        failure.kind    = Failure::Kind::EXCEPTION;
        failure.message = QString::fromUtf8(error.what());
    }
    catch (...)
    {
        failure.kind    = Failure::Kind::UNKNOWN;
        failure.message = QStringLiteral("unknown exception");
    }
    return failure;
}

QString Guard::describe(const Failure& failure)
{
    QString text = QStringLiteral("QtRocket: internal error: ") + failure.message;
    if (!failure.context.isEmpty())
    {
        text += QStringLiteral(" (in ") + failure.context + QStringLiteral(")");
    }
    return text;
}

}  // namespace QtRocket::Gui
