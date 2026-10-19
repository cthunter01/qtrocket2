#include "QtRocket/figure/FinSetShapes.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <utility>
#include <vector>

#include "ProviderComponent.h"
#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigurePath.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket::FinSetShapes
{

namespace
{

/// The thinnest fin whose back portion is drawn as a polygon of its own (0.5 mm).
constexpr double kMinimumBackThickness = 0.0005;

/// The open float polyline through @p points ((x, y) of each).
[[nodiscard]] FigurePath generatePath(std::span<const Coordinate> points)
{
    FigurePath finShape{FigurePath::Precision::FLOAT};
    for (std::size_t i = 0; i < points.size(); i++)
    {
        const Coordinate& curPoint = points[i];
        if (i == 0)
        {
            finShape.moveTo(curPoint.x, curPoint.y);
        }
        else
        {
            finShape.lineTo(curPoint.x, curPoint.y);
        }
    }
    return finShape;
}

/// The closed float polygon through @p array transformed by @p t, (z, y) of each.
[[nodiscard]] FigureShape makePolygonBack(std::span<const Coordinate> array,
                                          const Transformation&       t)
{
    // Make polygon
    FigurePath p{FigurePath::Precision::FLOAT};
    for (std::size_t i = 0; i < array.size(); i++)
    {
        const Coordinate a = t.transform(array[i]);
        if (i == 0)
        {
            p.moveTo(a.z, a.y);
        }
        else
        {
            p.lineTo(a.z, a.y);
        }
    }
    p.closePath();
    return FigureShape{std::move(p)};
}

[[nodiscard]] std::vector<FigureShape> uncantedShapesBack(const FinSet&         finset,
                                                          const Transformation& transformation)
{
    const double thickness = finset.getThickness();
    const double height    = finset.getSpan();
    const double tabHeight = finset.getTabHeight();

    // Generate base coordinates for a single fin
    const std::vector<Coordinate> c{
        Coordinate{0, 0, -thickness / 2}, Coordinate{0, 0, thickness / 2},
        Coordinate{0, height, thickness / 2}, Coordinate{0, height, -thickness / 2}};

    // Generate base coordinates for a single fin tab
    const std::vector<Coordinate> cTab{
        Coordinate{0, 0, -thickness / 2}, Coordinate{0, 0, thickness / 2},
        Coordinate{0, -tabHeight, thickness / 2}, Coordinate{0, -tabHeight, -thickness / 2}};

    // y translate the back view (if there is a fin point with non-zero y value)
    double yOffset = std::numeric_limits<double>::max();
    for (const Coordinate& point : finset.getFinPoints())
    {
        yOffset = MathUtil::min(yOffset, point.y);
    }
    const Transformation translateOffsetY   = Transformation::translation(0, yOffset, 0);
    const Transformation compositeTransform = transformation.applyTransformation(translateOffsetY);

    // Make polygon
    FigureShape p = makePolygonBack(c, compositeTransform);

    if (tabHeight != 0 && finset.getTabLength() != 0)
    {
        FigureShape pTab = makePolygonBack(cTab, compositeTransform);
        return {std::move(p), std::move(pTab)};
    }
    return {std::move(p)};
}

/// The side panel and the back portion of a canted fin or tab whose outline is @p points: the
/// points on the visible face, and the points from @p backIndex aft on both faces.
[[nodiscard]] std::vector<FigureShape> cantedPolygons(const FinSet&                  finset,
                                                      const Transformation&          transformation,
                                                      const std::vector<Coordinate>& points,
                                                      std::size_t                    backIndex)
{
    const double thickness = finset.getThickness();

    const Transformation cantTransform      = finset.getCantRotation();
    const Transformation compositeTransform = transformation.applyTransformation(cantTransform);

    const std::size_t       count = points.size();
    std::vector<Coordinate> sidePoints;
    sidePoints.reserve(count);
    std::vector<Coordinate> backPoints;
    backPoints.reserve(2 * (count - backIndex));
    const double sign = std::copysign(1.0, finset.getCantAngle());

    // Calculate points for the visible side panel
    for (const Coordinate& point : points)
    {
        sidePoints.push_back(point.add(0, 0, sign * thickness / 2));
    }

    // Calculate points for the back portion
    for (std::size_t j = count; j-- > backIndex;)
    {
        backPoints.push_back(points[j].add(0, 0, sign * thickness / 2));
    }
    for (std::size_t j = backIndex; j <= count - 1; j++)
    {
        backPoints.push_back(points[j].add(0, 0, -sign * thickness / 2));
    }

    // Generate shapes
    if (thickness > kMinimumBackThickness)
    {
        return {makePolygonBack(sidePoints, compositeTransform),
                makePolygonBack(backPoints, compositeTransform)};
    }
    return {makePolygonBack(sidePoints, compositeTransform)};
}

[[nodiscard]] std::vector<FigureShape> cantedShapesBackFins(const FinSet&         finset,
                                                            const Transformation& transformation)
{
    const std::vector<Coordinate> points = finset.getFinPoints();
    // Java's loops index points[-1] for no points (ArrayIndexOutOfBoundsException).
    QTROCKET_ASSERT(!points.empty());

    // this loop finds the index @ max-y, as visible from the back
    std::size_t maxIndex = points.size() - 1;
    for (; maxIndex > 0; maxIndex--)
    {
        if (points[maxIndex - 1].y < points[maxIndex].y)
        {
            break;
        }
    }
    return cantedPolygons(finset, transformation, points, maxIndex);
}

[[nodiscard]] std::vector<FigureShape> cantedShapesBackTabs(const FinSet&         finset,
                                                            const Transformation& transformation)
{
    const std::vector<Coordinate> points = finset.getTabPointsWithRoot();
    // Java's loops index points[-1] for no points (ArrayIndexOutOfBoundsException).
    QTROCKET_ASSERT(!points.empty());

    // this loop finds the index @ min-y, as visible from the back
    std::size_t minIndex = points.size() - 1;
    for (; minIndex > 0; minIndex--)
    {
        if (points[minIndex - 1].y > points[minIndex].y)
        {
            break;
        }
    }
    return cantedPolygons(finset, transformation, points, minIndex);
}

[[nodiscard]] std::vector<FigureShape> cantedShapesBack(const FinSet&         finset,
                                                        const Transformation& transformation)
{
    if (finset.getTabHeight() == 0 || finset.getTabLength() == 0)
    {
        return cantedShapesBackFins(finset, transformation);
    }

    std::vector<FigureShape> toReturn  = cantedShapesBackFins(finset, transformation);
    std::vector<FigureShape> shapesTab = cantedShapesBackTabs(finset, transformation);
    for (FigureShape& shape : shapesTab)
    {
        toReturn.push_back(std::move(shape));
    }
    return toReturn;
}

}  // namespace

std::vector<ComponentShape> getShapesSide(const RocketComponent& component,
                                          const Transformation&  transformation)
{
    const auto& finset = Detail::providerComponent<FinSet>(component, "FinSetShapes");

    // this supplied transformation includes:
    //  - baseRotationTransformation
    //  - mount-radius transformation
    //  - component-center offset transformation
    //  - component-instance offset transformation
    const Transformation cantRotation       = finset.getCantRotation();
    const Transformation compositeTransform = transformation.applyTransformation(cantRotation);

    // Translate & rotate points into place
    const std::vector<Coordinate> finPoints = compositeTransform.transform(finset.getFinPoints());
    const std::vector<Coordinate> tabPoints =
        compositeTransform.transform(finset.getTabPointsWithRoot());
    const std::vector<Coordinate> rootPoints = compositeTransform.transform(finset.getRootPoints());

    // Generate shapes: the fin polygon, the tab polygon and the root polygon
    return ComponentShape::fromShapes(
        {FigureShape{generatePath(finPoints)}, FigureShape{generatePath(tabPoints)},
         FigureShape{generatePath(rootPoints)}},
        component);
}

std::vector<ComponentShape> getShapesBack(const RocketComponent& component,
                                          const Transformation&  transformation)
{
    const auto& finset = Detail::providerComponent<FinSet>(component, "FinSetShapes");

    std::vector<FigureShape> toReturn;
    if (MathUtil::equals(finset.getCantAngle(), 0))
    {
        toReturn = uncantedShapesBack(finset, transformation);
    }
    else
    {
        toReturn = cantedShapesBack(finset, transformation);
    }
    return ComponentShape::fromShapes(std::move(toReturn), component);
}

}  // namespace QtRocket::FinSetShapes
