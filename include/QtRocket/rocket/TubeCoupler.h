#pragma once

#include <memory>

#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RadialParent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/ThicknessRingComponent.h"

namespace QtRocket
{

/// A tube coupler (OpenRocket's TubeCoupler): a tube joining two body tubes, holding internal
/// components. A new one has an automatic outer radius and is 60 mm long; its thickness stays 0,
/// as in OpenRocket, whose constructor sets 2 mm while the outer radius is still 0 (a detached
/// coupler has no parent to take the radius from), which the thickness is clamped to.
class TubeCoupler : public ThicknessRingComponent, public virtual RadialParent
{
public:
    using RocketComponent::isCompatible;
    using ThicknessRingComponent::getInnerRadius;
    using ThicknessRingComponent::getOuterRadius;

    /// A coupler with an automatic outer radius, 60 mm long, positioned BOTTOM.
    TubeCoupler();

    [[nodiscard]] ComponentKind kind() const noexcept override
    {
        return ComponentKind::TUBE_COUPLER;
    }

    /// Makes the outer radius automatic or not (public here); fires MASS_CHANGE when it changes.
    using RingComponent::setOuterRadiusAutomatic;

    /// Always true.
    [[nodiscard]] bool allowsChildren() const override;

    /// Accepts every internal component.
    [[nodiscard]] bool isCompatible(ComponentKind kind) const override;

    // ---- RadialParent

    /// getInnerRadius(): the inner radius is the same along the length.
    [[nodiscard]] double getInnerRadius(double x) const override;

    /// getOuterRadius().
    [[nodiscard]] double getOuterRadius(double x) const override;

    /// The length (RocketComponent's).
    [[nodiscard]] double getLength() const override;

protected:
    [[nodiscard]] std::unique_ptr<RocketComponent> cloneShallow() const override;
};

}  // namespace QtRocket
