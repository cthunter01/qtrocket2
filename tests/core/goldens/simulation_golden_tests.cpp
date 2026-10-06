// Simulation golden tests: the simulations of the thirteen rockets of tests/core/rocket/
// TestRockets.h against what OpenRocket's BasicEventSimulationEngine computes for the Java rockets,
// in tests/data/goldens/testrocket-<name>/sim_<NN>_<name>.json and the branch CSV files next to
// them (tools/openrocket-goldens: GoldenDumper.java and SimulationDumper.java; the format is in
// that tool's README.md). 50 simulations with 53 branches, 437 events and 22330 rows.
//
// The set-up is the harness's (GoldenDumper.dumpTestRocket()): one simulation per flight
// configuration in the rocket's order, the default configuration first, then the three variants
// of GoldenDumper.VARIANTS; the options of a fresh installation (applicationDefaultOptions():
// DefaultSimulationOptionFactory over an empty preferences store, and what the harness sets on
// top, the multi-level wind level included), then the variant's change, then
// SimulationDumper.makeReproducible() (seed 0, fixed; zero standard deviation of every wind
// model). The run is the harness's too (SimulationDumper.dump()): toSimulationConditions(),
// setSimulation(), the forces listener of the jitter removal first, the extensions (none), its
// conditions listener last, a new engine, simulate().
//
// The jitter removal is tests/core/simulation/JitterRemoval.h, the port of the harness's
// JitterRemoval.java that the stepper tests use as well: two system listeners with the harness's
// placement, the harness's extra calculator call and its counting. Java tells the hook the
// Runge-Kutta steppers fire from the one the landing and tumble steppers fire by walking the call
// stack; the listener API has no such thing, so the port tells them by the forces handed to the
// hook, which hold a normal force coefficient only when the aerodynamic calculator made them
// (the Euler steppers build theirs from a drag coefficient and leave it NaN).
//
// Compared (SimulationGolden, one test per rocket), per simulation:
// - its index, name and files in the manifest, the flight configuration (index, id, name), the
//   source of the options, the variant, that it has no extensions and was not skipped;
// - the options the run used with the golden "options", field by field and exactly (they are
//   settings; the one that is computed, the rod direction of a launch into a multi-level wind,
//   to 1e-12), and what makeReproducible() changed with the golden "harness";
// - "result": the status, the exception's type and message, the number of jitter replacements;
// - "summary": the ten values and the number of branches; "warnings", in order;
// - per branch: its index, name and source component (as a golden path), the number of rows, the
//   optimum altitude, the time to it, the optimum delay, the separation time, the name of its
//   file, the excluded columns; the columns in order (key, name, symbol, whether built in,
//   minimum and maximum); the events in order (type and source as one text, then the time and
//   the data of each by its kind: the motor, the abort, the warning as the warning set holds it,
//   the text); and the time series, every column of every row. NaN equals NaN.
// A golden field that nothing compares is reported (noteUncomparedKeys()); the file's "schema",
// "schemaVersion" and "input", and the agreement of a CSV file with its branch, are checked by
// goldens_schema_tests.cpp.
//
// WHAT CAN BE COMPARED. OpenRocket's results for 30 of the 50 simulations are not reproducible,
// by OpenRocket itself or by anything else. At the installation's time step of 0.05 s the pitch
// oscillation of these light rockets is integrated beyond the stability limit of the Runge-Kutta
// method (natural frequency times step up to 4.4), and the step size control that follows the
// oscillation amplifies a difference in the last bit by eleven orders of magnitude within a
// second of flight. Measured:
// - OpenRocket against itself, with nothing changed but the random ids of the components (the
//   order in which its hash maps are summed): the [C6-5] flight of the Estes Alpha III has 796,
//   803 or 817 rows and its apogee at 5.954 s, 5.967 s or 5.971 s; the [C6-3] flight of the one
//   with the inline pod reaches 102.63 m/s in the golden file and 102.74 m/s to 102.80 m/s in
//   eight other runs. The golden files are one such run each: 8 of the 50 golden simulations
//   come out differently when the harness's own code runs them again with another id sequence.
// - QtRocket against the golden files: equal to the last bit while the rocket is on the launch
//   rod (at most 2e-15 of a column's scale at the first record after it), 1e-12 some 5 to 70
//   records later, 1e-9 after 30 to 180 records, then different row counts (20 simulations),
//   maximum velocities (up to 1.8e-3), apogee times (up to 0.045 s) and ground hits (0.14 s).
// - QtRocket against itself under a libm whose results are moved by one ulp (the shim of the
//   aerodynamic goldens, in six patterns): the same growth, and row counts that change with the
//   pattern in 21 simulations ([C6-5]: 848 rows without the shim and 862, 802, 813, 806, 796 or
//   787 with it; 796 is the golden number).
// So no tolerance, and none within the plan's bounds (event times 1e-3 s, apogee and maximum
// velocity relative 1e-4), holds for the later part of such a flight on every platform, and the
// comparison does not pretend otherwise. It measures what is reproducible and compares that:
// - Every simulation is run twice: as the harness runs it, and once more with a last-bit
//   perturbation (LastBitListener: after every step each component of the velocity and of the
//   rotation velocity is moved to the next representable number).
// - A row of a branch is reproducible while every value of it, in the two runs, is within
//   1/kSensitivityMargin of the tolerance of its column; the first row that is not ends the
//   reproducible part of the branch (its horizon). A branch whose every row is reproducible, in
//   a run with as many rows, branches, jitter replacements and the same status, is whole.
// - Compared with the golden files, at the tolerances below: the rows of the reproducible part;
//   the time of an event the branch recorded in its reproducible part, or that the reproducible
//   part fixes (the LAUNCH, and the BURNOUT of a motor that ignited in it: see
//   isTimedInTheReproduciblePart()), and the separation time when the stage separated in it; the
//   launch rod velocity when the rod is cleared in the reproducible part; for a whole branch its
//   number of rows, optimum altitude and delay
//   and the minimum and maximum of every column; for a whole run the other summary values, the
//   number of jitter replacements (it counts the force calculations of the Runge-Kutta steppers,
//   the nested optimum-coast runs included) and the parameter, description and text of a warning
//   that has a parameter (a speed or an angle, which the text prints).
// - Always compared, in every simulation: everything that is not a number of the trajectory:
//   the options, the result's status, the branches, their names, sources and columns, the events
//   with their types, sources and data, the warnings with their classes, priorities and sources.
// - What is neither is counted as sensitive, and the sums are checked: compared plus sensitive is
//   what the files hold (in the test of each rocket; SimulationGoldenCoverage pins what the
//   files of the thirteen rockets hold in all). On Linux 20 simulations are whole (the 19
//   that never leave the launch rod and the Falcon 9 Heavy, a flight of 148 records that ends in
//   a tumble under thrust); of the 30 others the first 22 to 77 records are compared, which is
//   the launch rod and the first 0.04 s to 0.3 s of free flight; in all, 1670 of the 22330 rows.
// The test of each rocket also holds the floor under this, so that a calculation that became
// sensitive throughout could not pass by comparing nothing: a simulation that never clears the
// launch rod is whole, and every flight is reproducible at least until it has cleared the rod
// (in rows: the reproducible rows of a branch are at least the golden rows up to the clearing,
// 794 in the 31 flights and 26 in the 19 other simulations).
// SimulationGoldenStrict holds the comparison the plan asked for (everything, the rows exactly,
// the events in the order they were recorded in), disabled: it cannot pass.
//
// One more difference is not a matter of rounding: an event that ignites the motors of several
// mounts queues their IGNITION events in the order of a hash map in OpenRocket (it changes from
// one run of OpenRocket to the next) and in the order of the component tree here. Consecutive
// IGNITION events with the same time are therefore compared as a set (comparisonOrder()); 5
// golden simulations record such a pair the other way round.
//
// Tolerances. A value of the time series: 1e-9 of the scale of its column (the largest magnitude
// of the golden column), which also is the measure for the minimum and maximum; another value:
// 1e-9 of its magnitude; a time: 1e-6 s. Measured over what is compared (the values a
// perturbation of one ulp per step moves by less than 1e-12): the largest difference from the
// golden files is 3.0e-12 of the scale on Linux with glibc, and 2.9e-12 to 8.0e-12 under the
// one-ulp libm shim (always up, always down, and four random patterns), so the tolerance is 124
// times the largest measurement and five orders of magnitude inside the plan's bounds. The times
// compared differ by at most 2e-15 s; their tolerance covers what a time the motors set can
// differ by, which is the rounding of the simulation time it is counted from (6e-11 s between
// OpenRocket and QtRocket in a run of the three-stage rocket with a time step of 0.01 s).
// SimulationGoldenMeasurement, disabled, prints the table of the differences per simulation and
// column, compared and sensitive.
//
// Not vacuous: SimulationGoldenMutation changes one golden value at a time (a setting, a value
// of the time series, a time, a summary value, a row count), removes an event and swaps two, and
// expects the one line that reports it; a change within the tolerance is not reported, and a
// value beyond the horizon is, as said, not compared.
//
// The tests are cut by what a process has to simulate, since ctest starts one process per test
// and the runs are kept per process (goldenRun()): one test per rocket, one per simulation that
// the mutations change (not one per mutation), and none that runs the simulations of another.
//
// Not compared in this tier: the sixteen example-* inputs. Their simulation files are in
// tests/data/goldens, but the designs are .ork files, which need the .ork loader of the file
// tier, and three of their simulations use extensions. That tier adds them here: load the
// document and hand each simulation to compareSimulation().

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <numbers>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/mass/MotorClusterState.h"
#include "QtRocket/models/GravityModelType.h"
#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/PinkNoiseWindModel.h"
#include "QtRocket/models/WindModelType.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/BasicEventSimulationEngine.h"
#include "QtRocket/simulation/DefaultSimulationOptionFactory.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/SimulationStepperMethod.h"
#include "QtRocket/simulation/exception/SimulationCalculationException.h"
#include "QtRocket/simulation/exception/SimulationCancelledException.h"
#include "QtRocket/simulation/exception/SimulationException.h"
#include "QtRocket/simulation/exception/SimulationListenerException.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/Uuid.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenGeometry.h"
#include "goldens/GoldenMismatches.h"
#include "goldens/GoldenWarnings.h"
#include "rocket/TestRockets.h"
#include "simulation/JitterRemoval.h"
#include "unit/DefaultUnitsGuard.h"

namespace
{

using nlohmann::json;
using QtRocket::BasicEventSimulationEngine;
using QtRocket::Coordinate;
using QtRocket::FlightConfiguration;
using QtRocket::FlightData;
using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::FlightEvent;
using QtRocket::GeodeticComputationStrategy;
using QtRocket::GravityModelType;
using QtRocket::InMemoryPreferences;
using QtRocket::MultiLevelPinkNoiseWindModel;
using QtRocket::PinkNoiseWindModel;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Simulation;
using QtRocket::SimulationConditions;
using QtRocket::SimulationOptions;
using QtRocket::SimulationStatus;
using QtRocket::SimulationStepperMethod;
using QtRocket::Warning;
using QtRocket::WindModelType;
using QtRocket::Test::compareWarning;
using QtRocket::Test::compareWarningIdentity;
using QtRocket::Test::GoldenInput;
using QtRocket::Test::GoldenManifest;
using QtRocket::Test::goldenPathOf;
using QtRocket::Test::GoldenSimulation;
using QtRocket::Test::GoldenTable;
using QtRocket::Test::goldenValue;
using QtRocket::Test::JitterRemoval;
using QtRocket::Test::noteUncomparedKeys;
using QtRocket::Test::parameterOf;
using QtRocket::Test::TestRocketMaker;
using QtRocket::Test::testRocketMakers;

/// The comparison collector of the golden tests.
using Mismatches = QtRocket::Test::GoldenMismatches;

// ================================================================================ tolerances

/// A value against its golden value: within kValueRelative of the scale of the value (the larger
/// magnitude of the two, or, for a value of a time series and for the minimum and maximum of a
/// column, the largest magnitude of the golden column).
constexpr double kValueRelative = 1e-9;

/// A time (an event, the time to apogee, ...) against its golden value: within kTimeAbsolute
/// seconds.
constexpr double kTimeAbsolute = 1e-6;

/// A value of a time series is reproducible when the perturbed run moves it by no more than its
/// tolerance divided by this (1e-12 of the scale of its column).
constexpr double kSensitivityMargin = 1000.0;

/// The relative tolerance of the one option that is computed with mathematical functions: the
/// launch rod direction of a launch into a multi-level wind (see compareLaunchOptions()). With
/// glibc it is the golden value to the last bit, because the sum the model reduces to a full
/// turn (atan2() + 2 pi) absorbs a last-bit error of atan2(); a library that is three ulps off
/// gives 1.5707963267948961 for 1.5707963267948966 (3e-16).
constexpr double kComputedDirectionRelative = 1e-12;

// =================================================================================== harness

/// SimulationDumper.RANDOM_SEED: the random seed every golden simulation runs with.
constexpr int kHarnessRandomSeed = 0;

/// SimulationDumper.EXCLUDED_TYPES: the one data type left out of the CSV files (the wall clock).
[[nodiscard]] bool isExcludedType(const FlightDataType& type)
{
    return &type == &FlightDataType::builtin(QtRocket::FlightDataTypeId::TYPE_COMPUTATION_TIME);
}

/// SimulationDumper.columnKey(): the save key of a built-in type, "custom:<name>" otherwise.
[[nodiscard]] std::string columnKey(const FlightDataType& type)
{
    return type.isBuiltin() ? type.getSaveKey() : "custom:" + type.getName();
}

/// GoldenDumper.slug(): lower-case ASCII letters and digits, every other run of characters
/// replaced by one '-' (none at the start or the end); "unnamed" when nothing is left.
[[nodiscard]] std::string slug(std::string_view text)
{
    std::string result;
    bool        dash = false;
    for (const char character : text)
    {
        const char c = (character >= 'A' && character <= 'Z')
                           ? static_cast<char>(character - 'A' + 'a')
                           : character;
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
        {
            result += (dash && !result.empty()) ? "-" : "";
            result += c;
            dash = false;
        }
        else
        {
            dash = true;
        }
    }
    return result.empty() ? std::string{"unnamed"} : result;
}

/// One level of a multi-level wind model: its altitude, speed, direction and standard deviation.
struct WindLevel
{
    double altitude;
    double speed;
    double direction;
    double standardDeviation;
};

/// MultiLevelPinkNoiseWindModel.addWindLevel() of @p level; a level the model refuses (one at an
/// altitude it already has) is a test failure.
void addWindLevel(MultiLevelPinkNoiseWindModel& model, const WindLevel& level)
{
    const QtRocket::Result<void> added =
        model.addWindLevel(level.altitude, level.speed, level.direction, level.standardDeviation);
    if (!added)
    {
        ADD_FAILURE() << added.error().message;
    }
}

/// GoldenDumper.useMultiLevelWind(): three levels up to 200 m (the harness makes them calm).
void useMultiLevelWind(SimulationOptions& options)
{
    MultiLevelPinkNoiseWindModel& multiLevel = options.getMultiLevelWindModel();
    multiLevel.clearLevels();
    addWindLevel(
        multiLevel,
        {.altitude = 0, .speed = 2.0, .direction = std::numbers::pi / 2, .standardDeviation = 0.0});
    addWindLevel(multiLevel,
                 {.altitude = 100, .speed = 4.0, .direction = 2.2, .standardDeviation = 0.0});
    addWindLevel(multiLevel,
                 {.altitude = 200, .speed = 6.0, .direction = 3.0, .standardDeviation = 0.0});
    options.setWindModelType(WindModelType::MULTI_LEVEL);
}

/// The RK6 stepper variant.
void useRk6Stepper(SimulationOptions& options)
{
    options.setSimulationStepperMethodChoice(SimulationStepperMethod::RK6);
}

/// The WGS84 geodetics variant.
void useWgs84Geodetics(SimulationOptions& options)
{
    options.setGeodeticComputation(GeodeticComputationStrategy::WGS84);
}

/// GoldenDumper.Variant: a harness-defined extra simulation of a test rocket, the default
/// simulation of one flight configuration with one option changed.
struct Variant
{
    std::string_view input;                     ///< the golden input ("testrocket-estes-alpha-iii")
    std::string_view configuration;             ///< the name of the flight configuration ("[C6-5]")
    std::string_view label;                     ///< appended to the simulation's name
    std::string_view description;               ///< the golden "variant"
    void (*apply)(SimulationOptions& options);  ///< the change
};

/// GoldenDumper.VARIANTS, in the order they are appended to the document's simulations.
constexpr std::array<Variant, 3> kVariants{{
    {.input         = "testrocket-estes-alpha-iii",
     .configuration = "[C6-5]",
     .label         = "RK6 stepper",
     .description   = "stepperMethod = RK6",
     .apply         = useRk6Stepper},
    {.input         = "testrocket-estes-alpha-iii",
     .configuration = "[C6-5]",
     .label         = "WGS84 geodetics",
     .description   = "geodeticComputation = WGS84",
     .apply         = useWgs84Geodetics},
    {.input         = "testrocket-estes-alpha-iii",
     .configuration = "[C6-5]",
     .label         = "multi-level wind",
     .description   = "windModelType = MULTI_LEVEL; levels (altitude m, speed m/s, direction rad): "
                      "(0, 2, pi/2), (100, 4, 2.2), (200, 6, 3)",
     .apply         = useMultiLevelWind},
}};

/// ApplicationPreferences.getGeodeticComputation(): the strategy whose constant name the store
/// holds.
[[nodiscard]] GeodeticComputationStrategy geodeticComputationOf(
    const QtRocket::Preferences& preferences)
{
    const std::string stored = preferences.getGeodeticComputationName();
    for (const GeodeticComputationStrategy strategy : QtRocket::kAllGeodeticComputationStrategies)
    {
        if (name(strategy) == stored)
        {
            return strategy;
        }
    }
    return GeodeticComputationStrategy::SPHERICAL;  // not reached: the store gives constant names
}

/// ApplicationPreferences.getGravityModel(): the type whose constant name the store holds.
[[nodiscard]] GravityModelType gravityModelOf(const QtRocket::Preferences& preferences)
{
    const std::string stored = preferences.getGravityModelName();
    for (const GravityModelType type : QtRocket::kAllGravityModelTypes)
    {
        if (gravityModelTypeName(type) == stored)
        {
            return type;
        }
    }
    return GravityModelType::WGS;  // not reached: the store gives constant names
}

/// GoldenDumper.applicationDefaultOptions(): the options a fresh OpenRocket installation gives a
/// new simulation. The harness's ApplicationDefaultsPreferences answers every query with the
/// default ApplicationPreferences passes; here that is an empty store. The options keep a
/// pointer to their store, so the two live together.
struct InstallationDefaults
{
    InMemoryPreferences preferences;
    SimulationOptions   options{QtRocket::DefaultSimulationOptionFactory(preferences).getDefault()};

