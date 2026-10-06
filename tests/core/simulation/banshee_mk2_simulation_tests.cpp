// Regression coverage for GitHub issue #3161 of OpenRocket, reported with the Banshee Mk2 design
// by GitHub user Enderdyls (Dylan Cole): OpenRocket's BansheeMk2SimulationTest
// (core/src/test/java/info/openrocket/core/simulation/BansheeMk2SimulationTest.java).

#include <memory>
#include <utility>

#include <gtest/gtest.h>

#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/Error.h"
#include "rocket/TestRockets.h"
#include "simulation/SimulationRunSupport.h"

namespace
{

using QtRocket::BodyTube;
using QtRocket::NoseCone;
using QtRocket::PodSet;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::Simulation;
using QtRocket::TransitionShape;
using QtRocket::Test::JavaTestPreferences;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::testFcid;

// BansheeMk2SimulationTest.testSimulationWithZeroVolumeTailConeSlices: Verifies that zero-volume
// integration slices in a short Power-series tail cone do not contaminate the complete rocket's
// center of mass with NaN.
TEST(BansheeMk2SimulationTest, SimulationWithZeroVolumeTailConeSlices)
{
    const TestEstesAlphaIII alpha;
    Rocket&                 rocket   = *alpha.rocket;
    auto*                   bodyTube = dynamic_cast<BodyTube*>(&rocket.getStage(0)->getChild(1));
    ASSERT_NE(bodyTube, nullptr);

    auto    podSetOwner = std::make_unique<PodSet>();
    PodSet* podSet      = podSetOwner.get();
    podSet->setInstanceCount(1);
    bodyTube->addChild(std::move(podSetOwner));

    auto podNoseCone = std::make_unique<NoseCone>(TransitionShape::HAACK, 0.15, 0.011);
    podNoseCone->setShapeParameter(0.0);
    podSet->addChild(std::move(podNoseCone));

    // These are the dimensions and shape settings that triggered the original failure.
    auto tailCone = std::make_unique<NoseCone>(TransitionShape::POWER, 0.001, 0.011);
    tailCone->setShapeParameter(0.0);
    tailCone->setThickness(0.002);
    tailCone->setFlipped(true);
    podSet->addChild(std::move(tailCone));

    JavaTestPreferences preferences;
    Simulation          simulation(rocket, preferences.store);
    simulation.setFlightConfigurationId(testFcid(0));
    simulation.getOptions().setIsaAtmosphere(true);
    simulation.getOptions().setTimeStep(0.05);
    simulation.getOptions().setRandomSeed(0x3161);

    // assertDoesNotThrow: neither a SimulationException (an error of simulate() here) nor
    // anything else.
    Result<void> result;
    EXPECT_NO_THROW(result = simulation.simulate());
    EXPECT_TRUE(result.has_value()) << (result.has_value() ? "" : result.error().message);
    EXPECT_FALSE(simulation.hasErrors()) << simulation.getStatusDescription();
}

}  // namespace
