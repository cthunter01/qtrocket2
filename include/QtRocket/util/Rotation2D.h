#pragma once

#include <concepts>

namespace QtRocket
{

/// A weighted 3-D coordinate: public x, y, z and weight, and construction as T{x, y, z, weight}.
/// Coordinate has this shape, and so can a test double; Rotation2D works on any of them. The
/// concept cannot check the constructor's argument order: it must be x, y, z, weight, as
/// OpenRocket's Coordinate(double x, double y, double z, double w).
template <class T>
concept WeightedCoordinate = requires(const T& c) {
    { c.x } -> std::convertible_to<double>;
    { c.y } -> std::convertible_to<double>;
    { c.z } -> std::convertible_to<double>;
    { c.weight } -> std::convertible_to<double>;
    T{0.0, 0.0, 0.0, 0.0};
};

/// A rotation by a fixed angle about a coordinate axis, kept as its sine and cosine, as
/// OpenRocket's Rotation2D. rotateX/Y/Z turn a coordinate about that axis by the angle with
/// OpenRocket's sign convention (x' = cos x - sin y, y' = cos y + sin x about z, and the cyclic
/// equivalents about x and y); invRotateX/Y/Z turn it back. The weight is carried through.
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

    template <WeightedCoordinate C>
    [[nodiscard]] constexpr C rotateX(const C& c) const
    {
        return C{c.x, (m_cos * c.y) - (m_sin * c.z), (m_cos * c.z) + (m_sin * c.y), c.weight};
    }

    template <WeightedCoordinate C>
    [[nodiscard]] constexpr C rotateY(const C& c) const
    {
        return C{(m_cos * c.x) + (m_sin * c.z), c.y, (m_cos * c.z) - (m_sin * c.x), c.weight};
    }

    template <WeightedCoordinate C>
    [[nodiscard]] constexpr C rotateZ(const C& c) const
    {
        return C{(m_cos * c.x) - (m_sin * c.y), (m_cos * c.y) + (m_sin * c.x), c.z, c.weight};
    }

    template <WeightedCoordinate C>
    [[nodiscard]] constexpr C invRotateX(const C& c) const
    {
        return C{c.x, (m_cos * c.y) + (m_sin * c.z), (m_cos * c.z) - (m_sin * c.y), c.weight};
    }

    template <WeightedCoordinate C>
    [[nodiscard]] constexpr C invRotateY(const C& c) const
    {
        return C{(m_cos * c.x) - (m_sin * c.z), c.y, (m_cos * c.z) + (m_sin * c.x), c.weight};
    }

    template <WeightedCoordinate C>
    [[nodiscard]] constexpr C invRotateZ(const C& c) const
    {
        return C{(m_cos * c.x) + (m_sin * c.y), (m_cos * c.y) - (m_sin * c.x), c.z, c.weight};
    }

private:
    double m_sin;
    double m_cos;
};

}  // namespace QtRocket
