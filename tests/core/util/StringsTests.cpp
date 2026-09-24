#include "QtRocket/util/Strings.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <limits>
#include <numbers>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/util/Chars.h"

namespace
{

namespace Strings = QtRocket::Strings;
namespace Chars   = QtRocket::Chars;

constexpr double kPi      = std::numbers::pi;
constexpr double kEpsilon = 1e-8;  // MathUtil.EPSILON
constexpr double kNaN     = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf     = std::numeric_limits<double>::infinity();

std::vector<std::byte> bytesOf(std::initializer_list<int> values)
{
    std::vector<std::byte> out;
    for (const int v : values)
    {
        out.push_back(static_cast<std::byte>(v));
    }
    return out;
}

std::vector<std::byte> bytesOf(std::string_view text)
{
    std::vector<std::byte> out;
    for (const char c : text)
    {
        out.push_back(static_cast<std::byte>(c));
    }
    return out;
}

double parsed(std::string_view text)
{
    return Strings::parseDouble(text).value_or(kNaN);
}

/// A type std::format cannot print.
struct Opaque
{ };

/// True when join() accepts a range of this type.
template <typename Range>
concept Joinable = requires(const Range& values) { Strings::join(",", values); };

/// True when trim() accepts an argument of this type (a reference type for an lvalue).
template <typename T>
concept Trimmable = requires(T&& text) { Strings::trim(std::forward<T>(text)); };

std::vector<std::byte> randomBytes(std::mt19937& rng, int count)
{
    std::uniform_int_distribution<int> value(0, 255);
    std::vector<std::byte>             bytes;
    bytes.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; i++)
    {
        bytes.push_back(static_cast<std::byte>(value(rng)));
    }
    return bytes;
}

/// The reference spelling of hexString(): one std::format per byte.
std::string formattedHex(std::span<const std::byte> bytes)
{
    std::string out;
    for (const std::byte b : bytes)
    {
        out += std::format("{:02x}", std::to_integer<int>(b));
    }
    return out;
}

/// The code point of @p text when it is exactly one well-formed UTF-8 sequence of two bytes
/// (U+0080-U+07FF) or three (U+0800-U+FFFF), otherwise nullopt.
std::optional<std::uint32_t> utf8CodePoint(std::string_view text)
{
    if (text.empty())
    {
        return std::nullopt;
    }
    const auto        lead   = static_cast<unsigned char>(text.front());
    const std::size_t length = lead >= 0xE0 ? 3 : 2;
    if (lead < 0xC2 || lead > 0xEF || text.size() != length)
    {
        return std::nullopt;
    }
    std::uint32_t codePoint = length == 2 ? (lead & 0x1FU) : (lead & 0x0FU);
    for (const char c : text.substr(1))
    {
        const auto byte = static_cast<unsigned char>(c);
        if (byte < 0x80 || byte > 0xBF)
        {
            return std::nullopt;
        }
        codePoint = (codePoint << 6U) | (byte & 0x3FU);
    }
    return codePoint;
}

// ---------------------------------------------------------------- TextUtilTest

TEST(Strings, AsciiBytes)
{
    EXPECT_EQ(Strings::asciiBytes("PK"), bytesOf("PK"));
    EXPECT_EQ(Strings::asciiBytes("<openrocket"), bytesOf("<openrocket"));
    EXPECT_EQ(Strings::asciiBytes("<RockSimDoc"), bytesOf("<RockSimDoc"));
    EXPECT_TRUE(Strings::asciiBytes("").empty());
    // Java's US-ASCII encoder replaces each unmappable code point with one '?'.
    EXPECT_EQ(Strings::asciiBytes(std::string("a") + std::string(Chars::kDegree) + "b"),
              bytesOf("a?b"));
    EXPECT_EQ(Strings::asciiBytes(std::string(Chars::kFraction)), bytesOf("?"));
}

TEST(Strings, HexString)
{
    EXPECT_EQ(Strings::hexString({}), "");
    EXPECT_EQ(Strings::hexString(bytesOf({0x00})), "00");
    EXPECT_EQ(Strings::hexString(bytesOf({0xff})), "ff");
    EXPECT_EQ(Strings::hexString(bytesOf({0x0f, 0x1e, 0x2d, 0x3c, 0x4b, 0x5a, 0x69, 0x78})),
              "0f1e2d3c4b5a6978");
}

TEST(Strings, HexStringOfEveryByte)
{
    for (int i = 0; i <= 0xff; i++)
    {
        EXPECT_EQ(std::format("{:02x}", i), Strings::hexString(bytesOf({i})));
    }
}

TEST(Strings, HexStringOfRandomBytes)
{
    // NOLINTNEXTLINE(bugprone-random-generator-seed) a fixed seed keeps the test deterministic
    std::mt19937                       rng(20240923);
    std::uniform_int_distribution<int> length(0, 99);
    for (int count = 0; count < 10; count++)
    {
        const std::vector<std::byte> bytes = randomBytes(rng, length(rng));
        EXPECT_EQ(formattedHex(bytes), Strings::hexString(bytes));
    }
}

TEST(Strings, DoubleToStringSpecialCases)
{
    EXPECT_EQ(Strings::doubleToString(kNaN), "NaN");
    EXPECT_EQ(Strings::doubleToString(kInf), "Inf");
    EXPECT_EQ(Strings::doubleToString(-kInf), "-Inf");
    EXPECT_EQ(Strings::doubleToString(0.0), "0");
    EXPECT_EQ(Strings::doubleToString(-0.0), "0");
    EXPECT_EQ(Strings::doubleToString(kEpsilon / 3), "0");
    EXPECT_EQ(Strings::doubleToString(-kEpsilon / 3), "0");
    // Just outside MathUtil.equals(d, 0): printed, not zero.
    EXPECT_EQ(Strings::doubleToString(kEpsilon), "1e-8");
    EXPECT_EQ(Strings::doubleToString(-kEpsilon), "-1e-8");
}

TEST(Strings, DoubleToStringLong)
{
    EXPECT_EQ(Strings::doubleToString(kPi * 1.0e-5), "3.142e-5");
    EXPECT_EQ(Strings::doubleToString(kPi * 1.0e-4), "3.142e-4");
    EXPECT_EQ(Strings::doubleToString(kPi * 1.0e-3), "0.003");
    EXPECT_EQ(Strings::doubleToString(kPi * 1.0e-2), "0.031");
    EXPECT_EQ(Strings::doubleToString(kPi * 1.0e-1), "0.314");
    EXPECT_EQ(Strings::doubleToString(kPi), "3.142");
    EXPECT_EQ(Strings::doubleToString(kPi * 1.0e1), "31.416");
    EXPECT_EQ(Strings::doubleToString(kPi * 1.0e2), "314.159");
    EXPECT_EQ(Strings::doubleToString(kPi * 1.0e3), "3141.593");
    EXPECT_EQ(Strings::doubleToString(kPi * 1.0e4), "3.142e4");
    EXPECT_EQ(Strings::doubleToString(kPi * 1.0e5), "3.142e5");
    EXPECT_EQ(Strings::doubleToString(kPi * 1.0e6), "3.142e6");
    EXPECT_EQ(Strings::doubleToString(kPi * 1.0e7), "3.142e7");
    EXPECT_EQ(Strings::doubleToString(kPi * 1.0e8), "3.142e8");
    EXPECT_EQ(Strings::doubleToString(kPi * 1.0e9), "3.142e9");
    EXPECT_EQ(Strings::doubleToString(kPi * 1.0e10), "3.142e10");

    EXPECT_EQ(Strings::doubleToString(-kPi * 1.0e-5), "-3.142e-5");
    EXPECT_EQ(Strings::doubleToString(-kPi * 1.0e-4), "-3.142e-4");
    EXPECT_EQ(Strings::doubleToString(-kPi * 1.0e-3), "-0.003");
    EXPECT_EQ(Strings::doubleToString(-kPi * 1.0e-2), "-0.031");
    EXPECT_EQ(Strings::doubleToString(-kPi * 1.0e-1), "-0.314");
    EXPECT_EQ(Strings::doubleToString(-kPi), "-3.142");
    EXPECT_EQ(Strings::doubleToString(-kPi * 1.0e1), "-31.416");
    EXPECT_EQ(Strings::doubleToString(-kPi * 1.0e2), "-314.159");
    EXPECT_EQ(Strings::doubleToString(-kPi * 1.0e3), "-3141.593");
    EXPECT_EQ(Strings::doubleToString(-kPi * 1.0e4), "-3.142e4");
    EXPECT_EQ(Strings::doubleToString(-kPi * 1.0e5), "-3.142e5");
    EXPECT_EQ(Strings::doubleToString(-kPi * 1.0e6), "-3.142e6");
    EXPECT_EQ(Strings::doubleToString(-kPi * 1.0e7), "-3.142e7");
    EXPECT_EQ(Strings::doubleToString(-kPi * 1.0e8), "-3.142e8");
    EXPECT_EQ(Strings::doubleToString(-kPi * 1.0e9), "-3.142e9");
    EXPECT_EQ(Strings::doubleToString(-kPi * 1.0e10), "-3.142e10");
}

