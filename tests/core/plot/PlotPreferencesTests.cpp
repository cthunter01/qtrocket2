#include "QtRocket/plot/PlotPreferences.h"

#include <map>
#include <optional>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/PlotAppearance.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/LineStyle.h"

// Pinned to what OpenRocket's SwingPreferences stores and reads (the "SET" and "STORED" lines of
// tools/openrocket-probes/plot-model's expected.txt).

namespace
{

using QtRocket::Color;
using QtRocket::FlightDataType;
using QtRocket::InMemoryPreferences;
using QtRocket::LineStyle;
using QtRocket::PlotAppearance;
using QtRocket::Preferences;
using QtRocket::PlotPreferences::getDefaultPlotAppearance;
using QtRocket::PlotPreferences::setDefaultPlotAppearance;
using QtRocket::PreferenceKeys::kPlotDefaultAppearancesNode;
using Id = QtRocket::FlightDataTypeId;

[[nodiscard]] const FlightDataType& altitude()
{
    return FlightDataType::builtin(Id::TYPE_ALTITUDE);
}

/// The keys and values of the node of the default appearances.
[[nodiscard]] std::map<std::string, std::string> stored(const Preferences& prefs)
{
    std::map<std::string, std::string> values;
    const Preferences*                 node = prefs.findNode(kPlotDefaultAppearancesNode);
    if (node != nullptr)
    {
        for (const std::string& key : node->keys())
        {
            values.emplace(key, node->get(key).value_or("?"));
        }
    }
    return values;
}

using Stored = std::map<std::string, std::string>;

TEST(PlotPreferences, NothingStoredIsNoDefault)
{
    const InMemoryPreferences prefs;
    EXPECT_EQ(getDefaultPlotAppearance(prefs, altitude()), std::nullopt);
    EXPECT_EQ(prefs.findNode(kPlotDefaultAppearancesNode), nullptr);
}

TEST(PlotPreferences, AnAppearanceIsStoredByTheTypesName)
{
    InMemoryPreferences prefs;
    setDefaultPlotAppearance(prefs, altitude(), PlotAppearance(Color(1, 2, 3), LineStyle::DASHDOT));
    EXPECT_EQ(stored(prefs),
              (Stored{{"Altitude.color", "1,2,3"}, {"Altitude.lineStyle", "DASHDOT"}}));
    EXPECT_EQ(getDefaultPlotAppearance(prefs, altitude()),
              PlotAppearance(Color(1, 2, 3), LineStyle::DASHDOT));
}

TEST(PlotPreferences, TheAlphaIsNotStoredAndAMissingPartRemovesItsKey)
{
    InMemoryPreferences prefs;
    setDefaultPlotAppearance(prefs, altitude(), PlotAppearance(Color(1, 2, 3), LineStyle::DASHDOT));

    setDefaultPlotAppearance(prefs, altitude(),
                             PlotAppearance(Color(10, 20, 30, 40), std::nullopt));
    EXPECT_EQ(stored(prefs), (Stored{{"Altitude.color", "10,20,30"}}));
    EXPECT_EQ(getDefaultPlotAppearance(prefs, altitude()),
              PlotAppearance(Color(10, 20, 30, 255), std::nullopt));

    setDefaultPlotAppearance(prefs, altitude(), PlotAppearance(std::nullopt, LineStyle::SOLID));
    EXPECT_EQ(stored(prefs), (Stored{{"Altitude.lineStyle", "SOLID"}}));
    EXPECT_EQ(getDefaultPlotAppearance(prefs, altitude()),
              PlotAppearance(std::nullopt, LineStyle::SOLID));
}

TEST(PlotPreferences, AnEmptyAppearanceOrNoneRemovesTheTypesKeysOnly)
{
    InMemoryPreferences   prefs;
    const FlightDataType& velocity = FlightDataType::builtin(Id::TYPE_VELOCITY_Z);
    setDefaultPlotAppearance(prefs, altitude(), PlotAppearance(std::nullopt, LineStyle::SOLID));
    setDefaultPlotAppearance(prefs, velocity,
                             PlotAppearance(Color(0, 114, 189), LineStyle::DOTTED));

    setDefaultPlotAppearance(prefs, altitude(), PlotAppearance(std::nullopt, std::nullopt));
    EXPECT_EQ(stored(prefs), (Stored{{"Vertical velocity.color", "0,114,189"},
                                     {"Vertical velocity.lineStyle", "DOTTED"}}));
    EXPECT_EQ(getDefaultPlotAppearance(prefs, altitude()), std::nullopt);

    setDefaultPlotAppearance(prefs, velocity, std::nullopt);
    EXPECT_EQ(stored(prefs), Stored{});
}

/// What the probe's getDefaultPlotAppearance() gave for texts stored by hand.
struct StoredPin
{
    StoredPin(std::optional<std::string_view> pinColor, std::optional<std::string_view> pinStyle,
              std::optional<PlotAppearance> pinExpected)
      : color(pinColor), lineStyle(pinStyle), expected(pinExpected)
    {
    }

