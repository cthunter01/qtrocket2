#include "QtRocket/rocket/Bulkhead.h"

#include <memory>

#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RingComponent.h"
#include "QtRocket/rocket/RocketComponent.h"

namespace QtRocket
{

Bulkhead::Bulkhead()
{
    setOuterRadiusAutomatic(true);
    setLength(0.002);
    setDisplayOrderSide(8);  // Order for displaying the component in the 2D side view
    setDisplayOrderBack(6);  // Order for displaying the component in the 2D back view
}

std::unique_ptr<RocketComponent> Bulkhead::cloneShallow() const
{
    return std::make_unique<Bulkhead>(*this);
}

double Bulkhead::getInnerRadius() const
{
    return 0;
}

void Bulkhead::setInnerRadius(double /*r*/)
{
    // A bulkhead has no inner radius.
}

void Bulkhead::setOuterRadiusAutomatic(bool automatic)
{
    RingComponent::setOuterRadiusAutomatic(automatic);
    clearPreset();
}

bool Bulkhead::allowsChildren() const
{
    return false;
}

bool Bulkhead::isCompatible(ComponentKind /*kind*/) const
{
    return false;
}

}  // namespace QtRocket
