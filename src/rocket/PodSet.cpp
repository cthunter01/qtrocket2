#include "QtRocket/rocket/PodSet.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

#include "QtRocket/rocket/ComponentAssembly.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

PodSet::PodSet() : ComponentAssembly(AxialMethod::BOTTOM) { }

std::unique_ptr<RocketComponent> PodSet::cloneShallow() const
{
    return std::make_unique<PodSet>(*this);
}

bool PodSet::isCompatible(ComponentKind kind) const
{
    return isBodyComponent(kind);
}

bool PodSet::isAfter() const
{
    return false;
}

int PodSet::getRelativeToStage() const
{
    if (m_parent == nullptr)
    {
        return -1;
    }
    if (m_parent->kind() == ComponentKind::POD_SET)
    {
        // Java: NullPointerException when the parent pod set has no parent.
        const RocketComponent* grandParent = m_parent->getParent();
        if (grandParent == nullptr)
        {
            bug("PodSet::getRelativeToStage(): the parent pod set has no parent");
        }
        const std::optional<std::size_t> position = grandParent->getChildPosition(m_parent);
        return position ? static_cast<int>(*position) : -1;
    }
    return -1;
}

int PodSet::getInstanceCount() const
{
    return m_instanceCount;
}

void PodSet::setInstanceCount(int newCount)
{
    if (newCount < 1)
    {
        // There must be at least one instance.
        return;
    }
    m_instanceCount   = newCount;
    m_angleSeparation = std::numbers::pi * 2 / m_instanceCount;
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

std::vector<Coordinate> PodSet::getInstanceLocations() const
{
    return RocketComponent::getInstanceLocations();
}

std::vector<Coordinate> PodSet::getInstanceOffsets() const
{
    const double radius = QtRocket::getRadius(m_radiusMethod, m_parent, this, m_radiusOffset);

    std::vector<Coordinate>   toReturn;
    const std::vector<double> angles = getInstanceAngles();
    toReturn.reserve(angles.size());
    for (const double angle : angles)
    {
        const double curY = radius * std::cos(angle);
        const double curZ = radius * std::sin(angle);
        toReturn.emplace_back(0, curY, curZ);
    }
    return toReturn;
}

std::string PodSet::getPatternName() const
{
    return std::to_string(getInstanceCount()) + "-ring";
}

double PodSet::getInstanceAngleIncrement() const
{
    return m_angleSeparation;
}

std::vector<double> PodSet::getInstanceAngles() const
{
    const double baseAngle = getAngleOffset();
    const double incrAngle = getInstanceAngleIncrement();

    std::vector<double> result;
    result.reserve(static_cast<std::size_t>(std::max(getInstanceCount(), 0)));
    for (int i = 0; i < getInstanceCount(); ++i)
    {
        result.push_back(baseAngle + (incrAngle * i));
    }
    return result;
}

double PodSet::getAngleOffset() const
{
    return m_angleOffset;
}

void PodSet::setAngleOffset(double angle)
{
    m_angleOffset = angle;
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

AngleMethod PodSet::getAngleMethod() const
{
    return m_angleMethod;
}

void PodSet::setAngleMethod(AngleMethod /*newMethod*/)
{
    // Does nothing, as in Java.
}

double PodSet::getBoundingRadius() const
{
    return ComponentAssembly::getBoundingRadius();
}

double PodSet::getRadiusOffset() const
{
    return m_radiusOffset;
}

void PodSet::setRadiusOffset(double radius)
{
    if (radius == m_radiusOffset)
    {
        return;
    }
    m_radiusOffset = clampToZero(m_radiusMethod) ? 0.0 : radius;
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

RadiusMethod PodSet::getRadiusMethod() const
{
    return m_radiusMethod;
}

void PodSet::setRadiusMethod(RadiusMethod method)
{
    if (method == m_radiusMethod)
    {
        return;
    }
    // The radius from the parent's centre.
    const double radius = QtRocket::getRadius(m_radiusMethod, m_parent, this, m_radiusOffset);
    setRadius(method, radius);
}

void PodSet::setRadius(RadiusMethod method, double radius)
{
    double newRadius = radius;
    // As in Java, the method in force before the call decides (ParallelStage: the new one).
    if (clampToZero(m_radiusMethod))
    {
        newRadius = 0.0;
    }
    m_radiusMethod = method;
    m_radiusOffset = QtRocket::getAsOffset(m_radiusMethod, m_parent, this, newRadius);
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

double PodSet::getAxialOffset() const
{
    return getAxialOffset(m_axialMethod);
}

double PodSet::getAxialOffset(AxialMethod method) const
{
    if (isAfter())
    {
        bug("found a pod positioned via: AFTER, but is not on the centerline?!: " + getName() +
            "  is " + std::string{axialMethodName(getAxialMethod())});
    }
    double returnValue = RocketComponent::getAxialOffset(method);
    if (MathUtil::kEpsilon > std::abs(returnValue))
    {
        returnValue = 0.0;
    }
    return returnValue;
}

void PodSet::setAxialMethod(AxialMethod newMethod)
{
    ComponentAssembly::setAxialMethod(newMethod);
    fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
}

}  // namespace QtRocket
