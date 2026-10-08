#include "goldens/GoldenSimulations.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <limits>
#include <memory>
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
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStepperMethod.h"
#include "QtRocket/simulation/exception/SimulationCalculationException.h"
#include "QtRocket/simulation/exception/SimulationCancelledException.h"
#include "QtRocket/simulation/exception/SimulationException.h"
#include "QtRocket/simulation/exception/SimulationListenerException.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
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

namespace QtRocket::Test
{

namespace
{

using nlohmann::json;

/// The comparison collector of the golden tests.
using Mismatches = GoldenMismatches;

// =================================================================================== harness

/// SimulationDumper.EXCLUDED_TYPES: the one data type left out of the CSV files (the wall clock).
[[nodiscard]] bool isExcludedType(const FlightDataType& type)
{
    return &type == &FlightDataType::builtin(QtRocket::FlightDataTypeId::TYPE_COMPUTATION_TIME);
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

// ======================================================================================= run

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
/// listeners placed first and last, in the harness's order. @p perturbation (or null) is added
/// between them.
void simulate(SimulationRun& run, const std::shared_ptr<QtRocket::SimulationListener>& perturbation)
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
    if (perturbation != nullptr)
    {
        conditions->getSimulationListenerList().push_back(perturbation);
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

// ==================================================================================== counts

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

// =============================================================================== sensitivity

/// One column of a branch in the two runs, with the tolerance of the comparison of the column
/// with the golden time series, and whether it is a noise-dominated out-of-plane column, which
/// is compared on the launch rod only (RodOnlyRule).
struct TwinColumn
{
    const std::vector<double>* values{nullptr};
    const std::vector<double>* twin{nullptr};
    double                     tolerance{0};
    bool                       rodOnly{false};
};

/// Whether row @p row is reproducible in every one of @p columns that is compared in it: every
/// column in a row on the launch rod (@p onTheRod), at the margin kSensitivityMargin; every
/// column but the noise-dominated out-of-plane ones in a later row, at the margin
/// @p flightMargin.
[[nodiscard]] bool rowIsReproducible(const std::vector<TwinColumn>& columns, std::size_t row,
                                     bool onTheRod, double flightMargin)
{
    const double margin = onTheRod ? kSensitivityMargin : flightMargin;
    return std::ranges::all_of(columns, [row, onTheRod, margin](const TwinColumn& column) {
        return (column.rodOnly && !onTheRod) ||
               reproducible((*column.values)[row], (*column.twin)[row], column.tolerance, margin);
    });
}

/// The columns of @p branch and of the perturbed run's branch @p twin, with the tolerances of
/// the comparison with the golden time series @p table, whose noise-dominated out-of-plane
/// columns @p rule names; none when @p twin lacks one of them.
[[nodiscard]] std::vector<TwinColumn> twinColumns(const FlightDataBranch& branch,
                                                  const FlightDataBranch& twin,
                                                  const GoldenTable& table, const RodOnlyRule& rule)
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
                           .tolerance = kValueRelative * columnScale(table, i),
                           .rodOnly   = rule.isNoise(i)});
    }
    return columns;
}