    std::optional<std::string_view> color;
    std::optional<std::string_view> lineStyle;
    std::optional<PlotAppearance>   expected;
};

TEST(PlotPreferences, StoredTextsAreReadAsSwingPreferencesReadsThem)
{
    InMemoryPreferences prefs;
    for (const StoredPin& pin : {
             StoredPin("0,114,189", "DASHED",
                       PlotAppearance(Color(0, 114, 189), LineStyle::DASHED)),
             StoredPin("300,-5,20", std::nullopt, PlotAppearance(Color(255, 0, 20), std::nullopt)),
             StoredPin(std::nullopt, "dashed", std::nullopt),
             StoredPin("1,2", "SOLID", PlotAppearance(std::nullopt, LineStyle::SOLID)),
             StoredPin("a,b,c", std::nullopt, std::nullopt),
             StoredPin(" 1,2,3", std::nullopt, std::nullopt),
             StoredPin("1,2,3,", "DOTTED ", PlotAppearance(Color(1, 2, 3), std::nullopt)),
             StoredPin("1,2,3,4", "DASHDOT", PlotAppearance(std::nullopt, LineStyle::DASHDOT)),
             StoredPin("", "", std::nullopt),
             StoredPin(std::nullopt, "Solid", std::nullopt),
         })
    {
        prefs.putString(kPlotDefaultAppearancesNode, "Altitude.color", pin.color);
        prefs.putString(kPlotDefaultAppearancesNode, "Altitude.lineStyle", pin.lineStyle);
        EXPECT_EQ(getDefaultPlotAppearance(prefs, altitude()), pin.expected)
            << pin.color.value_or("absent") << " / " << pin.lineStyle.value_or("absent");
    }
}

TEST(PlotPreferences, AnotherTypesAppearanceIsNotThisOnes)
{
    InMemoryPreferences prefs;
    setDefaultPlotAppearance(prefs, altitude(), PlotAppearance(Color(1, 2, 3), LineStyle::DASHED));
    EXPECT_EQ(getDefaultPlotAppearance(prefs, FlightDataType::builtin(Id::TYPE_ALTITUDE_ABOVE_SEA)),
              std::nullopt);
}

TEST(PlotPreferences, TheFlagsHaveJavasDefaultsAndRoundTrip)
{
    InMemoryPreferences prefs;
    EXPECT_FALSE(QtRocket::PlotPreferences::isShowPoints(prefs));
    EXPECT_TRUE(QtRocket::PlotPreferences::isShowEvents(prefs));
    EXPECT_FALSE(QtRocket::PlotPreferences::isMarkerStyleIcon(prefs));

    QtRocket::PlotPreferences::setShowPoints(prefs, true);
    QtRocket::PlotPreferences::setShowEvents(prefs, false);
    QtRocket::PlotPreferences::setMarkerStyleIcon(prefs, true);
    EXPECT_TRUE(QtRocket::PlotPreferences::isShowPoints(prefs));
    EXPECT_FALSE(QtRocket::PlotPreferences::isShowEvents(prefs));
    EXPECT_TRUE(QtRocket::PlotPreferences::isMarkerStyleIcon(prefs));
    // Java's keys.
    EXPECT_EQ(prefs.get("ShowPlotPoints"), "true");
    EXPECT_EQ(prefs.get("ShowPlotEvents"), "false");
    EXPECT_EQ(prefs.get("MarkerStyleIcon"), "true");
}

}  // namespace
