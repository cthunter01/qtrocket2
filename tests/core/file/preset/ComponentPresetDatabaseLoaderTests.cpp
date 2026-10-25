// Tests of ComponentPresetDatabaseLoader, the port of OpenRocket's
// core/src/main/java/info/openrocket/core/database/ComponentPresetDatabaseLoader.java, which has
// no JUnit test of its own: OpenRocket's tests of the loaded database are OpenRocketCoreTest's
// (example_files_tests.cpp) and the preset golden (preset_golden_tests.cpp). The fixtures are the
// .orc files of tests/data/presets (README.md there), copied into directories of the tests' own.

#include "QtRocket/file/preset/ComponentPresetDatabaseLoader.h"

#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/logging/Warning.h"
#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestPaths.h"
#include "TestTempDir.h"

namespace
{

using QtRocket::ComponentPresetDatabaseLoader;
using QtRocket::ComponentPresetType;
using QtRocket::Test::TempDir;
using Origin = ComponentPresetDatabaseLoader::Origin;

/// OpenRocket's built-in materials, in which the loads look up the presets' materials.
[[nodiscard]] std::unique_ptr<QtRocket::MaterialStorage> builtinMaterialStorage()
{
    auto materials = std::make_unique<QtRocket::MaterialStorage>();
    QtRocket::addBuiltinMaterials(*materials);
    return materials;
}

/// The fixture @p name of tests/data/presets.
[[nodiscard]] std::filesystem::path presetFixture(std::string_view name)
{
    return QtRocket::Test::testDataDir() / "presets" / name;
}

/// The bundled directory, data/components.
[[nodiscard]] std::filesystem::path bundledDirectory()
{
    return QtRocket::Test::dataDir() / ComponentPresetDatabaseLoader::kBundledDirectory;
}

/// The problems of @p loader, one line each: where it was met, "refused" for a refused file,
/// the code and the message.
[[nodiscard]] std::vector<std::string> problemLines(const ComponentPresetDatabaseLoader& loader)
{
    std::vector<std::string> lines;
    for (const ComponentPresetDatabaseLoader::Problem& problem : loader.getProblems())
    {
        lines.push_back(std::format("{}{} {}: {}",
                                    problem.origin == Origin::USER ? "user" : "bundled",
                                    problem.refusedFile ? " refused" : "",
                                    QtRocket::toString(problem.error.code), problem.error.message));
    }
    return lines;
}

/// The files of the problems of @p loader.
[[nodiscard]] std::vector<std::filesystem::path> problemFiles(
    const ComponentPresetDatabaseLoader& loader)
{
    std::vector<std::filesystem::path> files;
    for (const ComponentPresetDatabaseLoader::Problem& problem : loader.getProblems())
    {
        files.push_back(problem.file);
    }
    return files;
}

/// The texts of the warnings of @p loader.
[[nodiscard]] std::vector<std::string> warningTexts(const ComponentPresetDatabaseLoader& loader)
{
    std::vector<std::string> texts;
    for (const QtRocket::Warning& warning : loader.getWarnings())
    {
        texts.push_back(warning.toString());
    }
    return texts;
}

/// The number of presets of each type in @p database: "<TYPE> <n>; " for every type.
[[nodiscard]] std::string typeCounts(const QtRocket::ComponentPresetDatabase& database)
{
    std::string counts;
    for (const ComponentPresetType type : QtRocket::kAllComponentPresetTypes)
    {
        counts += std::format("{} {}; ", QtRocket::componentPresetTypeName(type),
                              database.listForType(type).size());
    }
    return counts;
}

/// The number of legacy presets in @p database.
[[nodiscard]] std::size_t legacyCount(const QtRocket::ComponentPresetDatabase& database)
{
    std::size_t legacy = 0;
    for (const QtRocket::ComponentPresetDatabase::PresetPtr& preset : database.listAll())
    {
        const bool* const isLegacy =
            preset->getProperties().get(QtRocket::ComponentPreset::kLegacy);
        if (isLegacy != nullptr && *isLegacy)
        {
            legacy++;
        }
    }
    return legacy;
}

/// The message OpenRocketComponentLoader refuses a copy of broken.orc named @p name with.
[[nodiscard]] std::string brokenMessage(std::string_view name)
{
    return std::format(
        "Unable to parse component preset file: {}: the BodyTube 'BT-50' "
        "(component 2) has no <Length>",
        name);
}

// The bundled presets: 31 files, 5231 presets, of which the database keeps 5228 (three of
// semroc.orc have the digests of others), with neither a problem nor a warning. The numbers by
// type and the legacy ones are the scouting's count of OpenRocket's database.
TEST(ComponentPresetDatabaseLoader, LoadsTheBundledPresets)
{
    const auto                    materials = builtinMaterialStorage();
    ComponentPresetDatabaseLoader loader(*materials);
    loader.loadPresetComponents(bundledDirectory());
    EXPECT_EQ(problemLines(loader), std::vector<std::string>{});
    EXPECT_EQ(warningTexts(loader), std::vector<std::string>{});
    EXPECT_EQ(loader.getFileCount(), 31U);
    EXPECT_EQ(loader.getPresetCount(), 5231U);
    const QtRocket::ComponentPresetDatabase& database = loader.getDatabase();
    EXPECT_EQ(database.size(), 5228U);

    EXPECT_EQ(typeCounts(database),
              "BODY_TUBE 1496; NOSE_CONE 1252; TRANSITION 613; TUBE_COUPLER 332; BULK_HEAD 186; "
              "CENTERING_RING 695; ENGINE_BLOCK 64; LAUNCH_LUG 102; RAIL_BUTTON 8; STREAMER 58; "
              "PARACHUTE 422; ");
    EXPECT_EQ(legacyCount(database), 1544U);

    const QtRocket::ComponentPresetDatabase taken = loader.takeDatabase();
    EXPECT_EQ(taken.size(), 5228U);
    EXPECT_TRUE(loader.getDatabase().empty());
}

// SimpleFileFilter("", false, "orc"): the extension, ignoring ASCII case.
TEST(ComponentPresetDatabaseLoader, TakesTheFilesWithTheExtensionOrcInAnyCase)
{
    EXPECT_TRUE(ComponentPresetDatabaseLoader::isPresetFile("estes.orc"));
    EXPECT_TRUE(ComponentPresetDatabaseLoader::isPresetFile("database/BMS.ORC"));
    EXPECT_TRUE(ComponentPresetDatabaseLoader::isPresetFile("parts.Orc"));
    EXPECT_TRUE(ComponentPresetDatabaseLoader::isPresetFile("b2_Rocketry_Parachutes..orc"));
    EXPECT_TRUE(ComponentPresetDatabaseLoader::isPresetFile(".orc"));
    EXPECT_FALSE(ComponentPresetDatabaseLoader::isPresetFile("estes.orc.bak"));
    EXPECT_FALSE(ComponentPresetDatabaseLoader::isPresetFile("orc"));
    EXPECT_FALSE(ComponentPresetDatabaseLoader::isPresetFile("estes.xml"));
    EXPECT_FALSE(ComponentPresetDatabaseLoader::isPresetFile("orc/estes"));
}

// The user's files in the order of the list, a directory searched as DirectoryIterator walks it
// (names in byte order, depth first, names that start with '.' skipped): the problems come in
// that order.
TEST(ComponentPresetDatabaseLoader, ReadsTheUsersFilesAndDirectoriesInTheirOrder)
{
    const TempDir tempDir;
    static_cast<void>(tempDir.copy(presetFixture("broken.orc"), "user/b.orc"));
    static_cast<void>(tempDir.copy(presetFixture("broken.orc"), "user/a/x.orc"));
    static_cast<void>(tempDir.copy(presetFixture("broken.orc"), "user/.hidden.orc"));
    static_cast<void>(tempDir.copy(presetFixture("broken.orc"), "user/.hidden/y.orc"));
    static_cast<void>(tempDir.copy(presetFixture("legacy.orc"), "user/c.ORC"));
    static_cast<void>(tempDir.write("user/notes.txt", "not a preset file"));
    const std::filesystem::path missing = tempDir.resolve("missing.orc");

    const auto                                 materials = builtinMaterialStorage();
    ComponentPresetDatabaseLoader              loader(*materials);
    const std::array<std::filesystem::path, 3> userFiles{tempDir.resolve("user"),
                                                         presetFixture("every-kind.orc"), missing};
    loader.loadUserComponents(userFiles);

    EXPECT_EQ(problemLines(loader),
              (std::vector<std::string>{"user refused PARSE: " + brokenMessage("x.orc"),
                                        "user refused PARSE: " + brokenMessage("b.orc"),
                                        "user NOT_FOUND: User-defined component preset file " +
                                            QtRocket::pathToUtf8(missing) +
                                            " is neither file nor directory"}));
    EXPECT_EQ(problemFiles(loader),
              (std::vector<std::filesystem::path>{tempDir.resolve("user") / "a" / "x.orc",
                                                  tempDir.resolve("user") / "b.orc", missing}));
    // c.ORC (the two presets of legacy.orc) and every-kind.orc (eleven).
    EXPECT_EQ(loader.getFileCount(), 2U);
    EXPECT_EQ(loader.getPresetCount(), 13U);
    EXPECT_EQ(loader.getDatabase().size(), 13U);
    EXPECT_EQ(warningTexts(loader), std::vector<std::string>{});
}

// Bug 18 of Milestone 3: in OpenRocket the NullPointerException of broken.orc escapes the loader
// and ends the whole preset load. QtRocket refuses that file and reads the next.
TEST(ComponentPresetDatabaseLoader, RefusesABrokenFileAndReadsTheNext)
{
    const auto                                 materials = builtinMaterialStorage();
    ComponentPresetDatabaseLoader              loader(*materials);
    const std::array<std::filesystem::path, 2> userFiles{presetFixture("broken.orc"),
                                                         presetFixture("every-kind.orc")};
    loader.loadUserComponents(userFiles);
    ASSERT_EQ(loader.getProblems().size(), 1U);
    const ComponentPresetDatabaseLoader::Problem& problem = loader.getProblems().front();
    EXPECT_EQ(problem.file, presetFixture("broken.orc"));
    EXPECT_EQ(problem.origin, Origin::USER);
    EXPECT_TRUE(problem.refusedFile);
    EXPECT_EQ(problem.error.code, QtRocket::ErrorCode::PARSE);
    EXPECT_EQ(problem.error.message, brokenMessage("broken.orc"));
    EXPECT_EQ(loader.getFileCount(), 1U);
    EXPECT_EQ(loader.getPresetCount(), 11U);
    EXPECT_EQ(loader.getDatabase().size(), 11U);
}

// A path of the user's list that is no preset file and no directory: OpenRocket logs "... is
// neither file nor directory" for both, QtRocket says which.
TEST(ComponentPresetDatabaseLoader, RecordsAUsersPathThatIsNoPresetFile)
{
    const TempDir                 tempDir;
    const std::filesystem::path   text      = tempDir.write("parts.txt", "not a preset file");
    const std::filesystem::path   missing   = tempDir.resolve("nowhere");
    const auto                    materials = builtinMaterialStorage();
    ComponentPresetDatabaseLoader loader(*materials);
    const std::array<std::filesystem::path, 2> userFiles{text, missing};
    loader.loadUserComponents(userFiles);
    EXPECT_EQ(problemLines(loader),
              (std::vector<std::string>{
                  "user UNSUPPORTED_FORMAT: User-defined component preset file " +
                      QtRocket::pathToUtf8(text) + " does not have the extension .orc",
                  "user NOT_FOUND: User-defined component preset file " +
                      QtRocket::pathToUtf8(missing) + " is neither file nor directory"}));
    EXPECT_EQ(loader.getFileCount(), 0U);
    EXPECT_TRUE(loader.getDatabase().empty());
}

// A file the user lists is read whatever its name starts with (Java's loadUserFile() has no
// DirSelectionFileFilter); in a directory a name that starts with '.' is skipped.
TEST(ComponentPresetDatabaseLoader, ReadsAListedFileWhateverItsNameStartsWith)
{
    const TempDir               tempDir;
    const std::filesystem::path hidden = tempDir.copy(presetFixture("legacy.orc"), ".hidden.orc");
    const auto                  materials = builtinMaterialStorage();

    ComponentPresetDatabaseLoader listed(*materials);
    listed.loadUserComponents(std::array{hidden});
    EXPECT_TRUE(listed.getProblems().empty());
    EXPECT_EQ(listed.getFileCount(), 1U);
    EXPECT_EQ(listed.getDatabase().size(), 2U);

    ComponentPresetDatabaseLoader walked(*materials);
    walked.loadUserComponents(std::array{tempDir.path()});
    EXPECT_TRUE(walked.getProblems().empty());
    EXPECT_EQ(walked.getFileCount(), 0U);
}

// A preset whose digest the database holds is dropped by the database (Database.add()), and
// still counted among the presets read, as OpenRocket's presetCount counts it.
TEST(ComponentPresetDatabaseLoader, CountsThePresetsTheDatabaseDrops)
{
    const auto                                 materials = builtinMaterialStorage();
    ComponentPresetDatabaseLoader              loader(*materials);
    const std::array<std::filesystem::path, 2> userFiles{presetFixture("every-kind.orc"),
                                                         presetFixture("every-kind.orc")};
    loader.loadUserComponents(userFiles);
    EXPECT_TRUE(loader.getProblems().empty());
    EXPECT_EQ(loader.getFileCount(), 2U);
    EXPECT_EQ(loader.getPresetCount(), 22U);
    EXPECT_EQ(loader.getDatabase().size(), 11U);
}

// The warnings of the files that are read (the components OpenRocketComponentLoader skipped),
// at most Limits::warnings of them and then one that says the others were dropped.
TEST(ComponentPresetDatabaseLoader, KeepsTheWarningsOfTheFilesItReads)
{
    const auto materials = builtinMaterialStorage();

    ComponentPresetDatabaseLoader loader(*materials);
    loader.loadUserComponents(std::array{presetFixture("skipped.orc")});
    EXPECT_TRUE(loader.getProblems().empty());
    EXPECT_EQ(loader.getPresetCount(), 1U);
    const std::vector<std::string> all = warningTexts(loader);
    ASSERT_EQ(all.size(), 3U);
    EXPECT_EQ(all.front(),
              "Component preset file skipped.orc: the Parachute 'PK-12' (component 2) was "
              "skipped: it has no <LineCount> that is an integer");

    ComponentPresetDatabaseLoader bounded(*materials, {.warnings = 2});
    bounded.loadUserComponents(std::array{presetFixture("skipped.orc")});
    EXPECT_EQ(warningTexts(bounded),
              (std::vector<std::string>{
                  all[0], all[1],
                  "The component preset files gave more than 2 warnings; the further ones were "
                  "dropped."}));
}

// The bounds of the walks (decision D7) and of the files.
TEST(ComponentPresetDatabaseLoader, ReadsWithinItsBounds)
{
    const TempDir tempDir;
    static_cast<void>(tempDir.copy(presetFixture("legacy.orc"), "user/a.orc"));
    static_cast<void>(tempDir.copy(presetFixture("every-kind.orc"), "user/b.orc"));
    static_cast<void>(tempDir.copy(presetFixture("thumbnail.orc"), "user/c.orc"));
    static_cast<void>(tempDir.copy(presetFixture("legacy.orc"), "user/0deep/deeper/d.orc"));
    const std::filesystem::path user      = tempDir.resolve("user");
    const auto                  materials = builtinMaterialStorage();

    ComponentPresetDatabaseLoader walks(*materials, {.directory = {.files = 2, .depth = 1}});
    walks.loadUserComponents(std::array{user});
    EXPECT_EQ(problemLines(walks),
              (std::vector<std::string>{
                  "user UNSUPPORTED_FORMAT: " + QtRocket::pathToUtf8(user / "0deep" / "deeper") +
                      " is more than 1 directories deep; it was not searched",
                  "user UNSUPPORTED_FORMAT: " + QtRocket::pathToUtf8(user) +
                      " holds more than 2 files; the first 2 were taken"}));
    EXPECT_EQ(walks.getFileCount(), 2U);
    EXPECT_EQ(walks.getPresetCount(), 13U);

    // A file larger than the bound of the files is refused before it is read.
    ComponentPresetDatabaseLoader files(*materials, {.file = {.fileSize = 2000}});
    files.loadUserComponents(std::array{user});
    ASSERT_EQ(files.getProblems().size(), 1U);
    EXPECT_EQ(files.getProblems().front().file, user / "b.orc");
    EXPECT_TRUE(files.getProblems().front().refusedFile);
    EXPECT_EQ(files.getProblems().front().error.code, QtRocket::ErrorCode::IO);
    EXPECT_TRUE(files.getProblems().front().error.message.starts_with(
        "Unable to parse component preset file: b.orc: "))
        << files.getProblems().front().error.message;
    EXPECT_EQ(files.getFileCount(), 3U);
}

// The bundled directory: a file that fails is a problem (OpenRocket logs it) and the others are
// read; a directory without preset files is a problem too, after the one of a directory that
// cannot be listed.
TEST(ComponentPresetDatabaseLoader, RecordsTheProblemsOfTheBundledDirectory)
{
    const TempDir tempDir;
    static_cast<void>(tempDir.copy(presetFixture("broken.orc"), "bundled/broken.orc"));
    static_cast<void>(tempDir.copy(presetFixture("every-kind.orc"), "bundled/every-kind.orc"));
    static_cast<void>(tempDir.write("empty/readme.txt", "no presets"));
    const auto materials = builtinMaterialStorage();

    ComponentPresetDatabaseLoader loader(*materials);
    loader.loadPresetComponents(tempDir.resolve("bundled"));
    EXPECT_EQ(problemLines(loader),
              std::vector<std::string>{"bundled refused PARSE: " + brokenMessage("broken.orc")});
    EXPECT_EQ(loader.getFileCount(), 1U);
    EXPECT_EQ(loader.getDatabase().size(), 11U);

    const std::filesystem::path   empty = tempDir.resolve("empty");
    ComponentPresetDatabaseLoader none(*materials);
    none.loadPresetComponents(empty);
    EXPECT_EQ(problemLines(none),
              std::vector<std::string>{"bundled NOT_FOUND: No component preset file found in " +
                                       QtRocket::pathToUtf8(empty)});

    const std::filesystem::path   missing = tempDir.resolve("missing");
    ComponentPresetDatabaseLoader absent(*materials);
    absent.loadPresetComponents(missing);
    EXPECT_EQ(problemLines(absent),
              (std::vector<std::string>{
                  "bundled IO: not a directory or IOException occurred when listing files from " +
                      QtRocket::pathToUtf8(missing),
                  "bundled NOT_FOUND: No component preset file found in " +
                      QtRocket::pathToUtf8(missing)}));
}

// loadDatabase(): the bundled directory, then the user's files.
TEST(ComponentPresetDatabaseLoader, LoadsTheBundledDirectoryBeforeTheUsersFiles)
{
    const TempDir tempDir;
    static_cast<void>(tempDir.copy(presetFixture("broken.orc"), "bundled/z.orc"));
    static_cast<void>(tempDir.copy(presetFixture("legacy.orc"), "bundled/legacy.orc"));
    const std::filesystem::path user      = tempDir.copy(presetFixture("broken.orc"), "user/a.orc");
    const auto                  materials = builtinMaterialStorage();

    ComponentPresetDatabaseLoader loader(*materials);
    loader.loadDatabase(tempDir.resolve("bundled"),
                        std::array{user, presetFixture("every-kind.orc")});
    EXPECT_EQ(problemLines(loader),
              (std::vector<std::string>{"bundled refused PARSE: " + brokenMessage("z.orc"),
                                        "user refused PARSE: " + brokenMessage("a.orc")}));
    EXPECT_EQ(loader.getFileCount(), 2U);
    EXPECT_EQ(loader.getPresetCount(), 13U);
}

}  // namespace
