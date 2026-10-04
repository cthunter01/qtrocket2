#include "QtRocket/rocket/TubeFinSet.h"

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
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket
{

TubeFinSet::TubeFinSet()
  : Tube(AxialMethod::BOTTOM), m_finRotation(Transformation::rotateX(2 * std::numbers::pi / m_fins))
{
    m_length = 0.10;
    setDisplayOrderSide(3);  // Order for displaying the component in the 2D side view
    setDisplayOrderBack(3);  // Order for displaying the component in the 2D back view
}

std::unique_ptr<RocketComponent> TubeFinSet::cloneShallow() const
{
    return std::make_unique<TubeFinSet>(*this);
}

void TubeFinSet::setLength(double length)
{
    if (MathUtil::equals(m_length, length))
    {
        return;
    }
    m_length = length;
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

// ================================================================================== radii

double TubeFinSet::getOuterRadius() const
{
    if (m_autoRadius)
    {
        if (m_fins < 3)
        {
            return getBodyRadius();
        }
        return getTouchingRadius();
    }
    return m_outerRadius;
}

double TubeFinSet::getTubeSeparation() const
{
    return 2.0 * (getTouchingRadius() - getOuterRadius());
}

double TubeFinSet::getTouchingRadius() const
{
    double       r      = getBodyRadius();
    const double finSep = std::numbers::pi / m_fins;

    r *= std::sin(finSep) / (1.0 - std::sin(finSep));

    return r;
}

void TubeFinSet::setOuterRadius(double radius)
{
    if ((m_outerRadius == radius) && !m_autoRadius)
    {
        return;
    }

    m_autoRadius  = false;
    m_outerRadius = MathUtil::javaMax(radius, 0);

    // Java's comparison (not a minimum): a NaN thickness, or a NaN radius, leaves it alone.
    const bool thickerThanTheRadius = m_thickness > m_outerRadius;
    if (thickerThanTheRadius)
    {
        m_thickness = m_outerRadius;
    }
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
    clearPreset();
}

void TubeFinSet::setOuterRadiusAutomatic(bool automatic)
{
    if (m_autoRadius == automatic)
    {
        return;
    }

    m_autoRadius = automatic;
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
    clearPreset();
}

double TubeFinSet::getInnerRadius() const
{
    return MathUtil::javaMax(getOuterRadius() - m_thickness, 0);
}

void TubeFinSet::setInnerRadius(double r)
{
    setThickness(getOuterRadius() - r);
}

double TubeFinSet::getThickness() const
{
    return MathUtil::javaMin(m_thickness, getOuterRadius());
}

void TubeFinSet::setThickness(double thickness)
{
    if (m_thickness == thickness)
    {
        return;
    }

    m_thickness = MathUtil::clamp(thickness, 0, getOuterRadius());
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
    clearPreset();
}

double TubeFinSet::getBodyRadius() const
{
    const double x = getPosition().x;
    for (const RocketComponent* s = getParent(); s != nullptr; s = s->getParent())
    {
        if (const auto* symmetric = dynamic_cast<const SymmetricComponent*>(s))
        {
            return symmetric->getRadius(x);
        }
    }
    return 0;
}

// ================================================================================== tubes

void TubeFinSet::setFinCount(int n)
{
    if (m_fins == n)
    {
        return;
    }
    m_fins        = MathUtil::clamp(n, 1, 8);
    m_finRotation = Transformation::rotateX(2 * std::numbers::pi / m_fins);
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

double TubeFinSet::getBaseRotation() const
{
    return getAngleOffset();
}

void TubeFinSet::setBaseRotation(double r)
{
    setAngleOffset(r);
}

double TubeFinSet::getFinRotation() const
{
    return 2 * std::numbers::pi / m_fins;
}

// =============================================================================== position

void TubeFinSet::setAxialMethod(AxialMethod newMethod)
{
    RocketComponent::setAxialMethod(newMethod);
    fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
}

double TubeFinSet::getAxialOffset() const
{
    return RocketComponent::getAxialOffset();
}

void TubeFinSet::setAxialOffset(double newOffset)
{
    RocketComponent::setAxialOffset(newOffset);
}

bool TubeFinSet::isAfter() const
{
    return false;
}

double TubeFinSet::getAngleOffset() const
{
    return m_firstFinOffsetRadians;
}

void TubeFinSet::setAngleOffset(double angle)
{
    const double reducedAngle = MathUtil::reducePi(angle);
    if (MathUtil::equals(reducedAngle, m_firstFinOffsetRadians))
    {
        return;
    }
    m_firstFinOffsetRadians = reducedAngle;

    if (MathUtil::equals(m_firstFinOffsetRadians, 0))
    {
        m_baseRotation = Transformation::kIdentity;
    }
    else
    {
        m_baseRotation = Transformation::rotateX(m_firstFinOffsetRadians);
    }

    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

AngleMethod TubeFinSet::getAngleMethod() const
{
    return m_angleMethod;
}

void TubeFinSet::setAngleMethod(AngleMethod newMethod)
{
    m_angleMethod = newMethod;
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

double TubeFinSet::getBoundingRadius() const
{
    return getBodyRadius() + getOuterRadius();
}

double TubeFinSet::getRadiusOffset() const
{
    return RocketComponent::getRadiusOffset();
}

void TubeFinSet::setRadiusOffset(double /*radius*/)
{
    // Does nothing, as in Java.
}

RadiusMethod TubeFinSet::getRadiusMethod() const
{
    return RocketComponent::getRadiusMethod();
}

void TubeFinSet::setRadiusMethod(RadiusMethod /*method*/)
{
    // Does nothing, as in Java.
}

void TubeFinSet::setRadius(RadiusMethod /*method*/, double /*radius*/)
{
    // Does nothing, as in Java.
}

// ============================================================================== instances

int TubeFinSet::getInstanceCount() const
{
    return getFinCount();
}

void TubeFinSet::setInstanceCount(int newCount)
{
    setFinCount(newCount);
}

std::vector<Coordinate> TubeFinSet::getInstanceLocations() const
{
    return RocketComponent::getInstanceLocations();
}

std::string TubeFinSet::getPatternName() const
{
    return std::to_string(getInstanceCount()) + "-tubefin-ring";
}

double TubeFinSet::getInstanceAngleIncrement() const
{
    return 2 * std::numbers::pi / getFinCount();
}

std::vector<double> TubeFinSet::getInstanceAngles() const
{
    const double angleIncrementRadians = getInstanceAngleIncrement();

    std::vector<double> result;
    result.reserve(static_cast<std::size_t>(std::max(getFinCount(), 0)));
    for (int finNumber = 0; finNumber < getFinCount(); ++finNumber)
    {
        const double additionalOffset = angleIncrementRadians * finNumber;
        result.push_back(MathUtil::reduce2Pi(m_firstFinOffsetRadians + additionalOffset));
    }

    return result;
}

std::vector<Coordinate> TubeFinSet::getInstanceOffsets() const
{
    const double bodyRadius = getBodyRadius();

    // already includes the base rotation
    const std::vector<double> angles = getInstanceAngles();

    std::vector<Coordinate> toReturn;
    toReturn.reserve(angles.size());
    for (const double angle : angles)
    {
        const Coordinate raw{0, bodyRadius, 0};
        toReturn.push_back(Transformation::axialRotation(angle).transform(raw));
    }

    return toReturn;
}

// ========================================================================== mass and bounds

double TubeFinSet::getComponentVolume() const
{
    const double outer  = getOuterRadius();
    const double inner  = getInnerRadius();
    double       volume = (outer * outer) - (inner * inner);
    volume *= std::numbers::pi;
    volume *= m_length;
    volume *= m_fins;
    return volume;
}

Coordinate TubeFinSet::getComponentCG() const
{
    const double mass       = getComponentMass();  // safe
    const double halflength = m_length / 2;

    if (m_fins == 1)
    {
        return m_baseRotation.transform(
            Coordinate{halflength, getOuterRadius() + getBodyRadius(), 0, mass});
    }
    return m_baseRotation.transform(Coordinate{halflength, 0, 0, mass});
}

double TubeFinSet::getLongitudinalUnitInertia() const
{
    // Longitudinal Unit Inertia for a single tube fin.
    // 1/12 * (3 * (r1^2 + r2^2) + h^2)
    const double inertia =
        ((3 * (MathUtil::pow2(getOuterRadius()) + MathUtil::pow2(getInnerRadius()))) +
         MathUtil::pow2(getLength())) /
        12;
    if (m_fins == 1)
    {
        return inertia;
    }

    // translate each to the center of mass.
    double totalInertia = 0.0;
    for (int i = 0; i < m_fins; i++)
    {
        totalInertia += inertia + MathUtil::pow2(m_axialOffset);
    }
    return totalInertia;
}

double TubeFinSet::getRotationalUnitInertia() const
{
    // The rotational inertia of a single fin about its center.
    // 1/2 * (r1^2 + r2^2)
    const double icentermass =
        (MathUtil::pow2(getInnerRadius()) + MathUtil::pow2(getOuterRadius())) / 2;
    if (m_fins == 1)
    {
        return icentermass;
    }
    // Use parallel axis rule and multiply by number of fins.
    return m_fins * (icentermass + MathUtil::pow2(getOuterRadius()) + getBodyRadius());
}

std::vector<Coordinate> TubeFinSet::getComponentBounds() const
{
    std::vector<Coordinate> bounds;

    addBound(bounds, 0, 2 * getBoundingRadius());
    addBound(bounds, m_length, 2 * getBoundingRadius());

    return bounds;
}

BoundingBox TubeFinSet::getInstanceBoundingBox() const
{
    BoundingBox box;
    box.update(Coordinate{0, -getOuterRadius(), -getOuterRadius()});
    box.update(Coordinate{m_length, getOuterRadius(), getOuterRadius()});
    return box;
}

bool TubeFinSet::allowsChildren() const
{
    return false;
}

bool TubeFinSet::isCompatible(ComponentKind /*kind*/) const
{
    return false;
}

// ================================================================================= preset

void TubeFinSet::loadFromPreset(const ComponentPreset& preset, const PresetLoadOptions& options)
{
    Tube::loadFromPreset(preset, options);

    if (preset.has(ComponentPreset::kOuterDiameter))
    {
        m_autoRadius               = false;
        const double outerDiameter = preset.get(ComponentPreset::kOuterDiameter);
        m_outerRadius              = outerDiameter / 2.0;
        if (preset.has(ComponentPreset::kInnerDiameter))
        {
            const double innerDiameter = preset.get(ComponentPreset::kInnerDiameter);
            m_thickness                = (outerDiameter - innerDiameter) / 2.0;
        }
    }
}

}  // namespace QtRocket
