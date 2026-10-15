#include "QtRocket/cli/CliArguments.h"

#include <array>
#include <cstddef>
#include <expected>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "QtRocket/simulation/SimulationInputRanges.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

using Command = CliArguments::Command;

/// The options of the commands.
enum class Option
{
    DB,
    CONFIG,
    MACH,
    SIM,
    ALL,
    CALM,
    SEED,
    TIME_STEP,
    CSV,
    SEARCH,
    LOAD,
};

/// An option as the command line spells it.
struct OptionSpelling
{
    std::string_view name;
    Option           option;
    /// Whether the option takes a value ("--sim 2"); a flag ("--all") takes none.
    bool takesValue;
};

constexpr std::array<OptionSpelling, 11> kOptions{{
    {.name = "--db", .option = Option::DB, .takesValue = true},
    {.name = "--config", .option = Option::CONFIG, .takesValue = true},
    {.name = "--mach", .option = Option::MACH, .takesValue = true},
    {.name = "--sim", .option = Option::SIM, .takesValue = true},
    {.name = "--all", .option = Option::ALL, .takesValue = false},
    {.name = "--calm", .option = Option::CALM, .takesValue = false},
    {.name = "--seed", .option = Option::SEED, .takesValue = true},
    {.name = "--time-step", .option = Option::TIME_STEP, .takesValue = true},
    {.name = "--csv", .option = Option::CSV, .takesValue = true},
    {.name = "--search", .option = Option::SEARCH, .takesValue = true},
    {.name = "--load", .option = Option::LOAD, .takesValue = true},
}};

/// The option spelled @p name, or nullopt.
[[nodiscard]] std::optional<OptionSpelling> findOption(std::string_view name)
{
    for (const OptionSpelling& spelling : kOptions)
    {
        if (spelling.name == name)
        {
            return spelling;
        }
    }
    return std::nullopt;
}

/// Whether @p command takes @p option.
[[nodiscard]] bool takesOption(Command command, Option option)
{
    switch (command)
    {
        case Command::INFO:
            return option == Option::DB || option == Option::CONFIG || option == Option::MACH;
        case Command::SIMULATE:
            return option == Option::DB || option == Option::SIM || option == Option::ALL ||
                   option == Option::CALM || option == Option::SEED ||
                   option == Option::TIME_STEP || option == Option::CSV;
        case Command::MOTORS:
            return option == Option::DB || option == Option::SEARCH || option == Option::LOAD;
        case Command::HELP:
        case Command::VERSION:
            return false;
    }
    bug("unknown command");
}

/// Whether @p argument asks for the help.
[[nodiscard]] bool isHelp(std::string_view argument)
{
    return argument == "--help" || argument == "-h";
}

/// Whether @p argument is an option rather than a file (before "--"): it starts with '-' and is
/// not "-" alone.
[[nodiscard]] bool isOption(std::string_view argument)
{
    return argument.size() > 1 && argument.front() == '-';
}

/// The command @p name names.
[[nodiscard]] Result<Command> parseCommand(std::string_view name)
{
    if (name == "info")
    {
        return Command::INFO;
    }
    if (name == "simulate")
    {
        return Command::SIMULATE;
    }
    if (name == "motors")
    {
        return Command::MOTORS;
    }
    if (name == "--version")
    {
        return Command::VERSION;
    }
    return fail(ErrorCode::INVALID_ARGUMENT, std::format("unknown command '{}'", name));
}

/// @p text as a number in @p range; @p what names the number in the Error ("the time step").
[[nodiscard]] Result<double> parseNumberIn(std::string_view                    text,
                                           const SimulationInputRanges::Range& range,
                                           std::string_view                    what)
{
    const std::optional<double> value = Strings::parseDouble(text);
    if (!value.has_value() || !range.contains(*value))
    {
        return fail(ErrorCode::INVALID_ARGUMENT,
                    std::format("{} must be a number {}, not '{}'", what, range.text, text));
    }
    return *value;
}

