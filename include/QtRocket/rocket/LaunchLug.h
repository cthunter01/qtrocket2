#pragma once

#include <memory>
#include <numbers>
#include <string>
#include <vector>

#include "QtRocket/rocket/BoxBounded.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/InsideColorComponent.h"
#include "QtRocket/rocket/LineInstanceable.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Tube.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AnglePositionable.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket
{

class ComponentPreset;

/// A launch lug (OpenRocket's LaunchLug): a small tube on the outside of a body that slides over
/// the launch rod. It has an outer radius and a wall thickness, sits at an angle around its
/// parent's axis (pi by default, the negative y direction) and can be repeated along the axis
/// (LineInstanceable): getInstanceCount() lugs, each getInstanceSeparation() after the previous
/// one, front to front.
///
/// Its distance from the parent's axis is not a property but follows the body it sits on:
/// componentChanged() stores the radius of the nearest symmetric component among the ancestors,
/// the larger of its radii at the lug's two ends, plus the lug's outer radius, and
/// getInstanceOffsets() places the instances there. So the instances are on the body's surface
/// once an event has reached the lug (it is in a Rocket whose events are enabled) and on the
/// parent's axis before, as in OpenRocket. Reading the body's radius there also refreshes an
/// automatic body tube radius, which the automatic radii of neighbouring body components rely
/// on. The CG reads the parent itself (a symmetric component, else radius 0) at x = the lug's
/// axial offset.
///
/// A launch lug takes LAUNCH_LUG presets and holds no children.
///
/// Kept as OpenRocket has them: setOuterRadius() accepts a negative radius;
/// getComponentVolume() and the mass count every instance, while the unit inertias are those of
/// one lug; the angle method is always RELATIVE (setAngleMethod() does nothing).
///
/// Deviations from OpenRocket:
/// - A new launch lug's material is the built-in bulk default (see ExternalComponent); the GUI
///   calls applyDefaultMaterial() on a launch lug it creates, which applies the preferences'
///   default for "LaunchLug" (then "Tube", "ExternalComponent", "RocketComponent").
/// - The multi-edit config listeners are not ported (see RocketComponent).
// NOLINTNEXTLINE(misc-multiple-inheritance): the InsideColorComponent mixin carries data
class LaunchLug : public Tube,
                  public virtual AnglePositionable,
                  public virtual BoxBounded,
                  public virtual LineInstanceable,
                  public InsideColorComponent
{
public:
    using RocketComponent::getAxialOffset;
    using RocketComponent::isCompatible;

    /// A lug 0.03 m long with outer radius 5 mm and a 1 mm wall, positioned at the MIDDLE of its
    /// parent, at the angle pi; one instance, with an instance separation of twice the length.
    LaunchLug();

    [[nodiscard]] ComponentKind kind() const noexcept override { return ComponentKind::LAUNCH_LUG; }

    // ---- the radii (Coaxial)

    [[nodiscard]] double getOuterRadius() const override;

    /// Sets the outer radius; a wall thicker than the radius becomes the radius (Java's
    /// Math.min). Clears the preset and fires AEROMASS_CHANGE, unless the radius equals the
    /// current one (MathUtil::equals).
    void setOuterRadius(double radius) override;

    /// The outer radius less the wall thickness.
    [[nodiscard]] double getInnerRadius() const override;

    /// setOuterRadius(@p innerRadius + the wall thickness): the wall is kept and the outer radius
    /// follows.
    void setInnerRadius(double innerRadius) override;

    [[nodiscard]] double getThickness() const override;

    /// Sets the wall thickness, clamped to 0 ... the outer radius; clears the preset and fires
    /// AEROMASS_CHANGE, unless @p thickness equals the current one (MathUtil::equals).
    void setThickness(double thickness);

    /// Sets the length; fires AEROMASS_CHANGE unless it equals the current one (MathUtil::equals).
    /// The preset is kept.
    void setLength(double length);

    // ---- AnglePositionable

    /// The angle of the lug around the parent's axis, in radians (-pi ... pi; 0 is the positive y
    /// axis).
    [[nodiscard]] double getAngleOffset() const override;

    /// Sets the angle, clamped (not reduced) to -pi ... pi; fires AEROMASS_CHANGE unless it
    /// equals the current one (MathUtil::equals).
    void setAngleOffset(double angle) override;

    /// Always RELATIVE.
    [[nodiscard]] AngleMethod getAngleMethod() const override;

    /// Does nothing, as in Java.
    void setAngleMethod(AngleMethod newMethod) override;

    // ---- LineInstanceable

    [[nodiscard]] double getInstanceSeparation() const override;

    /// Sets the distance between neighbouring instances (front to front, along +x); fires
    /// AERODYNAMIC_CHANGE unless it equals the current one (MathUtil::equals).
    void setInstanceSeparation(double separation) override;

    // ---- Instanceable

    [[nodiscard]] int getInstanceCount() const override;

    /// Sets the number of instances and fires AEROMASS_CHANGE; nothing happens for the current
    /// count or one that is not positive.
    void setInstanceCount(int newCount) override;

    [[nodiscard]] std::vector<Coordinate> getInstanceLocations() const override;

    /// (i * separation, cos(angle) * d, sin(angle) * d) for each instance i, with d the radial
    /// distance componentChanged() stored (see the class comment).
    [[nodiscard]] std::vector<Coordinate> getInstanceOffsets() const override;

    /// "<count>-Line".
    [[nodiscard]] std::string getPatternName() const override;

    // ---- AxialPositionable (RocketComponent's, see AxialPositionable)

    [[nodiscard]] AxialMethod getAxialMethod() const override
    {
        return RocketComponent::getAxialMethod();
    }

    /// RocketComponent::setAxialMethod(): changes how the position is described, keeping the
    /// position; fires nothing.
    void setAxialMethod(AxialMethod newMethod) override;

    /// The stored offset for getAxialMethod().
    [[nodiscard]] double getAxialOffset() const override;

    /// Sets the offset for the current method and fires AEROMASS_CHANGE.
    void setAxialOffset(double newOffset) override;

    // ---- BoxBounded

    /// One lug: from (0, -r, -r) to (length, r, r) with r the outer radius.
    [[nodiscard]] BoundingBox getInstanceBoundingBox() const override;

    // ---- RocketComponent / ExternalComponent

    /// False: a launch lug is placed relative to its parent.
    [[nodiscard]] bool isAfter() const override;

    /// length * pi * (outer^2 - inner^2), times the number of instances.
    [[nodiscard]] double getComponentVolume() const override;

    /// The middle of the row of instances, x = length / 2 + separation * (count - 1) / 2, at the
    /// lug's angle on the parent's surface: the parent's radius (when it is a symmetric
    /// component, read at x = the lug's axial offset; else 0) plus the outer radius. The weight
    /// is the component mass.
    [[nodiscard]] Coordinate getComponentCG() const override;

    /// (3 (outer^2 + inner^2) + length^2) / 12.
    [[nodiscard]] double getLongitudinalUnitInertia() const override;

    /// (inner^2 + outer^2) / 2.
    [[nodiscard]] double getRotationalUnitInertia() const override;

    /// Four points around the lug's axis at the outer radius, at x = 0 and x = length.
    [[nodiscard]] std::vector<Coordinate> getComponentBounds() const override;

    /// Always false.
    [[nodiscard]] bool allowsChildren() const override;

    /// Always false: nothing is attached to a launch lug.
    [[nodiscard]] bool isCompatible(ComponentKind kind) const override;

protected:
    using RocketComponent::setAxialOffset;

    /// RocketComponent's, then stores the radial distance of the instances (see the class
    /// comment) and clears the cached locations again, which were computed with the old one.
    void componentChanged(const ComponentChangeEvent& event) override;

    /// ExternalComponent's properties (the length, the finish and the material), then an
    /// OUTER_DIAMETER (the outer radius is half of it; with an INNER_DIAMETER the wall thickness
    /// too), stored directly. Fires AEROMASS_CHANGE (always).
    void loadFromPreset(const ComponentPreset& preset, const PresetLoadOptions& options) override;

    [[nodiscard]] std::unique_ptr<RocketComponent> cloneShallow() const override;

private:
    /// The length of a new launch lug, in m.
    static constexpr double kDefaultLength = 0.03;

    /// The outer radius (Java: radius).
    double m_radius{0.01 / 2};
    double m_thickness{0.001};
    double m_angleOffsetRad{std::numbers::pi};
    /// The distance of the instances from the parent's axis (Java: radialOffset).
    double m_radialOffset{0};
    /// Front to front along the positive rocket axis (Java: instanceSeparation); a new lug's is
    /// twice its length.
    double m_instanceSeparation{kDefaultLength * 2};
    int    m_instanceCount{1};
};

}  // namespace QtRocket
