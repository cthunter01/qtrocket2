#pragma once

#include <iosfwd>
#include <string>

#include "QtRocket/util/Coordinate.h"

namespace QtRocket
{

/// An immutable quaternion (w, x, y, z) used as a rotation, ported from OpenRocket's Quaternion.
///
/// rotate() computes q * v * q^-1 and invRotate() computes q^-1 * v * q, both assuming a unit
/// quaternion (see normalizeIfNecessary()). The arithmetic is OpenRocket's line for line and is
/// the standard q*v*q^-1: rotation(kZUnit, pi/2).rotate(kXUnit) == kYUnit, i.e. the right-hand
/// rule in a right-handed frame, and rotation(Coordinate(pi/4, 0, 0)).invRotate(kYUnit) is
/// (0, 0.707, -0.707); both are pinned by QuaternionTests. OpenRocket's comment that its
/// rotations follow the left-hand rule (RocketComponent.java:1658) refers to how component angles
/// are applied, not to this class; do not change any sign here.
///
/// Products: multiplyRight(other) is this * other and multiplyLeft(other) is other * this, so
/// applying rotation a and then b is b.multiplyRight(a), equivalently a.multiplyLeft(b).
///
/// Deviations from OpenRocket:
/// - clone() is not ported (a copy is a clone) and normalizeIfNecessary() returns a value rather
///   than "the same instance"; the norm is recomputed rather than cached in the object.
/// - The openrocket.debug.quaternioncount instantiation counter (a debug log line every N
///   constructions) is dropped on purpose.
/// - The Java assert statements in rotate() and invRotate() (unit norm, zero w-part), which are
///   off at run time in OpenRocket, are not ported.
/// - toString() rounds, and spells NaN and infinity, differently; see there.
class Quaternion
{
public:
    /// The "one" quaternion (1, 0, 0, 0): the identity rotation.
    constexpr Quaternion() noexcept = default;

    constexpr Quaternion(double wValue, double xValue, double yValue, double zValue) noexcept
      : m_w{wValue}, m_x{xValue}, m_y{yValue}, m_z{zValue}
    {
    }

    /// The rotation whose axis is the direction of @p rotationVector and whose angle is its
    /// length. A vector shorter than 1e-6 gives the identity. Costs one length and two
    /// trigonometric functions.
    [[nodiscard]] static Quaternion rotation(const Coordinate& rotationVector) noexcept;

    /// The rotation of @p angle radians about @p axis (normalized here).
    /// @throws BugError when @p axis has (nearly) zero length (see Coordinate::normalize()).
    [[nodiscard]] static Quaternion rotation(const Coordinate& axis, double angle);

    [[nodiscard]] constexpr double w() const noexcept { return m_w; }
    [[nodiscard]] constexpr double x() const noexcept { return m_x; }
    [[nodiscard]] constexpr double y() const noexcept { return m_y; }
    [[nodiscard]] constexpr double z() const noexcept { return m_z; }

    /// True when w, x, y or z is NaN.
    [[nodiscard]] bool isNaN() const noexcept;

    /// The product this * other.
    [[nodiscard]] constexpr Quaternion multiplyRight(const Quaternion& other) const noexcept
    {
        const double newW =
            (m_w * other.m_w) - (m_x * other.m_x) - (m_y * other.m_y) - (m_z * other.m_z);
        const double newX =
            (m_w * other.m_x) + (m_x * other.m_w) + (m_y * other.m_z) - (m_z * other.m_y);
        const double newY =
            (m_w * other.m_y) + (m_y * other.m_w) + (m_z * other.m_x) - (m_x * other.m_z);
        const double newZ =
            (m_w * other.m_z) + (m_z * other.m_w) + (m_x * other.m_y) - (m_y * other.m_x);
        return Quaternion{newW, newX, newY, newZ};
    }

    /// The product other * this.
    [[nodiscard]] constexpr Quaternion multiplyLeft(const Quaternion& other) const noexcept
    {
        /* other(abcd) * this(wxyz) */
        const double newW =
            (other.m_w * m_w) - (other.m_x * m_x) - (other.m_y * m_y) - (other.m_z * m_z);
        const double newX =
            (other.m_w * m_x) + (other.m_x * m_w) + (other.m_y * m_z) - (other.m_z * m_y);
        const double newY =
            (other.m_w * m_y) + (other.m_y * m_w) + (other.m_z * m_x) - (other.m_x * m_z);
        const double newZ =
            (other.m_w * m_z) + (other.m_z * m_w) + (other.m_x * m_y) - (other.m_y * m_x);
        return Quaternion{newW, newX, newY, newZ};
    }

