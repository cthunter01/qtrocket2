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

#include <cmath>
#include <cstddef>
#include <format>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
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
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/extension/AbstractSimulationExtension.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/extension/example/AirStart.h"
#include "QtRocket/simulation/extension/example/RollControl.h"
#include "QtRocket/util/BigDecimal.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Config.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Signal.h"
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

// A finite value is not the validation's business, however absurd it is (the comment of
// validateInputs(), "Not checked"). Only the validation is asked here: with this time step the
// first step of the run would not end, here as in OpenRocket.
TEST(SimulationInputValidation, AHugeFiniteTimeStepIsNotRefused)
{
    AlphaRun           run;
    SimulationOptions& o = run.simulation.getOptions();
    o.setTimeStep(1e300);
    ASSERT_EQ(o.getTimeStep(), 1e300);
    EXPECT_EQ(validationMessage(run.simulation), "valid");
    o.setTimeStep(std::numeric_limits<double>::max());
    EXPECT_EQ(validationMessage(run.simulation), "valid");
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
