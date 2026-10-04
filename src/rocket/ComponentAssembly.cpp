#include "QtRocket/rocket/ComponentAssembly.h"

#include <vector>

#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

ComponentAssembly::ComponentAssembly(AxialMethod axialMethod) : RocketComponent(axialMethod) { }

bool ComponentAssembly::allowsChildren() const
{
    return true;
}

double ComponentAssembly::getAxialOffset() const
{
    return getAxialOffset(m_axialMethod);
}

std::vector<Coordinate> ComponentAssembly::getComponentBounds() const
{
    return {};
}

Coordinate ComponentAssembly::getComponentCG() const
{
    return Coordinate::kZero;
}

double ComponentAssembly::getComponentMass() const
{
    return 0;
}

BoundingBox ComponentAssembly::getInstanceBoundingBox() const
{
    return BoundingBox{};
}

double ComponentAssembly::getLongitudinalUnitInertia() const
{
    return 0;
}

double ComponentAssembly::getRotationalUnitInertia() const
{
    return 0;
}

double ComponentAssembly::getBoundingRadius() const
{
    double outerRadius = 0;
    for (const auto& comp : m_children)
    {
        double thisRadius = 0;
        if (const auto* tube = dynamic_cast<const BodyTube*>(comp.get()))
        {
            thisRadius = tube->getOuterRadius();
        }
        else if (const auto* trans = dynamic_cast<const Transition*>(comp.get()))
        {
            // Fore before aft, as Java reads them (reading an automatic radius refreshes it).
            const double fore = trans->getForeRadius();
            thisRadius        = MathUtil::javaMax(fore, trans->getAftRadius());
        }

        // Java's Math.max: a NaN radius makes the result NaN.
        outerRadius = MathUtil::javaMax(outerRadius, thisRadius);
    }
    return outerRadius;
}

bool ComponentAssembly::isAerodynamic() const
{
    return false;
}

bool ComponentAssembly::isMassive() const
{
    return false;
}

bool ComponentAssembly::isAxisymmetric() const
{
    return 2 != getInstanceCount();
}

void ComponentAssembly::setAxialOffset(double newOffset)
{
    updateBounds();
    RocketComponent::setAxialOffset(m_axialMethod, newOffset);
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

void ComponentAssembly::setAxialMethod(AxialMethod newMethod)
{
    if (nullptr == m_parent)
    {
        bug(" a Stage requires a parent before any positioning! ");
    }
    if (kind() == ComponentKind::PARALLEL_STAGE || kind() == ComponentKind::POD_SET)
    {
        if (AxialMethod::AFTER == newMethod)
        {
            // Stages or pods cannot be positioned AFTER other stages (Java logs a warning).
            RocketComponent::setAxialMethod(AxialMethod::TOP);
        }
        else
        {
            RocketComponent::setAxialMethod(newMethod);
        }
    }
    else if (kind() == ComponentKind::AXIAL_STAGE)
    {
        // Centerline stages are positioned AFTER, whatever was requested.
        RocketComponent::setAxialMethod(AxialMethod::AFTER);
    }
    else
    {
        bug("Unrecognized subclass of Component Assembly.  Please update this method.");
    }
    fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
}

void ComponentAssembly::update()
{
    updateBounds();
    if (isAfter())
    {
        setAfter();
    }
    else
    {
        RocketComponent::update();
    }

    updateChildSequence();
}

void ComponentAssembly::updateBounds()
{
    // Only the length, for now.
    m_length = 0;
    for (const auto& child : m_children)
    {
        if (child->isAfter())
        {
            m_length += child->getLength();
        }
    }
}

void ComponentAssembly::updateChildSequence()
{
    for (const auto& child : m_children)
    {
        if (AxialMethod::AFTER == child->getAxialMethod())
        {
            child->setAfter();
        }
    }
}

}  // namespace QtRocket