    /// This quaternion scaled to norm one.
    /// @throws BugError when the norm is below 1e-7 (OpenRocket: IllegalStateException).
    [[nodiscard]] Quaternion normalize() const;

    /// This quaternion, normalized only when its norm is more than 1 ppm from one.
    /// @throws BugError when the norm is (nearly) zero.
    [[nodiscard]] Quaternion normalizeIfNecessary() const;

    /// sqrt(w^2 + x^2 + y^2 + z^2)
    [[nodiscard]] double norm() const noexcept;

    /// w^2 + x^2 + y^2 + z^2
    [[nodiscard]] constexpr double norm2() const noexcept
    {
        return (m_x * m_x) + (m_y * m_y) + (m_z * m_z) + (m_w * m_w);
    }

    /// this * coord * this^-1 for a unit quaternion; the coordinate's weight is kept.
    [[nodiscard]] constexpr Coordinate rotate(const Coordinate& coord) const noexcept
    {
        // (a,b,c,d) = this * coord = (w,x,y,z) * (0,cx,cy,cz)
        const double a = (-m_x * coord.x) - (m_y * coord.y) - (m_z * coord.z);  // w
        const double b = (m_w * coord.x) + (m_y * coord.z) - (m_z * coord.y);   // x i
        const double c = (m_w * coord.y) - (m_x * coord.z) + (m_z * coord.x);   // y j
        const double d = (m_w * coord.z) + (m_x * coord.y) - (m_y * coord.x);   // z k

        // return = (a,b,c,d) * (this)^-1 = (a,b,c,d) * (w,-x,-y,-z); its w-part is zero
        return Coordinate{(-a * m_x) + (b * m_w) - (c * m_z) + (d * m_y),
                          (-a * m_y) + (b * m_z) + (c * m_w) - (d * m_x),
                          (-a * m_z) - (b * m_y) + (c * m_x) + (d * m_w), coord.weight};
    }

    /// this^-1 * coord * this for a unit quaternion: the inverse of rotate(); the weight is kept.
    [[nodiscard]] constexpr Coordinate invRotate(const Coordinate& coord) const noexcept
    {
        // (a,b,c,d) = (this)^-1 * coord = (w,-x,-y,-z) * (0,cx,cy,cz)
        const double a = (m_x * coord.x) + (m_y * coord.y) + (m_z * coord.z);
        const double b = (m_w * coord.x) - (m_y * coord.z) + (m_z * coord.y);
        const double c = (m_w * coord.y) + (m_x * coord.z) - (m_z * coord.x);
        const double d = (m_w * coord.z) - (m_x * coord.y) + (m_y * coord.x);

        // return = (a,b,c,d) * this = (a,b,c,d) * (w,x,y,z); its w-part is zero
        return Coordinate{(a * m_x) + (b * m_w) + (c * m_z) - (d * m_y),
                          (a * m_y) - (b * m_z) + (c * m_w) + (d * m_x),
                          (a * m_z) + (b * m_y) - (c * m_x) + (d * m_w), coord.weight};
    }

    /// rotate(Coordinate(0, 0, 1)) with about half the multiplications.
    [[nodiscard]] constexpr Coordinate rotateZ() const noexcept
    {
        return Coordinate{2 * ((m_w * m_y) + (m_x * m_z)), 2 * ((m_y * m_z) - (m_w * m_x)),
                          (m_w * m_w) - (m_x * m_x) - (m_y * m_y) + (m_z * m_z)};
    }

    /// Exact component-wise equality (OpenRocket's Quaternion has no tolerant equals).
    [[nodiscard]] constexpr bool operator==(const Quaternion& other) const noexcept = default;

    /// "Quaternion[w,x,y,z,norm=n]" with six decimals each (Java's %f). Deviation: std::format
    /// rounds the exact binary value half to even where Java's Formatter rounds the shortest
    /// round-trip decimal half up, so Quaternion(5e-7, 0, 0, 0) prints w as 0.000000 here and as
    /// 0.000001 in Java; NaN and infinity print as nan and inf (Java: NaN and Infinity). See
    /// Coordinate::toString().
    [[nodiscard]] std::string toString() const;

private:
    double m_w{1.0};
    double m_x{0.0};
    double m_y{0.0};
    double m_z{0.0};
};

/// Writes toString(), so that GoogleTest prints a readable value when EXPECT_EQ on two
/// quaternions fails.
std::ostream& operator<<(std::ostream& os, const Quaternion& q);

}  // namespace QtRocket
