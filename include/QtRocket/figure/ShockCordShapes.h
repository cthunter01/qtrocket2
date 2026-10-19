#pragma once

#include <vector>

#include "QtRocket/figure/ComponentShape.h"

namespace QtRocket
{

class RocketComponent;
class Transformation;

/// The shapes of a ShockCord (OpenRocket's ShockCordShapes): a rounded rectangle with a wavy cord
/// inside.
namespace ShockCordShapes
{

/// The side view: the RoundRect of the mass object (as MassComponentShapes draws it), then the
/// cord, one open path of eight cubic curves across the middle.
/// @throws BugError when @p component is not a ShockCord.
[[nodiscard]] std::vector<ComponentShape> getShapesSide(const RocketComponent& component,
                                                        const Transformation&  transformation);

/// The back view: one Ellipse of the radius around the transformed radial position.
/// @throws BugError when @p component is not a ShockCord.
[[nodiscard]] std::vector<ComponentShape> getShapesBack(const RocketComponent& component,
                                                        const Transformation&  transformation);

}  // namespace ShockCordShapes

}  // namespace QtRocket
