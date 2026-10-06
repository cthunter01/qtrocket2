// Whole flights of the engine against OpenRocket's, at a time step at which OpenRocket's own
// results are reproducible.
//
// The golden simulations (tests/data/goldens, time step 0.05 s) cannot be compared to many
// digits: at that step the Runge-Kutta integration of the pitch oscillation of a small rocket is
// at the edge of its stability, the step size control then amplifies the last bit of any value
// about 1e11 times within the first two seconds of the flight, and OpenRocket's own result
// depends on the hash order of its component ids (the [C6-5] flight of the Estes Alpha III has
// 796, 803 or 817 records and an apogee between 275.358 m and 275.361 m from one start of the
// JVM to the next; a launch latitude changed by 1e-6 degrees gives 798 to 848 records and
// 275.327 m to 275.362 m). At a time step of 0.005 s the oscillation is integrated stably, and
// six OpenRocket runs with six sets of component ids agree to 2.5e-8 (relative) in every value
// pinned here.
//
// The pins are the output of probes/tier8b-engine/SensitivityProbe.java
// ('./run.sh SensitivityProbe makeEstesAlphaIII 4 dt=0.005' and '... makeBeta 2 dt=0.005'): the
// options of the golden simulations (a steady wind of 2 m/s, the rod into the wind, the random
// seed 0) with the time step 0.005 s, and the pitch/yaw jitter removed by the two listeners of
// the golden harness (JitterRemoval.h).

#include <cmath>
#include <cstddef>
#include <format>
#include <iterator>
#include <memory>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/models/GravityModelType.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStepperMethod.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "rocket/TestRockets.h"
#include "simulation/JitterRemoval.h"
#include "simulation/SimulationRunSupport.h"

namespace
{

using QtRocket::FlightData;
using QtRocket::FlightDataBranch;
using QtRocket::FlightEvent;
using QtRocket::GeodeticComputationStrategy;
using QtRocket::GravityModelType;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::Simulation;
using QtRocket::SimulationListener;
using QtRocket::SimulationOptions;
using QtRocket::SimulationStepperMethod;
using QtRocket::Test::JitterRemoval;
using QtRocket::Test::simulatedData;
using QtRocket::Test::TestBeta;
using QtRocket::Test::TestEstesAlphaIII;

using Type = FlightEvent::Type;

/// The relative tolerance of a value at the end of a stably integrated flight. Measured: six
/// OpenRocket runs differ from each other by at most 2.5e-8 (the deployment velocity; 7e-9 in
/// the altitudes and times), and this port (glibc) differs from the pinned run by at most 6e-9.
/// The tolerance is 40 times OpenRocket's own spread, to leave room for the mathematical
/// functions of other platforms, and a hundredth of the plan's tolerance for an apogee.
constexpr double kStableTolerance = 1e-6;

/// The tolerances of a branch whose flight is not reproducible in OpenRocket itself, the
/// tumbling booster of the two-stage flight: when it starts to tumble is decided by a threshold
/// (TumbleDetector), and the flight from then on depends on the step at which it did.
/// - Event times and the time to the optimum altitude: 0.05 s. Measured: six OpenRocket runs
///   differ by up to 5e-3 s (the ground hit); this port differs from the pinned run by up to
///   7.3e-5 s, and by up to 3.1e-3 s under the one-ulp libm shim (six patterns).
/// - The optimum altitude: relative 1e-3. Measured: 2.9e-6 from the pin, and up to 1.23e-4
///   under the shim.
/// - The number of records: 1 %. Measured: 1022 in every run here.
/// What they are for: a tumble drag coefficient that is 10 % too high moves the ground hit by
/// 0.93 s, the apogee by 0.053 s and the optimum altitude by 4.1e-3 (and the number of records
/// by 2, which is why the event times and not the records have to catch it).
constexpr double kUnstableTimeTolerance     = 0.05;
constexpr double kUnstableAltitudeTolerance = 1e-3;
constexpr double kUnstableRowsTolerance     = 0.01;

/// An event of a pinned run: its type and its time.
struct EventPin
{
    Type   type;
    double time;

