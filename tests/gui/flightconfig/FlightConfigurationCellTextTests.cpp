#include "QtRocket/gui/flightconfig/FlightConfigurationCellText.h"

#include <optional>

#include <QString>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ClusterConfiguration.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/unit/UnitGroup.h"
#include "rocket/TestRockets.h"
#include "unit/DefaultUnitsGuard.h"

namespace QtRocket::Gui
{

namespace
{

// The expected texts are OpenRocket's (commit 5f164fd0e): the flight-config pins
// (tools/openrocket-gui-pins/flight-config, FlightConfigurationTablePins.h) for the test rockets'
// cells, and for the units the M3 run-2 probe UnitTextProbe.java (UnitGroup.UNITS_SHORT_TIME and
// UNITS_DISTANCE.toStringUnit() of 0.0011 s "0.001 s", 1.5 s "1.5 s", 2 s "2 s", 150 m "150 m",
// 152.4 m "152 m" and, with ft as the default unit, 150 m "492 ft"). The renderers' code is
// MotorConfigurationPanel.java:524-568, RecoveryConfigurationPanel.java:295-307 and
// SeparationConfigurationPanel.java:298-308.

using FlightConfigurationCellTextTest = Gui::Test::GuiTest;
using QtRocket::Test::testFcid;
using DeployEvent     = DeploymentConfiguration::DeployEvent;
using SeparationEvent = StageSeparationConfiguration::SeparationEvent;

/// @p text with the UTF-8 multiplication sign of a cluster, as the core writes it.
QString utf8(const char* text)
{
    return QString::fromUtf8(text);
}

/// The ClusterConfiguration of @p count motors that comes first in OpenRocket's list.
const ClusterConfiguration& clusterOf(int count)
{
    for (const ClusterConfiguration& cluster : ClusterConfiguration::configurations())
    {
        if (cluster.getClusterCount() == count)
        {
            return cluster;
        }
    }
    ADD_FAILURE() << "no cluster of " << count;
    return ClusterConfiguration::single();
}

TEST_F(FlightConfigurationCellTextTest, HeadsColumnZeroConfiguration)
{
    EXPECT_EQ(FlightConfigurationCellText::configurationHeader(), QStringLiteral("Configuration"));
}

TEST_F(FlightConfigurationCellTextTest, HeadsAComponentsColumnWithItsName)
{
    const QtRocket::Test::TestEstesAlphaIII alpha;
    EXPECT_EQ(FlightConfigurationCellText::componentHeader(*alpha.inner),
              QStringLiteral("Motor Mount Tube"));
    alpha.inner->setName("Main mount");
    EXPECT_EQ(FlightConfigurationCellText::componentHeader(*alpha.inner),
              QStringLiteral("Main mount"));
}

TEST_F(FlightConfigurationCellTextTest, NamesAConfigurationWithItsMotorsSubstituted)
{
    const QtRocket::Test::TestEstesAlphaIII alpha;
    const InMemoryPreferences               preferences;
    EXPECT_EQ(
        FlightConfigurationCellText::configurationName(*alpha.rocket, testFcid(0), preferences),
        QStringLiteral("[A8-0]"));
    alpha.rocket->getFlightConfiguration(testFcid(0)).setName("Flight {motors}");
    EXPECT_EQ(
        FlightConfigurationCellText::configurationName(*alpha.rocket, testFcid(0), preferences),
        QStringLiteral("Flight A8-0"));
    // An id the rocket does not have: the default configuration's name.
    EXPECT_EQ(FlightConfigurationCellText::configurationName(
                  *alpha.rocket, FlightConfigurationId::defaultValueId(), preferences),
              QStringLiteral("[No motors]"));
}

TEST_F(FlightConfigurationCellTextTest, ShowsNoneForAConfigurationWithoutAMotor)
{
    const QtRocket::Test::TestBeta beta;
    const InMemoryPreferences      preferences;
    // makeBeta's booster motor mount has a motor in TEST_FCID_1 only.
    EXPECT_EQ(FlightConfigurationCellText::motor(*beta.boosterMmt, testFcid(0), preferences),
              QStringLiteral("None"));
    EXPECT_EQ(FlightConfigurationCellText::motor(*beta.boosterMmt, testFcid(1), preferences),
              QStringLiteral("D21-0"));
}

TEST_F(FlightConfigurationCellTextTest, ShowsTheMotorWithItsEjectionDelay)
{
    const QtRocket::Test::TestEstesAlphaIII alpha;
    const InMemoryPreferences               preferences;
    EXPECT_EQ(FlightConfigurationCellText::motor(*alpha.inner, testFcid(0), preferences),
              QStringLiteral("A8-0"));
    EXPECT_EQ(FlightConfigurationCellText::motor(*alpha.inner, testFcid(3), preferences),
              QStringLiteral("C6-5"));
}

TEST_F(FlightConfigurationCellTextTest, CountsTheMotorsOfAClusterBeforeTheName)
{
    const QtRocket::Test::TestEstesAlphaIII alpha;
    const InMemoryPreferences               preferences;
    alpha.inner->setClusterConfiguration(clusterOf(3));
    EXPECT_EQ(FlightConfigurationCellText::motor(*alpha.inner, testFcid(2), preferences),
              utf8("3\xC3\x97 C6-3"));
    // A configuration without a motor shows none, whatever the cluster.
    alpha.inner->setMotorConfig(std::nullopt, testFcid(2));
    EXPECT_EQ(FlightConfigurationCellText::motor(*alpha.inner, testFcid(2), preferences),
              QStringLiteral("None"));
}

TEST_F(FlightConfigurationCellTextTest, NamesTheMotorByTheDesignationOrTheCommonName)
{
    const QtRocket::Test::TestEstesAlphaIII alpha;
    InMemoryPreferences                     preferences;
    // A motor whose designation is not its common name ("B6W", "B6").
    QtRocket::Test::addMotor(
        *alpha.inner, testFcid(1),
        QtRocket::Test::makeTestMotor("Quest", "B6W", "", "SU 18.0x70.0", {0, 4}, 0.018, 0.070,
                                      {0, 0.5, 1}, {0, 6, 0}, {0.02, 0.015, 0.012}),
        4);
    EXPECT_EQ(FlightConfigurationCellText::motor(*alpha.inner, testFcid(1), preferences),
              QStringLiteral("B6W-4"));
    preferences.setMotorNameColumn(false);
    EXPECT_EQ(FlightConfigurationCellText::motor(*alpha.inner, testFcid(1), preferences),
              QStringLiteral("B6-4"));
}

TEST_F(FlightConfigurationCellTextTest, ShowsTheDefaultIgnitionShadedWithoutAnOverride)
{
    const QtRocket::Test::TestEstesAlphaIII     alpha;
    const FlightConfigurationCellText::Ignition ignition =
        FlightConfigurationCellText::ignition(*alpha.inner, testFcid(0));
    EXPECT_EQ(ignition.text, QStringLiteral("Default (Automatic)"));
    EXPECT_TRUE(ignition.usesDefault);
}

TEST_F(FlightConfigurationCellTextTest, ShowsTheMountsDefaultIgnitionInsideDefault)
{
    const QtRocket::Test::TestEstesAlphaIII alpha;
    MotorConfiguration&                     mountDefault = alpha.inner->getDefaultMotorConfig();
    mountDefault.setIgnitionEvent(IgnitionEvent::BURNOUT);
    mountDefault.setIgnitionDelay(2);
    const FlightConfigurationCellText::Ignition ignition =
        FlightConfigurationCellText::ignition(*alpha.inner, testFcid(0));
    EXPECT_EQ(ignition.text, QStringLiteral("Default (Burnout + 2 s)"));
    EXPECT_TRUE(ignition.usesDefault);
}

TEST_F(FlightConfigurationCellTextTest, ShowsAnOverriddenIgnitionRegular)
{
    const QtRocket::Test::TestEstesAlphaIII alpha;
    MotorConfiguration& configuration = alpha.inner->getMotorConfig(testFcid(0));
    configuration.setIgnitionEvent(IgnitionEvent::LAUNCH);
    configuration.setIgnitionDelay(1.5);
    const FlightConfigurationCellText::Ignition ignition =
        FlightConfigurationCellText::ignition(*alpha.inner, testFcid(0));
    EXPECT_EQ(ignition.text, QStringLiteral("Launch + 1.5 s"));
    EXPECT_FALSE(ignition.usesDefault);

    // useDefaultIgnition() goes through the setters: the override stays (Java's "Reset ignition").
    configuration.useDefaultIgnition();
    const FlightConfigurationCellText::Ignition reset =
        FlightConfigurationCellText::ignition(*alpha.inner, testFcid(0));
    EXPECT_EQ(reset.text, QStringLiteral("Automatic"));
    EXPECT_FALSE(reset.usesDefault);
}

TEST_F(FlightConfigurationCellTextTest, LeavesOutTheDelayOfNeverAndADelayOfAtMostAMillisecond)
{
    const QtRocket::Test::TestEstesAlphaIII alpha;
    MotorConfiguration& configuration = alpha.inner->getMotorConfig(testFcid(0));
    configuration.setIgnitionEvent(IgnitionEvent::NEVER);
    configuration.setIgnitionDelay(2);
    EXPECT_EQ(FlightConfigurationCellText::ignition(*alpha.inner, testFcid(0)).text,
              QStringLiteral("Never"));
    configuration.setIgnitionEvent(IgnitionEvent::EJECTION_CHARGE);
    configuration.setIgnitionDelay(0.001);
    EXPECT_EQ(FlightConfigurationCellText::ignition(*alpha.inner, testFcid(0)).text,
              QStringLiteral("Ejection charge"));
    configuration.setIgnitionDelay(0.0011);
    EXPECT_EQ(FlightConfigurationCellText::ignition(*alpha.inner, testFcid(0)).text,
              QStringLiteral("Ejection charge + 0.001 s"));
}

TEST_F(FlightConfigurationCellTextTest, ShowsTheMountsDefaultForAnIdWithoutAConfiguration)
{
    const QtRocket::Test::TestBeta beta;
    // No configuration of the booster mount for TEST_FCID_0: getMotorConfig() is the default.
    ASSERT_FALSE(beta.boosterMmt->getMotorConfigurationSet().containsId(testFcid(0)));
    EXPECT_EQ(FlightConfigurationCellText::ignition(*beta.boosterMmt, testFcid(0)).text,
              QStringLiteral("Default (Automatic)"));

    // A default whose own ignition was set through its setters counts as an override for such an
    // id (Java: "Parallel booster staging.ork" shows "Automatic", regular, for its empty booster
    // cell).
    beta.boosterMmt->getDefaultMotorConfig().setIgnitionEvent(IgnitionEvent::LAUNCH);
    const FlightConfigurationCellText::Ignition ignition =
        FlightConfigurationCellText::ignition(*beta.boosterMmt, testFcid(0));
    EXPECT_EQ(ignition.text, QStringLiteral("Launch"));
    EXPECT_FALSE(ignition.usesDefault);
}

TEST_F(FlightConfigurationCellTextTest, ShowsTheDeploymentEventItsAltitudeAndItsDelay)
{
    DeploymentConfiguration deployment;
    deployment.setDeployEvent(DeployEvent::EJECTION);
    EXPECT_EQ(FlightConfigurationCellText::deployment(deployment),
              QStringLiteral("Ejection charge"));
    deployment.setDeployEvent(DeployEvent::ALTITUDE);
    deployment.setDeployAltitude(152.4);
    EXPECT_EQ(FlightConfigurationCellText::deployment(deployment),
              QStringLiteral("Altitude 152 m"));
    deployment.setDeployAltitude(150);
    deployment.setDeployDelay(1.5);
    EXPECT_EQ(FlightConfigurationCellText::deployment(deployment),
              QStringLiteral("Altitude 150 m + 1.5 s"));
    // The altitude only for ALTITUDE; the delay whatever the event, also NEVER.
    deployment.setDeployEvent(DeployEvent::APOGEE);
    EXPECT_EQ(FlightConfigurationCellText::deployment(deployment),
              QStringLiteral("Apogee + 1.5 s"));
    deployment.setDeployEvent(DeployEvent::NEVER);
    deployment.setDeployDelay(0.001);
    EXPECT_EQ(FlightConfigurationCellText::deployment(deployment), QStringLiteral("Never"));
    deployment.setDeployEvent(DeployEvent::LOWER_STAGE_SEPARATION);
    EXPECT_EQ(FlightConfigurationCellText::deployment(deployment),
              QStringLiteral("Lower stage separation"));
}

TEST_F(FlightConfigurationCellTextTest, ShowsTheAltitudeInTheDefaultUnit)
{
    const QtRocket::Test::DefaultUnitsGuard units;
    ASSERT_TRUE(unitGroup(UnitGroupId::DISTANCE).setDefaultUnit("ft"));
    DeploymentConfiguration deployment;
    deployment.setDeployEvent(DeployEvent::ALTITUDE);
    deployment.setDeployAltitude(150);
    EXPECT_EQ(FlightConfigurationCellText::deployment(deployment),
              QStringLiteral("Altitude 492 ft"));
}

TEST_F(FlightConfigurationCellTextTest, ShowsTheSeparationEventAndItsDelay)
{
    StageSeparationConfiguration separation;
    EXPECT_EQ(FlightConfigurationCellText::separation(separation),
              QStringLiteral("Current stage ejection charge"));
    separation.setSeparationEvent(SeparationEvent::UPPER_IGNITION);
    separation.setSeparationDelay(2);
    EXPECT_EQ(FlightConfigurationCellText::separation(separation),
              QStringLiteral("Upper stage motor ignition + 2 s"));
    // No altitude for the altitude events (Java's renderer writes none); the delay also for NEVER.
    separation.setSeparationEvent(SeparationEvent::ALTITUDE_ASCENDING);
    separation.setSeparationAltitude(300);
    separation.setSeparationDelay(0);
    EXPECT_EQ(FlightConfigurationCellText::separation(separation),
              QStringLiteral("Specific altitude during ascent"));
    separation.setSeparationEvent(SeparationEvent::NEVER);
    separation.setSeparationDelay(1);
    EXPECT_EQ(FlightConfigurationCellText::separation(separation), QStringLiteral("Never + 1 s"));
}

TEST_F(FlightConfigurationCellTextTest, ShowsTheDefaultDeploymentWhereTheIdHasNoOverride)
{
    const QtRocket::Test::TestEstesAlphaIII alpha;
    auto& deployments = alpha.chute->getDeploymentConfigurations();
    EXPECT_TRUE(FlightConfigurationCellText::deploymentIsDefault(*alpha.chute, testFcid(0)));
    // Java's isDefault() (by identity) is false for such an id: OpenRocket bug 1.
    EXPECT_FALSE(deployments.isDefault(testFcid(0)));

    // An override, even one equal to the default, is not the default.
    deployments.set(testFcid(0), deployments.getDefault().copy(testFcid(0)));
    EXPECT_FALSE(FlightConfigurationCellText::deploymentIsDefault(*alpha.chute, testFcid(0)));
    EXPECT_TRUE(FlightConfigurationCellText::deploymentIsDefault(*alpha.chute, testFcid(1)));
}

TEST_F(FlightConfigurationCellTextTest, ShowsTheDefaultSeparationWhereTheIdHasNoOverride)
{
    const QtRocket::Test::TestBeta beta;
    auto&                          separations = beta.boosterStage->getSeparationConfigurations();
    EXPECT_TRUE(FlightConfigurationCellText::separationIsDefault(*beta.boosterStage, testFcid(2)));
    StageSeparationConfiguration burnout = separations.getDefault().copy(testFcid(2));
    burnout.setSeparationEvent(SeparationEvent::BURNOUT);
    separations.set(testFcid(2), burnout);
    EXPECT_FALSE(FlightConfigurationCellText::separationIsDefault(*beta.boosterStage, testFcid(2)));
    EXPECT_EQ(FlightConfigurationCellText::separation(separations.get(testFcid(2))),
              QStringLiteral("Current stage motor burnout"));
}

}  // namespace

}  // namespace QtRocket::Gui