    InstallationDefaults()
    {
        options.setTimeStep(preferences.getTimeStep());
        options.setMaxSimulationTime(preferences.getMaxSimulationTime());
        options.setGeodeticComputation(geodeticComputationOf(preferences));
        options.setGravityModelType(gravityModelOf(preferences));
        options.setConstantGravity(preferences.getConstantGravityValue());
        PinkNoiseWindModel averageWind;  // defaults.getAverageWindModel()
        averageWind.loadFrom(preferences);
        MultiLevelPinkNoiseWindModel& multiLevel = options.getMultiLevelWindModel();
        multiLevel.clearLevels();
        addWindLevel(multiLevel, {.altitude          = 0,
                                  .speed             = averageWind.getAverage(),
                                  .direction         = averageWind.getDirection(),
                                  .standardDeviation = averageWind.getStandardDeviation()});
    }
};

/// SimulationDumper.makeReproducible(): fixed seed kHarnessRandomSeed and zero standard deviation
/// of every wind model. Returns what was changed, as the golden "harness" object.
[[nodiscard]] json makeReproducible(SimulationOptions& options)
{
    json harness = json::object();
    harness["documentRandomSeed"] =
        options.isRandomSeedFixed() ? json(options.getRandomSeed()) : json(nullptr);
    harness["documentAverageWindStandardDeviation"] =
        options.getAverageWindModel().getStandardDeviation();
    json levels = json::array();
    for (const MultiLevelPinkNoiseWindModel::LevelWindModel* level :
         std::as_const(options).getMultiLevelWindModel().getLevels())
    {
        levels.push_back(level->getStandardDeviation());
    }
    harness["documentMultiLevelWindStandardDeviations"] = std::move(levels);

    options.setRandomSeedFixed(true);
    options.setRandomSeed(kHarnessRandomSeed);
    options.getAverageWindModel().setStandardDeviation(0);
    for (MultiLevelPinkNoiseWindModel::LevelWindModel* level :
         options.getMultiLevelWindModel().getLevels())
    {
        level->setStandardDeviation(0);
    }
    harness["randomSeed"]            = kHarnessRandomSeed;
    harness["windStandardDeviation"] = 0.0;
    harness["pitchYawJitterRemoved"] = true;
    return harness;
}

/// One simulation of the document the harness builds for a test rocket
/// (GoldenDumper.dumpTestRocket()).
struct PlannedSimulation
{
    std::size_t    index{0};          ///< in the document, and in the manifest's list
    int            configuration{0};  ///< in the rocket's configurations, the default one first
    std::string    name;              ///< the configuration's name, and the variant's label
    const Variant* variant{nullptr};  ///< null for the simulation of a configuration
};

/// The simulations of the document: one per flight configuration in the rocket's order, the
/// default configuration first, then the variants of the input @p input.
[[nodiscard]] std::vector<PlannedSimulation> plannedSimulations(Rocket&          rocket,
                                                                std::string_view input)
{
    const InMemoryPreferences preferences;
    std::vector<std::string>  names;  // of the configurations
    names.reserve(static_cast<std::size_t>(rocket.getConfigurationCount()) + 1);
    for (int i = 0; i <= rocket.getConfigurationCount(); i++)
    {
        names.push_back(rocket.getFlightConfigurationByIndex(i, true).getName(preferences));
    }
    std::vector<PlannedSimulation> planned;
    planned.reserve(names.size() + kVariants.size());
    for (std::size_t i = 0; i < names.size(); i++)
    {
        planned.push_back({.index         = planned.size(),
                           .configuration = static_cast<int>(i),
                           .name          = names[i],
                           .variant       = nullptr});
    }
    for (const Variant& variant : kVariants)
    {
        // GoldenDumper.configurationNamed(): the first configuration with the name.
        const auto named = std::ranges::find(names, variant.configuration);
        if (variant.input == input && named != names.end())
        {
            planned.push_back({.index         = planned.size(),
                               .configuration = static_cast<int>(named - names.begin()),
                               .name          = std::format("{} {}", *named, variant.label),
                               .variant       = &variant});
        }
    }
    return planned;
}

/// "sim_<NN>_<slug of the name>": the base name of the files of a simulation.
[[nodiscard]] std::string baseName(const PlannedSimulation& planned)
{
    return std::format("sim_{:02}_{}", planned.index, slug(planned.name));
}

// ======================================================================================= run

/// @p value moved to the next representable number away from zero; zero stays zero.
[[nodiscard]] double lastBitMoved(double value)
{
    if (value == 0 || !std::isfinite(value))
    {
        return value;
    }
    return std::nextafter(value, value > 0 ? std::numeric_limits<double>::infinity()
                                           : -std::numeric_limits<double>::infinity());
}

/// The listener of the perturbed run: after every step it moves each component of the velocity
/// and of the rotation velocity of the rocket to the next representable number. A system
/// listener, so that it stays in the nested optimum-coast simulation and adds no "listeners
/// affected the simulation" warning.
class LastBitListener final : public QtRocket::CloneableSimulationListener<LastBitListener>
{
public:
    [[nodiscard]] bool isSystemListener() const override { return true; }

    void postStep(SimulationStatus& status) override
    {
        const Coordinate& velocity = status.getRocketVelocity();
        status.setRocketVelocity(Coordinate{lastBitMoved(velocity.x), lastBitMoved(velocity.y),
                                            lastBitMoved(velocity.z), velocity.weight});
        const Coordinate& rotation = status.getRocketRotationVelocity();
        status.setRocketRotationVelocity(Coordinate{lastBitMoved(rotation.x),
                                                    lastBitMoved(rotation.y),
                                                    lastBitMoved(rotation.z), rotation.weight});
    }
};

/// One golden simulation as QtRocket runs it.
struct SimulationRun
{
    std::string                 problem;  ///< why there is no run, else ""
    PlannedSimulation           planned;
    std::unique_ptr<Rocket>     rocket;      ///< the caller's rocket
    std::unique_ptr<Simulation> simulation;  ///< with the options the run used
    json                        harness;     ///< what makeReproducible() changed
    std::string                 status;      ///< "completed" or "exception"
    std::optional<std::string>  exceptionType;
    std::optional<std::string>  exceptionMessage;
    std::int64_t                jitterReplacements{0};
    std::shared_ptr<FlightData> data;  ///< null when the engine has none
};

/// Java's getClass().getSimpleName() of @p exception.
[[nodiscard]] std::string_view simpleName(const QtRocket::SimulationException& exception)
{
    if (dynamic_cast<const QtRocket::SimulationCalculationException*>(&exception) != nullptr)
    {
        return "SimulationCalculationException";
    }
    if (dynamic_cast<const QtRocket::SimulationCancelledException*>(&exception) != nullptr)
    {
        return "SimulationCancelledException";
    }
    if (dynamic_cast<const QtRocket::SimulationListenerException*>(&exception) != nullptr)
    {
        return "SimulationListenerException";
    }
    return "SimulationException";
}

/// SimulationDumper.dump(), the run: Simulation.simulate()'s steps with the jitter-removal
/// listeners placed first and last, in the harness's order. @p perturbed adds the
/// LastBitListener.
void simulate(SimulationRun& run, bool perturbed)
{
    Simulation&                            simulation = *run.simulation;
    QtRocket::Result<SimulationConditions> made = simulation.getOptions().toSimulationConditions();
    if (!made)
    {
        run.problem = made.error().message;
        return;
    }
    const auto conditions = std::make_shared<SimulationConditions>(std::move(*made));
    conditions->setSimulation(&simulation);
    const JitterRemoval jitterRemoval;
    conditions->getSimulationListenerList().push_back(jitterRemoval.forcesListener());
    if (perturbed)
    {
        conditions->getSimulationListenerList().push_back(std::make_shared<LastBitListener>());
    }
    BasicEventSimulationEngine engine;
    run.status = "completed";
    try
    {
        for (const std::shared_ptr<QtRocket::SimulationExtension>& extension :
             simulation.getSimulationExtensions())
        {
            extension->initialize(*conditions);
        }
        conditions->getSimulationListenerList().push_back(jitterRemoval.conditionsListener());
        engine.simulate(conditions);
    }
    catch (const QtRocket::SimulationException& exception)
    {
        run.status           = "exception";
        run.exceptionType    = std::string{simpleName(exception)};
        run.exceptionMessage = exception.getMessage();
    }
    run.jitterReplacements = jitterRemoval.replacements();
    run.data               = engine.getFlightData();
}

/// GoldenDumper.dumpTestRocket() for one simulation: the rocket of @p maker, a simulation of
/// the planned configuration with the installation's default options, the variant's change, the
/// harness's changes, and the run (@p perturbed: with the LastBitListener).
[[nodiscard]] SimulationRun runSimulation(const TestRocketMaker& maker, std::size_t index,
                                          bool perturbed)
{
    const QtRocket::Test::DefaultUnitsGuard units;  // warnings print lengths and speeds
    SimulationRun                           run;
    run.rocket                                   = maker.make();
    const std::vector<PlannedSimulation> planned = plannedSimulations(*run.rocket, maker.input);
    if (index >= planned.size())
    {
        run.problem = std::format("the document has {} simulations", planned.size());
        return run;
    }
    run.planned = planned.at(index);

    const InstallationDefaults defaults;
    run.simulation = std::make_unique<Simulation>(*run.rocket);
    run.simulation->setFlightConfigurationId(
        run.rocket->getFlightConfigurationByIndex(run.planned.configuration, true).getId());
    run.simulation->setName(run.planned.name);
    run.simulation->getOptions().copyConditionsFrom(defaults.options);
    if (run.planned.variant != nullptr)
    {
        run.planned.variant->apply(run.simulation->getOptions());
    }
    run.harness = makeReproducible(run.simulation->getOptions());
    simulate(run, perturbed);
    return run;
}

// ==================================================================================== golden

/// A golden simulation: its document and the time series of its branches.
struct GoldenFiles
{
    std::string              problem;  ///< why the files could not be read, else ""
    json                     document;
    std::vector<GoldenTable> tables;  ///< one per branch CSV of the manifest
};

/// Reads the files of the simulation @p simulation of the manifest.
[[nodiscard]] GoldenFiles loadGoldenFiles(const GoldenSimulation& simulation)
{
    GoldenFiles            files;
    QtRocket::Result<json> document = QtRocket::Test::loadGoldenJson(simulation.json);
    if (!document)
    {
        files.problem = document.error().message;
        return files;
    }
    files.document = std::move(*document);
    for (const std::string& csv : simulation.branches)
    {
        QtRocket::Result<GoldenTable> table = QtRocket::Test::loadGoldenCsv(csv);
        if (!table)
        {
            files.problem = table.error().message;
            return files;
        }
        files.tables.push_back(std::move(*table));
    }
    return files;
}

// ==================================================================================== counts

/// How much of the golden simulations there is to compare, was compared, or is sensitive.
struct SimulationCounts
{
    int simulations{0};
    int branches{0};
    int events{0};
    int columns{0};
    int warnings{0};  ///< the warnings of the simulations (those of the events not counted)
    /// The numbers outside the time series: the ten summary values, the jitter replacements,
    /// and per branch the number of rows, the four values of its header, the minimum and maximum
    /// of every column, the time of every event and the parameter of every warning that has one.
    int          numbers{0};
    std::int64_t rows{0};
    std::int64_t values{0};  ///< the values of the time series: rows times columns

    [[nodiscard]] bool operator==(const SimulationCounts&) const = default;

    SimulationCounts& operator+=(const SimulationCounts& other)
    {
        simulations += other.simulations;
        branches += other.branches;
        events += other.events;
        columns += other.columns;
        warnings += other.warnings;
        numbers += other.numbers;
        rows += other.rows;
        values += other.values;
        return *this;
    }
};

/// The sum of @p a and @p b.
[[nodiscard]] SimulationCounts operator+(SimulationCounts a, const SimulationCounts& b)
{
    a += b;
    return a;
}

/// "50 simulations, 53 branches, ...", for the messages of the tests.
[[nodiscard]] std::string toText(const SimulationCounts& counts)
{
    return std::format(
        "{} simulations, {} branches, {} events, {} columns, {} warnings, {} "
        "numbers, {} rows, {} values",
        counts.simulations, counts.branches, counts.events, counts.columns, counts.warnings,
        counts.numbers, counts.rows, counts.values);
}

/// toText() of @p counts, for the messages of the tests.
std::ostream& operator<<(std::ostream& out, const SimulationCounts& counts)
{
    return out << toText(counts);
}

/// The number of golden warnings of the list @p warnings that have a parameter.
[[nodiscard]] int parameterCount(const json& warnings)
{
    int count = 0;
    for (const json& warning : warnings)
    {
        count += warning.contains("parameter") ? 1 : 0;
    }
    return count;
}

/// The numbers the golden branch @p branch holds outside its time series: the rows, the four
/// header values, two per column, one per event, and the parameters of the warnings of the
/// events.
[[nodiscard]] int branchNumbers(const json& branch)
{
    int numbers = 5 + (2 * static_cast<int>(branch.at("columns").size())) +
                  static_cast<int>(branch.at("events").size());
    for (const json& event : branch.at("events"))
    {
        const json& data = event.at("data");
        numbers += data.is_object() && data.contains("parameter") ? 1 : 0;
    }
    return numbers;
}

/// What the files @p files of one simulation hold.
[[nodiscard]] SimulationCounts goldenCounts(const GoldenFiles& files)
{
    SimulationCounts counts;
    counts.simulations = 1;
    counts.warnings    = static_cast<int>(files.document.at("warnings").size());
    // The ten summary values and the jitter replacements; the parameters of the warnings.
    counts.numbers = 11 + parameterCount(files.document.at("warnings"));
    for (const json& branch : files.document.at("branches"))
    {
        counts.branches++;
        counts.events += static_cast<int>(branch.at("events").size());
        counts.columns += static_cast<int>(branch.at("columns").size());
        counts.numbers += branchNumbers(branch);
    }
    for (const GoldenTable& table : files.tables)
    {
        counts.rows += static_cast<std::int64_t>(table.rows.size());
        counts.values += static_cast<std::int64_t>(table.rows.size() * table.columns.size());
    }
    return counts;
}

// ============================================================================== measurements

/// The largest differences from the golden files, by simulation and by what was compared; see
/// SimulationGoldenMeasurement.
class Measurements
{
public:
    /// A difference from a golden value: @p difference against the tolerance @p tolerance, of a
    /// value that was compared, or (@p sensitive) one that was not.
    void record(const std::string& context, const std::string& what, bool sensitive,
                double difference, double tolerance)
    {
        Entry& entry = m_entries[std::format("{}\t{}\t{}", context, what,
                                             sensitive ? "sensitive" : "compared")];
        entry.count++;
        entry.absolute = std::max(entry.absolute, difference);
        if (tolerance > 0)
        {
            entry.ofTolerance = std::max(entry.ofTolerance, difference / tolerance);
        }
        else if (difference > 0)
        {
            entry.ofTolerance = std::numeric_limits<double>::infinity();
        }
    }

