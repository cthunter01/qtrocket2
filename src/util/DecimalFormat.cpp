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
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// DecimalFormat.DOUBLE_INTEGER_DIGITS: the integer digits a double can need, to which
/// doubleSubformat clamps the maximum.
constexpr int kDoubleIntegerDigits = 309;
/// DecimalFormat.DOUBLE_FRACTION_DIGITS, the same for the fraction digits.
constexpr int kDoubleFractionDigits = 340;

/// DecimalFormatSymbols.getInfinity() (U+221E).
constexpr std::string_view kInfinity = "\xE2\x88\x9E";

/// java.text.DigitList as DecimalFormat fills it from a double: the value is
/// 0.<digits> * 10^decimalAt (digits holds DigitList's first count characters).
struct DigitList
{
    std::string digits;
    int         decimalAt{0};

    [[nodiscard]] int count() const noexcept { return static_cast<int>(digits.size()); }

    [[nodiscard]] char at(int i) const { return digits.at(static_cast<std::size_t>(i)); }

    /// DigitList.isZero: no digit other than '0'.
    [[nodiscard]] bool isZero() const
    {
        return std::ranges::all_of(digits, [](char c) { return c == '0'; });
    }

    /// Drops the trailing zeros, keeping at least one digit.
    void trimTrailingZeros()
    {
        while (digits.size() > 1 && digits.back() == '0')
        {
            digits.pop_back();
        }
    }
};

/// DigitList.shouldRoundUp for RoundingMode.HALF_EVEN: whether keeping @p maximumDigits digits
/// increments the last kept one. A '5' that is the last digit is a tie only when FloatingDecimal
/// flagged its digits exact; digits it rounded up stay down and truncated digits go up.
[[nodiscard]] bool shouldRoundUp(const DigitList& list, int maximumDigits, bool alreadyRounded,
                                 bool valueExactAsDecimal)
{
    if (maximumDigits >= list.count())
    {
        return false;
    }
    const char digit = list.at(maximumDigits);
    if (digit != '5')
    {
        return digit > '5';
    }
    if (maximumDigits == list.count() - 1)
    {
        // The rounding position is exactly the last index. If FloatingDecimal rounded up (value
        // was below tie), then we should not round up again. Otherwise if the digits don't
        // represent exact value, value was above tie and FloatingDecimal truncated digits to tie:
        // we must round up. An exact tie rounds to even.
        if (alreadyRounded)
        {
            return false;
        }
        if (!valueExactAsDecimal)
        {
            return true;
        }
        return maximumDigits > 0 && ((list.at(maximumDigits - 1) - '0') % 2) != 0;
    }
    // Rounds up if it gives a non null digit after '5'
    return std::ranges::any_of(
        std::string_view(list.digits).substr(static_cast<std::size_t>(maximumDigits) + 1),
        [](char c) { return c != '0'; });
}

/// DigitList.round: keeps at most @p maximumDigits digits, rounding the rest away, then drops
/// trailing zeros. A carry out of the leading digit gives "1" and moves the decimal point.
void round(DigitList& list, int maximumDigits, bool alreadyRounded, bool valueExactAsDecimal)
{
    if (maximumDigits < 0 || maximumDigits >= list.count())
    {
        return;
    }
    if (shouldRoundUp(list, maximumDigits, alreadyRounded, valueExactAsDecimal))
    {
        // Rounding up involves incrementing digits from LSD to MSD; all 9's become a single one
        // and a larger exponent.
        int i = maximumDigits - 1;
        while (i >= 0 && list.at(i) == '9')
        {
            --i;
        }
        if (i < 0)
        {
            list.digits[0] = '1';
            ++list.decimalAt;
            maximumDigits = 1;
        }
        else
        {
            ++list.digits[static_cast<std::size_t>(i)];
            maximumDigits = i + 1;
        }
    }
    list.digits.resize(static_cast<std::size_t>(maximumDigits));
    list.trimTrailingZeros();
}

