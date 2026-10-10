// The validation of a simulation's inputs: Simulation::validateInputs(), and what simulate()
// does with what it finds.
//
// The validation is QtRocket's own; OpenRocket has none, and a NaN or an infinity among the
// options, the delays of a design or the settings of an extension ends there in a BugException
// or in an abort somewhere in the flight. The tests go through the list in the comment of
// validateInputs() value by value: every number a run reads is set to NaN and to each
// infinity, and simulate() has to refuse it with ErrorCode::INVALID_ARGUMENT and a text that
// names the value, or, where the setter of the value stores a finite number for it (a clamped
// angle, the plugged ejection delay), fly; the values a setter keeps finite are tried again
// through a preferences store, from which the options take them as they are. Never a BugError,
// and never an abort of the flight. What a run does not read is not refused (an altitude of a
// device or a stage that some other event triggers, for example, with which OpenRocket flies:
// probes/tier9a-review-extensions-fidelity, UnreadAltitude.java), and nothing valid is: every
// flight configuration of the test rockets passes.
//
// A finite number has to lie in the range of its input as well (decision U9 of tier 9; the
// table SimulationInputRanges::kTable): the second half of the tests of the options, "the
// ranges", tries every input with a range at its two bounds, which are inside it, and with the
// doubles next to them on either side.

#include <chrono>
#include <cmath>
#include <cstddef>
#include <format>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <ostream>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/lookup/MachAoALookup.h"
#include "QtRocket/models/GravityModelType.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/WindModel.h"
#include "QtRocket/models/WindModelType.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationInputRanges.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/extension/AbstractSimulationExtension.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/extension/example/AirStart.h"
#include "QtRocket/simulation/extension/example/RollControl.h"
#include "QtRocket/util/BigDecimal.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Config.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Signal.h"
#include "QtRocket/util/Strings.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::AirStart;
using QtRocket::BugError;
using QtRocket::Config;
using QtRocket::ErrorCode;
using QtRocket::FlightConfigurationId;
using QtRocket::GravityModelType;
using QtRocket::MachAoALookup;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::RollControl;
using QtRocket::Simulation;
using QtRocket::SimulationExtension;
using QtRocket::SimulationOptions;
using QtRocket::WindModelType;
using QtRocket::Test::TestBeta;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::testFcid;
using QtRocket::Test::TestRocketMaker;
using QtRocket::Test::testRocketMakers;

using DeployEvent     = QtRocket::DeploymentConfiguration::DeployEvent;
using SeparationEvent = QtRocket::StageSeparationConfiguration::SeparationEvent;
using Ranges          = QtRocket::SimulationInputRanges;
using Range           = QtRocket::SimulationInputRanges::Range;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

/// How simulate() of @p simulation ends: "ok" for a flight ("ok, aborted" when it holds a
/// SIM_ABORT event), "refused: <message>" for ErrorCode::INVALID_ARGUMENT, "error: <message>"
/// for another failure and "bug: <message>" for a BugError.
[[nodiscard]] std::string outcomeOf(Simulation& simulation)
{
    try
    {
        const Result<void> result = simulation.simulate();
        if (result.has_value())
        {
            return simulation.hasErrors() ? "ok, aborted" : "ok";
        }
        if (result.error().code == ErrorCode::INVALID_ARGUMENT)
        {
            return "refused: " + result.error().message;
        }
        return "error: " + result.error().message;
    }
    catch (const BugError& error)
    {
        return std::string("bug: ") + error.what();
    }
}

/// The message of validateInputs() of @p simulation; "valid" when it passes.
[[nodiscard]] std::string validationMessage(const Simulation& simulation)
{
    const Result<void> result = simulation.validateInputs();
    return result.has_value() ? std::string("valid") : result.error().message;
}

/// The text of the refusal of the value called @p what that is @p value.
[[nodiscard]] std::string refusal(std::string_view what, std::string_view value)
{
    return "refused: Cannot simulate: " + std::string(what) + " is not finite (" +
           std::string(value) + ").";
}

/// What outcomeOf() is expected to be for a value that was set to NaN, to +infinity and to
/// -infinity.
struct Expected
{
    std::string nan;
    std::string positiveInfinity;
    std::string negativeInfinity;
};

/// Every value is refused as it is.
[[nodiscard]] Expected allRefused(std::string_view what)
{
    return {.nan              = refusal(what, "NaN"),
            .positiveInfinity = refusal(what, "Infinity"),
            .negativeInfinity = refusal(what, "-Infinity")};
}

/// The NaN is refused; the setter turns an infinity into a finite number.
[[nodiscard]] Expected infinitiesClamped(std::string_view what)
{
    return {.nan = refusal(what, "NaN"), .positiveInfinity = "ok", .negativeInfinity = "ok"};
}

/// The setter turns every value into NaN.
[[nodiscard]] Expected allNaN(std::string_view what)
{
    return {.nan              = refusal(what, "NaN"),
            .positiveInfinity = refusal(what, "NaN"),
            .negativeInfinity = refusal(what, "NaN")};
}

/// A simulation of the Estes Alpha III in its first test configuration (an A8 whose ejection
/// charge fires at burnout), with the built-in default options.
struct AlphaRun
{
    TestEstesAlphaIII alpha;
    Simulation        simulation;

    AlphaRun() : simulation(*alpha.rocket)
    {
        simulation.setFlightConfigurationId(testFcid(0));
        simulation.getOptions().setRandomSeed(0);
    }
};

/// One input of a run: how a test sets it to a value, and what is expected then.
struct OptionCase
{
    using Setter = std::function<void(SimulationOptions&, double)>;

    std::string name;
    Setter      set;
    Expected    expected;

    OptionCase(std::string caseName, Setter setter, Expected outcomes)
      : name(std::move(caseName)), set(std::move(setter)), expected(std::move(outcomes))
    {
    }
};

/// The outcome of the Alpha III's simulation after @p set had its options with @p value.
[[nodiscard]] std::string outcomeWith(const std::function<void(SimulationOptions&, double)>& set,
                                      double                                                 value)
{
    AlphaRun run;
    set(run.simulation.getOptions(), value);
    return outcomeOf(run.simulation);
}

/// Expects the three outcomes of each of @p cases.
void expectOutcomes(const std::vector<OptionCase>& cases)
{
    for (const OptionCase& optionCase : cases)
    {
        SCOPED_TRACE(optionCase.name);
        EXPECT_EQ(outcomeWith(optionCase.set, kNaN), optionCase.expected.nan);
        EXPECT_EQ(outcomeWith(optionCase.set, kInf), optionCase.expected.positiveInfinity);
        EXPECT_EQ(outcomeWith(optionCase.set, -kInf), optionCase.expected.negativeInfinity);
    }
}

// ------------------------------------------------------------------------------ the options

TEST(SimulationInputValidation, AValidSimulationPassesAndFlies)
{
    AlphaRun run;
    EXPECT_TRUE(run.simulation.validateInputs().has_value());
    EXPECT_EQ(outcomeOf(run.simulation), "ok");
}

TEST(SimulationInputValidation, TheLaunchRodAndTheLaunchSite)
{
    const std::vector<OptionCase> cases{
        {"rod length", [](SimulationOptions& o, double v) { o.setLaunchRodLength(v); },
         allRefused("the launch rod length")},
        // setLaunchRodAngle() clamps to +-60 degrees.
        {"rod angle", [](SimulationOptions& o, double v) { o.setLaunchRodAngle(v); },
         infinitiesClamped("the launch rod angle")},
        // setLaunchRodDirection() reduces to 0 ... 2 pi, which no infinity has.
        {"rod direction",
         [](SimulationOptions& o, double v) {
             o.setLaunchIntoWind(false);
             o.setLaunchRodDirection(v);
         },
         allNaN("the launch rod direction")},
        // The setters clamp to +-90 and +-180 degrees.
        {"latitude", [](SimulationOptions& o, double v) { o.setLaunchLatitude(v); },
         infinitiesClamped("the launch latitude")},
        {"longitude", [](SimulationOptions& o, double v) { o.setLaunchLongitude(v); },
         infinitiesClamped("the launch longitude")},
        // setLaunchAltitude() stores the highest altitude of the atmospheric model for a NaN
        // and for anything above it.
        {"altitude",
         [](SimulationOptions& o, double v) { o.setLaunchAltitude(v); },
         {.nan              = "ok",
          .positiveInfinity = "ok",
          .negativeInfinity = refusal("the launch altitude", "-Infinity")}},
    };
    expectOutcomes(cases);
}

TEST(SimulationInputValidation, TheAverageWind)
{
    const std::vector<OptionCase> cases{
        // A negative speed is its magnitude from the other side.
        {"speed",
         [](SimulationOptions& o, double v) { o.getAverageWindModel().setAverage(v); },
         {.nan              = refusal("the average wind speed", "NaN"),
          .positiveInfinity = refusal("the average wind speed", "Infinity"),
          .negativeInfinity = refusal("the average wind speed", "Infinity")}},
        // A negative deviation is stored as 0.
        {"deviation",
         [](SimulationOptions& o, double v) {
             o.getAverageWindModel().setAverage(2.0);
             o.getAverageWindModel().setStandardDeviation(v);
         },
         {.nan              = refusal("the standard deviation of the wind speed", "NaN"),
          .positiveInfinity = refusal("the standard deviation of the wind speed", "Infinity"),
          .negativeInfinity = "ok"}},
        // The turbulence intensity is the deviation over the speed.
        {"turbulence",
         [](SimulationOptions& o, double v) {
             o.getAverageWindModel().setAverage(2.0);
             o.getAverageWindModel().setTurbulenceIntensity(v);
         },
         {.nan              = refusal("the standard deviation of the wind speed", "NaN"),
          .positiveInfinity = refusal("the standard deviation of the wind speed", "Infinity"),
          .negativeInfinity = "ok"}},
        {"turbulence of a calm",
         [](SimulationOptions& o, double v) {
             o.getAverageWindModel().setAverage(0.0);
             o.getAverageWindModel().setTurbulenceIntensity(v);
         },
         allNaN("the standard deviation of the wind speed")},
        {"direction",
         [](SimulationOptions& o, double v) { o.getAverageWindModel().setDirection(v); },
         allNaN("the wind direction")},
        // The deprecated accessors of the options reach the same model.
        {"speed (old accessor)",
         [](SimulationOptions& o, double v) { o.setWindSpeedAverage(v); },
         {.nan              = refusal("the average wind speed", "NaN"),
          .positiveInfinity = refusal("the average wind speed", "Infinity"),
          .negativeInfinity = refusal("the average wind speed", "Infinity")}},
        // Launching into the wind, the rod direction is the wind's: the wind is what is named.
        {"direction, launching into the wind",
         [](SimulationOptions& o, double v) {
             o.setLaunchIntoWind(true);
             o.getAverageWindModel().setDirection(v);
         },
         allNaN("the wind direction")},
    };
    expectOutcomes(cases);
}

/// Makes the multi-level wind the one in use, with two more levels above the initial one, and
/// returns the second of the three.
[[nodiscard]] QtRocket::MultiLevelPinkNoiseWindModel::LevelWindModel& secondWindLevel(
    SimulationOptions& o)
{
    o.setWindModelType(WindModelType::MULTI_LEVEL);
    QtRocket::MultiLevelPinkNoiseWindModel& wind = o.getMultiLevelWindModel();
    EXPECT_TRUE(wind.addWindLevel(500.0, 3.0, 1.0, 0.2).has_value());
    EXPECT_TRUE(wind.addWindLevel(1000.0, 5.0, 2.0, 0.3).has_value());
    const std::vector<QtRocket::MultiLevelPinkNoiseWindModel::LevelWindModel*> levels =
        wind.getLevels();
    EXPECT_EQ(levels.size(), 3U);
    return *levels.at(1);
}

TEST(SimulationInputValidation, EveryLevelOfAMultiLevelWind)
{
    const std::vector<OptionCase> cases{
        {"altitude", [](SimulationOptions& o, double v) { secondWindLevel(o).setAltitude(v); },
         allRefused("the altitude of wind level 2")},
        {"speed",
         [](SimulationOptions& o, double v) { secondWindLevel(o).setSpeed(v); },
         {.nan              = refusal("the wind speed of wind level 2", "NaN"),
          .positiveInfinity = refusal("the wind speed of wind level 2", "Infinity"),
          .negativeInfinity = refusal("the wind speed of wind level 2", "Infinity")}},
        {"direction", [](SimulationOptions& o, double v) { secondWindLevel(o).setDirection(v); },
         allNaN("the wind direction of wind level 2")},
        {"deviation",
         [](SimulationOptions& o, double v) { secondWindLevel(o).setStandardDeviation(v); },
         {.nan = refusal("the standard deviation of the wind speed of wind level 2", "NaN"),
          .positiveInfinity =
              refusal("the standard deviation of the wind speed of wind level 2", "Infinity"),
          .negativeInfinity = "ok"}},
        // The same when launching into that wind, whose direction the rod then takes.
        {"speed, launching into the wind",
         [](SimulationOptions& o, double v) {
             o.setLaunchIntoWind(true);
             secondWindLevel(o).setSpeed(v);
         },
         {.nan              = refusal("the wind speed of wind level 2", "NaN"),
          .positiveInfinity = refusal("the wind speed of wind level 2", "Infinity"),
          .negativeInfinity = refusal("the wind speed of wind level 2", "Infinity")}},
    };
    expectOutcomes(cases);

    // A level that is added with such a value is found like one that is changed: the third.
    AlphaRun run;
    static_cast<void>(secondWindLevel(run.simulation.getOptions()));
    ASSERT_TRUE(run.simulation.getOptions()
                    .getMultiLevelWindModel()
                    .addWindLevel(2000.0, kNaN, 0.0)
                    .has_value());
    EXPECT_EQ(outcomeOf(run.simulation), refusal("the wind speed of wind level 4", "NaN"));

    // A valid multi-level wind flies, also with the altitudes above the ground.
    AlphaRun valid;
    static_cast<void>(secondWindLevel(valid.simulation.getOptions()));
    valid.simulation.getOptions().getMultiLevelWindModel().setAltitudeReference(
        QtRocket::WindModel::AltitudeReference::AGL);
    valid.simulation.getOptions().setLaunchIntoWind(true);
    EXPECT_EQ(outcomeOf(valid.simulation), "ok");
}

