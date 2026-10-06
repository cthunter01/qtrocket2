// Whole flights of the engine WITH the pitch/yaw jitter against OpenRocket's, and the sequence
// of the random numbers a flight draws.
//
// Every other whole-flight comparison with OpenRocket removes the jitter (JitterRemoval.h: the
// golden simulations, engine_stable_run_tests.cpp), and the jitter itself is pinned for single
// Runge-Kutta steps only. These tests pin the path every ordinary run takes: the random numbers
// of java.util.Random (util/JavaRandom) drawn over a whole flight, two per force evaluation, by
// a stepper that starts its sequence anew at every initialize(), with the nested optimum-coast
// engine drawing from a stepper of its own.
//
// The pins are the output of probes/tier8b-fix/steppers/JitterFlightProbe.java (OpenRocket under
// the preferences of its JUnit tests): a launch rod of 1 m straight up, calm air, the ISA
// atmosphere, WGS gravity, the spherical earth at 28.61 N 80.6 W, a fixed random seed, and a
// time step of 0.005 s, at which the flights are integrated stably (at the default 0.05 s they
// are not reproducible in OpenRocket itself; see engine_stable_run_tests.cpp).
//
// Two kinds of test:
// - "JitteredFlight": the summary values, the events and the number of records of a flight
//   (one, two stages; RK4, RK6; and one flight on lookup tables in place of the Barrowman
//   calculator).
//   A draw that is missing, doubled or out of order changes every later value, but a sequence
//   that merely starts anew at another point changes them very little (an extra reseed when
//   the launch rod is cleared moves the single-stage flight by 2e-7, one at the stage
//   separation moves the sustainer by 8e-10), so these tests cannot tell where the stepper is
//   reseeded.
// - "JitterDraws": the sequence itself. A pair of system listeners that changes nothing works
//   out, for every force evaluation of a Runge-Kutta stepper, the two random numbers that were
//   added (the forces the listener is given, minus the calculator's result for the same flight
//   conditions), and the numbers are then matched against JavaRandom(seed ^ kSeedRandomization):
//   every pair must be the next pair of the sequence, or its first pair, which is a reseed.
//   The number of evaluations and the places of the reseeds are OpenRocket's.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <numbers>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/AerodynamicCalculator.h"
#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/aero/lookup/MachAoALookup.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/models/GravityModelType.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/AbstractRkSimulationStepper.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/SimulationStepperMethod.h"
#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/JavaRandom.h"
#include "rocket/TestRockets.h"
#include "simulation/SimulationRunSupport.h"

namespace
{

using QtRocket::AbstractRkSimulationStepper;
using QtRocket::AerodynamicCalculator;
using QtRocket::AerodynamicForces;
using QtRocket::CloneableSimulationListener;
using QtRocket::FlightConditions;
using QtRocket::FlightData;
using QtRocket::FlightDataBranch;
using QtRocket::FlightEvent;
using QtRocket::GeodeticComputationStrategy;
using QtRocket::GravityModelType;
using QtRocket::JavaRandom;
using QtRocket::MachAoALookup;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::Simulation;
using QtRocket::SimulationConditions;
using QtRocket::SimulationListener;
using QtRocket::SimulationOptions;
using QtRocket::SimulationStatus;
using QtRocket::SimulationStepperMethod;
using QtRocket::Test::JavaTestPreferences;
using QtRocket::Test::simulatedData;
using QtRocket::Test::TestBeta;
using QtRocket::Test::TestEstesAlphaIII;

using Type = FlightEvent::Type;

// ================================================================================ the flights

/// A simulation as JitterFlightProbe.simulation() makes one: configuration number
/// @p configIndex of @p rocket (in the order of the rocket's configurations, the default
/// first), the options of the probe, the random seed @p seed and the stepper @p method.
struct ProbeSimulation
{
    JavaTestPreferences     preferences;
    std::unique_ptr<Rocket> rocket;
    Simulation              simulation;

