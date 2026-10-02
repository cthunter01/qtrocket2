#include "QtRocket/rocket/ThicknessRingComponent.h"

#include <optional>

#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/RingComponent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

void ThicknessRingComponent::loadFromPreset(const ComponentPreset&   preset,
                                            const PresetLoadOptions& options)
{
    RingComponent::loadFromPreset(preset, options);
    if (preset.has(ComponentPreset::kOuterDiameter))
    {
        m_outerRadiusAutomatic     = false;
        m_innerRadiusAutomatic     = false;
        const double outerDiameter = preset.get(ComponentPreset::kOuterDiameter);
        m_outerRadius              = outerDiameter / 2.0;
        if (preset.has(ComponentPreset::kInnerDiameter))
        {
            const double innerDiameter = preset.get(ComponentPreset::kInnerDiameter);
            m_thickness                = (outerDiameter - innerDiameter) / 2.0;
        }
    }

    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

double ThicknessRingComponent::getOuterRadius() const
{
    if (isOuterRadiusAutomatic())
    {
        if (const std::optional<double> radius = parentInnerRadius())
        {
            m_outerRadius = *radius;
        }
    }
    return m_outerRadius;
}

void ThicknessRingComponent::setOuterRadius(double r)
{
    r = MathUtil::javaMax(r, 0.0);
    if (MathUtil::equals(m_outerRadius, r) && !isOuterRadiusAutomatic())
    {
        return;
    }

    m_outerRadius          = r;
    m_outerRadiusAutomatic = false;

    // Java's if (thickness > outerRadius) thickness = outerRadius, NaN and signed zeros
    // included.
    m_thickness = (m_thickness > m_outerRadius) ? m_outerRadius : m_thickness;

    clearPreset();
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

double ThicknessRingComponent::getThickness() const
{
    return MathUtil::javaMin(m_thickness, getOuterRadius());
}

void ThicknessRingComponent::setThickness(double thickness)
{
    const double outer = getOuterRadius();

    thickness = MathUtil::clamp(thickness, 0, outer);
    if (MathUtil::equals(getThickness(), thickness))
    {
        return;
    }

    m_thickness = thickness;
    clearPreset();
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

double ThicknessRingComponent::getInnerRadius() const
{
    return MathUtil::javaMax(getOuterRadius() - m_thickness, 0.0);
}

void ThicknessRingComponent::setInnerRadius(double r)
{
    r = MathUtil::javaMax(r, 0.0);
    setThickness(getOuterRadius() - r);
}

}  // namespace QtRocket
