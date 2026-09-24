#include "QtRocket/file/motor/MotorDatabaseLoader.h"

#include <cstddef>
#include <filesystem>
#include <format>
#include <random>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/ZipArchive.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestPaths.h"

namespace
{

using QtRocket::ErrorCode;
using QtRocket::MotorDatabaseLoader;
using QtRocket::Result;
using QtRocket::ThrustCurveMotorSet;
using QtRocket::ThrustCurveMotorSetDatabase;

constexpr std::string_view kA8 = "A8 18 70 3 0.003 0.016 Estes\n0 0\n0.5 5\n1 0\n";

// What OpenRocket 5f164fd0e makes of the bundled database, and of it with the five motor test
// files added in name order: sets, and motors in them (curves with a digest already in their set
// are dropped).
constexpr std::size_t kBundledCurves      = 1591;
constexpr std::size_t kBundledSets        = 1458;
constexpr std::size_t kBundledSetMotors   = 1588;
constexpr std::size_t kWithFilesSets      = 1462;
constexpr std::size_t kWithFilesSetMotors = 1592;

[[nodiscard]] std::filesystem::path bundledDirectory()
{
    return QtRocket::Test::dataDir() / "motors";
}

[[nodiscard]] std::size_t motorsInSets(const ThrustCurveMotorSetDatabase& database)
{
    std::size_t count = 0;
    for (const ThrustCurveMotorSet& set : database.getMotorSets())
    {
        count += set.getMotorCount();
    }
    return count;
}

/// A directory in the temporary directory, removed with its contents when the test ends.
class TempDir
{
public:
    TempDir()
    {
        const ::testing::TestInfo* info = ::testing::UnitTest::GetInstance()->current_test_info();
        m_path = std::filesystem::temp_directory_path() /
                 std::format("qtrocket_{}_{}_{}", info->test_suite_name(), info->name(),
                             std::random_device{}());
        std::filesystem::create_directories(m_path);
    }
    ~TempDir()
    {
        std::error_code ignored;
        std::filesystem::remove_all(m_path, ignored);
    }
    TempDir(const TempDir&)            = delete;
    TempDir& operator=(const TempDir&) = delete;
    TempDir(TempDir&&)                 = delete;
    TempDir& operator=(TempDir&&)      = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return m_path; }

    /// Writes @p text to @p name under the directory, creating directories as needed.
    [[nodiscard]] std::filesystem::path write(std::string_view name, std::string_view text) const
    {
        const std::filesystem::path file = m_path / name;
        std::filesystem::create_directories(file.parent_path());
        EXPECT_TRUE(QtRocket::writeTextFile(file, text));
        return file;
    }

    /// Copies @p source to @p target under the directory.
    [[nodiscard]] std::filesystem::path copy(const std::filesystem::path& source,
                                             std::string_view             target) const
    {
        const std::filesystem::path file = m_path / target;
        std::filesystem::create_directories(file.parent_path());
        std::filesystem::copy_file(source, file);
        return file;
    }

    /// Copies the motor test file @p name to @p target under the directory.
    [[nodiscard]] std::filesystem::path copyTestFile(std::string_view name,
                                                     std::string_view target) const
    {
        return copy(QtRocket::Test::testDataDir() / "motors" / name, target);
    }

private:
    std::filesystem::path m_path;
};

TEST(MotorDatabaseLoader, LoadsTheBundledDatabase)
{
    MotorDatabaseLoader loader;
    const Result<void>  loaded = loader.loadInternalMotorDatabase(bundledDirectory());
    ASSERT_TRUE(loaded) << loaded.error().toString();
    EXPECT_EQ(loader.getMotorCount(), static_cast<int>(kBundledCurves));
    EXPECT_EQ(loader.getDatabase().getMotorSets().size(), kBundledSets);
    EXPECT_EQ(motorsInSets(loader.getDatabase()), kBundledSetMotors);
    EXPECT_TRUE(loader.getProblems().empty());
    EXPECT_TRUE(loader.getSkippedCurves().empty());
}

TEST(MotorDatabaseLoader, AddsTheUsersMotorsAfterTheBundledOnes)
{
    const TempDir tempDir;
    for (const std::string_view name :
         {"Estes_A8.rse", "test.zip", "test1.eng", "test2.rse", "test3.rse"})
    {
        static_cast<void>(tempDir.copyTestFile(name, name));
    }
    // Hidden files and other extensions are not read.
    static_cast<void>(tempDir.write(".hidden.eng", "junk"));
    static_cast<void>(tempDir.write("notes.txt", "junk"));

    MotorDatabaseLoader                      loader;
    const std::vector<std::filesystem::path> userFiles{tempDir.path()};
    const Result<void> loaded = loader.loadDatabase(bundledDirectory(), userFiles);
    ASSERT_TRUE(loaded) << loaded.error().toString();
    EXPECT_TRUE(loader.getProblems().empty());
    EXPECT_EQ(loader.getMotorCount(), static_cast<int>(kBundledCurves + 6));
    EXPECT_EQ(loader.getDatabase().getMotorSets().size(), kWithFilesSets);
    EXPECT_EQ(motorsInSets(loader.getDatabase()), kWithFilesSetMotors);
    EXPECT_EQ(loader.getDatabase().getMotorSets().back().toString(),
              "ThrustCurveMotorSet[Water W90psi, type=Single-use, count=1]");
}

