// QtRocket's .ork save of OpenRocket's designs against OpenRocket's own re-save of them
// (tests/data/goldens/<input>/resave/rocket.ork, written by the golden harness with
// OpenRocketSaver and saveSimulationData=false, on the design as loaded or built, before
// anything was settled), text against text (compareOrkTexts() of GoldenResaves.h: the creator
// attribute of the root is the only text set aside; a number written otherwise but of the same
// value would be counted, and none is).
//
// The inputs (decision O4 of tier 10):
// - The sixteen examples, each loaded fresh from the ORIGINAL archive under
//   goldenExampleEnvironment() with the six presets and the bundled motor database, and saved
//   without simulation data with the environment's own preference store, as the harness saved
//   them: the save is the first thing done to the loaded design (the getters of automatic
//   dimensions store what they compute, so a save depends on what was asked before it; see
//   AutomaticDimensions.h). Compared: the XML declaration, the root element, the lines between
//   the rocket and the simulations, everything from </simulations> to the end (part S1 of tier
//   10a), and the <rocket> element, line for line (part S2: the component savers, the
//   <motormount> blocks with the motors of the database included). The simulations are
//   compared by the part that writes them (S3).
// - The same design saved again: three more saves of each loaded example. OpenRocket's own
//   saves of one loaded document differ in one example only, the dual parachute deployment, in
//   three lines of its rocket (the probe RepeatSaveProbe of the scouts of tier 10,
//   roundtrip-goldens out/repeat/: a body tube's automatic radius and a parachute's automatic
//   packed radius are stored by the first save's getters, then the packed length by the second's),
//   and the fourth save is the third. QtRocket's saves have to do the same, which holds the order
//   of the getter calls of the savers to OpenRocket's.
// - The three examples with a <preset> again with no presets (Presets::NONE): the six
//   references that cannot be resolved are kept (the user's decision U8 of 2026-10-08) and
//   written back, so the rocket is the re-save's all the same.
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
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/StorageOptions.h"
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

/// The index of the first of @p lines from @p from on that is @p line, or lines.size().
[[nodiscard]] std::size_t indexOf(const std::vector<std::string_view>& lines, std::string_view line,
                                  std::size_t from = 0)
{
    for (std::size_t i = from; i < lines.size(); ++i)
    {
        if (lines[i] == line)
        {
            return i;
        }
    }
    return lines.size();
}

