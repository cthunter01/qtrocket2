#include "QtRocket/unit/Unit.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeinfo>
#include <utility>
#include <vector>

#include "QtRocket/unit/GeneralUnit.h"
#include "QtRocket/unit/Tick.h"
#include "QtRocket/unit/Value.h"
#include "QtRocket/util/Chars.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// DecimalFormatSymbols.getInfinity(): U+221E.
constexpr std::string_view kInfinity = "\xE2\x88\x9E";

[[nodiscard]] std::string infinityText(double value)
{
    return value < 0 ? "-" + std::string(kInfinity) : std::string(kInfinity);
}

// ---- java.text.DecimalFormat, as far as OpenRocket's patterns need it ----
//
// DecimalFormat formats a double by parsing Double.toString's digits into a DigitList, rounding
// that list HALF_EVEN at the pattern's last fraction digit (or third significant digit for
// "0.00E0"), and printing the digits with the pattern's minimum and maximum digit counts. The
// rounding of a last digit '5' depends on whether FloatingDecimal truncated the digits (the value
// is above them), rounded them up (below) or converted exactly (a true tie, to even); the code
// below reads that off the exact binary value instead.

/// java.text.DigitList: the digits of a magnitude, without leading zeros, and the position of
/// the decimal point (the value is 0.<digits> * 10^decimalAt). Empty digits are zero.
struct DigitList
{
    std::string digits;
    int         decimalAt{0};
};

/// DigitList.set(String) on Double.toString(magnitude): "980.0" gives {"9800", 3}, "5.0E-6"
/// gives {"50", -5}, "0.005" gives {"5", -2} and "0.0" gives {"", 0}. The trailing zeros stay,
/// as they matter to the underflow check.
[[nodiscard]] DigitList parseJavaDigits(std::string_view text)
{
    DigitList list;
    int       decimalAt                = -1;
    int       exponent                 = 0;
    int       leadingZerosAfterDecimal = 0;
    bool      nonZeroDigitSeen         = false;
    for (std::size_t i = 0; i < text.size(); i++)
    {
        const char c = text[i];
        if (c == '.')
        {
            decimalAt = static_cast<int>(list.digits.size());
        }
        else if (c == 'E' || c == 'e')
        {
            exponent = Strings::parseInt(text.substr(i + 1)).value_or(0);
            break;
        }
        else
        {
            if (!nonZeroDigitSeen)
            {
                nonZeroDigitSeen = c != '0';
                if (!nonZeroDigitSeen && decimalAt != -1)
                {
                    ++leadingZerosAfterDecimal;
                }
            }
            if (nonZeroDigitSeen)
            {
                list.digits += c;
            }
        }
    }
    if (decimalAt == -1)
    {
        decimalAt = static_cast<int>(list.digits.size());
    }
    list.decimalAt = nonZeroDigitSeen ? decimalAt + exponent - leadingZerosAfterDecimal : 0;
    return list;
}

/// DigitList's "Eliminate trailing zeros": down to one digit.
void trimTrailingZeros(DigitList& list)
{
    while (list.digits.size() > 1 && list.digits.back() == '0')
    {
        list.digits.pop_back();
    }
}

/// Whether FloatingDecimal converts @p magnitude on its "easy" long path: an integer from 1 up
/// to (excluding) 2^63. Those digits are flagged neither exact nor rounded up (a JDK quirk), so
/// DigitList rounds a tie at their last digit up instead of to even.
[[nodiscard]] bool isEasyPathInteger(double magnitude)
{
    constexpr double kTwoPow63 = 9223372036854775808.0;
    return magnitude >= 1.0 && magnitude < kTwoPow63 && magnitude == std::trunc(magnitude);
}