    ProbeSimulation(std::unique_ptr<Rocket> simulatedRocket, int configIndex, int seed,
                    SimulationStepperMethod method)
      : rocket(std::move(simulatedRocket)), simulation(*rocket, preferences.store)
    {
        simulation.setFlightConfigurationId(
            rocket->getFlightConfigurationByIndex(configIndex, true).getFlightConfigurationId());
        SimulationOptions& o = simulation.getOptions();
        o.setLaunchRodLength(1.0);
        o.setLaunchIntoWind(false);
        o.setLaunchRodAngle(0.0);
        o.setLaunchRodDirection(0.0);
        o.getAverageWindModel().setAverage(0.0);
        o.getAverageWindModel().setStandardDeviation(0.0);
        o.getAverageWindModel().setTurbulenceIntensity(0.0);
        o.getAverageWindModel().setDirection(0.0);
        o.setLaunchAltitude(0.0);
        o.setLaunchLatitude(28.61);
        o.setLaunchLongitude(-80.6);
        o.setGeodeticComputation(GeodeticComputationStrategy::SPHERICAL);
        o.setIsaAtmosphere(true);
        o.setTimeStep(0.005);
        o.setMaxSimulationTime(1200.0);
        o.setMaximumStepAngle(3 * std::numbers::pi / 180);
        o.setGravityModelType(GravityModelType::WGS);
        o.setRandomSeedFixed(true);
        o.setRandomSeed(seed);
        o.setSimulationStepperMethodChoice(method);
    }
};

/// The Estes Alpha III in configuration number @p configIndex with the random seed 0.
[[nodiscard]] std::unique_ptr<ProbeSimulation> alphaSimulation(int                     configIndex,
                                                               SimulationStepperMethod method)
{
    return std::make_unique<ProbeSimulation>(TestEstesAlphaIII().rocket, configIndex, 0, method);
}

/// The two-stage Beta in its [B4-3; D21-0] configuration with the random seed 2.
[[nodiscard]] std::unique_ptr<ProbeSimulation> betaSimulation()
{
    return std::make_unique<ProbeSimulation>(TestBeta().rocket, 2, 2, SimulationStepperMethod::RK4);
}

/// An event of a pinned flight: its type, its time and how far from it the time may be.
struct EventPin
{
    Type   type;
    double time;
    double tolerance;

    constexpr EventPin(Type eventType, double eventTime, double timeTolerance)
      : type(eventType), time(eventTime), tolerance(timeTolerance)
    {
    }
};

/// A summary value of a pinned flight and its relative tolerance.
struct ValuePin
{
    std::string_view name;
    double           actual;
    double           pinned;
    double           tolerance;

