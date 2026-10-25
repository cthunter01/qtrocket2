#pragma once

#include <cstdio>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

#include "QtRocket/cli/ExitCode.h"

namespace QtRocket
{

class CliOutput;
class ComponentPresetDatabase;
class ThrustCurveMotorSetDatabase;

/// The command-line tool QtRocket_cli (QtRocket's own; OpenRocket has no command line), as a
/// function of its arguments, so that the tests run it in their process: the executable
/// (src/cli/main.cpp) only reads its arguments and its data directory (DataDirectory) and calls
/// runWithStreams(), which prints what run() writes.
///
/// run() parses the arguments (CliArguments; an error goes to the standard error with a hint to
/// --help, ExitCode::USAGE), then:
/// - --help writes usage(), --version creatorString() ("QtRocket <version>"), to the standard
///   output;
/// - 'motors --load FILE' lists a motor file (MotorsCommand::load());
/// - every other command first reads the motor database: --db FILE, else the one Settings hands
///   in, else CliEnvironment::defaultMotorDatabase() of the data directory. A database that
///   cannot be read is ExitCode::IO. Then 'motors' lists it (MotorsCommand::list()), and 'info'
///   and 'simulate' load their design in a CliEnvironment over it and over the component presets
///   (InfoCommand, SimulateCommand): the ones Settings hands in, else those the two commands read
///   from the data directory (CliEnvironment::readComponentPresets() of
///   CliEnvironment::defaultComponentPresetDirectory(), the user's decision U-M3-4 of
///   2026-10-24; 'motors', --help and --version read none). What cannot be read of the presets is
///   a line "warning: cannot load the component presets of '<file>': <message>" on the standard
///   error for each problem of the load, then a line "warning: <text>" for each of its warnings
///   (as the warnings of a design's load), and the command goes on with the presets that were
///   read, as OpenRocket logs these problems and goes on. The presets are read after the motor
///   database and before the design.
///
/// A BugError or any other exception that reaches run() is reported on the standard error as an
/// internal error, ExitCode::INTERNAL; nothing is thrown out of run() unless writing that report
/// throws.
class Cli
{
public:
    /// What run() is given beside the arguments.
    struct Settings
    {
        /// The data directory (DataDirectory::dataDirectory() for the executable; data/ of the
        /// source tree has the same layout), where the default motor database and the bundled
        /// component presets are.
        std::filesystem::path dataDirectory;
        /// A motor database to use instead of reading the data directory's, or null: for the
        /// tests, which read the database once per process. --db still reads its file. It must
        /// outlive the call.
        const ThrustCurveMotorSetDatabase* motorDatabase{nullptr};
        /// The component presets of 'info' and 'simulate', to use instead of reading the data
        /// directory's, or null: for the tests, which read the bundled presets once per process
        /// or hand in fewer. It must outlive the call.
        const ComponentPresetDatabase* presetDatabase{nullptr};
    };

    Cli() = delete;

    /// Runs the tool with @p arguments (main()'s argv without the program's name, as UTF-8),
    /// writing to @p output; the exit code says how it went (see the class comment and
    /// ExitCode).
    [[nodiscard]] static ExitCode run(std::span<const std::string> arguments,
                                      const Settings& settings, CliOutput& output);

    /// Runs the tool as the executable does: run() with @p arguments and @p settings, writing to
    /// the C streams @p out and @p err (CliOutput's constructor with streams). When @p out could
    /// not be written in full (CliOutput::outFailed(): a full disk, a closed standard output), a
    /// line "QtRocket_cli: cannot write the standard output" goes to @p err, and the exit code is
    /// ExitCode::IO instead of ExitCode::OK; a code that says that something else failed is
    /// kept.
    [[nodiscard]] static ExitCode runWithStreams(std::span<const std::string> arguments,
                                                 const Settings& settings, std::FILE* out,
                                                 std::FILE* err);

    /// The text --help prints: the commands, their options and the exit codes.
    [[nodiscard]] static std::string_view usage() noexcept;
};

}  // namespace QtRocket