/// -1, 0 or 1 as the decimal 0.<digits> * 10^decimalAt is below, equal to or above the exact
/// binary value of @p magnitude (FloatingDecimal's "rounded up", "exact" and "truncated").
[[nodiscard]] int compareToExact(const DigitList& list, double magnitude)
{
    // A double's exact decimal expansion has at most 767 significant digits.
    const std::string      exact    = std::format("{:.766e}", magnitude);  // d.ddd...e-xx
    const std::size_t      e        = exact.find('e');
    const std::string_view mantissa = std::string_view(exact).substr(0, e);
    std::string            exactDigits;
    for (const char c : mantissa)
    {
        if (c != '.')
        {
            exactDigits += c;
        }
    }
    while (exactDigits.size() > 1 && exactDigits.back() == '0')
    {
        exactDigits.pop_back();
    }
    const int exactDecimalAt =
        Strings::parseInt(std::string_view(exact).substr(e + 1)).value_or(0) + 1;

    std::string digits = list.digits;
    while (digits.size() > 1 && digits.back() == '0')
    {
        digits.pop_back();
    }
    if (list.decimalAt != exactDecimalAt)
    {
        return list.decimalAt < exactDecimalAt ? -1 : 1;
    }
    const std::size_t width = std::max(digits.size(), exactDigits.size());
    digits.resize(width, '0');
    exactDigits.resize(width, '0');
    return digits.compare(exactDigits) < 0 ? -1 : (digits == exactDigits ? 0 : 1);
}

/// DigitList.shouldRoundUp with RoundingMode.HALF_EVEN: whether the digits truncated at
/// @p position round up.
[[nodiscard]] bool shouldRoundUp(const DigitList& list, std::size_t position, double magnitude)
{
    const std::string& digits = list.digits;
    if (position >= digits.size())
    {
        return false;
    }
    if (digits[position] > '5')
    {
        return true;
    }
    if (digits[position] == '5')
    {
        if (position == digits.size() - 1)
        {
            // The rounding position is exactly the last digit.
            if (isEasyPathInteger(magnitude))
            {
                return true;  // neither rounded up nor exact, as FloatingDecimal flags it
            }
            const int comparison = compareToExact(list, magnitude);
            if (comparison > 0)
            {
                return false;  // FloatingDecimal rounded up: the value was below the tie
            }
            if (comparison < 0)
            {
                return true;  // truncated to the tie: the value was above it
            }
            // An exact tie: to even.
            return position > 0 && (digits[position - 1] % 2) != 0;
        }
        // Rounds up if it gives a non null digit after '5'
        for (std::size_t i = position + 1; i < digits.size(); i++)
        {
            if (digits[i] != '0')
            {
                return true;
            }
        }
    }
    return false;
}

/// DigitList.round: keeps @p maximumDigits digits, rounding the rest away; a carry out of the
/// leading digit moves the decimal point.
void roundDigits(DigitList& list, int maximumDigits, double magnitude)
{
    if (maximumDigits < 0 || std::cmp_greater_equal(maximumDigits, list.digits.size()))
    {
        return;
    }
    auto count = static_cast<std::size_t>(maximumDigits);
    if (shouldRoundUp(list, count, magnitude))
    {
        // Rounding up involves incrementing digits from LSD to MSD.
        std::size_t i = count;
        while (true)
        {
            if (i == 0)
            {
                // All 9's: a single 1 and a larger exponent.
                list.digits[0] = '1';
                ++list.decimalAt;
                count = 1;
                break;
            }
            --i;
            ++list.digits[i];
            if (list.digits[i] <= '9')
            {
                count = i + 1;
                break;
            }
        }
    }
    list.digits.resize(count);
    trimTrailingZeros(list);
}

/// DigitList.set(boolean, double, int, true): the digits of @p magnitude rounded for a
/// fixed-point pattern with @p maxFractionDigits decimals.
[[nodiscard]] DigitList fixedPointDigits(double magnitude, int maxFractionDigits)
{
    DigitList list = parseJavaDigits(Strings::javaDoubleToString(magnitude));
    if (-list.decimalAt > maxFractionDigits)
    {
        // An underflow to zero, as when 0.0009 is rounded to 2 fraction digits.
        list.digits.clear();
    }
    else if (-list.decimalAt == maxFractionDigits)
    {
        // As when 0.0009 is rounded to 3 fraction digits: a new digit in the last place, or 0.
        if (shouldRoundUp(list, 0, magnitude))
        {
            list.digits = "1";
            ++list.decimalAt;
        }
        else
        {
            list.digits.clear();
        }
    }
    else
    {
        trimTrailingZeros(list);
        roundDigits(list, maxFractionDigits + list.decimalAt, magnitude);
    }
    if (list.digits.empty())
    {
        list.decimalAt = 0;
    }
    return list;
}

