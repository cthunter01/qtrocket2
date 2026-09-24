#include "QtRocket/rocket/TransitionShape.h"

#include <cmath>
#include <numbers>
#include <optional>
#include <string_view>
#include <utility>

#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// CONICAL.getRadius()
[[nodiscard]] double conicalRadius(double x, double radius, double length) noexcept
{
    return radius * x / length;
}

/// OGIVE.getRadius()
[[nodiscard]] double ogiveRadius(double x, double radius, double length, double param) noexcept
{
    // Impossible to calculate ogive for length < radius, scale instead
    if (length < radius)
    {
        x      = x * radius / length;
        length = radius;
    }

    if (param < kTransitionMinFeature)
    {
        return conicalRadius(x, radius, length);
    }

    // Radius of circle is:
    const double bigR =
        MathUtil::safeSqrt((MathUtil::pow2(length) + MathUtil::pow2(radius)) *
                           (MathUtil::pow2((2 - param) * length) + MathUtil::pow2(param * radius)) /
                           (4 * MathUtil::pow2(param * radius)));
    const double bigL = length / param;
    const double y0   = MathUtil::safeSqrt((bigR * bigR) - (bigL * bigL));
    return MathUtil::safeSqrt((bigR * bigR) - ((bigL - x) * (bigL - x))) - y0;
}

/// ELLIPSOID.getRadius()
[[nodiscard]] double ellipsoidRadius(double x, double radius, double length) noexcept
{
    x = x * radius / length;
    return MathUtil::safeSqrt((2 * radius * x) - (x * x));  // radius/length * sphere
}

/// POWER.getRadius()
[[nodiscard]] double powerRadius(double x, double radius, double length, double param) noexcept
{
    if (param <= 0.00001)
    {
        if (x <= 0.00001)
        {
            return 0;
        }
        return radius;
    }
    return radius * std::pow(x / length, param);
}

/// PARABOLIC.getRadius()
[[nodiscard]] double parabolicRadius(double x, double radius, double length, double param) noexcept
{
    return radius * (((2 * x / length) - (param * MathUtil::pow2(x / length))) / (2 - param));
}

/// HAACK.getRadius()
[[nodiscard]] double haackRadius(double x, double radius, double length, double param) noexcept
{
    const double theta = std::acos(1 - (2 * x / length));
    if (MathUtil::equals(param, 0))
    {
        return radius * MathUtil::safeSqrt((theta - (std::sin(2 * theta) / 2)) / std::numbers::pi);
    }
    return radius * MathUtil::safeSqrt((theta - (std::sin(2 * theta) / 2) +
                                        (param * MathUtil::pow3(std::sin(theta)))) /
                                       std::numbers::pi);
}

}  // namespace

double getRadius(TransitionShape shape, double x, double radius, double length,
                 double param) noexcept
{
    switch (shape)
    {
        case TransitionShape::CONICAL:
            return conicalRadius(x, radius, length);
        case TransitionShape::OGIVE:
            return ogiveRadius(x, radius, length, param);
        case TransitionShape::ELLIPSOID:
            return ellipsoidRadius(x, radius, length);
        case TransitionShape::POWER:
            return powerRadius(x, radius, length, param);
        case TransitionShape::PARABOLIC:
            return parabolicRadius(x, radius, length, param);
        case TransitionShape::HAACK:
            return haackRadius(x, radius, length, param);
    }
    return conicalRadius(x, radius, length);
}

double calculateClipLength(TransitionShape shape, double r1, double r2, double length,
                           double param) noexcept
{
    double min = 0;
    double max = length;

    if (r1 >= r2)
    {
        std::swap(r1, r2);
    }

    if (r1 == 0)
    {
        return 0;
    }

    if (length <= 0)
    {
        return 0;
    }

    // Required:
    // getR(min,min+length,r2) - r1 < 0
    // getR(max,max+length,r2) - r1 > 0

    int n = 0;
    while (getRadius(shape, max, r2, max + length, param) - r1 < 0)
    {
        min = max;
        max *= 2;
        n++;
        if (n > 10)
        {
            break;
        }
    }

    while (true)
    {
        const double clipLength = (min + max) / 2;
        const double width      = max - min;
        // Deviation: a NaN width ends the search here; OpenRocket's (max - min) < CLIP_PRECISION
        // never holds for it, so it loops forever.
        if (width < kTransitionClipPrecision || std::isnan(width))
        {
            return clipLength;
        }
        const double val = getRadius(shape, clipLength, r2, clipLength + length, param);
        if (val - r1 > 0)
        {
            max = clipLength;
        }
        else
        {
            min = clipLength;
        }
    }
}

