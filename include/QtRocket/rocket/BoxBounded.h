#pragma once

#include "QtRocket/util/BoundingBox.h"

namespace QtRocket
{

/// A component that can give the bounding box of one of its instances (OpenRocket's
/// BoxBounded). FlightConfiguration combines it with each instance's transformation for the
/// bounds in the rocket's frame. A pure interface, inherited `public virtual` (see
/// AxialPositionable).
class BoxBounded
{
public:
    virtual ~BoxBounded() = default;

    BoxBounded& operator=(const BoxBounded&) = delete;
    BoxBounded& operator=(BoxBounded&&)      = delete;

    /// The bounding box of a single instance, from the instance's reference point. An empty box
    /// means the component has no physical extent (an assembly).
    [[nodiscard]] virtual BoundingBox getInstanceBoundingBox() const = 0;

protected:
    BoxBounded()                  = default;
    BoxBounded(const BoxBounded&) = default;
    BoxBounded(BoxBounded&&)      = default;
};

}  // namespace QtRocket
