#include "QtRocket/figure/SymmetricComponentShapes.h"

#include <cmath>
#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>

#include "ProviderComponent.h"
#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigurePath.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket::SymmetricComponentShapes
{

namespace
{

/*
 * cosAngle = v1.v2 / |v1|*|v2| = v1.v2 / sqrt(v1.v1*v2.v2)
 */
[[nodiscard]] double cosAngle(const Coordinate& v1, const Coordinate& v2,
                              const Coordinate& v3) noexcept
{
    const double cos = Coordinate::dot(v1.sub(v2), v2.sub(v3));
    const double len = MathUtil::safeSqrt(v1.sub(v2).length2() * v2.sub(v3).length2());
    return cos / len;
}

/// Java's cosAngle(v1, v2, v3) > ACCEPTABLE_ANGLE, but for an angle that has no cosine (NaN: a
/// coordinate that is not finite, or squares that overflow), which is acceptable here (a
/// deviation, see profilePoints()).
[[nodiscard]] bool angleAcceptable(const Coordinate& v1, const Coordinate& v2,
                                   const Coordinate& v3) noexcept
{
    const double cos = cosAngle(v1, v2, v3);
    return std::isnan(cos) || cos > kAcceptableAngle;
}

/// The profile: kMinPoints samples, refined where the outline turns too sharply.
///
/// Deviation from OpenRocket: Java's refinement loop has no bound, and it never ends when the
/// profile is not finite, as for a length of +inf or NaN (a dialog takes "1e400" as +inf, and
/// setLength() keeps it) or one so large that the squares of the segments overflow, or the radius
/// becomes NaN (an ogive of 1e300 m), nor where two neighbouring doubles more than 1 mm apart keep
/// a sharp angle: every pass inserts another midpoint, or the same one again, until the memory
/// runs out. So, as MassComponentShapes bounds the battery's loop, an angle without a cosine (NaN)
/// is acceptable, and a segment is split only while its midpoint lies strictly between its ends
/// (which it always does for a finite segment of at least 1 mm within 2^41 m of the origin). Every
/// decision for a finite profile of a sane size is Java's.
[[nodiscard]] std::vector<Coordinate> profilePoints(const SymmetricComponent& c)
{
    std::vector<Coordinate> points;
    points.reserve(static_cast<std::size_t>(kMinPoints));
    double x = kEndOffset;
    points.emplace_back(x, c.getRadius(x), 0);
    for (int i = 1; i < kMinPoints - 1; i++)
    {
        x = c.getLength() * i / (kMinPoints - 1);
        points.emplace_back(x, c.getRadius(x), 0);
    }
    x = c.getLength() - kEndOffset;
    points.emplace_back(x, c.getRadius(x), 0);

    std::size_t i = 0;
    while (i + 2 < points.size())
    {
        if (angleAcceptable(points[i], points[i + 1], points[i + 2]) ||
            points[i + 1].x - points[i].x < kMinimumRefinedLength)  // 1mm
        {
            i++;
            continue;
        }

        // Split the longer of the areas
        std::size_t n = 0;
        if (points[i + 2].x - points[i + 1].x > points[i + 1].x - points[i].x)
        {
            n = i + 1;
        }
        else
        {
            n = i;
        }

        x = (points[n].x + points[n + 1].x) / 2;
        // Not in Java: a segment whose midpoint is one of its ends can no longer be halved.
        const bool halved = x > points[n].x && x < points[n + 1].x;
        if (!halved)
        {
            i++;
            continue;
        }
        points.insert(std::next(points.begin(), static_cast<std::ptrdiff_t>(n + 1)),
                      Coordinate{x, c.getRadius(x), 0});
    }
    return points;
}

}  // namespace

std::vector<ComponentShape> getShapesSide(const RocketComponent& component,
                                          const Transformation& transformation, double scaleFactor)
{
    const auto& c =
        Detail::providerComponent<SymmetricComponent>(component, "SymmetricComponentShapes");

    const std::vector<Coordinate> points = profilePoints(c);

    const std::size_t len  = points.size();
    const Coordinate  nose = transformation.transform(Coordinate::kZero);

    // TODO: LOW: curved path instead of linear
    FigurePath path;
    path.moveTo((nose.x + points[len - 1].x) * scaleFactor,
                (nose.y + points[len - 1].y) * scaleFactor);
    // From points[len - 2] down to points[0].
    for (std::size_t k = 2; k <= len; k++)
    {
        const Coordinate& point = points[len - k];
        path.lineTo((nose.x + point.x) * scaleFactor, (nose.y + point.y) * scaleFactor);
    }
    for (const Coordinate& point : points)
    {
        path.lineTo((nose.x + point.x) * scaleFactor, (nose.y - point.y) * scaleFactor);
    }
    path.lineTo((nose.x + points[len - 1].x) * scaleFactor,
                (nose.y + points[len - 1].y) * scaleFactor);
    path.closePath();

    return ComponentShape::fromShapes({FigureShape{std::move(path)}}, component);
}

}  // namespace QtRocket::SymmetricComponentShapes
