#include "QtRocket/motor/IgnitionEvent.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

namespace
{

using QtRocket::IgnitionEvent;
using QtRocket::ignitionEventFromOrkName;
using QtRocket::kAllIgnitionEvents;

TEST(IgnitionEvent, DeclarationOrder)
{
    const std::vector<IgnitionEvent> expected{IgnitionEvent::AUTOMATIC, IgnitionEvent::LAUNCH,
                                              IgnitionEvent::EJECTION_CHARGE,
                                              IgnitionEvent::BURNOUT, IgnitionEvent::NEVER};
    EXPECT_EQ(std::vector<IgnitionEvent>(kAllIgnitionEvents.begin(), kAllIgnitionEvents.end()),
              expected);
}

TEST(IgnitionEvent, Names)
{
    EXPECT_EQ(name(IgnitionEvent::AUTOMATIC), "AUTOMATIC");
    EXPECT_EQ(name(IgnitionEvent::LAUNCH), "LAUNCH");
    EXPECT_EQ(name(IgnitionEvent::EJECTION_CHARGE), "EJECTION_CHARGE");
    EXPECT_EQ(name(IgnitionEvent::BURNOUT), "BURNOUT");
    EXPECT_EQ(name(IgnitionEvent::NEVER), "NEVER");
}

TEST(IgnitionEvent, DisplayKeys)
{
    for (const IgnitionEvent event : kAllIgnitionEvents)
    {
        EXPECT_EQ(displayKey(event), "MotorMount.IgnitionEvent." + std::string(name(event)));
        EXPECT_EQ(shortDisplayKey(event),
                  "MotorMount.IgnitionEvent.short." + std::string(name(event)));
    }
}

TEST(IgnitionEvent, DisplayNames)
{
    EXPECT_EQ(displayName(IgnitionEvent::AUTOMATIC), "Automatic (launch or ejection charge)");
    EXPECT_EQ(displayName(IgnitionEvent::LAUNCH), "Launch");
    EXPECT_EQ(displayName(IgnitionEvent::EJECTION_CHARGE),
              "First ejection charge of previous stage");
    EXPECT_EQ(displayName(IgnitionEvent::BURNOUT), "First burnout of previous stage");
    EXPECT_EQ(displayName(IgnitionEvent::NEVER), "Never");
}

TEST(IgnitionEvent, ShortDisplayNames)
{
    EXPECT_EQ(shortDisplayName(IgnitionEvent::AUTOMATIC), "Automatic");
    EXPECT_EQ(shortDisplayName(IgnitionEvent::LAUNCH), "Launch");
    EXPECT_EQ(shortDisplayName(IgnitionEvent::EJECTION_CHARGE), "Ejection charge");
    EXPECT_EQ(shortDisplayName(IgnitionEvent::BURNOUT), "Burnout");
    EXPECT_EQ(shortDisplayName(IgnitionEvent::NEVER), "Never");
}

/// @p text lower-cased without its underscores, as the .ork saver writes an event's name.
std::string lowerWithoutUnderscores(std::string_view text)
{
    std::string out;
    for (const char c : text)
    {
        if (c != '_')
        {
            out.push_back(static_cast<char>(c - 'A' + 'a'));
        }
    }
    return out;
}

TEST(IgnitionEvent, OrkNamesAreLowerCaseWithoutUnderscores)
{
    EXPECT_EQ(orkName(IgnitionEvent::AUTOMATIC), "automatic");
    EXPECT_EQ(orkName(IgnitionEvent::LAUNCH), "launch");
    EXPECT_EQ(orkName(IgnitionEvent::EJECTION_CHARGE), "ejectioncharge");
    EXPECT_EQ(orkName(IgnitionEvent::BURNOUT), "burnout");
    EXPECT_EQ(orkName(IgnitionEvent::NEVER), "never");
}

TEST(IgnitionEvent, OrkNamesFollowTheConstantNames)
{
    for (const IgnitionEvent event : kAllIgnitionEvents)
    {
        EXPECT_EQ(orkName(event), lowerWithoutUnderscores(name(event)));
    }
}

TEST(IgnitionEvent, MatchesOrkNameExactly)
{
    // IgnitionEvent.equals(String)
    EXPECT_TRUE(matchesOrkName(IgnitionEvent::EJECTION_CHARGE, "ejectioncharge"));
    EXPECT_FALSE(matchesOrkName(IgnitionEvent::EJECTION_CHARGE, "ejection_charge"));
    EXPECT_FALSE(matchesOrkName(IgnitionEvent::EJECTION_CHARGE, "EJECTIONCHARGE"));
    EXPECT_FALSE(matchesOrkName(IgnitionEvent::EJECTION_CHARGE, "EJECTION_CHARGE"));
    EXPECT_FALSE(matchesOrkName(IgnitionEvent::LAUNCH, "burnout"));
}

TEST(IgnitionEvent, FromOrkName)
{
    for (const IgnitionEvent event : kAllIgnitionEvents)
    {
        EXPECT_EQ(ignitionEventFromOrkName(orkName(event)), event);
    }
    EXPECT_EQ(ignitionEventFromOrkName("Launch"), std::nullopt);
    EXPECT_EQ(ignitionEventFromOrkName(" launch"), std::nullopt);
    EXPECT_EQ(ignitionEventFromOrkName("ejection_charge"), std::nullopt);
    EXPECT_EQ(ignitionEventFromOrkName(""), std::nullopt);
}

}  // namespace