double getTransitionRadius(TransitionShape shape, double x, double foreRadius, double aftRadius,
                           double length, double param, bool clipped) noexcept
{
    if (x < 0)
    {
        return foreRadius;
    }
    if (x >= length)
    {
        return aftRadius;
    }

    double r1 = foreRadius;
    double r2 = aftRadius;

    if (r1 == r2)
    {
        return r1;
    }

    if (r1 > r2)
    {
        x = length - x;
        std::swap(r1, r2);
    }

    if (clipped && isClippable(shape))
    {
        const double clipLength = calculateClipLength(shape, r1, r2, length, param);
        return getRadius(shape, clipLength + x, r2, clipLength + length, param);
    }
    // Not clipped
    return r1 + getRadius(shape, x, r2 - r1, length, param);
}

std::string_view transitionShapeName(TransitionShape shape) noexcept
{
    switch (shape)
    {
        case TransitionShape::CONICAL:
            return "CONICAL";
        case TransitionShape::OGIVE:
            return "OGIVE";
        case TransitionShape::ELLIPSOID:
            return "ELLIPSOID";
        case TransitionShape::POWER:
            return "POWER";
        case TransitionShape::PARABOLIC:
            return "PARABOLIC";
        case TransitionShape::HAACK:
            return "HAACK";
    }
    return "CONICAL";
}

std::optional<TransitionShape> transitionShapeFromName(std::string_view name) noexcept
{
    for (const TransitionShape shape : kAllTransitionShapes)
    {
        if (transitionShapeName(shape) == name)
        {
            return shape;
        }
    }
    return std::nullopt;
}

std::string_view orkName(TransitionShape shape) noexcept
{
    switch (shape)
    {
        case TransitionShape::CONICAL:
            return "conical";
        case TransitionShape::OGIVE:
            return "ogive";
        case TransitionShape::ELLIPSOID:
            return "ellipsoid";
        case TransitionShape::POWER:
            return "power";
        case TransitionShape::PARABOLIC:
            return "parabolic";
        case TransitionShape::HAACK:
            return "haack";
    }
    return "conical";
}

std::optional<TransitionShape> transitionShapeFromOrkName(std::string_view text)
{
    for (const TransitionShape shape : kAllTransitionShapes)
    {
        if (Strings::orkEnumNameMatches(text, transitionShapeName(shape)))
        {
            return shape;
        }
    }
    return std::nullopt;
}

std::string_view displayKey(TransitionShape shape) noexcept
{
    switch (shape)
    {
        case TransitionShape::CONICAL:
            return "Shape.Conical";
        case TransitionShape::OGIVE:
            return "Shape.Ogive";
        case TransitionShape::ELLIPSOID:
            return "Shape.Ellipsoid";
        case TransitionShape::POWER:
            return "Shape.Powerseries";
        case TransitionShape::PARABOLIC:
            return "Shape.Parabolicseries";
        case TransitionShape::HAACK:
            return "Shape.Haackseries";
    }
    return "Shape.Conical";
}

std::string_view displayName(TransitionShape shape) noexcept
{
    switch (shape)
    {
        case TransitionShape::CONICAL:
            return "Conical";
        case TransitionShape::OGIVE:
            return "Ogive";
        case TransitionShape::ELLIPSOID:
            return "Ellipsoid";
        case TransitionShape::POWER:
            return "Power series";
        case TransitionShape::PARABOLIC:
            return "Parabolic series";
        case TransitionShape::HAACK:
            return "Haack series";
    }
    return "Conical";
}

std::optional<TransitionShape> transitionShapeFromDisplayName(std::string_view name) noexcept
{
    for (const TransitionShape shape : kAllTransitionShapes)
    {
        if (displayName(shape) == name)
        {
            return shape;
        }
    }
    return std::nullopt;
}

std::string_view noseConeDescriptionKey(TransitionShape shape) noexcept
{
    switch (shape)
    {
        case TransitionShape::CONICAL:
            return "Shape.Conical.desc1";
        case TransitionShape::OGIVE:
            return "Shape.Ogive.desc1";
        case TransitionShape::ELLIPSOID:
            return "Shape.Ellipsoid.desc1";
        case TransitionShape::POWER:
            return "Shape.Powerseries.desc1";
        case TransitionShape::PARABOLIC:
            return "Shape.Parabolicseries.desc1";
        case TransitionShape::HAACK:
            return "Shape.Haackseries.desc1";
    }
    return "Shape.Conical.desc1";
}

