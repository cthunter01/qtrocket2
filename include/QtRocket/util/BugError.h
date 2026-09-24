#pragma once

#include <source_location>
#include <stdexcept>
#include <string_view>

namespace QtRocket
{

/// Thrown when a bug is noticed (OpenRocket's BugException): an invariant that the code itself
/// guarantees has been broken, so the failure is a programming error, never a user's. Core code
/// never catches it; the application boundary reports it as a crash with the location.
///
/// what() reads "BUG: <message> (<file>:<line>)", the file being the base name only.
///
/// OpenRocket's BugException extends FatalException, an abstract RuntimeException that only marks
/// the "fatal" family (with ConfigurationException); nothing in core catches it, so the port
/// derives from std::logic_error directly, without that layer. The (Throwable cause) constructors
/// have no counterpart either: C++ has no exception chaining (std::nested_exception is unused).
class BugError : public std::logic_error
{
public:
    explicit BugError(std::string_view     message,
                      std::source_location where = std::source_location::current());

    /// Where the bug was raised: the call site of the constructor, bug() or the macros.
    [[nodiscard]] const std::source_location& where() const noexcept { return m_where; }

private:
    std::source_location m_where;
};

/// Throws a BugError for @p message, raised at @p where (the caller by default).
[[noreturn]] void bug(std::string_view     message,
                      std::source_location where = std::source_location::current());

}  // namespace QtRocket

/// Checks an invariant in every build type (unlike assert()) and throws a BugError naming the
/// condition and the location when it does not hold. The condition is evaluated exactly once.
#define QTROCKET_ASSERT(condition) \
    ((condition) ? static_cast<void>(0) : ::QtRocket::bug("assertion failed: " #condition))

/// Marks a code path that must not be reached, such as the default of an exhaustive switch.
#define QTROCKET_UNREACHABLE() ::QtRocket::bug("unreachable code reached")
