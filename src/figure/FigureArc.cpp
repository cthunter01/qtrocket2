#include "QtRocket/figure/FigureArc.h"

#include <cmath>
#include <numbers>
#include <vector>

#include "QtRocket/figure/FigureEllipse.h"
#include "QtRocket/figure/PathSegment.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

namespace
{

/// ArcIterator.btan: the control value of a cubic approximating an arc of @p increment radians.
[[nodiscard]] double btan(double increment) noexcept
{
    const double half = increment / 2.0;
    return 4.0 / 3.0 * std::sin(half) / (1.0 + std::cos(half));
}

/// Arc2D.normalizeDegrees: @p angle in (-180, 180].
[[nodiscard]] double normalizeDegrees(double angle) noexcept
{
    if (angle > 180.0)
    {
        if (angle <= (180.0 + 360.0))
        {
            return angle - 360.0;
        }
        angle = std::remainder(angle, 360.0);
        // IEEEremainder can return -180 here for some input values...
        return angle == -180.0 ? 180.0 : angle;
    }
    if (angle <= -180.0)
    {
        if (angle > (-180.0 - 360.0))
        {
            return angle + 360.0;
        }
        angle = std::remainder(angle, 360.0);
        return angle == -180.0 ? 180.0 : angle;
    }
    return angle;
}

/// The parameters ArcIterator's constructor derives from an arc.
struct ArcSteps
{
    double centreX;
    double centreY;
    double halfWidth;
    double halfHeight;
    double startRadians;
    double increment;
    double controlValue;
    int    arcSegments;
    int    lineSegments;
};

[[nodiscard]] ArcSteps arcSteps(const FigureArc& arc) noexcept
{
    ArcSteps steps{};
    steps.halfWidth    = arc.width() / 2;
    steps.halfHeight   = arc.height() / 2;
    steps.centreX      = arc.x() + steps.halfWidth;
    steps.centreY      = arc.y() + steps.halfHeight;
    steps.startRadians = -MathUtil::javaToRadians(arc.angleStart());
    const double ext   = -arc.angleExtent();
    if (ext >= 360.0 || ext <= -360)
    {
        steps.arcSegments  = 4;
        steps.increment    = std::numbers::pi / 2;
        steps.controlValue = FigureEllipse::kControlValue;
        if (ext < 0)
        {
            steps.increment    = -steps.increment;
            steps.controlValue = -steps.controlValue;
        }
    }
    else
    {
        steps.arcSegments  = MathUtil::javaIntCast(std::ceil(std::abs(ext) / 90.0));
        steps.increment    = MathUtil::javaToRadians(ext / steps.arcSegments);
        steps.controlValue = btan(steps.increment);
        if (steps.controlValue == 0)
        {
            steps.arcSegments = 0;
        }
    }
    switch (arc.type())
    {
        case FigureArc::Type::OPEN:
            steps.lineSegments = 0;
            break;
        case FigureArc::Type::CHORD:
            steps.lineSegments = 1;
            break;
        case FigureArc::Type::PIE:
            steps.lineSegments = 2;
            break;
    }
    if (steps.halfWidth < 0 || steps.halfHeight < 0)
    {
        steps.arcSegments  = -1;
        steps.lineSegments = -1;
    }
    return steps;
}

/// ArcIterator.currentSegment(double[]) at @p index.
[[nodiscard]] PathSegment arcSegment(const ArcSteps& s, int index) noexcept
{
    double angle = s.startRadians;
    if (index == 0)
    {
        return PathSegment::moveTo({s.centreX + (std::cos(angle) * s.halfWidth),
                                    s.centreY + (std::sin(angle) * s.halfHeight)});
    }
    if (index > s.arcSegments)
    {
        if (index == s.arcSegments + s.lineSegments)
        {
            return PathSegment::close();
        }
        return PathSegment::lineTo({s.centreX, s.centreY});
    }
    angle += s.increment * (index - 1);
    double        relX = std::cos(angle);
    double        relY = std::sin(angle);
    const double  cv   = s.controlValue;
    const Point2D control1{s.centreX + ((relX - (cv * relY)) * s.halfWidth),
                           s.centreY + ((relY + (cv * relX)) * s.halfHeight)};
    angle += s.increment;
    relX = std::cos(angle);
    relY = std::sin(angle);
    const Point2D control2{s.centreX + ((relX + (cv * relY)) * s.halfWidth),
                           s.centreY + ((relY - (cv * relX)) * s.halfHeight)};
    const Point2D end{s.centreX + (relX * s.halfWidth), s.centreY + (relY * s.halfHeight)};
    return PathSegment::cubicTo(control1, control2, end);
}

}  // namespace

