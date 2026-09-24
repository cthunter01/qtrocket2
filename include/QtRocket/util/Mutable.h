#pragma once

#include <format>
#include <source_location>
#include <string_view>

#include "QtRocket/util/BugError.h"

namespace QtRocket
{

/// Helps an object become immutable from some point on (OpenRocket's util/Mutable). The object
/// holds a Mutable, forwards its own immute() to immute(), and calls check() at the start of every
/// method that changes its state.
///
/// Writing to an object after immute() is a programming error, so check() throws BugError where
/// Java throws IllegalStateException. The error names the file and line of the immute() call, the
/// counterpart of the stack trace Java keeps ("Object has been made immutable at ...").
///
/// A copy has the source's state (Java: clone()), so a copy of an immutable object is immutable.
class Mutable
{
public:
    /// Makes the object immutable for good; repeated calls do nothing. @p where, the call site by
    /// default, is what check() reports afterwards.
    void immute(std::source_location where = std::source_location::current()) noexcept
    {
        if (m_mutable)
        {
            m_mutable   = false;
            m_immutedAt = where;
        }
    }

    /// Throws when immute() has been called, raised at @p where (the caller by default).
    /// @throws BugError "Object has been made immutable at <file>:<line>"
    void check(std::source_location where = std::source_location::current()) const
    {
        if (!m_mutable)
        {
            // The file is cut down to its base name, as BugError does with its own location.
            std::string_view file = m_immutedAt.file_name();
            file                  = file.substr(file.find_last_of("/\\") + 1);
            bug(std::format("Object has been made immutable at {}:{}", file, m_immutedAt.line()),
                where);
        }
    }

    /// Whether immute() has not been called yet.
    [[nodiscard]] bool isMutable() const noexcept { return m_mutable; }

private:
    bool                 m_mutable{true};
    std::source_location m_immutedAt;
};

}  // namespace QtRocket
