#include "QtRocket/util/Strings.h"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <compare>
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

#include "QtRocket/util/FloatingDecimal.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket::Strings
{

namespace
{

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

/// The digits Java's Formatter gets from FloatingDecimal.getBinaryToASCIIConverter(d, false): an
/// integer below 2^63 exactly, anything else as the shortest digits. Deviation: from 2^63
/// upwards, and for subnormals, JDK 17's older digit generation often differs from the shortest
/// digits, being longer (6.8423234599999996E19 for 6.84232346E19) or not the closest
/// (-3.8189059803482716E25 where the shortest is ...717E25); JDK 21+ (JDK-8300869) uses the
/// shortest digits too.
[[nodiscard]] Decimal javaDecimal(double magnitude)
{
    if (magnitude >= 1.0 && magnitude < kTwoPow63 && magnitude == std::trunc(magnitude))
    {
        // FloatingDecimal's exact path for integers, the same in both of its modes.
        FloatingDecimal::BinaryToAscii exact = FloatingDecimal::binaryToAscii(magnitude, false);
        return Decimal{.digits = std::move(exact.digits), .exponent = exact.decimalExponent};
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

/// The integer part and the fraction of Java's "%.<precision>f" of a positive finite value, the
/// fraction padded with zeros to exactly @p precision digits (FormattedFloatingDecimal.fillDecimal
/// with the Formatter's zero padding).
struct FixedParts
{
    std::string integerPart;
    std::string fraction;
};

[[nodiscard]] FixedParts javaFixedParts(double magnitude, int precision)
{
    Decimal decimal = javaDecimal(magnitude);
    decimal.exponent =
        applyPrecision(decimal.digits, decimal.exponent, decimal.exponent + precision);
    const std::string& digits = decimal.digits;
    const auto         count  = static_cast<int>(digits.size());

    FixedParts parts;
    if (decimal.exponent > 0)
    {
        const auto exponent = static_cast<std::size_t>(decimal.exponent);
        if (count < decimal.exponent)
        {
            parts.integerPart = digits + std::string(exponent - digits.size(), '0');
        }
        else
        {
            parts.integerPart = digits.substr(0, exponent);
            parts.fraction    = digits.substr(exponent, static_cast<std::size_t>(precision));
        }
    }
    else
    {
        const int zeros       = std::min(-decimal.exponent, precision);
        const int significant = std::max(0, std::min(count, precision + decimal.exponent));
        parts.integerPart     = "0";
        parts.fraction        = std::string(static_cast<std::size_t>(zeros), '0') +
                                digits.substr(0, static_cast<std::size_t>(significant));
    }
    parts.fraction.resize(static_cast<std::size_t>(precision), '0');
    return parts;
}

/// Java's "%.<precision>f" of a positive finite value, then TextUtil's trimming of the trailing
/// zeros and a bare decimal point (trimTrailingZeros).
[[nodiscard]] std::string fixedNotation(double magnitude, int precision)
{
    FixedParts parts = javaFixedParts(magnitude, precision);
    while (!parts.fraction.empty() && parts.fraction.back() == '0')
    {
        parts.fraction.pop_back();
    }
    if (parts.fraction.empty())
    {
        return parts.integerPart;
    }
    return parts.integerPart + "." + parts.fraction;
}

/// Java's "%.<precision>e" of a positive finite value as its parts: exactly precision + 1 digits
/// d.ddd, rounded half-up, and the decimal exponent (FormattedFloatingDecimal.fillScientific).
struct ScientificParts
{
    std::string digits;
    int         exponent{};
};

[[nodiscard]] ScientificParts javaScientificParts(double magnitude, int precision)
{
    Decimal decimal            = javaDecimal(magnitude);
    decimal.exponent           = applyPrecision(decimal.digits, decimal.exponent, precision + 1);
    const auto      digitCount = static_cast<std::size_t>(precision) + 1;
    ScientificParts parts;
    parts.digits = decimal.digits.substr(0, digitCount);
    parts.digits.resize(digitCount, '0');
    parts.exponent = decimal.exponent - 1;
    return parts;
}

/// Java's "%.<precision>e" of a positive finite value, then TextUtil's trimming of the mantissa's
/// trailing zeros and its rewriting of the exponent ("e+05" to "e5", "e-05" to "e-5")
/// (trimTrailingZeros + reformatExponent).
[[nodiscard]] std::string scientificNotation(double magnitude, int precision)
{
    const ScientificParts parts    = javaScientificParts(magnitude, precision);
    std::string           mantissa = parts.digits.substr(0, 1);
    std::string           fraction = parts.digits.substr(1);
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
    return std::format("{}e{}", mantissa, parts.exponent);
}

/// U+FFFD, what Java's UTF-8 decoder puts in place of a malformed byte.
constexpr char32_t kReplacementCharacter = 0xFFFD;

/// Decodes the code point at @p position of UTF-8 @p text and moves @p position past it. A byte
/// that does not start a well-formed sequence (a stray continuation byte, a truncated or overlong
/// sequence, an encoded surrogate, a value above U+10FFFF) decodes alone to U+FFFD.
[[nodiscard]] char32_t decodeCodePoint(std::string_view text, std::size_t& position) noexcept
{
    const auto lead = static_cast<unsigned char>(text[position]);
    if (lead < 0x80)
    {
        position++;
        return lead;
    }
    std::size_t   length    = 0;
    std::uint32_t codePoint = 0;
    std::uint32_t minimum   = 0;
    if (lead >= 0xC2 && lead <= 0xDF)
    {
        length    = 2;
        codePoint = lead & 0x1FU;
        minimum   = 0x80;
    }
    else if (lead >= 0xE0 && lead <= 0xEF)
    {
        length    = 3;
        codePoint = lead & 0x0FU;
        minimum   = 0x800;
    }
    else if (lead >= 0xF0 && lead <= 0xF4)
    {
        length    = 4;
        codePoint = lead & 0x07U;
        minimum   = 0x10000;
    }
    bool valid = length != 0 && position + length <= text.size();
    for (std::size_t k = 1; valid && k < length; k++)
    {
        const auto next = static_cast<unsigned char>(text[position + k]);
        valid           = (next & 0xC0U) == 0x80U;
        codePoint       = (codePoint << 6U) | (next & 0x3FU);
    }
    const bool surrogate = codePoint >= 0xD800 && codePoint <= 0xDFFF;
    if (!valid || codePoint < minimum || codePoint > 0x10FFFF || surrogate)
    {
        position++;
        return kReplacementCharacter;
    }
    position += length;
    return static_cast<char32_t>(codePoint);
}

/// The UTF-16 code units of a Java String held as UTF-8, read one at a time.
class Utf16Reader
{
public:
    explicit Utf16Reader(std::string_view text) noexcept : m_text(text) { }

    [[nodiscard]] bool atEnd() const noexcept
    {
        return m_pendingLow == 0 && m_position >= m_text.size();
    }

    /// The next code unit; only when not atEnd().
    [[nodiscard]] char16_t next() noexcept
    {
        if (m_pendingLow != 0)
        {
            const char16_t low = m_pendingLow;
            m_pendingLow       = 0;
            return low;
        }
        const char32_t codePoint = decodeCodePoint(m_text, m_position);
        if (codePoint < 0x10000)
        {
            return static_cast<char16_t>(codePoint);
        }
        const std::uint32_t offset = static_cast<std::uint32_t>(codePoint) - 0x10000U;
        m_pendingLow               = static_cast<char16_t>(0xDC00U + (offset & 0x3FFU));
        return static_cast<char16_t>(0xD800U + (offset >> 10U));
    }

private:
    std::string_view m_text;
    std::size_t      m_position{0};
    char16_t         m_pendingLow{0};  ///< the low surrogate still to return, or 0
};

/// A run of code points that Java's case operations fold alike: every @p stride-th code point
/// from @p first to @p last becomes itself plus @p delta.
struct FoldRange
{
    std::uint32_t first;
    std::uint32_t last;
    std::uint32_t stride;
    std::int32_t  delta;
};

/// Character.toLowerCase(Character.toUpperCase(c)) of JDK 17 for every code point it changes
/// (1416 of them), in ascending order. Generated from the JDK: two characters are equal ignoring
/// case in String.equalsIgnoreCase exactly when they fold to the same code point.
constexpr std::array<FoldRange, 199> kCaseFolds{{
    {.first = 0x0041, .last = 0x005A, .stride = 1, .delta = 32},
    {.first = 0x00B5, .last = 0x00B5, .stride = 1, .delta = 775},
    {.first = 0x00C0, .last = 0x00D6, .stride = 1, .delta = 32},
    {.first = 0x00D8, .last = 0x00DE, .stride = 1, .delta = 32},
    {.first = 0x0100, .last = 0x012E, .stride = 2, .delta = 1},
    {.first = 0x0130, .last = 0x0130, .stride = 1, .delta = -199},
    {.first = 0x0131, .last = 0x0131, .stride = 1, .delta = -200},
    {.first = 0x0132, .last = 0x0136, .stride = 2, .delta = 1},
    {.first = 0x0139, .last = 0x0147, .stride = 2, .delta = 1},
    {.first = 0x014A, .last = 0x0176, .stride = 2, .delta = 1},
    {.first = 0x0178, .last = 0x0178, .stride = 1, .delta = -121},
    {.first = 0x0179, .last = 0x017D, .stride = 2, .delta = 1},
    {.first = 0x017F, .last = 0x017F, .stride = 1, .delta = -268},
    {.first = 0x0181, .last = 0x0181, .stride = 1, .delta = 210},
    {.first = 0x0182, .last = 0x0184, .stride = 2, .delta = 1},
    {.first = 0x0186, .last = 0x0186, .stride = 1, .delta = 206},
    {.first = 0x0187, .last = 0x0187, .stride = 1, .delta = 1},
    {.first = 0x0189, .last = 0x018A, .stride = 1, .delta = 205},
    {.first = 0x018B, .last = 0x018B, .stride = 1, .delta = 1},
    {.first = 0x018E, .last = 0x018E, .stride = 1, .delta = 79},
    {.first = 0x018F, .last = 0x018F, .stride = 1, .delta = 202},
    {.first = 0x0190, .last = 0x0190, .stride = 1, .delta = 203},
    {.first = 0x0191, .last = 0x0191, .stride = 1, .delta = 1},
    {.first = 0x0193, .last = 0x0193, .stride = 1, .delta = 205},
    {.first = 0x0194, .last = 0x0194, .stride = 1, .delta = 207},
    {.first = 0x0196, .last = 0x0196, .stride = 1, .delta = 211},
    {.first = 0x0197, .last = 0x0197, .stride = 1, .delta = 209},
    {.first = 0x0198, .last = 0x0198, .stride = 1, .delta = 1},
    {.first = 0x019C, .last = 0x019C, .stride = 1, .delta = 211},
    {.first = 0x019D, .last = 0x019D, .stride = 1, .delta = 213},
    {.first = 0x019F, .last = 0x019F, .stride = 1, .delta = 214},
    {.first = 0x01A0, .last = 0x01A4, .stride = 2, .delta = 1},
    {.first = 0x01A6, .last = 0x01A6, .stride = 1, .delta = 218},
    {.first = 0x01A7, .last = 0x01A7, .stride = 1, .delta = 1},
    {.first = 0x01A9, .last = 0x01A9, .stride = 1, .delta = 218},
    {.first = 0x01AC, .last = 0x01AC, .stride = 1, .delta = 1},
    {.first = 0x01AE, .last = 0x01AE, .stride = 1, .delta = 218},
    {.first = 0x01AF, .last = 0x01AF, .stride = 1, .delta = 1},
    {.first = 0x01B1, .last = 0x01B2, .stride = 1, .delta = 217},
    {.first = 0x01B3, .last = 0x01B5, .stride = 2, .delta = 1},
    {.first = 0x01B7, .last = 0x01B7, .stride = 1, .delta = 219},
    {.first = 0x01B8, .last = 0x01B8, .stride = 1, .delta = 1},
    {.first = 0x01BC, .last = 0x01BC, .stride = 1, .delta = 1},
    {.first = 0x01C4, .last = 0x01C4, .stride = 1, .delta = 2},
    {.first = 0x01C5, .last = 0x01C5, .stride = 1, .delta = 1},
    {.first = 0x01C7, .last = 0x01C7, .stride = 1, .delta = 2},
    {.first = 0x01C8, .last = 0x01C8, .stride = 1, .delta = 1},
    {.first = 0x01CA, .last = 0x01CA, .stride = 1, .delta = 2},
    {.first = 0x01CB, .last = 0x01DB, .stride = 2, .delta = 1},
    {.first = 0x01DE, .last = 0x01EE, .stride = 2, .delta = 1},
    {.first = 0x01F1, .last = 0x01F1, .stride = 1, .delta = 2},
    {.first = 0x01F2, .last = 0x01F4, .stride = 2, .delta = 1},
    {.first = 0x01F6, .last = 0x01F6, .stride = 1, .delta = -97},
    {.first = 0x01F7, .last = 0x01F7, .stride = 1, .delta = -56},
    {.first = 0x01F8, .last = 0x021E, .stride = 2, .delta = 1},
    {.first = 0x0220, .last = 0x0220, .stride = 1, .delta = -130},
    {.first = 0x0222, .last = 0x0232, .stride = 2, .delta = 1},
    {.first = 0x023A, .last = 0x023A, .stride = 1, .delta = 10795},
    {.first = 0x023B, .last = 0x023B, .stride = 1, .delta = 1},
    {.first = 0x023D, .last = 0x023D, .stride = 1, .delta = -163},
    {.first = 0x023E, .last = 0x023E, .stride = 1, .delta = 10792},
    {.first = 0x0241, .last = 0x0241, .stride = 1, .delta = 1},
    {.first = 0x0243, .last = 0x0243, .stride = 1, .delta = -195},
    {.first = 0x0244, .last = 0x0244, .stride = 1, .delta = 69},
    {.first = 0x0245, .last = 0x0245, .stride = 1, .delta = 71},
    {.first = 0x0246, .last = 0x024E, .stride = 2, .delta = 1},
    {.first = 0x0345, .last = 0x0345, .stride = 1, .delta = 116},
    {.first = 0x0370, .last = 0x0372, .stride = 2, .delta = 1},
    {.first = 0x0376, .last = 0x0376, .stride = 1, .delta = 1},
    {.first = 0x037F, .last = 0x037F, .stride = 1, .delta = 116},
    {.first = 0x0386, .last = 0x0386, .stride = 1, .delta = 38},
    {.first = 0x0388, .last = 0x038A, .stride = 1, .delta = 37},
    {.first = 0x038C, .last = 0x038C, .stride = 1, .delta = 64},
    {.first = 0x038E, .last = 0x038F, .stride = 1, .delta = 63},
    {.first = 0x0391, .last = 0x03A1, .stride = 1, .delta = 32},
    {.first = 0x03A3, .last = 0x03AB, .stride = 1, .delta = 32},
    {.first = 0x03C2, .last = 0x03C2, .stride = 1, .delta = 1},
    {.first = 0x03CF, .last = 0x03CF, .stride = 1, .delta = 8},
    {.first = 0x03D0, .last = 0x03D0, .stride = 1, .delta = -30},
    {.first = 0x03D1, .last = 0x03D1, .stride = 1, .delta = -25},
    {.first = 0x03D5, .last = 0x03D5, .stride = 1, .delta = -15},
    {.first = 0x03D6, .last = 0x03D6, .stride = 1, .delta = -22},
    {.first = 0x03D8, .last = 0x03EE, .stride = 2, .delta = 1},
    {.first = 0x03F0, .last = 0x03F0, .stride = 1, .delta = -54},
    {.first = 0x03F1, .last = 0x03F1, .stride = 1, .delta = -48},
    {.first = 0x03F4, .last = 0x03F4, .stride = 1, .delta = -60},
    {.first = 0x03F5, .last = 0x03F5, .stride = 1, .delta = -64},
    {.first = 0x03F7, .last = 0x03F7, .stride = 1, .delta = 1},
    {.first = 0x03F9, .last = 0x03F9, .stride = 1, .delta = -7},
    {.first = 0x03FA, .last = 0x03FA, .stride = 1, .delta = 1},
    {.first = 0x03FD, .last = 0x03FF, .stride = 1, .delta = -130},
    {.first = 0x0400, .last = 0x040F, .stride = 1, .delta = 80},
    {.first = 0x0410, .last = 0x042F, .stride = 1, .delta = 32},
    {.first = 0x0460, .last = 0x0480, .stride = 2, .delta = 1},
    {.first = 0x048A, .last = 0x04BE, .stride = 2, .delta = 1},
    {.first = 0x04C0, .last = 0x04C0, .stride = 1, .delta = 15},
    {.first = 0x04C1, .last = 0x04CD, .stride = 2, .delta = 1},
    {.first = 0x04D0, .last = 0x052E, .stride = 2, .delta = 1},
    {.first = 0x0531, .last = 0x0556, .stride = 1, .delta = 48},
    {.first = 0x10A0, .last = 0x10C5, .stride = 1, .delta = 7264},
    {.first = 0x10C7, .last = 0x10C7, .stride = 1, .delta = 7264},
    {.first = 0x10CD, .last = 0x10CD, .stride = 1, .delta = 7264},
    {.first = 0x13A0, .last = 0x13EF, .stride = 1, .delta = 38864},
    {.first = 0x13F0, .last = 0x13F5, .stride = 1, .delta = 8},
    {.first = 0x1C80, .last = 0x1C80, .stride = 1, .delta = -6222},
    {.first = 0x1C81, .last = 0x1C81, .stride = 1, .delta = -6221},
    {.first = 0x1C82, .last = 0x1C82, .stride = 1, .delta = -6212},
    {.first = 0x1C83, .last = 0x1C84, .stride = 1, .delta = -6210},
    {.first = 0x1C85, .last = 0x1C85, .stride = 1, .delta = -6211},
    {.first = 0x1C86, .last = 0x1C86, .stride = 1, .delta = -6204},
    {.first = 0x1C87, .last = 0x1C87, .stride = 1, .delta = -6180},
    {.first = 0x1C88, .last = 0x1C88, .stride = 1, .delta = 35267},
    {.first = 0x1C90, .last = 0x1CBA, .stride = 1, .delta = -3008},
    {.first = 0x1CBD, .last = 0x1CBF, .stride = 1, .delta = -3008},
    {.first = 0x1E00, .last = 0x1E94, .stride = 2, .delta = 1},
    {.first = 0x1E9B, .last = 0x1E9B, .stride = 1, .delta = -58},
    {.first = 0x1E9E, .last = 0x1E9E, .stride = 1, .delta = -7615},
    {.first = 0x1EA0, .last = 0x1EFE, .stride = 2, .delta = 1},
    {.first = 0x1F08, .last = 0x1F0F, .stride = 1, .delta = -8},
    {.first = 0x1F18, .last = 0x1F1D, .stride = 1, .delta = -8},
    {.first = 0x1F28, .last = 0x1F2F, .stride = 1, .delta = -8},
    {.first = 0x1F38, .last = 0x1F3F, .stride = 1, .delta = -8},
    {.first = 0x1F48, .last = 0x1F4D, .stride = 1, .delta = -8},
    {.first = 0x1F59, .last = 0x1F5F, .stride = 2, .delta = -8},
    {.first = 0x1F68, .last = 0x1F6F, .stride = 1, .delta = -8},
    {.first = 0x1F88, .last = 0x1F8F, .stride = 1, .delta = -8},
    {.first = 0x1F98, .last = 0x1F9F, .stride = 1, .delta = -8},
    {.first = 0x1FA8, .last = 0x1FAF, .stride = 1, .delta = -8},
    {.first = 0x1FB8, .last = 0x1FB9, .stride = 1, .delta = -8},
    {.first = 0x1FBA, .last = 0x1FBB, .stride = 1, .delta = -74},
    {.first = 0x1FBC, .last = 0x1FBC, .stride = 1, .delta = -9},
    {.first = 0x1FBE, .last = 0x1FBE, .stride = 1, .delta = -7173},
    {.first = 0x1FC8, .last = 0x1FCB, .stride = 1, .delta = -86},
    {.first = 0x1FCC, .last = 0x1FCC, .stride = 1, .delta = -9},
    {.first = 0x1FD8, .last = 0x1FD9, .stride = 1, .delta = -8},
    {.first = 0x1FDA, .last = 0x1FDB, .stride = 1, .delta = -100},
    {.first = 0x1FE8, .last = 0x1FE9, .stride = 1, .delta = -8},
    {.first = 0x1FEA, .last = 0x1FEB, .stride = 1, .delta = -112},
    {.first = 0x1FEC, .last = 0x1FEC, .stride = 1, .delta = -7},
    {.first = 0x1FF8, .last = 0x1FF9, .stride = 1, .delta = -128},
    {.first = 0x1FFA, .last = 0x1FFB, .stride = 1, .delta = -126},
    {.first = 0x1FFC, .last = 0x1FFC, .stride = 1, .delta = -9},
    {.first = 0x2126, .last = 0x2126, .stride = 1, .delta = -7517},
    {.first = 0x212A, .last = 0x212A, .stride = 1, .delta = -8383},
    {.first = 0x212B, .last = 0x212B, .stride = 1, .delta = -8262},
    {.first = 0x2132, .last = 0x2132, .stride = 1, .delta = 28},
    {.first = 0x2160, .last = 0x216F, .stride = 1, .delta = 16},
    {.first = 0x2183, .last = 0x2183, .stride = 1, .delta = 1},
    {.first = 0x24B6, .last = 0x24CF, .stride = 1, .delta = 26},
    {.first = 0x2C00, .last = 0x2C2E, .stride = 1, .delta = 48},
    {.first = 0x2C60, .last = 0x2C60, .stride = 1, .delta = 1},
    {.first = 0x2C62, .last = 0x2C62, .stride = 1, .delta = -10743},
    {.first = 0x2C63, .last = 0x2C63, .stride = 1, .delta = -3814},
    {.first = 0x2C64, .last = 0x2C64, .stride = 1, .delta = -10727},
    {.first = 0x2C67, .last = 0x2C6B, .stride = 2, .delta = 1},
    {.first = 0x2C6D, .last = 0x2C6D, .stride = 1, .delta = -10780},
    {.first = 0x2C6E, .last = 0x2C6E, .stride = 1, .delta = -10749},
    {.first = 0x2C6F, .last = 0x2C6F, .stride = 1, .delta = -10783},
    {.first = 0x2C70, .last = 0x2C70, .stride = 1, .delta = -10782},
    {.first = 0x2C72, .last = 0x2C72, .stride = 1, .delta = 1},
    {.first = 0x2C75, .last = 0x2C75, .stride = 1, .delta = 1},
    {.first = 0x2C7E, .last = 0x2C7F, .stride = 1, .delta = -10815},
    {.first = 0x2C80, .last = 0x2CE2, .stride = 2, .delta = 1},
    {.first = 0x2CEB, .last = 0x2CED, .stride = 2, .delta = 1},
    {.first = 0x2CF2, .last = 0x2CF2, .stride = 1, .delta = 1},
    {.first = 0xA640, .last = 0xA66C, .stride = 2, .delta = 1},
    {.first = 0xA680, .last = 0xA69A, .stride = 2, .delta = 1},
    {.first = 0xA722, .last = 0xA72E, .stride = 2, .delta = 1},
    {.first = 0xA732, .last = 0xA76E, .stride = 2, .delta = 1},
    {.first = 0xA779, .last = 0xA77B, .stride = 2, .delta = 1},
    {.first = 0xA77D, .last = 0xA77D, .stride = 1, .delta = -35332},
    {.first = 0xA77E, .last = 0xA786, .stride = 2, .delta = 1},
    {.first = 0xA78B, .last = 0xA78B, .stride = 1, .delta = 1},
    {.first = 0xA78D, .last = 0xA78D, .stride = 1, .delta = -42280},
    {.first = 0xA790, .last = 0xA792, .stride = 2, .delta = 1},
    {.first = 0xA796, .last = 0xA7A8, .stride = 2, .delta = 1},
    {.first = 0xA7AA, .last = 0xA7AA, .stride = 1, .delta = -42308},
    {.first = 0xA7AB, .last = 0xA7AB, .stride = 1, .delta = -42319},
    {.first = 0xA7AC, .last = 0xA7AC, .stride = 1, .delta = -42315},
    {.first = 0xA7AD, .last = 0xA7AD, .stride = 1, .delta = -42305},
    {.first = 0xA7AE, .last = 0xA7AE, .stride = 1, .delta = -42308},
    {.first = 0xA7B0, .last = 0xA7B0, .stride = 1, .delta = -42258},
    {.first = 0xA7B1, .last = 0xA7B1, .stride = 1, .delta = -42282},
    {.first = 0xA7B2, .last = 0xA7B2, .stride = 1, .delta = -42261},
    {.first = 0xA7B3, .last = 0xA7B3, .stride = 1, .delta = 928},
    {.first = 0xA7B4, .last = 0xA7BE, .stride = 2, .delta = 1},
    {.first = 0xA7C2, .last = 0xA7C2, .stride = 1, .delta = 1},
    {.first = 0xA7C4, .last = 0xA7C4, .stride = 1, .delta = -48},
    {.first = 0xA7C5, .last = 0xA7C5, .stride = 1, .delta = -42307},
    {.first = 0xA7C6, .last = 0xA7C6, .stride = 1, .delta = -35384},
    {.first = 0xA7C7, .last = 0xA7C9, .stride = 2, .delta = 1},
    {.first = 0xA7F5, .last = 0xA7F5, .stride = 1, .delta = 1},
    {.first = 0xFF21, .last = 0xFF3A, .stride = 1, .delta = 32},
    {.first = 0x10400, .last = 0x10427, .stride = 1, .delta = 40},
    {.first = 0x104B0, .last = 0x104D3, .stride = 1, .delta = 40},
    {.first = 0x10C80, .last = 0x10CB2, .stride = 1, .delta = 64},
    {.first = 0x118A0, .last = 0x118BF, .stride = 1, .delta = 32},
    {.first = 0x16E40, .last = 0x16E5F, .stride = 1, .delta = 32},
    {.first = 0x1E900, .last = 0x1E921, .stride = 1, .delta = 34},
}};

/// The case fold of @p codePoint (see kCaseFolds).
[[nodiscard]] std::uint32_t caseFold(char32_t codePoint) noexcept
{
    const auto value = static_cast<std::uint32_t>(codePoint);
    // An index, not an iterator: std::array's iterator is a pointer with libstdc++ and libc++
    // but a class with MSVC, so `const auto*` would not compile there.
    const auto rangesBefore = static_cast<std::size_t>(std::distance(
        kCaseFolds.begin(), std::ranges::upper_bound(kCaseFolds, value, {}, &FoldRange::first)));
    if (rangesBefore == 0)
    {
        return value;
    }
    const FoldRange& range = kCaseFolds.at(rangesBefore - 1);
    if (value > range.last || (value - range.first) % range.stride != 0)
    {
        return value;
    }
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(value) + range.delta);
}

/// The value of a hexadecimal digit, or -1.
[[nodiscard]] int hexDigitValue(char c) noexcept
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }
    return -1;
}

/// Whether @p text is empty or one of Java's float-literal suffixes f, F, d, D.
[[nodiscard]] bool isEmptyOrFloatSuffix(std::string_view text) noexcept
{
    return text.empty() || (text.size() == 1 &&
                            (text[0] == 'f' || text[0] == 'F' || text[0] == 'd' || text[0] == 'D'));
}

/// mantissa * 2^exponent rounded to the nearest double, ties to even; @p sticky says that nonzero
/// bits below the mantissa were dropped. Beyond the double range the result is an infinity, and
/// below half the smallest subnormal a zero.
[[nodiscard]] double roundBinary(std::uint64_t mantissa, std::int64_t exponent,
                                 bool sticky) noexcept
{
    if (mantissa == 0)
    {
        return 0.0;
    }
    constexpr std::int64_t kPrecision         = 53;
    constexpr std::int64_t kMinNormalExponent = -1022;
    constexpr std::int64_t kMaxExponent       = 1023;
    constexpr std::int64_t kSubnormalBits     = 1075;  // bits from 2^-1 down to 2^-1074, plus one
    const auto             width              = static_cast<std::int64_t>(std::bit_width(mantissa));
    const std::int64_t     topExponent        = exponent + width - 1;
    if (topExponent > kMaxExponent)
    {
        return std::numeric_limits<double>::infinity();
    }
    // The significant bits the result has room for: 53, or fewer for a subnormal.
    const std::int64_t precision =
        topExponent >= kMinNormalExponent ? kPrecision : topExponent + kSubnormalBits;
    if (precision < 0)
    {
        return 0.0;
    }
    const std::int64_t shift = width - precision;  // the low bits to round away
    if (shift <= 0)
    {
        return std::ldexp(static_cast<double>(mantissa), static_cast<int>(exponent));
    }
    const auto          bits = static_cast<unsigned int>(shift);
    const std::uint64_t kept = bits >= 64U ? 0U : mantissa >> bits;
    const std::uint64_t dropped =
        bits >= 64U ? mantissa : mantissa & ((std::uint64_t{1} << bits) - 1U);
    const std::uint64_t half = std::uint64_t{1} << (bits - 1U);
    const bool roundUp       = dropped > half || (dropped == half && (sticky || (kept & 1U) != 0));
    const std::uint64_t rounded = roundUp ? kept + 1U : kept;
    return std::ldexp(static_cast<double>(rounded), static_cast<int>(exponent + shift));
}

/// The significand of a hexadecimal float: its leading significant bits (at most 64; the ones
/// beyond only set the sticky bit) and the power of two that scales them.
struct HexSignificand
{
    std::uint64_t mantissa{0};
    bool          sticky{false};
    std::int64_t  scale{0};
    std::size_t   length{0};  ///< the characters read
};

/// Reads hexadecimal digits with an optional point, at least one digit, from the start of
/// @p text; nullopt for a second point or no digit.
[[nodiscard]] std::optional<HexSignificand> readHexSignificand(std::string_view text) noexcept
{
    constexpr std::uint64_t kRoom = std::uint64_t{1} << 60U;
    HexSignificand          significand;
    bool                    anyDigit = false;
    bool                    point    = false;
    for (; significand.length < text.size(); significand.length++)
    {
        const char c = text[significand.length];
        if (c == '.')
        {
            if (point)
            {
                return std::nullopt;
            }
            point = true;
            continue;
        }
        const int digit = hexDigitValue(c);
        if (digit < 0)
        {
            break;
        }
        anyDigit = true;
        if (significand.mantissa >= kRoom)
        {
            significand.sticky = significand.sticky || digit != 0;
            significand.scale += point ? 0 : 4;
            continue;
        }
        significand.mantissa = (significand.mantissa << 4U) | static_cast<std::uint64_t>(digit);
        significand.scale -= point ? 4 : 0;
    }
    if (!anyDigit)
    {
        return std::nullopt;
    }
    return significand;
}

/// A decimal exponent as Integer.parseInt reads it: the value, clamped past the int range.
struct DecimalExponent
{
    std::int64_t value{0};
    bool         overflows{false};  ///< beyond the int range, where parseInt fails
    std::size_t  length{0};         ///< the characters read
};

/// Reads an optional sign and at least one decimal digit from the start of @p text.
[[nodiscard]] std::optional<DecimalExponent> readDecimalExponent(std::string_view text) noexcept
{
    constexpr std::int64_t kIntMax = std::numeric_limits<int>::max();
    DecimalExponent        exponent;
    bool                   negative = false;
    if (!text.empty() && (text[0] == '+' || text[0] == '-'))
    {
        negative        = text[0] == '-';
        exponent.length = 1;
    }
    const std::size_t digitsStart = exponent.length;
    for (; exponent.length < text.size() && isAsciiDigit(text[exponent.length]); exponent.length++)
    {
        exponent.value = (exponent.value * 10) + (text[exponent.length] - '0');
        if (exponent.value > kIntMax)
        {
            exponent.overflows = true;
            exponent.value     = kIntMax;
        }
    }
    if (exponent.length == digitsStart)
    {
        return std::nullopt;
    }
    if (negative)
    {
        exponent.value = -exponent.value;
    }
    return exponent;
}

/// FloatingDecimal.parseHexString for what follows the sign and "0x": hexadecimal digits with an
/// optional point, a binary exponent [pP][+-]?digits and an optional float suffix, all of it;
/// the magnitude, or nullopt.
[[nodiscard]] std::optional<double> parseJavaHexMagnitude(std::string_view text) noexcept
{
    const std::optional<HexSignificand> significand = readHexSignificand(text);
    if (!significand.has_value())
    {
        return std::nullopt;
    }
    const std::size_t p = significand->length;
    if (p >= text.size() || (text[p] != 'p' && text[p] != 'P'))
    {
        return std::nullopt;
    }
    const std::optional<DecimalExponent> exponent = readDecimalExponent(text.substr(p + 1));
    if (!exponent.has_value() || !isEmptyOrFloatSuffix(text.substr(p + 1 + exponent->length)))
    {
        return std::nullopt;
    }
    if (significand->mantissa == 0)
    {
        return 0.0;
    }
    if (exponent->overflows)
    {
        // Integer.parseInt fails: the exponent's sign alone decides
        return exponent->value < 0 ? 0.0 : std::numeric_limits<double>::infinity();
    }
    return roundBinary(significand->mantissa, exponent->value + significand->scale,
                       significand->sticky);
}

/// The length of FloatingDecimal.readJavaFormatString's decimal number at the start of @p text:
/// digits with an optional point (at least one digit) and an optional exponent [eE][+-]?digits;
/// nullopt when there is none.
[[nodiscard]] std::optional<std::size_t> decimalNumberLength(std::string_view text) noexcept
{
    std::size_t i        = 0;
    bool        anyDigit = false;
    bool        point    = false;
    for (; i < text.size() && (isAsciiDigit(text[i]) || text[i] == '.'); i++)
    {
        if (text[i] == '.')
        {
            if (point)
            {
                return std::nullopt;
            }
            point = true;
        }
        else
        {
            anyDigit = true;
        }
    }
    if (!anyDigit)
    {
        return std::nullopt;
    }
    if (i < text.size() && (text[i] == 'e' || text[i] == 'E'))
    {
        const std::optional<DecimalExponent> exponent = readDecimalExponent(text.substr(i + 1));
        if (!exponent.has_value())
        {
            return std::nullopt;
        }
        i += 1 + exponent->length;
    }
    return i;
}

/// A run of BMP code points that JDK 17's Collator.getInstance(Locale.US) gives the same primary
/// collation weights: {first, last, primary, second primary}, up to two non-zero primaries each
/// (0 marks an absent one, so a run with none is ignorable at the PRIMARY strength). Transcribed
/// from CollationElementIterator.primaryOrder() for every code point from U+0000 to U+FFFF; a
/// code point outside every run is unmapped there.
using CollationRun = std::array<std::uint16_t, 4>;

constexpr std::size_t kRunFirst         = 0;
constexpr std::size_t kRunLast          = 1;
constexpr std::size_t kRunPrimary       = 2;
constexpr std::size_t kRunSecondPrimary = 3;

// clang-format off
constexpr std::array<CollationRun, 264> kUsCollationRuns{{
    {0x0000, 0x0020, 0, 0}, {0x0021, 0x0021, 6, 0}, {0x0022, 0x0022, 20, 0}, {0x0023, 0x0023, 55, 0},
    {0x0024, 0x0024, 39, 0}, {0x0025, 0x0025, 56, 0}, {0x0026, 0x0026, 54, 0}, {0x0027, 0x0027, 19, 0},
    {0x0028, 0x0028, 23, 0}, {0x0029, 0x0029, 24, 0}, {0x002A, 0x002A, 52, 0}, {0x002B, 0x002B, 57, 0},
    {0x002C, 0x002C, 3, 0}, {0x002D, 0x002D, 0, 0}, {0x002E, 0x002E, 11, 0}, {0x002F, 0x002F, 10, 0},
    {0x0030, 0x0030, 69, 0}, {0x0031, 0x0031, 70, 0}, {0x0032, 0x0032, 71, 0}, {0x0033, 0x0033, 72, 0},
    {0x0034, 0x0034, 73, 0}, {0x0035, 0x0035, 74, 0}, {0x0036, 0x0036, 75, 0}, {0x0037, 0x0037, 76, 0},
    {0x0038, 0x0038, 77, 0}, {0x0039, 0x0039, 78, 0}, {0x003A, 0x003A, 5, 0}, {0x003B, 0x003B, 4, 0},
    {0x003C, 0x003C, 61, 0}, {0x003D, 0x003D, 62, 0}, {0x003E, 0x003E, 63, 0}, {0x003F, 0x003F, 8, 0},
    {0x0040, 0x0040, 33, 0}, {0x0041, 0x0041, 82, 0}, {0x0042, 0x0042, 83, 0}, {0x0043, 0x0043, 84, 0},
    {0x0044, 0x0044, 85, 0}, {0x0045, 0x0045, 87, 0}, {0x0046, 0x0046, 88, 0}, {0x0047, 0x0047, 89, 0},
    {0x0048, 0x0048, 90, 0}, {0x0049, 0x0049, 91, 0}, {0x004A, 0x004A, 92, 0}, {0x004B, 0x004B, 93, 0},
    {0x004C, 0x004C, 94, 0}, {0x004D, 0x004D, 95, 0}, {0x004E, 0x004E, 96, 0}, {0x004F, 0x004F, 97, 0},
    {0x0050, 0x0050, 98, 0}, {0x0051, 0x0051, 99, 0}, {0x0052, 0x0052, 100, 0}, {0x0053, 0x0053, 101, 0},
    {0x0054, 0x0054, 102, 0}, {0x0055, 0x0055, 103, 0}, {0x0056, 0x0056, 104, 0}, {0x0057, 0x0057, 105, 0},
    {0x0058, 0x0058, 106, 0}, {0x0059, 0x0059, 107, 0}, {0x005A, 0x005A, 108, 0}, {0x005B, 0x005B, 25, 0},
    {0x005C, 0x005C, 53, 0}, {0x005D, 0x005D, 26, 0}, {0x005E, 0x005E, 14, 0}, {0x005F, 0x005F, 1, 0},
    {0x0060, 0x0060, 13, 0}, {0x0061, 0x0061, 82, 0}, {0x0062, 0x0062, 83, 0}, {0x0063, 0x0063, 84, 0},
    {0x0064, 0x0064, 85, 0}, {0x0065, 0x0065, 87, 0}, {0x0066, 0x0066, 88, 0}, {0x0067, 0x0067, 89, 0},
    {0x0068, 0x0068, 90, 0}, {0x0069, 0x0069, 91, 0}, {0x006A, 0x006A, 92, 0}, {0x006B, 0x006B, 93, 0},
    {0x006C, 0x006C, 94, 0}, {0x006D, 0x006D, 95, 0}, {0x006E, 0x006E, 96, 0}, {0x006F, 0x006F, 97, 0},
    {0x0070, 0x0070, 98, 0}, {0x0071, 0x0071, 99, 0}, {0x0072, 0x0072, 100, 0}, {0x0073, 0x0073, 101, 0},
    {0x0074, 0x0074, 102, 0}, {0x0075, 0x0075, 103, 0}, {0x0076, 0x0076, 104, 0}, {0x0077, 0x0077, 105, 0},
    {0x0078, 0x0078, 106, 0}, {0x0079, 0x0079, 107, 0}, {0x007A, 0x007A, 108, 0}, {0x007B, 0x007B, 27, 0},
    {0x007C, 0x007C, 65, 0}, {0x007D, 0x007D, 28, 0}, {0x007E, 0x007E, 16, 0}, {0x007F, 0x00A0, 0, 0},
    {0x00A1, 0x00A1, 7, 0}, {0x00A2, 0x00A2, 36, 0}, {0x00A3, 0x00A3, 47, 0}, {0x00A4, 0x00A4, 34, 0},
    {0x00A5, 0x00A5, 51, 0}, {0x00A6, 0x00A6, 66, 0}, {0x00A7, 0x00A7, 29, 0}, {0x00A8, 0x00A8, 15, 0},
    {0x00A9, 0x00A9, 31, 0}, {0x00AB, 0x00AB, 21, 0}, {0x00AC, 0x00AC, 64, 0}, {0x00AD, 0x00AD, 0, 0},
    {0x00AE, 0x00AE, 32, 0}, {0x00AF, 0x00AF, 2, 0}, {0x00B0, 0x00B0, 67, 0}, {0x00B1, 0x00B1, 58, 0},
    {0x00B4, 0x00B4, 12, 0}, {0x00B5, 0x00B5, 68, 0}, {0x00B6, 0x00B6, 30, 0}, {0x00B7, 0x00B7, 17, 0},
    {0x00B8, 0x00B8, 18, 0}, {0x00BB, 0x00BB, 22, 0}, {0x00BC, 0x00BC, 79, 0}, {0x00BD, 0x00BD, 80, 0},
    {0x00BE, 0x00BE, 81, 0}, {0x00BF, 0x00BF, 9, 0}, {0x00C0, 0x00C5, 82, 0}, {0x00C6, 0x00C6, 82, 87},
    {0x00C7, 0x00C7, 84, 0}, {0x00C8, 0x00CB, 87, 0}, {0x00CC, 0x00CF, 91, 0}, {0x00D0, 0x00D0, 86, 0},
    {0x00D1, 0x00D1, 96, 0}, {0x00D2, 0x00D6, 97, 0}, {0x00D7, 0x00D7, 60, 0}, {0x00D9, 0x00DC, 103, 0},
    {0x00DD, 0x00DD, 107, 0}, {0x00DE, 0x00DE, 102, 90}, {0x00DF, 0x00DF, 101, 101}, {0x00E0, 0x00E5, 82, 0},
    {0x00E6, 0x00E6, 82, 87}, {0x00E7, 0x00E7, 84, 0}, {0x00E8, 0x00EB, 87, 0}, {0x00EC, 0x00EF, 91, 0},
    {0x00F0, 0x00F0, 86, 0}, {0x00F1, 0x00F1, 96, 0}, {0x00F2, 0x00F6, 97, 0}, {0x00F7, 0x00F7, 59, 0},
    {0x00F9, 0x00FC, 103, 0}, {0x00FD, 0x00FD, 107, 0}, {0x00FE, 0x00FE, 102, 90}, {0x00FF, 0x00FF, 107, 0},
    {0x0100, 0x0105, 82, 0}, {0x0106, 0x010D, 84, 0}, {0x010E, 0x010F, 85, 0}, {0x0112, 0x011B, 87, 0},
    {0x011C, 0x0123, 89, 0}, {0x0124, 0x0125, 90, 0}, {0x0128, 0x0130, 91, 0}, {0x0134, 0x0135, 92, 0},
    {0x0136, 0x0137, 93, 0}, {0x0139, 0x013E, 94, 0}, {0x0143, 0x0148, 96, 0}, {0x014C, 0x0151, 97, 0},
    {0x0152, 0x0153, 97, 87}, {0x0154, 0x0159, 100, 0}, {0x015A, 0x0161, 101, 0}, {0x0162, 0x0165, 102, 0},
    {0x0168, 0x0173, 103, 0}, {0x0174, 0x0175, 105, 0}, {0x0176, 0x0178, 107, 0}, {0x0179, 0x017E, 108, 0},
    {0x01A0, 0x01A1, 97, 0}, {0x01AF, 0x01B0, 103, 0}, {0x01CD, 0x01CE, 82, 0}, {0x01CF, 0x01D0, 91, 0},
    {0x01D1, 0x01D2, 97, 0}, {0x01D3, 0x01DC, 103, 0}, {0x01DE, 0x01E1, 82, 0}, {0x01E2, 0x01E3, 32256, 0},
    {0x01E6, 0x01E7, 89, 0}, {0x01E8, 0x01E9, 93, 0}, {0x01EA, 0x01ED, 97, 0}, {0x01F0, 0x01F0, 92, 0},
    {0x01F4, 0x01F5, 89, 0}, {0x01F8, 0x01F9, 96, 0}, {0x01FA, 0x01FB, 82, 0}, {0x01FC, 0x01FD, 32256, 0},
    {0x0200, 0x0203, 82, 0}, {0x0204, 0x0207, 87, 0}, {0x0208, 0x020B, 91, 0}, {0x020C, 0x020F, 97, 0},
    {0x0210, 0x0213, 100, 0}, {0x0214, 0x0217, 103, 0}, {0x0218, 0x0219, 101, 0}, {0x021A, 0x021B, 102, 0},
    {0x021E, 0x021F, 90, 0}, {0x0226, 0x0227, 82, 0}, {0x0228, 0x0229, 87, 0}, {0x022A, 0x0231, 97, 0},
    {0x0232, 0x0233, 107, 0}, {0x0300, 0x0345, 0, 0}, {0x0360, 0x0361, 0, 0}, {0x037E, 0x037E, 4, 0},
    {0x0385, 0x0385, 15, 0}, {0x0387, 0x0387, 17, 0}, {0x0483, 0x0486, 0, 0}, {0x0E3F, 0x0E3F, 35, 0},
    {0x1E00, 0x1E01, 82, 0}, {0x1E02, 0x1E07, 83, 0}, {0x1E08, 0x1E09, 84, 0}, {0x1E0A, 0x1E13, 85, 0},
    {0x1E14, 0x1E1D, 87, 0}, {0x1E1E, 0x1E1F, 88, 0}, {0x1E20, 0x1E21, 89, 0}, {0x1E22, 0x1E2B, 90, 0},
    {0x1E2C, 0x1E2F, 91, 0}, {0x1E30, 0x1E35, 93, 0}, {0x1E36, 0x1E3D, 94, 0}, {0x1E3E, 0x1E43, 95, 0},
    {0x1E44, 0x1E4B, 96, 0}, {0x1E4C, 0x1E53, 97, 0}, {0x1E54, 0x1E57, 98, 0}, {0x1E58, 0x1E5F, 100, 0},
    {0x1E60, 0x1E69, 101, 0}, {0x1E6A, 0x1E71, 102, 0}, {0x1E72, 0x1E7B, 103, 0}, {0x1E7C, 0x1E7F, 104, 0},
    {0x1E80, 0x1E89, 105, 0}, {0x1E8A, 0x1E8D, 106, 0}, {0x1E8E, 0x1E8F, 107, 0}, {0x1E90, 0x1E95, 108, 0},
    {0x1E96, 0x1E96, 90, 0}, {0x1E97, 0x1E97, 102, 0}, {0x1E98, 0x1E98, 105, 0}, {0x1E99, 0x1E99, 107, 0},
    {0x1EA0, 0x1EB7, 82, 0}, {0x1EB8, 0x1EC7, 87, 0}, {0x1EC8, 0x1ECB, 91, 0}, {0x1ECC, 0x1EE3, 97, 0},
    {0x1EE4, 0x1EF1, 103, 0}, {0x1EF2, 0x1EF9, 107, 0}, {0x1FC1, 0x1FC1, 15, 0}, {0x1FED, 0x1FEE, 15, 0},
    {0x1FEF, 0x1FEF, 13, 0}, {0x1FFD, 0x1FFD, 12, 0}, {0x2000, 0x2015, 0, 0}, {0x20A1, 0x20A1, 37, 0},
    {0x20A2, 0x20A2, 38, 0}, {0x20A3, 0x20A3, 42, 0}, {0x20A4, 0x20A4, 43, 0}, {0x20A5, 0x20A5, 44, 0},
    {0x20A6, 0x20A6, 45, 0}, {0x20A7, 0x20A7, 46, 0}, {0x20A8, 0x20A8, 48, 0}, {0x20A9, 0x20A9, 50, 0},
    {0x20AA, 0x20AA, 49, 0}, {0x20AB, 0x20AB, 40, 0}, {0x20AC, 0x20AC, 41, 0}, {0x20D0, 0x20E1, 0, 0},
    {0x212A, 0x212A, 93, 0}, {0x212B, 0x212B, 82, 0}, {0x2212, 0x2212, 0, 0}, {0x2260, 0x2260, 62, 0},
    {0x226E, 0x226E, 61, 0}, {0x226F, 0x226F, 63, 0}, {0x3000, 0x3000, 0, 0}, {0xFEFF, 0xFEFF, 0, 0},
}};
// clang-format on

/// The primary weight Java gives an unmapped character, followed by its UTF-16 code unit(s).
constexpr std::uint16_t kUnmappedPrimary = 0x7FFF;

/// Appends the non-zero primary collation weights of @p codePoint, as Java's
/// CollationElementIterator returns them for the character.
void appendCollationPrimaries(char32_t codePoint, std::vector<std::uint16_t>& out)
{
    if (codePoint > 0xFFFF)
    {
        // A supplementary character is unmapped: the marker, then its surrogate pair.
        const std::uint32_t offset = static_cast<std::uint32_t>(codePoint) - 0x10000U;
        out.push_back(kUnmappedPrimary);
        out.push_back(static_cast<std::uint16_t>(0xD800U + (offset >> 10U)));
        out.push_back(static_cast<std::uint16_t>(0xDC00U + (offset & 0x3FFU)));
        return;
    }
    const auto unit = static_cast<std::uint16_t>(codePoint);
    // The number of runs starting at or before the unit; an index, not an iterator, since
    // std::array's iterator is a pointer with libstdc++ and libc++ but a class with MSVC.
    const auto runsBefore = static_cast<std::size_t>(std::distance(
        kUsCollationRuns.begin(),
        std::ranges::upper_bound(kUsCollationRuns, unit, {},
                                 [](const CollationRun& run) { return run[kRunFirst]; })));
    if (runsBefore > 0)
    {
        const CollationRun& run = kUsCollationRuns.at(runsBefore - 1);
        if (unit <= run[kRunLast])
        {
            for (const std::uint16_t primary : {run[kRunPrimary], run[kRunSecondPrimary]})
            {
                if (primary != 0)
                {
                    out.push_back(primary);
                }
            }
            return;
        }
    }
    out.push_back(kUnmappedPrimary);
    out.push_back(unit);
}

/// The primary collation weights of UTF-8 @p text, character by character (the US rules have no
/// contractions, so the weights of a string are those of its characters in turn).
[[nodiscard]] std::vector<std::uint16_t> collationPrimaries(std::string_view text)
{
    std::vector<std::uint16_t> primaries;
    primaries.reserve(text.size());
    std::size_t position = 0;
    while (position < text.size())
    {
        appendCollationPrimaries(decodeCodePoint(text, position), primaries);
    }
    return primaries;
}

/// True for the characters of Java's regex class \s: space, \t, \n, \x0B, \f and \r.
[[nodiscard]] bool isRegexWhitespace(char c) noexcept
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\x0B' || c == '\f' || c == '\r';
}