/// How far @p branch is reproducible: up to the first row in which a value of the perturbed
/// run's branch @p twin (null: it has none) differs from the run's by more than the tolerance
/// of its column over the margin of the row (kSensitivityMargin on the launch rod,
/// @p flightMargin after it), the tolerances being those of the comparison with the golden time
/// series @p table. A value that @p rule excludes from that comparison (a noise-dominated
/// out-of-plane column, off the launch rod) does not count.
[[nodiscard]] Horizon horizonOf(const FlightDataBranch& branch, const FlightDataBranch* twin,
                                const GoldenTable& table, const RodOnlyRule& rule,
                                double flightMargin)
{
    if (twin == nullptr)
    {
        return {};
    }
    const std::vector<TwinColumn> columns = twinColumns(branch, *twin, table, rule);
    if (columns.empty())
    {
        return {};
    }
    const std::size_t rows             = std::min(branch.getLength(), twin->getLength());
    std::size_t       reproducibleRows = 0;
    while (
        reproducibleRows < rows &&
        rowIsReproducible(columns, reproducibleRows, reproducibleRows < rule.rodRows, flightMargin))
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

/// How far @p run, which has flight data as the perturbed run @p twin has, is reproducible when a
/// row after the launch rod is judged at the margin @p flightMargin: the horizons of its
/// branches, and whether the whole run is reproducible (every branch to its last row, in a
/// perturbed run with as many branches and jitter replacements and the same status).
[[nodiscard]] Sensitivity sensitivityAt(const GoldenFiles& golden, const SimulationRun& run,
                                        const SimulationRun& twin, double flightMargin)
{
    Sensitivity sensitivity;
    sensitivity.whole = run.data->getBranchCount() == twin.data->getBranchCount() &&
                        run.jitterReplacements == twin.jitterReplacements &&
                        run.status == twin.status;
    const GoldenTable           noTable;
    const std::optional<double> cleared = goldenRodClearance(golden.document);
    for (std::size_t i = 0; i < run.data->getBranchCount(); i++)
    {
        const FlightDataBranch* twinBranch =
            i < twin.data->getBranchCount() ? &twin.data->getBranch(i) : nullptr;
        const GoldenTable& table = i < golden.tables.size() ? golden.tables[i] : noTable;
        sensitivity.horizons.push_back(horizonOf(run.data->getBranch(i), twinBranch, table,
                                                 rodOnlyRule(table, cleared), flightMargin));
        sensitivity.whole = sensitivity.whole && sensitivity.horizons.back().whole;
    }
    return sensitivity;
}

// ================================================================================ comparison

/// Compares the number @p actual with the golden number @p expected, within @p tolerance,
/// provided it is reproducible (@p stable); a number that is not is counted as sensitive.
void compareNumber(Mismatches& m, Comparison& c, const std::string& field, double expected,
                   double actual, bool stable, double tolerance)
{
    const bool compared = stable || c.strict;
    if (c.measurements != nullptr && std::isfinite(expected) && std::isfinite(actual))
    {
        c.measurements->record(c.context, field,
                               compared ? Treatment::COMPARED : Treatment::SENSITIVE,
                               std::abs(actual - expected), tolerance);
    }
    if (!compared)
    {
        c.sensitive.numbers++;
        return;
    }
    c.compared.numbers++;
    m.within(field, expected, actual, 0.0, tolerance);
}

/// Counts the number @p actual, whose golden number is @p expected, as one that the rule of
/// the out-of-plane noise columns excludes (RodOnlyRule): it is not compared.
void excludeNumber(Comparison& c, const std::string& field, double expected, double actual,
                   double tolerance)
{
    if (c.measurements != nullptr && std::isfinite(expected) && std::isfinite(actual))
    {
        c.measurements->record(c.context, field, Treatment::EXCLUDED, std::abs(actual - expected),
                               tolerance);
    }
    c.excluded.numbers++;
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
// the first three of the file, leaves to goldens_schema_tests.cpp). Those of the "harness"
// object are kHarnessKeys and kStableHarnessKeys of the header.
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

/// Compares what the harness changed (makeReproducible() and, for the stable-step set,
/// useStableTimeStep()), @p actual, with the golden "harness", whose keys are @p keys.
void compareHarness(Mismatches& m, const json& expected, const json& actual,
                    std::span<const std::string_view> keys)
{
    for (const std::string_view key : keys)
    {
        if (!expected.contains(key) || !actual.contains(key) || expected.at(key) != actual.at(key))
        {
            m.text(key, expected.contains(key) ? expected.at(key).dump() : "nothing",
                   actual.contains(key) ? actual.at(key).dump() : "nothing");
        }
    }
    noteUncomparedKeys(m, "", expected, keys);
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
    /// The noise-dominated out-of-plane columns of its golden time series, which are compared
    /// on the launch rod only (none in the strict comparison).
    RodOnlyRule rule;
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

/// One column of a branch against one column of the golden time series.
struct ColumnOf
{
    std::string                key;
    const GoldenTable*         table{nullptr};
    std::size_t                column{0};
    const std::vector<double>* values{nullptr};
    double                     scale{0};
    const RodOnlyRule*         rule{nullptr};  ///< of the golden time series
};

/// Records the differences of the column @p column from the golden one: those of the first
/// @p rows rows, which are compared, and those of the later ones, which are sensitive; but
/// those of the rows in which the rule of the noise columns excludes the column as excluded.
void measureColumn(const Comparison& c, const ColumnOf& column, std::size_t rows)
{
    const std::size_t common = std::min(column.values->size(), column.table->rows.size());
    for (std::size_t row = 0; row < common; row++)
    {
        const double expected = column.table->rows[row][column.column];
        const double actual   = (*column.values)[row];
        if (std::isfinite(expected) && std::isfinite(actual))
        {
            const Treatment sensitive = row >= rows ? Treatment::SENSITIVE : Treatment::COMPARED;
            c.measurements->record(
                c.context, "column:" + column.key,
                column.rule->excludes(column.column, row) ? Treatment::EXCLUDED : sensitive,
                std::abs(actual - expected), kValueRelative * column.scale);
        }
    }
}

/// Compares the values of the column @p column in the first @p rows rows with the golden ones,
/// row by row: each within kValueRelative of the scale of the column. A noise-dominated
/// out-of-plane column is compared in the rows on the launch rod only (RodOnlyRule). The rows
/// that differ are reported in one line: how many, and the first of them.
void compareColumnValues(Mismatches& m, const Comparison& c, const ColumnOf& column,
                         std::size_t rows)
{
    if (c.measurements != nullptr)
    {
        measureColumn(c, column, rows);
    }
    const double      tolerance = kValueRelative * column.scale;
    const std::size_t compared  = column.rule->comparedRows(column.column, rows);
    std::size_t       differing = 0;
    std::size_t       first     = 0;
    for (std::size_t row = 0; row < compared; row++)
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
        m.note(std::format(
            "column {}: {} of {} rows differ, the first at row {}: expected {}, "
            "got {} (difference {})",
            column.key, differing, compared, first, expected, actual, actual - expected));
    }
}

/// The golden minimum or maximum @p expected of a column of a branch against the run's.
struct ExtremeOf
{
    std::string field;  ///< "columns[3].min (altitude)"
    double      expected{0};
    double      actual{0};
};

/// Compares the minimum or maximum @p extreme of the column @p column of the branch with the
/// golden one, within kValueRelative of the scale of the column, when the whole branch is
/// reproducible (@p whole): it is sensitive otherwise. Of a noise-dominated out-of-plane
/// column of such a branch an extreme that the golden column or the run's attains only off the
/// launch rod is one of the values the rule excludes (RodOnlyRule::excludesExtreme()).
void compareExtreme(Mismatches& m, Comparison& c, const ColumnOf& column, const ExtremeOf& extreme,
                    bool whole)
{
    if (whole && column.values != nullptr && column.rule->isNoise(column.column))
    {
        const std::vector<double> golden =
            column.table->column(column.table->columns[column.column])
                .value_or(std::vector<double>{});
        if (column.rule->excludesExtreme(column.column, golden, extreme.expected) ||
            column.rule->excludesExtreme(column.column, *column.values, extreme.actual))
        {
            excludeNumber(
                c, extreme.field, extreme.expected, extreme.actual,
                kValueRelative * referenceOf(column.scale, extreme.expected, extreme.actual));
            return;
        }
    }
    compareValue(m, c, extreme.field, extreme.expected, extreme.actual, whole, column.scale);
}

/// Compares the columns of the branch with the golden "columns" (key, name, symbol, whether
/// built in, in order; minimum and maximum when the whole branch is reproducible) and with the
/// golden time series @p table: the values of the first @p rows rows, a noise-dominated
/// out-of-plane column in those on the launch rod only. Returns the number of golden columns
/// compared.
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
        const ColumnOf series{.key    = key,
                              .table  = &table,
                              .column = i,
                              .values = branch.getView(type),
                              .scale  = columnScale(table, i),
                              .rule   = &ours.rule};
        compareExtreme(m, c, series,
                       {.field    = std::format("{}.min ({})", field, key),
                        .expected = goldenValue(column.at("min")),
                        .actual   = branch.getMinimum(type)},
                       ours.horizon.whole);
        compareExtreme(m, c, series,
                       {.field    = std::format("{}.max ({})", field, key),
                        .expected = goldenValue(column.at("max")),
                        .actual   = branch.getMaximum(type)},
                       ours.horizon.whole);
        noteUncomparedKeys(m, field, column, kColumnKeys);
        compareColumnValues(m, c, series, rows);
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

/// The golden branch @p expected of a simulation whose rocket clears the launch rod at
/// @p cleared (nullopt: never), and its time series @p table.
struct GoldenBranch
{
    const json*           expected{nullptr};
    const GoldenTable*    table{nullptr};
    std::optional<double> cleared;
};

/// The rule of the out-of-plane noise columns for the golden time series @p golden, as the
/// comparison @p c applies it: not at all when it is strict.
[[nodiscard]] RodOnlyRule ruleOf(const Comparison& c, const GoldenBranch& golden)
{
    if (!c.strict)
    {
        return rodOnlyRule(*golden.table, golden.cleared);
    }
    return {.rows    = golden.table->rows.size(),
            .rodRows = golden.table->rows.size(),
            .noise   = std::vector<bool>(golden.table->columns.size(), false)};
}

/// Counts the rows and the values of a branch with the rule @p rule whose first @p rows rows
/// are compared, @p whole saying whether the branch is reproducible as a whole: the values that
/// are compared (those of the rows, without what the rule excludes in them), the values the
/// rule excludes in any row, and the rows from the horizon on with what the rule leaves of
/// their values, which are sensitive.
void countBranch(Comparison& c, const RodOnlyRule& rule, std::size_t rows, bool whole)
{
    const auto noise    = rule.noiseColumnCount();
    const auto columns  = static_cast<std::int64_t>(rule.noise.size());
    const auto compared = static_cast<std::int64_t>(rows);
    // The rows in which a noise column is compared: those of the rows that are on the rod.
    const auto onTheRod = static_cast<std::int64_t>(std::min(rows, rule.rodRows));
    c.compared.rows += compared;
    c.compared.values += (compared * (columns - noise)) + (onTheRod * noise);
    c.excluded.values += rule.excludedValues();
    if (!c.strict && !whole)
    {
        // The rows of the golden time series from the horizon on.
        const auto sensitive = static_cast<std::int64_t>(rule.rows - rows);
        c.sensitive.rows += sensitive;
        c.sensitive.values += (sensitive * (columns - noise)) +
                              ((static_cast<std::int64_t>(rule.rodRows) - onTheRod) * noise);
    }
}

/// Compares branch @p index of @p run with the golden branch @p golden; @p sensitivity says how
/// far the run is reproducible.
void compareBranch(Comparison& c, std::size_t index, const GoldenBranch& golden,
                   const SimulationRun& run, const Sensitivity& sensitivity)
{
    Mismatches         m(std::format("{} branch {}", c.context, index));
    const GoldenTable& table = *golden.table;
    const BranchOf     ours{.data    = run.data.get(),
                            .branch  = &run.data->getBranch(index),
                            .rocket  = run.rocket.get(),
                            .horizon = sensitivity.horizons.at(index),
                            .whole   = sensitivity.whole,
                            .rule    = ruleOf(c, golden)};
    compareBranchHeader(m, c, *golden.expected, index, ours, run.planned);

    // The rows compared: those that are reproducible, as far as both sides have them.
    const std::size_t rows = std::min({c.strict ? ours.branch->getLength() : ours.horizon.rows,
                                       ours.branch->getLength(), table.rows.size()});
    c.compared.branches++;
    c.compared.columns += compareColumns(m, c, golden.expected->at("columns"), table, ours, rows);
    c.compared.events += compareEvents(m, c, golden.expected->at("events"), ours);
    countBranch(c, ours.rule, rows, ours.horizon.whole);
    c.report += m.report();
}

// -------------------------------------------------------------------------------- simulation

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
    const std::optional<double> cleared = goldenRodClearance(document);
    for (std::size_t i = 0;
         i < std::min({branches.size(), run.data->getBranchCount(), golden.tables.size()}); i++)
    {
        compareBranch(c, i,
                      {.expected = &branches.at(i), .table = &golden.tables[i], .cleared = cleared},
                      run, sensitivity);
    }
}

// ============================================================ the scaffolding of the tests

/// The value at the JSON pointer @p pointer of @p document.
[[nodiscard]] json& valueAt(json& document, std::string_view pointer)
{
    return document.at(json::json_pointer{std::string{pointer}});
}

}  // namespace

