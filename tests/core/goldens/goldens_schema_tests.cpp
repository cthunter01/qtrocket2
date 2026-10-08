// Schema checks of the golden reference data in tests/data/goldens (tools/openrocket-goldens):
// every file manifest.json lists parses, carries the schema version and has the keys the golden
// comparison tests rely on. Also tests the GoldenData loader itself.
//
// The stable-step sets (every simulation once more at a time step of 0.01 s). That of the
// thirteen test rockets has the time series of its branches, as the default-step set of every
// input has. That of the sixteen example designs is the simulation documents alone: 54
// documents with the summary values, the warnings, the result and, for each of their 69
// branches, the number of rows, the columns with their minima and maxima, the events, the
// optimum altitude and the separation time; "csv" of such a branch is null and no time series
// is on file (the user's decision for tier 9: the time series would add 37 MB). The manifest
// says which kinds of inputs have a stable-step set and which of those have time series, and a
// branch without one is accepted only there.
//
// The checks here are of the files alone. No test compares QtRocket with the stable-step
// documents of the examples yet: an example cannot be loaded before the .ork loader exists
// (tier 9, run 9b), and the comparison of the 54 simulations with these documents is run 9c's.
// The tolerances of that comparison come from a measurement of OpenRocket against itself
// ("Reproducibility of the simulations" in tools/openrocket-goldens/README.md).

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <format>
#include <initializer_list>
#include <limits>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <pugixml.hpp>

#include "QtRocket/file/GzipStream.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "Sha256.h"
#include "TestPaths.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenMismatches.h"

namespace
{

using nlohmann::json;
using QtRocket::ErrorCode;
using QtRocket::Test::GoldenInput;
using QtRocket::Test::GoldenManifest;
using QtRocket::Test::GoldenSimulation;

constexpr std::size_t kExampleCount    = 16;
constexpr std::size_t kTestRocketCount = 13;
/// The stable-step set of the thirteen test rockets: its simulations, and its files, which are
/// one document per simulation and one time series per branch (53).
constexpr std::size_t kStableTestRocketSimulationCount = 50;
constexpr std::size_t kStableTestRocketFileCount       = 103;
/// The stable-step set of the sixteen examples: its simulations, and its files, which are the
/// documents alone.
constexpr std::size_t kStableExampleSimulationCount = 54;
constexpr std::size_t kStableExampleFileCount       = 54;
/// The Mach x AoA grid (7 x 3) plus the four off-grid points of aero.json: a lateral wind
/// direction, rotation about the nose tip and about the structure CG, thrusting nozzles.
constexpr std::size_t kAeroPointCount = 25;
/// The index of the thrusting-nozzle point (the last one).
constexpr std::size_t kNozzlePoint = 24;
constexpr std::size_t kMachCount   = 7;
constexpr std::size_t kMissing     = std::numeric_limits<std::size_t>::max();

/// A directory in the temporary directory, removed again when the test ends.
class TempDir
{
public:
    TempDir()
      : m_path(std::filesystem::temp_directory_path() /
               std::format("qtrocket_goldens_{}", std::random_device{}()))
    {
        std::filesystem::create_directories(m_path);
    }
    ~TempDir()
    {
        std::error_code ignored;
        std::filesystem::remove_all(m_path, ignored);
    }
    TempDir(const TempDir&)            = delete;
    TempDir& operator=(const TempDir&) = delete;
    TempDir(TempDir&&)                 = delete;
    TempDir& operator=(TempDir&&)      = delete;

