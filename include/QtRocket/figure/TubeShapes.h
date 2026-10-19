#pragma once

#include "QtRocket/figure/FigureShape.h"

namespace QtRocket
{

class Transformation;

/// The outline of a tube in the 2D figure, which the providers of the tube-like components share
/// (OpenRocket's TubeShapes; its getShapeClass() names the Tube class, which every tube-like
/// component's own provider shadows, so only its static helpers are used). The tube's front
/// centre is @p transformation's image of the origin, loc.
namespace TubeShapes
{

/// The tube seen from the side: Rect(loc.x, loc.y - radius, length, 2 radius), every value
/// multiplied by @p scaleFactor (PrintableNoseCone's shoulders use one; the figure's is 1).
[[nodiscard]] FigureShape getShapesSide(const Transformation& transformation, double length,
                                        double radius, double scaleFactor = 1.0);

/// The tube seen from behind: Ellipse(loc.z - radius, loc.y - radius, 2 radius, 2 radius).
[[nodiscard]] FigureShape getShapesBack(const Transformation& transformation, double radius);

}  // namespace TubeShapes

}  // namespace QtRocket
