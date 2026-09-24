#pragma once

#include <cstddef>
#include <functional>
#include <iosfwd>
#include <limits>
#include <string>

namespace QtRocket
{

/// An immutable weighted 3D coordinate: a point, a vector, or a centre of mass with its mass as the
/// weight. Weights are non-negative; a weight of zero means "unweighted". Every operation returns a
/// new value and the fields are read directly (they are public but never reassigned by this class).
/// Ported from OpenRocket's Coordinate and CoordinateIF.
///
/// Axis conventions (OpenRocket's):
/// - Body frame: x runs aft from the nose tip along the rocket's centreline; y and z are radial.
/// - World frame: z is up, x is east and y is north.
/// - Rotations (see Quaternion) are the standard q*v*q^-1: rotation(kZUnit, pi/2).rotate(kXUnit)
///   is kYUnit, i.e. the right-hand rule in a right-handed frame. OpenRocket's comment that its
///   rotations follow the left-hand rule (RocketComponent.java:1658) refers to how component
///   angles are applied, not to Quaternion; do not change any sign there.
///
/// Deviations from OpenRocket:
/// - MutableCoordinate and clone() are not ported: this is a value type, so a copy is a clone.
/// - The openrocket.debug.coordinatecount instantiation counter (a debug log line every N
///   constructions) is dropped on purpose.
/// - toString() and toPreciseString() round, and spell NaN and infinity, differently; see there.
///
/// Arithmetic that needs no <cmath> is constexpr. add() sums the weights (joining two masses),
/// sub() keeps this coordinate's weight, and the operators below follow the same rules.
class Coordinate
{
public:
    double x{0.0};
    double y{0.0};
    double z{0.0};
    double weight{0.0};

    static const Coordinate kZero;   ///< (0, 0, 0, w=0)
    static const Coordinate kNul;    ///< (0, 0, 0, w=0): OpenRocket's NUL, the same value as kZero
    static const Coordinate kNaN;    ///< every component NaN
    static const Coordinate kMax;    ///< every component the largest finite double
    static const Coordinate kMin;    ///< x, y and z the most negative finite double, weight 0
    static const Coordinate kXUnit;  ///< (1, 0, 0)
    static const Coordinate kYUnit;  ///< (0, 1, 0)
    static const Coordinate kZUnit;  ///< (0, 0, 1)

    /// (0, 0, 0, w=0)
    constexpr Coordinate() noexcept = default;

    /// Omitted components are zero, so Coordinate(x), Coordinate(x, y) and Coordinate(x, y, z) are
    /// the unweighted forms.
    constexpr explicit Coordinate(double xValue, double yValue = 0.0, double zValue = 0.0,
                                  double weightValue = 0.0) noexcept
      : x{xValue}, y{yValue}, z{zValue}, weight{weightValue}
    {
    }

    /// A copy with a different x.
    [[nodiscard]] constexpr Coordinate setX(double value) const noexcept
    {
        return Coordinate{value, y, z, weight};
    }

    /// A copy with a different y.
    [[nodiscard]] constexpr Coordinate setY(double value) const noexcept
    {
        return Coordinate{x, value, z, weight};
    }

    /// A copy with a different z.
    [[nodiscard]] constexpr Coordinate setZ(double value) const noexcept
    {
        return Coordinate{x, y, value, weight};
    }

    /// A copy with a different weight.
    [[nodiscard]] constexpr Coordinate setWeight(double value) const noexcept
    {
        return Coordinate{x, y, z, value};
    }

    /// The x, y and z of @p c with this coordinate's weight.
    [[nodiscard]] constexpr Coordinate setXYZ(const Coordinate& c) const noexcept
    {
        return Coordinate{c.x, c.y, c.z, weight};
    }

    /// True when the weight is greater than zero.
    [[nodiscard]] constexpr bool isWeighted() const noexcept { return weight > 0.0; }

    /// True when x, y, z or the weight is NaN.
    [[nodiscard]] bool isNaN() const noexcept;

    /// The distance from the origin, sqrt(x^2 + y^2 + z^2).
    [[nodiscard]] double length() const noexcept;

    /// The square of the distance from the origin.
    [[nodiscard]] constexpr double length2() const noexcept { return (x * x) + (y * y) + (z * z); }