bool FigureArc::isEmpty() const noexcept
{
    return m_width <= 0.0 || m_height <= 0.0;
}

Rectangle2D FigureArc::bounds() const noexcept
{
    if (isEmpty())
    {
        return Rectangle2D{.x = m_x, .y = m_y, .width = m_width, .height = m_height};
    }
    double x1 = 1.0;
    double y1 = 1.0;
    double x2 = -1.0;
    double y2 = -1.0;
    if (m_type == Type::PIE)
    {
        x1 = 0.0;
        y1 = 0.0;
        x2 = 0.0;
        y2 = 0.0;
    }
    double angle = 0.0;
    for (int i = 0; i < 6; ++i)
    {
        if (i < 4)
        {
            // 0-3 are the four quadrants
            angle += 90.0;
            if (!containsAngle(angle))
            {
                continue;
            }
        }
        else if (i == 4)
        {
            angle = m_angleStart;
        }
        else
        {
            angle += m_angleExtent;
        }
        const double rads = MathUtil::javaToRadians(-angle);
        const double xe   = std::cos(rads);
        const double ye   = std::sin(rads);
        x1                = MathUtil::javaMin(x1, xe);
        y1                = MathUtil::javaMin(y1, ye);
        x2                = MathUtil::javaMax(x2, xe);
        y2                = MathUtil::javaMax(y2, ye);
    }
    x2 = (x2 - x1) * 0.5 * m_width;
    y2 = (y2 - y1) * 0.5 * m_height;
    x1 = m_x + (((x1 * 0.5) + 0.5) * m_width);
    y1 = m_y + (((y1 * 0.5) + 0.5) * m_height);
    return Rectangle2D{.x = x1, .y = y1, .width = x2, .height = y2};
}

bool FigureArc::containsAngle(double angle) const noexcept
{
    double     angExt    = m_angleExtent;
    const bool backwards = angExt < 0.0;
    if (backwards)
    {
        angExt = -angExt;
    }
    if (angExt >= 360.0)
    {
        return true;
    }
    angle = normalizeDegrees(angle) - normalizeDegrees(m_angleStart);
    if (backwards)
    {
        angle = -angle;
    }
    if (angle < 0.0)
    {
        angle += 360.0;
    }
    return angle >= 0.0 && angle < angExt;
}

bool FigureArc::contains(Point2D p) const noexcept
{
    // Normalised to an ellipse of radius 0.5 centred at the origin.
    if (m_width <= 0.0)
    {
        return false;
    }
    const double normX = ((p.x - m_x) / m_width) - 0.5;
    if (m_height <= 0.0)
    {
        return false;
    }
    const double normY  = ((p.y - m_y) / m_height) - 0.5;
    const double distSq = (normX * normX) + (normY * normY);
    if (distSq >= 0.25)
    {
        return false;
    }
    const double angExt = std::abs(m_angleExtent);
    if (angExt >= 360.0)
    {
        return true;
    }
    const bool inArc = containsAngle(-MathUtil::javaToDegrees(std::atan2(normY, normX)));
    if (m_type == Type::PIE)
    {
        return inArc;
    }
    // CHORD and OPEN behave the same way
    if (inArc)
    {
        if (angExt >= 180.0)
        {
            return true;
        }
        // point must be outside the "pie triangle"
    }
    else if (angExt <= 180.0)
    {
        return false;
    }
    // Otherwise the point must be inside the "pie triangle": on the same side of the chord
    // between the ends of the arc as the centre.
    double       angle = MathUtil::javaToRadians(-m_angleStart);
    const double x1    = std::cos(angle);
    const double y1    = std::sin(angle);
    angle += MathUtil::javaToRadians(-m_angleExtent);
    const double  x2 = std::cos(angle);
    const double  y2 = std::sin(angle);
    const Point2D start{x1, y1};
    const Point2D end{x2, y2};
    const bool    inside =
        (relativeCcw(start, end, {2 * normX, 2 * normY}) * relativeCcw(start, end, {0, 0})) >= 0;
    return inArc ? !inside : inside;
}

std::vector<PathSegment> FigureArc::segments() const
{
    const ArcSteps           steps = arcSteps(*this);
    std::vector<PathSegment> result;
    for (int index = 0; index <= steps.arcSegments + steps.lineSegments; ++index)
    {
        result.push_back(arcSegment(steps, index));
    }
    return result;
}

}  // namespace QtRocket
