#include "QtRocket/file/openrocket/EntryHelper.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include <gtest/gtest.h>

#include "QtRocket/file/openrocket/ConfigHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/util/BigDecimal.h"
#include "QtRocket/util/Config.h"
#include "QtRocket/util/Strings.h"
#include "file/openrocket/EntryTestSupport.h"

// The expectations are what OpenRocket's EntryHelper.getValueFromEntry() returns for the same
// texts (probe EntryProbe of part D4).

namespace
{

using QtRocket::BigDecimal;
using QtRocket::Config;
using QtRocket::ConfigHandler;
using QtRocket::ElementHandler;
using QtRocket::EntryHelper;
using QtRocket::Test::describe;

/// The value of an entry of type @p type with the text @p content, described, or "null".
[[nodiscard]] std::string valueOf(std::optional<std::string_view> type, std::string_view content)
{
    const ConfigHandler        handler;
    ElementHandler::Attributes attributes;
    if (type.has_value())
    {
        attributes.emplace("type", *type);
    }
    const std::optional<Config::Value> value =
        EntryHelper::getValueFromEntry(handler, attributes, content);
    return value.has_value() ? describe(*value) : "null";
}

/// The Java type OpenRocket gives a number.
enum class Number
{
    NONE,         ///< no value: the entry is dropped
    INTEGER,      ///< value: the decimal digits
    LONG,         ///< value: the decimal digits
    BIG_DECIMAL,  ///< value: "<toString()>|<unscaled value>|<scale>" (a BigInteger has scale 0)
    DOUBLE,       ///< value: Java's Double.toString()
};

struct NumberCase
{
    std::string_view text;
    Number           type;
    std::string_view value;
};

/// "number [<text>] -> <type> <value>" of the probe, section A.
// clang-format off
constexpr auto kNumbers = std::to_array<NumberCase>({
    {.text = "0", .type = Number::INTEGER, .value = "0"},
    {.text = "-0", .type = Number::INTEGER, .value = "0"},
    {.text = "+0", .type = Number::INTEGER, .value = "0"},
    {.text = "5", .type = Number::INTEGER, .value = "5"},
    {.text = "+5", .type = Number::INTEGER, .value = "5"},
    {.text = "-5", .type = Number::INTEGER, .value = "-5"},
    {.text = "007", .type = Number::INTEGER, .value = "7"},
    {.text = "250", .type = Number::INTEGER, .value = "250"},
    {.text = "2147483647", .type = Number::INTEGER, .value = "2147483647"},
    {.text = "2147483648", .type = Number::LONG, .value = "2147483648"},
    {.text = "-2147483648", .type = Number::INTEGER, .value = "-2147483648"},
    {.text = "-2147483649", .type = Number::LONG, .value = "-2147483649"},
    {.text = "4294967296", .type = Number::LONG, .value = "4294967296"},
    {.text = "9223372036854775807", .type = Number::LONG, .value = "9223372036854775807"},
    {.text = "9223372036854775808", .type = Number::BIG_DECIMAL, .value = "9223372036854775808|9223372036854775808|0"},
    {.text = "-9223372036854775808", .type = Number::LONG, .value = "-9223372036854775808"},
    {.text = "-9223372036854775809", .type = Number::BIG_DECIMAL, .value = "-9223372036854775809|-9223372036854775809|0"},
    {.text = "123456789012345678901234567890", .type = Number::BIG_DECIMAL, .value = "123456789012345678901234567890|123456789012345678901234567890|0"},
    {.text = "12345678901234567890", .type = Number::BIG_DECIMAL, .value = "12345678901234567890|12345678901234567890|0"},
    {.text = "+12345678901234567890", .type = Number::BIG_DECIMAL, .value = "12345678901234567890|12345678901234567890|0"},
    {.text = "-123456789012345678901234567890", .type = Number::BIG_DECIMAL, .value = "-123456789012345678901234567890|-123456789012345678901234567890|0"},
    {.text = " 42 ", .type = Number::INTEGER, .value = "42"},
    {.text = "\t42\n", .type = Number::INTEGER, .value = "42"},
    {.text = "42 7", .type = Number::NONE, .value = ""},
    {.text = "0.0", .type = Number::DOUBLE, .value = "0.0"},
    {.text = "-0.0", .type = Number::DOUBLE, .value = "0.0"},
    {.text = "2.5", .type = Number::DOUBLE, .value = "2.5"},
    {.text = "2.50", .type = Number::BIG_DECIMAL, .value = "2.50|250|2"},
    {.text = "+2.5", .type = Number::DOUBLE, .value = "2.5"},
    {.text = "-2.5", .type = Number::DOUBLE, .value = "-2.5"},
    {.text = ".5", .type = Number::DOUBLE, .value = "0.5"},
    {.text = "5.", .type = Number::BIG_DECIMAL, .value = "5|5|0"},
    {.text = "0.1", .type = Number::DOUBLE, .value = "0.1"},
    {.text = "0.10", .type = Number::BIG_DECIMAL, .value = "0.10|10|2"},
    {.text = "0.1000000000000000055511151231257827", .type = Number::BIG_DECIMAL, .value = "0.1000000000000000055511151231257827|1000000000000000055511151231257827|34"},
    {.text = "1e1", .type = Number::BIG_DECIMAL, .value = "1E+1|1|-1"},
    {.text = "1E1", .type = Number::BIG_DECIMAL, .value = "1E+1|1|-1"},
    {.text = "1.0E1", .type = Number::BIG_DECIMAL, .value = "10|10|0"},
    {.text = "1e-1", .type = Number::DOUBLE, .value = "0.1"},
    {.text = "1E+3", .type = Number::BIG_DECIMAL, .value = "1E+3|1|-3"},
    {.text = "1.5e300", .type = Number::DOUBLE, .value = "1.5E300"},
    {.text = "1e400", .type = Number::NONE, .value = ""},
    {.text = "1e-400", .type = Number::BIG_DECIMAL, .value = "1E-400|1|400"},
    {.text = "1E2147483647", .type = Number::NONE, .value = ""},
    {.text = "100.0", .type = Number::DOUBLE, .value = "100.0"},
    {.text = "1.0E10", .type = Number::DOUBLE, .value = "1.0E10"},
    {.text = "1.0E7", .type = Number::DOUBLE, .value = "1.0E7"},
    {.text = "9999999.0", .type = Number::DOUBLE, .value = "9999999.0"},
    {.text = "1.0E-4", .type = Number::DOUBLE, .value = "1.0E-4"},
    {.text = "0.001", .type = Number::DOUBLE, .value = "0.001"},
    {.text = "1.0E-3", .type = Number::BIG_DECIMAL, .value = "0.0010|10|4"},
    {.text = "123456.789", .type = Number::DOUBLE, .value = "123456.789"},
    {.text = "3.141592653589793", .type = Number::DOUBLE, .value = "3.141592653589793"},
    {.text = "0.30000000000000004", .type = Number::DOUBLE, .value = "0.30000000000000004"},
    {.text = "1.7976931348623157E308", .type = Number::DOUBLE, .value = "1.7976931348623157E308"},
    {.text = "4.9E-324", .type = Number::DOUBLE, .value = "4.9E-324"},
    {.text = "2.2250738585072014E-308", .type = Number::DOUBLE, .value = "2.2250738585072014E-308"},
    {.text = "NaN", .type = Number::NONE, .value = ""},
    {.text = "Infinity", .type = Number::NONE, .value = ""},
    {.text = "-Infinity", .type = Number::NONE, .value = ""},
    {.text = "Inf", .type = Number::NONE, .value = ""},
    {.text = "abc", .type = Number::NONE, .value = ""},
    {.text = "", .type = Number::NONE, .value = ""},
    {.text = " ", .type = Number::NONE, .value = ""},
    {.text = "0x10", .type = Number::NONE, .value = ""},
    {.text = "1f", .type = Number::NONE, .value = ""},
    {.text = "1.0d", .type = Number::NONE, .value = ""},
    {.text = "1_000", .type = Number::NONE, .value = ""},
    {.text = "1,5", .type = Number::NONE, .value = ""},
    {.text = "--5", .type = Number::NONE, .value = ""},
    {.text = "+-5", .type = Number::NONE, .value = ""},
    {.text = "+", .type = Number::NONE, .value = ""},
    {.text = "-", .type = Number::NONE, .value = ""},
    {.text = ".", .type = Number::NONE, .value = ""},
    {.text = "e5", .type = Number::NONE, .value = ""},
    {.text = "5e", .type = Number::NONE, .value = ""},
    {.text = "5 ", .type = Number::INTEGER, .value = "5"},
    {.text = " 5", .type = Number::INTEGER, .value = "5"},
    {.text = "\xC2\xA0" "5", .type = Number::NONE, .value = ""},
    {.text = "5\xC2\xA0", .type = Number::NONE, .value = ""},
    {.text = "\xE2\x80\x83" "5", .type = Number::NONE, .value = ""},
    {.text = "1.0 ", .type = Number::DOUBLE, .value = "1.0"},
    {.text = "1L", .type = Number::NONE, .value = ""},
    {.text = "1e5", .type = Number::BIG_DECIMAL, .value = "1E+5|1|-5"},
    {.text = "100000.0", .type = Number::DOUBLE, .value = "100000.0"},
    {.text = "1E5", .type = Number::BIG_DECIMAL, .value = "1E+5|1|-5"},
    {.text = "12345678.9", .type = Number::DOUBLE, .value = "1.23456789E7"},
    {.text = "0.00001", .type = Number::BIG_DECIMAL, .value = "0.00001|1|5"},
    {.text = "1.0E-5", .type = Number::DOUBLE, .value = "1.0E-5"},
    {.text = "1.00E-5", .type = Number::BIG_DECIMAL, .value = "0.0000100|100|7"},
    {.text = "1.0E+1", .type = Number::BIG_DECIMAL, .value = "10|10|0"},
    {.text = "9007199254740993", .type = Number::LONG, .value = "9007199254740993"},
    {.text = "9007199254740993.0", .type = Number::BIG_DECIMAL, .value = "9007199254740993.0|90071992547409930|1"},
    {.text = "1.0000000000000001", .type = Number::BIG_DECIMAL, .value = "1.0000000000000001|10000000000000001|16"},
    {.text = "-1.5E-7", .type = Number::DOUBLE, .value = "-1.5E-7"},
    {.text = "5E-324", .type = Number::BIG_DECIMAL, .value = "5E-324|5|324"},
    {.text = "3E-324", .type = Number::BIG_DECIMAL, .value = "3E-324|3|324"},
});
// clang-format on

/// A double by its bits, so that it is compared as a number whatever digits print it.
[[nodiscard]] std::string bitsOf(double value)
{
    return std::format("{:016x}", std::bit_cast<std::uint64_t>(value));
}

/// What the table says of @p number: "none", "Integer <digits>", "Long <digits>", "BigDecimal
/// <toString()>|<unscaled value>|<scale>" or "Double <bits>".
[[nodiscard]] std::string expected(const NumberCase& number)
{
    switch (number.type)
    {
        case Number::NONE:
            return "none";
        case Number::INTEGER:
            return std::format("Integer {}", number.value);
        case Number::LONG:
            return std::format("Long {}", number.value);
        case Number::BIG_DECIMAL:
            return std::format("BigDecimal {}", number.value);
        case Number::DOUBLE:
            return "Double " +
                   bitsOf(QtRocket::Strings::javaParseDouble(number.value).value_or(-1.0));
    }
    return "?";
}

/// The same of what parseNumber() makes of @p text.
[[nodiscard]] std::string parsed(std::string_view text)
{
    const std::optional<Config::Value> value = EntryHelper::parseNumber(text);
    if (!value.has_value())
    {
        return "none";
    }
    if (const auto* integer = std::get_if<std::int32_t>(&value->variant()))
    {
        return std::format("Integer {}", *integer);
    }
    if (const auto* big = std::get_if<std::int64_t>(&value->variant()))
    {
        return std::format("Long {}", *big);
    }
    if (const auto* decimal = std::get_if<BigDecimal>(&value->variant()))
    {
        return std::format("BigDecimal {}|{}|{}", decimal->toString(), decimal->unscaledValue(),
                           decimal->scale());
    }
    if (const auto* real = std::get_if<double>(&value->variant()))
    {
        return "Double " + bitsOf(*real);
    }
    return "another type: " + describe(*value);
}

TEST(EntryHelper, ANumberHasJavasTypeAndValue)
{
    for (const NumberCase& number : kNumbers)
    {
        EXPECT_EQ(parsed(number.text), expected(number)) << "[" << number.text << "]";
    }
}

TEST(EntryHelper, TheNumberTableCoversEveryType)
{
    std::array<int, 5> counts{};
    for (const NumberCase& number : kNumbers)
    {
        counts.at(static_cast<std::size_t>(number.type))++;
    }
    EXPECT_EQ(kNumbers.size(), 97U);
    EXPECT_EQ(counts, (std::array<int, 5>{26, 14, 6, 25, 26}));
}

TEST(EntryHelper, ANumberEntryIsParsedAsANumber)
{
    // Through getValueFromEntry(), the probe's own notation.
    EXPECT_EQ(valueOf("number", "250"), "Integer 250");
    EXPECT_EQ(valueOf("number", "4294967296"), "Long 4294967296");
    EXPECT_EQ(valueOf("number", "123456789012345678901234567890"),
              "BigDecimal 123456789012345678901234567890 "
              "unscaled=123456789012345678901234567890 scale=0");
    EXPECT_EQ(valueOf("number", "2.5"), "Double 2.5");
    EXPECT_EQ(valueOf("number", "2.50"), "BigDecimal 2.50 unscaled=250 scale=2");
    EXPECT_EQ(valueOf("number", "\n  2.5\n"), "Double 2.5");
    EXPECT_EQ(valueOf("number", "abc"), "null");
    EXPECT_EQ(valueOf("number", ""), "null");
}

TEST(EntryHelper, TheIntegerTypesChangeAtJavasLimits)
{
    EXPECT_EQ(valueOf("number", "2147483647"), "Integer 2147483647");
    EXPECT_EQ(valueOf("number", "2147483648"), "Long 2147483648");
    EXPECT_EQ(valueOf("number", "-2147483648"), "Integer -2147483648");
    EXPECT_EQ(valueOf("number", "-2147483649"), "Long -2147483649");
    EXPECT_EQ(valueOf("number", "9223372036854775807"), "Long 9223372036854775807");
    EXPECT_EQ(valueOf("number", "-9223372036854775808"), "Long -9223372036854775808");
    EXPECT_EQ(valueOf("number", "9223372036854775808"),
              "BigDecimal 9223372036854775808 unscaled=9223372036854775808 scale=0");
    EXPECT_EQ(valueOf("number", "-9223372036854775809"),
              "BigDecimal -9223372036854775809 unscaled=-9223372036854775809 scale=0");
    // Zeros in front change nothing, however many.
    EXPECT_EQ(valueOf("number", "000000000000000000000000000002147483648"), "Long 2147483648");
    EXPECT_EQ(valueOf("number", "+00000000000000000000000000009223372036854775808"),
              "BigDecimal 9223372036854775808 unscaled=9223372036854775808 scale=0");
}

TEST(EntryHelper, DigitsOfOtherScriptsAreNoDigits)
{
    // Not OpenRocket's: Java's BigDecimal reads them (the Arabic-Indic 1 as the BigDecimal 1,
    // "1." and the Arabic-Indic 5 as the Double 1.5); BigDecimal::parse() reads ASCII digits.
    EXPECT_EQ(valueOf("number", "\xD9\xA1"), "null");
    EXPECT_EQ(valueOf("number", "\xD9\xA5\xD9\xA3"), "null");
    EXPECT_EQ(valueOf("number", "1.\xD9\xA5"), "null");
}

TEST(EntryHelper, ABooleanIsTrueForTheWordTrueOnly)
{
    for (const std::string_view text : {"true", "TRUE", "True", "tRuE"})
    {
        EXPECT_EQ(valueOf("boolean", text), "Boolean true") << text;
    }
    // Not trimmed.
    for (const std::string_view text : {" true", "true ", "false", "yes", "1", "", "\ttrue"})
    {
        EXPECT_EQ(valueOf("boolean", text), "Boolean false") << text;
    }
}

TEST(EntryHelper, AStringIsTheContentAsItIs)
{
    for (const std::string_view text : {"", " ", "  text  ", "a\nb", "true", "5"})
    {
        EXPECT_EQ(valueOf("string", text), "String " + std::string(text)) << text;
    }
}

TEST(EntryHelper, AnotherTypeOrNoneHasNoValue)
{
    for (const std::string_view type :
         {"integer", "double", "Number", "BOOLEAN", "String", "", " number"})
    {
        EXPECT_EQ(valueOf(type, "5"), "null") << type;
    }
    EXPECT_EQ(valueOf(std::nullopt, "5"), "null");
    // A list entry of a handler that has opened none.
    EXPECT_EQ(valueOf("list", "5"), "null");
}

TEST(EntryHelper, OtherAttributesAreNotLookedAt)
{
    const ConfigHandler                handler;
    const ElementHandler::Attributes   attributes{{"key", "k"}, {"type", "number"}, {"extra", "1"}};
    const std::optional<Config::Value> value =
        EntryHelper::getValueFromEntry(handler, attributes, "7");
    EXPECT_EQ(value.has_value() ? describe(*value) : "null", "Integer 7");
}

}  // namespace
