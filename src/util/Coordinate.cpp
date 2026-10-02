#include "QtRocket/util/Coordinate.h"

#include <cmath>
#include <cstddef>
#include <format>
#include <functional>
#include <ostream>
#include <string>

#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

bool Coordinate::isNaN() const noexcept
{
    return std::isnan(x) || std::isnan(y) || std::isnan(z) || std::isnan(weight);
}

double Coordinate::length() const noexcept
{
    // OpenRocket caches this in the object; sqrt is cheap enough that the value type stays plain.
    return MathUtil::safeSqrt((x * x) + (y * y) + (z * z));
}

Coordinate Coordinate::normalize() const
{
    const double l = length();
    if (l < 0.0000001)
    {
        bug("Cannot normalize zero coordinate");
    }
    return Coordinate{x / l, y / l, z / l, weight};
}

Coordinate Coordinate::average(const Coordinate& other) const noexcept
{
    const double w1 = weight + other.weight;
    if (std::abs(w1) < MathUtil::pow2(MathUtil::kEpsilon))
    {
        return Coordinate{(x + other.x) / 2, (y + other.y) / 2, (z + other.z) / 2, 0.0};
    }
    return Coordinate{((x * weight) + (other.x * other.weight)) / w1,
                      ((y * weight) + (other.y * other.weight)) / w1,
                      ((z * weight) + (other.z * other.weight)) / w1, w1};
}

double Coordinate::max() const noexcept
{
    return MathUtil::max(std::abs(x), std::abs(y), std::abs(z));
}

bool Coordinate::operator==(const Coordinate& other) const noexcept
{
    return MathUtil::equals(x, other.x) && MathUtil::equals(y, other.y) &&
           MathUtil::equals(z, other.z) && MathUtil::equals(weight, other.weight);
}

std::string Coordinate::toString() const
{
    // Java's String.format("%.5f"): Strings::formatFixed (std::format's {:.5f} rounds the binary
    // value half to even and spells NaN "nan").
    if (isWeighted())
    {
        return std::format("({},{},{},w={})", Strings::formatFixed(x, 5),
                           Strings::formatFixed(y, 5), Strings::formatFixed(z, 5),
                           Strings::formatFixed(weight, 5));
    }
    return std::format("({},{},{})", Strings::formatFixed(x, 5), Strings::formatFixed(y, 5),
                       Strings::formatFixed(z, 5));
}

std::string Coordinate::toPreciseString() const
{
    return std::format("cm= {}g @[{},{},{}]", Strings::formatFixed(weight, 8),
                       Strings::formatFixed(x, 8), Strings::formatFixed(y, 8),
                       Strings::formatFixed(z, 8));
}

std::ostream& operator<<(std::ostream& os, const Coordinate& c)
{
    return os << c.toString();
}

}  // namespace QtRocket

std::size_t std::hash<QtRocket::Coordinate>::operator()(
    const QtRocket::Coordinate& c) const noexcept
{
    return static_cast<std::size_t>(QtRocket::MathUtil::javaIntCast((c.x + c.y + c.z) * 100000));
}