TEST(Strings, DoubleToStringShort)
{
    double p = 3.1;
    EXPECT_EQ(Strings::doubleToString(p * 1.0e-5), "3.1e-5");
    EXPECT_EQ(Strings::doubleToString(p * 1.0e-4), "3.1e-4");
    EXPECT_EQ(Strings::doubleToString(p * 1.0e-3), "0.003");
    EXPECT_EQ(Strings::doubleToString(p * 1.0e-2), "0.031");
    EXPECT_EQ(Strings::doubleToString(p * 1.0e-1), "0.31");
    EXPECT_EQ(Strings::doubleToString(p), "3.1");
    EXPECT_EQ(Strings::doubleToString(p * 1.0e1), "31");
    EXPECT_EQ(Strings::doubleToString(p * 1.0e2), "310");
    EXPECT_EQ(Strings::doubleToString(p * 1.0e3), "3100");
    EXPECT_EQ(Strings::doubleToString(p * 1.0e4), "3.1e4");
    EXPECT_EQ(Strings::doubleToString(p * 1.0e5), "3.1e5");
    EXPECT_EQ(Strings::doubleToString(p * 1.0e6), "3.1e6");
    EXPECT_EQ(Strings::doubleToString(p * 1.0e7), "3.1e7");
    EXPECT_EQ(Strings::doubleToString(p * 1.0e8), "3.1e8");
    EXPECT_EQ(Strings::doubleToString(p * 1.0e9), "3.1e9");
    EXPECT_EQ(Strings::doubleToString(p * 1.0e10), "3.1e10");

    EXPECT_EQ(Strings::doubleToString(-p * 1.0e-5), "-3.1e-5");
    EXPECT_EQ(Strings::doubleToString(-p * 1.0e-4), "-3.1e-4");
    EXPECT_EQ(Strings::doubleToString(-p * 1.0e-3), "-0.003");
    EXPECT_EQ(Strings::doubleToString(-p * 1.0e-2), "-0.031");
    EXPECT_EQ(Strings::doubleToString(-p * 1.0e-1), "-0.31");
    EXPECT_EQ(Strings::doubleToString(-p), "-3.1");
    EXPECT_EQ(Strings::doubleToString(-p * 1.0e1), "-31");
    EXPECT_EQ(Strings::doubleToString(-p * 1.0e2), "-310");
    EXPECT_EQ(Strings::doubleToString(-p * 1.0e3), "-3100");
    EXPECT_EQ(Strings::doubleToString(-p * 1.0e4), "-3.1e4");
    EXPECT_EQ(Strings::doubleToString(-p * 1.0e5), "-3.1e5");
    EXPECT_EQ(Strings::doubleToString(-p * 1.0e6), "-3.1e6");
    EXPECT_EQ(Strings::doubleToString(-p * 1.0e7), "-3.1e7");
    EXPECT_EQ(Strings::doubleToString(-p * 1.0e8), "-3.1e8");
    EXPECT_EQ(Strings::doubleToString(-p * 1.0e9), "-3.1e9");
    EXPECT_EQ(Strings::doubleToString(-p * 1.0e10), "-3.1e10");

    p = 3;
    EXPECT_EQ(Strings::doubleToString(p * 1.0e-5), "3e-5");
    EXPECT_EQ(Strings::doubleToString(p * 1.0e-4), "3e-4");
    EXPECT_EQ(Strings::doubleToString(p * 1.0e-3), "0.003");
    EXPECT_EQ(Strings::doubleToString(p * 1.0e-2), "0.03");
    EXPECT_EQ(Strings::doubleToString(p * 1.0e-1), "0.3");
    EXPECT_EQ(Strings::doubleToString(p), "3");
    EXPECT_EQ(Strings::doubleToString(p * 1.0e1), "30");
    EXPECT_EQ(Strings::doubleToString(p * 1.0e2), "300");
    EXPECT_EQ(Strings::doubleToString(p * 1.0e3), "3000");
    EXPECT_EQ(Strings::doubleToString(p * 1.0e4), "3e4");
    EXPECT_EQ(Strings::doubleToString(p * 1.0e5), "3e5");
    EXPECT_EQ(Strings::doubleToString(p * 1.0e6), "3e6");
    EXPECT_EQ(Strings::doubleToString(p * 1.0e7), "3e7");
    EXPECT_EQ(Strings::doubleToString(p * 1.0e8), "3e8");
    EXPECT_EQ(Strings::doubleToString(p * 1.0e9), "3e9");
    EXPECT_EQ(Strings::doubleToString(p * 1.0e10), "3e10");

    EXPECT_EQ(Strings::doubleToString(-p * 1.0e-5), "-3e-5");
    EXPECT_EQ(Strings::doubleToString(-p * 1.0e-4), "-3e-4");
    EXPECT_EQ(Strings::doubleToString(-p * 1.0e-3), "-0.003");
    EXPECT_EQ(Strings::doubleToString(-p * 1.0e-2), "-0.03");
    EXPECT_EQ(Strings::doubleToString(-p * 1.0e-1), "-0.3");
    EXPECT_EQ(Strings::doubleToString(-p), "-3");
    EXPECT_EQ(Strings::doubleToString(-p * 1.0e1), "-30");
    EXPECT_EQ(Strings::doubleToString(-p * 1.0e2), "-300");
    EXPECT_EQ(Strings::doubleToString(-p * 1.0e3), "-3000");
    EXPECT_EQ(Strings::doubleToString(-p * 1.0e4), "-3e4");
    EXPECT_EQ(Strings::doubleToString(-p * 1.0e5), "-3e5");
    EXPECT_EQ(Strings::doubleToString(-p * 1.0e6), "-3e6");
    EXPECT_EQ(Strings::doubleToString(-p * 1.0e7), "-3e7");
    EXPECT_EQ(Strings::doubleToString(-p * 1.0e8), "-3e8");
    EXPECT_EQ(Strings::doubleToString(-p * 1.0e9), "-3e9");
    EXPECT_EQ(Strings::doubleToString(-p * 1.0e10), "-3e10");
}

TEST(Strings, DoubleToStringRounding)
{
    EXPECT_EQ(Strings::doubleToString(1.00096, 3), "1.001");
    EXPECT_EQ(Strings::doubleToString(1.0001500001e-5, 4), "1.0002e-5");
    EXPECT_EQ(Strings::doubleToString(1.0001499999e-5, 4), "1.0001e-5");
    EXPECT_EQ(Strings::doubleToString(1.0001500001e-4, 4), "1.0002e-4");
    EXPECT_EQ(Strings::doubleToString(1.0001499999e-4, 4), "1.0001e-4");

    EXPECT_EQ(Strings::doubleToString(-1.00096, 3), "-1.001");
    EXPECT_EQ(Strings::doubleToString(-1.0001500001e-5, 4), "-1.0002e-5");
    EXPECT_EQ(Strings::doubleToString(-1.0001499999e-5, 4), "-1.0001e-5");
    EXPECT_EQ(Strings::doubleToString(-1.0001500001e-4, 4), "-1.0002e-4");
    EXPECT_EQ(Strings::doubleToString(-1.0001499999e-4, 4), "-1.0001e-4");
}

TEST(Strings, DoubleToStringRoundTripsRandomValues)
{
    // NOLINTNEXTLINE(bugprone-random-generator-seed) a fixed seed keeps the test deterministic
    std::mt19937                           rng(31415);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    for (int i = 0; i < 10000; i++)
    {
        const double orig     = unit(rng);
        const double expected = std::rint(orig * 100000) / 100000.0;
        if (orig < 0.1)
        {
            continue;
        }
        const std::string s = Strings::doubleToString(orig);
        EXPECT_NEAR(expected, parsed(s), 0.001) << s;
    }
}

TEST(Strings, StorageDecimalPlaces)
{
    EXPECT_EQ(6, Strings::kStorageDecimalPlaces);

    // Values in the 0.001-0.1 m range where %.3f loses significant digits.
    EXPECT_EQ("0.0125", Strings::doubleToString(0.0125, Strings::kStorageDecimalPlaces));
    EXPECT_EQ("0.02473", Strings::doubleToString(0.02473, Strings::kStorageDecimalPlaces));
    EXPECT_EQ("0.001234", Strings::doubleToString(0.001234, Strings::kStorageDecimalPlaces));

    // The old DEFAULT_DECIMAL_PLACES (3) was insufficient for these values.
    EXPECT_NE("0.0125", Strings::doubleToString(0.0125, Strings::kDefaultDecimalPlaces));
    EXPECT_NE("0.02473", Strings::doubleToString(0.02473, Strings::kDefaultDecimalPlaces));
}

TEST(Strings, StorageDecimalPlacesRoundTrip)
{
    // Reference area for a 25 mm diameter rocket (< 0.001 m^2, exponential notation).
    const double      refArea = kPi * 0.0125 * 0.0125;
    const std::string storedRefArea =
        Strings::doubleToString(refArea, Strings::kStorageDecimalPlaces);
    EXPECT_NEAR(refArea, parsed(storedRefArea), refArea * 1e-5);

    // Round-trip: all values survive parse with relative error < 1e-5.
    const std::array values = {0.001234,   0.0125,     0.02473,      0.12345678,
                               1.23456789, 12345.6789, 1.23456789e-5};
    for (const double v : values)
    {
        const std::string s = Strings::doubleToString(v, Strings::kStorageDecimalPlaces);
        EXPECT_NEAR(v, parsed(s), std::abs(v) * 1e-5) << "Round-trip failed for " << v;
    }
}

TEST(Strings, EscapeXml)
{
    EXPECT_EQ(Strings::escapeXml(""), "");
    EXPECT_EQ(Strings::escapeXml("foo&bar"), "foo&amp;bar");
    EXPECT_EQ(Strings::escapeXml("<html>&"), "&lt;html&gt;&amp;");
    EXPECT_EQ(Strings::escapeXml("\"'"), "&quot;&#39;");
    EXPECT_EQ(Strings::escapeXml("foo\n\r\tbar"), "foo\n\r\tbar");
    const std::string controls = std::string("foo") + '\0' + '\x01' + '\x1f' + '\x7f' + "bar";
    EXPECT_EQ(Strings::escapeXml(controls), "foo&#0;&#1;&#31;&#127;bar");
    // Non-ASCII text passes through untouched.
    const std::string degrees = std::string("90") + std::string(Chars::kDegree);
    EXPECT_EQ(Strings::escapeXml(degrees), degrees);
}

// ------------------------------------------------------------- StringUtilTest

TEST(Strings, IsEmpty)
{
    EXPECT_TRUE(Strings::isEmpty(""));
    EXPECT_TRUE(Strings::isEmpty(std::string()));
    EXPECT_TRUE(Strings::isEmpty(" "));
    EXPECT_TRUE(Strings::isEmpty("  "));
    EXPECT_TRUE(Strings::isEmpty("       "));
    EXPECT_TRUE(Strings::isEmpty("\t\n\r "));

    EXPECT_FALSE(Strings::isEmpty("A"));
    EXPECT_FALSE(Strings::isEmpty("         .        "));
}

