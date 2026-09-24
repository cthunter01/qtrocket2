#pragma once

#include "QtRocket/rocket/position/AxialMethod.h"

namespace QtRocket
{

/// A component whose axial position the user can set with an AxialMethod and an offset
/// (OpenRocket's position.AxialPositionable). A pure interface: components are asked for it
/// with dynamic_cast off the hot path.
///
/// RocketComponent already declares these four functions (getAxialMethod() non-virtual, the
/// others virtual). An inherited function cannot implement another base's pure virtual in C++,
/// so an implementing class declares all four with `override`, forwarding to RocketComponent's
/// when it adds nothing (see ComponentAssembly).
///
/// Interfaces are inherited `public virtual`, by each other and by the component classes, so that
/// a class reaching one interface along two paths (InternalComponent and LineInstanceable both
/// bring AxialPositionable to a CenteringRing) holds a single copy and dynamic_cast to it is
/// unambiguous. Where an override then reaches the most derived class along only one of the
/// paths, MSVC warns that it is inherited "via dominance" (C4250, a level-2 warning, an error
/// under /WX): the most derived class re-declares that function with `override`.
class AxialPositionable
{
public:
    virtual ~AxialPositionable() = default;

    AxialPositionable& operator=(const AxialPositionable&) = delete;
    AxialPositionable& operator=(AxialPositionable&&)      = delete;

    /// The offset, whose meaning depends on getAxialMethod().
    [[nodiscard]] virtual double getAxialOffset() const                = 0;
    virtual void                 setAxialOffset(double newAxialOffset) = 0;

    [[nodiscard]] virtual AxialMethod getAxialMethod() const                = 0;
    virtual void                      setAxialMethod(AxialMethod newMethod) = 0;

protected:
    AxialPositionable()                         = default;
    AxialPositionable(const AxialPositionable&) = default;
    AxialPositionable(AxialPositionable&&)      = default;
};

}  // namespace QtRocket
