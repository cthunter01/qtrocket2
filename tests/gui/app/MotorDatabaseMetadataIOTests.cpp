#include "QtRocket/gui/app/MotorDatabaseMetadataIO.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/app/DataPaths.h"
#include "TestPaths.h"
#include "TestTempDir.h"

namespace QtRocket::Gui
{

namespace
{

using MotorDatabaseMetadataIOTest = Test::GuiTest;
using MotorDatabaseMetadataIO::readDatabaseVersion;

/// The version of a metadata file with the text @p text.
[[nodiscard]] std::optional<std::int64_t> versionOf(const QtRocket::Test::TempDir& directory,
                                                    std::string_view               text)
{
    return readDatabaseVersion(directory.write("metadata.json", text));
}

TEST_F(MotorDatabaseMetadataIOTest, ReadsTheBundledDatabasesVersion)
{
    // OpenRocket's datafiles/thrustcurves/metadata.json, in the build tree and in data/.
    EXPECT_EQ(readDatabaseVersion(DataPaths::forApplication().motorMetadata()),
              std::optional<std::int64_t>(20260418111610));
    EXPECT_EQ(readDatabaseVersion(QtRocket::Test::dataDir() / "motors" / "metadata.json"),
              std::optional<std::int64_t>(20260418111610));
}

TEST_F(MotorDatabaseMetadataIOTest, ReadsTheVersionOfAnyObject)
{
    const QtRocket::Test::TempDir directory;
    EXPECT_EQ(versionOf(directory, R"({"database_version": 7})"), std::optional<std::int64_t>(7));
    // Zero and below are versions too; the chooser shows them as unknown.
    EXPECT_EQ(versionOf(directory, R"({"database_version": 0})"), std::optional<std::int64_t>(0));
    EXPECT_EQ(versionOf(directory, R"({"database_version": -3})"), std::optional<std::int64_t>(-3));
    // Beyond the integers a double holds exactly, and written with an exponent.
    EXPECT_EQ(versionOf(directory, R"({"database_version": 9007199254740993})"),
              std::optional<std::int64_t>(9007199254740993));
    EXPECT_EQ(versionOf(directory, R"({"database_version": 2e3})"),
              std::optional<std::int64_t>(2000));
    EXPECT_EQ(versionOf(directory, R"({"schema_version": 2, "database_version": 5, "x": [1]})"),
              std::optional<std::int64_t>(5));
}

TEST_F(MotorDatabaseMetadataIOTest, AFileWithoutAnIntegerVersionGivesNone)
{
    const QtRocket::Test::TempDir directory;
    EXPECT_EQ(readDatabaseVersion(directory.resolve("missing.json")), std::nullopt);
    EXPECT_EQ(readDatabaseVersion(directory.path()), std::nullopt);
    EXPECT_EQ(versionOf(directory, ""), std::nullopt);
    EXPECT_EQ(versionOf(directory, "not json"), std::nullopt);
    EXPECT_EQ(versionOf(directory, R"({"database_version": 7)"), std::nullopt);
    EXPECT_EQ(versionOf(directory, R"([{"database_version": 7}])"), std::nullopt);
    EXPECT_EQ(versionOf(directory, "{}"), std::nullopt);
    // Gson's getAsLong() would read the first and cut the second.
    EXPECT_EQ(versionOf(directory, R"({"database_version": "7"})"), std::nullopt);
    EXPECT_EQ(versionOf(directory, R"({"database_version": 7.5})"), std::nullopt);
    EXPECT_EQ(versionOf(directory, R"({"database_version": null})"), std::nullopt);
    EXPECT_EQ(versionOf(directory, R"({"database_version": true})"), std::nullopt);
    EXPECT_EQ(versionOf(directory, R"({"database_version": 1e30})"), std::nullopt);
}

TEST_F(MotorDatabaseMetadataIOTest, ALargerFileIsNotRead)
{
    const QtRocket::Test::TempDir directory;
    const std::string             version = R"({"database_version": 7})";
    std::string                   padded  = version;
    padded.insert(1, std::string(MotorDatabaseMetadataIO::kMaxFileSize - version.size(), ' '));
    EXPECT_EQ(versionOf(directory, padded), std::optional<std::int64_t>(7));
    padded.insert(1, " ");
    EXPECT_EQ(versionOf(directory, padded), std::nullopt);
}

}  // namespace

}  // namespace QtRocket::Gui