    /// The table: one line per simulation and thing compared, with how many values, their
    /// largest difference and that difference as a multiple of its tolerance.
    [[nodiscard]] std::string text() const
    {
        std::string text;
        for (const auto& [what, entry] : m_entries)
        {
            text += std::format("{}\t{}\t{:.3e}\t{:.3e}\n", what, entry.count, entry.absolute,
                                entry.ofTolerance);
        }
        return text;
    }

    /// The largest difference of a value of the time series of the simulation @p context from
    /// its golden value, whether compared or sensitive, as a multiple of its tolerance (which is
    /// kValueRelative of the scale of its column).
    [[nodiscard]] double largestOfTheTimeSeries(const std::string& context) const
    {
        const std::string prefix  = context + "\tcolumn:";
        double            largest = 0;
        for (auto entry = m_entries.lower_bound(prefix);
             entry != m_entries.end() && entry->first.starts_with(prefix); ++entry)
        {
            largest = std::max(largest, entry->second.ofTolerance);
        }
        return largest;
    }

    /// The largest difference of a compared value, as a multiple of its tolerance.
    [[nodiscard]] double largestComparedOfTolerance() const
    {
        double largest = 0;
        for (const auto& [what, entry] : m_entries)
        {
            if (what.ends_with("\tcompared"))
            {
                largest = std::max(largest, entry.ofTolerance);
            }
        }
        return largest;
    }

private:
    struct Entry
    {
        std::int64_t count{0};
        double       absolute{0};     ///< the largest difference
        double       ofTolerance{0};  ///< the largest difference, as a multiple of its tolerance
    };
    std::map<std::string, Entry> m_entries;
};

// =============================================================================== sensitivity

/// Whether a value is reproducible: the perturbed run's value @p twin is within
/// 1/kSensitivityMargin of @p tolerance of the run's value @p actual (NaN equals NaN, and an
/// infinity the same infinity).
[[nodiscard]] bool reproducible(double actual, double twin, double tolerance)
{
    if (std::isnan(actual) || std::isnan(twin))
    {
        return std::isnan(actual) == std::isnan(twin);
    }
    return actual == twin || std::abs(actual - twin) <= tolerance / kSensitivityMargin;
}

/// How far the time series of a branch is reproducible.
struct Horizon
{
    std::size_t rows{0};       ///< the rows [0, rows) are reproducible
    bool        whole{false};  ///< every row is, and the perturbed run has as many rows
    /// The time of the last reproducible row: what the branch records up to it is reproducible.
    double time{-std::numeric_limits<double>::infinity()};
};

/// The data types of @p branch in OpenRocket's order, without the excluded ones
/// (SimulationDumper.csvTypes()).
[[nodiscard]] std::vector<const FlightDataType*> csvTypes(const FlightDataBranch& branch)
{
    std::vector<const FlightDataType*> types;
    for (const FlightDataType* type : branch.getTypes())
    {
        if (!isExcludedType(*type))
        {
            types.push_back(type);
        }
    }
    return types;
}

/// The largest finite magnitude of column @p column of @p table; 0 when it has no such column.
[[nodiscard]] double columnScale(const GoldenTable& table, std::size_t column)
{
    double scale = 0;
    if (column >= table.columns.size())
    {
        return scale;
    }
    for (const std::vector<double>& row : table.rows)
    {
        if (std::isfinite(row[column]))
        {
            scale = std::max(scale, std::abs(row[column]));
        }
    }
    return scale;
}

/// One column of a branch in the two runs, with the tolerance of the comparison of the column
/// with the golden time series.
struct TwinColumn
{
    const std::vector<double>* values{nullptr};
    const std::vector<double>* twin{nullptr};
    double                     tolerance{0};
};

/// Whether row @p row is reproducible in every one of @p columns.
[[nodiscard]] bool rowIsReproducible(const std::vector<TwinColumn>& columns, std::size_t row)
{
    return std::ranges::all_of(columns, [row](const TwinColumn& column) {
        return reproducible((*column.values)[row], (*column.twin)[row], column.tolerance);
    });
}

/// The columns of @p branch and of the perturbed run's branch @p twin, with the tolerances of
/// the comparison with the golden time series @p table; none when @p twin lacks one of them.
[[nodiscard]] std::vector<TwinColumn> twinColumns(const FlightDataBranch& branch,
                                                  const FlightDataBranch& twin,
                                                  const GoldenTable&      table)
{
    const std::vector<const FlightDataType*> types = csvTypes(branch);
    std::vector<TwinColumn>                  columns;
    for (std::size_t i = 0; i < types.size(); i++)
    {
        const std::vector<double>* twinValues = twin.getView(*types[i]);
        if (twinValues == nullptr)
        {
            return {};
        }
        columns.push_back({.values    = branch.getView(*types[i]),
                           .twin      = twinValues,
                           .tolerance = kValueRelative * columnScale(table, i)});
    }
    return columns;
}

/// How far @p branch is reproducible: up to the first row in which a value of the perturbed
/// run's branch @p twin (null: it has none) differs from the run's by more than
/// 1/kSensitivityMargin of the tolerance of its column, the tolerances being those of the
/// comparison with the golden time series @p table.
[[nodiscard]] Horizon horizonOf(const FlightDataBranch& branch, const FlightDataBranch* twin,
                                const GoldenTable& table)
{
    if (twin == nullptr)
    {
        return {};
    }
    const std::vector<TwinColumn> columns = twinColumns(branch, *twin, table);
    if (columns.empty())
    {
        return {};
    }
    const std::size_t rows             = std::min(branch.getLength(), twin->getLength());
    std::size_t       reproducibleRows = 0;
    while (reproducibleRows < rows && rowIsReproducible(columns, reproducibleRows))
    {
        reproducibleRows++;
    }
    Horizon                    horizon{.rows  = reproducibleRows,
                                       .whole = reproducibleRows == branch.getLength() &&
                                                branch.getLength() == twin->getLength() &&
                                                csvTypes(*twin).size() == columns.size()};
    const std::vector<double>* times =
        branch.getView(FlightDataType::builtin(QtRocket::FlightDataTypeId::TYPE_TIME));
    if (horizon.whole)
    {
        horizon.time = std::numeric_limits<double>::infinity();
    }
    else if (reproducibleRows > 0 && times != nullptr)
    {
        horizon.time = (*times)[reproducibleRows - 1];
    }
    return horizon;
}

/// How far a run is reproducible: the horizons of its branches, and whether the whole run is.
struct Sensitivity
{
    std::vector<Horizon> horizons;
    bool                 whole{false};
};

/// The sensitivity of @p run, measured with the perturbed run @p twin against the tolerances of
/// the comparison with the golden files @p golden.
[[nodiscard]] Sensitivity sensitivityOf(const GoldenFiles& golden, const SimulationRun& run,
                                        const SimulationRun& twin)
{
    Sensitivity sensitivity;
    if (run.data == nullptr || twin.data == nullptr)
    {
        sensitivity.whole = run.data == nullptr && twin.data == nullptr;
        return sensitivity;
    }
    sensitivity.whole = run.data->getBranchCount() == twin.data->getBranchCount() &&
                        run.jitterReplacements == twin.jitterReplacements &&
                        run.status == twin.status;
    const GoldenTable noTable;
    for (std::size_t i = 0; i < run.data->getBranchCount(); i++)
    {
        const FlightDataBranch* twinBranch =
            i < twin.data->getBranchCount() ? &twin.data->getBranch(i) : nullptr;
        sensitivity.horizons.push_back(
            horizonOf(run.data->getBranch(i), twinBranch,
                      i < golden.tables.size() ? golden.tables[i] : noTable));
        sensitivity.whole = sensitivity.whole && sensitivity.horizons.back().whole;
    }
    return sensitivity;
}

// ================================================================================ comparison

/// What a comparison works with and collects.
struct Comparison
{
    std::string      context;    ///< "testrocket-beta/sim_02_b4-3-d21-0"
    SimulationCounts compared;   ///< what was compared with the golden files
    SimulationCounts sensitive;  ///< the numbers, rows and values that are not reproducible
    std::string      report;     ///< the mismatches
    /// Whether everything is compared, reproducible or not, and the events in the order they
    /// were recorded in (the strict comparison, which no implementation passes).
    bool          strict{false};
    Measurements* measurements{nullptr};
};

/// The larger of @p scale and the magnitudes of @p expected and @p actual, as far as finite.
[[nodiscard]] double referenceOf(double scale, double expected, double actual)
{
    double reference = scale;
    for (const double value : {expected, actual})
    {
        if (std::isfinite(value))
        {
            reference = std::max(reference, std::abs(value));
        }
    }
    return reference;
}

/// Compares the number @p actual with the golden number @p expected, within @p tolerance,
/// provided it is reproducible (@p stable); a number that is not is counted as sensitive.
void compareNumber(Mismatches& m, Comparison& c, const std::string& field, double expected,
                   double actual, bool stable, double tolerance)
{
    const bool compared = stable || c.strict;
    if (c.measurements != nullptr && std::isfinite(expected) && std::isfinite(actual))
    {
        c.measurements->record(c.context, field, !compared, std::abs(actual - expected), tolerance);
    }
    if (!compared)
    {
        c.sensitive.numbers++;
        return;
    }
    c.compared.numbers++;
    m.within(field, expected, actual, 0.0, tolerance);
}

/// compareNumber() of a value: within kValueRelative of @p scale or of the larger magnitude of
/// the two numbers, whichever is larger.
void compareValue(Mismatches& m, Comparison& c, const std::string& field, double expected,
                  double actual, bool stable, double scale = 0)
{
    compareNumber(m, c, field, expected, actual, stable,
                  kValueRelative * referenceOf(scale, expected, actual));
}

/// compareNumber() of a time: within kTimeAbsolute.
void compareTime(Mismatches& m, Comparison& c, const std::string& field, double expected,
                 double actual, bool stable)
{
    compareNumber(m, c, field, expected, actual, stable, kTimeAbsolute);
}

/// Compares the count @p actual with the golden count @p expected, provided it is reproducible
/// (@p stable); a count that is not is counted as sensitive.
void compareCount(Mismatches& m, Comparison& c, std::string_view field, std::int64_t expected,
                  std::int64_t actual, bool stable)
{
    if (stable || c.strict)
    {
        c.compared.numbers++;
        m.integer(field, expected, actual);
    }
    else
    {
        c.sensitive.numbers++;
    }
}

// ----------------------------------------------------------------------------------- options

// The keys of the objects of a simulation document that the comparison reads (compares, or, for
// the first three of the file, leaves to goldens_schema_tests.cpp).
// clang-format off
constexpr std::array<std::string_view, 17> kFileKeys{
    "schema", "schemaVersion", "input", "index", "name", "flightConfiguration", "optionsSource",
    "variant", "options", "harness", "extensions", "skipped", "skipReason", "result", "summary",
    "warnings", "branches"};
constexpr std::array<std::string_view, 3>  kConfigurationKeys{"index", "id", "name"};
constexpr std::array<std::string_view, 29> kOptionKeys{
    "launchRodLength", "launchIntoWind", "launchRodAngle", "launchRodDirection", "windModelType",
    "averageWind", "multiLevelWind", "launchAltitude", "launchLatitude", "launchLongitude",
    "geodeticComputation", "isaAtmosphere", "launchTemperature", "launchPressure",
    "launchRelativeHumidity", "timeStep", "maxSimulationTime", "maximumStepAngle", "randomSeed",
    "randomSeedFixed", "gravityModelType", "constantGravity", "stepperMethod",
    "recoverySpeedWarning", "drogueLowSpeedWarning", "recoveryDrogueMainHighSpeedWarning",
    "recoveryDrogueMainLowSpeedWarning", "hasDragLookup", "hasStabilityLookup"};
constexpr std::array<std::string_view, 4>  kAverageWindKeys{
    "average", "standardDeviation", "turbulenceIntensity", "direction"};
constexpr std::array<std::string_view, 2>  kMultiLevelWindKeys{"altitudeReference", "levels"};
constexpr std::array<std::string_view, 4>  kLevelKeys{
    "altitude", "speed", "direction", "standardDeviation"};
constexpr std::array<std::string_view, 6>  kHarnessKeys{
    "documentRandomSeed", "documentAverageWindStandardDeviation",
    "documentMultiLevelWindStandardDeviations", "randomSeed", "windStandardDeviation",
    "pitchYawJitterRemoved"};
constexpr std::array<std::string_view, 4>  kResultKeys{
    "status", "exceptionType", "exceptionMessage", "jitterReplacements"};
constexpr std::array<std::string_view, 11> kSummaryKeys{
    "maxAltitude", "maxVelocity", "maxAcceleration", "maxMachNumber", "timeToApogee",
    "flightTime", "groundHitVelocity", "launchRodVelocity", "deploymentVelocity", "optimumDelay",
    "branchCount"};
constexpr std::array<std::string_view, 12> kBranchKeys{
    "index", "name", "sourceComponent", "rows", "optimumAltitude", "timeToOptimumAltitude",
    "optimumDelay", "separationTime", "csv", "excludedColumns", "columns", "events"};
constexpr std::array<std::string_view, 6>  kColumnKeys{
    "key", "name", "symbol", "builtin", "min", "max"};
constexpr std::array<std::string_view, 4>  kEventKeys{"time", "type", "source", "data"};
constexpr std::array<std::string_view, 3>  kMotorKeys{"mount", "designation", "motorCount"};
constexpr std::array<std::string_view, 2>  kAbortKeys{"cause", "description"};
// clang-format on

/// A golden string, or "null" for a JSON null.
[[nodiscard]] std::string textOrNull(const json& value)
{
    return value.is_null() ? std::string{"null"} : value.get<std::string>();
}

/// Compares the average wind model of the options with the golden "averageWind".
void compareAverageWind(Mismatches& m, const json& expected, const PinkNoiseWindModel& wind)
{
    m.exact("averageWind.average", goldenValue(expected.at("average")), wind.getAverage());
    m.exact("averageWind.standardDeviation", goldenValue(expected.at("standardDeviation")),
            wind.getStandardDeviation());
    m.exact("averageWind.turbulenceIntensity", goldenValue(expected.at("turbulenceIntensity")),
            wind.getTurbulenceIntensity());
    m.exact("averageWind.direction", goldenValue(expected.at("direction")), wind.getDirection());
    noteUncomparedKeys(m, "averageWind", expected, kAverageWindKeys);
}

/// Compares the multi-level wind model of the options with the golden "multiLevelWind".
void compareMultiLevelWind(Mismatches& m, const json& expected,
                           const MultiLevelPinkNoiseWindModel& wind)
{
    m.text("multiLevelWind.altitudeReference", expected.at("altitudeReference").get<std::string>(),
           wind.getAltitudeReference() == MultiLevelPinkNoiseWindModel::AltitudeReference::MSL
               ? "MSL"
               : "AGL");
    const std::vector<const MultiLevelPinkNoiseWindModel::LevelWindModel*> levels =
        wind.getLevels();
    const json& expectedLevels = expected.at("levels");
    m.integer("multiLevelWind.levels: number", static_cast<std::int64_t>(expectedLevels.size()),
              static_cast<std::int64_t>(levels.size()));
    for (std::size_t i = 0; i < std::min(levels.size(), expectedLevels.size()); i++)
    {
        const std::string field = std::format("multiLevelWind.levels[{}]", i);
        const json&       level = expectedLevels.at(i);
        m.exact(field + ".altitude", goldenValue(level.at("altitude")), levels[i]->getAltitude());
        m.exact(field + ".speed", goldenValue(level.at("speed")), levels[i]->getSpeed());
        m.exact(field + ".direction", goldenValue(level.at("direction")),
                levels[i]->getDirection());
        m.exact(field + ".standardDeviation", goldenValue(level.at("standardDeviation")),
                levels[i]->getStandardDeviation());
        noteUncomparedKeys(m, field, level, kLevelKeys);
    }
    noteUncomparedKeys(m, "multiLevelWind", expected, kMultiLevelWindKeys);
}

/// Compares the launch site and the atmosphere of @p options with the golden "options".
void compareLaunchOptions(Mismatches& m, const json& expected, const SimulationOptions& options)
{
    m.exact("launchRodLength", goldenValue(expected.at("launchRodLength")),
            options.getLaunchRodLength());
    m.boolean("launchIntoWind", expected.at("launchIntoWind").get<bool>(),
              options.getLaunchIntoWind());
    m.exact("launchRodAngle", goldenValue(expected.at("launchRodAngle")),
            options.getLaunchRodAngle());
    if (options.getLaunchIntoWind() && options.getWindModelType() == WindModelType::MULTI_LEVEL)
    {
        // Not a setting but a result: the direction of the model's wind at the launch site,
        // atan2() of the components that sin() and cos() of the level's direction gave.
        m.within("launchRodDirection", goldenValue(expected.at("launchRodDirection")),
                 options.getLaunchRodDirection(), kComputedDirectionRelative, 0.0);
    }
    else
    {
        m.exact("launchRodDirection", goldenValue(expected.at("launchRodDirection")),
                options.getLaunchRodDirection());
    }
    m.exact("launchAltitude", goldenValue(expected.at("launchAltitude")),
            options.getLaunchAltitude());
    m.exact("launchLatitude", goldenValue(expected.at("launchLatitude")),
            options.getLaunchLatitude());
    m.exact("launchLongitude", goldenValue(expected.at("launchLongitude")),
            options.getLaunchLongitude());
    m.text("geodeticComputation", expected.at("geodeticComputation").get<std::string>(),
           name(options.getGeodeticComputation()));
    m.boolean("isaAtmosphere", expected.at("isaAtmosphere").get<bool>(), options.isIsaAtmosphere());
    m.exact("launchTemperature", goldenValue(expected.at("launchTemperature")),
            options.getLaunchTemperature());
    m.exact("launchPressure", goldenValue(expected.at("launchPressure")),
            options.getLaunchPressure());
    m.exact("launchRelativeHumidity", goldenValue(expected.at("launchRelativeHumidity")),
            options.getLaunchRelativeHumidity());
}

/// Compares the options a run used, @p options, with the golden "options", field by field: they
/// are settings, so every number has to be the golden one exactly.
void compareOptions(Mismatches& m, const json& expected, const SimulationOptions& options)
{
    compareLaunchOptions(m, expected, options);
    m.text("windModelType", expected.at("windModelType").get<std::string>(),
           windModelTypeName(options.getWindModelType()));
    compareAverageWind(m, expected.at("averageWind"), options.getAverageWindModel());
    compareMultiLevelWind(m, expected.at("multiLevelWind"), options.getMultiLevelWindModel());
    m.exact("timeStep", goldenValue(expected.at("timeStep")), options.getTimeStep());
    m.exact("maxSimulationTime", goldenValue(expected.at("maxSimulationTime")),
            options.getMaxSimulationTime());
    m.exact("maximumStepAngle", goldenValue(expected.at("maximumStepAngle")),
            options.getMaximumStepAngle());
    m.integer("randomSeed", expected.at("randomSeed").get<std::int64_t>(), options.getRandomSeed());
    m.boolean("randomSeedFixed", expected.at("randomSeedFixed").get<bool>(),
              options.isRandomSeedFixed());
    m.text("gravityModelType", expected.at("gravityModelType").get<std::string>(),
           gravityModelTypeName(options.getGravityModelType()));
    m.exact("constantGravity", goldenValue(expected.at("constantGravity")),
            options.getConstantGravity());
    m.text("stepperMethod", expected.at("stepperMethod").get<std::string>(),
           simulationStepperMethodName(options.getSimulationStepperMethodChoice()));
    m.exact("recoverySpeedWarning", goldenValue(expected.at("recoverySpeedWarning")),
            options.getRecoverySpeedWarning());
    m.exact("drogueLowSpeedWarning", goldenValue(expected.at("drogueLowSpeedWarning")),
            options.getDrogueLowSpeedWarning());
    m.exact("recoveryDrogueMainHighSpeedWarning",
            goldenValue(expected.at("recoveryDrogueMainHighSpeedWarning")),
            options.getRecoveryDrogueMainHighSpeedWarning());
    m.exact("recoveryDrogueMainLowSpeedWarning",
            goldenValue(expected.at("recoveryDrogueMainLowSpeedWarning")),
            options.getRecoveryDrogueMainLowSpeedWarning());
    m.boolean("hasDragLookup", expected.at("hasDragLookup").get<bool>(), options.hasDragLookup());
    m.boolean("hasStabilityLookup", expected.at("hasStabilityLookup").get<bool>(),
              options.hasStabilityLookup());
    noteUncomparedKeys(m, "", expected, kOptionKeys);
}

/// Compares what makeReproducible() changed, @p actual, with the golden "harness".
void compareHarness(Mismatches& m, const json& expected, const json& actual)
{
    for (const std::string_view key : kHarnessKeys)
    {
        if (!expected.contains(key) || expected.at(key) != actual.at(key))
        {
            m.text(key, expected.contains(key) ? expected.at(key).dump() : "nothing",
                   actual.at(key).dump());
        }
    }
    noteUncomparedKeys(m, "", expected, kHarnessKeys);
}

// ------------------------------------------------------------------------------------ header

/// Compares what identifies the simulation: its index and name, its flight configuration, where
/// its options come from, its variant, and that it has no extensions and was not skipped.
void compareHeader(Mismatches& m, const json& expected, const SimulationRun& run,
                   bool randomConfigurationId)
{
    const InMemoryPreferences  preferences;
    const FlightConfiguration& config =
        run.rocket->getFlightConfigurationByIndex(run.planned.configuration, true);
    m.integer("index", expected.at("index").get<std::int64_t>(),
              static_cast<std::int64_t>(run.planned.index));
    m.text("name", expected.at("name").get<std::string>(), run.simulation->getName());
    const json& configuration = expected.at("flightConfiguration");
    m.integer("flightConfiguration.index", configuration.at("index").get<std::int64_t>(),
              run.planned.configuration);
    if (config.getId().isDefaultId() || !randomConfigurationId)
    {
        m.text("flightConfiguration.id", configuration.at("id").get<std::string>(),
               run.simulation->getFlightConfigurationId().toString());
    }
    m.text("flightConfiguration.name", configuration.at("name").get<std::string>(),
           config.getName(preferences));
    noteUncomparedKeys(m, "flightConfiguration", configuration, kConfigurationKeys);
    m.text("optionsSource", expected.at("optionsSource").get<std::string>(), "applicationDefaults");
    m.text("variant", textOrNull(expected.at("variant")),
           run.planned.variant != nullptr ? std::string{run.planned.variant->description}
                                          : std::string{"null"});
    m.integer("extensions: number", static_cast<std::int64_t>(expected.at("extensions").size()),
              static_cast<std::int64_t>(run.simulation->getSimulationExtensions().size()));
    m.boolean("skipped", expected.at("skipped").get<bool>(), false);
    m.text("skipReason", textOrNull(expected.at("skipReason")), "null");
    noteUncomparedKeys(m, "", expected, kFileKeys);
}

// ------------------------------------------------------------------------ result and summary

/// Compares how the run ended with the golden "result". The number of jitter replacements is
/// that of the force calculations of the run, so it is reproducible when the whole run is
/// (@p whole).
void compareResult(Mismatches& m, Comparison& c, const json& expected, const SimulationRun& run,
                   bool whole)
{
    m.text("status", expected.at("status").get<std::string>(), run.status);
    m.text("exceptionType", textOrNull(expected.at("exceptionType")),
           run.exceptionType.value_or("null"));
    m.text("exceptionMessage", textOrNull(expected.at("exceptionMessage")),
           run.exceptionMessage.value_or("null"));
    compareCount(m, c, "jitterReplacements", expected.at("jitterReplacements").get<std::int64_t>(),
                 run.jitterReplacements, whole);
    noteUncomparedKeys(m, "", expected, kResultKeys);
}

/// A summary value: its golden key, whether it is a time, and its getter.
struct SummaryValue
{
    std::string_view key;
    bool             time;
    double (FlightData::*get)() const noexcept;
};

/// The summary values of SimulationDumper.summary() that the whole flight decides.
constexpr std::array<SummaryValue, 9> kSummaryValues{{
    {.key = "maxAltitude", .time = false, .get = &FlightData::getMaxAltitude},
    {.key = "maxVelocity", .time = false, .get = &FlightData::getMaxVelocity},
    {.key = "maxAcceleration", .time = false, .get = &FlightData::getMaxAcceleration},
    {.key = "maxMachNumber", .time = false, .get = &FlightData::getMaxMachNumber},
    {.key = "timeToApogee", .time = true, .get = &FlightData::getTimeToApogee},
    {.key = "flightTime", .time = true, .get = &FlightData::getFlightTime},
    {.key = "groundHitVelocity", .time = false, .get = &FlightData::getGroundHitVelocity},
    {.key = "deploymentVelocity", .time = false, .get = &FlightData::getDeploymentVelocity},
    {.key = "optimumDelay", .time = true, .get = &FlightData::getOptimumDelay},
}};

/// Whether the first branch of @p data records the clearing of the launch rod in its
/// reproducible part, whose horizon is @p horizon.
[[nodiscard]] bool launchRodIsReproducible(const FlightData& data, const Horizon& horizon)
{
    if (data.getBranchCount() == 0)
    {
        return horizon.whole;
    }
    const FlightEvent* cleared = data.getBranch(0).getFirstEvent(FlightEvent::Type::LAUNCHROD);
    return horizon.whole || (cleared != nullptr && cleared->getTime() <= horizon.time);
}

/// Compares the summary values of @p data with the golden "summary". The whole flight decides
/// them, so they are reproducible when the whole run is; the launch rod velocity is that of
/// the moment the rod is cleared.
void compareSummary(Mismatches& m, Comparison& c, const json& expected, const FlightData& data,
                    const Sensitivity& sensitivity)
{
    for (const SummaryValue& value : kSummaryValues)
    {
        const std::string field{value.key};
        const double      golden = goldenValue(expected.at(value.key));
        if (value.time)
        {
            compareTime(m, c, field, golden, (data.*value.get)(), sensitivity.whole);
        }
        else
        {
            compareValue(m, c, field, golden, (data.*value.get)(), sensitivity.whole);
        }
    }
    const Horizon first = sensitivity.horizons.empty() ? Horizon{} : sensitivity.horizons.front();
    compareValue(m, c, "launchRodVelocity", goldenValue(expected.at("launchRodVelocity")),
                 data.getLaunchRodVelocity(),
                 sensitivity.whole || launchRodIsReproducible(data, first));
    m.integer("branchCount", expected.at("branchCount").get<std::int64_t>(),
              static_cast<std::int64_t>(data.getBranchCount()));
    noteUncomparedKeys(m, "", expected, kSummaryKeys);
}

// ---------------------------------------------------------------------------------- warnings

/// Compares the warning @p actual with the golden warning @p expected. A warning with a
/// parameter (a speed or an angle the simulation computed, which its text prints, and which a
/// later warning of the same kind replaces) is compared in full when the whole run is
/// reproducible (@p whole), and by its class, priority and sources otherwise.
void compareSimulationWarning(Mismatches& m, Comparison& c, const std::string& field,
                              const json& expected, const Warning& actual, bool whole,
                              const Rocket& rocket)
{
    const std::optional<double> parameter = parameterOf(actual);
    if (!parameter.has_value())
    {
        compareWarning(m, field, expected, actual, rocket, kValueRelative);
        return;
    }
    const bool compared = whole || c.strict;
    if (c.measurements != nullptr && expected.contains("parameter"))
    {
        c.measurements->record(c.context, field + ".parameter", !compared,
                               std::abs(*parameter - goldenValue(expected.at("parameter"))),
                               kValueRelative * std::abs(*parameter));
    }
    if (compared)
    {
        c.compared.numbers++;
        compareWarning(m, field, expected, actual, rocket, kValueRelative);
    }
    else
    {
        c.sensitive.numbers++;
        compareWarningIdentity(m, field, expected, actual, rocket);
    }
}

/// Compares the warnings of @p data with the golden "warnings": as many, and each one, in
/// order. Returns the number of golden warnings compared.
[[nodiscard]] int compareSimulationWarnings(Mismatches& m, Comparison& c, const json& expected,
                                            const FlightData& data, bool whole,
                                            const Rocket& rocket)
{
    m.integer("warnings: number", static_cast<std::int64_t>(expected.size()),
              static_cast<std::int64_t>(data.getWarningSet().size()));
    int         compared = 0;
    std::size_t index    = 0;
    for (const Warning& warning : data.getWarningSet())
    {
        if (index < expected.size())
        {
            compareSimulationWarning(m, c, std::format("warnings[{}]", index), expected.at(index),
                                     warning, whole, rocket);
            compared++;
        }
        index++;
    }
    return compared;
}

// ------------------------------------------------------------------------------------ events

/// The golden path of @p component, or "null" for none.
[[nodiscard]] std::string pathOrNull(const RocketComponent* component)
{
    return component == nullptr ? std::string{"null"} : goldenPathOf(*component);
}

/// What decides the place of an event in the comparison.
struct EventKey
{
    bool        ignition{false};
    double      time{0};
    std::string source;
};

/// The order in which events are compared: as they were recorded, except that consecutive
/// IGNITION events with the same time are sorted by the path of their source. One event
/// ignites the motors of several mounts at once; OpenRocket queues them in the order of a hash
/// map, which changes from one run of OpenRocket to the next, and QtRocket in the order of the
/// component tree. (@p recorded: the order they were recorded in, for the strict comparison.)
[[nodiscard]] std::vector<std::size_t> comparisonOrder(const std::vector<EventKey>& keys,
                                                       bool                         recorded)
{
    std::vector<std::size_t> order(keys.size());
    for (std::size_t i = 0; i < order.size(); i++)
    {
        order[i] = i;
    }
    std::size_t begin = 0;
    while (!recorded && begin < keys.size())
    {
        std::size_t end = begin + 1;
        while (end < keys.size() && keys[begin].ignition && keys[end].ignition &&
               keys[end].time == keys[begin].time)
        {
            end++;
        }
        std::ranges::sort(std::span<std::size_t>{order}.subspan(begin, end - begin), {},
                          [&keys](std::size_t i) { return keys[i].source; });
        begin = end;
    }
    return order;
}

/// The events of @p branch in the order of the comparison.
[[nodiscard]] std::vector<const FlightEvent*> orderedEvents(const FlightDataBranch& branch,
                                                            bool                    recorded)
{
    const std::vector<FlightEvent>& events = branch.getEvents();
    std::vector<EventKey>           keys;
    keys.reserve(events.size());
    for (const FlightEvent& event : events)
    {
        keys.push_back({.ignition = event.getType() == FlightEvent::Type::IGNITION,
                        .time     = event.getTime(),
                        .source   = pathOrNull(event.getSource())});
    }
    std::vector<const FlightEvent*> ordered;
    ordered.reserve(events.size());
    for (const std::size_t i : comparisonOrder(keys, recorded))
    {
        ordered.push_back(&events[i]);
    }
    return ordered;
}

/// The golden events @p events in the order of the comparison.
[[nodiscard]] std::vector<const json*> orderedGoldenEvents(const json& events, bool recorded)
{
    std::vector<EventKey> keys;
    keys.reserve(events.size());
    for (const json& event : events)
    {
        keys.push_back({.ignition = event.at("type").get<std::string>() == "IGNITION",
                        .time     = goldenValue(event.at("time")),
                        .source   = textOrNull(event.at("source"))});
    }
    std::vector<const json*> ordered;
    ordered.reserve(events.size());
    for (const std::size_t i : comparisonOrder(keys, recorded))
    {
        ordered.push_back(&events.at(i));
    }
    return ordered;
}

/// The types and sources of @p events, one after the other: "LAUNCH(/) IGNITION(/0/1/2) ...".
[[nodiscard]] std::string sequenceOf(const std::vector<const FlightEvent*>& events)
{
    std::string sequence;
    for (const FlightEvent* event : events)
    {
        sequence += sequence.empty() ? "" : " ";
        sequence += std::format("{}({})", name(event->getType()), pathOrNull(event->getSource()));
    }
    return sequence;
}

/// sequenceOf() of golden events.
[[nodiscard]] std::string sequenceOf(const std::vector<const json*>& events)
{
    std::string sequence;
    for (const json* event : events)
    {
        sequence += sequence.empty() ? "" : " ";
        sequence += std::format("{}({})", event->at("type").get<std::string>(),
                                textOrNull(event->at("source")));
    }
    return sequence;
}

/// The kind of the golden event data @p data: "none", "motor", "abort", "warning", "text" or
/// "other".
[[nodiscard]] std::string_view goldenDataKind(const json& data)
{
    if (data.is_null())
    {
        return "none";
    }
    if (data.is_string())
    {
        return "text";
    }
    if (!data.is_object())
    {
        return "other";
    }
    if (data.contains("mount"))
    {
        return "motor";
    }
    if (data.contains("cause"))
    {
        return "abort";
    }
    return data.contains("type") ? "warning" : "other";
}

/// The kind of the data of @p event, as goldenDataKind() names it.
[[nodiscard]] std::string_view dataKind(const FlightEvent& event)
{
    if (!event.hasData())
    {
        return "none";
    }
    if (event.getMotorState() != nullptr)
    {
        return "motor";
    }
    if (event.getAbort() != nullptr)
    {
        return "abort";
    }
    if (event.getWarning() != nullptr)
    {
        return "warning";
    }
    return event.getMessage() != nullptr ? "text" : "other";
}

/// Compares the motor state of @p event with the golden data @p expected.
void compareMotorData(Mismatches& m, const std::string& field, const json& expected,
                      const FlightEvent& event)
{
    const std::shared_ptr<QtRocket::MotorClusterState> state = event.getMotorState();
    m.text(field + ".mount", expected.at("mount").get<std::string>(),
           goldenPathOf(asComponent(state->getMount())));
    const QtRocket::Motor& motor = *state->getMotor();
    m.text(field + ".designation", expected.at("designation").get<std::string>(),
           motor.getDesignation());
    m.integer(field + ".motorCount", expected.at("motorCount").get<std::int64_t>(),
              state->getMotorCount());
    noteUncomparedKeys(m, field, expected, kMotorKeys);
}

/// Compares the abort of @p event with the golden data @p expected.
void compareAbortData(Mismatches& m, const std::string& field, const json& expected,
                      const FlightEvent& event)
{
    const QtRocket::SimulationAbort* abort = event.getAbort();
    m.text(field + ".cause", expected.at("cause").get<std::string>(), causeName(abort->cause()));
    m.text(field + ".description", expected.at("description").get<std::string>(),
           abort->messageDescription());
    noteUncomparedKeys(m, field, expected, kAbortKeys);
}

/// What the comparison of a branch works with.
struct BranchOf
{
    const FlightData*       data{nullptr};
    const FlightDataBranch* branch{nullptr};
    const Rocket*           rocket{nullptr};
    Horizon                 horizon;       ///< how far the branch is reproducible
    bool                    whole{false};  ///< whether the whole run is
};

/// Compares the data of @p event with the golden "data" @p expected, by its kind.
void compareEventData(Mismatches& m, Comparison& c, const std::string& field, const json& expected,
                      const FlightEvent& event, const BranchOf& ours)
{
    const std::string_view kind = goldenDataKind(expected);
    m.text(field + ": kind", kind, dataKind(event));
    if (kind != dataKind(event))
    {
        return;
    }
    if (kind == "motor")
    {
        compareMotorData(m, field, expected, event);
    }
    else if (kind == "abort")
    {
        compareAbortData(m, field, expected, event);
    }
    else if (kind == "warning")
    {
        // Java's event shares the object of the warning set, which a later warning of the same
        // kind updates; here the event holds a copy, and the set's warning is found by it.
        const Warning* warning = ours.data->findWarning(event);
        if (warning == nullptr)
        {
            m.note(field + ": the warning of the event is not in the warning set");
            return;
        }
        compareSimulationWarning(m, c, field, expected, *warning, ours.whole, *ours.rocket);
    }
    else if (kind == "text")
    {
        m.text(field, expected.get<std::string>(), *event.getMessage());
    }
    else if (kind == "other")
    {
        m.note(field + ": a kind of data the comparison does not know");
    }
}

/// Whether the time of @p event is fixed by the reproducible part of its branch, whatever the
/// trajectory after it: the LAUNCH, and the BURNOUT of a motor that ignited in that part (the
/// time at which its IGNITION was handled plus the burn time).
///
/// The other events the motors time are not: OpenRocket gives a queued event a time relative to
/// the simulation time at which the event that queues it is HANDLED (BasicEventSimulationEngine:
/// an EJECTION_CHARGE is the handling time of its BURNOUT plus the delay, a later IGNITION the
/// handling time of what ignites it plus the ignition delay, a STAGE_SEPARATION follows one of
/// those), and an event is handled at the end of the first step that reaches its time. That is
/// its time when the step is cut to end there, but a step that the angle limits push below a
/// twentieth of the time step is raised to that minimum "even at the cost of not being quite on
/// an event" (AbstractRKSimulationStepper.computeTimeStep()), so the handling time can lie up to
/// 0.0025 s later. Beyond the horizon the sequence of steps is not reproducible, so neither is
/// that handling time: on Windows (MSVC's math library) the BURNOUT at 2.1 s of the Estes
/// Alpha III's C6 flights is handled at 2.100235 s, which moves the EJECTION_CHARGE by
/// 0.000235 s, where OpenRocket, glibc under every pattern of the libm shim, and macOS all land
/// on 2.1 s.
[[nodiscard]] bool isTimedInTheReproduciblePart(const FlightEvent& event, const Horizon& horizon)
{
    if (event.getType() == FlightEvent::Type::LAUNCH)
    {
        return true;
    }
    if (event.getType() != FlightEvent::Type::BURNOUT)
    {
        return false;
    }
    const std::shared_ptr<QtRocket::MotorClusterState> state = event.getMotorState();
    return state != nullptr && state->getIgnitionTime() <= horizon.time;
}

/// Compares the events of the branch with the golden "events": first their types and sources
/// in order, as one text; then, when they are the same, the time and the data of each. The time
/// of an event is reproducible when the branch recorded it in its reproducible part, or when
/// that part fixes it (isTimedInTheReproduciblePart()). Returns the number of golden events
/// compared.
[[nodiscard]] int compareEvents(Mismatches& m, Comparison& c, const json& expected,
                                const BranchOf& ours)
{
    const std::vector<const FlightEvent*> events   = orderedEvents(*ours.branch, c.strict);
    const std::vector<const json*>        golden   = orderedGoldenEvents(expected, c.strict);
    const std::string                     sequence = sequenceOf(events);
    m.text("events", sequenceOf(golden), sequence);
    if (sequenceOf(golden) != sequence)
    {
        return 0;
    }
    for (std::size_t i = 0; i < events.size(); i++)
    {
        const std::string  field = std::format("events[{}]", i);
        const FlightEvent& event = *events[i];
        compareTime(m, c, field + ".time", goldenValue(golden[i]->at("time")), event.getTime(),
                    event.getTime() <= ours.horizon.time ||
                        isTimedInTheReproduciblePart(event, ours.horizon));
        compareEventData(m, c, field + ".data", golden[i]->at("data"), event, ours);
        noteUncomparedKeys(m, field, *golden[i], kEventKeys);
    }
    return static_cast<int>(events.size());
}

// ----------------------------------------------------------------------------------- columns

/// The keys of the excluded data types of @p branch, separated by spaces.
[[nodiscard]] std::string excludedKeys(const FlightDataBranch& branch)
{
    std::string keys;
    for (const FlightDataType* type : branch.getTypes())
    {
        if (isExcludedType(*type))
        {
            keys += keys.empty() ? "" : " ";
            keys += columnKey(*type);
        }
    }
    return keys;
}

/// The strings of the golden list @p list, separated by spaces.
[[nodiscard]] std::string joined(const json& list)
{
    std::string text;
    for (const json& entry : list)
    {
        text += text.empty() ? "" : " ";
        text += entry.get<std::string>();
    }
    return text;
}

/// Whether @p actual differs from the golden @p expected by more than @p tolerance: NaN equals
/// NaN, and an infinity the same infinity.
[[nodiscard]] bool differs(double expected, double actual, double tolerance)
{
    if (std::isnan(expected) || std::isnan(actual))
    {
        return std::isnan(expected) != std::isnan(actual);
    }
    return expected != actual && !(std::abs(actual - expected) <= tolerance);
}

/// One column of a branch against one column of the golden time series.
struct ColumnOf
{
    std::string                key;
    const GoldenTable*         table{nullptr};
    std::size_t                column{0};
    const std::vector<double>* values{nullptr};
    double                     scale{0};
};

/// Records the differences of the column @p column from the golden one: those of the first
/// @p rows rows, which are compared, and those of the later ones, which are sensitive.
void measureColumn(const Comparison& c, const ColumnOf& column, std::size_t rows)
{
    const std::size_t common = std::min(column.values->size(), column.table->rows.size());
    for (std::size_t row = 0; row < common; row++)
    {
        const double expected = column.table->rows[row][column.column];
        const double actual   = (*column.values)[row];
        if (std::isfinite(expected) && std::isfinite(actual))
        {
            c.measurements->record(c.context, "column:" + column.key, row >= rows,
                                   std::abs(actual - expected), kValueRelative * column.scale);
        }
    }
}

/// Compares the first @p rows values of the column @p column with the golden ones, row by row:
/// each within kValueRelative of the scale of the column. The rows that differ are reported in
/// one line: how many, and the first of them.
void compareColumnValues(Mismatches& m, const Comparison& c, const ColumnOf& column,
                         std::size_t rows)
{
    if (c.measurements != nullptr)
    {
        measureColumn(c, column, rows);
    }
    const double tolerance = kValueRelative * column.scale;
    std::size_t  differing = 0;
    std::size_t  first     = 0;
    for (std::size_t row = 0; row < rows; row++)
    {
        if (differs(column.table->rows[row][column.column], (*column.values)[row], tolerance))
        {
            first = differing == 0 ? row : first;
            differing++;
        }
    }
    if (differing > 0)
    {
        const double expected = column.table->rows[first][column.column];
        const double actual   = (*column.values)[first];
        m.note(
            std::format("column {}: {} of {} rows differ, the first at row {}: expected {}, "
                        "got {} (difference {})",
                        column.key, differing, rows, first, expected, actual, actual - expected));
    }
}

/// Compares the columns of the branch with the golden "columns" (key, name, symbol, whether
/// built in, in order; minimum and maximum when the whole branch is reproducible) and with the
/// golden time series @p table: every value of the first @p rows rows. Returns the number of
/// golden columns compared.
[[nodiscard]] int compareColumns(Mismatches& m, Comparison& c, const json& expected,
                                 const GoldenTable& table, const BranchOf& ours, std::size_t rows)
{
    const FlightDataBranch&                  branch = *ours.branch;
    const std::vector<const FlightDataType*> types  = csvTypes(branch);
    m.integer("columns: number", static_cast<std::int64_t>(expected.size()),
              static_cast<std::int64_t>(types.size()));
    int compared = 0;
    for (std::size_t i = 0; i < std::min({types.size(), expected.size(), table.columns.size()});
         i++)
    {
        const std::string     field  = std::format("columns[{}]", i);
        const json&           column = expected.at(i);
        const FlightDataType& type   = *types[i];
        const std::string     key    = columnKey(type);
        m.text(field + ".key", column.at("key").get<std::string>(), key);
        m.text(field + ".name", column.at("name").get<std::string>(), type.getName());
        m.text(field + ".symbol", column.at("symbol").get<std::string>(), type.getSymbol());
        m.boolean(field + ".builtin", column.at("builtin").get<bool>(), type.isBuiltin());
        const double scale = columnScale(table, i);
        compareValue(m, c, std::format("{}.min ({})", field, key), goldenValue(column.at("min")),
                     branch.getMinimum(type), ours.horizon.whole, scale);
        compareValue(m, c, std::format("{}.max ({})", field, key), goldenValue(column.at("max")),
                     branch.getMaximum(type), ours.horizon.whole, scale);
        noteUncomparedKeys(m, field, column, kColumnKeys);
        compareColumnValues(m, c,
                            {.key    = key,
                             .table  = &table,
                             .column = i,
                             .values = branch.getView(type),
                             .scale  = scale},
                            rows);
        compared++;
    }
    return compared;
}

// ------------------------------------------------------------------------------------ branch

/// Compares the header of the branch with the golden branch @p expected: its index, name and
/// source component, the name of its file and its excluded columns; the separation time when
/// the stage separated in the reproducible part; and, when the whole branch is reproducible, its
/// number of rows, the optimum altitude, the time to it and the optimum delay.
void compareBranchHeader(Mismatches& m, Comparison& c, const json& expected, std::size_t index,
                         const BranchOf& ours, const PlannedSimulation& planned)
{
    const FlightDataBranch& branch = *ours.branch;
    const bool              whole  = ours.horizon.whole;
    m.integer("index", expected.at("index").get<std::int64_t>(), static_cast<std::int64_t>(index));
    m.text("name", expected.at("name").get<std::string>(), branch.getName());
    const std::optional<QtRocket::Uuid>& source = branch.getSourceComponentId();
    m.text(
        "sourceComponent", textOrNull(expected.at("sourceComponent")),
        source.has_value() ? pathOrNull(ours.rocket->findComponent(*source)) : std::string{"null"});
    compareCount(m, c, "rows", expected.at("rows").get<std::int64_t>(),
                 static_cast<std::int64_t>(branch.getLength()), whole);
    compareValue(m, c, "optimumAltitude", goldenValue(expected.at("optimumAltitude")),
                 branch.getOptimumAltitude(), whole);
    compareTime(m, c, "timeToOptimumAltitude", goldenValue(expected.at("timeToOptimumAltitude")),
                branch.getTimeToOptimumAltitude(), whole);
    compareTime(m, c, "optimumDelay", goldenValue(expected.at("optimumDelay")),
                branch.getOptimumDelay(), whole);
    // The time of the STAGE_SEPARATION event (NaN without one): reproducible like that event's.
    compareTime(m, c, "separationTime", goldenValue(expected.at("separationTime")),
                branch.getSeparationTime(), !(branch.getSeparationTime() > ours.horizon.time));
    m.text("csv", expected.at("csv").get<std::string>(),
           std::format("{}_branch{}.csv.gz", baseName(planned), index));
    m.text("excludedColumns", joined(expected.at("excludedColumns")), excludedKeys(branch));
    noteUncomparedKeys(m, "", expected, kBranchKeys);
}

/// Compares branch @p index of @p run with the golden branch @p expected and its time series
/// @p table; @p sensitivity says how far the run is reproducible.
void compareBranch(Comparison& c, std::size_t index, const json& expected, const GoldenTable& table,
                   const SimulationRun& run, const Sensitivity& sensitivity)
{
    Mismatches     m(std::format("{} branch {}", c.context, index));
    const BranchOf ours{.data    = run.data.get(),
                        .branch  = &run.data->getBranch(index),
                        .rocket  = run.rocket.get(),
                        .horizon = sensitivity.horizons.at(index),
                        .whole   = sensitivity.whole};
    compareBranchHeader(m, c, expected, index, ours, run.planned);

    // The rows compared: those that are reproducible, as far as both sides have them.
    const std::size_t rows = std::min({c.strict ? ours.branch->getLength() : ours.horizon.rows,
                                       ours.branch->getLength(), table.rows.size()});
    c.compared.branches++;
    c.compared.columns += compareColumns(m, c, expected.at("columns"), table, ours, rows);
    c.compared.events += compareEvents(m, c, expected.at("events"), ours);
    const auto columns = static_cast<std::int64_t>(table.columns.size());
    c.compared.rows += static_cast<std::int64_t>(rows);
    c.compared.values += static_cast<std::int64_t>(rows) * columns;
    if (!c.strict && !ours.horizon.whole)
    {
        // The rows of the golden time series from the horizon on.
        const auto sensitive = static_cast<std::int64_t>(table.rows.size() - rows);
        c.sensitive.rows += sensitive;
        c.sensitive.values += sensitive * columns;
    }
    c.report += m.report();
}

// -------------------------------------------------------------------------------- simulation

/// What the comparison of a run with its golden files compared and found.
struct SimulationComparison
{
    SimulationCounts compared;
    SimulationCounts sensitive;
    std::string      report;  ///< the mismatches, empty when everything matched
};

/// Compares the data of @p run with the golden "summary", "warnings" and "branches".
void compareData(Comparison& c, const GoldenFiles& golden, const SimulationRun& run,
                 const Sensitivity& sensitivity)
{
    const json& document = golden.document;
    Mismatches  m(c.context);
    if (run.data == nullptr)
    {
        m.boolean("has flight data", !document.at("summary").is_null(), false);
        c.report += m.report();
        return;
    }
    Mismatches summary(c.context + " summary");
    compareSummary(summary, c, document.at("summary"), *run.data, sensitivity);
    c.report += summary.report();

    c.compared.warnings += compareSimulationWarnings(m, c, document.at("warnings"), *run.data,
                                                     sensitivity.whole, *run.rocket);
    const json& branches = document.at("branches");
    m.integer("branches: number", static_cast<std::int64_t>(branches.size()),
              static_cast<std::int64_t>(run.data->getBranchCount()));
    c.report += m.report();
    for (std::size_t i = 0;
         i < std::min({branches.size(), run.data->getBranchCount(), golden.tables.size()}); i++)
    {
        compareBranch(c, i, branches.at(i), golden.tables[i], run, sensitivity);
    }
}

/// How compareSimulation() compares.
struct ComparisonMode
{
    bool          randomConfigurationId{false};  ///< the maker draws the configuration's id
    bool          strict{false};                 ///< see Comparison::strict
    Measurements* measurements{nullptr};         ///< where the differences are recorded, or null
};

/// Compares @p run with the golden files @p golden of its simulation; @p sensitivity says what
/// of the run is reproducible, and @p context names the simulation in the report.
[[nodiscard]] SimulationComparison compareSimulation(const GoldenFiles&    golden,
                                                     const SimulationRun&  run,
                                                     const Sensitivity&    sensitivity,
                                                     const std::string&    context,
                                                     const ComparisonMode& mode)
{
    const QtRocket::Test::DefaultUnitsGuard units;  // the texts of the warnings print units
    Comparison                              c;
    c.context            = context;
    c.strict             = mode.strict;
    c.measurements       = mode.measurements;
    const json& document = golden.document;

    Mismatches header(context);
    compareHeader(header, document, run, mode.randomConfigurationId);
    c.report += header.report();

    Mismatches options(context + " options");
    compareOptions(options, document.at("options"), run.simulation->getOptions());
    c.report += options.report();

    Mismatches harness(context + " harness");
    compareHarness(harness, document.at("harness"), run.harness);
    c.report += harness.report();

    Mismatches result(context + " result");
    compareResult(result, c, document.at("result"), run, sensitivity.whole);
    c.report += result.report();

    compareData(c, golden, run, sensitivity);
    c.compared.simulations = 1;
    return {.compared = c.compared, .sensitive = c.sensitive, .report = c.report};
}

// ============================================================================== the goldens

/// A golden simulation compared with QtRocket's run of it.
struct GoldenRun
{
    std::string          problem;  ///< why there is no comparison, else ""
    std::string          context;  ///< "<input>/sim_<NN>_<name>"
    bool                 randomConfigurationId{false};
    GoldenFiles          files;
    SimulationRun        run;
    SimulationRun        twin;  ///< the perturbed run
    Sensitivity          sensitivity;
    SimulationCounts     golden;
    SimulationComparison comparison;
};

/// Runs simulation @p index of the golden input @p input, the one of @p maker, twice (see
/// LastBitListener) and compares it with its files.
[[nodiscard]] GoldenRun runGolden(const TestRocketMaker& maker, const GoldenInput& input,
                                  std::size_t index)
{
    GoldenRun               result;
    const GoldenSimulation& simulation = input.simulations.at(index);
    result.files                       = loadGoldenFiles(simulation);
    if (!result.files.problem.empty())
    {
        result.problem = result.files.problem;
        return result;
    }
    result.golden = goldenCounts(result.files);
    result.run    = runSimulation(maker, index, false);
    result.twin   = runSimulation(maker, index, true);
    if (!result.run.problem.empty() || !result.twin.problem.empty())
    {
        result.problem = result.run.problem + result.twin.problem;
        return result;
    }
    result.context               = std::format("{}/{}", input.name, baseName(result.run.planned));
    result.randomConfigurationId = maker.randomConfigurationId;
    result.sensitivity           = sensitivityOf(result.files, result.run, result.twin);
    Mismatches m(result.context);
    m.text("name in the manifest", simulation.name, result.run.planned.name);
    m.text("file in the manifest", simulation.json, result.context + ".json");
    result.comparison =
        compareSimulation(result.files, result.run, result.sensitivity, result.context,
                          {.randomConfigurationId = maker.randomConfigurationId});
    result.comparison.report = m.report() + result.comparison.report;
    return result;
}

/// The manifest, read once per test process.
[[nodiscard]] const QtRocket::Result<GoldenManifest>& manifest()
{
    static const QtRocket::Result<GoldenManifest> kManifest = QtRocket::Test::loadGoldenManifest();
    return kManifest;
}

/// The golden input of @p maker, or null (also when the manifest cannot be read).
[[nodiscard]] const GoldenInput* inputOf(const TestRocketMaker& maker)
{
    return manifest().has_value() ? manifest()->find(maker.input) : nullptr;
}

/// runGolden() of simulation @p index of @p maker, run once per test process and kept (std::map
/// keeps the entries where they are): the tests read the same runs. The result depends on the
/// maker and the index alone, so the order of the tests does not matter.
[[nodiscard]] const GoldenRun& goldenRun(const TestRocketMaker& maker, const GoldenInput& input,
                                         std::size_t index)
{
    static std::mutex                                    s_mutex;
    static std::map<std::string, GoldenRun, std::less<>> s_runs;
    const std::scoped_lock                               lock{s_mutex};
    const std::string key    = std::format("{}#{}", maker.input, index);
    auto              cached = s_runs.find(key);
    if (cached == s_runs.end())
    {
        cached = s_runs.emplace(key, runGolden(maker, input, index)).first;
    }
    return cached->second;
}

/// When the rocket of the golden simulation @p document clears the launch rod (nullopt: it never
/// does).
[[nodiscard]] std::optional<double> goldenRodClearance(const json& document)
{
    for (const json& branch : document.at("branches"))
    {
        for (const json& event : branch.at("events"))
        {
            if (event.at("type").get<std::string>() == "LAUNCHROD")
            {
                return goldenValue(event.at("time"));
            }
        }
    }
    return std::nullopt;
}

/// The number of rows of the golden time series @p table up to the time @p cleared at which the
/// launch rod is cleared: the records on the rod and the first one after it (the LAUNCHROD event
/// has the time of that record, a number of the same file). Every row for nullopt, a simulation
/// that never clears the rod.
[[nodiscard]] std::size_t rowsUpToTheClearance(const GoldenTable&           table,
                                               const std::optional<double>& cleared)
{
    const std::optional<std::size_t> time = table.columnIndex("time");
    if (!cleared.has_value() || !time.has_value())
    {
        return table.rows.size();
    }
    return static_cast<std::size_t>(std::ranges::count_if(
        table.rows,
        [&time, &cleared](const std::vector<double>& row) { return row[*time] <= *cleared; }));
}

/// The floor under the reproducible part of the golden simulation @p files: the rows of its
/// branches up to the clearing of the launch rod (every row when it never clears it).
[[nodiscard]] std::int64_t floorRows(const GoldenFiles& files)
{
    const std::optional<double> cleared = goldenRodClearance(files.document);
    std::int64_t                rows    = 0;
    for (const GoldenTable& table : files.tables)
    {
        rows += static_cast<std::int64_t>(rowsUpToTheClearance(table, cleared));
    }
    return rows;
}

/// What is wrong with the reproducible part of @p run, "" when nothing is: a simulation that
/// never clears the launch rod has to be reproducible as a whole, and a flight in every branch
/// at least until the rod is cleared. The floor is stated in rows, counted in the golden file
/// alone: the reproducible rows of a branch must be at least the golden rows up to the
/// clearance (comparing the time of the run's last reproducible row with the golden time of the
/// clearance would compare two numbers of different origin for equality).
[[nodiscard]] std::string floorProblem(const GoldenRun& run)
{
    const std::optional<double> cleared = goldenRodClearance(run.files.document);
    if (!cleared.has_value())
    {
        return run.sensitivity.whole
                   ? std::string{}
                   : std::format(
                         "{}: never clears the launch rod, but is not reproducible as a "
                         "whole\n",
                         run.context);
    }
    std::string problem;
    for (std::size_t i = 0; i < run.sensitivity.horizons.size() && i < run.files.tables.size(); i++)
    {
        const std::size_t floor = rowsUpToTheClearance(run.files.tables[i], cleared);
        if (run.sensitivity.horizons[i].rows < floor)
        {
            problem += std::format(
                "{} branch {}: {} reproducible rows only, the launch rod is "
                "cleared at row {} (t = {} s)\n",
                run.context, i, run.sensitivity.horizons[i].rows, floor, *cleared);
        }
    }
    return problem;
}

/// What the simulations of one golden input hold, and what their comparison compared and found.
struct InputResult
{
    std::string      problems;  ///< a simulation that could not be run, a floor that does not hold
    SimulationCounts golden;
    SimulationCounts compared;
    SimulationCounts sensitive;
    std::string      report;
    int              flights{0};    ///< the simulations whose rocket clears the launch rod
    int              whole{0};      ///< the simulations that are reproducible as a whole
    std::int64_t     floorRows{0};  ///< floorRows() of the simulations
};

/// Runs and compares every simulation of the golden input of @p maker.
[[nodiscard]] InputResult compareInput(const TestRocketMaker& maker)
{
    InputResult        result;
    const GoldenInput* input = inputOf(maker);
    if (input == nullptr)
    {
        result.problems = std::format("no golden input named {}", maker.input);
        return result;
    }
    const std::unique_ptr<Rocket> rocket  = maker.make();
    const std::size_t             planned = plannedSimulations(*rocket, maker.input).size();
    if (planned != input->simulations.size())
    {
        result.problems += std::format(
            "{}: the harness's document has {} simulations, the "
            "manifest lists {}\n",
            maker.input, planned, input->simulations.size());
    }
    for (std::size_t i = 0; i < input->simulations.size(); i++)
    {
        const GoldenRun& run = goldenRun(maker, *input, i);
        if (!run.problem.empty())
        {
            result.problems += std::format("{}: {}\n", input->simulations[i].json, run.problem);
            continue;
        }
        result.golden += run.golden;
        result.compared += run.comparison.compared;
        result.sensitive += run.comparison.sensitive;
        result.report += run.comparison.report;
        result.flights += goldenRodClearance(run.files.document).has_value() ? 1 : 0;
        result.whole += run.sensitivity.whole ? 1 : 0;
        result.floorRows += floorRows(run.files);
        result.problems += floorProblem(run);
    }
    return result;
}

// ===================================================================================== tests

/// One test rocket of TestRockets.h: its golden simulations.
class SimulationGolden : public ::testing::TestWithParam<TestRocketMaker>
{ };

// Every simulation of the rocket is compared as far as it is reproducible: what was compared
// and what is sensitive add up to what the files hold. And the reproducible part has a floor
// (floorProblem(), among the problems), so that a calculation that became sensitive throughout
// cannot pass by comparing nothing: a simulation that never clears the launch rod is
// reproducible as a whole, and a flight in every branch at least until it has cleared the rod.
TEST_P(SimulationGolden, EverySimulationOfTheRocket)
{
    const InputResult result = compareInput(GetParam());
    ASSERT_EQ(result.problems, "");
    EXPECT_EQ(result.report, "");
    // Everything the files hold was compared or is sensitive: nothing skipped.
    EXPECT_EQ(result.compared + result.sensitive, result.golden)
        << "what was compared or is sensitive, and what the files hold";
    EXPECT_GT(result.golden.simulations, 0);
    // The structure is compared in every simulation, sensitive or not.
    EXPECT_EQ(result.sensitive.simulations + result.sensitive.branches + result.sensitive.events +
                  result.sensitive.columns + result.sensitive.warnings,
              0);
    // The floor, in sums: the simulations that never clear the rod are whole, and at least the
    // rows up to the clearing of the rod were compared.
    EXPECT_GE(result.whole, result.golden.simulations - result.flights);
    EXPECT_GE(result.compared.rows, result.floorRows);
}

/// The test name of @p paramInfo's maker: its golden input with '-' as '_'.
[[nodiscard]] std::string makerTestName(const ::testing::TestParamInfo<TestRocketMaker>& paramInfo)
{
    std::string name{paramInfo.param.input};
    std::ranges::replace(name, '-', '_');
    return name;
}

INSTANTIATE_TEST_SUITE_P(Makers, SimulationGolden, ::testing::ValuesIn(testRocketMakers()),
                         makerTestName);

// ================================================================================== coverage

/// What the golden simulations of the test rockets hold, read from the files alone (no
/// simulation is run), and whether each input has a maker in TestRockets.h, whose test
/// (SimulationGolden) compares its simulations.
struct GoldenCoverage
{
    int              goldenInputs{0};  ///< the "testrocket" inputs of the manifest
    SimulationCounts golden;           ///< what their simulation files hold
    int              flights{0};       ///< the simulations whose rocket clears the launch rod
    std::int64_t     floorRows{0};     ///< floorRows() of the simulations
    std::string      problems;         ///< an input without a maker, a file that cannot be read
};

/// Adds the simulations of the golden input @p input to @p coverage.
void cover(GoldenCoverage& coverage, const GoldenInput& input)
{
    for (const GoldenSimulation& simulation : input.simulations)
    {
        const GoldenFiles files = loadGoldenFiles(simulation);
        if (!files.problem.empty())
        {
            coverage.problems += std::format("{}: {}\n", simulation.json, files.problem);
            continue;
        }
        coverage.golden += goldenCounts(files);
        coverage.flights += goldenRodClearance(files.document).has_value() ? 1 : 0;
        coverage.floorRows += floorRows(files);
    }
}

/// Reads every simulation of every "testrocket" input of the manifest.
[[nodiscard]] GoldenCoverage goldenCoverage()
{
    GoldenCoverage coverage;
    if (!manifest())
    {
        coverage.problems = manifest().error().message;
        return coverage;
    }
    const std::span<const TestRocketMaker> makers = testRocketMakers();
    for (const GoldenInput& input : manifest()->inputs)
    {
        if (input.kind != "testrocket")
        {
            continue;
        }
        coverage.goldenInputs++;
        if (std::ranges::find(makers, std::string_view{input.name}, &TestRocketMaker::input) ==
            makers.end())
        {
            coverage.problems += std::format("{}: no maker\n", input.name);
        }
        cover(coverage, input);
    }
    return coverage;
}

/// Every golden simulation of the test rockets is compared: each of the thirteen inputs has a
/// maker, and the test of each maker (SimulationGolden) checks that what it compared and what
/// is sensitive add up to what the files of its input hold, and that the floor holds. This
/// test pins what the files hold in all, and the floor in all; it runs no simulation, so that
/// the simulations are not run a second time for the sums (ctest starts a process per test).
TEST(SimulationGoldenCoverage, EveryGoldenSimulationOfTheTestRocketsHasAMaker)
{
    const GoldenCoverage coverage = goldenCoverage();
    EXPECT_EQ(coverage.problems, "");
    EXPECT_EQ(coverage.goldenInputs, 13);
    EXPECT_EQ(static_cast<std::size_t>(coverage.goldenInputs), testRocketMakers().size());
    EXPECT_EQ(coverage.golden.simulations, 50) << "one per configuration, and three variants";
    EXPECT_EQ(coverage.golden.branches, 53);
    EXPECT_EQ(coverage.golden.events, 437);
    EXPECT_EQ(coverage.golden.columns, 2826);
    EXPECT_EQ(coverage.golden.warnings, 21);
    EXPECT_EQ(coverage.golden.numbers, 6920);
    EXPECT_EQ(coverage.golden.rows, 22330);
    EXPECT_EQ(coverage.golden.values, 1562004);
    // The floor (floorProblem()): 31 flights, which are reproducible until they have cleared
    // the rod, 794 rows in all, and 19 simulations that never clear it and are reproducible as
    // a whole, 26 rows.
    EXPECT_EQ(coverage.flights, 31);
    EXPECT_EQ(coverage.floorRows, 820);
}

// ==================================================================================== strict

/// The comparison the plan asked for: every value of every simulation at the tolerances, the
/// rows exactly, the events in the order they were recorded in. It cannot pass (see "WHAT CAN BE
/// COMPARED" at the top of the file): OpenRocket does not reproduce these values itself.
/// Disabled; run it with --gtest_also_run_disabled_tests to see what differs on a platform.
TEST(SimulationGoldenStrict, DISABLED_EverySimulationIsReproducedInFull)
{
    std::string report;
    for (const TestRocketMaker& maker : testRocketMakers())
    {
        const GoldenInput* input = inputOf(maker);
        ASSERT_NE(input, nullptr) << maker;
        for (std::size_t i = 0; i < input->simulations.size(); i++)
        {
            const GoldenRun& run = goldenRun(maker, *input, i);
            ASSERT_EQ(run.problem, "");
            report += compareSimulation(
                          run.files, run.run, run.sensitivity, run.context,
                          {.randomConfigurationId = run.randomConfigurationId, .strict = true})
                          .report;
        }
    }
    EXPECT_EQ(report, "");
}

// =============================================================================== measurement

/// "branch 0: 37 of 796 rows (until t = 0.2507 s); the run has 848 rows, the perturbed run 794"
/// for every branch of @p run that is not reproducible as a whole.
[[nodiscard]] std::string sensitivityText(const GoldenRun& run)
{
    std::string text;
    for (std::size_t i = 0; i < run.sensitivity.horizons.size(); i++)
    {
        const Horizon& horizon = run.sensitivity.horizons[i];
        if (horizon.whole || i >= run.files.tables.size() || i >= run.twin.data->getBranchCount())
        {
            continue;
        }
        text += std::format(
            " branch {}: {} of {} rows (until t = {:.4f} s); the run has {} rows, "
            "the perturbed run {};",
            i, horizon.rows, run.files.tables[i].rows.size(), horizon.time,
            run.run.data->getBranch(i).getLength(), run.twin.data->getBranch(i).getLength());
    }
    return text;
}

/// "848+283" for a flight of two branches with 848 and 283 rows.
[[nodiscard]] std::string rowCounts(const std::vector<std::size_t>& rows)
{
    std::string text;
    for (const std::size_t count : rows)
    {
        text += std::format("{}{}", text.empty() ? "" : "+", count);
    }
    return text;
}

/// One line of the overview of the measurement: the apogee (the maximum altitude) and the flight
/// time of @p run and of its golden file, the rows of their branches, and the largest difference
/// of a value of the time series (over the rows both have), as a fraction of its column's scale.
[[nodiscard]] std::string overviewLine(const GoldenRun& run, const Measurements& measurements)
{
    if (run.run.data == nullptr)
    {
        return std::format("{}\tno flight data\n", run.context);
    }
    const json&              summary = run.files.document.at("summary");
    std::vector<std::size_t> rows;
    std::vector<std::size_t> goldenRows;
    rows.reserve(run.run.data->getBranchCount());
    goldenRows.reserve(run.files.tables.size());
    for (std::size_t i = 0; i < run.run.data->getBranchCount(); i++)
    {
        rows.push_back(run.run.data->getBranch(i).getLength());
    }
    for (const GoldenTable& table : run.files.tables)
    {
        goldenRows.push_back(table.rows.size());
    }
    return std::format("{}\t{}\t{}\t{}\t{}\t{}\t{}\t{:.1e}\n", run.context,
                       run.run.data->getMaxAltitude(), goldenValue(summary.at("maxAltitude")),
                       run.run.data->getFlightTime(), goldenValue(summary.at("flightTime")),
                       rowCounts(rows), rowCounts(goldenRows),
                       measurements.largestOfTheTimeSeries(run.context) * kValueRelative);
}

/// The measurement behind the tolerances: prints, per simulation, how far it is reproducible,
/// and a table of the largest differences from the golden files, one line per simulation and
/// thing compared (a summary value, the times of the events, a column, ...), separately for what
/// is compared and for what is sensitive: the number of values, the largest difference, and that
/// difference as a multiple of its tolerance; then an overview, one line per simulation: the
/// apogee and the flight time here and in the golden file, the rows, and the largest difference
/// of the time series. Disabled; run it with --gtest_also_run_disabled_tests (under a libm of
/// another platform, or the one-ulp shim, to see what the tolerances have to cover there).
TEST(SimulationGoldenMeasurement, DISABLED_PrintsTheDifferencesFromTheGoldenFiles)
{
    Measurements measurements;
    std::string  sensitivity;
    std::string  overview;
    for (const TestRocketMaker& maker : testRocketMakers())
    {
        const GoldenInput* input = inputOf(maker);
        ASSERT_NE(input, nullptr) << maker;
        for (std::size_t i = 0; i < input->simulations.size(); i++)
        {
            const GoldenRun& run = goldenRun(maker, *input, i);
            ASSERT_EQ(run.problem, "");
            (void)compareSimulation(run.files, run.run, run.sensitivity, run.context,
                                    {.randomConfigurationId = run.randomConfigurationId,
                                     .measurements          = &measurements});
            sensitivity += run.sensitivity.whole
                               ? std::string{}
                               : std::format("{}:{}\n", run.context, sensitivityText(run));
            overview += overviewLine(run, measurements);
        }
    }
    std::cout << "Not reproducible as a whole:\n"
              << sensitivity
              << "simulation\twhat\tcompared or sensitive\tvalues\tlargest difference\t"
                 "... as a multiple of its tolerance\n"
              << measurements.text()
              << "simulation\tapogee (m)\t... in the golden file\tflight time (s)\t"
                 "... in the golden file\trows per branch\t... in the golden file\t"
                 "largest difference of the time series, of the column's scale\n"
              << overview
              << std::format(
                     "The largest difference of a compared value is {:.3e} of its "
                     "tolerance.\n",
                     measurements.largestComparedOfTolerance());
    EXPECT_LE(measurements.largestComparedOfTolerance(), 1.0);
}

// ================================================================================= mutations

// The comparison is not vacuous: a golden value changed in a copy of the files of a simulation
// is reported, in one line that names it. The simulations: the [A8-0; None] one of the Beta,
// which ends on the launch pad after one step and is reproducible as a whole on every platform;
// the [C6-5] flight of the Estes Alpha III, of which the launch rod is reproducible on every
// platform; and the simulation with motors of the cluster pods, which has a warning.

/// Four times the tolerance of a value.
constexpr double kBeyondTolerance = 4 * kValueRelative;
/// A quarter of it.
constexpr double kWithinTolerance = kValueRelative / 4;
/// Four times the tolerance of a time, in s.
constexpr double kBeyondTimeTolerance = 4 * kTimeAbsolute;
/// A quarter of it.
constexpr double kWithinTimeTolerance = kTimeAbsolute / 4;

/// The simulations the mutations change: their golden input and their index in it.
struct Subject
{
    std::string_view input;
    std::size_t      index;
};
constexpr Subject kOnThePad{.input = "testrocket-beta", .index = 1};
constexpr Subject kFlight{.input = "testrocket-estes-alpha-iii", .index = 4};
constexpr Subject kWithWarning{.input = "testrocket-cluster-pods", .index = 1};

/// The value at the JSON pointer @p pointer of @p document.
[[nodiscard]] json& valueAt(json& document, std::string_view pointer)
{
    return document.at(json::json_pointer{std::string{pointer}});
}

/// Multiplies the number at @p pointer by 1 + @p relative.
void scale(GoldenFiles& files, std::string_view pointer, double relative)
{
    json& value = valueAt(files.document, pointer);
    value       = value.get<double>() * (1 + relative);
}

/// Adds @p offset to the number at @p pointer.
void shift(GoldenFiles& files, std::string_view pointer, double offset)
{
    json& value = valueAt(files.document, pointer);
    value       = value.get<double>() + offset;
}

/// Adds @p relative times the scale of the column to the value of column @p key in row @p row
/// of the time series of the first branch.
void shiftSeries(GoldenFiles& files, std::string_view key, std::size_t row, double relative)
{
    GoldenTable&                     table  = files.tables.at(0);
    const std::optional<std::size_t> column = table.columnIndex(key);
    if (column.has_value())
    {
        table.rows.at(row).at(*column) += relative * columnScale(table, *column);
    }
}

/// A change to the golden files of a simulation that the comparison has to report, in one line.
struct Mutation
{
    std::string_view name;              ///< what is changed; it names the change in a failure
    Subject          subject;           ///< the simulation
    void (*apply)(GoldenFiles& files);  ///< makes the change
    std::string_view heading;           ///< what follows the context in the report's heading
    std::string_view line;              ///< what its one line starts with
};

/// The changes to what identifies a simulation, to its options and to how it ended.
[[nodiscard]] std::vector<Mutation> settingMutations()
{
    return {
        {.name    = "SimulationName",
         .subject = kFlight,
         .apply   = [](GoldenFiles& files) { files.document["name"] = "[C6-6]"; },
         .heading = "",
         .line    = R"(  name: expected "[C6-6]", got "[C6-5]")"},
        {.name    = "FlightConfigurationName",
         .subject = kFlight,
         .apply = [](GoldenFiles& files) { files.document["flightConfiguration"]["name"] = "[X]"; },
         .heading = "",
         .line    = R"(  flightConfiguration.name: expected "[X]")"},
        {.name    = "Variant",
         .subject = kFlight,
         .apply   = [](GoldenFiles& files) { files.document["variant"] = "something"; },
         .heading = "",
         .line    = R"(  variant: expected "something", got "null")"},
        {.name    = "TimeStep",
         .subject = kFlight,
         .apply   = [](GoldenFiles& files) { scale(files, "/options/timeStep", 1e-15); },
         .heading = " options",
         .line    = "  timeStep: expected "},
        {.name    = "WindDirection",
         .subject = kFlight,
         .apply = [](GoldenFiles& files) { scale(files, "/options/averageWind/direction", 1e-15); },
         .heading = " options",
         .line    = "  averageWind.direction: expected "},
        {.name    = "StepperMethod",
         .subject = kFlight,
         .apply   = [](GoldenFiles& files) { files.document["options"]["stepperMethod"] = "RK6"; },
         .heading = " options",
         .line    = R"(  stepperMethod: expected "RK6", got "RK4")"},
        {.name    = "HarnessSeed",
         .subject = kFlight,
         .apply   = [](GoldenFiles& files) { files.document["harness"]["randomSeed"] = 1; },
         .heading = " harness",
         .line    = R"(  randomSeed: expected "1", got "0")"},
        {.name    = "Status",
         .subject = kFlight,
         .apply   = [](GoldenFiles& files) { files.document["result"]["status"] = "exception"; },
         .heading = " result",
         .line    = R"(  status: expected "exception", got "completed")"},
        {.name    = "ExceptionMessage",
         .subject = kFlight,
         .apply = [](GoldenFiles& files) { files.document["result"]["exceptionMessage"] = "bad"; },
         .heading = " result",
         .line    = R"(  exceptionMessage: expected "bad", got "null")"},
        {.name    = "JitterReplacements",
         .subject = kOnThePad,
         .apply   = [](GoldenFiles& files) { files.document["result"]["jitterReplacements"] = 5; },
         .heading = " result",
         .line    = "  jitterReplacements: expected 5, got 4"},
        {.name    = "UnknownField",
         .subject = kFlight,
         .apply   = [](GoldenFiles& files) { files.document["result"]["steps"] = 1; },
         .heading = " result",
         .line    = "  steps: not compared"},
    };
}

/// The changes to the summary, the warnings and the header and columns of a branch.
[[nodiscard]] std::vector<Mutation> branchMutations()
{
    return {
        {.name    = "MaximumMachNumber",
         .subject = kOnThePad,
         .apply =
             [](GoldenFiles& files) { scale(files, "/summary/maxMachNumber", kBeyondTolerance); },
         .heading = " summary",
         .line    = "  maxMachNumber: expected "},
        {.name    = "FlightTime",
         .subject = kOnThePad,
         .apply =
             [](GoldenFiles& files) { shift(files, "/summary/flightTime", kBeyondTimeTolerance); },
         .heading = " summary",
         .line    = "  flightTime: expected "},
        {.name    = "LaunchRodVelocity",
         .subject = kFlight,
         .apply =
             [](GoldenFiles& files) {
                 scale(files, "/summary/launchRodVelocity", kBeyondTolerance);
             },
         .heading = " summary",
         .line    = "  launchRodVelocity: expected "},
        {.name    = "BranchCount",
         .subject = kFlight,
         .apply   = [](GoldenFiles& files) { files.document["summary"]["branchCount"] = 2; },
         .heading = " summary",
         .line    = "  branchCount: expected 2, got 1"},
        {.name    = "WarningText",
         .subject = kWithWarning,
         .apply = [](GoldenFiles& files) { files.document["warnings"][0]["text"] = "No device."; },
         .heading = "",
         .line    = R"(  warnings[0].text: expected "No device.")"},
        {.name    = "RowCount",
         .subject = kOnThePad,
         .apply   = [](GoldenFiles& files) { files.document["branches"][0]["rows"] = 3; },
         .heading = " branch 0",
         .line    = "  rows: expected 3, got 2"},
        {.name    = "BranchName",
         .subject = kFlight,
         .apply   = [](GoldenFiles& files) { files.document["branches"][0]["name"] = "Booster"; },
         .heading = " branch 0",
         .line    = R"(  name: expected "Booster", got "Stage")"},
        {.name    = "SourceComponent",
         .subject = kFlight,
         .apply =
             [](GoldenFiles& files) { files.document["branches"][0]["sourceComponent"] = "/1"; },
         .heading = " branch 0",
         .line    = R"(  sourceComponent: expected "/1", got "/0")"},
        {.name    = "ExcludedColumns",
         .subject = kFlight,
         .apply =
             [](GoldenFiles& files) {
                 files.document["branches"][0]["excludedColumns"] = json::array();
             },
         .heading = " branch 0",
         .line    = R"(  excludedColumns: expected "", got "computation_time")"},
        {.name    = "ColumnSymbol",
         .subject = kFlight,
         .apply =
             [](GoldenFiles& files) {
                 files.document["branches"][0]["columns"][1]["symbol"] = "H";
             },
         .heading = " branch 0",
         .line    = R"(  columns[1].symbol: expected "H", got "h")"},
        {.name    = "ColumnMaximum",
         .subject = kOnThePad,
         .apply =
             [](GoldenFiles& files) {
                 scale(files, "/branches/0/columns/0/max", kBeyondTolerance);
             },
         .heading = " branch 0",
         .line    = "  columns[0].max (time): expected "},
    };
}

/// The changes to the events and to the time series of a branch.
[[nodiscard]] std::vector<Mutation> trajectoryMutations()
{
    return {
        {.name    = "ValueOfTheTimeSeries",
         .subject = kFlight,
         .apply   = [](GoldenFiles& files) { shiftSeries(files, "altitude", 5, kBeyondTolerance); },
         .heading = " branch 0",
         .line    = "  column altitude: 1 of "},
        {.name    = "TimeOfARecord",
         .subject = kFlight,
         .apply   = [](GoldenFiles& files) { shiftSeries(files, "time", 3, kBeyondTolerance); },
         .heading = " branch 0",
         .line    = "  column time: 1 of "},
        {.name    = "TimeOfTheLiftoff",
         .subject = kFlight,
         .apply =
             [](GoldenFiles& files) {
                 shift(files, "/branches/0/events/2/time", kBeyondTimeTolerance);
             },
         .heading = " branch 0",
         .line    = "  events[2].time: expected "},
        {.name    = "TimeOfTheBurnout",
         .subject = kFlight,
         .apply =
             [](GoldenFiles& files) {
                 shift(files, "/branches/0/events/4/time", kBeyondTimeTolerance);
             },
         .heading = " branch 0",
         .line    = "  events[4].time: expected "},
        {.name    = "RemovedEvent",
         .subject = kFlight,
         .apply   = [](GoldenFiles& files) { files.document["branches"][0]["events"].erase(3); },
         .heading = " branch 0",
         .line    = R"(  events: expected "LAUNCH(/) IGNITION(/0/1/2) LIFTOFF(null) )"
                    "BURNOUT(/0/1/2) "},
        {.name    = "SwappedEvents",
         .subject = kFlight,
         .apply =
             [](GoldenFiles& files) {
                 json& events = files.document["branches"][0]["events"];
                 std::swap(events[5], events[6]);
             },
         .heading = " branch 0",
         .line    = R"(  events: expected "LAUNCH(/) IGNITION(/0/1/2) LIFTOFF(null) )"
                    "LAUNCHROD(null) BURNOUT(/0/1/2) EJECTION_CHARGE(/0) APOGEE(/) "},
        {.name    = "SourceOfAnEvent",
         .subject = kFlight,
         .apply =
             [](GoldenFiles& files) { files.document["branches"][0]["events"][6]["source"] = "/"; },
         .heading = " branch 0",
         .line    = R"(  events: expected ")"},
        {.name    = "MotorOfAnEvent",
         .subject = kFlight,
         .apply =
             [](GoldenFiles& files) {
                 files.document["branches"][0]["events"][1]["data"]["designation"] = "C7";
             },
         .heading = " branch 0",
         .line    = R"(  events[1].data.designation: expected "C7", got "C6")"},
        {.name    = "CauseOfAnAbort",
         .subject = kOnThePad,
         .apply =
             [](GoldenFiles& files) {
                 files.document["branches"][0]["events"][1]["data"]["cause"] = "NO_CP";
             },
         .heading = " branch 0",
         .line    = R"(  events[1].data.cause: expected "NO_CP", got "NO_MOTORS_FIRED")"},
        {.name    = "WarningOfAnEvent",
         .subject = kWithWarning,
         .apply =
             [](GoldenFiles& files) {
                 files.document["branches"][0]["events"][0]["data"]["priority"] = "LOW";
             },
         .heading = " branch 0",
         .line    = R"(  events[0].data.priority: expected "LOW", got "HIGH")"},
        {.name    = "KindOfTheDataOfAnEvent",
         .subject = kFlight,
         .apply =
             [](GoldenFiles& files) {
                 files.document["branches"][0]["events"][2]["data"] = "text";
             },
         .heading = " branch 0",
         .line    = R"(  events[2].data: kind: expected "text", got "none")"},
    };
}

