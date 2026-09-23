#pragma once

#include <expected>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>

namespace QtRocket
{

/// Why an operation failed. Recoverable failures are reported through Result, never thrown.
enum class ErrorCode
{
    UNKNOWN,
    IO,                   ///< a file or stream could not be read or written
    PARSE,                ///< malformed content (XML, motor file, archive, ...)
    UNSUPPORTED_FORMAT,   ///< a file type this version cannot read
    UNSUPPORTED_VERSION,  ///< a known file type in a version this version cannot read
    INVALID_ARGUMENT,     ///< a caller-supplied value is out of range or inconsistent
    NOT_FOUND,            ///< a lookup (component, motor, entry, ...) found nothing
    DATABASE,             ///< the motor database could not be opened or queried
    CANCELLED,            ///< the operation was cancelled by the caller
    SIMULATION_ABORTED,   ///< the simulation stopped early (see the abort cause in its warnings)
};

[[nodiscard]] std::string_view toString(ErrorCode code) noexcept;

/// A failure: what went wrong, a message for the user, and where in the code it was raised.
struct Error
{
    ErrorCode            code{ErrorCode::UNKNOWN};
    std::string          message;
    std::source_location where;

    /// e.g. "PARSE: unexpected element <foo> (OpenRocketLoader.cpp:120)"
    [[nodiscard]] std::string toString() const;
};

/// The return type of every operation that can fail in a recoverable way.
template <class T>
using Result = std::expected<T, Error>;

/// Builds the failure side of a Result: `return fail(ErrorCode::PARSE, "...");`
[[nodiscard]] inline std::unexpected<Error> fail(
    ErrorCode code, std::string message,
    std::source_location where = std::source_location::current())
{
    return std::unexpected(Error{.code = code, .message = std::move(message), .where = where});
}

}  // namespace QtRocket
