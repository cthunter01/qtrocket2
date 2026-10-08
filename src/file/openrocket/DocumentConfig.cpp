#include "QtRocket/file/openrocket/DocumentConfig.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <optional>
#include <string_view>

#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

bool DocumentConfig::isSupportedVersion(std::string_view version) noexcept
{
    return std::ranges::find(kSupportedVersions, version) != kSupportedVersions.end();
}

Result<double> DocumentConfig::stringToDouble(std::optional<std::string_view> text)
{
    if (!text.has_value())
    {
        return fail(ErrorCode::INVALID_ARGUMENT, "null string");
    }
    if (Strings::javaEqualsIgnoreCase(*text, "NaN"))
    {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (Strings::javaEqualsIgnoreCase(*text, "Inf"))
    {
        return std::numeric_limits<double>::infinity();
    }
    if (Strings::javaEqualsIgnoreCase(*text, "-Inf"))
    {
        return -std::numeric_limits<double>::infinity();
    }
    if (const std::optional<double> value = Strings::javaParseDouble(*text))
    {
        return *value;
    }
    // FloatingDecimal.readJavaFormatString(): the text is trimmed before anything is said of it.
    const std::string_view trimmed = Strings::trim(*text);
    if (trimmed.empty())
    {
        return fail(ErrorCode::INVALID_ARGUMENT, "empty String");
    }
    return fail(ErrorCode::INVALID_ARGUMENT, std::format("For input string: \"{}\"", trimmed));
}

std::optional<double> DocumentConfig::parseFiniteDouble(std::string_view text) noexcept
{
    const std::optional<double> value = Strings::javaParseDouble(text);
    if (!value.has_value() || !std::isfinite(*value))
    {
        return std::nullopt;
    }
    return value;
}

}  // namespace QtRocket