/// DecimalFormat.subformat for a fixed-point pattern with one minimum integer digit ("0.0##",
/// "0.#", "0", "#.###"; "#" prints the same, as a zero is printed when nothing else would be).
[[nodiscard]] std::string renderFixedPoint(const DigitList& list, int minFractionDigits,
                                           int maxFractionDigits)
{
    const std::string& digits     = list.digits;
    const int          count      = static_cast<int>(digits.size());
    const int          decimalAt  = list.decimalAt;
    int                digitIndex = 0;
    std::string        result;

    // The integer portion: the digits before the point, zeros beyond the digits.
    const int integerCount = std::max(1, decimalAt);
    for (int i = integerCount - 1; i >= 0; --i)
    {
        if (i < decimalAt && digitIndex < count)
        {
            result += digits[static_cast<std::size_t>(digitIndex++)];
        }
        else
        {
            result += '0';
        }
    }

    const bool fractionPresent = minFractionDigits > 0 || digitIndex < count;
    if (fractionPresent)
    {
        result += '.';
    }
    for (int i = 0; i < maxFractionDigits; ++i)
    {
        if (i >= minFractionDigits && digitIndex >= count)
        {
            break;
        }
        if (-1 - i > decimalAt - 1)
        {
            result += '0';  // a leading fractional zero, before the first significant digit
            continue;
        }
        if (digitIndex < count)
        {
            result += digits[static_cast<std::size_t>(digitIndex++)];
        }
        else
        {
            result += '0';
        }
    }
    return result;
}

/// DecimalFormat("0.00E0") for the magnitudes above 1e6 that Unit::toString sends there: three
/// significant digits, HALF_EVEN as DigitList applies it, then "E" and the bare exponent
/// ("1.23E6", "-2.50E15", "1.25E6" for 1245000, an integer, but "1.24E19" for 1.245e19).
[[nodiscard]] std::string formatExponential(double value)
{
    if (std::isnan(value))
    {
        return "NaN";
    }
    if (std::isinf(value))
    {
        return infinityText(value);
    }
    const double magnitude = std::abs(value);
    DigitList    list      = parseJavaDigits(Strings::javaDoubleToString(magnitude));
    if (list.digits.empty())
    {
        return std::signbit(value) ? "-0.00E0" : "0.00E0";
    }
    trimTrailingZeros(list);
    roundDigits(list, 3, magnitude);
    list.digits.resize(3, '0');
    const std::string text = std::format("{}.{}E{}", list.digits.substr(0, 1),
                                         list.digits.substr(1), list.decimalAt - 1);
    return std::signbit(value) ? "-" + text : text;
}

}  // namespace

const Unit& Unit::noUnit()
{
    static const GeneralUnit kNoUnit(1, std::string(Chars::kZwsp), 2);
    return kNoUnit;
}

Unit::Unit(double multiplier, std::string unit) : m_multiplier(multiplier), m_unit(std::move(unit))
{
    if (multiplier == 0)
    {
        throw std::invalid_argument("Unit has multiplier=0");
    }
}

double Unit::toUnit(double value) const
{
    return value / m_multiplier;
}

double Unit::fromUnit(double value) const
{
    return value * m_multiplier;
}

bool Unit::hasSpace() const
{
    return true;
}

std::string Unit::toString(double value) const
{
    if (std::isnan(value))
    {
        return "N/A";
    }

    double val = toUnit(value);

    if (std::abs(val) > 1.0E6)
    {
        return formatExponential(val);
    }
    if (std::abs(val) >= 100)
    {
        return formatInteger(val);
    }
    if (std::abs(val) <= 0.0005)
    {
        return "0";
    }

    val = roundForDecimalFormat(val);
    // Check for approximate integer
    if (std::abs(val - std::floor(val)) < 0.0001)
    {
        return formatInteger(val);
    }
    return formatDecimal(val, 1, 3);
}

double Unit::roundForDecimalFormat(double val) const
{
    const double sign = MathUtil::signum(val);
    val               = std::abs(val);
    double mul        = 1.0;
    while (val < 100 && mul < 1000)
    {
        mul *= 10;
        val *= 10;
    }
    val = std::nearbyint(val) / mul * sign;
    return val;
}

std::string Unit::toStringUnit(double value) const
{
    if (std::isnan(value))
    {
        return "N/A";
    }

    std::string s = toString(value);
    if (hasSpace())
    {
        s += " ";
    }
    s += m_unit;
    return s;
}