TEST(SimulationInputValidation, TheAtmosphereAndTheGravity)
{
    const auto withoutIsa = [](SimulationOptions& o) { o.setIsaAtmosphere(false); };
    const std::vector<OptionCase> cases{
        {"temperature",
         [withoutIsa](SimulationOptions& o, double v) {
             withoutIsa(o);
             o.setLaunchTemperature(v);
         },
         allRefused("the launch temperature")},
        {"pressure",
         [withoutIsa](SimulationOptions& o, double v) {
             withoutIsa(o);
             o.setLaunchPressure(v);
         },
         allRefused("the launch pressure")},
        {"humidity",
         [withoutIsa](SimulationOptions& o, double v) {
             withoutIsa(o);
             o.setLaunchRelativeHumidity(v);
         },
         allRefused("the launch relative humidity")},
        {"constant gravity",
         [](SimulationOptions& o, double v) {
             o.setGravityModelType(GravityModelType::CONSTANT);
             o.setConstantGravity(v);
         },
         allRefused("the constant gravity")},
    };
    expectOutcomes(cases);

    // Valid launch conditions of one's own and a constant gravity fly.
    AlphaRun valid;
    valid.simulation.getOptions().setIsaAtmosphere(false);
    valid.simulation.getOptions().setLaunchTemperature(300.0);
    valid.simulation.getOptions().setLaunchPressure(95000.0);
    valid.simulation.getOptions().setLaunchRelativeHumidity(0.5);
    valid.simulation.getOptions().setGravityModelType(GravityModelType::CONSTANT);
    valid.simulation.getOptions().setConstantGravity(9.81);
    EXPECT_EQ(outcomeOf(valid.simulation), "ok");
}

/// Makes @p built the drag lookup table of @p o.
void setDragTable(SimulationOptions& o, const MachAoALookup::Builder& built)
{
    Result<MachAoALookup> table = built.build();
    ASSERT_TRUE(table.has_value()) << table.error().message;
    o.setDragLookup(std::nullopt, std::make_shared<const MachAoALookup>(std::move(*table)));
}

/// Makes @p built the stability lookup table of @p o.
void setStabilityTable(SimulationOptions& o, const MachAoALookup::Builder& built)
{
    Result<MachAoALookup> table = built.build();
    ASSERT_TRUE(table.has_value()) << table.error().message;
    o.setStabilityLookup(std::nullopt, std::make_shared<const MachAoALookup>(std::move(*table)));
}

// A lookup table replaces a calculation of the flight by numbers from a file, and a run
// interpolates in whichever rows the flight comes by: every number of a table in use has to be
// finite. (Before the validation each of these ended in the BugError "Simulation resulted in
// not-a-number (NaN) value for params.a", or flew on a table that made no sense.)
TEST(SimulationInputValidation, TheLookupTables)
{
    const std::vector<OptionCase> cases{
        {"a drag coefficient",
         [](SimulationOptions& o, double v) {
             setDragTable(
                 o,
                 MachAoALookup::dragBuilder().addDragData(0.0, 0.5).addDragData(0.1, v).addDragData(
                     2.0, 0.6));
         },
         allRefused("the 'cd' of the drag lookup table at Mach 0.1")},
        {"a Mach number of the drag table",
         [](SimulationOptions& o, double v) {
             setDragTable(o, MachAoALookup::dragBuilder()
                                 .addDragData(0.0, 0.5)
                                 .addDragData(v, 0.55)
                                 .addDragData(2.0, 0.6));
         },
         allRefused("a Mach number of the drag lookup table")},
        {"an angle of attack of the drag table",
         [](SimulationOptions& o, double v) {
             setDragTable(o, MachAoALookup::dragBuilder()
                                 .addDragData(0.0, 0.0, 0.5)
                                 .addDragData(0.0, v, 0.5)
                                 .addDragData(2.0, 0.0, 0.6)
                                 .addDragData(2.0, 10.0, 0.6));
         },
         allRefused("an angle of attack of the drag lookup table at Mach 0.0")},
        {"a drag coefficient at an angle of attack",
         [](SimulationOptions& o, double v) {
             setDragTable(o, MachAoALookup::dragBuilder()
                                 .addDragData(0.0, 0.0, 0.5)
                                 .addDragData(0.0, 10.0, 0.5)
                                 .addDragData(2.0, 0.0, 0.6)
                                 .addDragData(2.0, 10.0, v));
         },
         allRefused("the 'cd' of the drag lookup table at Mach 2.0 and an angle of attack of "
                    "10.0 degrees")},
        {"a normal force coefficient",
         [](SimulationOptions& o, double v) {
             setStabilityTable(o, MachAoALookup::stabilityBuilder()
                                      .addStabilityData(0.0, 2.0, 1.0, 0.25)
                                      .addStabilityData(0.1, v, 1.0, 0.25)
                                      .addStabilityData(2.0, 2.0, 1.0, 0.25));
         },
         allRefused("the 'cn' of the stability lookup table at Mach 0.1")},
        {"a pitch moment coefficient",
         [](SimulationOptions& o, double v) {
             setStabilityTable(o, MachAoALookup::stabilityBuilder()
                                      .addStabilityData(0.0, 2.0, v, 0.25)
                                      .addStabilityData(2.0, 2.0, 1.0, 0.25));
         },
         allRefused("the 'cm' of the stability lookup table at Mach 0.0")},
        {"a centre of pressure",
         [](SimulationOptions& o, double v) {
             setStabilityTable(o, MachAoALookup::stabilityBuilder()
                                      .addStabilityData(0.0, 2.0, 1.0, 0.25)
                                      .addStabilityData(2.0, 2.0, 1.0, v));
         },
         allRefused("the 'cp' of the stability lookup table at Mach 2.0")},
        {"a Mach number of the stability table",
         [](SimulationOptions& o, double v) {
             setStabilityTable(o, MachAoALookup::stabilityBuilder()
                                      .addStabilityData(0.0, 2.0, 1.0, 0.25)
                                      .addStabilityData(v, 2.0, 1.0, 0.25));
         },
         allRefused("a Mach number of the stability lookup table")},
    };
    expectOutcomes(cases);

    // Tables of finite numbers fly.
    AlphaRun valid;
    setDragTable(valid.simulation.getOptions(), MachAoALookup::dragBuilder()
                                                    .addDragData(0.0, 0.0, 0.5)
                                                    .addDragData(0.0, 10.0, 0.7)
                                                    .addDragData(2.0, 0.0, 0.6)
                                                    .addDragData(2.0, 10.0, 0.8));
    setStabilityTable(valid.simulation.getOptions(), MachAoALookup::stabilityBuilder()
                                                         .addStabilityData(0.0, 2.0, 1.0, 0.25)
                                                         .addStabilityData(2.0, 2.0, 1.0, 0.25));
    EXPECT_TRUE(valid.simulation.validateInputs().has_value());
    EXPECT_EQ(outcomeOf(valid.simulation), "ok");

    // With both tables at fault the stability table is named: the one the conditions are
    // given first. A table that was taken away again is not looked at.
    AlphaRun both;
    setDragTable(both.simulation.getOptions(), MachAoALookup::dragBuilder().addDragData(1.0, kNaN));
    setStabilityTable(both.simulation.getOptions(),
                      MachAoALookup::stabilityBuilder().addStabilityData(1.0, kInf, 1.0, 0.25));
    EXPECT_EQ(outcomeOf(both.simulation),
              refusal("the 'cn' of the stability lookup table at Mach 1.0", "Infinity"));
    both.simulation.getOptions().clearStabilityLookup();
    EXPECT_EQ(outcomeOf(both.simulation),
              refusal("the 'cd' of the drag lookup table at Mach 1.0", "NaN"));
    both.simulation.getOptions().clearDragLookup();
    EXPECT_EQ(outcomeOf(both.simulation), "ok");
}

TEST(SimulationInputValidation, TheStepperLimitsAndTheWarningThresholds)
{
    const std::vector<OptionCase> cases{
        {"time step", [](SimulationOptions& o, double v) { o.setTimeStep(v); },
         allRefused("the time step")},
        {"maximum time", [](SimulationOptions& o, double v) { o.setMaxSimulationTime(v); },
         allRefused("the maximum simulation time")},
        // setMaximumStepAngle() clamps to 1 ... 20 degrees.
        {"step angle", [](SimulationOptions& o, double v) { o.setMaximumStepAngle(v); },
         infinitiesClamped("the maximum step angle")},
        {"recovery speed", [](SimulationOptions& o, double v) { o.setRecoverySpeedWarning(v); },
         allRefused("the recovery speed warning threshold")},
        {"drogue low speed", [](SimulationOptions& o, double v) { o.setDrogueLowSpeedWarning(v); },
         allRefused("the drogue low speed warning threshold")},
        {"main high speed",
         [](SimulationOptions& o, double v) { o.setRecoveryDrogueMainHighSpeedWarning(v); },
         allRefused("the main high speed warning threshold")},
        {"main low speed",
         [](SimulationOptions& o, double v) { o.setRecoveryDrogueMainLowSpeedWarning(v); },
         allRefused("the main low speed warning threshold")},
    };
    expectOutcomes(cases);
}

// What the run does not read is not refused: the wind model that is not in use, the launch
// conditions under the ISA atmosphere, the constant gravity under the WGS model, the stored
// rod direction of a launch into the wind.
TEST(SimulationInputValidation, AValueTheRunDoesNotReadIsNotRefused)
{
    const std::vector<std::function<void(SimulationOptions&)>> changes{
        [](SimulationOptions& o) {
            o.setWindModelType(WindModelType::MULTI_LEVEL);
            o.getAverageWindModel().setAverage(kNaN);
        },
        [](SimulationOptions& o) {
            QtRocket::MultiLevelPinkNoiseWindModel& wind = o.getMultiLevelWindModel();
            EXPECT_TRUE(wind.addWindLevel(500.0, kInf, 0.0).has_value());
            o.setWindModelType(WindModelType::AVERAGE);
        },
        [](SimulationOptions& o) {
            o.setIsaAtmosphere(true);
            o.setLaunchTemperature(kNaN);
            o.setLaunchPressure(kInf);
            o.setLaunchRelativeHumidity(-kInf);
        },
        [](SimulationOptions& o) {
            o.setGravityModelType(GravityModelType::WGS);
            o.setConstantGravity(kNaN);
        },
        [](SimulationOptions& o) {
            o.setLaunchRodDirection(kNaN);
            o.setLaunchIntoWind(true);
        },
    };
    for (std::size_t i = 0; i < changes.size(); i++)
    {
        SCOPED_TRACE(i);
        AlphaRun run;
        changes[i](run.simulation.getOptions());
        EXPECT_TRUE(run.simulation.validateInputs().has_value());
        EXPECT_EQ(outcomeOf(run.simulation), "ok");
    }
}

/// The outcome of a simulation of the Alpha III whose options were made of a preferences store
/// in which the number of key @p key is @p value, as the .ork reader makes the options of a
/// simulation before it applies what the file holds.
[[nodiscard]] std::string outcomeWithPreference(std::string_view key, double value)
{
    QtRocket::InMemoryPreferences preferences;
    preferences.putDouble(key, value);
    TestEstesAlphaIII alpha;
    Simulation simulation(nullptr, *alpha.rocket, Simulation::Status::NOT_SIMULATED, "from a file",
                          SimulationOptions(preferences), {}, nullptr);
    simulation.setFlightConfigurationId(testFcid(0));
    return outcomeOf(simulation);
}

// The setters of the rod angle, the latitude, the longitude and the altitude never store an
// infinity, but the options need not have their values from a setter: a SimulationOptions made
// of a preferences store takes the launch rod, the launch site, the launch conditions and the
// time limits as the store holds them. The validation looks at the values, however they came.
TEST(SimulationInputValidation, ValuesTakenFromThePreferencesAreCheckedToo)
{
    namespace Keys = QtRocket::PreferenceKeys;
    struct PreferenceCase
    {
        std::string_view key;
        std::string_view what;
    };
    const std::vector<PreferenceCase> cases{
        {.key = Keys::kLaunchRodLength, .what = "the launch rod length"},
        {.key = Keys::kLaunchRodAngle, .what = "the launch rod angle"},
        {.key = Keys::kLaunchLatitude, .what = "the launch latitude"},
        {.key = Keys::kLaunchLongitude, .what = "the launch longitude"},
        {.key = Keys::kLaunchAltitude, .what = "the launch altitude"},
        {.key = Keys::kSimulationTimeStep, .what = "the time step"},
        {.key = Keys::kSimulationMaxTime, .what = "the maximum simulation time"},
    };
    for (const PreferenceCase& preferenceCase : cases)
    {
        SCOPED_TRACE(preferenceCase.key);
        EXPECT_EQ(outcomeWithPreference(preferenceCase.key, kNaN),
                  refusal(preferenceCase.what, "NaN"));
        EXPECT_EQ(outcomeWithPreference(preferenceCase.key, kInf),
                  refusal(preferenceCase.what, "Infinity"));
        EXPECT_EQ(outcomeWithPreference(preferenceCase.key, -kInf),
                  refusal(preferenceCase.what, "-Infinity"));
    }
    // A store of valid numbers gives a simulation that flies.
    EXPECT_EQ(outcomeWithPreference(Keys::kLaunchRodAngle, 0.1), "ok");
}