    constexpr ValuePin(std::string_view valueName, double actualValue, double pinnedValue,
                       double relativeTolerance)
      : name(valueName), actual(actualValue), pinned(pinnedValue), tolerance(relativeTolerance)
    {
    }
};

/// Notes in @p mismatches the values of @p values that are further from their pins than their
/// relative tolerances. Written so that a NaN fails.
void compareValues(std::vector<std::string>& mismatches, const std::vector<ValuePin>& values)
{
    for (const ValuePin& value : values)
    {
        if (!(std::abs(value.actual - value.pinned) <= value.tolerance * std::abs(value.pinned)))
        {
            mismatches.push_back(std::format(
                "{}: {} (OpenRocket: {}; relative difference {:.3e})", value.name, value.actual,
                value.pinned, std::abs(value.actual - value.pinned) / std::abs(value.pinned)));
        }
    }
}

/// Notes in @p mismatches what differs between the events of @p branch and @p pins: another
/// sequence of types, or a time further from its pin than the pin's tolerance.
void compareEvents(std::vector<std::string>& mismatches, const FlightDataBranch& branch,
                   const std::vector<EventPin>& pins)
{
    const std::vector<FlightEvent>& events = branch.getEvents();
    if (events.size() != pins.size())
    {
        mismatches.push_back(std::format("branch {}: {} events (OpenRocket: {})", branch.getName(),
                                         events.size(), pins.size()));
        return;
    }
    for (std::size_t i = 0; i < events.size(); i++)
    {
        const std::string what =
            std::format("branch {} event {} {}", branch.getName(), i, name(pins[i].type));
        if (events[i].getType() != pins[i].type)
        {
            mismatches.push_back(std::format("{}: is {}", what, name(events[i].getType())));
        }
        else if (!(std::abs(events[i].getTime() - pins[i].time) <= pins[i].tolerance))
        {
            mismatches.push_back(std::format("{}: {} s (OpenRocket: {} s; {:.3e} s apart)", what,
                                             events[i].getTime(), pins[i].time,
                                             std::abs(events[i].getTime() - pins[i].time)));
        }
    }
}

/// Runs @p probe; the flight data, or null after a note in @p mismatches.
[[nodiscard]] const FlightData* flightOf(ProbeSimulation&          probe,
                                         std::vector<std::string>& mismatches)
{
    const Result<void> result = probe.simulation.simulate();
    if (!result.has_value())
    {
        mismatches.push_back(std::format("the run failed: {}", result.error().message));
        return nullptr;
    }
    return &simulatedData(probe.simulation);
}

// ---- the single-stage flight
//
// The Estes Alpha III [C6-5]: the parachute opens after apogee, so the whole ascent is flown by
// the Runge-Kutta stepper with the jitter, without a nested run.
//
// Tolerances, from the measurements: ten OpenRocket runs (ten sets of component ids) differ
// from each other by at most 1.3e-12 relative in the summary values and 1.2e-10 s in the event
// times; this port differs from the pinned run by 2e-14 and 2e-12 s with glibc, and by at most
// 8.4e-12 and 7.6e-10 s under the one-ulp libm shim (six patterns). The number of records is
// 2394 in every one of those runs. So: relative 1e-9 for the summary values and 1e-8 s for the
// event times, a hundred times OpenRocket's own spread and ten times the shim's; the records
// and the event sequence exactly.
constexpr double kValueTolerance = 1e-9;
constexpr double kTimeTolerance  = 1e-8;

/// What OpenRocket's flight data hold after the single-stage flight with a stepper method.
struct SingleStagePin
{
    double maxAltitude;
    double maxVelocity;
    double maxAcceleration;
    double maxMachNumber;
    double timeToApogee;
    double flightTime;
};

/// What differs between the single-stage flight flown with @p method and @p pin.
[[nodiscard]] std::vector<std::string> singleStageDifferences(SimulationStepperMethod method,
                                                              const SingleStagePin&   pin)
{
    std::vector<std::string>               mismatches;
    const std::unique_ptr<ProbeSimulation> probe = alphaSimulation(4, method);
    const FlightData*                      data  = flightOf(*probe, mismatches);
    if (data == nullptr || data->getBranchCount() != 1)
    {
        mismatches.emplace_back("not a flight of one branch");
        return mismatches;
    }
    compareValues(
        mismatches,
        {{"maxAltitude", data->getMaxAltitude(), pin.maxAltitude, kValueTolerance},
         {"maxVelocity", data->getMaxVelocity(), pin.maxVelocity, kValueTolerance},
         {"maxAcceleration", data->getMaxAcceleration(), pin.maxAcceleration, kValueTolerance},
         {"maxMachNumber", data->getMaxMachNumber(), pin.maxMachNumber, kValueTolerance},
         {"timeToApogee", data->getTimeToApogee(), pin.timeToApogee, kValueTolerance},
         {"flightTime", data->getFlightTime(), pin.flightTime, kValueTolerance}});
    // The same events with both methods: the apogee, found one step late, is the only one the
    // steps time (the ground hit is the flight time).
    compareEvents(mismatches, data->getBranch(0),
                  {{Type::LAUNCH, 0.0, kTimeTolerance},
                   {Type::IGNITION, 0.0, kTimeTolerance},
                   {Type::LIFTOFF, 0.05400000000000004, kTimeTolerance},
                   {Type::LAUNCHROD, 0.17300000000000013, kTimeTolerance},
                   {Type::BURNOUT, 2.1, kTimeTolerance},
                   {Type::APOGEE, 6.014999999999917, kTimeTolerance},
                   {Type::EJECTION_CHARGE, 7.1, kTimeTolerance},
                   {Type::RECOVERY_DEVICE_DEPLOYMENT, 7.101, kTimeTolerance},
                   {Type::GROUND_HIT, pin.flightTime, pin.flightTime * kValueTolerance},
                   {Type::SIMULATION_END, pin.flightTime, pin.flightTime * kValueTolerance}});
    if (data->getBranch(0).getLength() != 2394)
    {
        mismatches.push_back(
            std::format("{} records (OpenRocket: 2394)", data->getBranch(0).getLength()));
    }
    if (!data->getWarningSet().empty())
    {
        mismatches.emplace_back("there are warnings (OpenRocket: none)");
    }
    return mismatches;
}

TEST(JitteredFlight, TheSingleStageFlightIsOpenRocketsWithRk4)
{
    const SingleStagePin pin{.maxAltitude     = 276.35839611756967,
                             .maxVelocity     = 102.93475592201659,
                             .maxAcceleration = 271.1331175613874,
                             .maxMachNumber   = 0.3029494158286047,
                             .timeToApogee    = 5.974999999999918,
                             .flightTime      = 89.84553919871905};
    EXPECT_EQ(singleStageDifferences(SimulationStepperMethod::RK4, pin),
              std::vector<std::string>{});
}

// The flight time of the RK6 flight varies in its eleventh digit from one start of the JVM to
// the next (89.84551305336 to 89.84551305342), so it is pinned to that digit.
TEST(JitteredFlight, TheSingleStageFlightIsOpenRocketsWithRk6)
{
    const SingleStagePin pin{.maxAltitude     = 276.35837787607807,
                             .maxVelocity     = 102.93475472712213,
                             .maxAcceleration = 271.13312755661104,
                             .maxMachNumber   = 0.3029494122514061,
                             .timeToApogee    = 5.974999999999918,
                             .flightTime      = 89.8455130534};
    EXPECT_EQ(singleStageDifferences(SimulationStepperMethod::RK6, pin),
              std::vector<std::string>{});
}

// ---- the two-stage flight
//
// The Beta [B4-3; D21-0]: the booster separates at its burnout and tumbles down; the sustainer
// ignites, and its parachute opens before apogee, so there is a nested coast run, which draws
// from a stepper of its own, and the apogee is found by the landing stepper.
//
// Only the sustainer is compared: the flight of the tumbling booster is not reproducible in
// OpenRocket itself (ten runs: 1166 to 1287 records, the ground hit between 27.14 s and
// 27.26 s), so of the booster only the order of its events is checked.
//
// Tolerances. Nine of ten OpenRocket runs differ from each other by at most 2.3e-10 relative
// and 8.1e-9 s, and this port from the pinned run by at most 2.9e-10 and 1.3e-8 s under the
// libm shim: relative 1e-8 for the summary values and 5e-7 s for the event times. The tenth run
// took the other side of a knife-edge of OpenRocket's own: the landing stepper aims its step at
// the apogee, where v + a * |v / a| is 0.0 or 1.1e-16, and the latter costs one more step of
// the minimum time step (0.001 s). That run has 2589 records where the others have 2588, its
// APOGEE event is 0.001 s later, and it lands 1.1e-5 s later. A platform with other
// mathematical functions may take either side, so the apogee is allowed that one step, the
// landing 5e-5 s, and the records one more.
constexpr double kSustainerValueTolerance = 1e-8;
constexpr double kSustainerTimeTolerance  = 5e-7;
constexpr double kApogeeStep              = 0.001 + kSustainerTimeTolerance;
constexpr double kLandingTolerance        = 5e-5;

/// What differs between the sustainer's branch @p branch and OpenRocket's.
void compareSustainerBranch(std::vector<std::string>& mismatches, const FlightDataBranch& branch)
{
    compareEvents(mismatches, branch,
                  {{Type::LAUNCH, 0.0, kSustainerTimeTolerance},
                   {Type::IGNITION, 0.0, kSustainerTimeTolerance},
                   {Type::LIFTOFF, 0.09500000000000007, kSustainerTimeTolerance},
                   {Type::LAUNCHROD, 0.2780000000000002, kSustainerTimeTolerance},
                   {Type::BURNOUT, 2.0, kSustainerTimeTolerance},
                   {Type::EJECTION_CHARGE, 2.0, kSustainerTimeTolerance},
                   {Type::STAGE_SEPARATION, 2.0, kSustainerTimeTolerance},
                   {Type::IGNITION, 2.0, kSustainerTimeTolerance},
                   {Type::BURNOUT, 4.0, kSustainerTimeTolerance},
                   {Type::EJECTION_CHARGE, 7.0, kSustainerTimeTolerance},
                   {Type::RECOVERY_DEVICE_DEPLOYMENT, 7.001, kSustainerTimeTolerance},
                   {Type::APOGEE, 7.376987072234737, kApogeeStep},
                   {Type::GROUND_HIT, 169.41999355825354, kLandingTolerance},
                   {Type::SIMULATION_END, 169.41999355825354, kLandingTolerance}});
    if (branch.getLength() != 2588 && branch.getLength() != 2589)
    {
        mismatches.push_back(std::format("branch {}: {} records (OpenRocket: 2588 or 2589)",
                                         branch.getName(), branch.getLength()));
    }
}

/// What differs between the two-stage flight and OpenRocket's.
[[nodiscard]] std::vector<std::string> twoStageDifferences()
{
    std::vector<std::string>               mismatches;
    const std::unique_ptr<ProbeSimulation> probe = betaSimulation();
    const FlightData*                      data  = flightOf(*probe, mismatches);
    if (data == nullptr || data->getBranchCount() != 2)
    {
        mismatches.emplace_back("not a flight of two branches");
        return mismatches;
    }
    // The summary values are those of the first branch, the sustainer's.
    compareValues(
        mismatches,
        {{"maxAltitude", data->getMaxAltitude(), 539.2190162319596, kSustainerValueTolerance},
         {"maxVelocity", data->getMaxVelocity(), 166.59144195655335, kSustainerValueTolerance},
         {"maxAcceleration", data->getMaxAcceleration(), 211.09121437087526,
          kSustainerValueTolerance},
         {"maxMachNumber", data->getMaxMachNumber(), 0.4899784018147501, kSustainerValueTolerance},
         {"timeToApogee", data->getTimeToApogee(), 7.376987072234737, kSustainerValueTolerance},
         {"flightTime", data->getFlightTime(), 169.41999355825354,
          kLandingTolerance / 169.41999355825354}});
    compareSustainerBranch(mismatches, data->getBranch(0));
    if (QtRocket::Test::eventNames(*data, 1) !=
        std::vector<std::string>{"IGNITION", "BURNOUT", "EJECTION_CHARGE", "STAGE_SEPARATION",
                                 "TUMBLE", "APOGEE", "GROUND_HIT", "SIMULATION_END"})
    {
        mismatches.emplace_back("the events of the booster are not OpenRocket's");
    }
    return mismatches;
}

TEST(JitteredFlight, TheSustainerOfTheTwoStageFlightIsOpenRockets)
{
    EXPECT_EQ(twoStageDifferences(), std::vector<std::string>{});
}

// ---- a flight on lookup tables
//
// The drag coefficient and the stability coefficients come from tables over the Mach number and
// the angle of attack, which SimulationOptions::toSimulationConditions() turns into the lookup
// calculators in place of the Barrowman calculator: the one whole flight on them that is
// compared with OpenRocket's. The Estes Alpha III [C6-7], whose parachute opens after apogee.
// OpenRocket's flight (JitterFlightProbe.lookupFlight()) is the same to the last bit in four
// runs, since the lookup calculators sum nothing in the order of a hash map; the tolerances are
// those of the single-stage flight above.

/// The table of JitterFlightProbe.lookupFlight(): for the Mach numbers 0, 0.5, 1 and 2 and the
/// angles of attack 0, 2, 5, 10 and 17 degrees, CD = 0.45 + 0.1 M + 0.02 a for @p drag, and
/// CN = (0.2 + 0.02 M) a, the centre of pressure at 0.24 + 0.005 M and Cm = CN CP / 0.0248 for
/// @p stability.
void fillLookupTables(MachAoALookup::Builder& drag, MachAoALookup::Builder& stability)
{
    for (const double mach : {0.0, 0.5, 1.0, 2.0})
    {
        for (const double aoa : {0.0, 2.0, 5.0, 10.0, 17.0})
        {
            drag.addDragData(mach, aoa, 0.45 + (0.1 * mach) + (0.02 * aoa));
            const double cn = (0.2 + (0.02 * mach)) * aoa;
            const double cp = 0.24 + (0.005 * mach);
            stability.addStabilityData(mach, aoa, cn, cn * cp / 0.0248, cp);
        }
    }
}

/// Gives the options of @p probe the two lookup tables; false (after a test failure) when one
/// could not be built.
[[nodiscard]] bool useLookupTables(ProbeSimulation& probe)
{
    MachAoALookup::Builder drag      = MachAoALookup::dragBuilder();
    MachAoALookup::Builder stability = MachAoALookup::stabilityBuilder();
    fillLookupTables(drag, stability);
    Result<MachAoALookup> dragTable      = drag.build();
    Result<MachAoALookup> stabilityTable = stability.build();
    if (!dragTable.has_value() || !stabilityTable.has_value())
    {
        ADD_FAILURE() << "a lookup table could not be built";
        return false;
    }
    SimulationOptions& options = probe.simulation.getOptions();
    options.setDragLookup(std::nullopt,
                          std::make_shared<const MachAoALookup>(std::move(*dragTable)));
    options.setStabilityLookup(std::nullopt,
                               std::make_shared<const MachAoALookup>(std::move(*stabilityTable)));
    return true;
}

/// What differs between the flight on the lookup tables and OpenRocket's.
[[nodiscard]] std::vector<std::string> lookupFlightDifferences()
{
    std::vector<std::string>               mismatches;
    const std::unique_ptr<ProbeSimulation> probe = alphaSimulation(5, SimulationStepperMethod::RK4);
    if (!useLookupTables(*probe))
    {
        mismatches.emplace_back("no lookup tables");
        return mismatches;
    }
    const FlightData* data = flightOf(*probe, mismatches);
    if (data == nullptr || data->getBranchCount() != 1)
    {
        mismatches.emplace_back("not a flight of one branch");
        return mismatches;
    }
    const double flightTime = 177.54015179116308;
    compareValues(
        mismatches,
        {{"maxAltitude", data->getMaxAltitude(), 553.9534076571639, kValueTolerance},
         {"maxVelocity", data->getMaxVelocity(), 162.11300427542912, kValueTolerance},
         {"maxAcceleration", data->getMaxAcceleration(), 275.5473116090664, kValueTolerance},
         {"maxMachNumber", data->getMaxMachNumber(), 0.4774154175400719, kValueTolerance},
         {"timeToApogee", data->getTimeToApogee(), 8.925000000000018, kValueTolerance},
         {"flightTime", data->getFlightTime(), flightTime, kValueTolerance}});
    compareEvents(mismatches, data->getBranch(0),
                  {{Type::LAUNCH, 0.0, kTimeTolerance},
                   {Type::IGNITION, 0.0, kTimeTolerance},
                   {Type::LIFTOFF, 0.05400000000000004, kTimeTolerance},
                   {Type::LAUNCHROD, 0.17300000000000013, kTimeTolerance},
                   {Type::BURNOUT, 2.1, kTimeTolerance},
                   {Type::APOGEE, 8.970000000000026, kTimeTolerance},
                   {Type::EJECTION_CHARGE, 9.1, kTimeTolerance},
                   {Type::RECOVERY_DEVICE_DEPLOYMENT, 9.100999999999999, kTimeTolerance},
                   {Type::GROUND_HIT, flightTime, flightTime * kValueTolerance},
                   {Type::SIMULATION_END, flightTime, flightTime * kValueTolerance}});
    if (data->getBranch(0).getLength() != 2963)
    {
        mismatches.push_back(
            std::format("{} records (OpenRocket: 2963)", data->getBranch(0).getLength()));
    }
    if (!data->getWarningSet().empty())
    {
        mismatches.emplace_back("there are warnings (OpenRocket: none)");
    }
    return mismatches;
}

TEST(JitteredFlight, AFlightOnLookupTablesIsOpenRockets)
{
    EXPECT_EQ(lookupFlightDifferences(), std::vector<std::string>{});
}

// The lookup tables are what the flight above flew on: without them the same simulation is
// another flight (the Barrowman calculator's).
TEST(JitteredFlight, TheLookupTablesChangeTheFlight)
{
    const std::unique_ptr<ProbeSimulation> barrowman =
        alphaSimulation(5, SimulationStepperMethod::RK4);
    ASSERT_TRUE(barrowman->simulation.simulate().has_value());
    const double altitude = simulatedData(barrowman->simulation).getMaxAltitude();
    EXPECT_GT(std::abs(altitude - 553.9534076571639), 100.0) << altitude;
}

// ================================================================================== the draws

/// What the two recording listeners and their clones share: the flight conditions of the
/// calculation that is under way, and the two random numbers of every force evaluation so far.
struct DrawLog
{
    std::optional<FlightConditions>        conditions;
    std::vector<std::pair<double, double>> draws;
};

/// JitterFlightProbe.Conditions: keeps the flight conditions of the aerodynamic calculation that
/// follows. A system listener, so that the nested coast run keeps it; it changes nothing.
class DrawConditions final : public CloneableSimulationListener<DrawConditions>
{
public:
    explicit DrawConditions(std::shared_ptr<DrawLog> log) : m_log(std::move(log)) { }

