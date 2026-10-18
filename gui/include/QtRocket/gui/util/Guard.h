#pragma once

#include <concepts>
#include <cstddef>
#include <exception>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

#include <QString>

/// The GUI's boundary for exceptions (OpenRocket's SwingExceptionHandler): nothing may leave a
/// slot into Qt's event loop, which cannot carry an exception through, and the core never catches
/// a BugError. So every connection into the core goes through guarded(), which catches whatever
/// the call throws and reports it through the process-wide error hook; the application's
/// QApplication::notify() is the backstop for the rest (app/Application). The default hook writes
/// the report to the standard error; the application installs one that opens the internal error
/// dialog, and a test one that records the reports.
///
/// Only the GUI thread reports (Milestone 2 runs nothing on another thread).
namespace QtRocket::Gui
{

/// The error hook and the reports (OpenRocket's ExceptionHandler).
class Guard
{
public:
    /// What a guarded call threw, or the error condition the code reported.
    struct Failure
    {
        enum class Kind
        {
            /// A BugError: a broken invariant (OpenRocket's BugException).
            BUG,
            /// Another std::exception (std::bad_alloc, a std::runtime_error out of a library, ...).
            EXCEPTION,
            /// Something that is not a std::exception.
            UNKNOWN,
            /// No exception: an error condition the code reported with reportErrorCondition()
            /// (OpenRocket's handleErrorCondition(String)), such as a resource that is missing.
            ERROR_CONDITION,
        };

        Kind kind = Kind::UNKNOWN;
        /// what() of the exception ("BUG: <message> (<file>:<line>)" for a BugError), the
        /// condition's message, or "unknown exception".
        QString message;
        /// Where a BugError was raised (BugError::where()): the file, the line and the function;
        /// empty and 0 for the other kinds.
        QString file;
        int     line = 0;
        QString function;
        /// What the guarded code was doing, as the code that guarded it says ("Delete"); may be
        /// empty.
        QString context;
    };

    /// Receives every report. It may open a dialog (a nested event loop); a failure reported
    /// while it runs goes to the standard error instead (OpenRocket ignores an exception while it
    /// handles one). Whatever it throws is caught and written to the standard error.
    using Hook = std::function<void(const Failure&)>;

    Guard() = delete;

    /// Installs @p hook for the whole process and returns the hook it replaces (empty for the
    /// default one). An empty @p hook brings the default back. Whoever installs a hook puts the
    /// previous one back when it ends (a test, the application).
    static Hook setHook(Hook hook);

    /// Hands @p failure to the hook. Never throws.
    static void report(const Failure& failure) noexcept;

    /// Reports the exception being handled; only inside a catch block. Never throws.
    static void reportCurrentException(const QString& context = {}) noexcept;

    /// Reports an error condition without an exception (Kind::ERROR_CONDITION). Never throws.
    static void reportErrorCondition(const QString& message, const QString& context = {}) noexcept;

    /// The failure that @p exception is: a BugError, another std::exception or something else.
    /// A QUnhandledException (an exception QtConcurrent carried over from another thread) is the
    /// failure of the exception it carries.
    [[nodiscard]] static Failure failureOf(const std::exception_ptr& exception,
                                           const QString&            context);

    /// The text the default hook writes: "QtRocket: internal error: <message>", with
    /// " (in <context>)" when the failure names one.
    [[nodiscard]] static QString describe(const Failure& failure);

    /// Calls @p callable; what it throws is reported with @p context. True when it returned
    /// normally.
    template <class Callable>
        requires std::invocable<Callable>
    static bool run(Callable&& callable, const QString& context = {}) noexcept
    {
        try
        {
            std::invoke(std::forward<Callable>(callable));
            return true;
        }
        catch (...)
        {
            reportCurrentException(context);
            return false;
        }
    }
};

namespace GuardDetail
{

/// Whether @p Callable can be called with lvalues of the first @p Count types of @p Tuple.
template <class Callable, class Tuple, std::size_t Count>
[[nodiscard]] constexpr bool invocableWithFirst()
{
    return []<std::size_t... Indices>(std::index_sequence<Indices...> /*indices*/) {
        return std::is_invocable_v<
            Callable&, std::add_lvalue_reference_t<std::tuple_element_t<Indices, Tuple>>...>;
    }(std::make_index_sequence<Count>{});
}

/// The most leading arguments of @p Tuple that @p Callable takes.
template <class Callable, class Tuple, std::size_t Count = std::tuple_size_v<Tuple>>
[[nodiscard]] constexpr std::size_t leadingArgumentCount()
{
    if constexpr (invocableWithFirst<Callable, Tuple, Count>())
    {
        return Count;
    }
    else
    {
        static_assert(Count > 0, "guarded(): the callable takes none of the signal's arguments");
        return leadingArgumentCount<Callable, Tuple, Count - 1>();
    }
}

template <class Callable, class Tuple, std::size_t... Indices>
void invokeWithIndices(Callable& callable, Tuple& arguments,
                       std::index_sequence<Indices...> /*indices*/)
{
    std::invoke(callable, std::get<Indices>(arguments)...);
}

/// Calls @p callable with as many of the leading @p arguments as it takes, as a slot drops the
/// signal's trailing arguments; each one is passed as an lvalue.
template <class Callable, class... Arguments>
void invokeWithLeadingArguments(Callable& callable, Arguments&&... arguments)
{
    using Tuple = std::tuple<Arguments&&...>;
    Tuple values(std::forward<Arguments>(arguments)...);
    invokeWithIndices(callable, values,
                      std::make_index_sequence<leadingArgumentCount<Callable, Tuple>()>{});
}

}  // namespace GuardDetail

/// A slot that calls @p callable and reports what it throws (Guard::run() with @p context):
/// connect(button, &QPushButton::clicked, this, guarded([this] { deleteSelection(); })). The
/// slot takes the signal's arguments and passes on as many of the leading ones as @p callable
/// takes; it never throws.
template <class Callable>
[[nodiscard]] auto guarded(Callable&& callable, QString context = {})
{
    return [callable = std::forward<Callable>(callable),
            context  = std::move(context)](auto&&... arguments) mutable noexcept -> void {
        try
        {
            GuardDetail::invokeWithLeadingArguments(
                callable, std::forward<decltype(arguments)>(arguments)...);
        }
        catch (...)
        {
            Guard::reportCurrentException(context);
        }
    };
}

}  // namespace QtRocket::Gui
