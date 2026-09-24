#pragma once

#include <string>

#include "QtRocket/unit/GeneralUnit.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"

namespace QtRocket
{

/// A "dumb" group of one arbitrary unit with multiplier 1 (OpenRocket's FixedUnitGroup), for
/// custom expressions: it cannot convert to anything else. As in OpenRocket, only the unit count
/// (1), the default and SI unit (GeneralUnit(1, unitString)) and contains() (true for every unit)
/// are its own; everything else is UnitGroup's over an empty unit list. So getUnits() is empty,
/// getUnit(0) and setDefaultUnit(0) throw BugError, getUnit(name), findApproximate() and
/// getUnitIndex() find nothing, fromString() takes a bare number but no unit name (not even the
/// group's own), and equals() holds between any two FixedUnitGroups and with any group without
/// units, hash() being 0 (OpenRocket's FlightDataType relies on that when a custom expression's
/// unit changes).
///
/// Deviation: OpenRocket creates a new GeneralUnit on each getDefaultUnit() and getSIUnit() call;
/// this group returns the one it holds, so two Values made from it refer to the same unit and can
/// compare equal (Value::operator== compares units by identity; OpenRocket's never match).
class FixedUnitGroup final : public UnitGroup
{
public:
    explicit FixedUnitGroup(std::string unitString);

    [[nodiscard]] const std::string& getUnitString() const noexcept { return m_unitString; }

    /// 1, although getUnits() is empty.
    [[nodiscard]] int getUnitCount() const override;
    /// GeneralUnit(1, unitString).
    [[nodiscard]] const Unit& getDefaultUnit() const override;
    /// The same unit as getDefaultUnit().
    [[nodiscard]] const Unit& getSIUnit() const override;
    /// True for every unit.
    [[nodiscard]] bool contains(const Unit& u) const override;
    /// "FixedUnitGroup:<unit>".
    [[nodiscard]] std::string toString() const override;
    using UnitGroup::toString;

private:
    std::string m_unitString;
    GeneralUnit m_unit;
};

}  // namespace QtRocket
