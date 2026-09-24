#pragma once

#include <array>
#include <iosfwd>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "QtRocket/util/Coordinate.h"

namespace QtRocket
{

class Quaternion;

/// An affine transformation A*x + c of a Coordinate: a 3x3 matrix A (OpenRocket calls it the
/// rotation, though the kProject* constants are projections) and a translation c. Immutable:
/// every operation returns a new transformation. Ported from OpenRocket's Transformation.
///
/// Composition: a.applyTransformation(b) is "b, then a": its transform(x) equals
/// a.transform(b.transform(x)), so the argument is applied to the coordinate first and the
/// matrix is the product A_a * A_b.
///
/// The translation is kept as the Coordinate it was given, weight included, and that weight takes
/// part in operator== and in applyTransformation() (Coordinate::add sums the weights), exactly as
/// in OpenRocket. transform() and linearTransform() ignore it and keep their argument's weight.
///
/// Deviations from OpenRocket:
/// - transform(CoordinateIF[]) and transform(Collection) replaced their elements in place; here
///   transform(std::span<const Coordinate>) returns a new std::vector. (OpenRocket's
///   FlightConfiguration.calculateBounds relies on the in-place form cumulatively across
///   contexts; its port must reproduce that explicitly.)
/// - print() (System.out) is not ported: toString() gives the same text. hashCode() is not
///   ported either (it hashed only the translation, and the tolerant equality makes a hash
///   unsound anyway).
/// - rotation(const Quaternion&) and inverse() are additions asked for by the QtRocket plan.
/// - toString() rounds, and spells NaN and infinity, differently; see there.
class Transformation
{
public:
    /// A 3x3 matrix, rows first: matrix[row][column].
    using Matrix3 = std::array<std::array<double, 3>, 3>;

    /// A rotation by an angle (in radians) below this is the identity (ANGLE_EPSILON).
    static constexpr double kAngleEpsilon = 0.000000001;

    static const Transformation kIdentity;
    static const Transformation kProjectXY;  ///< Drops the z component: A = diag(1, 1, 0).
    static const Transformation kProjectYZ;  ///< Drops the x component: A = diag(0, 1, 1).
    static const Transformation kProjectXZ;  ///< Drops the y component: A = diag(1, 0, 1).

    /// The identity transformation.
    constexpr Transformation() noexcept = default;

    /// A translation by (x, y, z).
    constexpr explicit Transformation(double x, double y, double z) noexcept : m_translate{x, y, z}
    {
    }

    /// A translation by @p translation, kept as given (weight included).
    constexpr explicit Transformation(const Coordinate& translation) noexcept
      : m_translate{translation}
    {
    }

    /// A x + translation.
    constexpr Transformation(const Matrix3& rotation, const Coordinate& translation) noexcept
      : m_rotation{rotation}, m_translate{translation}
    {
    }

    /// A x, with no translation.
    constexpr explicit Transformation(const Matrix3& rotation) noexcept : m_rotation{rotation} { }

    /// A translation by (x, y, z) (Java: getTranslationTransform).
    [[nodiscard]] static constexpr Transformation translation(double x, double y, double z) noexcept
    {
        return Transformation{x, y, z};
    }

    /// A translation by @p translate, kept as given (Java: getTranslationTransform).
    [[nodiscard]] static constexpr Transformation translation(const Coordinate& translate) noexcept
    {
        return Transformation{translate};
    }

    /// The rotation by @p angles.x about the x axis, @p angles.y about y and @p angles.z about z
    /// (radians), composed as rotateX(x).applyTransformation(rotateY(y)).applyTransformation(
    /// rotateZ(z)): the matrix product Rx * Ry * Rz, so the z rotation acts on a coordinate first.
    /// An angle whose magnitude is not above kAngleEpsilon is skipped (Java: getRotationTransform).
    [[nodiscard]] static Transformation rotation(const Coordinate& angles) noexcept;

    /// rotation(angles) about the point @p origin instead of the origin of the coordinate system:
    /// translate @p origin to the origin, rotate, translate back (Java: getRotationTransform).
    [[nodiscard]] static Transformation rotation(const Coordinate& angles,
                                                 const Coordinate& origin) noexcept;

