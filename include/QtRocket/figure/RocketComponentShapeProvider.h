#pragma once

#include <vector>

#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigureView.h"

namespace QtRocket
{

class RocketComponent;
class Transformation;

/// The shapes of a component in the 2D figure, by its kind (OpenRocket's
/// RocketComponentShapeProvider, which finds a RocketComponentShapeService through a
/// ServiceLoader and the component's superclass chain; a switch on ComponentKind here). Each kind
/// has one provider in figure/, a namespace named after Java's *Shapes class with the two
/// functions getShapesSide(component, transformation) and getShapesBack(component,
/// transformation); this namespace dispatches to them.
///
/// @p transformation takes the component's frame into the figure's: the figure's rotation applied
/// after the instance's transformation (figureRotation(view, rotation).applyTransformation(
/// context.transform)). The shapes are in the figure's plane, projected as projectToView() does.
namespace RocketComponentShapeProvider
{

/// The shapes of @p component seen from the side (the side and top views), in the provider's
/// order. A plain AxialStage has none (ComponentAssemblyShapes returns null for it).
[[nodiscard]] std::vector<ComponentShape> getShapesSide(const RocketComponent& component,
                                                        const Transformation&  transformation);

/// The shapes of @p component seen from behind (the back view), in the provider's order. A
/// plain AxialStage has none.
[[nodiscard]] std::vector<ComponentShape> getShapesBack(const RocketComponent& component,
                                                        const Transformation&  transformation);

/// The shapes RocketFigure draws for @p component in @p view (RocketFigure.addThisShape): none
/// for the Rocket and a plain AxialStage, else getShapesSide() for the side and top views and
/// getShapesBack() for the back view.
[[nodiscard]] std::vector<ComponentShape> getShapes(FigureView             view,
                                                    const RocketComponent& component,
                                                    const Transformation&  transformation);

}  // namespace RocketComponentShapeProvider

}  // namespace QtRocket
