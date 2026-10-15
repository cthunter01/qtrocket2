#pragma once

// Running the command-line tool QtRocket_cli in the test process (Cli::run()) and reading what it
// printed: what tests/core/cli/CliTests.cpp and cli_golden_tests.cpp share. Test-only.
//
// The commands that need motors are handed the bundled motor database (bundledMotorDatabase(),
// read once per test process), as the executable reads it from its data directory; --db still
// reads its own file. The binary itself is run by tests/core/cli/CMakeLists.txt.

#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/cli/Cli.h"
#include "QtRocket/cli/ExitCode.h"

namespace QtRocket::Test
{

/// What a run of the tool gave: its exit code and what it wrote to its standard output and
/// error.
struct CliRun
{
    ExitCode    code;
    std::string out;
    std::string err;
};

/// Runs the tool with @p arguments (the program's name left out) and @p settings.
[[nodiscard]] CliRun runCli(const std::vector<std::string>& arguments,
                            const Cli::Settings&            settings);

/// Runs the tool with @p arguments over the bundled motor database, with data/ of the source
/// tree as its data directory.
[[nodiscard]] CliRun runCliWithMotors(const std::vector<std::string>& arguments);

/// The example design @p name of data/examples ("A simple model rocket.ork") as an argument of
/// the tool (UTF-8).
[[nodiscard]] std::string exampleArgument(std::string_view name);

/// The fixture @p name of tests/data/cli as an argument of the tool (UTF-8).
[[nodiscard]] std::string cliFixtureArgument(std::string_view name);

/// The number on the first line "  <label>: <number>..." of @p text after the first occurrence of
/// @p after (a line the tool indents by two spaces under a heading, see InfoCommand.h and
/// SimulateCommand.h); NaN, and a failure of the test, when there is no such line or no number
/// on it.
[[nodiscard]] double numberAfter(std::string_view text, std::string_view after,
                                 std::string_view label);

/// The lines of @p text, each without its '\n'; nothing for the empty text after the last line
/// end.
[[nodiscard]] std::vector<std::string> outputLines(std::string_view text);

}  // namespace QtRocket::Test
