#include "QtRocket/rocket/RingComponent.h"

#include <cmath>
#include <optional>
#include <vector>

#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/RadialParent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

namespace
{

/// The x of the first coordinate of @p coordinates (Java: toRelative(...)[0].getX()).
[[nodiscard]] double firstX(const std::vector<Coordinate>& coordinates)
{
    // Java: ArrayIndexOutOfBoundsException; every component has at least one location.
    QTROCKET_ASSERT(!coordinates.empty());
    return coordinates.front().x;
}

}  // namespace

void RingComponent::setOuterRadiusAutomatic(bool automatic)
{
    if (automatic == m_outerRadiusAutomatic)
    {
        return;
    }
    m_outerRadiusAutomatic = automatic;
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

void RingComponent::setInnerRadiusAutomatic(bool automatic)
{
    if (automatic == m_innerRadiusAutomatic)
    {
        return;
    }
    m_innerRadiusAutomatic = automatic;
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

std::optional<double> RingComponent::parentInnerRadius() const
{
    // Java: getParent() instanceof RadialParent (false without a parent).
    const auto* radialParent = dynamic_cast<const RadialParent*>(m_parent);
    if (radialParent == nullptr)
    {
        return std::nullopt;
    }
    double pos1 = firstX(toRelative(Coordinate::kNul, *m_parent));
    double pos2 = firstX(toRelative(Coordinate{getLength()}, *m_parent));
    pos1        = MathUtil::clamp(pos1, 0, m_parent->getLength());
    pos2        = MathUtil::clamp(pos2, 0, m_parent->getLength());
    return MathUtil::javaMin(radialParent->getInnerRadius(pos1),
                             radialParent->getInnerRadius(pos2));
}

void RingComponent::setLength(double length)
{
    const double l = MathUtil::javaMax(length, 0.0);
    if (m_length == l)
    {
        return;
    }
    m_length = l;
    clearPreset();
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

void RingComponent::setRadialDirection(double dir)
{
    dir = MathUtil::reducePi(dir);
    if (m_radialDirection == dir)
    {
        return;
    }
    m_radialDirection = dir;
    m_shiftY          = m_radialPosition * std::cos(m_radialDirection);
    m_shiftZ          = m_radialPosition * std::sin(m_radialDirection);
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

void RingComponent::setRadialPosition(double pos)
{
    pos = MathUtil::javaMax(pos, 0.0);
    if (m_radialPosition == pos)
    {
        return;
    }
    m_radialPosition = pos;
    m_shiftY         = m_radialPosition * std::cos(m_radialDirection);
    m_shiftZ         = m_radialPosition * std::sin(m_radialDirection);
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

void RingComponent::setRadialShift(double y, double z)
{
    // Java's Math.hypot (not MathUtil.hypot) and Math.atan2.
    m_radialPosition  = std::hypot(y, z);
    m_radialDirection = std::atan2(z, y);

    // Recalculated to keep the three consistent.
    m_shiftY = m_radialPosition * std::cos(m_radialDirection);
    m_shiftZ = m_radialPosition * std::sin(m_radialDirection);

    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

BoundingBox RingComponent::getInstanceBoundingBox() const
{
    BoundingBox instanceBounds;
    instanceBounds.update(Coordinate{getLength(), 0, 0});

    const double r = getOuterRadius();
    instanceBounds.update(Coordinate{0, r, r});
    instanceBounds.update(Coordinate{0, -r, -r});
    return instanceBounds;
}

std::vector<Coordinate> RingComponent::getComponentBounds() const
{
    std::vector<Coordinate> bounds;
    addBound(bounds, 0, getOuterRadius());
    addBound(bounds, m_length, getOuterRadius());
    return bounds;
}

Coordinate RingComponent::getComponentCG() const
{
    Coordinate   cg            = Coordinate::kZero;
    const int    instanceCount = getInstanceCount();
    const double instanceMass =
        ringMass(getOuterRadius(), getInnerRadius(), getLength(), getMaterial().getDensity());

    if (1 == instanceCount)
    {
        cg = Coordinate{m_length / 2, 0, 0, instanceMass};
    }
    else
    {
        for (const Coordinate& offset : getInstanceOffsets())
        {
            cg = cg.average(offset.setWeight(instanceMass));
        }
        cg = cg.add(m_length / 2, 0, 0);
    }
    return cg;
}

double RingComponent::getComponentMass() const
{
    return ringMass(getOuterRadius(), getInnerRadius(), getLength(), getMaterial().getDensity()) *
           getInstanceCount();
}

double RingComponent::getLongitudinalUnitInertia() const
{
    return ringLongitudinalUnitInertia(getOuterRadius(), getInnerRadius(), getLength());
}

double RingComponent::getRotationalUnitInertia() const
{
    return ringRotationalUnitInertia(getOuterRadius(), getInnerRadius());
}

}  // namespace QtRocket
