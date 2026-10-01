#pragma once

#include "QtRocket/rocket/RingComponent.h"
#include "QtRocket/rocket/RocketComponent.h"

namespace QtRocket
{

class ComponentPreset;

/// A ring component whose dimensions are an outer radius and a wall thickness (OpenRocket's
/// ThicknessRingComponent): inner tubes, tube couplers and engine blocks. The inner radius
/// follows from them, and the thickness never exceeds the outer radius. An automatic outer
/// radius is the parent's inner radius (RingComponent::parentInnerRadius()); getOuterRadius()
/// stores it, as Java's getter does, so the field is mutable.
class ThicknessRingComponent : public RingComponent
{
public:
    // ---- Coaxial

    /// The outer radius: the automatic one when it is automatic and the parent is a
    /// RadialParent (and then stored), else the stored one.
    [[nodiscard]] double getOuterRadius() const override;

    /// Sets the outer radius (negative values become 0) and makes it manual; the thickness is
    /// cut to the new radius. Nothing happens when the radius is manual and equal (within
    /// MathUtil::equals()) to the stored one; otherwise the preset is cleared and MASS_CHANGE
    /// fires.
    void setOuterRadius(double r) override;

    /// The stored thickness, at most the outer radius.
    [[nodiscard]] double getThickness() const override;

    /// Sets the thickness, clamped to 0 ... the outer radius; when it differs (beyond
    /// MathUtil::equals()) from getThickness(), clears the preset and fires MASS_CHANGE.
    void setThickness(double thickness) override;

    /// The outer radius minus the stored thickness, at least 0.
    [[nodiscard]] double getInnerRadius() const override;

    /// Sets the thickness to the outer radius minus @p r (negative values become 0).
    void setInnerRadius(double r) override;

protected:
    ThicknessRingComponent() = default;

    /// The base class's values, then from an OUTER_DIAMETER: both radii manual, the outer radius
    /// half of it and, with an INNER_DIAMETER too, the thickness half their difference. Fires
    /// MASS_CHANGE (always).
    void loadFromPreset(const ComponentPreset& preset, const PresetLoadOptions& options) override;

    /// The outer radius (Java: outerRadius); mutable since getOuterRadius() refreshes it.
    mutable double m_outerRadius{0};
    /// The wall thickness (Java: thickness).
    double m_thickness{0};
};

}  // namespace QtRocket