/// The parsing half of DigitList.set(isNegative, String, ...): the digits of a Java-format
/// number ("980.0", "0.0015", "5.0E-6") from the first non-zero one, trailing zeros included, and
/// the decimal point's position.
[[nodiscard]] DigitList parseJavaFormat(std::string_view text)
{
    DigitList list;
    int       decimalAt = -1;
    int       exponent  = 0;
    // Number of zeros between decimal point and first non-zero digit after decimal point, for
    // numbers < 1.
    int  leadingZerosAfterDecimal = 0;
    bool nonZeroDigitSeen         = false;
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        const char c = text[i];
        if (c == '.')
        {
            decimalAt = list.count();
        }
        else if (c == 'e' || c == 'E')
        {
            exponent = Strings::parseInt(text.substr(i + 1)).value_or(0);
            break;
        }
        else if (nonZeroDigitSeen || c != '0')
        {
            nonZeroDigitSeen = true;
            list.digits.push_back(c);
        }
        else if (decimalAt != -1)
        {
            ++leadingZerosAfterDecimal;
        }
    }
    if (decimalAt == -1)
    {
        decimalAt = list.count();
    }
    if (nonZeroDigitSeen)
    {
        decimalAt += exponent - leadingZerosAfterDecimal;
    }
    list.decimalAt = decimalAt;
    return list;
}

/// DigitList.set(isNegative, source, maximumDigits, fixedPoint) for a non-negative finite
/// @p source: FloatingDecimal's digits, parsed back from its Java-format string (which keeps the
/// trailing zero of forms such as "5.0E-6", looked at when rounding into the first digit), rounded
/// to @p maximumDigits fraction digits (@p fixedPoint) or significant digits.
[[nodiscard]] DigitList toDigitList(double source, int maximumDigits, bool fixedPoint)
{
    const FloatingDecimal::BinaryToAscii converted = FloatingDecimal::binaryToAscii(source);
    const bool                           roundedUp = converted.digitsRoundedUp;
    const bool                           valueExactAsDecimal = converted.decimalDigitsExact;

    DigitList list = parseJavaFormat(FloatingDecimal::toJavaFormatString(converted));
    if (fixedPoint)
    {
        // The negative of the exponent represents the number of leading zeros between the decimal
        // and the first non-zero digit, for a value < 0.1 (e.g., for 0.00123, -decimalAt == 2).
        // If this is more than the maximum fraction digits, then we have an underflow for the
        // printed representation, as when 0.0009 is rounded to 2 fraction digits.
        if (-list.decimalAt > maximumDigits)
        {
            list.digits.clear();
            return list;
        }
        // If we round 0.0009 to 3 fractional digits, then we have to create a new one digit in
        // the least significant location, or none.
        if (-list.decimalAt == maximumDigits)
        {
            const bool up = shouldRoundUp(list, 0, roundedUp, valueExactAsDecimal);
            list.digits   = up ? "1" : "";
            list.decimalAt += up ? 1 : 0;
            return list;
        }
    }
    list.trimTrailingZeros();
    // Eliminate digits beyond maximum digits to be displayed. Round up if appropriate.
    round(list, fixedPoint ? (maximumDigits + list.decimalAt) : maximumDigits, roundedUp,
          valueExactAsDecimal);
    return list;
}

/// The digit counts of a pattern's number part, as DecimalFormat.applyPattern's phase 1 records
/// them for "####0000.####E00".
struct PatternCounts
{
    int  digitLeftCount{0};
    int  zeroDigitCount{0};
    int  digitRightCount{0};
    int  decimalPos{-1};
    bool useExponentialNotation{false};
    int  minExponentDigits{0};
};

/// Adds the pattern character @p ch to @p counts; false when it is not a digit or the decimal
/// separator.
[[nodiscard]] bool addPatternCharacter(PatternCounts& counts, char ch, std::string_view pattern)
{
    switch (ch)
    {
        case '#':
            ++(counts.zeroDigitCount > 0 ? counts.digitRightCount : counts.digitLeftCount);
            return true;
        case '0':
            if (counts.digitRightCount > 0)
            {
                bug(std::format("Unexpected '0' in pattern \"{}\"", pattern));
            }
            ++counts.zeroDigitCount;
            return true;
        case '.':
            if (counts.decimalPos >= 0)
            {
                bug(std::format("Multiple decimal separators in pattern \"{}\"", pattern));
            }
            counts.decimalPos =
                counts.digitLeftCount + counts.zeroDigitCount + counts.digitRightCount;
            return true;
        default:
            return false;
    }
}

