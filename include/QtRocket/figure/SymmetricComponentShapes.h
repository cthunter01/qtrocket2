#pragma once

#include <vector>

#include "QtRocket/figure/ComponentShape.h"

namespace QtRocket
{

class RocketComponent;
class Transformation;

/// The side outline of a SymmetricComponent drawn from its radius profile (OpenRocket's
/// SymmetricComponentShapes; TransitionShapes draws every transition shape but the conical one
/// with it). Its getShapeClass() names SymmetricComponent, which BodyTubeShapes and
/// TransitionShapes shadow for every concrete class, so only the static getShapesSide is used and
/// its back view (an error report in Java) is not ported.
namespace SymmetricComponentShapes
{

/// The number of points the profile is sampled at before it is refined (MINPOINTS).
inline constexpr int kMinPoints = 91;

/// The distance of the first and last samples from the component's ends (Java: delta), so that
/// the profile is sampled inside the component.
inline constexpr double kEndOffset = 0.0000001;

/// The cosine of the largest angle between two neighbouring segments of the outline that is not
/// refined (ACCEPTABLE_ANGLE, Math.cos(7.0 * Math.PI / 180.0)): Java's value, pinned rather than
/// computed so that the last bits of the C library's cos cannot move a refinement decision.
inline constexpr double kAcceptableAngle = 0.992546151641322;

/// Segments shorter than this along the axis (1 mm) are not refined.
inline constexpr double kMinimumRefinedLength = 0.001;

/// The outline of @p component seen from the side: one closed Path2D.Double through the profile.
/// The profile is sampled at x = kEndOffset, length * i / (kMinPoints - 1) for i = 1 ...
/// kMinPoints - 2 and length - kEndOffset (radius getRadius(x)); then, from the front, wherever
/// the angle at a point is too sharp (its cosine not above kAcceptableAngle) and the segment
/// before it is at least kMinimumRefinedLength long, the longer of its two segments gets its
/// midpoint, until no point needs one. The path starts at the aft end of the upper edge, runs
/// forward along it, back along the lower edge (y mirrored) and closes at its start; every point is
/// nose + (x, +-r), with nose = transformation.transform(origin), times @p scaleFactor.
/// @throws BugError when @p component is not a SymmetricComponent.
[[nodiscard]] std::vector<ComponentShape> getShapesSide(const RocketComponent& component,
                                                        const Transformation&  transformation,
                                                        double                 scaleFactor = 1.0);

}  // namespace SymmetricComponentShapes

}  // namespace QtRocket
