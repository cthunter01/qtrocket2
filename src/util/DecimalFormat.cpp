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
/// 0.<digits> * 10^pointPosition. Zero has no digits and the point position 0; otherwise the
/// first digit is not a zero.
struct DecimalDigits
{
    std::string digits;
    int         pointPosition{0};

    [[nodiscard]] int count() const noexcept { return static_cast<int>(digits.size()); }

    [[nodiscard]] bool isZero() const noexcept { return digits.empty(); }

    /// The digit of 10^@p power, '0' where the number has none.
    [[nodiscard]] char digitOfPower(int power) const
    {
        const int index = pointPosition - 1 - power;
        return index >= 0 && index < count() ? digits[static_cast<std::size_t>(index)] : '0';
    }

    /// The power of ten of the last digit, the point position for zero.
    [[nodiscard]] int lastPower() const noexcept { return pointPosition - count(); }

    void dropTrailingZeros()
    {
        const std::size_t last = digits.find_last_not_of('0');
        digits.resize(last == std::string::npos ? 0 : last + 1);
    }

    void becomeZero()
    {
        digits.clear();
        pointPosition = 0;
    }
};

/// The number DecimalFormat rounds, with FloatingDecimal's two flags, which tell where the double
/// lies relative to its digits.
struct StartingDigits
{
    DecimalDigits number;
    bool          convertedUp{false};
    bool          convertedExact{false};
};

/// JDK 17's DecimalFormat starts from FloatingDecimal's digits in the compatible format (not the
/// shortest ones), which it reads back from Double.toString's text. That text adds zeros (up to
/// the point and one after it for a whole number below 10^7, "980.0", and a second digit in
/// E-form), but they never change a result, so they are left out here. Rounding drops trailing
/// zeros before it cuts whenever it keeps at least one digit. It keeps none only for a number
/// below one unit of the last place kept (point position -maxFraction, at most 0), where the text
/// adds a zero only in E-form, below 10^-3, and there the conversion has already forced a second
/// digit ("50" for 5e-6).
[[nodiscard]] StartingDigits startingDigits(double magnitude)
{
    const FloatingDecimal::BinaryToAscii converted = FloatingDecimal::binaryToAscii(magnitude);
    StartingDigits                       start{.number         = {},
                                               .convertedUp    = converted.digitsRoundedUp,
                                               .convertedExact = converted.decimalDigitsExact};
    if (converted.digits != "0")
    {
        start.number.digits        = converted.digits;
        start.number.pointPosition = converted.decimalExponent;
    }
    return start;
}

/// Where the digits from index @p keep on lie against half a unit of the last kept digit: -1
/// below, 0 at, 1 above, for the double these digits stand for, as JDK 17 decides it. A first
/// dropped digit other than 5 decides alone. After a 5, any non-zero digit is above, and zeros
/// alone count as half a unit; they occur only when nothing is kept, as the conversion's own
/// trailing zeros, and JDK 17 does not look further there: with "0.#####" it formats 5e-6, whose
/// double lies above 5e-6 and whose digits are "50", as "0". A 5 that is the last digit is half a
/// unit in the digits only: the double is there when the conversion was exact, below when the
/// conversion rounded up to the 5, and above otherwise.
[[nodiscard]] int compareDroppedWithHalf(const StartingDigits& start, std::size_t keep)
{
    const std::string& digits = start.number.digits;
    const char         first  = digits[keep];
    if (first != '5')
    {
        return first < '5' ? -1 : 1;
    }
    if (keep + 1 < digits.size())
    {
        return digits.find_first_not_of('0', keep + 1) == std::string::npos ? 0 : 1;
    }
    if (start.convertedExact)
    {
        return 0;
    }
    return start.convertedUp ? -1 : 1;
}

/// Keeps the first @p keep digits of the number (@p keep >= 0), rounding the double it stands for
/// with RoundingMode.HALF_EVEN: up above half a unit of the last kept digit, and at half a unit
/// only to an even digit (zero counting as even when nothing is kept). A carry out of the first
/// digit leaves "1" one place higher; trailing zeros are dropped.
void keepRounded(StartingDigits& start, int keep)
{
    DecimalDigits& number = start.number;
    if (keep >= number.count())
    {
        return;
    }
    auto       length  = static_cast<std::size_t>(keep);
    const int  dropped = compareDroppedWithHalf(start, length);
    const bool odd     = length > 0 && (number.digits[length - 1] - '0') % 2 != 0;
    if (dropped > 0 || (dropped == 0 && odd))
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
    if (number.isZero())
    {
        number.becomeZero();
    }
}

