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
/// The spellings, named as for LineStyle and WindModel::AltitudeReference:
/// - toString(): the .ork spelling, <gravity model="wgs"/> or <gravity model="constant">.
///   Deviation: Java's toString() is the string value, here toStringValue().
/// - toStringValue(): the string value ("WGS", "Constant"), which the GUI shows.
/// - gravityModelTypeName(): Enum.name() ("WGS", "CONSTANT"), what the preference store keeps
///   (Preferences::getGravityModelName()).
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

/// The .ork spelling, the model attribute of a <gravity> element: "wgs" or "constant" (the
/// literals of OpenRocketSaver and GravityHandler). Not Java's toString() (see toStringValue()).
[[nodiscard]] std::string_view toString(GravityModelType type) noexcept;

/// The translation key of the type's tooltip in the simulation options panel:
/// "simedtdlg.GravityModel.WGS84.ttip" or "simedtdlg.GravityModel.Constant.ttip".
[[nodiscard]] std::string_view tooltipKey(GravityModelType type) noexcept;

/// The English text of tooltipKey(): "WGS84 ellipsoid gravity model (varies with latitude and
/// altitude)" or "Constant gravity model (same value everywhere)".
[[nodiscard]] std::string_view tooltip(GravityModelType type) noexcept;

/// The key under which the GUI translates displayName() (EnumText): "GravityModelType.WGS" or
/// "GravityModelType.CONSTANT". Not a key of OpenRocket's messages.properties: Java's combo boxes
/// show toString(), the string value, untranslated. QtRocket's key lets a translation give the
/// type a name of its own (in English the text is Java's).
[[nodiscard]] std::string_view displayKey(GravityModelType type) noexcept;

/// The name the GUI shows, the string value: "WGS" or "Constant" (toStringValue()).
[[nodiscard]] std::string_view displayName(GravityModelType type) noexcept;

/// Java's fromString(): the type whose string value equals @p text ignoring case as
/// String.equalsIgnoreCase does (Strings::javaEqualsIgnoreCase; no trimming), so "wgs",
/// "Constant" and the .ork spellings match. Anything else is nullopt, where Java throws
/// IllegalArgumentException ("No enum constant ... for string value: <text>"): the text comes
/// from a file, so an unknown value is the caller's to report.
[[nodiscard]] std::optional<GravityModelType> gravityModelTypeFromString(
    std::string_view text) noexcept;

}  // namespace QtRocket
