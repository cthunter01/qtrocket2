#pragma once

#include <memory>

#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RadiusRingComponent.h"
#include "QtRocket/rocket/RocketComponent.h"

namespace QtRocket
{

/// A centering ring (OpenRocket's CenteringRing): a ring between a body tube and an inner tube.
/// A new one has automatic outer and inner radii and is 2 mm long. The automatic outer radius is
/// the parent's inner radius (RadiusRingComponent); the automatic inner radius is the largest
/// outer radius of the sibling inner tubes the ring overlaps axially, at most the outer radius
/// (0 without such a tube), and getInnerRadius() stores it, as Java's getter does. It holds no
/// children.
class CenteringRing : public RadiusRingComponent
{
public:
    using RocketComponent::isCompatible;

    /// A ring with automatic radii, 2 mm long, positioned BOTTOM.
    CenteringRing();

    [[nodiscard]] ComponentKind kind() const noexcept override
    {
        return ComponentKind::CENTERING_RING;
    }

    /// The inner radius. When it is automatic, it is first recomputed and stored: the largest
    /// outer radius of the sibling InnerTubes that overlap the ring along the axis (a tube counts
    /// unless the ring ends before the tube's front or starts after its end, both measured from
    /// the tube's first instance), at most getOuterRadius(); 0 without such a tube or without a
    /// parent.
    [[nodiscard]] double getInnerRadius() const override;

    /// Makes the outer radius automatic or not (public here); fires MASS_CHANGE when it changes.
    using RingComponent::setOuterRadiusAutomatic;

    /// Makes the inner radius automatic or not (public here); fires MASS_CHANGE when it changes.
    using RingComponent::setInnerRadiusAutomatic;

    /// Always false.
    [[nodiscard]] bool allowsChildren() const override;

    /// Always false.
    [[nodiscard]] bool isCompatible(ComponentKind kind) const override;

protected:
    [[nodiscard]] std::unique_ptr<RocketComponent> cloneShallow() const override;
};

}  // namespace QtRocket
