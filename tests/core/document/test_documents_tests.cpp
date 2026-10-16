#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/FlightConfigurableParameterSet.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/Coordinate.h"
#include "document/TestDocuments.h"
#include "simulation/SimulationRunSupport.h"

// The document makers of TestRockets (document/TestDocuments.h): what their documents hold that
// no save shows. That each document is saved as OpenRocket saves the document of the same maker
// is goldens/test_documents_golden_tests.cpp's (TestDocumentsGolden).

namespace
{

using QtRocket::Coordinate;
using QtRocket::FlightConfigurationId;
using QtRocket::InnerTube;
using QtRocket::OpenRocketDocument;
using QtRocket::Simulation;
using QtRocket::SimulationAbort;
using QtRocket::Test::JavaTestPreferences;

}  // namespace

// TestRockets.getTestMotor(). The manufacturer "A" is one of AeroTech's codes
// (Manufacturer.getManufacturer("A") gives AeroTech in Java too).
TEST(TestDocuments, TheTestMotor)
{
    const std::shared_ptr<const QtRocket::ThrustCurveMotor> motor = QtRocket::Test::getTestMotor();
    EXPECT_EQ(motor->getManufacturer().getSimpleName(), "AeroTech");
    EXPECT_EQ(motor->getDesignation(), "F12X");
    EXPECT_EQ(motor->getDescription(), "Desc");
    EXPECT_EQ(motor->getCaseInfo(), "info");
    EXPECT_EQ(motor->getMotorType(), QtRocket::Motor::Type::UNKNOWN);
    EXPECT_TRUE(motor->getStandardDelays().empty());
    EXPECT_EQ(motor->getDiameter(), 0.024);
    EXPECT_EQ(motor->getLength(), 0.07);
    EXPECT_EQ(motor->getTimePoints(), (std::vector<double>{0, 1, 2}));
    EXPECT_EQ(motor->getThrustPoints(), (std::vector<double>{0, 1, 0}));
    EXPECT_EQ(motor->getCGPoints(),
              (std::vector<Coordinate>{Coordinate::kNul, Coordinate::kNul, Coordinate::kNul}));
    EXPECT_EQ(motor->getDigest(), "digestA");
}

// What no save shows. makeTestRocket_v104_withMotor() gives the motor to the default
// configuration, which keeps no motor (FlightConfigurableParameterSet.set() of the default id
// does nothing). makeTestRocket_v106_withMotorMountIgnitionConfig() gives it to an id of no
// configuration of the rocket, which the mount keeps with the ignition delay set after it was
// stored. The preset of makeTestRocket_v105_withComponentPreset() has OpenRocket's digest.
// makeTestRocket_v106_withStageSeparationConfig() gives the first stage a separation 3 s after
// its event in the configuration "3SecondDelay", which is no configuration of the rocket either,
// so that OpenRocket does not write it.
TEST(TestDocuments, WhatTheMakersHoldBesideTheirSaves)
{
    const std::unique_ptr<OpenRocketDocument> withMotor =
        QtRocket::Test::makeTestRocketV104WithMotor();
    const auto& mount =
        dynamic_cast<const InnerTube&>(withMotor->getRocket().getStage(0)->getChild(0).getChild(0));
    EXPECT_TRUE(mount.isMotorMount());
    EXPECT_FALSE(mount.getMotorConfig(FlightConfigurationId::defaultValueId()).hasMotor());
    const JavaTestPreferences preferences;
    EXPECT_EQ(withMotor->getRocket().getSelectedConfiguration().getNameRaw(preferences.store),
              "F12X");

    const std::unique_ptr<OpenRocketDocument> ignition =
        QtRocket::Test::makeTestRocketV106WithMotorMountIgnitionConfig();
    const auto& ignitionMount =
        dynamic_cast<const InnerTube&>(ignition->getRocket().getStage(0)->getChild(0).getChild(0));
    const std::vector<FlightConfigurationId> ids =
        ignitionMount.getMotorConfigurationSet().getIds();
    ASSERT_EQ(ids.size(), 1U);
    EXPECT_FALSE(ignition->getRocket().containsFlightConfigurationId(ids.front()));
    const QtRocket::MotorConfiguration& config = ignitionMount.getMotorConfig(ids.front());
    ASSERT_TRUE(config.hasMotor());
    EXPECT_EQ(config.getMotor()->getDigest(), "digestA");
    EXPECT_EQ(config.getIgnitionDelay(), 2.0);

    const std::unique_ptr<OpenRocketDocument> preset =
        QtRocket::Test::makeTestRocketV105WithComponentPreset();
    const QtRocket::ComponentPreset* bodyTubePreset =
        preset->getRocket().getStage(0)->getChild(0).getPresetComponent();
    ASSERT_NE(bodyTubePreset, nullptr);
    EXPECT_EQ(bodyTubePreset->getDigest(), QtRocket::Test::kV105PresetDigest);

    const std::unique_ptr<OpenRocketDocument> separation =
        QtRocket::Test::makeTestRocketV106WithStageSeparationConfig();
    const auto& separations = separation->getRocket().getStage(0)->getSeparationConfigurations();
    const FlightConfigurationId threeSecondDelay =
        FlightConfigurationId::fromString("3SecondDelay");
    const std::vector<FlightConfigurationId> separationIds = separations.getIds();
    ASSERT_EQ(separationIds.size(), 1U);
    EXPECT_EQ(separationIds.front().toString(), threeSecondDelay.toString());
    EXPECT_EQ(separations.get(threeSecondDelay).getSeparationDelay(), 3.0);
    EXPECT_FALSE(separation->getRocket().containsFlightConfigurationId(threeSecondDelay));
}

// The two simulations of makeTestRocket_for_estimateFileSize() ran and aborted at once: the
// rocket has no motor.
TEST(TestDocuments, TheSimulationsOfTheEstimateDocumentAborted)
{
    JavaTestPreferences                       preferences;
    const std::unique_ptr<OpenRocketDocument> document =
        QtRocket::Test::makeTestRocketForEstimateFileSize(preferences.store);
    ASSERT_EQ(document->getSimulationCount(), 2U);
    const std::vector<SimulationAbort::Cause> noMotors{SimulationAbort::Cause::NO_MOTORS_DEFINED};
    Simulation&                               first = *document->getSimulation(0);
    EXPECT_EQ(first.getStatus(), Simulation::Status::ABORTED);
    EXPECT_EQ(QtRocket::Test::abortCauses(QtRocket::Test::simulatedData(first), 0), noMotors);
    Simulation& second = *document->getSimulation(1);
    EXPECT_EQ(second.getStatus(), Simulation::Status::ABORTED);
    EXPECT_EQ(QtRocket::Test::abortCauses(QtRocket::Test::simulatedData(second), 0), noMotors);
}
