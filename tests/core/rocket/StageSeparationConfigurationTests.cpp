#include "QtRocket/rocket/StageSeparationConfiguration.h"

#include <algorithm>
#include <optional>

#include <gtest/gtest.h>

#include "QtRocket/rocket/FlightConfigurableParameter.h"
#include "QtRocket/rocket/FlightConfigurationId.h"

namespace
{

using QtRocket::FlightConfigurationId;
using QtRocket::StageSeparationConfiguration;
using SeparationEvent = StageSeparationConfiguration::SeparationEvent;

static_assert(QtRocket::FlightConfigurableParameter<StageSeparationConfiguration>);

TEST(StageSeparationConfiguration, DefaultsAsJava)
{
    const StageSeparationConfiguration config;
    EXPECT_EQ(config.getSeparationEvent(), SeparationEvent::EJECTION);
    EXPECT_EQ(config.getSeparationAltitude(), 200.0);
    EXPECT_EQ(config.getSeparationDelay(), 0.0);
}

TEST(StageSeparationConfiguration, Setters)
{
    StageSeparationConfiguration config;
    config.setSeparationEvent(SeparationEvent::APOGEE);
    config.setSeparationAltitude(350);
    config.setSeparationDelay(1.5);
    EXPECT_EQ(config.getSeparationEvent(), SeparationEvent::APOGEE);
    EXPECT_EQ(config.getSeparationAltitude(), 350.0);
    EXPECT_EQ(config.getSeparationDelay(), 1.5);

    // A value within MathUtil.equals of the current one is ignored.
    config.setSeparationDelay(1.5 + 1e-12);
    EXPECT_EQ(config.getSeparationDelay(), 1.5);
    config.setSeparationAltitude(350 + 1e-9);
    EXPECT_EQ(config.getSeparationAltitude(), 350.0);
}

TEST(StageSeparationConfiguration, ToString)
{
    StageSeparationConfiguration config;
    EXPECT_EQ(config.toString(), "Current stage ejection charge");
    config.setSeparationDelay(1.5);
    EXPECT_EQ(config.toString(), "Current stage ejection charge + 1.5s");
    config.setSeparationEvent(SeparationEvent::NEVER);
    config.setSeparationDelay(3);
    EXPECT_EQ(config.toString(), "Never + 3.0s");
    config.setSeparationDelay(-1);
    EXPECT_EQ(config.toString(), "Never");
}

TEST(StageSeparationConfiguration, EqualityIgnoresTheAltitude)
{
    StageSeparationConfiguration a;
    StageSeparationConfiguration b;
    b.setSeparationAltitude(999);
    EXPECT_EQ(a, b);
    b.setSeparationDelay(2);
    EXPECT_NE(a, b);
    a.setSeparationDelay(2);
    EXPECT_EQ(a, b);
    a.setSeparationEvent(SeparationEvent::LAUNCH);
    EXPECT_NE(a, b);
}

TEST(StageSeparationConfiguration, CloneAndCopyKeepEveryField)
{
    StageSeparationConfiguration config;
    config.setSeparationEvent(SeparationEvent::ALTITUDE_DESCENDING);
    config.setSeparationAltitude(123);
    config.setSeparationDelay(0.5);

    const StageSeparationConfiguration clone = config.clone();
    EXPECT_EQ(clone.getSeparationEvent(), SeparationEvent::ALTITUDE_DESCENDING);
    EXPECT_EQ(clone.getSeparationAltitude(), 123.0);
    EXPECT_EQ(clone.getSeparationDelay(), 0.5);

    const StageSeparationConfiguration copy = config.copy(FlightConfigurationId{});
    EXPECT_EQ(copy, config);
    EXPECT_EQ(copy.getSeparationAltitude(), 123.0);
}

TEST(StageSeparationConfiguration, EventNames)
{
    EXPECT_EQ(StageSeparationConfiguration::kAllSeparationEvents.size(), 9U);
    EXPECT_EQ(separationEventName(SeparationEvent::UPPER_IGNITION), "UPPER_IGNITION");
    EXPECT_EQ(orkName(SeparationEvent::LAUNCH), "launch");
    EXPECT_EQ(orkName(SeparationEvent::UPPER_IGNITION), "upperignition");
    EXPECT_EQ(orkName(SeparationEvent::ALTITUDE_ASCENDING), "altitudeascending");
    EXPECT_EQ(orkName(SeparationEvent::ALTITUDE_DESCENDING), "altitudedescending");
}

TEST(StageSeparationConfiguration, EventOrkNamesReadBack)
{
    EXPECT_TRUE(std::ranges::all_of(
        StageSeparationConfiguration::kAllSeparationEvents, [](SeparationEvent event) {
            return QtRocket::separationEventFromOrkName(orkName(event)) == event;
        }));
    EXPECT_EQ(QtRocket::separationEventFromOrkName("upper_ignition"), std::nullopt);
    EXPECT_EQ(QtRocket::separationEventFromOrkName(" apogee\t"), SeparationEvent::APOGEE);
}

TEST(StageSeparationConfiguration, EventDisplayNames)
{
    EXPECT_EQ(displayKey(SeparationEvent::BURNOUT), "Stage.SeparationEvent.BURNOUT");
    EXPECT_EQ(displayName(SeparationEvent::ALTITUDE_ASCENDING), "Specific altitude during ascent");
    EXPECT_EQ(displayName(SeparationEvent::UPPER_IGNITION), "Upper stage motor ignition");
}

}  // namespace
