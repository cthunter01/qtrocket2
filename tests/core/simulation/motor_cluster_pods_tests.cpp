// Ports OpenRocket's MotorClusterPodsTest (core/src/test/java/info/openrocket/core/simulation/
// MotorClusterPodsTest.java).

#include <memory>
#include <numbers>
#include <optional>
#include <utility>

#include <gtest/gtest.h>

#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/mass/MotorClusterState.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/rocket/ComponentAssembly.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/AbstractSimulationStepper.h"
#include "QtRocket/simulation/Rk4SimulationStepper.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/MathUtil.h"
#include "rocket/TestRockets.h"
#include "simulation/StepperTestSupport.h"

namespace
{

using QtRocket::AbstractSimulationStepper;
using QtRocket::ComponentAssembly;
using QtRocket::FlightConfiguration;
using QtRocket::IgnitionEvent;
using QtRocket::MotorClusterState;
using QtRocket::Result;
using QtRocket::Rk4SimulationStepper;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Simulation;
using QtRocket::SimulationConditions;
using QtRocket::SimulationStatus;
using QtRocket::Test::burnOutAllMotors;
using QtRocket::Test::igniteAllMotors;
using QtRocket::Test::setNozzleExitDiameters;
using QtRocket::Test::TestClusterPods;
using QtRocket::Test::TestMultiStageEventTestRocket;
namespace MathUtil = QtRocket::MathUtil;

constexpr double kC6NozzleExitDiameter = 0.010;

/// The conditions of `simulation.getOptions().toSimulationConditions()` with
/// `simulationConditions.setSimulation(simulation)`.
[[nodiscard]] std::shared_ptr<SimulationConditions> conditionsOf(Simulation& simulation)
{
    Result<SimulationConditions> made = simulation.getOptions().toSimulationConditions();
    if (!made.has_value())
    {
        ADD_FAILURE() << made.error().message;
        return std::make_shared<SimulationConditions>();
    }
    auto conditions = std::make_shared<SimulationConditions>(std::move(*made));
    conditions->setSimulation(&simulation);
    return conditions;
}

/// The nozzle exit diameter of a C6, whatever the motor.
[[nodiscard]] double c6NozzleExitDiameter(double /*motorDiameter*/)
{
    return kC6NozzleExitDiameter;
}

/// Java: `motorDiameter > 0.015 ? 0.010 : 0.008`.
[[nodiscard]] double nozzleExitDiameterBySize(double motorDiameter)
{
    return motorDiameter > 0.015 ? 0.010 : 0.008;
}

/// The thrusting nozzle exit area of @p assembly in the store's flight conditions.
[[nodiscard]] double nozzleArea(const AbstractSimulationStepper::DataStore& store,
                                const RocketComponent&                      assembly)
{
    const auto* componentAssembly = dynamic_cast<const ComponentAssembly*>(&assembly);
    if (componentAssembly == nullptr || !store.flightConditions.has_value())
    {
        ADD_FAILURE() << assembly.getName() << " is not an assembly, or there are no conditions";
        return -1;
    }
    return store.flightConditions->getThrustingNozzleExitArea(*componentAssembly);
}

/// The total thrusting nozzle exit area of the store's flight conditions.
[[nodiscard]] double nozzleArea(const AbstractSimulationStepper::DataStore& store)
{
    if (!store.flightConditions.has_value())
    {
        ADD_FAILURE() << "there are no flight conditions";
        return -1;
    }
    return store.flightConditions->getThrustingNozzleExitArea();
}

/// The first half of testThrustingNozzleExitAreaTracksMotorAndStageState: without nozzle
/// diameters no motor contributes, armed or thrusting.
void expectNoNozzleAreaWithoutKnownNozzles(
    const FlightConfiguration&                   configuration,
    const std::shared_ptr<SimulationConditions>& simulationConditions)
{
    Rk4SimulationStepper                 stepper;
    AbstractSimulationStepper::DataStore store;
    SimulationStatus status(std::make_shared<FlightConfiguration>(configuration.clone()),
                            simulationConditions);
    stepper.calculateFlightConditions(status, store);
    EXPECT_NEAR(0, nozzleArea(store), MathUtil::kEpsilon)
        << "Armed motors must not reduce base drag before ignition";

    igniteAllMotors(status, 0);
    status.setSimulationTime(0.4);
    stepper.calculateFlightConditions(status, store);
    EXPECT_NEAR(0, nozzleArea(store), MathUtil::kEpsilon)
        << "Unknown nozzle geometry must preserve the legacy base-drag calculation";
}

/// Java: `for (MotorClusterState motorState : status.getMotors()) if
/// (motorState.getIgnitionEvent() == IgnitionEvent.LAUNCH) motorState.ignite(0);`
void igniteMotorsOfLaunch(const SimulationStatus& status)
{
    for (const std::shared_ptr<MotorClusterState>& motorState : status.getMotors())
    {
        if (motorState->getIgnitionEvent() == IgnitionEvent::LAUNCH)
        {
            motorState->ignite(0);
        }
    }
}

// MotorClusterPodsTest.testMotorClusterPods
TEST(MotorClusterPodsTest, MotorClusterPods)
{
    const TestClusterPods      pods;
    const FlightConfiguration& config = pods.rocket->getFlightConfigurationByIndex(0);

    // The status holds a clone of the configuration, on which the test sets the stages (Java:
    // the status holds the rocket's own configuration, which the test changes).
    SimulationStatus status(std::make_shared<FlightConfiguration>(config.clone()),
                            std::make_shared<SimulationConditions>());
    igniteAllMotors(status, 0.0);

    Rk4SimulationStepper stepper;
    stepper.getStore() = AbstractSimulationStepper::DataStore();
    stepper.getStore().flightConditions.emplace(status.getConfiguration());

    status.setSimulationTime(0.4);
    // Thrust of a single C6 at time 0.4 is 5 (from TestRockets.java, not actual thrustcurve)
    const double c6Thrust = 5.0;

    // Two motors in sustainer
    status.getConfiguration().setOnlyStage(0);
    double thrust = stepper.calculateThrust(status, stepper.getStore());
    EXPECT_NEAR(2.0 * c6Thrust, thrust, MathUtil::kEpsilon) << "Sustainer thrust incorrect";

    // Three side boosters with four motors in each
    status.getConfiguration().setOnlyStage(1);
    thrust = stepper.calculateThrust(status, stepper.getStore());
    EXPECT_NEAR(12.0 * c6Thrust, thrust, MathUtil::kEpsilon) << "side booster thrust incorrect";

    // All 14 motors now
    status.getConfiguration().setAllStages();
    thrust = stepper.calculateThrust(status, stepper.getStore());
    EXPECT_NEAR(14.0 * c6Thrust, thrust, MathUtil::kEpsilon) << "Total thrust incorrect";
}

// MotorClusterPodsTest.testThrustingNozzleExitAreaTracksMotorAndStageState: flight conditions
// include only the nozzle exit area of motor clusters that are both on active stages and
// currently thrusting.
//
// One deviation in the set-up: Java sets the nozzle exit diameters after the status is made,
// and its motor states see them, because they refer to the mounts' motor configurations. A
// MotorClusterState here holds a copy of its configuration (see MotorClusterState, deviations),
// so the status of the second half is made after the diameters are set.
TEST(MotorClusterPodsTest, ThrustingNozzleExitAreaTracksMotorAndStageState)
{
    const TestClusterPods pods;
    Rocket&               rocket                = *pods.rocket;
    FlightConfiguration&  selectedConfiguration = rocket.getFlightConfigurationByIndex(0);
    Simulation            simulation(rocket);
    simulation.setFlightConfigurationId(selectedConfiguration.getFlightConfigurationId());
    const std::shared_ptr<SimulationConditions> simulationConditions = conditionsOf(simulation);
    FlightConfiguration&                        configuration = simulation.getActiveConfiguration();
    Rk4SimulationStepper                        stepper;
    AbstractSimulationStepper::DataStore        store;
    const RocketComponent&                      sustainerAssembly = rocket.getChild(0);
    const RocketComponent& sideBoosterAssembly = rocket.getChild(0).getChild(0).getChild(1);

    expectNoNozzleAreaWithoutKnownNozzles(configuration, simulationConditions);

    setNozzleExitDiameters(configuration, c6NozzleExitDiameter);
    SimulationStatus status(std::make_shared<FlightConfiguration>(configuration.clone()),
                            simulationConditions);
    igniteAllMotors(status, 0);
    status.setSimulationTime(0.4);
    stepper.calculateFlightConditions(status, store);
    const double singleNozzleExitArea =
        std::numbers::pi * MathUtil::pow2(kC6NozzleExitDiameter / 2);
    EXPECT_NEAR(14 * singleNozzleExitArea, nozzleArea(store), MathUtil::kEpsilon)
        << "All fourteen thrusting motors should contribute their nozzle exit area";
    EXPECT_NEAR(2 * singleNozzleExitArea, nozzleArea(store, sustainerAssembly), MathUtil::kEpsilon)
        << "The two sustainer motors should belong only to the sustainer wake";
    EXPECT_NEAR(12 * singleNozzleExitArea, nozzleArea(store, sideBoosterAssembly),
                MathUtil::kEpsilon)
        << "The twelve side-booster motors should belong only to the booster wakes";

    status.getConfiguration().setOnlyStage(0);
    stepper.calculateFlightConditions(status, store);
    EXPECT_NEAR(2 * singleNozzleExitArea, nozzleArea(store), MathUtil::kEpsilon)
        << "Motors on inactive booster stages must not affect the active rocket base";
    EXPECT_NEAR(0, nozzleArea(store, sideBoosterAssembly), MathUtil::kEpsilon)
        << "An inactive booster assembly must not retain stale nozzle area";

    burnOutAllMotors(status, 1);
    stepper.calculateFlightConditions(status, store);
    EXPECT_NEAR(0, nozzleArea(store), MathUtil::kEpsilon)
        << "Nozzle area correction must end at burnout";
}

// MotorClusterPodsTest.testDifferentMotorSizesRemainInTheirOwnWakes: simultaneously burning
// motors of different diameters remain associated with their own core and side-booster wakes.
TEST(MotorClusterPodsTest, DifferentMotorSizesRemainInTheirOwnWakes)
{
    const TestMultiStageEventTestRocket fixture;
    Rocket&                             rocket                = *fixture.rocket;
    FlightConfiguration&                selectedConfiguration = rocket.getSelectedConfiguration();
    setNozzleExitDiameters(selectedConfiguration, nozzleExitDiameterBySize);
    Simulation simulation(rocket);
    simulation.setFlightConfigurationId(selectedConfiguration.getFlightConfigurationId());
    const std::shared_ptr<SimulationConditions> simulationConditions = conditionsOf(simulation);
    const FlightConfiguration&                  configuration = simulation.getActiveConfiguration();
    SimulationStatus status(std::make_shared<FlightConfiguration>(configuration.clone()),
                            simulationConditions);

    igniteMotorsOfLaunch(status);

    Rk4SimulationStepper                 stepper;
    AbstractSimulationStepper::DataStore store;
    stepper.calculateFlightConditions(status, store);

    const RocketComponent& sustainerAssembly   = rocket.getChild(0);
    const RocketComponent& coreBoosterAssembly = rocket.getChild(1);
    const RocketComponent& sideBoosterAssembly = rocket.getChild(1).getChild(0).getChild(1);
    const double           coreNozzleExitArea  = std::numbers::pi * MathUtil::pow2(0.010 / 2);
    const double           sideNozzleExitArea  = 2 * std::numbers::pi * MathUtil::pow2(0.008 / 2);

    EXPECT_NEAR(0, nozzleArea(store, sustainerAssembly), MathUtil::kEpsilon)
        << "The unignited sustainer motor must not affect any powered base area";
    EXPECT_NEAR(coreNozzleExitArea, nozzleArea(store, coreBoosterAssembly), MathUtil::kEpsilon)
        << "The 18 mm center motor should belong to the core-booster wake";
    EXPECT_NEAR(sideNozzleExitArea, nozzleArea(store, sideBoosterAssembly), MathUtil::kEpsilon)
        << "Both 13 mm side motors should belong to the side-booster wakes";
    EXPECT_NEAR(coreNozzleExitArea + sideNozzleExitArea, nozzleArea(store), MathUtil::kEpsilon)
        << "The reported total should remain the sum of all independent wakes";
}

}  // namespace