    [[nodiscard]] const std::filesystem::path& path() const { return m_path; }

private:
    std::filesystem::path m_path;
};

GoldenManifest loadManifestOrFail()
{
    auto manifest = QtRocket::Test::loadGoldenManifest();
    if (!manifest)
    {
        ADD_FAILURE() << manifest.error().toString();
        return {};
    }
    return *manifest;
}

json loadJsonOrFail(const std::string& path)
{
    auto parsed = QtRocket::Test::loadGoldenJson(path);
    if (!parsed)
    {
        ADD_FAILURE() << parsed.error().toString();
        return json::object();
    }
    return *parsed;
}

/// The error code of a failed result; UNKNOWN for a success, which the callers do not expect.
template <class T>
ErrorCode errorCode(const QtRocket::Result<T>& result)
{
    return result.has_value() ? ErrorCode::UNKNOWN : result.error().code;
}

bool manifestParses(const json& manifest)
{
    return QtRocket::Test::parseGoldenManifest(manifest).has_value();
}

void expectKeys(const json& object, std::initializer_list<std::string_view> keys,
                const std::string& context)
{
    ASSERT_TRUE(object.is_object()) << context;
    for (const auto key : keys)
    {
        EXPECT_TRUE(object.contains(key)) << context << ": missing \"" << key << "\"";
    }
}

void expectNumber(const json& object, std::string_view key, const std::string& context)
{
    const auto it = object.find(key);
    ASSERT_NE(it, object.end()) << context << ": missing \"" << key << "\"";
    EXPECT_TRUE(QtRocket::Test::goldenNumber(*it).has_value())
        << context << ": \"" << key << "\" is not a number";
}

/// A coordinate array of @p size numbers.
void expectCoordinate(const json& value, std::size_t size, const std::string& context)
{
    ASSERT_TRUE(value.is_array()) << context;
    ASSERT_EQ(value.size(), size) << context;
    for (const auto& element : value)
    {
        EXPECT_TRUE(QtRocket::Test::goldenNumber(element).has_value()) << context;
    }
}

void expectHeader(const json& document, std::string_view schema, const std::string& input,
                  const std::string& context)
{
    expectKeys(document, {"schema", "schemaVersion", "input"}, context);
    EXPECT_EQ(document.value("schema", ""), schema) << context;
    EXPECT_EQ(document.value("schemaVersion", -1), QtRocket::Test::kGoldenSchemaVersion) << context;
    EXPECT_EQ(document.value("input", ""), input) << context;
}

void expectConfigurationHeader(const json& configuration, std::size_t index,
                               const std::string& context)
{
    expectKeys(configuration, {"index", "id", "isDefault", "name"}, context);
    EXPECT_EQ(configuration.value("index", kMissing), index) << context;
}

void expectRigidBody(const json& body, const std::string& context)
{
    expectKeys(body,
               {"mass", "cm", "ixx", "iyy", "izz", "longitudinalInertia", "rotationalInertia"},
               context);
    expectNumber(body, "mass", context);
    expectCoordinate(body.value("cm", json::array()), 4, context + " cm");
}

void expectForces(const json& forces, const std::string& context)
{
    for (const std::string_view key :
         {"cn", "cm", "cside", "cyaw", "croll", "crollDamp", "crollForce", "cd", "cdAxial",
          "pressureCD", "baseCD", "frictionCD", "overrideCD", "pitchDampingMoment",
          "yawDampingMoment"})
    {
        expectNumber(forces, key, context);
    }
    expectCoordinate(forces.value("cp", json::array()), 4, context + " cp");
}

// ---- manifest checks ----

bool isLowerHex(std::string_view text)
{
    return std::ranges::all_of(
        text, [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

/// An example input names its directory after, and has as its source, a data/examples file:
/// the very file the goldens were made from, by its SHA-256 (the tests that load an example
/// compare what they load with these goldens).
void checkExampleInput(const GoldenInput& input)
{
    constexpr std::string_view kExamplesPrefix = "data/examples/";
    EXPECT_TRUE(input.name.starts_with("example-")) << input.name;
    ASSERT_TRUE(input.source.starts_with(kExamplesPrefix)) << input.source;
    const std::filesystem::path file =
        QtRocket::Test::dataDir() / "examples" /
        std::filesystem::path{input.source.substr(kExamplesPrefix.size())};
    const auto bytes = QtRocket::readFile(file);
    ASSERT_TRUE(bytes.has_value()) << input.source << ": " << bytes.error().toString();
    EXPECT_EQ(QtRocket::Test::sha256Hex(*bytes), input.sourceSha256)
        << input.source << " is not the file the goldens were made from";
}

/// The file @p file of the default-step set, as the stable-step set names it: in the directory
/// "stable" next to it.
std::string stableFileName(const std::string& file)
{
    const std::filesystem::path path{file};
    return (path.parent_path() / "stable" / path.filename()).generic_string();
}

/// What is wrong with the entry @p stable of a stable-step set, which has to be the simulation
/// @p simulation of the default-step set: the same name, the same file name in the directory
/// "stable", and branch files named after it; no branch file at all in a set without time
/// series (@p timeSeries false). (How many branches there are is a result: a flight can end
/// otherwise at another time step.)
std::string stableEntryProblems(const GoldenSimulation& stable, const GoldenSimulation& simulation,
                                bool timeSeries)
{
    std::string problems;
    if (stable.name != simulation.name || stable.json != stableFileName(simulation.json))
    {
        problems += std::format("{} ({}) is not the stable-step simulation of {} ({})\n",
                                stable.json, stable.name, simulation.json, simulation.name);
    }
    if (!timeSeries && !stable.branches.empty())
    {
        problems += std::format(
            "{}: lists {} time series, but the stable-step set of its kind "
            "is the documents alone\n",
            stable.json, stable.branches.size());
    }
    const std::string base = stable.json.substr(0, stable.json.rfind(".json"));
    for (std::size_t b = 0; b < stable.branches.size(); ++b)
    {
        if (stable.branches[b] != std::format("{}_branch{}.csv.gz", base, b))
        {
            problems += std::format("{}: branch {} is {}\n", stable.json, b, stable.branches[b]);
        }
    }
    return problems;
}

/// The stable-step set of @p input, which every input has: the simulations of the default-step
/// set, one for one, with the same names and with the same file names in the directory
/// "stable"; with the time series of its branches for a test rocket, without for an example.
void checkStableList(const GoldenInput& input, const GoldenManifest& manifest)
{
    EXPECT_TRUE(manifest.hasStableSimulations(input)) << input.name;
    const bool timeSeries = manifest.hasStableTimeSeries(input);
    EXPECT_EQ(timeSeries, input.kind == "testrocket") << input.name;
    ASSERT_EQ(input.stableSimulations.size(), input.simulations.size()) << input.name;
    std::string problems;
    for (std::size_t i = 0; i < input.simulations.size(); ++i)
    {
        problems +=
            stableEntryProblems(input.stableSimulations[i], input.simulations[i], timeSeries);
    }
    EXPECT_EQ(problems, "") << input.name;
}

/// checkStableList() of every input of @p manifest.
void checkStableLists(const GoldenManifest& manifest)
{
    for (const auto& input : manifest.inputs)
    {
        checkStableList(input, manifest);
    }
}

/// Checks one manifest input and counts it by kind.
void checkManifestInput(const GoldenInput& input, std::size_t& examples, std::size_t& testRockets)
{
    EXPECT_FALSE(input.simulations.empty()) << input.name;
    if (input.kind == "example")
    {
        checkExampleInput(input);
        ++examples;
        return;
    }
    EXPECT_EQ(input.kind, "testrocket") << input.name;
    EXPECT_TRUE(input.name.starts_with("testrocket-")) << input.name;
    EXPECT_EQ(input.sourceSha256, "") << input.name << ": a test rocket is not made from a file";
    ++testRockets;
}

std::size_t countExampleFiles()
{
    std::size_t count = 0;
    for (const auto& entry :
         std::filesystem::directory_iterator(QtRocket::Test::dataDir() / "examples"))
    {
        if (entry.path().extension() == ".ork")
        {
            ++count;
        }
    }
    return count;
}

std::set<std::string> listedFiles(const GoldenManifest& manifest)
{
    std::set<std::string> listed;
    for (const auto& input : manifest.inputs)
    {
        listed.insert({input.geometry, input.mass, input.aero, input.resave});
        for (const auto* simulations : {&input.simulations, &input.stableSimulations})
        {
            for (const auto& simulation : *simulations)
            {
                listed.insert(simulation.json);
                listed.insert(simulation.branches.begin(), simulation.branches.end());
            }
        }
    }
    return listed;
}

/// How large the stable-step sets of the inputs of one kind are.
struct StableSetSize
{
    std::size_t simulations{0};
    std::size_t files{0};  ///< the documents and the time series
};

/// What @p manifest lists for the stable-step sets of its inputs of the kind @p kind.
StableSetSize stableSetSize(const GoldenManifest& manifest, std::string_view kind)
{
    StableSetSize size;
    for (const auto& input : manifest.inputs)
    {
        if (input.kind != kind)
        {
            continue;
        }
        for (const auto& simulation : input.stableSimulations)
        {
            ++size.simulations;
            size.files += 1 + simulation.branches.size();
        }
    }
    return size;
}

/// Every file under the input's directory is in @p listed.
void expectDirectoryListed(const std::string& inputName, const std::set<std::string>& listed)
{
    const auto dir = QtRocket::Test::goldensDir() / inputName;
    ASSERT_TRUE(std::filesystem::is_directory(dir)) << dir.generic_string();
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }
        const auto relative =
            entry.path().lexically_relative(QtRocket::Test::goldensDir()).generic_string();
        EXPECT_TRUE(listed.contains(relative)) << "not in manifest.json: " << relative;
    }
}

// ---- geometry.json ----

/// An override value is given exactly when the override is set (OpenRocket's getters return a
/// computed stand-in otherwise).
void checkOverrides(const json& overrides, const std::string& context)
{
    for (const auto& [flag, value] :
         {std::pair{"massOverridden", "overrideMass"}, std::pair{"cgOverridden", "overrideCGX"},
          std::pair{"cdOverridden", "overrideCD"}})
    {
        const auto overridden = overrides.find(flag);
        ASSERT_TRUE(overridden != overrides.end() && overridden->is_boolean())
            << context << ": " << flag;
        const auto overrideValue = overrides.find(value);
        ASSERT_NE(overrideValue, overrides.end()) << context << ": " << value;
        EXPECT_EQ(overrideValue->is_null(), !overridden->get<bool>()) << context << ": " << value;
    }
}

void checkComponent(const json& component, const std::string& file, std::set<std::string>& paths)
{
    const std::string context = file + " " + component.value("path", "?");
    expectKeys(component,
               {"path",
                "type",
                "name",
                "id",
                "stageNumber",
                "length",
                "axialMethod",
                "position",
                "componentMass",
                "componentCG",
                "longitudinalUnitInertia",
                "rotationalUnitInertia",
                "mass",
                "cg",
                "overrides",
                "componentBounds",
                "instanceCount",
                "instanceOffsets",
                "instanceAngles",
                "componentLocations",
                "details"},
               context);
    EXPECT_TRUE(paths.insert(component.value("path", "")).second) << context;
    expectNumber(component, "componentMass", context);
    expectCoordinate(component.value("componentCG", json::array()), 4, context);
    expectCoordinate(component.value("position", json::array()), 3, context);
    EXPECT_EQ(component.value("instanceOffsets", json::array()).size(),
              component.value("instanceCount", kMissing))
        << context;
    checkOverrides(component.value("overrides", json::object()), context);
}

void checkInstances(const json& entry, const std::set<std::string>& paths,
                    const std::string& context)
{
    EXPECT_TRUE(paths.contains(entry.value("path", ""))) << context;
    for (const auto& instance : entry.value("instances", json::array()))
    {
        expectKeys(instance, {"instanceNumber", "location", "transform"}, context);
        const auto& transform = instance.value("transform", json::object());
        expectCoordinate(transform.value("rotation", json::array()), 9, context);
        expectCoordinate(transform.value("translation", json::array()), 3, context);
    }
}

void checkGeometryConfiguration(const json& configuration, std::size_t index,
                                const std::set<std::string>& paths, const std::string& file)
{
    const std::string context = file + " configuration " + std::to_string(index);
    expectConfigurationHeader(configuration, index, context);
    expectKeys(configuration,
               {"stageCount", "activeStages", "referenceLength", "referenceArea", "length",
                "boundingBox", "motors", "activeComponents", "instances"},
               context);
    for (const auto& entry : configuration.value("instances", json::array()))
    {
        checkInstances(entry, paths, context);
    }
}

// ---- mass.json ----

void checkMassConfiguration(const json& configuration, std::size_t index, const std::string& file)
{
    const std::string context = file + " configuration " + std::to_string(index);
    expectConfigurationHeader(configuration, index, context);
    for (const std::string_view key : {"structure", "launch", "burnout", "motor"})
    {
        expectRigidBody(configuration.value(key, json::object()),
                        std::format("{} {}", context, key));
    }
    const auto& analysis = configuration.value("cmAnalysis", json::array());
    ASSERT_FALSE(analysis.empty()) << context;
    EXPECT_EQ(analysis.back().value("kind", ""), "total") << context;
    for (const auto& row : analysis)
    {
        expectKeys(row, {"kind", "path", "name", "eachMass", "totalCM"}, context);
        expectCoordinate(row.value("totalCM", json::array()), 4, context);
    }
}

// ---- aero.json ----

void checkAeroPoint(const json& point, const std::string& context)
{
    expectKeys(point, {"conditions", "cp", "forces", "components", "warnings"}, context);
    const auto& conditions = point.value("conditions", json::object());
    expectKeys(conditions,
               {"mach", "aoa", "theta", "rollRate", "pitchRate", "yawRate", "pitchCenter",
                "thrustingNozzleExitAreas"},
               context);
    for (const auto& nozzle : conditions.value("thrustingNozzleExitAreas", json::array()))
    {
        expectKeys(nozzle, {"assembly", "area"}, context);
        expectNumber(nozzle, "area", context);
    }
    expectCoordinate(point.value("cp", json::array()), 4, context);
    expectForces(point.value("forces", json::object()), context + " forces");
    for (const auto& component : point.value("components", json::array()))
    {
        expectKeys(component, {"path"}, context);
        expectForces(component, context + " " + component.value("path", "?"));
    }
}

void checkAeroConfiguration(const json& aero, std::size_t index, const std::string& file)
{
    const std::string context       = file + " configuration " + std::to_string(index);
    const auto&       configuration = aero.at("configurations").at(index);
    expectConfigurationHeader(configuration, index, context);
    expectKeys(configuration, {"referenceLength", "referenceArea", "sameResultsAs"}, context);
    const json* results = QtRocket::Test::aeroResults(aero, index);
    ASSERT_NE(results, nullptr) << context;
    expectKeys(*results, {"geometryWarnings", "points", "worstCP"}, context);
    const auto& points = results->value("points", json::array());
    ASSERT_EQ(points.size(), kAeroPointCount) << context;
    for (std::size_t p = 0; p < points.size(); ++p)
    {
        checkAeroPoint(points[p], std::format("{} point {}", context, p));
        // Only the nozzle point has thrusting nozzles.
        if (p != kNozzlePoint)
        {
            EXPECT_TRUE(points[p]
                            .value("conditions", json::object())
                            .value("thrustingNozzleExitAreas", json::array({0}))
                            .empty())
                << context << " point " << p;
        }
    }
    EXPECT_EQ(results->value("worstCP", json::array()).size(), kMachCount) << context;
}

// ---- sim_<sim>.json and the branch CSVs ----

void checkSimulationHeader(const json& simulation, const GoldenSimulation& listed,
                           std::size_t index, const std::string& inputName)
{
    const std::string& context = listed.json;
    expectHeader(simulation, "simulation", inputName, context);
    expectKeys(simulation,
               {"index", "name", "flightConfiguration", "optionsSource", "variant", "options",
                "harness", "extensions", "skipped", "skipReason"},
               context);
    EXPECT_EQ(simulation.value("index", kMissing), index) << context;
    EXPECT_EQ(simulation.value("name", ""), listed.name) << context;
    const auto& options = simulation.value("options", json::object());
    expectKeys(
        options,
        {"launchRodLength", "launchRodAngle", "launchRodDirection", "windModelType", "averageWind",
         "timeStep", "maxSimulationTime", "randomSeed", "gravityModelType", "stepperMethod"},
        context);
    EXPECT_EQ(options.value("randomSeed", -1), 0) << context;
    EXPECT_EQ(options.value("averageWind", json::object()).value("standardDeviation", -1.0), 0.0)
        << context;
}

void checkEvents(const json& branch, const std::string& csvFile)
{
    for (const auto& event : branch.value("events", json::array()))
    {
        expectKeys(event, {"time", "type", "source", "data"}, csvFile);
        expectNumber(event, "time", csvFile);
    }
}

/// The minimum and the maximum of the column @p column of a branch; nullopt when one of them
/// is missing or not a number.
std::optional<std::pair<double, double>> columnRange(const json& column)
{
    const auto minimum = column.find("min");
    const auto maximum = column.find("max");
    if (minimum == column.end() || maximum == column.end())
    {
        return std::nullopt;
    }
    const auto low  = QtRocket::Test::goldenNumber(*minimum);
    const auto high = QtRocket::Test::goldenNumber(*maximum);
    if (!low || !high)
    {
        return std::nullopt;
    }
    return std::pair{*low, *high};
}

/// Whether @p value is in @p range, its ends included (false for NaN).
bool isWithin(double value, const std::pair<double, double>& range)
{
    return value >= range.first && value <= range.second;
}

/// The range of the time column of the branch @p branch; nullopt when it has none.
std::optional<std::pair<double, double>> timeRange(const json& branch)
{
    for (const auto& column : branch.value("columns", json::array()))
    {
        if (column.value("key", "") == "time")
        {
            return columnRange(column);
        }
    }
    return std::nullopt;
}

/// What is wrong with the column @p column of a branch: it has a key, a name, a symbol and the
/// built-in flag, and a minimum that is not above its maximum (both NaN for a column without a
/// number).
std::string columnProblems(const json& column)
{
    const std::string key = column.value("key", "?");
    for (const std::string_view member : {"key", "name", "symbol", "builtin"})
    {
        if (!column.contains(member))
        {
            return std::format("column {}: no \"{}\"\n", key, member);
        }
    }
    const auto range = columnRange(column);
    if (!range)
    {
        return std::format("column {}: no minimum and maximum\n", key);
    }
    const bool ordered =
        std::isnan(range->first) ? std::isnan(range->second) : range->first <= range->second;
    return ordered ? "" : std::format("column {}: its minimum is above its maximum\n", key);
}

/// What is wrong with what the document of a branch says of its time series, which holds
/// whether or not the time series is on file: at least one row, columns with keys of their own
/// (columnProblems()), and a time column that starts at zero in the first branch and has every
/// event of the branch in its range.
std::string branchProblems(const json& branch)
{
    std::string problems;
    if (branch.value("rows", 0) < 1)
    {
        problems += "no rows\n";
    }
    std::set<std::string> keys;
    for (const auto& column : branch.value("columns", json::array()))
    {
        problems += columnProblems(column);
        if (!keys.insert(column.value("key", "")).second)
        {
            problems += std::format("two columns have the key {}\n", column.value("key", ""));
        }
    }
    const auto time = timeRange(branch);
    if (!time)
    {
        return problems + "no time column\n";
    }
    if (branch.value("index", -1) == 0 && time->first != 0.0)
    {
        problems += std::format("the first branch starts at {} s\n", time->first);
    }
    for (const auto& event : branch.value("events", json::array()))
    {
        const auto at = QtRocket::Test::goldenNumber(event.value("time", json()));
        if (!at || !isWithin(*at, *time))
        {
            problems += std::format("the event {} is outside the time of the rows\n",
                                    event.value("type", "?"));
        }
    }
    return problems;
}

/// What the document of a branch holds, with or without its time series.
void checkBranchDocument(const json& branch, const std::string& context)
{
    expectKeys(
        branch,
        {"index", "name", "sourceComponent", "rows", "optimumAltitude", "timeToOptimumAltitude",
         "optimumDelay", "separationTime", "csv", "columns", "events", "excludedColumns"},
        context);
    checkEvents(branch, context);
    EXPECT_EQ(branchProblems(branch), "") << context;
}

/// A branch of a set of documents alone: it names no time series.
void checkBranchWithoutTimeSeries(const json& branch, const std::string& context)
{
    checkBranchDocument(branch, context);
    const auto csv = QtRocket::Test::goldenBranchCsv(branch);
    ASSERT_TRUE(csv.has_value()) << context << ": " << csv.error().toString();
    EXPECT_EQ(csv->value_or(""), "")
        << context << ": names a time series in a set of documents alone";
}

/// The name of the time series of the branch @p branch (its "csv"); "", after a test failure,
/// when it names none: a branch without a time series ("csv": null) is legal only in a set of
/// documents alone.
std::string timeSeriesName(const json& branch, const std::string& context)
{
    const auto csv = QtRocket::Test::goldenBranchCsv(branch);
    if (!csv)
    {
        ADD_FAILURE() << context << ": " << csv.error().toString();
        return "";
    }
    if (!csv->has_value())
    {
        ADD_FAILURE() << context << ": the branch names no time series";
    }
    return csv->value_or("");
}

/// The time series @p csvFile of the branch @p branch: the columns and the number of rows its
/// document gives, in the order of the time.
void checkTimeSeries(const json& branch, const std::string& csvFile)
{
    const auto table = QtRocket::Test::loadGoldenCsv(csvFile);
    ASSERT_TRUE(table.has_value()) << table.error().toString();
    std::vector<std::string> keys;
    for (const auto& column : branch.value("columns", json::array()))
    {
        keys.push_back(column.value("key", ""));
    }
    EXPECT_EQ(table->columns, keys) << csvFile;
    EXPECT_EQ(table->rows.size(), branch.value("rows", kMissing)) << csvFile;
    const auto time = table->column("time").value_or(std::vector<double>{});
    EXPECT_EQ(time.size(), table->rows.size()) << csvFile << ": no time column";
    EXPECT_TRUE(std::ranges::is_sorted(time)) << csvFile;
}

/// A branch with its time series. @p directory: that of the simulation's document, in which
/// "csv" names the time series.
void checkBranch(const json& branch, const std::string& csvFile, const std::string& directory)
{
    checkBranchDocument(branch, csvFile);
    EXPECT_EQ(directory + "/" + timeSeriesName(branch, csvFile), csvFile);
    checkTimeSeries(branch, csvFile);
}

/// The branches @p branches of the document of the simulation @p listed, each with its time
/// series.
void checkBranchesWithTimeSeries(const json& branches, const GoldenSimulation& listed)
{
    ASSERT_EQ(branches.size(), listed.branches.size()) << listed.json;
    const std::string directory = std::filesystem::path{listed.json}.parent_path().generic_string();
    for (std::size_t b = 0; b < branches.size(); ++b)
    {
        checkBranch(branches[b], listed.branches[b], directory);
    }
}

/// The branches @p branches of the document of the simulation @p listed in a set of documents
/// alone: the manifest lists no time series and no branch names one.
void checkBranchesWithoutTimeSeries(const json& branches, const GoldenSimulation& listed)
{
    EXPECT_TRUE(listed.branches.empty()) << listed.json;
    for (std::size_t b = 0; b < branches.size(); ++b)
    {
        checkBranchWithoutTimeSeries(branches[b], std::format("{} branch {}", listed.json, b));
    }
}

/// Checks the files of the simulation @p listed, number @p index of the input @p inputName (of
/// its default-step set or of its stable-step set). @p timeSeries: whether the set has the time
/// series of its branches (the default-step set always has).
void checkSimulation(const GoldenSimulation& listed, std::size_t index,
                     const std::string& inputName, bool timeSeries)
{
    const json simulation = loadJsonOrFail(listed.json);
    checkSimulationHeader(simulation, listed, index, inputName);
    if (simulation.value("skipped", false))
    {
        EXPECT_TRUE(listed.branches.empty()) << listed.json;
        EXPECT_TRUE(simulation.value("skipReason", json()).is_string()) << listed.json;
        return;
    }
    expectKeys(simulation, {"result", "summary", "warnings", "branches"}, listed.json);
    const auto& summary = simulation.value("summary", json::object());
    expectKeys(summary, {"maxAltitude", "maxVelocity", "timeToApogee", "flightTime", "branchCount"},
               listed.json);
    const auto& branches = simulation.value("branches", json::array());
    EXPECT_EQ(summary.value("branchCount", kMissing), branches.size()) << listed.json;
    if (timeSeries)
    {
        checkBranchesWithTimeSeries(branches, listed);
    }
    else
    {
        checkBranchesWithoutTimeSeries(branches, listed);
    }
}

/// Removes the entry @p key of the object @p name of @p simulation, when there is one.
void eraseEntry(json& simulation, std::string_view name, std::string_view key)
{
    const auto object = simulation.find(name);
    if (object != simulation.end() && object->is_object())
    {
        object->erase(key);
    }
}

/// What the harness was given to simulate, read from the document @p simulation: everything
/// but the results, the time step of the options and the two entries of "harness" that record
/// the stable time step.
json settingsOf(json simulation)
{
    for (const std::string_view key : {"result", "summary", "warnings", "branches"})
    {
        simulation.erase(key);
    }
    eraseEntry(simulation, "options", "timeStep");
    eraseEntry(simulation, "harness", "documentTimeStep");
    eraseEntry(simulation, "harness", "timeStep");
    return simulation;
}

/// The number @p key of the object @p name of @p simulation; NaN when there is none.
double numberOf(const json& simulation, std::string_view name, std::string_view key)
{
    const auto object = simulation.find(name);
    if (object == simulation.end() || !object->is_object() || !object->contains(key))
    {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return QtRocket::Test::goldenNumber(object->at(key))
        .value_or(std::numeric_limits<double>::quiet_NaN());
}

/// Simulation @p index of the stable-step set of @p input is its simulation of the default-step
/// set with the time step @p stableTimeStep and nothing else changed: the same flight
/// configuration (with the same id), the same options otherwise, the same changes of the
/// harness; "harness" records the stable time step and the one the simulation had.
void checkStableSimulation(const GoldenInput& input, std::size_t index, double stableTimeStep)
{
    const std::string& file   = input.stableSimulations[index].json;
    const json         stable = loadJsonOrFail(file);
    const json         twin   = loadJsonOrFail(input.simulations[index].json);
    EXPECT_EQ(numberOf(stable, "options", "timeStep"), stableTimeStep) << file;
    EXPECT_EQ(numberOf(stable, "harness", "timeStep"), stableTimeStep) << file;
    EXPECT_EQ(numberOf(stable, "harness", "documentTimeStep"),
              numberOf(twin, "options", "timeStep"))
        << file;
    EXPECT_TRUE(std::isnan(numberOf(twin, "harness", "timeStep")))
        << file << ": the default-step set records no stable time step";
    EXPECT_EQ(settingsOf(stable), settingsOf(twin)) << file;
}

/// What the simulation documents of a stable-step set hold.
struct DocumentCounts
{
    std::size_t simulations{0};
    std::size_t branches{0};
    std::size_t events{0};
    std::size_t columns{0};
    std::size_t warnings{0};
    std::size_t rows{0};
    /// The simulations by the time step they had ("harness.documentTimeStep"), in s; -1 for a
    /// document that does not say.
    std::map<double, std::size_t> documentTimeSteps;
    /// The inputs with a simulation whose time step was not that of an installation, 0.05 s.
    std::set<std::string> inputsWithATimeStepOfTheirOwn;
};

std::string toText(const DocumentCounts& counts)
{
    std::string text =
        std::format("{} simulations, {} branches, {} events, {} columns, {} warnings, {} rows;",
                    counts.simulations, counts.branches, counts.events, counts.columns,
                    counts.warnings, counts.rows);
    for (const auto& [timeStep, simulations] : counts.documentTimeSteps)
    {
        text += std::format(" {} with a time step of {} s", simulations, timeStep);
    }
    for (const std::string& input : counts.inputsWithATimeStepOfTheirOwn)
    {
        text += std::format(" ({})", input);
    }
    return text;
}

/// Adds what the stable-step document @p document of the input @p inputName holds to @p counts.
void countDocument(const json& document, const std::string& inputName, DocumentCounts& counts)
{
    constexpr double kInstallationTimeStep = 0.05;
    ++counts.simulations;
    counts.warnings += document.value("warnings", json::array()).size();
    // NaN is no key of a map: a document without the entry is counted under -1.
    const double recorded = numberOf(document, "harness", "documentTimeStep");
    const double timeStep = std::isnan(recorded) ? -1.0 : recorded;
    ++counts.documentTimeSteps[timeStep];
    if (timeStep != kInstallationTimeStep)
    {
        counts.inputsWithATimeStepOfTheirOwn.insert(inputName);
    }
    for (const auto& branch : document.value("branches", json::array()))
    {
        ++counts.branches;
        counts.events += branch.value("events", json::array()).size();
        counts.columns += branch.value("columns", json::array()).size();
        counts.rows += branch.value("rows", std::size_t{0});
    }
}

/// What the documents of the stable-step sets of the inputs of the kind @p kind hold.
DocumentCounts stableDocumentCounts(const GoldenManifest& manifest, std::string_view kind)
{
    DocumentCounts counts;
    for (const auto& input : manifest.inputs)
    {
        if (input.kind != kind)
        {
            continue;
        }
        for (const auto& simulation : input.stableSimulations)
        {
            countDocument(loadJsonOrFail(simulation.json), input.name, counts);
        }
    }
    return counts;
}

// ---------------------------------------------------------------------------------------------
// The loader

TEST(GoldenData, GoldenNumberAcceptsNumbersAndNonFiniteStrings)
{
    EXPECT_EQ(QtRocket::Test::goldenNumber(json::parse("1.0E-5")), 1.0e-5);
    EXPECT_EQ(QtRocket::Test::goldenNumber(json::parse("-0.0")), -0.0);
    EXPECT_EQ(QtRocket::Test::goldenNumber(json(3)), 3.0);
    EXPECT_EQ(QtRocket::Test::goldenNumber(json::parse("0.30000000000000004")),
              0.30000000000000004);
    EXPECT_TRUE(std::isnan(QtRocket::Test::goldenNumber(json("NaN")).value_or(0.0)));
    EXPECT_EQ(QtRocket::Test::goldenNumber(json("Infinity")),
              std::numeric_limits<double>::infinity());
    EXPECT_EQ(QtRocket::Test::goldenNumber(json("-Infinity")),
              -std::numeric_limits<double>::infinity());
}

TEST(GoldenData, GoldenNumberRejectsOtherValues)
{
    EXPECT_FALSE(QtRocket::Test::goldenNumber(json("nan")).has_value());
    EXPECT_FALSE(QtRocket::Test::goldenNumber(json("1.5")).has_value());
    EXPECT_FALSE(QtRocket::Test::goldenNumber(json(nullptr)).has_value());
    EXPECT_FALSE(QtRocket::Test::goldenNumber(json(true)).has_value());
    EXPECT_FALSE(QtRocket::Test::goldenNumber(json::array({1.0})).has_value());
}

TEST(GoldenData, GoldenCheckCollectsMismatches)
{
    const json golden = json::parse(
        R"({"a": 1.0, "b": [1, 2, 3, 4], "c": [[0, 0, 0]], "s": "x", "t": true, "n": 3,
            "d": {"e": "NaN"}})");
    QtRocket::Test::GoldenCheck check{golden};
    check.number("/a", 1.0 + 1e-15);  // within the relative 1e-12
    check.coordinate("/b", QtRocket::Coordinate{1, 2, 3, 4});
    check.coordinates("/c", std::vector{QtRocket::Coordinate{0, 0, 0}});
    check.string("/s", "x");
    check.boolean("/t", true);
    check.integer("/n", 3);
    check.number("/d/e", std::numeric_limits<double>::quiet_NaN());
    EXPECT_EQ(check.failures(), std::vector<std::string>{});

    check.number("/a", 1.001);
    check.number("/missing", 0);
    check.coordinate("/b", QtRocket::Coordinate{1, 2, 3, 5});
    check.coordinates("/c", {});
    check.string("/s", "y");
    check.boolean("/t", false);
    check.integer("/n", 4);
    check.number("/s", 1);  // not a number
    ASSERT_EQ(check.failures().size(), 8U);
    EXPECT_EQ(check.failures().front(), "/a: 1.0009999999999999 differs from the golden 1.0");
    EXPECT_EQ(check.failures()[1], "/missing: 0 differs from the golden (none)");
    EXPECT_EQ(check.failures()[4], "/s: \"y\" differs from the golden \"x\"");
}

TEST(GoldenData, GoldenCheckComparesListsOfNumbers)
{
    const json                  golden = json::parse(R"({"v": [0.0, 1.5, "NaN"], "s": "x"})");
    QtRocket::Test::GoldenCheck check{golden};
    check.numbers("/v", std::vector{0.0, 1.5, std::numeric_limits<double>::quiet_NaN()});
    EXPECT_EQ(check.failures(), std::vector<std::string>{});

    check.numbers("/v", std::vector{0.0, 1.5});
    check.numbers("/v", std::vector{0.0, 1.0, 0.0});
    check.numbers("/s", std::vector{1.0});
    ASSERT_EQ(check.failures().size(), 4U);
    EXPECT_EQ(check.failures()[0], R"(/v: 2 numbers differs from the golden [0.0,1.5,"NaN"])");
    EXPECT_EQ(check.failures()[1], "/v/1: 1 differs from the golden 1.5");
    EXPECT_EQ(check.failures()[2], R"(/v/2: 0 differs from the golden "NaN")");
    EXPECT_EQ(check.failures()[3], R"(/s: 1 numbers differs from the golden "x")");
}

TEST(GoldenData, GoldenGeometryComponentFindsTheEntry)
{
    const auto tube =
        QtRocket::Test::goldenGeometryComponent("testrocket-estes-alpha-iii", "/0/1/2");
    ASSERT_TRUE(tube.has_value()) << tube.error().toString();
    EXPECT_EQ((*tube)->at("name").get<std::string>(), "Motor Mount Tube");
    // The second lookup reads the cached file: the same entry.
    const auto again =
        QtRocket::Test::goldenGeometryComponent("testrocket-estes-alpha-iii", "/0/1/2");
    ASSERT_TRUE(again.has_value());
    EXPECT_EQ(*again, *tube);

    const auto missing =
        QtRocket::Test::goldenGeometryComponent("testrocket-estes-alpha-iii", "/9");
    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error().code, QtRocket::ErrorCode::NOT_FOUND);
    EXPECT_EQ(missing.error().message,
              "testrocket-estes-alpha-iii/geometry.json has no component /9");
    const auto noInput = QtRocket::Test::goldenGeometryComponent("no-such-input", "/0");
    ASSERT_FALSE(noInput.has_value());
    EXPECT_EQ(noInput.error().code, QtRocket::ErrorCode::IO);

    // The reporting variant: the entry, or a test failure and an empty object.
    EXPECT_EQ(
        &QtRocket::Test::goldenGeometryComponentOrFail("testrocket-estes-alpha-iii", "/0/1/2"),
        *tube);
    EXPECT_NONFATAL_FAILURE(EXPECT_TRUE(QtRocket::Test::goldenGeometryComponentOrFail(
                                            "testrocket-estes-alpha-iii", "/9")
                                            .empty()),
                            "has no component /9");
}

TEST(GoldenData, GoldenCheckMatchesInfinitiesExactly)
{
    // An infinity would make the relative tolerance infinite: it matches the same infinity only.
    const json       golden = json::parse(R"({"a": 1.0, "inf": "Infinity", "minf": "-Infinity"})");
    constexpr double kInf   = std::numeric_limits<double>::infinity();
    QtRocket::Test::GoldenCheck check{golden};
    check.number("/inf", kInf);
    check.number("/minf", -kInf);
    EXPECT_EQ(check.failures(), std::vector<std::string>{});

    check.number("/a", kInf);
    check.number("/a", -kInf);
    check.number("/inf", 1.0);
    check.number("/minf", kInf);
    check.number("/inf", std::numeric_limits<double>::max());
    ASSERT_EQ(check.failures().size(), 5U);
    EXPECT_EQ(check.failures().front(), "/a: inf differs from the golden 1.0");
    EXPECT_EQ(check.failures()[2], "/inf: 1 differs from the golden \"Infinity\"");
    EXPECT_EQ(check.failures()[3], "/minf: inf differs from the golden \"-Infinity\"");
}

/// The report of GoldenMismatches::relative() for @p actual against the golden @p expected; ""
/// when they match.
[[nodiscard]] std::string relativeReport(double expected, double actual)
{
    QtRocket::Test::GoldenMismatches m("value");
    m.relative("x", expected, actual);
    return m.report();
}

TEST(GoldenData, GoldenMismatchesRelativeMatchesInfinitiesExactly)
{
    // The ejection delay of a plugged motor is +infinity in the golden file and in the rocket.
    // The difference of two equal infinities is NaN, and an infinite scale would admit any
    // difference: an infinity matches the same infinity only, as in GoldenMismatches::within().
    constexpr double kInf = std::numeric_limits<double>::infinity();
    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(relativeReport(kInf, kInf), "");
    EXPECT_EQ(relativeReport(-kInf, -kInf), "");
    EXPECT_EQ(relativeReport(kInf, -kInf),
              "value:\n  x: expected inf, got -inf (difference -inf)\n");
    EXPECT_EQ(relativeReport(kInf, 3.0), "value:\n  x: expected inf, got 3 (difference -inf)\n");
    EXPECT_EQ(relativeReport(3.0, kInf), "value:\n  x: expected 3, got inf (difference inf)\n");
    EXPECT_NE(relativeReport(-kInf, std::numeric_limits<double>::lowest()), "");
    EXPECT_NE(relativeReport(kInf, kNaN), "");
    // The finite numbers, zero and NaN, as before: within 1e-9 of the larger magnitude.
    EXPECT_EQ(relativeReport(1.0, 1.0 + 5e-10), "");
    EXPECT_NE(relativeReport(1.0, 1.0 + 2e-9), "");
    EXPECT_EQ(relativeReport(0.0, 0.0), "");
    EXPECT_EQ(relativeReport(0.0, -0.0), "");
    EXPECT_NE(relativeReport(0.0, 1e-300), "") << "a golden 0 is matched exactly";
    EXPECT_EQ(relativeReport(kNaN, kNaN), "");
    EXPECT_NE(relativeReport(kNaN, 1.0), "");
    EXPECT_NE(relativeReport(1.0, kNaN), "");
}

TEST(GoldenData, ParsesCsv)
{
    const auto table = QtRocket::Test::parseGoldenCsv(
        "time,altitude,custom:Control fin cant\n0.0,0.0,NaN\n0.01,1.0E-5,-Infinity\n");
    ASSERT_TRUE(table.has_value()) << table.error().toString();
    EXPECT_EQ(table->columns,
              (std::vector<std::string>{"time", "altitude", "custom:Control fin cant"}));
    ASSERT_EQ(table->rows.size(), 2U);
    EXPECT_EQ(table->rows[1][0], 0.01);
    EXPECT_EQ(table->rows[1][1], 1.0e-5);
    EXPECT_TRUE(std::isnan(table->rows[0][2]));
    EXPECT_EQ(table->rows[1][2], -std::numeric_limits<double>::infinity());

    EXPECT_EQ(table->columnIndex("altitude"), 1U);
    EXPECT_FALSE(table->columnIndex("mass").has_value());
    EXPECT_EQ(table->column("time"), (std::vector<double>{0.0, 0.01}));
    EXPECT_FALSE(table->column("mass").has_value());
}

TEST(GoldenData, CsvWithOnlyAHeaderHasNoRows)
{
    const auto table = QtRocket::Test::parseGoldenCsv("time,altitude\n");
    ASSERT_TRUE(table.has_value()) << table.error().toString();
    EXPECT_EQ(table->columns.size(), 2U);
    EXPECT_TRUE(table->rows.empty());
    // The column exists and is empty (the default would have one value).
    EXPECT_EQ(table->column("time").value_or(std::vector<double>{-1.0}).size(), 0U);
}

TEST(GoldenData, RejectsMalformedCsv)
{
    for (const std::string_view text :
         {std::string_view{""}, std::string_view{"time,altitude"},
          std::string_view{"time,altitude\n0.0\n"},
          std::string_view{"time,altitude\n0.0,1.0,2.0\n"},
          std::string_view{"time,altitude\n0.0,one\n"}, std::string_view{"time,altitude\n0.0,\n"},
          std::string_view{"time,altitude\n0.0,1.0\n1.0,2.0"}})
    {
        const auto table = QtRocket::Test::parseGoldenCsv(text);
        EXPECT_FALSE(table.has_value()) << "accepted: " << text;
        EXPECT_EQ(errorCode(table), ErrorCode::PARSE) << text;
    }
}

TEST(GoldenData, LoadsGzipCompressedCsv)
{
    const TempDir dir;
    const auto    compressed =
        QtRocket::gzipDeflate(QtRocket::stringToBytes("time,mass\n0.0,1.5\n0.5,1.25\n"));
    ASSERT_TRUE(compressed.has_value()) << compressed.error().toString();
    const auto file = dir.path() / "good.csv.gz";
    ASSERT_TRUE(QtRocket::writeFile(file, *compressed).has_value());
    const auto table = QtRocket::Test::loadGoldenCsv(file);
    ASSERT_TRUE(table.has_value()) << table.error().toString();
    EXPECT_EQ(table->column("mass"), (std::vector<double>{1.5, 1.25}));
}

TEST(GoldenData, ReportsUnreadableCsvFiles)
{
    const TempDir dir;
    const auto    notGzip = dir.path() / "plain.csv.gz";
    ASSERT_TRUE(QtRocket::writeTextFile(notGzip, "time\n0.0\n").has_value());
    EXPECT_EQ(errorCode(QtRocket::Test::loadGoldenCsv(notGzip)), ErrorCode::PARSE);

    const auto ragged = QtRocket::gzipDeflate(QtRocket::stringToBytes("time\n0.0,1.0\n"));
    ASSERT_TRUE(ragged.has_value()) << ragged.error().toString();
    const auto raggedFile = dir.path() / "ragged.csv.gz";
    ASSERT_TRUE(QtRocket::writeFile(raggedFile, *ragged).has_value());
    EXPECT_EQ(errorCode(QtRocket::Test::loadGoldenCsv(raggedFile)), ErrorCode::PARSE);

    EXPECT_EQ(errorCode(QtRocket::Test::loadGoldenCsv(dir.path() / "missing.csv.gz")),
              ErrorCode::IO);
}

/// Writes the first @p kept bytes of @p compressed to a file in @p dir and expects the loader to
/// reject it as a PARSE error.
void expectTruncatedCsvRejected(const TempDir& dir, const std::vector<std::byte>& compressed,
                                std::size_t kept)
{
    const std::vector<std::byte> truncated(compressed.begin(),
                                           compressed.begin() + static_cast<std::ptrdiff_t>(kept));
    const auto                   file = dir.path() / std::format("truncated_{}.csv.gz", kept);
    ASSERT_TRUE(QtRocket::writeFile(file, truncated).has_value());
    const auto table = QtRocket::Test::loadGoldenCsv(file);
    EXPECT_FALSE(table.has_value()) << "accepted a stream cut to " << kept << " bytes";
    EXPECT_EQ(errorCode(table), ErrorCode::PARSE) << kept;
}

TEST(GoldenData, ReportsTruncatedGzipCsvFiles)
{
    const TempDir dir;
    const auto    compressed =
        QtRocket::gzipDeflate(QtRocket::stringToBytes("time,mass\n0.0,1.5\n0.5,1.25\n"));
    ASSERT_TRUE(compressed.has_value()) << compressed.error().toString();
    ASSERT_GT(compressed->size(), 10U);
    // Cut inside the deflate data and just before the trailer (CRC-32 and size).
    expectTruncatedCsvRejected(dir, *compressed, compressed->size() / 2);
    expectTruncatedCsvRejected(dir, *compressed, compressed->size() - 4);
}

TEST(GoldenData, ParsesJson)
{
    const auto parsed = QtRocket::Test::parseGoldenJson(R"({"a": [1.0, "NaN"]})");
    ASSERT_TRUE(parsed.has_value()) << parsed.error().toString();
    EXPECT_EQ(parsed->at("a").size(), 2U);
}

TEST(GoldenData, RejectsInvalidJson)
{
    for (const std::string_view text :
         {std::string_view{""}, std::string_view{"{"}, std::string_view{R"({"a": NaN})"}})
    {
        EXPECT_EQ(errorCode(QtRocket::Test::parseGoldenJson(text)), ErrorCode::PARSE) << text;
    }
    EXPECT_EQ(errorCode(QtRocket::Test::loadGoldenJson("no-such-input/geometry.json")),
              ErrorCode::IO);
}

TEST(GoldenData, ParsesManifest)
{
    const json manifest = json::parse(R"({
        "schemaVersion": 1,
        "openrocket": {"commit": "abc", "version": "26.xx"},
        "settings": {"stableTimeStep": 0.01, "stableSimulationsOf": ["testrocket"],
                     "stableTimeSeriesOf": ["testrocket"]},
        "inputs": [{"name": "example-x", "kind": "example", "source": "data/examples/X.ork",
                    "sourceSha256": "0123abcd",
                    "geometry": "example-x/geometry.json", "mass": "example-x/mass.json",
                    "aero": "example-x/aero.json", "resave": "example-x/resave/rocket.ork",
                    "simulations": [{"name": "Simulation 1", "json": "example-x/sim_00.json",
                                     "branches": ["example-x/sim_00_branch0.csv.gz"]}]},
                   {"name": "testrocket-y", "kind": "testrocket", "source": "makeY",
                    "sourceSha256": null,
                    "geometry": "g", "mass": "m", "aero": "a", "resave": "r",
                    "simulations": [{"name": "[A8-3]", "json": "testrocket-y/sim_00.json",
                                     "branches": []}],
                    "stableSimulations": [
                        {"name": "[A8-3]", "json": "testrocket-y/stable/sim_00.json",
                         "branches": ["testrocket-y/stable/sim_00_branch0.csv.gz"]}]}]})");
    const auto parsed   = QtRocket::Test::parseGoldenManifest(manifest);
    ASSERT_TRUE(parsed.has_value()) << parsed.error().toString();
    EXPECT_EQ(parsed->schemaVersion, 1);
    EXPECT_EQ(parsed->openrocketCommit, "abc");
    EXPECT_EQ(parsed->openrocketVersion, "26.xx");
    EXPECT_EQ(parsed->stableTimeStep, 0.01);
    EXPECT_EQ(parsed->stableSimulationsOf, std::vector<std::string>{"testrocket"});
    EXPECT_EQ(parsed->stableTimeSeriesOf, std::vector<std::string>{"testrocket"});
    EXPECT_EQ(parsed->uuidSalt, "") << "no \"uuidSalt\": not a dump with other component ids";
    EXPECT_EQ(parsed->lastBitPerturbation, "")
        << "no \"lastBitPerturbation\": not a dump of perturbed runs";
    ASSERT_EQ(parsed->inputs.size(), 2U);
    const auto* input = parsed->find("example-x");
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(input->kind, "example");
    EXPECT_EQ(input->sourceSha256, "0123abcd");
    EXPECT_EQ(input->resave, "example-x/resave/rocket.ork");
    ASSERT_EQ(input->simulations.size(), 1U);
    EXPECT_EQ(input->simulations[0].branches,
              std::vector<std::string>{"example-x/sim_00_branch0.csv.gz"});
    // An input without a stable-step set has no "stableSimulations".
    EXPECT_TRUE(input->stableSimulations.empty());
    EXPECT_FALSE(parsed->hasStableSimulations(*input));
    EXPECT_FALSE(parsed->hasStableTimeSeries(*input));
    EXPECT_EQ(parsed->find("example-y"), nullptr);

