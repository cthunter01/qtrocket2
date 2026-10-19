#pragma once

#include <vector>

#include "QtRocket/figure/FigureShape.h"

namespace QtRocket
{

class Transformation;

/// The marker of an "empty" or virtual object, e.g. a pod set without children
/// (OpenRocket's EmptyShapes, which ComponentAssemblyShapes draws): a square of half-width
/// @p radius around transformation.transform(origin) with a line of 3 * @p radius sticking out
/// of the middle of each side, north, east, south and west. The side view places it at the
/// transformed (x, y), the back view at (z, y).
namespace EmptyShapes
{

/// The length of each line, outside the square: 3 * @p radius.
[[nodiscard]] constexpr double getLineLength(double radius) noexcept
{
    return radius * 3;
}

/// The square and its four lines, in the side view (five shapes).
[[nodiscard]] std::vector<FigureShape> getShapesSide(const Transformation& transformation,
                                                     double                radius);

/// getShapesSide() and, last, the square around the whole marker, lines included, with which a
/// click selects the object (ComponentAssemblyShapes makes it invisible).
[[nodiscard]] std::vector<FigureShape> getShapesSideWithSelectionSquare(
    const Transformation& transformation, double radius);

/// The square and its four lines, in the back view (five shapes).
[[nodiscard]] std::vector<FigureShape> getShapesBack(const Transformation& transformation,
                                                     double                radius);

/// getShapesBack() and, last, the selection square around the whole marker.
[[nodiscard]] std::vector<FigureShape> getShapesBackWithSelectionSquare(
    const Transformation& transformation, double radius);

}  // namespace EmptyShapes

}  // namespace QtRocket
