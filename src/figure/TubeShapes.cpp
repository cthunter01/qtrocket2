#include "QtRocket/figure/TubeShapes.h"

#include "QtRocket/figure/FigureEllipse.h"
#include "QtRocket/figure/FigureRect.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket::TubeShapes
{

FigureShape getShapesSide(const Transformation& transformation, double length, double radius,
                          double scaleFactor)
{
    const Coordinate instanceAbsoluteLocation = transformation.transform(Coordinate::kZero);

    return FigureShape{FigureRect{instanceAbsoluteLocation.x * scaleFactor,
                                  (instanceAbsoluteLocation.y - radius) * scaleFactor,
                                  length * scaleFactor, 2 * radius * scaleFactor}};
}

FigureShape getShapesBack(const Transformation& transformation, double radius)
{
    const Coordinate instanceAbsoluteLocation = transformation.transform(Coordinate::kZero);

    return FigureShape{FigureEllipse{instanceAbsoluteLocation.z - radius,
                                     instanceAbsoluteLocation.y - radius, 2 * radius, 2 * radius}};
}

}  // namespace QtRocket::TubeShapes