TEST(Strings, ConvertToDouble)
{
    EXPECT_NEAR(0.2, Strings::convertToDouble(".2").value_or(kNaN), kEpsilon);
    EXPECT_NEAR(0.2, Strings::convertToDouble(",2").value_or(kNaN), kEpsilon);
    EXPECT_NEAR(1, Strings::convertToDouble("1,").value_or(kNaN), kEpsilon);
    EXPECT_NEAR(2, Strings::convertToDouble("2.").value_or(kNaN), kEpsilon);
    EXPECT_NEAR(1, Strings::convertToDouble("1").value_or(kNaN), kEpsilon);
    EXPECT_NEAR(1.52, Strings::convertToDouble("1.52").value_or(kNaN), kEpsilon);
    EXPECT_NEAR(1.52, Strings::convertToDouble("1,52").value_or(kNaN), kEpsilon);
    EXPECT_NEAR(1.5, Strings::convertToDouble("1.500").value_or(kNaN), kEpsilon);
    EXPECT_NEAR(1.5, Strings::convertToDouble("1,500").value_or(kNaN), kEpsilon);
    EXPECT_NEAR(1500.61, Strings::convertToDouble("1.500,61").value_or(kNaN), kEpsilon);
    EXPECT_NEAR(1500.61, Strings::convertToDouble("1,500.61").value_or(kNaN), kEpsilon);
    EXPECT_NEAR(1500.2, Strings::convertToDouble("1,500,200").value_or(kNaN), kEpsilon);
    EXPECT_NEAR(1500.2, Strings::convertToDouble("1.500.200").value_or(kNaN), kEpsilon);
    EXPECT_NEAR(1500200.23, Strings::convertToDouble("1500200.23").value_or(kNaN), kEpsilon);
    EXPECT_NEAR(1500200.23, Strings::convertToDouble("1500200,23").value_or(kNaN), kEpsilon);
    EXPECT_NEAR(1500200.23, Strings::convertToDouble("1,500,200.23").value_or(kNaN), kEpsilon);
    EXPECT_NEAR(1500200.23, Strings::convertToDouble("1.500.200,23").value_or(kNaN), kEpsilon);

    // Java throws NumberFormatException; here the result is empty.
    EXPECT_EQ(Strings::convertToDouble(""), std::nullopt);
    EXPECT_EQ(Strings::convertToDouble("abc"), std::nullopt);
    EXPECT_EQ(Strings::convertToDouble("1.5x"), std::nullopt);
    // Double.parseDouble: digits beyond the double range are an infinity, "Inf" is no number.
    EXPECT_EQ(Strings::convertToDouble("1" + std::string(309, '0')), kInf);
    EXPECT_EQ(Strings::convertToDouble("Inf"), std::nullopt);
    EXPECT_EQ(Strings::convertToDouble("Infinity"), kInf);
}

TEST(Strings, JoinValues)
{
    EXPECT_EQ("", Strings::join(",", std::vector<std::string>{}));
    EXPECT_EQ("a,b,c", Strings::join(",", std::vector<std::string>{"a", "b", "c"}));
    EXPECT_EQ("1-2-3", Strings::join("-", std::vector<int>{1, 2, 3}));
    EXPECT_EQ("x", Strings::join(", ", std::array<std::string_view, 1>{"x"}));
    EXPECT_EQ("one;two;three", Strings::join(";", std::vector<std::string>{"one", "two", "three"}));
    // Deviation from OpenRocket, which would give "b" here.
    EXPECT_EQ(",b", Strings::join(",", std::vector<std::string>{"", "b"}));
    // StringUtilTest's "1-true-null": the boolean element formats as Java prints it (a range
    // holds one type, so the int and the null of that case are not applicable).
    EXPECT_EQ("true-false", Strings::join("-", std::vector<bool>{true, false}));
    EXPECT_EQ("true-false", Strings::join("-", std::array<bool, 2>{true, false}));
    EXPECT_EQ("1.5-2", Strings::join("-", std::array<double, 2>{1.5, 2.0}));
}

TEST(Strings, JoinRequiresFormattableElements)
{
    static_assert(!Joinable<std::vector<Opaque>>);
    static_assert(Joinable<std::vector<bool>>);
    static_assert(Joinable<std::vector<std::string_view>>);
    static_assert(Joinable<std::array<double, 2>>);
}

TEST(Strings, EscapeCsvHandlesSpecialCharacters)
{
    EXPECT_EQ("", Strings::escapeCsv(""));
    EXPECT_EQ("plain", Strings::escapeCsv("plain"));
    EXPECT_EQ("\"quoted,comma\"", Strings::escapeCsv("quoted,comma"));
    EXPECT_EQ("\"contains\"\"quote\"\"\"", Strings::escapeCsv("contains\"quote\""));
    EXPECT_EQ("\"line\nbreak\"", Strings::escapeCsv("line\nbreak"));
    EXPECT_EQ("\"carriage\rreturn\"", Strings::escapeCsv("carriage\rreturn"));
}

TEST(Strings, RemoveHtmlTags)
{
    EXPECT_EQ("", Strings::removeHtmlTags(""));
    EXPECT_EQ("plain text", Strings::removeHtmlTags("plain text"));
    EXPECT_EQ("Hello world", Strings::removeHtmlTags("<p>Hello <b>world</b></p>"));
    EXPECT_EQ("nested", Strings::removeHtmlTags("<div><span>nested</span></div>"));
    // The regex "<[^>]*>" matches across anything but '>', and an unclosed '<' stays.
    EXPECT_EQ("ab", Strings::removeHtmlTags("a<x<y>b"));
    EXPECT_EQ("a<b", Strings::removeHtmlTags("a<b"));
    EXPECT_EQ("a>b", Strings::removeHtmlTags("a>b"));
}

TEST(Strings, EscapeHtml)
{
    EXPECT_EQ("", Strings::escapeHtml(""));
    EXPECT_EQ("plain", Strings::escapeHtml("plain"));
    EXPECT_EQ("&lt;b&gt;&amp;&quot;&#39;", Strings::escapeHtml("<b>&\"'"));
    EXPECT_EQ("one<br>two", Strings::escapeHtml("one\ntwo"));
}

// ---------------------------------------------------- behaviour OpenRocket did not test

TEST(Strings, DoubleToStringRoundsHalfUpOnDecimalDigits)
{
    // Java's Formatter rounds the shortest decimal digits half-up; printf would give "1.000",
    // "0.12" and "2" for these.
    EXPECT_EQ(Strings::doubleToString(1.0005, 3), "1.001");
    EXPECT_EQ(Strings::doubleToString(0.125, 2), "0.13");
    EXPECT_EQ(Strings::doubleToString(2.5, 0), "3");
    EXPECT_EQ(Strings::doubleToString(0.0125, 3), "0.013");
    EXPECT_EQ(Strings::doubleToString(-1.0005, 3), "-1.001");
    EXPECT_EQ(Strings::doubleToString(1.00049, 3), "1");
}

TEST(Strings, DoubleToStringCarriesIntoNewDigit)
{
    EXPECT_EQ(Strings::doubleToString(9999.9996), "10000");
    EXPECT_EQ(Strings::doubleToString(0.9999999), "1");
    EXPECT_EQ(Strings::doubleToString(0.00099999), "1e-3");
    EXPECT_EQ(Strings::doubleToString(99999.99), "1e5");
    EXPECT_EQ(Strings::doubleToString(-99999.99), "-1e5");
    EXPECT_EQ(Strings::doubleToString(0.0095, 2), "0.01");
    EXPECT_EQ(Strings::doubleToString(0.0094, 2), "0.01");
    EXPECT_EQ(Strings::doubleToString(0.0049, 2), "0");
}

TEST(Strings, DoubleToStringWithoutExponentialNotation)
{
    EXPECT_EQ(Strings::doubleToString(31415.9265, 3, false), "31415.927");
    EXPECT_EQ(Strings::doubleToString(12345678.9, 3, false), "12345678.9");
    EXPECT_EQ(Strings::doubleToString(1e20, 3, false), "100000000000000000000");
    EXPECT_EQ(Strings::doubleToString(3.1e-5, 3, false), "0");
    EXPECT_EQ(Strings::doubleToString(0.0004, 3, false), "0");
    // Java keeps the sign of a negative value whose digits all round away ("-0.000" trimmed).
    EXPECT_EQ(Strings::doubleToString(-0.0004, 3, false), "-0");
    EXPECT_EQ(Strings::doubleToString(0.0005, 3, false), "0.001");
    EXPECT_EQ(Strings::doubleToString(kPi * 1.0e-5, 3, true), "3.142e-5");
}

TEST(Strings, DoubleToStringDecimalPlaces)
{
    EXPECT_EQ(Strings::doubleToString(31415.9, 0), "3e4");
    EXPECT_EQ(Strings::doubleToString(3.7, 0), "4");
    EXPECT_EQ(Strings::doubleToString(0.4, 0), "0");
    EXPECT_EQ(Strings::doubleToString(3.7, -2), "4");  // negative counts as 0
    EXPECT_EQ(Strings::doubleToString(kPi, 10), "3.1415926536");
    EXPECT_EQ(Strings::doubleToString(kPi, 20), "3.141592653589793");
    EXPECT_EQ(Strings::doubleToString(kPi * 1e-5, 20), "3.1415926535897935e-5");
    EXPECT_EQ(Strings::doubleToString(1.5, 1), "1.5");
    EXPECT_EQ(Strings::doubleToString(1.25, 1), "1.3");
}