/// Every change.
[[nodiscard]] std::vector<Mutation> mutations()
{
    std::vector<Mutation> all = settingMutations();
    for (const Mutation& mutation : branchMutations())
    {
        all.push_back(mutation);
    }
    for (const Mutation& mutation : trajectoryMutations())
    {
        all.push_back(mutation);
    }
    return all;
}

/// The run of the simulation @p subject, or null (with a test failure) when there is none.
[[nodiscard]] const GoldenRun* subjectRun(const Subject& subject)
{
    const std::span<const TestRocketMaker> makers = testRocketMakers();
    const auto maker = std::ranges::find(makers, subject.input, &TestRocketMaker::input);
    if (maker == makers.end() || inputOf(*maker) == nullptr)
    {
        ADD_FAILURE() << "no golden input or maker named " << subject.input;
        return nullptr;
    }
    const GoldenRun& run = goldenRun(*maker, *inputOf(*maker), subject.index);
    if (!run.problem.empty())
    {
        ADD_FAILURE() << run.problem;
        return nullptr;
    }
    return &run;
}

/// What the comparison of the simulation @p subject with a copy of its golden files finds after
/// @p change changed the copy (null: nothing changed).
[[nodiscard]] SimulationComparison comparisonAfter(const Subject& subject,
                                                   void (*change)(GoldenFiles& files))
{
    const GoldenRun* run = subjectRun(subject);
    if (run == nullptr)
    {
        SimulationComparison none;
        none.report = "no run";
        return none;
    }
    GoldenFiles files = run->files;
    if (change != nullptr)
    {
        change(files);
    }
    return compareSimulation(files, run->run, run->sensitivity, run->context,
                             {.randomConfigurationId = run->randomConfigurationId});
}

