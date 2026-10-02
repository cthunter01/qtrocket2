#include "QtRocket/rocket/TubeCoupler.h"

#include <memory>

#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/ThicknessRingComponent.h"

namespace QtRocket
{

TubeCoupler::TubeCoupler()
{
    setOuterRadiusAutomatic(true);
    setThickness(0.002);
    setLength(0.06);
    setDisplayOrderSide(6);   // Order for displaying the component in the 2D side view
    setDisplayOrderBack(13);  // Order for displaying the component in the 2D back view
}

std::unique_ptr<RocketComponent> TubeCoupler::cloneShallow() const
{
    return std::make_unique<TubeCoupler>(*this);
}

bool TubeCoupler::allowsChildren() const
{
    return true;
}

bool TubeCoupler::isCompatible(ComponentKind kind) const
{
    return isInternal(kind);
}

double TubeCoupler::getInnerRadius(double /*x*/) const
{
    return getInnerRadius();
}

double TubeCoupler::getOuterRadius(double /*x*/) const
{
    return getOuterRadius();
}

double TubeCoupler::getLength() const
{
    return ThicknessRingComponent::getLength();
}

}  // namespace QtRocket
