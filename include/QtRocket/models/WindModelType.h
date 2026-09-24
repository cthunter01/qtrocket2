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
/// spellings OpenRocket stores in a .ork file:
/// - <windmodeltype>Average</windmodeltype> or MultiLevel: the string value (toStringValue()),
/// - <wind model="average"> or <wind model="multilevel">: orkName(), the lower-cased value that
///   OpenRocketSaver writes and WindHandler reads.
/// Java's toString() is Enum's default, the constant name: windModelTypeName().
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

/// The model attribute of a .ork file's <wind> element: "average" or "multilevel".
[[nodiscard]] std::string_view orkName(WindModelType type) noexcept;

/// Java's fromString(): the type whose string value equals @p text ignoring case as
/// String.equalsIgnoreCase does (Strings::javaEqualsIgnoreCase; no trimming), so "average" and
/// "MULTILEVEL" match. Anything else is nullopt, where Java throws IllegalArgumentException ("No
/// enum constant ... for string value: <text>"): the text comes from a .ork file, so an unknown
/// value is the caller's to report.
[[nodiscard]] std::optional<WindModelType> windModelTypeFromString(std::string_view text) noexcept;

}  // namespace QtRocket
