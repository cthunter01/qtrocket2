#include "QtRocket/util/BigDecimal.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "QtRocket/util/BugError.h"
#include "QtRocket/util/FloatingDecimal.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// The most exponent digits BigDecimal.parseExp() reads.
constexpr std::size_t kMaxExponentDigits = 10;

/// The decimal exponents beyond which every BigDecimal is outside the double range: the largest
/// double is below 1e309 and the smallest subnormal above 1e-324, with room to spare.
constexpr std::int64_t kDoubleOverflowExponent  = 400;
constexpr std::int64_t kDoubleUnderflowExponent = -400;

[[nodiscard]] constexpr bool isDigit(char c) noexcept
{
    return c >= '0' && c <= '9';
}

[[nodiscard]] constexpr bool fitsInt32(std::int64_t value) noexcept
{
    return value >= std::numeric_limits<std::int32_t>::min() &&
           value <= std::numeric_limits<std::int32_t>::max();
}

/// BigDecimal.parseExp(): the exponent written in @p text, the characters after the 'e'. nullopt
/// where Java throws: no digits, more than ten of them once the leading zeros that exceed ten
/// characters are skipped, a character that is no digit, or a value outside the int range.
[[nodiscard]] std::optional<std::int64_t> parseExponent(std::string_view text) noexcept
{
    bool negative = false;
    if (!text.empty() && (text.front() == '-' || text.front() == '+'))
    {
        negative = text.front() == '-';
        text.remove_prefix(1);
    }
    if (text.empty())
    {
        return std::nullopt;  // no exponent digits
    }
    // skip leading zeros in the exponent
    while (text.size() > kMaxExponentDigits && text.front() == '0')
    {
        text.remove_prefix(1);
    }
    if (text.size() > kMaxExponentDigits)
    {
        return std::nullopt;  // too many nonzero exponent digits
    }
    std::int64_t exponent = 0;
    for (const char c : text)
    {
        if (!isDigit(c))
        {
            return std::nullopt;
        }
        exponent = (exponent * 10) + (c - '0');
    }
    if (negative)
    {
        exponent = -exponent;
    }
    if (!fitsInt32(exponent))
    {
        return std::nullopt;  // exponent overflow
    }
    return exponent;
}

/// What the significand of a BigDecimal text holds.
struct Significand
{
    /// The digits without the point and without leading zeros; empty for zero.
    std::string digits;
    /// The number of digits after the point.
    std::int64_t fractionDigits{0};
    /// Where the significand ends in the text: at its end or at the exponent mark.
    std::size_t end{0};
};

/// The significand of @p text from @p start on, or nullopt for a second point, a character that
/// is neither a digit, a point nor an exponent mark, or no digit at all.
[[nodiscard]] std::optional<Significand> parseSignificand(std::string_view text, std::size_t start)
{
    Significand significand;
    bool        dot      = false;
    bool        anyDigit = false;
    std::size_t position = start;
    for (; position < text.size(); ++position)
    {
        const char c = text[position];
        if (isDigit(c))
        {
            anyDigit = true;
            if (c != '0' || !significand.digits.empty())
            {
                significand.digits.push_back(c);
            }
            if (dot)
            {
                ++significand.fractionDigits;
            }
        }
        else if (c == '.')
        {
            if (dot)
            {
                return std::nullopt;  // more than one decimal point
            }
            dot = true;
        }
        else if (c == 'e' || c == 'E')
        {
            break;
        }
        else
        {
            return std::nullopt;
        }
    }
    if (!anyDigit)
    {
        return std::nullopt;  // no digits found
    }
    significand.end = position;
    return significand;
}

}  // namespace

BigDecimal::BigDecimal(bool negative, std::string digits, std::int32_t scale) noexcept
  : m_digits(std::move(digits)), m_negative(negative), m_scale(scale)
{
}

std::optional<BigDecimal> BigDecimal::parse(std::string_view text)
{
    std::size_t start    = 0;
    bool        negative = false;
    if (!text.empty() && (text.front() == '-' || text.front() == '+'))
    {
        negative = text.front() == '-';
        start    = 1;
    }
    std::optional<Significand> significand = parseSignificand(text, start);
    if (!significand.has_value())
    {
        return std::nullopt;
    }
    std::int64_t exponent = 0;
    if (significand->end < text.size())
    {
        const std::optional<std::int64_t> parsed = parseExponent(text.substr(significand->end + 1));
        if (!parsed.has_value())
        {
            return std::nullopt;
        }
        exponent = *parsed;
    }
    // adjustScale(): the scale must fit an int
    if (significand->fractionDigits > std::numeric_limits<std::int32_t>::max())
    {
        return std::nullopt;
    }
    const std::int64_t scale = significand->fractionDigits - exponent;
    if (!fitsInt32(scale))
    {
        return std::nullopt;  // scale out of range
    }
    if (significand->digits.empty())
    {
        // Every zero has the unscaled value 0, which carries no sign.
        return BigDecimal(false, "0", static_cast<std::int32_t>(scale));
    }
    return BigDecimal(negative, std::move(significand->digits), static_cast<std::int32_t>(scale));
}

