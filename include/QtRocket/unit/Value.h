#pragma once

#include <cstddef>
#include <string>

namespace QtRocket
{

class Unit;
class UnitGroup;

/// An SI value together with the unit it is shown in (OpenRocket's Value): immutable, formats
/// itself with the unit, and sorts by the unit name and then by the value in that unit
/// (compareTo, ValueComparator), so a table column of Values orders sensibly.
///
/// The Value refers to its Unit and does not own it, where Java's garbage collector keeps the
/// unit alive: the unit must outlive the Value. A unit of the process-wide groups (unitGroup())
/// lives for the whole process; one of a StabilityUnitGroup (UnitGroup::stabilityUnits) or of a
/// FixedUnitGroup lives as long as that group. A temporary Unit or UnitGroup is refused at
/// compile time, since it would be gone before the Value is used.
class Value
{
public:
    Value(double value, const Unit& unit) noexcept;
    Value(double value, const Unit&& unit) = delete;
    /// The value in the group's default unit ("currently it simply uses the default unit of the
    /// group, but may later change").
    Value(double value, const UnitGroup& group);
    Value(double value, const UnitGroup&& group) = delete;

    /// The value in SI units.
    [[nodiscard]] double getValue() const noexcept { return m_value; }
    /// The value converted to the unit.
    [[nodiscard]] double      getUnitValue() const;
    [[nodiscard]] const Unit& getUnit() const noexcept { return *m_unit; }

    /// Unit::toStringUnit(value): "283.15 K", "N/A" for NaN.
    [[nodiscard]] std::string toString() const;

    /// Value.equals: the same Value object, or the same Unit object and the same value within
    /// MathUtil::equals (so a Value always equals itself, even for NaN, and two NaN Values never
    /// equal each other). This is coarser than compareTo(), which compares the unit names and
    /// the exact unit values, as in OpenRocket, where equals and compareTo disagree the same way;
    /// that is why Value has no operator< or operator<=>: sort with ValueComparator.
    [[nodiscard]] bool operator==(const Value& other) const noexcept;

    /// Value.compareTo: by the unit name (String.compareTo, Strings::javaCompareTo), then by the
    /// value in the units, ordered as Double.compare (-0.0 below 0.0, NaN above everything).
    /// Negative, zero or positive.
    [[nodiscard]] int compareTo(const Value& other) const;

    /// Value.hashCode's shape: the unit's hash and the value's, so that equal Values hash alike
    /// when their values are identical. There is no std::hash<Value>: equality allows a
    /// tolerance, so equal Values can hash apart and an unordered container would break its
    /// contract.
    [[nodiscard]] std::size_t hash() const noexcept;

private:
    double      m_value;
    const Unit* m_unit;
};

}  // namespace QtRocket
