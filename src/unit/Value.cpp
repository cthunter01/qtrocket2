#include "QtRocket/unit/Value.h"

#include <cstddef>
#include <functional>
#include <string>

#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

Value::Value(double value, const Unit& unit) noexcept : m_value(value), m_unit(&unit) { }

Value::Value(double value, const UnitGroup& group) : Value(value, group.getDefaultUnit()) { }

double Value::getUnitValue() const
{
    return m_unit->toUnit(m_value);
}

std::string Value::toString() const
{
    return m_unit->toStringUnit(m_value);
}

bool Value::operator==(const Value& other) const noexcept
{
    if (this == &other)
    {
        return true;
    }
    if (m_unit != other.m_unit)
    {
        return false;
    }
    return MathUtil::equals(m_value, other.m_value);
}

int Value::compareTo(const Value& other) const
{
    const int n = Strings::javaCompareTo(m_unit->getUnit(), other.m_unit->getUnit());
    if (n != 0)
    {
        return n;
    }
    return MathUtil::javaDoubleCompare(getUnitValue(), other.getUnitValue());
}

std::size_t Value::hash() const noexcept
{
    // Value.hashCode: 31 * (31 * 1 + unit.hashCode()) + Double.hashCode(value).
    constexpr std::size_t kPrime = 31;
    std::size_t           result = 1;
    result                       = (kPrime * result) + m_unit->hash();
    result                       = (kPrime * result) + std::hash<double>{}(m_value);
    return result;
}

}  // namespace QtRocket