// ------------------------------------------------------------------------------- the ranges

/// The message of validateInputs() for the value @p value of an input that is called @p what
/// and has the range @p range: "valid" inside the range, the refusal outside it.
[[nodiscard]] std::string expectedOf(std::string_view what, const Range& range, double value)
{
    if (range.contains(value))
    {
        return "valid";
    }
    return std::format("Cannot simulate: {} is outside its range, {} ({}).", what, range.text,
                       QtRocket::Strings::javaDoubleToString(value));
}

/// The outcome of simulate() for the value @p value of the input @p what outside its range
/// @p range, as outcomeOf() tells it.
[[nodiscard]] std::string outside(std::string_view what, const Range& range, double value)
{
    return "refused: " + expectedOf(what, range, value);
}

/// What a range is tried with: its two bounds and the doubles next to each of them, inside
/// the range and outside it.
[[nodiscard]] std::vector<double> boundsOf(const Range& range)
{
    return {std::nextafter(range.min, -kInf), range.min, std::nextafter(range.min, kInf),
            std::nextafter(range.max, -kInf), range.max, std::nextafter(range.max, kInf)};
}

/// An input that has a range: what a refusal calls it, its range, and the message of
/// validateInputs() ("valid" when it passes) of a simulation that got the input with a value.
struct RangedInput
{
    using Validate = std::function<std::string(double)>;

    std::string  what;
    const Range* range;
    Validate     validate;
    /// Whether the setter of the input makes a value below the range one inside it (the
    /// magnitude of a negative wind speed, no deviation for a negative one): the value below
    /// the lower bound is then not tried.
    bool nothingBelow{false};

    RangedInput(std::string name, const Range& itsRange, Validate validation,
                bool noValueBelow = false)
      : what(std::move(name)),
        range(&itsRange),
        validate(std::move(validation)),
        nothingBelow(noValueBelow)
    {
    }
};

/// What validateInputs() says otherwise than the ranges of @p inputs have it, for each of the
/// values of boundsOf(): "<input> = <value>: <message>".
[[nodiscard]] std::vector<std::string> wrongAtTheBounds(const std::vector<RangedInput>& inputs)
{
    std::vector<std::string> wrong;
    for (const RangedInput& input : inputs)
    {
        for (const double value : boundsOf(*input.range))
        {
            if (input.nothingBelow && value < input.range->min)
            {
                continue;
            }
            const std::string message = input.validate(value);
            if (message != expectedOf(input.what, *input.range, value))
            {
                wrong.push_back(std::format("{} = {}: {}", input.what,
                                            QtRocket::Strings::javaDoubleToString(value), message));
            }
        }
    }
    return wrong;
}

/// The message of validateInputs() of the Alpha III's simulation after @p set had its options
/// with @p value.
[[nodiscard]] std::string validationWith(const std::function<void(SimulationOptions&, double)>& set,
                                         double value)
{
    AlphaRun run;
    set(run.simulation.getOptions(), value);
    return validationMessage(run.simulation);
}

/// A RangedInput of the options: @p set gives the options the value.
[[nodiscard]] RangedInput option(std::string what, const Range& range,
                                 std::function<void(SimulationOptions&, double)> set,
                                 bool nothingBelow = false)
{
    return {std::move(what), range,
            [set = std::move(set)](double value) { return validationWith(set, value); },
            nothingBelow};
}

void setTimeStepOf(SimulationOptions& o, double v)
{
    o.setTimeStep(v);
}

/// The numbers of the options that a setter stores as they are given.
[[nodiscard]] std::vector<RangedInput> rangedOptions()
{
    const auto withoutIsa = [](SimulationOptions& o) { o.setIsaAtmosphere(false); };
    return {
        option("the launch rod length", Ranges::kLaunchRodLength,
               [](SimulationOptions& o, double v) {
                   o.setLaunchRodLength(5.0);
                   o.setLaunchRodLength(v);
               }),
        option("the launch temperature", Ranges::kLaunchTemperature,
               [withoutIsa](SimulationOptions& o, double v) {
                   withoutIsa(o);
                   o.setLaunchTemperature(v);
               }),
        option("the launch pressure", Ranges::kLaunchPressure,
               [withoutIsa](SimulationOptions& o, double v) {
                   withoutIsa(o);
                   o.setLaunchPressure(v);
               }),
        option("the launch relative humidity", Ranges::kLaunchRelativeHumidity,
               [withoutIsa](SimulationOptions& o, double v) {
                   withoutIsa(o);
                   // First another value: a setter stores nothing that equals what it has
                   // (MathUtil::equals), and the humidity starts at 0.
                   o.setLaunchRelativeHumidity(0.5);
                   o.setLaunchRelativeHumidity(v);
               }),
        option("the constant gravity", Ranges::kConstantGravity,
               [](SimulationOptions& o, double v) {
                   o.setGravityModelType(GravityModelType::CONSTANT);
                   o.setConstantGravity(v);
               }),
        option("the time step", Ranges::kTimeStep,
               [](SimulationOptions& o, double v) { o.setTimeStep(v); }),
        option("the maximum simulation time", Ranges::kMaxSimulationTime,
               [](SimulationOptions& o, double v) { o.setMaxSimulationTime(v); }),
        option("the recovery speed warning threshold", Ranges::kRecoverySpeedThreshold,
               [](SimulationOptions& o, double v) { o.setRecoverySpeedWarning(v); }),
        option("the drogue low speed warning threshold", Ranges::kRecoverySpeedThreshold,
               [](SimulationOptions& o, double v) { o.setDrogueLowSpeedWarning(v); }),
        option("the main high speed warning threshold", Ranges::kRecoverySpeedThreshold,
               [](SimulationOptions& o, double v) { o.setRecoveryDrogueMainHighSpeedWarning(v); }),
        option("the main low speed warning threshold", Ranges::kRecoverySpeedThreshold,
               [](SimulationOptions& o, double v) { o.setRecoveryDrogueMainLowSpeedWarning(v); }),
    };
}

/// The launch temperature of options that keep the ISA atmosphere, which a run then does not
/// read.
void setTemperatureUnderIsa(SimulationOptions& o, double v)
{
    o.setLaunchTemperature(v);
}

/// The constant gravity of options that keep the WGS model, which a run then does not read.
void setGravityUnderWgs(SimulationOptions& o, double v)
{
    o.setConstantGravity(v);
}

TEST(SimulationInputValidation, TheRangesOfTheOptions)
{
    EXPECT_EQ(wrongAtTheBounds(rangedOptions()), std::vector<std::string>{});

    // The time step starts at a ten thousandth of a second: the bound is inside the range, the
    // double below it is not, and neither is 0.
    EXPECT_FALSE(Ranges::kTimeStep.minExcluded);
    EXPECT_EQ(validationWith(setTimeStepOf, 0.0001), "valid");
    EXPECT_EQ(validationWith(setTimeStepOf, std::nextafter(0.0001, 0.0)),
              "Cannot simulate: the time step is outside its range, from 0.0001 to 10 s "
              "(9.999999999999999E-5).");
    EXPECT_EQ(validationWith(setTimeStepOf, 0.0),
              "Cannot simulate: the time step is outside its range, from 0.0001 to 10 s (0.0).");
    EXPECT_EQ(validationWith(setTimeStepOf, 10.0), "valid");
    EXPECT_EQ(validationWith(setTimeStepOf, std::nextafter(10.0, 11.0)),
              "Cannot simulate: the time step is outside its range, from 0.0001 to 10 s "
              "(10.000000000000002).");
    // What a run does not read has no range to keep to either.
    EXPECT_EQ(validationWith(setTemperatureUnderIsa, -5.0), "valid");
    EXPECT_EQ(validationWith(setGravityUnderWgs, -1.0), "valid");
}

/// The inputs of the two wind models that a setter stores as they are given, but for a value
/// below the range: a wind model takes a negative speed as its magnitude from the other side
/// and a negative standard deviation as none.
[[nodiscard]] std::vector<RangedInput> rangedWindInputs()
{
    return {
        option(
            "the average wind speed", Ranges::kWindSpeed,
            [](SimulationOptions& o, double v) {
                o.getAverageWindModel().setAveragePreservingStandardDeviation(v);
            },
            true),
        option(
            "the standard deviation of the wind speed", Ranges::kWindSpeedDeviation,
            [](SimulationOptions& o, double v) { o.getAverageWindModel().setStandardDeviation(v); },
            true),
        option("the altitude of wind level 2", Ranges::kWindLevelAltitude,
               [](SimulationOptions& o, double v) { secondWindLevel(o).setAltitude(v); }),
        option(
            "the wind speed of wind level 2", Ranges::kWindSpeed,
            [](SimulationOptions& o, double v) {
                secondWindLevel(o).setSpeedPreservingStandardDeviation(v);
            },
            true),
        option(
            "the standard deviation of the wind speed of wind level 2", Ranges::kWindSpeedDeviation,
            [](SimulationOptions& o, double v) { secondWindLevel(o).setStandardDeviation(v); },
            true),
    };
}

/// An average wind of 500 m/s with a standard deviation of 1000 m/s, and then the speed @p v:
/// the speed takes the turbulence intensity, 2, along.
void setSpeedThatTheDeviationFollows(SimulationOptions& o, double v)
{
    o.getAverageWindModel().setAverage(500.0);
    o.getAverageWindModel().setStandardDeviation(1000.0);
    o.getAverageWindModel().setAverage(v);
}

void setAverageWindDirection(SimulationOptions& o, double v)
{
    o.getAverageWindModel().setDirection(v);
}

void setAverageWindDirectionLaunchingIntoIt(SimulationOptions& o, double v)
{
    o.setLaunchIntoWind(true);
    o.getAverageWindModel().setDirection(v);
}

void setWindLevelDirection(SimulationOptions& o, double v)
{
    secondWindLevel(o).setDirection(v);
}

void setRodDirection(SimulationOptions& o, double v)
{
    o.setLaunchIntoWind(false);
    o.setLaunchRodDirection(v);
}

// The two wind models. Below the range of a speed or a standard deviation there is nothing a
// setter would store, and a wind model reduces every direction to one turn: a direction leaves
// its range only as the nonsense that a huge number is reduced to.
TEST(SimulationInputValidation, TheRangesOfTheWind)
{
    EXPECT_EQ(wrongAtTheBounds(rangedWindInputs()), std::vector<std::string>{});

    // A speed takes the turbulence intensity along: the standard deviation that follows it
    // out of its range is what is named.
    EXPECT_EQ(validationWith(setSpeedThatTheDeviationFollows, 500.0), "valid");
    EXPECT_EQ(validationWith(setSpeedThatTheDeviationFollows, 600.0),
              expectedOf("the standard deviation of the wind speed", Ranges::kWindSpeedDeviation,
                         1200.0));

    // MathUtil::reduce2Pi() of 1.7e308 is about -2e292: no direction.
    const double      nonsense = QtRocket::MathUtil::reduce2Pi(1.7e308);
    const std::string refused  = expectedOf("the wind direction", Ranges::kDirection, nonsense);
    ASSERT_NE(refused, "valid");
    EXPECT_EQ(validationWith(setAverageWindDirection, 1.7e308), refused);
    EXPECT_EQ(validationWith(setAverageWindDirection, -7.0), "valid");
    EXPECT_EQ(validationWith(setAverageWindDirection, 2 * std::numbers::pi), "valid");
    // Launching into such a wind, the wind is what is named, not the rod.
    EXPECT_EQ(validationWith(setAverageWindDirectionLaunchingIntoIt, 1.7e308), refused);
    EXPECT_EQ(validationWith(setWindLevelDirection, 1.7e308),
              expectedOf("the wind direction of wind level 2", Ranges::kDirection, nonsense));
    // The launch rod has its direction reduced in the same way.
    EXPECT_EQ(validationWith(setRodDirection, 1.7e308),
              expectedOf("the launch rod direction", Ranges::kDirection, nonsense));
}

void setRodAngle(SimulationOptions& o, double v)
{
    o.setLaunchRodAngle(v);
}

void setLatitude(SimulationOptions& o, double v)
{
    o.setLaunchLatitude(v);
}

void setLongitude(SimulationOptions& o, double v)
{
    o.setLaunchLongitude(v);
}

void setStepAngle(SimulationOptions& o, double v)
{
    o.setMaximumStepAngle(v);
}

void setAltitude(SimulationOptions& o, double v)
{
    o.setLaunchAltitude(v);
}

/// What validateInputs() says of the Alpha III's simulation after @p set had its options with
/// 1e300, with -1e300 and with 0, each in a new simulation.
[[nodiscard]] std::vector<std::string> validationsWithExtremes(
    const std::function<void(SimulationOptions&, double)>& set)
{
    return {validationWith(set, 1e300), validationWith(set, -1e300), validationWith(set, 0.0)};
}

// The setters of the options keep the launch rod angle, the latitude, the longitude and the
// maximum step angle inside their ranges, and the launch altitude below the top of its range:
// whatever they are given, the simulation is valid. The ranges of the two angles are what the
// setters clamp to (a static_assert of SimulationOptions.cpp holds them together).
TEST(SimulationInputValidation, TheSettersThatClampKeepTheirOptionsInRange)
{
    const std::vector<std::string> valid(3, "valid");
    EXPECT_EQ(validationsWithExtremes(setRodAngle), valid);
    EXPECT_EQ(validationsWithExtremes(setLatitude), valid);
    EXPECT_EQ(validationsWithExtremes(setLongitude), valid);
    EXPECT_EQ(validationsWithExtremes(setStepAngle), valid);

    EXPECT_EQ(validationWith(setAltitude, 1e300), "valid");
    EXPECT_EQ(validationWith(setAltitude, -1000.0), "valid");
    EXPECT_EQ(validationWith(setAltitude, std::nextafter(-1000.0, -2000.0)),
              expectedOf("the launch altitude", Ranges::kLaunchAltitude,
                         std::nextafter(-1000.0, -2000.0)));
    EXPECT_EQ(Ranges::kLaunchRodAngle.max, SimulationOptions::kMaxLaunchRodAngle);
    EXPECT_EQ(Ranges::kMaximumStepAngle.min, std::numbers::pi / 180);
    EXPECT_EQ(Ranges::kMaximumStepAngle.max, 20 * std::numbers::pi / 180);
}