    const auto* testRocket = parsed->find("testrocket-y");
    ASSERT_NE(testRocket, nullptr);
    EXPECT_EQ(testRocket->sourceSha256, "") << "null: not made from a file";
    EXPECT_TRUE(parsed->hasStableSimulations(*testRocket));
    EXPECT_TRUE(parsed->hasStableTimeSeries(*testRocket));
    ASSERT_EQ(testRocket->stableSimulations.size(), 1U);
    EXPECT_EQ(testRocket->stableSimulations[0].name, "[A8-3]");
    EXPECT_EQ(testRocket->stableSimulations[0].json, "testrocket-y/stable/sim_00.json");
    EXPECT_EQ(testRocket->stableSimulations[0].branches,
              std::vector<std::string>{"testrocket-y/stable/sim_00_branch0.csv.gz"});

    // A dump made with UUID_SALT records the salt of its component ids.
    json salted                    = manifest;
    salted["settings"]["uuidSalt"] = "salt-b";
    const auto dump                = QtRocket::Test::parseGoldenManifest(salted);
    ASSERT_TRUE(dump.has_value()) << dump.error().toString();
    EXPECT_EQ(dump->uuidSalt, "salt-b");
    EXPECT_EQ(dump->lastBitPerturbation, "");

    // A dump made with LAST_BIT records the pattern of the perturbation of its runs.
    json perturbed                               = manifest;
    perturbed["settings"]["lastBitPerturbation"] = "all-random-a";
    const auto perturbedDump                     = QtRocket::Test::parseGoldenManifest(perturbed);
    ASSERT_TRUE(perturbedDump.has_value()) << perturbedDump.error().toString();
    EXPECT_EQ(perturbedDump->lastBitPerturbation, "all-random-a");
    EXPECT_EQ(perturbedDump->uuidSalt, "");
}

