// A design reads a lookup table only from inside its own directory (the user's decision of
// 2026-10-08; CsvLookupHandler has the rule, and CsvLookupHandlerTests.cpp the rule by itself).
// Here is what a whole load makes of it: designs that name their drag table in every form a
// name can take are loaded through GeneralRocketLoader, in every way a design can be loaded,
// and no load may come back with a table that lies outside the design's directory.
//
// The directories of a test, under a temporary one:
//   design/               the design's directory: the design file and tables/drag.csv
//   outside/drag.csv      a table no design may read
//   drag.csv              another, in the directory above the design's
//   cwd/                  the current directory of the process, with drag.csv and
//                         tables/drag.csv: tables that OpenRocket, which resolves a name
//                         against the current directory, would read
// The three kinds of table have different values, so a table says where it was read from.

#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/lookup/MachAoALookup.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/GzipStream.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestTempDir.h"
#include "file/RawZip.h"
#include "file/RocketLoaderTestSupport.h"
#include "file/openrocket/ConditionsTestSupport.h"

namespace
{

using QtRocket::GeneralRocketLoader;
using QtRocket::LoadedDocument;
using QtRocket::MachAoALookup;
using QtRocket::OpenRocketDocument;
using QtRocket::Result;
using QtRocket::Simulation;
using QtRocket::stringToBytes;
using QtRocket::Warning;
using QtRocket::Test::CurrentDirectoryGuard;
using QtRocket::Test::genericUtf8;
using QtRocket::Test::LoaderEnvironment;
using QtRocket::Test::replaceAll;
using QtRocket::Test::TempDir;

using Bytes = std::vector<std::byte>;
using Texts = std::vector<std::string>;

/// The table of the design's directory, whose drag coefficient at Mach 0.5 is 0.4.
constexpr std::string_view kInsideTable = "Mach,Cd\n0,0.3\n1,0.5\n";
/// The tables no design may read, whose drag coefficient is 0.9.
constexpr std::string_view kOutsideTable = "Mach,Cd\n0,0.9\n1,0.9\n";
/// The tables of the current directory, whose drag coefficient is 0.7.
constexpr std::string_view kCurrentTable = "Mach,Cd\n0,0.7\n1,0.7\n";

/// The directories of a test (see the head of this file), with the current directory of the
/// process in "cwd" for as long as it lives.
class Site
{
public:
    Site() : m_current(m_directory.resolve("cwd"))
    {
        EXPECT_TRUE(written("design/tables/drag.csv", kInsideTable));
        EXPECT_TRUE(written("outside/drag.csv", kOutsideTable));
        EXPECT_TRUE(written("drag.csv", kOutsideTable));
        EXPECT_TRUE(written("cwd/drag.csv", kCurrentTable));
        EXPECT_TRUE(written("cwd/tables/drag.csv", kCurrentTable));
    }

    /// The temporary directory everything is in.
    [[nodiscard]] const std::filesystem::path& top() const noexcept { return m_directory.path(); }
    /// The design's directory.
    [[nodiscard]] std::filesystem::path design() const { return m_directory.resolve("design"); }

    /// Writes @p bytes as the design file of the design's directory and returns the file.
    [[nodiscard]] std::filesystem::path writeDesign(const Bytes& bytes) const
    {
        const std::filesystem::path file = design() / "rocket.ork";
        EXPECT_TRUE(QtRocket::writeFile(file, bytes));
        return file;
    }

private:
    [[nodiscard]] bool written(std::string_view name, std::string_view text) const
    {
        return std::filesystem::exists(m_directory.write(QtRocket::pathFromUtf8(name), text));
    }

