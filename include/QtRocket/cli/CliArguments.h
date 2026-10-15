#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// The command line of QtRocket_cli, parsed (QtRocket's own; OpenRocket has no command line):
///
///     QtRocket_cli --version | --help
///     QtRocket_cli info FILE [--config NAME|NUMBER] [--mach M] [--db FILE]
///     QtRocket_cli simulate FILE [--sim NAME|NUMBER | --all] [--calm] [--seed N]
///                  [--time-step S] [--csv OUT] [--db FILE]
///     QtRocket_cli motors [--search TEXT] [--db FILE]
///     QtRocket_cli motors --load FILE
///
/// The command comes first; the options of a command and its FILE may come in any order after
/// it. An option's value is the next argument ("--sim 2"; also one that starts with '-', as
/// "--seed -7") or follows an '=' ("--sim=2"). "--" ends the options: every argument after it is
/// a FILE. "--help" or "-h" anywhere asks for the help.
///
/// What parse() refuses (ErrorCode::INVALID_ARGUMENT, with a text that names the argument; the
/// tool's exit code 1): no command, an unknown command, an option the command does not take, an
/// option without its value or given twice, a value to an option that takes none, a second
/// FILE, a command without its FILE, '--sim' with '--all', '--load' with '--search' or '--db',
/// and a value out of its range: '--seed' not an integer from -2147483648 to 2147483647 (Java's
/// int), '--time-step' not a number in SimulationInputRanges::kTimeStep (from 0.0001 to 10 s),
/// '--mach' not a number in SimulationInputRanges::kLookupMach (from 0 to 100). Numbers are read
/// with Strings::parseDouble() and Strings::parseInt(), which do not depend on the locale.
///
/// Names and paths are kept as the text they were given in (UTF-8 on Windows, the argument's own
/// bytes elsewhere); a command makes a path of one with DataDirectory::pathFromArgument().
struct CliArguments
{
    /// What the tool is asked to do.
    enum class Command
    {
        /// Print the usage (--help, -h).
        HELP,
        /// Print the version (--version).
        VERSION,
        /// Print the numbers of a design's flight configurations.
        INFO,
        /// Run simulations of a design.
        SIMULATE,
        /// List the motors of the motor database or of a motor file.
        MOTORS,
    };

    Command command{Command::HELP};
    /// The design file of 'info' and 'simulate'.
    std::string file;
    /// --db: the motor database to read instead of the one of the data directory.
    std::optional<std::string> database;

    /// info --config: the flight configuration to print (all when not given).
    std::optional<std::string> configuration;
    /// info --mach: the Mach number of the CP (the preferences' default Mach number, 0.3, when not
    /// given).
    std::optional<double> mach;

    /// simulate --sim: the simulation to run (the first when not given).
    std::optional<std::string> simulation;
    /// simulate --all: run every simulation.
    bool allSimulations{false};
    /// simulate --calm: no turbulence (the standard deviation of every wind model 0).
    bool calm{false};
    /// simulate --seed: the random seed of every simulation that runs.
    std::optional<int> seed;
    /// simulate --time-step: the time step of every simulation that runs, in s.
    std::optional<double> timeStep;
    /// simulate --csv: the CSV file of the flight data.
    std::optional<std::string> csv;

    /// motors --search: the words a motor must contain.
    std::optional<std::string> search;
    /// motors --load: the motor file to list instead of the database.
    std::optional<std::string> motorFile;

    /// The arguments @p arguments (the program's name left out) as a command; the Error says
    /// what is wrong with them (see the class comment).
    [[nodiscard]] static Result<CliArguments> parse(std::span<const std::string> arguments);

    /// The name of @p command on the command line ("info"; "--help" and "--version").
    [[nodiscard]] static std::string_view commandName(Command command) noexcept;
};

}  // namespace QtRocket
