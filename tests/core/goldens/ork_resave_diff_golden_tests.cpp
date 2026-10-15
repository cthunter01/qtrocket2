// QtRocket's .ork save of OpenRocket's designs against OpenRocket's own re-save of them
// (tests/data/goldens/<input>/resave/rocket.ork, written by the golden harness with
// OpenRocketSaver and saveSimulationData=false, on the design as loaded or built, before
// anything was settled), the whole text against the whole text (compareOrkTexts() of
// GoldenResaves.h: the creator attribute of the root is the only text set aside; a number written
// otherwise but of the same value would be counted, and none is).
//
// The inputs (decision O4 of tier 10):
// - The sixteen examples, each loaded fresh from the ORIGINAL archive under
//   goldenExampleEnvironment() with the six presets and the bundled motor database, and saved
//   without simulation data with the environment's own preference store, as the harness saved
//   them: the save is the first thing done to the loaded design (the getters of automatic
//   dimensions store what they compute, so a save depends on what was asked before it; see
//   AutomaticDimensions.h). Every line is OpenRocket's: the declaration and the root, the
//   rocket with the <motormount> blocks and the motors of the database, the simulations (the
//   conditions, the extensions, the summary values and the stored warnings), the photo studio
//   and the document preferences.
// - The same design saved again: three more saves of each loaded example. OpenRocket's own
//   saves of one loaded document differ in one example only, the dual parachute deployment, in
//   three lines of its rocket (the probe RepeatSaveProbe of the scouts of tier 10,
//   roundtrip-goldens out/repeat/: a body tube's automatic radius and a parachute's automatic
//   packed radius are stored by the first save's getters, then the packed length by the second's),
//   and the fourth save is the third. QtRocket's saves have to do the same, which holds the order
//   of the getter calls of the savers to OpenRocket's.
// - The same design saved once more WITH its simulation data (all sixteen store full flight
//   data) and loaded back from the text in memory, without a directory (decision O5 of tier 10):
//   every flight comes back bit for bit (flightDifferences(): the summary values, the stored
//   warnings, the branches with their events and every value), and a save of the reload is the
//   first save with data but for one known difference, the sixteen stored warnings of the type
//   RecoveryHighSpeedDeployment (in six examples), which the loader reads back as warnings of the
//   type Other without their parameter, as OpenRocket's does, until the user's decision E2 of
//   2026-10-14 (group 'decisions') makes it read them as they were written. OpenRocket's own save
//   with data of the same sixteen loads (the scouts' SaverProbe, saver-simulations out/java/) is
//   QtRocket's line for line; those files are not part of the repository.
// - The three examples with a <preset> again with no presets (Presets::NONE): the six
//   references that cannot be resolved are kept (the user's decision U8 of 2026-10-08) and
//   written back, so the file is the re-save's all the same.
// - The thirteen test rockets as save(load(re-save)) (loadResave() of GoldenResaves.h, with the
//   seven motors of TestRockets and OpenRocket's test preferences): the re-save but for one line,
//   the wall of the pod body of the Alpha III with pods, 2 mm in the file and none in the loaded
//   tube (a tube without a radius keeps no wall), as OpenRocket's own reload has it.
//
// The same loop pins OpenRocket's estimateFileSize() of each loaded example (one load per
// example serves everything).

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/StorageOptions.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/file/openrocket/OpenRocketSaver.h"
#include "QtRocket/logging/ErrorSet.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"
#include "file/DesignFileEnvironment.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenExamples.h"
#include "goldens/GoldenResaves.h"
#include "simulation/FlightDifferences.h"

