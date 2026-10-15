// Tests of cli/CliArguments.h, the parsing of QtRocket_cli's command line. QtRocket's own:
// OpenRocket has no command line.

#include "QtRocket/cli/CliArguments.h"

#include <algorithm>
#include <array>
#include <format>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/util/Error.h"

namespace
{

using QtRocket::CliArguments;
using QtRocket::ErrorCode;
using QtRocket::Result;
using Command = QtRocket::CliArguments::Command;

/// parse() of @p arguments.
[[nodiscard]] Result<CliArguments> parse(std::initializer_list<const char*> arguments)
{
    const std::vector<std::string> list(arguments.begin(), arguments.end());
    return CliArguments::parse(list);
}

/// The message of the Error parse() gives for @p arguments; "(parsed)" when it parses.
[[nodiscard]] std::string errorOf(const std::vector<std::string>& arguments)
{
    const Result<CliArguments> parsed = CliArguments::parse(arguments);
    if (parsed.has_value())
    {
        return "(parsed)";
    }
    EXPECT_EQ(parsed.error().code, ErrorCode::INVALID_ARGUMENT);
    return parsed.error().message;
}

/// errorOf() of @p arguments.
[[nodiscard]] std::string errorOf(std::initializer_list<const char*> arguments)
{
    return errorOf(std::vector<std::string>(arguments.begin(), arguments.end()));
}

/// An option of the command line and a value it accepts (empty for a flag, which takes none).
struct OptionCase
{
    std::string_view option;
    std::string_view value;
};

constexpr std::array<OptionCase, 3> kInfoOptions{{
    {.option = "--config", .value = "1"},
    {.option = "--mach", .value = "0.5"},
    {.option = "--db", .value = "m.db"},
}};
constexpr std::array<OptionCase, 7> kSimulateOptions{{
    {.option = "--sim", .value = "1"},
    {.option = "--all", .value = ""},
    {.option = "--calm", .value = ""},
    {.option = "--seed", .value = "1"},
    {.option = "--time-step", .value = "0.01"},
    {.option = "--csv", .value = "o.csv"},
    {.option = "--db", .value = "m.db"},
}};
constexpr std::array<OptionCase, 3> kMotorsOptions{{
    {.option = "--search", .value = "a"},
    {.option = "--load", .value = "a.eng"},
    {.option = "--db", .value = "m.db"},
}};

/// A command, its file, and the options it takes.
struct CommandCase
{
    std::vector<std::string>    start;  ///< the command and its file ("info", "x.ork")
    std::string                 name;   ///< the command's name in the messages
    std::span<const OptionCase> options;
};

/// The three commands.
[[nodiscard]] std::vector<CommandCase> commandCases()
{
    return {
        {.start = {"info", "x.ork"}, .name = "info", .options = kInfoOptions},
        {.start = {"simulate", "x.ork"}, .name = "simulate", .options = kSimulateOptions},
        {.start = {"motors"}, .name = "motors", .options = kMotorsOptions},
    };
}

/// @p command's start with @p option (and its value, if it takes one) once.
[[nodiscard]] std::vector<std::string> withOption(const CommandCase& command,
                                                  const OptionCase&  option)
{
    std::vector<std::string> arguments = command.start;
    arguments.emplace_back(option.option);
    if (!option.value.empty())
    {
        arguments.emplace_back(option.value);
    }
    return arguments;
}

/// Whether @p options holds @p option.
[[nodiscard]] bool holds(std::span<const OptionCase> options, std::string_view option)
{
    return std::ranges::any_of(
        options, [option](const OptionCase& candidate) { return candidate.option == option; });
}

/// Where errorOf() of @p arguments is not @p expected: "<arguments>: <message>", or nothing.
void expectError(const std::vector<std::string>& arguments, const std::string& expected,
                 std::vector<std::string>& mismatches)
{
    const std::string message = errorOf(arguments);
    if (message != expected)
    {
        std::string line;
        for (const std::string& argument : arguments)
        {
            line += argument + ' ';
        }
        mismatches.push_back(line + "gives: " + message);
    }
}

/// The mismatches of the options of every command given to each command that does not take
/// them.
[[nodiscard]] std::vector<std::string> foreignOptionMismatches()
{
    std::vector<std::string>       mismatches;
    const std::vector<CommandCase> commands = commandCases();
    for (const CommandCase& command : commands)
    {
        for (const CommandCase& other : commands)
        {
            for (const OptionCase& option : other.options)
            {
                if (!holds(command.options, option.option))
                {
                    expectError(
                        withOption(command, option),
                        std::format("unknown option '{}' for '{}'", option.option, command.name),
                        mismatches);
                }
            }
        }
    }
    return mismatches;
}

/// The mismatches of every option of every command given twice, and of every option that
/// takes a value given without one.
[[nodiscard]] std::vector<std::string> repeatedAndMissingValueMismatches()
{
    std::vector<std::string> mismatches;
    for (const CommandCase& command : commandCases())
    {
        for (const OptionCase& option : command.options)
        {
            std::vector<std::string> twice = withOption(command, option);
            twice.emplace_back(option.option);
            if (!option.value.empty())
            {
                twice.emplace_back(option.value);
                std::vector<std::string> missing = command.start;
                missing.emplace_back(option.option);
                expectError(missing, std::format("option '{}' needs a value", option.option),
                            mismatches);
            }
            expectError(twice, std::format("option '{}' is given twice", option.option),
                        mismatches);
        }
    }
    return mismatches;
}

/// The mismatches of every option of every command given alone with its value, after '=' too:
/// each parses.
[[nodiscard]] std::vector<std::string> acceptedOptionMismatches()
{
    std::vector<std::string> mismatches;
    for (const CommandCase& command : commandCases())
    {
        for (const OptionCase& option : command.options)
        {
            expectError(withOption(command, option), "(parsed)", mismatches);
            if (!option.value.empty())
            {
                std::vector<std::string> equals = command.start;
                equals.push_back(std::format("{}={}", option.option, option.value));
                expectError(equals, "(parsed)", mismatches);
            }
        }
    }
    return mismatches;
}

TEST(CliArguments, TheCommandComesFirstAndItsFileAnywhereAfterIt)
{
    const Result<CliArguments> info = parse({"info", "--mach", "0.5", "rocket.ork"});
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->command, Command::INFO);
    EXPECT_EQ(info->file, "rocket.ork");
    EXPECT_EQ(info->mach, 0.5);
    EXPECT_FALSE(info->configuration.has_value());
    EXPECT_FALSE(info->database.has_value());

