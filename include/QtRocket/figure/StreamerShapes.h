#pragma once

#include <vector>

#include "QtRocket/figure/ComponentShape.h"

namespace QtRocket
{

class RocketComponent;
class Transformation;

/// The shapes of a Streamer (OpenRocket's StreamerShapes): a rounded rectangle with a fluttering
/// streamer inside.
namespace StreamerShapes
{

/// The side view: the RoundRect of the mass object (as MassComponentShapes draws it), then the
/// streamer, one path: up the left edge, eight cubic curves along the top, down the right edge,
/// and a second subpath of eight cubic curves along the bottom.
/// @throws BugError when @p component is not a MassObject.
[[nodiscard]] std::vector<ComponentShape> getShapesSide(const RocketComponent& component,
                                                        const Transformation&  transformation);

/// The back view: one Ellipse of the radius around the transformed radial position.
/// @throws BugError when @p component is not a MassObject.
[[nodiscard]] std::vector<ComponentShape> getShapesBack(const RocketComponent& component,
                                                        const Transformation&  transformation);

}  // namespace StreamerShapes

}  // namespace QtRocket