TEST(Strings, DoubleToStringExtremes)
{
    EXPECT_EQ(Strings::doubleToString(1e300), "1e300");
    EXPECT_EQ(Strings::doubleToString(-1e-300), "0");  // within MathUtil.EPSILON / 2 of zero
    EXPECT_EQ(Strings::doubleToString(std::numeric_limits<double>::max()), "1.798e308");
    EXPECT_EQ(Strings::doubleToString(1e-8), "1e-8");
    EXPECT_EQ(Strings::doubleToString(123456789012.0), "1.235e11");
    EXPECT_EQ(Strings::doubleToString(0.001), "0.001");
    EXPECT_EQ(Strings::doubleToString(0.000999), "9.99e-4");
    EXPECT_EQ(Strings::doubleToString(10000.0), "1e4");
    EXPECT_EQ(Strings::doubleToString(9999.0), "9999");
}

TEST(Strings, DoubleToStringPrintsLargeIntegersExactly)
{
    // Java's Formatter prints an integer below 2^63 from its exact digits, not the shortest
    // round-trip digits (FloatingDecimal.developLongDigits); expectations replayed on JDK 17.
    EXPECT_EQ(Strings::doubleToString(std::ldexp(1.0, 52) + 1, 3, false), "4503599627370497");
    EXPECT_EQ(Strings::doubleToString(std::ldexp(1.0, 53), 3, false), "9007199254740992");
    EXPECT_EQ(Strings::doubleToString(std::ldexp(1.0, 54), 3, false), "18014398509481984");
    EXPECT_EQ(Strings::doubleToString(std::ldexp(1.0, 54) + 8, 3, false), "18014398509481992");
    EXPECT_EQ(Strings::doubleToString(std::ldexp(1.0, 55), 3, false), "36028797018963968");
    EXPECT_EQ(Strings::doubleToString(std::ldexp(1.0, 56), 3, false), "72057594037927936");
    EXPECT_EQ(Strings::doubleToString(std::ldexp(1.0, 57), 3, false), "144115188075855872");
    // From 2^58 the digits below the double's precision are rounded away (one digit up to 2^61,
    // two from there): 288230376151711744 -> ...740, 2305843009213693952 -> ...4000.
    EXPECT_EQ(Strings::doubleToString(std::ldexp(1.0, 58), 3, false), "288230376151711740");
    EXPECT_EQ(Strings::doubleToString(std::ldexp(1.0, 59), 3, false), "576460752303423490");
    EXPECT_EQ(Strings::doubleToString(std::ldexp(1.0, 60), 3, false), "1152921504606846980");
    EXPECT_EQ(Strings::doubleToString(std::ldexp(1.0, 61), 3, false), "2305843009213694000");
    EXPECT_EQ(Strings::doubleToString(std::ldexp(1.0, 62), 3, false), "4611686018427387900");
    EXPECT_EQ(Strings::doubleToString(1e18, 3, false), "1000000000000000000");
    EXPECT_EQ(Strings::doubleToString(-std::ldexp(1.0, 60), 3, false), "-1152921504606846980");
    // The same digits feed the exponential notation.
    EXPECT_EQ(Strings::doubleToString(std::ldexp(1.0, 55), 16), "3.6028797018963968e16");
    EXPECT_EQ(Strings::doubleToString(std::ldexp(1.0, 55), 20), "3.6028797018963968e16");
    EXPECT_EQ(Strings::doubleToString(std::ldexp(1.0, 60), 17), "1.15292150460684698e18");
    EXPECT_EQ(Strings::doubleToString(std::ldexp(1.0, 60)), "1.153e18");
    EXPECT_EQ(Strings::doubleToString(std::ldexp(1.0, 61), 2), "2.31e18");
    EXPECT_EQ(Strings::doubleToString(std::ldexp(1.0, 62)), "4.612e18");
}

TEST(Strings, DoubleToStringFromTwoPow63UsesShortestDigits)
{
    // From 2^63 Java's older digit generation applies; where it agrees with the shortest digits
    // (JDK 17 replay) the port matches it.
    EXPECT_EQ(Strings::doubleToString(std::ldexp(1.0, 63), 3, false), "9223372036854776000");
    EXPECT_EQ(Strings::doubleToString(std::ldexp(1.0, 63)), "9.223e18");
    EXPECT_EQ(Strings::doubleToString(1e20, 3, false), "100000000000000000000");
    EXPECT_EQ(Strings::doubleToString(1e20), "1e20");
    EXPECT_EQ(Strings::doubleToString(12345678901234567890.0, 3, false), "12345678901234567000");
    // Documented deviation: at a rounding tie JDK 17 prints "8.638e20" and "9.2550626e18" here;
    // JDK 21+ (JDK-8300869) prints these.
    EXPECT_EQ(Strings::doubleToString(8.6385e20), "8.639e20");
    EXPECT_EQ(Strings::doubleToString(9.25506265e18, 7), "9.2550627e18");
}

TEST(Strings, DoubleToStringReadsBack)
{
    // Every stored value must come back through parseDouble.
    const std::array values = {kPi * 1e-7, -kPi, 0.0125, 9999.9996, 1e300, -3.1e10};
    for (const double v : values)
    {
        const std::string s = Strings::doubleToString(v, Strings::kStorageDecimalPlaces);
        EXPECT_NEAR(v, parsed(s), std::abs(v) * 1e-5) << s;
    }
    EXPECT_TRUE(std::isnan(parsed(Strings::doubleToString(kNaN))));
    EXPECT_EQ(parsed(Strings::doubleToString(kInf)), kInf);
    EXPECT_EQ(parsed(Strings::doubleToString(-kInf)), -kInf);
}

TEST(Strings, ParseDoubleJavaFormats)
{
    EXPECT_EQ(Strings::parseDouble("1"), 1.0);
    EXPECT_EQ(Strings::parseDouble("1.5"), 1.5);
    EXPECT_EQ(Strings::parseDouble("-1.5"), -1.5);
    EXPECT_EQ(Strings::parseDouble("+1.5"), 1.5);
    EXPECT_EQ(Strings::parseDouble(".5"), 0.5);
    EXPECT_EQ(Strings::parseDouble("5."), 5.0);
    EXPECT_EQ(Strings::parseDouble("-.5"), -0.5);
    EXPECT_EQ(Strings::parseDouble("0"), 0.0);
    EXPECT_EQ(Strings::parseDouble("1e-5"), 1e-5);
    EXPECT_EQ(Strings::parseDouble("1E-5"), 1e-5);
    EXPECT_EQ(Strings::parseDouble("3.142e4"), 3.142e4);
    EXPECT_EQ(Strings::parseDouble("-2.5E+3"), -2500.0);
    EXPECT_EQ(Strings::parseDouble("3.1415926535897935e-5"), 3.1415926535897935e-5);
    EXPECT_EQ(Strings::parseDouble("0.1"), 0.1);  // correctly rounded, as Java
    EXPECT_EQ(Strings::parseDouble("4.9e-324"), 4.9e-324);
    // Whitespace at either end is trimmed, as Double.parseDouble does.
    EXPECT_EQ(Strings::parseDouble("  2.5  "), 2.5);
    EXPECT_EQ(Strings::parseDouble("\t2.5\n"), 2.5);
}

TEST(Strings, ParseDoubleSpecialValues)
{
    EXPECT_TRUE(std::isnan(parsed("NaN")));
    EXPECT_TRUE(std::isnan(parsed("-NaN")));
    EXPECT_TRUE(std::isnan(parsed(" NaN ")));
    EXPECT_EQ(Strings::parseDouble("Infinity"), kInf);
    EXPECT_EQ(Strings::parseDouble("-Infinity"), -kInf);
    EXPECT_EQ(Strings::parseDouble("+Infinity"), kInf);
    EXPECT_EQ(Strings::parseDouble("Inf"), kInf);
    EXPECT_EQ(Strings::parseDouble("-Inf"), -kInf);
    EXPECT_EQ(Strings::parseDouble(" -Inf "), -kInf);
}

TEST(Strings, ParseDoubleRejectsJunk)
{
    EXPECT_EQ(Strings::parseDouble(""), std::nullopt);
    EXPECT_EQ(Strings::parseDouble("   "), std::nullopt);
    EXPECT_EQ(Strings::parseDouble("abc"), std::nullopt);
    EXPECT_EQ(Strings::parseDouble("-"), std::nullopt);
    EXPECT_EQ(Strings::parseDouble("+"), std::nullopt);
    EXPECT_EQ(Strings::parseDouble("."), std::nullopt);
    EXPECT_EQ(Strings::parseDouble("- 1"), std::nullopt);
    EXPECT_EQ(Strings::parseDouble("--1"), std::nullopt);
    EXPECT_EQ(Strings::parseDouble("1,5"), std::nullopt);
    EXPECT_EQ(Strings::parseDouble("1 5"), std::nullopt);
    // Partial matches fail rather than parse a prefix.
    EXPECT_EQ(Strings::parseDouble("1.5x"), std::nullopt);
    EXPECT_EQ(Strings::parseDouble("1.5 m"), std::nullopt);
    EXPECT_EQ(Strings::parseDouble("1e"), std::nullopt);
    EXPECT_EQ(Strings::parseDouble("1e+"), std::nullopt);
    EXPECT_EQ(Strings::parseDouble("1.2.3"), std::nullopt);
    EXPECT_EQ(Strings::parseDouble("0x10"), std::nullopt);
    EXPECT_EQ(Strings::parseDouble("1.0d"), std::nullopt);
    // Only Java's exact spellings of the special values.
    EXPECT_EQ(Strings::parseDouble("nan"), std::nullopt);
    EXPECT_EQ(Strings::parseDouble("NAN"), std::nullopt);
    EXPECT_EQ(Strings::parseDouble("inf"), std::nullopt);
    EXPECT_EQ(Strings::parseDouble("INFINITY"), std::nullopt);
    EXPECT_EQ(Strings::parseDouble("infinity"), std::nullopt);
    EXPECT_EQ(Strings::parseDouble("Infinite"), std::nullopt);
    EXPECT_EQ(Strings::parseDouble("nan(1)"), std::nullopt);
    // Above the double range: OpenRocket writes "Inf" for infinities, never such literals.
    EXPECT_EQ(Strings::parseDouble("1e999"), std::nullopt);
    EXPECT_EQ(Strings::parseDouble("-1e999"), std::nullopt);
    EXPECT_EQ(Strings::parseDouble("1e-400x"), std::nullopt);
}

