// Whole design files that are damaged: recorded designs of tests/data/ork cut short, with
// single bytes replaced, and with elements of their documents taken out or written twice, each
// loaded through GeneralRocketLoader. What such a file gives is not pinned here (the tests of
// the loader and of its handlers pin outcomes): the one assertion is the failure policy of the
// loader, that everything a file can hold gives a document or an Error and never an exception,
// a BugError included (decisions D9 and T6), under the sanitizers no invalid memory access and
// no leak either.
//
// The sweep of the test suite is small, so that the suite stays quick under the sanitizers:
// three of the smaller designs, one per container, and the elements of one document. The wide
// sweep, every byte of every recorded design and every element of every document, is a
// DISABLED_ test in shards that is run by hand (see LoaderHostileInputWideSweep below).

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <format>
#include <iostream>
#include <ostream>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/motor/MotorDatabase.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestPaths.h"
#include "file/DesignFileEnvironment.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "file/openrocket/RecordedOrkFiles.h"
#include "motor/TestMotorDatabase.h"

namespace
{

using QtRocket::GeneralRocketLoader;
using QtRocket::LoadedDocument;
using QtRocket::Result;
using QtRocket::ThrustCurveMotorSetDatabase;
using QtRocket::Test::DesignFileEnvironment;

using Bytes = std::vector<std::byte>;

/// The designs of the sweep of the test suite, the smallest of each container: plain XML
/// (30942 bytes), gzip (2074 bytes) and zip (1628 bytes).
constexpr std::string_view kPlainFile = "v1.6-apocd.ork";
constexpr std::string_view kGzipFile  = "v1.5-preset-usage.ork";
constexpr std::string_view kZipFile   = "v1.8-logo-rocket.ork";

/// The seed of the generator that draws the places and the values of the replaced bytes.
constexpr std::uint32_t kSeed = 20261008;

[[nodiscard]] std::filesystem::path recordedPath(std::string_view file)
{
    return QtRocket::Test::testDataDir() / "ork" / file;
}

/// The bytes of the recorded design @p file; none, with a test failure, when it cannot be read.
[[nodiscard]] Bytes recordedBytes(std::string_view file)
{
    Result<Bytes> bytes = QtRocket::readFile(recordedPath(file));
    if (!bytes)
    {
        ADD_FAILURE() << file << ": " << bytes.error().toString();
        return {};
    }
    return std::move(*bytes);
}

/// What a sweep met.
struct Outcomes
{
    /// How many inputs were loaded.
    std::size_t inputs{0};
    /// How many of them gave a document, and how many an Error.
    std::size_t documents{0};
    std::size_t errors{0};
    /// How many made the loader throw, and the first of them, one line each: the input and what
    /// was thrown.
    std::size_t thrown{0};
    std::string firstThrown;

    void add(const Outcomes& other)
    {
        inputs += other.inputs;
        documents += other.documents;
        errors += other.errors;
        thrown += other.thrown;
        if (firstThrown.size() < kMostReported)
        {
            firstThrown += other.firstThrown;
        }
    }

    /// "<n> inputs: <n> documents, <n> errors, <n> thrown".
    [[nodiscard]] std::string summary() const
    {
        return std::format("{} inputs: {} documents, {} errors, {} thrown", inputs, documents,
                           errors, thrown);
    }

    /// No more of firstThrown than this is collected.
    static constexpr std::size_t kMostReported = 4000;
};

/// The environment of the sweeps and a loader made with it: the six example presets, the
/// bundled extensions, and a motor database that holds nothing unless a sweep hands one in (a
/// test of the suite is a process of its own under ctest, and reading the bundled database
/// would take longer than the sweep).
class SweepLoader
{
public:
    SweepLoader() : m_environment(DesignFileEnvironment::Presets::EXAMPLES, m_noMotors) { }
    explicit SweepLoader(const QtRocket::MotorDatabase& motors)
      : m_environment(DesignFileEnvironment::Presets::EXAMPLES, motors)
    {
    }

