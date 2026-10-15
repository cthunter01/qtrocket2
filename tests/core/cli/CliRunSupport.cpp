#include "cli/CliRunSupport.h"

#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/cli/Cli.h"
#include "QtRocket/cli/CliOutput.h"
#include "QtRocket/cli/ExitCode.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"
#include "TestPaths.h"
#include "motor/TestMotorDatabase.h"

namespace QtRocket::Test
{

CliRun runCli(const std::vector<std::string>& arguments, const Cli::Settings& settings)
{
    CliOutput      output;
    const ExitCode code = Cli::run(arguments, settings, output);
    return CliRun{.code = code, .out = output.out(), .err = output.err()};
}

CliRun runCliWithMotors(const std::vector<std::string>& arguments)
{
    return runCli(arguments, Cli::Settings{.dataDirectory = dataDir(),
                                           .motorDatabase = &bundledMotorDatabase()});
}

std::string exampleArgument(std::string_view name)
{
    return pathToUtf8(dataDir() / "examples" / pathFromUtf8(name));
}

std::string cliFixtureArgument(std::string_view name)
{
    return pathToUtf8(testDataDir() / "cli" / pathFromUtf8(name));
}

double numberAfter(std::string_view text, std::string_view after, std::string_view label)
{
    const std::string key   = "\n  " + std::string(label) + ": ";
    const std::size_t start = text.find(after);
    const std::size_t line  = start == std::string_view::npos ? start : text.find(key, start);
    if (line == std::string_view::npos)
    {
        ADD_FAILURE() << "no line '" << label << "' after '" << after << "' in\n" << text;
        return std::numeric_limits<double>::quiet_NaN();
    }
    const std::size_t           begin = line + key.size();
    const std::size_t           end   = text.find_first_of(" \n", begin);
    const std::optional<double> value = Strings::parseDouble(text.substr(begin, end - begin));
    if (!value.has_value())
    {
        ADD_FAILURE() << "no number in the line '" << label << "'";
        return std::numeric_limits<double>::quiet_NaN();
    }
    return *value;
}

std::vector<std::string> outputLines(std::string_view text)
{
    std::vector<std::string> lines = Strings::split(text, '\n');
    if (!lines.empty() && lines.back().empty())
    {
        lines.pop_back();
    }
    return lines;
}

}  // namespace QtRocket::Test
