// Tests of cli/DataDirectory.h, the command-line tool's calls to the operating system. QtRocket's
// own.

#include "QtRocket/cli/DataDirectory.h"

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/util/FileIo.h"

namespace
{

using QtRocket::DataDirectory;

TEST(DataDirectory, TheDataLiesInShareBesideTheDirectoryOfTheProgram)
{
    const std::filesystem::path root = std::filesystem::path("/") / "opt" / "qtrocket";
    EXPECT_EQ(DataDirectory::forExecutable(root / "bin" / "QtRocket_cli"),
              root / "share" / "QtRocket");
    EXPECT_EQ(DataDirectory::forExecutable(std::filesystem::path("bin") / "QtRocket_cli"),
              std::filesystem::path("share") / "QtRocket");
}

// The test binary lies in bin/ of the build tree like QtRocket_cli, so its data directory is the
// one the build fills (src/cli/CMakeLists.txt): the build tree has the installed layout.
TEST(DataDirectory, TheRunningProgramIsFoundAndTheBuildTreeHasTheInstalledLayout)
{
    const std::optional<std::filesystem::path> executable = DataDirectory::executablePath();
    // A plain if: clang-tidy's unchecked-optional-access check cannot see through ASSERT_TRUE.
    if (!executable.has_value())
    {
        FAIL() << "the operating system does not say where the program is";
    }
    EXPECT_TRUE(executable->is_absolute()) << QtRocket::pathToUtf8(*executable);
    EXPECT_TRUE(std::filesystem::is_regular_file(*executable)) << QtRocket::pathToUtf8(*executable);
    EXPECT_EQ(executable->stem(), std::filesystem::path("QtRocket_core_tests"));

    const std::filesystem::path data = DataDirectory::dataDirectory("");
    EXPECT_EQ(data, DataDirectory::forExecutable(*executable));
    EXPECT_TRUE(std::filesystem::is_regular_file(data / "motors" / "initial_motors.db"))
        << QtRocket::pathToUtf8(data);
}

TEST(DataDirectory, TheArgumentsLeaveTheProgramOut)
{
#ifdef _WIN32
    GTEST_SKIP() << "Windows reads the arguments of the process from GetCommandLineW()";
#else
    std::array<char, 5>  program{"prog"};
    std::array<char, 5>  first{"info"};
    std::array<char, 8>  second{"a b.ork"};
    std::array<char*, 3> argv{program.data(), first.data(), second.data()};
    EXPECT_EQ(DataDirectory::commandLineArguments(3, argv.data()),
              (std::vector<std::string>{"info", "a b.ork"}));
    EXPECT_TRUE(DataDirectory::commandLineArguments(0, nullptr).empty());
#endif
}

}  // namespace