namespace
{

using QtRocket::ErrorSet;
using QtRocket::FlightData;
using QtRocket::GeneralRocketLoader;
using QtRocket::LoadedDocument;
using QtRocket::OpenRocketDocument;
using QtRocket::OpenRocketSaver;
using QtRocket::Preferences;
using QtRocket::Result;
using QtRocket::StorageOptions;
using QtRocket::WarningSet;
using QtRocket::Test::compareOrkTexts;
using QtRocket::Test::DesignFileEnvironment;
using QtRocket::Test::ExampleSource;
using QtRocket::Test::flightDifferences;
using QtRocket::Test::GoldenExample;
using QtRocket::Test::goldenExampleEnvironment;
using QtRocket::Test::goldenExamples;
using QtRocket::Test::GoldenInput;
using QtRocket::Test::kMaxReportedLines;
using QtRocket::Test::kPods;
using QtRocket::Test::LoadedResave;
using QtRocket::Test::loadGoldenExample;
using QtRocket::Test::loadResave;
using QtRocket::Test::OrkTextComparison;
using QtRocket::Test::rocketElementOf;
using QtRocket::Test::sameByValue;
using QtRocket::Test::testRocketInputs;
using QtRocket::Test::withoutCreator;

using Presets = DesignFileEnvironment::Presets;

/// OpenRocket's estimateFileSize() of an example as loaded, with the document's default storage
/// options (which save the simulation data of all sixteen): the scouts' Java probe of tier 10
/// (saver-simulations, out/estimates.txt).
struct EstimatedSize
{
    std::string_view file;
    std::int64_t     bytes;
};

constexpr std::array<EstimatedSize, 16> kEstimatedSizes{{
    {.file = "3D printable nose cone and fins.ork", .bytes = 200680},
    {.file = "Airstart timing.ork", .bytes = 328440},
    {.file = "ARC payload rocket.ork", .bytes = 142760},
    {.file = "A simple model rocket.ork", .bytes = 259540},
    {.file = "Chute release.ork", .bytes = 102800},
    {.file = "Clustered motors.ork", .bytes = 153060},
    {.file = "Deployable payload.ork", .bytes = 422520},
    {.file = "Dual parachute deployment.ork", .bytes = 335500},
    {.file = "Parallel booster staging.ork", .bytes = 240040},
    {.file = "Pods--airframes and winglets.ork", .bytes = 243520},
    {.file = "Pods--powered with recovery deployment.ork", .bytes = 38700},
    {.file = "Simulation extensions.ork", .bytes = 310820},
    {.file = "Simulation scripting.ork", .bytes = 329120},
    {.file = "Three stage low power rocket.ork", .bytes = 394640},
    {.file = "Tube fin rocket.ork", .bytes = 65320},
    {.file = "Two stage high power rocket.ork", .bytes = 160300},
}};

/// The pinned estimate of the example in @p file, or -1.
[[nodiscard]] std::int64_t estimatedSizeOf(std::string_view file)
{
    for (const EstimatedSize& size : kEstimatedSizes)
    {
        if (size.file == file)
        {
            return size.bytes;
        }
    }
    return -1;
}

/// The lines of @p text, each without its '\n'.
[[nodiscard]] std::vector<std::string_view> linesOf(std::string_view text)
{
    std::vector<std::string_view> lines;
    while (!text.empty())
    {
        const std::size_t end = text.find('\n');
        lines.push_back(text.substr(0, end));
        text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
    }
    return lines;
}

/// The lines that differ between @p before and @p after, two texts of the same number of lines,
/// each as "<line of before> -> <line of after>" without the indentation, one per line; a text
/// of another number of lines is named instead.
[[nodiscard]] std::string changedLines(std::string_view before, std::string_view after)
{
    const std::vector<std::string_view> a = linesOf(before);
    const std::vector<std::string_view> b = linesOf(after);
    if (a.size() != b.size())
    {
        return std::format("{} lines -> {} lines\n", a.size(), b.size());
    }
    std::string changes;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        if (a[i] != b[i])
        {
            const std::size_t from = a[i].find_first_not_of(' ');
            const std::size_t to   = b[i].find_first_not_of(' ');
            changes +=
                std::format("{} -> {}\n", a[i].substr(from == std::string_view::npos ? 0 : from),
                            b[i].substr(to == std::string_view::npos ? 0 : to));
        }
    }
    return changes;
}

/// Saves @p document, with its simulation data when @p withData, with @p preferences; the
/// warnings of the save go to @p warnings. The text, or the Error's text.
[[nodiscard]] std::string saveOf(OpenRocketDocument& document, const Preferences& preferences,
                                 WarningSet& warnings, bool withData)
{
    StorageOptions options;
    options.setSaveSimulationData(withData);
    ErrorSet                  errors;
    const Result<std::string> saved =
        OpenRocketSaver::save(document, options, preferences, warnings, errors);
    if (!saved)
    {
        return "ERROR " + saved.error().toString();
    }
    return *saved;
}

/// Saves @p document without simulation data with @p preferences, as the harness wrote the
/// re-saves (saveOf()).
[[nodiscard]] std::string saveWithoutData(OpenRocketDocument& document,
                                          const Preferences& preferences, WarningSet& warnings)
{
    return saveOf(document, preferences, warnings, false);
}

/// What a set of saves compared with their re-saves gave.
struct SavesCompared
{
    int         designs{0};      ///< the designs saved and compared
    std::size_t lines{0};        ///< the lines of their saves
    std::size_t byValue{0};      ///< the lines of the same numbers written otherwise
    std::size_t components{0};   ///< the components of the saves (the <id> lines of the rockets)
    std::size_t presets{0};      ///< the <preset> lines of the saves
    std::size_t motors{0};       ///< the <motor> elements of the saves
    std::size_t simulations{0};  ///< the <simulation> elements of the saves
    std::size_t warnings{0};     ///< the stored warnings of the saves
    std::string problems;        ///< what differs, by design: empty when nothing does
};

