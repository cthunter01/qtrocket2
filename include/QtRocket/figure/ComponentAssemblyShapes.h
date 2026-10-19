#pragma once

#include <vector>

#include "QtRocket/figure/ComponentShape.h"

namespace QtRocket
{

class RocketComponent;
class Transformation;

/// The shapes of a component assembly (OpenRocket's ComponentAssemblyShapes, with its subclasses
/// PodSetShapes and ParallelStageShapes, which add nothing): the marker of EmptyShapes for a pod
/// set, a booster set or the rocket, none for a plain AxialStage. RocketFigure never asks for the
/// rocket's (RocketComponentShapeProvider::getShapes() leaves it out), and draws the markers of
/// pod sets and booster sets only when they are selected if the preference
/// Preferences::isShowMarkers() is set (RocketFigureGeometry).
///
/// The marker's radius is 0.03 times the rocket's bounding radius (Rocket::getBoundingRadius());
/// it sits at the assembly's reference point: the transformed origin, moved aft by the
/// assembly's length for AxialMethod::BOTTOM and by half of it for MIDDLE (the side view only),
/// and down by the assembly's bounding radius for RadiusMethod::RELATIVE.
namespace ComponentAssemblyShapes
{

/// The marker's radius as a fraction of the rocket's bounding radius.
inline constexpr double kMarkerRadiusFraction = 0.03;

/// The side view: nothing for a plain AxialStage (Java: null), else the five shapes of
/// EmptyShapes::getShapesSideWithSelectionSquare() at the reference point, the last (the
/// selection square) invisible (ComponentShape::invisible()).
/// @throws BugError when @p component is not a ComponentAssembly, or not in a Rocket.
[[nodiscard]] std::vector<ComponentShape> getShapesSide(const RocketComponent& component,
                                                        const Transformation&  transformation);

/// The back view: nothing for a plain AxialStage, else
/// EmptyShapes::getShapesBackWithSelectionSquare() at the reference point (without the axial
/// correction), the selection square invisible.
/// @throws BugError when @p component is not a ComponentAssembly, or not in a Rocket.
[[nodiscard]] std::vector<ComponentShape> getShapesBack(const RocketComponent& component,
                                                        const Transformation&  transformation);

}  // namespace ComponentAssemblyShapes

}  // namespace QtRocket
