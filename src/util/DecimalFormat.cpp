#include "QtRocket/util/DecimalFormat.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <string>
#include <string_view>

#include "QtRocket/util/BugError.h"
#include "QtRocket/util/FloatingDecimal.h"

// java.text.DecimalFormat's formatting of a double, for the patterns described in DecimalFormat.h,
// written from its documentation (the class description: pattern syntax, scientific notation,
// special values; RoundingMode.HALF_EVEN; the limits of 309 integer and 340 fraction digits for a
// double in setMaximumIntegerDigits and setMaximumFractionDigits) and from JDK 17's output, which
// the tests pin. Where JDK 17's results go beyond the documentation (the digits it starts from, how
// it decides a tie) the comments below say so.

namespace QtRocket
{

namespace
{

/// The most integer and fraction digits DecimalFormat writes for a double, whatever the pattern.
constexpr int kIntegerDigitLimit  = 309;
constexpr int kFractionDigitLimit = 340;

/// DecimalFormatSymbols.getInfinity() of Locale.US: U+221E.
constexpr std::string_view kInfinitySign = "\xE2\x88\x9E";

/// A decimal number as its significant digits and the place of the point: the value is
/// 0.<digits> * 10^pointPosition. Zero has no digits.
struct DecimalDigits
{
    std::string digits;
    int         pointPosition{0};

    [[nodiscard]] int count() const noexcept { return static_cast<int>(digits.size()); }

    /// The digit of 10^@p power, '0' where the number has none.
    [[nodiscard]] char digitOfPower(int power) const
    {
        const int index = pointPosition - 1 - power;
        return index >= 0 && index < count() ? digits[static_cast<std::size_t>(index)] : '0';
    }

    /// The power of ten of the last digit, the point position for zero.
    [[nodiscard]] int lastPower() const noexcept { return pointPosition - count(); }

    [[nodiscard]] bool isZero() const { return digits.find_first_not_of('0') == std::string::npos; }

