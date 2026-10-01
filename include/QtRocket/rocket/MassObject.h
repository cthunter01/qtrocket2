#pragma once

#include <vector>

#include "QtRocket/rocket/InternalComponent.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket
{

/// An internal component with a mass but not necessarily a fixed shape (OpenRocket's
/// MassObject): mass components, shock cords, parachutes and streamers. It is modelled as a solid
/// cylinder of the packed length and radius, placed at a radial position (a distance from the
/// parent's axis in a direction, 0 being the y axis). A new one is positioned TOP with offset 0.
///
/// Automatic radius: the packed volume (radius^2 * length, without pi) is kept, the radius is
/// the parent's (getAutoRadius()) and the length follows from the volume. As in Java, the getters
/// store what they compute: getRadius() stores the automatic radius and the length that goes
/// with it, getLength() stores the volume divided by the square of the stored radius (so it
/// reads a stale radius until getRadius() refreshes it). The fields are therefore mutable
/// (RocketComponent::m_length too), and the methods that use both read the radius first, in
/// Java's order of evaluation.
///
/// Not ported: the multi-edit config listeners (see RocketComponent).
class MassObject : public InternalComponent
{
public:
    /// False: a mass object is positioned relative to its parent.
    [[nodiscard]] bool isAfter() const override;

    /// The packed length; with an automatic radius, the volume divided by the square of the
    /// stored radius (the length is kept when the stored radius is 0 within MathUtil::equals()),
    /// stored.
    [[nodiscard]] double getLength() const override;

    /// Sets the packed length (negative values become 0) and recomputes the volume with the
    /// automatic or the stored radius; fires MASS_CHANGE unless the length equals (within
    /// MathUtil::equals()) the stored one.
    void setLength(double length);

    /// The packed radius; when automatic, getAutoRadius() and the length that keeps the volume
    /// are stored first.
    [[nodiscard]] double getRadius() const;

    /// The radius the parent allows: getMaxParentRadius() when there is a parent and it is not 0
    /// (within MathUtil::equals()), else the stored radius.
    [[nodiscard]] double getAutoRadius() const;

    /// The radius of the space the parent offers: the base radius of a nose cone, the larger
    /// radius of a transition, the inner radius of a body tube or a ring component, else 0.
    /// HOOK(rocket-components): until NoseCone and Transition exist, a nose cone's or
    /// transition's radii are read through RadialParent as ReferenceType does: the fore radius
    /// is getOuterRadius(-1) and the aft radius getOuterRadius(getLength()) (Transition's
    /// getRadius() gives exactly those), and a nose cone's base radius is the larger of the two
    /// (its tip radius is 0); a body tube's inner radius is Coaxial::getInnerRadius().
    [[nodiscard]] double getMaxParentRadius() const;

    /// Sets the packed radius (negative values become 0), makes it manual and recomputes the
    /// volume; fires MASS_CHANGE unless the radius was manual and equal (within
    /// MathUtil::equals()) to the stored one.
    void setRadius(double radius);

    /// Whether the radius is taken from the parent.
    [[nodiscard]] bool isRadiusAutomatic() const noexcept { return m_autoRadius; }

    /// Makes the radius automatic or not; fires AEROMASS_CHANGE when it changes.
    void setRadiusAutomatic(bool automatic);

    /// The distance of the object's center from the parent's axis.
    [[nodiscard]] double getRadialPosition() const noexcept { return m_radialPosition; }

    /// Sets the distance (negative values become 0); when it differs (beyond MathUtil::equals()),
    /// updates the shift and fires MASS_CHANGE.
    void setRadialPosition(double radialPosition);

    /// The direction of the radial displacement, in radians (0 is the y direction).
    [[nodiscard]] double getRadialDirection() const noexcept { return m_radialDirection; }

    /// Sets the direction, reduced to -pi ... pi; when it differs (beyond MathUtil::equals()),
    /// updates the shift and fires MASS_CHANGE.
    void setRadialDirection(double radialDirection);

    /// (length / 2, shift y, shift z) with the component mass as the weight.
    [[nodiscard]] Coordinate getComponentCG() const final;

    /// (3 r^2 + length^2) / 12, a solid cylinder's.
    [[nodiscard]] double getLongitudinalUnitInertia() const final;

    /// r^2 / 2, a solid cylinder's.
    [[nodiscard]] double getRotationalUnitInertia() const final;

    /// Four points around the axis at the radius, at the front and at the back.
    [[nodiscard]] std::vector<Coordinate> getComponentBounds() const final;

protected:
    /// A mass object 25 mm long with a radius of 12.5 mm.
    MassObject();

    /// A mass object of @p length and @p radius, positioned TOP with offset 0.
    MassObject(double length, double radius);

    /// The packed radius (Java: radius); mutable since getRadius() refreshes it.
    mutable double m_radius;

private:
    /// The length that keeps the volume at the stored radius (the stored length when that
    /// radius is 0).
    [[nodiscard]] double getAutoLength() const;

    /// volume = radius^2 * length.
    void updateVolume(double radius);

    bool   m_autoRadius{false};
    double m_volume{0};  // the packed volume without pi
    double m_radialPosition{0};
    double m_radialDirection{0};
    double m_shiftY{0};
    double m_shiftZ{0};
};

}  // namespace QtRocket
