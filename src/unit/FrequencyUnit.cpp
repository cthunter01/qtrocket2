#include "QtRocket/unit/FrequencyUnit.h"

#include <string>
#include <utility>

#include "QtRocket/unit/GeneralUnit.h"

namespace QtRocket
{

FrequencyUnit::FrequencyUnit(double multiplier, std::string unit)
  : GeneralUnit(multiplier, std::move(unit))
{
}

double FrequencyUnit::toUnit(double value) const
{
    const double hz = 1 / value;
    return hz / getMultiplier();
}

double FrequencyUnit::fromUnit(double value) const
{
    const double hz = value * getMultiplier();
    return 1 / hz;
}

}  // namespace QtRocket