/// The document level of an .ork text, one line per line: the declaration, the root with its
/// creator set aside, the lines from </rocket> to <simulations> and those from </simulations> to
/// the end. A part that is not there is named instead.
[[nodiscard]] std::string documentLevel(std::string_view text)
{
    const std::vector<std::string_view> lines = linesOf(text);
    if (lines.size() < 2)
    {
        return "(no root)";
    }
    std::string       level       = std::string(lines[0]) + "\n" + withoutCreator(lines[1]) + "\n";
    const std::size_t rocketEnd   = indexOf(lines, "  </rocket>");
    const std::size_t simulations = indexOf(lines, "  <simulations>", rocketEnd);
    if (simulations == lines.size())
    {
        return level + "(no </rocket> and <simulations>)";
    }
    for (std::size_t i = rocketEnd; i <= simulations; ++i)
    {
        level += std::string(lines[i]) + "\n";
    }
    const std::size_t simulationsEnd = indexOf(lines, "  </simulations>", simulations);
    if (simulationsEnd == lines.size())
    {
        return level + "(no </simulations>)";
    }
    for (std::size_t i = simulationsEnd; i < lines.size(); ++i)
    {
        level += std::string(lines[i]) + "\n";
    }
    return level;
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

/// Saves @p document without simulation data with @p preferences, as the harness wrote the
/// re-saves; the warnings of the save go to @p warnings. The text, or the Error's text.
[[nodiscard]] std::string saveWithoutData(OpenRocketDocument& document,
                                          const Preferences& preferences, WarningSet& warnings)
{
    StorageOptions options;
    options.setSaveSimulationData(false);
    ErrorSet                  errors;
    const Result<std::string> saved =
        OpenRocketSaver::save(document, options, preferences, warnings, errors);
    if (!saved)
    {
        return "ERROR " + saved.error().toString();
    }
    return *saved;
}

/// What the rocket elements of a set of saves compared with their re-saves gave.
struct RocketsCompared
{
    int         designs{0};     ///< the designs saved and compared
    std::size_t lines{0};       ///< the lines of their rocket elements
    std::size_t byValue{0};     ///< the lines of the same numbers written otherwise
    std::size_t components{0};  ///< the components of the saves (their <id> lines)
    std::size_t presets{0};     ///< the <preset> lines of the saves
    std::size_t motors{0};      ///< the <motor> elements of the saves
    std::string problems;       ///< what differs, by design: empty when nothing does
};

/// Whether @p line is an <id> element, with any indentation.
[[nodiscard]] bool isIdLine(std::string_view line)
{
    const std::size_t start = line.find_first_not_of(' ');
    return start != std::string_view::npos && line.substr(start).starts_with("<id>");
}

/// Compares the rocket element of @p saved, QtRocket's save of the design @p name, with that of
/// @p resave and adds the result to @p compared; a difference is a problem unless it is
/// @p expected (changedLines() from the re-save to the save).
void compareRocketElements(RocketsCompared& compared, std::string_view name, std::string_view saved,
                           std::string_view resave, std::string_view expected = "")
{
    const std::string       ours       = rocketElementOf(saved);
    const std::string       theirs     = rocketElementOf(resave);
    const OrkTextComparison comparison = compareOrkTexts(ours, theirs);
    compared.designs++;
    compared.lines += comparison.lines;
    compared.byValue += comparison.byValue;
    for (const std::string_view line : linesOf(ours))
    {
        compared.components += isIdLine(line) ? 1U : 0U;
        compared.presets += line.contains("<preset ") ? 1U : 0U;
        compared.motors += line.contains("<motor configid=") ? 1U : 0U;
    }
    if (ours.empty() || theirs.empty())
    {
        compared.problems += std::format("{}: no rocket element\n", name);
        return;
    }
    if (expected.empty() ? comparison.differing != 0 : changedLines(theirs, ours) != expected)
    {
        compared.problems += std::format("{}: the rocket differs from the re-save's\n{}", name,
                                         comparison.differences);
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

/// Saves @p document three more times after its first save @p first and checks the rocket
/// elements against kDualParachuteSaves (nothing changes for any other example). Returns the
/// problems.
[[nodiscard]] std::string repeatedSaveProblems(const GoldenExample& example,
                                               OpenRocketDocument&  document,
                                               const std::string&   first)
{
    const Preferences& preferences = goldenExampleEnvironment().preferences();
    const bool         dual        = example.name == kDualParachuteSaves.example;
    std::string        problems;
    std::string        previous = rocketElementOf(first);
    for (int save = 2; save <= 4; ++save)
    {
        WarningSet        warnings;
        const std::string rocket =
            rocketElementOf(saveWithoutData(document, preferences, warnings));
        std::string_view expected;
        if (dual && save == 2)
        {
            expected = kDualParachuteSaves.second;
        }
        else if (dual && save == 3)
        {
            expected = kDualParachuteSaves.third;
        }
        const std::string changes = changedLines(previous, rocket);
        if (changes != expected)
        {
            problems += std::format("{}: save {} changes\n{}where OpenRocket's changes\n{}",
                                    example.name, save, changes, expected);
        }
        previous = rocket;
    }
    return problems;
}

/// Loads @p example fresh, pins its estimate, saves it and compares the document level and the
/// rocket with the re-save, then saves it three times more (repeatedSaveProblems()). Adds the
/// rocket's comparison to @p rockets; returns the other problems.
[[nodiscard]] std::string exampleProblems(const GoldenExample& example, RocketsCompared& rockets)
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
    const std::string ours   = documentLevel(saved);
    const std::string theirs = documentLevel(*resave);
    if (ours != theirs)
    {
        problems +=
            std::format("{}: the document level differs\n--- QtRocket\n{}--- OpenRocket\n{}",
                        example.name, ours, theirs);
    }
    compareRocketElements(rockets, example.name, saved, *resave);
    return problems + repeatedSaveProblems(example, document, saved);
}

/// Saves each of the sixteen examples (exampleProblems()): the problems besides those of the
/// rockets, which go to @p rockets.
[[nodiscard]] std::string saveTheExamples(RocketsCompared& rockets)
{
    std::string problems;
    for (const GoldenExample& example : goldenExamples())
    {
        problems += exampleProblems(example, rockets);
    }
    return problems;
}

// Each of the sixteen examples is saved as OpenRocket re-saved it: the document level (the
// declaration, the root of version 1.11, no custom expressions, the photo studio, empty in
// fourteen and 24 settings in two, no document preference, and the document materials of each)
// and the rocket, 7200 lines with 327 components, 65 motors and the six presets; OpenRocket's
// estimate of each file's size; and the saves of one loaded document that follow the first
// change as OpenRocket's do. Not one number is written otherwise than OpenRocket writes it.
TEST(OrkResaveDiffGolden, TheExamplesAreSavedAsOpenRocketResavedThem)
{
    RocketsCompared rockets;
    EXPECT_EQ(saveTheExamples(rockets), "");
    EXPECT_EQ(rockets.problems, "");
    EXPECT_EQ(rockets.designs, 16);
    EXPECT_EQ(rockets.lines, 7200U);
    EXPECT_EQ(rockets.components, 327U);
    EXPECT_EQ(rockets.byValue, 0U);
    EXPECT_EQ(rockets.presets, 6U);
    EXPECT_EQ(rockets.motors, 65U);
    EXPECT_EQ(goldenExamples().size(), kEstimatedSizes.size());
}

/// Saves the examples whose re-save holds a <preset>, loaded without presets, into @p rockets;
/// counts the others in @p withoutPresets. Returns what could not be loaded or read.
[[nodiscard]] std::string saveTheExamplesWithoutPresets(RocketsCompared& rockets,
                                                        int&             withoutPresets)
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
        compareRocketElements(rockets, example.name, saved, *resave);
    }
    return problems;
}

