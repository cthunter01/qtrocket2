#include "QtRocket/unit/InchUnit.h"

#include <cmath>
#include <string>
#include <utility>

#include "QtRocket/unit/GeneralUnit.h"

namespace QtRocket
{

InchUnit::InchUnit(double multiplier, std::string unit) : InchUnit(multiplier, std::move(unit), 1)
{
}

InchUnit::InchUnit(double multiplier, std::string unit, double precision)
  : GeneralUnit(multiplier, std::move(unit)), m_precision(precision)
{
}

double InchUnit::roundForDecimalFormat(double val) const
{
    const double mul = 1000.0;
    val              = std::nearbyint(val * mul) / mul;
    return val;
}

double InchUnit::getNextValue(double value) const
{
    return value + m_precision;
}

double InchUnit::getPreviousValue(double value) const
{
    return value - m_precision;
}

}  // namespace QtRocket
