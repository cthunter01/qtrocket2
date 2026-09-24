#include "QtRocket/unit/FixedPrecisionUnit.h"

#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "QtRocket/unit/Tick.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

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
    m_decimals(0),
    m_displayTrailingZeros(displayTrailingZeros)
{
    // Calculate number of decimal places needed
    double p = precision;
    while ((p - std::floor(p)) > 0.0000001)
    {
        p *= 10;
        m_decimals++;
    }
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
    return formatDecimal(unitValue, 0, m_decimals);  // DecimalFormat("0.###")
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
