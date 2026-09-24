#pragma once

namespace QtRocket
{

/// A component of constant radius along its length, such as a tube (OpenRocket's Coaxial).
/// Unlike RadialParent, whose radius may vary with the position, the radii here are single
/// values, in SI units. A pure interface, inherited `public virtual` (see AxialPositionable).
class Coaxial
{
public:
    virtual ~Coaxial() = default;

    Coaxial& operator=(const Coaxial&) = delete;
    Coaxial& operator=(Coaxial&&)      = delete;

    /// The radius of the inside dimension.
    [[nodiscard]] virtual double getInnerRadius() const   = 0;
    virtual void                 setInnerRadius(double v) = 0;

    /// The radius of the outside dimension.
    [[nodiscard]] virtual double getOuterRadius() const   = 0;
    virtual void                 setOuterRadius(double v) = 0;

    /// The wall thickness, typically the outer radius minus the inner radius.
    [[nodiscard]] virtual double getThickness() const = 0;

protected:
    Coaxial()               = default;
    Coaxial(const Coaxial&) = default;
    Coaxial(Coaxial&&)      = default;
};

}  // namespace QtRocket
