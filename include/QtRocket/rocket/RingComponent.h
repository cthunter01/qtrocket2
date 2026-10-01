#pragma once

#include <optional>
#include <vector>

#include "QtRocket/rocket/BoxBounded.h"
#include "QtRocket/rocket/Coaxial.h"
#include "QtRocket/rocket/StructuralComponent.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket
{

/// An internal component shaped as a hollow cylinder (OpenRocket's RingComponent): inner tubes,
/// tube couplers, centering rings, bulkheads and engine blocks. It has inner and outer radii, a
/// length, and a radial position: a distance from the parent's axis in a direction (0 is the y
/// axis), kept also as the y and z shift.
///
/// The outer radius (and for a centering ring the inner one) can be automatic, taken from the
/// parent when it is a RadialParent (parentInnerRadius()); the subclasses ThicknessRingComponent
/// and RadiusRingComponent keep the dimensions, as an outer radius with a thickness or as two
/// radii. The getters of automatic dimensions store what they compute, as Java's do, so the
/// fields are mutable there.
///
/// The mass and CG are those of the ring (RocketComponent::ringMass()) times the instance count;
/// the CG of a single instance ignores its radial position, as in Java.
///
/// Not ported: the multi-edit config listeners (see RocketComponent).
class RingComponent : public StructuralComponent, public virtual BoxBounded, public virtual Coaxial
{
public:
    // ---- the dimensions (the radii and getThickness() are Coaxial's)

    /// Sets the wall thickness (how depends on the subclass).
    virtual void setThickness(double thickness) = 0;

    // ---- automatic dimensions

    /// Whether the outer radius is taken from the parent.
    [[nodiscard]] bool isOuterRadiusAutomatic() const noexcept { return m_outerRadiusAutomatic; }

    /// Whether the inner radius is automatic (only a centering ring makes it so).
    [[nodiscard]] bool isInnerRadiusAutomatic() const noexcept { return m_innerRadiusAutomatic; }

    // ---- length and radial position

    /// Sets the length (negative values become 0); when it changes, clears the preset and fires
    /// MASS_CHANGE.
    void setLength(double length);

    /// The direction of the radial displacement, in radians (0 is the y direction).
    [[nodiscard]] double getRadialDirection() const noexcept { return m_radialDirection; }

    /// Sets the direction, reduced to -pi ... pi; when it changes, updates the shift and fires
    /// MASS_CHANGE.
    void setRadialDirection(double dir);

    /// The distance of the component's center from the parent's axis.
    [[nodiscard]] double getRadialPosition() const noexcept { return m_radialPosition; }

    /// Sets the distance (negative values become 0); when it changes, updates the shift and fires
    /// MASS_CHANGE.
    void setRadialPosition(double pos);

    /// The y of the radial displacement: position * cos(direction).
    [[nodiscard]] double getRadialShiftY() const noexcept { return m_shiftY; }

    /// The z of the radial displacement: position * sin(direction).
    [[nodiscard]] double getRadialShiftZ() const noexcept { return m_shiftZ; }

    /// Sets the radial displacement to (@p y, @p z): the position becomes hypot(y, z) and the
    /// direction atan2(z, y), and the shift is recomputed from them. Fires MASS_CHANGE always.
    /// (Java also asserts that the recomputed shift equals (y, z), an assertion that is off in
    /// production and not ported.)
    void setRadialShift(double y, double z);

    // ---- BoxBounded

    /// The box from (0, -r, -r) to (length, r, r) with r the outer radius.
    [[nodiscard]] BoundingBox getInstanceBoundingBox() const override;

    // ---- mass, CG and bounds

    /// Four points around the axis at radius getOuterRadius() at the front and at the back.
    [[nodiscard]] std::vector<Coordinate> getComponentBounds() const override;

    /// The CG: (length / 2, 0, 0) with the mass of one instance as the weight for a single
    /// instance; otherwise the weighted average of the instance offsets, each weighted with the
    /// mass of one instance, moved by length / 2 along x.
    [[nodiscard]] Coordinate getComponentCG() const override;

    /// The mass of one ring (outer and inner radius, length, the material's density) times the
    /// instance count.
    [[nodiscard]] double getComponentMass() const override;

    [[nodiscard]] double getLongitudinalUnitInertia() const override;
    [[nodiscard]] double getRotationalUnitInertia() const override;

protected:
    RingComponent() = default;

    /// Sets whether the outer radius is automatic; fires MASS_CHANGE when it changes. Protected
    /// as in Java: the classes whose outer radius can be automatic make it public (with a
    /// using-declaration; Bulkhead's own version also clears the preset). Not virtual: unlike
    /// Java's, nothing calls it through a base class (it is protected there).
    void setOuterRadiusAutomatic(bool automatic);

    /// Sets whether the inner radius is automatic; fires MASS_CHANGE when it changes. Protected
    /// as in Java: CenteringRing makes it public. Not virtual, see setOuterRadiusAutomatic().
    void setInnerRadiusAutomatic(bool automatic);

    /// The automatic outer radius, shared by ThicknessRingComponent and RadiusRingComponent: the
    /// smaller of the parent's inner radii (RadialParent::getInnerRadius()) at this component's
    /// front and back, both positions taken relative to the parent's first instance and clamped
    /// to 0 ... the parent's length; nullopt when the parent is not a RadialParent (or there is
    /// none).
    [[nodiscard]] std::optional<double> parentInnerRadius() const;

    /// Whether the outer radius is automatic (Java: outerRadiusAutomatic).
    bool m_outerRadiusAutomatic{false};
    /// Whether the inner radius is automatic (Java: innerRadiusAutomatic).
    bool m_innerRadiusAutomatic{false};
    /// The radial direction (Java: radialDirection).
    double m_radialDirection{0};
    /// The radial position (Java: radialPosition).
    double m_radialPosition{0};

private:
    double m_shiftY{0};
    double m_shiftZ{0};
};

}  // namespace QtRocket
