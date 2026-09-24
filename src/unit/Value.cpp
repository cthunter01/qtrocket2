#include "QtRocket/unit/Value.h"

#include <compare>
#include <cstddef>
#include <functional>
#include <string>

#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/MathUtil.h"

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
    if (m_unit != other.m_unit)
    {
        return false;
    }
    return MathUtil::equals(m_value, other.m_value);
}

int Value::compareTo(const Value& other) const
{
    const int n = m_unit->getUnit().compare(other.m_unit->getUnit());
    if (n != 0)
    {
        return n;
    }
    return MathUtil::javaDoubleCompare(getUnitValue(), other.getUnitValue());
}

std::weak_ordering Value::operator<=>(const Value& other) const
{
    const int c = compareTo(other);
    if (c < 0)
    {
        return std::weak_ordering::less;
    }
    if (c > 0)
    {
        return std::weak_ordering::greater;
    }
    return std::weak_ordering::equivalent;
}

}  // namespace QtRocket

std::size_t std::hash<QtRocket::Value>::operator()(const QtRocket::Value& value) const noexcept
{
    // Value.hashCode: 31 * (31 * 1 + unit.hashCode()) + Double.hashCode(value).
    constexpr std::size_t kPrime = 31;
    std::size_t           result = 1;
    result                       = (kPrime * result) + value.getUnit().hash();
    result                       = (kPrime * result) + std::hash<double>{}(value.getValue());
    return result;
}
