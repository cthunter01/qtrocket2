// QtRocket's saves for section 7's interop check (plan section 7: a design QtRocket saved opens in
// OpenRocket without a warning): a DISABLED_ test, which neither ctest nor CI runs, writes them,
// and the Java tool tools/openrocket-goldens/InteropCheck.java, run by interop.sh, has OpenRocket
// read each and compares what it gets with what it gets from its own save of the same input
// (tools/openrocket-goldens/README.md, "Interop check"). Run it by hand from the build tree:
//
//   bin/QtRocket_core_tests --gtest_also_run_disabled_tests --gtest_filter='OrkInteropSaves.*'
//
// The saves go into the directory interop-saves of the build tree (the directory above the bin/ of
// the test program: build/<preset>/interop-saves), or into the directory a command-line argument
// --interop-saves=<directory> names. The test makes the directory, removes the *.ork files an
// earlier run left there, and writes one file per save, named <input>.<form>.ork after the input of
// the goldens' manifest (tests/data/goldens/manifest.json) and the form of the save: "zip-data" or
// "zip-nodata" for an .ork archive (GeneralRocketSaver) with or without the simulation data,
// "plain-data" or "plain-nodata" for plain XML (OpenRocketSaver). Each save is the first save of a
// load of its own: a save stores the automatic dimensions its getters compute, so the saves of one
// loaded document can differ (kDualParachuteSaves of ork_resave_diff_golden_tests.cpp), and
// OpenRocket's own save, which the check compares with, is the first save of its load. 65 files:
// - the sixteen examples (data/examples), loaded from the original archives with the six presets
//   of the examples (goldenExampleEnvironment()): an archive with the document's default storage
//   options (all sixteen save their flight data) and plain XML without the simulation data (the
//   form of the harness's re-saves);
// - the thirteen test rockets, loaded from their re-saves against the seven motors of TestRockets
//   (loadResave() of GoldenResaves.h): an archive with the document's default storage options;
// - the eighteen design files of tests/data/ork (the legacy files), loaded as RecordedOrkFiles.h
//   records them (DesignFileEnvironment): an archive with the simulation data, but plain XML with
//   it for v1.6-apocd.ork, whose decal images are no files beside it, so that its archive fails in
//   OpenRocket as here (the manifest's archiveError; legacy_ork_roundtrip_tests.cpp);
// - the two saver-coverage documents of the harness, loaded from their re-saves
//   (DesignFileEnvironment): an archive with the document's default storage options.

#include <cstddef>
#include <filesystem>
#include <format>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/cli/DataDirectory.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/StorageOptions.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/GeneralRocketSaver.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/file/openrocket/OpenRocketSaver.h"
#include "QtRocket/logging/ErrorSet.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "file/DesignFileEnvironment.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenExamples.h"
#include "goldens/GoldenResaves.h"

namespace
{

using QtRocket::ErrorSet;
using QtRocket::GeneralRocketLoader;
using QtRocket::GeneralRocketSaver;
using QtRocket::LoadedDocument;
using QtRocket::OpenRocketDocument;
using QtRocket::OpenRocketSaver;
using QtRocket::Preferences;
using QtRocket::Result;
using QtRocket::StorageOptions;
using QtRocket::WarningSet;
using QtRocket::Test::DesignFileEnvironment;
using QtRocket::Test::GoldenExample;
using QtRocket::Test::GoldenInput;
using QtRocket::Test::LoadedResave;
using QtRocket::Test::ResaveInput;

/// The command-line argument that names the directory of the saves.
constexpr std::string_view kDirectoryArgument = "--interop-saves=";
/// The directory of the saves in the build tree, when no argument names one.
constexpr std::string_view kBuildTreeDirectory = "interop-saves";

/// The directory of the saves: the one --interop-saves=<directory> names on the command line of
/// the test program (GoogleTest keeps the arguments it does not know), else interop-saves in the
/// build tree, the directory above the bin/ of the test program; nothing when the operating system
/// does not say where the program is.
[[nodiscard]] std::optional<std::filesystem::path> savesDirectory()
{
    for (const std::string& argument : ::testing::internal::GetArgvs())
    {
        if (argument.starts_with(kDirectoryArgument))
        {
            return QtRocket::pathFromUtf8(
                std::string_view(argument).substr(kDirectoryArgument.size()));
        }
    }
    const std::optional<std::filesystem::path> program = QtRocket::DataDirectory::executablePath();
    if (!program.has_value())
    {
        return std::nullopt;
    }
    return program->parent_path().parent_path() / kBuildTreeDirectory;
}

/// Makes @p directory and removes the files named *.ork in it, the saves of an earlier run, so
/// that the check reads the saves of this run alone; what went wrong, or "".
[[nodiscard]] std::string preparedDirectory(const std::filesystem::path& directory)
{
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    std::vector<std::filesystem::path> earlier;
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(directory, error))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".ork")
        {
            earlier.push_back(entry.path());
        }
    }
    for (const std::filesystem::path& file : earlier)
    {
        std::filesystem::remove(file, error);
    }
    return error ? std::format("{}: {}\n", QtRocket::pathToUtf8(directory), error.message()) : "";
}

