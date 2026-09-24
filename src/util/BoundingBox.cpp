#include "QtRocket/util/BoundingBox.h"

#include <cmath>
#include <cstddef>
#include <format>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket
{

namespace
{

/// Java's "%g" of @p value with the default precision of six significant digits, laid out as
/// java.util.Formatter does (the header of BoundingBox::toString() lists the forms and the
/// rounding deviation).
[[nodiscard]] std::string javaGeneral(double value)
{
    if (std::isnan(value))
    {
        return "NaN";
    }
    if (std::isinf(value))
    {
        return value < 0 ? "-Infinity" : "Infinity";
    }
    // Java prints the sign of a negative zero ("-0.00000").
    const std::string sign      = std::signbit(value) ? "-" : "";
    const double      magnitude = std::abs(value);
    if (magnitude == 0.0)
    {
        return sign + "0.00000";
    }

    // "d.ddddde+XX": the exponent once the value is rounded to six significant digits decides
    // the form, as Formatter's getExponentRounded does.
    const std::string      scientific = std::format("{:.5e}", magnitude);
    const std::size_t      e          = scientific.find('e');
    const std::string_view exponentText{scientific};
    int                    exponent = 0;
    for (const char c : exponentText.substr(e + 2))
    {
        exponent = (exponent * 10) + (c - '0');
    }
    if (exponentText[e + 1] == '-')
    {
        exponent = -exponent;
    }

    if (exponent < -4 || exponent >= 6)
    {
        return sign + scientific;
    }
    // Decimal notation with the digits after the point that six significant digits leave.
    return sign + std::format("{:.{}f}", magnitude, 5 - exponent);
}

}  // namespace

BoundingBox BoundingBox::transform(const Transformation& transformation) const noexcept
{
    const Coordinate p1 = transformation.transform(m_min);
    const Coordinate p2 = transformation.transform(m_max);

    BoundingBox newBox;
    newBox.update(p1);
    newBox.update(p2);
    return newBox;
}

std::vector<Coordinate> BoundingBox::toCollection() const
{
    return {m_max, m_min};
}

std::string BoundingBox::toString() const
{
    return std::format("[( {}, {}, {}) < ( {}, {}, {})]", javaGeneral(m_min.x),
                       javaGeneral(m_min.y), javaGeneral(m_min.z), javaGeneral(m_max.x),
                       javaGeneral(m_max.y), javaGeneral(m_max.z));
}

bool BoundingBox::operator==(const BoundingBox& other) const noexcept
{
    // Java: other.min.equals(this.min), so MathUtil::equals takes its tolerance from this box.
    return other.m_min == m_min && other.m_max == m_max;
}

std::ostream& operator<<(std::ostream& os, const BoundingBox& box)
{
    return os << box.toString();
}

}  // namespace QtRocket
