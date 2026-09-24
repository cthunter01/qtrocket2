#include "QtRocket/unit/FractionalUnit.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/unit/Tick.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/util/Chars.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// The superscript digits 0-9 (U+2070, U+00B9, U+00B2, U+00B3, U+2074-U+2079) as UTF-8.
constexpr std::array<std::string_view, 10> kNumerator{
    "\xE2\x81\xB0", "\xC2\xB9",     "\xC2\xB2",     "\xC2\xB3",     "\xE2\x81\xB4",
    "\xE2\x81\xB5", "\xE2\x81\xB6", "\xE2\x81\xB7", "\xE2\x81\xB8", "\xE2\x81\xB9",
};

/// The subscript digits 0-9 (U+2080-U+2089) as UTF-8.
constexpr std::array<std::string_view, 10> kDenominator{
    "\xE2\x82\x80", "\xE2\x82\x81", "\xE2\x82\x82", "\xE2\x82\x83", "\xE2\x82\x84",
    "\xE2\x82\x85", "\xE2\x82\x86", "\xE2\x82\x87", "\xE2\x82\x88", "\xE2\x82\x89",
};

/// The decimal digits of a non-negative @p value in the given digit set; "0" for zero and for a
/// negative value the empty string, as OpenRocket's loop leaves it.
[[nodiscard]] std::string digitString(int value, const std::array<std::string_view, 10>& digits)
{
    if (value == 0)
    {
        return "0";
    }
    std::string rep;
    while (value > 0)
    {
        rep.insert(0, digits.at(static_cast<std::size_t>(value % 10)));
        value = value / 10;
    }
    return rep;
}

}  // namespace

FractionalUnit::FractionalUnit(double multiplier, std::string unit, std::string unitLabel,
                               int fractionBase, double incrementValue)
  : FractionalUnit(multiplier, std::move(unit), std::move(unitLabel), fractionBase, incrementValue,
                   0.1 / fractionBase)
{
}

FractionalUnit::FractionalUnit(double multiplier, std::string unit, std::string unitLabel,
                               int fractionBase, double incrementValue, double epsilon)
  : Unit(multiplier, std::move(unit)),
    m_fractionBase(fractionBase),
    m_fractionValue(1.0 / fractionBase),
    m_incrementValue(incrementValue),
    m_epsilon(epsilon),
    m_unitLabel(std::move(unitLabel))
{
}

double FractionalUnit::round(double value) const
{
    return roundTo(value, m_fractionValue);
}

double FractionalUnit::roundTo(double value, double fraction) noexcept
{
    // Math.IEEEremainder
    const double remainder = std::remainder(value, fraction);
    return value - remainder;
}

double FractionalUnit::getNextValue(double value) const
{
    double rounded = roundTo(value, m_incrementValue);
    if (rounded <= value + m_epsilon)
    {
        rounded += m_incrementValue;
    }
    return rounded;
}

double FractionalUnit::getPreviousValue(double value) const
{
    double rounded = roundTo(value, m_incrementValue);
    if (rounded >= value - m_epsilon)
    {
        rounded -= m_incrementValue;
    }
    return rounded;
}

std::vector<Tick> FractionalUnit::getTicks(double start, double end, double minor,
                                           double major) const
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
        one /= 2;
    }
    while (one < minor)
    {
        one *= 2;
    }
    minstep = one;
    mod2    = 16;

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

std::string FractionalUnit::toString(double value) const
{
    const double correctVal = toUnit(value);
    const double val        = round(correctVal);

    if (std::abs(val - correctVal) > m_epsilon)
    {
        return formatDecimal(correctVal, 0, 3);  // DecimalFormat("#.###")
    }

    const double sign = MathUtil::signum(val);

    double posValue = sign * val;

    const double intPart = std::floor(posValue);

    double frac     = std::nearbyint((posValue - intPart) / m_fractionValue);
    double fracBase = m_fractionBase;

    // Reduce fraction.
    while (frac > 0 && fracBase > 2 && std::fmod(frac, 2) == 0)
    {
        frac /= 2.0;
        fracBase /= 2.0;
    }

    posValue *= sign;

    if (frac == 0.0)
    {
        return formatInteger(posValue);  // DecimalFormat("#")
    }
    const std::string fraction = digitString(MathUtil::javaIntCast(frac), kNumerator) +
                                 std::string(Chars::kFraction) +
                                 digitString(MathUtil::javaIntCast(fracBase), kDenominator);
    if (intPart == 0.0)
    {
        return (sign < 0 ? "-" : "") + fraction;
    }
    return formatInteger(sign * intPart) + " " + fraction;
}

std::string FractionalUnit::toStringUnit(double value) const
{
    if (std::isnan(value))
    {
        return "N/A";
    }

    std::string s = toString(value);
    s += " " + m_unitLabel;
    return s;
}

std::unique_ptr<Unit> FractionalUnit::clone() const
{
    return std::make_unique<FractionalUnit>(*this);
}

}  // namespace QtRocket
