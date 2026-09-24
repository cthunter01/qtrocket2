#pragma once

#include "QtRocket/rocket/Instanceable.h"
#include "QtRocket/rocket/position/AxialPositionable.h"

namespace QtRocket
{

/// A component whose instances are spaced along the rocket's axis (OpenRocket's
/// LineInstanceable: launch lugs, rail buttons, centering rings, bulkheads).
class LineInstanceable : public virtual AxialPositionable, public virtual Instanceable
{
public:
    ~LineInstanceable() override = default;

    LineInstanceable& operator=(const LineInstanceable&) = delete;
    LineInstanceable& operator=(LineInstanceable&&)      = delete;

    /// The axial distance between neighbouring instances.
    [[nodiscard]] virtual double getInstanceSeparation() const            = 0;
    virtual void                 setInstanceSeparation(double separation) = 0;

protected:
    LineInstanceable()                        = default;
    LineInstanceable(const LineInstanceable&) = default;
    LineInstanceable(LineInstanceable&&)      = default;
};

}  // namespace QtRocket