[[nodiscard]] char asciiUpper(char c) noexcept
{
    if (c >= 'a' && c <= 'z')
    {
        return static_cast<char>(c - ('a' - 'A'));
    }
    return c;
}

}  // namespace

std::string doubleToString(double value, int decimalPlaces, bool exponentialNotation)
{
    // TextUtil checks for zero before NaN: MathUtil.equals(NaN, 0) is false, so the order is safe.
    if (MathUtil::equals(value, 0.0))
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

std::string formatFixed(double value, int precision)
{
    if (std::isnan(value))
    {
        return "NaN";
    }
    if (std::isinf(value))
    {
        return value < 0 ? "-Infinity" : "Infinity";
    }
    const int   digits = std::max(0, precision);
    FixedParts  parts  = javaFixedParts(std::abs(value), digits);
    std::string text   = std::move(parts.integerPart);
    if (digits > 0)
    {
        text += ".";
        text += parts.fraction;
    }
    // The Formatter prints the sign of a negative zero and of digits that round to zero.
    return std::signbit(value) ? "-" + text : text;
}

std::string formatScientific(double value, int precision)
{
    if (std::isnan(value))
    {
        return "NaN";
    }
    if (std::isinf(value))
    {
        return value < 0 ? "-Infinity" : "Infinity";
    }
    const int             digits = std::max(0, precision);
    const ScientificParts parts  = javaScientificParts(std::abs(value), digits);
    std::string           text   = parts.digits.substr(0, 1);
    if (digits > 0)
    {
        text += ".";
        text += parts.digits.substr(1);
    }
    text += std::format("e{}{:02}", parts.exponent < 0 ? '-' : '+', std::abs(parts.exponent));
    return std::signbit(value) ? "-" + text : text;
}

std::string javaDoubleToString(double value)
{
    if (std::isnan(value))
    {
        return "NaN";
    }
    if (std::isinf(value))
    {
        return value < 0 ? "-Infinity" : "Infinity";
    }
    if (value == 0)
    {
        return std::signbit(value) ? "-0.0" : "0.0";
    }
    const double       magnitude = std::abs(value);
    const Decimal      decimal   = javaDecimal(magnitude);
    const std::string& digits    = decimal.digits;
    const auto         count     = static_cast<int>(digits.size());

    std::string text;
    if (magnitude >= 0.001 && magnitude < 10000000.0)
    {
        if (decimal.exponent <= 0)
        {
            text = "0." + std::string(static_cast<std::size_t>(-decimal.exponent), '0') + digits;
        }
        else if (decimal.exponent >= count)
        {
            text = digits + std::string(static_cast<std::size_t>(decimal.exponent - count), '0') +
                   ".0";
        }
        else
        {
            const auto point = static_cast<std::size_t>(decimal.exponent);
            text             = digits.substr(0, point) + "." + digits.substr(point);
        }
    }
    else
    {
        text = digits.substr(0, 1) + "." + (count > 1 ? digits.substr(1) : std::string("0")) + "E" +
               std::format("{}", decimal.exponent - 1);
    }
    return std::signbit(value) ? "-" + text : text;
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

std::optional<double> javaParseDouble(std::string_view text) noexcept
{
    text = trim(text);
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
    if (text.empty())
    {
        return std::nullopt;
    }
    std::optional<double> magnitude;
    if (text.front() == 'N')
    {
        if (text != "NaN")
        {
            return std::nullopt;
        }
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (text.front() == 'I')
    {
        if (text != "Infinity")
        {
            return std::nullopt;
        }
        magnitude = std::numeric_limits<double>::infinity();
    }
    else if (text.size() > 1 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
    {
        magnitude = parseJavaHexMagnitude(text.substr(2));
    }
    else if (const std::optional<std::size_t> length = decimalNumberLength(text);
             length.has_value() && isEmptyOrFloatSuffix(text.substr(*length)))
    {
        // parseDouble() reads such a number correctly rounded, underflow included; its only
        // refusal left is an overflow, which Java makes an infinity.
        magnitude =
            parseDouble(text.substr(0, *length)).value_or(std::numeric_limits<double>::infinity());
    }
    if (!magnitude.has_value())
    {
        return std::nullopt;
    }
    return negative ? -*magnitude : *magnitude;
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
    return javaParseDouble(input);
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

std::string toUpper(std::string_view text)
{
    std::string out(text);
    std::ranges::transform(out, out.begin(), asciiUpper);
    return out;
}

std::string collapseWhitespace(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    bool inRun = false;
    for (const char c : text)
    {
        if (isRegexWhitespace(c))
        {
            if (!inRun)
            {
                out.push_back(' ');
            }
            inRun = true;
        }
        else
        {
            out.push_back(c);
            inRun = false;
        }
    }
    return std::string(trim(out));
}

bool equalsIgnoreAsciiCase(std::string_view a, std::string_view b) noexcept
{
    return std::ranges::equal(a, b, {}, asciiLower, asciiLower);
}

std::string toOrkEnumName(std::string_view enumName)
{
    std::string out;
    out.reserve(enumName.size());
    for (const char c : enumName)
    {
        if (c != '_')
        {
            out.push_back(asciiLower(c));
        }
    }
    return out;
}

bool orkEnumNameMatches(std::string_view text, std::string_view enumName)
{
    return trim(text) == toOrkEnumName(enumName);
}

std::u32string toCodePoints(std::string_view text)
{
    std::u32string codePoints;
    codePoints.reserve(text.size());
    std::size_t position = 0;
    while (position < text.size())
    {
        codePoints.push_back(decodeCodePoint(text, position));
    }
    return codePoints;
}

bool javaEqualsIgnoreCase(std::string_view a, std::string_view b) noexcept
{
    // Java's case mappings keep a character within its plane, so equal lengths in code points
    // and in UTF-16 code units come to the same here.
    std::size_t i = 0;
    std::size_t j = 0;
    while (i < a.size() && j < b.size())
    {
        const char32_t x = decodeCodePoint(a, i);
        const char32_t y = decodeCodePoint(b, j);
        if (x != y && caseFold(x) != caseFold(y))
        {
            return false;
        }
    }
    return i == a.size() && j == b.size();
}

int javaCompareTo(std::string_view a, std::string_view b) noexcept
{
    Utf16Reader x(a);
    Utf16Reader y(b);
    while (!x.atEnd() && !y.atEnd())
    {
        const char16_t c1 = x.next();
        const char16_t c2 = y.next();
        if (c1 != c2)
        {
            return static_cast<int>(c1) - static_cast<int>(c2);
        }
    }
    // len1 - len2, counted from the end of the common part
    int difference = 0;
    for (; !x.atEnd(); difference++)
    {
        static_cast<void>(x.next());
    }
    for (; !y.atEnd(); difference--)
    {
        static_cast<void>(y.next());
    }
    return difference;
}

int javaHashCode(std::string_view text) noexcept
{
    std::uint32_t hash = 0;
    Utf16Reader   reader(text);
    while (!reader.atEnd())
    {
        hash = (31U * hash) + static_cast<std::uint32_t>(reader.next());
    }
    return static_cast<int>(hash);
}

std::size_t javaLength(std::string_view text) noexcept
{
    std::size_t length   = 0;
    std::size_t position = 0;
    while (position < text.size())
    {
        // a code point above U+FFFF is a surrogate pair
        length += decodeCodePoint(text, position) > 0xFFFF ? 2U : 1U;
    }
    return length;
}

int javaPrimaryCollatorCompare(std::string_view a, std::string_view b)
{
    // The weights compare element by element, and a string whose weights run out first sorts
    // first: std::vector's lexicographic ordering.
    const std::strong_ordering order = collationPrimaries(a) <=> collationPrimaries(b);
    if (std::is_lt(order))
    {
        return -1;
    }
    return std::is_gt(order) ? 1 : 0;
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
