#pragma once

#include <vector>

#include "QtRocket/figure/ComponentShape.h"

namespace QtRocket
{

class RocketComponent;
class Transformation;

/// The shapes of a LaunchLug (OpenRocket's LaunchLugShapes): a tube of the lug's length and
/// outer radius (TubeShapes).
namespace LaunchLugShapes
{

/// One Rect: TubeShapes::getShapesSide(transformation, length, outer radius).
/// @throws BugError when @p component is not a LaunchLug.
[[nodiscard]] std::vector<ComponentShape> getShapesSide(const RocketComponent& component,
                                                        const Transformation&  transformation);

/// One Ellipse: TubeShapes::getShapesBack(transformation, outer radius).
/// @throws BugError when @p component is not a LaunchLug.
[[nodiscard]] std::vector<ComponentShape> getShapesBack(const RocketComponent& component,
                                                        const Transformation&  transformation);

}  // namespace LaunchLugShapes

}  // namespace QtRocket
