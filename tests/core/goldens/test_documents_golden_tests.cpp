#include <cstddef>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/StorageOptions.h"
#include "QtRocket/file/openrocket/OpenRocketSaver.h"
#include "QtRocket/logging/ErrorSet.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestPaths.h"
#include "document/TestDocuments.h"
#include "goldens/GoldenResaves.h"
#include "simulation/SimulationRunSupport.h"

// The document makers of TestRockets (document/TestDocuments.h) against OpenRocket's: QtRocket's
// save of each maker's document is OpenRocket's save of the same maker
// (tests/data/goldens/test-documents/<maker>.ork, written by OpenRocketSaver with the simulation
// data, in the set-up of OpenRocketSaverTest but for the translator, which is
// ServicesForTesting's English one: tools/openrocket-test-documents/generate.sh), line for line,
// after three normalisations of both texts: the creator attribute (QtRocket writes its own), the
// random ids (each version-4 UUID becomes "#<n>", numbered in the order of first appearance, so
// that the references between ids are still compared) and the computation times of the flight
// data (the wall-clock time of each step). A line that differs only in numbers of the same value
// counts as a difference too: the sixteen files hold no number that JDK 17 writes longer than
// QtRocket does (decision U10). No decided deviation shows in these documents: v110's script is
// written as OpenRocket writes it (decision D1 acts on the load, which
// OpenRocketSaver.TestCreateLoadSave covers), and they hold no mass component (E1) and no text
// that E3 changes.
//
// And the normalisation itself. What the saves cannot show (the motors, the parts of the
// documents that are no file's) is document/test_documents_tests.cpp's.

namespace
{

using QtRocket::OpenRocketDocument;
using QtRocket::OpenRocketSaver;
using QtRocket::Result;
using QtRocket::StorageOptions;
using QtRocket::Test::JavaTestPreferences;
using QtRocket::Test::TestDocumentMaker;

/// Whether @p c is a lowercase hexadecimal digit.
[[nodiscard]] bool isHexDigit(char c) noexcept
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

/// Whether the 36 characters of @p text at @p at are a random (version 4, variant 1) UUID as
/// Java and QtRocket write one: "xxxxxxxx-xxxx-4xxx-[89ab]xxx-xxxxxxxxxxxx".
[[nodiscard]] bool isRandomUuidAt(std::string_view text, std::size_t at) noexcept
{
    constexpr std::size_t kLength = 36;
    if (text.size() - at < kLength)
    {
        return false;
    }
    for (std::size_t i = 0; i < kLength; ++i)
    {
        const char c       = text[at + i];
        const bool hyphen  = i == 8 || i == 13 || i == 18 || i == 23;
        const bool matches = hyphen ? c == '-' : isHexDigit(c);
        if (!matches)
        {
            return false;
        }
    }
    if (text.size() - at > kLength && isHexDigit(text[at + kLength]))
    {
        return false;
    }
    const char variant = text[at + 19];
    return text[at + 14] == '4' &&
           (variant == '8' || variant == '9' || variant == 'a' || variant == 'b');
}

/// @p text with each random UUID replaced by "#<n>", n counting the distinct ones from 1 in the
/// order of their first appearance.
[[nodiscard]] std::string withNumberedIds(std::string_view text)
{
    std::vector<std::string_view> seen;
    std::string                   result;
    result.reserve(text.size());
    std::size_t at = 0;
    while (at < text.size())
    {
        const bool boundary = at == 0 || !isHexDigit(text[at - 1]);
        if (boundary && isRandomUuidAt(text, at))
        {
            const std::string_view uuid = text.substr(at, 36);
            std::size_t            n    = 0;
            while (n < seen.size() && seen[n] != uuid)
            {
                ++n;
            }
            if (n == seen.size())
            {
                seen.push_back(uuid);
            }
            result += std::format("#{}", n + 1);
            at += uuid.size();
            continue;
        }
        result += text[at];
        ++at;
    }
    return result;
}

/// The index of the column "computation_time" in the types="..." attribute of the <databranch>
/// line @p line, or npos when it has none.
[[nodiscard]] std::size_t computationTimeColumn(std::string_view line)
{
    constexpr std::string_view kTypes = "types=\"";
    const std::size_t          start  = line.find(kTypes);
    if (start == std::string_view::npos)
    {
        return std::string_view::npos;
    }
    const std::size_t      end   = line.find('"', start + kTypes.size());
    const std::string_view types = line.substr(start + kTypes.size(), end - start - kTypes.size());
    std::size_t            index = 0;
    std::size_t            from  = 0;
    while (true)
    {
        const std::size_t comma = types.find(',', from);
        if (types.substr(from, comma - from) == "computation_time")
        {
            return index;
        }
        if (comma == std::string_view::npos)
        {
            return std::string_view::npos;
        }
        from = comma + 1;
        ++index;
    }
}

/// The <datapoint> line @p line with its field @p column replaced by "T"; the line as it is when
/// it has no such field.
[[nodiscard]] std::string withoutField(std::string_view line, std::size_t column)
{
    constexpr std::string_view kOpen = "<datapoint>";
    std::size_t                start = line.find(kOpen) + kOpen.size();
    for (std::size_t i = 0; i < column; ++i)
    {
        const std::size_t comma = line.find(',', start);
        if (comma == std::string_view::npos)
        {
            return std::string(line);
        }
        start = comma + 1;
    }
    const std::size_t comma = line.find(',', start);
    const std::size_t close = line.find('<', start);
    const std::size_t end   = comma < close ? comma : close;
    std::string       result(line.substr(0, start));
    result += 'T';
    result += line.substr(end);
    return result;
}

/// @p text with the computation time of every data point of a flight data branch replaced by
/// "T" (the column "computation_time" of the branch, wherever its types put it).
[[nodiscard]] std::string withoutComputationTimes(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    std::size_t column = std::string_view::npos;
    for (const std::string_view line : QtRocket::Test::lineViewsOf(text))
    {
        if (line.contains("<databranch "))
        {
            column = computationTimeColumn(line);
        }
        else if (line.contains("</databranch>"))
        {
            column = std::string_view::npos;
        }
        if (column != std::string_view::npos && line.contains("<datapoint>"))
        {
            result += withoutField(line, column);
        }
        else
        {
            result += line;
        }
        result += '\n';
    }
    // A text without a final line feed stays without one (compareOrkTexts() reports that).
    if (!text.ends_with('\n') && !result.empty())
    {
        result.pop_back();
    }
    return result;
}

/// Both normalisations but the creator's, which compareOrkTexts() makes.
[[nodiscard]] std::string normalised(std::string_view text)
{
    return withoutComputationTimes(withNumberedIds(text));
}

/// QtRocket's save of @p document with the simulation data, as OpenRocketSaverTest asks, with
/// @p preferences; "ERROR <error>" when the save fails, and a failure for any save warning.
[[nodiscard]] std::string savedText(OpenRocketDocument&                  document,
                                    const QtRocket::InMemoryPreferences& preferences)
{
    StorageOptions options;
    options.setSaveSimulationData(true);
    QtRocket::WarningSet      warnings;
    QtRocket::ErrorSet        errors;
    const Result<std::string> text =
        OpenRocketSaver::save(document, options, preferences, warnings, errors);
    EXPECT_TRUE(warnings.empty());
    EXPECT_TRUE(errors.empty());
    return text.has_value() ? *text : "ERROR " + text.error().toString();
}

/// What differs between QtRocket's save of @p maker's document and OpenRocket's, after the
/// normalisations; empty when nothing does.
[[nodiscard]] std::string differencesOf(const TestDocumentMaker& maker)
{
    const Result<std::string> theirs =
        QtRocket::readTextFile(QtRocket::Test::testDataDir() / "goldens" / "test-documents" /
                               (std::string(maker.name) + ".ork"));
    if (!theirs.has_value())
    {
        return theirs.error().toString();
    }
    JavaTestPreferences                       preferences;
    const std::unique_ptr<OpenRocketDocument> document = maker.make(preferences.store);
    const std::string                         ours     = savedText(*document, preferences.store);
    const QtRocket::Test::OrkTextComparison   comparison =
        QtRocket::Test::compareOrkTexts(normalised(ours), normalised(*theirs));
    if (comparison.differing == 0 && comparison.byValue == 0)
    {
        return {};
    }
    return std::format("{} of {} lines differ ({} by value only):\n{}", comparison.differing,
                       comparison.lines, comparison.byValue, comparison.differences);
}

}  // namespace