// A stable-step set that is the documents alone: the kind of its inputs is one of
// "stableSimulationsOf" and not one of "stableTimeSeriesOf", and its simulations list no branch
// file.
TEST(GoldenData, ParsesManifestWithAStableStepSetWithoutTimeSeries)
{
    const json manifest = json::parse(R"({
        "schemaVersion": 1,
        "openrocket": {"commit": "abc", "version": "26.xx"},
        "settings": {"stableTimeStep": 0.01, "stableSimulationsOf": ["example", "testrocket"],
                     "stableTimeSeriesOf": ["testrocket"]},
        "inputs": [{"name": "example-x", "kind": "example", "source": "data/examples/X.ork",
                    "geometry": "g", "mass": "m", "aero": "a", "resave": "r",
                    "simulations": [{"name": "Simulation 1", "json": "example-x/sim_00.json",
                                     "branches": ["example-x/sim_00_branch0.csv.gz"]}],
                    "stableSimulations": [
                        {"name": "Simulation 1", "json": "example-x/stable/sim_00.json",
                         "branches": []}]},
                   {"name": "testrocket-y", "kind": "testrocket", "source": "makeY",
                    "geometry": "g", "mass": "m", "aero": "a", "resave": "r",
                    "simulations": [{"name": "[A8-3]", "json": "testrocket-y/sim_00.json",
                                     "branches": ["testrocket-y/sim_00_branch0.csv.gz"]}],
                    "stableSimulations": [
                        {"name": "[A8-3]", "json": "testrocket-y/stable/sim_00.json",
                         "branches": ["testrocket-y/stable/sim_00_branch0.csv.gz"]}]}]})");
    const auto parsed   = QtRocket::Test::parseGoldenManifest(manifest);
    ASSERT_TRUE(parsed.has_value()) << parsed.error().toString();
    EXPECT_EQ(parsed->stableSimulationsOf, (std::vector<std::string>{"example", "testrocket"}));
    EXPECT_EQ(parsed->stableTimeSeriesOf, std::vector<std::string>{"testrocket"});
    const auto* example = parsed->find("example-x");
    ASSERT_NE(example, nullptr);
    EXPECT_TRUE(parsed->hasStableSimulations(*example));
    EXPECT_FALSE(parsed->hasStableTimeSeries(*example));
    ASSERT_EQ(example->stableSimulations.size(), 1U);
    EXPECT_EQ(example->stableSimulations[0].json, "example-x/stable/sim_00.json");
    EXPECT_TRUE(example->stableSimulations[0].branches.empty());
    const auto* testRocket = parsed->find("testrocket-y");
    ASSERT_NE(testRocket, nullptr);
    EXPECT_TRUE(parsed->hasStableSimulations(*testRocket));
    EXPECT_TRUE(parsed->hasStableTimeSeries(*testRocket));
}

