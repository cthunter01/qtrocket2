#pragma once

#include <vector>

#include "QtRocket/figure/ComponentShape.h"

namespace QtRocket
{

class RocketComponent;
class Transformation;

/// The shapes of a fin set: a TrapezoidFinSet, EllipticalFinSet or FreeformFinSet (OpenRocket's
/// FinSetShapes). Every shape is a Path2D.Float, so its coordinates are rounded to floats.
namespace FinSetShapes
{

/// Three open polylines (FigurePath::Precision::FLOAT, no closePath), each through the points
/// transformed by @p transformation applied after the fin set's cant rotation, (x, y) of each:
/// the fin's outline (FinSet::getFinPoints()), the tab with its root
/// (FinSet::getTabPointsWithRoot(), an empty path without a tab) and the root
/// (FinSet::getRootPoints()), in that order.
/// @throws BugError when @p component is not a FinSet.
[[nodiscard]] std::vector<ComponentShape> getShapesSide(const RocketComponent& component,
                                                        const Transformation&  transformation);

/// Closed float polygons in the back view's (z, y):
/// - without cant (MathUtil::equals(cant, 0)): the fin's cross-section, a rectangle of the
///   thickness from y = 0 to the span, and when the tab height and length are not 0 the tab's,
///   from y = 0 down to minus the tab height, both moved up by the lowest y of the fin points;
/// - with cant: the fin's side panel (the fin points moved by half the thickness towards the
///   cant's sign) and, when the fin is thicker than 0.5 mm, its back portion (the points from
///   the highest one aft, on both faces); then, when the tab height and length are not 0, the
///   same two of the tab (from its lowest point aft), all through the cant rotation.
/// @throws BugError when @p component is not a FinSet (and for a canted fin set without fin or
///         tab points, where Java throws an ArrayIndexOutOfBoundsException).
[[nodiscard]] std::vector<ComponentShape> getShapesBack(const RocketComponent& component,
                                                        const Transformation&  transformation);

}  // namespace FinSetShapes

}  // namespace QtRocket
