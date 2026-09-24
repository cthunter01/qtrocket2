#pragma once

#include "QtRocket/rocket/position/RadiusMethod.h"

namespace QtRocket
{

/// A component whose radial distance from its parent's axis the user can set (OpenRocket's
/// position.RadiusPositionable). A pure interface, inherited `public virtual` (see
/// AxialPositionable); getRadiusOffset() and getRadiusMethod() also exist in RocketComponent, so
/// an implementing class declares them with `override`.
class RadiusPositionable
{
public:
    virtual ~RadiusPositionable() = default;

    RadiusPositionable& operator=(const RadiusPositionable&) = delete;
    RadiusPositionable& operator=(RadiusPositionable&&)      = delete;

    /// The radius of the component's own outline, used by RadiusMethod::RELATIVE and SURFACE.
    [[nodiscard]] virtual double getBoundingRadius() const = 0;

    [[nodiscard]] virtual double getRadiusOffset() const        = 0;
    virtual void                 setRadiusOffset(double radius) = 0;

    [[nodiscard]] virtual RadiusMethod getRadiusMethod() const              = 0;
    virtual void                       setRadiusMethod(RadiusMethod method) = 0;

    /// Equivalent to setRadiusMethod(method) followed by setRadiusOffset(radius).
    virtual void setRadius(RadiusMethod method, double radius) = 0;

protected:
    RadiusPositionable()                          = default;
    RadiusPositionable(const RadiusPositionable&) = default;
    RadiusPositionable(RadiusPositionable&&)      = default;
};

}  // namespace QtRocket