/// The message of validateInputs() of a simulation of the Alpha III whose options were made of
/// a preferences store in which the number of key @p key is @p value.
[[nodiscard]] std::string validationWithPreference(std::string_view key, double value)
{
    QtRocket::InMemoryPreferences preferences;
    preferences.putDouble(key, value);
    TestEstesAlphaIII alpha;
    Simulation simulation(nullptr, *alpha.rocket, Simulation::Status::NOT_SIMULATED, "from a file",
                          SimulationOptions(preferences), {}, nullptr);
    simulation.setFlightConfigurationId(testFcid(0));
    return validationMessage(simulation);
}

/// A RangedInput whose value comes from the key @p key of a preferences store.
[[nodiscard]] RangedInput preference(std::string what, const Range& range, std::string_view key)
{
    return {std::move(what), range,
            [key](double value) { return validationWithPreference(key, value); }};
}

// The options need not have their values from a setter: a SimulationOptions made of a
// preferences store takes them as the store holds them, the ones a setter would clamp too.
TEST(SimulationInputValidation, TheRangesOfValuesTakenFromThePreferences)
{
    namespace Keys = QtRocket::PreferenceKeys;
    const std::vector<RangedInput> inputs{
        preference("the launch rod length", Ranges::kLaunchRodLength, Keys::kLaunchRodLength),
        preference("the launch rod angle", Ranges::kLaunchRodAngle, Keys::kLaunchRodAngle),
        preference("the launch latitude", Ranges::kLaunchLatitude, Keys::kLaunchLatitude),
        preference("the launch longitude", Ranges::kLaunchLongitude, Keys::kLaunchLongitude),
        preference("the launch altitude", Ranges::kLaunchAltitude, Keys::kLaunchAltitude),
        preference("the time step", Ranges::kTimeStep, Keys::kSimulationTimeStep),
    };
    EXPECT_EQ(wrongAtTheBounds(inputs), std::vector<std::string>{});

    // The time step of OpenRocket's own test preferences, 0, is one no simulation takes.
    EXPECT_EQ(outcomeWithPreference(Keys::kSimulationTimeStep, 0.0),
              outside("the time step", Ranges::kTimeStep, 0.0));
}

/// A drag table whose rows are those of @p built, as the options hold one.
void setDragRows(SimulationOptions& o, const MachAoALookup::Builder& built)
{
    Result<MachAoALookup> table = built.build();
    if (table.has_value())
    {
        o.setDragLookup(std::nullopt, std::make_shared<const MachAoALookup>(std::move(*table)));
    }
}

/// A stability table whose rows are those of @p built, as the options hold one.
void setStabilityRows(SimulationOptions& o, const MachAoALookup::Builder& built)
{
    Result<MachAoALookup> table = built.build();
    if (table.has_value())
    {
        o.setStabilityLookup(std::nullopt,
                             std::make_shared<const MachAoALookup>(std::move(*table)));
    }
}

// Every number of a lookup table in use: a Mach number, an angle of attack, a coefficient and
// the centre of pressure, each in its own range.
TEST(SimulationInputValidation, TheRangesOfTheLookupTables)
{
    const std::vector<RangedInput> inputs{
        option("a Mach number of the drag lookup table", Ranges::kLookupMach,
               [](SimulationOptions& o, double v) {
                   setDragRows(o, MachAoALookup::dragBuilder().addDragData(v, 0.5));
               }),
        option("an angle of attack of the drag lookup table at Mach 1.0",
               Ranges::kLookupAngleOfAttack,
               [](SimulationOptions& o, double v) {
                   setDragRows(o, MachAoALookup::dragBuilder().addDragData(1.0, v, 0.5));
               }),
        option("the 'cd' of the drag lookup table at Mach 1.0", Ranges::kLookupCoefficient,
               [](SimulationOptions& o, double v) {
                   setDragRows(o, MachAoALookup::dragBuilder().addDragData(1.0, v));
               }),
        option("the 'cn' of the stability lookup table at Mach 1.0", Ranges::kLookupCoefficient,
               [](SimulationOptions& o, double v) {
                   setStabilityRows(
                       o, MachAoALookup::stabilityBuilder().addStabilityData(1.0, v, 1.0, 0.25));
               }),
        option("the 'cm' of the stability lookup table at Mach 1.0", Ranges::kLookupCoefficient,
               [](SimulationOptions& o, double v) {
                   setStabilityRows(
                       o, MachAoALookup::stabilityBuilder().addStabilityData(1.0, 2.0, v, 0.25));
               }),
        option("the 'cp' of the stability lookup table at Mach 1.0",
               Ranges::kLookupCentreOfPressure,
               [](SimulationOptions& o, double v) {
                   setStabilityRows(
                       o, MachAoALookup::stabilityBuilder().addStabilityData(1.0, 2.0, 1.0, v));
               }),
    };
    EXPECT_EQ(wrongAtTheBounds(inputs), std::vector<std::string>{});

    // The first number of the table that may not be read is the one named, in the table's
    // order, whether it is not finite or outside its range.
    AlphaRun run;
    setDragRows(run.simulation.getOptions(), MachAoALookup::dragBuilder()
                                                 .addDragData(0.5, 0.4)
                                                 .addDragData(1.0, 1e300)
                                                 .addDragData(2.0, kNaN));
    EXPECT_EQ(validationMessage(run.simulation),
              expectedOf("the 'cd' of the drag lookup table at Mach 1.0",
                         Ranges::kLookupCoefficient, 1e300));
    setDragRows(run.simulation.getOptions(),
                MachAoALookup::dragBuilder().addDragData(0.5, kInf).addDragData(1.0, 1e300));
    EXPECT_EQ(validationMessage(run.simulation),
              "Cannot simulate: the 'cd' of the drag lookup table at Mach 0.5 is not finite "
              "(Infinity).");
}

void setAverageWindSpeed(SimulationOptions& o, double v)
{
    o.getAverageWindModel().setAverage(v);
}

void setDeviationOfAWindOfTwo(SimulationOptions& o, double v)
{
    o.getAverageWindModel().setAverage(2.0);
    o.getAverageWindModel().setStandardDeviation(v);
}

void setWindLevelSpeed(SimulationOptions& o, double v)
{
    secondWindLevel(o).setSpeed(v);
}

void setDragCoefficientAtMachOne(SimulationOptions& o, double v)
{
    setDragRows(o, MachAoALookup::dragBuilder().addDragData(0.0, 0.5).addDragData(1.0, v));
}

/// The row OpenRocket's reader takes without a word: "5,<v>,-<v>,<v>".
void setStabilityRowOfHugeNumbers(SimulationOptions& o, double v)
{
    setStabilityRows(o, MachAoALookup::stabilityBuilder()
                            .addStabilityData(0.0, 2.0, 1.0, 0.25)
                            .addStabilityData(5.0, v, -v, v));
}

// What the earlier runs of the port found a finite number to do, each of which is now an
// ordinary error of simulate(), before anything runs: a time step of 1e300 s (the first step
// did not end, and no stop request reached it), a time step of zero, of less or of a millionth
// of a second (the steppers start from a millisecond, but cut a step down to a twentieth of the
// time step as it is given whenever a limit binds: a run of millions of rows, and with 0 one
// that does not end), a wind of 1e300 m/s (a BugError), and a lookup table with a cell of
// 1e300 (the BugError of a NaN in the stepper).
TEST(SimulationInputValidation, TheAbsurdNumbersOfTheEarlierRunsAreOrdinaryErrors)
{
    constexpr double kLargest = std::numeric_limits<double>::max();
    EXPECT_EQ(outcomeWith(setTimeStepOf, 1e300),
              outside("the time step", Ranges::kTimeStep, 1e300));
    EXPECT_EQ(outcomeWith(setTimeStepOf, kLargest),
              outside("the time step", Ranges::kTimeStep, kLargest));
    EXPECT_EQ(outcomeWith(setTimeStepOf, 0.0), outside("the time step", Ranges::kTimeStep, 0.0));
    EXPECT_EQ(outcomeWith(setTimeStepOf, -0.05),
              outside("the time step", Ranges::kTimeStep, -0.05));
    EXPECT_EQ(outcomeWith(setTimeStepOf, 0.000001),
              "refused: Cannot simulate: the time step is outside its range, from 0.0001 to 10 s "
              "(1.0E-6).");
    EXPECT_EQ(
        outcomeWith(setTimeStepOf, std::numeric_limits<double>::denorm_min()),
        outside("the time step", Ranges::kTimeStep, std::numeric_limits<double>::denorm_min()));

    EXPECT_EQ(outcomeWith(setAverageWindSpeed, 1e300),
              outside("the average wind speed", Ranges::kWindSpeed, 1e300));
    EXPECT_EQ(
        outcomeWith(setDeviationOfAWindOfTwo, 1e300),
        outside("the standard deviation of the wind speed", Ranges::kWindSpeedDeviation, 1e300));
    EXPECT_EQ(outcomeWith(setWindLevelSpeed, 1e300),
              outside("the wind speed of wind level 2", Ranges::kWindSpeed, 1e300));

    EXPECT_EQ(outcomeWith(setDragCoefficientAtMachOne, 1e300),
              outside("the 'cd' of the drag lookup table at Mach 1.0", Ranges::kLookupCoefficient,
                      1e300));
    EXPECT_EQ(outcomeWith(setStabilityRowOfHugeNumbers, 1e300),
              outside("the 'cn' of the stability lookup table at Mach 5.0",
                      Ranges::kLookupCoefficient, 1e300));
}

// ------------------------------------------------------------------------------- the design

/// A simulation of the Estes Alpha III with one value of its design set: the kinds of value.
enum class AlphaValue
{
    IGNITION_DELAY,
    EJECTION_DELAY,
    IGNITION_DELAY_OF_ANOTHER_CONFIGURATION,
    DEPLOYMENT_DELAY,
    DEPLOYMENT_ALTITUDE,        ///< under the event of the design, the ejection charge
    DEPLOYMENT_AT_AN_ALTITUDE,  ///< the altitude, with the parachute deploying at it
};

/// Sets the value @p which of the design of @p run to @p value.
void setAlphaValue(const AlphaRun& run, AlphaValue which, double value)
{
    switch (which)
    {
        case AlphaValue::IGNITION_DELAY:
            run.alpha.inner->getMotorConfig(testFcid(0)).setIgnitionDelay(value);
            return;
        case AlphaValue::EJECTION_DELAY:
            run.alpha.inner->getMotorConfig(testFcid(0)).setEjectionDelay(value);
            return;
        case AlphaValue::IGNITION_DELAY_OF_ANOTHER_CONFIGURATION:
            run.alpha.inner->getMotorConfig(testFcid(3)).setIgnitionDelay(value);
            return;
        case AlphaValue::DEPLOYMENT_DELAY:
            run.alpha.chute->getDeploymentConfigurations().get(testFcid(0)).setDeployDelay(value);
            return;
        case AlphaValue::DEPLOYMENT_ALTITUDE:
            run.alpha.chute->getDeploymentConfigurations()
                .get(testFcid(0))
                .setDeployAltitude(value);
            return;
        case AlphaValue::DEPLOYMENT_AT_AN_ALTITUDE:
        {
            QtRocket::DeploymentConfiguration& deployment =
                run.alpha.chute->getDeploymentConfigurations().get(testFcid(0));
            deployment.setDeployEvent(DeployEvent::ALTITUDE);
            deployment.setDeployAltitude(value);
            return;
        }
    }
}

/// The outcome of the Alpha III's simulation with the value @p which of its design set to
/// @p value.
[[nodiscard]] std::string outcomeWithDesign(AlphaValue which, double value)
{
    AlphaRun run;
    setAlphaValue(run, which, value);
    return outcomeOf(run.simulation);
}

TEST(SimulationInputValidation, TheDelaysOfAMotor)
{
    const std::string_view ignitionName = "the ignition delay of the motor in 'Motor Mount Tube'";
    EXPECT_EQ(outcomeWithDesign(AlphaValue::IGNITION_DELAY, kNaN), refusal(ignitionName, "NaN"));
    EXPECT_EQ(outcomeWithDesign(AlphaValue::IGNITION_DELAY, kInf),
              refusal(ignitionName, "Infinity"));
    EXPECT_EQ(outcomeWithDesign(AlphaValue::IGNITION_DELAY, -kInf),
              refusal(ignitionName, "-Infinity"));

    const std::string_view ejectionName = "the ejection delay of the motor in 'Motor Mount Tube'";
    EXPECT_EQ(outcomeWithDesign(AlphaValue::EJECTION_DELAY, kNaN), refusal(ejectionName, "NaN"));
    EXPECT_EQ(outcomeWithDesign(AlphaValue::EJECTION_DELAY, -kInf),
              refusal(ejectionName, "-Infinity"));
    // +infinity is the plugged delay: a motor without an ejection charge. The flight has none,
    // and the parachute, which waits for one, stays in.
    ASSERT_EQ(QtRocket::Motor::kPluggedDelay, kInf);
    EXPECT_EQ(outcomeWithDesign(AlphaValue::EJECTION_DELAY, QtRocket::Motor::kPluggedDelay), "ok");

    // The motor of another configuration is not this simulation's.
    EXPECT_EQ(outcomeWithDesign(AlphaValue::IGNITION_DELAY_OF_ANOTHER_CONFIGURATION, kNaN), "ok");
}