/// Counts the digits of @p pattern, rejecting what Java rejects and what the port does not
/// support (affixes, grouping, percent, quotes, ';' subpatterns and the empty pattern).
[[nodiscard]] PatternCounts scanPattern(std::string_view pattern)
{
    PatternCounts counts;
    std::size_t   pos = 0;
    while (pos < pattern.size() && addPatternCharacter(counts, pattern[pos], pattern))
    {
        ++pos;
    }
    if (pos < pattern.size() && pattern[pos] == 'E')
    {
        // The exponent: 'E' and its minimum digits, all zeros.
        const std::size_t zeros =
            std::min(pattern.find_first_not_of('0', pos + 1), pattern.size()) - (pos + 1);
        counts.useExponentialNotation = true;
        counts.minExponentDigits      = static_cast<int>(zeros);
        pos += 1 + zeros;
        if ((counts.digitLeftCount + counts.zeroDigitCount) < 1 || zeros < 1)
        {
            bug(std::format("Malformed exponential pattern \"{}\"", pattern));
        }
    }
    if (pattern.empty() || pos < pattern.size())
    {
        bug(std::format("Unsupported DecimalFormat pattern \"{}\"", pattern));
    }
    return counts;
}

/// The settings DecimalFormat.subformatNumber reads.
struct Layout
{
    int  minIntDigits{0};
    int  maxIntDigits{0};
    int  minFraDigits{0};
    int  maxFraDigits{0};
    bool decimalSeparatorAlwaysShown{false};
    int  minExponentDigits{0};
};

/// subformatNumber's exponential branch: the digits with the point after the integer digits, "E",
/// and the exponent with at least minExponentDigits digits. Maximum integer digits above the
/// minimum define a repeating range for the exponent (engineering notation).
void appendExponential(std::string& result, const DigitList& list, const Layout& layout)
{
    int exponent             = list.decimalAt;
    int minimumIntegerDigits = layout.minIntDigits;
    if (const int repeat = layout.maxIntDigits; repeat > 1 && repeat > layout.minIntDigits)
    {
        // A repeating range is defined; adjust to it as follows. If repeat == 3, we have
        // 6,5,4=>3; 3,2,1=>0; 0,-1,-2=>-3; -3,-4,-5=>-6, etc. (integer division rounds towards
        // 0).
        exponent             = exponent >= 1 ? ((exponent - 1) / repeat) * repeat
                                             : ((exponent - repeat) / repeat) * repeat;
        minimumIntegerDigits = 1;
    }
    else
    {
        // No repeating range is defined; use minimum integer digits.
        exponent -= minimumIntegerDigits;
    }

    // We output a minimum number of digits, and more if there are more digits, up to the maximum.
    // The number of integer digits is handled specially if the number is zero, since then there
    // may be no digits.
    const bool zero          = list.isZero();
    const int  integerDigits = zero ? minimumIntegerDigits : list.decimalAt - exponent;
    const int  minimumDigits = std::max(layout.minIntDigits + layout.minFraDigits, integerDigits);
    const int  totalDigits   = std::max(list.count(), minimumDigits);
    for (int i = 0; i < totalDigits; ++i)
    {
        if (i == integerDigits)
        {
            result += '.';
        }
        result += i < list.count() ? list.at(i) : '0';
    }
    if (layout.decimalSeparatorAlwaysShown && totalDigits == integerDigits)
    {
        result += '.';
    }
    // For zero values, we force the exponent to zero.
    exponent = zero ? 0 : exponent;
    result += exponent < 0 ? "E-" : "E";
    result += std::format("{:0{}}", std::abs(exponent), layout.minExponentDigits);
}

/// subformatNumber's fixed-point branch: the integer digits (a zero when there would be none),
/// the point when there is a fraction, and the fraction digits between the minimum and maximum.
void appendFixedPoint(std::string& result, const DigitList& list, const Layout& layout)
{
    // The integer digits: leading zeros for the minimum, then the digits before the point. When
    // there are more than the maximum, the least significant ones are output.
    const int count        = std::max(layout.minIntDigits, list.decimalAt);
    const int integerCount = std::min(count, layout.maxIntDigits);
    int       digitIndex   = count > layout.maxIntDigits ? list.decimalAt - integerCount : 0;
    for (int i = integerCount - 1; i >= 0; --i)
    {
        const bool realDigit = i < list.decimalAt && digitIndex < list.count();
        result += realDigit ? list.at(digitIndex++) : '0';
    }

    // Whether there are any printable fractional digits; if not, and no integer digit was
    // printed, print a zero.
    const bool fractionPresent = layout.minFraDigits > 0 || digitIndex < list.count();
    if (!fractionPresent && integerCount == 0)
    {
        result += '0';
    }
    if (layout.decimalSeparatorAlwaysShown || fractionPresent)
    {
        result += '.';
    }
    for (int i = 0; i < layout.maxFraDigits; ++i)
    {
        // Stop once the minimum digits are output and the significant digits are used up.
        if (i >= layout.minFraDigits && digitIndex >= list.count())
        {
            break;
        }
        // Leading fractional zeros (before any significant digit), then a digit if we have any
        // precision left, or a zero if we don't.
        const bool leadingZero = -1 - i > list.decimalAt - 1;
        result += !leadingZero && digitIndex < list.count() ? list.at(digitIndex++) : '0';
    }
}

}  // namespace

