#include "QtRocket/unit/TemperatureUnit.h"

#include <string>
#include <utility>

#include "QtRocket/unit/FixedPrecisionUnit.h"

namespace QtRocket
{

TemperatureUnit::TemperatureUnit(double multiplier, double addition, double precision,
                                 std::string unit)
  : FixedPrecisionUnit(std::move(unit), precision, multiplier), m_addition(addition)
{
}

bool TemperatureUnit::hasSpace() const
{
    return false;
}

double TemperatureUnit::toUnit(double value) const
{
    return (value / getMultiplier()) - m_addition;
}

double TemperatureUnit::fromUnit(double value) const
{
    return (value + m_addition) * getMultiplier();
}

}  // namespace QtRocket
