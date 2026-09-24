#include "QtRocket/unit/FixedPrecisionUnit.h"

#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "QtRocket/unit/Tick.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/util/DecimalFormat.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// The number of decimal places the precision needs, as OpenRocket counts them.
[[nodiscard]] int decimalsOf(double precision)
{
    int    decimals = 0;
    double p        = precision;
    while ((p - std::floor(p)) > 0.0000001)
    {
        p *= 10;
        decimals++;
    }
    return decimals;
}

/// The DecimalFormat pattern OpenRocket builds: "0", or "0." and the decimals as '0' when trailing
/// zeros are shown, as '#' otherwise.
[[nodiscard]] std::string patternOf(int decimals, bool displayTrailingZeros)
{
    std::string pattern = "0";
    if (decimals > 0)
    {
        pattern += '.';
        pattern.append(static_cast<std::size_t>(decimals), displayTrailingZeros ? '0' : '#');
    }
    return pattern;
}

}  // namespace

FixedPrecisionUnit::FixedPrecisionUnit(std::string unit, double precision)
  : FixedPrecisionUnit(std::move(unit), precision, 1.0)
{
}

FixedPrecisionUnit::FixedPrecisionUnit(std::string unit, double precision, double multiplier)
  : FixedPrecisionUnit(std::move(unit), precision, multiplier, true)
{
}

FixedPrecisionUnit::FixedPrecisionUnit(std::string unit, double precision, double multiplier,
                                       bool displayTrailingZeros)
  : Unit(multiplier, std::move(unit)),
    m_precision(precision),
    m_decimals(decimalsOf(precision)),
    m_displayTrailingZeros(displayTrailingZeros),
    m_formatter(patternOf(m_decimals, displayTrailingZeros))
{
}

double FixedPrecisionUnit::getNextValue(double value) const
{
    return round(value + m_precision);
}

double FixedPrecisionUnit::getPreviousValue(double value) const
{
    return round(value - m_precision);
}

double FixedPrecisionUnit::round(double value) const
{
    const double scale = 1.0 / m_precision;
    return static_cast<double>(MathUtil::javaRound(value * scale)) / scale;
}

std::string FixedPrecisionUnit::toString(double value) const
{
    const double unitValue = toUnit(value);
    if (m_displayTrailingZeros)
    {
        return Strings::formatFixed(unitValue, m_decimals);  // String.format("%.<decimals>f")
    }
    return m_formatter.format(unitValue);
}

std::vector<Tick> FixedPrecisionUnit::getTicks(double start, double end, double minor,
                                               double major) const
{
    // OpenRocket copies GeneralUnit.getTicks here verbatim.
    return decimalTicks(start, end, minor, major);
}

std::unique_ptr<Unit> FixedPrecisionUnit::clone() const
{
    return std::make_unique<FixedPrecisionUnit>(*this);
}

}  // namespace QtRocket