TEST(SimulationInputValidation, TheDeploymentOfARecoveryDevice)
{
    const std::string_view delayName = "the deployment delay of 'Parachute'";
    EXPECT_EQ(outcomeWithDesign(AlphaValue::DEPLOYMENT_DELAY, kNaN), refusal(delayName, "NaN"));
    EXPECT_EQ(outcomeWithDesign(AlphaValue::DEPLOYMENT_DELAY, kInf),
              refusal(delayName, "Infinity"));
    EXPECT_EQ(outcomeWithDesign(AlphaValue::DEPLOYMENT_DELAY, -kInf),
              refusal(delayName, "-Infinity"));

    // A device that deploys at an altitude: the engine compares that altitude with every step
    // of the descent.
    const std::string_view altitudeName = "the deployment altitude of 'Parachute'";
    EXPECT_EQ(outcomeWithDesign(AlphaValue::DEPLOYMENT_AT_AN_ALTITUDE, kNaN),
              refusal(altitudeName, "NaN"));
    EXPECT_EQ(outcomeWithDesign(AlphaValue::DEPLOYMENT_AT_AN_ALTITUDE, kInf),
              refusal(altitudeName, "Infinity"));
    EXPECT_EQ(outcomeWithDesign(AlphaValue::DEPLOYMENT_AT_AN_ALTITUDE, -kInf),
              refusal(altitudeName, "-Infinity"));
    EXPECT_EQ(outcomeWithDesign(AlphaValue::DEPLOYMENT_AT_AN_ALTITUDE, 50.0), "ok");
}

/// The deployment events with which validateInputs() refuses the Alpha III whose parachute has
/// the deployment altitude @p altitude.
[[nodiscard]] std::vector<DeployEvent> deployEventsRefusedWith(double altitude)
{
    std::vector<DeployEvent> refused;
    for (const DeployEvent event : QtRocket::DeploymentConfiguration::kAllDeployEvents)
    {
        const AlphaRun                     run;
        QtRocket::DeploymentConfiguration& deployment =
            run.alpha.chute->getDeploymentConfigurations().get(testFcid(0));
        deployment.setDeployEvent(event);
        deployment.setDeployAltitude(altitude);
        if (validationMessage(run.simulation) != "valid")
        {
            refused.push_back(event);
        }
    }
    return refused;
}

/// What two runs of one design share when they are the same flight.
struct Flight
{
    double      maxAltitude{0};
    double      flightTime{0};
    std::size_t rows{0};
    std::size_t events{0};

    [[nodiscard]] bool operator==(const Flight&) const = default;
};

/// For a failed expectation.
std::ostream& operator<<(std::ostream& out, const Flight& flight)
{
    return out << std::format("maximum altitude {}, flight time {}, {} rows, {} events",
                              flight.maxAltitude, flight.flightTime, flight.rows, flight.events);
}

/// The flight @p simulation has simulated; all zeros without one.
[[nodiscard]] Flight flightOf(const Simulation& simulation)
{
    const std::shared_ptr<QtRocket::FlightData>& data = simulation.getSimulatedData();
    if (data == nullptr || data->getBranchCount() == 0)
    {
        return {};
    }
    return {.maxAltitude = data->getMaxAltitude(),
            .flightTime  = data->getFlightTime(),
            .rows        = data->getBranch(0).getLength(),
            .events      = data->getBranch(0).getEvents().size()};
}

// The engine reads the deployment altitude for one deployment event only, a given altitude
// during the descent. The parachute of the Alpha III opens at the ejection charge, and its
// flight with a deployment altitude that is no number is its flight with any other, as in
// OpenRocket, which flies such a design (UnreadAltitude.java: 133.004 m and the same eleven
// events with NaN, with -Infinity and with 200 m).
TEST(SimulationInputValidation, ADeploymentAltitudeThatTheEventDoesNotReadIsNotRefused)
{
    const std::vector<DeployEvent> onlyAtAnAltitude{DeployEvent::ALTITUDE};
    EXPECT_EQ(deployEventsRefusedWith(kNaN), onlyAtAnAltitude);
    EXPECT_EQ(deployEventsRefusedWith(kInf), onlyAtAnAltitude);
    EXPECT_EQ(deployEventsRefusedWith(-kInf), onlyAtAnAltitude);
    EXPECT_TRUE(deployEventsRefusedWith(200.0).empty());

    AlphaRun plain;
    ASSERT_EQ(plain.alpha.chute->getDeploymentConfigurations().get(testFcid(0)).getDeployEvent(),
              DeployEvent::EJECTION);
    ASSERT_EQ(outcomeOf(plain.simulation), "ok");
    const Flight flight = flightOf(plain.simulation);
    EXPECT_GT(flight.maxAltitude, 10.0);

    AlphaRun withNaN;
    setAlphaValue(withNaN, AlphaValue::DEPLOYMENT_ALTITUDE, kNaN);
    EXPECT_EQ(outcomeOf(withNaN.simulation), "ok");
    EXPECT_EQ(flightOf(withNaN.simulation), flight);

    AlphaRun withInfinity;
    setAlphaValue(withInfinity, AlphaValue::DEPLOYMENT_ALTITUDE, -kInf);
    EXPECT_EQ(outcomeOf(withInfinity.simulation), "ok");
    EXPECT_EQ(flightOf(withInfinity.simulation), flight);
}

/// A simulation of the two-stage Beta in its configuration (TEST_FCID_1), with the built-in
/// default options.
struct BetaRun
{
    TestBeta   beta;
    Simulation simulation;

    BetaRun() : simulation(*beta.rocket)
    {
        simulation.setFlightConfigurationId(testFcid(1));
        simulation.getOptions().setRandomSeed(0);
    }
};

/// A simulation of the Beta with one value of its design set: the kinds of value.
enum class BetaValue
{
    NONE,
    SEPARATION_DELAY,
    SEPARATION_ALTITUDE,
    SEPARATION_DELAY_OF_THE_SUSTAINER,
    IGNITION_DELAY_OF_THE_BOOSTER,
};

/// Sets the value @p which of the design of @p run to @p value.
void setBetaValue(const BetaRun& run, BetaValue which, double value)
{
    switch (which)
    {
        case BetaValue::NONE:
            return;
        case BetaValue::SEPARATION_DELAY:
            run.beta.boosterStage->getSeparationConfigurations()
                .get(testFcid(1))
                .setSeparationDelay(value);
            return;
        case BetaValue::SEPARATION_ALTITUDE:
            run.beta.boosterStage->getSeparationConfigurations()
                .get(testFcid(1))
                .setSeparationAltitude(value);
            return;
        case BetaValue::SEPARATION_DELAY_OF_THE_SUSTAINER:
            run.beta.stage->getSeparationConfigurations()
                .get(testFcid(1))
                .setSeparationDelay(value);
            return;
        case BetaValue::IGNITION_DELAY_OF_THE_BOOSTER:
            run.beta.boosterMmt->getMotorConfig(testFcid(1)).setIgnitionDelay(value);
            return;
    }
}

/// The outcome of the Beta's simulation with the value @p which of its design set to @p value.
[[nodiscard]] std::string outcomeWithBeta(BetaValue which, double value)
{
    BetaRun run;
    setBetaValue(run, which, value);
    return outcomeOf(run.simulation);
}

/// Makes the booster of @p run separate at @p event, with the separation altitude @p altitude.
void setSeparation(const BetaRun& run, SeparationEvent event, double altitude)
{
    QtRocket::StageSeparationConfiguration& separation =
        run.beta.boosterStage->getSeparationConfigurations().get(testFcid(1));
    separation.setSeparationEvent(event);
    separation.setSeparationAltitude(altitude);
}

/// The outcome of the Beta's simulation whose booster separates at @p event and has the
/// separation altitude @p altitude.
[[nodiscard]] std::string outcomeWithSeparation(SeparationEvent event, double altitude)
{
    BetaRun run;
    setSeparation(run, event, altitude);
    return outcomeOf(run.simulation);
}

/// The separation events with which validateInputs() refuses the Beta whose booster has the
/// separation altitude @p altitude.
[[nodiscard]] std::vector<SeparationEvent> separationEventsRefusedWith(double altitude)
{
    std::vector<SeparationEvent> refused;
    for (const SeparationEvent event : QtRocket::StageSeparationConfiguration::kAllSeparationEvents)
    {
        const BetaRun run;
        setSeparation(run, event, altitude);
        if (validationMessage(run.simulation) != "valid")
        {
            refused.push_back(event);
        }
    }
    return refused;
}

TEST(SimulationInputValidation, TheSeparationOfAStage)
{
    EXPECT_EQ(outcomeWithBeta(BetaValue::NONE, 0.0), "ok");

    const std::string_view delayName = "the separation delay of 'Booster Stage'";
    EXPECT_EQ(outcomeWithBeta(BetaValue::SEPARATION_DELAY, kNaN), refusal(delayName, "NaN"));
    EXPECT_EQ(outcomeWithBeta(BetaValue::SEPARATION_DELAY, kInf), refusal(delayName, "Infinity"));
    EXPECT_EQ(outcomeWithBeta(BetaValue::SEPARATION_DELAY, -kInf), refusal(delayName, "-Infinity"));

    // A stage that separates at an altitude, on the way up or on the way down: the engine
    // compares that altitude with every step.
    const std::string_view altitudeName = "the separation altitude of 'Booster Stage'";
    EXPECT_EQ(outcomeWithSeparation(SeparationEvent::ALTITUDE_ASCENDING, kNaN),
              refusal(altitudeName, "NaN"));
    EXPECT_EQ(outcomeWithSeparation(SeparationEvent::ALTITUDE_ASCENDING, kInf),
              refusal(altitudeName, "Infinity"));
    EXPECT_EQ(outcomeWithSeparation(SeparationEvent::ALTITUDE_DESCENDING, -kInf),
              refusal(altitudeName, "-Infinity"));

    // The topmost stage has nothing to separate from: the engine never reads its settings.
    EXPECT_EQ(outcomeWithBeta(BetaValue::SEPARATION_DELAY_OF_THE_SUSTAINER, kNaN), "ok");

    // The motor of the booster is one of the flight.
    EXPECT_EQ(outcomeWithBeta(BetaValue::IGNITION_DELAY_OF_THE_BOOSTER, kNaN),
              refusal("the ignition delay of the motor in 'Booster MMT'", "NaN"));
}

// The engine reads the separation altitude for the two separation events at an altitude only.
// The booster of the Beta separates at its ejection charge, and the flight with a separation
// altitude that is no number is the flight with any other, as in OpenRocket (UnreadAltitude.java:
// 539.28 m and fourteen events with NaN and with Infinity).
TEST(SimulationInputValidation, ASeparationAltitudeThatTheEventDoesNotReadIsNotRefused)
{
    const std::vector<SeparationEvent> onlyAtAnAltitude{SeparationEvent::ALTITUDE_ASCENDING,
                                                        SeparationEvent::ALTITUDE_DESCENDING};
    EXPECT_EQ(separationEventsRefusedWith(kNaN), onlyAtAnAltitude);
    EXPECT_EQ(separationEventsRefusedWith(kInf), onlyAtAnAltitude);
    EXPECT_EQ(separationEventsRefusedWith(-kInf), onlyAtAnAltitude);
    EXPECT_TRUE(separationEventsRefusedWith(100.0).empty());

    BetaRun plain;
    ASSERT_EQ(plain.beta.boosterStage->getSeparationConfigurations()
                  .get(testFcid(1))
                  .getSeparationEvent(),
              SeparationEvent::EJECTION);
    ASSERT_EQ(outcomeOf(plain.simulation), "ok");
    const Flight flight = flightOf(plain.simulation);
    EXPECT_GT(flight.maxAltitude, 10.0);

    BetaRun withNaN;
    setBetaValue(withNaN, BetaValue::SEPARATION_ALTITUDE, kNaN);
    EXPECT_EQ(outcomeOf(withNaN.simulation), "ok");
    EXPECT_EQ(flightOf(withNaN.simulation), flight);
}

// A stage that is not active in the simulated configuration does not fly, and nothing of it
// is read.
TEST(SimulationInputValidation, AStageThatIsNotActiveIsNotChecked)
{
    BetaRun run;
    run.beta.boosterMmt->getMotorConfig(testFcid(1)).setIgnitionDelay(kNaN);
    run.beta.boosterStage->getSeparationConfigurations().get(testFcid(1)).setSeparationDelay(kNaN);
    ASSERT_EQ(outcomeOf(run.simulation),
              refusal("the ignition delay of the motor in 'Booster MMT'", "NaN"));

    run.beta.rocket->getFlightConfiguration(testFcid(1)).setOnlyStage(0);
    EXPECT_TRUE(run.simulation.validateInputs().has_value());
    EXPECT_EQ(outcomeOf(run.simulation), "ok");
}

// --------------------------------------------------------------- the ranges of the design

/// The message of validateInputs() of the Alpha III's simulation with the value @p which of
/// its design set to @p value.
[[nodiscard]] std::string validationWithDesign(AlphaValue which, double value)
{
    const AlphaRun run;
    // First another value: the setters of a deployment store nothing that equals what they
    // have (MathUtil::equals), which the doubles next to 0 do.
    setAlphaValue(run, which, 5.0);
    setAlphaValue(run, which, value);
    return validationMessage(run.simulation);
}

/// The message of validateInputs() of the Beta's simulation whose booster has the separation
/// delay @p delay, or, with @p event, separates at that event with the altitude @p altitude.
[[nodiscard]] std::string validationWithSeparation(std::optional<SeparationEvent> event,
                                                   double                         value)
{
    const BetaRun run;
    if (event.has_value())
    {
        setSeparation(run, *event, 5.0);
        setSeparation(run, *event, value);
    }
    else
    {
        setBetaValue(run, BetaValue::SEPARATION_DELAY, 5.0);
        setBetaValue(run, BetaValue::SEPARATION_DELAY, value);
    }
    return validationMessage(run.simulation);
}

