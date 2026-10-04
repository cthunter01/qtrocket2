#pragma once

// Java's protected RocketComponent.setAxialOffset(AxialMethod, double) for the tests that port a
// JUnit case calling it (OpenRocket's component tests are in RocketComponent's package). Test-only.

#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AxialMethod.h"

namespace QtRocket::Test
{

namespace Detail
{

/// Reaches RocketComponent's protected setAxialOffset(method, offset) the way a derived class
/// may: through a pointer to member that it forms itself. Never instantiated.
class AxialOffsetAccess : public RocketComponent
{
public:
    /// Calls the protected setAxialOffset(@p method, @p offset) of @p component.
    static void set(RocketComponent& component, AxialMethod method, double offset)
    {
        using Setter        = void (RocketComponent::*)(AxialMethod, double);
        const Setter setter = &AxialOffsetAccess::setAxialOffset;
        (component.*setter)(method, offset);
    }
};

}  // namespace Detail

/// Java's protected setAxialOffset(method, offset), called as the JUnit tests call it: it stores
/// the method and the offset and moves the component, and nothing else. No event is fired, so in
/// a rocket nothing is updated or cleared: a freeform outline is not clamped again, the cached
/// area and CG stay, and so do the cached locations (which the public setAxialMethod() would
/// compute first for the ABSOLUTE method).
inline void setAxialOffset(RocketComponent& component, AxialMethod method, double offset)
{
    Detail::AxialOffsetAccess::set(component, method, offset);
}

}  // namespace QtRocket::Test
