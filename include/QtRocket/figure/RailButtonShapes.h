#pragma once

#include <vector>

#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigurePath.h"

namespace QtRocket
{

class FigureShape;
class RocketComponent;
class Transformation;

/// The shapes of a RailButton (OpenRocket's RailButtonShapes): three cylinders stacked on top of
/// each other, the base, the inner part and the flange, and a screw head on top. Seen from the
/// side, the faces of each cylinder are ellipses and its sides lines; seen from behind, each
/// cylinder is a rectangle turned with the button.
///
/// The view angle is the button's angle offset plus PI/2 - atan2(y', z') of the transformed unit
/// y vector (the figure's rotation and the instance's angle).
namespace RailButtonShapes
{

/// The side view:
/// - one Path2D.Double appending, for the base (when its height is positive), the inner part,
///   and the flange (when its height or the screw's is positive): the lower face (an Ellipse of
///   the diameter, |sin| of the view angle times the diameter high), the two side Lines and the
///   upper face, the heights multiplied by the cosine of the view angle;
/// - when the screw height is positive, the screw head, an OPEN Arc on the flange from 0 degrees
///   over -signum(screw height x cos) * 180 degrees (an empty extent when the head would hide
///   in the flange's face);
/// - last, an invisible Path2D.Double of a Rect over each cylinder drawn (ComponentShape::
///   invisible()), so that a click between the lines selects the button.
/// @throws BugError when @p component is not a RailButton.
[[nodiscard]] std::vector<ComponentShape> getShapesSide(const RocketComponent& component,
                                                        const Transformation&  transformation);

/// The back view: one Path2D.Double appending getRotatedRectangle() of the base (when its height
/// is positive), the inner part and the flange (when its height or the screw's is positive),
/// each starting where the one below ends along the view angle; then, when the screw height is
/// positive, the screw head, an OPEN Arc from 0 over -180 degrees rotated by minus the view
/// angle about the button's location (getRotatedShape(), a Path2D.Double).
/// @throws BugError when @p component is not a RailButton.
[[nodiscard]] std::vector<ComponentShape> getShapesBack(const RocketComponent& component,
                                                        const Transformation&  transformation);

/// A rectangle of half-width @p radius and height @p height standing on (x, y), turned by
/// @p angleRad (zero: upright, along +y): a closed Path2D.Double of its four corners, from the
/// lower left (x - r cos, y + r sin).
[[nodiscard]] FigurePath getRotatedRectangle(double x, double y, double radius, double height,
                                             double angleRad);

/// @p base rotated by @p angle radians about (x, y) (AffineTransform.getRotateInstance(angle, x,
/// y).createTransformedShape(base)): a double-precision path.
[[nodiscard]] FigurePath getRotatedShape(const FigureShape& base, double x, double y, double angle);

}  // namespace RailButtonShapes

}  // namespace QtRocket
