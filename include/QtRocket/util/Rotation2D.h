#pragma once

#include "QtRocket/util/Coordinate.h"

namespace QtRocket
{

/// A rotation by a fixed angle about a coordinate axis, kept as its sine and cosine, as
/// OpenRocket's Rotation2D. rotateX/Y/Z turn a coordinate about that axis by the angle with
/// OpenRocket's sign convention (x' = cos x - sin y, y' = cos y + sin x about z, and the cyclic
/// equivalents about x and y: the right-hand rule, so that rotateZ() agrees with
/// Quaternion::rotation(Coordinate::kZUnit, angle).rotate()); invRotateX/Y/Z turn it back. The
/// weight is carried through unchanged, as Java's new Coordinate(..., c.getWeight()) does. The
/// simulation stepper builds one from the flight conditions' theta, or from the airspeed
/// direction as Rotation2D(y / len, x / len).
///
/// Not ported: rotateZInPlace() and invRotateZInPlace(), which work on MutableCoordinate (not
/// ported either; Coordinate is a value type, so rotateZ() costs the same).
class Rotation2D
{
public:
    /// The rotation by @p angle radians.
    explicit Rotation2D(double angle);
    /// A rotation from its sine and cosine, for example from a unit vector's y and x
    /// components; sin^2 + cos^2 == 1 is not checked.
    constexpr Rotation2D(double sin, double cos) noexcept : m_sin(sin), m_cos(cos) { }

    /// No rotation (Java: Rotation2D.ID).
    [[nodiscard]] static constexpr Rotation2D identity() noexcept { return {0.0, 1.0}; }

    [[nodiscard]] constexpr double sin() const noexcept { return m_sin; }
    [[nodiscard]] constexpr double cos() const noexcept { return m_cos; }

    [[nodiscard]] constexpr Coordinate rotateX(const Coordinate& c) const noexcept
    {
        return Coordinate{c.x, (m_cos * c.y) - (m_sin * c.z), (m_cos * c.z) + (m_sin * c.y),
                          c.weight};
    }

    [[nodiscard]] constexpr Coordinate rotateY(const Coordinate& c) const noexcept
    {
        return Coordinate{(m_cos * c.x) + (m_sin * c.z), c.y, (m_cos * c.z) - (m_sin * c.x),
                          c.weight};
    }

    [[nodiscard]] constexpr Coordinate rotateZ(const Coordinate& c) const noexcept
    {
        return Coordinate{(m_cos * c.x) - (m_sin * c.y), (m_cos * c.y) + (m_sin * c.x), c.z,
                          c.weight};
    }

    [[nodiscard]] constexpr Coordinate invRotateX(const Coordinate& c) const noexcept
    {
        return Coordinate{c.x, (m_cos * c.y) + (m_sin * c.z), (m_cos * c.z) - (m_sin * c.y),
                          c.weight};
    }

    [[nodiscard]] constexpr Coordinate invRotateY(const Coordinate& c) const noexcept
    {
        return Coordinate{(m_cos * c.x) - (m_sin * c.z), c.y, (m_cos * c.z) + (m_sin * c.x),
                          c.weight};
    }

    [[nodiscard]] constexpr Coordinate invRotateZ(const Coordinate& c) const noexcept
    {
        return Coordinate{(m_cos * c.x) + (m_sin * c.y), (m_cos * c.y) - (m_sin * c.x), c.z,
                          c.weight};
    }

private:
    double m_sin;
    double m_cos;
};

}  // namespace QtRocket
