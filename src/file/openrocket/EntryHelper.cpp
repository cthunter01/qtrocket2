#include "QtRocket/file/openrocket/EntryHelper.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "QtRocket/file/openrocket/EntryHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/util/BigDecimal.h"
#include "QtRocket/util/Config.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// Whether @p text matches "^[+-]?[0-9]+$".
[[nodiscard]] bool isInteger(std::string_view text) noexcept
{
    if (text.starts_with('+') || text.starts_with('-'))
    {
        text.remove_prefix(1);
    }
    return !text.empty() && std::ranges::all_of(text, [](char c) { return c >= '0' && c <= '9'; });
}

/// The value of the decimal @p digits, negated when @p negative, or nullopt when it does not
/// fit 64 bits (Long.parseLong: the value is built as a negative number, whose range is the
/// larger one).
[[nodiscard]] std::optional<std::int64_t> toLong(bool negative, std::string_view digits) noexcept
{
    constexpr std::int64_t kMin  = std::numeric_limits<std::int64_t>::min();
    std::int64_t           value = 0;
    for (const char c : digits)
    {
        const std::int64_t digit = c - '0';
        if (value < (kMin + digit) / 10)
        {
            return std::nullopt;
        }
        value = (value * 10) - digit;
    }
    if (negative)
    {
        return value;
    }
    if (value == kMin)
    {
        return std::nullopt;
    }
    return -value;
}

/// The integer @p text (which isInteger()) with the narrowest of Java's types that holds it.
[[nodiscard]] std::optional<Config::Value> integerValue(std::string_view text)
{
    const bool             negative = text.starts_with('-');
    const std::string_view digits =
        text.starts_with('+') || text.starts_with('-') ? text.substr(1) : text;
    if (const std::optional<std::int64_t> value = toLong(negative, digits))
    {
        if (*value >= std::numeric_limits<std::int32_t>::min() &&
            *value <= std::numeric_limits<std::int32_t>::max())
        {
            return Config::Value(static_cast<std::int32_t>(*value));
        }
        return Config::Value(*value);
    }
    // Beyond a long: Java's BigInteger, which a Config keeps as new BigDecimal(its digits).
    const std::optional<BigDecimal> big = BigDecimal::parse(text);
    if (!big.has_value())
    {
        return std::nullopt;
    }
    return Config::Value(*big);
}

}  // namespace

std::optional<Config::Value> EntryHelper::getValueFromEntry(
    EntryHandler& handler, const ElementHandler::Attributes& attributes, std::string_view content)
{
    // The nested list is the closing entry's, whatever the attributes say the entry is: taken
    // here, it cannot be found again by a later entry (not OpenRocket's: see EntryHandler).
    std::optional<Config::List> nested = handler.takeNestedList();

    const auto type = attributes.find("type");
    if (type == attributes.end())
    {
        return std::nullopt;
    }
    if (type->second == "boolean")
    {
        return Config::Value(Strings::javaEqualsIgnoreCase(content, "true"));
    }
    if (type->second == "string")
    {
        return Config::Value(std::string(content));
    }
    if (type->second == "number")
    {
        return parseNumber(content);
    }
    if (type->second == "list" && nested.has_value())
    {
        return Config::Value(std::move(*nested));
    }
    return std::nullopt;
}

std::optional<Config::Value> EntryHelper::parseNumber(std::string_view text)
{
    const std::string_view str = Strings::trim(text);
    if (isInteger(str))
    {
        return integerValue(str);
    }
    const std::optional<BigDecimal> value = BigDecimal::parse(str);
    if (!value.has_value())
    {
        return std::nullopt;
    }
    const double                    asDouble = value->doubleValue();
    const std::optional<BigDecimal> back     = BigDecimal::valueOfDouble(asDouble);
    if (!back.has_value())
    {
        // BigDecimal.valueOf() of an infinity: a NumberFormatException, which Java catches.
        return std::nullopt;
    }
    if (*value == *back)
    {
        return Config::Value(asDouble);
    }
    return Config::Value(*value);
}

}  // namespace QtRocket