// The delays of the simulated configuration and the altitudes the engine reads from it.
TEST(SimulationInputValidation, TheRangesOfTheDesign)
{
    const auto design = [](AlphaValue which) {
        return [which](double value) { return validationWithDesign(which, value); };
    };
    const std::vector<RangedInput> inputs{
        {"the ignition delay of the motor in 'Motor Mount Tube'", Ranges::kDelay,
         design(AlphaValue::IGNITION_DELAY)},
        {"the ejection delay of the motor in 'Motor Mount Tube'", Ranges::kDelay,
         design(AlphaValue::EJECTION_DELAY)},
        {"the deployment delay of 'Parachute'", Ranges::kDelay,
         design(AlphaValue::DEPLOYMENT_DELAY)},
        {"the deployment altitude of 'Parachute'", Ranges::kEventAltitude,
         design(AlphaValue::DEPLOYMENT_AT_AN_ALTITUDE)},
        {"the separation delay of 'Booster Stage'", Ranges::kDelay,
         [](double value) { return validationWithSeparation(std::nullopt, value); }},
        {"the separation altitude of 'Booster Stage'", Ranges::kEventAltitude,
         [](double value) {
             return validationWithSeparation(SeparationEvent::ALTITUDE_ASCENDING, value);
         }},
        {"the separation altitude of 'Booster Stage'", Ranges::kEventAltitude,
         [](double value) {
             return validationWithSeparation(SeparationEvent::ALTITUDE_DESCENDING, value);
         }},
    };
    EXPECT_EQ(wrongAtTheBounds(inputs), std::vector<std::string>{});

    // The plugged delay, +infinity, is outside every range and valid all the same.
    EXPECT_EQ(validationWithDesign(AlphaValue::EJECTION_DELAY, QtRocket::Motor::kPluggedDelay),
              "valid");
    // A negative delay is refused when the simulation is run.
    EXPECT_EQ(
        outcomeWithDesign(AlphaValue::IGNITION_DELAY, -1.0),
        outside("the ignition delay of the motor in 'Motor Mount Tube'", Ranges::kDelay, -1.0));
    EXPECT_EQ(outcomeWithBeta(BetaValue::SEPARATION_DELAY, 1e300),
              outside("the separation delay of 'Booster Stage'", Ranges::kDelay, 1e300));
}

// An altitude that the event of its device or stage does not read has no range to keep to, as
// it need not be finite.
TEST(SimulationInputValidation, AnAltitudeOutsideItsRangeThatTheEventDoesNotReadIsNotRefused)
{
    const std::vector<DeployEvent>     onlyAtAnAltitude{DeployEvent::ALTITUDE};
    const std::vector<SeparationEvent> onlyAtAltitudes{SeparationEvent::ALTITUDE_ASCENDING,
                                                       SeparationEvent::ALTITUDE_DESCENDING};
    EXPECT_EQ(deployEventsRefusedWith(-120.0), onlyAtAnAltitude);
    EXPECT_EQ(deployEventsRefusedWith(1e300), onlyAtAnAltitude);
    EXPECT_EQ(separationEventsRefusedWith(-120.0), onlyAtAltitudes);
    EXPECT_EQ(separationEventsRefusedWith(1e300), onlyAtAltitudes);
    EXPECT_EQ(outcomeWithDesign(AlphaValue::DEPLOYMENT_ALTITUDE, -120.0), "ok");
    EXPECT_EQ(outcomeWithBeta(BetaValue::SEPARATION_ALTITUDE, 1e300), "ok");
}

// ----------------------------------------------------------------- the order and the state

// The first value that is not finite is the one named: the options in the order of the list
// in the comment of validateInputs(), then the design.
TEST(SimulationInputValidation, NamesTheFirstValueThatIsNotFinite)
{
    AlphaRun           run;
    SimulationOptions& o = run.simulation.getOptions();
    run.alpha.chute->getDeploymentConfigurations().get(testFcid(0)).setDeployDelay(kNaN);
    run.alpha.inner->getMotorConfig(testFcid(0)).setIgnitionDelay(kNaN);
    // The components in the order of the configuration's active components.
    EXPECT_EQ(validationMessage(run.simulation),
              "Cannot simulate: the ignition delay of the motor in 'Motor Mount Tube' "
              "is not finite (NaN).");
    o.setRecoveryDrogueMainLowSpeedWarning(kNaN);
    EXPECT_EQ(validationMessage(run.simulation),
              "Cannot simulate: the main low speed warning threshold is not finite "
              "(NaN).");
    o.setTimeStep(kInf);
    EXPECT_EQ(validationMessage(run.simulation),
              "Cannot simulate: the time step is not finite (Infinity).");
    setDragTable(o, MachAoALookup::dragBuilder().addDragData(0.5, kNaN));
    EXPECT_EQ(validationMessage(run.simulation),
              "Cannot simulate: the 'cd' of the drag lookup table at Mach 0.5 is not finite "
              "(NaN).");
    o.getAverageWindModel().setDirection(kNaN);
    EXPECT_EQ(validationMessage(run.simulation),
              "Cannot simulate: the wind direction is not finite (NaN).");
    o.setLaunchRodLength(-kInf);
    EXPECT_EQ(validationMessage(run.simulation),
              "Cannot simulate: the launch rod length is not finite (-Infinity).");

    // validateInputs() only looks.
    EXPECT_EQ(run.simulation.getStoredStatus(), Simulation::Status::NOT_SIMULATED);
    EXPECT_EQ(run.simulation.getSimulatedData(), nullptr);
}

// The design is gone through component by component, in the order of the configuration's
// active components, and not kind by kind: in the Beta the parachute of the sustainer comes
// before the motor mount of the booster, so it is the one named. The stages follow the
// components, and the extensions the design.
TEST(SimulationInputValidation, NamesTheComponentsInTheOrderOfTheConfiguration)
{
    BetaRun                            run;
    const std::shared_ptr<RollControl> roll = std::make_shared<RollControl>();
    roll->setKP(kNaN);
    run.simulation.getSimulationExtensions().push_back(roll);
    EXPECT_EQ(validationMessage(run.simulation),
              "Cannot simulate: the 'KP' of the simulation extension 'Roll Control' is not "
              "finite (NaN).");

    setBetaValue(run, BetaValue::SEPARATION_DELAY, kInf);
    EXPECT_EQ(validationMessage(run.simulation),
              "Cannot simulate: the separation delay of 'Booster Stage' is not finite "
              "(Infinity).");

    setBetaValue(run, BetaValue::IGNITION_DELAY_OF_THE_BOOSTER, kNaN);
    EXPECT_EQ(validationMessage(run.simulation),
              "Cannot simulate: the ignition delay of the motor in 'Booster MMT' is not finite "
              "(NaN).");

    run.beta.chute->getDeploymentConfigurations().get(testFcid(1)).setDeployDelay(kNaN);
    EXPECT_EQ(validationMessage(run.simulation),
              "Cannot simulate: the deployment delay of 'Parachute' is not finite (NaN).");

    // The motor mount of the sustainer comes before its parachute.
    run.beta.inner->getMotorConfig(testFcid(1)).setEjectionDelay(-kInf);
    EXPECT_EQ(validationMessage(run.simulation),
              "Cannot simulate: the ejection delay of the motor in 'Motor Mount Tube' is not "
              "finite (-Infinity).");

    run.simulation.getOptions().setMaxSimulationTime(kNaN);
    EXPECT_EQ(validationMessage(run.simulation),
              "Cannot simulate: the maximum simulation time is not finite (NaN).");
}

/// Counts the emissions of a signal while it lives.
class ChangeCounter
{
public:
    explicit ChangeCounter(QtRocket::Signal<>& signal)
      : m_connection(signal.connect([this] { ++m_count; }))
    {
    }

    [[nodiscard]] int count() const noexcept { return m_count; }

private:
    mutable int                          m_count{0};
    QtRocket::Signal<>::ScopedConnection m_connection;
};

/// An extension that counts how often it is initialised.
class CountingExtension final : public QtRocket::AbstractSimulationExtension
{
public:
    explicit CountingExtension(std::shared_ptr<int> count)
      : AbstractSimulationExtension("test.Counting"), m_count(std::move(count))
    {
    }

    void initialize(QtRocket::SimulationConditions& /*conditions*/) override { ++*m_count; }

    [[nodiscard]] std::unique_ptr<QtRocket::SimulationExtension> clone() const override
    {
        return std::make_unique<CountingExtension>(*this);
    }

private:
    std::shared_ptr<int> m_count;
};

// A refused run is a failed run like that of an imported simulation: nothing ran (no extension
// was initialised, the engine made no data), and the bookkeeping of simulate() took place: the
// data of the run before are gone, the simulated conditions are the options, the status is
// UPTODATE, and changed() was emitted once.
TEST(SimulationInputValidation, ARefusedRunLeavesTheSimulationAsAnyFailedRun)
{
    AlphaRun                   run;
    const std::shared_ptr<int> initialised = std::make_shared<int>(0);
    run.simulation.getSimulationExtensions().push_back(
        std::make_shared<CountingExtension>(initialised));
    ASSERT_EQ(outcomeOf(run.simulation), "ok");
    ASSERT_EQ(*initialised, 1);
    ASSERT_NE(run.simulation.getSimulatedData(), nullptr);

    run.simulation.getOptions().setTimeStep(kNaN);
    const ChangeCounter events(run.simulation.changed());
    const Result<void>  result = run.simulation.simulate();

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(result.error().message, "Cannot simulate: the time step is not finite (NaN).");
    EXPECT_EQ(*initialised, 1);
    EXPECT_EQ(events.count(), 1);
    EXPECT_EQ(run.simulation.getSimulatedData(), nullptr);
    EXPECT_FALSE(run.simulation.hasSummaryData());
    EXPECT_EQ(run.simulation.getStoredStatus(), Simulation::Status::UPTODATE);
    const SimulationOptions* simulated = run.simulation.getSimulatedConditions();
    ASSERT_NE(simulated, nullptr);
    EXPECT_TRUE(std::isnan(simulated->getTimeStep()));
    EXPECT_EQ(run.simulation.getSimulatedConfigurationDescription(), "[A8-0]");
    // The options never equal conditions with a NaN, so the status is found outdated.
    EXPECT_EQ(run.simulation.getStatus(), Simulation::Status::OUTDATED);

    // With the value repaired the simulation runs again.
    run.simulation.getOptions().setTimeStep(0.05);
    EXPECT_EQ(outcomeOf(run.simulation), "ok");
    EXPECT_EQ(*initialised, 2);
}

// An imported simulation is refused for being that before its inputs are looked at, as in
// Java's order.
TEST(SimulationInputValidation, AnImportedSimulationIsRefusedFirst)
{
    TestEstesAlphaIII alpha;
    SimulationOptions options;
    options.setTimeStep(kNaN);
    Simulation simulation(nullptr, *alpha.rocket, Simulation::Status::EXTERNAL, "imported",
                          std::move(options), {}, nullptr);
    simulation.setFlightConfigurationId(testFcid(0));

    const Result<void> result = simulation.simulate();
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::SIMULATION_ABORTED);
    EXPECT_EQ(result.error().message, "Cannot simulate imported simulation.");
    // Asked directly, the validation answers for itself.
    EXPECT_FALSE(simulation.validateInputs().has_value());
}

// Whether a value is not finite or outside its range makes no difference to the order: the
// first input of the list that a run may not read is the one named.
TEST(SimulationInputValidation, NamesTheFirstValueThatIsNotFiniteOrOutsideItsRange)
{
    AlphaRun           run;
    SimulationOptions& o = run.simulation.getOptions();
    run.alpha.inner->getMotorConfig(testFcid(0)).setIgnitionDelay(-1.0);
    o.setRecoverySpeedWarning(kNaN);
    o.setTimeStep(0.0);
    o.setLaunchRodLength(-2.0);
    EXPECT_EQ(validationMessage(run.simulation),
              expectedOf("the launch rod length", Ranges::kLaunchRodLength, -2.0));
    o.setLaunchRodLength(1.0);
    EXPECT_EQ(validationMessage(run.simulation), expectedOf("the time step", Ranges::kTimeStep, 0));
    o.setTimeStep(0.05);
    EXPECT_EQ(validationMessage(run.simulation),
              "Cannot simulate: the recovery speed warning threshold is not finite (NaN).");
    o.setRecoverySpeedWarning(20.0);
    EXPECT_EQ(
        validationMessage(run.simulation),
        expectedOf("the ignition delay of the motor in 'Motor Mount Tube'", Ranges::kDelay, -1.0));
    run.alpha.inner->getMotorConfig(testFcid(0)).setIgnitionDelay(0.0);
    EXPECT_EQ(validationMessage(run.simulation), "valid");
}

// ---------------------------------------------------------------------------- the extensions

/// What a refusal calls the setting of key @p key of a RollControl.
[[nodiscard]] std::string rollControlSetting(std::string_view key)
{
    return std::format("the '{}' of the simulation extension 'Roll Control'", key);
}

/// What a refusal calls the setting of key @p key of an AirStart.
[[nodiscard]] std::string airStartSetting(std::string_view key)
{
    return std::format("the '{}' of the simulation extension 'Air-start'", key);
}

/// The outcome of the Alpha III's simulation with a RollControl on its fins whose setter
/// @p set was given @p value.
[[nodiscard]] std::string outcomeWithRollControl(void (RollControl::*set)(double), double value)
{
    AlphaRun                           run;
    const std::shared_ptr<RollControl> roll = std::make_shared<RollControl>();
    roll->setControlFinName("3 Fin Set");
    (*roll.*set)(value);
    run.simulation.getSimulationExtensions().push_back(roll);
    return outcomeOf(run.simulation);
}