    const Result<CliArguments> simulate =
        parse({"simulate", "a b.ork", "--calm", "--seed", "7", "--time-step", "0.01", "--csv",
               "out.csv", "--sim", "Simulation 2", "--db", "motors.db"});
    ASSERT_TRUE(simulate.has_value());
    EXPECT_EQ(simulate->command, Command::SIMULATE);
    EXPECT_EQ(simulate->file, "a b.ork");
    EXPECT_TRUE(simulate->calm);
    EXPECT_EQ(simulate->seed, 7);
    EXPECT_EQ(simulate->timeStep, 0.01);
    EXPECT_EQ(simulate->csv, "out.csv");
    EXPECT_EQ(simulate->simulation, "Simulation 2");
    EXPECT_EQ(simulate->database, "motors.db");
    EXPECT_FALSE(simulate->allSimulations);

    const Result<CliArguments> motors = parse({"motors", "--search", "estes a8"});
    ASSERT_TRUE(motors.has_value());
    EXPECT_EQ(motors->command, Command::MOTORS);
    EXPECT_EQ(motors->search, "estes a8");
    EXPECT_TRUE(motors->file.empty());

    const Result<CliArguments> load = parse({"motors", "--load", "A8.rse"});
    ASSERT_TRUE(load.has_value());
    EXPECT_EQ(load->motorFile, "A8.rse");
}

