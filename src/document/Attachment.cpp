#include "QtRocket/document/Attachment.h"

#include <expected>
#include <format>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>

#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

Attachment::Attachment(std::string name) : m_name(std::move(name)) { }

int Attachment::compareTo(const Attachment& other) const noexcept
{
    return Strings::javaCompareTo(m_name, other.m_name);
}

std::unexpected<Error> decalNotFound(std::string_view source, std::source_location where)
{
    // messages.properties, ExportDecalDialog.source.exception (a MessageFormat pattern, whose ''
    // is one quote).
    return fail(ErrorCode::NOT_FOUND,
                std::format("Could not find decal source file '{}'. <br> <br>Would you like to "
                            "look for this file?",
                            source),
                where);
}

}  // namespace QtRocket
