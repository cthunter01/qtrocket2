#pragma once

#include <vector>

#include "QtRocket/figure/ComponentShape.h"

namespace QtRocket
{

class RocketComponent;
class Transformation;

/// The shapes of a Parachute (OpenRocket's ParachuteShapes): a rounded rectangle with a canopy
/// and its shroud lines inside.
namespace ParachuteShapes
{

/// The side view: the RoundRect of the mass object (as MassComponentShapes draws it), then the
/// canopy, a PIE Arc from 180 to 360 degrees whose diameter is half the height but at most 0.75
/// of the width, and the shroud lines, one open path of three subpaths.
/// @throws BugError when @p component is not a MassObject.
[[nodiscard]] std::vector<ComponentShape> getShapesSide(const RocketComponent& component,
                                                        const Transformation&  transformation);

/// The back view: one Ellipse of the radius around the transformed radial position.
/// @throws BugError when @p component is not a MassObject.
[[nodiscard]] std::vector<ComponentShape> getShapesBack(const RocketComponent& component,
                                                        const Transformation&  transformation);

}  // namespace ParachuteShapes

}  // namespace QtRocket