    /// Loads @p bytes, the contents of a design file, and counts into @p outcomes what came of
    /// it; @p what names the input in the report of an exception. The document is dropped.
    void load(Bytes bytes, const std::string& what, Outcomes& outcomes) const
    {
        outcomes.inputs++;
        std::string thrown;
        try
        {
            const Result<LoadedDocument> loaded = m_loader.load(std::move(bytes));
            (loaded ? outcomes.documents : outcomes.errors)++;
            return;
        }
        catch (const QtRocket::BugError& bug)
        {
            thrown = std::format("BugError: {}", bug.what());
        }
        catch (const std::exception& exception)
        {
            thrown = std::format("std::exception: {}", exception.what());
        }
        outcomes.thrown++;
        if (outcomes.firstThrown.size() < Outcomes::kMostReported)
        {
            outcomes.firstThrown += std::format("{}: {}\n", what, thrown);
        }
    }

private:
    ThrustCurveMotorSetDatabase m_noMotors;
    DesignFileEnvironment       m_environment;
    GeneralRocketLoader         m_loader{m_environment.context()};
};

/// Whether @p index is one of the indices the shard @p shard of @p shards takes.
[[nodiscard]] constexpr bool inShard(std::size_t index, std::size_t shard,
                                     std::size_t shards) noexcept
{
    return index % shards == shard;
}

/// @p file cut off at every @p stride-th byte, from the empty file up: the first 0, stride,
/// 2 * stride, ... bytes, of which the shard @p shard of @p shards is loaded.
[[nodiscard]] Outcomes sweepTruncations(const SweepLoader& loader, std::string_view name,
                                        const Bytes& file, std::size_t stride,
                                        std::size_t shard = 0, std::size_t shards = 1)
{
    Outcomes outcomes;
    for (std::size_t length = 0; length < file.size(); length += stride)
    {
        if (inShard(length / stride, shard, shards))
        {
            loader.load(Bytes(file.begin(), file.begin() + static_cast<std::ptrdiff_t>(length)),
                        std::format("{} cut off at byte {}", name, length), outcomes);
        }
    }
    return outcomes;
}

/// The byte that replaces @p original when @p drawn was drawn for it: the low eight bits of
/// @p drawn, or their complement when those are the original's (a replacement changes the
/// byte).
[[nodiscard]] constexpr std::byte replacement(std::byte original, std::uint32_t drawn) noexcept
{
    const auto value = static_cast<std::byte>(drawn & 0xFFU);
    return value == original ? ~value : value;
}

/// @p file with one byte replaced, @p count times: the place and the value of each are raw
/// 32-bit outputs of a std::mt19937 seeded with kSeed (the place modulo the size of the file;
/// no distribution, whose results differ between standard libraries).
[[nodiscard]] Outcomes sweepRandomReplacements(const SweepLoader& loader, std::string_view name,
                                               const Bytes& file, std::size_t count)
{
    Outcomes outcomes;
    // NOLINTNEXTLINE(bugprone-random-generator-seed): the same sweep in every run is the point
    std::mt19937 random(kSeed);
    for (std::size_t i = 0; i < count && !file.empty(); i++)
    {
        const std::size_t place   = random() % file.size();
        const auto        drawn   = static_cast<std::uint32_t>(random());
        Bytes             damaged = file;
        damaged[place]            = replacement(file[place], drawn);
        loader.load(std::move(damaged),
                    std::format("{} with byte {} replaced by {:#04x}", name, place,
                                std::to_integer<unsigned>(replacement(file[place], drawn))),
                    outcomes);
    }
    return outcomes;
}

/// @p file with the byte at every @p stride-th place replaced in turn, by a value drawn as in
/// sweepRandomReplacements() (one draw per place of the file, in the order of the places, so
/// that a place gets the same value whatever the stride and the shard); of the places taken,
/// the shard @p shard of @p shards is loaded.
[[nodiscard]] Outcomes sweepReplacements(const SweepLoader& loader, std::string_view name,
                                         const Bytes& file, std::size_t stride, std::size_t shard,
                                         std::size_t shards)
{
    Outcomes outcomes;
    // NOLINTNEXTLINE(bugprone-random-generator-seed): the same sweep in every run is the point
    std::mt19937 random(kSeed);
    for (std::size_t place = 0; place < file.size(); place++)
    {
        const auto drawn = static_cast<std::uint32_t>(random());
        if (place % stride == 0 && inShard(place / stride, shard, shards))
        {
            const std::byte value   = replacement(file[place], drawn);
            Bytes           damaged = file;
            damaged[place]          = value;
            loader.load(std::move(damaged),
                        std::format("{} with byte {} replaced by {:#04x}", name, place,
                                    std::to_integer<unsigned>(value)),
                        outcomes);
        }
    }
    return outcomes;
}

/// An element of a document: from the '<' of its start tag to behind the '>' of its end tag.
struct ElementSpan
{
    std::size_t begin;
    std::size_t end;
};

/// The place behind the '>' that ends the tag starting at @p begin of @p text (a '>' inside a
/// quoted attribute value does not end it); the end of the text for a tag that never ends.
[[nodiscard]] std::size_t endOfTag(std::string_view text, std::size_t begin) noexcept
{
    char quote = '\0';
    for (std::size_t i = begin; i < text.size(); i++)
    {
        const char c = text[i];
        if (quote != '\0')
        {
            quote = c == quote ? '\0' : quote;
        }
        else if (c == '"' || c == '\'')
        {
            quote = c;
        }
        else if (c == '>')
        {
            return i + 1;
        }
    }
    return text.size();
}

/// The place behind the first @p terminator at or after @p from in @p text, or the end of the
/// text.
[[nodiscard]] std::size_t behind(std::string_view text, std::size_t from,
                                 std::string_view terminator) noexcept
{
    const std::size_t at = text.find(terminator, from);
    return at == std::string_view::npos ? text.size() : at + terminator.size();
}

/// The elements of the XML document @p text below its root element, in the order of their
/// start tags. The documents of the recorded designs are what OpenRocket's saver wrote: a
/// declaration, comments, elements with attributes and text, no CDATA section and no document
/// type; a text that is no such document gives the spans this simple reading finds.
[[nodiscard]] std::vector<ElementSpan> elementsOf(std::string_view text)
{
    std::vector<ElementSpan> elements;
    std::vector<std::size_t> open;
    std::size_t              at = text.find('<');
    while (at != std::string_view::npos && at + 1 < text.size())
    {
        std::size_t next = 0;
        if (text.substr(at).starts_with("<!--"))
        {
            next = behind(text, at, "-->");
        }
        else if (text[at + 1] == '?' || text[at + 1] == '!')
        {
            next = behind(text, at, ">");
        }
        else if (text[at + 1] == '/')
        {
            next = endOfTag(text, at);
            if (open.size() > 1)
            {
                elements.push_back({.begin = open.back(), .end = next});
            }
            if (!open.empty())
            {
                open.pop_back();
            }
        }
        else
        {
            next = endOfTag(text, at);
            if (next >= 2 && text[next - 2] == '/')
            {
                if (!open.empty())
                {
                    elements.push_back({.begin = at, .end = next});
                }
            }
            else
            {
                open.push_back(at);
            }
        }
        at = text.find('<', next);
    }
    std::ranges::sort(elements, {}, &ElementSpan::begin);
    return elements;
}

/// The document @p text with each of its elements below the root in turn taken out, and in
/// turn written twice (the copy right behind the element), each loaded as a plain XML design;
/// of the elements the shard @p shard of @p shards is taken.
[[nodiscard]] Outcomes sweepElements(const SweepLoader& loader, std::string_view name,
                                     std::string_view text, std::size_t shard = 0,
                                     std::size_t shards = 1)
{
    Outcomes                       outcomes;
    const std::vector<ElementSpan> elements = elementsOf(text);
    for (std::size_t i = 0; i < elements.size(); i++)
    {
        if (!inShard(i, shard, shards))
        {
            continue;
        }
        const std::string_view before = text.substr(0, elements[i].begin);
        const std::string_view element =
            text.substr(elements[i].begin, elements[i].end - elements[i].begin);
        const std::string_view after = text.substr(elements[i].end);
        loader.load(
            QtRocket::stringToBytes(std::format("{}{}", before, after)),
            std::format("{} without its element {} (at byte {})", name, i, elements[i].begin),
            outcomes);
        loader.load(
            QtRocket::stringToBytes(std::format("{}{}{}{}", before, element, element, after)),
            std::format("{} with its element {} (at byte {}) twice", name, i, elements[i].begin),
            outcomes);
    }
    return outcomes;
}

/// The XML document of the recorded design @p file.
[[nodiscard]] std::string recordedDocument(std::string_view file)
{
    return QtRocket::Test::documentOfDesignFile(recordedPath(file));
}

// The reading of elements the sweep rests on, on a document with everything the recorded
// documents hold: the root is no element of the list, a nested element comes after the one it
// is in, an empty-element tag is an element, and a '>' in an attribute value ends no tag.
TEST(LoaderHostileInput, TheSweepFindsTheElementsOfADocument)
{
    const std::string_view text =
        "<?xml version='1.0'?>\n<!-- a > comment -->\n"
        "<openrocket version=\"1.9\"><rocket><name a=\"x>y\">R</name><stage/></rocket>\n"
        "<simulations/></openrocket>\n";
    std::vector<std::string> found;
    for (const ElementSpan& element : elementsOf(text))
    {
        found.emplace_back(text.substr(element.begin, element.end - element.begin));
    }
    EXPECT_EQ(found, (std::vector<std::string>{
                         "<rocket><name a=\"x>y\">R</name><stage/></rocket>",
                         "<name a=\"x>y\">R</name>",
                         "<stage/>",
                         "<simulations/>",
                     }));
    EXPECT_EQ(elementsOf(recordedDocument(kGzipFile)).size(), 136U);
}

// A gzip stream and an archive, cut off at every 97th byte and with 60 single bytes replaced at
// places and by values drawn from a generator with a fixed seed: every outcome is a document
// or an Error. Most damage of this kind is met by the container (a check sum, a length, a
// stream that ends too early, which never gives a document when it ends in its data).
TEST(LoaderHostileInput, NoDamagedContainerMakesTheLoaderThrow)
{
    const SweepLoader loader;
    Outcomes          outcomes;
    for (const std::string_view file : {kGzipFile, kZipFile})
    {
        const Bytes bytes = recordedBytes(file);
        outcomes.add(sweepTruncations(loader, file, bytes, 97));
        outcomes.add(sweepRandomReplacements(loader, file, bytes, 60));
    }
    EXPECT_EQ(outcomes.firstThrown, "") << outcomes.summary();
    EXPECT_EQ(outcomes.thrown, 0U);
    EXPECT_EQ(outcomes.inputs, 22U + 60U + 17U + 60U);
    // Both kinds of outcome occur: a sweep that only ever failed, or never, would show little.
    EXPECT_GT(outcomes.documents, 0U) << outcomes.summary();
    EXPECT_GT(outcomes.errors, 0U) << outcomes.summary();
}

// The plain XML design, whose every damaged byte reaches the XML reader and the handlers
// behind it. It is fifteen times as large as the other two and a load of it takes fifty times
// as long (the handlers have read everything before the damage), so the suite loads every
// sixteenth of its 319 cuts at every 97th byte and replaces 8 bytes; the wide sweep does the
// rest.
TEST(LoaderHostileInput, NoDamagedPlainDocumentMakesTheLoaderThrow)
{
    const SweepLoader loader;
    const Bytes       bytes    = recordedBytes(kPlainFile);
    Outcomes          outcomes = sweepTruncations(loader, kPlainFile, bytes, 97, 0, 16);
    outcomes.add(sweepRandomReplacements(loader, kPlainFile, bytes, 8));
    EXPECT_EQ(outcomes.firstThrown, "") << outcomes.summary();
    EXPECT_EQ(outcomes.thrown, 0U);
    EXPECT_EQ(outcomes.inputs, 20U + 8U);
    EXPECT_GT(outcomes.documents, 0U) << outcomes.summary();
    EXPECT_GT(outcomes.errors, 0U) << outcomes.summary();
}

class LoaderHostileInputElements : public ::testing::TestWithParam<std::size_t>
{ };

// The document of a design with each of its 136 elements in turn taken out and in turn written
// twice: a document that is well-formed and has an element too few or too many somewhere (a
// second <name>, two <motormount>, a simulation without its <conditions>, a rocket without
// its <subcomponents>, a <stage> twice). Every outcome is a document or an Error. The 272
// loads are four tests of 68, a quarter of the elements each, so that under the sanitizers each
// takes seconds and ctest runs them side by side.
TEST_P(LoaderHostileInputElements, NoMissingOrRepeatedElementMakesTheLoaderThrow)
{
    const SweepLoader loader;
    const Outcomes    outcomes =
        sweepElements(loader, kGzipFile, recordedDocument(kGzipFile), GetParam(), 4);
    EXPECT_EQ(outcomes.firstThrown, "") << outcomes.summary();
    EXPECT_EQ(outcomes.thrown, 0U);
    EXPECT_EQ(outcomes.inputs, 68U);
    EXPECT_GT(outcomes.documents, 0U) << outcomes.summary();
}

INSTANTIATE_TEST_SUITE_P(Quarters, LoaderHostileInputElements,
                         ::testing::Range(std::size_t{0}, std::size_t{4}));

// Of those 272 documents two are refused, with OpenRocket's Error for a simulation that has no
// <conditions> and for one whose <conditions> lack the <configid> (decision L8): the two
// elements a design cannot do without.
TEST(LoaderHostileInput, OnlyASimulationWithoutItsConfigurationIsRefused)
{
    const SweepLoader              loader;
    const std::string              text     = recordedDocument(kGzipFile);
    const std::vector<ElementSpan> elements = elementsOf(text);
    std::vector<std::string>       refused;
    for (const ElementSpan& element : elements)
    {
        const std::string_view name = std::string_view(text).substr(
            element.begin + 1, text.find_first_of(" />", element.begin) - element.begin - 1);
        if (name != "conditions" && name != "configid")
        {
            continue;
        }
        Outcomes one;
        loader.load(
            QtRocket::stringToBytes(text.substr(0, element.begin) + text.substr(element.end)),
            std::string(name), one);
        if (one.errors == 1)
        {
            refused.emplace_back(name);
        }
    }
    EXPECT_EQ(refused, (std::vector<std::string>{"conditions", "configid"}));
}

/// The number of shards of the wide sweep.
constexpr std::size_t kShards = 20;

/// A part of the wide sweep: every @p stride-th byte, shard @p shard of kShards.
struct WideSweepPart
{
    std::size_t stride;
    std::size_t shard;
};

/// What GoogleTest prints for the part of a test that failed.
// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
void PrintTo(const WideSweepPart& part, std::ostream* out)
{
    *out << "stride " << part.stride << ", shard " << part.shard;
}

/// The wide sweep over one recorded design (see LoaderHostileInputWideSweep).
[[nodiscard]] Outcomes wideSweepOf(const SweepLoader& loader, std::string_view file,
                                   const WideSweepPart& part)
{
    const Bytes bytes    = recordedBytes(file);
    Outcomes    outcomes = sweepTruncations(loader, file, bytes, part.stride, part.shard, kShards);
    outcomes.add(sweepReplacements(loader, file, bytes, part.stride, part.shard, kShards));
    outcomes.add(sweepElements(loader, file, recordedDocument(file), part.shard, kShards));
    return outcomes;
}

/// The wide sweep over every recorded design (the table of RecordedOrkFiles.h, which a test
/// checks against the directory), with the bundled motor database; what each design gave and
/// how long it took is printed.
[[nodiscard]] Outcomes wideSweep(const WideSweepPart& part)
{
    const SweepLoader loader(QtRocket::Test::bundledMotorDatabase());
    Outcomes          outcomes;
    for (const QtRocket::Test::RecordedOrkFile& recorded : QtRocket::Test::kRecordedOrkFiles)
    {
        const std::string_view file  = recorded.file;
        const auto             start = std::chrono::steady_clock::now();
        const Outcomes         one   = wideSweepOf(loader, file, part);
        std::cout << std::format("{}: {} ({} s)\n", file, one.summary(),
                                 std::chrono::duration_cast<std::chrono::seconds>(
                                     std::chrono::steady_clock::now() - start)
                                     .count())
                  << std::flush;
        outcomes.add(one);
    }
    return outcomes;
}

class LoaderHostileInputWideSweep : public ::testing::TestWithParam<WideSweepPart>
{ };

// The wide sweep: each of the 18 recorded designs (363,558 bytes) cut off at every byte and with
// every byte replaced in turn, and the document of each with every one of its elements in turn
// taken out and in turn written twice (6,661 elements), with the bundled motor database:
// 740,438 loads. It is a measurement run by hand, in kShards shards side by side:
//
//     QtRocket_core_tests --gtest_also_run_disabled_tests
//         --gtest_filter='EveryByte/LoaderHostileInputWideSweep.*/<n>'      for n = 0 ... 19
//
// A load costs what the document costs up to the damage, and most damage sits behind most of
// the document: the sweep is 3.7 hours of processor time in an optimised build without
// sanitizers and would be about 150 hours in the build of the asan preset (30 in an optimised
// build with the sanitizers). "EveryByte" is therefore for an optimised build, and under the
// sanitizers the same sweep is run over every 32nd byte, and every element:
//
//         --gtest_filter='Every32ndByte/LoaderHostileInputWideSweep.*/<n>'
//
// Both were run when the test was written. "EveryByte" (GCC, -O3, 12 minutes for the 20
// shards): 162,073 loads gave a document and 578,365 an Error, none threw. "Every32ndByte"
// (the asan preset: AddressSanitizer and UndefinedBehaviorSanitizer, 17 processor hours):
// 36,058 loads, 17,889 documents and 18,169 Errors, none threw and no sanitizer reported
// anything.
TEST_P(LoaderHostileInputWideSweep, DISABLED_NoDamagedRecordedFileMakesTheLoaderThrow)
{
    const Outcomes outcomes = wideSweep(GetParam());
    std::cout << "total: " << outcomes.summary() << "\n";
    EXPECT_EQ(outcomes.firstThrown, "") << outcomes.summary();
    EXPECT_EQ(outcomes.thrown, 0U);
}

/// The shards of a wide sweep over every @p stride-th byte.
[[nodiscard]] std::vector<WideSweepPart> shardsOf(std::size_t stride)
{
    std::vector<WideSweepPart> parts;
    parts.reserve(kShards);
    for (std::size_t shard = 0; shard < kShards; shard++)
    {
        parts.push_back({.stride = stride, .shard = shard});
    }
    return parts;
}

/// The name of a shard's test: its number.
[[nodiscard]] std::string shardName(const ::testing::TestParamInfo<WideSweepPart>& info)
{
    return std::to_string(info.param.shard);
}

INSTANTIATE_TEST_SUITE_P(EveryByte, LoaderHostileInputWideSweep, ::testing::ValuesIn(shardsOf(1)),
                         shardName);
INSTANTIATE_TEST_SUITE_P(Every32ndByte, LoaderHostileInputWideSweep,
                         ::testing::ValuesIn(shardsOf(32)), shardName);

/// "<what>: <summary> (<n> ms)": what the sweep @p sweep met and how long it took.
template <class Sweep>
[[nodiscard]] std::string timed(std::string_view what, Sweep sweep)
{
    const auto     start    = std::chrono::steady_clock::now();
    const Outcomes outcomes = sweep();
    return std::format("{}: {} ({} ms)\n", what, outcomes.summary(),
                       std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - start)
                           .count());
}

// A measurement, not a test: how long sweeps of the kind the suite runs take, per design, and
// what they meet (run with --gtest_also_run_disabled_tests).
TEST(LoaderHostileInput, DISABLED_PrintsWhatSweepsCost)
{
    const SweepLoader loader;
    for (const std::string_view file : {kGzipFile, kZipFile, kPlainFile})
    {
        const Bytes bytes = recordedBytes(file);
        std::cout << timed(std::format("{} cut off at every 97th byte", file),
                           [&] { return sweepTruncations(loader, file, bytes, 97); })
                  << timed(std::format("{} with 60 bytes replaced", file),
                           [&] { return sweepRandomReplacements(loader, file, bytes, 60); });
    }
    for (const std::string_view file : {kGzipFile, kZipFile})
    {
        std::cout << timed(std::format("{} with each element out and twice", file),
                           [&] { return sweepElements(loader, file, recordedDocument(file)); });
    }
}

}  // namespace
