// Golden tests of the command-line tool QtRocket_cli (cli/Cli.h, run in this process): what it
// prints is compared with what OpenRocket computes (tests/data/goldens, tools/openrocket-goldens).
// QtRocket's own tool, so QtRocket's own tests.
//
// CliGolden.InfoMatchesTheSixteenExamples runs 'info' on OpenRocket's sixteen example designs
// (data/examples, the "example" inputs of the goldens' manifest) and reads back every number it
// printed (std::format's shortest form, which reads back as the same double). Per flight
// configuration, matched by its number, which is its index among the golden configurations (the
// tool's factory preferences name a configuration by its motors' designations, "[H669N-P]",
// where the goldens use common names, "[H669-P]"):
// - mass.json: the default flag; the CG (the x of the launch centre of mass, absolute 1e-9 m) and
//   the masses with motors (launch) and without (structure), relative 1e-9;
// - aero.json, the results of the configuration or of the one it shares them with
//   ("sameResultsAs"): the CP and the CNa of the point at Mach 0.3 (the default Mach number), an
//   angle of attack of 0 and a theta of 0, and the worst CP at Mach 0.3, each relative 1e-9
//   (plan section 6.4); the stability in metres is the worst CP less the CG, absolute 1e-9 m;
//   the number of warnings is that of the golden geometry warnings (none in any example);
// - geometry.json: the rocket's name, the number of configurations and each one's length
//   (relative 1e-9).
// The tool loads with its own environment (CliEnvironment: factory preferences, the bundled
// component presets), not the harness's, and is handed the bundled presets the executable reads
// from its data directory (read once per test process): the three examples with a <preset> find
// the six presets they name, so that no example prints anything on the standard error, as
// OpenRocket's harness loads them without a warning. The tool settles the automatic dimensions
// first, as the harness does before it dumps. Once more for 'Two stage high power rocket' at
// --mach 2, the points at Mach 2.0. One test for all, which reads the motor database and the
// presets once.
//
// CliGolden.SimulateMeetsTheMilestoneExitCheck is the exit check of Milestone 1 (plan section 7):
// 'simulate "A simple model rocket.ork" --calm --time-step 0.01 --csv out.csv' gives the apogee
// of the stable-step golden of its first simulation (stable/sim_00_simulation-1.json,
// summary.maxAltitude) within 1e-4 relative. The tool leaves the pitch and yaw jitter in and the
// seed random: measured within 6e-7 of the golden for any seed (the scouts' exit probe, Linux).

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/cli/ExitCode.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"
#include "TestTempDir.h"
#include "cli/CliRunSupport.h"
#include "file/DesignFileEnvironment.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenExamples.h"
#include "goldens/GoldenGeometry.h"
#include "goldens/GoldenMismatches.h"