BigDecimal BigDecimal::valueOf(std::int64_t value)
{
    if (value == 0)
    {
        return {};
    }
    // The magnitude through the unsigned type: -value overflows for the smallest long.
    const auto          bits      = static_cast<std::uint64_t>(value);
    const std::uint64_t magnitude = value < 0 ? 0 - bits : bits;
    return {value < 0, std::format("{}", magnitude), 0};
}

std::optional<BigDecimal> BigDecimal::valueOfDouble(double value)
{
    if (!std::isfinite(value))
    {
        return std::nullopt;
    }
    return parse(FloatingDecimal::toJavaFormatString(value));
}

int BigDecimal::signum() const noexcept
{
    if (m_negative)
    {
        return -1;
    }
    return m_digits == "0" ? 0 : 1;
}

std::string BigDecimal::unscaledValue() const
{
    return m_negative ? "-" + m_digits : m_digits;
}

std::int64_t BigDecimal::adjustedExponent() const noexcept
{
    return static_cast<std::int64_t>(m_digits.size()) - 1 - m_scale;
}

std::string BigDecimal::toString() const
{
    if (m_scale == 0)
    {
        return unscaledValue();
    }
    std::string        text     = m_negative ? "-" : "";
    const auto         length   = static_cast<std::int64_t>(m_digits.size());
    const std::int64_t adjusted = adjustedExponent();
    if (m_scale >= 0 && adjusted >= -6)
    {
        // Plain notation. pad is the count of zeros between the point and the digits.
        const std::int64_t pad = m_scale - length;
        if (pad >= 0)
        {
            text += "0.";
            text.append(static_cast<std::size_t>(pad), '0');
            text += m_digits;
        }
        else
        {
            const auto integerDigits = static_cast<std::size_t>(-pad);
            text.append(m_digits, 0, integerDigits);
            text += '.';
            text.append(m_digits, integerDigits);
        }
        return text;
    }
    // Scientific notation
    text += m_digits.front();
    if (length > 1)
    {
        text += '.';
        text.append(m_digits, 1);
    }
    // adjusted is never 0 here: it is positive for a negative scale and below -6 otherwise.
    text += 'E';
    if (adjusted > 0)
    {
        text += '+';
    }
    text += std::format("{}", adjusted);
    return text;
}

std::int64_t BigDecimal::longValue() const noexcept
{
    // The low 64 bits of the integer part, accumulated modulo 2^64.
    const auto    length = static_cast<std::int64_t>(m_digits.size());
    std::uint64_t bits   = 0;
    if (m_scale >= 0)
    {
        // The last scale digits are the fraction; nothing is left of a value below one.
        const std::int64_t integerDigits = length - m_scale;
        for (std::int64_t i = 0; i < integerDigits; ++i)
        {
            const char digit = m_digits[static_cast<std::size_t>(i)];
            bits             = (bits * 10) + static_cast<std::uint64_t>(digit - '0');
        }
    }
    else
    {
        // 10^64 is a multiple of 2^64, so from 64 trailing zeros on every low bit is zero (Java
        // returns 0 there, too).
        if (m_scale <= -64)
        {
            return 0;
        }
        for (const char digit : m_digits)
        {
            bits = (bits * 10) + static_cast<std::uint64_t>(digit - '0');
        }
        for (std::int32_t i = 0; i < -m_scale; ++i)
        {
            bits *= 10;
        }
    }
    if (m_negative)
    {
        bits = 0 - bits;  // two's complement
    }
    return static_cast<std::int64_t>(bits);
}

std::int32_t BigDecimal::intValue() const noexcept
{
    // (int) longValue(): the low 32 bits.
    return static_cast<std::int32_t>(
        static_cast<std::uint32_t>(static_cast<std::uint64_t>(longValue())));
}

double BigDecimal::doubleValue() const
{
    if (signum() == 0)
    {
        return 0.0;
    }
    // Far outside the double range the exponent alone decides, whatever it takes to write it.
    const std::int64_t adjusted = adjustedExponent();
    if (adjusted > kDoubleOverflowExponent)
    {
        return m_negative ? -std::numeric_limits<double>::infinity()
                          : std::numeric_limits<double>::infinity();
    }
    if (adjusted < kDoubleUnderflowExponent)
    {
        return m_negative ? -0.0 : 0.0;
    }
    // Java: Double.parseDouble(toString()), or one exact division or multiplication of two
    // doubles for a short unscaled value; both are the correctly rounded value.
    const std::optional<double> value = Strings::javaParseDouble(toString());
    if (!value.has_value())
    {
        bug("BigDecimal::toString() gave a text that is not a number");
    }
    return *value;
}

}  // namespace QtRocket
