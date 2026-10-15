// Tests of cli/CliArguments.h, the parsing of QtRocket_cli's command line. QtRocket's own:
// OpenRocket has no command line.

#include "QtRocket/cli/CliArguments.h"

#include <initializer_list>
#include <string>
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
[[nodiscard]] std::string errorOf(std::initializer_list<const char*> arguments)
{
    const Result<CliArguments> parsed = parse(arguments);
    if (parsed.has_value())
    {
        return "(parsed)";
    }
    EXPECT_EQ(parsed.error().code, ErrorCode::INVALID_ARGUMENT);
    return parsed.error().message;
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

TEST(CliArguments, CommandNames)
{
    EXPECT_EQ(CliArguments::commandName(Command::HELP), "--help");
    EXPECT_EQ(CliArguments::commandName(Command::VERSION), "--version");
    EXPECT_EQ(CliArguments::commandName(Command::INFO), "info");
    EXPECT_EQ(CliArguments::commandName(Command::SIMULATE), "simulate");
    EXPECT_EQ(CliArguments::commandName(Command::MOTORS), "motors");
}

}  // namespace