namespace
{

using QtRocket::ExitCode;
using QtRocket::Result;
using QtRocket::Test::CliRun;
using QtRocket::Test::GoldenExample;
using QtRocket::Test::GoldenMismatches;
using QtRocket::Test::runCliWithMotors;
using Presets = QtRocket::Test::DesignFileEnvironment::Presets;

/// One flight configuration as 'info' printed it.
struct PrintedConfiguration
{
    /// The number of its heading ("Configuration 3: ...").
    int number{-1};
    /// Whether its heading says "(default)".
    bool isDefault{false};
    /// Its lines "  <label>: <value>", by label ("warnings" is "none" when it has none, and
    /// empty when its warnings follow on lines of their own).
    std::map<std::string, std::string, std::less<>> values;
    /// The lines "    <warning>" under "  warnings:".
    int warningLines{0};
};

/// What 'info' printed of a design.
struct PrintedDesign
{
    std::string                       rocket;
    std::string                       configurationCount;
    std::vector<PrintedConfiguration> configurations;
};

/// The heading "Configuration <n>: <name>[ (default)][ (selected)]" @p line as a configuration;
/// nullopt for another line.
[[nodiscard]] std::optional<PrintedConfiguration> parseHeading(std::string_view line)
{
    constexpr std::string_view kHeading = "Configuration ";
    const std::size_t          colon    = line.find(": ");
    if (!line.starts_with(kHeading) || colon == std::string_view::npos)
    {
        return std::nullopt;
    }
    const std::optional<int> number =
        QtRocket::Strings::parseInt(line.substr(kHeading.size(), colon - kHeading.size()));
    if (!number.has_value())
    {
        return std::nullopt;
    }
    std::string_view flags = line.substr(colon);
    if (flags.ends_with(" (selected)"))
    {
        flags.remove_suffix(std::string_view(" (selected)").size());
    }
    PrintedConfiguration configuration;
    configuration.number    = *number;
    configuration.isDefault = flags.ends_with(" (default)");
    return configuration;
}

/// Adds the line @p line of a configuration's block to @p configuration.
void addConfigurationLine(PrintedConfiguration& configuration, std::string_view line)
{
    if (line.starts_with("    "))
    {
        configuration.warningLines++;
        return;
    }
    if (line == "  warnings:")
    {
        configuration.values["warnings"] = "";
        return;
    }
    const std::size_t colon = line.find(": ");
    if (line.starts_with("  ") && colon != std::string_view::npos)
    {
        configuration.values[std::string(line.substr(2, colon - 2))] =
            std::string(line.substr(colon + 2));
    }
}

/// Reads what 'info' printed (InfoCommand.h has the layout).
[[nodiscard]] PrintedDesign parseInfo(std::string_view out)
{
    PrintedDesign design;
    for (const std::string& line : QtRocket::Test::outputLines(out))
    {
        if (std::optional<PrintedConfiguration> heading = parseHeading(line); heading.has_value())
        {
            design.configurations.push_back(std::move(*heading));
        }
        else if (!design.configurations.empty())
        {
            addConfigurationLine(design.configurations.back(), line);
        }
        else if (line.starts_with("Rocket: "))
        {
            design.rocket = line.substr(std::string_view("Rocket: ").size());
        }
        else if (line.starts_with("Flight configurations: "))
        {
            design.configurationCount =
                line.substr(std::string_view("Flight configurations: ").size());
        }
    }
    return design;
}

/// The number at the start of the value of @p label in @p configuration ("0.425 m" gives 0.425;
/// "1.5 cal (0.04 m)" gives 1.5); NaN, and a line in @p m, when there is none.
[[nodiscard]] double printedNumber(const PrintedConfiguration& configuration,
                                   std::string_view label, GoldenMismatches& m)
{
    const auto value = configuration.values.find(label);
    if (value == configuration.values.end())
    {
        m.note(std::format("configuration {}: no line '{}'", configuration.number, label));
        return std::numeric_limits<double>::quiet_NaN();
    }
    const std::string_view      text = value->second;
    const std::optional<double> number =
        QtRocket::Strings::parseDouble(text.substr(0, text.find(' ')));
    if (!number.has_value())
    {
        m.note(std::format("configuration {}: '{}: {}' has no number", configuration.number, label,
                           text));
        return std::numeric_limits<double>::quiet_NaN();
    }
    return *number;
}

/// The stability in metres of @p configuration: the number in the parentheses of
/// "stability: <cal> cal (<m> m)"; NaN, and a line in @p m, when there is none.
[[nodiscard]] double printedStabilityMetres(const PrintedConfiguration& configuration,
                                            GoldenMismatches&           m)
{
    const auto value = configuration.values.find("stability");
    if (value == configuration.values.end())
    {
        m.note(std::format("configuration {}: no stability", configuration.number));
        return std::numeric_limits<double>::quiet_NaN();
    }
    const std::string_view      text = value->second;
    const std::size_t           open = text.find('(');
    const std::size_t           end  = text.find(" m)");
    const std::optional<double> metres =
        open == std::string_view::npos || end < open
            ? std::nullopt
            : QtRocket::Strings::parseDouble(text.substr(open + 1, end - open - 1));
    if (!metres.has_value())
    {
        m.note(std::format("configuration {}: 'stability: {}'", configuration.number, text));
        return std::numeric_limits<double>::quiet_NaN();
    }
    return *metres;
}

/// The golden number at the JSON pointer @p pointer of @p golden; NaN when there is none.
[[nodiscard]] double goldenAt(const nlohmann::json& golden, const char* pointer)
{
    const nlohmann::json::json_pointer path(pointer);
    return golden.contains(path) ? QtRocket::Test::goldenValue(golden.at(path))
                                 : std::numeric_limits<double>::quiet_NaN();
}

/// The CP entry [x, y, z, CNa] of the point of @p results at Mach @p mach, an angle of attack of
/// 0 and a theta of 0; null when there is none.
[[nodiscard]] const nlohmann::json* goldenPointCp(const nlohmann::json& results, double mach)
{
    const auto points = results.find("points");
    if (points == results.end() || !points->is_array())
    {
        return nullptr;
    }
    for (const nlohmann::json& point : *points)
    {
        if (goldenAt(point, "/conditions/mach") == mach &&
            goldenAt(point, "/conditions/aoa") == 0.0 &&
            goldenAt(point, "/conditions/theta") == 0.0)
        {
            return point.contains("cp") ? &point.at("cp") : nullptr;
        }
    }
    return nullptr;
}

/// The worst CP entry [x, y, z, CNa] of @p results at Mach @p mach; null when there is none.
[[nodiscard]] const nlohmann::json* goldenWorstCp(const nlohmann::json& results, double mach)
{
    const auto worst = results.find("worstCP");
    if (worst == results.end() || !worst->is_array())
    {
        return nullptr;
    }
    for (const nlohmann::json& entry : *worst)
    {
        if (goldenAt(entry, "/mach") == mach)
        {
            return entry.contains("cp") ? &entry.at("cp") : nullptr;
        }
    }
    return nullptr;
}

/// The golden documents of one example.
struct ExampleGoldens
{
    nlohmann::json geometry;
    nlohmann::json mass;
    nlohmann::json aero;
};

/// Reads the golden documents of the input @p input; nullopt, and a line in @p m, when one cannot
/// be read.
[[nodiscard]] std::optional<ExampleGoldens> loadExampleGoldens(const std::string& input,
                                                               GoldenMismatches&  m)
{
    Result<nlohmann::json> geometry = QtRocket::Test::loadGoldenJson(input + "/geometry.json");
    Result<nlohmann::json> mass     = QtRocket::Test::loadGoldenJson(input + "/mass.json");
    Result<nlohmann::json> aero     = QtRocket::Test::loadGoldenJson(input + "/aero.json");
    if (!geometry || !mass || !aero)
    {
        m.note("the golden documents cannot be read");
        return std::nullopt;
    }
    return ExampleGoldens{
        .geometry = std::move(*geometry), .mass = std::move(*mass), .aero = std::move(*aero)};
}

/// Compares the CP, the CNa and the worst CP at Mach @p mach of @p printed with @p results.
void compareCp(const PrintedConfiguration& printed, const nlohmann::json& results, double mach,
               GoldenMismatches& m)
{
    const nlohmann::json* point = goldenPointCp(results, mach);
    const nlohmann::json* worst = goldenWorstCp(results, mach);
    if (point == nullptr || worst == nullptr)
    {
        m.note(std::format("configuration {}: no golden point at Mach {}", printed.number, mach));
        return;
    }
    const std::string field = std::format("configuration {} at Mach {}: ", printed.number, mach);
    m.relative(field + "CP", QtRocket::Test::goldenValue(point->at(0)),
               printedNumber(printed, std::format("CP at Mach {}, angle of attack 0", mach), m));
    m.relative(field + "CNa", QtRocket::Test::goldenValue(point->at(3)),
               printedNumber(printed, std::format("CNa at Mach {}, angle of attack 0", mach), m));
    m.relative(field + "worst CP", QtRocket::Test::goldenValue(worst->at(0)),
               printedNumber(printed, std::format("worst CP at Mach {}", mach), m));
}

/// Compares the configuration @p printed at the default Mach number with its golden entries in
/// @p goldens.
void compareConfiguration(const PrintedConfiguration& printed, const ExampleGoldens& goldens,
                          GoldenMismatches& m)
{
    const auto            index   = static_cast<std::size_t>(printed.number);
    const nlohmann::json& mass    = goldens.mass.at("configurations").at(index);
    const nlohmann::json& length  = goldens.geometry.at("configurations").at(index).at("length");
    const nlohmann::json* results = QtRocket::Test::aeroResults(goldens.aero, index);
    if (results == nullptr)
    {
        m.note(std::format("configuration {}: no golden aerodynamics", printed.number));
        return;
    }
    const std::string field = std::format("configuration {}: ", printed.number);
    m.boolean(field + "default", mass.at("isDefault").get<bool>(), printed.isDefault);
    m.relative(field + "length", QtRocket::Test::goldenValue(length),
               printedNumber(printed, "length", m));
    m.relative(field + "mass with motors", goldenAt(mass, "/launch/mass"),
               printedNumber(printed, "mass with motors", m));
    m.relative(field + "mass without motors", goldenAt(mass, "/structure/mass"),
               printedNumber(printed, "mass without motors", m));
    const double cg = goldenAt(mass, "/launch/cm/0");
    m.absolute(field + "CG", cg, printedNumber(printed, "CG", m));
    compareCp(printed, *results, 0.3, m);
    const nlohmann::json* worst = goldenWorstCp(*results, 0.3);
    if (worst != nullptr)
    {
        m.absolute(field + "stability (m)", QtRocket::Test::goldenValue(worst->at(0)) - cg,
                   printedStabilityMetres(printed, m));
    }
    const nlohmann::json& geometryWarnings = results->at("geometryWarnings");
    const auto            warnings         = printed.values.find("warnings");
    m.text(field + "warnings", geometryWarnings.empty() ? "none" : "",
           warnings == printed.values.end() ? "(no line)" : warnings->second);
    m.integer(field + "warning lines", static_cast<std::int64_t>(geometryWarnings.size()),
              printed.warningLines);
}

/// Adds every line of the standard error @p err to @p m: with the bundled presets no example
/// gives a warning.
void expectNothingOnTheStandardError(std::string_view err, GoldenMismatches& m)
{
    for (const std::string& line : QtRocket::Test::outputLines(err))
    {
        m.note(std::format("standard error: {}", line));
    }
}

/// What the comparison of 'info' on an example compared.
struct InfoCounts
{
    int examples{0};
    int configurations{0};
};

/// Runs 'info' on @p example and compares what it printed with its goldens; the mismatches go to
/// @p report, the counts to @p counts.
void compareInfo(const GoldenExample& example, std::string& report, InfoCounts& counts)
{
    GoldenMismatches                    m(example.file);
    const std::optional<ExampleGoldens> goldens = loadExampleGoldens(example.name, m);
    const CliRun                        info =
        runCliWithMotors({"info", QtRocket::pathToUtf8(example.original)}, Presets::BUNDLED);
    if (info.code != ExitCode::OK || !goldens.has_value())
    {
        report += std::format("{}: exit code {}\n{}", example.file, QtRocket::exitStatus(info.code),
                              info.err);
        return;
    }
    expectNothingOnTheStandardError(info.err, m);
    const PrintedDesign printed = parseInfo(info.out);
    m.text("rocket", goldens->geometry.at("rocketName").get<std::string>(), printed.rocket);
    const std::size_t count = goldens->mass.at("configurations").size();
    m.text("configurations", std::to_string(count), printed.configurationCount);
    m.integer("configurations printed", static_cast<std::int64_t>(count),
              static_cast<std::int64_t>(printed.configurations.size()));
    for (std::size_t i = 0; i < printed.configurations.size() && i < count; i++)
    {
        m.integer("configuration number", static_cast<std::int64_t>(i),
                  printed.configurations[i].number);
        if (std::cmp_equal(printed.configurations[i].number, i))
        {
            compareConfiguration(printed.configurations[i], *goldens, m);
            counts.configurations++;
        }
    }
    counts.examples++;
    report += m.report();
}

/// Runs 'info --mach @p mach' on @p example and compares the CP, the CNa and the worst CP of
/// every configuration with the golden points at that Mach number; the number of configurations
/// compared.
[[nodiscard]] int compareInfoAtMach(const GoldenExample& example, double mach, std::string& report)
{
    GoldenMismatches                    m(std::format("{} at Mach {}", example.file, mach));
    const std::optional<ExampleGoldens> goldens = loadExampleGoldens(example.name, m);
    const CliRun                        info    = runCliWithMotors(
        {"info", QtRocket::pathToUtf8(example.original), "--mach", std::format("{}", mach)},
        Presets::BUNDLED);
    int compared = 0;
    if (info.code == ExitCode::OK && goldens.has_value())
    {
        for (const PrintedConfiguration& printed : parseInfo(info.out).configurations)
        {
            const nlohmann::json* results = QtRocket::Test::aeroResults(
                goldens->aero, static_cast<std::size_t>(printed.number));
            if (results != nullptr)
            {
                compareCp(printed, *results, mach, m);
                compared++;
            }
        }
    }
    report += m.report();
    return compared;
}

TEST(CliGolden, InfoMatchesTheSixteenExamples)
{
    std::string report;
    InfoCounts  counts;
    for (const GoldenExample& example : QtRocket::Test::goldenExamples())
    {
        compareInfo(example, report, counts);
    }
    EXPECT_EQ(report, "");
    // Nothing is skipped: the sixteen examples and their 66 configurations.
    EXPECT_EQ(counts.examples, 16);
    EXPECT_EQ(counts.configurations, 66);

    const GoldenExample* twoStage =
        QtRocket::Test::findGoldenExample("Two stage high power rocket.ork");
    ASSERT_NE(twoStage, nullptr);
    std::string supersonic;
    EXPECT_EQ(compareInfoAtMach(*twoStage, 2.0, supersonic), 3);
    EXPECT_EQ(supersonic, "");
}

TEST(CliGolden, SimulateMeetsTheMilestoneExitCheck)
{
    const Result<nlohmann::json> golden = QtRocket::Test::loadGoldenJson(
        "example-a-simple-model-rocket/stable/sim_00_simulation-1.json");
    ASSERT_TRUE(golden.has_value()) << golden.error().message;
    const double apogee = goldenAt(*golden, "/summary/maxAltitude");
    ASSERT_EQ(apogee, 50.482784669989435);

    const QtRocket::Test::TempDir directory;
    const std::filesystem::path   csv = directory.resolve("out.csv");
    const CliRun                  simulate =
        runCliWithMotors({"simulate", QtRocket::Test::exampleArgument("A simple model rocket.ork"),
                          "--calm", "--time-step", "0.01", "--csv", QtRocket::pathToUtf8(csv)});
    EXPECT_EQ(simulate.code, ExitCode::OK);
    EXPECT_TRUE(simulate.err.empty()) << simulate.err;
    EXPECT_TRUE(simulate.out.starts_with("Simulation 1: Simulation 1\n")) << simulate.out;
    EXPECT_NEAR(QtRocket::Test::numberAfter(simulate.out, "Simulation 1:", "apogee"), apogee,
                1e-4 * apogee);
    EXPECT_NE(simulate.out.find("  time step: 0.01 s\n"), std::string::npos);
    EXPECT_NE(simulate.out.find(": APOGEE (A simple model rocket)\n"), std::string::npos);

    const Result<std::string> text = QtRocket::readTextFile(csv);
    ASSERT_TRUE(text.has_value());
    EXPECT_TRUE(text->starts_with("# Simulation 1 (Up to date)\n")) << text->substr(0, 100);
    EXPECT_NE(text->find("\n# Time (s),Altitude (m),"), std::string::npos);
    EXPECT_EQ(text->find("Computation time"), std::string::npos);
    EXPECT_NE(text->find("\n# Event APOGEE occurred at t="), std::string::npos);
    EXPECT_EQ(text->find('\r'), std::string::npos);
}

}  // namespace
