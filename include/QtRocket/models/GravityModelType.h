#pragma once

#include <array>
#include <optional>
#include <string_view>

namespace QtRocket
{

/// The gravity models a simulation can use (OpenRocket's models/gravity/GravityModelType).
///
/// Java's enum carries a string value ("WGS", "Constant") that toStringValue() and toString()
/// return and fromString() parses; here those are free functions, as for the other ported enums.
/// The spellings OpenRocket stores:
/// - the preference store keeps Enum.name() ("WGS", "CONSTANT"; gravityModelTypeName(), what
///   Preferences::getGravityModelName() holds),
/// - a .ork file writes <gravity model="wgs"/> or <gravity model="constant"> (orkName()).
enum class GravityModelType
{
    /// The WGS84 ellipsoid (WgsGravityModel).
    WGS,
    /// One value everywhere (ConstantGravityModel).
    CONSTANT,
};

/// Every type, in declaration order (GravityModelType.values()).
inline constexpr std::array<GravityModelType, 2> kAllGravityModelTypes{GravityModelType::WGS,
                                                                       GravityModelType::CONSTANT};

/// Java's toStringValue() and toString(): "WGS" or "Constant", which the simulation options
/// panel shows in its gravity model combo box.
[[nodiscard]] std::string_view toStringValue(GravityModelType type) noexcept;

/// Java's Enum.name(): "WGS" or "CONSTANT", the preference spelling.
[[nodiscard]] std::string_view gravityModelTypeName(GravityModelType type) noexcept;

/// The model attribute of a .ork file's <gravity> element: "wgs" or "constant" (the literals of
/// OpenRocketSaver and GravityHandler).
[[nodiscard]] std::string_view orkName(GravityModelType type) noexcept;

/// The translation key of the type's tooltip in the simulation options panel:
/// "simedtdlg.GravityModel.WGS84.ttip" or "simedtdlg.GravityModel.Constant.ttip".
[[nodiscard]] std::string_view tooltipKey(GravityModelType type) noexcept;

/// Java's fromString(): the type whose string value equals @p text ignoring case as
/// String.equalsIgnoreCase does (Strings::javaEqualsIgnoreCase; no trimming), so "wgs",
/// "Constant" and the .ork spellings match. Anything else is nullopt, where Java throws
/// IllegalArgumentException ("No enum constant ... for string value: <text>"): the text comes
/// from a file, so an unknown value is the caller's to report.
[[nodiscard]] std::optional<GravityModelType> gravityModelTypeFromString(
    std::string_view text) noexcept;

}  // namespace QtRocket