/// Changes within the tolerances to the [C6-5] flight: a value and a time of the time series,
/// the times of two events, a summary value.
void changeTheFlightWithinTheTolerances(GoldenFiles& files)
{
    shiftSeries(files, "altitude", 5, kWithinTolerance);
    shiftSeries(files, "time", 3, kWithinTolerance);
    shift(files, "/branches/0/events/2/time", kWithinTimeTolerance);
    shift(files, "/branches/0/events/4/time", kWithinTimeTolerance);
    scale(files, "/summary/launchRodVelocity", kWithinTolerance);
}

/// Changes within the tolerances to the run that ends on the pad: two summary values and the
/// maximum of a column.
void changeTheRunOnThePadWithinTheTolerances(GoldenFiles& files)
{
    scale(files, "/summary/maxMachNumber", kWithinTolerance);
    shift(files, "/summary/flightTime", kWithinTimeTolerance);
    scale(files, "/branches/0/columns/0/max", kWithinTolerance);
}

/// A simulation the mutations change, with the changes within the tolerances that go with it
/// (null: none). The tests are per simulation, not per mutation: ctest starts a process per
/// test, and a process runs the simulation (twice) before it can compare anything.
struct SubjectCase
{
    std::string_view name;  ///< it names the tests
    Subject          subject;
    void (*withinTheTolerances)(GoldenFiles& files);
};

