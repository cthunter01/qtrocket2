#include "QtRocket/unit/GeneralUnit.h"

#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "QtRocket/unit/Tick.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket
{

GeneralUnit::GeneralUnit(double multiplier, std::string unit)
  : GeneralUnit(multiplier, std::move(unit), 2, 10)
{
}

GeneralUnit::GeneralUnit(double multiplier, std::string unit, int significantNumbers)
  : GeneralUnit(multiplier, std::move(unit), significantNumbers, 10)
{
}

GeneralUnit::GeneralUnit(double multiplier, std::string unit, int significantNumbers,
                         int decimalRounding)
  : GeneralUnit(multiplier, std::move(unit), significantNumbers, decimalRounding, 1.0)
{
}

GeneralUnit::GeneralUnit(double multiplier, std::string unit, int significantNumbers,
                         int decimalRounding, double stepValue)
  : Unit(multiplier, std::move(unit)),
    m_significantNumbers(significantNumbers),
    m_decimalRounding(decimalRounding),
    m_stepValue(stepValue),
    m_decimalLimit(1),
    m_significantNumbersLimit(10)
{
    QTROCKET_ASSERT(significantNumbers > 0);
    QTROCKET_ASSERT(decimalRounding > 0);

    for (int i = 1; i < significantNumbers; i++)
    {
        m_decimalLimit *= 10.0;
        m_significantNumbersLimit *= 10.0;
    }
}

double GeneralUnit::round(double value) const
{
    if (value < m_decimalLimit)
    {
        // Round to closest 1/decimalRounding
        const auto rounding = static_cast<double>(m_decimalRounding);
        return std::nearbyint(value * rounding) / rounding;
    }
    if (std::isinf(value))
    {
        return value;  // OpenRocket never leaves the loop below for an infinity
    }
    // Round to given amount of significant numbers
    double m = 1;
    while (value >= m_significantNumbersLimit)
    {
        m *= 10.0;
        value /= 10.0;
    }
    return std::nearbyint(value) * m;
}

std::vector<Tick> GeneralUnit::getTicks(double start, double end, double minor, double major) const
{
    return decimalTicks(start, end, minor, major);
}

double GeneralUnit::getNextValue(double value) const
{
    return value + 1;
}

double GeneralUnit::getPreviousValue(double value) const
{
    return value - 1;
}

std::unique_ptr<Unit> GeneralUnit::clone() const
{
    return std::make_unique<GeneralUnit>(*this);
}

}  // namespace QtRocket
