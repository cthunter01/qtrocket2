// The integration cases of OpenRocket's TumbleDetectorTest
// (core/src/test/java/info/openrocket/core/simulation/TumbleDetectorTest.java): they confirm the
// behaviour that issue #3183 of OpenRocket is about, on a real airframe in a real crosswind. (The
// cases that drive the detector directly are in TumbleDetectorTests.cpp.)
//
// The simulations run under the preferences of OpenRocket's test set-up (SimulationRunSupport.h),
// with the random seed of the options fixed (Java: whatever seed the new options drew). The wind
// is DeterministicWind's, as in Java.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStepperMethod.h"
#include "QtRocket/util/Error.h"
#include "rocket/TestRockets.h"
#include "simulation/DeterministicWind.h"
#include "simulation/SimulationRunSupport.h"

namespace
{

using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::FinSet;
using QtRocket::FlightData;
using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::FlightDataTypeId;
using QtRocket::FlightEvent;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Simulation;
using QtRocket::SimulationAbort;
using QtRocket::SimulationStepperMethod;
using QtRocket::Warning;
using QtRocket::Test::DeterministicWind;
using QtRocket::Test::JavaTestPreferences;
using QtRocket::Test::simulatedData;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::testFcid;

/// TumbleDetectorTest.crosswindSimulation(), with the random seed fixed.
[[nodiscard]] std::unique_ptr<Simulation> crosswindSimulation(Rocket&                 rocket,
                                                              JavaTestPreferences&    preferences,
                                                              double                  windSpeed,
                                                              SimulationStepperMethod method)
{
    auto simulation = std::make_unique<Simulation>(rocket, preferences.store);
    simulation->setFlightConfigurationId(testFcid(0));
    simulation->getOptions().setIsaAtmosphere(true);
    simulation->getOptions().setTimeStep(0.05);
    simulation->getOptions().setSimulationStepperMethodChoice(method);
    simulation->getOptions().setLaunchRodLength(1.0);
    simulation->getOptions().setWindSpeedAverage(windSpeed);
    simulation->getOptions().setWindTurbulenceIntensity(0.0);
    simulation->getOptions().setRandomSeed(0);  // Java: left to chance
    return simulation;
}

/// Runs @p simulation in a steady crosswind of @p windSpeed (Java:
/// simulation.simulate(DeterministicWind.steady(windSpeed))) and expects the run to succeed.
void simulateInSteadyWind(Simulation& simulation, double windSpeed)
{
    const Result<void> result = simulation.simulate({DeterministicWind::steady(windSpeed)});
    EXPECT_TRUE(result.has_value()) << (result.has_value() ? "" : result.error().message);
}

/// TumbleDetectorTest.removeFins(): takes the fin sets out of @p component, and out of the body
/// tubes and stages below it.
void removeFins(RocketComponent& component)
{
    // getChildren() is a new list (Java: List.copyOf(component.getChildren())): the loop removes
    // children.
    const std::vector<RocketComponent*> children = component.getChildren();
    for (RocketComponent* child : children)
    {
        if (dynamic_cast<FinSet*>(child) != nullptr)
        {
            static_cast<void>(component.removeChild(child));
        }
        else if (dynamic_cast<BodyTube*>(child) != nullptr ||
                 dynamic_cast<AxialStage*>(child) != nullptr)
        {
            removeFins(*child);
        }
    }
}

/// Whether the events of @p branch hold a SIM_ABORT with the cause TUMBLE_UNDER_THRUST.
[[nodiscard]] bool abortedAsTumblingUnderThrust(const FlightDataBranch& branch)
{
    return std::ranges::any_of(branch.getEvents(), [](const FlightEvent& event) {
        return event.getType() == FlightEvent::Type::SIM_ABORT && event.getAbort() != nullptr &&
               event.getAbort()->cause() == SimulationAbort::Cause::TUMBLE_UNDER_THRUST;
    });
}

/// Whether the events of @p branch hold a TUMBLE event.
[[nodiscard]] bool tumbled(const FlightDataBranch& branch)
{
    return std::ranges::any_of(branch.getEvents(), [](const FlightEvent& event) {
        return event.getType() == FlightEvent::Type::TUMBLE;
    });
}

/// The largest altitude of the first branch of what @p simulation has just simulated (Java:
/// getSimulatedData().getBranch(0).getMaximum(FlightDataType.TYPE_ALTITUDE)); NaN, and a
/// failure, without such a branch.
[[nodiscard]] double apogeeOf(const Simulation& simulation)
{
    const FlightData& data = simulatedData(simulation);
    if (data.getBranchCount() == 0)
    {
        ADD_FAILURE() << "the simulation has no branch";
        return std::numeric_limits<double>::quiet_NaN();
    }
    return data.getBranch(0).getMaximum(FlightDataType::builtin(FlightDataTypeId::TYPE_ALTITUDE));
}

/// The integration cases, for each stepper method (Java: @EnumSource(SimulationStepperMethod)).
class TumbleDetectorSimulationTest : public ::testing::TestWithParam<SimulationStepperMethod>
{ };

// TumbleDetectorTest.testSteadyCrosswindDoesNotAbort: Issue #3183. A steady 6 m/s crosswind --
// no gust, no turbulence, nothing random anywhere in the scenario -- aborted every run under the
// previous criterion. The wind is supplied by a listener rather than by the bundled pink noise
// model, so that the scenario is written down rather than sampled.
TEST_P(TumbleDetectorSimulationTest, SteadyCrosswindDoesNotAbort)
{
    const TestEstesAlphaIII           alpha;
    JavaTestPreferences               preferences;
    const std::unique_ptr<Simulation> simulation =
        crosswindSimulation(*alpha.rocket, preferences, 6.0, GetParam());
    simulateInSteadyWind(*simulation, 6.0);

    const FlightData& data = simulatedData(*simulation);
    ASSERT_GE(data.getBranchCount(), 1U);

    EXPECT_FALSE(abortedAsTumblingUnderThrust(data.getBranch(0)))
        << "a stable rocket in a steady crosswind must not abort as tumbling under thrust";
}

// TumbleDetectorTest.testRecoveredFlightReachesExpectedApogee: The recovered flights must be
// healthy ones, not aborts traded for nonsense trajectories: the 6 m/s flight must reach an
// apogee close to the still-air one.
TEST_P(TumbleDetectorSimulationTest, RecoveredFlightReachesExpectedApogee)
{
    JavaTestPreferences preferences;

    const TestEstesAlphaIII           calmAlpha;
    const std::unique_ptr<Simulation> calm =
        crosswindSimulation(*calmAlpha.rocket, preferences, 0.0, GetParam());
    simulateInSteadyWind(*calm, 0.0);
    const double calmApogee = apogeeOf(*calm);

    const TestEstesAlphaIII           windyAlpha;
    const std::unique_ptr<Simulation> windy =
        crosswindSimulation(*windyAlpha.rocket, preferences, 6.0, GetParam());
    simulateInSteadyWind(*windy, 6.0);
    const double windyApogee = apogeeOf(*windy);

    EXPECT_TRUE(std::abs(windyApogee - calmApogee) < 0.10 * calmApogee)
        << "recovered flight apogee " << windyApogee << " m should be close to the " << calmApogee
        << " m reached in still air";
}

// TumbleDetectorTest.testUnstableRocketIsStillDetected: The complementary guarantee: removing
// the fins makes the rocket genuinely unstable, and that must still be detected.
TEST_P(TumbleDetectorSimulationTest, UnstableRocketIsStillDetected)
{
    const TestEstesAlphaIII alpha;
    removeFins(*alpha.rocket);

    JavaTestPreferences               preferences;
    const std::unique_ptr<Simulation> simulation =
        crosswindSimulation(*alpha.rocket, preferences, 2.0, GetParam());
    simulateInSteadyWind(*simulation, 2.0);

    const FlightData& data = simulatedData(*simulation);
    ASSERT_GE(data.getBranchCount(), 1U);
    const FlightDataBranch& branch = data.getBranch(0);

    const bool detected = tumbled(branch) || abortedAsTumblingUnderThrust(branch);

    EXPECT_TRUE(detected) << "a finless rocket must still be detected as tumbling";
}

/// The rows of @p branch whose wind velocity is not the one a DeterministicWind(@p baseSpeed,
/// @p gustSpeed, @p gustStart, @p gustEnd) gives at the time of the row (rows without a wind
/// velocity, such as the last one of a flight, are left out).
[[nodiscard]] std::vector<std::string> rowsWithAnotherWind(const FlightDataBranch& branch,
                                                           double baseSpeed, double gustSpeed,
                                                           double gustStart, double gustEnd)
{
    std::vector<std::string>   rows;
    const std::vector<double>* time =
        branch.getView(FlightDataType::builtin(FlightDataTypeId::TYPE_TIME));
    const std::vector<double>* wind =
        branch.getView(FlightDataType::builtin(FlightDataTypeId::TYPE_WIND_VELOCITY));
    if (time == nullptr || wind == nullptr || time->size() != wind->size())
    {
        rows.emplace_back("the branch has no wind velocity for every time");
        return rows;
    }
    for (std::size_t i = 0; i < time->size(); i++)
    {
        const double t        = (*time)[i];
        const double expected = baseSpeed + ((t >= gustStart && t < gustEnd) ? gustSpeed : 0);
        if (!std::isnan((*wind)[i]) && (*wind)[i] != expected)
        {
            rows.push_back(std::format("row {} (t = {}): {}, not {}", i, t, (*wind)[i], expected));
        }
    }
    return rows;
}

// Beyond the Java class: the wind such a flight records is the listener's, record by record,
// through a gust too (the simulation's own wind model, set to another speed, is never asked),
// and the listener, a system listener, leaves no warning that listeners affected the simulation.
TEST_P(TumbleDetectorSimulationTest, TheWindOfTheFlightIsTheListeners)
{
    const TestEstesAlphaIII           alpha;
    JavaTestPreferences               preferences;
    const std::unique_ptr<Simulation> simulation =
        crosswindSimulation(*alpha.rocket, preferences, 1.0, GetParam());
    const Result<void> result =
        simulation->simulate({std::make_shared<DeterministicWind>(2.0, 4.0, 0.5, 1.0)});
    ASSERT_TRUE(result.has_value()) << result.error().message;

    const FlightData& data = simulatedData(*simulation);
    ASSERT_GE(data.getBranchCount(), 1U);
    const FlightDataBranch& branch = data.getBranch(0);
    EXPECT_EQ(rowsWithAnotherWind(branch, 2.0, 4.0, 0.5, 1.0), std::vector<std::string>{});
    EXPECT_EQ(branch.getMaximum(FlightDataType::builtin(FlightDataTypeId::TYPE_WIND_VELOCITY)),
              6.0);
    EXPECT_EQ(branch.getMinimum(FlightDataType::builtin(FlightDataTypeId::TYPE_WIND_VELOCITY)),
              2.0);
    EXPECT_NE(branch.getFirstEvent(FlightEvent::Type::GROUND_HIT), nullptr);
    EXPECT_FALSE(data.getWarningSet().contains(Warning::kListenersAffected))
        << data.getWarningSet().toString();
}

INSTANTIATE_TEST_SUITE_P(TumbleDetectorTest, TumbleDetectorSimulationTest,
                         ::testing::ValuesIn(QtRocket::kAllSimulationStepperMethods),
                         QtRocket::Test::stepperMethodTestName);

}  // namespace
