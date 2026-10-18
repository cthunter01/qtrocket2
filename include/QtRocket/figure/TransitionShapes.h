#pragma once

#include <vector>

#include "QtRocket/figure/ComponentShape.h"

namespace QtRocket
{

class RocketComponent;
class Transformation;

/// The shapes of a Transition or a NoseCone (OpenRocket's TransitionShapes).
namespace TransitionShapes
{

/// Shoulders up to this long (0.5 mm) are not drawn.
inline constexpr double kMinimumShoulderLength = 0.0005;

/// The outline seen from the side, then the shoulders as tubes (TubeShapes::getShapesSide()):
/// - a conical transition is one closed Path2D.Float, (x, y + r1) (x + L, y + r2) (x + L, y - r2)
///   (x, y - r1), with (x, y) = transformation.transform(origin), r1 and r2 the fore and aft
///   radii and L the length; every other shape is SymmetricComponentShapes::getShapesSide();
/// - the fore shoulder, when longer than kMinimumShoulderLength, is a Rect of its length and
///   radius ending at the front (translated by minus its length), the aft shoulder one starting
///   at the back (translated by L), in that order.
/// Every coordinate is multiplied by @p scaleFactor: 1 in the figure, the points per metre in
/// PrintableNoseCone (Milestone 8), which passes the identity.
/// @throws BugError when @p component is not a Transition.
[[nodiscard]] std::vector<ComponentShape> getShapesSide(const RocketComponent& component,
                                                        const Transformation&  transformation,
                                                        double                 scaleFactor = 1.0);

/// Two Ellipses around transformation.transform(origin), of the fore radius and of the aft
/// radius (in the back view's (z, y)).
/// @throws BugError when @p component is not a Transition.
[[nodiscard]] std::vector<ComponentShape> getShapesBack(const RocketComponent& component,
                                                        const Transformation&  transformation);

}  // namespace TransitionShapes

}  // namespace QtRocket
