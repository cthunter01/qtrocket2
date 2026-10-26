#pragma once

// Running the command-line tool QtRocket_cli in the test process (Cli::run()) and reading what it
// printed: what the tests of tests/core/cli share. Test-only.
//
// The commands that need motors are handed the bundled motor database (bundledMotorDatabase(),
// read once per test process), as the executable reads it from its data directory; --db still
// reads its own file. The commands that open a design are handed component presets too, the six
// of the examples unless a test asks for the bundled ones (runCliWithMotors()). The binary
// itself is run by tests/core/cli/CMakeLists.txt.

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/cli/Cli.h"
#include "QtRocket/cli/ExitCode.h"
#include "file/DesignFileEnvironment.h"

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
/// tree as its data directory, and with the component presets @p presets
/// (DesignFileEnvironment::presetDatabase()) for 'info' and 'simulate'. By default these are the
/// six presets of the example designs, made at no cost, with which every example loads as with
/// the bundled presets the tool reads from its data directory
/// (ExampleFilesWithTheBundledPresets); Presets::BUNDLED hands in the bundled presets themselves,
/// read once per test process (a second and a half in a debug build, several seconds under the
/// sanitizers: keep such tests few).
[[nodiscard]] CliRun runCliWithMotors(
    const std::vector<std::string>& arguments,
    DesignFileEnvironment::Presets  presets = DesignFileEnvironment::Presets::EXAMPLES);

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

/// Closes a C stream (std::fclose()).
struct CloseCFile
{
    void operator()(std::FILE* file) const noexcept;
};

/// A C stream that is closed when it goes.
using CFile = std::unique_ptr<std::FILE, CloseCFile>;

/// How openCFile() opens a file.
enum class CFileMode : std::uint8_t
{
    /// For reading, in binary mode ("rb"): every write to the stream fails.
    READ,
    /// For writing, in binary mode ("wb"), the file replaced.
    WRITE,
};

/// The file @p path opened as a C stream with @p mode; null when it cannot be opened. (On
/// Windows the file cannot be opened a second time while the stream is open: close it before
/// reading what was written.)
[[nodiscard]] CFile openCFile(const std::filesystem::path& path, CFileMode mode);

/// What Cli::runWithStreams() of @p arguments with @p settings gave, as the executable runs it,
/// with the standard output @p out and as its standard error a file of its own, read back
/// afterwards: the exit code and the standard error (CliRun::out stays empty).
[[nodiscard]] CliRun runCliWithStreams(std::span<const std::string> arguments,
                                       const Cli::Settings& settings, std::FILE* out);

}  // namespace QtRocket::Test
