#pragma once

#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/AxialPositionable.h"

namespace QtRocket
{

/// A component inside the rocket (OpenRocket's InternalComponent): it has no effect on the
/// aerodynamics, only on the mass (whether it actually lies inside an external component is not
/// checked). An internal component is positioned relative to its parent, internal or external,
/// or absolutely; a new one is positioned BOTTOM.
///
/// The ring components (RingComponent) and the mass objects (MassObject) derive from it.
class InternalComponent : public RocketComponent, public virtual AxialPositionable
{
public:
    using RocketComponent::getAxialOffset;

    // ---- AxialPositionable (RocketComponent's, see AxialPositionable)

    [[nodiscard]] AxialMethod getAxialMethod() const override
    {
        return RocketComponent::getAxialMethod();
    }

    /// Changes how the position is described, keeping the position
    /// (RocketComponent::setAxialMethod()), and fires NONFUNCTIONAL_CHANGE, also when the method
    /// does not change (as Java).
    void setAxialMethod(AxialMethod newAxialMethod) override;

    /// The stored offset for getAxialMethod().
    [[nodiscard]] double getAxialOffset() const override;

    /// Sets the offset for the current method and fires AEROMASS_CHANGE.
    void setAxialOffset(double newOffset) override;

    // ---- RocketComponent

    /// False: internal components have no aerodynamic effect.
    [[nodiscard]] bool isAerodynamic() const final;

    /// True: internal components have mass.
    [[nodiscard]] bool isMassive() const final;

protected:
    using RocketComponent::setAxialOffset;

    /// A component positioned BOTTOM.
    InternalComponent();
};

}  // namespace QtRocket
