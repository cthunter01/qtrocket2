#include "QtRocket/rocket/RadiusRingComponent.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/InternalComponent.h"
#include "QtRocket/rocket/RingComponent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

void RadiusRingComponent::loadFromPreset(const ComponentPreset&   preset,
                                         const PresetLoadOptions& options)
{
    RingComponent::loadFromPreset(preset, options);

    if (preset.has(ComponentPreset::kOuterDiameter))
    {
        m_outerRadius          = preset.get(ComponentPreset::kOuterDiameter) / 2.0;
        m_outerRadiusAutomatic = false;
    }
    m_innerRadiusAutomatic = false;
    if (preset.has(ComponentPreset::kInnerDiameter))
    {
        m_innerRadius = preset.get(ComponentPreset::kInnerDiameter) / 2.0;
    }

    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

double RadiusRingComponent::getOuterRadius() const
{
    if (m_outerRadiusAutomatic)
    {
        if (const std::optional<double> radius = parentInnerRadius())
        {
            m_outerRadius = *radius;
        }
    }
    return m_outerRadius;
}

void RadiusRingComponent::setOuterRadius(double r)
{
    r = MathUtil::javaMax(r, 0.0);

    if (MathUtil::equals(m_outerRadius, r) && !isOuterRadiusAutomatic())
    {
        return;
    }

    m_outerRadius          = r;
    m_outerRadiusAutomatic = false;
    if (getInnerRadius() > r)
    {
        m_innerRadius          = r;
        m_innerRadiusAutomatic = false;
    }

    clearPreset();
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

double RadiusRingComponent::getInnerRadius() const
{
    return m_innerRadius;
}

void RadiusRingComponent::setInnerRadius(double r)
{
    r = MathUtil::javaMax(r, 0.0);

    if (MathUtil::equals(m_innerRadius, r))
    {
        return;
    }

    m_innerRadius          = r;
    m_innerRadiusAutomatic = false;
    if (getOuterRadius() < r)
    {
        m_outerRadius          = r;
        m_outerRadiusAutomatic = false;
    }

    clearPreset();
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

double RadiusRingComponent::getThickness() const
{
    return MathUtil::javaMax(getOuterRadius() - getInnerRadius(), 0.0);
}

void RadiusRingComponent::setThickness(double thickness)
{
    const double outer = getOuterRadius();

    thickness = MathUtil::clamp(thickness, 0, outer);
    setInnerRadius(outer - thickness);
}

void RadiusRingComponent::setInstanceSeparation(double separation)
{
    if (MathUtil::equals(m_instanceSeparation, separation))
    {
        return;
    }
    m_instanceSeparation = separation;
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

std::vector<Coordinate> RadiusRingComponent::getInstanceLocations() const
{
    return RocketComponent::getInstanceLocations();
}

std::vector<Coordinate> RadiusRingComponent::getInstanceOffsets() const
{
    std::vector<Coordinate> offsets;
    offsets.reserve(static_cast<std::size_t>(std::max(getInstanceCount(), 0)));
    for (int index = 0; index < getInstanceCount(); index++)
    {
        offsets.emplace_back(index * m_instanceSeparation, 0, 0);
    }
    return offsets;
}

void RadiusRingComponent::setInstanceCount(int newCount)
{
    if (newCount == m_instanceCount || newCount <= 0)
    {
        return;
    }
    m_instanceCount = newCount;
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

std::string RadiusRingComponent::getPatternName() const
{
    return std::to_string(getInstanceCount()) + "-Line";
}

AxialMethod RadiusRingComponent::getAxialMethod() const
{
    return InternalComponent::getAxialMethod();
}

void RadiusRingComponent::setAxialMethod(AxialMethod newAxialMethod)
{
    InternalComponent::setAxialMethod(newAxialMethod);
}

double RadiusRingComponent::getAxialOffset() const
{
    return InternalComponent::getAxialOffset();
}

void RadiusRingComponent::setAxialOffset(double newOffset)
{
    InternalComponent::setAxialOffset(newOffset);
}

}  // namespace QtRocket
