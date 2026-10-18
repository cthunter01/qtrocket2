#include "QtRocket/figure/TubeFinSetShapes.h"

#include <vector>

#include "ProviderComponent.h"
#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigureEllipse.h"
#include "QtRocket/figure/FigureRect.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TubeFinSet.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket::TubeFinSetShapes
{

std::vector<ComponentShape> getShapesSide(const RocketComponent& component,
                                          const Transformation&  transformation)
{
    const auto& finSet = Detail::providerComponent<TubeFinSet>(component, "TubeFinSetShapes");

    const double     outerRadius = finSet.getOuterRadius();
    const double     length      = finSet.getLength();
    const Coordinate location    = transformation.transform(Coordinate{0, outerRadius, 0});

    return ComponentShape::fromShapes(
        {FigureShape{FigureRect{location.x, location.y - outerRadius, length, 2 * outerRadius}}},
        component);
}

std::vector<ComponentShape> getShapesBack(const RocketComponent& component,
                                          const Transformation&  transformation)
{
    const auto& finSet = Detail::providerComponent<TubeFinSet>(component, "TubeFinSetShapes");

    const double     outerRadius = finSet.getOuterRadius();
    const Coordinate location    = transformation.transform(Coordinate{0, outerRadius, 0});

    return ComponentShape::fromShapes(
        {FigureShape{FigureEllipse{location.z - outerRadius, location.y - outerRadius,
                                   2 * outerRadius, 2 * outerRadius}}},
        component);
}

}  // namespace QtRocket::TubeFinSetShapes