/// Whether @p line is an <id> element, with any indentation.
[[nodiscard]] bool isIdLine(std::string_view line)
{
    const std::size_t start = line.find_first_not_of(' ');
    return start != std::string_view::npos && line.substr(start).starts_with("<id>");
}

/// Counts the components, presets and motors of the rocket of @p saved and its simulations and
/// stored warnings into @p compared.
void countParts(SavesCompared& compared, std::string_view saved)
{
    for (const std::string_view line : linesOf(rocketElementOf(saved)))
    {
        compared.components += isIdLine(line) ? 1U : 0U;
        compared.presets += line.contains("<preset ") ? 1U : 0U;
        compared.motors += line.contains("<motor configid=") ? 1U : 0U;
    }
    for (const std::string_view line : linesOf(saved))
    {
        compared.simulations += line.contains("<simulation status=") ? 1U : 0U;
        compared.warnings += line.contains("<warning type=") ? 1U : 0U;
    }
}

/// Compares @p saved, QtRocket's save of the design @p name, with @p resave, the whole text, and
/// adds the result to @p compared; a difference is a problem unless it is @p expected
/// (changedLines() from the re-save to the save).
void compareSaves(SavesCompared& compared, std::string_view name, std::string_view saved,
                  std::string_view resave, std::string_view expected = "")
{
    const OrkTextComparison comparison = compareOrkTexts(saved, resave);
    compared.designs++;
    compared.lines += comparison.lines;
    compared.byValue += comparison.byValue;
    countParts(compared, saved);
    if (saved.empty() || resave.empty())
    {
        compared.problems += std::format("{}: nothing to compare\n", name);
        return;
    }
    if (expected.empty() ? comparison.differing != 0
                         : changedLines(withoutCreator(resave), withoutCreator(saved)) != expected)
    {
        compared.problems +=
            std::format("{}: the save differs from the re-save\n{}", name, comparison.differences);
    }
}

/// The lines in which OpenRocket's saves 2, 3 and 4 of one loaded document differ from the save
/// before, by example (changedLines()): the dual parachute deployment's (RepeatSaveProbe); no
/// other example's change.
struct RepeatedSaves
{
    std::string_view example;
    std::string_view second;
    std::string_view third;
};

constexpr RepeatedSaves kDualParachuteSaves{
    .example = "example-dual-parachute-deployment",
    .second =
        "<radius>auto 0.025</radius> -> <radius>auto 0.028321</radius>\n"
        "<packedradius>auto 0.024009400000000004</packedradius> -> "
        "<packedradius>auto 0.0273304</packedradius>\n",
    .third =
        "<packedlength>0.11285771168170697</packedlength> -> "
        "<packedlength>0.08709673571783902</packedlength>\n"};

/// Saves @p document three more times after its first save @p first and checks the whole texts
/// against kDualParachuteSaves (nothing changes for any other example). Returns the problems.
[[nodiscard]] std::string repeatedSaveProblems(const GoldenExample& example,
                                               OpenRocketDocument&  document,
                                               const std::string&   first)
{
    const Preferences& preferences = goldenExampleEnvironment().preferences();
    const bool         dual        = example.name == kDualParachuteSaves.example;
    std::string        problems;
    std::string        previous = first;
    for (int save = 2; save <= 4; ++save)
    {
        WarningSet       warnings;
        std::string      text = saveWithoutData(document, preferences, warnings);
        std::string_view expected;
        if (dual && save == 2)
        {
            expected = kDualParachuteSaves.second;
        }
        else if (dual && save == 3)
        {
            expected = kDualParachuteSaves.third;
        }
        const std::string changes = changedLines(previous, text);
        if (changes != expected)
        {
            problems += std::format("{}: save {} changes\n{}where OpenRocket's changes\n{}",
                                    example.name, save, changes, expected);
        }
        previous = std::move(text);
    }
    return problems;
}

/// The line that begins a stored warning of the type RecoveryHighSpeedDeployment, without its
/// indentation.
constexpr std::string_view kHighSpeedWarning = "<warning type=\"RecoveryHighSpeedDeployment\">";

