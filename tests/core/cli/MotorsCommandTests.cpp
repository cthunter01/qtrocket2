// Tests of the search of cli/MotorsCommand.h (a subset of swing MotorRowFilter's). QtRocket's
// own: MotorRowFilterTest tests the motor chooser's filter as a whole (Milestone 3); the command
// itself is run in CliTests.cpp.

#include "QtRocket/cli/MotorsCommand.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace
{

using QtRocket::MotorsCommand;

// MotorRowFilter.setSearchTerms() of searchField.getText().trim().split("\\s+").
TEST(MotorsCommand, TheSearchTextIsSplitAtWhitespaceAndCaseFolded)
{
    EXPECT_EQ(MotorsCommand::searchTerms("  Estes\tA8\n"),
              (std::vector<std::string>{"estes", "a8"}));
    EXPECT_EQ(MotorsCommand::searchTerms("AeroTech  H128W\r\f\x0BX"),
              (std::vector<std::string>{"aerotech", "h128w", "x"}));
    EXPECT_TRUE(MotorsCommand::searchTerms(" \t ").empty());
    EXPECT_TRUE(MotorsCommand::searchTerms("").empty());
    // Beyond ASCII as Java folds the case (Strings::javaCaseFold()).
    EXPECT_EQ(MotorsCommand::searchTerms("\xC3\x96L"), (std::vector<std::string>{"\xC3\xB6l"}));
}

}  // namespace