TEST(MotorDatabaseLoader, DropsARepeatedCurve)
{
    const TempDir                            tempDir;
    const std::vector<std::filesystem::path> userFiles{tempDir.write("a.eng", kA8),
                                                       tempDir.write("b.ENG", kA8)};
    MotorDatabaseLoader                      loader;
    loader.loadUserDefinedMotors(userFiles);
    EXPECT_TRUE(loader.getProblems().empty());
    EXPECT_EQ(loader.getMotorCount(), 2);
    ASSERT_EQ(loader.getDatabase().getMotorSets().size(), 1U);
    EXPECT_EQ(loader.getDatabase().getMotorSets().front().getMotorCount(), 1U);
}

TEST(MotorDatabaseLoader, SearchesDirectoriesRecursivelyInNameOrder)
{
    const TempDir tempDir;
    static_cast<void>(tempDir.write("b/c.eng", "B4 18 70 None 0.004 0.02 Estes\n0.2 3\n0.8 0\n"));
    static_cast<void>(tempDir.write("a.eng", kA8));
    static_cast<void>(tempDir.write(".hidden/d.eng", "junk"));
    static_cast<void>(tempDir.copyTestFile("test3.rse", "b/z/test3.rse"));

    MotorDatabaseLoader                      loader;
    const std::vector<std::filesystem::path> userFiles{tempDir.path()};
    loader.loadUserDefinedMotors(userFiles);
    EXPECT_TRUE(loader.getProblems().empty());
    ASSERT_EQ(loader.getDatabase().getMotorSets().size(), 3U);
    EXPECT_EQ(loader.getDatabase().getMotorSets()[0].getDesignation(), "A8");
    EXPECT_EQ(loader.getDatabase().getMotorSets()[1].getDesignation(), "B4");
    EXPECT_EQ(loader.getDatabase().getMotorSets()[2].getDesignation(), "W90psi");
}

TEST(MotorDatabaseLoader, RecordsFilesItCannotLoad)
{
    const TempDir               tempDir;
    const std::filesystem::path unsupported = tempDir.write("motor.txt", kA8);
    const std::filesystem::path missing     = tempDir.path() / "missing.eng";
    const std::filesystem::path malformed   = tempDir.write("bad.eng", "junk\n");
    // The second motor has a negative thrust: the first one stays loaded.
    const std::filesystem::path partly = tempDir.write(
        "partly.eng", std::string(kA8) + ";\nB4 18 70 None 0.004 0.02 Estes\n0.2 -3\n0.8 0\n");
    const std::filesystem::path good = tempDir.write("good.eng", kA8);

    MotorDatabaseLoader                      loader;
    const std::vector<std::filesystem::path> userFiles{unsupported, missing, malformed, partly,
                                                       good};
    loader.loadUserDefinedMotors(userFiles);

    const std::vector<MotorDatabaseLoader::Problem>& problems = loader.getProblems();
    ASSERT_EQ(problems.size(), 4U);
    EXPECT_EQ(problems[0].file, unsupported);
    EXPECT_EQ(problems[0].error.code, ErrorCode::UNSUPPORTED_FORMAT);
    EXPECT_TRUE(problems[0].error.message.ends_with(" does not have a supported extension"));
    EXPECT_EQ(problems[1].file, missing);
    EXPECT_EQ(problems[1].error.code, ErrorCode::NOT_FOUND);
    EXPECT_TRUE(problems[1].error.message.ends_with(" is neither file nor directory"));
    EXPECT_EQ(problems[2].file, malformed);
    EXPECT_EQ(problems[2].error.code, ErrorCode::PARSE);
    EXPECT_EQ(problems[3].file, partly);
    EXPECT_EQ(problems[3].error.message, "Negative thrust.");

    // partly.eng's A8 and good.eng's A8 (the same curve) plus the counted B4.
    EXPECT_EQ(loader.getMotorCount(), 3);
    ASSERT_EQ(loader.getDatabase().getMotorSets().size(), 1U);
    EXPECT_EQ(loader.getDatabase().getMotorSets().front().getDesignation(), "A8");
}

