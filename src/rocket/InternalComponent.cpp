#include "QtRocket/rocket/InternalComponent.h"

#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AxialMethod.h"

namespace QtRocket
{

InternalComponent::InternalComponent() : RocketComponent(AxialMethod::BOTTOM) { }

void InternalComponent::setAxialMethod(AxialMethod newAxialMethod)
{
    RocketComponent::setAxialMethod(newAxialMethod);
    fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
}

double InternalComponent::getAxialOffset() const
{
    return RocketComponent::getAxialOffset();
}

void InternalComponent::setAxialOffset(double newOffset)
{
    RocketComponent::setAxialOffset(newOffset);
}

bool InternalComponent::isAerodynamic() const
{
    return false;
}

bool InternalComponent::isMassive() const
{
    return true;
}

}  // namespace QtRocket
