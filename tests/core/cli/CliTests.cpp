// Tests of cli/Cli.h: QtRocket_cli run in this process, every command once. QtRocket's own:
// OpenRocket has no command line. The commands that need motors share the bundled database, read
// once per test process (TestMotorDatabase.h). Fresh flights are chaotic in their last digits, so a
// flight is compared with its stable-step golden within a tolerance, never byte for byte.

#include "QtRocket/cli/Cli.h"

#include <cstddef>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/cli/CliOutput.h"
#include "QtRocket/cli/ExitCode.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/Version.h"
#include "TestPaths.h"
#include "TestTempDir.h"
#include "motor/TestMotorDatabase.h"

namespace
{

using QtRocket::Cli;
using QtRocket::CliOutput;
using QtRocket::ExitCode;
using QtRocket::pathToUtf8;
using QtRocket::Test::bundledMotorDatabase;
using QtRocket::Test::dataDir;
using QtRocket::Test::TempDir;
using QtRocket::Test::testDataDir;

/// The apogee of the stable-step golden of "A simple model rocket.ork", simulation 1
/// (tests/data/goldens/example-a-simple-model-rocket/stable/sim_00_simulation-1.json,
/// summary.maxAltitude): the milestone's exit check (plan section 7).
constexpr double kMilestoneApogee = 50.482784669989435;

/// What a run gave.
struct CliRun
{
    ExitCode    code;
    std::string out;
    std::string err;
};

/// Runs the tool with @p arguments and @p settings.
[[nodiscard]] CliRun run(const std::vector<std::string>& arguments, const Cli::Settings& settings)
{
    CliOutput      output;
    const ExitCode code = Cli::run(arguments, settings, output);
    return CliRun{.code = code, .out = output.out(), .err = output.err()};
}

/// Runs the tool with @p arguments over the bundled motor database.
[[nodiscard]] CliRun runWithMotors(const std::vector<std::string>& arguments)
{
    return run(arguments,
               Cli::Settings{.dataDirectory = dataDir(), .motorDatabase = &bundledMotorDatabase()});
}

/// The example design @p name of data/examples, as a UTF-8 argument.
[[nodiscard]] std::string example(std::string_view name)
{
    return pathToUtf8(dataDir() / "examples" / name);
}

/// The number on the line "<label>: <number> ..." of @p text that follows @p after (the first
/// such line after it); NaN, and a failure of the test, when there is none.
[[nodiscard]] double numberAfter(std::string_view text, std::string_view after,
                                 std::string_view label)
{
    const std::string key   = "\n  " + std::string(label) + ": ";
    const std::size_t start = text.find(after);
    const std::size_t line  = start == std::string_view::npos ? start : text.find(key, start);
    if (line == std::string_view::npos)
    {
        ADD_FAILURE() << "no line '" << label << "' after '" << after << "' in\n" << text;
        return std::numeric_limits<double>::quiet_NaN();
    }
    const std::size_t           begin = line + key.size();
    const std::size_t           end   = text.find_first_of(" \n", begin);
    const std::optional<double> value =
        QtRocket::Strings::parseDouble(text.substr(begin, end - begin));
    if (!value.has_value())
    {
        ADD_FAILURE() << "no number in the line '" << label << "'";
        return std::numeric_limits<double>::quiet_NaN();
    }
    return *value;
}

/// Whether every line of the table @p table after its header is empty or starts with @p start.
[[nodiscard]] bool everyMotorLineStartsWith(std::string_view table, std::string_view start)
{
    const std::vector<std::string> lines = QtRocket::Strings::split(table, '\n');
    for (std::size_t i = 1; i < lines.size(); i++)
    {
        if (!lines[i].empty() && !lines[i].starts_with(start))
        {
            ADD_FAILURE() << lines[i];
            return false;
        }
    }
    return true;
}

TEST(Cli, VersionAndHelpNeedNoMotorDatabase)
{
    const Cli::Settings nothing{.dataDirectory = {}, .motorDatabase = nullptr};
    const CliRun        version = run({"--version"}, nothing);
    EXPECT_EQ(version.code, ExitCode::OK);
    EXPECT_EQ(version.out, std::string(QtRocket::creatorString()) + "\n");
    EXPECT_TRUE(version.err.empty());

    const CliRun help = run({"--help"}, nothing);
    EXPECT_EQ(help.code, ExitCode::OK);
    EXPECT_EQ(help.out, Cli::usage());
    EXPECT_NE(help.out.find("Exit codes:"), std::string::npos);
    EXPECT_NE(help.out.find("  5  internal error"), std::string::npos);
}

TEST(Cli, AWrongCommandLineIsAUsageErrorOnTheStandardError)
{
    const CliRun wrong = run({"simulate", "x.ork", "--time-step", "0"}, {});
    EXPECT_EQ(wrong.code, ExitCode::USAGE);
    EXPECT_TRUE(wrong.out.empty());
    EXPECT_EQ(wrong.err,
              "QtRocket_cli: the time step must be a number from 0.0001 to 10 s, not '0'\n"
              "Run 'QtRocket_cli --help' for the usage.\n");
    EXPECT_EQ(run({}, {}).code, ExitCode::USAGE);
}

TEST(Cli, AMotorDatabaseThatCannotBeReadIsAnIoError)
{
    const TempDir directory;
    // No motors/initial_motors.db in the data directory.
    const CliRun missing =
        run({"motors"}, Cli::Settings{.dataDirectory = directory.path(), .motorDatabase = nullptr});
    EXPECT_EQ(missing.code, ExitCode::IO);
    EXPECT_NE(missing.err.find("QtRocket_cli: cannot read the motor database '"), std::string::npos)
        << missing.err;

    // --db wins over a database handed in.
    const QtRocket::ThrustCurveMotorSetDatabase handedIn;
    const std::filesystem::path broken = directory.write("broken.db", "not a database");
    const CliRun                given =
        run({"info", "x.ork", "--db", pathToUtf8(broken)},
            Cli::Settings{.dataDirectory = directory.path(), .motorDatabase = &handedIn});
    EXPECT_EQ(given.code, ExitCode::IO);
}

TEST(Cli, MotorsListsAndSearchesTheDatabaseAndLoadsAMotorFile)
{
    const CliRun search = runWithMotors({"motors", "--search", "ESTES  a8"});
    EXPECT_EQ(search.code, ExitCode::OK);
    EXPECT_TRUE(search.out.starts_with("Manufacturer\tDesignation\t")) << search.out;
    EXPECT_NE(search.out.find("\nEstes\tA8\tA8\tSingle-use\t"), std::string::npos);
    EXPECT_TRUE(everyMotorLineStartsWith(search.out, "Estes\tA8\tA8\tSingle-use\t"));

    const CliRun load =
        run({"motors", "--load", pathToUtf8(testDataDir() / "motors" / "Estes_A8.rse")}, {});
    EXPECT_EQ(load.code, ExitCode::OK);
    EXPECT_NE(load.out.find("\t3,5\t22aec01287ea1e3b8c6f66b26fe5fea6\n"), std::string::npos)
        << load.out;

    const CliRun missing = run({"motors", "--load", "no such motor.eng"}, {});
    EXPECT_EQ(missing.code, ExitCode::LOAD);
    EXPECT_NE(missing.err.find("QtRocket_cli: cannot load 'no such motor.eng': "),
              std::string::npos);
}

// 'info' settles the automatic dimensions: the launch CG of the first motor configuration of
// "Dual parachute deployment" is then that of mass.json (0.9620825931703152; as loaded it is off
// by about 1e-3 m).
TEST(Cli, InfoPrintsEveryConfigurationOfTheSettledDesign)
{
    const CliRun info = runWithMotors({"info", example("Dual parachute deployment.ork")});
    EXPECT_EQ(info.code, ExitCode::OK);
    EXPECT_TRUE(info.err.empty()) << info.err;
    EXPECT_NE(info.out.find("Flight configurations: 7\n"), std::string::npos);
    EXPECT_NE(info.out.find("Configuration 0: [No motors] (default)\n"), std::string::npos);
    EXPECT_NEAR(numberAfter(info.out, "Configuration 1: ", "CG"), 0.9620825931703152, 1e-12);

    const CliRun one =
        runWithMotors({"info", example("Dual parachute deployment.ork"), "--config", "6"});
    EXPECT_EQ(one.code, ExitCode::OK);
    EXPECT_EQ(one.out.find("Configuration 1: "), std::string::npos);
    EXPECT_NE(one.out.find("Configuration 6: "), std::string::npos);

    const CliRun none =
        runWithMotors({"info", example("Dual parachute deployment.ork"), "--config", "7"});
    EXPECT_EQ(none.code, ExitCode::USAGE);

    const CliRun missing =
        runWithMotors({"info", pathToUtf8(testDataDir() / "no such design.ork")});
    EXPECT_EQ(missing.code, ExitCode::LOAD);
    EXPECT_NE(missing.err.find("QtRocket_cli: cannot load '"), std::string::npos);
}

// Plan section 7: 'QtRocket_cli simulate "data/examples/A simple model rocket.ork" --calm
// --time-step 0.01 --csv out.csv' reproduces the apogee of the stable-step golden within 1e-4
// relative (measured: within 4e-7 for any seed, the pitch and yaw jitter left in).
TEST(Cli, SimulateMeetsTheMilestoneExitCheckAndWritesTheCsv)
{
    const TempDir               directory;
    const std::filesystem::path csv = directory.resolve("out.csv");
    const CliRun                simulate =
        runWithMotors({"simulate", example("A simple model rocket.ork"), "--calm", "--time-step",
                       "0.01", "--csv", pathToUtf8(csv)});
    EXPECT_EQ(simulate.code, ExitCode::OK);
    EXPECT_TRUE(simulate.err.empty()) << simulate.err;
    EXPECT_TRUE(simulate.out.starts_with("Simulation 1: Simulation 1\n")) << simulate.out;
    EXPECT_NEAR(numberAfter(simulate.out, "Simulation 1:", "apogee"), kMilestoneApogee,
                1e-4 * kMilestoneApogee);
    EXPECT_NE(simulate.out.find("  time step: 0.01 s\n"), std::string::npos);
    EXPECT_NE(simulate.out.find(": APOGEE (A simple model rocket)\n"), std::string::npos);

    const QtRocket::Result<std::string> text = QtRocket::readTextFile(csv);
    ASSERT_TRUE(text.has_value());
    EXPECT_TRUE(text->starts_with("# Simulation 1 (Up to date)\n")) << text->substr(0, 100);
    EXPECT_NE(text->find("\n# Time (s),Altitude (m),"), std::string::npos);
    EXPECT_EQ(text->find("Computation time"), std::string::npos);
    EXPECT_NE(text->find("\n# Event APOGEE occurred at t="), std::string::npos);
    EXPECT_EQ(text->find('\r'), std::string::npos);
}

TEST(Cli, SimulateSeedsSelectsAndNumbersTheCsvFiles)
{
    const TempDir directory;
    const CliRun  seeded = runWithMotors({"simulate", example("A simple model rocket.ork"), "--sim",
                                          "Simulation 2", "--seed", "-7"});
    EXPECT_EQ(seeded.code, ExitCode::OK);
    EXPECT_TRUE(seeded.out.starts_with("Simulation 2: Simulation 2\n")) << seeded.out;
    EXPECT_NE(seeded.out.find("  random seed: -7\n"), std::string::npos);

    const CliRun all = runWithMotors({"simulate", example("Chute release.ork"), "--all", "--calm",
                                      "--csv", pathToUtf8(directory.resolve("flight.csv"))});
    EXPECT_EQ(all.code, ExitCode::OK) << all.err;
    EXPECT_TRUE(std::filesystem::is_regular_file(directory.resolve("flight-1.csv")));
    EXPECT_FALSE(std::filesystem::exists(directory.resolve("flight.csv")));

    const CliRun unknown =
        runWithMotors({"simulate", example("A simple model rocket.ork"), "--sim", "6"});
    EXPECT_EQ(unknown.code, ExitCode::USAGE);
    EXPECT_NE(unknown.err.find("has no simulation '6'"), std::string::npos);

    const CliRun unwritable =
        runWithMotors({"simulate", example("A simple model rocket.ork"), "--csv",
                       pathToUtf8(directory.resolve("missing") / "out.csv")});
    EXPECT_EQ(unwritable.code, ExitCode::IO);
    EXPECT_NE(unwritable.err.find("cannot open '"), std::string::npos) << unwritable.err;
}

}  // namespace