/// The message of the failure of goldenBranchCsv() for @p branch; "" when it succeeds.
[[nodiscard]] std::string branchCsvFailure(const json& branch)
{
    const auto csv = QtRocket::Test::goldenBranchCsv(branch);
    return csv.has_value() ? "" : csv.error().message;
}

TEST(GoldenData, GoldenBranchCsvIsTheFileNameOrNothing)
{
    // A branch with its time series names the file, which is in the document's directory.
    const auto named =
        QtRocket::Test::goldenBranchCsv(json::parse(R"({"csv": "sim_00_branch0.csv.gz"})"));
    ASSERT_TRUE(named.has_value()) << named.error().toString();
    EXPECT_EQ(*named, std::optional<std::string>{"sim_00_branch0.csv.gz"});
    // A document without time series: "csv" is null, the rest of the branch is as ever.
    const auto none = QtRocket::Test::goldenBranchCsv(json::parse(R"({"rows": 535, "csv": null})"));
    ASSERT_TRUE(none.has_value()) << none.error().toString();
    EXPECT_FALSE(none->has_value());

    EXPECT_EQ(branchCsvFailure(json::parse(R"({"rows": 535})")), R"(a branch has no "csv")");
    EXPECT_EQ(branchCsvFailure(json::parse(R"({"csv": 1})")),
              R"("csv" of a branch is neither a file name nor null)");
    EXPECT_EQ(branchCsvFailure(json::parse(R"({"csv": false})")),
              R"("csv" of a branch is neither a file name nor null)");
    EXPECT_EQ(branchCsvFailure(json::array()), "a branch is not an object");
    EXPECT_EQ(errorCode(QtRocket::Test::goldenBranchCsv(json::array())), ErrorCode::PARSE);
    EXPECT_EQ(errorCode(QtRocket::Test::goldenBranchCsv(json::object())), ErrorCode::PARSE);
}