    TempDir m_directory;
    // After the directory it points into, so that it ends first.
    CurrentDirectoryGuard m_current;
};

/// @p text as the text of an XML element or of an attribute between double quotes.
[[nodiscard]] std::string xmlText(std::string text)
{
    text = replaceAll(std::move(text), "&", "&amp;");
    text = replaceAll(std::move(text), "<", "&lt;");
    return replaceAll(std::move(text), "\"", "&quot;");
}

/// A design with two simulations that name @p file as their drag table: the first by a
/// <draglookup> element and the second by the <draglookupcsv> element of older files.
[[nodiscard]] std::string designNaming(std::string_view file)
{
    return std::format(
        R"(<?xml version='1.0' encoding='utf-8'?>
<openrocket version="1.10" creator="test">
<rocket><name>R</name><subcomponents><stage><subcomponents><bodytube/></subcomponents></stage></subcomponents></rocket>
<simulations>
<simulation status="notsimulated"><name>by a lookup element</name><conditions><configid>11111111-1111-1111-1111-111111111111</configid><draglookup file="{0}"/></conditions></simulation>
<simulation status="notsimulated"><name>by a legacy element</name><conditions><configid>11111111-1111-1111-1111-111111111111</configid><draglookupcsv>{0}</draglookupcsv></conditions></simulation>
</simulations>
</openrocket>
)",
        xmlText(std::string(file)));
}

/// How a design reaches the loader.
enum class How
{
    /// A plain file of the design's directory.
    PLAIN_FILE,
    /// The same as a gzip stream.
    GZIP_FILE,
    /// The same as a zip archive whose entry is the design.
    ARCHIVE_FILE,
    /// Bytes in memory, with the design's directory as their base directory.
    BYTES_WITH_DIRECTORY,
    /// An archive in memory, with the design's directory as its base directory.
    ARCHIVE_WITH_DIRECTORY,
    /// Bytes in memory and no directory.
    BYTES,
    /// An archive in memory and no directory.
    ARCHIVE,
};

/// Whether a design loaded @p how has a directory.
[[nodiscard]] bool hasDirectory(How how) noexcept
{
    return how != How::BYTES && how != How::ARCHIVE;
}

/// The bytes of the design @p design as @p how wants them.
[[nodiscard]] Bytes bytesOf(const std::string& design, How how)
{
    if (how == How::GZIP_FILE)
    {
        Result<Bytes> stream = QtRocket::gzipDeflate(stringToBytes(design));
        if (!stream)
        {
            QtRocket::bug("gzipDeflate failed: " + stream.error().message);
        }
        return std::move(*stream);
    }
    if (how == How::ARCHIVE_FILE || how == How::ARCHIVE_WITH_DIRECTORY || how == How::ARCHIVE)
    {
        return QtRocket::Test::archive({QtRocket::Test::stored("rocket.ork", design)});
    }
    return stringToBytes(design);
}

/// Loads @p design, the text of a design file, @p how at @p site.
[[nodiscard]] Result<LoadedDocument> loadDesign(const GeneralRocketLoader& loader, const Site& site,
                                                const std::string& design, How how)
{
    Bytes bytes = bytesOf(design, how);
    switch (how)
    {
        case How::PLAIN_FILE:
        case How::GZIP_FILE:
        case How::ARCHIVE_FILE:
            return loader.load(site.writeDesign(bytes));
        case How::BYTES_WITH_DIRECTORY:
        case How::ARCHIVE_WITH_DIRECTORY:
            return loader.load(std::move(bytes), site.design());
        case How::BYTES:
        case How::ARCHIVE:
            break;
    }
    return loader.load(std::move(bytes));
}

/// Where the drag table of @p simulation was read from, told by its value: "inside" for the
/// table of the design's directory, "OUTSIDE" and "CURRENT" for the tables no design may read,
/// "none" without a table.
[[nodiscard]] std::string_view tableOf(const Simulation& simulation)
{
    const std::shared_ptr<const MachAoALookup> table = simulation.getOptions().getDragLookupTable();
    if (table == nullptr)
    {
        return "none";
    }
    const double cd = table->interpolate(0.5, 0, "cd");
    if (cd < 0.55)
    {
        return "inside";
    }
    return cd < 0.8 ? "CURRENT" : "OUTSIDE";
}

