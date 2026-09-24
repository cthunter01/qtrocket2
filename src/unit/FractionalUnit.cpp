#include "QtRocket/unit/FractionalUnit.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/unit/Tick.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Chars.h"
#include "QtRocket/util/DecimalFormat.h"
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
        bug(std::format("getTicks called with minor={} major={}",
                        Strings::javaDoubleToString(minor), Strings::javaDoubleToString(major)));
    }

    // Find the smallest possible step size: a halving of one unit
    double one = 1;
    while (one > minor)
    {
        one /= 2;
    }
    while (one < minor)
    {
        one *= 2;
    }
    const double minstep = one;
    const int    mod2 = 16;  // minor-notable modulus, changed later if it clashes with major ticks

    return ticksAtMinorSteps(start, end, major, minstep, mod2);
}

std::string FractionalUnit::toString(double value) const
{
    const double correctVal = toUnit(value);
    const double val        = round(correctVal);

    if (std::abs(val - correctVal) > m_epsilon)
    {
        static const DecimalFormat kDecimalFormat("#.###");
        return kDecimalFormat.format(correctVal);
    }

    static const DecimalFormat kIntegerFormat("#");
    const double               sign = MathUtil::signum(val);

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
        return kIntegerFormat.format(posValue);
    }
    const std::string fraction = digitString(MathUtil::javaIntCast(frac), kNumerator) +
                                 std::string(Chars::kFraction) +
                                 digitString(MathUtil::javaIntCast(fracBase), kDenominator);
    if (intPart == 0.0)
    {
        return (sign < 0 ? "-" : "") + fraction;
    }
    return kIntegerFormat.format(sign * intPart) + " " + fraction;
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

}  // namespace QtRocket
