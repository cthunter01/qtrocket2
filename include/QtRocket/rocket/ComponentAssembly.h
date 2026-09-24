#pragma once

#include <vector>

#include "QtRocket/rocket/BoxBounded.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/AxialPositionable.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket
{

/// The base of the component assemblies: rocket, stages, booster sets and pod sets (OpenRocket's
/// ComponentAssembly). An assembly has no mass, bounds or aerodynamics of its own; its length is
/// the sum of its AFTER-positioned children's lengths (updateBounds()). A mass or CG override
/// on an assembly covers its subcomponents as well as itself.
class ComponentAssembly : public RocketComponent,
                          public virtual AxialPositionable,
                          public virtual BoxBounded
{
public:
    using RocketComponent::getAxialOffset;

    /// Assemblies always accept children (subject to isCompatible()).
    [[nodiscard]] bool allowsChildren() const override;

    /// The offset for the current method, computed from the position (not the stored offset).
    [[nodiscard]] double getAxialOffset() const override;

    /// No bounds of its own (empty).
    [[nodiscard]] std::vector<Coordinate> getComponentBounds() const override;

    /// No mass of its own: (0, 0, 0) with weight 0.
    [[nodiscard]] Coordinate getComponentCG() const override;

    /// No mass of its own: 0.
    [[nodiscard]] double getComponentMass() const override;

    /// An empty box: an assembly has no physical extent of its own.
    [[nodiscard]] BoundingBox getInstanceBoundingBox() const override;

    [[nodiscard]] double getLongitudinalUnitInertia() const override;
    [[nodiscard]] double getRotationalUnitInertia() const override;

    /// The largest outer radius among the direct children that are body tubes or transitions
    /// (a transition counts with the larger of its fore and aft radii); 0 without any.
    [[nodiscard]] virtual double getBoundingRadius() const;

    /// False: an assembly has no aerodynamic effect of its own.
    [[nodiscard]] bool isAerodynamic() const override;

    /// False, even though the override values may have an effect.
    [[nodiscard]] bool isMassive() const override;

    /// False for exactly two instances (a symmetric pair is not axisymmetric).
    [[nodiscard]] bool isAxisymmetric() const override;

    /// Updates the bounds, sets the offset for the current method and fires AEROMASS_CHANGE.
    void setAxialOffset(double newOffset) override;

    /// The method of this assembly: a booster set or pod set takes TOP for AFTER (they cannot be
    /// positioned AFTER), an axial stage is always AFTER, and anything else is a bug. Fires
    /// NONFUNCTIONAL_CHANGE.
    /// @throws BugError without a parent, and for an unknown assembly kind (Java:
    ///         NullPointerException, BugException).
    void setAxialMethod(AxialMethod newMethod) override;

    [[nodiscard]] AxialMethod getAxialMethod() const override
    {
        return RocketComponent::getAxialMethod();
    }

    /// Recomputes the length: the sum of the lengths of the children positioned AFTER.
    void updateBounds() override;

    /// Updates the bounds, positions this assembly (after its sibling when it is AFTER), then
    /// places the children positioned AFTER one after the other.
    void update() override;

protected:
    using RocketComponent::setAxialOffset;

    /// An assembly positioned by @p axialMethod (AFTER by default).
    explicit ComponentAssembly(AxialMethod axialMethod = AxialMethod::AFTER);

    /// Calls setAfter() on every child positioned AFTER, in order.
    void updateChildSequence();
};

}  // namespace QtRocket