const json& completeManifest()
{
    static const json kManifest = json::parse(R"({
        "schemaVersion": 1,
        "openrocket": {"commit": "abc", "version": "26.xx"},
        "settings": {"stableTimeStep": 0.01, "stableSimulationsOf": ["example"],
                     "stableTimeSeriesOf": ["example"]},
        "inputs": [{"name": "n", "kind": "example", "source": "s", "geometry": "g", "mass": "m",
                    "aero": "a", "resave": "r",
                    "simulations": [{"name": "s", "json": "j", "branches": []}],
                    "stableSimulations": [{"name": "s", "json": "k", "branches": []}]}]})");
    return kManifest;
}

TEST(GoldenData, RejectsManifestsWithoutARequiredField)
{
    ASSERT_TRUE(manifestParses(completeManifest()));
    for (const char* pointer : {"/schemaVersion",
                                "/openrocket",
                                "/openrocket/commit",
                                "/openrocket/version",
                                "/settings",
                                "/settings/stableTimeStep",
                                "/settings/stableSimulationsOf",
                                "/settings/stableTimeSeriesOf",
                                "/inputs",
                                "/inputs/0/name",
                                "/inputs/0/kind",
                                "/inputs/0/source",
                                "/inputs/0/geometry",
                                "/inputs/0/mass",
                                "/inputs/0/aero",
                                "/inputs/0/resave",
                                "/inputs/0/simulations",
                                "/inputs/0/simulations/0/name",
                                "/inputs/0/simulations/0/json",
                                "/inputs/0/simulations/0/branches",
                                "/inputs/0/stableSimulations/0/name",
                                "/inputs/0/stableSimulations/0/json",
                                "/inputs/0/stableSimulations/0/branches"})
    {
        const json::json_pointer removed{pointer};
        json                     broken = completeManifest();
        broken[removed.parent_pointer()].erase(removed.back());
        EXPECT_FALSE(manifestParses(broken)) << "accepted without " << pointer;
    }
    // The list of the stable-step set itself may be missing: the input has none then.
    json withoutStableSet = completeManifest();
    withoutStableSet["inputs"][0].erase("stableSimulations");
    EXPECT_TRUE(manifestParses(withoutStableSet));
}