    [[nodiscard]] bool isSystemListener() const override { return true; }

    [[nodiscard]] std::optional<FlightConditions> postFlightConditions(
        SimulationStatus& /*status*/, const FlightConditions& flightConditions) override
    {
        m_log->conditions = flightConditions;
        return std::nullopt;
    }

private:
    std::shared_ptr<DrawLog> m_log;
};

/// JitterFlightProbe.Recorder: works out the two random numbers of a force evaluation of a
/// Runge-Kutta stepper from the forces it is given (Cm and Cyaw with kPitchYawRandom * 2 *
/// (r - 0.5) added) and the calculator's result for the same flight conditions. It answers
/// nothing: the flight keeps its jitter. As JitterRemoval, it tells the forces of the landing
/// and tumble steppers, which carry no jitter, by their normal force coefficient, which is NaN.
class DrawRecorder final : public CloneableSimulationListener<DrawRecorder>
{
public:
    explicit DrawRecorder(std::shared_ptr<DrawLog> log) : m_log(std::move(log)) { }

    [[nodiscard]] bool isSystemListener() const override { return true; }

    [[nodiscard]] std::optional<AerodynamicForces> postAerodynamicCalculation(
        SimulationStatus& status, const AerodynamicForces& forces) override
    {
        if (std::isnan(forces.getCN()) || !m_log->conditions.has_value())
        {
            return std::nullopt;
        }
        const FlightConditions conditions = std::move(*m_log->conditions);
        m_log->conditions.reset();
        const SimulationConditions& simulationConditions = *status.getSimulationConditions();
        AerodynamicCalculator&      calculator = *simulationConditions.getAerodynamicCalculator();
        const AerodynamicForces     clean =
            calculator.getAerodynamicForces(status.getConfiguration(), conditions, nullptr);
        const double k = 2 * AbstractRkSimulationStepper::kPitchYawRandom;
        m_log->draws.emplace_back(((forces.getCm() - clean.getCm()) / k) + 0.5,
                                  ((forces.getCyaw() - clean.getCyaw()) / k) + 0.5);
        return std::nullopt;
    }

private:
    std::shared_ptr<DrawLog> m_log;
};

/// How close a recovered random number must be to the one of the sequence. The recovery
/// divides a difference of two moment coefficients by 0.001: the errors measured are below
/// 1.5e-11 (in OpenRocket too), and two successive numbers of the sequence agree with two
/// others to 1e-9 once in 1e18 pairs.
constexpr double kDrawTolerance = 1e-9;

/// What the draws of a flight are, as a sequence of JavaRandom(seed ^ kSeedRandomization).
struct DrawAnalysis
{
    /// The number of force evaluations.
    std::size_t evaluations{0};
    /// The evaluations, after the first, at which the sequence starts anew.
    std::vector<std::size_t> reseeds;
    /// The evaluations whose numbers are neither the next of the sequence nor its first.
    std::size_t unexplained{0};

