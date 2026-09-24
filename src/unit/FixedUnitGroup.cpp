#include "QtRocket/unit/FixedUnitGroup.h"

#include <string>
#include <utility>

#include "QtRocket/unit/Unit.h"

namespace QtRocket
{

FixedUnitGroup::FixedUnitGroup(std::string unitString)
  : m_unitString(std::move(unitString)), m_unit(1, m_unitString)
{
}

int FixedUnitGroup::getUnitCount() const
{
    return 1;
}

const Unit& FixedUnitGroup::getDefaultUnit() const
{
    return m_unit;
}

const Unit& FixedUnitGroup::getSIUnit() const
{
    return m_unit;
}

bool FixedUnitGroup::contains(const Unit& /*u*/) const
{
    return true;
}

std::string FixedUnitGroup::toString() const
{
    return "FixedUnitGroup:" + getSIUnit().getUnit();
}

}  // namespace QtRocket