TEST(GoldenData, RejectsManifestsWithWrongTypes)
{
    json wrongVersion             = completeManifest();
    wrongVersion["schemaVersion"] = "1";
    EXPECT_FALSE(manifestParses(wrongVersion));

    json badBranch                                       = completeManifest();
    badBranch["inputs"][0]["simulations"][0]["branches"] = json::array({1});
    EXPECT_FALSE(manifestParses(badBranch));

    json badInput         = completeManifest();
    badInput["inputs"][0] = "example";
    EXPECT_FALSE(manifestParses(badInput));

    json badSimulation                        = completeManifest();
    badSimulation["inputs"][0]["simulations"] = json::array({42});
    EXPECT_FALSE(manifestParses(badSimulation));

    json badStableSimulation                              = completeManifest();
    badStableSimulation["inputs"][0]["stableSimulations"] = json::array({42});
    EXPECT_FALSE(manifestParses(badStableSimulation));

    json badStableList                              = completeManifest();
    badStableList["inputs"][0]["stableSimulations"] = "none";
    EXPECT_FALSE(manifestParses(badStableList));

    json badTimeStep                          = completeManifest();
    badTimeStep["settings"]["stableTimeStep"] = "0.01";
    EXPECT_FALSE(manifestParses(badTimeStep));

    json badKinds                               = completeManifest();
    badKinds["settings"]["stableSimulationsOf"] = json::array({1});
    EXPECT_FALSE(manifestParses(badKinds));

    json badTimeSeriesKinds                              = completeManifest();
    badTimeSeriesKinds["settings"]["stableTimeSeriesOf"] = json::array({1});
    EXPECT_FALSE(manifestParses(badTimeSeriesKinds));

    json badTimeSeriesList                              = completeManifest();
    badTimeSeriesList["settings"]["stableTimeSeriesOf"] = "example";
    EXPECT_FALSE(manifestParses(badTimeSeriesList));

    json badSalt                    = completeManifest();
    badSalt["settings"]["uuidSalt"] = 1;
    EXPECT_FALSE(manifestParses(badSalt));

    json badPattern                               = completeManifest();
    badPattern["settings"]["lastBitPerturbation"] = true;
    EXPECT_FALSE(manifestParses(badPattern));

    json badHash                         = completeManifest();
    badHash["inputs"][0]["sourceSha256"] = 5;
    EXPECT_FALSE(manifestParses(badHash));

    json badSettings        = completeManifest();
    badSettings["settings"] = json::array();
    EXPECT_FALSE(manifestParses(badSettings));

    EXPECT_FALSE(manifestParses(json::array()));
    EXPECT_EQ(errorCode(QtRocket::Test::parseGoldenManifest(json::array())), ErrorCode::PARSE);
}

// Time series without a stable-step set are a contradiction: a kind of "stableTimeSeriesOf" is
// one of "stableSimulationsOf". A set without time series is fine (the documents alone), and
// so is no set at all.
TEST(GoldenData, RejectsManifestsWhoseTimeSeriesHaveNoStableStepSet)
{
    json contradiction                               = completeManifest();
    contradiction["settings"]["stableSimulationsOf"] = json::array({"testrocket"});
    const auto parsed = QtRocket::Test::parseGoldenManifest(contradiction);
    ASSERT_FALSE(parsed.has_value());
    EXPECT_EQ(parsed.error().code, ErrorCode::PARSE);
    EXPECT_EQ(parsed.error().message,
              R"(manifest.json settings: "stableTimeSeriesOf" names the kind "example", which )"
              R"("stableSimulationsOf" does not)");

    json documentsAlone                              = completeManifest();
    documentsAlone["settings"]["stableTimeSeriesOf"] = json::array();
    EXPECT_TRUE(manifestParses(documentsAlone));

    json noSet                               = completeManifest();
    noSet["settings"]["stableSimulationsOf"] = json::array();
    noSet["settings"]["stableTimeSeriesOf"]  = json::array();
    EXPECT_TRUE(manifestParses(noSet));
}

TEST(GoldenData, AeroResultsFollowSameResultsAs)
{
    const json  aero           = json::parse(R"({"configurations": [
        {"index": 0, "sameResultsAs": null, "points": [0]},
        {"index": 1, "sameResultsAs": 0},
        {"index": 2, "sameResultsAs": 2},
        {"index": 3, "sameResultsAs": 1},
        {"index": 4},
        {"index": 5, "sameResultsAs": -1}]})");
    const auto& configurations = aero.at("configurations");
    EXPECT_EQ(QtRocket::Test::aeroResults(aero, 0), &configurations[0]);
    EXPECT_EQ(QtRocket::Test::aeroResults(aero, 1), &configurations[0]);
    EXPECT_EQ(QtRocket::Test::aeroResults(aero, 2), nullptr);  // refers to itself
    EXPECT_EQ(QtRocket::Test::aeroResults(aero, 3), nullptr);  // refers to a reference
    EXPECT_EQ(QtRocket::Test::aeroResults(aero, 4), nullptr);  // no sameResultsAs
    EXPECT_EQ(QtRocket::Test::aeroResults(aero, 5), nullptr);  // negative
    EXPECT_EQ(QtRocket::Test::aeroResults(aero, 6), nullptr);  // out of range
    EXPECT_EQ(QtRocket::Test::aeroResults(json::array(), 0), nullptr);
}

// ---------------------------------------------------------------------------------------------
// The committed golden data

TEST(GoldenSchema, ManifestDescribesItsSource)
{
    const auto manifest = loadManifestOrFail();
    EXPECT_EQ(manifest.schemaVersion, QtRocket::Test::kGoldenSchemaVersion);
    EXPECT_EQ(manifest.openrocketCommit.size(), 40U);
    EXPECT_TRUE(isLowerHex(manifest.openrocketCommit)) << manifest.openrocketCommit;
    EXPECT_FALSE(manifest.openrocketVersion.empty());
    // The committed data is the harness's own run: a dump made with UUID_SALT (other component
    // ids, to measure how reproducible OpenRocket's results are) records its salt and is never
    // committed. Every other test would accept most of such a dump.
    EXPECT_EQ(manifest.uuidSalt, "") << "the goldens are a dump with other component ids";
    // Nor is a dump whose runs were perturbed in the last bit (LAST_BIT), which measures the same
    // for the designs that are loaded from files.
    EXPECT_EQ(manifest.lastBitPerturbation, "") << "the goldens are a dump of perturbed runs";
}

TEST(GoldenSchema, ManifestListsEveryInput)
{
    const auto            manifest = loadManifestOrFail();
    std::set<std::string> names;
    std::size_t           examples    = 0;
    std::size_t           testRockets = 0;
    for (const auto& input : manifest.inputs)
    {
        EXPECT_TRUE(names.insert(input.name).second) << "duplicate input " << input.name;
        checkManifestInput(input, examples, testRockets);
    }
    EXPECT_EQ(examples, kExampleCount);
    EXPECT_EQ(testRockets, kTestRocketCount);
    // Every example design has goldens.
    EXPECT_EQ(countExampleFiles(), kExampleCount);
}

// The stable-step sets: the simulations of every input once more, with the time step the
// manifest records. That of the test rockets has the time series of its branches; that of the
// example designs is the documents alone (the harness can write their time series too, into a
// scratch directory: those files are not committed).
TEST(GoldenSchema, ManifestListsTheStableStepSets)
{
    const auto manifest = loadManifestOrFail();
    EXPECT_EQ(manifest.stableTimeStep, 0.01);
    EXPECT_EQ(manifest.stableSimulationsOf, (std::vector<std::string>{"example", "testrocket"}));
    EXPECT_EQ(manifest.stableTimeSeriesOf, std::vector<std::string>{"testrocket"});
    checkStableLists(manifest);
    const StableSetSize examples = stableSetSize(manifest, "example");
    EXPECT_EQ(examples.simulations, kStableExampleSimulationCount);
    EXPECT_EQ(examples.files, kStableExampleFileCount);
    const StableSetSize testRockets = stableSetSize(manifest, "testrocket");
    EXPECT_EQ(testRockets.simulations, kStableTestRocketSimulationCount);
    EXPECT_EQ(testRockets.files, kStableTestRocketFileCount);
}