/// The kind of the warning @p warning of a load: "refused" for a file a design may not read,
/// "device" for the name of a device, "failed" for a file that was looked for and could not be
/// read, and the text itself for anything else.
[[nodiscard]] std::string kindOf(const Warning& warning)
{
    const std::string text = warning.toString();
    if (text.contains("a design may name such a file only by a relative path inside its own"))
    {
        return "refused";
    }
    if (text.contains("a name that Windows reserves for a device"))
    {
        return "device";
    }
    return text.starts_with("Failed to load ") ? "failed" : text;
}

/// What a load gave: where the drag table of each simulation was read from, then, behind a
/// bar, the kinds of the warnings: "inside inside |", "none none | refused refused".
[[nodiscard]] std::string outcomeOf(const Result<LoadedDocument>& loaded)
{
    if (!loaded.has_value())
    {
        return "no document: " + loaded.error().message;
    }
    const OpenRocketDocument& document = *loaded->document;
    std::string               text;
    for (std::size_t i = 0; i < document.getSimulationCount(); i++)
    {
        text += std::format("{}{}", i == 0 ? "" : " ", tableOf(*document.getSimulation(i)));
    }
    text += " |";
    for (const Warning& warning : loaded->warnings)
    {
        text += " " + kindOf(warning);
    }
    return text;
}

/// A name a design gives its drag table, and what a load of a design with a directory gives
/// for it. "{TOP}" in the name stands for the temporary directory of the test, spelled with
/// '/', and "{NATIVE}" for the same as the platform spells it: they make the name absolute.
struct Form
{
    std::string_view file;
    std::string_view expected;
};

constexpr std::string_view kRead    = "inside inside |";
constexpr std::string_view kRefused = "none none | refused refused";
constexpr std::string_view kMissing = "none none | failed failed";
/// A name of a device of Windows: refused on Windows, where it opens the device, and a file of
/// the design's directory elsewhere, which is not there.
#ifdef _WIN32
constexpr std::string_view kDevice = "none none | device device";
#else
constexpr std::string_view kDevice = kMissing;
#endif

