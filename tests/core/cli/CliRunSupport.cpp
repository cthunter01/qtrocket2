#include "cli/CliRunSupport.h"

#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/cli/Cli.h"
#include "QtRocket/cli/CliOutput.h"
#include "QtRocket/cli/ExitCode.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"
#include "TestPaths.h"
#include "TestTempDir.h"
#include "file/DesignFileEnvironment.h"
#include "motor/TestMotorDatabase.h"

namespace QtRocket::Test
{

CliRun runCli(const std::vector<std::string>& arguments, const Cli::Settings& settings)
{
    CliOutput      output;
    const ExitCode code = Cli::run(arguments, settings, output);
    return CliRun{.code = code, .out = output.out(), .err = output.err()};
}

CliRun runCliWithMotors(const std::vector<std::string>& arguments,
                        DesignFileEnvironment::Presets  presets)
{
    const ComponentPresetDatabase database = DesignFileEnvironment::presetDatabase(presets);
    return runCli(arguments, Cli::Settings{.dataDirectory  = dataDir(),
                                           .motorDatabase  = &bundledMotorDatabase(),
                                           .presetDatabase = &database});
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

void CloseCFile::operator()(std::FILE* file) const noexcept
{
    // The std::unique_ptr of CFile owns the stream; there is no gsl::owner here.
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    static_cast<void>(std::fclose(file));
}

CFile openCFile(const std::filesystem::path& path, CFileMode mode)
{
    const bool write = mode == CFileMode::WRITE;
#ifdef _WIN32
    // _wfopen_s: the path as Windows spells it (fopen() would take it in the ANSI code page),
    // and MSVC rejects fopen() as unsafe.
    std::FILE* file = nullptr;
    if (_wfopen_s(&file, path.c_str(), write ? L"wb" : L"rb") != 0)
    {
        return nullptr;
    }
    return CFile(file);
#else
    return CFile(std::fopen(path.c_str(), write ? "wb" : "rb"));
#endif
}

CliRun runCliWithStreams(std::span<const std::string> arguments, const Cli::Settings& settings,
                         std::FILE* out)
{
    const TempDir               directory;
    const std::filesystem::path errFile = directory.resolve("stderr.txt");
    ExitCode                    code    = ExitCode::INTERNAL;
    {
        const CFile err = openCFile(errFile, CFileMode::WRITE);
        if (err == nullptr)
        {
            ADD_FAILURE() << "cannot open " << pathToUtf8(errFile);
            return CliRun{.code = code, .out = {}, .err = {}};
        }
        code = Cli::runWithStreams(arguments, settings, out, err.get());
    }
    const Result<std::string> err = readTextFile(errFile);
    EXPECT_TRUE(err.has_value());
    return CliRun{.code = code, .out = {}, .err = err.value_or("")};
}

}  // namespace QtRocket::Test
