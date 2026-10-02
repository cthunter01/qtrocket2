#pragma once

#include <string>
#include <vector>

#include "QtRocket/rocket/LineInstanceable.h"
#include "QtRocket/rocket/RingComponent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket
{

class ComponentPreset;

/// A ring component whose dimensions are an outer and an inner radius (OpenRocket's
/// RadiusRingComponent): centering rings and bulkheads. It can be instanced along the axis
/// (LineInstanceable): getInstanceCount() rings, each getInstanceSeparation() after the previous
/// one. An automatic outer radius is the parent's inner radius
/// (RingComponent::parentInnerRadius()); getOuterRadius() stores it, as Java's getter does, so
/// the radii are mutable (CenteringRing's automatic inner radius is stored too).
class RadiusRingComponent : public RingComponent, public virtual LineInstanceable
{
public:
    using InternalComponent::getAxialOffset;

    // ---- Coaxial

    /// The outer radius: the automatic one when it is automatic and the parent is a
    /// RadialParent (and then stored), else the stored one.
    [[nodiscard]] double getOuterRadius() const override;

    /// Sets the outer radius (negative values become 0) and makes it manual; an inner radius
    /// beyond it becomes manual and equal to it. Nothing happens when the outer radius is manual
    /// and equal (within MathUtil::equals()) to the stored one; otherwise the preset is cleared
    /// and MASS_CHANGE fires.
    void setOuterRadius(double r) override;

    /// The stored inner radius.
    [[nodiscard]] double getInnerRadius() const override;

    /// Sets the inner radius (negative values become 0) and makes it manual; an outer radius
    /// below it becomes manual and equal to it. Nothing happens when it equals (within
    /// MathUtil::equals()) the stored one; otherwise the preset is cleared and MASS_CHANGE
    /// fires.
    void setInnerRadius(double r) override;

    /// The outer radius minus the inner radius, at least 0.
    [[nodiscard]] double getThickness() const override;

    /// Sets the inner radius to the outer radius minus @p thickness clamped to 0 ... the outer
    /// radius.
    void setThickness(double thickness) override;

    // ---- LineInstanceable

    [[nodiscard]] double getInstanceSeparation() const override { return m_instanceSeparation; }

    /// Sets the distance between neighbouring instances (front to front, along +x); fires
    /// AEROMASS_CHANGE unless it equals (within MathUtil::equals()) the current one.
    void setInstanceSeparation(double separation) override;

    // ---- Instanceable

    [[nodiscard]] std::vector<Coordinate> getInstanceLocations() const override;

    /// (i * separation, 0, 0) for each instance i.
    [[nodiscard]] std::vector<Coordinate> getInstanceOffsets() const override;

    [[nodiscard]] int getInstanceCount() const override { return m_instanceCount; }

    /// Sets the number of instances; fires AEROMASS_CHANGE unless it is unchanged or not
    /// positive (then nothing happens).
    void setInstanceCount(int newCount) override;

    /// "<count>-Line".
    [[nodiscard]] std::string getPatternName() const override;

    // ---- AxialPositionable (InternalComponent's; re-declared because LineInstanceable brings
    // AxialPositionable along a second path, see AxialPositionable)

    [[nodiscard]] AxialMethod getAxialMethod() const override;
    void                      setAxialMethod(AxialMethod newAxialMethod) override;
    [[nodiscard]] double      getAxialOffset() const override;
    void                      setAxialOffset(double newOffset) override;

protected:
    using InternalComponent::setAxialOffset;

    RadiusRingComponent() = default;

    /// The base class's values, then from an OUTER_DIAMETER a manual outer radius of half of it;
    /// the inner radius always becomes manual, half the INNER_DIAMETER when there is one. Fires
    /// MASS_CHANGE (always).
    void loadFromPreset(const ComponentPreset& preset, const PresetLoadOptions& options) override;

    /// The outer radius (Java: outerRadius); mutable since getOuterRadius() refreshes it.
    mutable double m_outerRadius{0};
    /// The inner radius (Java: innerRadius); mutable since CenteringRing::getInnerRadius()
    /// refreshes it.
    mutable double m_innerRadius{0};
    /// The number of instances (Java: instanceCount).
    int m_instanceCount{1};
    /// The distance between the fronts of neighbouring instances (Java: instanceSeparation).
    double m_instanceSeparation{0};
};

}  // namespace QtRocket