/// What the test wrote.
struct InteropSaves
{
    std::filesystem::path directory;
    int                   files{0};
    std::string           problems;  ///< what went wrong, one line each; empty when nothing did
};

/// The form of a save in its file name.
[[nodiscard]] std::string_view formOf(bool archive, bool withData)
{
    if (archive)
    {
        return withData ? "zip-data" : "zip-nodata";
    }
    return withData ? "plain-data" : "plain-nodata";
}

/// Writes @p bytes, the save of the input @p input in the form @p form, as
/// <directory>/<input>.<form>.ork.
void writeSave(InteropSaves& saves, std::string_view input, std::string_view form,
               std::span<const std::byte> bytes)
{
    const std::filesystem::path file =
        saves.directory / QtRocket::pathFromUtf8(std::format("{}.{}.ork", input, form));
    const Result<void> written = QtRocket::writeFile(file, bytes);
    if (!written)
    {
        saves.problems += std::format("{}: {}\n", input, written.error().toString());
        return;
    }
    saves.files++;
}

/// Saves @p document as an .ork archive with @p options (a GeneralRocketSaver with @p preferences)
/// and writes the archive; a save that fails or warns is a problem.
void writeArchive(InteropSaves& saves, std::string_view input, OpenRocketDocument& document,
                  StorageOptions& options, const Preferences& preferences)
{
    const bool                           withData = options.getSaveSimulationData();
    GeneralRocketSaver                   saver(preferences);
    const Result<std::vector<std::byte>> bytes = saver.saveToBytes(document, options);
    if (!bytes || !saver.getWarnings().empty())
    {
        saves.problems +=
            std::format("{}: the archive {}\n", input,
                        bytes ? std::format("gave {} warnings", saver.getWarnings().size())
                              : "failed: " + bytes.error().toString());
        return;
    }
    writeSave(saves, input, formOf(true, withData), *bytes);
}

/// Saves @p document as plain XML, with its simulation data or without (OpenRocketSaver with
/// @p preferences), and writes the text; a save that fails or warns is a problem.
void writePlain(InteropSaves& saves, std::string_view input, OpenRocketDocument& document,
                bool withData, const Preferences& preferences)
{
    StorageOptions options;
    options.setSaveSimulationData(withData);
    WarningSet                warnings;
    ErrorSet                  errors;
    const Result<std::string> text =
        OpenRocketSaver::save(document, options, preferences, warnings, errors);
    if (!text || !warnings.empty() || !errors.empty())
    {
        saves.problems += std::format("{}: the plain save {}\n", input,
                                      text ? "warned" : "failed: " + text.error().toString());
        return;
    }
    writeSave(saves, input, formOf(false, withData), std::as_bytes(std::span(*text)));
}

