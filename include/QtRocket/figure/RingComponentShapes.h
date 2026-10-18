#pragma once

#include <vector>

#include "QtRocket/figure/ComponentShape.h"

namespace QtRocket
{

class RocketComponent;
class Transformation;

/// The shapes of a RingComponent: an InnerTube, TubeCoupler, EngineBlock, CenteringRing or
/// Bulkhead (OpenRocket's RingComponentShapes). The ring is a tube of its outer radius, with a
/// second tube of its inner radius inside when the wall is at least kMinimumWall thick and the
/// inner radius is positive.
namespace RingComponentShapes
{

/// The thinnest wall drawn with an inner outline (1.2 mm); a thinner ring shows its outer outline
/// only.
inline constexpr double kMinimumWall = 0.0012;

/// TubeShapes::getShapesSide() of the outer radius, then of the inner radius when
/// outer - inner >= kMinimumWall and inner > 0.
/// @throws BugError when @p component is not a RingComponent.
[[nodiscard]] std::vector<ComponentShape> getShapesSide(const RocketComponent& component,
                                                        const Transformation&  transformation);

/// TubeShapes::getShapesBack() of the outer radius, then of the inner radius on the same
/// condition.
/// @throws BugError when @p component is not a RingComponent.
[[nodiscard]] std::vector<ComponentShape> getShapesBack(const RocketComponent& component,
                                                        const Transformation&  transformation);

}  // namespace RingComponentShapes

}  // namespace QtRocket
