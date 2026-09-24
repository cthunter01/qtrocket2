#pragma once

#include <array>
#include <optional>
#include <span>
#include <string_view>

namespace QtRocket
{

/// How a component or plot series is drawn (OpenRocket's LineStyle).
enum class LineStyle
{
    SOLID,
    DASHED,
    DOTTED,
    DASHDOT,
};

/// Every style, in declaration order (LineStyle.values()).
inline constexpr std::array<LineStyle, 4> kAllLineStyles{LineStyle::SOLID, LineStyle::DASHED,
                                                         LineStyle::DOTTED, LineStyle::DASHDOT};

/// The dash pattern as alternating on/off lengths, as OpenRocket hands them to a stroke: solid is
/// {10, 0}, dashed {6, 4}, dotted {2, 3} and dash-dotted {8, 3, 2, 3}.
[[nodiscard]] std::span<const double> dashes(LineStyle style) noexcept;

/// The translation key of the style's display name, e.g. "LineStyle.Dash-dotted".
[[nodiscard]] std::string_view displayKey(LineStyle style) noexcept;

/// The .ork spelling: the lower-cased enum name ("solid", "dashed", "dotted", "dashdot").
[[nodiscard]] std::string_view toString(LineStyle style) noexcept;

/// The style whose name is @p text, trimmed and compared without regard to ASCII case, so the .ork
/// spelling "dashdot" and the preference spelling "DASHDOT" both match. Anything else is nullopt.
[[nodiscard]] std::optional<LineStyle> lineStyleFromString(std::string_view text) noexcept;

}  // namespace QtRocket
