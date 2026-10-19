#include "QtRocket/figure/EmptyShapes.h"

#include <vector>

#include "QtRocket/figure/FigureLine.h"
#include "QtRocket/figure/FigureRect.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket::EmptyShapes
{

namespace
{

/// The square and its lines around (h, v): the side view's (x, y) or the back view's (z, y).
[[nodiscard]] std::vector<FigureShape> markerShapes(double h, double v, double radius)
{
    const double lineLength =
        getLineLength(radius);  // Length of the line protruding the center square

    std::vector<FigureShape> s;
    s.reserve(5);
    // Center square
    s.emplace_back(FigureRect{h - radius, v - radius, 2 * radius, 2 * radius});
    // Line North
    s.emplace_back(FigureLine{h, v + radius, h, v + radius + lineLength});
    // Line East
    s.emplace_back(FigureLine{h + radius, v, h + radius + lineLength, v});
    // Line South
    s.emplace_back(FigureLine{h, v - radius, h, v - radius - lineLength});
    // Line West
    s.emplace_back(FigureLine{h - radius, v, h - radius - lineLength, v});
    return s;
}

/// Invisible shape for selecting the component (= a square encompassing the component)
[[nodiscard]] FigureShape selectionSquare(double h, double v, double radius)
{
    const double lineLength = getLineLength(radius);
    return FigureShape{FigureRect{h - radius - lineLength, v - radius - lineLength,
                                  (lineLength * 2) + (radius * 2),
                                  (lineLength * 2) + (radius * 2)}};
}

}  // namespace

std::vector<FigureShape> getShapesSide(const Transformation& transformation, double radius)
{
    const Coordinate instanceAbsoluteLocation = transformation.transform(Coordinate::kZero);
    return markerShapes(instanceAbsoluteLocation.x, instanceAbsoluteLocation.y, radius);
}

std::vector<FigureShape> getShapesSideWithSelectionSquare(const Transformation& transformation,
                                                          double                radius)
{
    const Coordinate instanceAbsoluteLocation = transformation.transform(Coordinate::kZero);
    const double     x                        = instanceAbsoluteLocation.x;
    const double     y                        = instanceAbsoluteLocation.y;

    std::vector<FigureShape> shapes = getShapesSide(transformation, radius);
    shapes.push_back(selectionSquare(x, y, radius));
    return shapes;
}

std::vector<FigureShape> getShapesBack(const Transformation& transformation, double radius)
{
    const Coordinate instanceAbsoluteLocation = transformation.transform(Coordinate::kZero);
    return markerShapes(instanceAbsoluteLocation.z, instanceAbsoluteLocation.y, radius);
}

std::vector<FigureShape> getShapesBackWithSelectionSquare(const Transformation& transformation,
                                                          double                radius)
{
    const Coordinate instanceAbsoluteLocation = transformation.transform(Coordinate::kZero);
    const double     z                        = instanceAbsoluteLocation.z;
    const double     y                        = instanceAbsoluteLocation.y;

    std::vector<FigureShape> shapes = getShapesBack(transformation, radius);
    shapes.push_back(selectionSquare(z, y, radius));
    return shapes;
}

}  // namespace QtRocket::EmptyShapes