constexpr std::array<SubjectCase, 3> kSubjectCases{{
    {.name                = "TheRunOnThePad",
     .subject             = kOnThePad,
     .withinTheTolerances = changeTheRunOnThePadWithinTheTolerances},
    {.name                = "TheFlight",
     .subject             = kFlight,
     .withinTheTolerances = changeTheFlightWithinTheTolerances},
    {.name = "TheRunWithAWarning", .subject = kWithWarning, .withinTheTolerances = nullptr},
}};

/// The name of @p subjectCase, for the messages of the tests.
std::ostream& operator<<(std::ostream& out, const SubjectCase& subjectCase)
{
    return out << subjectCase.name;
}

/// The mutations of the simulation @p subject.
[[nodiscard]] std::vector<Mutation> mutationsOf(const Subject& subject)
{
    std::vector<Mutation> own;
    for (const Mutation& mutation : mutations())
    {
        if (mutation.subject.input == subject.input && mutation.subject.index == subject.index)
        {
            own.push_back(mutation);
        }
    }
    return own;
}

/// What is wrong with the report of @p mutation, applied to a copy of the files of @p run: ""
/// when the report is the heading and the one line that name the change.
[[nodiscard]] std::string mutationProblem(const Mutation& mutation, const GoldenRun& run)
{
    const std::string report = comparisonAfter(mutation.subject, mutation.apply).report;
    const std::string start =
        std::format("{}{}:\n{}", run.context, mutation.heading, mutation.line);
    if (!report.starts_with(start))
    {
        return std::format("{}: the report\n{}\ndoes not start with\n{}\n", mutation.name, report,
                           start);
    }
    if (std::ranges::count(report, '\n') != 2)
    {
        return std::format("{}: the report is not one line:\n{}\n", mutation.name, report);
    }
    return {};
}

