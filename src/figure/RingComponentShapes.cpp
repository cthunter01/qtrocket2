#include "QtRocket/figure/RingComponentShapes.h"

#include <utility>
#include <vector>

#include "ProviderComponent.h"
#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/figure/TubeShapes.h"
#include "QtRocket/rocket/RingComponent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket::RingComponentShapes
{

namespace
{

/// Whether the inner outline is drawn too.
[[nodiscard]] bool drawsInner(double outerRadius, double innerRadius) noexcept
{
    return (outerRadius - innerRadius >= kMinimumWall) && (innerRadius > 0);
}

}  // namespace

std::vector<ComponentShape> getShapesSide(const RocketComponent& component,
                                          const Transformation&  transformation)
{
    const auto& tube = Detail::providerComponent<RingComponent>(component, "RingComponentShapes");

    const double length      = tube.getLength();
    const double outerRadius = tube.getOuterRadius();
    const double innerRadius = tube.getInnerRadius();

    std::vector<FigureShape> s;
    if (drawsInner(outerRadius, innerRadius))
    {
        // Draw outer and inner
        s = {TubeShapes::getShapesSide(transformation, length, outerRadius),
             TubeShapes::getShapesSide(transformation, length, innerRadius)};
    }
    else
    {
        // Draw only outer
        s = {TubeShapes::getShapesSide(transformation, length, outerRadius)};
    }
    return ComponentShape::fromShapes(std::move(s), component);
}

std::vector<ComponentShape> getShapesBack(const RocketComponent& component,
                                          const Transformation&  transformation)
{
    const auto& tube = Detail::providerComponent<RingComponent>(component, "RingComponentShapes");

    const double outerRadius = tube.getOuterRadius();
    const double innerRadius = tube.getInnerRadius();

    std::vector<FigureShape> s;
    if (drawsInner(outerRadius, innerRadius))
    {
        s = {TubeShapes::getShapesBack(transformation, outerRadius),
             TubeShapes::getShapesBack(transformation, innerRadius)};
    }
    else
    {
        s = {TubeShapes::getShapesBack(transformation, outerRadius)};
    }
    return ComponentShape::fromShapes(std::move(s), component);
}

}  // namespace QtRocket::RingComponentShapes
