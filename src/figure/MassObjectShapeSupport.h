#pragma once

// The outlines every mass object starts from (internal to the figure/ subsystem): the rounded
// rectangle of the side view and the circle of the back view, which OpenRocket's
// MassComponentShapes, ParachuteShapes, StreamerShapes and ShockCordShapes each compute with the
// same lines, and the rectangle helpers their symbols use.

#include <cmath>

#include "QtRocket/figure/FigureEllipse.h"
#include "QtRocket/figure/FigureRoundRect.h"
#include "QtRocket/rocket/MassObject.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Transformation.h"

namespace QtRocket::Detail
{

/// Where @p transformation puts the axis of @p massObj: its radial position and direction
/// (rp cos(phi), rp sin(phi)) in the component's frame, transformed.
[[nodiscard]] inline Coordinate massObjectRenderPosition(const MassObject&     massObj,
                                                         const Transformation& transformation)
{
    const double radialDistance     = massObj.getRadialPosition();
    const double radialAngleRadians = massObj.getRadialDirection();

    const Coordinate localPosition{0, radialDistance * std::cos(radialAngleRadians),
                                   radialDistance * std::sin(radialAngleRadians)};
    return transformation.transform(localPosition);
}

/// The side view of @p massObj: a rectangle of its length and diameter from the transformed axis
/// position, its corners rounded with arcs of min(length, 2 radius) * 0.7 ("magic number, but
/// it's only cosmetic -- it just has to look pretty"). The length is read before the radius, as
/// in Java (with an automatic radius, MassObject::getRadius() changes what getLength() reads).
[[nodiscard]] inline FigureRoundRect massObjectSide(const MassObject&     massObj,
                                                    const Transformation& transformation)
{
    const double length = massObj.getLength();
    const double radius = massObj.getRadius();  // radius of the object, itself
    const double arc    = MathUtil::javaMin(length, 2 * radius) * 0.7;

    const Coordinate renderPosition = massObjectRenderPosition(massObj, transformation);
    return FigureRoundRect{
        renderPosition.x, renderPosition.y - radius, length, 2 * radius, arc, arc};
}

/// The back view of @p massObj: the circle of its radius around the transformed axis position,
/// in (z, y).
[[nodiscard]] inline FigureEllipse massObjectBack(const MassObject&     massObj,
                                                  const Transformation& transformation)
{
    const double radius   = massObj.getRadius();  // radius of the object, itself
    const double diameter = 2 * radius;

    const Coordinate renderPosition = massObjectRenderPosition(massObj, transformation);
    return FigureEllipse{renderPosition.z - radius, renderPosition.y - radius, diameter, diameter};
}

/// RectangularShape.getCenterX(): x + width / 2.
[[nodiscard]] constexpr double centerX(const Rectangle2D& bounds) noexcept
{
    return bounds.x + (bounds.width / 2.0);
}

/// RectangularShape.getCenterY(): y + height / 2.
[[nodiscard]] constexpr double centerY(const Rectangle2D& bounds) noexcept
{
    return bounds.y + (bounds.height / 2.0);
}

}  // namespace QtRocket::Detail