// =================================================================================== harness

std::string columnKey(const FlightDataType& type)
{
    return type.isBuiltin() ? type.getSaveKey() : "custom:" + type.getName();
}

std::string slug(std::string_view text)
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

InstallationDefaults::InstallationDefaults()
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

json makeReproducible(SimulationOptions& options)
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

std::vector<PlannedSimulation> plannedSimulations(Rocket& rocket, std::string_view input)
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

std::string baseName(const PlannedSimulation& planned)
{
    return std::format("sim_{:02}_{}", planned.index, slug(planned.name));
}

// ======================================================================================= run

double lastBitMoved(double value)
{
    if (value == 0 || !std::isfinite(value))
    {
        return value;
    }
    return std::nextafter(value, value > 0 ? std::numeric_limits<double>::infinity()
                                           : -std::numeric_limits<double>::infinity());
}

void useStableTimeStep(SimulationOptions& options, json& harness, double timeStep)
{
    harness["documentTimeStep"] = options.getTimeStep();
    options.setTimeStep(timeStep);
    harness["timeStep"] = timeStep;
}

SimulationRun runSimulation(const TestRocketMaker& maker, std::size_t index,
                            const RunVariation& variation)
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
    if (variation.stableTimeStep.has_value())
    {
        useStableTimeStep(run.simulation->getOptions(), run.harness, *variation.stableTimeStep);
    }
    simulate(run, variation.perturbation);
    return run;
}

