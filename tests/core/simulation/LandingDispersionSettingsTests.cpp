#include "QtRocket/simulation/LandingDispersionSettings.h"

#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

// LandingDispersionSettings has no counterpart in OpenRocket's tests: it stands for
// MonteCarloSettings until the Monte Carlo analysis is ported, and keeps the texts of a
// <landingdispersion> element.

namespace
{

using QtRocket::LandingDispersionSettings;

using Attributes = LandingDispersionSettings::Attributes;

static_assert(std::is_copy_constructible_v<LandingDispersionSettings>);
static_assert(std::is_copy_assignable_v<LandingDispersionSettings>);
static_assert(std::is_nothrow_move_constructible_v<LandingDispersionSettings>);

TEST(LandingDispersionSettings, AreEmptyAsTheyAreMade)
{
    const LandingDispersionSettings settings;
    EXPECT_TRUE(settings.getAttributes().empty());
    EXPECT_TRUE(settings.getUncertainties().empty());
    EXPECT_FALSE(settings.getAttribute("runs").has_value());
    EXPECT_EQ(settings.toString(), "{} []");
}

TEST(LandingDispersionSettings, KeepTheAttributesAndTheUncertaintiesAsTheyAreGiven)
{
    LandingDispersionSettings settings(
        Attributes{{"seed", " 12345 "}, {"runs", "500"}, {"other", ""}},
        {Attributes{{"parameter", "windspeed"}, {"distribution", "normal"}, {"spread", "0.5"}}});
    settings.addUncertainty(Attributes{{"parameter", "windspeed"}, {"spread", "nonsense"}});
    settings.addUncertainty(Attributes{});

    EXPECT_EQ(settings.getAttributes().size(), 3U);
    EXPECT_EQ(settings.getAttribute("runs"), std::optional<std::string_view>("500"));
    // Nothing is trimmed or read.
    EXPECT_EQ(settings.getAttribute("seed"), std::optional<std::string_view>(" 12345 "));
    EXPECT_EQ(settings.getAttribute("other"), std::optional<std::string_view>(""));
    EXPECT_FALSE(settings.getAttribute("Runs").has_value());

    // The uncertainties stay in their order, also two of one parameter and an empty one.
    ASSERT_EQ(settings.getUncertainties().size(), 3U);
    EXPECT_EQ(settings.getUncertainties().at(0).at("distribution"), "normal");
    EXPECT_EQ(settings.getUncertainties().at(1).at("spread"), "nonsense");
    EXPECT_TRUE(settings.getUncertainties().at(2).empty());

    EXPECT_EQ(settings.toString(),
              "{other=, runs=500, seed= 12345 } [{distribution=normal, parameter=windspeed, "
              "spread=0.5}, {parameter=windspeed, spread=nonsense}, {}]");
}

TEST(LandingDispersionSettings, AreEqualWhenTheirTextsAre)
{
    const Attributes                wind{{"parameter", "windspeed"}, {"spread", "0.5"}};
    const Attributes                mass{{"parameter", "totalmass"}, {"spread", "0.02"}};
    const LandingDispersionSettings settings(Attributes{{"runs", "500"}, {"seed", "1"}},
                                             {wind, mass});

    EXPECT_TRUE(settings == LandingDispersionSettings(Attributes{{"seed", "1"}, {"runs", "500"}},
                                                      {wind, mass}));
    // Another seed (SimulationTest.testLandingDispersionSettingsAreOptionalAndCopied compares
    // two settings that differ in nothing else).
    EXPECT_FALSE(settings == LandingDispersionSettings(Attributes{{"runs", "500"}, {"seed", "2"}},
                                                       {wind, mass}));
    // The texts are compared, not the numbers they may stand for.
    EXPECT_FALSE(settings == LandingDispersionSettings(Attributes{{"runs", "500.0"}, {"seed", "1"}},
                                                       {wind, mass}));
    // One attribute more.
    EXPECT_FALSE(settings ==
                 LandingDispersionSettings(
                     Attributes{{"runs", "500"}, {"seed", "1"}, {"threads", "2"}}, {wind, mass}));
    // The uncertainties in another order, one less, one changed.
    EXPECT_FALSE(settings == LandingDispersionSettings(Attributes{{"runs", "500"}, {"seed", "1"}},
                                                       {mass, wind}));
    EXPECT_FALSE(settings ==
                 LandingDispersionSettings(Attributes{{"runs", "500"}, {"seed", "1"}}, {wind}));
    EXPECT_FALSE(settings ==
                 LandingDispersionSettings(Attributes{{"runs", "500"}, {"seed", "1"}},
                                           {wind, Attributes{{"parameter", "totalmass"}}}));
    EXPECT_FALSE(settings == LandingDispersionSettings());
}

TEST(LandingDispersionSettings, ACopyIsIndependent)
{
    LandingDispersionSettings       settings(Attributes{{"runs", "500"}});
    const LandingDispersionSettings copy = settings;
    settings.addUncertainty(Attributes{{"parameter", "thrust"}});
    EXPECT_FALSE(copy == settings);
    EXPECT_TRUE(copy.getUncertainties().empty());
    EXPECT_EQ(settings.getUncertainties().size(), 1U);
}

}  // namespace
