#include "QtRocket/rocket/MassObject.h"

#include <cmath>
#include <vector>

#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/RingComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

MassObject::MassObject() : MassObject(0.025, 0.0125) { }

MassObject::MassObject(double length, double radius) : m_radius(radius)
{
    m_length = length;
    updateVolume(radius);

    setAxialMethod(AxialMethod::TOP);
    setAxialOffset(0.0);
}

bool MassObject::isAfter() const
{
    return false;
}

void MassObject::updateVolume(double radius)
{
    // Java's Math.pow(radius, 2): radius * radius, the correctly rounded square (Math.PI is left
    // out, it is not needed).
    m_volume = MathUtil::pow2(radius) * m_length;
}

double MassObject::getLength() const
{
    if (m_autoRadius)
    {
        m_length = getAutoLength();
    }
    return m_length;
}

void MassObject::setLength(double length)
{
    length = MathUtil::javaMax(length, 0.0);

    if (MathUtil::equals(m_length, length))
    {
        return;
    }
    m_length = length;
    updateVolume(m_autoRadius ? getAutoRadius() : m_radius);

    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

double MassObject::getAutoLength() const
{
    // The volume is the same with the automatic radius and the stored one, so the length at the
    // stored radius follows from it.
    if (MathUtil::equals(m_radius, 0))
    {
        return m_length;
    }
    return m_volume / MathUtil::pow2(m_radius);
}

double MassObject::getRadius() const
{
    if (m_autoRadius)
    {
        m_radius = getAutoRadius();
        m_length = getAutoLength();
    }
    return m_radius;
}

double MassObject::getAutoRadius() const
{
    if (m_parent == nullptr)
    {
        return m_radius;
    }
    const double autoRadius = getMaxParentRadius();

    return MathUtil::equals(autoRadius, 0) ? m_radius : autoRadius;
}

double MassObject::getMaxParentRadius() const
{
    if (const auto* noseCone = dynamic_cast<const NoseCone*>(m_parent))
    {
        return noseCone->getBaseRadius();
    }
    if (const auto* transition = dynamic_cast<const Transition*>(m_parent))
    {
        // Fore before aft, as Java reads them (reading an automatic radius refreshes it).
        const double foreRadius = transition->getForeRadius();
        const double aftRadius  = transition->getAftRadius();
        return MathUtil::javaMax(foreRadius, aftRadius);
    }
    if (const auto* body = dynamic_cast<const SymmetricComponent*>(m_parent))
    {
        // Java: instanceof BodyComponent, whose getInnerRadius() a body tube overrides.
        return body->getInnerRadius();
    }
    if (const auto* ring = dynamic_cast<const RingComponent*>(m_parent))
    {
        return ring->getInnerRadius();
    }
    // Any other parent, and none at all (Java's instanceof is false for null).
    return 0;
}

void MassObject::setRadius(double radius)
{
    radius = MathUtil::javaMax(radius, 0.0);

    if (MathUtil::equals(m_radius, radius) && !m_autoRadius)
    {
        return;
    }

    m_autoRadius = false;
    m_radius     = radius;
    updateVolume(radius);
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

void MassObject::setRadiusAutomatic(bool automatic)
{
    if (m_autoRadius == automatic)
    {
        return;
    }

    m_autoRadius = automatic;

    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

void MassObject::setRadialPosition(double radialPosition)
{
    radialPosition = MathUtil::javaMax(radialPosition, 0.0);

    if (MathUtil::equals(m_radialPosition, radialPosition))
    {
        return;
    }
    m_radialPosition = radialPosition;
    m_shiftY         = radialPosition * std::cos(m_radialDirection);
    m_shiftZ         = radialPosition * std::sin(m_radialDirection);
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

void MassObject::setRadialDirection(double radialDirection)
{
    radialDirection = MathUtil::reducePi(radialDirection);
    if (MathUtil::equals(m_radialDirection, radialDirection))
    {
        return;
    }
    m_radialDirection = radialDirection;
    m_shiftY          = m_radialPosition * std::cos(radialDirection);
    m_shiftZ          = m_radialPosition * std::sin(radialDirection);
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

Coordinate MassObject::getComponentCG() const
{
    // Java evaluates the length before the mass (a braced list keeps that order).
    return Coordinate{getLength() / 2, m_shiftY, m_shiftZ, getComponentMass()};
}

double MassObject::getLongitudinalUnitInertia() const
{
    // Java evaluates the radius before the length.
    const double radius = getRadius();
    const double length = getLength();
    return ((3 * MathUtil::pow2(radius)) + MathUtil::pow2(length)) / 12;
}

double MassObject::getRotationalUnitInertia() const
{
    return MathUtil::pow2(getRadius()) / 2;
}

std::vector<Coordinate> MassObject::getComponentBounds() const
{
    std::vector<Coordinate> bounds;
    addBound(bounds, 0, getRadius());
    // Java evaluates the length before the radius.
    const double length = getLength();
    const double radius = getRadius();
    addBound(bounds, length, radius);
    return bounds;
}

}  // namespace QtRocket
