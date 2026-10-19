#include "QtRocket/figure/ComponentAssemblyShapes.h"

#include <utility>
#include <vector>

#include "ProviderComponent.h"
#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/EmptyShapes.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/rocket/ComponentAssembly.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket::ComponentAssemblyShapes
{

namespace
{

/// Whether @p component is a normal stage, which has no marker (an AxialStage but not a
/// ParallelStage).
[[nodiscard]] bool isNormalStage(const RocketComponent& component) noexcept
{
    return component.kind() == ComponentKind::AXIAL_STAGE;
}

/// Returns the radius of the marker (i.e. the marker size), based on the rocket size.
[[nodiscard]] double getDisplayRadius(const RocketComponent& component)
{
    return component.getRocket().getBoundingRadius() * kMarkerRadiusFraction;
}

/// @p transformation moved to the "reference point" dictated by the component's radius offset.
[[nodiscard]] Transformation radiusCorrected(const ComponentAssembly& assembly,
                                             const Transformation&    transformation)
{
    if (assembly.getRadiusMethod() == RadiusMethod::RELATIVE)
    {
        const double boundingRadius = assembly.getBoundingRadius();
        return transformation.applyTransformation(
            Transformation::translation(0, -boundingRadius, 0));
    }
    return transformation;
}

/// @p shapes, the last one (the selection square) invisible.
[[nodiscard]] std::vector<ComponentShape> withInvisibleSelection(std::vector<FigureShape> shapes,
                                                                 const RocketComponent&   component)
{
    FigureShape selection = std::move(shapes.back());
    shapes.pop_back();
    std::vector<ComponentShape> result = ComponentShape::fromShapes(std::move(shapes), component);
    result.push_back(ComponentShape::invisible(std::move(selection), component));
    return result;
}

}  // namespace

std::vector<ComponentShape> getShapesSide(const RocketComponent& component,
                                          const Transformation&  transformation)
{
    // Ignore normal stages
    if (isNormalStage(component))
    {
        return {};
    }

    const auto& assembly =
        Detail::providerComponent<ComponentAssembly>(component, "ComponentAssemblyShapes");

    // Update the marker location based on the axial method. The axial method changes the
    // "reference point" of the component.
    Transformation correctedTransform = transformation;
    if (assembly.getAxialMethod() == AxialMethod::BOTTOM)
    {
        correctedTransform = transformation.applyTransformation(
            Transformation::translation(assembly.getLength(), 0, 0));
    }
    else if (assembly.getAxialMethod() == AxialMethod::MIDDLE)
    {
        correctedTransform = transformation.applyTransformation(
            Transformation::translation(assembly.getLength() / 2, 0, 0));
    }

    // Correct the radius to be at the "reference point" dictated by the component's radius
    // offset.
    correctedTransform = radiusCorrected(assembly, correctedTransform);

    const double markerRadius = getDisplayRadius(component);
    return withInvisibleSelection(
        EmptyShapes::getShapesSideWithSelectionSquare(correctedTransform, markerRadius), component);
}

std::vector<ComponentShape> getShapesBack(const RocketComponent& component,
                                          const Transformation&  transformation)
{
    // Ignore normal stages
    if (isNormalStage(component))
    {
        return {};
    }

    const auto& assembly =
        Detail::providerComponent<ComponentAssembly>(component, "ComponentAssemblyShapes");

    // Correct the radius to be at the "reference point" dictated by the component's radius
    // offset.
    const Transformation correctedTransform = radiusCorrected(assembly, transformation);

    const double markerRadius = getDisplayRadius(component);
    return withInvisibleSelection(
        EmptyShapes::getShapesBackWithSelectionSquare(correctedTransform, markerRadius), component);
}

}  // namespace QtRocket::ComponentAssemblyShapes
