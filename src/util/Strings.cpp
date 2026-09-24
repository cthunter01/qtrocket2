#include "QtRocket/util/Strings.h"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iterator>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

// fast_float.h is the library's entry point: it declares from_chars with its default arguments,
// which the defining parse_number.h that the include-cleaner check would ask for lacks.
// NOLINTNEXTLINE(misc-include-cleaner)
#include <fast_float/fast_float.h>

namespace QtRocket::Strings
{

namespace
{

/// MathUtil.EPSILON (10 mm^3 in m^3); MathUtil.equals(d, 0) is |d| < kEpsilon / 2.
constexpr double kEpsilon = 1e-8;

/// 2^63: FloatingDecimal converts the integers below it exactly (MAX_SMALL_BIN_EXP = 62).
constexpr double kTwoPow63 = 9223372036854775808.0;

[[nodiscard]] bool isAsciiDigit(char c) noexcept
{
    return c >= '0' && c <= '9';
}

[[nodiscard]] char asciiLower(char c) noexcept
{
    if (c >= 'A' && c <= 'Z')
    {
        return static_cast<char>(c + ('a' - 'A'));
    }
    return c;
}

/// A positive finite double as 0.<digits> * 10^exponent: what Java's Formatter gets from
/// FloatingDecimal before rounding (its digits and decExp).
struct Decimal
{
    std::string digits;
    int         exponent{};
};

/// The shortest digits that read back as the same double.
[[nodiscard]] Decimal shortestDecimal(double magnitude)
{
    // The longest shortest-round-trip form is "d.ddddddddddddddddde-308" (24 characters).
    std::array<char, 32> buffer{};
    const auto [end, ec] = std::to_chars(buffer.data(), std::next(buffer.data(), buffer.size()),
                                         magnitude, std::chars_format::scientific);
    const std::string_view text(buffer.data(), end);
    const std::size_t      e = text.find('e');

    Decimal decimal;
    for (const char c : text.substr(0, e))
    {
        if (c != '.')
        {
            decimal.digits.push_back(c);
        }
    }
    // to_chars writes the exponent as a sign and at least two digits: "e+05", "e-308".
    const std::string_view exponentText = text.substr(e + 1);
    int                    exponent     = 0;
    for (const char c : exponentText.substr(1))
    {
        exponent = (exponent * 10) + (c - '0');
    }
    if (exponentText.front() == '-')
    {
        exponent = -exponent;
    }
    decimal.exponent = exponent + 1;  // d.ddd * 10^E is 0.dddd * 10^(E + 1)
    return decimal;
}

/// FloatingDecimal.insignificantDigitsNumber: for 2^p2 the number of times it can be divided by 10
/// before dropping below 10, i.e. the count of its decimal digits below the leading one.
constexpr std::array<int, 64> kInsignificantDigitsNumber{
    0,  0,  0,  0,  1,  1,  1,  2,  2,  2,  3,  3,  3,  3,  4,  4,  4,  5,  5,  5,  6,  6,
    6,  6,  7,  7,  7,  8,  8,  8,  9,  9,  9,  9,  10, 10, 10, 11, 11, 11, 12, 12, 12, 12,
    13, 13, 13, 14, 14, 14, 15, 15, 15, 15, 16, 16, 16, 17, 17, 17, 18, 18, 18, 19};

/// FloatingDecimal.insignificantDigitsForPow2.
[[nodiscard]] int insignificantDigitsForPow2(int p2)
{
    if (p2 > 1 && std::cmp_less(p2, kInsignificantDigitsNumber.size()))
    {
        return kInsignificantDigitsNumber.at(static_cast<std::size_t>(p2));
    }
    return 0;
}

/// FloatingDecimal.developLongDigits, reached from dtoa's "easy case": the exact digits of an
/// integer @p value in [1, 2^63), with the low digits that lie below a double's 53 significant
/// bits rounded away half-up (2^60 = 1152921504606846976 gives "115292150460684698" * 10).
[[nodiscard]] Decimal exactIntegerDecimal(std::uint64_t value)
{
    constexpr int kSignificantBits = 53;  // FloatingDecimal's nSignificantBits of a normal double
    const int     binExp           = std::bit_width(value) - 1;
    int           exponent         = 0;
    const int     insignificant =
        binExp > kSignificantBits ? insignificantDigitsForPow2(binExp - kSignificantBits - 1) : 0;
    if (insignificant != 0)
    {
        std::uint64_t pow10 = 1;
        for (int i = 0; i < insignificant; i++)
        {
            pow10 *= 10;
        }
        const std::uint64_t residue = value % pow10;
        value /= pow10;
        exponent += insignificant;
        if (residue >= pow10 / 2)
        {
            ++value;  // round up on the digits discarded
        }
    }
    Decimal decimal;
    decimal.digits = std::format("{}", value);
    while (decimal.digits.size() > 1 && decimal.digits.back() == '0')
    {
        decimal.digits.pop_back();
        ++exponent;
    }
    decimal.exponent = exponent + static_cast<int>(decimal.digits.size());
    return decimal;
}

/// The digits Java's Formatter gets from FloatingDecimal.getBinaryToASCIIConverter(d, false): an
/// integer below 2^63 exactly, anything else as the shortest digits. Deviation: from 2^63 Java 17
/// keeps its older digit generation, which can differ from the shortest digits in the last place
/// at a rounding tie; JDK 21+ (JDK-8300869) uses the shortest digits too.
[[nodiscard]] Decimal javaDecimal(double magnitude)
{
    if (magnitude >= 1.0 && magnitude < kTwoPow63)
    {
        const auto integer = static_cast<std::uint64_t>(magnitude);
        if (static_cast<double>(integer) == magnitude)
        {
            return exactIntegerDecimal(integer);
        }
    }
    return shortestDecimal(magnitude);
}

/// FormattedFloatingDecimal.applyPrecision: keeps @p keep significant digits, rounding half-up on
/// the decimal digits (so 1.0005 to three decimals is "1.001", where C's printf, which rounds the
/// exact binary value, gives "1.000"). The digits beyond @p keep become zeros. Returns the
/// exponent, raised by one when the rounding carried out of the leading digit.
[[nodiscard]] int applyPrecision(std::string& digits, int exponent, int keep)
{
    const auto count = static_cast<int>(digits.size());
    if (keep >= count || keep < 0)
    {
        return exponent;  // nothing to round
    }
    if (keep == 0)
    {
        // Every digit is beyond the precision: the value rounds to 0 or to one unit.
        const bool roundUp = digits[0] >= '5';
        digits.assign(digits.size(), '0');
        if (roundUp)
        {
            digits[0] = '1';
            return exponent + 1;
        }
        return exponent;
    }
    const auto position = static_cast<std::size_t>(keep);
    if (digits[position] < '5')
    {
        std::ranges::fill(std::views::drop(digits, keep), '0');
        return exponent;
    }
    std::size_t i = position - 1;
    while (i > 0 && digits[i] == '9')
    {
        --i;
    }
    if (digits[i] == '9')
    {
        // All nines: the carry runs out of the leading digit.
        digits.assign(digits.size(), '0');
        digits[0] = '1';
        return exponent + 1;
    }
    ++digits[i];
    std::ranges::fill(std::views::drop(digits, static_cast<std::ptrdiff_t>(i + 1)), '0');
    return exponent;
}

/// Java's "%.<precision>f" of a positive finite value, then TextUtil's trimming of the trailing
/// zeros and a bare decimal point (FormattedFloatingDecimal.fillDecimal + trimTrailingZeros).
[[nodiscard]] std::string fixedNotation(double magnitude, int precision)
{
    Decimal decimal = javaDecimal(magnitude);
    decimal.exponent =
        applyPrecision(decimal.digits, decimal.exponent, decimal.exponent + precision);
    const std::string& digits = decimal.digits;
    const auto         count  = static_cast<int>(digits.size());

    std::string integerPart;
    std::string fraction;
    if (decimal.exponent > 0)
    {
        const auto exponent = static_cast<std::size_t>(decimal.exponent);
        if (count < decimal.exponent)
        {
            integerPart = digits + std::string(exponent - digits.size(), '0');
        }
        else
        {
            integerPart = digits.substr(0, exponent);
            fraction    = digits.substr(exponent, static_cast<std::size_t>(precision));
        }
    }
    else
    {
        const int zeros       = std::min(-decimal.exponent, precision);
        const int significant = std::max(0, std::min(count, precision + decimal.exponent));
        integerPart           = "0";
        fraction              = std::string(static_cast<std::size_t>(zeros), '0') +
                                digits.substr(0, static_cast<std::size_t>(significant));
    }
    while (!fraction.empty() && fraction.back() == '0')
    {
        fraction.pop_back();
    }
    if (fraction.empty())
    {
        return integerPart;
    }
    return integerPart + "." + fraction;
}

/// Java's "%.<precision>e" of a positive finite value, then TextUtil's trimming of the mantissa's
/// trailing zeros and its rewriting of the exponent ("e+05" to "e5", "e-05" to "e-5")
/// (FormattedFloatingDecimal.fillScientific + trimTrailingZeros + reformatExponent).
[[nodiscard]] std::string scientificNotation(double magnitude, int precision)
{
    Decimal decimal           = javaDecimal(magnitude);
    decimal.exponent          = applyPrecision(decimal.digits, decimal.exponent, precision + 1);
    const std::string& digits = decimal.digits;

    std::string mantissa = digits.substr(0, 1);
    std::string fraction = digits.substr(1, static_cast<std::size_t>(precision));
    while (!fraction.empty() && fraction.back() == '0')
    {
        fraction.pop_back();
    }
    if (!fraction.empty())
    {
        mantissa += "." + fraction;
    }
    // TextUtil never reaches a zero exponent (exponential notation is used only below 0.001 and
    // from 10000); it would print "3.1e" there, this prints "3.1e0".
    return std::format("{}e{}", mantissa, decimal.exponent - 1);
}

}  // namespace

std::string doubleToString(double value, int decimalPlaces, bool exponentialNotation)
{
    // TextUtil checks for zero before NaN: MathUtil.equals(NaN, 0) is false, so the order is safe.
    if (std::abs(value) < kEpsilon / 2)
    {
        return "0";
    }
    if (std::isnan(value))
    {
        return "NaN";
    }
    if (std::isinf(value))
    {
        return value < 0 ? "-Inf" : "Inf";
    }
    const int    precision      = std::max(0, decimalPlaces);
    const double magnitude      = std::abs(value);
    const bool   exponential    = exponentialNotation && (magnitude < 0.001 || magnitude >= 10000);
    const std::string formatted = exponential ? scientificNotation(magnitude, precision)
                                              : fixedNotation(magnitude, precision);
    // Java prints the sign of a negative value even when the digits round to zero ("-0").
    return value < 0 ? "-" + formatted : formatted;
}

std::string doubleToString(double value, int decimalPlaces)
{
    return doubleToString(value, decimalPlaces, true);
}

std::string doubleToString(double value)
{
    return doubleToString(value, kDefaultDecimalPlaces, true);
}

std::optional<double> parseDouble(std::string_view text) noexcept
{
    text = trim(text);  // Double.parseDouble trims the same characters as String.trim()
    if (text.empty())
    {
        return std::nullopt;
    }
    bool negative = false;
    if (text.front() == '+' || text.front() == '-')
    {
        negative = text.front() == '-';
        text.remove_prefix(1);
    }
    if (text == "NaN")
    {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (text == "Infinity" || text == "Inf")
    {
        const double infinity = std::numeric_limits<double>::infinity();
        return negative ? -infinity : infinity;
    }
    // from_chars would also take "inf", "nan(...)" and their upper-case forms, which Java rejects.
    if (text.empty() || (!isAsciiDigit(text.front()) && text.front() != '.'))
    {
        return std::nullopt;
    }
    // std::from_chars for floating point is missing from Apple's libc++ (availability-gated), so
    // fast_float, which libstdc++'s from_chars itself wraps, parses on every platform alike; its
    // default format is the general one, decimal digits with an optional exponent.
    double            value = 0.0;
    const char* const end   = std::next(text.data(), static_cast<std::ptrdiff_t>(text.size()));
    // end is data() + size(); from_chars comes from fast_float.h (see the include).
    // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage,misc-include-cleaner)
    const auto [ptr, ec] = fast_float::from_chars(text.data(), end, value);
    if (ptr != end || (ec != std::errc{} && ec != std::errc::result_out_of_range))
    {
        return std::nullopt;
    }
    // Out of range, fast_float stores what the literal rounds to: 0 below the smallest subnormal
    // (Java's 0.0 too) or the infinity above the largest double.
    if (std::isinf(value))
    {
        return std::nullopt;
    }
    return negative ? -value : value;
}

std::optional<int> parseInt(std::string_view text) noexcept
{
    // Integer.parseInt: one optional sign, then digits only.
    std::string_view digits = text;
    if (!digits.empty() && (digits.front() == '+' || digits.front() == '-'))
    {
        digits.remove_prefix(1);
    }
    if (digits.empty() || !std::ranges::all_of(digits, isAsciiDigit))
    {
        return std::nullopt;
    }
    if (text.front() == '+')
    {
        text.remove_prefix(1);  // Java takes a leading '+', from_chars does not
    }
    int               value = 0;
    const char* const end   = std::next(text.data(), static_cast<std::ptrdiff_t>(text.size()));
    // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage) end is data() + size()
    const auto result = std::from_chars(text.data(), end, value, 10);
    if (result.ec != std::errc{})
    {
        return std::nullopt;  // out of range
    }
    return value;
}

std::optional<double> convertToDouble(std::string_view text)
{
    std::string input(text);
    std::ranges::replace(input, ',', '.');
    const std::size_t separator = input.rfind('.');
    if (separator != std::string::npos)
    {
        std::string integerPart = input.substr(0, separator);
        std::erase(integerPart, '.');
        input = integerPart + input.substr(separator);
    }
    return parseDouble(input);
}

std::string_view trim(std::string_view text) noexcept
{
    const auto isBlank = [](char c) { return static_cast<unsigned char>(c) <= ' '; };
    while (!text.empty() && isBlank(text.front()))
    {
        text.remove_prefix(1);
    }
    while (!text.empty() && isBlank(text.back()))
    {
        text.remove_suffix(1);
    }
    return text;
}

bool isEmpty(std::string_view text) noexcept
{
    return trim(text).empty();
}

std::string toLower(std::string_view text)
{
    std::string out(text);
    std::ranges::transform(out, out.begin(), asciiLower);
    return out;
}

bool equalsIgnoreAsciiCase(std::string_view a, std::string_view b) noexcept
{
    return std::ranges::equal(a, b, {}, asciiLower, asciiLower);
}

std::vector<std::string> split(std::string_view text, char separator)
{
    std::vector<std::string> parts;
    std::size_t              start = 0;
    while (true)
    {
        const std::size_t end = text.find(separator, start);
        if (end == std::string_view::npos)
        {
            parts.emplace_back(text.substr(start));
            return parts;
        }
        parts.emplace_back(text.substr(start, end - start));
        start = end + 1;
    }
}

std::vector<std::string> splitJava(std::string_view text, char separator)
{
    // String.split: no match, the input itself; otherwise the fields without the trailing empties.
    if (!text.contains(separator))
    {
        return {std::string(text)};
    }
    std::vector<std::string> parts = split(text, separator);
    while (!parts.empty() && parts.back().empty())
    {
        parts.pop_back();
    }
    return parts;
}

std::string hexString(std::span<const std::byte> bytes)
{
    constexpr std::string_view kHex = "0123456789abcdef";
    std::string                out;
    out.reserve(bytes.size() * 2);
    for (const std::byte b : bytes)
    {
        const auto value = std::to_integer<unsigned>(b);
        out.push_back(kHex[value >> 4U]);
        out.push_back(kHex[value & 0xFU]);
    }
    return out;
}

std::vector<std::byte> asciiBytes(std::string_view text)
{
    std::vector<std::byte> bytes;
    bytes.reserve(text.size());
    for (const char c : text)
    {
        const auto value = static_cast<unsigned char>(c);
        if (value < 0x80)
        {
            bytes.push_back(std::byte{value});
        }
        else if (value >= 0xC0)
        {
            bytes.push_back(std::byte{'?'});  // the lead byte of a non-ASCII code point
        }
        // A continuation byte (0x80-0xBF) belongs to the code point already replaced.
    }
    return bytes;
}

std::string escapeXml(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    for (const char c : text)
    {
        const auto value = static_cast<unsigned char>(c);
        if (c == '&')
        {
            out += "&amp;";
        }
        else if (c == '<')
        {
            out += "&lt;";
        }
        else if (c == '>')
        {
            out += "&gt;";
        }
        else if (c == '"')
        {
            out += "&quot;";
        }
        else if ((value < 32 && c != '\t' && c != '\n' && c != '\r') || c == '\'' || value == 127)
        {
            // &apos; is not standard HTML, so a numeric reference is used instead.
            std::format_to(std::back_inserter(out), "&#{};", value);
        }
        else
        {
            out.push_back(c);
        }
    }
    return out;
}

std::string escapeHtml(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    for (const char c : text)
    {
        switch (c)
        {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            case '"':
                out += "&quot;";
                break;
            case '\'':
                out += "&#39;";
                break;
            case '\n':
                out += "<br>";
                break;
            default:
                out.push_back(c);
                break;
        }
    }
    return out;
}

std::string escapeCsv(std::string_view text)
{
    if (text.find_first_of("\",\r\n") == std::string_view::npos)
    {
        return std::string(text);
    }
    std::string out = "\"";
    for (const char c : text)
    {
        if (c == '"')
        {
            out.push_back('"');
        }
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

std::string removeHtmlTags(std::string_view text)
{
    // The regex "<[^>]*>": from a '<' to the next '>', however far away; a '<' with no '>' after it
    // does not match and stays.
    std::string out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size())
    {
        if (text[i] == '<')
        {
            const std::size_t close = text.find('>', i + 1);
            if (close != std::string_view::npos)
            {
                i = close + 1;
                continue;
            }
        }
        out.push_back(text[i]);
        ++i;
    }
    return out;
}

}  // namespace QtRocket::Strings