/// What is wrong with the reports of the mutations of the simulation @p subject, each named;
/// "" when every one is reported in its one line.
[[nodiscard]] std::string mutationProblems(const Subject& subject)
{
    const GoldenRun* run = subjectRun(subject);
    if (run == nullptr)
    {
        return "no run";
    }
    std::string problems;
    for (const Mutation& mutation : mutationsOf(subject))
    {
        problems += mutationProblem(mutation, *run);
    }
    return problems;
}

/// The report of the changes within the tolerances of @p subjectCase; "" when it has none.
[[nodiscard]] std::string reportWithinTheTolerances(const SubjectCase& subjectCase)
{
    return subjectCase.withinTheTolerances == nullptr
               ? std::string{}
               : comparisonAfter(subjectCase.subject, subjectCase.withinTheTolerances).report;
}

/// The golden values of one simulation, changed one at a time in a copy of its files.
class SimulationGoldenMutation : public ::testing::TestWithParam<SubjectCase>
{ };

TEST_P(SimulationGoldenMutation, EveryChangeIsReportedInOneLine)
{
    EXPECT_FALSE(mutationsOf(GetParam().subject).empty());
    EXPECT_EQ(mutationProblems(GetParam().subject), "");
}

TEST_P(SimulationGoldenMutation, TheFilesAsTheyAreAndChangesWithinTheTolerancesMatch)
{
    EXPECT_EQ(comparisonAfter(GetParam().subject, nullptr).report, "");
    EXPECT_EQ(reportWithinTheTolerances(GetParam()), "");
}

