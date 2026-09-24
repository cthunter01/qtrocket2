#include "QtRocket/rocket/DeploymentConfiguration.h"

#include <array>
#include <cstddef>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "QtRocket/rocket/FlightConfigurableParameter.h"
#include "QtRocket/rocket/FlightConfigurableParameterSet.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/unit/UnitGroup.h"
#include "unit/DefaultUnitsGuard.h"

namespace
{

using QtRocket::DeploymentConfiguration;
using QtRocket::FlightConfigurableParameterSet;
using QtRocket::FlightConfigurationId;
using DeployEvent = DeploymentConfiguration::DeployEvent;

static_assert(QtRocket::FlightConfigurableParameter<DeploymentConfiguration>);

TEST(DeploymentConfiguration, Defaults)
{
    const DeploymentConfiguration config;
    EXPECT_EQ(config.getDeployEvent(), DeployEvent::EJECTION);
    EXPECT_EQ(config.getDeployAltitude(), 200.0);
    EXPECT_EQ(config.getDeployDelay(), 0.0);
    EXPECT_EQ(config.toString(), "First ejection charge of this stage");
}

TEST(DeploymentConfiguration, SettersIgnoreValuesWithinTheEpsilon)
{
    DeploymentConfiguration config;
    config.setDeployAltitude(200 * (1 + 1e-10));
    EXPECT_EQ(config.getDeployAltitude(), 200.0);
    config.setDeployAltitude(150);
    EXPECT_EQ(config.getDeployAltitude(), 150.0);
    config.setDeployDelay(1.5);
    EXPECT_EQ(config.getDeployDelay(), 1.5);
    config.setDeployDelay(1.5 * (1 + 1e-10));
    EXPECT_EQ(config.getDeployDelay(), 1.5);
    config.setDeployEvent(DeployEvent::APOGEE);
    EXPECT_EQ(config.getDeployEvent(), DeployEvent::APOGEE);
}

TEST(DeploymentConfiguration, ToString)
{
    const QtRocket::Test::DefaultUnitsGuard guard;
    DeploymentConfiguration                 config;
    config.setDeployEvent(DeployEvent::APOGEE);
    config.setDeployDelay(2);
    EXPECT_EQ(config.toString(), "Apogee + 2.0s");

    config.setDeployEvent(DeployEvent::ALTITUDE);
    config.setDeployDelay(0);
    config.setDeployAltitude(300);
    EXPECT_EQ(config.toString(),
              "Specific altitude during descent " +
                  QtRocket::unitGroup(QtRocket::UnitGroupId::DISTANCE).toString(300));
    config.setDeployDelay(1.5);
    EXPECT_EQ(config.toString(),
              "Specific altitude during descent + 1.5s " +
                  QtRocket::unitGroup(QtRocket::UnitGroupId::DISTANCE).toString(300));
    config.setDeployAltitude(0);
    EXPECT_EQ(config.toString(), "Specific altitude during descent + 1.5s") << "no altitude";

    config.setDeployEvent(DeployEvent::NEVER);
    config.setDeployAltitude(300);
    EXPECT_EQ(config.toString(), "Never + 1.5s") << "the altitude only for ALTITUDE";
}

TEST(DeploymentConfiguration, EqualityIsJavasDoubleCompare)
{
    DeploymentConfiguration a;
    DeploymentConfiguration b;
    EXPECT_EQ(a, b);
    b.setDeployAltitude(300);
    EXPECT_NE(a, b) << "unlike StageSeparationConfiguration, the altitude takes part";
    b.setDeployAltitude(200);
    b.setDeployDelay(1);
    EXPECT_NE(a, b);
    b.setDeployDelay(0);
    b.setDeployEvent(DeployEvent::LAUNCH);
    EXPECT_NE(a, b);

    DeploymentConfiguration nanA;
    DeploymentConfiguration nanB;
    nanA.setDeployAltitude(std::numeric_limits<double>::quiet_NaN());
    nanB.setDeployAltitude(std::numeric_limits<double>::quiet_NaN());
    EXPECT_EQ(nanA, nanB) << "Double.compare: NaN equals NaN";

    DeploymentConfiguration minusZero;
    minusZero.setDeployDelay(-0.0);  // within the epsilon of 0: ignored
    EXPECT_EQ(minusZero, a);
}

TEST(DeploymentConfiguration, CopyAndClone)
{
    DeploymentConfiguration config;
    config.setDeployEvent(DeployEvent::LOWER_STAGE_SEPARATION);
    config.setDeployDelay(3);
    config.setDeployAltitude(123);
    const DeploymentConfiguration clone = config.clone();
    EXPECT_EQ(clone, config);
    const DeploymentConfiguration copy = config.copy(FlightConfigurationId{});
    EXPECT_EQ(copy, config);
    EXPECT_EQ(copy.getDeployEvent(), DeployEvent::LOWER_STAGE_SEPARATION);
    EXPECT_EQ(copy.getDeployDelay(), 3.0);
    EXPECT_EQ(copy.getDeployAltitude(), 123.0);
}

TEST(DeploymentConfiguration, InAParameterSet)
{
    FlightConfigurableParameterSet<DeploymentConfiguration> set{DeploymentConfiguration{}};
    const FlightConfigurationId                             fcid;
    DeploymentConfiguration                                 apogee;
    apogee.setDeployEvent(DeployEvent::APOGEE);
    set.set(fcid, apogee);
    EXPECT_EQ(set.get(fcid).getDeployEvent(), DeployEvent::APOGEE);
    EXPECT_EQ(set.toDebug(),
              "====== Dumping ConfigurationSet<DeploymentConfiguration> (1 configurations)\n"
              "    [" +
                  fcid.toShortKey() + "    ]: Apogee\n");
}

/// The names of one event.
struct EventRow
{
    DeployEvent      event;
    std::string_view name;
    std::string_view ork;
    std::string_view display;
    std::string_view shortDisplay;
};

/// Whether every name of @p row.event, the @p index-th event, is as in @p row.
::testing::AssertionResult namesMatch(std::size_t index, const EventRow& row)
{
    if (DeploymentConfiguration::kAllDeployEvents.at(index) != row.event)
    {
        return ::testing::AssertionFailure() << "the event order";
    }
    if (QtRocket::deployEventName(row.event) != row.name ||
        QtRocket::orkName(row.event) != row.ork ||
        QtRocket::deployEventFromOrkName(row.ork) != row.event)
    {
        return ::testing::AssertionFailure() << "the name or .ork name of " << row.name;
    }
    if (QtRocket::displayKey(row.event) != std::format("RecoveryDevice.DeployEvent.{}", row.name) ||
        QtRocket::displayName(row.event) != row.display)
    {
        return ::testing::AssertionFailure() << "the display key or name of " << row.name;
    }
    if (QtRocket::shortDisplayKey(row.event) !=
            std::format("RecoveryDevice.DeployEvent.short.{}", row.name) ||
        QtRocket::shortDisplayName(row.event) != row.shortDisplay)
    {
        return ::testing::AssertionFailure() << "the short display key or name of " << row.name;
    }
    return ::testing::AssertionSuccess();
}

TEST(DeploymentConfiguration, EventNames)
{
    const std::array<EventRow, 6> rows{{
        {.event        = DeployEvent::LAUNCH,
         .name         = "LAUNCH",
         .ork          = "launch",
         .display      = "Launch (plus NN seconds)",
         .shortDisplay = "Launch"},
        {.event        = DeployEvent::EJECTION,
         .name         = "EJECTION",
         .ork          = "ejection",
         .display      = "First ejection charge of this stage",
         .shortDisplay = "Ejection charge"},
        {.event        = DeployEvent::APOGEE,
         .name         = "APOGEE",
         .ork          = "apogee",
         .display      = "Apogee",
         .shortDisplay = "Apogee"},
        {.event        = DeployEvent::ALTITUDE,
         .name         = "ALTITUDE",
         .ork          = "altitude",
         .display      = "Specific altitude during descent",
         .shortDisplay = "Altitude"},
        {.event        = DeployEvent::LOWER_STAGE_SEPARATION,
         .name         = "LOWER_STAGE_SEPARATION",
         .ork          = "lowerstageseparation",
         .display      = "Lower stage separation",
         .shortDisplay = "Lower stage separation"},
        {.event        = DeployEvent::NEVER,
         .name         = "NEVER",
         .ork          = "never",
         .display      = "Never",
         .shortDisplay = "Never"},
    }};
    static_assert(DeploymentConfiguration::kAllDeployEvents.size() == 6);
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        EXPECT_TRUE(namesMatch(i, rows.at(i)));
    }
}

TEST(DeploymentConfiguration, EventFromOrkName)
{
    // DocumentConfig.findEnum(): trimmed, then exactly the lower-cased name without underscores.
    EXPECT_EQ(QtRocket::deployEventFromOrkName(" lowerstageseparation\n"),
              DeployEvent::LOWER_STAGE_SEPARATION);
    EXPECT_EQ(QtRocket::deployEventFromOrkName("LowerStageSeparation"), std::nullopt);
    EXPECT_EQ(QtRocket::deployEventFromOrkName("lower_stage_separation"), std::nullopt);
    EXPECT_EQ(QtRocket::deployEventFromOrkName("burnout"), std::nullopt);
    EXPECT_EQ(QtRocket::deployEventFromOrkName(""), std::nullopt);
}

}  // namespace