    /// The linear transformation x -> q * x * conj(q), with no translation. For a unit quaternion
    /// that is the rotation q describes, and transform(x) equals q.rotate(x) for any q (both
    /// evaluate the same product). An addition to OpenRocket.
    [[nodiscard]] static Transformation rotation(const Quaternion& q) noexcept;

    /// A rotation of @p theta radians about the rocket's long axis: rotateX(theta) (Java:
    /// getAxialRotation).
    [[nodiscard]] static Transformation axialRotation(double theta) noexcept;

    /// A rotation of @p theta radians about the x axis, or kIdentity when |theta| is below
    /// kAngleEpsilon (Java: rotate_x). The x rotation is what Rotation2D::rotateX does.
    [[nodiscard]] static Transformation rotateX(double theta) noexcept;

    /// A rotation of @p theta radians about the y axis, or kIdentity when |theta| is below
    /// kAngleEpsilon (Java: rotate_y).
    [[nodiscard]] static Transformation rotateY(double theta) noexcept;

    /// A rotation of @p theta radians about the z axis, or kIdentity when |theta| is below
    /// kAngleEpsilon (Java: rotate_z).
    [[nodiscard]] static Transformation rotateZ(double theta) noexcept;

    /// The rotation with Euler angles in z-x-z order, Rz(alpha) * Rx(beta) * Rz(gamma), with no
    /// translation (Java: getEulerAngle313Transform).
    /// @param alpha rotation about z (radians)
    /// @param beta  rotation about the rotated x axis (radians)
    /// @param gamma rotation about the twice-rotated z axis (radians)
    [[nodiscard]] static Transformation eulerAngle313(double alpha, double beta,
                                                      double gamma) noexcept;

    /// A * orig + c; the weight of @p orig is kept.
    [[nodiscard]] constexpr Coordinate transform(const Coordinate& orig) const noexcept
    {
        const double x = (m_rotation[0][0] * orig.x) + (m_rotation[0][1] * orig.y) +
                         (m_rotation[0][2] * orig.z) + m_translate.x;
        const double y = (m_rotation[1][0] * orig.x) + (m_rotation[1][1] * orig.y) +
                         (m_rotation[1][2] * orig.z) + m_translate.y;
        const double z = (m_rotation[2][0] * orig.x) + (m_rotation[2][1] * orig.y) +
                         (m_rotation[2][2] * orig.z) + m_translate.z;
        return Coordinate{x, y, z, orig.weight};
    }

    /// transform() of every coordinate, in order, as a new vector (OpenRocket replaced them in
    /// their array or collection).
    [[nodiscard]] std::vector<Coordinate> transform(std::span<const Coordinate> orig) const;

    /// Only the linear part A * orig, without the translation; the weight of @p orig is kept.
    [[nodiscard]] constexpr Coordinate linearTransform(const Coordinate& orig) const noexcept
    {
        const double x =
            (m_rotation[0][0] * orig.x) + (m_rotation[0][1] * orig.y) + (m_rotation[0][2] * orig.z);
        const double y =
            (m_rotation[1][0] * orig.x) + (m_rotation[1][1] * orig.y) + (m_rotation[1][2] * orig.z);
        const double z =
            (m_rotation[2][0] * orig.x) + (m_rotation[2][1] * orig.y) + (m_rotation[2][2] * orig.z);
        return Coordinate{x, y, z, orig.weight};
    }

    /// The transformation that applies @p other first and then this one: with other = A x + b and
    /// this = C x + d the result is C A x + (C b + d), and result.transform(x) equals
    /// this->transform(other.transform(x)). The translation weights add up (Coordinate::add).
    [[nodiscard]] constexpr Transformation applyTransformation(
        const Transformation& other) const noexcept
    {
        Transformation combined{linearTransform(other.m_translate).add(m_translate)};
        combined.m_rotation = {multiplyRow(m_rotation[0], other.m_rotation),
                               multiplyRow(m_rotation[1], other.m_rotation),
                               multiplyRow(m_rotation[2], other.m_rotation)};
        return combined;
    }