TEST(CliArguments, AValueFollowsTheOptionOrAnEqualsSign)
{
    const Result<CliArguments> equals = parse({"simulate", "--sim=2", "--seed=-7", "x.ork"});
    ASSERT_TRUE(equals.has_value());
    EXPECT_EQ(equals->simulation, "2");
    EXPECT_EQ(equals->seed, -7);

    // The next argument is the value even when it starts with '-'.
    const Result<CliArguments> next = parse({"simulate", "x.ork", "--seed", "-7", "--sim", "-1"});
    ASSERT_TRUE(next.has_value());
    EXPECT_EQ(next->seed, -7);
    EXPECT_EQ(next->simulation, "-1");

    // An empty value after '=' is a value.
    const Result<CliArguments> empty = parse({"motors", "--search="});
    ASSERT_TRUE(empty.has_value());
    EXPECT_EQ(empty->search, "");
}

TEST(CliArguments, TwoDashesEndTheOptions)
{
    const Result<CliArguments> dashed = parse({"info", "--", "--help"});
    ASSERT_TRUE(dashed.has_value());
    EXPECT_EQ(dashed->command, Command::INFO);
    EXPECT_EQ(dashed->file, "--help");

    const Result<CliArguments> lone = parse({"info", "-"});
    ASSERT_TRUE(lone.has_value());
    EXPECT_EQ(lone->file, "-");
}

TEST(CliArguments, HelpAnywhereAsksForTheHelpAndVersionStandsAlone)
{
    EXPECT_EQ(parse({"--help"}).value().command, Command::HELP);
    EXPECT_EQ(parse({"-h"}).value().command, Command::HELP);
    EXPECT_EQ(parse({"simulate", "x.ork", "--help"}).value().command, Command::HELP);
    EXPECT_EQ(parse({"bogus", "-h"}).value().command, Command::HELP);
    EXPECT_EQ(parse({"--version"}).value().command, Command::VERSION);
    EXPECT_EQ(errorOf({"--version", "x"}), "'--version' takes no arguments");
}

TEST(CliArguments, WhatIsRefusedIsNamed)
{
    EXPECT_EQ(errorOf({}), "no command given");
    EXPECT_EQ(errorOf({"bogus"}), "unknown command 'bogus'");
    EXPECT_EQ(errorOf({"info", "x.ork", "--calm"}), "unknown option '--calm' for 'info'");
    EXPECT_EQ(errorOf({"info", "x.ork", "-x"}), "unknown option '-x' for 'info'");
    EXPECT_EQ(errorOf({"motors", "--sim", "1"}), "unknown option '--sim' for 'motors'");
    EXPECT_EQ(errorOf({"simulate", "x.ork", "--sim"}), "option '--sim' needs a value");
    EXPECT_EQ(errorOf({"simulate", "x.ork", "--all=yes"}), "option '--all' takes no value");
    EXPECT_EQ(errorOf({"simulate", "x.ork", "--calm", "--calm"}), "option '--calm' is given twice");
    EXPECT_EQ(errorOf({"simulate", "x.ork", "--seed", "1", "--seed", "1"}),
              "option '--seed' is given twice");
    EXPECT_EQ(errorOf({"info", "a.ork", "b.ork"}), "unexpected argument 'b.ork'");
    EXPECT_EQ(errorOf({"motors", "a.eng"}), "unexpected argument 'a.eng'");
    EXPECT_EQ(errorOf({"info"}), "'info' needs a design file");
    EXPECT_EQ(errorOf({"simulate", "--calm"}), "'simulate' needs a design file");
    EXPECT_EQ(errorOf({"simulate", "x.ork", "--sim", "1", "--all"}),
              "'--sim' and '--all' cannot be given together");
    EXPECT_EQ(errorOf({"motors", "--load", "a.eng", "--search", "x"}),
              "'--load' cannot be given together with '--search' or '--db'");
    EXPECT_EQ(errorOf({"motors", "--load", "a.eng", "--db", "m.db"}),
              "'--load' cannot be given together with '--search' or '--db'");
}