Value Unit::toValue(double value) const
{
    return {value, *this};
}

std::vector<Tick> Unit::decimalTicks(double start, double end, double minor, double major) const
{
    // Convert values
    start = toUnit(start);
    end   = toUnit(end);
    minor = toUnit(minor);
    major = toUnit(major);

    if (minor <= 0 || major <= 0 || major < minor)
    {
        throw std::invalid_argument(std::format("getTicks called with minor={} major={}",
                                                Strings::javaDoubleToString(minor),
                                                Strings::javaDoubleToString(major)));
    }

    std::vector<Tick> ticks;

    int    mod2    = 0;  // Moduli for minor-notable, major-nonnotable, major-notable
    int    mod3    = 0;
    int    mod4    = 0;
    double minstep = 0;

    // Find the smallest possible step size
    double one = 1;
    while (one > minor)
    {
        one /= 10;
    }
    while (one < minor)
    {
        one *= 10;
    }
    // one is the smallest round-ten that is larger than minor
    if (one / 2 >= minor)
    {
        // smallest step is round-five
        minstep = one / 2;
        mod2    = 2;  // Changed later if clashes with major ticks
    }
    else
    {
        minstep = one;
        mod2    = 10;  // Changed later if clashes with major ticks
    }

    // Find step size for major ticks; Java narrows Math.round's long to an int here
    one = 1;
    while (one > major)
    {
        one /= 10;
    }
    while (one < major)
    {
        one *= 10;
    }
    if (one / 2 >= major)
    {
        // major step is round-five, major-notable is next round-ten
        const double majorstep = one / 2;
        mod3 =
            static_cast<int>(static_cast<std::uint32_t>(MathUtil::javaRound(majorstep / minstep)));
        mod4 = mod3 * 2;
    }
    else
    {
        // major step is round-ten, major-notable is next round-ten
        mod3 = static_cast<int>(static_cast<std::uint32_t>(MathUtil::javaRound(one / minstep)));
        mod4 = mod3 * 10;
    }
    // Check for clashes between minor-notable and major-nonnotable
    if (mod3 == mod2)
    {
        if (mod2 == 2)
        {
            mod2 = 1;  // Every minor tick is notable
        }
        else
        {
            mod2 = 5;  // Every fifth minor tick is notable
        }
    }

    // Calculate starting position
    int pos = MathUtil::javaIntCast(std::ceil(start / minstep));
    while (pos * minstep <= end)
    {
        const double unitValue = pos * minstep;
        const double value     = fromUnit(unitValue);

        if (pos % mod4 == 0)
        {
            ticks.push_back(
                {.value = value, .unitValue = unitValue, .major = true, .notable = true});
        }
        else if (pos % mod3 == 0)
        {
            ticks.push_back(
                {.value = value, .unitValue = unitValue, .major = true, .notable = false});
        }
        else if (pos % mod2 == 0)
        {
            ticks.push_back(
                {.value = value, .unitValue = unitValue, .major = false, .notable = true});
        }
        else
        {
            ticks.push_back(
                {.value = value, .unitValue = unitValue, .major = false, .notable = false});
        }

        pos++;
    }

    return ticks;
}

bool Unit::equals(const Unit& other) const
{
    if (typeid(*this) != typeid(other))
    {
        return false;
    }
    return m_multiplier == other.m_multiplier && m_unit == other.m_unit;
}

std::size_t Unit::hash() const
{
    return typeid(*this).hash_code() + std::hash<std::string>{}(m_unit);
}

std::string Unit::formatDecimal(double value, int minFractionDigits, int maxFractionDigits)
{
    if (std::isnan(value))
    {
        return "NaN";
    }
    if (std::isinf(value))
    {
        return infinityText(value);
    }
    // DecimalFormat decides the sign before rounding, so "-0" and "-0.0" come out as in Java.
    const bool        negative = std::signbit(value);
    const DigitList   list     = fixedPointDigits(std::abs(value), maxFractionDigits);
    const std::string text     = renderFixedPoint(list, minFractionDigits, maxFractionDigits);
    return negative ? "-" + text : text;
}

std::string Unit::formatInteger(double value)
{
    return formatDecimal(value, 0, 0);
}

}  // namespace QtRocket
