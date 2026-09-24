#include "QtRocket/rocket/Finish.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

namespace
{

using QtRocket::Finish;

struct Expected
{
    Finish           finish;
    std::string_view name;
    std::string_view ork;
    double           roughness;
    std::string_view key;
    std::string_view display;
    std::string_view text;  ///< Finish.toString() with the default (metric) units
};

// Pinned from OpenRocket: name(), getRoughnessSize(), the translation key, its English text and
// toString().
constexpr std::array<Expected, 9> kExpected{{
    {.finish    = Finish::ROUGH,
     .name      = "ROUGH",
     .ork       = "rough",
     .roughness = 500.0e-6,
     .key       = "ExternalComponent.Rough",
     .display   = "Rough",
     .text      = "Rough (500 \xC2\xB5m)"},
    {.finish    = Finish::ROUGHUNFINISHED,
     .name      = "ROUGHUNFINISHED",
     .ork       = "roughunfinished",
     .roughness = 250.0e-6,
     .key       = "ExternalComponent.Roughunfinished",
     .display   = "Rough unfinished",
     .text      = "Rough unfinished (250 \xC2\xB5m)"},
    {.finish    = Finish::UNFINISHED,
     .name      = "UNFINISHED",
     .ork       = "unfinished",
     .roughness = 150.0e-6,
     .key       = "ExternalComponent.Unfinished",
     .display   = "Unfinished",
     .text      = "Unfinished (150 \xC2\xB5m)"},
    {.finish    = Finish::NORMAL,
     .name      = "NORMAL",
     .ork       = "normal",
     .roughness = 60.0e-6,
     .key       = "ExternalComponent.Regularpaint",
     .display   = "Regular paint",
     .text      = "Regular paint (60 \xC2\xB5m)"},
    {.finish    = Finish::SMOOTH,
     .name      = "SMOOTH",
     .ork       = "smooth",
     .roughness = 20.0e-6,
     .key       = "ExternalComponent.Smoothpaint",
     .display   = "Smooth paint",
     .text      = "Smooth paint (20 \xC2\xB5m)"},
    {.finish    = Finish::OPTIMUM,
     .name      = "OPTIMUM",
     .ork       = "optimum",
     .roughness = 5.0e-6,
     .key       = "ExternalComponent.Optimumpaint",
     .display   = "Optimum paint",
     .text      = "Optimum paint (5 \xC2\xB5m)"},
    {.finish    = Finish::POLISHED,
     .name      = "POLISHED",
     .ork       = "polished",
     .roughness = 2.0e-6,
     .key       = "ExternalComponent.Polished",
     .display   = "Aircraft sheet-metal",
     .text      = "Aircraft sheet-metal (2 \xC2\xB5m)"},
    {.finish    = Finish::FINISHPOLISHED,
     .name      = "FINISHPOLISHED",
     .ork       = "finishpolished",
     .roughness = 0.5e-6,
     .key       = "ExternalComponent.Finishedpolished",
     .display   = "Finished/polished surface",
     .text      = "Finished/polished surface (0.5 \xC2\xB5m)"},
    {.finish    = Finish::MIRROR,
     .name      = "MIRROR",
     .ork       = "mirror",
     .roughness = 0.0,
     .key       = "ExternalComponent.Mirror",
     .display   = "Mirror surface",
     .text      = "Mirror surface (0 \xC2\xB5m)"},
}};

void expectFinish(const Expected& e)
{
    SCOPED_TRACE(std::string(e.name));
    EXPECT_EQ(finishName(e.finish), e.name);
    EXPECT_EQ(orkName(e.finish), e.ork);
    EXPECT_EQ(roughnessSize(e.finish), e.roughness);
    EXPECT_EQ(displayKey(e.finish), e.key);
    EXPECT_EQ(displayName(e.finish), e.display);
    EXPECT_EQ(toString(e.finish), e.text);
}

void expectLookups(const Expected& e)
{
    SCOPED_TRACE(std::string(e.name));
    EXPECT_EQ(QtRocket::finishFromName(e.name), e.finish);
    EXPECT_EQ(QtRocket::finishFromOrkName(e.ork), e.finish);
}

TEST(Finish, DeclarationOrderIsJavas)
{
    ASSERT_EQ(QtRocket::kAllFinishes.size(), kExpected.size());
    for (std::size_t i = 0; i < kExpected.size(); i++)
    {
        EXPECT_EQ(QtRocket::kAllFinishes.at(i), kExpected.at(i).finish);
        EXPECT_EQ(static_cast<std::size_t>(kExpected.at(i).finish), i);
    }
}

TEST(Finish, NamesAndRoughness)
{
    for (const Expected& e : kExpected)
    {
        expectFinish(e);
        expectLookups(e);
    }
    static_assert(roughnessSize(Finish::NORMAL) == 60.0e-6);
}

TEST(Finish, LookupsFailCleanly)
{
    EXPECT_EQ(QtRocket::finishFromName("normal"), std::nullopt);
    EXPECT_EQ(QtRocket::finishFromName("Regular paint"), std::nullopt);
    EXPECT_EQ(QtRocket::finishFromOrkName(" smooth "), Finish::SMOOTH);
    EXPECT_EQ(QtRocket::finishFromOrkName("SMOOTH"), std::nullopt);
    EXPECT_EQ(QtRocket::finishFromOrkName("finish_polished"), std::nullopt);
    EXPECT_EQ(QtRocket::finishFromOrkName(""), std::nullopt);
}

}  // namespace