/// @p text as it is saved again after a load before the user's decision E2 of 2026-10-14 (group
/// 'decisions') is merged: every stored warning of the type RecoveryHighSpeedDeployment (what the
/// saver writes, Java's class name) of the type Other and without its <parameter> line, as the
/// loader reads it back (it knows the type by the name HighSpeedDeployment only, as OpenRocket's
/// loader). Counts those warnings into @p count.
[[nodiscard]] std::string asReadBackBeforeE2(std::string_view text, std::size_t& count)
{
    std::string result;
    result.reserve(text.size());
    bool inHighSpeedWarning = false;
    for (const std::string_view line : linesOf(text))
    {
        const std::string_view trimmed = QtRocket::Strings::trim(line);
        if (trimmed == kHighSpeedWarning)
        {
            ++count;
            inHighSpeedWarning = true;
            result += std::format("{}<warning type=\"Other\">\n",
                                  line.substr(0, line.size() - trimmed.size()));
            continue;
        }
        if (inHighSpeedWarning && trimmed.starts_with("<parameter>"))
        {
            continue;
        }
        if (trimmed == "</warning>")
        {
            inHighSpeedWarning = false;
        }
        result += std::format("{}\n", line);
    }
    return result;
}

/// @p text without its <rocket> element (rocketElementOf()).
[[nodiscard]] std::string withoutRocket(std::string_view text)
{
    const std::string rocket = rocketElementOf(text);
    std::string       result(text);
    const std::size_t at = result.find(rocket);
    if (!rocket.empty() && at != std::string::npos)
    {
        result.erase(at, rocket.size());
    }
    return result;
}

/// What the round trips of the examples' flight data gave.
struct DataRoundTrips
{
    std::size_t simulations{0};  ///< the simulations whose flight data were compared
    std::size_t branches{0};     ///< their branches
    /// The stored warnings of the type RecoveryHighSpeedDeployment that came back as Other
    /// (asReadBackBeforeE2()).
    std::size_t highSpeedWarnings{0};
    std::string problems;  ///< what differs, by example: empty when nothing does
};

/// The flight data of the simulations of @p document against those of @p reloaded, the same
/// document saved and loaded back: what differs (flightDifferences()), counted into @p trips.
[[nodiscard]] std::string flightProblems(const OpenRocketDocument& document,
                                         const OpenRocketDocument& reloaded, DataRoundTrips& trips)
{
    if (document.getSimulationCount() != reloaded.getSimulationCount())
    {
        return std::format("{} simulations back, not {}\n", reloaded.getSimulationCount(),
                           document.getSimulationCount());
    }
    std::string problems;
    for (std::size_t i = 0; i < document.getSimulationCount(); ++i)
    {
        const std::shared_ptr<FlightData>& ours = document.getSimulation(i)->getSimulatedData();
        const std::shared_ptr<FlightData>& back = reloaded.getSimulation(i)->getSimulatedData();
        if (ours == nullptr || back == nullptr)
        {
            problems += ours == back ? "" : std::format("simulation {}: flight data lost\n", i);
            continue;
        }
        trips.simulations++;
        trips.branches += ours->getBranchCount();
        const std::string differences = flightDifferences(*ours, *back);
        if (!differences.empty())
        {
            problems += std::format("simulation {}:\n{}", i, differences);
        }
    }
    return problems;
}

/// Saves @p document, an example, with its simulation data, loads the text back from memory
/// without a directory, compares the flight data and the save of the reload with the first save
/// (asReadBackBeforeE2()), all but the rocket: the rocket of a design read from a plain text has
/// no decal images (decision O5 of tier 10: they go through the archive), and the first save of a
/// load writes the automatic dimensions the dual parachute deployment's first save writes
/// (kDualParachuteSaves), not those of a design saved before. Adds the result to @p trips.
void roundTripTheData(const GoldenExample& example, OpenRocketDocument& document,
                      DataRoundTrips& trips)
{
    const Preferences&           preferences = goldenExampleEnvironment().preferences();
    WarningSet                   warnings;
    const std::string            first = saveOf(document, preferences, warnings, true);
    const GeneralRocketLoader    loader(goldenExampleEnvironment().context());
    const Result<LoadedDocument> loaded = loader.load(QtRocket::stringToBytes(first));
    if (!loaded)
    {
        trips.problems += std::format("{}: {}\n", example.name, loaded.error().toString());
        return;
    }
    const std::string flights = flightProblems(document, *loaded->document, trips);
    if (!flights.empty())
    {
        trips.problems += std::format("{}: the flight data differ\n{}", example.name, flights);
    }
    const std::string second =
        withoutRocket(saveOf(*loaded->document, preferences, warnings, true));
    const std::string expected = asReadBackBeforeE2(withoutRocket(first), trips.highSpeedWarnings);
    if (second != expected)
    {
        trips.problems += std::format("{}: the second save with data differs from the first\n{}",
                                      example.name, compareOrkTexts(second, expected).differences);
    }
    if (!warnings.empty())
    {
        trips.problems += std::format("{}: {} save warnings\n", example.name, warnings.size());
    }
}

