#pragma once

#include "QtRocket/rocket/position/AngleMethod.h"

namespace QtRocket
{

/// A component whose angular position about its parent's axis the user can set (OpenRocket's
/// position.AnglePositionable). Angles are in radians. A pure interface, inherited
/// `public virtual` (see AxialPositionable); getAngleOffset() also exists in RocketComponent, so
/// an implementing class declares it with `override`.
class AnglePositionable
{
public:
    virtual ~AnglePositionable() = default;

    AnglePositionable& operator=(const AnglePositionable&) = delete;
    AnglePositionable& operator=(AnglePositionable&&)      = delete;

    /// The angle to the first element, in radians.
    [[nodiscard]] virtual double getAngleOffset() const = 0;
    /// Sets the offset angle, in radians.
    virtual void setAngleOffset(double angle) = 0;

    [[nodiscard]] virtual AngleMethod getAngleMethod() const                = 0;
    virtual void                      setAngleMethod(AngleMethod newMethod) = 0;

protected:
    AnglePositionable()                         = default;
    AnglePositionable(const AnglePositionable&) = default;
    AnglePositionable(AnglePositionable&&)      = default;
};

}  // namespace QtRocket