TEST(Strings, ParseDoubleUnderflowsToSignedZero)
{
    // Double.parseDouble gives 0.0 / -0.0 for a literal below the smallest subnormal.
    const double positive = parsed("1e-400");
    EXPECT_EQ(positive, 0.0);
    EXPECT_FALSE(std::signbit(positive));
    const double negative = parsed("-1e-400");
    EXPECT_EQ(negative, 0.0);
    EXPECT_TRUE(std::signbit(negative));
    EXPECT_EQ(Strings::parseDouble("2.4e-324"), 0.0);  // below half the smallest subnormal
    EXPECT_EQ(Strings::parseDouble("2.5e-324"), 4.9e-324);
    EXPECT_EQ(Strings::parseDouble("0e-400"), 0.0);
}

// Every javaParseDouble result below was checked against Double.parseDouble on JDK 17 (a sweep of
// 170,000 random decimal and hexadecimal strings, exact ties included, found no difference).
TEST(Strings, JavaParseDoubleAcceptsJavasGrammar)
{
    EXPECT_EQ(Strings::javaParseDouble("1.5"), 1.5);
    EXPECT_EQ(Strings::javaParseDouble(".5"), 0.5);
    EXPECT_EQ(Strings::javaParseDouble("5."), 5.0);
    EXPECT_EQ(Strings::javaParseDouble("5.e3"), 5000.0);
    EXPECT_EQ(Strings::javaParseDouble("-2.5E3"), -2500.0);
    EXPECT_EQ(Strings::javaParseDouble("1e+05"), 1e5);
    EXPECT_EQ(Strings::javaParseDouble(" \t1.5\n"), 1.5);
    EXPECT_EQ(Strings::javaParseDouble("1.5d"), 1.5);  // parsed as a double all the same
    EXPECT_EQ(Strings::javaParseDouble("1.1f"), 1.1);
}

TEST(Strings, JavaParseDoubleSpecialValues)
{
    EXPECT_EQ(Strings::javaParseDouble("+Infinity"), kInf);
    EXPECT_EQ(Strings::javaParseDouble("-Infinity"), -kInf);
    EXPECT_TRUE(std::isnan(Strings::javaParseDouble("-NaN").value_or(0.0)));
    EXPECT_TRUE(std::signbit(Strings::javaParseDouble("-0").value_or(1.0)));
}

TEST(Strings, JavaParseDoubleOutOfRangeIsAnInfinityOrAZero)
{
    EXPECT_EQ(Strings::javaParseDouble("1e999"), kInf);
    EXPECT_EQ(Strings::javaParseDouble("-1e999"), -kInf);
    EXPECT_EQ(Strings::javaParseDouble("1.7976931348623159e308"), kInf);
    EXPECT_TRUE(std::signbit(Strings::javaParseDouble("-1e-400").value_or(1.0)));
    EXPECT_EQ(Strings::javaParseDouble("-1e-400"), 0.0);
    EXPECT_EQ(Strings::javaParseDouble("2.4703282292062328e-324"), 4.9e-324);
    EXPECT_EQ(Strings::javaParseDouble("2.4703282292062327e-324"), 0.0);
}

TEST(Strings, JavaParseDoubleReadsHexadecimalFloats)
{
    EXPECT_EQ(Strings::javaParseDouble("0x1.8p1"), 3.0);
    EXPECT_EQ(Strings::javaParseDouble("0X.8P-3"), 0.0625);
    EXPECT_EQ(Strings::javaParseDouble("-0x1p0d"), -1.0);
    EXPECT_EQ(Strings::javaParseDouble("0x1.p1"), 2.0);
    EXPECT_EQ(Strings::javaParseDouble("0x1P-1074"), 4.9e-324);
    EXPECT_EQ(Strings::javaParseDouble("0x1p-1075"), 0.0);  // a tie, to even
    EXPECT_EQ(Strings::javaParseDouble("0x1.0000000000001p-1075"), 4.9e-324);
    EXPECT_EQ(Strings::javaParseDouble("0x1.fffffffffffff7ffffp1023"),
              std::numeric_limits<double>::max());
    EXPECT_EQ(Strings::javaParseDouble("0x1.fffffffffffff8p1023"), kInf);
    EXPECT_EQ(Strings::javaParseDouble("0x123456789abcdef0123456789p-60"), 7.818749353073778E10);
    // An exponent beyond the int range: its sign alone decides.
    EXPECT_EQ(Strings::javaParseDouble("0x1p99999999999"), kInf);
    EXPECT_EQ(Strings::javaParseDouble("0x1p-99999999999"), 0.0);
    EXPECT_EQ(Strings::javaParseDouble("0x0p99999999999"), 0.0);
}

TEST(Strings, JavaParseDoubleRejectsWhatJavaRejects)
{
    for (const std::string_view bad :
         {"",     " ",         ".",     "+",     "-",     "e5",       "1e",
          "1e+",  "1.5dd",     "1.5 d", "nan",   "NaNd",  "infinity", "Inf",
          "-Inf", "Infinityf", "0x1",   "0x.p1", "0xp1",  "0x1p",     "0x1.8p1x",
          "1,5",  "1.5.5",     "--1",   "1e1e1", "1_000", "0x",       "\xEF\xBC\x91"})
    {
        EXPECT_EQ(Strings::javaParseDouble(bad), std::nullopt) << bad;
    }
}

TEST(Strings, JavaEqualsIgnoreCaseFoldsAsJava)
{
    EXPECT_TRUE(Strings::javaEqualsIgnoreCase("Aluminum", "aLUMINUM"));
    EXPECT_TRUE(Strings::javaEqualsIgnoreCase("", ""));
    EXPECT_FALSE(Strings::javaEqualsIgnoreCase("a", "ab"));
    EXPECT_TRUE(Strings::javaEqualsIgnoreCase("\u00D6lpapier", "\u00F6LPAPIER"));
    EXPECT_TRUE(Strings::javaEqualsIgnoreCase("\u00B5m", "\u03BCM"));  // micro sign, Greek mu
    EXPECT_TRUE(Strings::javaEqualsIgnoreCase("\u212A", "k"));         // Kelvin sign
    EXPECT_TRUE(Strings::javaEqualsIgnoreCase("\u017F", "S"));         // long s
    EXPECT_TRUE(Strings::javaEqualsIgnoreCase("\u0130", "i"));
    EXPECT_TRUE(Strings::javaEqualsIgnoreCase("\u0131", "I"));
    EXPECT_TRUE(Strings::javaEqualsIgnoreCase("\U00010400", "\U00010428"));  // Deseret
    EXPECT_FALSE(Strings::javaEqualsIgnoreCase("\u00DF", "SS"));             // no full case folding
    EXPECT_FALSE(Strings::javaEqualsIgnoreCase("\u00E9", "e"));
}

TEST(Strings, JavaCompareToComparesUtf16CodeUnits)
{
    EXPECT_EQ(Strings::javaCompareTo("abc", "abc"), 0);
    EXPECT_EQ(Strings::javaCompareTo("abc", "abd"), -1);
    EXPECT_EQ(Strings::javaCompareTo("ab", "abcd"), -2);
    EXPECT_EQ(Strings::javaCompareTo("abcd", "ab"), 2);
    EXPECT_EQ(Strings::javaCompareTo("\u00E9", "e"), 0xE9 - 'e');
    // A surrogate pair (D835 DC0C) sorts before U+FF4D, unlike the UTF-8 bytes.
    EXPECT_EQ(Strings::javaCompareTo("\U0001D40C", "\uFF4D"), 0xD835 - 0xFF4D);
    EXPECT_EQ(Strings::javaCompareTo("\U0001D40C", "\U0001D40D"), -1);
    // A malformed byte reads as U+FFFD.
    EXPECT_EQ(Strings::javaCompareTo("\xFF", "\uFFFD"), 0);
}

TEST(Strings, JavaHashCodeHashesUtf16CodeUnits)
{
    EXPECT_EQ(Strings::javaHashCode(""), 0);
    EXPECT_EQ(Strings::javaHashCode("a|b"), 97159);
    EXPECT_EQ(Strings::javaHashCode("Aluminum"), 2133183776);
    EXPECT_EQ(Strings::javaHashCode("\U0001D40C"), 1772151);
    EXPECT_EQ(Strings::javaHashCode("\uFF4D"), 65357);
}

TEST(Strings, ToCodePointsDecodesUtf8)
{
    EXPECT_EQ(Strings::toCodePoints("a\u00E9\u2044\U0001F680"), U"a\u00E9\u2044\U0001F680");
    EXPECT_EQ(Strings::toCodePoints(""), U"");
    // Malformed input: each offending byte is U+FFFD.
    EXPECT_EQ(Strings::toCodePoints("\xC3"), U"\uFFFD");
    EXPECT_EQ(Strings::toCodePoints("\xC3"
                                    "A"),
              U"\uFFFDA");
    EXPECT_EQ(Strings::toCodePoints("\xC0\x80"), U"\uFFFD\uFFFD");            // overlong
    EXPECT_EQ(Strings::toCodePoints("\xED\xA0\x80"), U"\uFFFD\uFFFD\uFFFD");  // a surrogate
    EXPECT_EQ(Strings::toCodePoints("\xF4\x90\x80\x80"), U"\uFFFD\uFFFD\uFFFD\uFFFD");
}