/// The test name of @p paramInfo's simulation.
[[nodiscard]] std::string subjectTestName(const ::testing::TestParamInfo<SubjectCase>& paramInfo)
{
    return std::string{paramInfo.param.name};
}

INSTANTIATE_TEST_SUITE_P(Changes, SimulationGoldenMutation, ::testing::ValuesIn(kSubjectCases),
                         subjectTestName);

/// The names of @p all, sorted.
[[nodiscard]] std::vector<std::string_view> sortedNames(const std::vector<Mutation>& all)
{
    std::vector<std::string_view> names;
    names.reserve(all.size());
    for (const Mutation& mutation : all)
    {
        names.push_back(mutation.name);
    }
    std::ranges::sort(names);
    return names;
}

// The tests above run every mutation: each belongs to one of the three simulations, and each
// has a name of its own for the report. No simulation is run here.
TEST(SimulationGoldenMutations, EveryMutationBelongsToASimulationOfTheTests)
{
    const std::vector<Mutation> all = mutations();
    EXPECT_EQ(all.size(), 33U);
    std::size_t covered = 0;
    for (const SubjectCase& subjectCase : kSubjectCases)
    {
        covered += mutationsOf(subjectCase.subject).size();
    }
    EXPECT_EQ(covered, all.size());
    const std::vector<std::string_view> names = sortedNames(all);
    EXPECT_EQ(std::ranges::adjacent_find(names), names.end()) << "two mutations of one name";
}

/// Changes to what is not reproducible in the [C6-5] flight: the last record, the time of the
/// apogee, the maximum altitude, the number of rows and the number of jitter replacements.
void changeTheFlightBeyondItsHorizon(GoldenFiles& files)
{
    files.tables.at(0).rows.back().at(1) += 1.0;
    shift(files, "/branches/0/events/5/time", 1.0);
    scale(files, "/summary/maxAltitude", 0.5);
    files.document["branches"][0]["rows"]          = 1;
    files.document["result"]["jitterReplacements"] = 1;
}

/// The other side of comparing what is reproducible: a value beyond the horizon of a flight is
/// not compared, whatever it is. It is counted as sensitive, with the same sums.
TEST(SimulationGoldenMutations, AValueBeyondTheHorizonIsCountedAsSensitiveNotCompared)
{
    const SimulationComparison unchanged = comparisonAfter(kFlight, nullptr);
    const SimulationComparison changed = comparisonAfter(kFlight, changeTheFlightBeyondItsHorizon);
    EXPECT_EQ(changed.report, "");
    EXPECT_EQ(changed.compared, unchanged.compared);
    EXPECT_EQ(changed.sensitive, unchanged.sensitive);
    EXPECT_GT(unchanged.sensitive.rows, 0);
    EXPECT_GT(unchanged.sensitive.numbers, 0);
    EXPECT_GT(unchanged.compared.rows, 0);
}

// ============================================================================ the comparison

// What no golden simulation of the test rockets exercises on every platform.

/// A golden warning as the dumper writes it (Values.warning()), for a warning without sources.
[[nodiscard]] json goldenFormOf(const Warning& warning)
{
    json form           = json::object();
    form["type"]        = std::string{warning.typeName()};
    form["priority"]    = std::string{QtRocket::exportLabel(warning.priority())};
    form["description"] = warning.messageDescription();
    form["text"]        = warning.toString();
    form["sources"]     = json::array();
    if (const std::optional<double> parameter = parameterOf(warning))
    {
        form["parameter"] = *parameter;
    }
    return form;
}

/// What compareSimulationWarning() reports and counts for @p actual against @p expected in a run
/// that is reproducible as a whole (@p whole) or not.
[[nodiscard]] Comparison warningComparison(const json& expected, const Warning& actual, bool whole)
{
    const Rocket rocket;
    Comparison   c;
    c.context = "warning";
    Mismatches m("warning");
    compareSimulationWarning(m, c, "w", expected, actual, whole, rocket);
    c.report = m.report();
    return c;
}

TEST(SimulationGoldenWarnings, AWarningWithAParameterIsComparedInFullInAReproducibleRun)
{
    const QtRocket::Test::DefaultUnitsGuard    units;  // the text prints the speed
    const Warning::RecoveryHighSpeedDeployment warning{80.5};
    json                                       expected = goldenFormOf(warning);
    ASSERT_TRUE(expected.contains("parameter"));
    EXPECT_EQ(expected.at("type"), "RecoveryHighSpeedDeployment");

    const Comparison same = warningComparison(expected, warning, true);
    EXPECT_EQ(same.report, "");
    EXPECT_EQ(same.compared.numbers, 1);
    EXPECT_EQ(same.sensitive.numbers, 0);

    expected["parameter"] = 80.5 * (1 + kWithinTolerance);
    EXPECT_EQ(warningComparison(expected, warning, true).report, "");

    expected["parameter"]    = 80.5 * (1 + kBeyondTolerance);
    const std::string report = warningComparison(expected, warning, true).report;
    EXPECT_TRUE(report.starts_with("warning:\n  w.parameter: expected 80.5000003")) << report;
    EXPECT_EQ(std::ranges::count(report, '\n'), 2) << report;
}

TEST(SimulationGoldenWarnings, AWarningWithAParameterIsComparedByItsIdentityInASensitiveRun)
{
    const QtRocket::Test::DefaultUnitsGuard units;
    const Warning::LargeAOA                 warning{0.4};
    json                                    expected = goldenFormOf(warning);
    expected["parameter"]                            = 0.5;
    expected["text"]                                 = "Another angle";
    expected["description"]                          = "Another angle";

    // The angle and the texts that print it are not compared, and counted as sensitive.
    const Comparison sensitive = warningComparison(expected, warning, false);
    EXPECT_EQ(sensitive.report, "");
    EXPECT_EQ(sensitive.compared.numbers, 0);
    EXPECT_EQ(sensitive.sensitive.numbers, 1);

    // The class, the priority and the sources are.
    expected["priority"]     = "HIGH";
    const std::string report = warningComparison(expected, warning, false).report;
    EXPECT_TRUE(report.starts_with("warning:\n  w.priority: expected \"HIGH\", got \"LOW\""))
        << report;
    EXPECT_EQ(std::ranges::count(report, '\n'), 2) << report;
}

TEST(SimulationGoldenWarnings, AWarningWithoutAParameterIsAlwaysComparedInFull)
{
    const Warning& warning  = Warning::kSupersonic;
    json           expected = goldenFormOf(warning);
    EXPECT_FALSE(expected.contains("parameter"));
    const Comparison same = warningComparison(expected, warning, false);
    EXPECT_EQ(same.report, "");
    EXPECT_EQ(same.compared.numbers + same.sensitive.numbers, 0) << "it has no number";

    expected["text"]         = "Something else";
    const std::string report = warningComparison(expected, warning, false).report;
    EXPECT_TRUE(report.starts_with("warning:\n  w.text: expected \"Something else\"")) << report;
    EXPECT_EQ(std::ranges::count(report, '\n'), 2) << report;
}

/// Whether GoldenMismatches::within() takes @p actual for the golden @p expected at the
/// tolerances @p relative and @p absolute.
[[nodiscard]] bool isWithin(double expected, double actual, double relative, double absolute)
{
    Mismatches m("number");
    m.within("x", expected, actual, relative, absolute);
    return m.report().empty();
}

TEST(SimulationGoldenNumbers, ANumberMatchesWithinTheRelativeOrTheAbsoluteTolerance)
{
    const double nan      = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    EXPECT_TRUE(isWithin(100.0, 100.0 + 5e-8, 1e-9, 0.0)) << "relative to the larger magnitude";
    EXPECT_FALSE(isWithin(100.0, 100.0 + 2e-7, 1e-9, 0.0));
    EXPECT_TRUE(isWithin(100.0, 100.0 + 2e-7, 0.0, 3e-7)) << "or within the absolute tolerance";
    EXPECT_FALSE(isWithin(100.0, 100.0 + 2e-7, 0.0, 1e-7));
    EXPECT_TRUE(isWithin(0.0, 0.0, 0.0, 0.0));
    EXPECT_FALSE(isWithin(0.0, 1e-300, 0.0, 0.0)) << "without a tolerance only the same number";
    // NaN matches NaN only, and an infinity the same infinity only, whatever the tolerances.
    EXPECT_TRUE(isWithin(nan, nan, 0.0, 0.0));
    EXPECT_FALSE(isWithin(nan, 1.0, 1.0, 1.0));
    EXPECT_FALSE(isWithin(1.0, nan, 1.0, 1.0));
    EXPECT_TRUE(isWithin(infinity, infinity, 0.0, 0.0));
    EXPECT_FALSE(isWithin(infinity, -infinity, 1.0, 1.0));
    EXPECT_FALSE(isWithin(infinity, 1.0, 1.0, 1.0));
    EXPECT_FALSE(isWithin(1.0, infinity, 1.0, infinity));
}

TEST(SimulationGoldenNumbers, AValueIsReproducibleWhenThePerturbedRunMovesItByLittle)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    // Within 1/kSensitivityMargin of the tolerance.
    EXPECT_TRUE(reproducible(1.0, 1.0, 0.0));
    EXPECT_TRUE(reproducible(1.0, 1.0 + 5e-13, 1e-9));
    EXPECT_FALSE(reproducible(1.0, 1.0 + 2e-12, 1e-9));
    EXPECT_FALSE(reproducible(0.0, 1e-300, 0.0)) << "a column of zeros has no tolerance";
    EXPECT_TRUE(reproducible(nan, nan, 0.0));
    EXPECT_FALSE(reproducible(nan, 1.0, 1.0));
    EXPECT_FALSE(reproducible(1.0, nan, 1.0));
}

/// The order comparisonOrder() gives the events @p keys, as their sources one after the other.
[[nodiscard]] std::string orderedSources(const std::vector<EventKey>& keys, bool recorded)
{
    std::string sources;
    for (const std::size_t i : comparisonOrder(keys, recorded))
    {
        sources += sources.empty() ? "" : " ";
        sources += keys[i].source;
    }
    return sources;
}

TEST(SimulationGoldenEvents, SimultaneousIgnitionsAreComparedAsASet)
{
    // LAUNCH, two ignitions at 0 s, one at 0.01 s, LIFTOFF, two burnouts at the same time, two
    // ignitions at 2 s: only the consecutive ignitions with the same time are sorted.
    const std::vector<EventKey> keys{
        {.ignition = false, .time = 0.0, .source = "/"},
        {.ignition = true, .time = 0.0, .source = "/0/1/5"},
        {.ignition = true, .time = 0.0, .source = "/0/1/2"},
        {.ignition = true, .time = 0.01, .source = "/0/0"},
        {.ignition = false, .time = 0.06, .source = "null"},
        {.ignition = false, .time = 1.0, .source = "/9"},
        {.ignition = false, .time = 1.0, .source = "/8"},
        {.ignition = true, .time = 2.0, .source = "/1/1"},
        {.ignition = true, .time = 2.0, .source = "/1/0"},
    };
    EXPECT_EQ(orderedSources(keys, false), "/ /0/1/2 /0/1/5 /0/0 null /9 /8 /1/0 /1/1");
    EXPECT_EQ(orderedSources(keys, true), "/ /0/1/5 /0/1/2 /0/0 null /9 /8 /1/1 /1/0");
    EXPECT_EQ(orderedSources({}, false), "");
}

TEST(SimulationGoldenHarness, TheFileNamesAreTheDumpers)
{
    // GoldenDumper.slug(), and "sim_<NN>_<slug>".
    EXPECT_EQ(slug("[C6-5] RK6 stepper"), "c6-5-rk6-stepper");
    EXPECT_EQ(slug("[No motors]"), "no-motors");
    EXPECT_EQ(slug("[; 2 A10-0; A8-0]"), "2-a10-0-a8-0");
    EXPECT_EQ(slug("---"), "unnamed");
    EXPECT_EQ(slug(""), "unnamed");
    EXPECT_EQ(baseName({.index = 7, .configuration = 4, .name = "[C6-5] WGS84 geodetics"}),
              "sim_07_c6-5-wgs84-geodetics");
}

TEST(SimulationGoldenHarness, ThePerturbationMovesTheLastBitAwayFromZero)
{
    EXPECT_EQ(lastBitMoved(0.0), 0.0);
    EXPECT_EQ(lastBitMoved(1.0), 1.0 + std::numeric_limits<double>::epsilon());
    EXPECT_EQ(lastBitMoved(-1.0), -1.0 - std::numeric_limits<double>::epsilon());
    EXPECT_TRUE(std::isnan(lastBitMoved(std::numeric_limits<double>::quiet_NaN())));
    EXPECT_EQ(lastBitMoved(std::numeric_limits<double>::infinity()),
              std::numeric_limits<double>::infinity());
}

}  // namespace
