// QtRocket's .ork save of OpenRocket's examples against OpenRocket's own re-save of them
// (tests/data/goldens/example-*/resave/rocket.ork, written by the golden harness with
// OpenRocketSaver and saveSimulationData=false, on the design as loaded, before anything was
// settled). Decision O4 of tier 10: each example is loaded fresh from the ORIGINAL archive under
// goldenExampleEnvironment() with the six presets and saved without simulation data with the
// environment's own preference store; the creator attribute of the root is the only text that
// may differ.
//
// What is compared so far (part S1 of tier 10a, the document level of the saver): the XML
// declaration, the root element, the lines between the rocket and the simulations (the blank
// line and the custom expressions), and everything from </simulations> to the end: the photo
// studio, the document preferences and the document materials. The rocket (part S2) and the
// simulations (part S3) are compared by the parts that write them.
//
// The same loop pins OpenRocket's estimateFileSize() of each loaded example (one load per
// example serves both).

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
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "file/DesignFileEnvironment.h"
#include "goldens/GoldenExamples.h"

namespace
{

using QtRocket::ErrorSet;
using QtRocket::LoadedDocument;
using QtRocket::OpenRocketSaver;
using QtRocket::Result;
using QtRocket::StorageOptions;
using QtRocket::WarningSet;
using QtRocket::Test::ExampleSource;
using QtRocket::Test::GoldenExample;
using QtRocket::Test::goldenExampleEnvironment;
using QtRocket::Test::goldenExamples;
using QtRocket::Test::loadGoldenExample;

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

/// @p line with the value of its creator attribute replaced by "CREATOR".
[[nodiscard]] std::string withoutCreator(std::string_view line)
{
    constexpr std::string_view kAttribute = "creator=\"";
    std::string                result(line);
    const std::size_t          start = result.find(kAttribute);
    if (start == std::string::npos)
    {
        return result;
    }
    const std::size_t value = start + kAttribute.size();
    return result.replace(value, result.find('"', value) - value, "CREATOR");
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

/// The document level of an .ork text (see the top of this file), one line per line: the
/// declaration, the root with its creator set aside, the lines from </rocket> to <simulations>
/// and those from </simulations> to the end. A part that is not there is named instead.
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

/// What QtRocket's save of @p example differs in from OpenRocket's re-save, at the document
/// level, with the estimate of its size: an empty text when nothing does.
[[nodiscard]] std::string documentLevelProblems(const GoldenExample& example)
{
    const Result<LoadedDocument> loaded = loadGoldenExample(example);
    if (!loaded)
    {
        return std::format("{}: {}\n", example.name, loaded.error().toString());
    }
    std::string                   problems;
    QtRocket::OpenRocketDocument& document = *loaded->document;

    const std::int64_t estimate =
        OpenRocketSaver::estimateFileSize(document, document.getDefaultStorageOptions());
    if (estimate != estimatedSizeOf(example.file))
    {
        problems += std::format("{}: estimateFileSize {} where OpenRocket's is {}\n", example.name,
                                estimate, estimatedSizeOf(example.file));
    }

    StorageOptions options;
    options.setSaveSimulationData(false);
    WarningSet                warnings;
    ErrorSet                  errors;
    const Result<std::string> saved = OpenRocketSaver::save(
        document, options, goldenExampleEnvironment().preferences(), warnings, errors);
    const Result<std::string> resave = QtRocket::readTextFile(example.resave);
    if (!saved || !resave)
    {
        return problems + std::format("{}: no text to compare\n", example.name);
    }
    if (!warnings.empty())
    {
        problems += std::format("{}: {} save warnings\n", example.name, warnings.size());
    }
    const std::string ours   = documentLevel(*saved);
    const std::string theirs = documentLevel(*resave);
    if (ours != theirs)
    {
        problems +=
            std::format("{}: the document level differs\n--- QtRocket\n{}--- OpenRocket\n{}",
                        example.name, ours, theirs);
    }
    return problems;
}

// The document level of each of the sixteen examples is that of OpenRocket's re-save: the
// declaration, the root (version 1.11), no custom expressions, the photo studio (empty in
// fourteen, 24 settings in two), no document preference, and the document materials of each.
// And OpenRocket's estimate of each file's size.
TEST(OrkResaveDiffGolden, TheDocumentLevelOfTheExamplesIsThatOfTheResaves)
{
    std::string problems;
    for (const GoldenExample& example : goldenExamples())
    {
        problems += documentLevelProblems(example);
    }
    EXPECT_EQ(problems, "");
    EXPECT_EQ(goldenExamples().size(), kEstimatedSizes.size());
}

// The comparison finds a difference in each part it compares.
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

}  // namespace