TEST(Strings, ParseInt)
{
    EXPECT_EQ(Strings::parseInt("0"), 0);
    EXPECT_EQ(Strings::parseInt("42"), 42);
    EXPECT_EQ(Strings::parseInt("-42"), -42);
    EXPECT_EQ(Strings::parseInt("+42"), 42);
    EXPECT_EQ(Strings::parseInt("007"), 7);
    EXPECT_EQ(Strings::parseInt("2147483647"), 2147483647);
    EXPECT_EQ(Strings::parseInt("-2147483648"), std::numeric_limits<int>::min());

    EXPECT_EQ(Strings::parseInt(""), std::nullopt);
    EXPECT_EQ(Strings::parseInt("-"), std::nullopt);
    EXPECT_EQ(Strings::parseInt("+"), std::nullopt);
    EXPECT_EQ(Strings::parseInt("+-1"), std::nullopt);
    EXPECT_EQ(Strings::parseInt("--1"), std::nullopt);
    EXPECT_EQ(Strings::parseInt(" 1"), std::nullopt);  // Integer.parseInt does not trim
    EXPECT_EQ(Strings::parseInt("1 "), std::nullopt);
    EXPECT_EQ(Strings::parseInt("1.0"), std::nullopt);
    EXPECT_EQ(Strings::parseInt("1e3"), std::nullopt);
    EXPECT_EQ(Strings::parseInt("0x10"), std::nullopt);
    EXPECT_EQ(Strings::parseInt("abc"), std::nullopt);
    EXPECT_EQ(Strings::parseInt("2147483648"), std::nullopt);
    EXPECT_EQ(Strings::parseInt("-2147483649"), std::nullopt);
    EXPECT_EQ(Strings::parseInt("99999999999999999999"), std::nullopt);
}

TEST(Strings, Trim)
{
    EXPECT_EQ(Strings::trim(""), "");
    EXPECT_EQ(Strings::trim("   "), "");
    EXPECT_EQ(Strings::trim("abc"), "abc");
    EXPECT_EQ(Strings::trim("  abc  "), "abc");
    EXPECT_EQ(Strings::trim("\t\n abc \r\n"), "abc");
    EXPECT_EQ(Strings::trim("a b"), "a b");
    // Java's trim() strips every character at or below U+0020, control characters included.
    const std::string controls = std::string("\x01\x02") + "abc" + '\x1f' + '\0';
    EXPECT_EQ(Strings::trim(controls), "abc");
    // A UTF-8 no-break space is above U+0020 and stays.
    const std::string nbsp = std::string(Chars::kNbsp) + "abc";
    EXPECT_EQ(Strings::trim(nbsp), nbsp);
}

TEST(Strings, TrimRejectsTemporaryStrings)
{
    // The result views its argument, so trimming a temporary std::string would dangle.
    static_assert(!Trimmable<std::string>);  // an rvalue
    static_assert(!Trimmable<const std::string>);
    static_assert(Trimmable<std::string&>);
    static_assert(Trimmable<const std::string&>);
    static_assert(Trimmable<std::string_view>);
    static_assert(Trimmable<decltype("x")>);  // a string literal
    static_assert(Trimmable<const char*>);
    EXPECT_EQ(Strings::trim(std::string_view(" x ")), "x");
    EXPECT_TRUE(Strings::isEmpty(std::string("  ")));  // returns a bool, so a temporary is safe
}

TEST(Strings, ToLower)
{
    EXPECT_EQ(Strings::toLower(""), "");
    EXPECT_EQ(Strings::toLower("DashDot"), "dashdot");
    EXPECT_EQ(Strings::toLower("already lower 123"), "already lower 123");
    EXPECT_EQ(Strings::toLower("ABC_DEF-GHI"), "abc_def-ghi");
    // Non-ASCII bytes are left alone.
    const std::string alpha = std::string("A") + std::string(Chars::kAlpha);
    EXPECT_EQ(Strings::toLower(alpha), std::string("a") + std::string(Chars::kAlpha));
}

TEST(Strings, ToUpper)
{
    EXPECT_EQ(Strings::toUpper(""), "");
    EXPECT_EQ(Strings::toUpper("g80-7a"), "G80-7A");
    EXPECT_EQ(Strings::toUpper("ALREADY UPPER 123"), "ALREADY UPPER 123");
    // Non-ASCII bytes are left alone.
    EXPECT_EQ(Strings::toUpper("stra\u00DFe"), "STRA\u00DFE");
}

TEST(Strings, CollapseWhitespace)
{
    EXPECT_EQ(Strings::collapseWhitespace(""), "");
    EXPECT_EQ(Strings::collapseWhitespace("   "), "");
    EXPECT_EQ(Strings::collapseWhitespace("Hello  world! "), "Hello world!");
    EXPECT_EQ(Strings::collapseWhitespace("\nHello\tworld!\n\r"), "Hello world!");
    EXPECT_EQ(Strings::collapseWhitespace("Hello\r\r\r\nworld!"), "Hello world!");
    EXPECT_EQ(Strings::collapseWhitespace("a\x0B\x0C"
                                          "b"),
              "a b");
    // Other control characters are no regex whitespace, but trim() takes them at either end.
    EXPECT_EQ(Strings::collapseWhitespace("\x01"
                                          "a\x01"
                                          "b\x01"),
              "a\x01"
              "b");
    // Non-ASCII spaces are kept (Java's \s is ASCII only).
    EXPECT_EQ(Strings::collapseWhitespace("a\u00A0b"), "a\u00A0b");
}

TEST(Strings, JavaLengthCountsUtf16CodeUnits)
{
    EXPECT_EQ(Strings::javaLength(""), 0U);
    EXPECT_EQ(Strings::javaLength("abc"), 3U);
    EXPECT_EQ(Strings::javaLength("\u00E9t\u00E9"), 3U);
    EXPECT_EQ(Strings::javaLength("\u4E00"), 1U);
    // A code point above U+FFFF is a surrogate pair.
    EXPECT_EQ(Strings::javaLength("\U0001F600"), 2U);
    // A malformed byte reads as U+FFFD, one unit.
    EXPECT_EQ(Strings::javaLength("\xFF"), 1U);
}

TEST(Strings, JavaPrimaryCollatorCompareMatchesJava)
{
    // Collator.getInstance(Locale.US) at PRIMARY strength, pinned from JDK 17.
    EXPECT_EQ(Strings::javaPrimaryCollatorCompare("AeroTech", "Apogee"), -1);
    EXPECT_EQ(Strings::javaPrimaryCollatorCompare("Kosdon by AeroTech", "Kosdon"), 1);
    EXPECT_EQ(Strings::javaPrimaryCollatorCompare("LOC/Precision", "Loki Research"), -1);
    EXPECT_EQ(Strings::javaPrimaryCollatorCompare("Public Missiles, Ltd.", "Propulsion Polymers"),
              1);
    EXPECT_EQ(Strings::javaPrimaryCollatorCompare("a-b", "ab"), 0);
    EXPECT_EQ(Strings::javaPrimaryCollatorCompare("a b", "ab"), 0);
    EXPECT_EQ(Strings::javaPrimaryCollatorCompare("a.b", "ab"), -1);
    EXPECT_EQ(Strings::javaPrimaryCollatorCompare("ab", "a b c"), -1);
    EXPECT_EQ(Strings::javaPrimaryCollatorCompare("A", "a"), 0);
    EXPECT_EQ(Strings::javaPrimaryCollatorCompare("-5", "5"), 0);
    EXPECT_EQ(Strings::javaPrimaryCollatorCompare("", "-"), 0);
    EXPECT_EQ(Strings::javaPrimaryCollatorCompare("-", "5"), -1);
    EXPECT_EQ(Strings::javaPrimaryCollatorCompare("W", "-"), 1);
    EXPECT_EQ(Strings::javaPrimaryCollatorCompare("1", "a"), -1);
    EXPECT_EQ(Strings::javaPrimaryCollatorCompare("_", "a"), -1);
    EXPECT_EQ(Strings::javaPrimaryCollatorCompare("\x01"
                                                  "a",
                                                  "a"),
              0);
    EXPECT_EQ(Strings::javaPrimaryCollatorCompare(std::string_view("x\0", 2), "x"), 0);
    // Accents, expansions and characters beyond Latin-1.
    EXPECT_EQ(Strings::javaPrimaryCollatorCompare("\u00E9t\u00E9", "ete"), 0);
    EXPECT_EQ(Strings::javaPrimaryCollatorCompare("\u00C6ther", "aether"), 0);
    EXPECT_EQ(Strings::javaPrimaryCollatorCompare("stra\u00DFe", "strasse"), 0);
    EXPECT_EQ(Strings::javaPrimaryCollatorCompare("z", "\u00E9"), 1);
    EXPECT_EQ(Strings::javaPrimaryCollatorCompare("x", "\u00F8"), -1);
    EXPECT_EQ(Strings::javaPrimaryCollatorCompare("\u00F8", "\u4E00"), -1);
    EXPECT_EQ(Strings::javaPrimaryCollatorCompare("\u4E00", "\U0001F600"), -1);
    EXPECT_EQ(Strings::javaPrimaryCollatorCompare("\u2126", "z"), 1);
    EXPECT_EQ(Strings::javaPrimaryCollatorCompare("\u212A", "k"), 0);
    EXPECT_EQ(Strings::javaPrimaryCollatorCompare("\uFFFF", "\U00010000"), 1);
    EXPECT_EQ(Strings::javaPrimaryCollatorCompare("a\U0001F600", "a\uFFFD"), -1);
}

TEST(Strings, JavaPrimaryCollatorOrdersAsciiAsJava)
{
    // The printable ASCII characters sorted by JDK 17's US collator at PRIMARY strength: '<'
    // between two characters means the first sorts before the second, '=' that they are equal.
    // '-' has no primary weight at all, so it sorts first.
    constexpr std::string_view kOrder =
        R"x(-<_<,<;<:<!<?</<.<`<^<~<'<"<(<)<[<]<{<}<@<$<*<\<&<#<%<+<<<=<><|<0<1<2<3<4<5<6<7<8<9<A=a<B=b<C=c<D=d<E=e<F=f<G=g<H=h<I=i<J=j<K=k<L=l<M=m<N=n<O=o<P=p<Q=q<R=r<S=s<T=t<U=u<V=v<W=w<X=x<Y=y<Z=z)x";
    ASSERT_EQ(kOrder.size() % 2, 1U);
    for (std::size_t i = 0; i + 2 < kOrder.size(); i += 2)
    {
        const std::string_view first    = kOrder.substr(i, 1);
        const std::string_view second   = kOrder.substr(i + 2, 1);
        const int              expected = kOrder[i + 1] == '=' ? 0 : -1;
        EXPECT_EQ(Strings::javaPrimaryCollatorCompare(first, second), expected)
            << first << " vs " << second;
        EXPECT_EQ(Strings::javaPrimaryCollatorCompare(second, first), -expected)
            << second << " vs " << first;
    }
}

