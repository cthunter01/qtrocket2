#include "QtRocket/figure/BodyTubeShapes.h"

#include <vector>

#include "ProviderComponent.h"
#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/TubeShapes.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket::BodyTubeShapes
{

std::vector<ComponentShape> getShapesSide(const RocketComponent& component,
                                          const Transformation&  transformation)
{
    const auto& tube = Detail::providerComponent<BodyTube>(component, "BodyTubeShapes");

    const double length = tube.getLength();
    const double radius = tube.getOuterRadius();

    return ComponentShape::fromShapes({TubeShapes::getShapesSide(transformation, length, radius)},
                                      component);
}

std::vector<ComponentShape> getShapesBack(const RocketComponent& component,
                                          const Transformation&  transformation)
{
    const auto& tube = Detail::providerComponent<BodyTube>(component, "BodyTubeShapes");

    const double radius = tube.getOuterRadius();

    return ComponentShape::fromShapes({TubeShapes::getShapesBack(transformation, radius)},
                                      component);
}

}  // namespace QtRocket::BodyTubeShapes
