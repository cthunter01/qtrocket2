#include "QtRocket/gui/app/DataPaths.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <QDir>
#include <QStandardPaths>
#include <QString>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Sha256.h"
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
    EXPECT_EQ(paths.motorMetadata(), prefix / "share" / "QtRocket" / "motors" / "metadata.json");
    EXPECT_EQ(paths.componentDirectory(),
              std::optional(prefix / "share" / "QtRocket" / "components"));
    // The data alone: no user's data directory.
    EXPECT_EQ(paths.userDataDirectory(), std::nullopt);
    EXPECT_EQ(paths.userThrustCurveDirectory(), std::nullopt);
    EXPECT_EQ(paths.userComponentDirectory(), std::nullopt);
}

TEST_F(DataPathsTest, TheUsersDirectoriesLieInTheUsersDataDirectory)
{
    const std::filesystem::path user  = std::filesystem::path("home") / "QtRocket";
    const DataPaths             paths = DataPaths("data").withUserDataDirectory(user);
    EXPECT_EQ(paths.userDataDirectory(), std::optional(user));
    // OpenRocket's getDefaultUserThrustCurveFile() and getDefaultUserComponentFile().
    EXPECT_EQ(paths.userThrustCurveDirectory(), std::optional(user / "ThrustCurves"));
    EXPECT_EQ(paths.userComponentDirectory(), std::optional(user / "Components"));
    EXPECT_EQ(paths.withUserDataDirectory(std::nullopt).userThrustCurveDirectory(), std::nullopt);
    // The other paths stay.
    EXPECT_EQ(paths.dataDirectory(), std::filesystem::path("data"));
    EXPECT_EQ(paths.componentDirectory(),
              std::optional(std::filesystem::path("data") / "components"));
}

TEST_F(DataPathsTest, TheBundledPresetsCanBeReplaced)
{
    const DataPaths paths = DataPaths("data").withUserDataDirectory(std::filesystem::path("home"));
    const DataPaths other = paths.withComponentDirectory(std::filesystem::path("presets"));
    EXPECT_EQ(other.componentDirectory(), std::optional(std::filesystem::path("presets")));
    EXPECT_EQ(other.withComponentDirectory(std::nullopt).componentDirectory(), std::nullopt);
    EXPECT_EQ(other.userDataDirectory(), paths.userDataDirectory());
    EXPECT_EQ(other.motorDirectory(), paths.motorDirectory());
    // The paths it was made of are left as they were.
    EXPECT_EQ(paths.componentDirectory(),
              std::optional(std::filesystem::path("data") / "components"));
}

/// The path of the user's data directory in QStandardPaths' current mode.
[[nodiscard]] std::filesystem::path genericDataLocation()
{
    const QString generic = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    EXPECT_FALSE(generic.isEmpty());
    return QtStrings::toPath(QDir::toNativeSeparators(generic)) / "QtRocket";
}

TEST_F(DataPathsTest, TheTestsHaveNoUsersDataDirectoryAndNoBundledPresets)
{
    // The test seam: in QStandardPaths' test mode (every test, GuiTestMain.cpp) the application's
    // data has neither, so that no test reads the user's files or makes directories in the
    // user's home.
    ASSERT_TRUE(QStandardPaths::isTestModeEnabled());
    const DataPaths paths = DataPaths::forApplication();
    EXPECT_EQ(paths.userDataDirectory(), std::nullopt);
    EXPECT_EQ(paths.userThrustCurveDirectory(), std::nullopt);
    EXPECT_EQ(paths.userComponentDirectory(), std::nullopt);
    EXPECT_EQ(paths.componentDirectory(), std::nullopt);
    EXPECT_EQ(paths.bundledComponentDirectory(), paths.dataDirectory() / "components");
    // The path alone, here the one of that mode: nothing is made or read.
    EXPECT_EQ(DataPaths::standardUserDataDirectory(), std::optional(genericDataLocation()));
}

/// QStandardPaths' test mode turned off while it lives, and on again when it ends.
class WithoutTestMode
{
public:
    WithoutTestMode() { QStandardPaths::setTestModeEnabled(false); }
    ~WithoutTestMode() { QStandardPaths::setTestModeEnabled(true); }
    WithoutTestMode(const WithoutTestMode&)            = delete;
    WithoutTestMode& operator=(const WithoutTestMode&) = delete;
    WithoutTestMode(WithoutTestMode&&)                 = delete;
    WithoutTestMode& operator=(WithoutTestMode&&)      = delete;
};

TEST_F(DataPathsTest, TheApplicationsUsersDataDirectoryIsThePlatformsGenericDataLocation)
{
    // Outside QStandardPaths' test mode, for a moment (only the paths are computed: nothing is
    // made or read).
    const WithoutTestMode       withoutTestMode;
    const std::filesystem::path expected = genericDataLocation();
    EXPECT_EQ(DataPaths::standardUserDataDirectory(), std::optional(expected));
    const DataPaths paths = DataPaths::forApplication();
    EXPECT_EQ(paths.userDataDirectory(), std::optional(expected));
    EXPECT_EQ(paths.userThrustCurveDirectory(), std::optional(expected / "ThrustCurves"));
    EXPECT_EQ(paths.userComponentDirectory(), std::optional(expected / "Components"));
    EXPECT_EQ(paths.componentDirectory(), std::optional(paths.bundledComponentDirectory()));
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

TEST_F(DataPathsTest, TheMotorMetadataIsOpenRocketsByteForByte)
{
    // OpenRocket 5f164fd0e's core/src/main/resources/datafiles/thrustcurves/metadata.json
    // (sha256sum), in data/ and in the build tree's copy.
    constexpr std::size_t      kSize = 363;
    constexpr std::string_view kSha256 =
        "3b6120b00274f2a60eb3c63a80ccf00acd444c636f47f8aecc51f51b58e67769";
    for (const std::filesystem::path& file :
         {QtRocket::Test::dataDir() / "motors" / "metadata.json",
          DataPaths::forApplication().motorMetadata()})
    {
        const Result<std::vector<std::byte>> bytes = readFile(file);
        ASSERT_TRUE(bytes.has_value()) << pathToUtf8(file);
        EXPECT_EQ(bytes->size(), kSize) << pathToUtf8(file);
        EXPECT_EQ(sha256Hex(*bytes), kSha256) << pathToUtf8(file);
    }
}

}  // namespace

}  // namespace QtRocket::Gui