constexpr auto kForms = std::to_array<Form>({
    // Relative names that stay inside: the table of the design's directory.
    {.file = "tables/drag.csv", .expected = kRead},
    {.file = "./tables/drag.csv", .expected = kRead},
    {.file = "tables//drag.csv", .expected = kRead},
    {.file = "tables/sub/../drag.csv", .expected = kRead},
    {.file = "tables/../tables/./drag.csv", .expected = kRead},
    {.file = "  tables/drag.csv  ", .expected = kRead},
    // Relative names that stay inside and name no file there, although the current directory
    // and the directory above have a file of that name.
    {.file = "drag.csv", .expected = kMissing},
    {.file = "outside/drag.csv", .expected = kMissing},
    {.file = "cwd/drag.csv", .expected = kMissing},
    {.file = "tables", .expected = kMissing},
    {.file = ".", .expected = kMissing},
    // Absolute paths: to a table outside, to the table inside, to the current directory's.
    {.file = "{TOP}/outside/drag.csv", .expected = kRefused},
    {.file = "{NATIVE}/outside/drag.csv", .expected = kRefused},
    {.file = "{TOP}/drag.csv", .expected = kRefused},
    {.file = "{TOP}/design/tables/drag.csv", .expected = kRefused},
    {.file = "{TOP}/cwd/drag.csv", .expected = kRefused},
    {.file = "{TOP}/design/../outside/drag.csv", .expected = kRefused},
    {.file = "/", .expected = kRefused},
    {.file = "//", .expected = kRefused},
    // Two dots at the start, in the middle and after a subdirectory.
    {.file = "..", .expected = kRefused},
    {.file = "../drag.csv", .expected = kRefused},
    {.file = "../outside/drag.csv", .expected = kRefused},
    {.file = "../cwd/drag.csv", .expected = kRefused},
    {.file = "../cwd/tables/drag.csv", .expected = kRefused},
    {.file = "./../drag.csv", .expected = kRefused},
    {.file = "..//outside//drag.csv", .expected = kRefused},
    {.file = "tables/../../drag.csv", .expected = kRefused},
    {.file = "tables/../../outside/drag.csv", .expected = kRefused},
    {.file = "tables/sub/../../../outside/drag.csv", .expected = kRefused},
    {.file = "tables/./../.././outside/drag.csv", .expected = kRefused},
    // Out of the directory and into it again.
    {.file = "../design/tables/drag.csv", .expected = kRefused},
    {.file = "tables/../../design/tables/drag.csv", .expected = kRefused},
    // Elements of dots and blanks.
    {.file = ".../drag.csv", .expected = kRefused},
    {.file = "tables/.. /drag.csv", .expected = kRefused},
    {.file = ".. /outside/drag.csv", .expected = kRefused},
    // The names of devices of Windows (kDevice: what is expected of them differs by platform).
    {.file = "nul", .expected = kDevice},
    {.file = "tables/nul", .expected = kDevice},
    {.file = "aux.csv", .expected = kDevice},
    {.file = "COM1", .expected = kDevice},
// The forms of a Windows path. On Windows they have a root, lead out of the directory or
// name a device; elsewhere each is one odd name inside the design's directory, where no
// such file is (see CsvLookupHandlerTests.cpp).
#ifdef _WIN32
    {.file = R"(..\outside\drag.csv)", .expected = kRefused},
    {.file = R"(tables\..\..\outside\drag.csv)", .expected = kRefused},
    {.file = R"(C:\Windows\win.ini)", .expected = kRefused},
    {.file = "C:drag.csv", .expected = kRefused},
    {.file = R"(\Windows\win.ini)", .expected = kRefused},
    {.file = R"(\\localhost\C$\Windows\win.ini)", .expected = kRefused},
    {.file = R"(\\.\NUL)", .expected = kRefused},
    {.file = R"(\\?\C:\Windows\win.ini)", .expected = kRefused},
    {.file = R"(tables\nul)", .expected = kDevice},
    {.file = R"(tables\drag.csv)", .expected = kRead},
#else
    {.file = R"(..\outside\drag.csv)", .expected = kMissing},
    {.file = R"(tables\..\..\outside\drag.csv)", .expected = kMissing},
    {.file = R"(C:\Windows\win.ini)", .expected = kMissing},
    {.file = "C:drag.csv", .expected = kMissing},
    {.file = R"(\Windows\win.ini)", .expected = kMissing},
    {.file = R"(\\localhost\C$\Windows\win.ini)", .expected = kMissing},
    {.file = R"(\\.\NUL)", .expected = kMissing},
    {.file = R"(\\?\C:\Windows\win.ini)", .expected = kMissing},
    {.file = R"(tables\nul)", .expected = kMissing},
    {.file = R"(tables\drag.csv)", .expected = kMissing},
#endif
});

/// The name of @p form with the directory of @p site in the place of its marks.
[[nodiscard]] std::string nameOf(const Form& form, const Site& site)
{
    std::string name = replaceAll(std::string(form.file), "{TOP}", genericUtf8(site.top()));
    return replaceAll(std::move(name), "{NATIVE}", QtRocket::pathToUtf8(site.top()));
}

