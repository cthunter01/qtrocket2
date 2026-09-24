#include "QtRocket/models/ConstantGravityModel.h"

#include <string>

#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/WorldCoordinate.h"

namespace QtRocket
{

double ConstantGravityModel::getGravity(const WorldCoordinate& /*wc*/) const
{
    return m_gravity;
}

bool ConstantGravityModel::operator==(const ConstantGravityModel& other) const noexcept
{
    return MathUtil::javaDoubleCompare(m_gravity, other.m_gravity) == 0;
}

int ConstantGravityModel::hashCode() const noexcept
{
    return MathUtil::javaDoubleHashCode(m_gravity);
}

std::string ConstantGravityModel::toString() const
{
    return "ConstantGravityModel[gravity=" + Strings::javaDoubleToString(m_gravity) + "]";
}

}  // namespace QtRocket