std::string_view noseConeDescription(TransitionShape shape) noexcept
{
    switch (shape)
    {
        case TransitionShape::CONICAL:
            return "A conical nose cone has a profile of a triangle.";
        case TransitionShape::OGIVE:
            return "An ogive nose cone has a profile that is a segment of a circle.  The shape "
                   "parameter value 1 produces a <b>tangent ogive</b>, which has a smooth "
                   "transition to the body tube, values less than 1 produce <b>secant "
                   "ogives</b>.";
        case TransitionShape::ELLIPSOID:
            return "An ellipsoidal nose cone has a profile of a half-ellipse with major axes of "
                   "lengths 2&times;<i>Length</i> and <i>Diameter</i>.";
        case TransitionShape::POWER:
            return "A power series nose cone has a profile of "
                   "<i>Radius</i>&nbsp;&times;&nbsp;(<i>x</i>&nbsp;/&nbsp;<i>Length</i>)"
                   "<sup><i>k</i></sup> where <i>k</i> is the shape parameter.  For <i>k</i>=0.5 "
                   "this is a <b>\xC2\xBD-power</b> or <b>parabolic</b> nose cone, for "
                   "<i>k</i>=0.75 a <b>\xC2\xBE-power</b>, and for <i>k</i>=1 a <b>conical</b> "
                   "nose cone.";
        case TransitionShape::PARABOLIC:
            return "A parabolic series nose cone has a profile of a parabola.  The shape "
                   "parameter defines the segment of the parabola to utilize.  The shape "
                   "parameter 1.0 produces a <b>full parabola</b> which is tangent to the body "
                   "tube, 0.75 produces a <b>3/4 parabola</b>, 0.5 procudes a <b>1/2 parabola</b> "
                   "and 0 produces a <b>conical</b> nose cone.";
        case TransitionShape::HAACK:
            return "The Haack series nose cones are designed to minimize drag.  The shape "
                   "parameter 0 produces an <b>LD-Haack</b> or <b>Von Karman</b> nose cone, which "
                   "minimizes drag for fixed length and diameter, while a value of 0.333 "
                   "produces an <b>LV-Haack</b> nose cone, which minimizes drag for fixed length "
                   "and volume.";
    }
    return "";
}

std::string_view transitionDescriptionKey(TransitionShape shape) noexcept
{
    switch (shape)
    {
        case TransitionShape::CONICAL:
            return "Shape.Conical.desc2";
        case TransitionShape::OGIVE:
            return "Shape.Ogive.desc2";
        case TransitionShape::ELLIPSOID:
            return "Shape.Ellipsoid.desc2";
        case TransitionShape::POWER:
            return "Shape.Powerseries.desc2";
        case TransitionShape::PARABOLIC:
            return "Shape.Parabolicseries.desc2";
        case TransitionShape::HAACK:
            return "Shape.Haackseries.desc2";
    }
    return "Shape.Conical.desc2";
}

std::string_view transitionDescription(TransitionShape shape) noexcept
{
    switch (shape)
    {
        case TransitionShape::CONICAL:
            return "A conical transition has straight sides.";
        case TransitionShape::OGIVE:
            // The tab is in OpenRocket's messages.properties.
            return "An ogive transition has a profile that is a segment of a circle.  \tThe shape "
                   "parameter value 1 produces a <b>tangent ogive</b>, which has a smooth "
                   "transition to the body tube at the aft end, values less than 1 produce "
                   "<b>secant ogives</b>.";
        case TransitionShape::ELLIPSOID:
            return "An ellipsoidal transition has a profile of a half-ellipse with major axes of "
                   "lengths 2&times;<i>Length</i> and <i>Diameter</i>.  If the transition is not "
                   "clipped, then the profile is extended at the center by the corresponding "
                   "radius.";
        case TransitionShape::POWER:
            return "A power series transition has a profile of "
                   "<i>Radius</i>&nbsp;&times;&nbsp;(<i>x</i>&nbsp;/&nbsp;<i>Length</i>)"
                   "<sup><i>k</i></sup> where <i>k</i> is the shape parameter.  For <i>k</i>=0.5 "
                   "the transition is <b>\xC2\xBD-power</b> or <b>parabolic</b>, for "
                   "<i>k</i>=0.75 a <b>\xC2\xBE-power</b>, and for <i>k</i>=1 <b>conical</b>.";
        case TransitionShape::PARABOLIC:
            return "A parabolic series transition has a profile of a parabola.  The shape "
                   "parameter defines the segment of the parabola to utilize.  The shape "
                   "parameter 1.0 produces a <b>full parabola</b> which is tangent to the body "
                   "tube at the aft end, 0.75 produces a <b>3/4 parabola</b>, 0.5 procudes a "
                   "<b>1/2 parabola</b> and 0 produces a <b>conical</b> transition.";
        case TransitionShape::HAACK:
            return "The Haack series <i>nose cones</i> are designed to minimize drag.  These "
                   "transition shapes are their equivalents, but do not necessarily produce "
                   "optimal drag for transitions.  The shape parameter 0 produces an "
                   "<b>LD-Haack</b> or <b>Von Karman</b> shape, while a value of 0.333 produces "
                   "an <b>LV-Haack</b> shape.";
    }
    return "";
}

}  // namespace QtRocket