/// The outcome of the Alpha III's simulation with an AirStart whose setter @p set was given
/// @p value.
[[nodiscard]] std::string outcomeWithAirStart(void (AirStart::*set)(double), double value)
{
    AlphaRun                        run;
    const std::shared_ptr<AirStart> airStart = std::make_shared<AirStart>();
    (*airStart.*set)(value);
    run.simulation.getSimulationExtensions().push_back(airStart);
    return outcomeOf(run.simulation);
}

/// The three outcomes of @p outcome, a function of the value a setting is given: with NaN,
/// with +infinity and with -infinity.
[[nodiscard]] Expected outcomesOf(const std::function<std::string(double)>& outcome)
{
    return {.nan              = outcome(kNaN),
            .positiveInfinity = outcome(kInf),
            .negativeInfinity = outcome(-kInf)};
}

/// Expects @p actual to be the three outcomes @p expected.
void expectSame(const Expected& actual, const Expected& expected)
{
    EXPECT_EQ(actual.nan, expected.nan);
    EXPECT_EQ(actual.positiveInfinity, expected.positiveInfinity);
    EXPECT_EQ(actual.negativeInfinity, expected.negativeInfinity);
}

// The six numbers the listener of a RollControl reads. OpenRocket flies with some of these
// values and dies of a BugException in the middle of the flight with the others ("Counted 0
// parallel fins ..." after 51 or 150 rows: probes/tier9a-fix-extensions, ExtNonFinite.java);
// here each is refused before anything runs.
TEST(SimulationInputValidation, TheNumbersOfARollControl)
{
    struct Setting
    {
        std::string_view key;
        void (RollControl::*set)(double);
    };
    const std::vector<Setting> settings{
        {.key = "startTime", .set = &RollControl::setStartTime},
        {.key = "setPoint", .set = &RollControl::setSetPoint},
        {.key = "finRate", .set = &RollControl::setFinRate},
        {.key = "maxFinAngle", .set = &RollControl::setMaxFinAngle},
        {.key = "KP", .set = &RollControl::setKP},
        {.key = "KI", .set = &RollControl::setKI},
    };
    for (const Setting& setting : settings)
    {
        SCOPED_TRACE(setting.key);
        expectSame(outcomesOf([&setting](double value) {
                       return outcomeWithRollControl(setting.set, value);
                   }),
                   allRefused(rollControlSetting(setting.key)));
    }
    // With numbers the simulation flies.
    EXPECT_EQ(outcomeWithRollControl(&RollControl::setKP, 0.01), "ok");
}

// The two numbers the listener of an AirStart reads. In OpenRocket a launch altitude that is
// no number and any such launch velocity end in a BugException, an infinite launch altitude in
// the SimulationException "Simulation values exceeded limits" (ExtNonFinite.java).
TEST(SimulationInputValidation, TheNumbersOfAnAirStart)
{
    expectSame(outcomesOf([](double value) {
                   return outcomeWithAirStart(&AirStart::setLaunchAltitude, value);
               }),
               allRefused(airStartSetting("launchAltitude")));
    expectSame(outcomesOf([](double value) {
                   return outcomeWithAirStart(&AirStart::setLaunchVelocity, value);
               }),
               allRefused(airStartSetting("launchVelocity")));
    EXPECT_EQ(outcomeWithAirStart(&AirStart::setLaunchVelocity, 20.0), "ok");
}

/// The message of validateInputs() of the Alpha III's simulation with a RollControl whose
/// setter @p set was given @p value.
[[nodiscard]] std::string validationWithRollControl(void (RollControl::*set)(double), double value)
{
    AlphaRun                           run;
    const std::shared_ptr<RollControl> roll = std::make_shared<RollControl>();
    (*roll.*set)(value);
    run.simulation.getSimulationExtensions().push_back(roll);
    return validationMessage(run.simulation);
}

/// The message of validateInputs() of the Alpha III's simulation with an AirStart whose setter
/// @p set was given @p value.
[[nodiscard]] std::string validationWithAirStart(void (AirStart::*set)(double), double value)
{
    AlphaRun                        run;
    const std::shared_ptr<AirStart> airStart = std::make_shared<AirStart>();
    (*airStart.*set)(value);
    run.simulation.getSimulationExtensions().push_back(airStart);
    return validationMessage(run.simulation);
}

// The ranges the two extensions give their numbers (RollControl::kStartTimeRange and the
// constants after it, AirStart::kLaunchAltitudeRange and kLaunchVelocityRange). OpenRocket
// flies with any finite number.
TEST(SimulationInputValidation, TheRangesOfTheNumbersOfTheExtensions)
{
    const auto roll = [](void (RollControl::*set)(double)) {
        return [set](double value) { return validationWithRollControl(set, value); };
    };
    const auto air = [](void (AirStart::*set)(double)) {
        return [set](double value) { return validationWithAirStart(set, value); };
    };
    const std::vector<RangedInput> inputs{
        {rollControlSetting("startTime"), RollControl::kStartTimeRange,
         roll(&RollControl::setStartTime)},
        {rollControlSetting("setPoint"), RollControl::kSetPointRange,
         roll(&RollControl::setSetPoint)},
        {rollControlSetting("finRate"), RollControl::kFinRateRange, roll(&RollControl::setFinRate)},
        {rollControlSetting("maxFinAngle"), RollControl::kMaxFinAngleRange,
         roll(&RollControl::setMaxFinAngle)},
        {rollControlSetting("KP"), RollControl::kGainRange, roll(&RollControl::setKP)},
        {rollControlSetting("KI"), RollControl::kGainRange, roll(&RollControl::setKI)},
        {airStartSetting("launchAltitude"), AirStart::kLaunchAltitudeRange,
         air(&AirStart::setLaunchAltitude)},
        {airStartSetting("launchVelocity"), AirStart::kLaunchVelocityRange,
         air(&AirStart::setLaunchVelocity)},
    };
    EXPECT_EQ(wrongAtTheBounds(inputs), std::vector<std::string>{});

    // A refused number of an extension is an ordinary error of simulate(), like an option.
    EXPECT_EQ(outcomeWithRollControl(&RollControl::setMaxFinAngle, 15.0),
              outside(rollControlSetting("maxFinAngle"), RollControl::kMaxFinAngleRange, 15.0));
    EXPECT_EQ(outcomeWithAirStart(&AirStart::setLaunchVelocity, -1.0),
              outside(airStartSetting("launchVelocity"), AirStart::kLaunchVelocityRange, -1.0));
    // The defaults of both extensions lie inside the ranges.
    EXPECT_EQ(validationWithRollControl(&RollControl::setKP, RollControl().getKP()), "valid");
    EXPECT_EQ(validationWithAirStart(&AirStart::setLaunchAltitude, AirStart().getLaunchAltitude()),
              "valid");
}

/// The BigDecimal @p text.
[[nodiscard]] QtRocket::BigDecimal big(std::string_view text)
{
    const std::optional<QtRocket::BigDecimal> value = QtRocket::BigDecimal::parse(text);
    if (!value.has_value())
    {
        ADD_FAILURE() << "not a BigDecimal: " << text;
        return QtRocket::BigDecimal::valueOf(0);
    }
    return *value;
}

// The settings as a .ork file gives them, a Config: the one number of a file that is not
// finite as a double is an integer too large for one, which the reader keeps as a big number
// (OpenRocket: a BigInteger, whose doubleValue() is Infinity). An entry that is no number at
// all is the default of the setting, and that is finite.
TEST(SimulationInputValidation, TheNumbersOfAnExtensionAsAFileGivesThem)
{
    AlphaRun                        run;
    const std::shared_ptr<AirStart> airStart = std::make_shared<AirStart>();
    run.simulation.getSimulationExtensions().push_back(airStart);

    Config huge;
    huge.put("launchAltitude", big("1" + std::string(400, '0')));
    airStart->setConfig(huge);
    ASSERT_EQ(airStart->getLaunchAltitude(), kInf);
    EXPECT_EQ(outcomeOf(run.simulation), refusal(airStartSetting("launchAltitude"), "Infinity"));

    Config texts;
    texts.put("launchAltitude", "NaN");
    texts.put("launchVelocity", "Infinity");
    airStart->setConfig(texts);
    EXPECT_EQ(validationMessage(run.simulation), "valid");
    EXPECT_EQ(outcomeOf(run.simulation), "ok");
}

/// An extension that lists the numbers it was given as its inputs.
class ListingExtension final : public QtRocket::AbstractSimulationExtension
{
public:
    ListingExtension(std::string name, std::vector<double> numbers)
      : AbstractSimulationExtension("test.Listing", std::move(name)), m_numbers(std::move(numbers))
    {
    }

    void initialize(QtRocket::SimulationConditions& /*conditions*/) override { }

    [[nodiscard]] std::vector<InputNumber> getInputNumbers() const override
    {
        std::vector<InputNumber> inputs;
        inputs.reserve(m_numbers.size());
        for (std::size_t i = 0; i < m_numbers.size(); i++)
        {
            inputs.push_back(inputNumber(getName(), std::format("n{}", i), m_numbers[i]));
        }
        return inputs;
    }

    [[nodiscard]] std::unique_ptr<SimulationExtension> clone() const override
    {
        return std::make_unique<ListingExtension>(*this);
    }

private:
    std::vector<double> m_numbers;
};

// The extensions are asked in the order of the simulation's list and each for its numbers in
// its own order; an extension that lists none is not looked at (the default of the interface),
// and a null in the list is no input: simulate() reports that as the bug it is.
TEST(SimulationInputValidation, AsksEveryExtensionForItsNumbersInOrder)
{
    AlphaRun                                           run;
    std::vector<std::shared_ptr<SimulationExtension>>& extensions =
        run.simulation.getSimulationExtensions();
    const std::shared_ptr<int> initialised = std::make_shared<int>(0);
    extensions.push_back(std::make_shared<CountingExtension>(initialised));
    // An extension that gives its numbers no range has them held to nothing but finite.
    extensions.push_back(std::make_shared<ListingExtension>(
        "first", std::vector<double>{1.0, std::numeric_limits<double>::max(), -0.0}));
    EXPECT_EQ(validationMessage(run.simulation), "valid");

    extensions.push_back(
        std::make_shared<ListingExtension>("second", std::vector<double>{2.0, kInf, kNaN}));
    extensions.push_back(std::make_shared<ListingExtension>("third", std::vector<double>{kNaN}));
    EXPECT_EQ(validationMessage(run.simulation),
              "Cannot simulate: the 'n1' of the simulation extension 'second' is not finite "
              "(Infinity).");
    // Refused before any extension was initialised, also one that comes first in the list.
    EXPECT_EQ(outcomeOf(run.simulation),
              refusal("the 'n1' of the simulation extension 'second'", "Infinity"));
    EXPECT_EQ(*initialised, 0);

    extensions.insert(extensions.begin(), nullptr);
    EXPECT_EQ(validationMessage(run.simulation),
              "Cannot simulate: the 'n1' of the simulation extension 'second' is not finite "
              "(Infinity).");
    extensions.resize(3);
    EXPECT_EQ(validationMessage(run.simulation), "valid");
    EXPECT_TRUE(outcomeOf(run.simulation)
                    .starts_with("bug: BUG: The simulation holds a null "
                                 "extension"));
}

// ------------------------------------------------------------------- nothing valid is refused

// ------------------------------------------------------------- a measurement at the bounds

/// How simulate() of @p simulation ends when it is given @p seconds: as outcomeOf() tells it,
/// or "stopped after <n> s" when the run was still going then and a stop was requested.
[[nodiscard]] std::string outcomeWithin(Simulation& simulation, int seconds)
{
    // (Held through a pointer: a stop source must not be const, see system_listener_tests.cpp.)
    const auto         source = std::make_shared<std::stop_source>();
    const auto         done   = std::make_shared<std::stop_source>();
    const std::jthread watchdog([source, done, seconds] {
        const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
        while (!done->stop_requested() && std::chrono::steady_clock::now() < end)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        if (!done->stop_requested())
        {
            source->request_stop();
        }
    });
    std::string        outcome;
    try
    {
        const Result<void> result = simulation.simulate(source->get_token());
        if (result.has_value())
        {
            outcome = simulation.hasErrors() ? "ok, aborted" : "ok";
        }
        else if (result.error().code == ErrorCode::CANCELLED)
        {
            outcome = std::format("stopped after {} s", seconds);
        }
        else if (result.error().code == ErrorCode::INVALID_ARGUMENT)
        {
            outcome = "refused: " + result.error().message;
        }
        else
        {
            outcome = "error: " + result.error().message;
        }
    }
    catch (const BugError& error)
    {
        outcome = std::string("bug: ") + error.what();
    }
    done->request_stop();
    return outcome;
}

/// A line of the measurement: what the Alpha III's simulation is given, and how.
struct AtABound
{
    std::string                    name;
    std::function<void(AlphaRun&)> give;
};

/// The Alpha III's simulation with a RollControl on its fins that @p give has set up.
void addRollControl(AlphaRun& run, const std::function<void(RollControl&)>& give)
{
    const std::shared_ptr<RollControl> roll = std::make_shared<RollControl>();
    roll->setControlFinName("3 Fin Set");
    give(*roll);
    run.simulation.getSimulationExtensions().push_back(roll);
}

/// The Alpha III's simulation with an AirStart that @p give has set up.
void addAirStart(AlphaRun& run, const std::function<void(AirStart&)>& give)
{
    const std::shared_ptr<AirStart> airStart = std::make_shared<AirStart>();
    give(*airStart);
    run.simulation.getSimulationExtensions().push_back(airStart);
}