TEST(MotorDatabaseLoader, ReadsAUserDatabaseFile)
{
    const TempDir tempDir;
    static_cast<void>(tempDir.copy(bundledDirectory() / "initial_motors.db", "sub/Motors.DB"));
    MotorDatabaseLoader                      loader;
    const std::vector<std::filesystem::path> userFiles{tempDir.path()};
    loader.loadUserDefinedMotors(userFiles);
    EXPECT_TRUE(loader.getProblems().empty());
    EXPECT_EQ(loader.getMotorCount(), static_cast<int>(kBundledCurves));

    // A broken one is recorded, not fatal.
    const std::vector<std::filesystem::path> broken{tempDir.write("broken.db", "junk")};
    loader.loadUserDefinedMotors(broken);
    ASSERT_EQ(loader.getProblems().size(), 1U);
    EXPECT_EQ(loader.getProblems().front().error.code, ErrorCode::DATABASE);
}

TEST(MotorDatabaseLoader, PrefersAReadableMotorLibraryDatabase)
{
    const TempDir               tempDir;
    const std::filesystem::path library = tempDir.write("motors.db", "");
    // An empty database file reads but lacks the tables: recorded, then the bundled one is used.
    MotorDatabaseLoader fallback;
    ASSERT_TRUE(fallback.loadInternalMotorDatabase(bundledDirectory(), library));
    ASSERT_EQ(fallback.getProblems().size(), 1U);
    EXPECT_EQ(fallback.getProblems().front().file, library);
    EXPECT_EQ(fallback.getProblems().front().error.message,
              "SQLite motor database missing meta table");
    EXPECT_EQ(fallback.getMotorCount(), static_cast<int>(kBundledCurves));

    // A good one is used instead of the bundled files, which are then not read at all.
    const TempDir               other;
    const std::filesystem::path goodLibrary =
        other.copy(bundledDirectory() / "initial_motors.db", "motors.db");
    MotorDatabaseLoader preferred;
    ASSERT_TRUE(preferred.loadInternalMotorDatabase(tempDir.path() / "nowhere", goodLibrary));
    EXPECT_TRUE(preferred.getProblems().empty());
    EXPECT_EQ(preferred.getMotorCount(), static_cast<int>(kBundledCurves));
}

TEST(MotorDatabaseLoader, NoBundledDatabaseIsAProblemNotAFailure)
{
    const TempDir       tempDir;
    MotorDatabaseLoader loader;
    const Result<void>  loaded = loader.loadDatabase(
        tempDir.path(), std::vector<std::filesystem::path>{tempDir.write("a.eng", kA8)});
    ASSERT_TRUE(loaded);
    ASSERT_EQ(loader.getProblems().size(), 1U);
    EXPECT_EQ(loader.getProblems().front().error.code, ErrorCode::NOT_FOUND);
    EXPECT_EQ(loader.getMotorCount(), 1);  // the user's motors are still read
}

TEST(MotorDatabaseLoader, ABrokenBundledDatabaseFailsTheLoad)
{
    const TempDir tempDir;
    static_cast<void>(tempDir.write("broken.db", "junk"));
    MotorDatabaseLoader loader;
    const Result<void>  loaded = loader.loadDatabase(
        tempDir.path(), std::vector<std::filesystem::path>{tempDir.write("a.eng", kA8)});
    ASSERT_FALSE(loaded);
    EXPECT_EQ(loaded.error().code, ErrorCode::DATABASE);
    EXPECT_EQ(loader.getMotorCount(), 0);  // as OpenRocket's BugException, before the user files
}

TEST(MotorDatabaseLoader, ReadsUserZipArchives)
{
    const TempDir       tempDir;
    QtRocket::ZipWriter writer;
    writer.add("inside/a.eng", QtRocket::stringToBytes(kA8));
    const std::filesystem::path archive = tempDir.path() / "motors.zip";
    ASSERT_TRUE(QtRocket::writeFile(archive, writer.finish().value()));

    MotorDatabaseLoader loader;
    loader.loadUserDefinedMotors(std::vector<std::filesystem::path>{archive});
    EXPECT_TRUE(loader.getProblems().empty());
    EXPECT_EQ(loader.getMotorCount(), 1);
}

TEST(MotorDatabaseLoader, TakeDatabaseLeavesAnEmptyOne)
{
    const TempDir       tempDir;
    MotorDatabaseLoader loader;
    loader.loadUserDefinedMotors(std::vector<std::filesystem::path>{tempDir.write("a.eng", kA8)});
    const ThrustCurveMotorSetDatabase database = loader.takeDatabase();
    EXPECT_EQ(database.getMotorSets().size(), 1U);
    EXPECT_TRUE(loader.getDatabase().getMotorSets().empty());
}

}  // namespace