    /// The sum of the coordinates and of the weights.
    [[nodiscard]] constexpr Coordinate add(const Coordinate& other) const noexcept
    {
        return Coordinate{x + other.x, y + other.y, z + other.z, weight + other.weight};
    }

    /// Adds to x, y and z; the weight is unchanged.
    [[nodiscard]] constexpr Coordinate add(double x1, double y1, double z1) const noexcept
    {
        return Coordinate{x + x1, y + y1, z + z1, weight};
    }

    /// Adds to x, y, z and the weight.
    [[nodiscard]] constexpr Coordinate add(double x1, double y1, double z1,
                                           double w1) const noexcept
    {
        return Coordinate{x + x1, y + y1, z + z1, weight + w1};
    }

    /// this + coord * scale, weight included.
    [[nodiscard]] constexpr Coordinate addScaled(const Coordinate& coord,
                                                 double            scale) const noexcept
    {
        return Coordinate{x + (coord.x * scale), y + (coord.y * scale), z + (coord.z * scale),
                          weight + (coord.weight * scale)};
    }

    /// Subtracts the coordinates; the result keeps this coordinate's weight and ignores the
    /// other's.
    [[nodiscard]] constexpr Coordinate sub(const Coordinate& other) const noexcept
    {
        return Coordinate{x - other.x, y - other.y, z - other.z, weight};
    }

    /// Subtracts from x, y and z; the weight is unchanged.
    [[nodiscard]] constexpr Coordinate sub(double x1, double y1, double z1) const noexcept
    {
        return Coordinate{x - x1, y - y1, z - z1, weight};
    }

    /// Scales x, y, z and the weight by @p m.
    [[nodiscard]] constexpr Coordinate multiply(double m) const noexcept
    {
        return Coordinate{x * m, y * m, z * m, weight * m};
    }

    /// The component-by-component product, weights included.
    [[nodiscard]] constexpr Coordinate multiply(const Coordinate& other) const noexcept
    {
        return Coordinate{x * other.x, y * other.y, z * other.z, weight * other.weight};
    }

    /// The dot product x1*x2 + y1*y2 + z1*z2 (weights are ignored).
    [[nodiscard]] constexpr double dot(const Coordinate& other) const noexcept
    {
        return (x * other.x) + (y * other.y) + (z * other.z);
    }

    /// The dot product of two coordinates taken as vectors.
    [[nodiscard]] static constexpr double dot(const Coordinate& v1, const Coordinate& v2) noexcept
    {
        return (v1.x * v2.x) + (v1.y * v2.y) + (v1.z * v2.z);
    }

    /// The cross product this x other, unweighted whatever the operands' weights.
    [[nodiscard]] constexpr Coordinate cross(const Coordinate& other) const noexcept
    {
        return cross(*this, other);
    }

    /// The cross product a x b, unweighted whatever the operands' weights.
    [[nodiscard]] static constexpr Coordinate cross(const Coordinate& a,
                                                    const Coordinate& b) noexcept
    {
        return Coordinate{(a.y * b.z) - (a.z * b.y), (a.z * b.x) - (a.x * b.z),
                          (a.x * b.y) - (a.y * b.x)};
    }

    /// The same direction at distance one from the origin; the weight is unchanged.
    /// @throws std::domain_error when the length is below 1e-7 (OpenRocket: IllegalStateException).
    [[nodiscard]] Coordinate normalize() const;

    /// The weighted average: the weight is the sum of the weights and the position is weighted by
    /// them. When the weights sum to (nearly) zero the result is the plain midpoint with weight
    /// zero.
    [[nodiscard]] Coordinate average(const Coordinate& other) const noexcept;

    /// Linear interpolation towards @p other, weight included: 0 gives this, 1 gives other.
    [[nodiscard]] constexpr Coordinate interpolate(const Coordinate& other,
                                                   double            fraction) const noexcept
    {
        return Coordinate{x + ((other.x - x) * fraction), y + ((other.y - y) * fraction),
                          z + ((other.z - z) * fraction),
                          weight + ((other.weight - weight) * fraction)};
    }

    /// The largest of |x|, |y| and |z|: a norm that is cheaper than length().
    [[nodiscard]] double max() const noexcept;