// What the documents of the stable-step sets hold, read from the files alone. A comparison of
// QtRocket with the documents of the examples (run 9c) has these numbers to account for: 54
// simulations in 69 branches with 734 events (the default-step set of the examples has 733) and
// 92286 rows, of which the documents give the number per branch only. The simulations of the
// examples had the installation's time step of 0.05 s, but for the five of "Pods--airframes
// and winglets", whose document sets 0.04 s; every one was run at 0.01 s.
TEST(GoldenSchema, StableStepDocumentsHoldWhatIsPinned)
{
    const auto manifest = loadManifestOrFail();
    EXPECT_EQ(toText(stableDocumentCounts(manifest, "example")),
              "54 simulations, 69 branches, 734 events, 4832 columns, 20 warnings, 92286 rows;"
              " 5 with a time step of 0.04 s 49 with a time step of 0.05 s"
              " (example-pods-airframes-and-winglets)");
    // The numbers of SimulationStableGoldenCoverage, which reads the time series as well.
    EXPECT_EQ(toText(stableDocumentCounts(manifest, "testrocket")),
              "50 simulations, 53 branches, 437 events, 2826 columns, 21 warnings, 35323 rows;"
              " 50 with a time step of 0.05 s");
}

/// A directory name the dumper owns (it deletes and rewrites these directories).
bool isInputDirectoryName(std::string_view name)
{
    return name.starts_with("example-") || name.starts_with("testrocket-");
}

TEST(GoldenSchema, EveryInputDirectoryIsAnInput)
{
    const auto  manifest    = loadManifestOrFail();
    std::size_t directories = 0;
    for (const auto& entry : std::filesystem::directory_iterator(QtRocket::Test::goldensDir()))
    {
        const std::string name = entry.path().filename().string();
        if (!entry.is_directory() || !isInputDirectoryName(name))
        {
            continue;
        }
        ++directories;
        EXPECT_NE(manifest.find(name), nullptr) << "not an input of manifest.json: " << name;
    }
    EXPECT_EQ(directories, manifest.inputs.size());
}

TEST(GoldenSchema, EveryFileOfAnInputIsListed)
{
    const auto manifest = loadManifestOrFail();
    const auto listed   = listedFiles(manifest);
    for (const auto& input : manifest.inputs)
    {
        expectDirectoryListed(input.name, listed);
    }
    for (const auto& file : listed)
    {
        EXPECT_TRUE(std::filesystem::is_regular_file(QtRocket::Test::goldensDir() / file))
            << "listed but missing: " << file;
    }
}

// ---- one input's design files ----

void checkGeometryFile(const GoldenInput& input)
{
    const std::string& file     = input.geometry;
    const json         geometry = loadJsonOrFail(file);
    expectHeader(geometry, "geometry", input.name, file);
    expectKeys(
        geometry,
        {"rocketName", "selectedConfiguration", "loadWarnings", "components", "configurations"},
        file);

    const auto& components = geometry.value("components", json::array());
    ASSERT_FALSE(components.empty()) << file;
    EXPECT_EQ(components[0].value("path", ""), "/") << file;
    EXPECT_EQ(components[0].value("type", ""), "Rocket") << file;
    std::set<std::string> paths;
    for (const auto& component : components)
    {
        checkComponent(component, file, paths);
    }

    const auto& configurations = geometry.value("configurations", json::array());
    ASSERT_FALSE(configurations.empty()) << file;
    for (std::size_t i = 0; i < configurations.size(); ++i)
    {
        checkGeometryConfiguration(configurations[i], i, paths, file);
    }
}

void checkMassFile(const GoldenInput& input)
{
    const std::string& file = input.mass;
    const json         mass = loadJsonOrFail(file);
    expectHeader(mass, "mass", input.name, file);
    const auto& configurations = mass.value("configurations", json::array());
    ASSERT_FALSE(configurations.empty()) << file;
    for (std::size_t i = 0; i < configurations.size(); ++i)
    {
        checkMassConfiguration(configurations[i], i, file);
    }
}

void checkAeroFile(const GoldenInput& input)
{
    const std::string& file = input.aero;
    const json         aero = loadJsonOrFail(file);
    expectHeader(aero, "aero", input.name, file);
    expectKeys(aero, {"atmosphere", "stallAngle", "configurations"}, file);
    const auto& configurations = aero.value("configurations", json::array());
    ASSERT_FALSE(configurations.empty()) << file;
    EXPECT_TRUE(configurations[0].value("sameResultsAs", json(0)).is_null()) << file;
    for (std::size_t i = 0; i < configurations.size(); ++i)
    {
        checkAeroConfiguration(aero, i, file);
    }
}

/// The re-saved design is well-formed XML with OpenRocketSaver's root element, a rocket and the
/// input's simulations.
void checkResaveXml(const std::string& text, const GoldenInput& input)
{
    pugi::xml_document           document;
    const pugi::xml_parse_result parsed = document.load_buffer(text.data(), text.size());
    ASSERT_TRUE(parsed) << input.resave << ": " << parsed.description() << " at offset "
                        << parsed.offset;
    const pugi::xml_node root = document.document_element();
    EXPECT_STREQ(root.name(), "openrocket") << input.resave;
    EXPECT_STREQ(root.attribute("version").value(), "1.11") << input.resave;
    EXPECT_TRUE(std::string_view{root.attribute("creator").value()}.starts_with("OpenRocket "))
        << input.resave;
    EXPECT_TRUE(root.child("rocket")) << input.resave;
    std::size_t simulations = 0;
    for ([[maybe_unused]] const pugi::xml_node simulation :
         root.child("simulations").children("simulation"))
    {
        ++simulations;
    }
    EXPECT_EQ(simulations, input.simulations.size()) << input.resave;
}

void checkResaveFile(const GoldenInput& input)
{
    const auto text = QtRocket::readTextFile(QtRocket::Test::goldensDir() / input.resave);
    ASSERT_TRUE(text.has_value()) << text.error().toString();
    EXPECT_TRUE(text->starts_with("<?xml version='1.0' encoding='utf-8'?>\n")) << input.resave;
    EXPECT_TRUE(text->ends_with("</openrocket>\n")) << input.resave;
    checkResaveXml(*text, input);
}

/// The names of the inputs listed in manifest.json; none when it cannot be read
/// (which GoldenSchema.ManifestDescribesItsSource reports).
std::vector<std::string> goldenInputNames()
{
    std::vector<std::string> names;
    if (const auto manifest = QtRocket::Test::loadGoldenManifest())
    {
        for (const auto& input : manifest->inputs)
        {
            names.push_back(input.name);
        }
    }
    return names;
}

/// The schema checks of one input's files, split in two tests per input (the
/// design files, the simulations) so that no single test parses all of the
/// golden data (its time series hold millions of values) while the number of
/// test processes stays small.
class GoldenInputSchema : public ::testing::TestWithParam<std::string>
{
protected:
    void SetUp() override
    {
        auto manifest = QtRocket::Test::loadGoldenManifest();
        ASSERT_TRUE(manifest.has_value()) << manifest.error().toString();
        const auto* input = manifest->find(GetParam());
        ASSERT_NE(input, nullptr) << GetParam();
        m_input            = *input;
        m_stableTimeStep   = manifest->stableTimeStep;
        m_stableTimeSeries = manifest->hasStableTimeSeries(*input);
    }

    [[nodiscard]] const GoldenInput& input() const { return m_input; }

    /// The time step of the stable-step set (GoldenManifest::stableTimeStep).
    [[nodiscard]] double stableTimeStep() const { return m_stableTimeStep; }

    /// Whether the stable-step set of the input has the time series of its branches
    /// (GoldenManifest::hasStableTimeSeries()); the documents alone otherwise.
    [[nodiscard]] bool stableTimeSeries() const { return m_stableTimeSeries; }

private:
    GoldenInput m_input;
    double      m_stableTimeStep{0};
    bool        m_stableTimeSeries{false};
};

TEST_P(GoldenInputSchema, DesignFilesHaveTheRequiredKeys)
{
    checkGeometryFile(input());
    checkMassFile(input());
    checkAeroFile(input());
    checkResaveFile(input());
}

TEST_P(GoldenInputSchema, SimulationsMatchTheirTimeSeries)
{
    for (std::size_t s = 0; s < input().simulations.size(); ++s)
    {
        checkSimulation(input().simulations[s], s, input().name, /*timeSeries=*/true);
    }
}

// The stable-step set of the input: the same schema (with the time series of the branches for
// a test rocket, the documents alone for an example), and each simulation is its default-step
// simulation with nothing but the time step changed.
TEST_P(GoldenInputSchema, StableSimulationsMatchTheirFilesAndTheirDefaultStepSimulations)
{
    ASSERT_EQ(input().stableSimulations.size(), input().simulations.size());
    for (std::size_t s = 0; s < input().stableSimulations.size(); ++s)
    {
        checkSimulation(input().stableSimulations[s], s, input().name, stableTimeSeries());
        checkStableSimulation(input(), s, stableTimeStep());
    }
}

// One instantiation per input of manifest.json, named after it ('-' becomes
// '_').
INSTANTIATE_TEST_SUITE_P(Goldens, GoldenInputSchema, ::testing::ValuesIn(goldenInputNames()),
                         [](const ::testing::TestParamInfo<std::string>& paramInfo) {
                             std::string name = paramInfo.param;
                             std::ranges::replace(name, '-', '_');
                             return name;
                         });

}  // namespace
