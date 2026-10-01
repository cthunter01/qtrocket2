#pragma once

#include <memory>

#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RadiusRingComponent.h"
#include "QtRocket/rocket/RocketComponent.h"

namespace QtRocket
{

/// A bulkhead (OpenRocket's Bulkhead): a solid disc across the body. A new one has an automatic
/// outer radius and is 2 mm long; its inner radius is always 0. It holds no children.
class Bulkhead : public RadiusRingComponent
{
public:
    using RocketComponent::isCompatible;

    /// A bulkhead with an automatic outer radius, 2 mm long, positioned BOTTOM.
    Bulkhead();

    [[nodiscard]] ComponentKind kind() const noexcept override { return ComponentKind::BULKHEAD; }

    /// Always 0.
    [[nodiscard]] double getInnerRadius() const override;

    /// Does nothing: a bulkhead is solid.
    void setInnerRadius(double r) override;

    /// Makes the outer radius automatic or not (public here; fires MASS_CHANGE when it changes),
    /// then clears the preset (always).
    // NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method): Java's override
    void setOuterRadiusAutomatic(bool automatic);

    /// Always false.
    [[nodiscard]] bool allowsChildren() const override;

    /// Always false.
    [[nodiscard]] bool isCompatible(ComponentKind kind) const override;

protected:
    [[nodiscard]] std::unique_ptr<RocketComponent> cloneShallow() const override;
};

}  // namespace QtRocket