DecimalFormat::DecimalFormat(std::string_view pattern)
{
    PatternCounts c = scanPattern(pattern);

    // Handle patterns with no '0' pattern character. These patterns are legal, but must be
    // interpreted. "##.###" -> "#0.###". ".###" -> ".0##".
    if (c.zeroDigitCount == 0 && c.digitLeftCount > 0 && c.decimalPos >= 0)
    {
        // Handle "###.###" and "###." and ".###"
        const int n       = std::max(c.decimalPos, 1);
        c.digitRightCount = c.digitLeftCount - n;
        c.digitLeftCount  = n - 1;
        c.zeroDigitCount  = 1;
    }

    // Do syntax checking on the digits.
    if ((c.decimalPos < 0 && c.digitRightCount > 0) ||
        (c.decimalPos >= 0 &&
         (c.decimalPos < c.digitLeftCount || c.decimalPos > (c.digitLeftCount + c.zeroDigitCount))))
    {
        bug(std::format("Malformed pattern \"{}\"", pattern));
    }

    const int digitTotalCount = c.digitLeftCount + c.zeroDigitCount + c.digitRightCount;
    // The effectiveDecimalPos is the position the decimal is at or would be at if there is no
    // decimal.
    const int effectiveDecimalPos = c.decimalPos >= 0 ? c.decimalPos : digitTotalCount;
    m_useExponentialNotation      = c.useExponentialNotation;
    m_minExponentDigits           = c.minExponentDigits;
    m_minIntegerDigits            = effectiveDecimalPos - c.digitLeftCount;
    m_maxIntegerDigits            = m_useExponentialNotation ? c.digitLeftCount + m_minIntegerDigits
                                                             : std::numeric_limits<int>::max();
    m_maxFractionDigits           = c.decimalPos >= 0 ? (digitTotalCount - c.decimalPos) : 0;
    m_minFractionDigits =
        c.decimalPos >= 0 ? (c.digitLeftCount + c.zeroDigitCount - c.decimalPos) : 0;
    m_decimalSeparatorAlwaysShown = c.decimalPos == 0 || c.decimalPos == digitTotalCount;
}

std::string DecimalFormat::format(double value) const
{
    if (std::isnan(value))
    {
        return "NaN";
    }
    // Negative zero is negative, and a negative value that rounds to zero stays negative.
    std::string result = std::signbit(value) ? "-" : "";
    if (std::isinf(value))
    {
        return result + std::string(kInfinity);
    }

    // doubleSubformat reads the digit counts as NumberFormat clamps them for doubles.
    const Layout layout{
        .minIntDigits                = std::min(m_minIntegerDigits, kDoubleIntegerDigits),
        .maxIntDigits                = std::min(m_maxIntegerDigits, kDoubleIntegerDigits),
        .minFraDigits                = std::min(m_minFractionDigits, kDoubleFractionDigits),
        .maxFraDigits                = std::min(m_maxFractionDigits, kDoubleFractionDigits),
        .decimalSeparatorAlwaysShown = m_decimalSeparatorAlwaysShown,
        .minExponentDigits           = m_minExponentDigits,
    };
    DigitList list = toDigitList(
        std::abs(value),
        m_useExponentialNotation ? layout.maxIntDigits + layout.maxFraDigits : layout.maxFraDigits,
        !m_useExponentialNotation);
    if (list.isZero())
    {
        list.decimalAt = 0;  // Normalize
    }
    if (m_useExponentialNotation)
    {
        appendExponential(result, list, layout);
    }
    else
    {
        appendFixedPoint(result, list, layout);
    }
    return result;
}

}  // namespace QtRocket
