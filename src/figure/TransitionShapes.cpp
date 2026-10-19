#include "QtRocket/figure/TransitionShapes.h"

#include <utility>
#include <vector>

#include "ProviderComponent.h"
#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigureEllipse.h"
#include "QtRocket/figure/FigurePath.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/figure/SymmetricComponentShapes.h"
#include "QtRocket/figure/TubeShapes.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket::TransitionShapes
{

namespace
{

/// The closed float path of a conical transition.
[[nodiscard]] FigurePath conicalPath(const Transition& transition, const Coordinate& frontCenter,
                                     double scaleFactor)
{
    const double length = transition.getLength();
    const double r1     = transition.getForeRadius();
    const double r2     = transition.getAftRadius();

    FigurePath path{FigurePath::Precision::FLOAT};
    path.moveTo(frontCenter.x * scaleFactor, (frontCenter.y + r1) * scaleFactor);
    path.lineTo((frontCenter.x + length) * scaleFactor, (frontCenter.y + r2) * scaleFactor);
    path.lineTo((frontCenter.x + length) * scaleFactor, (frontCenter.y - r2) * scaleFactor);
    path.lineTo(frontCenter.x * scaleFactor, (frontCenter.y - r1) * scaleFactor);
    path.closePath();
    return path;
}

}  // namespace

std::vector<ComponentShape> getShapesSide(const RocketComponent& component,
                                          const Transformation& transformation, double scaleFactor)
{
    const auto& transition = Detail::providerComponent<Transition>(component, "TransitionShapes");

    const Coordinate instanceAbsoluteLocation = transformation.transform(Coordinate::kZero);

    std::vector<ComponentShape> shapes;

    // Simpler shape for conical transition, others use the method from SymmetricComponent
    if (transition.getShapeType() == TransitionShape::CONICAL)
    {
        shapes = ComponentShape::fromShapes(
            {FigureShape{conicalPath(transition, instanceAbsoluteLocation, scaleFactor)}},
            component);
    }
    else
    {
        shapes = SymmetricComponentShapes::getShapesSide(component, transformation, scaleFactor);
    }

    if (transition.getForeShoulderLength() > kMinimumShoulderLength)
    {
        const double         shoulderLength = transition.getForeShoulderLength();
        const double         shoulderRadius = transition.getForeShoulderRadius();
        const Transformation offsetTransform =
            Transformation::translation(-transition.getForeShoulderLength(), 0, 0);
        const Transformation foreShoulderTransform =
            transformation.applyTransformation(offsetTransform);

        shapes.emplace_back(TubeShapes::getShapesSide(foreShoulderTransform, shoulderLength,
                                                      shoulderRadius, scaleFactor),
                            component);
    }
    if (transition.getAftShoulderLength() > kMinimumShoulderLength)
    {
        const double         shoulderLength = transition.getAftShoulderLength();
        const double         shoulderRadius = transition.getAftShoulderRadius();
        const Transformation offsetTransform =
            Transformation::translation(transition.getLength(), 0, 0);
        const Transformation aftShoulderTransform =
            transformation.applyTransformation(offsetTransform);

        shapes.emplace_back(TubeShapes::getShapesSide(aftShoulderTransform, shoulderLength,
                                                      shoulderRadius, scaleFactor),
                            component);
    }
    return shapes;
}

std::vector<ComponentShape> getShapesBack(const RocketComponent& component,
                                          const Transformation&  transformation)
{
    const auto& transition = Detail::providerComponent<Transition>(component, "TransitionShapes");

    const double r1 = transition.getForeRadius();
    const double r2 = transition.getAftRadius();

    const Coordinate center = transformation.transform(Coordinate::kZero);

    std::vector<FigureShape> s{
        FigureShape{FigureEllipse{center.z - r1, center.y - r1, 2 * r1, 2 * r1}},
        FigureShape{FigureEllipse{center.z - r2, center.y - r2, 2 * r2, 2 * r2}}};
    return ComponentShape::fromShapes(std::move(s), component);
}

}  // namespace QtRocket::TransitionShapes
