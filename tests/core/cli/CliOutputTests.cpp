// Tests of cli/CliOutput.h. QtRocket's own.

#include "QtRocket/cli/CliOutput.h"

#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "QtRocket/logging/WarningSet.h"

namespace
{

using QtRocket::CliOutput;
using QtRocket::WarningSet;

TEST(CliOutput, WithoutSinksItKeepsWhatWasWritten)
{
    CliOutput output;
    output.outLine("apogee: {} m", 50.5);
    output.writeOut("x");
    output.errLine("warning: {}", "text");
    EXPECT_EQ(output.out(), "apogee: 50.5 m\nx");
    EXPECT_EQ(output.err(), "warning: text\n");
}

TEST(CliOutput, WithSinksItHandsEveryTextOnAndKeepsNothing)
{
    std::string out;
    std::string err;
    CliOutput   output([&out](std::string_view text) { out += text; },
                       [&err](std::string_view text) { err += text; });
    output.outLine("{}", 1);
    output.errLine("{}", 2);
    EXPECT_EQ(out, "1\n");
    EXPECT_EQ(err, "2\n");
    EXPECT_TRUE(output.out().empty());
    EXPECT_TRUE(output.err().empty());
}

TEST(CliOutput, WarningsGoToTheStandardErrorOneALine)
{
    WarningSet warnings;
    warnings.add(std::string_view("First."));
    warnings.add(std::string_view("Second."));
    CliOutput output;
    output.warningLines(warnings);
    EXPECT_EQ(output.err(), "warning: First.\nwarning: Second.\n");
    EXPECT_TRUE(output.out().empty());
}

}  // namespace