/// The inputs of the Alpha III's simulation at the bounds of their ranges, one at a time.
[[nodiscard]] std::vector<AtABound> theBounds()
{
    const auto options = [](std::function<void(SimulationOptions&)> give) {
        return [give = std::move(give)](AlphaRun& run) { give(run.simulation.getOptions()); };
    };
    const auto design = [](AlphaValue which, double value) {
        return [which, value](AlphaRun& run) { setAlphaValue(run, which, value); };
    };
    const auto withoutIsa = [](SimulationOptions& o) { o.setIsaAtmosphere(false); };
    return {
        {.name = "nothing changed", .give = [](AlphaRun& /*run*/) { }},
        {.name = "rod length 0 m",
         .give = options([](SimulationOptions& o) { o.setLaunchRodLength(0.0); })},
        {.name = "rod length 1000 m",
         .give = options([](SimulationOptions& o) { o.setLaunchRodLength(1000.0); })},
        {.name = "rod angle 60 degrees",
         .give = options([](SimulationOptions& o) { o.setLaunchRodAngle(std::numbers::pi / 3); })},
        {.name = "launch altitude -1000 m",
         .give = options([](SimulationOptions& o) { o.setLaunchAltitude(-1000.0); })},
        {.name = "launch altitude 11 km, the highest the setter stores",
         .give = options([](SimulationOptions& o) { o.setLaunchAltitude(1e9); })},
        {.name = "wind 1000 m/s",
         .give = options([](SimulationOptions& o) { o.getAverageWindModel().setAverage(1000.0); })},
        {.name = "wind 1000 m/s with a deviation of 1000 m/s",
         .give = options([](SimulationOptions& o) {
             o.getAverageWindModel().setAverage(1000.0);
             o.getAverageWindModel().setStandardDeviation(1000.0);
         })},
        {.name = "wind 100 m/s with a deviation of 100 m/s",
         .give = options([](SimulationOptions& o) {
             o.getAverageWindModel().setAverage(100.0);
             o.getAverageWindModel().setStandardDeviation(100.0);
         })},
        {.name = "temperature 1 K", .give = options([withoutIsa](SimulationOptions& o) {
                                        withoutIsa(o);
                                        o.setLaunchTemperature(1.0);
                                    })},
        {.name = "temperature 1000 K", .give = options([withoutIsa](SimulationOptions& o) {
                                           withoutIsa(o);
                                           o.setLaunchTemperature(1000.0);
                                       })},
        {.name = "pressure 0.001 Pa", .give = options([withoutIsa](SimulationOptions& o) {
                                          withoutIsa(o);
                                          o.setLaunchPressure(0.001);
                                      })},
        {.name = "pressure 1000000 Pa", .give = options([withoutIsa](SimulationOptions& o) {
                                            withoutIsa(o);
                                            o.setLaunchPressure(1000000.0);
                                        })},
        {.name = "temperature 1 K and pressure 1000000 Pa",
         .give = options([withoutIsa](SimulationOptions& o) {
             withoutIsa(o);
             o.setLaunchTemperature(1.0);
             o.setLaunchPressure(1000000.0);
         })},
        {.name = "gravity 0 m/s^2", .give = options([](SimulationOptions& o) {
                                        o.setGravityModelType(GravityModelType::CONSTANT);
                                        o.setConstantGravity(0.0);
                                    })},
        {.name = "gravity 100 m/s^2", .give = options([](SimulationOptions& o) {
                                          o.setGravityModelType(GravityModelType::CONSTANT);
                                          o.setConstantGravity(100.0);
                                      })},
        {.name = "time step 10 s",
         .give = options([](SimulationOptions& o) { o.setTimeStep(10.0); })},
        {.name = "time step 1 s",
         .give = options([](SimulationOptions& o) { o.setTimeStep(1.0); })},
        {.name = "time step 0.0001 s",
         .give = options([](SimulationOptions& o) { o.setTimeStep(0.0001); })},
        // Where a limit of the stepper binds, the steps are a twentieth of the time step.
        {.name = "time step 0.0001 s without a launch rod and without wind",
         .give = options([](SimulationOptions& o) {
             o.setTimeStep(0.0001);
             o.setLaunchRodLength(0.0);
             o.getAverageWindModel().setAverage(0.0);
             o.getAverageWindModel().setStandardDeviation(0.0);
         })},
        {.name = "maximum time 0 s",
         .give = options([](SimulationOptions& o) { o.setMaxSimulationTime(0.0); })},
        {.name = "maximum time 1000000 s",
         .give = options([](SimulationOptions& o) { o.setMaxSimulationTime(1000000.0); })},
        {.name = "maximum time 1000000 s without gravity",
         .give = options([](SimulationOptions& o) {
             o.setMaxSimulationTime(1000000.0);
             o.setGravityModelType(GravityModelType::CONSTANT);
             o.setConstantGravity(0.0);
         })},
        {.name = "thresholds 0 m/s", .give = options([](SimulationOptions& o) {
                                         o.setRecoverySpeedWarning(0.0);
                                         o.setDrogueLowSpeedWarning(0.0);
                                         o.setRecoveryDrogueMainHighSpeedWarning(0.0);
                                         o.setRecoveryDrogueMainLowSpeedWarning(0.0);
                                     })},
        {.name = "thresholds 10000 m/s", .give = options([](SimulationOptions& o) {
                                             o.setRecoverySpeedWarning(10000.0);
                                             o.setDrogueLowSpeedWarning(10000.0);
                                             o.setRecoveryDrogueMainHighSpeedWarning(10000.0);
                                             o.setRecoveryDrogueMainLowSpeedWarning(10000.0);
                                         })},
        {.name = "ignition delay 1000000 s", .give = design(AlphaValue::IGNITION_DELAY, 1000000.0)},
        {.name = "ejection delay 1000000 s", .give = design(AlphaValue::EJECTION_DELAY, 1000000.0)},
        {.name = "deployment delay 1000000 s",
         .give = design(AlphaValue::DEPLOYMENT_DELAY, 1000000.0)},
        {.name = "deployment at 0 m", .give = design(AlphaValue::DEPLOYMENT_AT_AN_ALTITUDE, 0.0)},
        {.name = "deployment at 1000000 m",
         .give = design(AlphaValue::DEPLOYMENT_AT_AN_ALTITUDE, 1000000.0)},
        {.name = "drag table with cd 1000000 at Mach 0 to 100",
         .give = options([](SimulationOptions& o) {
             setDragRows(
                 o, MachAoALookup::dragBuilder().addDragData(0.0, 1e6).addDragData(100.0, 1e6));
         })},
        {.name = "drag table with cd -1000000",
         .give = options([](SimulationOptions& o) {
             setDragRows(o, MachAoALookup::dragBuilder().addDragData(0.0, -1e6));
         })},
        {.name = "stability table with cn, cm 1000000 and cp 1000 m",
         .give = options([](SimulationOptions& o) {
             setStabilityRows(
                 o, MachAoALookup::stabilityBuilder().addStabilityData(0.0, 1e6, 1e6, 1000.0));
         })},
        {.name = "stability table with cn, cm -1000000 and cp -1000 m",
         .give = options([](SimulationOptions& o) {
             setStabilityRows(
                 o, MachAoALookup::stabilityBuilder().addStabilityData(0.0, -1e6, -1e6, -1000.0));
         })},
        {.name = "roll control at its upper bounds",
         .give =
             [](AlphaRun& run) {
                 addRollControl(run, [](RollControl& roll) {
                     roll.setStartTime(0.0);
                     roll.setSetPoint(1000.0);
                     roll.setFinRate(1000.0);
                     roll.setMaxFinAngle(std::numbers::pi / 2);
                     roll.setKP(1000.0);
                     roll.setKI(1000.0);
                 });
             }},
        {.name = "roll control with a setpoint of -1000 rad/s and no gain",
         .give =
             [](AlphaRun& run) {
                 addRollControl(run, [](RollControl& roll) {
                     roll.setSetPoint(-1000.0);
                     roll.setKP(0.0);
                     roll.setKI(0.0);
                     roll.setFinRate(0.0);
                     roll.setMaxFinAngle(0.0);
                 });
             }},
        {.name = "air start at 1000000 m with 10000 m/s",
         .give =
             [](AlphaRun& run) {
                 addAirStart(run, [](AirStart& airStart) {
                     airStart.setLaunchAltitude(1000000.0);
                     airStart.setLaunchVelocity(10000.0);
                 });
             }},
        {.name = "air start at 0 m with 0 m/s",
         .give =
             [](AlphaRun& run) {
                 addAirStart(run, [](AirStart& airStart) {
                     airStart.setLaunchAltitude(0.0);
                     airStart.setLaunchVelocity(0.0);
                 });
             }},
    };
}

// What the ranges let through: how the Alpha III flies with one input at a bound of its range,
// each run given a quarter of a minute. A measurement for whoever reviews the table, not a
// test: a value inside its range need not make a flight that succeeds, and some of these do
// not (see SimulationInputRanges and the notes of tier 9d).
//
// It is a measurement of ONE rocket, and what it shows holds for that rocket: none of these
// runs ended in a BugError when part K3 took them, and the same inputs ended the flight of
// another design in one (very dense air, and humid air without a density: both are closed
// since, see launch_site_air_tests.cpp). The run without gravity for 1000000 s is stopped here
// after its quarter of a minute; left alone it ends at the bound on the rows of a branch (see
// simulation_row_bound_tests.cpp, which has that measurement).
//   build/ci-gcc/bin/QtRocket_core_tests --gtest_also_run_disabled_tests
//       --gtest_filter=SimulationInputValidation.DISABLED_PrintsHowTheAlphaFliesAtTheBounds
TEST(SimulationInputValidation, DISABLED_PrintsHowTheAlphaFliesAtTheBounds)
{
    for (const AtABound& bound : theBounds())
    {
        AlphaRun run;
        bound.give(run);
        const auto        start   = std::chrono::steady_clock::now();
        const std::string outcome = outcomeWithin(run.simulation, 15);
        const double      seconds =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::cout << std::format("{}: {} ({:.1f} s, {})\n", bound.name, outcome, seconds,
                                 validationMessage(run.simulation))
                  << std::flush;
    }
}

class SimulationInputValidationOfTestRockets : public ::testing::TestWithParam<TestRocketMaker>
{ };

// Every flight configuration of every test rocket, the default one included, with the options
// a new simulation of the application has (a wind of 2 m/s, the launch into it): the inputs
// are valid, and simulate() does not refuse them. (Some of these flights abort, the ones
// without a motor for example: that is an outcome of the flight.)
TEST_P(SimulationInputValidationOfTestRockets, EveryConfigurationIsValid)
{
    const std::unique_ptr<Rocket> rocket = GetParam().make();
    QtRocket::InMemoryPreferences preferences;
    for (int i = 0; i <= rocket->getConfigurationCount(); i++)
    {
        SCOPED_TRACE(i);
        const FlightConfigurationId fcid =
            rocket->getFlightConfigurationByIndex(i, true).getFlightConfigurationId();
        Simulation simulation(*rocket, preferences);
        simulation.setFlightConfigurationId(fcid);
        simulation.getOptions().setRandomSeed(0);

        const Result<void> valid = simulation.validateInputs();
        EXPECT_TRUE(valid.has_value()) << (valid.has_value() ? "" : valid.error().message);
        const std::string outcome = outcomeOf(simulation);
        EXPECT_TRUE(outcome == "ok" || outcome == "ok, aborted") << outcome;
    }
}

/// Plugs every motor that @p rocket has in the configuration @p fcid (no test rocket is made
/// with a plugged motor), and returns how many those are.
[[nodiscard]] int plugEveryMotor(const Rocket& rocket, const FlightConfigurationId& fcid)
{
    int plugged = 0;
    for (QtRocket::RocketComponent* component :
         rocket.getFlightConfiguration(fcid).getAllComponents())
    {
        auto* mount = dynamic_cast<QtRocket::MotorMount*>(component);
        if (mount == nullptr || !mount->isMotorMount() || mount->getMotorConfig(fcid).isEmpty())
        {
            continue;
        }
        mount->getMotorConfig(fcid).setEjectionDelay(QtRocket::Motor::kPluggedDelay);
        plugged++;
    }
    return plugged;
}

/// How many flight configurations of the rocket of @p maker, each with every one of its motors
/// plugged, validateInputs() refuses; @p motors gets the number of motors that were plugged.
[[nodiscard]] int refusedWithPluggedMotors(const TestRocketMaker& maker, int& motors)
{
    const std::unique_ptr<Rocket> rocket  = maker.make();
    int                           refused = 0;
    for (int i = 0; i <= rocket->getConfigurationCount(); i++)
    {
        const FlightConfigurationId fcid =
            rocket->getFlightConfigurationByIndex(i, true).getFlightConfigurationId();
        motors += plugEveryMotor(*rocket, fcid);
        Simulation simulation(*rocket);
        simulation.setFlightConfigurationId(fcid);
        if (!simulation.validateInputs().has_value())
        {
            refused++;
        }
    }
    return refused;
}

// The same configurations with every motor plugged: the plugged ejection delay is +infinity,
// and it is a valid value wherever a motor is.
TEST_P(SimulationInputValidationOfTestRockets, EveryConfigurationIsValidWithItsMotorsPlugged)
{
    int motors = 0;
    EXPECT_EQ(refusedWithPluggedMotors(GetParam(), motors), 0);
}

// Not vacuous: the test rockets have motors to plug.
TEST(SimulationInputValidation, TheTestRocketsHaveMotorsToPlug)
{
    int motors = 0;
    for (const TestRocketMaker& maker : testRocketMakers())
    {
        static_cast<void>(refusedWithPluggedMotors(maker, motors));
    }
    EXPECT_GE(motors, 20);
}

INSTANTIATE_TEST_SUITE_P(Makers, SimulationInputValidationOfTestRockets,
                         ::testing::ValuesIn(testRocketMakers()),
                         [](const ::testing::TestParamInfo<TestRocketMaker>& paramInfo) {
                             return std::string(paramInfo.param.method);
                         });

}  // namespace
