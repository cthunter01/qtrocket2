#include "QtRocket/unit/TemperatureUnit.h"

#include <memory>
#include <string>
#include <utility>

#include "QtRocket/unit/FixedPrecisionUnit.h"
#include "QtRocket/unit/Unit.h"

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

std::unique_ptr<Unit> TemperatureUnit::clone() const
{
    return std::make_unique<TemperatureUnit>(*this);
}

}  // namespace QtRocket
