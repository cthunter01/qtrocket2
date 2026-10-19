#include "QtRocket/figure/AffineTransform2D.h"

#include <cmath>
#include <cstddef>
#include <vector>

#include "QtRocket/figure/FigurePath.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/figure/WindingRule.h"
#include "QtRocket/util/Geometry2D.h"

namespace QtRocket
{

AffineTransform2D AffineTransform2D::rotation(double theta) noexcept
{
    // AffineTransform.setToRotation(theta)
    AffineTransform2D result;
    double            sine   = std::sin(theta);
    double            cosine = 0.0;
    if (sine == 1.0 || sine == -1.0)
    {
        cosine         = 0.0;
        result.m_state = kApplyShear;
    }
    else
    {
        cosine = std::cos(theta);
        if (cosine == -1.0)
        {
            sine           = 0.0;
            result.m_state = kApplyScale;
        }
        else if (cosine == 1.0)
        {
            sine           = 0.0;
            result.m_state = 0U;
        }
        else
        {
            result.m_state = kApplyShear | kApplyScale;
        }
    }
    result.m_scaleX     = cosine;
    result.m_shearY     = sine;
    result.m_shearX     = -sine;
    result.m_scaleY     = cosine;
    result.m_translateX = 0.0;
    result.m_translateY = 0.0;
    return result;
}

AffineTransform2D AffineTransform2D::rotation(double theta, double anchorX, double anchorY) noexcept
{
    // AffineTransform.setToRotation(theta, anchorx, anchory)
    AffineTransform2D result      = rotation(theta);
    const double      sine        = result.m_shearY;
    const double      oneMinusCos = 1.0 - result.m_scaleX;
    result.m_translateX           = (anchorX * oneMinusCos) + (anchorY * sine);
    result.m_translateY           = (anchorY * oneMinusCos) - (anchorX * sine);
    if (result.m_translateX != 0.0 || result.m_translateY != 0.0)
    {
        result.m_state |= kApplyTranslate;
    }
    return result;
}

Point2D AffineTransform2D::transform(Point2D p) const noexcept
{
    const double x = p.x;
    const double y = p.y;
    switch (m_state)
    {
        case kApplyShear | kApplyScale | kApplyTranslate:
            return {(x * m_scaleX) + (y * m_shearX) + m_translateX,
                    (x * m_shearY) + (y * m_scaleY) + m_translateY};
        case kApplyShear | kApplyScale:
            return {(x * m_scaleX) + (y * m_shearX), (x * m_shearY) + (y * m_scaleY)};
        case kApplyShear | kApplyTranslate:
            return {(y * m_shearX) + m_translateX, (x * m_shearY) + m_translateY};
        case kApplyShear:
            return {y * m_shearX, x * m_shearY};
        case kApplyScale | kApplyTranslate:
            return {(x * m_scaleX) + m_translateX, (y * m_scaleY) + m_translateY};
        case kApplyScale:
            return {x * m_scaleX, y * m_scaleY};
        case kApplyTranslate:
            return {x + m_translateX, y + m_translateY};
        default:
            return p;
    }
}

FigurePath AffineTransform2D::createTransformedShape(const FigureShape& shape) const
{
    // new Path2D.Double(shape, this)
    const auto transformed = [this](PathSegment segment) {
        for (int i = 0; i < segment.pointCount(); ++i)
        {
            Point2D& point = segment.points.at(static_cast<std::size_t>(i));
            point          = transform(point);
        }
        return segment;
    };
    if (const auto* path = shape.getIf<FigurePath>())
    {
        // A Path2D is copied segment by segment (cloneCoordsDouble), its rule kept.
        FigurePath               result{FigurePath::Precision::DOUBLE, path->windingRule()};
        std::vector<PathSegment> segments;
        segments.reserve(path->segments().size());
        for (const PathSegment& segment : path->segments())
        {
            segments.push_back(transformed(segment));
        }
        result.append(segments, false);
        return result;
    }
    // Any other shape: its iterator with the transformation, appended without connecting.
    std::vector<PathSegment> segments = shape.segments();
    for (PathSegment& segment : segments)
    {
        segment = transformed(segment);
    }
    FigurePath result{FigurePath::Precision::DOUBLE, WindingRule::NON_ZERO};
    result.append(segments, false);
    return result;
}

}  // namespace QtRocket
