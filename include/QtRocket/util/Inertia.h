#pragma once

/// Unit moments of inertia (moment of inertia per unit mass) of simple solids, as OpenRocket's
/// Inertia class. Radii and lengths are in metres; the results are in m^2.
namespace QtRocket::Inertia
{

/// The rotational unit moment of inertia of a solid cylinder about its axis: r^2 / 2.
[[nodiscard]] constexpr double filledCylinderRotational(double radius) noexcept
{
    return (radius * radius) / 2;
}

/// The longitudinal unit moment of inertia of a solid cylinder about an axis perpendicular to
/// its own through its lengthwise midpoint: (3 r^2 + l^2) / 12.
[[nodiscard]] constexpr double filledCylinderLongitudinal(double radius, double length) noexcept
{
    return ((3 * (radius * radius)) + (length * length)) / 12;
}

/// The unit moment of inertia about an axis parallel to one through the CG and @p distance away
/// from it (parallel axis theorem): I_cg + d^2.
[[nodiscard]] constexpr double shift(double cgInertia, double distance) noexcept
{
    return cgInertia + (distance * distance);
}

}  // namespace QtRocket::Inertia
