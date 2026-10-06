// Ports OpenRocket's MotorPressureCorrectionTest (core/src/test/java/info/openrocket/core/
// simulation/MotorPressureCorrectionTest.java).

#include <cmath>
#include <memory>
#include <numbers>
#include <optional>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/mass/MotorClusterState.h"
#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/AbstractSimulationStepper.h"
#include "QtRocket/simulation/Rk4SimulationStepper.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/MathUtil.h"

namespace
{

using QtRocket::AbstractSimulationStepper;
using QtRocket::AtmosphericConditions;
using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::Coordinate;
using QtRocket::FlightConfiguration;
using QtRocket::MotorClusterState;
using QtRocket::MotorConfiguration;
using QtRocket::Result;
using QtRocket::Rk4SimulationStepper;
using QtRocket::Rocket;
using QtRocket::Simulation;
using QtRocket::SimulationConditions;
using QtRocket::SimulationStatus;
using QtRocket::ThrustCurveMotor;
namespace MathUtil = QtRocket::MathUtil;

/// A motor of the test: `new ThrustCurveMotor.Builder().setDiameter(...).setLength(...)
/// .setTimePoints(...).setThrustPoints(...).setCGPoints(...).build()`.
[[nodiscard]] std::shared_ptr<const ThrustCurveMotor> buildMotor(double diameter, double length,
                                                                 std::vector<double>     time,
                                                                 std::vector<double>     thrust,
                                                                 std::vector<Coordinate> cg)
{
    ThrustCurveMotor::Builder builder;
    builder.setDiameter(diameter)
        .setLength(length)
        .setTimePoints(std::move(time))
        .setThrustPoints(std::move(thrust))
        .setCGPoints(std::move(cg));
    Result<ThrustCurveMotor> built = builder.build();
    if (!built.has_value())
    {
        ADD_FAILURE() << built.error().message;
        return nullptr;
    }
    return std::make_shared<const ThrustCurveMotor>(std::move(*built));
}

// MotorPressureCorrectionTest.testMotorPressureCorrection
TEST(MotorPressureCorrectionTest, MotorPressureCorrection)
{
    const double motorLength   = 0.1;
    const double motorDiameter = 0.05;

    const double nozzle1Diameter = motorDiameter / 2;
    const double nozzle2Diameter = motorDiameter / 4;

    const double time1              = 1;  // both motors are burning
    const double time2              = 3;  // motor2 burns out here
    const double pressureDifference = 1000;

    // calculate expected results
    const double nozzle1Area       = std::numbers::pi * std::pow(nozzle1Diameter / 2, 2);
    const double thrust1Correction = nozzle1Area * pressureDifference;

    const double nozzle2Area       = std::numbers::pi * std::pow(nozzle2Diameter / 2, 2);
    const double thrust2Correction = nozzle2Area * pressureDifference;

    // Just enough rocket to test
    Rocket rocket;

    AxialStage& axialStage = rocket.addChild(std::make_unique<AxialStage>());

    const auto flightConfiguration = std::make_shared<FlightConfiguration>(rocket);

    BodyTube& bodyTube1 = axialStage.addChild(std::make_unique<BodyTube>());

    bodyTube1.setMotorMount(true);
    MotorConfiguration motor1Configuration(bodyTube1, flightConfiguration->getId());

    // motor1 has a larger nozzle, and burns longer
    const std::shared_ptr<const ThrustCurveMotor> motor1 = buildMotor(
        motorDiameter, motorLength, {0, 1, 3, 4}, {0, 2, 3, 0},
        {Coordinate(0.02, 0, 0, motorLength / 2), Coordinate(0.02, 0, 0, motorLength / 2),
         Coordinate(0.02, 0, 0, motorLength / 2), Coordinate(0.03, 0, 0, motorLength / 2)});
    ASSERT_NE(motor1, nullptr);

    motor1Configuration.setMotor(motor1);
    ASSERT_TRUE(motor1Configuration.setNozzleExitDiameter(nozzle1Diameter).has_value());
    flightConfiguration->addMotor(motor1Configuration);

    BodyTube& bodyTube2 = axialStage.addChild(std::make_unique<BodyTube>());

    bodyTube2.setMotorMount(true);
    MotorConfiguration motor2Configuration(bodyTube2, flightConfiguration->getId());

    // motor2 burns shorter, and has a smaller nozzle
    const std::shared_ptr<const ThrustCurveMotor> motor2 = buildMotor(
        motorDiameter, motorLength, {0, 1, 3}, {0, 2, 0},
        {Coordinate(0.02, 0, 0, motorLength / 2), Coordinate(0.02, 0, 0, motorLength / 2),
         Coordinate(0.02, 0, 0, motorLength / 2)});
    ASSERT_NE(motor2, nullptr);

    motor2Configuration.setMotor(motor2);
    ASSERT_TRUE(motor2Configuration.setNozzleExitDiameter(nozzle2Diameter).has_value());
    flightConfiguration->addMotor(motor2Configuration);

    // set up simulation stepper so we can calculate thrust
    Simulation                   simulation(rocket);
    Result<SimulationConditions> made = simulation.getOptions().toSimulationConditions();
    ASSERT_TRUE(made.has_value());
    const auto simulationConditions = std::make_shared<SimulationConditions>(std::move(*made));
    SimulationStatus simulationStatus(flightConfiguration, simulationConditions);

    Rk4SimulationStepper stepper;
    // Java: stepper.store = new DataStore(); stepper.store.flightConditions = new
    // FlightConditions(flightConfiguration);
    AbstractSimulationStepper::DataStore& store = stepper.getStore();
    store                                       = AbstractSimulationStepper::DataStore();
    store.flightConditions.emplace(*flightConfiguration);
    const double standardPressure = AtmosphericConditions::kStandardPressure;

    // at time 1, both motors are active
    simulationStatus.setSimulationTime(time1);
    for (const std::shared_ptr<MotorClusterState>& motorClusterState : simulationStatus.getMotors())
    {
        motorClusterState->ignite(0);
    }

    // At standard pressure, thrust should simply be the value from the thrustcurve
    stepper.calculateFlightConditions(simulationStatus, store);
    ASSERT_TRUE(store.flightConditions.has_value());
    store.flightConditions->getAtmosphericConditions().setPressure(standardPressure);

    EXPECT_NEAR(nozzle1Area + nozzle2Area, store.flightConditions->getThrustingNozzleExitArea(),
                MathUtil::kEpsilon)
        << "thrusting nozzle exit area incorrect";

    EXPECT_NEAR(motor1->getThrust(time1) + motor2->getThrust(time1),
                stepper.calculateThrust(simulationStatus, store), MathUtil::kEpsilon)
        << "Thrust at sea level incorrect";

    // Test correction with both motors active
    stepper.calculateFlightConditions(simulationStatus, store);
    ASSERT_TRUE(store.flightConditions.has_value());
    store.flightConditions->getAtmosphericConditions().setPressure(standardPressure -
                                                                   pressureDifference);

    EXPECT_NEAR(
        motor1->getThrust(time1) + motor2->getThrust(time1) + thrust1Correction + thrust2Correction,
        stepper.calculateThrust(simulationStatus, store), MathUtil::kEpsilon)
        << "Corrected thrust incorrect";

    // at time2, motor2 has burned out
    simulationStatus.setSimulationTime(time2);
    for (const std::shared_ptr<MotorClusterState>& motorClusterState : simulationStatus.getMotors())
    {
        if (MathUtil::equals(motorClusterState->getThrust(time2), 0))
        {
            motorClusterState->burnOut(time2);
        }
    }

    // Test correction with only motor 1 active
    stepper.calculateFlightConditions(simulationStatus, store);
    ASSERT_TRUE(store.flightConditions.has_value());
    store.flightConditions->getAtmosphericConditions().setPressure(standardPressure -
                                                                   pressureDifference);

    // Check nozzle area
    EXPECT_NEAR(nozzle1Area, store.flightConditions->getThrustingNozzleExitArea(),
                MathUtil::kEpsilon)
        << "thrusting nozzle exit area incorrect";

    EXPECT_NEAR(motor1->getThrust(time2) + thrust1Correction,
                stepper.calculateThrust(simulationStatus, store), MathUtil::kEpsilon)
        << "Corrected thrust incorrect";
}

}  // namespace
