#include "QtRocket/rocket/EllipticalFinSet.h"

#include <cmath>
#include <cstddef>
#include <memory>
#include <numbers>
#include <vector>

#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

namespace
{

/// The outline of a fin of length 1 and height 1 (Java's static POINT_X and POINT_Y).
[[nodiscard]] std::vector<Point2D> makeUnitOutline()
{
    constexpr int        kPoints = EllipticalFinSet::kPoints;
    std::vector<Point2D> points;
    points.reserve(static_cast<std::size_t>(kPoints));
    for (int i = 0; i < kPoints; i++)
    {
        const double a = std::numbers::pi * (kPoints - 1 - i) / (kPoints - 1);
        points.emplace_back((std::cos(a) + 1) / 2, std::sin(a));
    }
    points.front() = Point2D{0, 0};
    points.back()  = Point2D{1, 0};
    return points;
}

/// makeUnitOutline(), made once.
[[nodiscard]] const std::vector<Point2D>& unitOutline()
{
    static const std::vector<Point2D> kOutline = makeUnitOutline();
    return kOutline;
}

}  // namespace

EllipticalFinSet::EllipticalFinSet()
{
    m_length = 0.05;
}

std::unique_ptr<RocketComponent> EllipticalFinSet::cloneShallow() const
{
    return std::make_unique<EllipticalFinSet>(*this);
}

std::vector<Coordinate> EllipticalFinSet::getFinPoints() const
{
    const double            len = MathUtil::max(m_length, 0.0001);
    std::vector<Coordinate> finPoints;
    finPoints.reserve(static_cast<std::size_t>(kPoints));
    for (const Point2D& point : unitOutline())
    {
        finPoints.emplace_back(point.x * len, point.y * m_height);
    }

    alignEndsWithRoot(finPoints);
    return finPoints;
}

double EllipticalFinSet::getSpan() const
{
    return m_height;
}

void EllipticalFinSet::setHeight(double height)
{
    if (MathUtil::equals(m_height, height))
    {
        return;
    }
    m_height = height;
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

void EllipticalFinSet::setLength(double length)
{
    if (MathUtil::equals(m_length, length))
    {
        return;
    }
    m_length = length;
    validateFinTabLength();
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

}  // namespace QtRocket