/// @p text as a random seed: an int, as Java's.
[[nodiscard]] Result<int> parseSeed(std::string_view text)
{
    const std::optional<int> value = Strings::parseInt(text);
    if (!value.has_value())
    {
        return fail(ErrorCode::INVALID_ARGUMENT,
                    std::format("the random seed must be an integer from -2147483648 to "
                                "2147483647, not '{}'",
                                text));
    }
    return *value;
}

/// The failure of an option given twice.
[[nodiscard]] std::unexpected<Error> givenTwice(std::string_view name)
{
    return fail(ErrorCode::INVALID_ARGUMENT, std::format("option '{}' is given twice", name));
}

/// Sets @p target to @p value, or fails when the option @p name was given before.
[[nodiscard]] Result<void> setOnce(std::optional<std::string>& target, std::string_view name,
                                   std::string_view value)
{
    if (target.has_value())
    {
        return givenTwice(name);
    }
    target = std::string(value);
    return {};
}

/// Sets the flag @p target, or fails when the option @p name was given before.
[[nodiscard]] Result<void> setFlagOnce(bool& target, std::string_view name)
{
    if (target)
    {
        return givenTwice(name);
    }
    target = true;
    return {};
}

/// Applies the option @p option, spelled @p name, that takes a number, with @p value to
/// @p arguments.
[[nodiscard]] Result<void> applyNumberOption(CliArguments& arguments, Option option,
                                             std::string_view name, std::string_view value)
{
    if (option == Option::MACH)
    {
        if (arguments.mach.has_value())
        {
            return givenTwice(name);
        }
        const Result<double> mach =
            parseNumberIn(value, SimulationInputRanges::kLookupMach, "the Mach number");
        if (!mach)
        {
            return std::unexpected(mach.error());
        }
        arguments.mach = *mach;
        return {};
    }
    if (option == Option::TIME_STEP)
    {
        if (arguments.timeStep.has_value())
        {
            return givenTwice(name);
        }
        const Result<double> timeStep =
            parseNumberIn(value, SimulationInputRanges::kTimeStep, "the time step");
        if (!timeStep)
        {
            return std::unexpected(timeStep.error());
        }
        arguments.timeStep = *timeStep;
        return {};
    }
    QTROCKET_ASSERT(option == Option::SEED);
    if (arguments.seed.has_value())
    {
        return givenTwice(name);
    }
    const Result<int> seed = parseSeed(value);
    if (!seed)
    {
        return std::unexpected(seed.error());
    }
    arguments.seed = *seed;
    return {};
}

/// Applies the option @p option, spelled @p name, with @p value (empty for a flag) to
/// @p arguments.
[[nodiscard]] Result<void> applyOption(CliArguments& arguments, Option option,
                                       std::string_view name, std::string_view value)
{
    switch (option)
    {
        case Option::DB:
            return setOnce(arguments.database, name, value);
        case Option::CONFIG:
            return setOnce(arguments.configuration, name, value);
        case Option::SIM:
            return setOnce(arguments.simulation, name, value);
        case Option::CSV:
            return setOnce(arguments.csv, name, value);
        case Option::SEARCH:
            return setOnce(arguments.search, name, value);
        case Option::LOAD:
            return setOnce(arguments.motorFile, name, value);
        case Option::ALL:
            return setFlagOnce(arguments.allSimulations, name);
        case Option::CALM:
            return setFlagOnce(arguments.calm, name);
        case Option::MACH:
        case Option::TIME_STEP:
        case Option::SEED:
            return applyNumberOption(arguments, option, name, value);
    }
    bug("unknown option");
}

/// Checks the options of @p arguments together, once all are read.
[[nodiscard]] Result<void> checkCombination(const CliArguments& arguments)
{
    const Command command = arguments.command;
    if ((command == Command::INFO || command == Command::SIMULATE) && arguments.file.empty())
    {
        return fail(ErrorCode::INVALID_ARGUMENT,
                    std::format("'{}' needs a design file", CliArguments::commandName(command)));
    }
    if (arguments.simulation.has_value() && arguments.allSimulations)
    {
        return fail(ErrorCode::INVALID_ARGUMENT, "'--sim' and '--all' cannot be given together");
    }
    if (arguments.motorFile.has_value() &&
        (arguments.search.has_value() || arguments.database.has_value()))
    {
        return fail(ErrorCode::INVALID_ARGUMENT,
                    "'--load' cannot be given together with '--search' or '--db'");
    }
    return {};
}

