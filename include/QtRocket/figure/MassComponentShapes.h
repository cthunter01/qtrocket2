#pragma once

#include <vector>

#include "QtRocket/figure/ComponentShape.h"

namespace QtRocket
{

class RocketComponent;
class Transformation;

/// The shapes of a MassComponent (OpenRocket's MassComponentShapes): a rounded rectangle with a
/// symbol inside that shows the component's MassComponent::MassComponentType.
namespace MassComponentShapes
{

/// The rays of a deployment charge's symbol (Java draws rays 1 to 14 of "15").
inline constexpr int kDeploymentChargeRays = 14;

/// The side view: first the RoundRect of the mass object (x, y - r, length, 2 r, arc, arc) at
/// the transformed radial position, with arc = min(length, 2 r) * 0.7, then, by type, the
/// symbol drawn inside its bounds:
/// - ALTIMETER: an arrow, one open path;
/// - FLIGHTCOMPUTER: a chip (a Rect) and twelve pins (Rects), the upper six then the lower six;
/// - DEPLOYMENTCHARGE: a CHORD Arc and a path of kDeploymentChargeRays rays from the centre;
/// - TRACKER: a dot (an Ellipse) and six OPEN Arcs, three on the right and three on the left;
/// - PAYLOAD: an Ellipse;
/// - RECOVERYHARDWARE: two RoundRects (arcs of 15 and 5 m, clamped to their size) and a Rect;
/// - BATTERY: one path of the cells;
/// - MASSCOMPONENT: nothing.
///
/// Deviations from OpenRocket:
/// - The rays of a deployment charge are drawn from a generator seeded by the component's id
///   (std::mt19937_64 seeded with the xor of the id's two halves, each draw (bits >> 11) * 2^-53),
///   where Java draws them from an unseeded java.util.Random on every paint, so that they no
///   longer flicker from one paint to the next (the user's decision U4); each ray still ends at
///   (x + rx, y + ry) with rx and ry in [0, min(width, height) / 4).
/// - The battery's cells stop where adding a cell width no longer moves the position (a
///   component so small that its cells are below the rounding of its position), where Java's
///   loop never ends.
/// @throws BugError when @p component is not a MassComponent.
[[nodiscard]] std::vector<ComponentShape> getShapesSide(const RocketComponent& component,
                                                        const Transformation&  transformation);

/// The back view: one Ellipse of the radius around the transformed radial position.
/// @throws BugError when @p component is not a MassObject.
[[nodiscard]] std::vector<ComponentShape> getShapesBack(const RocketComponent& component,
                                                        const Transformation&  transformation);

}  // namespace MassComponentShapes

}  // namespace QtRocket
