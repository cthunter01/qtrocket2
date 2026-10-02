#pragma once

#include <memory>

#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/ThicknessRingComponent.h"

namespace QtRocket
{

/// An engine block (OpenRocket's EngineBlock): a thick ring that stops the motor from sliding
/// forward in its mount. A new one has an automatic outer radius and is 5 mm long; its thickness
/// stays 0, as in OpenRocket, whose constructor sets 5 mm while the outer radius is still 0 (see
/// TubeCoupler). It holds no children.
class EngineBlock : public ThicknessRingComponent
{
public:
    using RocketComponent::isCompatible;

    /// A block with an automatic outer radius, 5 mm long, positioned BOTTOM.
    EngineBlock();

    [[nodiscard]] ComponentKind kind() const noexcept override
    {
        return ComponentKind::ENGINE_BLOCK;
    }

    /// Makes the outer radius automatic or not (public here); fires MASS_CHANGE when it changes.
    using RingComponent::setOuterRadiusAutomatic;

    /// Always false.
    [[nodiscard]] bool allowsChildren() const override;

    /// Always false.
    [[nodiscard]] bool isCompatible(ComponentKind kind) const override;

protected:
    [[nodiscard]] std::unique_ptr<RocketComponent> cloneShallow() const override;
};

}  // namespace QtRocket
