#include "QtRocket/gui/app/DataPaths.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/util/FileIo.h"
#include "TestPaths.h"

namespace QtRocket::Gui
{

namespace
{

using DataPathsTest = Test::GuiTest;

/// The names of the .ork files in @p directory, sorted.
[[nodiscard]] std::vector<std::string> designFileNames(const std::filesystem::path& directory)
{
    std::vector<std::string> names;
    std::error_code          error;
    for (const auto& entry : std::filesystem::directory_iterator(directory, error))
    {
        if (entry.path().extension() == ".ork")
        {
            names.push_back(pathToUtf8(entry.path().filename()));
        }
    }
    std::ranges::sort(names);
    return names;
}

TEST_F(DataPathsTest, IsShareQtRocketBesideTheProgramsDirectory)
{
    const std::filesystem::path prefix = std::filesystem::path("opt") / "qtrocket";
    const DataPaths             paths  = DataPaths::forApplicationDirectory(prefix / "bin");
    EXPECT_EQ(paths.dataDirectory(), prefix / "share" / "QtRocket");
    EXPECT_EQ(paths.motorDirectory(), prefix / "share" / "QtRocket" / "motors");
    EXPECT_EQ(paths.motorDatabase(),
              prefix / "share" / "QtRocket" / "motors" / "initial_motors.db");
    EXPECT_EQ(paths.examplesDirectory(), prefix / "share" / "QtRocket" / "examples");
}

TEST_F(DataPathsTest, KeepsTheDirectoryItIsGiven)
{
    const std::filesystem::path directory = std::filesystem::path("data");
    EXPECT_EQ(DataPaths(directory).dataDirectory(), directory);
    EXPECT_EQ(DataPaths(directory).motorDatabase(), directory / "motors" / "initial_motors.db");
}

TEST_F(DataPathsTest, TheBuildTreeHoldsTheMotorDatabaseAndTheExamples)
{
    // The test program lies in the build tree's bin/, beside the copy of data/ (QtRocket_data)
    // in share/QtRocket, as the application does.
    const DataPaths paths = DataPaths::forApplication();
    EXPECT_EQ(paths.dataDirectory().filename(), std::filesystem::path("QtRocket"));
    std::error_code error;
    ASSERT_TRUE(std::filesystem::is_regular_file(paths.motorDatabase(), error))
        << pathToUtf8(paths.motorDatabase());
    EXPECT_EQ(std::filesystem::file_size(paths.motorDatabase(), error),
              std::filesystem::file_size(QtRocket::Test::dataDir() / "motors" / "initial_motors.db",
                                         error));
    const std::vector<std::string> examples = designFileNames(paths.examplesDirectory());
    EXPECT_EQ(examples.size(), 16U);
    EXPECT_EQ(examples, designFileNames(QtRocket::Test::dataDir() / "examples"));
}

}  // namespace

}  // namespace QtRocket::Gui