/// Loads @p example fresh, pins its estimate, saves it and compares the whole text with the
/// re-save, then saves it three times more (repeatedSaveProblems()) and once with its data
/// (roundTripTheData()). Adds the comparisons to @p saves and @p trips; returns the other
/// problems.
[[nodiscard]] std::string exampleProblems(const GoldenExample& example, SavesCompared& saves,
                                          DataRoundTrips& trips)
{
    const Result<LoadedDocument> loaded = loadGoldenExample(example);
    if (!loaded)
    {
        return std::format("{}: {}\n", example.name, loaded.error().toString());
    }
    std::string         problems;
    OpenRocketDocument& document = *loaded->document;

    const std::int64_t estimate =
        OpenRocketSaver::estimateFileSize(document, document.getDefaultStorageOptions());
    if (estimate != estimatedSizeOf(example.file))
    {
        problems += std::format("{}: estimateFileSize {} where OpenRocket's is {}\n", example.name,
                                estimate, estimatedSizeOf(example.file));
    }

    WarningSet        warnings;
    const std::string saved =
        saveWithoutData(document, goldenExampleEnvironment().preferences(), warnings);
    const Result<std::string> resave = QtRocket::readTextFile(example.resave);
    if (!resave)
    {
        return problems + std::format("{}: no re-save to compare\n", example.name);
    }
    if (!warnings.empty())
    {
        problems += std::format("{}: {} save warnings\n", example.name, warnings.size());
    }
    compareSaves(saves, example.name, saved, *resave);
    problems += repeatedSaveProblems(example, document, saved);
    roundTripTheData(example, document, trips);
    return problems;
}

/// Saves each of the sixteen examples (exampleProblems()): the problems besides those of the
/// comparisons, which go to @p saves and @p trips.
[[nodiscard]] std::string saveTheExamples(SavesCompared& saves, DataRoundTrips& trips)
{
    std::string problems;
    for (const GoldenExample& example : goldenExamples())
    {
        problems += exampleProblems(example, saves, trips);
    }
    return problems;
}

// Each of the sixteen examples is saved as OpenRocket re-saved it, every one of the 10000 lines:
// the declaration, the root of version 1.11, the rocket (327 components, 65 motors and the six
// presets), the 54 simulations with their conditions, extensions, summary values and 20 stored
// warnings, no custom expression, the photo studio (empty in fourteen and 24 settings in two), no
// document preference and the document materials of each. Not one number is written otherwise
// than OpenRocket writes it. With it: OpenRocket's estimate of each file's size; the saves of one
// loaded document that follow the first change as OpenRocket's do; and saved with their data and
// loaded back, the flights of the 54 simulations (69 branches) come back bit for bit, and a
// second save is the first but for the sixteen warnings of decision E2.
TEST(OrkResaveDiffGolden, TheExamplesAreSavedAsOpenRocketResavedThem)
{
    SavesCompared  saves;
    DataRoundTrips trips;
    EXPECT_EQ(saveTheExamples(saves, trips), "");
    EXPECT_EQ(saves.problems, "");
    EXPECT_EQ(saves.designs, 16);
    EXPECT_EQ(saves.lines, 10000U);  // the lines of the sixteen re-saves, as it happens
    EXPECT_EQ(saves.components, 327U);
    EXPECT_EQ(saves.byValue, 0U);
    EXPECT_EQ(saves.presets, 6U);
    EXPECT_EQ(saves.motors, 65U);
    EXPECT_EQ(saves.simulations, 54U);
    EXPECT_EQ(saves.warnings, 20U);
    EXPECT_EQ(goldenExamples().size(), kEstimatedSizes.size());

    EXPECT_EQ(trips.problems, "");
    EXPECT_EQ(trips.simulations, 54U);
    EXPECT_EQ(trips.branches, 69U);
    // Decision E2 of 2026-10-14 (group 'decisions'): the loader is to read the type
    // RecoveryHighSpeedDeployment back. Once that is merged, the second save equals the first and
    // asReadBackBeforeE2() turns this known difference into a reported one: then compare the two
    // saves as they are and drop this pin.
    EXPECT_EQ(trips.highSpeedWarnings, 16U);
}

/// The beginnings of the document materials whose order the presets change.
constexpr std::string_view kSpiralKraft =
    "<material>BULK|Paper, spiral kraft glassine, Estes avg, bulk|894.4|0.0|";
