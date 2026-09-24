#include "QtRocket/util/Quaternion.h"

#include <cmath>
#include <format>
#include <ostream>
#include <stdexcept>
#include <string>

#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

Quaternion Quaternion::rotation(const Coordinate& rotationVector) noexcept
{
    const double length = rotationVector.length();
    if (length < 0.000001)
    {
        return Quaternion{1, 0, 0, 0};
    }
    const double sin = std::sin(length / 2);
    const double cos = std::cos(length / 2);
    return Quaternion{cos, sin * rotationVector.x / length, sin * rotationVector.y / length,
                      sin * rotationVector.z / length};
}

Quaternion Quaternion::rotation(const Coordinate& axis, double angle)
{
    const Coordinate a   = axis.normalize();
    const double     sin = std::sin(angle / 2);
    const double     cos = std::cos(angle / 2);
    return Quaternion{cos, sin * a.x, sin * a.y, sin * a.z};
}

bool Quaternion::isNaN() const noexcept
{
    return std::isnan(m_x) || std::isnan(m_y) || std::isnan(m_z) || std::isnan(m_w);
}

Quaternion Quaternion::normalize() const
{
    const double n = norm();
    if (n < 0.0000001)
    {
        throw std::domain_error("attempting to normalize zero-quaternion");
    }
    return Quaternion{m_w / n, m_x / n, m_y / n, m_z / n};
}

Quaternion Quaternion::normalizeIfNecessary() const
{
    const double n2 = norm2();
    if (n2 < 0.999999 || n2 > 1.000001)
    {
        return normalize();
    }
    return *this;
}

double Quaternion::norm() const noexcept
{
    // OpenRocket caches this in the object; sqrt is cheap enough that the value type stays plain.
    return MathUtil::safeSqrt((m_x * m_x) + (m_y * m_y) + (m_z * m_z) + (m_w * m_w));
}

std::string Quaternion::toString() const
{
    // The header lists how this differs from Java's %f (rounding, nan/inf spelling).
    return std::format("Quaternion[{:f},{:f},{:f},{:f},norm={:f}]", m_w, m_x, m_y, m_z, norm());
}

std::ostream& operator<<(std::ostream& os, const Quaternion& q)
{
    return os << q.toString();
}

}  // namespace QtRocket