/// Loads a design for every form of kForms @p how and returns the forms whose load did not
/// give what is expected, each as "<name>: <outcome>, not <expected>". A design without a
/// directory is expected to read nothing and to be refused whatever it names.
[[nodiscard]] Texts wrongOutcomes(How how)
{
    const Site                site;
    LoaderEnvironment         environment;
    const GeneralRocketLoader loader(environment.context());
    Texts                     wrong;
    for (const Form& form : kForms)
    {
        const std::string_view expected = hasDirectory(how) ? form.expected : kRefused;
        const std::string      outcome =
            outcomeOf(loadDesign(loader, site, designNaming(nameOf(form, site)), how));
        if (outcome != expected)
        {
            wrong.push_back(std::format("{}: {}, not {}", form.file, outcome, expected));
        }
    }
    return wrong;
}

// The sweep stands on its tables: what a load would give if it read each of them.
TEST(LookupFileContainment, ATableTellsWhereItWasReadFrom)
{
    const Site                site;
    LoaderEnvironment         environment;
    const GeneralRocketLoader loader(environment.context());

    // A design in the directory above reads "drag.csv" there, and one in the current
    // directory the table of that.
    EXPECT_EQ(outcomeOf(loader.load(stringToBytes(designNaming("drag.csv")), site.top())),
              "OUTSIDE OUTSIDE |");
    EXPECT_EQ(outcomeOf(loader.load(stringToBytes(designNaming("drag.csv")), site.top() / "cwd")),
              "CURRENT CURRENT |");
    EXPECT_EQ(outcomeOf(loader.load(stringToBytes(designNaming("tables/drag.csv")),
                                    site.top() / "design")),
              kRead);
    // A design that names nothing has no table and no warning.
    EXPECT_EQ(outcomeOf(loader.load(stringToBytes(designNaming("  ")), site.design())),
              "none none |");
}

TEST(LookupFileContainment, APlainDesignFileReadsOnlyInsideItsDirectory)
{
    EXPECT_EQ(wrongOutcomes(How::PLAIN_FILE), Texts{});
}

TEST(LookupFileContainment, AGzipDesignFileReadsOnlyInsideItsDirectory)
{
    EXPECT_EQ(wrongOutcomes(How::GZIP_FILE), Texts{});
}

// The tables of an archive are files beside it too: an archive holds decal images, no tables.
TEST(LookupFileContainment, AnArchiveReadsOnlyInsideItsDirectory)
{
    EXPECT_EQ(wrongOutcomes(How::ARCHIVE_FILE), Texts{});
}

TEST(LookupFileContainment, ADesignFromMemoryReadsOnlyInsideItsBaseDirectory)
{
    EXPECT_EQ(wrongOutcomes(How::BYTES_WITH_DIRECTORY), Texts{});
    EXPECT_EQ(wrongOutcomes(How::ARCHIVE_WITH_DIRECTORY), Texts{});
}

// No directory: nothing is read, not even the table that lies under the very name in the
// current directory of the process (which is where OpenRocket would look for it).
TEST(LookupFileContainment, ADesignFromMemoryWithoutADirectoryReadsNothing)
{
    EXPECT_EQ(wrongOutcomes(How::BYTES), Texts{});
    EXPECT_EQ(wrongOutcomes(How::ARCHIVE), Texts{});
}

// A design file that is named without a directory is one of the current directory, which is
// then the design's own (GeneralRocketLoader gives the context "." for it).
TEST(LookupFileContainment, AFileNamedWithoutADirectoryHasTheCurrentDirectoryAsItsOwn)
{
    const Site                site;
    LoaderEnvironment         environment;
    const GeneralRocketLoader loader(environment.context());
    ASSERT_TRUE(QtRocket::writeFile(site.top() / "cwd" / "here.ork",
                                    stringToBytes(designNaming("tables/drag.csv"))));
    ASSERT_TRUE(QtRocket::writeFile(site.top() / "cwd" / "above.ork",
                                    stringToBytes(designNaming("../outside/drag.csv"))));

    EXPECT_EQ(outcomeOf(loader.load(std::filesystem::path("here.ork"))), "CURRENT CURRENT |");
    EXPECT_EQ(outcomeOf(loader.load(std::filesystem::path("above.ork"))), kRefused);
}

}  // namespace