// The ranges are SimulationInputRanges::kTimeStep and kLookupMach, and Java's int.
TEST(CliArguments, NumbersAreCheckedAgainstTheirRanges)
{
    EXPECT_EQ(parse({"simulate", "x.ork", "--time-step", "0.0001"}).value().timeStep, 0.0001);
    EXPECT_EQ(parse({"simulate", "x.ork", "--time-step", "10"}).value().timeStep, 10.0);
    EXPECT_EQ(errorOf({"simulate", "x.ork", "--time-step", "0"}),
              "the time step must be a number from 0.0001 to 10 s, not '0'");
    EXPECT_EQ(errorOf({"simulate", "x.ork", "--time-step", "10.5"}),
              "the time step must be a number from 0.0001 to 10 s, not '10.5'");
    EXPECT_EQ(errorOf({"simulate", "x.ork", "--time-step", "NaN"}),
              "the time step must be a number from 0.0001 to 10 s, not 'NaN'");
    EXPECT_EQ(errorOf({"simulate", "x.ork", "--time-step", "0.01s"}),
              "the time step must be a number from 0.0001 to 10 s, not '0.01s'");

    EXPECT_EQ(parse({"simulate", "x.ork", "--seed", "-2147483648"}).value().seed, -2147483647 - 1);
    EXPECT_EQ(parse({"simulate", "x.ork", "--seed", "2147483647"}).value().seed, 2147483647);
    EXPECT_EQ(errorOf({"simulate", "x.ork", "--seed", "2147483648"}),
              "the random seed must be an integer from -2147483648 to 2147483647, not "
              "'2147483648'");
    EXPECT_EQ(errorOf({"simulate", "x.ork", "--seed", "1.5"}),
              "the random seed must be an integer from -2147483648 to 2147483647, not '1.5'");

    EXPECT_EQ(parse({"info", "x.ork", "--mach", "0"}).value().mach, 0.0);
    EXPECT_EQ(parse({"info", "x.ork", "--mach", "100"}).value().mach, 100.0);
    EXPECT_EQ(errorOf({"info", "x.ork", "--mach", "-0.1"}),
              "the Mach number must be a number from 0 to 100, not '-0.1'");
    EXPECT_EQ(errorOf({"info", "x.ork", "--mach", "Infinity"}),
              "the Mach number must be a number from 0 to 100, not 'Infinity'");
}

TEST(CliArguments, WithoutOptionsEverythingIsLeftToItsDefault)
{
    const Result<CliArguments> simulate = parse({"simulate", "x.ork"});
    ASSERT_TRUE(simulate.has_value());
    EXPECT_EQ(simulate->command, Command::SIMULATE);
    EXPECT_FALSE(simulate->database.has_value());
    EXPECT_FALSE(simulate->simulation.has_value());
    EXPECT_FALSE(simulate->allSimulations);
    EXPECT_FALSE(simulate->calm);
    EXPECT_FALSE(simulate->seed.has_value());
    EXPECT_FALSE(simulate->timeStep.has_value());
    EXPECT_FALSE(simulate->csv.has_value());

    const Result<CliArguments> motors = parse({"motors"});
    ASSERT_TRUE(motors.has_value());
    EXPECT_EQ(motors->command, Command::MOTORS);
    EXPECT_TRUE(motors->file.empty());
    EXPECT_FALSE(motors->search.has_value());
    EXPECT_FALSE(motors->motorFile.has_value());
    EXPECT_FALSE(motors->database.has_value());
}

TEST(CliArguments, EveryOptionOfEachCommand)
{
    const Result<CliArguments> info =
        parse({"info", "--config=[H669N-P]", "--db", "motors.db", "x.ork", "--mach", "2"});
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->configuration, "[H669N-P]");
    EXPECT_EQ(info->database, "motors.db");
    EXPECT_EQ(info->mach, 2.0);

    const Result<CliArguments> all = parse({"simulate", "--all", "x.ork"});
    ASSERT_TRUE(all.has_value());
    EXPECT_TRUE(all->allSimulations);
    EXPECT_FALSE(all->simulation.has_value());

    const Result<CliArguments> motors = parse({"motors", "--db=m.db", "--search", "H128"});
    ASSERT_TRUE(motors.has_value());
    EXPECT_EQ(motors->database, "m.db");
    EXPECT_EQ(motors->search, "H128");

    // Every option of every command alone, its value as the next argument and after '='.
    EXPECT_EQ(acceptedOptionMismatches(), std::vector<std::string>{});
}