// ==================================================================================== golden

GoldenFiles loadGoldenFiles(const GoldenSimulation& simulation)
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

SimulationCounts operator+(SimulationCounts a, const SimulationCounts& b)
{
    a += b;
    return a;
}

std::string toText(const SimulationCounts& counts)
{
    return std::format(
        "{} simulations, {} branches, {} events, {} columns, {} warnings, {} "
        "numbers, {} rows, {} values",
        counts.simulations, counts.branches, counts.events, counts.columns, counts.warnings,
        counts.numbers, counts.rows, counts.values);
}

std::ostream& operator<<(std::ostream& out, const SimulationCounts& counts)
{
    return out << toText(counts);
}

SimulationCounts goldenCounts(const GoldenFiles& files)
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

std::string_view treatmentName(Treatment treatment)
{
    switch (treatment)
    {
        case Treatment::COMPARED:
            return "compared";
        case Treatment::SENSITIVE:
            return "sensitive";
        case Treatment::EXCLUDED:
            break;
    }
    return "excluded";
}

// ============================================================ the out-of-plane noise columns

std::vector<bool> noiseColumns(const GoldenTable& table)
{
    std::vector<bool> noise(table.columns.size(), false);
    for (const OutOfPlaneColumn& outOfPlane : kOutOfPlaneColumns)
    {
        const std::optional<std::size_t> column      = table.columnIndex(outOfPlane.key);
        const std::optional<std::size_t> measure     = table.columnIndex(outOfPlane.measure);
        const std::optional<std::size_t> counterpart = table.columnIndex(outOfPlane.counterpart);
        if (!column.has_value() || !measure.has_value() || !counterpart.has_value())
        {
            continue;
        }
        const double scale = columnScale(table, *measure);
        noise[*column]     = scale > 0 && scale < kNoiseRatio * columnScale(table, *counterpart);
    }
    return noise;
}

