#pragma once

#include "QtRocket/figure/FigurePath.h"
#include "QtRocket/util/Geometry2D.h"

namespace QtRocket
{

class FigureShape;

/// A 2D affine transformation of the figure's plane: the part of java.awt.geom.AffineTransform
/// that OpenRocket's shape providers use (RailButtonShapes rotates its back-view screw with
/// getRotateInstance(angle, x, y).createTransformedShape(arc)). Like Java's class it keeps a
/// state of which matrix terms apply and transforms points with the formula of that state, so
/// that the results match Java's to the last bit, signed zeros included.
///
/// The matrix is [scaleX shearX translateX; shearY scaleY translateY] (Java's m00 m01 m02 / m10
/// m11 m12): x' = scaleX x + shearX y + translateX, y' = shearY x + scaleY y + translateY.
class AffineTransform2D
{
public:
    /// The identity.
    constexpr AffineTransform2D() noexcept = default;

    /// A rotation by @p theta radians about the origin (AffineTransform.getRotateInstance(theta)):
    /// positive angles turn +x towards +y. When the sine is exactly 1 or -1 the cosine is taken
    /// as 0, and when the cosine is exactly 1 or -1 the sine as 0, as Java does.
    [[nodiscard]] static AffineTransform2D rotation(double theta) noexcept;

    /// A rotation by @p theta radians about (anchorX, anchorY)
    /// (AffineTransform.getRotateInstance(theta, anchorx, anchory)).
    [[nodiscard]] static AffineTransform2D rotation(double theta, double anchorX,
                                                    double anchorY) noexcept;

    [[nodiscard]] constexpr double scaleX() const noexcept { return m_scaleX; }
    [[nodiscard]] constexpr double shearY() const noexcept { return m_shearY; }
    [[nodiscard]] constexpr double shearX() const noexcept { return m_shearX; }
    [[nodiscard]] constexpr double scaleY() const noexcept { return m_scaleY; }
    [[nodiscard]] constexpr double translateX() const noexcept { return m_translateX; }
    [[nodiscard]] constexpr double translateY() const noexcept { return m_translateY; }

    /// True when the transformation changes nothing (Java's TYPE_IDENTITY state).
    [[nodiscard]] constexpr bool isIdentity() const noexcept { return m_state == 0; }

    /// The image of @p p (AffineTransform.transform(Point2D, Point2D)).
    [[nodiscard]] Point2D transform(Point2D p) const noexcept;

    /// The image of @p shape as a new double-precision path
    /// (AffineTransform.createTransformedShape, that is new Path2D.Double(shape, this)): a
    /// FigurePath's segments with every point transformed, keeping its winding rule; any other
    /// shape's outline transformed point by point, as its PathIterator applies a transformation,
    /// with the NON_ZERO rule.
    [[nodiscard]] FigurePath createTransformedShape(const FigureShape& shape) const;

    /// Equal matrices and states.
    [[nodiscard]] constexpr bool operator==(const AffineTransform2D&) const noexcept = default;

private:
    // Java's state bits: which terms of the matrix take part in a transformation.
    static constexpr unsigned kApplyTranslate = 1U;
    static constexpr unsigned kApplyScale     = 2U;
    static constexpr unsigned kApplyShear     = 4U;

    double   m_scaleX{1.0};
    double   m_shearY{0.0};
    double   m_shearX{0.0};
    double   m_scaleY{1.0};
    double   m_translateX{0.0};
    double   m_translateY{0.0};
    unsigned m_state{0U};
};

}  // namespace QtRocket