TEST(CliArguments, ACommandRefusesTheOptionsOfTheOthers)
{
    EXPECT_EQ(foreignOptionMismatches(), std::vector<std::string>{});
    EXPECT_EQ(errorOf({"info", "x.ork", "--version"}), "unknown option '--version' for 'info'");
    EXPECT_EQ(errorOf({"info", "x.ork", "--calm=1"}), "unknown option '--calm' for 'info'");
    EXPECT_EQ(errorOf({"simulate", "x.ork", "--bogus=1"}),
              "unknown option '--bogus' for 'simulate'");
    // An option where the command belongs is no command.
    EXPECT_EQ(errorOf({"--calm"}), "unknown command '--calm'");
    EXPECT_EQ(errorOf({"-x", "info"}), "unknown command '-x'");
}

TEST(CliArguments, EveryOptionIsGivenOnceAndWithItsValue)
{
    EXPECT_EQ(repeatedAndMissingValueMismatches(), std::vector<std::string>{});
    EXPECT_EQ(errorOf({"simulate", "x.ork", "--calm=1"}), "option '--calm' takes no value");
    // A value after '=' and the same option again are still twice.
    EXPECT_EQ(errorOf({"info", "x.ork", "--mach=0.5", "--mach", "0.6"}),
              "option '--mach' is given twice");
}

TEST(CliArguments, EveryArgumentAfterTwoDashesIsAFile)
{
    EXPECT_EQ(errorOf({"info", "a.ork", "--", "b.ork"}), "unexpected argument 'b.ork'");
    EXPECT_EQ(errorOf({"info", "a.ork", "--", "--calm"}), "unexpected argument '--calm'");
    EXPECT_EQ(errorOf({"motors", "--", "a.eng"}), "unexpected argument 'a.eng'");
    EXPECT_EQ(errorOf({"simulate", "--", "--"}), "(parsed)");
    EXPECT_EQ(parse({"simulate", "--calm", "--", "-x.ork"}).value().file, "-x.ork");
}

// Strings::parseDouble() and Strings::parseInt(), as Java's Double.parseDouble and
// Integer.parseInt read numbers.
TEST(CliArguments, NumbersAreReadAsJavaReadsThem)
{
    EXPECT_EQ(parse({"simulate", "x.ork", "--time-step", "1e-3"}).value().timeStep, 0.001);
    EXPECT_EQ(parse({"simulate", "x.ork", "--time-step", ".05"}).value().timeStep, 0.05);
    EXPECT_EQ(parse({"simulate", "x.ork", "--seed", "+5"}).value().seed, 5);
    EXPECT_EQ(parse({"info", "x.ork", "--mach=1.5E0"}).value().mach, 1.5);
    EXPECT_EQ(errorOf({"simulate", "x.ork", "--seed", "0x10"}),
              "the random seed must be an integer from -2147483648 to 2147483647, not '0x10'");
    EXPECT_EQ(errorOf({"simulate", "x.ork", "--seed", " 5"}),
              "the random seed must be an integer from -2147483648 to 2147483647, not ' 5'");
    EXPECT_EQ(errorOf({"info", "x.ork", "--mach", "0,5"}),
              "the Mach number must be a number from 0 to 100, not '0,5'");
    EXPECT_EQ(errorOf({"info", "x.ork", "--mach="}),
              "the Mach number must be a number from 0 to 100, not ''");
}

TEST(CliArguments, CommandNames)
{
    EXPECT_EQ(CliArguments::commandName(Command::HELP), "--help");
    EXPECT_EQ(CliArguments::commandName(Command::VERSION), "--version");
    EXPECT_EQ(CliArguments::commandName(Command::INFO), "info");
    EXPECT_EQ(CliArguments::commandName(Command::SIMULATE), "simulate");
    EXPECT_EQ(CliArguments::commandName(Command::MOTORS), "motors");
}

}  // namespace
