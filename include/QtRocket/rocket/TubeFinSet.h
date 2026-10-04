#pragma once

#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "QtRocket/rocket/BoxBounded.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/InsideColorComponent.h"
#include "QtRocket/rocket/RingInstanceable.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Tube.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/AxialPositionable.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket
{

class ComponentPreset;

/// A set of tube fins (OpenRocket's TubeFinSet): 1 to 8 identical tubes around a body, each
/// touching the body along its length. The tubes are the instances of the component
/// (RingInstanceable): the fin count is the instance count, and the first tube sits at the base
/// rotation (the angle offset, 0 = the positive y axis).
///
/// The outer radius can be automatic, which is the default: with three tubes or more the radius
/// at which neighbouring tubes touch each other around the body, r_body * sin(pi / n) /
/// (1 - sin(pi / n)); with one or two tubes the body radius itself. The body radius is that of
/// the nearest symmetric component above the fin set, at the fin set's front (getBodyRadius()).
///
/// The wall thickness of a new tube fin set is NaN ("not set yet"): BodyTube::addChild() gives a
/// tube fin set added with a NaN thickness the body tube's own. Until then the inner radius, the
/// thickness, the volume and the mass are NaN, as in OpenRocket.
///
/// A tube fin set takes BODY_TUBE presets (getPresetType(), RocketComponent's default for the
/// kind) and holds no children.
///
/// Kept as OpenRocket computes them, although they are not the physical values:
/// - getInstanceOffsets() places every tube's axis on the body's surface (at the body radius,
///   not the body radius plus the tube radius);
/// - getLongitudinalUnitInertia() of several tubes is the sum over the tubes of one tube's unit
///   inertia plus the square of the axial offset (it is not divided by the count);
/// - getRotationalUnitInertia() of several tubes adds the body radius unsquared, times the count;
/// - getComponentBounds() spans twice the bounding radius;
/// - setFinCount() compares with the requested count before clamping it to 1 ... 8, so asking
///   for 9 tubes when there are 8 fires an event.
///
/// Deviations from OpenRocket:
/// - A new tube fin set's material is the built-in bulk default (see ExternalComponent); the GUI
///   calls applyDefaultMaterial() on a tube fin set it creates, which applies the preferences'
///   default for "TubeFinSet" (then "Tube", "ExternalComponent", "RocketComponent").
/// - Java's field radiusMethod (RELATIVE) is not ported: nothing reads it. The radius method and
///   offset are RocketComponent's (COAXIAL and 0), and setRadius(), setRadiusOffset() and
///   setRadiusMethod() do nothing, as Java's stubs.
/// - The multi-edit config listeners, mutex.verify() and checkState() are not ported (see
///   RocketComponent).
// NOLINTNEXTLINE(misc-multiple-inheritance): the InsideColorComponent mixin carries data
class TubeFinSet : public Tube,
                   public virtual AxialPositionable,
                   public virtual BoxBounded,
                   public virtual RingInstanceable,
                   public InsideColorComponent
{
public:
    using RocketComponent::getAxialOffset;
    using RocketComponent::getRadiusOffset;
    using RocketComponent::isCompatible;

    /// Six tubes 0.1 m long, positioned at the BOTTOM of the parent, with an automatic outer
    /// radius, a NaN thickness, the angle method FIXED and no base rotation.
    TubeFinSet();

    [[nodiscard]] ComponentKind kind() const noexcept override
    {
        return ComponentKind::TUBE_FIN_SET;
    }

    /// Sets the length of the tubes; fires AEROMASS_CHANGE unless it equals the current one
    /// (MathUtil::equals). The preset is kept.
    void setLength(double length);

    // ---- the radii (Coaxial)

    /// Whether the outer radius is automatic.
    [[nodiscard]] bool isOuterRadiusAutomatic() const noexcept { return m_autoRadius; }

    /// The outer radius of one tube: the stored one, or, when automatic, the body radius for
    /// fewer than three tubes and the touching radius otherwise (see the class comment).
    [[nodiscard]] double getOuterRadius() const override;

    /// Sets a fixed outer radius (negative values become 0), switching the automatic radius off;
    /// a wall thicker than the radius becomes the radius. Fires AEROMASS_CHANGE and clears the
    /// preset, unless the stored radius is already @p radius and not automatic.
    void setOuterRadius(double radius) override;

    /// Switches the automatic outer radius on or off; fires AEROMASS_CHANGE and clears the preset
    /// when it changes.
    void setOuterRadiusAutomatic(bool automatic);

    /// The distance between neighbouring tubes: twice the touching radius less the outer radius;
    /// 0 when they touch, negative when they overlap. (For two tubes the touching radius is
    /// infinite, or NaN without a body radius, and so is this.)
    [[nodiscard]] double getTubeSeparation() const;

    /// The outer radius less the wall thickness, not below 0; NaN while the thickness is.
    [[nodiscard]] double getInnerRadius() const override;

    /// setThickness(getOuterRadius() - @p r).
    void setInnerRadius(double r) override;

    /// The wall thickness, at most the outer radius; NaN until it is set.
    [[nodiscard]] double getThickness() const override;

    /// Sets the wall thickness, clamped to 0 ... the outer radius (a NaN stays NaN); fires
    /// AEROMASS_CHANGE and clears the preset, unless it is exactly the stored thickness.
    void setThickness(double thickness);

    /// The radius of the body the tubes sit on: the radius of the nearest SymmetricComponent
    /// among the ancestors at x = this component's position in its parent; 0 without one.
    [[nodiscard]] double getBodyRadius() const;

    // ---- the tubes

    [[nodiscard]] int getFinCount() const noexcept { return m_fins; }

    /// Sets the number of tubes, clamped to 1 ... 8, and fires AEROMASS_CHANGE; nothing happens
    /// when @p n is the current count.
    void setFinCount(int n);

    /// getAngleOffset(): the angle of the first tube, in radians.
    [[nodiscard]] double getBaseRotation() const;

    /// setAngleOffset(@p r).
    void setBaseRotation(double r);

    /// The angle between neighbouring tubes: 2 pi / count.
    [[nodiscard]] double getFinRotation() const;

    /// The rotation about the x axis by the base rotation (the identity for none).
    [[nodiscard]] const Transformation& getBaseRotationTransformation() const noexcept
    {
        return m_baseRotation;
    }

    /// The rotation about the x axis by getFinRotation().
    [[nodiscard]] const Transformation& getFinRotationTransformation() const noexcept
    {
        return m_finRotation;
    }

    // ---- AxialPositionable (RocketComponent's, see AxialPositionable)

    [[nodiscard]] AxialMethod getAxialMethod() const override
    {
        return RocketComponent::getAxialMethod();
    }

    /// Changes how the position is described, keeping the position
    /// (RocketComponent::setAxialMethod()), and fires NONFUNCTIONAL_CHANGE, also when the method
    /// does not change (as Java).
    void setAxialMethod(AxialMethod newMethod) override;

    /// The stored offset for getAxialMethod().
    [[nodiscard]] double getAxialOffset() const override;

    /// Sets the offset for the current method and fires AEROMASS_CHANGE.
    void setAxialOffset(double newOffset) override;

    // ---- Instanceable

    /// getFinCount().
    [[nodiscard]] int getInstanceCount() const override;

    /// setFinCount(@p newCount).
    void setInstanceCount(int newCount) override;

    [[nodiscard]] std::vector<Coordinate> getInstanceLocations() const override;

    /// (0, r_body, 0) rotated about the x axis by each instance angle (see the class comment).
    [[nodiscard]] std::vector<Coordinate> getInstanceOffsets() const override;

    /// "<count>-tubefin-ring".
    [[nodiscard]] std::string getPatternName() const override;

    // ---- RingInstanceable

    /// 2 pi / count.
    [[nodiscard]] double getInstanceAngleIncrement() const override;

    /// The base rotation plus i times the increment, each reduced to 0 ... 2 pi.
    [[nodiscard]] std::vector<double> getInstanceAngles() const override;

    // ---- AnglePositionable

    /// The angle of the first tube, in radians (-pi ... pi).
    [[nodiscard]] double getAngleOffset() const override;

    /// Sets the angle of the first tube, reduced to -pi ... pi, and fires AEROMASS_CHANGE; nothing
    /// happens when the reduced angle equals the current one (MathUtil::equals).
    void setAngleOffset(double angle) override;

    /// The angle method: FIXED until set otherwise. It is only stored.
    [[nodiscard]] AngleMethod getAngleMethod() const override;

    /// Stores the angle method and fires AEROMASS_CHANGE (always).
    void setAngleMethod(AngleMethod newMethod) override;

    // ---- RadiusPositionable

    /// The body radius plus the outer radius.
    [[nodiscard]] double getBoundingRadius() const override;

    /// 0 (RocketComponent's).
    [[nodiscard]] double getRadiusOffset() const override;

    /// Does nothing, as in Java.
    void setRadiusOffset(double radius) override;

    /// COAXIAL (RocketComponent's).
    [[nodiscard]] RadiusMethod getRadiusMethod() const override;

    /// Does nothing, as in Java.
    void setRadiusMethod(RadiusMethod method) override;

    /// Does nothing, as in Java.
    void setRadius(RadiusMethod method, double radius) override;

    // ---- BoxBounded

    /// One tube: from (0, -r, -r) to (length, r, r) with r the outer radius.
    [[nodiscard]] BoundingBox getInstanceBoundingBox() const override;

    // ---- RocketComponent / ExternalComponent

    /// False: a tube fin set is placed relative to its parent.
    [[nodiscard]] bool isAfter() const override;

    /// pi (outer^2 - inner^2) length, times the number of tubes.
    [[nodiscard]] double getComponentVolume() const override;

    /// (length / 2, 0, 0) with the component mass as the weight; for a single tube
    /// (length / 2, outer radius + body radius, 0). Rotated by the base rotation.
    [[nodiscard]] Coordinate getComponentCG() const override;

    /// One tube's (3 (outer^2 + inner^2) + length^2) / 12 for a single tube; for several the sum
    /// described in the class comment.
    [[nodiscard]] double getLongitudinalUnitInertia() const override;

    /// One tube's (inner^2 + outer^2) / 2 for a single tube; for several, count * (that +
    /// outer^2 + the body radius) (see the class comment).
    [[nodiscard]] double getRotationalUnitInertia() const override;

    /// Four points around the axis at twice the bounding radius, at x = 0 and x = length.
    [[nodiscard]] std::vector<Coordinate> getComponentBounds() const override;

    /// Always false.
    [[nodiscard]] bool allowsChildren() const override;

    /// Always false.
    [[nodiscard]] bool isCompatible(ComponentKind kind) const override;

protected:
    using RocketComponent::setAxialOffset;

    /// ExternalComponent's properties (the length, the finish and the material), then an
    /// OUTER_DIAMETER (a fixed outer radius of half of it; with an INNER_DIAMETER the wall
    /// thickness too), stored directly. Fires nothing of its own, as in Java.
    void loadFromPreset(const ComponentPreset& preset, const PresetLoadOptions& options) override;

    [[nodiscard]] std::unique_ptr<RocketComponent> cloneShallow() const override;

private:
    /// The stored outer radius of a new tube fin set (DEFAULT_RADIUS), in m.
    static constexpr double kDefaultRadius = 0.025;

    /// The outer radius at which neighbouring tubes touch each other around the body.
    [[nodiscard]] double getTouchingRadius() const;

    /// Whether the outer radius is chosen from the parent (Java: autoRadius).
    bool m_autoRadius{true};
    /// The number of tubes (Java: fins).
    int m_fins{6};
    /// The stored outer radius (Java: outerRadius).
    double m_outerRadius{kDefaultRadius};
    /// The wall thickness, NaN until it is set (Java: thickness).
    double m_thickness{std::numeric_limits<double>::quiet_NaN()};
    /// The angle of the first tube; zero is the positive y axis (Java: firstFinOffsetRadians).
    double      m_firstFinOffsetRadians{0};
    AngleMethod m_angleMethod{AngleMethod::FIXED};
    /// The rotation about the x axis by the angle offset (Java: baseRotation).
    Transformation m_baseRotation{Transformation::kIdentity};
    /// The rotation about the x axis by 2 pi / count (Java: finRotation).
    Transformation m_finRotation;
};

}  // namespace QtRocket