constexpr std::string_view kCarpetThread = "<material>LINE|Carpet Thread|3.3E-4|0.0|";
constexpr std::string_view kHdpeFilm =
    "<material>SURFACE|Polyethylene film, HDPE, 1.0 mil, bare|0.0235|0.0|";

/// changedLines() of two document materials @p prefix ... that changed places: the one of the
/// group Other and the one of the group Custom.
[[nodiscard]] std::string swappedMaterials(std::string_view prefix)
{
    return std::format(
        "{0}Other</material> -> {0}Custom</material>\n"
        "{0}Custom</material> -> {0}Other</material>\n",
        prefix);
}

/// The lines in which the save of an example loaded without presets differs from OpenRocket's
/// re-save, which was made with the presets (changedLines() from the re-save to the save): the
/// order of two document materials of one name and density, one with the group of the file
/// (Custom) and one with that of a preset (Other). OpenRocket's own save of the same loads
/// without presets has QtRocket's order (the probe MaterialsProbe of tier 10a S3, Boot2 "none"):
/// Java's Database puts a material before one that compares equal (name, then density), so two
/// such materials stand in the order the load added them, which the presets change.
[[nodiscard]] std::string withoutPresetsChanges(std::string_view name)
{
    if (name == "example-3d-printable-nose-cone-and-fins")
    {
        return swappedMaterials(kSpiralKraft);
    }
    if (name == "example-deployable-payload")
    {
        return swappedMaterials(kCarpetThread) + swappedMaterials(kHdpeFilm);
    }
    return {};
}

/// Saves the examples whose re-save holds a <preset>, loaded without presets, into @p saves;
/// counts the others in @p withoutPresets. Returns what could not be loaded or read.
[[nodiscard]] std::string saveTheExamplesWithoutPresets(SavesCompared& saves, int& withoutPresets)
{
    std::string problems;
    for (const GoldenExample& example : goldenExamples())
    {
        const Result<std::string> resave = QtRocket::readTextFile(example.resave);
        if (!resave)
        {
            problems += std::format("{}: no re-save\n", example.name);
            continue;
        }
        if (!resave->contains("<preset "))
        {
            withoutPresets++;
            continue;
        }
        const Result<LoadedDocument> loaded =
            loadGoldenExample(example, ExampleSource::ORIGINAL, Presets::NONE);
        if (!loaded)
        {
            problems += std::format("{}: {}\n", example.name, loaded.error().toString());
            continue;
        }
        WarningSet        warnings;
        const std::string saved = saveWithoutData(
            *loaded->document, goldenExampleEnvironment(Presets::NONE).preferences(), warnings);
        compareSaves(saves, example.name, saved, *resave, withoutPresetsChanges(example.name));
    }
    return problems;
}

// The three examples with a <preset> loaded without presets (Presets::NONE: six "No matching
// ComponentPreset" warnings) keep the six references (decision U8) and write them back: the file
// is the re-save's, line for line, but for the order of three pairs of document materials, which
// OpenRocket's own saves without presets have too (withoutPresetsChanges()). The thirteen other
// re-saves hold no <preset>.
TEST(OrkResaveDiffGolden, TheExamplesWithoutPresetsWriteTheirReferencesBack)
{
    SavesCompared saves;
    int           withoutPresets = 0;
    EXPECT_EQ(saveTheExamplesWithoutPresets(saves, withoutPresets), "");
    EXPECT_EQ(saves.problems, "");
    EXPECT_EQ(saves.designs, 3);
    EXPECT_EQ(saves.lines, 1658U);
    EXPECT_EQ(withoutPresets, 13);
    EXPECT_EQ(saves.presets, 6U);
    EXPECT_EQ(saves.byValue, 0U);
}

/// The one line in which QtRocket's save of the loaded re-save of the Alpha III with pods
/// differs from the re-save (changedLines() from the re-save to the save): the wall of the pod
/// body.
constexpr std::string_view kPodBodyWall =
    "<thickness>0.002</thickness> -> <thickness>0.0</thickness>\n";

/// Loads the re-save of each test rocket and saves it again into @p saves. Returns what could
/// not be loaded or read, and the saves that warned.
[[nodiscard]] std::string saveTheTestRockets(SavesCompared& saves)
{
    std::string                    problems;
    const std::vector<GoldenInput> inputs = testRocketInputs(problems);
    for (const GoldenInput& input : inputs)
    {
        const Result<LoadedResave> resave = loadResave(input);
        const Result<std::string>  file =
            QtRocket::readTextFile(QtRocket::Test::goldensDir() / input.resave);
        if (!resave || !file)
        {
            problems += std::format("{}: no load or no re-save\n", input.name);
            continue;
        }
        WarningSet        warnings;
        const std::string saved =
            saveWithoutData(*resave->loaded.document, resave->environment->preferences(), warnings);
        if (!warnings.empty())
        {
            problems += std::format("{}: {} save warnings\n", input.name, warnings.size());
        }
        compareSaves(saves, input.name, saved, *file, input.name == kPods ? kPodBodyWall : "");
    }
    return problems;
}