TEST(Strings, JavaPrimaryCollatorOrdersWordsAsJava)
{
    // Java's sort of these words; a pair joined by true compares equal.
    const std::vector<std::pair<std::string_view, bool>> sorted{
        {"", false},
        {" ", true},
        {"--", true},
        {"#1", false},
        {"12", false},
        {"1-2", true},
        {"123", false},
        {"a#", false},
        {"A1", false},
        {"a10", false},
        {"a9", false},
        {"AeroTech", false},
        {"Aerotech", true},
        {"Alpha Hybrid Rocketry LLC", false},
        {"\u00C5ngstr\u00F6m", false},
        {"Angstrom", true},
        {"Animal Motor Works", false},
        {"Apogee", false},
        {"caf\u00E9", false},
        {"cafe", true},
        {"cafes", false},
        {"Cesaroni Technology Inc.", false},
        {"Contrail Rockets", false},
        {"Ellis Mountain", false},
        {"Estes", false},
        {"Estes_2", false},
        {"Estes.2", false},
        {"estes2", false},
        {"Estes 2", true},
        {"Estes-2", true},
        {"Gorilla Rocket Motors", false},
        {"HyperTEK", false},
        {"Kosdon by AeroTech", false},
        {"LOC/Precision", false},
        {"Loki Research", false},
        {"Propulsion Polymers", false},
        {"Public Missiles, Ltd.", false},
        {"Quest", false},
        {"quest", true},
        {"Q-uest", true},
        {"RATT Works", false},
        {"Roadrunner Rocketry", false},
        {"Rocketvision", false},
        {"Sky Ripper Systems", false},
        {"WECO Feuerwerk", false},
        {"West Coast Hybrids", false},
        {"Z\u00FCrich", false},
        {"Zurich", true},
    };
    for (std::size_t i = 1; i < sorted.size(); i++)
    {
        const std::string_view previous = sorted.at(i - 1).first;
        const auto& [current, equal]    = sorted.at(i);
        const int expected              = equal ? 0 : -1;
        EXPECT_EQ(Strings::javaPrimaryCollatorCompare(previous, current), expected)
            << previous << " vs " << current;
        EXPECT_EQ(Strings::javaPrimaryCollatorCompare(current, previous), -expected)
            << current << " vs " << previous;
    }
}

TEST(Strings, EqualsIgnoreAsciiCase)
{
    EXPECT_TRUE(Strings::equalsIgnoreAsciiCase("", ""));
    EXPECT_TRUE(Strings::equalsIgnoreAsciiCase("dashdot", "DASHDOT"));
    EXPECT_TRUE(Strings::equalsIgnoreAsciiCase("DashDot", "dASHdOT"));
    EXPECT_TRUE(Strings::equalsIgnoreAsciiCase("a-b_1", "A-B_1"));
    EXPECT_FALSE(Strings::equalsIgnoreAsciiCase("dashdot", "dashdots"));
    EXPECT_FALSE(Strings::equalsIgnoreAsciiCase("dashdot", " dashdot"));
    EXPECT_FALSE(Strings::equalsIgnoreAsciiCase("", " "));
    // Only ASCII letters fold: Greek alpha and its capital differ.
    const std::string capitalAlpha = "\xCE\x91";
    EXPECT_FALSE(Strings::equalsIgnoreAsciiCase(capitalAlpha, Chars::kAlpha));
    EXPECT_TRUE(Strings::equalsIgnoreAsciiCase(Chars::kAlpha, Chars::kAlpha));
}

TEST(Strings, Split)
{
    using Parts = std::vector<std::string>;
    EXPECT_EQ(Strings::split("", ','), Parts{""});
    EXPECT_EQ(Strings::split("a", ','), Parts{"a"});
    EXPECT_EQ(Strings::split("a,b,c", ','), (Parts{"a", "b", "c"}));
    EXPECT_EQ(Strings::split("a,,b", ','), (Parts{"a", "", "b"}));
    EXPECT_EQ(Strings::split(",a,", ','), (Parts{"", "a", ""}));
    EXPECT_EQ(Strings::split("1 2 3 4", ' '), (Parts{"1", "2", "3", "4"}));
    EXPECT_EQ(Strings::split("no separator", ','), Parts{"no separator"});
}

TEST(Strings, SplitJavaDropsTrailingEmptyFields)
{
    // Java's String.split with limit 0: trailing empty strings are discarded, and an input the
    // separator never matches comes back as itself.
    using Parts = std::vector<std::string>;
    EXPECT_EQ(Strings::splitJava("", ','), Parts{""});
    EXPECT_EQ(Strings::splitJava("a", ','), Parts{"a"});
    EXPECT_EQ(Strings::splitJava("a,b,c", ','), (Parts{"a", "b", "c"}));
    EXPECT_EQ(Strings::splitJava("a,,b", ','), (Parts{"a", "", "b"}));
    EXPECT_EQ(Strings::splitJava("a,,b,,", ','), (Parts{"a", "", "b"}));
    EXPECT_EQ(Strings::splitJava(",a,", ','), (Parts{"", "a"}));
    EXPECT_EQ(Strings::splitJava(",", ','), Parts{});
    EXPECT_EQ(Strings::splitJava(",,,", ','), Parts{});
    EXPECT_EQ(Strings::splitJava("1,2,3,", ','), (Parts{"1", "2", "3"}));
    EXPECT_EQ(Strings::splitJava("a\nb\n", '\n'), (Parts{"a", "b"}));
}

TEST(Strings, Utf8CodePointDecodesOneSequence)
{
    // The test helper itself: one two- or three-byte sequence, nothing else.
    EXPECT_EQ(utf8CodePoint("\xC2\xB0"), 0x00B0U);
    EXPECT_EQ(utf8CodePoint("\xE2\x81\x84"), 0x2044U);
    EXPECT_EQ(utf8CodePoint(""), std::nullopt);
    EXPECT_EQ(utf8CodePoint("a"), std::nullopt);
    EXPECT_EQ(utf8CodePoint("\xC2"), std::nullopt);
    EXPECT_EQ(utf8CodePoint("\xC2\x41"), std::nullopt);
    EXPECT_EQ(utf8CodePoint("\xC2\xB0\xB0"), std::nullopt);
}

TEST(Strings, CharsAreUtf8)
{
    EXPECT_EQ(Chars::kDegree, "\xC2\xB0");
    EXPECT_EQ(Chars::kFraction, "\xE2\x81\x84");

    // Every constant decodes to the code point Chars.java names.
    using CharCase                    = std::pair<std::string_view, std::uint32_t>;
    const std::vector<CharCase> cases = {
        {Chars::kFrac12, 0x00BD},    {Chars::kFrac14, 0x00BC},     {Chars::kFrac34, 0x00BE},
        {Chars::kFraction, 0x2044},  {Chars::kDegree, 0x00B0},     {Chars::kSquared, 0x00B2},
        {Chars::kCubed, 0x00B3},     {Chars::kPermille, 0x2030},   {Chars::kDot, 0x00B7},
        {Chars::kTimes, 0x00D7},     {Chars::kNbsp, 0x00A0},       {Chars::kZwsp, 0x200B},
        {Chars::kEmDash, 0x2014},    {Chars::kMicro, 0x00B5},      {Chars::kAlpha, 0x03B1},
        {Chars::kTheta, 0x0398},     {Chars::kCopy, 0x00A9},       {Chars::kBullet, 0x2022},
        {Chars::kLeftArrow, 0x2190}, {Chars::kRightArrow, 0x2192}, {Chars::kUpArrow, 0x2191},
    };
    ASSERT_EQ(cases.size(), 21U);
    for (const auto& [text, codePoint] : cases)
    {
        EXPECT_EQ(utf8CodePoint(text), codePoint) << Strings::hexString(bytesOf(text));
    }
}

// ---- QtRocket additions: Java's Formatter "%.Nf" / "%.Ne" and Double.toString, pinned on JDK 17

