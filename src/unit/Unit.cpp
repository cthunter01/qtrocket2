#include "QtRocket/unit/Unit.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <typeinfo>
#include <utility>
#include <vector>

#include "QtRocket/unit/GeneralUnit.h"
#include "QtRocket/unit/Tick.h"
#include "QtRocket/unit/Value.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Chars.h"
#include "QtRocket/util/DecimalFormat.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

const Unit& Unit::noUnit()
{
    static const GeneralUnit kNoUnit(1, std::string(Chars::kZwsp), 2);
    return kNoUnit;
}

Unit::Unit(double multiplier, std::string unit) : m_multiplier(multiplier), m_unit(std::move(unit))
{
    if (multiplier == 0)
    {
        bug("Unit has multiplier=0");
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

    // OpenRocket keeps these per thread and locale; they are immutable here.
    static const DecimalFormat kIntegerFormat("#");
    static const DecimalFormat kDecimalFormat("0.0##");
    static const DecimalFormat kExponentialFormat("0.00E0");

    double val = toUnit(value);

    if (std::abs(val) > 1.0E6)
    {
        return kExponentialFormat.format(val);
    }
    if (std::abs(val) >= 100)
    {
        return kIntegerFormat.format(val);
    }
    if (std::abs(val) <= 0.0005)
    {
        return "0";
    }

    val = roundForDecimalFormat(val);
    // Check for approximate integer
    if (std::abs(val - std::floor(val)) < 0.0001)
    {
        return kIntegerFormat.format(val);
    }
    return kDecimalFormat.format(val);
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
        bug(std::format("getTicks called with minor={} major={}",
                        Strings::javaDoubleToString(minor), Strings::javaDoubleToString(major)));
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

}  // namespace QtRocket
