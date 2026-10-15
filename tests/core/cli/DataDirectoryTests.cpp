// Tests of cli/DataDirectory.h, the command-line tool's calls to the operating system. QtRocket's
// own.

#include "QtRocket/cli/DataDirectory.h"

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#ifdef _WIN32
#include <cstddef>
#include <cstdlib>
#include <span>
#endif

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
    std::array<char, 5>  program{"prog"};
    std::array<char, 5>  first{"info"};
    std::array<char, 8>  second{"a b.ork"};
    std::array<char*, 3> argv{program.data(), first.data(), second.data()};
#ifdef _WIN32
    // Windows reads the arguments of this process from GetCommandLineW(), whatever argv says:
    // they are the test's own (under ctest its --gtest_filter=...), those the C runtime has in
    // __argc and __argv, the program left out. (The tool's binary tests pass one beyond ASCII,
    // tests/core/cli/CMakeLists.txt.)
    const std::vector<std::string> own = DataDirectory::commandLineArguments(0, nullptr);
    EXPECT_EQ(DataDirectory::commandLineArguments(3, argv.data()), own);
    const std::span<char*> runtime(__argv, static_cast<std::size_t>(__argc));
    ASSERT_EQ(own.size() + 1, runtime.size());
    for (std::size_t i = 1; i < runtime.size(); i++)
    {
        EXPECT_EQ(own[i - 1], runtime[i]);
    }
#else
    EXPECT_EQ(DataDirectory::commandLineArguments(3, argv.data()),
              (std::vector<std::string>{"info", "a b.ork"}));
    EXPECT_TRUE(DataDirectory::commandLineArguments(0, nullptr).empty());
#endif
}

// An argument names a file: on Windows it is UTF-8 (commandLineArguments() made it so), elsewhere
// the file's name in its own bytes, which need not be UTF-8 (a Latin-1 e with acute accent, byte
// E9, is a Linux file name of its own).
TEST(DataDirectory, AnArgumentNamesThePathOfItsText)
{
    EXPECT_EQ(DataDirectory::pathFromArgument("a b.ork"), std::filesystem::path("a b.ork"));
    // U+00FC.
    EXPECT_EQ(DataDirectory::pathFromArgument("Flug-\xC3\xBC.csv"),
              QtRocket::pathFromUtf8("Flug-\xC3\xBC.csv"));
#ifdef _WIN32
    // U+FFFD for the malformed byte, as pathFromUtf8() reads it.
    EXPECT_EQ(DataDirectory::pathFromArgument("lat\xE9.ork"),
              QtRocket::pathFromUtf8("lat\xEF\xBF\xBD.ork"));
#else
    EXPECT_EQ(DataDirectory::pathFromArgument("lat\xE9.ork").native(), std::string("lat\xE9.ork"));
#endif
}

}  // namespace
