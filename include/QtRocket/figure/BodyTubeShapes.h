#pragma once

#include <vector>

#include "QtRocket/figure/ComponentShape.h"

namespace QtRocket
{

class RocketComponent;
class Transformation;

/// The shapes of a BodyTube (OpenRocket's BodyTubeShapes): a tube of the body tube's length and
/// outer radius (TubeShapes).
namespace BodyTubeShapes
{

/// One Rect: TubeShapes::getShapesSide(transformation, length, outer radius).
/// @throws BugError when @p component is not a BodyTube.
[[nodiscard]] std::vector<ComponentShape> getShapesSide(const RocketComponent& component,
                                                        const Transformation&  transformation);

/// One Ellipse: TubeShapes::getShapesBack(transformation, outer radius).
/// @throws BugError when @p component is not a BodyTube.
[[nodiscard]] std::vector<ComponentShape> getShapesBack(const RocketComponent& component,
                                                        const Transformation&  transformation);

}  // namespace BodyTubeShapes

}  // namespace QtRocket