// The thirteen test rockets, loaded from their re-saves against the seven motors of TestRockets
// and saved again, are their re-saves but for the wall of the pod body of the Alpha III with pods
// (OpenRocket's own reload has no wall there either: ResaveGoldenExceptions): 5090 lines with 151
// components, 48 motors and 50 simulations (a fixed seed, multi-level winds, the WGS84
// computation).
TEST(OrkResaveDiffGolden, TheTestRocketsAreSavedAsOpenRocketResavedThem)
{
    SavesCompared saves;
    EXPECT_EQ(saveTheTestRockets(saves), "");
    EXPECT_EQ(saves.problems, "");
    EXPECT_EQ(saves.designs, 13);
    EXPECT_EQ(saves.lines, 5090U);
    EXPECT_EQ(saves.components, 151U);
    EXPECT_EQ(saves.byValue, 0U);
    EXPECT_EQ(saves.presets, 0U);
    EXPECT_EQ(saves.motors, 48U);
    EXPECT_EQ(saves.simulations, 50U);
}

// What a second save after a load makes of a stored warning of the type
// RecoveryHighSpeedDeployment until decision E2 is merged: a warning of the type Other without
// its parameter; every other line stays.
TEST(OrkResaveDiffGolden, AHighSpeedWarningIsReadBackAsOtherBeforeE2)
{
    constexpr std::string_view kText =
        "        <warning type=\"RecoveryHighSpeedDeployment\">\n"
        "          <id>9e26b796-6349-4902-ba33-14816cf9d3bd</id>\n"
        "          <parameter>31.94803510503947</parameter>\n"
        "          Text\n"
        "        </warning>\n"
        "        <warning type=\"LargeAOA\">\n"
        "          <parameter>0.5</parameter>\n"
        "        </warning>\n";
    std::size_t count = 0;
    EXPECT_EQ(asReadBackBeforeE2(kText, count),
              "        <warning type=\"Other\">\n"
              "          <id>9e26b796-6349-4902-ba33-14816cf9d3bd</id>\n"
              "          Text\n"
              "        </warning>\n"
              "        <warning type=\"LargeAOA\">\n"
              "          <parameter>0.5</parameter>\n"
              "        </warning>\n");
    EXPECT_EQ(count, 1U);
}

/// A rocket element of a few lines, with the root around it.
constexpr std::string_view kRocket =
    "<openrocket version=\"1.11\" creator=\"QtRocket 0.1.0\">\n"
    "  <rocket>\n"
    "    <name>R</name>\n"
    "    <id>5b3bdde2-5a1e-4223-87ba-cc52d1641209</id>\n"
    "    <axialoffset method=\"absolute\">0.001</axialoffset>\n"
    "    <designtype>original</designtype>\n"
    "  </rocket>\n"
    "</openrocket>\n";

/// kRocket with the first @p from replaced by @p to.
[[nodiscard]] std::string changed(std::string_view from, std::string_view to)
{
    std::string text(kRocket);
    return text.replace(text.find(from), from.size(), to);
}