// Every maker's document is saved as OpenRocket saves the document of the same maker.
TEST(TestDocumentsGolden, EveryDocumentIsSavedAsOpenRocketSavesIt)
{
    std::size_t compared = 0;
    for (const TestDocumentMaker& maker : QtRocket::Test::testDocumentMakers())
    {
        EXPECT_EQ(differencesOf(maker), "") << maker.name;
        ++compared;
    }
    EXPECT_EQ(compared, 16U);
}

// The normalisation numbers the random ids by their first appearance and keeps every other id:
// the default configuration's, one made from a text ("testParachute"), a random id inside a
// longer word and an id of another variant.
TEST(TestDocumentsGolden, TheNormalisationNumbersTheRandomIdsOnly)
{
    EXPECT_EQ(withoutComputationTimes("a\n\nb"), "a\n\nb");
    EXPECT_EQ(withNumberedIds("<a id=\"d010716e-ce0e-469d-ae46-190f3653ebbf\">"
                              "8c9aafcb-9a2f-4cea-ac5d-7c602303d388 "
                              "d010716e-ce0e-469d-ae46-190f3653ebbf</a>"),
              "<a id=\"#1\">#2 #1</a>");
    EXPECT_EQ(withNumberedIds("ffffffff-f4f2-f1f0-0000-00000000162c "
                              "00000000-0000-0000-ffff-ffff9bde0acf "
                              "a8c9aafcb-9a2f-4cea-ac5d-7c602303d388 "
                              "8c9aafcb-9a2f-4cea-ac5d-7c602303d388f "
                              "8c9aafcb-9a2f-4cea-cc5d-7c602303d388"),
              "ffffffff-f4f2-f1f0-0000-00000000162c 00000000-0000-0000-ffff-ffff9bde0acf "
              "a8c9aafcb-9a2f-4cea-ac5d-7c602303d388 8c9aafcb-9a2f-4cea-ac5d-7c602303d388f "
              "8c9aafcb-9a2f-4cea-cc5d-7c602303d388");
    EXPECT_EQ(
        withoutComputationTimes("<databranch name=\"S\" types=\"time,computation_time,altitude\">\n"
                                "<datapoint>0,0.0123,1</datapoint>\n"
                                "<datapoint>1,0.5,2</datapoint>\n"
                                "</databranch>\n"
                                "<datapoint>2,0.7,3</datapoint>\n"),
        "<databranch name=\"S\" types=\"time,computation_time,altitude\">\n"
        "<datapoint>0,T,1</datapoint>\n"
        "<datapoint>1,T,2</datapoint>\n"
        "</databranch>\n"
        "<datapoint>2,0.7,3</datapoint>\n");
}
