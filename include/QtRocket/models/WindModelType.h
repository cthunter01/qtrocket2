#pragma once

#include <array>
#include <optional>
#include <string_view>

namespace QtRocket
{

/// The wind models a simulation can use (OpenRocket's models/wind/WindModelType).
///
/// Java's enum carries a string value ("Average", "MultiLevel") that toStringValue() returns and
/// fromString() parses; here those are free functions, as for the other ported enums. The
/// spellings, named as for LineStyle and WindModel::AltitudeReference:
/// - toString(): the model attribute of a .ork file's <wind model="average"> or
///   <wind model="multilevel">, the lower-cased value that OpenRocketSaver writes and
///   WindHandler reads. Deviation: Java's toString() is Enum's default, the constant name, here
///   windModelTypeName().
/// - toStringValue(): the string value, which a .ork file also stores as
///   <windmodeltype>Average</windmodeltype> or MultiLevel.
/// - windModelTypeName(): Enum.name() ("AVERAGE", "MULTI_LEVEL").
enum class WindModelType
{
    /// One PinkNoiseWindModel for every altitude.
    AVERAGE,
    /// A MultiLevelPinkNoiseWindModel.
    MULTI_LEVEL,
};

/// Every type, in declaration order (WindModelType.values()).
inline constexpr std::array<WindModelType, 2> kAllWindModelTypes{WindModelType::AVERAGE,
                                                                 WindModelType::MULTI_LEVEL};

/// Java's toStringValue(): "Average" or "MultiLevel".
[[nodiscard]] std::string_view toStringValue(WindModelType type) noexcept;

/// Java's Enum.name() (and toString()): "AVERAGE" or "MULTI_LEVEL".
[[nodiscard]] std::string_view windModelTypeName(WindModelType type) noexcept;

/// The .ork spelling, the model attribute of a <wind> element: "average" or "multilevel". Not
/// Java's toString() (see windModelTypeName()).
[[nodiscard]] std::string_view toString(WindModelType type) noexcept;

/// The translation key of the type's name in the GUI: "simedtdlg.radio.AverageWind" or
/// "simedtdlg.radio.MultiLevelWind", the keys of the simulation dialog's wind model radio
/// buttons, which is where OpenRocket shows the type by a name (the enum has no text of its
/// own). With displayName() the GUI shows the type by its translated name (EnumText).
[[nodiscard]] std::string_view displayKey(WindModelType type) noexcept;

/// The English text of displayKey(): "Average" or "Multi-level".
[[nodiscard]] std::string_view displayName(WindModelType type) noexcept;

/// The translation key of the type's description, the radio button's tooltip:
/// "simedtdlg.radio.AverageWind.ttip" or "simedtdlg.radio.MultiLevelWind.ttip".
[[nodiscard]] std::string_view descriptionKey(WindModelType type) noexcept;

/// The English text of descriptionKey(): "Model the wind as pink noise from the average wind
/// speed and standard deviation." or "Model the wind using speed and direction entries at
/// various altitude levels."
[[nodiscard]] std::string_view description(WindModelType type) noexcept;

/// Java's fromString(): the type whose string value equals @p text ignoring case as
/// String.equalsIgnoreCase does (Strings::javaEqualsIgnoreCase; no trimming), so "average" and
/// "MULTILEVEL" match. Anything else is nullopt, where Java throws IllegalArgumentException ("No
/// enum constant ... for string value: <text>"): the text comes from a .ork file, so an unknown
/// value is the caller's to report.
[[nodiscard]] std::optional<WindModelType> windModelTypeFromString(std::string_view text) noexcept;

}  // namespace QtRocket
