#include "QtRocket/file/DirectoryIterator.h"

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"
#include "TestPaths.h"
#include "TestTempDir.h"

namespace
{

using QtRocket::DirectoryIterator;
using QtRocket::ErrorCode;
using QtRocket::Test::TempDir;

/// The names of @p files relative to @p base, as UTF-8 with '/' between their parts.
[[nodiscard]] std::vector<std::string> relativeNames(
    const std::vector<std::filesystem::path>& files, const std::filesystem::path& base)
{
    std::vector<std::string> names;
    for (const std::filesystem::path& file : files)
    {
        const std::u8string name = file.lexically_relative(base).generic_u8string();
        names.emplace_back(name.begin(), name.end());
    }
    return names;
}

[[nodiscard]] bool acceptAll(const std::filesystem::path& /*file*/)
{
    return true;
}

[[nodiscard]] bool isOrcFile(const std::filesystem::path& file)
{
    return file.extension() == ".orc";
}

TEST(DirectoryIterator, WalksDepthFirstInTheOrderOfTheUtf8Names)
{
    const TempDir tempDir;
    // Upper-case letters come before lower-case ones, '-' before letters and a name before the
    // longer ones it starts (byte order). A directory is walked where its own name falls among
    // the names beside it, so a/ comes before a-b.orc, although "a-b.orc" sorts before "a/a.orc"
    // as a path. (No two names differ in case only: macOS and Windows would make them one.)
    constexpr std::array<std::string_view, 9> kNames{"b2.orc",  "B.ORC",        "a/z.orc",
                                                     "a/a.orc", "a-b.orc",      "c/d/e.orc",
                                                     "ab.orc",  "\xC3\xA9.orc", "z.orc"};
    for (const std::string_view name : kNames)
    {
        static_cast<void>(tempDir.write(QtRocket::pathFromUtf8(name), "x"));
    }

    const DirectoryIterator::Listing listing =
        DirectoryIterator::listFiles(tempDir.path(), acceptAll);
    EXPECT_TRUE(listing.problems.empty());
    EXPECT_EQ(relativeNames(listing.files, tempDir.path()),
              (std::vector<std::string>{"B.ORC", "a/a.orc", "a/z.orc", "a-b.orc", "ab.orc",
                                        "b2.orc", "c/d/e.orc", "z.orc", "\xC3\xA9.orc"}));
}

TEST(DirectoryIterator, SkipsNamesThatStartWithADot)
{
    const TempDir tempDir;
    static_cast<void>(tempDir.write(".hidden.orc", "x"));
    static_cast<void>(tempDir.write(".git/inside.orc", "x"));
    static_cast<void>(tempDir.write("visible/.also-hidden.orc", "x"));
    static_cast<void>(tempDir.write("visible/shown.orc", "x"));
    static_cast<void>(tempDir.write("b2_Rocketry_Parachutes..orc", "x"));

    const DirectoryIterator::Listing listing =
        DirectoryIterator::listFiles(tempDir.path(), acceptAll);
    EXPECT_TRUE(listing.problems.empty());
    EXPECT_EQ(relativeNames(listing.files, tempDir.path()),
              (std::vector<std::string>{"b2_Rocketry_Parachutes..orc", "visible/shown.orc"}));
}

TEST(DirectoryIterator, KeepsTheFilesTheFilterAccepts)
{
    const TempDir tempDir;
    static_cast<void>(tempDir.write("keep.orc", "x"));
    static_cast<void>(tempDir.write("drop.txt", "x"));
    static_cast<void>(tempDir.write("dir.txt/keep.orc", "x"));  // the filter never sees directories

    const DirectoryIterator::Listing listing = DirectoryIterator::listFiles(
        tempDir.path(),
        [](const std::filesystem::path& file) { return file.extension() == ".orc"; });
    EXPECT_TRUE(listing.problems.empty());
    EXPECT_EQ(relativeNames(listing.files, tempDir.path()),
              (std::vector<std::string>{"dir.txt/keep.orc", "keep.orc"}));
}

TEST(DirectoryIterator, AMissingDirectoryIsAProblem)
{
    const TempDir                    tempDir;
    const std::filesystem::path      missing = tempDir.resolve("nowhere");
    const std::filesystem::path      file    = tempDir.write("file.orc", "x");
    const DirectoryIterator::Listing listing = DirectoryIterator::listFiles(missing, acceptAll);
    EXPECT_TRUE(listing.files.empty());
    ASSERT_EQ(listing.problems.size(), 1U);
    EXPECT_EQ(listing.problems.front().directory, missing);
    EXPECT_EQ(listing.problems.front().error.code, ErrorCode::IO);
    EXPECT_EQ(listing.problems.front().error.message,
              "not a directory or IOException occurred when listing files from " +
                  QtRocket::pathToUtf8(missing));

    // A file is not a directory either (Java's listFiles() gives null for it).
    const DirectoryIterator::Listing ofFile = DirectoryIterator::listFiles(file, acceptAll);
    EXPECT_TRUE(ofFile.files.empty());
    ASSERT_EQ(ofFile.problems.size(), 1U);
    EXPECT_EQ(ofFile.problems.front().directory, file);
}

TEST(DirectoryIterator, WalksADirectoryReachedTwiceThroughLinksOnce)
{
    const TempDir tempDir;
    static_cast<void>(tempDir.write("files/a.orc", "x"));
    std::error_code error;
    std::filesystem::create_directory_symlink(tempDir.resolve("files"),
                                              tempDir.resolve("files/loop"), error);
    if (!error)
    {
        std::filesystem::create_directory_symlink(tempDir.resolve("files"), tempDir.resolve("link"),
                                                  error);
    }
    if (error)
    {
        GTEST_SKIP() << "cannot create directory links here: " << error.message();
    }

    const DirectoryIterator::Listing listing =
        DirectoryIterator::listFiles(tempDir.path(), acceptAll);
    EXPECT_TRUE(listing.problems.empty());
    // files/a.orc once, and no endless loop through files/loop; link/ is files/ again.
    EXPECT_EQ(relativeNames(listing.files, tempDir.path()),
              (std::vector<std::string>{"files/a.orc"}));
}

TEST(DirectoryIterator, RecordsADirectoryBelowItCannotListAndGoesOn)
{
    const TempDir tempDir;
    static_cast<void>(tempDir.write("a.orc", "x"));
    static_cast<void>(tempDir.write("z.orc", "x"));
    const std::filesystem::path closed = tempDir.resolve("closed");
    std::filesystem::create_directories(closed);
    std::filesystem::permissions(closed, std::filesystem::perms::none);
    std::error_code                           error;
    const std::filesystem::directory_iterator probe(closed, error);
    if (!error)
    {
        std::filesystem::permissions(closed, std::filesystem::perms::owner_all);
        GTEST_SKIP() << "permissions do not stop this user from listing a directory";
    }

    const DirectoryIterator::Listing listing =
        DirectoryIterator::listFiles(tempDir.path(), acceptAll);
    std::filesystem::permissions(closed, std::filesystem::perms::owner_all);
    EXPECT_EQ(relativeNames(listing.files, tempDir.path()),
              (std::vector<std::string>{"a.orc", "z.orc"}));
    ASSERT_EQ(listing.problems.size(), 1U);
    EXPECT_EQ(listing.problems.front().directory, closed);
    EXPECT_EQ(listing.problems.front().error.code, ErrorCode::IO);
}

TEST(DirectoryIterator, EndsAWalkAtItsBoundOfFiles)
{
    const TempDir tempDir;
    static_cast<void>(tempDir.write("a/1.orc", "x"));
    static_cast<void>(tempDir.write("a/2.orc", "x"));
    static_cast<void>(tempDir.write("b.orc", "x"));
    static_cast<void>(tempDir.write("c.orc", "x"));
    static_cast<void>(tempDir.write("skipped.txt", "x"));

    // As many files as the bound: no problem.
    const DirectoryIterator::Listing all =
        DirectoryIterator::listFiles(tempDir.path(), isOrcFile, {.files = 4});
    EXPECT_TRUE(all.problems.empty());
    EXPECT_EQ(all.files.size(), 4U);

    // One more is a problem for the directory the walk started from, which ends there; the files
    // the filter refuses do not count.
    const DirectoryIterator::Listing bounded =
        DirectoryIterator::listFiles(tempDir.path(), isOrcFile, {.files = 3});
    EXPECT_EQ(relativeNames(bounded.files, tempDir.path()),
              (std::vector<std::string>{"a/1.orc", "a/2.orc", "b.orc"}));
    ASSERT_EQ(bounded.problems.size(), 1U);
    EXPECT_EQ(bounded.problems.front().directory, tempDir.path());
    EXPECT_EQ(bounded.problems.front().error.code, ErrorCode::UNSUPPORTED_FORMAT);
    EXPECT_EQ(
        bounded.problems.front().error.message,
        QtRocket::pathToUtf8(tempDir.path()) + " holds more than 3 files; the first 3 were taken");

    // The bound is reached in a subdirectory: the walk ends there too.
    const DirectoryIterator::Listing inside =
        DirectoryIterator::listFiles(tempDir.path(), isOrcFile, {.files = 1});
    EXPECT_EQ(relativeNames(inside.files, tempDir.path()), std::vector<std::string>{"a/1.orc"});
    EXPECT_EQ(inside.problems.size(), 1U);

    // No file at all, then.
    const DirectoryIterator::Listing none =
        DirectoryIterator::listFiles(tempDir.path(), isOrcFile, {.files = 0});
    EXPECT_TRUE(none.files.empty());
    EXPECT_EQ(none.problems.size(), 1U);
}

TEST(DirectoryIterator, EntersDirectoriesDownToItsBoundOfDepth)
{
    const TempDir tempDir;
    static_cast<void>(tempDir.write("0.orc", "x"));
    static_cast<void>(tempDir.write("a/1.orc", "x"));
    static_cast<void>(tempDir.write("a/b/2.orc", "x"));
    static_cast<void>(tempDir.write("a/b/c/3.orc", "x"));
    static_cast<void>(tempDir.write("z/1.orc", "x"));

    const DirectoryIterator::Listing deep =
        DirectoryIterator::listFiles(tempDir.path(), acceptAll, {.depth = 3});
    EXPECT_TRUE(deep.problems.empty());
    EXPECT_EQ(deep.files.size(), 5U);

    // Depth 1: the directory and the directories in it; a/b is not entered, with a problem for
    // it, and the walk goes on with z.
    const DirectoryIterator::Listing shallow =
        DirectoryIterator::listFiles(tempDir.path(), acceptAll, {.depth = 1});
    EXPECT_EQ(relativeNames(shallow.files, tempDir.path()),
              (std::vector<std::string>{"0.orc", "a/1.orc", "z/1.orc"}));
    ASSERT_EQ(shallow.problems.size(), 1U);
    EXPECT_EQ(shallow.problems.front().directory, tempDir.resolve("a") / "b");
    EXPECT_EQ(shallow.problems.front().error.code, ErrorCode::UNSUPPORTED_FORMAT);
    EXPECT_EQ(shallow.problems.front().error.message,
              QtRocket::pathToUtf8(tempDir.resolve("a") / "b") +
                  " is more than 1 directories deep; it was not searched");

    // Depth 0: the directory alone.
    const DirectoryIterator::Listing flat =
        DirectoryIterator::listFiles(tempDir.path(), acceptAll, {.depth = 0});
    EXPECT_EQ(relativeNames(flat.files, tempDir.path()), std::vector<std::string>{"0.orc"});
    EXPECT_EQ(flat.problems.size(), 2U);
}

TEST(DirectoryIterator, FindsTheBundledPresetFilesInTheManifestsOrder)
{
    // The *.orc files (ignoring case, as SimpleFileFilter) come in the order of the presetFiles
    // of tests/data/goldens/manifest.json, the harness's sorted paths.
    const std::filesystem::path      components = QtRocket::Test::dataDir() / "components";
    const DirectoryIterator::Listing listing =
        DirectoryIterator::listFiles(components, [](const std::filesystem::path& file) {
            return QtRocket::Strings::toLower(QtRocket::pathToUtf8(file.filename()))
                .ends_with(".orc");
        });
    EXPECT_TRUE(listing.problems.empty());
    EXPECT_EQ(relativeNames(listing.files, components),
              (std::vector<std::string>{"database/BMS.ORC",
                                        "database/ROCKETARIUM.ORC",
                                        "database/apogee.orc",
                                        "database/bluetube.orc",
                                        "database/competition_chutes.orc",
                                        "database/estes_classic.orc",
                                        "database/estes_ps2.orc",
                                        "database/generic_materials.orc",
                                        "database/giantleaprocketry.orc",
                                        "database/loc_precision.orc",
                                        "database/madcow.orc",
                                        "database/mpc.orc",
                                        "database/publicmissiles.orc",
                                        "database/quest.orc",
                                        "database/semroc.orc",
                                        "database/top_flight.orc",
                                        "internal/Estes-legacy.orc",
                                        "internal/Front_Range_Rocket_Recovery.orc",
                                        "internal/Fruity_Chutes_Enhanced.orc",
                                        "internal/LocPrecision-legacy.orc",
                                        "internal/Quest-legacy.orc",
                                        "internal/RailButton_Database.orc",
                                        "internal/Rocketman.orc",
                                        "internal/Spherachutes_Parachutes.orc",
                                        "internal/b2_Rocketry_Parachutes..orc",
                                        "internal/bluetube-legacy.orc",
                                        "internal/bms-legacy.orc",
                                        "internal/fliskits-legacy.orc",
                                        "internal/giantleaprocketry-legacy.orc",
                                        "internal/publicmissiles-legacy.orc",
                                        "internal/semroc-legacy.orc"}));
}

}  // namespace
