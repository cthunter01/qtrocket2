#include "QtRocket/cli/Cli.h"

#include <cstdio>
#include <exception>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

#include "QtRocket/cli/CliArguments.h"
#include "QtRocket/cli/CliEnvironment.h"
#include "QtRocket/cli/CliOutput.h"
#include "QtRocket/cli/DataDirectory.h"
#include "QtRocket/cli/ExitCode.h"
#include "QtRocket/cli/InfoCommand.h"
#include "QtRocket/cli/MotorsCommand.h"
#include "QtRocket/cli/SimulateCommand.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Version.h"

namespace QtRocket
{

namespace
{

/// Runs a command that needs the motor database @p motors.
[[nodiscard]] ExitCode runWithMotors(const CliArguments&                arguments,
                                     const ThrustCurveMotorSetDatabase& motors, CliOutput& output)
{
    if (arguments.command == CliArguments::Command::MOTORS)
    {
        return MotorsCommand::list(motors, arguments.search, output);
    }
    CliEnvironment environment(motors);
    if (arguments.command == CliArguments::Command::INFO)
    {
        return InfoCommand::run(arguments, environment, output);
    }
    return SimulateCommand::run(arguments, environment, output);
}

/// Runs the command of @p arguments.
[[nodiscard]] ExitCode runCommand(const CliArguments& arguments, const Cli::Settings& settings,
                                  CliOutput& output)
{
    switch (arguments.command)
    {
        case CliArguments::Command::HELP:
            output.writeOut(Cli::usage());
            return ExitCode::OK;
        case CliArguments::Command::VERSION:
            output.outLine("{}", creatorString());
            return ExitCode::OK;
        case CliArguments::Command::MOTORS:
        case CliArguments::Command::INFO:
        case CliArguments::Command::SIMULATE:
            break;
    }
    if (arguments.motorFile.has_value())
    {
        return MotorsCommand::load(*arguments.motorFile, output);
    }
    if (settings.motorDatabase != nullptr && !arguments.database.has_value())
    {
        return runWithMotors(arguments, *settings.motorDatabase, output);
    }
    const std::filesystem::path file =
        arguments.database.has_value()
            ? DataDirectory::pathFromArgument(*arguments.database)
            : CliEnvironment::defaultMotorDatabase(settings.dataDirectory);
    Result<ThrustCurveMotorSetDatabase> motors = CliEnvironment::readMotorDatabase(file);
    if (!motors)
    {
        output.errLine("QtRocket_cli: cannot read the motor database '{}': {}", pathToUtf8(file),
                       motors.error().message);
        return ExitCode::IO;
    }
    return runWithMotors(arguments, *motors, output);
}

}  // namespace

ExitCode Cli::run(std::span<const std::string> arguments, const Settings& settings,
                  CliOutput& output)
{
    try
    {
        const Result<CliArguments> parsed = CliArguments::parse(arguments);
        if (!parsed)
        {
            output.errLine("QtRocket_cli: {}", parsed.error().message);
            output.errLine("Run 'QtRocket_cli --help' for the usage.");
            return ExitCode::USAGE;
        }
        return runCommand(*parsed, settings, output);
    }
    catch (const std::exception& error)
    {
        // A BugError (a std::logic_error) or anything else: a defect of the program.
        output.errLine("QtRocket_cli: internal error: {}", error.what());
        return ExitCode::INTERNAL;
    }
    catch (...)
    {
        output.errLine("QtRocket_cli: internal error");
        return ExitCode::INTERNAL;
    }
}

ExitCode Cli::runWithStreams(std::span<const std::string> arguments, const Settings& settings,
                             std::FILE* out, std::FILE* err)
{
    CliOutput      output(out, err);
    const ExitCode code = run(arguments, settings, output);
    if (!output.outFailed())
    {
        return code;
    }
    output.errLine("QtRocket_cli: cannot write the standard output");
    return code == ExitCode::OK ? ExitCode::IO : code;
}

std::string_view Cli::usage() noexcept
{
    return R"(Usage: QtRocket_cli <command> [options]

Commands:
  info FILE [--config NAME|NUMBER] [--mach M] [--db FILE]
      Print the length, the masses, the CG, the CP and the stability of every
      flight configuration of the design FILE, or of the one --config names
      (its exact name, else its number as 'info' prints it: 0 is the default
      configuration). The CP is computed at Mach M (default 0.3).

  simulate FILE [--sim NAME|NUMBER | --all] [--calm] [--seed N]
           [--time-step S] [--csv OUT] [--db FILE]
      Run the first simulation of the design FILE, the one --sim names (its
      exact name, else its number from 1), or with --all every simulation, and
      print the summary, the events and the warnings of each flight.
        --calm         no turbulence: the standard deviation of the wind is 0
                       (the average wind stays)
        --seed N       the random seed (default: a new one, unless the design
                       fixes one; the seed used is printed)
        --time-step S  the time step in seconds, from 0.0001 to 10
        --csv OUT      write the flight data of the first branch to the CSV
                       file OUT; with several simulations, simulation N writes
                       to OUT with -N before its extension

  motors [--search TEXT] [--db FILE]
      List the motors of the motor database: all, or those whose manufacturer,
      designation, common name or case contains every word of TEXT (ignoring
      case).

  motors --load FILE
      List the motors of a motor file (.eng, .rse, .zip or .db).

  --version
      Print the version.

  --help
      Print this help.

--db FILE reads the motors from the motor database FILE instead of
motors/initial_motors.db in the data directory (share/QtRocket beside the
directory of the program).

Every value is printed in SI units. The warnings of a load go to the standard
error.

Exit codes:
  0  success
  1  usage error: a wrong command, option or value, or a simulation or flight
     configuration the design does not have
  2  a design file or a motor file cannot be loaded
  3  a simulation fails or is aborted
  4  an output file or the standard output cannot be written, or the motor
     database cannot be read
  5  internal error
)";
}

}  // namespace QtRocket