// The three examples with a <preset> loaded without presets (Presets::NONE: six "No matching
// ComponentPreset" warnings) keep the six references (decision U8) and write them back: the
// rocket is the re-save's, line for line. The thirteen other re-saves hold no <preset>.
TEST(OrkResaveDiffGolden, TheExamplesWithoutPresetsWriteTheirReferencesBack)
{
    RocketsCompared rockets;
    int             withoutPresets = 0;
    EXPECT_EQ(saveTheExamplesWithoutPresets(rockets, withoutPresets), "");
    EXPECT_EQ(rockets.problems, "");
    EXPECT_EQ(rockets.designs, 3);
    EXPECT_EQ(rockets.lines, 1118U);
    EXPECT_EQ(withoutPresets, 13);
    EXPECT_EQ(rockets.presets, 6U);
    EXPECT_EQ(rockets.byValue, 0U);
}

/// The one line in which QtRocket's save of the loaded re-save of the Alpha III with pods
/// differs from the re-save (changedLines() from the re-save to the save): the wall of the pod
/// body.
constexpr std::string_view kPodBodyWall =
    "<thickness>0.002</thickness> -> <thickness>0.0</thickness>\n";

/// Loads the re-save of each test rocket and saves it again into @p rockets. Returns what could
/// not be loaded or read, and the saves that warned.
[[nodiscard]] std::string saveTheTestRockets(RocketsCompared& rockets)
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
        compareRocketElements(rockets, input.name, saved, *file,
                              input.name == kPods ? kPodBodyWall : "");
    }
    return problems;
}

// The thirteen test rockets, loaded from their re-saves against the seven motors of TestRockets
// and saved again, are their re-saves but for the wall of the pod body of the Alpha III with pods
// (OpenRocket's own reload has no wall there either: ResaveGoldenExceptions). 3021 lines with
// 151 components and 48 motors.
TEST(OrkResaveDiffGolden, TheTestRocketsAreSavedAsOpenRocketResavedThem)
{
    RocketsCompared rockets;
    EXPECT_EQ(saveTheTestRockets(rockets), "");
    EXPECT_EQ(rockets.problems, "");
    EXPECT_EQ(rockets.designs, 13);
    EXPECT_EQ(rockets.lines, 3021U);
    EXPECT_EQ(rockets.components, 151U);
    EXPECT_EQ(rockets.byValue, 0U);
    EXPECT_EQ(rockets.presets, 0U);
    EXPECT_EQ(rockets.motors, 48U);
}

// The comparison finds a difference in each part of the document level it compares.
TEST(OrkResaveDiffGolden, TheDocumentLevelHoldsEachPartItCompares)
{
    constexpr std::string_view kText =
        "<?xml version='1.0' encoding='utf-8'?>\n"
        "<openrocket version=\"1.11\" creator=\"OpenRocket 26\">\n"
        "  <rocket>\n"
        "    <name>R</name>\n"
        "  </rocket>\n"
        "\n"
        "  <simulations>\n"
        "    <simulation/>\n"
        "  </simulations>\n"
        "  <photostudio>\n"
        "  </photostudio>\n"
        "</openrocket>\n";
    EXPECT_EQ(documentLevel(kText),
              "<?xml version='1.0' encoding='utf-8'?>\n"
              "<openrocket version=\"1.11\" creator=\"CREATOR\">\n"
              "  </rocket>\n"
              "\n"
              "  <simulations>\n"
              "  </simulations>\n"
              "  <photostudio>\n"
              "  </photostudio>\n"
              "</openrocket>\n");
    EXPECT_EQ(documentLevel(std::string(kText).replace(kText.find("26"), 2, "27")),
              documentLevel(kText));
    EXPECT_NE(documentLevel(std::string(kText).replace(kText.find("1.11"), 4, "1.10")),
              documentLevel(kText));
    EXPECT_NE(documentLevel(std::string(kText).replace(kText.find("\n\n"), 2, "\n")),
              documentLevel(kText));
    EXPECT_NE(documentLevel(std::string(kText).replace(kText.find("  </photo"), 2, " ")),
              documentLevel(kText));
    EXPECT_EQ(documentLevel("<?xml?>\n<openrocket>\n"),
              "<?xml?>\n<openrocket>\n(no </rocket> and <simulations>)");
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
