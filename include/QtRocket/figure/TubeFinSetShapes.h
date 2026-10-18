#pragma once

#include <vector>

#include "QtRocket/figure/ComponentShape.h"

namespace QtRocket
{

class RocketComponent;
class Transformation;

/// The shapes of a TubeFinSet (OpenRocket's TubeFinSetShapes): one tube fin, whose front centre is
/// the transformation's image of (0, outer radius, 0), drawn with its outer radius.
namespace TubeFinSetShapes
{

/// One Rect(loc.x, loc.y - R, length, 2 R), with R the outer radius and loc =
/// transformation.transform((0, R, 0)).
/// @throws BugError when @p component is not a TubeFinSet.
[[nodiscard]] std::vector<ComponentShape> getShapesSide(const RocketComponent& component,
                                                        const Transformation&  transformation);

/// One Ellipse(loc.z - R, loc.y - R, 2 R, 2 R) with the same loc.
/// @throws BugError when @p component is not a TubeFinSet.
[[nodiscard]] std::vector<ComponentShape> getShapesBack(const RocketComponent& component,
                                                        const Transformation&  transformation);

}  // namespace TubeFinSetShapes

}  // namespace QtRocket