    /// OpenRocket's equality: x, y, z and the weight each agree within MathUtil::kEpsilon
    /// (relative, see MathUtil::equals). Never true when a component is NaN. Not transitive, so it
    /// must not order or key containers; std::hash follows OpenRocket's hashCode and is coarse for
    /// the same reason.
    [[nodiscard]] bool operator==(const Coordinate& other) const noexcept;

    /// Bitwise-exact comparison of x, y, z and the weight (as doubles: -0.0 equals 0.0, NaN
    /// equals nothing).
    [[nodiscard]] constexpr bool exactlyEquals(const Coordinate& other) const noexcept
    {
        return x == other.x && y == other.y && z == other.z && weight == other.weight;
    }

    /// "(x,y,z)" or "(x,y,z,w=weight)" when weighted, five decimals each (Java's %.5f).
    /// Deviation: std::format rounds the exact binary value half to even, while Java's Formatter
    /// rounds the shortest round-trip decimal half up, so 0.015625 prints as 0.01562 here and as
    /// 0.01563 in Java. NaN and infinity print as nan and inf (Java: NaN and Infinity), and a NaN
    /// with its sign bit set (what x86 makes of 0.0/0.0) prints as -nan. These strings are for
    /// people and exception messages only; nothing written to a file goes through them.
    [[nodiscard]] std::string toString() const;

    /// "cm= <weight>g @[x,y,z]" with eight decimals (Java's %.8f), for checking calculations by
    /// hand. Rounds and spells NaN and infinity as toString() does.
    [[nodiscard]] std::string toPreciseString() const;
};

inline constexpr Coordinate Coordinate::kZero{0.0, 0.0, 0.0, 0.0};
inline constexpr Coordinate Coordinate::kNul{0.0, 0.0, 0.0, 0.0};
inline constexpr Coordinate Coordinate::kNaN{
    std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(),
    std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()};
inline constexpr Coordinate Coordinate::kMax{
    std::numeric_limits<double>::max(), std::numeric_limits<double>::max(),
    std::numeric_limits<double>::max(), std::numeric_limits<double>::max()};
inline constexpr Coordinate Coordinate::kMin{-std::numeric_limits<double>::max(),
                                             -std::numeric_limits<double>::max(),
                                             -std::numeric_limits<double>::max(), 0.0};
inline constexpr Coordinate Coordinate::kXUnit{1.0, 0.0, 0.0};
inline constexpr Coordinate Coordinate::kYUnit{0.0, 1.0, 0.0};
inline constexpr Coordinate Coordinate::kZUnit{0.0, 0.0, 1.0};

/// a.add(b): coordinates and weights summed.
[[nodiscard]] constexpr Coordinate operator+(const Coordinate& a, const Coordinate& b) noexcept
{
    return a.add(b);
}

/// a.sub(b): coordinates subtracted, the weight of a kept.
[[nodiscard]] constexpr Coordinate operator-(const Coordinate& a, const Coordinate& b) noexcept
{
    return a.sub(b);
}

/// a.multiply(-1): every component negated, the weight included.
[[nodiscard]] constexpr Coordinate operator-(const Coordinate& a) noexcept
{
    return a.multiply(-1.0);
}

/// a.multiply(m)
[[nodiscard]] constexpr Coordinate operator*(const Coordinate& a, double m) noexcept
{
    return a.multiply(m);
}

/// a.multiply(m)
[[nodiscard]] constexpr Coordinate operator*(double m, const Coordinate& a) noexcept
{
    return a.multiply(m);
}

/// Every component divided by @p m, the weight included.
[[nodiscard]] constexpr Coordinate operator/(const Coordinate& a, double m) noexcept
{
    return Coordinate{a.x / m, a.y / m, a.z / m, a.weight / m};
}

/// Writes toString(), so that GoogleTest prints a readable value when EXPECT_EQ on two
/// coordinates fails.
std::ostream& operator<<(std::ostream& os, const Coordinate& c);

}  // namespace QtRocket

/// OpenRocket's hashCode: (int)((x + y + z) * 100000). Coarse on purpose, since operator== is
/// tolerant; two coordinates that compare equal can still hash differently across a bucket edge,
/// exactly as in OpenRocket.
template <>
struct std::hash<QtRocket::Coordinate>
{
    [[nodiscard]] std::size_t operator()(const QtRocket::Coordinate& c) const noexcept;
};
