#pragma once

#include <vector>

#include "QtRocket/rocket/Instanceable.h"
#include "QtRocket/rocket/position/AnglePositionable.h"
#include "QtRocket/rocket/position/RadiusPositionable.h"

namespace QtRocket
{

/// A component whose instances are spread around its parent's axis at one radius (OpenRocket's
/// RingInstanceable: pod sets, booster sets, tube fin sets). Java re-declares the angle and radius
/// accessors of its super-interfaces; here they are inherited from AnglePositionable and
/// RadiusPositionable.
class RingInstanceable : public virtual Instanceable,
                         public virtual AnglePositionable,
                         public virtual RadiusPositionable
{
public:
    ~RingInstanceable() override = default;

    RingInstanceable& operator=(const RingInstanceable&) = delete;
    RingInstanceable& operator=(RingInstanceable&&)      = delete;

    /// The angle between neighbouring instances, in radians.
    [[nodiscard]] virtual double getInstanceAngleIncrement() const = 0;

    /// The angle of each instance, in radians (RocketComponent declares it too).
    [[nodiscard]] virtual std::vector<double> getInstanceAngles() const = 0;

protected:
    RingInstanceable()                        = default;
    RingInstanceable(const RingInstanceable&) = default;
    RingInstanceable(RingInstanceable&&)      = default;
};

}  // namespace QtRocket