/// The sixteen examples: an archive with the default storage options and plain XML without the
/// simulation data, each of a load of its own.
void writeTheExamples(InteropSaves& saves)
{
    const DesignFileEnvironment& environment = QtRocket::Test::goldenExampleEnvironment();
    for (const GoldenExample& example : QtRocket::Test::goldenExamples())
    {
        const Result<LoadedDocument> forTheArchive  = QtRocket::Test::loadGoldenExample(example);
        const Result<LoadedDocument> forThePlainXml = QtRocket::Test::loadGoldenExample(example);
        if (!forTheArchive || !forThePlainXml)
        {
            saves.problems += std::format("{}: no load\n", example.name);
            continue;
        }
        OpenRocketDocument& document = *forTheArchive->document;
        writeArchive(saves, example.name, document, document.getDefaultStorageOptions(),
                     environment.preferences());
        writePlain(saves, example.name, *forThePlainXml->document, false,
                   environment.preferences());
    }
}

/// The thirteen test rockets, loaded from their re-saves: an archive with the default storage
/// options.
void writeTheTestRockets(InteropSaves& saves)
{
    for (const GoldenInput& input : QtRocket::Test::testRocketInputs(saves.problems))
    {
        const Result<LoadedResave> resave = QtRocket::Test::loadResave(input);
        if (!resave)
        {
            saves.problems += std::format("{}: {}\n", input.name, resave.error().toString());
            continue;
        }
        OpenRocketDocument& document = *resave->loaded.document;
        writeArchive(saves, input.name, document, document.getDefaultStorageOptions(),
                     resave->environment->preferences());
    }
}

/// The eighteen legacy files: an archive with the simulation data, or plain XML with it where
/// OpenRocket's archive fails (archiveError).
void writeTheLegacyFiles(InteropSaves& saves)
{
    for (const ResaveInput& input :
         QtRocket::Test::resaveInputsOf(QtRocket::Test::kLegacyKind, saves.problems))
    {
        const DesignFileEnvironment  environment;
        const GeneralRocketLoader    loader(environment.context());
        const Result<LoadedDocument> loaded = loader.load(QtRocket::Test::legacyFileOf(input));
        if (!loaded)
        {
            saves.problems += std::format("{}: {}\n", input.name, loaded.error().toString());
            continue;
        }
        if (!input.archiveError.empty())
        {
            writePlain(saves, input.name, *loaded->document, true, environment.preferences());
            continue;
        }
        StorageOptions options;
        options.setSaveSimulationData(true);
        writeArchive(saves, input.name, *loaded->document, options, environment.preferences());
    }
}

/// The saver-coverage documents, loaded from their re-saves: an archive with the default storage
/// options.
void writeTheCoverageDocuments(InteropSaves& saves)
{
    for (const ResaveInput& input :
         QtRocket::Test::resaveInputsOf(QtRocket::Test::kCoverageKind, saves.problems))
    {
        const DesignFileEnvironment  environment;
        const GeneralRocketLoader    loader(environment.context());
        const Result<LoadedDocument> loaded =
            loader.load(QtRocket::Test::goldensDir() / input.resave);
        if (!loaded)
        {
            saves.problems += std::format("{}: {}\n", input.name, loaded.error().toString());
            continue;
        }
        OpenRocketDocument& document = *loaded->document;
        writeArchive(saves, input.name, document, document.getDefaultStorageOptions(),
                     environment.preferences());
    }
}

// Writes the 65 saves of the interop check (see the top of this file) and prints where.
TEST(OrkInteropSaves, DISABLED_WritesTheSavesOfTheInteropCheck)
{
    const std::optional<std::filesystem::path> directory = savesDirectory();
    // A plain if: clang-tidy's unchecked-optional-access check cannot see through ASSERT_TRUE.
    if (!directory.has_value())
    {
        FAIL() << "the operating system does not say where the test program is";
    }
    InteropSaves saves{
        .directory = *directory, .files = 0, .problems = preparedDirectory(*directory)};
    writeTheExamples(saves);
    writeTheTestRockets(saves);
    writeTheLegacyFiles(saves);
    writeTheCoverageDocuments(saves);
    EXPECT_EQ(saves.problems, "");
    EXPECT_EQ(saves.files, 65);
    std::println("{} saves in {}", saves.files, QtRocket::pathToUtf8(*directory));
}

}  // namespace