bool RodOnlyRule::isNoise(std::size_t column) const
{
    return column < noise.size() && noise[column];
}

bool RodOnlyRule::excludes(std::size_t column, std::size_t row) const
{
    return row >= rodRows && isNoise(column);
}

std::size_t RodOnlyRule::comparedRows(std::size_t column, std::size_t compared) const
{
    return isNoise(column) ? std::min(compared, rodRows) : compared;
}

bool RodOnlyRule::excludesExtreme(std::size_t column, std::span<const double> values,
                                  double extreme) const
{
    if (!isNoise(column))
    {
        return false;
    }
    bool onTheRod  = false;
    bool offTheRod = false;
    for (std::size_t row = 0; row < values.size(); row++)
    {
        if (values[row] == extreme)
        {
            onTheRod  = onTheRod || row < rodRows;
            offTheRod = offTheRod || row >= rodRows;
        }
    }
    return offTheRod && !onTheRod;
}

std::int64_t RodOnlyRule::noiseColumnCount() const
{
    // (A loop: a vector<bool> is no range of bool for the algorithms of every standard library.)
    std::int64_t count = 0;
    for (const bool flag : noise)
    {
        count += flag ? 1 : 0;
    }
    return count;
}

std::int64_t RodOnlyRule::excludedValues() const
{
    return noiseColumnCount() * static_cast<std::int64_t>(rows - std::min(rows, rodRows));
}

