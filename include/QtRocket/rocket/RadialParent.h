#pragma once

namespace QtRocket
{

/// An axially symmetric component whose radius may vary along its length and that can hold other
/// components radially (OpenRocket's RadialParent: symmetric body components, tube couplers,
/// inner tubes). A pure interface, inherited `public virtual` (see AxialPositionable);
/// getLength() also exists in RocketComponent, so an implementing class declares it with
/// `override`.
class RadialParent
{
public:
    virtual ~RadialParent() = default;

    RadialParent& operator=(const RadialParent&) = delete;
    RadialParent& operator=(RadialParent&&)      = delete;

    /// The outer radius at the local coordinate @p x; undefined for x < 0 and x beyond the
    /// component's length.
    [[nodiscard]] virtual double getOuterRadius(double x) const = 0;

    /// The inner radius at the local coordinate @p x; undefined outside the component as for
    /// getOuterRadius().
    [[nodiscard]] virtual double getInnerRadius(double x) const = 0;

    /// The length of this component.
    [[nodiscard]] virtual double getLength() const = 0;

protected:
    RadialParent()                    = default;
    RadialParent(const RadialParent&) = default;
    RadialParent(RadialParent&&)      = default;
};

}  // namespace QtRocket
