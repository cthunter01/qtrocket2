#include "QtRocket/rocket/LaunchLug.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/Tube.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
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

LaunchLug::LaunchLug() : Tube(AxialMethod::MIDDLE)
{
    m_length = kDefaultLength;
    setDisplayOrderSide(15);  // Order for displaying the component in the 2D side view
    setDisplayOrderBack(12);  // Order for displaying the component in the 2D back view
}

std::unique_ptr<RocketComponent> LaunchLug::cloneShallow() const
{
    return std::make_unique<LaunchLug>(*this);
}

// ================================================================================== radii

double LaunchLug::getOuterRadius() const
{
    return m_radius;
}

void LaunchLug::setOuterRadius(double radius)
{
    if (MathUtil::equals(m_radius, radius))
    {
        return;
    }
    m_radius    = radius;
    m_thickness = MathUtil::javaMin(m_thickness, m_radius);
    clearPreset();
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

double LaunchLug::getInnerRadius() const
{
    return m_radius - m_thickness;
}

void LaunchLug::setInnerRadius(double innerRadius)
{
    setOuterRadius(innerRadius + m_thickness);
}

double LaunchLug::getThickness() const
{
    return m_thickness;
}

void LaunchLug::setThickness(double thickness)
{
    if (MathUtil::equals(m_thickness, thickness))
    {
        return;
    }
    m_thickness = MathUtil::clamp(thickness, 0, m_radius);
    clearPreset();
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

void LaunchLug::setLength(double length)
{
    if (MathUtil::equals(m_length, length))
    {
        return;
    }
    m_length = length;
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

// =============================================================================== position

double LaunchLug::getAngleOffset() const
{
    return m_angleOffsetRad;
}

void LaunchLug::setAngleOffset(double angle)
{
    const double clampedRad = MathUtil::clamp(angle, -std::numbers::pi, std::numbers::pi);
    if (MathUtil::equals(m_angleOffsetRad, clampedRad))
    {
        return;
    }
    m_angleOffsetRad = clampedRad;
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

AngleMethod LaunchLug::getAngleMethod() const
{
    return AngleMethod::RELATIVE;
}

void LaunchLug::setAngleMethod(AngleMethod /*newMethod*/)
{
    // Does nothing, as in Java.
}

void LaunchLug::setAxialMethod(AxialMethod newMethod)
{
    RocketComponent::setAxialMethod(newMethod);
}

double LaunchLug::getAxialOffset() const
{
    return RocketComponent::getAxialOffset();
}

void LaunchLug::setAxialOffset(double newOffset)
{
    RocketComponent::setAxialOffset(newOffset);
}

bool LaunchLug::isAfter() const
{
    return false;
}

// ============================================================================== instances

double LaunchLug::getInstanceSeparation() const
{
    return m_instanceSeparation;
}

void LaunchLug::setInstanceSeparation(double separation)
{
    if (MathUtil::equals(m_instanceSeparation, separation))
    {
        return;
    }
    m_instanceSeparation = separation;
    fireComponentChangeEvent(ComponentChangeEvent::kAerodynamicChange);
}

int LaunchLug::getInstanceCount() const
{
    return m_instanceCount;
}

void LaunchLug::setInstanceCount(int newCount)
{
    if (newCount == m_instanceCount || newCount <= 0)
    {
        return;
    }
    m_instanceCount = newCount;
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

std::vector<Coordinate> LaunchLug::getInstanceLocations() const
{
    return RocketComponent::getInstanceLocations();
}

std::vector<Coordinate> LaunchLug::getInstanceOffsets() const
{
    const double yOffset = std::cos(m_angleOffsetRad) * m_radialOffset;
    const double zOffset = std::sin(m_angleOffsetRad) * m_radialOffset;

    std::vector<Coordinate> toReturn;
    toReturn.reserve(static_cast<std::size_t>(std::max(getInstanceCount(), 0)));
    for (int index = 0; index < getInstanceCount(); index++)
    {
        toReturn.emplace_back(index * m_instanceSeparation, yOffset, zOffset);
    }

    return toReturn;
}

std::string LaunchLug::getPatternName() const
{
    return std::to_string(getInstanceCount()) + "-Line";
}

void LaunchLug::componentChanged(const ComponentChangeEvent& event)
{
    Tube::componentChanged(event);

    // The radial distance is computed here, since computing it in getInstanceOffsets() would
    // recurse through toRelative().
    const SymmetricComponent* body = nullptr;
    for (const RocketComponent* ancestor = getParent(); ancestor != nullptr;
         ancestor                        = ancestor->getParent())
    {
        body = dynamic_cast<const SymmetricComponent*>(ancestor);
        if (body != nullptr)
        {
            break;
        }
    }

    double parentRadius = 0;
    if (body != nullptr)
    {
        double x1 = firstX(toRelative(Coordinate::kNul, *body));
        double x2 = firstX(toRelative(Coordinate{m_length, 0, 0}, *body));
        x1        = MathUtil::clamp(x1, 0, body->getLength());
        x2        = MathUtil::clamp(x2, 0, body->getLength());
        // Both radii are read, in this order, whatever their values: reading an automatic body
        // tube radius refreshes it.
        const double radius1 = body->getRadius(x1);
        const double radius2 = body->getRadius(x2);
        parentRadius         = MathUtil::javaMax(radius1, radius2);
    }

    m_radialOffset = parentRadius + m_radius;
    clearCoordinateCaches();
}

// ========================================================================== mass and bounds

double LaunchLug::getComponentVolume() const
{
    return m_length * std::numbers::pi *
           (MathUtil::pow2(m_radius) - MathUtil::pow2(m_radius - m_thickness)) * getInstanceCount();
}

std::vector<Coordinate> LaunchLug::getComponentBounds() const
{
    std::vector<Coordinate> set;
    addBound(set, 0, m_radius);
    addBound(set, m_length, m_radius);
    return set;
}

Coordinate LaunchLug::getComponentCG() const
{
    const auto*  symmetricParent = dynamic_cast<const SymmetricComponent*>(getParent());
    const double parentRadius =
        symmetricParent != nullptr ? symmetricParent->getRadius(getAxialOffset()) : 0;

    const double cmX = (m_length / 2) + ((m_instanceSeparation * (m_instanceCount - 1)) / 2);
    const double cmY = std::cos(m_angleOffsetRad) * (parentRadius + getOuterRadius());
    const double cmZ = std::sin(m_angleOffsetRad) * (parentRadius + getOuterRadius());
    return Coordinate{cmX, cmY, cmZ, getComponentMass()};
}

double LaunchLug::getLongitudinalUnitInertia() const
{
    // 1/12 * (3 * (r2^2 + r1^2) + h^2)
    return ((3 * (MathUtil::pow2(getOuterRadius()) + MathUtil::pow2(getInnerRadius()))) +
            MathUtil::pow2(getLength())) /
           12;
}

double LaunchLug::getRotationalUnitInertia() const
{
    // 1/2 * (r1^2 + r2^2)
    return (MathUtil::pow2(getInnerRadius()) + MathUtil::pow2(getOuterRadius())) / 2;
}

BoundingBox LaunchLug::getInstanceBoundingBox() const
{
    BoundingBox instanceBounds;

    instanceBounds.update(Coordinate{getLength(), 0, 0});

    const double r = getOuterRadius();
    instanceBounds.update(Coordinate{0, r, r});
    instanceBounds.update(Coordinate{0, -r, -r});

    return instanceBounds;
}

bool LaunchLug::allowsChildren() const
{
    return false;
}

bool LaunchLug::isCompatible(ComponentKind /*kind*/) const
{
    // Allow nothing to be attached to a LaunchLug
    return false;
}

// ================================================================================= preset

void LaunchLug::loadFromPreset(const ComponentPreset& preset, const PresetLoadOptions& options)
{
    Tube::loadFromPreset(preset, options);

    if (preset.has(ComponentPreset::kOuterDiameter))
    {
        const double outerDiameter = preset.get(ComponentPreset::kOuterDiameter);
        m_radius                   = outerDiameter / 2.0;
        if (preset.has(ComponentPreset::kInnerDiameter))
        {
            const double innerDiameter = preset.get(ComponentPreset::kInnerDiameter);
            m_thickness                = (outerDiameter - innerDiameter) / 2.0;
        }
    }

    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

}  // namespace QtRocket
