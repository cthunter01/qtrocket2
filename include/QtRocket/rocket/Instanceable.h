#pragma once

#include <string>
#include <vector>

#include "QtRocket/util/Coordinate.h"

namespace QtRocket
{

/// A component that represents several identical instances (fins, rail buttons, clustered tubes,
/// pods, ...) (OpenRocket's Instanceable). getInstanceOffsets().size() and
/// getInstanceLocations().size() always equal getInstanceCount().
///
/// A pure interface, inherited `public virtual` (see AxialPositionable). RocketComponent already
/// declares the first four functions as virtuals with single-instance defaults, so an
/// implementing class declares them with `override`.
class Instanceable
{
public:
    virtual ~Instanceable() = default;

    Instanceable& operator=(const Instanceable&) = delete;
    Instanceable& operator=(Instanceable&&)      = delete;

    /// The location of each instance relative to the component's parent.
    [[nodiscard]] virtual std::vector<Coordinate> getInstanceLocations() const = 0;

    /// The location of each instance relative to this component's own reference point (usually
    /// its front center).
    [[nodiscard]] virtual std::vector<Coordinate> getInstanceOffsets() const = 0;

    /// Sets how many instances this component represents.
    virtual void setInstanceCount(int newCount) = 0;

    /// How many instances this component represents.
    [[nodiscard]] virtual int getInstanceCount() const = 0;

    /// A human-readable name of the instance arrangement; the same count may have different
    /// pattern names.
    [[nodiscard]] virtual std::string getPatternName() const = 0;

protected:
    Instanceable()                    = default;
    Instanceable(const Instanceable&) = default;
    Instanceable(Instanceable&&)      = default;
};

}  // namespace QtRocket
