#include "QtRocket/util/Transformation.h"

#include <cmath>
#include <cstddef>
#include <format>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <vector>

#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Quaternion.h"

namespace QtRocket
{

Transformation Transformation::rotation(const Coordinate& angles) noexcept
{
    Transformation transformation;

    // Apply rotations in X-Y-Z order
    if (std::abs(angles.x) > kAngleEpsilon)
    {
        transformation = transformation.applyTransformation(rotateX(angles.x));
    }
    if (std::abs(angles.y) > kAngleEpsilon)
    {
        transformation = transformation.applyTransformation(rotateY(angles.y));
    }
    if (std::abs(angles.z) > kAngleEpsilon)
    {
        transformation = transformation.applyTransformation(rotateZ(angles.z));
    }
    return transformation;
}

Transformation Transformation::rotation(const Coordinate& angles, const Coordinate& origin) noexcept
{
    // 1. Translate to origin point
    const Transformation translateToOrigin = translation(-origin.x, -origin.y, -origin.z);
    // 2. Apply the rotations
    const Transformation rotateTransform = rotation(angles);
    // 3. Translate back from origin
    const Transformation translateBack = translation(origin);

    // First translate to origin, then rotate, then translate back
    return translateBack.applyTransformation(
        rotateTransform.applyTransformation(translateToOrigin));
}

Transformation Transformation::rotation(const Quaternion& q) noexcept
{
    // The matrix of x -> q * x * conj(q), term for term what Quaternion::rotate() evaluates, so
    // that the two agree for a non-unit quaternion as well.
    const double w = q.w();
    const double x = q.x();
    const double y = q.y();
    const double z = q.z();
    return Transformation{Matrix3{
        {{(w * w) + (x * x) - (y * y) - (z * z), 2 * ((x * y) - (w * z)), 2 * ((x * z) + (w * y))},
         {2 * ((x * y) + (w * z)), (w * w) - (x * x) + (y * y) - (z * z), 2 * ((y * z) - (w * x))},
         {2 * ((x * z) - (w * y)), 2 * ((y * z) + (w * x)),
          (w * w) - (x * x) - (y * y) + (z * z)}}}};
}

Transformation Transformation::axialRotation(double theta) noexcept
{
    return rotateX(theta);
}

Transformation Transformation::rotateX(double theta) noexcept
{
    if (kAngleEpsilon > std::abs(theta))
    {
        return kIdentity;
    }
    const double cosTheta = std::cos(theta);
    const double sinTheta = std::sin(theta);
    return Transformation{
        Matrix3{{{1.0, 0.0, 0.0}, {0.0, cosTheta, -sinTheta}, {0.0, sinTheta, cosTheta}}}};
}

Transformation Transformation::rotateY(double theta) noexcept
{
    if (kAngleEpsilon > std::abs(theta))
    {
        return kIdentity;
    }
    const double cosTheta = std::cos(theta);
    const double sinTheta = std::sin(theta);
    return Transformation{
        Matrix3{{{cosTheta, 0.0, sinTheta}, {0.0, 1.0, 0.0}, {-sinTheta, 0.0, cosTheta}}}};
}

Transformation Transformation::rotateZ(double theta) noexcept
{
    if (kAngleEpsilon > std::abs(theta))
    {
        return kIdentity;
    }
    const double cosTheta = std::cos(theta);
    const double sinTheta = std::sin(theta);
    return Transformation{
        Matrix3{{{cosTheta, -sinTheta, 0.0}, {sinTheta, cosTheta, 0.0}, {0.0, 0.0, 1.0}}}};
}

Transformation Transformation::eulerAngle313(double alpha, double beta, double gamma) noexcept
{
    return Transformation{Matrix3{{{(std::cos(alpha) * std::cos(gamma)) -
                                        (std::sin(alpha) * std::cos(beta) * std::sin(gamma)),
                                    (-std::cos(alpha) * std::sin(gamma)) -
                                        (std::sin(alpha) * std::cos(beta) * std::cos(gamma)),
                                    std::sin(alpha) * std::sin(beta)},
                                   {(std::sin(alpha) * std::cos(gamma)) +
                                        (std::cos(alpha) * std::cos(beta) * std::sin(gamma)),
                                    (-std::sin(alpha) * std::sin(gamma)) +
                                        (std::cos(alpha) * std::cos(beta) * std::cos(gamma)),
                                    -std::cos(alpha) * std::sin(beta)},
                                   {std::sin(beta) * std::sin(gamma),
                                    std::sin(beta) * std::cos(gamma), std::cos(beta)}}},
                          Coordinate::kZero};
}

std::vector<Coordinate> Transformation::transform(std::span<const Coordinate> orig) const
{
    std::vector<Coordinate> result;
    result.reserve(orig.size());
    for (const Coordinate& c : orig)
    {
        result.push_back(transform(c));
    }
    return result;
}

std::optional<Transformation> Transformation::inverse() const noexcept
{
    const double m00 = m_rotation[0][0];
    const double m01 = m_rotation[0][1];
    const double m02 = m_rotation[0][2];
    const double m10 = m_rotation[1][0];
    const double m11 = m_rotation[1][1];
    const double m12 = m_rotation[1][2];
    const double m20 = m_rotation[2][0];
    const double m21 = m_rotation[2][1];
    const double m22 = m_rotation[2][2];

    // The cofactors of the first row give det by expansion along it; adj(A) = C^T puts them in
    // the first column of the inverse.
    const double c00 = (m11 * m22) - (m12 * m21);
    const double c01 = -((m10 * m22) - (m12 * m20));
    const double c02 = (m10 * m21) - (m11 * m20);
    const double det = (m00 * c00) + (m01 * c01) + (m02 * c02);
    if (!std::isfinite(det) || det == 0.0)
    {
        return std::nullopt;
    }
    if (!std::isfinite(m_translate.x) || !std::isfinite(m_translate.y) ||
        !std::isfinite(m_translate.z))
    {
        return std::nullopt;
    }

    // A^-1 = adj(A) / det, the transposed cofactor matrix: entry [i][j] is C_ji / det.
    const Matrix3    inverseMatrix{{{c00 / det,                             // C00
                                     -((m01 * m22) - (m02 * m21)) / det,    // C10
                                     ((m01 * m12) - (m02 * m11)) / det},    // C20
                                    {c01 / det,                             // C01
                                     ((m00 * m22) - (m02 * m20)) / det,     // C11
                                     -((m00 * m12) - (m02 * m10)) / det},   // C21
                                    {c02 / det,                             // C02
                                     -((m00 * m21) - (m01 * m20)) / det,    // C12
                                     ((m00 * m11) - (m01 * m10)) / det}}};  // C22
    const Coordinate moved = Transformation{inverseMatrix}.linearTransform(m_translate);
    return Transformation{inverseMatrix,
                          Coordinate{-moved.x, -moved.y, -moved.z, m_translate.weight}};
}

bool Transformation::isIdentity() const noexcept
{
    return *this == kIdentity;
}

double Transformation::xRotation() const noexcept
{
    return std::atan2((m_rotation[2][1] - m_rotation[1][2]) / 2.0,
                      (m_rotation[1][1] + m_rotation[2][2]) / 2.0);
}

double Transformation::yRotation() const noexcept
{
    return std::atan2((m_rotation[0][2] - m_rotation[2][0]) / 2.0,
                      (m_rotation[0][0] + m_rotation[2][2]) / 2.0);
}

double Transformation::zRotation() const noexcept
{
    return std::atan2((m_rotation[1][0] - m_rotation[0][1]) / 2.0,
                      (m_rotation[0][0] + m_rotation[1][1]) / 2.0);
}

bool Transformation::operator==(const Transformation& other) const noexcept
{
    for (std::size_t i = 0; i < 3; i++)
    {
        for (std::size_t j = 0; j < 3; j++)
        {
            if (!MathUtil::equals(m_rotation.at(i).at(j), other.m_rotation.at(i).at(j)))
            {
                return false;
            }
        }
    }
    return m_translate == other.m_translate;
}

std::string Transformation::toString() const
{
    // The header lists how this differs from Java's %3.2f (rounding, nan/inf spelling).
    return std::format(
        "[{:3.2f} {:3.2f} {:3.2f}]   [{:3.2f}]\n"
        "[{:3.2f} {:3.2f} {:3.2f}] + [{:3.2f}]\n"
        "[{:3.2f} {:3.2f} {:3.2f}]   [{:3.2f}]\n",
        m_rotation[0][0], m_rotation[0][1], m_rotation[0][2], m_translate.x, m_rotation[1][0],
        m_rotation[1][1], m_rotation[1][2], m_translate.y, m_rotation[2][0], m_rotation[2][1],
        m_rotation[2][2], m_translate.z);
}

std::ostream& operator<<(std::ostream& os, const Transformation& t)
{
    return os << t.toString();
}

}  // namespace QtRocket
