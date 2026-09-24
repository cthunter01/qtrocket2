#pragma once

#include <compare>
#include <cstddef>
#include <functional>
#include <string>

namespace QtRocket
{

class Unit;
class UnitGroup;

/// An SI value together with the unit it is shown in (OpenRocket's Value): immutable, formats
/// itself with the unit, and sorts by the unit name and then by the value in that unit, so a
/// table column of Values orders sensibly. The Value refers to its Unit and does not own it; a
/// unit from the UnitGroup registry lives for the whole process.
class Value
{
public:
    Value(double value, const Unit& unit) noexcept;
    /// The value in the group's default unit ("currently it simply uses the default unit of the
    /// group, but may later change").
    Value(double value, const UnitGroup& group);

    /// The value in SI units.
    [[nodiscard]] double getValue() const noexcept { return m_value; }
    /// The value converted to the unit.
    [[nodiscard]] double      getUnitValue() const;
    [[nodiscard]] const Unit& getUnit() const noexcept { return *m_unit; }

    /// Unit::toStringUnit(value): "283.15 K", "N/A" for NaN.
    [[nodiscard]] std::string toString() const;

    /// Value.equals: the same Unit object and the same value within MathUtil::equals. This is
    /// coarser than operator<=> (whose "equivalent" compares the unit names and the exact unit
    /// values), as in OpenRocket, where equals and compareTo disagree the same way.
    [[nodiscard]] bool operator==(const Value& other) const noexcept;

    /// Value.compareTo: by the unit name (as std::string compares), then by the value in the
    /// units, ordered as Double.compare (-0.0 below 0.0, NaN above everything). Negative, zero
    /// or positive.
    [[nodiscard]] int compareTo(const Value& other) const;
    /// compareTo() as an ordering, for sorting (ValueComparator).
    [[nodiscard]] std::weak_ordering operator<=>(const Value& other) const;

private:
    double      m_value;
    const Unit* m_unit;
};

}  // namespace QtRocket

/// Value.hashCode's shape: the unit's hash and the value's, so that equal Values hash alike when
/// their values are identical (a tolerance-based equals cannot be hashed exactly, as in Java).
template <>
struct std::hash<QtRocket::Value>
{
    [[nodiscard]] std::size_t operator()(const QtRocket::Value& value) const noexcept;
};