TEST(Strings, FormatFixedRoundsHalfUpOnJavaDigits)
{
    EXPECT_EQ(Strings::formatFixed(0.25, 1), "0.3");  // C's printf gives "0.2"
    EXPECT_EQ(Strings::formatFixed(0.35, 1), "0.4");  // the exact value is below the tie
    EXPECT_EQ(Strings::formatFixed(0.45, 1), "0.5");
    EXPECT_EQ(Strings::formatFixed(1.05, 1), "1.1");
    EXPECT_EQ(Strings::formatFixed(1.015, 1), "1.0");
    EXPECT_EQ(Strings::formatFixed(1.005, 2), "1.01");
    EXPECT_EQ(Strings::formatFixed(2.675, 2), "2.68");
    EXPECT_EQ(Strings::formatFixed(0.125, 2), "0.13");
    EXPECT_EQ(Strings::formatFixed(12.3456785, 5), "12.34568");
    EXPECT_EQ(Strings::formatFixed(0.0005, 3), "0.001");
    EXPECT_EQ(Strings::formatFixed(0.0015, 3), "0.002");
    EXPECT_EQ(Strings::formatFixed(99.95, 1), "100.0");
    EXPECT_EQ(Strings::formatFixed(0.95, 1), "1.0");
    EXPECT_EQ(Strings::formatFixed(0.5, 0), "1");
    EXPECT_EQ(Strings::formatFixed(1.5, 0), "2");
    EXPECT_EQ(Strings::formatFixed(2.5, 0), "3");
    EXPECT_EQ(Strings::formatFixed(-0.5, 0), "-1");
    EXPECT_EQ(Strings::formatFixed(9.5, 0), "10");
    EXPECT_EQ(Strings::formatFixed(0.49999999999999994, 0), "0");
    EXPECT_EQ(Strings::formatFixed(0.49999999999999994, 1), "0.5");

    // Trailing zeros and the sign are kept, even for digits that round to zero.
    EXPECT_EQ(Strings::formatFixed(1.0, 1), "1.0");
    EXPECT_EQ(Strings::formatFixed(1.5, 2), "1.50");
    EXPECT_EQ(Strings::formatFixed(0.0, 3), "0.000");
    EXPECT_EQ(Strings::formatFixed(0.0, 0), "0");
    EXPECT_EQ(Strings::formatFixed(-0.0, 1), "-0.0");
    EXPECT_EQ(Strings::formatFixed(-0.04, 1), "-0.0");
    EXPECT_EQ(Strings::formatFixed(-0.04, 0), "-0");
    EXPECT_EQ(Strings::formatFixed(1e-5, 5), "0.00001");
    EXPECT_EQ(Strings::formatFixed(1e-5, 2), "0.00");
    EXPECT_EQ(Strings::formatFixed(1e-7, 1), "0.0");
    EXPECT_EQ(Strings::formatFixed(1e7, 1), "10000000.0");
    EXPECT_EQ(Strings::formatFixed(1e20, 1), "100000000000000000000.0");
    EXPECT_EQ(Strings::formatFixed(1e20, 0), "100000000000000000000");
    EXPECT_EQ(Strings::formatFixed(123456789.123, 2), "123456789.12");
    EXPECT_EQ(Strings::formatFixed(101325.0, 2), "101325.00");
    EXPECT_EQ(Strings::formatFixed(-12.3456789, 1), "-12.3");
    EXPECT_EQ(Strings::formatFixed(4.9e-324, 3), "0.000");
    EXPECT_EQ(Strings::formatFixed(1.5, -2), "2");  // a negative precision counts as 0
    // Above 2^63 the digits are the shortest ones: 1e20 / 0.001 is the double 1e23, which JDK 17
    // prints as "99999999999999990000000" (JDK-4511638) and JDK 19+ as here.
    EXPECT_EQ(Strings::formatFixed(9.999999999999999e22, 0), "100000000000000000000000");

    EXPECT_EQ(Strings::formatFixed(kNaN, 1), "NaN");
    EXPECT_EQ(Strings::formatFixed(kInf, 1), "Infinity");
    EXPECT_EQ(Strings::formatFixed(-kInf, 0), "-Infinity");
}

TEST(Strings, FormatScientificMatchesJavaFormatter)
{
    EXPECT_EQ(Strings::formatScientific(1234567.89, 2), "1.23e+06");
    EXPECT_EQ(Strings::formatScientific(1235000.0, 2), "1.24e+06");
    EXPECT_EQ(Strings::formatScientific(1245000.0, 2), "1.25e+06");  // half-up, not to even
    EXPECT_EQ(Strings::formatScientific(1225000.0, 2), "1.23e+06");
    EXPECT_EQ(Strings::formatScientific(0.1235, 2), "1.24e-01");
    EXPECT_EQ(Strings::formatScientific(9995000.0, 2), "1.00e+07");
    EXPECT_EQ(Strings::formatScientific(9.999999999999999e22, 2), "1.00e+23");
    EXPECT_EQ(Strings::formatScientific(0.024, 2), "2.40e-02");
    EXPECT_EQ(Strings::formatScientific(-0.0004, 2), "-4.00e-04");
    EXPECT_EQ(Strings::formatScientific(1e100, 2), "1.00e+100");
    EXPECT_EQ(Strings::formatScientific(1e-320, 2), "1.00e-320");
    EXPECT_EQ(Strings::formatScientific(0.0, 2), "0.00e+00");
    EXPECT_EQ(Strings::formatScientific(0.0, 0), "0e+00");
    EXPECT_EQ(Strings::formatScientific(-0.0, 1), "-0.0e+00");
    EXPECT_EQ(Strings::formatScientific(12345.0, 0), "1e+04");
    EXPECT_EQ(Strings::formatScientific(2.5, 3), "2.500e+00");
    EXPECT_EQ(Strings::formatScientific(kNaN, 2), "NaN");
    EXPECT_EQ(Strings::formatScientific(kInf, 2), "Infinity");
    EXPECT_EQ(Strings::formatScientific(-kInf, 2), "-Infinity");
}

TEST(Strings, JavaDoubleToStringMatchesJdk)
{
    EXPECT_EQ(Strings::javaDoubleToString(1.0), "1.0");
    EXPECT_EQ(Strings::javaDoubleToString(980.0), "980.0");
    EXPECT_EQ(Strings::javaDoubleToString(-314.0), "-314.0");
    EXPECT_EQ(Strings::javaDoubleToString(100.0), "100.0");
    EXPECT_EQ(Strings::javaDoubleToString(1234.5), "1234.5");
    EXPECT_EQ(Strings::javaDoubleToString(0.0125), "0.0125");
    EXPECT_EQ(Strings::javaDoubleToString(0.001), "0.001");
    EXPECT_EQ(Strings::javaDoubleToString(0.002), "0.002");
    EXPECT_EQ(Strings::javaDoubleToString(123456.789), "123456.789");
    EXPECT_EQ(Strings::javaDoubleToString(1.0e6), "1000000.0");
    EXPECT_EQ(Strings::javaDoubleToString(9999999.0), "9999999.0");
    EXPECT_EQ(Strings::javaDoubleToString(1000999.0), "1000999.0");
    EXPECT_EQ(Strings::javaDoubleToString(6.89475729e6), "6894757.29");
    EXPECT_EQ(Strings::javaDoubleToString(1.35581795), "1.35581795");
    EXPECT_EQ(Strings::javaDoubleToString(0.1 + 0.2), "0.30000000000000004");
    EXPECT_EQ(Strings::javaDoubleToString(1.0 / 3.0), "0.3333333333333333");

    EXPECT_EQ(Strings::javaDoubleToString(1e7), "1.0E7");
    EXPECT_EQ(Strings::javaDoubleToString(123456789.0), "1.23456789E8");
    EXPECT_EQ(Strings::javaDoubleToString(0.0003), "3.0E-4");
    EXPECT_EQ(Strings::javaDoubleToString(0.0001), "1.0E-4");
    EXPECT_EQ(Strings::javaDoubleToString(1e-5), "1.0E-5");
    EXPECT_EQ(Strings::javaDoubleToString(1.0e-10), "1.0E-10");
    EXPECT_EQ(Strings::javaDoubleToString(0.00014808), "1.4808E-4");
    EXPECT_EQ(Strings::javaDoubleToString(1.7e9), "1.7E9");
    EXPECT_EQ(Strings::javaDoubleToString(0.946e9), "9.46E8");
    EXPECT_EQ(Strings::javaDoubleToString(26.0e9), "2.6E10");
    EXPECT_EQ(Strings::javaDoubleToString(1e16), "1.0E16");
    EXPECT_EQ(Strings::javaDoubleToString(1e21), "1.0E21");
    EXPECT_EQ(Strings::javaDoubleToString(1.0E22), "1.0E22");
    EXPECT_EQ(Strings::javaDoubleToString(9.223372036854776E18), "9.223372036854776E18");
    EXPECT_EQ(Strings::javaDoubleToString(123456789012345680.0), "1.2345678901234568E17");
    EXPECT_EQ(Strings::javaDoubleToString(9007199254740994.0), "9.007199254740994E15");
    // Integers between 2^53 and 2^63 take Java's exact digits: 2^60 has 18 significant ones.
    EXPECT_EQ(Strings::javaDoubleToString(1152921504606846976.0), "1.15292150460684698E18");
    EXPECT_EQ(Strings::javaDoubleToString(std::numeric_limits<double>::max()),
              "1.7976931348623157E308");
    EXPECT_EQ(Strings::javaDoubleToString(std::numeric_limits<double>::min()),
              "2.2250738585072014E-308");
    EXPECT_EQ(Strings::javaDoubleToString(1.5e-323), "1.5E-323");

    EXPECT_EQ(Strings::javaDoubleToString(0.0), "0.0");
    EXPECT_EQ(Strings::javaDoubleToString(-0.0), "-0.0");
    EXPECT_EQ(Strings::javaDoubleToString(kNaN), "NaN");
    EXPECT_EQ(Strings::javaDoubleToString(kInf), "Infinity");
    EXPECT_EQ(Strings::javaDoubleToString(-kInf), "-Infinity");
}

TEST(Strings, OrkEnumNameIsLowerCaseWithoutUnderscores)
{
    EXPECT_EQ(Strings::toOrkEnumName("UPPER_IGNITION"), "upperignition");
    EXPECT_EQ(Strings::toOrkEnumName("KIT_BASH"), "kitbash");
    EXPECT_EQ(Strings::toOrkEnumName("ABSOLUTE"), "absolute");
    EXPECT_EQ(Strings::toOrkEnumName(""), "");
}

// DocumentConfig.findEnum: trim, then compare with the lower-cased name without underscores.
TEST(Strings, OrkEnumNameMatchesLikeFindEnum)
{
    EXPECT_TRUE(Strings::orkEnumNameMatches("upperignition", "UPPER_IGNITION"));
    EXPECT_TRUE(Strings::orkEnumNameMatches("  upperignition\n", "UPPER_IGNITION"));
    EXPECT_TRUE(Strings::orkEnumNameMatches("mirrorxy", "MIRROR_XY"));
    // The savers write some names with the underscore kept, which findEnum does not match.
    EXPECT_FALSE(Strings::orkEnumNameMatches("mirror_xy", "MIRROR_XY"));
    // The comparison itself is case-sensitive: only the constant's name is lower-cased.
    EXPECT_FALSE(Strings::orkEnumNameMatches("Absolute", "ABSOLUTE"));
    EXPECT_FALSE(Strings::orkEnumNameMatches("", "ABSOLUTE"));
}

}  // namespace