    /// The transformation that undoes this one: A^-1 x - A^-1 c, so that
    /// inverse()->transform(transform(x)) is x and applyTransformation(*inverse()) is the
    /// identity. Nullopt when A is singular (its determinant is zero, as for the kProject*
    /// constants), when A holds a NaN or infinity, or when the translation's x, y or z is NaN or
    /// infinite, so that a value means a finite, invertible transformation. The translation's
    /// weight is kept and not checked. An addition to OpenRocket.
    [[nodiscard]] std::optional<Transformation> inverse() const noexcept;

    /// True when this equals kIdentity (with operator==' tolerance).
    [[nodiscard]] bool isIdentity() const noexcept;

    /// The linear part A, rows first.
    [[nodiscard]] constexpr const Matrix3& matrix() const noexcept { return m_rotation; }

    /// The translation c, as it was given (Java: getTranslationVector).
    [[nodiscard]] constexpr const Coordinate& translationVector() const noexcept
    {
        return m_translate;
    }

    /// The 4x4 homogeneous matrix in column-major order, ready for glLoadMatrix: elements 0-2,
    /// 4-6 and 8-10 are the columns of A, 12-14 the translation, and 3, 7, 11 and 15 are 0, 0, 0
    /// and 1 (Java: getGLMatrix).
    [[nodiscard]] constexpr std::array<double, 16> glMatrix() const noexcept
    {
        return {m_rotation[0][0], m_rotation[1][0], m_rotation[2][0], 0.0,
                m_rotation[0][1], m_rotation[1][1], m_rotation[2][1], 0.0,
                m_rotation[0][2], m_rotation[1][2], m_rotation[2][2], 0.0,
                m_translate.x,    m_translate.y,    m_translate.z,    1.0};
    }

    /// The rotation angle about the x axis read off the matrix, atan2 of the mean of the two
    /// off-diagonal x terms over the mean of the two diagonal ones; exact for a rotation about x
    /// alone (Java: getXrotation).
    [[nodiscard]] double xRotation() const noexcept;

    /// As xRotation(), about the y axis (Java: getYrotation).
    [[nodiscard]] double yRotation() const noexcept;

    /// As xRotation(), about the z axis (Java: getZrotation).
    [[nodiscard]] double zRotation() const noexcept;

    /// OpenRocket's equality: the nine matrix entries agree within MathUtil::equals and the
    /// translations are equal as Coordinates (weight included). Never true when an entry is NaN.
    [[nodiscard]] bool operator==(const Transformation& other) const noexcept;

    /// Three lines "[a b c]   [x]", "[d e f] + [y]", "[g h i]   [z]", each ending in a newline,
    /// two decimals each (Java's %3.2f). Deviation: std::format rounds the exact binary value
    /// half to even while Java's Formatter rounds the shortest round-trip decimal half up, and
    /// NaN and infinity print as nan and inf (Java: NaN and Infinity); see Coordinate::toString().
    /// For people and test output only; nothing written to a file goes through this.
    [[nodiscard]] std::string toString() const;

private:
    /// One row of the product row * other.
    [[nodiscard]] static constexpr std::array<double, 3> multiplyRow(
        const std::array<double, 3>& row, const Matrix3& other) noexcept
    {
        const double x = row[0];
        const double y = row[1];
        const double z = row[2];
        return {(x * other[0][0]) + (y * other[1][0]) + (z * other[2][0]),
                (x * other[0][1]) + (y * other[1][1]) + (z * other[2][1]),
                (x * other[0][2]) + (y * other[1][2]) + (z * other[2][2])};
    }

    Matrix3    m_rotation{{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}};
    Coordinate m_translate;
};

inline constexpr Transformation Transformation::kIdentity{};
inline constexpr Transformation Transformation::kProjectXY{
    Matrix3{{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 0.0}}}};
inline constexpr Transformation Transformation::kProjectYZ{
    Matrix3{{{0.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}}};
inline constexpr Transformation Transformation::kProjectXZ{
    Matrix3{{{1.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, {0.0, 0.0, 1.0}}}};

/// Writes toString(), so that GoogleTest prints a readable value when EXPECT_EQ on two
/// transformations fails.
std::ostream& operator<<(std::ostream& os, const Transformation& t);

}  // namespace QtRocket