// The comparison of the texts: the creator is the only text set aside; a number of the same value
// written otherwise is counted, not reported (none is expected of a save); a changed digit, a
// carriage return, a line missing and two lines in another order are reported in their lines.
TEST(OrkResaveDiffGolden, TheComparisonReportsEveryChangeButTheCreator)
{
    const OrkTextComparison same =
        compareOrkTexts(kRocket, changed("QtRocket 0.1.0", "OpenRocket 26.xx"));
    EXPECT_EQ(same.lines, 8U);
    EXPECT_EQ(same.differing, 0U);
    EXPECT_EQ(same.byValue, 0U);
    EXPECT_EQ(same.differences, "");

    const OrkTextComparison byValue = compareOrkTexts(kRocket, changed("0.001", "1.0E-3"));
    EXPECT_EQ(byValue.differing, 0U);
    EXPECT_EQ(byValue.byValue, 1U);

    const OrkTextComparison digit = compareOrkTexts(kRocket, changed("0.001", "0.002"));
    EXPECT_EQ(digit.differing, 1U);
    EXPECT_EQ(digit.differences,
              "line 5:\n"
              "  QtRocket:       <axialoffset method=\"absolute\">0.001</axialoffset>\n"
              "  OpenRocket:     <axialoffset method=\"absolute\">0.002</axialoffset>\n");

    const OrkTextComparison cr =
        compareOrkTexts(kRocket, changed("<name>R</name>\n", "<name>R</name>\r\n"));
    EXPECT_EQ(cr.differing, 1U);
    EXPECT_EQ(cr.differences,
              "line 3:\n"
              "  QtRocket:       <name>R</name>\n"
              "  OpenRocket:     <name>R</name>\\r\n");

    const OrkTextComparison reordered = compareOrkTexts(
        kRocket,
        changed("    <name>R</name>\n    <id>5b3bdde2-5a1e-4223-87ba-cc52d1641209</id>\n",
                "    <id>5b3bdde2-5a1e-4223-87ba-cc52d1641209</id>\n    <name>R</name>\n"));
    EXPECT_EQ(reordered.differing, 2U);

    const OrkTextComparison missing = compareOrkTexts(kRocket, changed("</openrocket>\n", ""));
    EXPECT_EQ(missing.differing, 1U);
    EXPECT_EQ(missing.differences, "line 8:\n  QtRocket:   </openrocket>\n  OpenRocket: (none)\n");
}

// A report names at most kMaxReportedLines lines and counts the others.
TEST(OrkResaveDiffGolden, TheComparisonCountsWhatItDoesNotShow)
{
    std::string ours;
    std::string theirs;
    for (std::size_t i = 0; i < kMaxReportedLines + 3; ++i)
    {
        ours += std::format("<a>{}</a>\n", i);
        theirs += std::format("<b>{}</b>\n", i);
    }
    const OrkTextComparison comparison = compareOrkTexts(ours, theirs);
    EXPECT_EQ(comparison.differing, kMaxReportedLines + 3);
    EXPECT_TRUE(comparison.differences.ends_with("... and 3 more lines\n"))
        << comparison.differences;
}

// What is a number of the same value: the text around the numbers is the same, the numbers read
// as the same double (bit for bit: -0.0 is not 0.0), and one at least is written otherwise. The
// digits inside a word (an id) are no number of their own.
TEST(OrkResaveDiffGolden, TheSameValueIsTheSameDoubleInTheSameText)
{
    EXPECT_TRUE(sameByValue("<x>1.0E-3</x>", "<x>0.001</x>"));
    EXPECT_TRUE(sameByValue(R"(<p x="9.999999999999999E22" y="1"/>)", R"(<p x="1.0E23" y="1"/>)"));
    EXPECT_TRUE(sameByValue("<x>-1.5</x>", "<x>-1.50</x>"));
    EXPECT_FALSE(sameByValue("<x>0.001</x>", "<x>0.001</x>")) << "nothing written otherwise";
    EXPECT_FALSE(sameByValue("<x>0.0</x>", "<x>-0.0</x>"));
    EXPECT_FALSE(sameByValue("<x>0.001</x>", "<x>0.002</x>"));
    EXPECT_FALSE(sameByValue("<x>0.001</x>", "<y>1.0E-3</y>"));
    EXPECT_FALSE(sameByValue("<x>1</x><x>2</x>", "<x>1</x>"));
    EXPECT_FALSE(sameByValue("<id>a5b</id>", "<id>a05b</id>"));
    EXPECT_TRUE(sameByValue("<id>5b</id>", "<id>05b</id>")) << "a number before a letter";
    EXPECT_FALSE(sameByValue("<x>-0.5</x>", "<x>0.5</x>")) << "the sign is the number's";
    EXPECT_TRUE(sameByValue("<id>2-5</id>", "<id>2-5.0</id>")) << "a '-' after a digit is no sign";
}

// The rocket element of a text, from "  <rocket>" to "  </rocket>".
TEST(OrkResaveDiffGolden, TheRocketElementIsItsLines)
{
    EXPECT_EQ(rocketElementOf(kRocket),
              "  <rocket>\n"
              "    <name>R</name>\n"
              "    <id>5b3bdde2-5a1e-4223-87ba-cc52d1641209</id>\n"
              "    <axialoffset method=\"absolute\">0.001</axialoffset>\n"
              "    <designtype>original</designtype>\n"
              "  </rocket>\n");
    EXPECT_EQ(rocketElementOf("  <rocket>\n  </rocket>\n"), "  <rocket>\n  </rocket>\n");
    EXPECT_EQ(rocketElementOf("<openrocket>\n  <rocket>\n"), "");
    EXPECT_EQ(rocketElementOf("<openrocket/>\n"), "");
}

}  // namespace
