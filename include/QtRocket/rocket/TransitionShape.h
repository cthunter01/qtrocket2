#pragma once

#include <array>
#include <optional>
#include <string_view>

namespace QtRocket
{

/// The profile of a transition or nose cone (OpenRocket's Transition.Shape). Java's constants are
/// objects with overridden methods; here the enum carries the identity and the free functions
/// below the behaviour: getRadius() (the profile), the shape parameter's range
/// (usesParameter(), minParameter(), maxParameter(), defaultParameter()), clipping
/// (isClippable(), calculateClipLength(), getTransitionRadius()) and the names (the constant's
/// name, the .ork spelling, the translation keys and OpenRocket's English texts).
///
/// The declaration order is Java's: static_cast<int>(shape) is Shape.ordinal(), which the preset
/// digest writes (ComponentPreset.computeDigest).
///
/// Java's getRadius() implementations open with assert statements (x within [0, length], a
/// non-negative radius, the parameter within its range). Java runs without -ea, so they never
/// fire in OpenRocket, and a rounding error can put x a hair beyond length; they are not ported.
enum class TransitionShape
{
    CONICAL,    ///< straight sides
    OGIVE,      ///< a circular arc; the parameter is the portion of a tangent ogive (1 = tangent)
    ELLIPSOID,  ///< a half ellipse (clippable)
    POWER,      ///< radius * (x / length)^k with k the parameter (clippable)
    PARABOLIC,  ///< a parabolic series; the parameter selects the segment of the parabola
    HAACK,      ///< a Haack series; parameter 0 is LD-Haack (Von Karman), 1/3 LV-Haack (clippable)
};

/// Every shape, in declaration order (Shape.values()).
inline constexpr std::array<TransitionShape, 6> kAllTransitionShapes{
    TransitionShape::CONICAL, TransitionShape::OGIVE,     TransitionShape::ELLIPSOID,
    TransitionShape::POWER,   TransitionShape::PARABOLIC, TransitionShape::HAACK};

/// The smallest feature size of a transition, 1 mm (Transition.MINFEATURE): an ogive parameter
/// below it is a cone, and Transition ignores shoulders no longer than it.
inline constexpr double kTransitionMinFeature = 0.001;

/// The precision to which calculateClipLength() solves the clip length (Transition.CLIP_PRECISION).
inline constexpr double kTransitionClipPrecision = 0.0001;

/// Whether the shape differs in clipped mode (Shape.isClippable()): true for ELLIPSOID, POWER and
/// HAACK. A new transition of such a shape is clipped by default.
[[nodiscard]] constexpr bool isClippable(TransitionShape shape) noexcept
{
    return shape == TransitionShape::ELLIPSOID || shape == TransitionShape::POWER ||
           shape == TransitionShape::HAACK;
}

/// Whether the shape uses the shape parameter (Shape.usesParameter()): OGIVE, POWER, PARABOLIC
/// and HAACK.
[[nodiscard]] constexpr bool usesParameter(TransitionShape shape) noexcept
{
    return shape == TransitionShape::OGIVE || shape == TransitionShape::POWER ||
           shape == TransitionShape::PARABOLIC || shape == TransitionShape::HAACK;
}

/// The smallest shape parameter (Shape.minParameter()): 0 for every shape.
[[nodiscard]] constexpr double minParameter(TransitionShape /*shape*/) noexcept
{
    return 0.0;
}

/// The largest shape parameter (Shape.maxParameter()): 1/3 for HAACK, 1 for the others.
[[nodiscard]] constexpr double maxParameter(TransitionShape shape) noexcept
{
    return shape == TransitionShape::HAACK ? 1.0 / 3.0 : 1.0;
}

/// The shape parameter of a new transition of this shape (Shape.defaultParameter()): 1 for OGIVE
/// (a tangent ogive) and PARABOLIC (a full parabola), 0.5 for POWER, 0 for the others.
[[nodiscard]] constexpr double defaultParameter(TransitionShape shape) noexcept
{
    switch (shape)
    {
        case TransitionShape::OGIVE:
        case TransitionShape::PARABOLIC:
            return 1.0;
        case TransitionShape::POWER:
            return 0.5;
        case TransitionShape::CONICAL:
        case TransitionShape::ELLIPSOID:
        case TransitionShape::HAACK:
            return 0.0;
    }
    return 0.0;
}

/// The basic radius of a transition of this shape (Shape.getRadius()) at @p x from its tip, for a
/// fore radius of zero, an aft radius @p radius (>= 0), a length @p length (>= 0) and a valid
/// shape parameter @p param. Boattails are made by reversing the component. The formulas are
/// OpenRocket's, operation for operation:
/// - CONICAL: radius * x / length.
/// - OGIVE: a length below the radius is scaled up to the radius (x with it); a parameter below
///   kTransitionMinFeature gives the cone; otherwise the arc of the circle of radius
///   R = safeSqrt((length^2 + radius^2) * (((2 - param) * length)^2 + (param * radius)^2) /
///   (4 * (param * radius)^2)) centred L = length / param ahead of the aft end, less its offset
///   y0 = safeSqrt(R^2 - L^2).
/// - ELLIPSOID: with x scaled by radius / length, safeSqrt(2 * radius * x - x^2).
/// - POWER: radius * (x / length)^param; a parameter up to 1e-5 gives 0 for x up to 1e-5 and the
///   radius beyond.
/// - PARABOLIC: radius * ((2 * x / length - param * (x / length)^2) / (2 - param)).
/// - HAACK: with theta = acos(1 - 2 * x / length),
///   radius * safeSqrt((theta - sin(2 * theta) / 2 + param * sin(theta)^3) / pi), the parameter
///   term dropped when the parameter equals 0 (MathUtil::equals).
[[nodiscard]] double getRadius(TransitionShape shape, double x, double radius, double length,
                               double param) noexcept;

/// The clip length of a clipped transition (Transition.calculateClip()): the length by which the
/// shape is extended ahead of the fore end so that the extended profile, of length
/// clipLength + @p length and aft radius r2, passes through r1 at the fore end. The radii are
/// swapped when @p r1 exceeds @p r2. 0 when the smaller radius is 0 or the length is not
/// positive; otherwise found by doubling the upper bound from @p length while the profile stays
/// below r1 (at most 11 doublings) and then bisecting until the bracket is narrower than
/// kTransitionClipPrecision, returning its midpoint.
///
/// Deviation: when the bracket width becomes NaN (a NaN or infinite length), the bisection stops
/// and returns the NaN or infinite midpoint; OpenRocket loops forever there.
[[nodiscard]] double calculateClipLength(TransitionShape shape, double r1, double r2, double length,
                                         double param) noexcept;

/// The radius of a transition at @p x from its fore end (Transition.getRadius(x)): the fore radius
/// ahead of it (x < 0), the aft radius from @p length on, the fore radius all along when both
/// radii are equal. Otherwise, with r1 the smaller radius and r2 the larger one (x measured from
/// the aft end when the transition narrows): for a clipped transition (@p clipped and a clippable
/// shape, Transition.isClipped())
/// getRadius(shape, clipLength + x, r2, clipLength + length, param) with clipLength from
/// calculateClipLength(); for an unclipped one r1 + getRadius(shape, x, r2 - r1, length, param).
///
/// Transition caches the clip length; this function solves it again on every call.
[[nodiscard]] double getTransitionRadius(TransitionShape shape, double x, double foreRadius,
                                         double aftRadius, double length, double param,
                                         bool clipped) noexcept;

/// The constant's name, e.g. "HAACK" (Java: name()).
[[nodiscard]] std::string_view transitionShapeName(TransitionShape shape) noexcept;

/// The shape named exactly @p name (Shape.valueOf()), or nullopt.
[[nodiscard]] std::optional<TransitionShape> transitionShapeFromName(
    std::string_view name) noexcept;

/// The .ork spelling the transition saver writes in <shape>: the lower-cased name ("conical",
/// "ogive", "ellipsoid", "power", "parabolic", "haack").
[[nodiscard]] std::string_view orkName(TransitionShape shape) noexcept;

/// The shape @p text names, matched as DocumentConfig.findEnum() does (trimmed, against the
/// lower-cased name); nullopt for anything else.
[[nodiscard]] std::optional<TransitionShape> transitionShapeFromOrkName(std::string_view text);

/// The translation key of the shape's name, e.g. "Shape.Powerseries".
[[nodiscard]] std::string_view displayKey(TransitionShape shape) noexcept;

/// The English name (Shape.getName() and toString()): "Conical", "Ogive", "Ellipsoid",
/// "Power series", "Parabolic series", "Haack series".
[[nodiscard]] std::string_view displayName(TransitionShape shape) noexcept;

/// The shape whose English name is exactly @p name (Shape.toShape(localizedName)), or nullopt.
[[nodiscard]] std::optional<TransitionShape> transitionShapeFromDisplayName(
    std::string_view name) noexcept;

/// The translation key of the nose cone description, e.g. "Shape.Conical.desc1".
[[nodiscard]] std::string_view noseConeDescriptionKey(TransitionShape shape) noexcept;

/// The English nose cone description, HTML markup included (Shape.getNoseConeDescription()).
[[nodiscard]] std::string_view noseConeDescription(TransitionShape shape) noexcept;

/// The translation key of the transition description, e.g. "Shape.Conical.desc2".
[[nodiscard]] std::string_view transitionDescriptionKey(TransitionShape shape) noexcept;

/// The English transition description, HTML markup included
/// (Shape.getTransitionDescription()).
[[nodiscard]] std::string_view transitionDescription(TransitionShape shape) noexcept;

}  // namespace QtRocket
