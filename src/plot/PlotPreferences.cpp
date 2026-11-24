#include "QtRocket/plot/PlotPreferences.h"

#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/PlotAppearance.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/LineStyle.h"

namespace QtRocket::PlotPreferences
{

namespace
{

constexpr std::string_view kColorSuffix     = ".color";
constexpr std::string_view kLineStyleSuffix = ".lineStyle";

/// LineStyle.valueOf(): the style whose constant name is @p text exactly.
[[nodiscard]] std::optional<LineStyle> lineStyleValueOf(std::string_view text) noexcept
{
    for (const LineStyle style : kAllLineStyles)
    {
        if (lineStyleName(style) == text)
        {
            return style;
        }
    }
    return std::nullopt;
}

}  // namespace

std::optional<PlotAppearance> getDefaultPlotAppearance(const Preferences&    preferences,
                                                       const FlightDataType& type)
{
    const std::string& key = type.getName();
    if (key.empty())
    {
        return std::nullopt;
    }

    const std::optional<std::string> colorText = preferences.getInNode(
        PreferenceKeys::kPlotDefaultAppearancesNode, key + std::string(kColorSuffix));
    const std::optional<std::string> lineStyleText = preferences.getInNode(
        PreferenceKeys::kPlotDefaultAppearancesNode, key + std::string(kLineStyleSuffix));
    // No entry stored yet for this type.
    if (!colorText.has_value() && !lineStyleText.has_value())
    {
        return std::nullopt;
    }

    const std::optional<Color> color =
        colorText.has_value() ? Preferences::parseColor(*colorText) : std::nullopt;
    // Unknown value in prefs: treat as absent.
    const std::optional<LineStyle> lineStyle =
        lineStyleText.has_value() ? lineStyleValueOf(*lineStyleText) : std::nullopt;
    const PlotAppearance result(color, lineStyle);

    if (result.isEmpty())
    {
        return std::nullopt;
    }
    return result;
}

void setDefaultPlotAppearance(Preferences& preferences, const FlightDataType& type,
                              const std::optional<PlotAppearance>& appearance)
{
    const std::string& key = type.getName();
    if (key.empty())
    {
        return;
    }
    const std::string colorKey     = key + std::string(kColorSuffix);
    const std::string lineStyleKey = key + std::string(kLineStyleSuffix);

    // Null or empty appearance clears any previously stored default.
    if (!appearance.has_value() || appearance->isEmpty())
    {
        preferences.putString(PreferenceKeys::kPlotDefaultAppearancesNode, colorKey, std::nullopt);
        preferences.putString(PreferenceKeys::kPlotDefaultAppearancesNode, lineStyleKey,
                              std::nullopt);
        return;
    }
    // Store color as "r,g,b", or remove the key if no color override is set.
    const std::optional<Color>&      color = appearance->getColor();
    const std::optional<std::string> colorText =
        color.has_value() ? std::optional<std::string>(Preferences::stringifyColor(*color))
                          : std::nullopt;
    preferences.putString(PreferenceKeys::kPlotDefaultAppearancesNode, colorKey, colorText);
    // Store line style by name, or remove the key if not set.
    const std::optional<LineStyle>& lineStyle = appearance->getLineStyle();
    preferences.putString(PreferenceKeys::kPlotDefaultAppearancesNode, lineStyleKey,
                          lineStyle.has_value()
                              ? std::optional<std::string_view>(lineStyleName(*lineStyle))
                              : std::nullopt);
}

bool isShowPoints(const Preferences& preferences)
{
    return preferences.getBoolean(PreferenceKeys::kPlotShowPoints, false);
}

void setShowPoints(Preferences& preferences, bool show)
{
    preferences.putBoolean(PreferenceKeys::kPlotShowPoints, show);
}

bool isShowEvents(const Preferences& preferences)
{
    return preferences.getBoolean(PreferenceKeys::kPlotShowEvents, true);
}

void setShowEvents(Preferences& preferences, bool show)
{
    preferences.putBoolean(PreferenceKeys::kPlotShowEvents, show);
}

bool isMarkerStyleIcon(const Preferences& preferences)
{
    return preferences.getBoolean(PreferenceKeys::kMarkerStyleIcon, false);
}

void setMarkerStyleIcon(Preferences& preferences, bool icon)
{
    preferences.putBoolean(PreferenceKeys::kMarkerStyleIcon, icon);
}

}  // namespace QtRocket::PlotPreferences
