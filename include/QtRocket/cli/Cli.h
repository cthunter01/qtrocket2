#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <string_view>

#include "QtRocket/cli/ExitCode.h"

namespace QtRocket
{

class CliOutput;
class ThrustCurveMotorSetDatabase;

/// The command-line tool QtRocket_cli (QtRocket's own; OpenRocket has no command line), as a
/// function of its arguments, so that the tests run it in their process: the executable
/// (src/cli/main.cpp) only reads its arguments and its data directory (DataDirectory) and prints
/// what run() writes.
///
/// run() parses the arguments (CliArguments; an error goes to the standard error with a hint to
/// --help, ExitCode::USAGE), then:
/// - --help writes usage(), --version creatorString() ("QtRocket <version>"), to the standard
///   output;
/// - 'motors --load FILE' lists a motor file (MotorsCommand::load());
/// - every other command first reads the motor database: --db FILE, else the one Settings hands
///   in, else CliEnvironment::defaultMotorDatabase() of the data directory. A database that
///   cannot be read is ExitCode::IO. Then 'motors' lists it (MotorsCommand::list()), and 'info'
///   and 'simulate' load their design in a CliEnvironment over it (InfoCommand,
///   SimulateCommand).
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
        /// source tree has the same layout), where the default motor database is.
        std::filesystem::path dataDirectory;
        /// A motor database to use instead of reading the data directory's, or null: for the
        /// tests, which read the database once per process. --db still reads its file. It must
        /// outlive the call.
        const ThrustCurveMotorSetDatabase* motorDatabase{nullptr};
    };

    Cli() = delete;

    /// Runs the tool with @p arguments (main()'s argv without the program's name, as UTF-8),
    /// writing to @p output; the exit code says how it went (see the class comment and
    /// ExitCode).
    [[nodiscard]] static ExitCode run(std::span<const std::string> arguments,
                                      const Settings& settings, CliOutput& output);

    /// The text --help prints: the commands, their options and the exit codes.
    [[nodiscard]] static std::string_view usage() noexcept;
};

}  // namespace QtRocket
