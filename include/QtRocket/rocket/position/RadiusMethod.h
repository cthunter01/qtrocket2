#pragma once

#include <array>
#include <optional>
#include <string_view>

namespace QtRocket
{

class RocketComponent;

/// How a component's radial distance from its parent's axis is given (OpenRocket's
/// position.RadiusMethod). The radius is the distance of the component's axis from the parent's;
/// the offset is what the user edits.
///
/// | method   | radius from offset                               | offset from radius       |
/// |----------|--------------------------------------------------|--------------------------|
/// | COAXIAL  | 0 (same axis as the parent)                      | 0                        |
/// | FREE     | offset (from the parent's axis)                  | radius                   |
/// | RELATIVE | offset + parent tube radius + own bounding radius| radius - both            |
/// | SURFACE  | parent tube radius + own bounding radius         | 0                        |
///
/// "Parent tube radius" is the parent's outer radius when the parent is a body tube (Java:
/// parentComponent instanceof BodyTube, answered here with ComponentKind::BODY_TUBE and the
/// Coaxial interface), else 0; "own bounding radius" is the component's getBoundingRadius() when
/// it is a RadiusPositionable, else 0. A null parent counts as "not a body tube".
enum class RadiusMethod
{
    COAXIAL,   ///< same axis as the target component
    FREE,      ///< from the center of the parent component
    RELATIVE,  ///< from the surface of the parent component
    SURFACE,   ///< on the surface of the parent component, without offset
};

/// Every method, in declaration order (RadiusMethod.values()).
inline constexpr std::array<RadiusMethod, 4> kAllRadiusMethods{
    RadiusMethod::COAXIAL, RadiusMethod::FREE, RadiusMethod::RELATIVE, RadiusMethod::SURFACE};

/// The methods offered to the user (RadiusMethod.choices()): FREE and RELATIVE.
inline constexpr std::array<RadiusMethod, 2> kRadiusMethodChoices{RadiusMethod::FREE,
                                                                  RadiusMethod::RELATIVE};

/// Whether an offset near zero should snap to zero (DistanceMethod.clampToZero()): false for
/// FREE and RELATIVE, true for COAXIAL and SURFACE.
[[nodiscard]] constexpr bool clampToZero(RadiusMethod method) noexcept
{
    return method != RadiusMethod::FREE && method != RadiusMethod::RELATIVE;
}

/// The radius (from the parent's axis) of @p thisComponent at @p requestedOffset in
/// @p parentComponent (RadiusMethod.getRadius()); see the table above.
[[nodiscard]] double getRadius(RadiusMethod method, const RocketComponent* parentComponent,
                               const RocketComponent* thisComponent, double requestedOffset);

/// The offset of this method that gives @p radius, the inverse of getRadius()
/// (RadiusMethod.getAsOffset()).
[[nodiscard]] double getAsOffset(RadiusMethod method, const RocketComponent* parentComponent,
                                 const RocketComponent* thisComponent, double radius);

/// The constant's name, e.g. "RELATIVE" (Java: name()).
[[nodiscard]] std::string_view radiusMethodName(RadiusMethod method) noexcept;

/// The .ork spelling, the lower-cased name the savers write in method="..." ("coaxial", "free",
/// "relative", "surface").
[[nodiscard]] std::string_view orkName(RadiusMethod method) noexcept;

/// The method @p text names, matched as DocumentConfig.findEnum() does; nullopt for anything
/// else (the loader then falls back to SURFACE).
[[nodiscard]] std::optional<RadiusMethod> radiusMethodFromOrkName(std::string_view text);

/// The translation key of the description, e.g. "RocketComponent.Position.Method.Radius.FREE".
[[nodiscard]] std::string_view displayKey(RadiusMethod method) noexcept;

/// The English description (Java: toString()), e.g. "Center of the parent component".
[[nodiscard]] std::string_view displayName(RadiusMethod method) noexcept;

}  // namespace QtRocket
