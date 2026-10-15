// Tests of cli/InfoCommand.h beyond the command, which CliTests.cpp runs: QtRocket's own, and
// the two behaviours of swing RocketInfoContextHelperTest (OpenRocket's design window, whose
// numbers 'info' prints).

#include "QtRocket/cli/InfoCommand.h"

#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "QtRocket/cli/CliOutput.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/unit/CaliberUnit.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::CaliberUnit;
using QtRocket::CliOutput;
using QtRocket::FlightConfiguration;
using QtRocket::InfoCommand;
using QtRocket::InMemoryPreferences;
using QtRocket::Rocket;
using QtRocket::Warning;
using QtRocket::Test::TestEstesAlphaIII;

// CaliberUnit.calculateCaliber(FlightConfiguration): the body tube of the rocket of swing
// RocketInfoContextHelperTest.bodyTubeOnlyUsesSameFiniteCpAndStabilityInBothViews, whose
// stability "-2 cal" is (0 - 0.5) / 0.25.
TEST(InfoCommand, TheCaliberIsTheLargestDiameter)
{
    Rocket      rocket;
    AxialStage& stage = rocket.addChild(std::make_unique<AxialStage>());
    stage.addChild(std::make_unique<BodyTube>(1.0, 0.125));
    FlightConfiguration configuration(rocket);
    configuration.setAllStages();
    EXPECT_EQ(InfoCommand::caliber(configuration), 0.25);
}

TEST(InfoCommand, ARocketWithoutBodyHasTheDefaultCaliber)
{
    Rocket rocket;
    rocket.addChild(std::make_unique<AxialStage>());
    FlightConfiguration configuration(rocket);
    configuration.setAllStages();
    EXPECT_EQ(InfoCommand::caliber(configuration), CaliberUnit::kDefaultCaliber);
}

// Java: RocketInfoContextHelperTest.displayPhysicsIncludesGeometryWarnings. The design window's
// CP comes with the warnings of the geometry check (checkGeometry() before getWorstCP()), so a
// nose cone wider than the body tube behind it gives Warning.DIAMETER_DISCONTINUITY; 'info'
// prints them under the configuration.
TEST(InfoCommand, TheWarningsOfTheGeometryCheckArePrinted)
{
    const TestEstesAlphaIII alpha;
    alpha.nose->setAftRadius(alpha.body->getOuterRadius() * 1.25);
    const InMemoryPreferences preferences;
    CliOutput                 output;
    InfoCommand::printConfiguration(*alpha.rocket, 0, 0.3, preferences, output);
    const std::string expected =
        "\n  warnings:\n    " + Warning::kDiameterDiscontinuity.messageDescription();
    EXPECT_NE(output.out().find(expected), std::string::npos) << output.out();
}

// Java: RocketInfoContextHelperTest.bodyTubeOnlyUsesSameFiniteCpAndStabilityInBothViews. A lone
// body tube has no lift (getWorstCP() gives its sentinel at Double.MAX_VALUE with a weight of 0):
// OpenRocket shows a CP of "0 m" and a stability of "-2 cal" from it; 'info' shows no CP and no
// stability (a deviation, see InfoCommand), and never the sentinel.
TEST(InfoCommand, ADesignWithoutLiftHasNoCp)
{
    Rocket      rocket;
    AxialStage& stage = rocket.addChild(std::make_unique<AxialStage>());
    stage.addChild(std::make_unique<BodyTube>(1.0, 0.125));
    const InMemoryPreferences preferences;
    CliOutput                 output;
    InfoCommand::printConfiguration(rocket, 0, 0.3, preferences, output);
    const std::string& text = output.out();
    EXPECT_NE(text.find("\n  CP at Mach 0.3, angle of attack 0: none (no lift)\n"),
              std::string::npos)
        << text;
    EXPECT_NE(text.find("\n  worst CP at Mach 0.3: none (no lift)\n"), std::string::npos);
    EXPECT_NE(text.find("\n  stability: none\n"), std::string::npos);
    EXPECT_EQ(text.find("e+308"), std::string::npos);
}

}  // namespace