/// Reads the option at @p index of @p rest into @p arguments; @p index moves past its value
/// when that is the next argument.
[[nodiscard]] Result<void> readOption(CliArguments& arguments, std::span<const std::string> rest,
                                      std::size_t& index)
{
    const std::string_view              argument = rest[index];
    const std::size_t                   equals   = argument.find('=');
    const std::string_view              name     = argument.substr(0, equals);
    const std::optional<OptionSpelling> spelling = findOption(name);
    const std::string_view              command  = CliArguments::commandName(arguments.command);
    if (!spelling.has_value() || !takesOption(arguments.command, spelling->option))
    {
        return fail(ErrorCode::INVALID_ARGUMENT,
                    std::format("unknown option '{}' for '{}'", name, command));
    }
    if (!spelling->takesValue)
    {
        if (equals != std::string_view::npos)
        {
            return fail(ErrorCode::INVALID_ARGUMENT,
                        std::format("option '{}' takes no value", name));
        }
        return applyOption(arguments, spelling->option, name, {});
    }
    if (equals != std::string_view::npos)
    {
        return applyOption(arguments, spelling->option, name, argument.substr(equals + 1));
    }
    if (index + 1 >= rest.size())
    {
        return fail(ErrorCode::INVALID_ARGUMENT, std::format("option '{}' needs a value", name));
    }
    index++;
    return applyOption(arguments, spelling->option, name, rest[index]);
}

/// Reads the options and the file of a command from @p rest, the arguments after the command.
[[nodiscard]] Result<void> readCommandLine(CliArguments&                arguments,
                                           std::span<const std::string> rest)
{
    bool endOfOptions = false;
    for (std::size_t index = 0; index < rest.size(); index++)
    {
        const std::string_view argument = rest[index];
        if (!endOfOptions && argument == "--")
        {
            endOfOptions = true;
            continue;
        }
        if (!endOfOptions && isOption(argument))
        {
            if (const Result<void> read = readOption(arguments, rest, index); !read)
            {
                return read;
            }
            continue;
        }
        if (arguments.command == Command::MOTORS || !arguments.file.empty())
        {
            return fail(ErrorCode::INVALID_ARGUMENT,
                        std::format("unexpected argument '{}'", argument));
        }
        arguments.file = std::string(argument);
    }
    return checkCombination(arguments);
}

}  // namespace

Result<CliArguments> CliArguments::parse(std::span<const std::string> arguments)
{
    if (arguments.empty())
    {
        return fail(ErrorCode::INVALID_ARGUMENT, "no command given");
    }
    CliArguments parsed;
    for (const std::string& argument : arguments)
    {
        if (argument == "--")
        {
            break;
        }
        if (isHelp(argument))
        {
            return parsed;  // Command::HELP
        }
    }
    const Result<Command> named = parseCommand(arguments.front());
    if (!named)
    {
        return std::unexpected(named.error());
    }
    parsed.command = *named;
    if (parsed.command == Command::VERSION)
    {
        if (arguments.size() > 1)
        {
            return fail(ErrorCode::INVALID_ARGUMENT, "'--version' takes no arguments");
        }
        return parsed;
    }
    if (const Result<void> read = readCommandLine(parsed, arguments.subspan(1)); !read)
    {
        return std::unexpected(read.error());
    }
    return parsed;
}

std::string_view CliArguments::commandName(Command command) noexcept
{
    switch (command)
    {
        case Command::HELP:
            return "--help";
        case Command::VERSION:
            return "--version";
        case Command::INFO:
            return "info";
        case Command::SIMULATE:
            return "simulate";
        case Command::MOTORS:
            return "motors";
    }
    return "?";
}

}  // namespace QtRocket
