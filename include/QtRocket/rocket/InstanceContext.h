#pragma once

#include <cstdint>
#include <string>

#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket
{

class RocketComponent;

/// One physical instance of a component in a flight configuration (OpenRocket's
/// InstanceContext): the component, the instance's number relative to its parent, the
/// transformation from the component's frame to the rocket's, and the transformation of the
/// parent instance it belongs to. FlightConfiguration::updateActiveInstances() makes them, one per
/// instance of every active component and every instance of its parents, in an InstanceMap.
///
/// The fields are public, as in Java (parentTransform is private there, with a getter). The
/// component is a non-owning pointer, never null; a context is only valid while its component is
/// in the tree it was made from (the maps are rebuilt on every change event, and the Rocket drops
/// the contexts of a removed subtree at once; see FlightConfiguration).
class InstanceContext
{
public:
    /// A context for instance @p number of @p instanceOf, placed by @p toRocket, within the
    /// parent instance placed by @p parentToRocket.
    InstanceContext(RocketComponent& instanceOf, int number, const Transformation& toRocket,
                    const Transformation& parentToRocket = Transformation::kIdentity) noexcept
      : component(&instanceOf),
        instanceNumber(number),
        transform(toRocket),
        parentTransform(parentToRocket)
    {
    }

    /// The instance's location in the rocket's frame: the transformed origin.
    [[nodiscard]] Coordinate getLocation() const noexcept
    {
        return transform.transform(Coordinate::kZero);
    }

    /// The transformation of the parent instance (from the parent's frame to the rocket's), with
    /// which a consumer can gather a component's own instances without descending the tree.
    [[nodiscard]] const Transformation& getParentTransform() const noexcept
    {
        return parentTransform;
    }

    /// "Context for <component> #<instance number>" (the component's toString()).
    [[nodiscard]] std::string toString() const;

    /// Java's equals(): components that equals() (the same class and id) and equal
    /// transformations; the instance numbers and parent transformations do not take part.
    [[nodiscard]] bool operator==(const InstanceContext& other) const noexcept;

    /// Java's hashCode(): the component's.
    [[nodiscard]] std::int32_t hashCode() const noexcept;

    /// The component this is an instance of (Java: component).
    RocketComponent* component;
    /// The instance number relative to the parent (Java: instanceNumber).
    int instanceNumber;
    /// From the component's frame to the rocket's (Java: transform).
    Transformation transform;
    /// From the parent's frame to the rocket's (Java: the private parentTransform).
    Transformation parentTransform;
};

}  // namespace QtRocket
