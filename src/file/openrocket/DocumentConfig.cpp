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

namespace
{

/// Whether FloatingDecimal.readJavaFormatString() fails @p trimmed, a text that is no number,
/// with "multiple points": it meets a second point in the digits and points the number starts
/// with, before it looks at anything else.
[[nodiscard]] bool hasMultiplePoints(std::string_view trimmed) noexcept
{
    if (trimmed.starts_with('+') || trimmed.starts_with('-'))
    {
        trimmed.remove_prefix(1);
    }
    // "NaN", "Infinity" and a hexadecimal number are told by their start and read apart.
    if (trimmed.starts_with('N') || trimmed.starts_with('I') || trimmed.starts_with("0x") ||
        trimmed.starts_with("0X"))
    {
        return false;
    }
    bool pointSeen = false;
    for (const char c : trimmed)
    {
        if (c == '.')
        {
            if (pointSeen)
            {
                return true;
            }
            pointSeen = true;
        }
        else if (c < '0' || c > '9')
        {
            break;
        }
    }
    return false;
}

}  // namespace

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
    return parseDouble(*text);
}

Result<double> DocumentConfig::parseDouble(std::string_view text)
{
    if (const std::optional<double> value = Strings::javaParseDouble(text))
    {
        return *value;
    }
    // FloatingDecimal.readJavaFormatString(): the text is trimmed before anything is said of it.
    const std::string_view trimmed = Strings::trim(text);
    if (trimmed.empty())
    {
        return fail(ErrorCode::INVALID_ARGUMENT, "empty String");
    }
    if (hasMultiplePoints(trimmed))
    {
        return fail(ErrorCode::INVALID_ARGUMENT, "multiple points");
    }
    return fail(ErrorCode::INVALID_ARGUMENT, std::format("For input string: \"{}\"", trimmed));
}

Result<int> DocumentConfig::parseInt(std::optional<std::string_view> text)
{
    if (!text.has_value())
    {
        return fail(ErrorCode::INVALID_ARGUMENT, "Cannot parse null string");
    }
    if (const std::optional<int> value = Strings::parseInt(*text))
    {
        return *value;
    }
    // NumberFormatException.forInputString(): the text as it is, nothing trimmed.
    return fail(ErrorCode::INVALID_ARGUMENT, std::format("For input string: \"{}\"", *text));
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
