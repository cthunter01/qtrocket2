#include "QtRocket/figure/LaunchLugShapes.h"

#include <vector>

#include "ProviderComponent.h"
#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/TubeShapes.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket::LaunchLugShapes
{

std::vector<ComponentShape> getShapesSide(const RocketComponent& component,
                                          const Transformation&  transformation)
{
    const auto& lug = Detail::providerComponent<LaunchLug>(component, "LaunchLugShapes");

    const double length = lug.getLength();
    const double radius = lug.getOuterRadius();

    return ComponentShape::fromShapes({TubeShapes::getShapesSide(transformation, length, radius)},
                                      component);
}

std::vector<ComponentShape> getShapesBack(const RocketComponent& component,
                                          const Transformation&  transformation)
{
    const auto& lug = Detail::providerComponent<LaunchLug>(component, "LaunchLugShapes");

    const double radius = lug.getOuterRadius();

    return ComponentShape::fromShapes({TubeShapes::getShapesBack(transformation, radius)},
                                      component);
}

}  // namespace QtRocket::LaunchLugShapes