RodOnlyRule rodOnlyRule(const GoldenTable& table, const std::optional<double>& cleared)
{
    return {.rows    = table.rows.size(),
            .rodRows = rowsUpToTheClearance(table, cleared),
            .noise   = noiseColumns(table)};
}

// =============================================================================== sensitivity

bool reproducible(double actual, double twin, double tolerance, double margin)
{
    if (std::isnan(actual) || std::isnan(twin))
    {
        return std::isnan(actual) == std::isnan(twin);
    }
    return actual == twin || std::abs(actual - twin) <= tolerance / margin;
}

std::vector<const FlightDataType*> csvTypes(const FlightDataBranch& branch)
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

double columnScale(const GoldenTable& table, std::size_t column)
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

Sensitivity sensitivityOf(const GoldenFiles& golden, const SimulationRun& run,
                          const SimulationRun& twin)
{
    if (run.data == nullptr || twin.data == nullptr)
    {
        Sensitivity none;
        none.whole = run.data == nullptr && twin.data == nullptr;
        return none;
    }
    // A run that the perturbed run reproduces to its last row at the margin of the launch rod
    // is whole: nothing grows in it. Any other run diverges somewhere, and the reproducible
    // part of its free flight ends at the margin of the free flight.
    const Sensitivity steady = sensitivityAt(golden, run, twin, kSensitivityMargin);
    return steady.whole ? steady : sensitivityAt(golden, run, twin, kFlightSensitivityMargin);
}

// ================================================================================ comparison

double referenceOf(double scale, double expected, double actual)
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

