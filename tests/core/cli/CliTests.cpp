// Tests of cli/Cli.h: QtRocket_cli run in this process, every command and every exit code.
// QtRocket's own: OpenRocket has no command line. The commands that need motors share the bundled
// database, read once per test process (TestMotorDatabase.h); a test that needs none hands in an
// empty one. Fresh flights are chaotic in their last digits, so no number of a flight is pinned
// here (cli_golden_tests.cpp compares the milestone flight with its golden within a tolerance),
// and neither are the bytes of its CSV. The executable itself is run by
// tests/core/cli/CMakeLists.txt.

#include "QtRocket/cli/Cli.h"

#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/cli/CliOutput.h"
#include "QtRocket/cli/ExitCode.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/Version.h"
#include "TestPaths.h"
#include "TestTempDir.h"
#include "cli/CliRunSupport.h"
#include "motor/TestMotorDatabase.h"

namespace
{

using QtRocket::Cli;
using QtRocket::CliOutput;
using QtRocket::ExitCode;
using QtRocket::pathToUtf8;
using QtRocket::ThrustCurveMotorSetDatabase;
using QtRocket::Test::bundledMotorDatabase;
using QtRocket::Test::CFile;
using QtRocket::Test::CFileMode;
using QtRocket::Test::cliFixtureArgument;
using QtRocket::Test::CliRun;
using QtRocket::Test::dataDir;
using QtRocket::Test::exampleArgument;
using QtRocket::Test::numberAfter;
using QtRocket::Test::openCFile;
using QtRocket::Test::outputLines;
using QtRocket::Test::runCli;
using QtRocket::Test::runCliWithMotors;
using QtRocket::Test::runCliWithStreams;
using QtRocket::Test::TempDir;
using QtRocket::Test::testDataDir;

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

/// The number of motors of @p database: the lines of the table of 'motors' after its header.
[[nodiscard]] std::size_t motorCount(const ThrustCurveMotorSetDatabase& database)
{
    std::size_t count = 0;
    for (const QtRocket::ThrustCurveMotorSet& set : database.getMotorSets())
    {
        count += set.getMotors().size();
    }
    return count;
}

TEST(Cli, VersionAndHelpNeedNoMotorDatabase)
{
    const Cli::Settings nothing{.dataDirectory = {}, .motorDatabase = nullptr};
    const CliRun        version = runCli({"--version"}, nothing);
    EXPECT_EQ(version.code, ExitCode::OK);
    EXPECT_EQ(version.out, std::string(QtRocket::creatorString()) + "\n");
    EXPECT_TRUE(version.err.empty());

    const CliRun help = runCli({"--help"}, nothing);
    EXPECT_EQ(help.code, ExitCode::OK);
    EXPECT_EQ(help.out, Cli::usage());
    EXPECT_TRUE(help.err.empty());
    EXPECT_TRUE(help.out.starts_with("Usage: QtRocket_cli <command> [options]\n"));
    EXPECT_NE(help.out.find("Exit codes:"), std::string::npos);
    EXPECT_NE(help.out.find("  5  internal error"), std::string::npos);
    // "-h" anywhere, even after a command and its file.
    EXPECT_EQ(runCli({"simulate", "x.ork", "-h"}, nothing).out, Cli::usage());
}

TEST(Cli, AWrongCommandLineIsAUsageErrorOnTheStandardError)
{
    const CliRun wrong = runCli({"simulate", "x.ork", "--time-step", "0"}, {});
    EXPECT_EQ(wrong.code, ExitCode::USAGE);
    EXPECT_TRUE(wrong.out.empty());
    EXPECT_EQ(wrong.err,
              "QtRocket_cli: the time step must be a number from 0.0001 to 10 s, not '0'\n"
              "Run 'QtRocket_cli --help' for the usage.\n");

    const CliRun nothing = runCli({}, {});
    EXPECT_EQ(nothing.code, ExitCode::USAGE);
    EXPECT_TRUE(nothing.out.empty());
    EXPECT_EQ(nothing.err,
              "QtRocket_cli: no command given\nRun 'QtRocket_cli --help' for the usage.\n");

    // The command line is checked before anything is read: no motor database is needed.
    const CliRun option = runCli({"info", "x.ork", "--calm"}, {});
    EXPECT_EQ(option.code, ExitCode::USAGE);
    EXPECT_TRUE(option.err.starts_with("QtRocket_cli: unknown option '--calm' for 'info'\n"));
}

/// What Cli::run() of @p arguments writes to the standard error, and its exit code, when writing
/// to the standard output throws: std::runtime_error("the sink failed") or, with
/// @p standardException false, an int.
[[nodiscard]] CliRun runWithFailingStandardOutput(const std::vector<std::string>& arguments,
                                                  bool                            standardException)
{
    std::string err;
    CliOutput   output(
        [standardException](std::string_view) {
            if (standardException)
            {
                throw std::runtime_error("the sink failed");
            }
            // Not a std::exception, for the catch-all of Cli::run().
            // NOLINTNEXTLINE(bugprone-std-exception-baseclass)
            throw 7;
        },
        [&err](std::string_view text) { err += text; });
    const ExitCode code = Cli::run(arguments, {}, output);
    return CliRun{.code = code, .out = {}, .err = err};
}

// A BugError or any other exception that reaches the tool is exit code 5 with a line on the
// standard error; here the standard output throws when it is written to.
TEST(Cli, ADefectIsAnInternalError)
{
    const CliRun standard = runWithFailingStandardOutput({"--version"}, true);
    EXPECT_EQ(standard.code, ExitCode::INTERNAL);
    EXPECT_EQ(standard.err, "QtRocket_cli: internal error: the sink failed\n");

    const CliRun other = runWithFailingStandardOutput({"--help"}, false);
    EXPECT_EQ(other.code, ExitCode::INTERNAL);
    EXPECT_EQ(other.err, "QtRocket_cli: internal error\n");

    EXPECT_EQ(QtRocket::exitStatus(ExitCode::OK), 0);
    EXPECT_EQ(QtRocket::exitStatus(ExitCode::USAGE), 1);
    EXPECT_EQ(QtRocket::exitStatus(ExitCode::LOAD), 2);
    EXPECT_EQ(QtRocket::exitStatus(ExitCode::SIMULATION), 3);
    EXPECT_EQ(QtRocket::exitStatus(ExitCode::IO), 4);
    EXPECT_EQ(QtRocket::exitStatus(ExitCode::INTERNAL), 5);
}

// The executable runs the tool through Cli::runWithStreams() on its standard streams: what it
// prints is in the file its standard output is, byte for byte.
TEST(Cli, TheExecutablePrintsToItsStandardStreams)
{
    const TempDir               directory;
    const std::filesystem::path outFile = directory.resolve("stdout.txt");
    CFile                       out     = openCFile(outFile, CFileMode::WRITE);
    ASSERT_NE(out, nullptr);
    const CliRun version = runCliWithStreams(std::vector<std::string>{"--version"}, {}, out.get());
    // Closed before it is read (Windows would not open it a second time).
    out.reset();
    EXPECT_EQ(version.code, ExitCode::OK);
    EXPECT_EQ(version.err, "");
    EXPECT_EQ(QtRocket::readTextFile(outFile), std::string(QtRocket::creatorString()) + "\n");
}

// A standard output that cannot be written (a full disk, a closed descriptor) is exit code 4 with
// a line on the standard error, unless the run failed otherwise: here a stream opened for reading,
// whose every write fails at once (std::print throws), and on Linux /dev/full, where the text
// fits into the buffer and the flush fails.
TEST(Cli, AStandardOutputThatCannotBeWrittenIsAnIoError)
{
    const TempDir directory;
    const CFile   readOnly = openCFile(directory.write("read-only.txt", ""), CFileMode::READ);
    ASSERT_NE(readOnly, nullptr);
    const CliRun version =
        runCliWithStreams(std::vector<std::string>{"--version"}, {}, readOnly.get());
    EXPECT_EQ(version.code, ExitCode::IO);
    EXPECT_EQ(version.err, "QtRocket_cli: cannot write the standard output\n");

    // An aborted simulation prints its summary: its own code stays, the line is added.
    const ThrustCurveMotorSetDatabase noMotors;
    const CliRun                      aborted = runCliWithStreams(
        std::vector<std::string>{"simulate", cliFixtureArgument("no-motors.ork")},
        Cli::Settings{.dataDirectory = {}, .motorDatabase = &noMotors}, readOnly.get());
    EXPECT_EQ(aborted.code, ExitCode::SIMULATION);
    EXPECT_NE(aborted.err.find("QtRocket_cli: simulation 1 'Without a motor' was aborted"),
              std::string::npos)
        << aborted.err;
    EXPECT_TRUE(aborted.err.ends_with("\nQtRocket_cli: cannot write the standard output\n"))
        << aborted.err;

    // Nothing was to be written to the standard output: no line.
    const CliRun usage = runCliWithStreams(std::vector<std::string>{}, {}, readOnly.get());
    EXPECT_EQ(usage.code, ExitCode::USAGE);
    EXPECT_EQ(usage.err,
              "QtRocket_cli: no command given\nRun 'QtRocket_cli --help' for the usage.\n");

#ifdef __linux__
    const CFile full = openCFile("/dev/full", CFileMode::WRITE);
    ASSERT_NE(full, nullptr);
    const CliRun help = runCliWithStreams(std::vector<std::string>{"--help"}, {}, full.get());
    EXPECT_EQ(help.code, ExitCode::IO);
    EXPECT_EQ(help.err, "QtRocket_cli: cannot write the standard output\n");
#endif
}

TEST(Cli, AMotorDatabaseThatCannotBeReadIsAnIoError)
{
    const TempDir directory;
    // No motors/initial_motors.db in the data directory.
    const CliRun missing = runCli(
        {"motors"}, Cli::Settings{.dataDirectory = directory.path(), .motorDatabase = nullptr});
    EXPECT_EQ(missing.code, ExitCode::IO);
    EXPECT_NE(missing.err.find("QtRocket_cli: cannot read the motor database '"), std::string::npos)
        << missing.err;

    // --db wins over a database handed in, and it is read before the design is loaded.
    const ThrustCurveMotorSetDatabase handedIn;
    const std::filesystem::path       broken = directory.write("broken.db", "not a database");
    const CliRun                      given =
        runCli({"info", "x.ork", "--db", pathToUtf8(broken)},
               Cli::Settings{.dataDirectory = directory.path(), .motorDatabase = &handedIn});
    EXPECT_EQ(given.code, ExitCode::IO);
    EXPECT_TRUE(given.out.empty());
}

TEST(Cli, MotorsListsAndSearchesTheDatabaseAndLoadsAMotorFile)
{
    const CliRun search = runCliWithMotors({"motors", "--search", "ESTES  a8"});
    EXPECT_EQ(search.code, ExitCode::OK);
    EXPECT_TRUE(search.out.starts_with("Manufacturer\tDesignation\t")) << search.out;
    EXPECT_NE(search.out.find("\nEstes\tA8\tA8\tSingle-use\t"), std::string::npos);
    EXPECT_TRUE(everyMotorLineStartsWith(search.out, "Estes\tA8\tA8\tSingle-use\t"));

    const CliRun none = runCliWithMotors({"motors", "--search", "no such motor"});
    EXPECT_EQ(none.code, ExitCode::OK);
    EXPECT_EQ(outputLines(none.out).size(), 1U) << none.out;

    const CliRun load =
        runCli({"motors", "--load", pathToUtf8(testDataDir() / "motors" / "Estes_A8.rse")}, {});
    EXPECT_EQ(load.code, ExitCode::OK);
    EXPECT_NE(load.out.find("\t3,5\t22aec01287ea1e3b8c6f66b26fe5fea6\n"), std::string::npos)
        << load.out;

    const CliRun missing = runCli({"motors", "--load", "no such motor.eng"}, {});
    EXPECT_EQ(missing.code, ExitCode::LOAD);
    EXPECT_NE(missing.err.find("QtRocket_cli: cannot load 'no such motor.eng': "),
              std::string::npos);
}

// --db reads the motor database file it names, the repository's here, as the executable reads its
// own: the whole table is the same as that of the database the tests read, one line per motor.
TEST(Cli, MotorsListsTheDatabaseThatDbNames)
{
    const CliRun listed =
        runCli({"motors", "--db", pathToUtf8(dataDir() / "motors" / "initial_motors.db")},
               Cli::Settings{.dataDirectory = {}, .motorDatabase = nullptr});
    EXPECT_EQ(listed.code, ExitCode::OK);
    EXPECT_TRUE(listed.err.empty()) << listed.err;
    EXPECT_EQ(outputLines(listed.out).size(), 1 + motorCount(bundledMotorDatabase()));
    EXPECT_EQ(listed.out, runCliWithMotors({"motors"}).out);
}

// 'info' settles the automatic dimensions: the launch CG of the first motor configuration of
// "Dual parachute deployment" is then that of mass.json (0.9620825931703152; as loaded it is off
// by about 1e-3 m). Configurations are named by their motors' designations (the factory
// preferences), and --config takes such a name before a number.
TEST(Cli, InfoPrintsEveryConfigurationOfTheSettledDesign)
{
    const std::string design = exampleArgument("Dual parachute deployment.ork");
    const CliRun      info   = runCliWithMotors({"info", design});
    EXPECT_EQ(info.code, ExitCode::OK);
    EXPECT_TRUE(info.err.empty()) << info.err;
    EXPECT_TRUE(info.out.starts_with("Design: " + design + "\nRocket: ")) << info.out;
    EXPECT_NE(info.out.find("\nSimulations: 6\n  1: Simulation 1\n"), std::string::npos);
    EXPECT_NE(info.out.find("Flight configurations: 7\n"), std::string::npos);
    EXPECT_NE(info.out.find("Configuration 0: [No motors] (default)\n"), std::string::npos);
    EXPECT_NE(info.out.find("Configuration 4: [H999N-P] (selected)\n"), std::string::npos);
    EXPECT_NEAR(numberAfter(info.out, "Configuration 1: ", "CG"), 0.9620825931703152, 1e-12);

    const CliRun named = runCliWithMotors({"info", design, "--config", "[J570W-P]", "--mach", "2"});
    EXPECT_EQ(named.code, ExitCode::OK);
    EXPECT_EQ(named.out.find("Configuration 1: "), std::string::npos);
    EXPECT_NE(named.out.find("\nConfiguration 3: [J570W-P]\n"), std::string::npos) << named.out;
    EXPECT_NE(named.out.find("\n  CP at Mach 2, angle of attack 0: "), std::string::npos);
    EXPECT_NE(named.out.find("\n  worst CP at Mach 2: "), std::string::npos);

    const CliRun numbered = runCliWithMotors({"info", design, "--config", "6"});
    EXPECT_EQ(numbered.code, ExitCode::OK);
    EXPECT_EQ(numbered.out.find("Configuration 1: "), std::string::npos);
    EXPECT_NE(numbered.out.find("Configuration 6: "), std::string::npos);

    const CliRun none = runCliWithMotors({"info", design, "--config", "7"});
    EXPECT_EQ(none.code, ExitCode::USAGE);
    EXPECT_TRUE(none.err.starts_with("QtRocket_cli: ")) << none.err;

    const CliRun missing =
        runCliWithMotors({"info", pathToUtf8(testDataDir() / "no such design.ork")});
    EXPECT_EQ(missing.code, ExitCode::LOAD);
    EXPECT_TRUE(missing.out.empty());
    EXPECT_NE(missing.err.find("QtRocket_cli: cannot load '"), std::string::npos);

    const CliRun corrupt = runCliWithMotors({"info", cliFixtureArgument("malformed.ork")});
    EXPECT_EQ(corrupt.code, ExitCode::LOAD);
    EXPECT_NE(corrupt.err.find("QtRocket_cli: cannot load '"), std::string::npos);
}

TEST(Cli, SimulateSeedsSelectsAndNumbersTheCsvFiles)
{
    const TempDir directory;
    const CliRun  seeded =
        runCliWithMotors({"simulate", exampleArgument("A simple model rocket.ork"), "--sim",
                          "Simulation 2", "--seed", "-7"});
    EXPECT_EQ(seeded.code, ExitCode::OK);
    EXPECT_TRUE(seeded.out.starts_with("Simulation 2: Simulation 2\n")) << seeded.out;
    EXPECT_NE(seeded.out.find("  random seed: -7\n"), std::string::npos);
    EXPECT_EQ(seeded.out.find("  CSV: "), std::string::npos);

    const CliRun all =
        runCliWithMotors({"simulate", exampleArgument("Chute release.ork"), "--all", "--calm",
                          "--csv", pathToUtf8(directory.resolve("flight.csv"))});
    EXPECT_EQ(all.code, ExitCode::OK) << all.err;
    EXPECT_TRUE(std::filesystem::is_regular_file(directory.resolve("flight-1.csv")));
    EXPECT_TRUE(std::filesystem::is_regular_file(directory.resolve("flight-2.csv")));
    EXPECT_FALSE(std::filesystem::exists(directory.resolve("flight.csv")));

    const CliRun unknown =
        runCliWithMotors({"simulate", exampleArgument("A simple model rocket.ork"), "--sim", "6"});
    EXPECT_EQ(unknown.code, ExitCode::USAGE);
    EXPECT_NE(unknown.err.find("has no simulation '6'"), std::string::npos);
}

// The CSV files are checked before the first simulation runs, so that nothing is printed of a
// command that cannot finish: a missing directory and a directory are exit code 4, the design
// itself as the CSV file exit code 1 (the design is left as it was).
TEST(Cli, SimulateChecksTheCsvFilesBeforeItRuns)
{
    const TempDir               directory;
    const std::filesystem::path missing    = directory.resolve("missing") / "out.csv";
    const CliRun                unwritable = runCliWithMotors(
        {"simulate", exampleArgument("Chute release.ork"), "--all", "--csv", pathToUtf8(missing)});
    EXPECT_EQ(unwritable.code, ExitCode::IO);
    EXPECT_TRUE(unwritable.out.empty()) << unwritable.out;
    // Simulation 1 writes out-1.csv.
    EXPECT_EQ(unwritable.err, "QtRocket_cli: cannot write '" +
                                  pathToUtf8(directory.resolve("missing") / "out-1.csv") +
                                  "': the directory '" + pathToUtf8(directory.resolve("missing")) +
                                  "' does not exist\n");
    EXPECT_FALSE(std::filesystem::exists(directory.resolve("missing")));

    const CliRun intoDirectory =
        runCliWithMotors({"simulate", exampleArgument("A simple model rocket.ork"), "--csv",
                          pathToUtf8(directory.path())});
    EXPECT_EQ(intoDirectory.code, ExitCode::IO);
    EXPECT_TRUE(intoDirectory.out.empty()) << intoDirectory.out;
    EXPECT_EQ(intoDirectory.err, "QtRocket_cli: cannot write '" + pathToUtf8(directory.path()) +
                                     "': it is a directory\n");

    const std::filesystem::path design = directory.resolve("design.ork");
    std::filesystem::copy_file(testDataDir() / "cli" / "no-motors.ork", design);
    const std::string before = QtRocket::readTextFile(design).value_or("");
    EXPECT_FALSE(before.empty());
    const ThrustCurveMotorSetDatabase noMotors;
    const CliRun itself = runCli({"simulate", pathToUtf8(design), "--csv", pathToUtf8(design)},
                                 Cli::Settings{.dataDirectory = {}, .motorDatabase = &noMotors});
    EXPECT_EQ(itself.code, ExitCode::USAGE);
    EXPECT_TRUE(itself.out.empty()) << itself.out;
    EXPECT_EQ(itself.err,
              "QtRocket_cli: the CSV file '" + pathToUtf8(design) + "' is the design itself\n");
    EXPECT_EQ(QtRocket::readTextFile(design), before);
}

// The seed a run prints repeats its flight: the same output again with --seed (the design's wind
// is turbulent, so the seed matters).
TEST(Cli, APrintedSeedRepeatsTheFlight)
{
    const std::string design = exampleArgument("A simple model rocket.ork");
    const CliRun      first  = runCliWithMotors({"simulate", design});
    EXPECT_EQ(first.code, ExitCode::OK) << first.err;
    const std::string key   = "\n  random seed: ";
    const std::size_t begin = first.out.find(key);
    ASSERT_NE(begin, std::string::npos) << first.out;
    const std::size_t start = begin + key.size();
    const std::string seed  = first.out.substr(start, first.out.find('\n', start) - start);
    const CliRun      again = runCliWithMotors({"simulate", design, "--seed", seed});
    EXPECT_EQ(again.code, ExitCode::OK) << again.err;
    EXPECT_EQ(again.out, first.out);
}

// tests/data/cli/no-motors.ork: a simulation of a configuration without motors ends in its first
// step with a SIM_ABORT event ("No motors defined in the simulation"), an ordinary abort: exit code
// 3, its summary printed and its CSV written all the same. It needs no motor at all.
TEST(Cli, AnAbortedSimulationIsASimulationError)
{
    const TempDir                     directory;
    const ThrustCurveMotorSetDatabase noMotors;
    const std::filesystem::path       csv = directory.resolve("aborted.csv");
    const CliRun                      aborted =
        runCli({"simulate", cliFixtureArgument("no-motors.ork"), "--csv", pathToUtf8(csv)},
               Cli::Settings{.dataDirectory = {}, .motorDatabase = &noMotors});
    EXPECT_EQ(aborted.code, ExitCode::SIMULATION);
    EXPECT_TRUE(aborted.out.starts_with("Simulation 1: Without a motor\n")) << aborted.out;
    EXPECT_NE(aborted.out.find(" s: SIM_ABORT: No motors defined in the simulation\n"),
              std::string::npos)
        << aborted.out;
    EXPECT_NE(aborted.out.find("\n  CSV: " + pathToUtf8(csv) + "\n"), std::string::npos);
    EXPECT_NE(aborted.err.find("QtRocket_cli: simulation 1 'Without a motor' was aborted: No "
                               "motors defined in the simulation\n"),
              std::string::npos)
        << aborted.err;

    const QtRocket::Result<std::string> text = QtRocket::readTextFile(csv);
    ASSERT_TRUE(text.has_value());
    EXPECT_TRUE(text->starts_with("# Without a motor\n")) << text->substr(0, 100);
    EXPECT_NE(text->find("\n# Event SIM_ABORT occurred at t=0 seconds\n"), std::string::npos)
        << *text;
}

#ifdef __linux__
// On Linux a file name is bytes, which need not be UTF-8: an argument names the file of its own
// bytes (a Latin-1 e with acute accent here, byte E9), for the design, --csv and motors --load.
TEST(Cli, AFileNameThatIsNotUtf8NamesItsOwnFile)
{
    const TempDir               directory;
    const std::filesystem::path design = directory.path() / std::string("lat\xE9.ork");
    std::filesystem::copy_file(testDataDir() / "cli" / "no-motors.ork", design);
    const ThrustCurveMotorSetDatabase noMotors;
    const Cli::Settings               settings{.dataDirectory = {}, .motorDatabase = &noMotors};

    const CliRun info = runCli({"info", design.native()}, settings);
    EXPECT_EQ(info.code, ExitCode::OK) << info.err;

    const std::filesystem::path csv = directory.path() / std::string("fl\xFCg.csv");
    const CliRun simulate = runCli({"simulate", design.native(), "--csv", csv.native()}, settings);
    EXPECT_EQ(simulate.code, ExitCode::SIMULATION) << simulate.err;
    EXPECT_TRUE(std::filesystem::is_regular_file(csv));

    const std::filesystem::path motor = directory.path() / std::string("A8-\xE9.rse");
    std::filesystem::copy_file(testDataDir() / "motors" / "Estes_A8.rse", motor);
    const CliRun load = runCli({"motors", "--load", motor.native()}, settings);
    EXPECT_EQ(load.code, ExitCode::OK) << load.err;
    EXPECT_NE(load.out.find("\tA8\t"), std::string::npos) << load.out;
}
#endif

}  // namespace