    constexpr EventPin(Type eventType, double eventTime) : type(eventType), time(eventTime) { }
};

/// A branch of a pinned run. In a branch whose flight is not reproducible in OpenRocket itself
/// (@p stable false: a tumbling booster) the events from @p stableEvents on, the optimum
/// altitude, the time to it and the number of records are compared with the looser tolerances
/// above; the events before (those the motors time) with the stable tolerance.
struct BranchPin
{
    std::string           name;
    std::size_t           rows;
    double                optimumAltitude;
    double                timeToOptimumAltitude;
    std::vector<EventPin> events;
    std::size_t           stableEvents;
    bool                  stable;
};

/// What OpenRocket's FlightData hold after a pinned run.
struct RunPin
{
    std::string            configuration;
    long                   replacements;
    double                 maxAltitude;
    double                 maxVelocity;
    double                 timeToApogee;
    double                 flightTime;
    double                 deploymentVelocity;
    double                 optimumDelay;
    double                 maxAcceleration;
    double                 maxMachNumber;
    double                 groundHitVelocity;
    double                 launchRodVelocity;
    std::vector<BranchPin> branches;
};

/// SensitivityProbe's options: those of the golden simulations, with the time step @p timeStep.
void setGoldenOptions(SimulationOptions& o, double timeStep)
{
    o.setLaunchRodLength(1.0);
    o.getAverageWindModel().setAverage(2.0);
    o.getAverageWindModel().setStandardDeviation(0);
    o.getAverageWindModel().setDirection(std::numbers::pi / 2);
    o.setLaunchIntoWind(true);
    o.setLaunchAltitude(0);
    o.setLaunchLatitude(28.61);
    o.setLaunchLongitude(-80.6);
    o.setGeodeticComputation(GeodeticComputationStrategy::SPHERICAL);
    o.setIsaAtmosphere(true);
    o.setTimeStep(timeStep);
    o.setMaxSimulationTime(1200);
    o.setMaximumStepAngle(3 * std::numbers::pi / 180);
    o.setRandomSeedFixed(true);
    o.setRandomSeed(0);
    o.setGravityModelType(GravityModelType::WGS);
    o.setSimulationStepperMethodChoice(SimulationStepperMethod::RK4);
    o.setRecoverySpeedWarning(20.0);
    o.setDrogueLowSpeedWarning(3.048);
    o.setRecoveryDrogueMainHighSpeedWarning(30.48);
    o.setRecoveryDrogueMainLowSpeedWarning(15.24);
}

/// Notes in @p mismatches when @p actual is not @p pinned within the stable tolerance.
void compare(std::vector<std::string>& mismatches, std::string_view what, double actual,
             double pinned)
{
    const bool same = (std::isnan(actual) && std::isnan(pinned)) ||
                      std::abs(actual - pinned) <= kStableTolerance * std::abs(pinned);
    if (!same)
    {
        mismatches.push_back(std::format("{}: {} (OpenRocket: {}; relative difference {:.3e})",
                                         what, actual, pinned,
                                         std::abs(actual - pinned) / std::abs(pinned)));
    }
}

/// Notes in @p mismatches when the time @p actual is further than kUnstableTimeTolerance from
/// @p pinned.
void compareUnstableTime(std::vector<std::string>& mismatches, std::string_view what, double actual,
                         double pinned)
{
    // Written so that a NaN fails.
    if (!(std::abs(actual - pinned) <= kUnstableTimeTolerance))
    {
        mismatches.push_back(std::format("{}: {} s (OpenRocket: {} s; more than {} s apart)", what,
                                         actual, pinned, kUnstableTimeTolerance));
    }
}

/// The differences between a branch that is not reproducible and its pin @p pin, beyond the
/// events: the number of records, the optimum altitude and the time to it.
void compareUnstableBranch(std::vector<std::string>& mismatches, const FlightDataBranch& branch,
                           const BranchPin& pin)
{
    const auto rows       = static_cast<double>(branch.getLength());
    const auto pinnedRows = static_cast<double>(pin.rows);
    if (std::abs(rows - pinnedRows) > kUnstableRowsTolerance * pinnedRows)
    {
        mismatches.push_back(
            std::format("branch {}: {} records (OpenRocket: {}; more than {} % "
                        "apart)",
                        pin.name, branch.getLength(), pin.rows, 100 * kUnstableRowsTolerance));
    }
    const double altitude = branch.getOptimumAltitude();
    if (!(std::abs(altitude - pin.optimumAltitude) <=
          kUnstableAltitudeTolerance * std::abs(pin.optimumAltitude)))
    {
        mismatches.push_back(std::format(
            "branch {} optimumAltitude: {} (OpenRocket: {}; relative difference {:.3e})", pin.name,
            altitude, pin.optimumAltitude,
            std::abs(altitude - pin.optimumAltitude) / std::abs(pin.optimumAltitude)));
    }
    compareUnstableTime(mismatches, std::format("branch {} timeToOptimumAltitude", pin.name),
                        branch.getTimeToOptimumAltitude(), pin.timeToOptimumAltitude);
}

/// The differences between the events of @p branch and those of @p pin.
void compareEvents(std::vector<std::string>& mismatches, const FlightDataBranch& branch,
                   const BranchPin& pin)
{
    const std::vector<FlightEvent>& events = branch.getEvents();
    if (events.size() != pin.events.size())
    {
        mismatches.push_back(std::format("branch {}: {} events (OpenRocket: {})", pin.name,
                                         events.size(), pin.events.size()));
        return;
    }
    for (std::size_t i = 0; i < events.size(); i++)
    {
        const std::string what =
            std::format("branch {} event {} {}", pin.name, i, name(pin.events[i].type));
        if (events[i].getType() != pin.events[i].type)
        {
            mismatches.push_back(std::format("{}: is {}", what, name(events[i].getType())));
        }
        else if (i < pin.stableEvents)
        {
            compare(mismatches, what, events[i].getTime(), pin.events[i].time);
        }
        else
        {
            compareUnstableTime(mismatches, what, events[i].getTime(), pin.events[i].time);
        }
    }
}

/// The differences between @p data and @p pin.
[[nodiscard]] std::vector<std::string> differences(const FlightData& data, const RunPin& pin)
{
    std::vector<std::string> mismatches;
    compare(mismatches, "maxAltitude", data.getMaxAltitude(), pin.maxAltitude);
    compare(mismatches, "maxVelocity", data.getMaxVelocity(), pin.maxVelocity);
    compare(mismatches, "timeToApogee", data.getTimeToApogee(), pin.timeToApogee);
    compare(mismatches, "flightTime", data.getFlightTime(), pin.flightTime);
    compare(mismatches, "deploymentVelocity", data.getDeploymentVelocity(), pin.deploymentVelocity);
    compare(mismatches, "optimumDelay", data.getOptimumDelay(), pin.optimumDelay);
    compare(mismatches, "maxAcceleration", data.getMaxAcceleration(), pin.maxAcceleration);
    compare(mismatches, "maxMachNumber", data.getMaxMachNumber(), pin.maxMachNumber);
    compare(mismatches, "groundHitVelocity", data.getGroundHitVelocity(), pin.groundHitVelocity);
    compare(mismatches, "launchRodVelocity", data.getLaunchRodVelocity(), pin.launchRodVelocity);

    if (data.getBranchCount() != pin.branches.size())
    {
        mismatches.push_back(std::format("{} branches (OpenRocket: {})", data.getBranchCount(),
                                         pin.branches.size()));
        return mismatches;
    }
    for (std::size_t b = 0; b < pin.branches.size(); b++)
    {
        const FlightDataBranch& branch    = data.getBranch(b);
        const BranchPin&        branchPin = pin.branches[b];
        if (branch.getName() != branchPin.name)
        {
            mismatches.push_back(
                std::format("branch {} is named {}", branchPin.name, branch.getName()));
        }
        compareEvents(mismatches, branch, branchPin);
        if (!branchPin.stable)
        {
            compareUnstableBranch(mismatches, branch, branchPin);
            continue;
        }
        if (branch.getLength() != branchPin.rows)
        {
            mismatches.push_back(std::format("branch {}: {} records (OpenRocket: {})",
                                             branchPin.name, branch.getLength(), branchPin.rows));
        }
        compare(mismatches, std::format("branch {} optimumAltitude", branchPin.name),
                branch.getOptimumAltitude(), branchPin.optimumAltitude);
        compare(mismatches, std::format("branch {} timeToOptimumAltitude", branchPin.name),
                branch.getTimeToOptimumAltitude(), branchPin.timeToOptimumAltitude);
    }
    return mismatches;
}

/// Runs configuration number @p configIndex of @p rocket (in the order of the rocket's
/// configurations, the default first) as SensitivityProbe does, and returns what differs from
/// @p pin.
[[nodiscard]] std::vector<std::string> runAndCompare(Rocket& rocket, int configIndex,
                                                     const RunPin& pin)
{
    std::vector<std::string> mismatches;
    Simulation               sim(rocket);
    sim.setFlightConfigurationId(
        rocket.getFlightConfigurationByIndex(configIndex, true).getFlightConfigurationId());
    setGoldenOptions(sim.getOptions(), 0.005);

    const JitterRemoval                              jitterRemoval;
    std::vector<std::shared_ptr<SimulationListener>> listeners;
    jitterRemoval.install(listeners);

    const Result<void> result = sim.simulate(listeners);
    if (!result.has_value())
    {
        mismatches.push_back(std::format("the run failed: {}", result.error().message));
        return mismatches;
    }
    const std::string description = sim.getSimulatedConfigurationDescription().value_or("(none)");
    if (description != pin.configuration)
    {
        mismatches.push_back(std::format("the configuration is {}", description));
    }
    // One replacement per force evaluation of the flight stepper, the nested optimum-coast
    // run included: four per Runge-Kutta step.
    if (jitterRemoval.replacements() != pin.replacements)
    {
        mismatches.push_back(std::format("{} force evaluations (OpenRocket: {})",
                                         jitterRemoval.replacements(), pin.replacements));
    }
    std::vector<std::string> found = differences(simulatedData(sim), pin);
    mismatches.insert(mismatches.end(), std::make_move_iterator(found.begin()),
                      std::make_move_iterator(found.end()));
    return mismatches;
}

// The single-stage flight: the recovery device opens after apogee, so the optimum altitude is
// the apogee of the flight itself (no nested run). Note the ejection charge: 5 s after the
// simulation time at which the burnout event was handled (2.1002...), not after the burnout.
TEST(EngineStableRun, TheSingleStageFlightIsOpenRockets)
{
    const RunPin pin{
        .configuration      = "[C6-5]",
        .replacements       = 6364,
        .maxAltitude        = 275.3434114366192,
        .maxVelocity        = 102.80347867708689,
        .timeToApogee       = 5.972344759913752,
        .flightTime         = 89.58826587397517,
        .deploymentVelocity = 8.995127397341344,
        .optimumDelay       = 3.917344759913751,
        .maxAcceleration    = 271.01880811295047,
        .maxMachNumber      = 0.3029784286874992,
        .groundHitVelocity  = 3.8126180943088874,
        .launchRodVelocity  = 18.456937977591,
        .branches           = {BranchPin{
            .name                  = "Stage",
            .rows                  = 2089,
            .optimumAltitude       = 275.3434114366192,
            .timeToOptimumAltitude = 6.017344759913751,
            .events =
                {
                    {Type::LAUNCH, 0.0},
                    {Type::IGNITION, 0.0},
                    {Type::LIFTOFF, 0.05400000000000004},
                    {Type::LAUNCHROD, 0.17300000000000013},
                    {Type::BURNOUT, 2.1},
                    {Type::APOGEE, 6.017344759913751},
                    {Type::EJECTION_CHARGE, 7.100219759913834},
                    {Type::RECOVERY_DEVICE_DEPLOYMENT, 7.1012197599138345},
                    {Type::GROUND_HIT, 89.58826587397517},
                    {Type::SIMULATION_END, 89.58826587397517},
                },
            .stableEvents = 10,
            .stable       = true,
        }},
    };

    const TestEstesAlphaIII alpha;
    EXPECT_EQ(runAndCompare(*alpha.rocket, 4, pin), std::vector<std::string>{});
}

// The two-stage flight: the booster separates at its burnout and tumbles down, the sustainer
// ignites, and its recovery device opens before apogee, so its optimum altitude comes from the
// nested coast run. The flight of the tumbling booster is not reproducible in OpenRocket itself
// (six runs: the time of the TUMBLE event within 6e-4 s, the ground hit within 5e-3 s), so its
// events from the TUMBLE on, its optimum altitude and its number of records are compared with
// the tolerances of an unstable branch (see kUnstableTimeTolerance): loose against the last
// bits, tight against the tumbling descent itself, which no other whole-flight test checks
// (BasicTumbleStepper is otherwise pinned one step at a time).
TEST(EngineStableRun, TheTwoStageFlightIsOpenRockets)
{
    const RunPin pin{
        .configuration      = "[B4-3; D21-0]",
        .replacements       = 16128,
        .maxAltitude        = 537.5263513274191,
        .maxVelocity        = 166.46946279551508,
        .timeToApogee       = 7.366402293653893,
        .flightTime         = 168.91352160436728,
        .deploymentVelocity = 9.119409443090362,
        .optimumDelay       = 3.913124999999984,
        .maxAcceleration    = 211.14054462377945,
        .maxMachNumber      = 0.48997880170276964,
        .groundHitVelocity  = 3.8495167972551627,
        .launchRodVelocity  = 11.905335450038272,
        .branches =
            {
                BranchPin{
                    .name                  = "Sustainer Stage",
                    .rows                  = 2585,
                    .optimumAltitude       = 540.5452388653105,
                    .timeToOptimumAltitude = 7.913124999999984,
                    .events =
                        {
                            {Type::LAUNCH, 0.0},
                            {Type::IGNITION, 0.0},
                            {Type::LIFTOFF, 0.09500000000000007},
                            {Type::LAUNCHROD, 0.2780000000000002},
                            {Type::BURNOUT, 2.0},
                            {Type::EJECTION_CHARGE, 2.0},
                            {Type::STAGE_SEPARATION, 2.0},
                            {Type::IGNITION, 2.0},
                            {Type::BURNOUT, 4.0},
                            {Type::EJECTION_CHARGE, 7.0},
                            {Type::RECOVERY_DEVICE_DEPLOYMENT, 7.001},
                            {Type::APOGEE, 7.366402293653893},
                            {Type::GROUND_HIT, 168.91352160436728},
                            {Type::SIMULATION_END, 168.91352160436728},
                        },
                    .stableEvents = 14,
                    .stable       = true,
                },
                BranchPin{
                    .name                  = "Booster Stage",
                    .rows                  = 1022,
                    .optimumAltitude       = 231.98062250843094,
                    .timeToOptimumAltitude = 3.489432315206101,
                    .events =
                        {
                            {Type::IGNITION, 0.0},
                            {Type::BURNOUT, 2.0},
                            {Type::EJECTION_CHARGE, 2.0},
                            {Type::STAGE_SEPARATION, 2.0},
                            {Type::TUMBLE, 2.1998095731038685},
                            {Type::APOGEE, 3.489432315206101},
                            {Type::GROUND_HIT, 26.922114765025437},
                            {Type::SIMULATION_END, 26.922114765025437},
                        },
                    .stableEvents = 4,
                    .stable       = false,
                },
            },
    };

    const TestBeta beta;
    EXPECT_EQ(runAndCompare(*beta.rocket, 2, pin), std::vector<std::string>{});
}

}  // namespace
