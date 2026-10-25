#include "QtRocket/file/preset/JaxbLexical.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string_view>

#include "QtRocket/util/Strings.h"

namespace QtRocket::JaxbLexical
{

namespace
{

/// Whether a double's text may start and end with @p c: a digit, '.', '+' or '-'.
[[nodiscard]] constexpr bool isNumberEnd(char c) noexcept
{
    return (c >= '0' && c <= '9') || c == '.' || c == '+' || c == '-';
}

/// The largest code point a single UTF-16 unit holds.
constexpr char32_t kLastBmpCodePoint = 0xFFFF;

}  // namespace

std::string_view trim(std::string_view text) noexcept
{
    while (!text.empty() && isWhiteSpace(text.front()))
    {
        text.remove_prefix(1);
    }
    while (!text.empty() && isWhiteSpace(text.back()))
    {
        text.remove_suffix(1);
    }
    return text;
}

bool isWhiteSpaceOnly(std::string_view text) noexcept
{
    return std::ranges::all_of(text, [](char c) { return isWhiteSpace(c); });
}

std::optional<double> parseDouble(std::string_view text) noexcept
{
    const std::string_view trimmed = trim(text);
    if (trimmed == "NaN")
    {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (trimmed == "INF")
    {
        return std::numeric_limits<double>::infinity();
    }
    if (trimmed == "-INF")
    {
        return -std::numeric_limits<double>::infinity();
    }
    if (trimmed.empty() || !isNumberEnd(trimmed.front()) || !isNumberEnd(trimmed.back()))
    {
        return std::nullopt;
    }
    return Strings::javaParseDouble(trimmed);
}

std::optional<int> parseInt(std::string_view text) noexcept
{
    // Unsigned arithmetic wraps around as Java's int does, without undefined behaviour.
    std::uint32_t magnitude = 0;
    bool          negative  = false;
    for (const char c : text)
    {
        if (isWhiteSpace(c) || c == '+')
        {
            continue;
        }
        if (c == '-')
        {
            negative = true;
        }
        else if (c >= '0' && c <= '9')
        {
            magnitude = (magnitude * 10U) + static_cast<std::uint32_t>(c - '0');
        }
        else
        {
            return std::nullopt;
        }
    }
    const std::uint32_t bits = negative ? 0U - magnitude : magnitude;
    return static_cast<std::int32_t>(bits);
}

ParsedBoolean parseBoolean(std::string_view text) noexcept
{
    if (text.empty())
    {
        return ParsedBoolean::IS_NULL;
    }
    std::size_t position = 0;
    while (position < text.size() && isWhiteSpace(text[position]))
    {
        position++;
    }
    if (position == text.size())
    {
        return ParsedBoolean::IS_FALSE;
    }

    ParsedBoolean value = ParsedBoolean::IS_FALSE;
    switch (const char first = text[position]; first)
    {
        case '1':
            value = ParsedBoolean::IS_TRUE;
            position++;
            break;
        case '0':
            position++;
            break;
        case 't':
        case 'f':
        {
            position++;
            if (position == text.size())
            {
                return ParsedBoolean::REJECTED;
            }
            const std::string_view rest = first == 't' ? "rue" : "alse";
            if (text.substr(position, rest.size()) != rest)
            {
                return ParsedBoolean::IS_NULL;
            }
            value = first == 't' ? ParsedBoolean::IS_TRUE : ParsedBoolean::IS_FALSE;
            position += rest.size();
            break;
        }
        default:
            // One character to Java: a code point beyond U+FFFF is two, the second of which
            // is no white space.
            if (Strings::nextCodePoint(text, position) > kLastBmpCodePoint)
            {
                return ParsedBoolean::IS_NULL;
            }
            break;
    }
    return isWhiteSpaceOnly(text.substr(position)) ? value : ParsedBoolean::IS_NULL;
}

std::optional<std::size_t> parseEnum(std::string_view                  text,
                                     std::span<const std::string_view> names) noexcept
{
    const auto found = std::ranges::find(names, text);
    if (found == names.end())
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(found - names.begin());
}

}  // namespace QtRocket::JaxbLexical