    void dropTrailingZeros()
    {
        const std::size_t last = digits.find_last_not_of('0');
        digits.resize(last == std::string::npos ? std::min<std::size_t>(digits.size(), 1)
                                                : last + 1);
    }
};

/// The digits JDK 17's DecimalFormat starts from, with FloatingDecimal's two flags, which tell
/// where the double lies relative to them.
struct StartingDigits
{
    DecimalDigits number;
    bool          convertedUp{false};
    bool          convertedExact{false};
};

/// The digits of FloatingDecimal's conversion (not the shortest ones) as Double.toString writes
/// them, from the first non-zero digit on, because JDK 17 reads them back from that text. The text
/// adds zeros: up to the point and one after it for a whole number below 10^7 ("980.0"), and a
/// second digit in E-form ("5.0E-6"). The second one matters when the value is rounded just before
/// its first digit, see roundsUp(); the others are dropped before rounding.
[[nodiscard]] StartingDigits startingDigits(double magnitude)
{
    const FloatingDecimal::BinaryToAscii converted = FloatingDecimal::binaryToAscii(magnitude);
    StartingDigits                       start;
    start.convertedUp     = converted.digitsRoundedUp;
    start.convertedExact  = converted.decimalDigitsExact;
    DecimalDigits& number = start.number;
    if (converted.digits == "0")
    {
        return start;
    }
    number.digits        = converted.digits;
    number.pointPosition = converted.decimalExponent;
    const int exponent   = number.pointPosition;
    if (exponent >= 1 && exponent <= 7)
    {
        number.digits.append(static_cast<std::size_t>(std::max(0, exponent - number.count())), '0');
        if (number.count() == exponent)
        {
            number.digits += '0';
        }
    }
    else if ((exponent < -2 || exponent > 7) && number.count() == 1)
    {
        number.digits += '0';
    }
    return start;
}

/// Whether keeping the first @p keep digits rounds the kept part up, under RoundingMode.HALF_EVEN
/// applied to the double's binary value as JDK 17 decides it from the digits and the two flags of
/// the conversion. A first dropped digit other than 5 decides alone. After a 5, any non-zero digit
/// means above the tie; zeros alone (which occur only when nothing is kept, the zeros coming from
/// the text) count as a tie, rounded to the even zero. A 5 that is the last digit is a tie of the
/// digits only: the double lies below it when the conversion rounded up to it, above it when the
/// conversion was not exact, and only an exact tie goes to the even neighbour.
[[nodiscard]] bool roundsUp(const DecimalDigits& number, int keep, bool convertedUp,
                            bool convertedExact)
{
    if (keep >= number.count())
    {
        return false;
    }
    const auto cut   = static_cast<std::size_t>(keep);
    const char first = number.digits[cut];
    if (first != '5')
    {
        return first > '5';
    }
    if (cut + 1 < number.digits.size())
    {
        return number.digits.find_first_not_of('0', cut + 1) != std::string::npos;
    }
    if (convertedUp)
    {
        return false;
    }
    if (!convertedExact)
    {
        return true;
    }
    return cut > 0 && (number.digits[cut - 1] - '0') % 2 != 0;
}

/// Keeps the first @p keep digits (@p keep >= 0), adding one to the last kept digit when @p up,
/// and drops trailing zeros. A carry out of the first digit leaves "1" one place higher.
void keepDigits(DecimalDigits& number, int keep, bool up)
{
    if (keep >= number.count())
    {
        return;
    }
    auto length = static_cast<std::size_t>(keep);
    if (up)
    {
        while (length > 0 && number.digits[length - 1] == '9')
        {
            --length;
        }
        if (length == 0)
        {
            number.digits = "1";
            ++number.pointPosition;
            return;
        }
        ++number.digits[length - 1];
    }
    number.digits.resize(length);
    number.dropTrailingZeros();
}

/// Rounds @p number half to even, as JDK 17 does, to @p maxFraction fraction digits.
void roundToFractionDigits(DecimalDigits& number, int maxFraction, bool convertedUp,
                           bool convertedExact)
{
    const int keep = maxFraction + number.pointPosition;
    if (keep < 0)
    {
        number.digits.clear();  // below half of the last place: zero
        return;
    }
    if (keep > 0)
    {
        number.dropTrailingZeros();
    }
    keepDigits(number, keep, roundsUp(number, keep, convertedUp, convertedExact));
}

/// Rounds @p number half to even, as JDK 17 does, to @p maxDigits significant digits.
void roundToSignificantDigits(DecimalDigits& number, int maxDigits, bool convertedUp,
                              bool convertedExact)
{
    number.dropTrailingZeros();
    keepDigits(number, maxDigits, roundsUp(number, maxDigits, convertedUp, convertedExact));
}

/// floor(@p numerator / @p denominator) for a positive denominator.
[[nodiscard]] int floorDivide(int numerator, int denominator)
{
    const int quotient = numerator / denominator;
    return (numerator % denominator != 0 && numerator < 0) ? quotient - 1 : quotient;
}

/// The digit counts in effect for a double (the pattern's, within the limits above).
struct DigitCounts
{
    int  minInteger{0};
    int  maxInteger{0};
    int  minFraction{0};
    int  maxFraction{0};
    bool pointAlwaysShown{false};
    int  minExponentDigits{0};
};

/// Plain notation: the integer digits, at least minInteger of them (leading zeros) and no more
/// than maxInteger (the lowest ones), a zero when there would be none and no fraction follows;
/// then the fraction digits, at least minFraction of them, more while significant digits remain,
/// no more than maxFraction.
void writePlain(std::string& text, const DecimalDigits& number, const DigitCounts& counts)
{
    const int integerDigits =
        std::min(std::max(counts.minInteger, number.pointPosition), counts.maxInteger);
    for (int power = integerDigits - 1; power >= 0; --power)
    {
        text += number.digitOfPower(power);
    }
    const bool fraction = counts.minFraction > 0 || (number.count() > 0 && number.lastPower() < 0);
    if (integerDigits == 0 && !fraction)
    {
        text += '0';
    }
    if (fraction || counts.pointAlwaysShown)
    {
        text += '.';
    }
    for (int place = 1; place <= counts.maxFraction; ++place)
    {
        if (place > counts.minFraction && -place < number.lastPower())
        {
            break;
        }
        text += number.digitOfPower(-place);
    }
}

/// Scientific notation. With more maximum than minimum integer digits, and more than one, the
/// exponent is a multiple of the maximum and one to that many digits come before the point
/// ("##0.##E0": 12345 is "12.345E3"); otherwise the minimum integer digits come before it. The
/// digits written are at least the minimum integer plus minimum fraction digits. Zero has the
/// exponent 0.
void writeScientific(std::string& text, const DecimalDigits& number, const DigitCounts& counts)
{
    const bool repeating = counts.maxInteger > 1 && counts.maxInteger > counts.minInteger;
    const int  exponent =
        repeating ? floorDivide(number.pointPosition - 1, counts.maxInteger) * counts.maxInteger
                  : number.pointPosition - counts.minInteger;
    const bool zero         = number.isZero();
    const int  zeroIntegers = repeating ? 1 : counts.minInteger;
    const int  beforePoint  = zero ? zeroIntegers : number.pointPosition - exponent;
    const int  written =
        std::max({number.count(), counts.minInteger + counts.minFraction, beforePoint});
    for (int i = 0; i < written; ++i)
    {
        if (i == beforePoint)
        {
            text += '.';
        }
        text += i < number.count() ? number.digits[static_cast<std::size_t>(i)] : '0';
    }
    if (counts.pointAlwaysShown && written == beforePoint)
    {
        text += '.';
    }
    const int shownExponent = zero ? 0 : exponent;
    text += shownExponent < 0 ? "E-" : "E";
    text += std::format("{:0{}}", std::abs(shownExponent), counts.minExponentDigits);
}

/// The parts of a pattern: integer digits '#'* '0'*, then optionally '.' and fraction digits
/// '0'* '#'*, then optionally 'E' and one or more '0'.
struct PatternParts
{
    std::string_view integer;
    std::string_view fraction;
    bool             point{false};
    bool             exponent{false};
    int              exponentDigits{0};
};

/// Whether @p digits is @p first repeated, then @p second repeated.
[[nodiscard]] bool isRun(std::string_view digits, char first, char second)
{
    const std::size_t split = digits.find_first_not_of(first);
    return split == std::string_view::npos ||
           digits.find_first_not_of(second, split) == std::string_view::npos;
}

/// Splits @p pattern into its parts, rejecting what Java rejects (a '0' after an optional
/// fraction '#', '#' after a required integer '0' without a point between, two points, an
/// exponent without digits on either side) and what is outside the subset (prefixes, suffixes,
/// grouping, percent, quotes, negative subpatterns, the empty pattern).
[[nodiscard]] PatternParts splitPattern(std::string_view pattern)
{
    if (pattern.empty())
    {
        bug("Unsupported DecimalFormat pattern \"\"");
    }
    PatternParts      parts;
    std::string_view  number = pattern;
    const std::size_t e      = pattern.find('E');
    if (e != std::string_view::npos)
    {
        const std::string_view digits = pattern.substr(e + 1);
        if (digits.empty() || digits.find_first_not_of('0') != std::string_view::npos ||
            pattern.substr(0, e).find_first_of("#0") == std::string_view::npos)
        {
            bug(std::format("Malformed or unsupported exponential pattern \"{}\"", pattern));
        }
        parts.exponent       = true;
        parts.exponentDigits = static_cast<int>(digits.size());
        number               = pattern.substr(0, e);
    }
    if (number.find_first_not_of("#0.") != std::string_view::npos)
    {
        bug(std::format("Unsupported DecimalFormat pattern \"{}\"", pattern));
    }
    const std::size_t point = number.find('.');
    parts.point             = point != std::string_view::npos;
    parts.integer           = number.substr(0, point);
    parts.fraction          = parts.point ? number.substr(point + 1) : std::string_view();
    if (parts.fraction.contains('.') || !isRun(parts.integer, '#', '0') ||
        !isRun(parts.fraction, '0', '#'))
    {
        bug(std::format("Malformed DecimalFormat pattern \"{}\"", pattern));
    }
    return parts;
}

}  // namespace

DecimalFormat::DecimalFormat(std::string_view pattern)
{
    const PatternParts parts          = splitPattern(pattern);
    const auto         integerZeros   = static_cast<int>(std::ranges::count(parts.integer, '0'));
    const auto         fractionZeros  = static_cast<int>(std::ranges::count(parts.fraction, '0'));
    const auto         integerLength  = static_cast<int>(parts.integer.size());
    const auto         fractionLength = static_cast<int>(parts.fraction.size());

    m_minIntegerDigits  = integerZeros;
    m_minFractionDigits = fractionZeros;
    if (parts.point && integerZeros == 0 && fractionZeros == 0)
    {
        // Without any '0', Java reads the '#' next to the point as one: "#.##" as "0.##" and
        // ".##" as ".0#".
        if (integerLength > 0)
        {
            m_minIntegerDigits = 1;
        }
        else if (fractionLength > 0)
        {
            m_minFractionDigits = 1;
        }
    }
    m_maxIntegerDigits       = parts.exponent ? integerLength : std::numeric_limits<int>::max();
    m_maxFractionDigits      = fractionLength;
    m_useExponentialNotation = parts.exponent;
    m_minExponentDigits      = parts.exponentDigits;
    m_decimalSeparatorAlwaysShown = parts.point && (integerLength == 0 || fractionLength == 0);
}

std::string DecimalFormat::format(double value) const
{
    if (std::isnan(value))
    {
        return "NaN";
    }
    // The sign of a negative value stays, also when it rounds to zero, and so does -0.0's.
    std::string text = std::signbit(value) ? "-" : "";
    if (std::isinf(value))
    {
        return text + std::string(kInfinitySign);
    }

    const DigitCounts counts{
        .minInteger        = std::min(m_minIntegerDigits, kIntegerDigitLimit),
        .maxInteger        = std::min(m_maxIntegerDigits, kIntegerDigitLimit),
        .minFraction       = std::min(m_minFractionDigits, kFractionDigitLimit),
        .maxFraction       = std::min(m_maxFractionDigits, kFractionDigitLimit),
        .pointAlwaysShown  = m_decimalSeparatorAlwaysShown,
        .minExponentDigits = m_minExponentDigits,
    };
    StartingDigits start  = startingDigits(std::abs(value));
    DecimalDigits& number = start.number;
    if (m_useExponentialNotation)
    {
        roundToSignificantDigits(number, counts.maxInteger + counts.maxFraction, start.convertedUp,
                                 start.convertedExact);
    }
    else
    {
        roundToFractionDigits(number, counts.maxFraction, start.convertedUp, start.convertedExact);
    }
    if (number.isZero())
    {
        number.pointPosition = 0;
    }
    if (m_useExponentialNotation)
    {
        writeScientific(text, number, counts);
    }
    else
    {
        writePlain(text, number, counts);
    }
    return text;
}

}  // namespace QtRocket