/// Rounds the number half to even, as JDK 17 does, to @p maxFraction fraction digits.
void roundToFractionDigits(StartingDigits& start, int maxFraction)
{
    const int keep = maxFraction + start.number.pointPosition;
    if (keep < 0)
    {
        start.number.becomeZero();  // below half of the last place
        return;
    }
    if (keep > 0)
    {
        start.number.dropTrailingZeros();
    }
    keepRounded(start, keep);
}

/// Rounds the number half to even, as JDK 17 does, to @p maxDigits significant digits.
void roundToSignificantDigits(StartingDigits& start, int maxDigits)
{
    start.number.dropTrailingZeros();
    keepRounded(start, maxDigits);
}

/// floor(@p numerator / @p denominator) for a positive denominator.
[[nodiscard]] int floorDivide(int numerator, int denominator)
{
    const int quotient = numerator / denominator;
    return (numerator % denominator != 0 && numerator < 0) ? quotient - 1 : quotient;
}

/// The digit counts in effect for a double: the pattern's, within the limits above.
struct DigitCounts
{
    int  minInteger{0};
    int  maxInteger{0};
    int  minFraction{0};
    int  maxFraction{0};
    bool pointAlwaysShown{false};
    int  minExponentDigits{0};

    [[nodiscard]] DigitCounts limitedForADouble() const
    {
        DigitCounts limited = *this;
        limited.minInteger  = std::min(minInteger, kIntegerDigitLimit);
        limited.maxInteger  = std::min(maxInteger, kIntegerDigitLimit);
        limited.minFraction = std::min(minFraction, kFractionDigitLimit);
        limited.maxFraction = std::min(maxFraction, kFractionDigitLimit);
        return limited;
    }
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
/// mantissa has at least the minimum integer plus minimum fraction digits, padded with zeros.
/// Zero has the exponent 0 and, before the point, one digit or the minimum integer digits.
void writeScientific(std::string& text, const DecimalDigits& number, const DigitCounts& counts)
{
    const bool repeating   = counts.maxInteger > 1 && counts.maxInteger > counts.minInteger;
    int        exponent    = 0;
    int        beforePoint = repeating ? 1 : counts.minInteger;
    if (!number.isZero())
    {
        exponent =
            repeating ? floorDivide(number.pointPosition - 1, counts.maxInteger) * counts.maxInteger
                      : number.pointPosition - counts.minInteger;
        beforePoint = number.pointPosition - exponent;
    }
    std::string mantissa = number.digits;
    mantissa.resize(static_cast<std::size_t>(std::max(
                        {number.count(), counts.minInteger + counts.minFraction, beforePoint})),
                    '0');
    const auto point = static_cast<std::size_t>(beforePoint);
    if (point < mantissa.size())
    {
        mantissa.insert(point, 1, '.');
    }
    else if (counts.pointAlwaysShown)
    {
        mantissa += '.';
    }
    text += mantissa;
    text += std::format("E{}{:0{}}", exponent < 0 ? "-" : "", std::abs(exponent),
                        counts.minExponentDigits);
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
    const DigitCounts counts = DigitCounts{.minInteger        = m_minIntegerDigits,
                                           .maxInteger        = m_maxIntegerDigits,
                                           .minFraction       = m_minFractionDigits,
                                           .maxFraction       = m_maxFractionDigits,
                                           .pointAlwaysShown  = m_decimalSeparatorAlwaysShown,
                                           .minExponentDigits = m_minExponentDigits}
                                   .limitedForADouble();
    StartingDigits    start  = startingDigits(std::abs(value));
    if (m_useExponentialNotation)
    {
        roundToSignificantDigits(start, counts.maxInteger + counts.maxFraction);
        writeScientific(text, start.number, counts);
    }
    else
    {
        roundToFractionDigits(start, counts.maxFraction);
        writePlain(text, start.number, counts);
    }
    return text;
}

}  // namespace QtRocket