    [[nodiscard]] bool operator==(const DrawAnalysis&) const = default;
};

/// Whether @p draw is the pair of numbers @p random gives next; @p random is then past them.
[[nodiscard]] bool isNextPair(const std::pair<double, double>& draw, JavaRandom& random)
{
    const double first  = random.nextDouble();
    const double second = random.nextDouble();
    return std::abs(draw.first - first) < kDrawTolerance &&
           std::abs(draw.second - second) < kDrawTolerance;
}

/// Matches @p draws against the sequence of the stepper for the random seed @p seed
/// (JitterFlightProbe.draws()).
[[nodiscard]] DrawAnalysis analyse(const std::vector<std::pair<double, double>>& draws, int seed)
{
    // Java: new Random(seed ^ SEED_RANDOMIZATION), an int widened to the long seed.
    const std::int32_t scrambled = seed ^ AbstractRkSimulationStepper::kSeedRandomization;
    DrawAnalysis       analysis;
    analysis.evaluations = draws.size();
    JavaRandom random(std::int64_t{scrambled});
    for (std::size_t i = 0; i < draws.size(); i++)
    {
        JavaRandom ahead = random;
        JavaRandom fresh(std::int64_t{scrambled});
        if (isNextPair(draws[i], ahead))
        {
            random = ahead;
        }
        else if (isNextPair(draws[i], fresh))
        {
            analysis.reseeds.push_back(i);
            random = fresh;
        }
        else
        {
            analysis.unexplained++;
        }
    }
    return analysis;
}

/// Runs @p probe with the two recording listeners (the recorder first, the conditions listener
/// last, as the listeners of JitterRemoval) and returns the draws of the run; empty after a
/// test failure.
[[nodiscard]] std::vector<std::pair<double, double>> drawsOf(ProbeSimulation& probe)
{
    const std::shared_ptr<DrawLog> log    = std::make_shared<DrawLog>();
    const Result<void>             result = probe.simulation.simulate(
        {std::make_shared<DrawRecorder>(log), std::make_shared<DrawConditions>(log)});
    if (!result.has_value())
    {
        ADD_FAILURE() << result.error().message;
        return {};
    }
    return log->draws;
}

/// The analysis of the draws of @p probe, which was made with the random seed @p seed.
[[nodiscard]] DrawAnalysis drawAnalysisOf(ProbeSimulation& probe, int seed)
{
    return analyse(drawsOf(probe), seed);
}

/// A text for a failed comparison of two analyses.
std::ostream& operator<<(std::ostream& out, const DrawAnalysis& analysis)
{
    std::string reseeds;
    for (const std::size_t reseed : analysis.reseeds)
    {
        reseeds += std::format("{} ", reseed);
    }
    return out << std::format("{} evaluations, reseeds at [{}], {} unexplained",
                              analysis.evaluations, reseeds, analysis.unexplained);
}

// The analysis itself: a sequence, a sequence that starts anew, and numbers of another seed.
TEST(JitterDraws, TheAnalysisFindsWhereTheSequenceStartsAnew)
{
    const int          seed      = 7;
    const std::int32_t scrambled = seed ^ AbstractRkSimulationStepper::kSeedRandomization;
    JavaRandom         random(std::int64_t{scrambled});
    std::vector<std::pair<double, double>> draws;
    for (int i = 0; i < 5; i++)
    {
        const double first = random.nextDouble();
        draws.emplace_back(first, random.nextDouble());
    }
    EXPECT_EQ(analyse(draws, seed),
              (DrawAnalysis{.evaluations = 5, .reseeds = {}, .unexplained = 0}));

    // The first three pairs again after the fifth: a restart at evaluation 5.
    std::vector<std::pair<double, double>> restarted = draws;
    restarted.insert(restarted.end(), draws.begin(), draws.begin() + 3);
    EXPECT_EQ(analyse(restarted, seed),
              (DrawAnalysis{.evaluations = 8, .reseeds = {5}, .unexplained = 0}));

    // One number that is not the sequence's: that pair is unexplained, and so is every pair
    // after it, since the sequence has not moved on.
    std::vector<std::pair<double, double>> wrong = draws;
    wrong[2].second += 1e-6;
    EXPECT_EQ(analyse(wrong, seed),
              (DrawAnalysis{.evaluations = 5, .reseeds = {}, .unexplained = 3}));

    // Another seed: nothing fits.
    EXPECT_EQ(analyse(draws, seed + 1),
              (DrawAnalysis{.evaluations = 5, .reseeds = {}, .unexplained = 5}));
}

// The recording listeners are system listeners that answer nothing: the flight is, to the last
// bit, the flight without them.
TEST(JitterDraws, TheRecordingListenersLeaveTheFlightAsItIs)
{
    const std::unique_ptr<ProbeSimulation> plain = alphaSimulation(4, SimulationStepperMethod::RK4);
    const std::unique_ptr<ProbeSimulation> recorded =
        alphaSimulation(4, SimulationStepperMethod::RK4);
    ASSERT_TRUE(plain->simulation.simulate().has_value());
    ASSERT_FALSE(drawsOf(*recorded).empty());

    const FlightData& expected = simulatedData(plain->simulation);
    const FlightData& actual   = simulatedData(recorded->simulation);
    EXPECT_EQ(actual.getMaxAltitude(), expected.getMaxAltitude());
    EXPECT_EQ(actual.getMaxVelocity(), expected.getMaxVelocity());
    EXPECT_EQ(actual.getFlightTime(), expected.getFlightTime());
    EXPECT_EQ(actual.getGroundHitVelocity(), expected.getGroundHitVelocity());
    EXPECT_EQ(actual.getBranch(0).getLength(), expected.getBranch(0).getLength());
    EXPECT_TRUE(actual.getWarningSet().empty());
}

// A flight without a nested run and without a stage separation: one initialize(), one
// sequence, two numbers per force evaluation: four evaluations per RK4 step and seven per RK6
// step, for the 1897 steps up to the deployment of the parachute. OpenRocket: 7588 and 13279
// evaluations, no reseed.
TEST(JitterDraws, ASingleStageFlightDrawsOneSequence)
{
    const std::unique_ptr<ProbeSimulation> rk4 = alphaSimulation(4, SimulationStepperMethod::RK4);
    EXPECT_EQ(drawAnalysisOf(*rk4, 0),
              (DrawAnalysis{.evaluations = 7588, .reseeds = {}, .unexplained = 0}));

    const std::unique_ptr<ProbeSimulation> rk6 = alphaSimulation(4, SimulationStepperMethod::RK6);
    EXPECT_EQ(drawAnalysisOf(*rk6, 0),
              (DrawAnalysis{.evaluations = 13279, .reseeds = {}, .unexplained = 0}));
}

// The Estes Alpha III [A8-0]: the parachute opens before apogee, so the engine flies the flight
// once more in a nested engine to find the optimum altitude. The nested engine has steppers of
// its own: its flight stepper starts the sequence at the first number, and the flight stepper of
// the outer run is not touched (it is not used again in this flight, since the outer run goes
// on under the parachute). OpenRocket: 8484 evaluations, a reseed at evaluation 2724, where the
// nested run begins.
TEST(JitterDraws, TheNestedCoastRunDrawsFromItsOwnStepper)
{
    const std::unique_ptr<ProbeSimulation> probe = alphaSimulation(1, SimulationStepperMethod::RK4);
    EXPECT_EQ(drawAnalysisOf(*probe, 0),
              (DrawAnalysis{.evaluations = 8484, .reseeds = {2724}, .unexplained = 0}));
}

// The two-stage Beta: the sequence starts anew three times after the launch, and nowhere else
// (not at the stage separation, where the sustainer flies on with its stepper as it is): at
// evaluation 6500 the nested coast run of the sustainer begins, at evaluation 13776 the nested
// run goes on to the booster's branch, and then the outer run does. The last place and the
// number of evaluations depend on when the booster of the nested run starts to tumble, which is
// not reproducible in OpenRocket itself (four runs: the third reseed at evaluation 15224 to
// 15244, 17388 to 18116 evaluations), so they are not pinned.
TEST(JitterDraws, EveryBranchOfATwoStageFlightStartsTheSequenceAnew)
{
    const std::unique_ptr<ProbeSimulation> probe    = betaSimulation();
    const DrawAnalysis                     analysis = drawAnalysisOf(*probe, 2);
    EXPECT_EQ(analysis.unexplained, 0U);
    ASSERT_EQ(analysis.reseeds.size(), 3U);
    EXPECT_EQ(analysis.reseeds[0], 6500U);
    EXPECT_EQ(analysis.reseeds[1], 13776U);
    EXPECT_GT(analysis.reseeds[2], analysis.reseeds[1]);
    EXPECT_LT(analysis.reseeds[2], analysis.evaluations);
}

}  // namespace
