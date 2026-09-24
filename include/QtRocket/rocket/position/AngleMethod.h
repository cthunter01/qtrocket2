#pragma once

#include <array>
#include <optional>
#include <string_view>

namespace QtRocket
{

class RocketComponent;

/// How a component's angular position about its parent's axis is given (OpenRocket's
/// position.AngleMethod). Angles are in radians.
enum class AngleMethod
{
    RELATIVE,   ///< the parent's angle offset plus this component's offset
    FIXED,      ///< always zero, whatever the parent
    MIRROR_XY,  ///< mirrored about the rocket's x-y plane (meant for two-instance components)
};

/// Every method, in declaration order (AngleMethod.values()).
inline constexpr std::array<AngleMethod, 3> kAllAngleMethods{
    AngleMethod::RELATIVE, AngleMethod::FIXED, AngleMethod::MIRROR_XY};

/// The methods offered to the user (AngleMethod.choices()): RELATIVE only.
inline constexpr std::array<AngleMethod, 1> kAngleMethodChoices{AngleMethod::RELATIVE};

/// Whether an offset near zero should snap to zero (DistanceMethod.clampToZero()): false for
/// MIRROR_XY, true otherwise.
[[nodiscard]] constexpr bool clampToZero(AngleMethod method) noexcept
{
    return method != AngleMethod::MIRROR_XY;
}

/// The angle of @p thisComponent for @p requestedOffset (radians) about @p parentComponent
/// (AngleMethod.getAngle()):
/// - RELATIVE: parent.getAngleOffset() + requestedOffset;
/// - FIXED: 0;
/// - MIRROR_XY: a = reduce2Pi(parent.getAngleOffset() + requestedOffset), then pi - a when
///   a < pi (-(a - pi) in Java), else a.
/// @throws BugError when the method needs the parent and @p parentComponent is null (Java: a
///         NullPointerException).
[[nodiscard]] double getAngle(AngleMethod method, const RocketComponent* parentComponent,
                              const RocketComponent* thisComponent, double requestedOffset);

/// The constant's name, e.g. "MIRROR_XY" (Java: name()).
[[nodiscard]] std::string_view angleMethodName(AngleMethod method) noexcept;

/// The .ork spelling, the lower-cased name the savers write in method="..." ("relative", "fixed",
/// "mirror_xy"). Note that DocumentConfig.findEnum(), and so angleMethodFromOrkName(), does not
/// match "mirror_xy" (it compares against "mirrorxy"): OpenRocket then loads RELATIVE.
[[nodiscard]] std::string_view orkName(AngleMethod method) noexcept;

/// The method @p text names, matched as DocumentConfig.findEnum() does; nullopt for anything
/// else (the loader then falls back to RELATIVE).
[[nodiscard]] std::optional<AngleMethod> angleMethodFromOrkName(std::string_view text);

/// The translation key of the description, e.g. "RocketComponent.Position.Method.Angle.FIXED".
[[nodiscard]] std::string_view displayKey(AngleMethod method) noexcept;

/// The English description (Java: toString()), e.g. "Relative to the parent component".
[[nodiscard]] std::string_view displayName(AngleMethod method) noexcept;

}  // namespace QtRocket