// ---------------------------------------------------------------------------------- warnings

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
        c.measurements->record(c.context, field + ".parameter",
                               compared ? Treatment::COMPARED : Treatment::SENSITIVE,
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

// ------------------------------------------------------------------------------------ events

std::string pathOrNull(const RocketComponent* component)
{
    return component == nullptr ? std::string{"null"} : goldenPathOf(*component);
}

std::vector<std::size_t> comparisonOrder(const std::vector<EventKey>& keys, bool recorded)
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

std::vector<const FlightEvent*> orderedEvents(const FlightDataBranch& branch, bool recorded)
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

std::vector<const json*> orderedGoldenEvents(const json& events, bool recorded)
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

std::string sequenceOf(const std::vector<const FlightEvent*>& events)
{
    std::string sequence;
    for (const FlightEvent* event : events)
    {
        sequence += sequence.empty() ? "" : " ";
        sequence += std::format("{}({})", name(event->getType()), pathOrNull(event->getSource()));
    }
    return sequence;
}

std::string sequenceOf(const std::vector<const json*>& events)
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

// ----------------------------------------------------------------------------------- columns

bool differs(double expected, double actual, double tolerance)
{
    if (std::isnan(expected) || std::isnan(actual))
    {
        return std::isnan(expected) != std::isnan(actual);
    }
    return expected != actual && !(std::abs(actual - expected) <= tolerance);
}

// -------------------------------------------------------------------------------- simulation

SimulationComparison compareSimulation(const GoldenFiles& golden, const SimulationRun& run,
                                       const Sensitivity& sensitivity, const std::string& context,
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
    if (mode.stableSet)
    {
        compareHarness(harness, document.at("harness"), run.harness, kStableHarnessKeys);
    }
    else
    {
        compareHarness(harness, document.at("harness"), run.harness, kHarnessKeys);
    }
    c.report += harness.report();

    Mismatches result(context + " result");
    compareResult(result, c, document.at("result"), run, sensitivity.whole);
    c.report += result.report();

    compareData(c, golden, run, sensitivity);
    c.compared.simulations = 1;
    return {.compared  = c.compared,
            .sensitive = c.sensitive,
            .excluded  = c.excluded,
            .report    = c.report};
}

// ============================================================================== the goldens

const QtRocket::Result<GoldenManifest>& manifest()
{
    static const QtRocket::Result<GoldenManifest> kManifest = QtRocket::Test::loadGoldenManifest();
    return kManifest;
}

const GoldenInput* inputOf(const TestRocketMaker& maker)
{
    return manifest().has_value() ? manifest()->find(maker.input) : nullptr;
}

std::optional<double> goldenRodClearance(const json& document)
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

std::size_t rowsUpToTheClearance(const GoldenTable& table, const std::optional<double>& cleared)
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

std::int64_t floorRows(const GoldenFiles& files)
{
    const std::optional<double> cleared = goldenRodClearance(files.document);
    std::int64_t                rows    = 0;
    for (const GoldenTable& table : files.tables)
    {
        rows += static_cast<std::int64_t>(rowsUpToTheClearance(table, cleared));
    }
    return rows;
}

// ============================================================ the scaffolding of the tests

std::string makerTestName(const ::testing::TestParamInfo<TestRocketMaker>& paramInfo)
{
    std::string name{paramInfo.param.input};
    std::ranges::replace(name, '-', '_');
    return name;
}

void scale(GoldenFiles& files, std::string_view pointer, double relative)
{
    json& value = valueAt(files.document, pointer);
    value       = value.get<double>() * (1 + relative);
}

void shift(GoldenFiles& files, std::string_view pointer, double offset)
{
    json& value = valueAt(files.document, pointer);
    value       = value.get<double>() + offset;
}

void shiftSeries(GoldenFiles& files, std::string_view key, std::size_t row, double relative)
{
    GoldenTable&                     table  = files.tables.at(0);
    const std::optional<std::size_t> column = table.columnIndex(key);
    if (column.has_value())
    {
        table.rows.at(row).at(*column) += relative * columnScale(table, *column);
    }
}

std::ostream& operator<<(std::ostream& out, const SubjectCase& subjectCase)
{
    return out << subjectCase.name;
}

std::string subjectTestName(const ::testing::TestParamInfo<SubjectCase>& paramInfo)
{
    return std::string{paramInfo.param.name};
}

std::vector<std::string_view> sortedNames(const std::vector<Mutation>& all)
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

json goldenFormOf(const Warning& warning)
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

}  // namespace QtRocket::Test
