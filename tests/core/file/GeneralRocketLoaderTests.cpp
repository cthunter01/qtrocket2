// Tests of GeneralRocketLoader, the entry of the file subsystem (OpenRocket's
// file/GeneralRocketLoader). What is expected is what OpenRocket gives for the same bytes
// (the probes of tier 9c, part "loader": TopProbe.java for the documents of
// file/RocketLoaderCases.h, Sniff3.java for the inputs of this file, which it is given as the
// test DISABLED_PrintsTheInputs prints them), but where a test says that QtRocket answers
// otherwise on purpose.

#include "QtRocket/file/GeneralRocketLoader.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <format>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "QtRocket/document/DecalImage.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/AttachmentFactory.h"
#include "QtRocket/file/DatabaseMotorFinder.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/FileSystemAttachmentFactory.h"
#include "QtRocket/file/GzipStream.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/file/ZipFileAttachmentFactory.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/extension/SimulationExtensionRegistry.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Uuid.h"
#include "TestPaths.h"
#include "TestTempDir.h"
#include "file/RawZip.h"
#include "file/RocketLoaderCases.h"
#include "file/RocketLoaderTestSupport.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"
#include "simulation/SimulationRunSupport.h"
#include "unit/DefaultUnitsGuard.h"

namespace
{

using QtRocket::BugError;
using QtRocket::DocumentLoadingContext;
using QtRocket::ErrorCode;
using QtRocket::GeneralRocketLoader;
using QtRocket::LoadedDocument;
using QtRocket::OpenRocketDocument;
using QtRocket::Result;
using QtRocket::stringToBytes;
using QtRocket::Test::CurrentDirectoryGuard;
using QtRocket::Test::failedLoaderCases;
using QtRocket::Test::kLoadCases;
using QtRocket::Test::kOwnLoadCases;
using QtRocket::Test::LoaderEnvironment;
using QtRocket::Test::loadLinesOf;
using QtRocket::Test::onOneLine;
using QtRocket::Test::printedLoaderCases;
using QtRocket::Test::TempDir;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::StartsWith;

using Bytes = std::vector<std::byte>;

// --------------------------------------------------------------------------- building blocks

/// The design of the probe's inputs: a nose cone and three body tubes with decals, of which
/// two name the same image.
constexpr std::string_view kDesign =
    "<?xml version='1.0' encoding='utf-8'?>\n"
    "<openrocket version=\"1.10\" creator=\"probe\">\n"
    "<rocket><name>R</name><subcomponents><stage><subcomponents>"
    "<nosecone><appearance><decal name=\"decals/a.png\" rotation=\"0\" edgemode=\"REPEAT\"/>"
    "</appearance></nosecone>"
    "<bodytube><appearance><decal name=\"b.png\" rotation=\"0\" edgemode=\"REPEAT\"/>"
    "</appearance></bodytube>"
    "<bodytube><appearance><decal name=\"sub/decals/a.png\" rotation=\"0\" edgemode=\"REPEAT\"/>"
    "</appearance></bodytube>"
    "<bodytube><appearance><decal name=\"decals/a.png\" rotation=\"0\" edgemode=\"REPEAT\"/>"
    "</appearance></bodytube>"
    "</subcomponents></stage></subcomponents></rocket>\n"
    "</openrocket>\n";

/// The root element of kDesign and what follows it.
[[nodiscard]] std::string_view designFromRoot()
{
    return kDesign.substr(kDesign.find("<openrocket"));
}

/// The start and the end of a small design whose rocket has the name between them.
constexpr std::string_view kHead = "<openrocket version=\"1.10\"><rocket><name>";
constexpr std::string_view kTail = "</name></rocket></openrocket>";

/// The bytes of an image: 1 to 5.
[[nodiscard]] Bytes image()
{
    return {std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}, std::byte{5}};
}

[[nodiscard]] Bytes joined(std::initializer_list<Bytes> parts)
{
    Bytes all;
    for (const Bytes& part : parts)
    {
        all.insert(all.end(), part.begin(), part.end());
    }
    return all;
}

/// The first @p count bytes of @p bytes.
[[nodiscard]] Bytes firstBytes(const Bytes& bytes, std::size_t count)
{
    const std::span<const std::byte> start = std::span(bytes).first(std::min(count, bytes.size()));
    return {start.begin(), start.end()};
}

/// @p bytes with the byte at @p index changed.
[[nodiscard]] Bytes withByteFlipped(Bytes bytes, std::size_t index)
{
    bytes.at(index) ^= std::byte{0x55};
    return bytes;
}

/// @p bytes as a gzip stream.
[[nodiscard]] Bytes gzipOf(const Bytes& bytes)
{
    Result<Bytes> stream = QtRocket::gzipDeflate(bytes);
    if (!stream)
    {
        QtRocket::bug("gzipDeflate failed: " + stream.error().message);
    }
    return std::move(*stream);
}

/// A little-endian number of four bytes at @p offset of @p bytes.
[[nodiscard]] std::uint32_t number32(const Bytes& bytes, std::size_t offset)
{
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; i++)
    {
        value |= std::to_integer<std::uint32_t>(bytes.at(offset + i)) << (8U * i);
    }
    return value;
}

/// An entry of an archive: a name and its contents.
struct Item
{
    Item(std::string itemName, Bytes itemContents)
      : name(std::move(itemName)), contents(std::move(itemContents))
    {
    }

    std::string name;
    Bytes       contents;
};

/// An archive of @p items as java.util.zip.ZipOutputStream writes one, which is how OpenRocket
/// writes a design: every entry deflated, its sizes and check sum not in its header but in a
/// data descriptor behind its data (the local headers only: the loader reads nothing else).
[[nodiscard]] Bytes zipOf(const std::vector<Item>& items)
{
    constexpr std::size_t kGzipHeader  = 10;
    constexpr std::size_t kGzipTrailer = 8;
    Bytes                 out;
    for (const Item& item : items)
    {
        // The deflate stream of the contents is what a gzip stream of them holds between its
        // header and its trailer, which is the check sum and the length.
        const Bytes stream = gzipOf(item.contents);
        const Bytes deflated(stream.begin() + static_cast<std::ptrdiff_t>(kGzipHeader),
                             stream.end() - static_cast<std::ptrdiff_t>(kGzipTrailer));
        QtRocket::Test::LocalEntry entry{.name           = item.name,
                                         .data           = deflated,
                                         .method         = QtRocket::Test::kDeflated,
                                         .flags          = 0x0808,
                                         .crc            = 0,
                                         .compressedSize = 0,
                                         .size           = 0,
                                         .extra          = {}};
        QtRocket::Test::put32(entry.data, 0x08074b50);
        QtRocket::Test::put32(entry.data, number32(stream, stream.size() - kGzipTrailer));
        QtRocket::Test::put32(entry.data, static_cast<std::uint32_t>(deflated.size()));
        QtRocket::Test::put32(entry.data, static_cast<std::uint32_t>(item.contents.size()));
        QtRocket::Test::append(out, entry);
    }
    return out;
}

/// An archive of @p items whose entries are stored, not deflated, with their sizes and check
/// sums in their headers.
[[nodiscard]] Bytes storedZipOf(const std::vector<Item>& items)
{
    Bytes out;
    for (const Item& item : items)
    {
        QtRocket::Test::append(
            out, QtRocket::Test::stored(item.name, QtRocket::bytesToString(item.contents)));
    }
    return out;
}

/// An item whose contents are @p text.
[[nodiscard]] Item textItem(std::string name, std::string_view text)
{
    return {std::move(name), stringToBytes(text)};
}

/// The archive of no entry that ZipOutputStream writes: the end record of the central
/// directory alone.
[[nodiscard]] Bytes emptyZip()
{
    Bytes out;
    QtRocket::Test::put32(out, 0x06054b50);
    out.resize(22);
    return out;
}

/// @p text in UTF-16 with a byte order mark, big endian (Java's StandardCharsets.UTF_16); the
/// text is ASCII.
[[nodiscard]] Bytes utf16Of(std::string_view text)
{
    Bytes out{std::byte{0xFE}, std::byte{0xFF}};
    for (const char c : text)
    {
        out.push_back(std::byte{0});
        out.push_back(static_cast<std::byte>(c));
    }
    return out;
}

/// What a load gave, in the notation of the probe Sniff3.java: "FAILED <code>: <message>" (the
/// probe has no code), or "LOADED '<rocket name>' components=<n> warnings=[<texts>]
/// decals=[<names>]", the names of the decal images sorted, each with "=<number of bytes>" or
/// "=missing" behind it when @p readDecals.
[[nodiscard]] std::string outcomeOf(const Result<LoadedDocument>& loaded, bool readDecals)
{
    if (!loaded)
    {
        return std::format("FAILED {}: {}", toString(loaded.error().code),
                           onOneLine(loaded.error().message));
    }
    const OpenRocketDocument& document   = *loaded->document;
    std::size_t               components = 0;
    document.getRocket().forEach(
        [&components](const QtRocket::RocketComponent& /*component*/) { components++; });
    std::string warnings;
    for (const QtRocket::Warning& warning : loaded->warnings)
    {
        warnings += (warnings.empty() ? "" : " | ") + onOneLine(warning.toString());
    }
    std::vector<std::string> decals;
    for (const std::shared_ptr<QtRocket::DecalImage>& decal : document.getDecalList())
    {
        std::string text = decal->getName();
        if (readDecals)
        {
            const Result<Bytes> bytes = decal->getBytes();
            text += bytes.has_value() ? std::format("={}", bytes->size()) : "=missing";
        }
        decals.push_back(std::move(text));
    }
    std::ranges::sort(decals);
    std::string names;
    for (const std::string& decal : decals)
    {
        names += (names.empty() ? "" : ", ") + decal;
    }
    return std::format("LOADED '{}' components={} warnings=[{}] decals=[{}]",
                       onOneLine(document.getRocket().getName()), components, warnings, names);
}

/// A directory with the image "decals/a.png" in it, as the probe makes one for the inputs it
/// loads as files.
class DesignDirectory
{
public:
    DesignDirectory()
    {
        std::filesystem::create_directories(m_directory.resolve("decals"));
        const Result<void> written =
            QtRocket::writeFile(m_directory.resolve("decals/a.png"), image());
        if (!written)
        {
            QtRocket::bug("cannot write the image: " + written.error().message);
        }
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return m_directory.path(); }

    /// Writes @p bytes as the file @p name of the directory and gives its path.
    [[nodiscard]] std::filesystem::path write(std::string_view name, const Bytes& bytes) const
    {
        const std::filesystem::path file = m_directory.resolve(name);
        if (const Result<void> written = QtRocket::writeFile(file, bytes); !written)
        {
            QtRocket::bug("cannot write the design: " + written.error().message);
        }
        return file;
    }

private:
    TempDir m_directory;
};

/// What loading @p bytes gives (outcomeOf()): as the file "design.ork" of a directory with
/// "decals/a.png" when @p asFile, else from memory without a base directory.
[[nodiscard]] std::string outcomeOfLoading(const Bytes& bytes, bool asFile)
{
    LoaderEnvironment         environment;
    const GeneralRocketLoader loader(environment.context());
    if (asFile)
    {
        const DesignDirectory directory;
        return outcomeOf(loader.load(directory.write("design.ork", bytes)), true);
    }
    return outcomeOf(loader.load(bytes), false);
}

// ------------------------------------------------------------- what a file is: the inputs

/// Why QtRocket answers an input otherwise than OpenRocket.
enum class Deviation
{
    /// It does not.
    NONE,
    /// An archive in memory has its attachments (decision T1). OpenRocket dies of a
    /// NullPointerException for every archive that is read from a stream without a file.
    ARCHIVE_IN_MEMORY,
    /// The first later entry named *.ork is the document, and an archive without a document
    /// is refused (decision T3). OpenRocket returns the empty rocket, silently.
    ARCHIVE_ENTRY,
    /// XmlScanner does not read a document type declaration with declarations of its own.
    /// OpenRocket loads the document.
    DOCUMENT_TYPE,
    /// A damaged stream fails in both, with another text: QtRocket unpacks a stream whole
    /// before it looks at the document, OpenRocket reads it through buffers and notices the
    /// damage when it gets there, and the reason is QtRocket's own text where it comes from
    /// zlib.
    DAMAGE,
};

/// An input of the loader and what loading it gives.
struct Input
{
    /// What the input is, in the words of the scouts' probe.
    std::string name;
    /// Whether it is loaded as a file (in a directory with "decals/a.png") or from memory.
    bool asFile;
    /// The bytes.
    Bytes bytes;
    /// What QtRocket gives (outcomeOf()).
    std::string expected;
    /// Why that is not what OpenRocket gives, and what OpenRocket gives then.
    Deviation   deviation{Deviation::NONE};
    std::string openRocket;
};

constexpr std::string_view kUnsupported = "FAILED UNSUPPORTED_FORMAT: Unsupported or corrupt file.";
constexpr std::string_view kMalformed   = "FAILED PARSE: Malformed XML in input.";
constexpr std::string_view kDiesOfNull  = "JAVA DIES: java.lang.NullPointerException";
/// The empty rocket OpenRocket returns for an archive whose first entry is no design.
constexpr std::string_view kEmptyRocket = "LOADED 'Rocket' components=1 warnings=[] decals=[]";
/// The design read without a base directory: the images are files, which the document names
/// "decals/<file name>", with a number for a second file of the same name.
constexpr std::string_view kDesignFromMemory =
    "LOADED 'R' components=6 warnings=[] decals=[decals/a (1).png, decals/a.png, decals/b.png]";
/// The design read from an archive in memory: the images keep the names the design gives.
constexpr std::string_view kDesignFromArchiveInMemory =
    "LOADED 'R' components=6 warnings=[] decals=[b.png, decals/a.png, sub/decals/a.png]";
/// The design read from an archive file that holds no image.
constexpr std::string_view kDesignFromArchive =
    "LOADED 'R' components=6 warnings=[] decals=[b.png=missing, decals/a.png=missing, "
    "sub/decals/a.png=missing]";
/// The design read as a plain or gzip file beside "decals/a.png".
constexpr std::string_view kDesignBesideImage =
    "LOADED 'R' components=6 warnings=[] decals=[decals/a (1).png=missing, decals/a.png=5, "
    "decals/b.png=missing]";

/// A row of the table.
[[nodiscard]] Input input(std::string_view name, bool asFile, Bytes bytes,
                          std::string_view expected, Deviation deviation = Deviation::NONE,
                          std::string_view openRocket = {})
{
    return {.name       = std::string(name),
            .asFile     = asFile,
            .bytes      = std::move(bytes),
            .expected   = std::string(expected),
            .deviation  = deviation,
            .openRocket = std::string(openRocket)};
}

/// The scouts' input "corrupt zip data", byte for byte: the archive Java's ZipOutputStream made
/// of kDesign as "rocket.ork", with the byte at a third of its length (117 of 351) changed. The
/// changed byte is in the deflated data, which still inflates to its end, to 623 bytes of
/// which a few are wrong: the document then starts "<?xml version='1.0'
/// encoding='utf-8'Rg<<openrocket version=...". So the entry fails its check sum, which nobody
/// looks at before the document is looked at, and the document is none: its "<openrocket"
/// stands behind a "<", where Java's matcher does not find it.
constexpr std::string_view kScoutsCorruptArchive =
    "504b0304140008080800c2ae475d0000000000000000000000000a000000726f636b65742e6f726bad924d4fc330"
    "0c86effd15512e3d51af370e69260ebba3893fe024a69a58ed284911fbf70b65a28849203e4efed0abf7b1659bed"
    "cb7454cf94f2417868fb6ed32a622fe1c0e3d0cee5f1e6b6dd8fc648244ee29fa8bc8b75dff51bad7c222c92061d"
    "9338d255fba6b3867122bb37b0449367e7658ac2c425d7b2e078dd65c9e46b610dc64898907dcd03793caa579b41"
    "2f7906ec228f5a2529589661ea2414469a2454d17e77bfbb7bd0600d7cf481d5de493895d97d0172bf22acbedf13"
    "eaeef0a7757e02fb27107c3a185cee78d5bf3c4163607d1ddb9c01504b07081ea00b8fd90000006f020000504b01"
    "021400140008080800c2ae475d1ea00b8fd90000006f0200000a0000000000000000000000000000000000726f63"
    "6b65742e6f726b504b0506000000000100010038000000110100000000";

/// The archives of the scouts' probe, by the name of the input.
[[nodiscard]] std::vector<Item> itemsOf(std::string_view name)
{
    const Bytes design = stringToBytes(kDesign);
    if (name == "zip rocket.ork first")
    {
        return {{"rocket.ork", design}, {"decals/a.png", image()}};
    }
    if (name == "zip preview first")
    {
        return {{"preview.png", image()}, {"rocket.ork", design}};
    }
    if (name == "zip dir entry first")
    {
        return {{"decals/", {}}, {"rocket.ork", design}};
    }
    if (name == "zip holding gzip ork")
    {
        return {{"rocket.ork", gzipOf(design)}};
    }
    if (name == "zip holding zip ork")
    {
        return {{"rocket.ork", zipOf({{"rocket.ork", design}})}};
    }
    if (name == "zip holding short ork")
    {
        return {textItem("rocket.ork", "<a/>")};
    }
    if (name == "zip holding garbage ork")
    {
        return {textItem("rocket.ork", "0123456789012345678901234567890")};
    }
    // "zip <name of the one entry> ...": the design under that name.
    const std::string_view entry = name.substr(4, name.find(' ', 4) - 4);
    return {{std::string(entry), design}};
}

/// The inputs that are loaded from memory (the scouts' probe Sniff.java, "stream").
[[nodiscard]] std::vector<Input> inputsFromMemory()
{
    const Bytes design  = stringToBytes(kDesign);
    const auto  archive = [](std::string_view name, std::string_view expected) {
        return input(name, false, zipOf(itemsOf(name)), expected, Deviation::ARCHIVE_IN_MEMORY,
                     kDiesOfNull);
    };
    std::vector<Input> inputs;
    inputs.push_back(input("plain", false, design, kDesignFromMemory));
    inputs.push_back(input("gzip", false, gzipOf(design), kDesignFromMemory));
    inputs.push_back(archive("zip rocket.ork first", kDesignFromArchiveInMemory));
    inputs.push_back(archive("zip preview first", kDesignFromArchiveInMemory));
    inputs.push_back(archive("zip ROCKET.ORK", kDesignFromArchiveInMemory));
    inputs.push_back(archive("zip dir/x.OrK", kDesignFromArchiveInMemory));
    inputs.push_back(archive("zip x.rkt holding ork", kDesignFromArchiveInMemory));
    inputs.push_back(archive("zip x.cdx1 holding ork", kDesignFromArchiveInMemory));
    inputs.push_back(archive("zip dir entry first", kDesignFromArchiveInMemory));
    inputs.push_back(input("zip empty", false, emptyZip(), kUnsupported,
                           Deviation::ARCHIVE_IN_MEMORY, kDiesOfNull));
    inputs.push_back(archive("zip x.ork.bak", kUnsupported));
    inputs.push_back(
        input("gzip of zip", false, gzipOf(zipOf(itemsOf("zip rocket.ork first"))), kUnsupported));
    inputs.push_back(archive("zip holding gzip ork", kUnsupported));
    inputs.push_back(input("bom", false,
                           joined({{std::byte{0xEF}, std::byte{0xBB}, std::byte{0xBF}}, design}),
                           kDesignFromMemory));
    inputs.push_back(input("signature after 300 bytes", false,
                           stringToBytes("<?xml version='1.0'?><!--" + std::string(300, 'x') +
                                         "-->" + std::string(designFromRoot())),
                           kUnsupported));
    // The comment ends with byte 290, so the signature would end with byte 301.
    inputs.push_back(input("signature ends at byte 300?", false,
                           stringToBytes("<?xml version='1.0'?><!--" + std::string(262, 'x') +
                                         "-->" + std::string(designFromRoot())),
                           kUnsupported));
    std::string wide(kDesign);
    wide.replace(wide.find("utf-8"), 5, "utf-16");
    inputs.push_back(input("utf-16", false, utf16Of(wide), kUnsupported));
    inputs.push_back(input(
        "latin1 declared", false,
        stringToBytes("<?xml version='1.0' encoding='ISO-8859-1'?><openrocket version=\"1.10\">"
                      "<rocket><name>caf\xE9</name></rocket></openrocket>"),
        "LOADED 'caf\xC3\xA9' components=1 warnings=[] decals=[]"));
    inputs.push_back(input("9 bytes", false, stringToBytes("<openrock"), kUnsupported));
    inputs.push_back(
        input("truncated gzip", false, firstBytes(gzipOf(design), 40),
              "FAILED PARSE: Exception loading stream: Unexpected end of ZLIB input stream"));
    inputs.push_back(
        input("truncated zip", false, firstBytes(zipOf(itemsOf("zip rocket.ork first")), 60),
              "FAILED PARSE: Exception loading stream: Unexpected end of ZLIB input stream",
              Deviation::ARCHIVE_IN_MEMORY, kDiesOfNull));
    inputs.push_back(input("doctype", false,
                           stringToBytes("<?xml version='1.0'?><!DOCTYPE openrocket [<!ENTITY e "
                                         "'x'>]>" +
                                         std::string(designFromRoot())),
                           "FAILED UNSUPPORTED_FORMAT: Unsupported document type declaration: "
                           "markup declarations",
                           Deviation::DOCUMENT_TYPE, kDesignFromMemory));
    return inputs;
}

/// The archives that are loaded as files (the scouts' probes Sniff.java and Sniff2.java,
/// "file").
[[nodiscard]] std::vector<Input> archiveFiles()
{
    const Bytes design  = stringToBytes(kDesign);
    const auto  archive = [](std::string_view name, std::string_view expected,
                             Deviation        deviation  = Deviation::NONE,
                             std::string_view openRocket = {}) {
        return input(name, true, zipOf(itemsOf(name)), expected, deviation, openRocket);
    };
    std::vector<Input> inputs;
    inputs.push_back(
        input("zip with decals", true,
              zipOf({{"rocket.ork", design}, {"decals/a.png", image()}, {"b.png", image()}}),
              "LOADED 'R' components=6 warnings=[] decals=[b.png=5, decals/a.png=5, "
              "sub/decals/a.png=missing]"));
    inputs.push_back(archive("zip rocket.ork first",
                             "LOADED 'R' components=6 warnings=[] decals=[b.png=missing, "
                             "decals/a.png=5, sub/decals/a.png=missing]"));
    inputs.push_back(
        archive("zip preview first", kDesignFromArchive, Deviation::ARCHIVE_ENTRY, kEmptyRocket));
    inputs.push_back(archive("zip ROCKET.ORK", kDesignFromArchive));
    inputs.push_back(archive("zip dir/x.OrK", kDesignFromArchive));
    inputs.push_back(archive("zip x.rkt holding ork", kDesignFromArchive));
    inputs.push_back(archive("zip x.cdx1 holding ork", kDesignFromArchive));
    inputs.push_back(
        archive("zip dir entry first", kDesignFromArchive, Deviation::ARCHIVE_ENTRY, kEmptyRocket));
    inputs.push_back(input("zip empty", true, emptyZip(), kUnsupported));
    inputs.push_back(
        archive("zip x.ork.bak", kUnsupported, Deviation::ARCHIVE_ENTRY, kEmptyRocket));
    inputs.push_back(archive("zip holding gzip ork", kUnsupported));
    inputs.push_back(archive("zip holding zip ork", kUnsupported));
    inputs.push_back(archive("zip holding short ork", kUnsupported));
    inputs.push_back(archive("zip holding garbage ork", kUnsupported));
    inputs.push_back(
        input("truncated zip", true, firstBytes(zipOf(itemsOf("zip rocket.ork first")), 60),
              "FAILED PARSE: Exception loading stream: Unexpected end of ZLIB input stream"));
    inputs.push_back(input("PK garbage", true, stringToBytes("PK garbage that is not a zip at all"),
                           kUnsupported));
    inputs.push_back(input("corrupt zip data", true,
                           QtRocket::Test::bytesFromHex(kScoutsCorruptArchive), kUnsupported));
    return inputs;
}

/// The plain documents that are loaded as files (the scouts' probes, "file").
[[nodiscard]] std::vector<Input> documentFiles()
{
    const Bytes design = stringToBytes(kDesign);
    const auto  named  = [](std::string_view declaration, std::string_view name) {
        return stringToBytes(std::string(declaration) + std::string(kHead) + std::string(name) +
                             std::string(kTail));
    };
    std::vector<Input> inputs;
    inputs.push_back(input("plain xml next to decals/a.png", true, design, kDesignBesideImage));
    inputs.push_back(input("gzip next to decals/a.png", true, gzipOf(design), kDesignBesideImage));
    inputs.push_back(input("invalid utf-8 no decl", true, named("", "caf\xE9"), kMalformed));
    inputs.push_back(input("invalid utf-8 with utf-8 decl", true,
                           named("<?xml version='1.0' encoding='utf-8'?>", "caf\xE9"), kMalformed));
    inputs.push_back(input("windows-1252 decl", true,
                           named("<?xml version='1.0' encoding='windows-1252'?>", "caf\xE9\x80"),
                           "LOADED 'caf\xC3\xA9\xE2\x82\xAC' components=1 warnings=[] decals=[]"));
    inputs.push_back(input("unknown encoding decl", true,
                           named("<?xml version='1.0' encoding='klingon'?>", "x"),
                           "FAILED IO: I/O error: klingon"));
    inputs.push_back(input("us-ascii decl with high byte", true,
                           named("<?xml version='1.0' encoding='US-ASCII'?>", "c\xE9"),
                           kMalformed));
    // The byte order mark says UTF-8, the declaration Latin-1: the declaration holds, so the
    // two bytes of the e with an acute accent are two characters.
    inputs.push_back(
        input("utf-8 bom + latin1 decl", true,
              joined({{std::byte{0xEF}, std::byte{0xBB}, std::byte{0xBF}},
                      named("<?xml version='1.0' encoding='ISO-8859-1'?>", "caf\xC3\xA9")}),
              "LOADED 'caf\xC3\x83\xC2\xA9' components=1 warnings=[] decals=[]"));
    inputs.push_back(input("utf-16 decl but utf-8 bytes", true,
                           named("<?xml version='1.0' encoding='UTF-16'?>", "x"), kMalformed));
    inputs.push_back(input("xml 1.1", true, named("<?xml version='1.1' encoding='utf-8'?>", "x"),
                           "LOADED 'x' components=1 warnings=[] decals=[]"));
    inputs.push_back(input("leading whitespace before decl", true,
                           named("\n<?xml version='1.0'?>", "x"), kMalformed));
    inputs.push_back(input("leading whitespace no decl", true, named("  \n", "x"),
                           "LOADED 'x' components=1 warnings=[] decals=[]"));
    inputs.push_back(input("namespaced", true,
                           stringToBytes("<or:openrocket xmlns:or='urn:x' version=\"1.10\">"
                                         "<or:rocket><or:name>ns</or:name></or:rocket>"
                                         "</or:openrocket>"),
                           kUnsupported));
    inputs.push_back(
        input("<openrocketx> element", true,
              stringToBytes("<openrocketx version=\"1.10\"><rocket><name>ns</name></rocket>"
                            "</openrocketx>"),
              "LOADED 'Rocket' components=1 warnings=[Unknown element openrocketx, ignoring.] "
              "decals=[]"));
    return inputs;
}

/// The inputs of the scouts' two probes with what OpenRocket made of them, 53 of their 54 (the
/// last is the file that does not exist, which has no bytes: see
/// AFileThatCannotBeReadFailsWithJavasText).
[[nodiscard]] std::vector<Input> scoutInputs()
{
    std::vector<Input> inputs = inputsFromMemory();
    std::ranges::move(archiveFiles(), std::back_inserter(inputs));
    std::ranges::move(documentFiles(), std::back_inserter(inputs));
    return inputs;
}

/// More inputs, of this part's own probe: the edges of the rules.
[[nodiscard]] std::vector<Input> moreInputs()
{
    const Bytes        design = stringToBytes(kDesign);
    std::vector<Input> inputs;
    // The comment ends with byte 289: the signature ends with byte 300.
    inputs.push_back(input("signature ends with byte 300", false,
                           stringToBytes("<?xml version='1.0'?><!--" + std::string(261, 'x') +
                                         "-->" + std::string(designFromRoot())),
                           kDesignFromMemory));
    // Ten bytes are enough to be looked at, and are no document; eleven can be the signature,
    // and are no XML.
    inputs.push_back(input("10 bytes", false, stringToBytes("<openrocke"), kUnsupported));
    inputs.push_back(input("11 bytes", false, stringToBytes("<openrocket"), kMalformed));
    // The signature anywhere in the first bytes will do, in a comment too.
    inputs.push_back(
        input("signature in a comment", false, stringToBytes("<!--<openrocket--><foo/>"),
              "LOADED 'Rocket' components=1 warnings=[Unknown element foo, ignoring.] decals=[]"));
    // Java's matcher starts over at a byte that does not fit and does not try that byte as a
    // first one, so "<<openrocket" is not found.
    inputs.push_back(input("signature behind its own first byte", false,
                           stringToBytes("<!--<<openrocket--><foo/>"), kUnsupported));
    inputs.push_back(input("signature behind a part of itself", false,
                           stringToBytes("<!--<open<openrocket--><foo/>"), kUnsupported));
    // An archive whose stored design is intact loads.
    inputs.push_back(input("stored zip", true,
                           storedZipOf({{"rocket.ork", design}, {"decals/a.png", image()}}),
                           "LOADED 'R' components=6 warnings=[] decals=[b.png=missing, "
                           "decals/a.png=5, sub/decals/a.png=missing]"));
    // The first of two designs is the document.
    inputs.push_back(
        input("zip of two designs", true,
              zipOf({textItem("a.ork", std::string(kHead) + "first" + std::string(kTail)),
                     textItem("b.ork", std::string(kHead) + "second" + std::string(kTail))}),
              "LOADED 'first' components=1 warnings=[] decals=[]"));
    // Java's pattern for the name of the first entry has to match the whole name, and its
    // "." matches no line terminator.
    inputs.push_back(input("zip name with a line feed", true, zipOf({{"a\nb.ork", design}}),
                           kUnsupported, Deviation::ARCHIVE_ENTRY, kEmptyRocket));
    inputs.push_back(input("zip name that goes on behind .ork", true, zipOf({{"x.ork\n", design}}),
                           kUnsupported, Deviation::ARCHIVE_ENTRY, kEmptyRocket));
    inputs.push_back(
        input("zip name .ork alone", true, zipOf({{".ork", design}}), kDesignFromArchive));
    // A later entry is the document only when it is named *.ork: a first entry of another
    // design format is looked at, a later one is not.
    inputs.push_back(input("zip with a later .rkt", true,
                           zipOf({{"preview.png", image()}, {"x.rkt", design}}), kUnsupported,
                           Deviation::ARCHIVE_ENTRY, kEmptyRocket));
    return inputs;
}

/// Damaged gzip streams and archives, of this part's own probe. Each is loaded as a file.
[[nodiscard]] std::vector<Input> damagedInputs()
{
    const Bytes design = stringToBytes(kDesign);
    const Bytes small  = stringToBytes(std::string(kHead) + "x" + std::string(kTail));
    // More than the 8192 bytes Java's reader asks an entry for at once.
    const Bytes large =
        stringToBytes(std::string(kDesign) + "<!--" + std::string(9000, 'x') + "-->\n");
    const Bytes gzip        = gzipOf(design);
    const Bytes smallGzip   = gzipOf(small);
    const Bytes zip         = zipOf({{"rocket.ork", design}});
    const Bytes smallZip    = zipOf({{"rocket.ork", small}});
    const Bytes stored      = storedZipOf({{"rocket.ork", design}});
    const Bytes largeStored = storedZipOf({{"rocket.ork", large}});
    // Where the signature of the root element is in a stored archive of one entry.
    const std::size_t signature =
        30 + std::string_view("rocket.ork").size() + kDesign.find("<openrocket") + 3;
    const auto damaged = [](std::string_view name, Bytes bytes, std::string_view expected,
                            std::string_view openRocket = {}) {
        return input(name, true, std::move(bytes), expected,
                     openRocket.empty() ? Deviation::NONE : Deviation::DAMAGE, openRocket);
    };
    constexpr std::string_view kGzipDamage = "gzip: corrupt or truncated stream (minizip error -3)";
    constexpr std::string_view kGzipTrailer = "FAILED: I/O error: Corrupt GZIP trailer";

    std::vector<Input> inputs;
    // A gzip stream that ends too early. Java's XML parser takes the end of such a stream for
    // the end of the document: cut in its data, the document is incomplete; cut in the check
    // sum behind its data, the document is all there, and loads.
    inputs.push_back(
        damaged("gzip cut in its data", firstBytes(gzip, gzip.size() - 20), kMalformed));
    inputs.push_back(
        damaged("gzip cut in its trailer", firstBytes(gzip, gzip.size() - 4), kDesignBesideImage));
    // Cut before 300 bytes of the document could be unpacked, the stream fails when its first
    // bytes are asked for.
    inputs.push_back(damaged("short gzip cut in its data",
                             firstBytes(smallGzip, smallGzip.size() - 12),
                             "FAILED PARSE: Exception loading stream: Unexpected end of ZLIB input "
                             "stream"));
    // A gzip stream whose check sum or length is wrong.
    inputs.push_back(damaged("gzip with a wrong check sum", withByteFlipped(gzip, gzip.size() - 8),
                             std::format("FAILED PARSE: I/O error: {}", kGzipDamage),
                             kGzipTrailer));
    inputs.push_back(damaged("gzip with a wrong length", withByteFlipped(gzip, gzip.size() - 4),
                             std::format("FAILED PARSE: I/O error: {}", kGzipDamage),
                             kGzipTrailer));
    inputs.push_back(damaged(
        "short gzip with a wrong check sum", withByteFlipped(smallGzip, smallGzip.size() - 8),
        std::format("FAILED PARSE: Exception loading stream: {}", kGzipDamage), kGzipTrailer));
    // An archive whose deflated design has a wrong check sum or size behind it.
    inputs.push_back(damaged("zip with a wrong check sum", withByteFlipped(zip, zip.size() - 12),
                             "FAILED PARSE: I/O error: invalid entry CRC (expected 0x8f0ba04b but "
                             "got 0x8f0ba01e)"));
    inputs.push_back(damaged("zip with a wrong size", withByteFlipped(zip, zip.size() - 4),
                             "FAILED PARSE: I/O error: invalid entry size (expected 570 but got "
                             "623 bytes)"));
    inputs.push_back(damaged("short zip with a wrong check sum",
                             withByteFlipped(smallZip, smallZip.size() - 12),
                             "FAILED PARSE: Exception loading stream: invalid entry CRC (expected "
                             "0xaf16e85b but got 0xaf16e80e)"));
    // An archive whose stored design has a changed byte, so that its check sum is wrong: in
    // its last bytes, and in the signature of its root element. Java refuses the last block
    // of up to 8192 bytes of such an entry, which here is the whole design.
    inputs.push_back(damaged("stored zip with a changed byte",
                             withByteFlipped(stored, stored.size() - 5),
                             "FAILED PARSE: I/O error: invalid entry CRC (expected 0x8f0ba01e but "
                             "got 0x7ff8e0a5)",
                             "FAILED: Exception loading stream: invalid entry CRC (expected "
                             "0x8f0ba01e but got 0x7ff8e0a5)"));
    inputs.push_back(damaged("stored zip with a changed signature",
                             withByteFlipped(stored, signature), kUnsupported,
                             "FAILED: Exception loading stream: invalid entry CRC (expected "
                             "0x8f0ba01e but got 0x7130f1b7)"));
    // In a design of more than 8192 bytes Java has read the first bytes before it meets the
    // wrong check sum, as QtRocket has for a design of any size.
    inputs.push_back(damaged("large stored zip with a changed byte",
                             withByteFlipped(largeStored, largeStored.size() - 5),
                             "FAILED PARSE: I/O error: invalid entry CRC (expected 0x1df9120d but "
                             "got 0xed0a52b6)"));
    inputs.push_back(damaged("large stored zip with a changed signature",
                             withByteFlipped(largeStored, signature), kUnsupported));
    inputs.push_back(damaged("stored zip cut short", firstBytes(stored, stored.size() - 100),
                             "FAILED PARSE: I/O error: unexpected EOF"));
    // An entry that cannot be read on the way to the design, which OpenRocket does not go.
    Bytes unreadable = storedZipOf({{"preview.png", image()}, {"rocket.ork", design}});
    unreadable.at(14) ^= std::byte{0x55};
    inputs.push_back(input("zip with a damaged entry before the design", true, unreadable,
                           "FAILED PARSE: Exception loading stream: invalid entry CRC (expected "
                           "0x470b99a1 but got 0x470b99f4)",
                           Deviation::ARCHIVE_ENTRY, kEmptyRocket));
    // An encrypted first entry.
    Bytes encrypted = storedZipOf({{"rocket.ork", design}});
    encrypted.at(6) |= std::byte{0x01};
    inputs.push_back(damaged("zip with an encrypted design", encrypted,
                             "FAILED PARSE: Exception loading stream: encrypted ZIP entry not "
                             "supported"));
    return inputs;
}

/// The inputs of @p inputs that do not give what they expect, each with its name and both
/// outcomes; empty when every input agrees.
[[nodiscard]] std::string failedInputs(const std::vector<Input>& inputs)
{
    std::string report;
    for (const Input& one : inputs)
    {
        const std::string found = outcomeOfLoading(one.bytes, one.asFile);
        if (found != one.expected)
        {
            report += std::format("{} ({})\n  expected: {}\n  found:    {}\n", one.name,
                                  one.asFile ? "file" : "memory", one.expected, found);
        }
    }
    return report;
}

/// The names of the inputs of @p inputs that QtRocket answers otherwise than OpenRocket for
/// the reason @p deviation, each with " (file)" or " (memory)" behind it.
[[nodiscard]] std::vector<std::string> inputsWith(const std::vector<Input>& inputs,
                                                  Deviation                 deviation)
{
    std::vector<std::string> names;
    for (const Input& one : inputs)
    {
        if (one.deviation == deviation)
        {
            names.push_back(one.name + (one.asFile ? " (file)" : " (memory)"));
        }
    }
    return names;
}

/// What OpenRocket gives for the inputs of @p inputs that QtRocket answers otherwise for the
/// reason @p deviation.
[[nodiscard]] std::vector<std::string> openRocketsOutcomesWith(const std::vector<Input>& inputs,
                                                               Deviation                 deviation)
{
    std::vector<std::string> outcomes;
    for (const Input& one : inputs)
    {
        if (one.deviation == deviation)
        {
            outcomes.push_back(one.openRocket);
        }
    }
    return outcomes;
}

/// @p inputs as the probe Sniff3.java reads them, a line each: the name, "file" or "stream"
/// and the bytes in hexadecimal digits, with tabs between; and, for the script that compares,
/// a line "expect" with what OpenRocket has to give.
[[nodiscard]] std::string printedInputs(const std::vector<Input>& inputs)
{
    std::string text;
    for (const Input& one : inputs)
    {
        std::string hex;
        for (const std::byte b : one.bytes)
        {
            hex += std::format("{:02x}", std::to_integer<unsigned>(b));
        }
        text += std::format("{}\t{}\t{}\n", one.name, one.asFile ? "file" : "stream", hex);
        text += std::format("expect\t{}\n",
                            one.deviation == Deviation::NONE ? one.expected : one.openRocket);
    }
    return text;
}

// Every input of the scouts' probes gives what OpenRocket gives, or what the row says instead.
TEST(GeneralRocketLoaderInputs, EveryInputOfTheScoutsGivesItsOutcome)
{
    const std::vector<Input> inputs = scoutInputs();
    EXPECT_EQ(inputs.size(), 53U);
    EXPECT_EQ(failedInputs(inputs), "");
}

// The edges of the rules: the 300 bytes, the ten bytes, the signature in a comment and Java's
// matcher, a stored archive, the names of the entries.
TEST(GeneralRocketLoaderInputs, TheEdgesOfTheRulesGiveTheirOutcomes)
{
    EXPECT_EQ(failedInputs(moreInputs()), "");
}

// The three outcomes that the rule for the entries of an archive changes (decision T3):
// OpenRocket looks at the first entry only and returns the empty rocket, without a message,
// when that is no design. QtRocket takes the first later entry named *.ork (an archive of
// OpenRocket 13.05 has "decals/" first, and one with a preview image could have that first),
// and refuses an archive without a design.
TEST(GeneralRocketLoaderInputs, TheRuleForArchiveEntriesChangesThreeOutcomesOfTheScouts)
{
    const std::vector<Input> inputs = scoutInputs();
    EXPECT_THAT(inputsWith(inputs, Deviation::ARCHIVE_ENTRY),
                ElementsAre("zip preview first (file)", "zip dir entry first (file)",
                            "zip x.ork.bak (file)"));
    EXPECT_THAT(openRocketsOutcomesWith(inputs, Deviation::ARCHIVE_ENTRY),
                ElementsAre(kEmptyRocket, kEmptyRocket, kEmptyRocket));
}

// An archive in memory: OpenRocket has no attachments for one and dies of a
// NullPointerException before it reads anything (decision T1). QtRocket loads it as it loads
// the file, with the entries as attachments.
TEST(GeneralRocketLoaderInputs, AnArchiveInMemoryIsLoadedWhereOpenRocketDies)
{
    const std::vector<Input> inputs = scoutInputs();
    EXPECT_THAT(
        inputsWith(inputs, Deviation::ARCHIVE_IN_MEMORY),
        ElementsAre("zip rocket.ork first (memory)", "zip preview first (memory)",
                    "zip ROCKET.ORK (memory)", "zip dir/x.OrK (memory)",
                    "zip x.rkt holding ork (memory)", "zip x.cdx1 holding ork (memory)",
                    "zip dir entry first (memory)", "zip empty (memory)", "zip x.ork.bak (memory)",
                    "zip holding gzip ork (memory)", "truncated zip (memory)"));
}

// The one other input of the scouts that QtRocket does not answer as OpenRocket: a document
// type declaration with an entity of its own, which XmlScanner refuses (a deviation that is
// older than the loader).
TEST(GeneralRocketLoaderInputs, ADocumentTypeWithDeclarationsIsTheOneOtherDeviation)
{
    const std::vector<Input> inputs = scoutInputs();
    EXPECT_THAT(inputsWith(inputs, Deviation::DOCUMENT_TYPE), ElementsAre("doctype (memory)"));
    EXPECT_EQ(inputsWith(inputs, Deviation::NONE).size(), 53U - 3U - 11U - 1U);
}

// A gzip stream or an archive that is damaged, each row with what OpenRocket makes of the same
// bytes. QtRocket unpacks a stream whole before it looks at the document, and then decides as
// OpenRocket meets the damage while it reads:
// - damage before 300 bytes of the document could be unpacked is "Exception loading stream:
//   <reason>";
// - a deflate stream that ends too early further on is, to Java's XML parser, the end of the
//   document: "Malformed XML in input.", or a load when the document is complete all the same;
// - other damage further on is "I/O error: <reason>" when the first bytes are a document's,
//   and "Unsupported or corrupt file." when they are not.
// Where OpenRocket meets a damage depends on its buffers, and five rows differ by that (three
// of them also in the reason, which is Java's own there): a wrong check sum or length behind a gzip
// stream is met after the document was read, however short that is ("I/O error: Corrupt GZIP
// trailer"), and a stored entry with a wrong check sum is refused with its last block of up
// to 8192 bytes ("Exception loading stream: invalid entry CRC ..." for a design of that
// size). Each of them fails in both.
TEST(GeneralRocketLoaderInputs, ADamagedStreamIsReadAsFarAsOpenRocketReadsIt)
{
    const std::vector<Input> inputs = damagedInputs();
    EXPECT_EQ(failedInputs(inputs), "");
    EXPECT_THAT(inputsWith(inputs, Deviation::DAMAGE),
                ElementsAre("gzip with a wrong check sum (file)", "gzip with a wrong length (file)",
                            "short gzip with a wrong check sum (file)",
                            "stored zip with a changed byte (file)",
                            "stored zip with a changed signature (file)"));
}

// For the probe Sniff3.java: the inputs, and what OpenRocket has to make of each.
TEST(GeneralRocketLoaderInputs, DISABLED_PrintsTheInputs)
{
    std::cout << printedInputs(scoutInputs()) << printedInputs(moreInputs())
              << printedInputs(damagedInputs());
}

// ------------------------------------------------------------------- the cases of the probe

// Every document of the probe gives OpenRocket's warnings, OpenRocket's failure and the state
// OpenRocket's document has after the load: the rocket and its flight configurations with
// the stage activeness applied, the simulations with their status, the storage options, the
// saved and undo state, the modification ids, the document materials and the number of events
// the document emitted (less those of a simulation that is not in the document's list yet:
// see file/RocketLoaderCases.h).
TEST(GeneralRocketLoader, GivesWhatOpenRocketGivesForEveryCaseOfTheProbe)
{
    EXPECT_EQ(failedLoaderCases(kLoadCases, loadLinesOf), "");
}

// The cases with an expectation of QtRocket's own:
// - the three in which an element stands before <rocket>: OpenRocket has no document yet and
//   dies of a NullPointerException; here the document exists from the start;
// - the extension of an unknown id, which is kept (decisions D11 and L10) where OpenRocket
//   drops it;
// - the simulation of a new flight configuration: OpenRocket's document hears it change twice
//   before it is in the list (see OpenRocketDocument: a simulation is heard while it is in
//   the list), which is one event more than the table takes off for every simulation.
TEST(GeneralRocketLoader, FewCasesOfTheProbeAreAnsweredOtherwiseThanOpenRocket)
{
    EXPECT_THAT(kOwnLoadCases,
                ElementsAre("c-docprefs-first", "c-photostudio-first", "c-simulations-first",
                            "p-ext-unknown", "p-sim-new-configuration"));
}

// For the script that compares with the probe's output: the cases as the probe prints them.
TEST(GeneralRocketLoader, DISABLED_PrintsTheCasesOfTheProbe)
{
    std::cout << printedLoaderCases(kLoadCases, 'L', loadLinesOf);
}

// ----------------------------------------------------------------------- the environment

// What the handlers ask of the environment whenever a file has the element for it is checked
// when the loader is made, not when a file happens to have the element.
TEST(GeneralRocketLoader, TheEnvironmentMustHaveAMotorFinderAndAPreferenceStore)
{
    LoaderEnvironment      environment;
    DocumentLoadingContext complete = environment.context();
    EXPECT_NO_THROW(static_cast<void>(GeneralRocketLoader(complete)));

    DocumentLoadingContext noFinder = complete;
    noFinder.setMotorFinder(nullptr);
    EXPECT_THROW(static_cast<void>(GeneralRocketLoader(noFinder)), BugError);

    DocumentLoadingContext noPreferences = complete;
    noPreferences.setPreferences(nullptr);
    EXPECT_THROW(static_cast<void>(GeneralRocketLoader(noPreferences)), BugError);

    // The others may be missing (DocumentLoadingContext says what a load makes of that).
    DocumentLoadingContext bare = complete;
    bare.setApplicationMaterials(nullptr);
    bare.setComponentPresetDatabase(nullptr);
    bare.setSimulationExtensionRegistry(nullptr);
    const GeneralRocketLoader    loader(bare);
    const Result<LoadedDocument> loaded = loader.load(stringToBytes(kDesign));
    ASSERT_TRUE(loaded.has_value()) << loaded.error().message;
    EXPECT_EQ(loaded->document->getRocket().getName(), "R");
}

/// What the handlers of a load were given: the context of the load, as the loader set it up.
struct SeenContext
{
    OpenRocketDocument*                  document{nullptr};
    int                                  fileVersion{-1};
    std::optional<std::filesystem::path> designDirectory;
    /// "files" with the base directory of the factory, or "archive".
    std::string attachments;
    bool        called{false};
};

/// Where @p factory takes its attachments from: "files " and its base directory or "without a
/// directory", or "archive".
[[nodiscard]] std::string attachmentsOf(const QtRocket::AttachmentFactory& factory)
{
    if (const auto* const files =
            dynamic_cast<const QtRocket::FileSystemAttachmentFactory*>(&factory))
    {
        const std::optional<std::filesystem::path>& base = files->getBaseDirectory();
        return base.has_value() ? "files " + QtRocket::pathToUtf8(*base)
                                : std::string("files without a directory");
    }
    if (dynamic_cast<const QtRocket::ZipFileAttachmentFactory*>(&factory) != nullptr)
    {
        return "archive";
    }
    return "other";
}

/// Options that record the context of a load in @p seen.
[[nodiscard]] GeneralRocketLoader::Options seeingOptions(const std::shared_ptr<SeenContext>& seen)
{
    GeneralRocketLoader::Options options;
    options.beforeReading = [seen](const DocumentLoadingContext& context) {
        seen->called          = true;
        seen->document        = context.getOpenRocketDocument();
        seen->fileVersion     = context.getFileVersion();
        seen->designDirectory = context.getDesignDirectory();
        seen->attachments     = attachmentsOf(*context.getAttachmentFactory());
    };
    return options;
}

// The loader works on a copy of the context it was given: the document, the file version,
// the attachment factory and the design's directory of a load are the loader's own, whatever
// the caller set, and the caller's context stays as it was.
TEST(GeneralRocketLoader, SetsUpAContextOfItsOwnForEveryLoad)
{
    LoaderEnvironment                         environment;
    const std::unique_ptr<OpenRocketDocument> other =
        QtRocket::OpenRocketDocumentFactory::createEmptyRocket();
    const QtRocket::Test::MapAttachmentFactory othersAttachments;
    environment.context().setOpenRocketDocument(other.get());
    environment.context().setFileVersion(42);
    environment.context().setAttachmentFactory(&othersAttachments);
    environment.context().setDesignDirectory(std::filesystem::path("elsewhere"));
    const std::shared_ptr<SeenContext> seen = std::make_shared<SeenContext>();
    const GeneralRocketLoader          loader(environment.context(), seeingOptions(seen));

    const Result<LoadedDocument> loaded = loader.load(stringToBytes(kDesign));

    ASSERT_TRUE(loaded.has_value()) << loaded.error().message;
    ASSERT_TRUE(seen->called);
    EXPECT_EQ(seen->document, loaded->document.get());
    EXPECT_NE(seen->document, other.get());
    EXPECT_EQ(seen->fileVersion, 0);
    EXPECT_EQ(seen->designDirectory, std::nullopt);
    EXPECT_EQ(seen->attachments, "files without a directory");
    EXPECT_THAT(othersAttachments.asked(), IsEmpty());
    // The caller's document and context were not touched.
    EXPECT_EQ(other->getRocket().getName(), "Rocket");
    EXPECT_EQ(environment.context().getOpenRocketDocument(), other.get());
    EXPECT_EQ(environment.context().getFileVersion(), 42);
    EXPECT_EQ(environment.context().getAttachmentFactory(), &othersAttachments);
    EXPECT_EQ(environment.context().getDesignDirectory(), std::filesystem::path("elsewhere"));
}

// Where the attachments come from and which directory is the design's (the directory a file
// that the design names will be confined to): for a file its own directory, "." when it is
// named without one; for bytes the base directory, when one is given; and for an archive its
// entries, whatever the directory.
TEST(GeneralRocketLoader, ChoosesTheAttachmentsAndTheDesignsDirectoryByWhereTheDesignComesFrom)
{
    const DesignDirectory              directory;
    LoaderEnvironment                  environment;
    const std::shared_ptr<SeenContext> seen = std::make_shared<SeenContext>();
    const GeneralRocketLoader          loader(environment.context(), seeingOptions(seen));
    const Bytes                        design  = stringToBytes(kDesign);
    const Bytes                        archive = zipOf({{"rocket.ork", design}});
    // The directory as the loader spells it (as Java's File does).
    const std::filesystem::path home  = QtRocket::withoutRedundantSeparators(directory.path());
    const std::string           where = QtRocket::pathToUtf8(home);

    // A file: plain, gzip and an archive.
    ASSERT_TRUE(loader.load(directory.write("plain.ork", design)).has_value());
    EXPECT_EQ(seen->attachments, "files " + where);
    EXPECT_EQ(seen->designDirectory, home);
    ASSERT_TRUE(loader.load(directory.write("gzip.ork", gzipOf(design))).has_value());
    EXPECT_EQ(seen->attachments, "files " + where);
    EXPECT_EQ(seen->designDirectory, home);
    ASSERT_TRUE(loader.load(directory.write("archive.ork", archive)).has_value());
    EXPECT_EQ(seen->attachments, "archive");
    EXPECT_EQ(seen->designDirectory, home);

    // Bytes with a base directory, and without.
    ASSERT_TRUE(loader.load(design, home).has_value());
    EXPECT_EQ(seen->attachments, "files " + where);
    EXPECT_EQ(seen->designDirectory, home);
    ASSERT_TRUE(loader.load(archive, home).has_value());
    EXPECT_EQ(seen->attachments, "archive");
    EXPECT_EQ(seen->designDirectory, home);
    ASSERT_TRUE(loader.load(gzipOf(design)).has_value());
    EXPECT_EQ(seen->attachments, "files without a directory");
    EXPECT_EQ(seen->designDirectory, std::nullopt);
    ASSERT_TRUE(loader.load(archive).has_value());
    EXPECT_EQ(seen->attachments, "archive");
    EXPECT_EQ(seen->designDirectory, std::nullopt);

    // A file that is named without a directory is one of the current directory: its
    // attachments are named against that (Java: a File without a parent), and the design's
    // directory is ".".
    {
        const CurrentDirectoryGuard inside(directory.path());
        ASSERT_TRUE(loader.load(std::filesystem::path("plain.ork")).has_value());
        EXPECT_EQ(seen->attachments, "files without a directory");
        EXPECT_EQ(seen->designDirectory, std::filesystem::path("."));
    }
}

// --------------------------------------------------------------------------- attachments

/// The bytes of the decal image named @p name of @p document as a text of their values
/// ("1 2 3 4 5"), or why they cannot be read.
[[nodiscard]] std::string decalBytes(const OpenRocketDocument& document, std::string_view name)
{
    const std::shared_ptr<QtRocket::DecalImage> decal = document.findDecalImage(name);
    if (decal == nullptr)
    {
        return "no such image";
    }
    const Result<Bytes> bytes = decal->getBytes();
    if (!bytes)
    {
        return std::string(toString(bytes.error().code));
    }
    std::string text;
    for (const std::byte b : *bytes)
    {
        text += (text.empty() ? "" : " ") + std::to_string(std::to_integer<int>(b));
    }
    return text;
}

// An archive in memory: the images are its entries, found by their exact names, and they can
// be read when the loader and the bytes it was given are gone (the document shares the
// archive).
TEST(GeneralRocketLoader, TheAttachmentsOfAnArchiveInMemoryAreItsEntries)
{
    LoaderEnvironment            environment;
    const Result<LoadedDocument> loaded = [&environment] {
        const GeneralRocketLoader loader(environment.context());
        Bytes                     archive = zipOf({{"rocket.ork", stringToBytes(kDesign)},
                                                   {"decals/a.png", image()},
                                                   {"/b.png", image()},
                                                   {"B.PNG", image()}});
        return loader.load(std::move(archive));
    }();

    ASSERT_TRUE(loaded.has_value()) << loaded.error().message;
    const OpenRocketDocument& document = *loaded->document;
    EXPECT_EQ(decalBytes(document, "decals/a.png"), "1 2 3 4 5");
    // "b.png" is neither "/b.png" nor "B.PNG".
    EXPECT_EQ(decalBytes(document, "b.png"), "NOT_FOUND");
    EXPECT_EQ(decalBytes(document, "sub/decals/a.png"), "NOT_FOUND");
}

// A plain design in memory with a base directory: the images are the files of that directory.
TEST(GeneralRocketLoader, TheAttachmentsOfBytesWithABaseDirectoryAreItsFiles)
{
    const DesignDirectory     directory;
    LoaderEnvironment         environment;
    const GeneralRocketLoader loader(environment.context());

    const Result<LoadedDocument> loaded = loader.load(stringToBytes(kDesign), directory.path());

    ASSERT_TRUE(loaded.has_value()) << loaded.error().message;
    EXPECT_EQ(outcomeOf(loaded, true), kDesignBesideImage);
    EXPECT_EQ(decalBytes(*loaded->document, "decals/a.png"), "1 2 3 4 5");
}

// A plain design in memory without a base directory: the images are files named against the
// current directory of the process, as Java's File names them.
TEST(GeneralRocketLoader, TheAttachmentsOfBytesWithoutADirectoryAreNamedAgainstTheCurrentOne)
{
    const DesignDirectory       directory;
    LoaderEnvironment           environment;
    const GeneralRocketLoader   loader(environment.context());
    const CurrentDirectoryGuard inside(directory.path());

    const Result<LoadedDocument> loaded = loader.load(stringToBytes(kDesign));

    ASSERT_TRUE(loaded.has_value()) << loaded.error().message;
    EXPECT_EQ(outcomeOf(loaded, true), kDesignBesideImage);
}

// ------------------------------------------------------------------------ the two forms

/// The state of a loaded document: the lines of the probe (documentLines()) and everything
/// run 9b's description of a rocket reads, the ids as "random" (they are the file's or drawn).
[[nodiscard]] std::string stateOf(const Result<LoadedDocument>& loaded,
                                  LoaderEnvironment&            environment)
{
    if (!loaded)
    {
        return QtRocket::Test::failureLine(loaded.error());
    }
    std::string state = QtRocket::Test::warningLines(loaded->warnings) +
                        QtRocket::Test::documentLines(*loaded->document, nullptr);
    for (const std::string& line : QtRocket::Test::describeRocket(
             loaded->document->getRocket(), environment.preferences(), std::set<QtRocket::Uuid>{}))
    {
        state += line + "\n";
    }
    return state;
}

/// Loads each file of @p files through both forms, the bytes with the file's directory as
/// their base, and reports where the two differ, where a load fails and where the rocket has
/// no component; empty when every file gives the same document twice.
[[nodiscard]] std::string differencesBetweenTheForms(
    const std::vector<std::filesystem::path>& files)
{
    std::string report;
    for (const std::filesystem::path& file : files)
    {
        LoaderEnvironment            environment;
        const GeneralRocketLoader    loader(environment.context());
        const std::string            name     = QtRocket::pathToUtf8(file.filename());
        const Result<Bytes>          bytes    = QtRocket::readFile(file);
        const Result<LoadedDocument> fromFile = loader.load(file);
        if (!bytes || !fromFile)
        {
            report +=
                name + ": " + (bytes ? fromFile.error().message : bytes.error().message) + "\n";
            continue;
        }
        const Result<LoadedDocument> fromBytes = loader.load(*bytes, file.parent_path());
        if (fromFile->document->getRocket().getChildCount() == 0)
        {
            report += name + ": the rocket is empty\n";
        }
        if (stateOf(fromFile, environment) != stateOf(fromBytes, environment) ||
            outcomeOf(fromFile, true) != outcomeOf(fromBytes, true))
        {
            report += name + ": the two forms differ\n";
        }
    }
    return report;
}

// The same bytes give the same document whether they are read from a file or handed over,
// with the file's directory as their base: a plain design, a gzip stream and an archive.
TEST(GeneralRocketLoader, BothFormsGiveTheSameDocumentForTheSameBytes)
{
    const QtRocket::Test::DefaultUnitsGuard units;
    const DesignDirectory                   directory;
    const Bytes                             design = stringToBytes(kDesign);

    EXPECT_EQ(
        differencesBetweenTheForms(
            {directory.write("plain.ork", design), directory.write("gzip.ork", gzipOf(design)),
             directory.write("archive.ork",
                             zipOf({{"rocket.ork", design}, {"decals/a.png", image()}}))}),
        "");
    // The helper tells a difference: the same design with another name is another document.
    LoaderEnvironment            environment;
    const GeneralRocketLoader    loader(environment.context());
    const Result<LoadedDocument> one = loader.load(design);
    const Result<LoadedDocument> other =
        loader.load(stringToBytes(std::string(kHead) + "x" + std::string(kTail)));
    EXPECT_NE(stateOf(one, environment), stateOf(other, environment));
    EXPECT_THAT(stateOf(one, environment), HasSubstr("rocket 'R' stages=1 components=6"));
}

// The exit check of this part: a gzip, a plain and a zip file of tests/data/ork and an example
// of data/examples load through both forms without an Error, to the same document. (Their
// motors are not found here: the finder of these tests knows none. The tests of the recorded
// files and of the examples compare what is loaded.)
TEST(GeneralRocketLoader, RecordedFilesAndAnExampleLoadThroughBothForms)
{
    const QtRocket::Test::DefaultUnitsGuard units;
    const std::filesystem::path             recorded = QtRocket::Test::testDataDir() / "ork";

    EXPECT_EQ(differencesBetweenTheForms(
                  {recorded / "simplerocket.ork", recorded / "v1.0-roll-stabilized.ork",
                   recorded / "v1.6-a-simple-model-rocket.ork",
                   QtRocket::Test::dataDir() / "examples" / "A simple model rocket.ork"}),
              "");
}

// -------------------------------------------------------------------------------- failures

// A file that cannot be opened: "Exception loading file: <file> , <reason>", the file as
// Java's File spells it. The reason is QtRocket's own text (Java: the operating system's) and
// is not pinned.
TEST(GeneralRocketLoader, AFileThatCannotBeReadFailsWithJavasText)
{
    const TempDir             directory;
    LoaderEnvironment         environment;
    const GeneralRocketLoader loader(environment.context());
    const std::string         missing =
        QtRocket::pathToUtf8(QtRocket::withoutRedundantSeparators(directory.resolve("nope.ork")));

    // The scouts' input "missing file".
    const Result<LoadedDocument> none = loader.load(directory.resolve("nope.ork"));
    ASSERT_FALSE(none.has_value());
    EXPECT_EQ(none.error().code, ErrorCode::IO);
    EXPECT_THAT(none.error().message, StartsWith("Exception loading file: " + missing + " , "));
    EXPECT_GT(none.error().message.size(), missing.size() + 27);

    // A doubled separator is not part of the name.
    const Result<LoadedDocument> doubled =
        loader.load(QtRocket::pathFromUtf8(QtRocket::pathToUtf8(directory.path()) + "//nope.ork"));
    ASSERT_FALSE(doubled.has_value());
    EXPECT_THAT(doubled.error().message, StartsWith("Exception loading file: " + missing + " , "));

    // A directory is no file.
    const Result<LoadedDocument> folder = loader.load(directory.path());
    ASSERT_FALSE(folder.has_value());
    EXPECT_EQ(folder.error().code, ErrorCode::IO);
    EXPECT_THAT(
        folder.error().message,
        StartsWith("Exception loading file: " +
                   QtRocket::pathToUtf8(QtRocket::withoutRedundantSeparators(directory.path())) +
                   " , "));
}

/// What @p loader makes of @p document: "loaded", or the failure as "<code>: <message>".
[[nodiscard]] std::string failureOf(const GeneralRocketLoader& loader, std::string_view document)
{
    const Result<LoadedDocument> loaded = loader.load(stringToBytes(document));
    return loaded.has_value()
               ? std::string("loaded")
               : std::format("{}: {}", toString(loaded.error().code), loaded.error().message);
}

// A failure of a handler or setter is the message of Java's exception behind "Exception
// loading stream: ", deep inside a component and inside a simulation alike; an exception
// without a message is a failure all the same, its text the prefix alone. (OpenRocket's texts
// for the three: the cases f-id-deep, f-sim-no-conditions and f-id-empty-group of the probe.)
TEST(GeneralRocketLoader, AHandlersFailureArrivesBehindJavasPrefix)
{
    LoaderEnvironment         environment;
    const GeneralRocketLoader loader(environment.context());

    EXPECT_EQ(
        failureOf(loader,
                  R"(<openrocket version="1.10"><rocket><subcomponents><stage><subcomponents>)"
                  R"(<bodytube><subcomponents><innertube><id>not-a-uuid</id></innertube>)"
                  R"(</subcomponents></bodytube></subcomponents></stage></subcomponents>)"
                  R"(</rocket></openrocket>)"),
        "INVALID_ARGUMENT: Exception loading stream: Invalid UUID string: not-a-uuid");
    EXPECT_EQ(failureOf(loader, R"(<openrocket version="1.10"><rocket/><simulations>)"
                                R"(<simulation status="uptodate"><name>S</name></simulation>)"
                                R"(</simulations></openrocket>)"),
              "INVALID_ARGUMENT: Exception loading stream: Attempted to set the configuration to "
              "an error id. Not Allowed!");
    EXPECT_EQ(failureOf(loader, R"(<openrocket version="1.10"><rocket><id>1--3-4-5</id></rocket>)"
                                R"(</openrocket>)"),
              "INVALID_ARGUMENT: Exception loading stream: ");
    // The root handler's own failure, the version that no int holds, is one of them.
    EXPECT_EQ(failureOf(loader, R"(<openrocket version="99999999999.1"><rocket/></openrocket>)"),
              "INVALID_ARGUMENT: Exception loading stream: For input string: \"99999999999\"");
}

// The texts of the loader's own failures, each with its code.
TEST(GeneralRocketLoader, TheLoadersOwnFailuresHaveJavasTexts)
{
    LoaderEnvironment         environment;
    const GeneralRocketLoader loader(environment.context());

    EXPECT_EQ(failureOf(loader, ""), "UNSUPPORTED_FORMAT: Unsupported or corrupt file.");
    EXPECT_EQ(failureOf(loader, "no design at all"),
              "UNSUPPORTED_FORMAT: Unsupported or corrupt file.");
    EXPECT_EQ(failureOf(loader, R"(<openrocket version="1.10"><rocket></openrocket>)"),
              "PARSE: Malformed XML in input.");
    EXPECT_EQ(
        failureOf(loader, "<?xml version='1.0' encoding='klingon'?><openrocket version=\"1.10\"/>"),
        "IO: I/O error: klingon");
    // A failure of a handler comes before a later error of the XML, and an earlier error of
    // the XML before a failure of a handler (the cases f-handler-before-malformed and
    // f-malformed-before-handler of the probe).
    EXPECT_EQ(failureOf(loader, R"(<openrocket version="1.10"><rocket><id>x</id></rocket><open>)"
                                R"(</openrocket>)"),
              "INVALID_ARGUMENT: Exception loading stream: Invalid UUID string: x");
    EXPECT_EQ(
        failureOf(loader, R"(<openrocket version="1.10"><rocket><name>R</rocket><rocket><id>x</id>)"
                          R"(</rocket></openrocket>)"),
        "PARSE: Malformed XML in input.");
}

// A design of RockSim or RASAero is recognised by its first eleven bytes, as in OpenRocket,
// which reads it; QtRocket does not, and says which program's file it is.
TEST(GeneralRocketLoader, ARockSimOrRasAeroDesignIsRecognisedAndRefused)
{
    LoaderEnvironment         environment;
    const GeneralRocketLoader loader(environment.context());

    const Result<LoadedDocument> rockSim =
        loader.load(stringToBytes("<RockSimDocument><DesignInformation/></RockSimDocument>"));
    ASSERT_FALSE(rockSim.has_value());
    EXPECT_EQ(rockSim.error().code, ErrorCode::UNSUPPORTED_FORMAT);
    EXPECT_EQ(rockSim.error().message, "RockSim design files are not supported.");

    const Result<LoadedDocument> rasAero = loader.load(
        stringToBytes("<RASAeroDocument><FileVersion>2</FileVersion></RASAeroDocument>"));
    ASSERT_FALSE(rasAero.has_value());
    EXPECT_EQ(rasAero.error().code, ErrorCode::UNSUPPORTED_FORMAT);
    EXPECT_EQ(rasAero.error().message, "RASAero design files are not supported.");

    // The signature has to be the very start of the document: behind an XML declaration, as
    // RockSim writes its files, OpenRocket does not recognise it either.
    const Result<LoadedDocument> declared =
        loader.load(stringToBytes("<?xml version='1.0'?><RockSimDocument></RockSimDocument>"));
    ASSERT_FALSE(declared.has_value());
    EXPECT_EQ(declared.error().message, "Unsupported or corrupt file.");
    // In an archive the entry named *.rkt is looked at.
    const Result<LoadedDocument> archived = loader.load(
        zipOf({textItem("design.rkt", "<RockSimDocument><DesignInformation/></RockSimDocument>")}));
    ASSERT_FALSE(archived.has_value());
    EXPECT_EQ(archived.error().message, "RockSim design files are not supported.");
}

// A failed load gives no document, and what the loader was building is gone: the next load of
// the same loader starts from an empty document.
TEST(GeneralRocketLoader, AFailedLoadLeavesNothingToTheNextOne)
{
    LoaderEnvironment         environment;
    const GeneralRocketLoader loader(environment.context());

    const Result<LoadedDocument> failed = loader.load(stringToBytes(
        R"(<openrocket version="1.10"><rocket><name>Half</name><subcomponents><stage>)"
        R"(<name>S</name><id>x</id></stage></subcomponents></rocket></openrocket>)"));
    ASSERT_FALSE(failed.has_value());

    const Result<LoadedDocument> next =
        loader.load(stringToBytes(R"(<openrocket version="1.10"></openrocket>)"));
    ASSERT_TRUE(next.has_value());
    EXPECT_EQ(next->document->getRocket().getName(), "Rocket");
    EXPECT_EQ(next->document->getRocket().getChildCount(), 0U);
    EXPECT_THAT(QtRocket::Test::warningTexts(next->warnings), IsEmpty());
}

/// What loading @p document throws, or "" when it throws nothing: a document or an Error is
/// as good as the other.
[[nodiscard]] std::string whatLoadingThrows(std::string_view document)
{
    try
    {
        LoaderEnvironment environment;
        environment.findEveryMotor();
        const GeneralRocketLoader loader(environment.context());
        static_cast<void>(loader.load(stringToBytes(document)).has_value());
    }
    catch (const std::exception& thrown)
    {
        return thrown.what();
    }
    return "";
}

/// The cases of @p names (of kLoadCases) whose document makes the loader throw when it is cut
/// off behind one of its '>', each with the cut-off text and what was thrown; empty when none
/// does. A document that ends too early is one a file can hold: the handlers have read a part
/// of it when the reader finds that out, and a damaged gzip stream is read like that too.
[[nodiscard]] std::string casesThatThrowWhenCutOff(std::initializer_list<std::string_view> names)
{
    std::string report;
    for (const std::string_view name : names)
    {
        const std::string_view document = QtRocket::Test::documentOfLoadCase(name);
        for (std::size_t end = document.find('>'); end != std::string_view::npos;
             end             = document.find('>', end + 1))
        {
            const std::string_view cut   = document.substr(0, end + 1);
            const std::string      wrong = whatLoadingThrows(cut);
            if (!wrong.empty())
            {
                report += std::format("case {}, cut off as\n{}\n{}\n", name, cut, wrong);
                break;
            }
        }
    }
    return report;
}

// Nothing a file holds makes the loader throw (there is no catch-all in it that would hide a
// missing guard): the documents of four cases of the probe, with stages, motors, materials,
// simulations with stored data and extensions, each cut off at every place.
TEST(GeneralRocketLoader, NoCutOffDocumentMakesTheLoaderThrow)
{
    EXPECT_EQ(casesThatThrowWhenCutOff(
                  {"c-all-five", "p-stage-activeness", "p-materials", "m-ext-script-enabled"}),
              "");
}

// ---------------------------------------------------------------------------------- bounds

/// A document of @p size bytes: a rocket and a comment that fills the rest.
[[nodiscard]] Bytes documentOfSize(std::size_t size)
{
    const std::string start = std::string(kHead) + "big" + std::string(kTail) + "<!--";
    return stringToBytes(start + std::string(size - start.size() - 3, 'x') + "-->");
}

/// What a loader whose document bound is @p maxDocumentBytes makes of @p bytes.
[[nodiscard]] std::string boundedOutcome(const Bytes& bytes, std::size_t maxDocumentBytes)
{
    LoaderEnvironment            environment;
    GeneralRocketLoader::Options options;
    options.maxDocumentBytes = maxDocumentBytes;
    const GeneralRocketLoader loader(environment.context(), options);
    return outcomeOf(loader.load(bytes), false);
}

constexpr std::string_view kBigLoaded = "LOADED 'big' components=1 warnings=[] decals=[]";
constexpr std::string_view kBeyond1000 =
    "FAILED IO: Exception loading stream: Input exceeds maximum size of 1000 bytes";

// The bounds are what the header says.
TEST(GeneralRocketLoader, TheBoundsAreAQuarterOfAGibibyteForTheDocumentAndOneForTheFile)
{
    EXPECT_EQ(GeneralRocketLoader::kMaxDocumentBytes, 268435456U);
    EXPECT_EQ(GeneralRocketLoader::kMaxFileBytes, 1073741824U);
    EXPECT_EQ(GeneralRocketLoader::Options{}.maxDocumentBytes, 268435456U);
    EXPECT_EQ(GeneralRocketLoader::Options{}.maxFileBytes, 1073741824U);
}

// A plain document of the bound loads; one byte more is refused.
TEST(GeneralRocketLoader, APlainDocumentBeyondTheBoundIsRefused)
{
    EXPECT_EQ(boundedOutcome(documentOfSize(1000), 1000), kBigLoaded);
    EXPECT_EQ(boundedOutcome(documentOfSize(1001), 1000), kBeyond1000);
}

// A gzip stream delivers what it holds, whatever the length at its end says: the unpacking
// stops at the bound.
TEST(GeneralRocketLoader, AGzipStreamIsGivenUpAtTheBound)
{
    EXPECT_EQ(boundedOutcome(gzipOf(documentOfSize(1000)), 1000), kBigLoaded);
    EXPECT_EQ(boundedOutcome(gzipOf(documentOfSize(1001)), 1000), kBeyond1000);
    // A megabyte, which is 1000 bytes of gzip: nothing near a megabyte is unpacked.
    EXPECT_EQ(boundedOutcome(gzipOf(documentOfSize(1000000)), 1000), kBeyond1000);

    // The length at the end of the stream is the stream's word: a stream that says it holds
    // little and delivers more is given up at the bound all the same, ...
    Bytes understated                      = gzipOf(documentOfSize(5000));
    understated.at(understated.size() - 4) = std::byte{100};
    understated.at(understated.size() - 3) = std::byte{0};
    EXPECT_EQ(boundedOutcome(understated, 1000), kBeyond1000);
    // ... and one that says it holds more than the bound and delivers less is not refused for
    // what it says: it is unpacked, and then is a stream with a wrong length.
    Bytes overstated                     = gzipOf(documentOfSize(900));
    overstated.at(overstated.size() - 1) = std::byte{0x7F};
    EXPECT_THAT(boundedOutcome(overstated, 1000), StartsWith("FAILED PARSE: I/O error: gzip: "));
}

// An archive entry that says in its header that it holds more than the bound is refused
// before it is read; one that does not say, or says less than it delivers, is given up at
// the bound.
TEST(GeneralRocketLoader, AnArchiveEntryBeyondTheBoundIsRefused)
{
    // The header says: stored, with the size in the header.
    EXPECT_EQ(boundedOutcome(storedZipOf({{"rocket.ork", documentOfSize(1000)}}), 1000),
              kBigLoaded);
    EXPECT_EQ(boundedOutcome(storedZipOf({{"rocket.ork", documentOfSize(1001)}}), 1000),
              kBeyond1000);
    // The header says more than there is: refused for what it says, nothing being read (the
    // entry is cut off after its first bytes, and no failure of a cut-off entry is reported).
    const Bytes stated = storedZipOf({{"rocket.ork", documentOfSize(2000)}});
    EXPECT_EQ(boundedOutcome(firstBytes(stated, 400), 1000), kBeyond1000);

    // The header does not say: deflated, with the sizes behind the data.
    EXPECT_EQ(boundedOutcome(zipOf({{"rocket.ork", documentOfSize(1000)}}), 1000), kBigLoaded);
    EXPECT_EQ(boundedOutcome(zipOf({{"rocket.ork", documentOfSize(1001)}}), 1000), kBeyond1000);
    EXPECT_EQ(boundedOutcome(zipOf({{"rocket.ork", documentOfSize(1000000)}}), 1000), kBeyond1000);

    // The bound is the document's, not the archive's: attachments do not count.
    EXPECT_EQ(boundedOutcome(storedZipOf({{"rocket.ork", documentOfSize(1000)},
                                          {"decals/big.png", documentOfSize(5000)}}),
                             1000),
              kBigLoaded);
}

// A file beyond the file bound is not read.
TEST(GeneralRocketLoader, AFileBeyondTheFileBoundIsRefused)
{
    const DesignDirectory        directory;
    LoaderEnvironment            environment;
    GeneralRocketLoader::Options options;
    options.maxFileBytes = 1000;
    const GeneralRocketLoader loader(environment.context(), options);

    EXPECT_EQ(outcomeOf(loader.load(directory.write("fits.ork", documentOfSize(1000))), false),
              kBigLoaded);
    const std::filesystem::path  large   = directory.write("large.ork", documentOfSize(1001));
    const Result<LoadedDocument> refused = loader.load(large);
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error().code, ErrorCode::IO);
    EXPECT_EQ(refused.error().message,
              "Exception loading file: " +
                  QtRocket::pathToUtf8(QtRocket::withoutRedundantSeparators(large)) +
                  " , Input exceeds maximum size of 1000 bytes");
}

// --------------------------------------------------------------------------------- lifetime

/// A design with a motor, a simulation with stored data and a decal image.
constexpr std::string_view kWholeDesign =
    R"(<openrocket version="1.10"><rocket><name>Whole</name>)"
    R"(<motorconfiguration configid="da326836-0959-4c94-bcd5-49dee07235a4" default="true">)"
    R"(<stage number="0" active="true"/></motorconfiguration>)"
    R"(<subcomponents><stage><name>S0</name><subcomponents>)"
    R"(<bodytube><name>T0</name><appearance><decal name="decals/a.png" rotation="0" )"
    R"(edgemode="REPEAT"/></appearance><length>0.1</length><radius>0.02</radius>)"
    R"(<motormount><motor configid="da326836-0959-4c94-bcd5-49dee07235a4">)"
    R"(<manufacturer>Estes</manufacturer><designation>A8</designation><diameter>0.024</diameter>)"
    R"(<length>0.07</length><delay>3.0</delay></motor><ignitionevent>automatic</ignitionevent>)"
    R"(<ignitiondelay>0.0</ignitiondelay><overhang>0.0</overhang></motormount>)"
    R"(</bodytube></subcomponents></stage></subcomponents></rocket>)"
    R"(<simulations><simulation status="uptodate"><name>Flight</name>)"
    R"(<conditions><configid>da326836-0959-4c94-bcd5-49dee07235a4</configid>)"
    R"(<timestep>0.05</timestep></conditions>)"
    R"(<extension extensionid="info.openrocket.core.simulation.extension.example.AirStart">)"
    R"(<entry key="launchAltitude" type="number">100.0</entry></extension>)"
    R"(<flightdata maxaltitude="47.272" maxvelocity="27.47">)"
    R"(<databranch name="b" types="Time,Altitude"><datapoint>0.0,0.0</datapoint>)"
    R"(<datapoint>0.05,1.0</datapoint></databranch></flightdata></simulation></simulations>)"
    R"(</openrocket>)";

/// Loads kWholeDesign from an archive in memory with an environment that is made for the load
/// and destroyed after it, every part of it but @p preferences: the motor database and its
/// finder, the application's materials, the extension registry, the context, the loader.
[[nodiscard]] Result<LoadedDocument> loadAndDestroyTheEnvironment(
    QtRocket::InMemoryPreferences& preferences)
{
    auto database = std::make_unique<QtRocket::ThrustCurveMotorSetDatabase>();
    database->addMotor(QtRocket::Test::makeEmbeddedTestMotor("A8", 10.0, "digest"));
    auto finder    = std::make_unique<QtRocket::DatabaseMotorFinder>(*database);
    auto materials = std::make_unique<QtRocket::MaterialStorage>();
    QtRocket::addBuiltinMaterials(*materials);
    auto registry = std::make_unique<QtRocket::SimulationExtensionRegistry>(
        QtRocket::SimulationExtensionRegistry::bundled());
    auto context = std::make_unique<DocumentLoadingContext>();
    context->setMotorFinder(finder.get());
    context->setApplicationMaterials(materials.get());
    context->setPreferences(&preferences);
    context->setSimulationExtensionRegistry(registry.get());
    auto loader = std::make_unique<GeneralRocketLoader>(*context);
    // The loader has its copy of the context.
    context.reset();
    Bytes archive = zipOf({textItem("rocket.ork", kWholeDesign), {"decals/a.png", image()}});
    Result<LoadedDocument> result = loader->load(std::move(archive));
    loader.reset();
    finder.reset();
    database.reset();
    materials.reset();
    registry.reset();
    return result;
}

// The document a load returns needs nothing of the load any more but the preference store:
// not the loader, not the context the loader was given, not the motor database and its
// finder, not the application's materials, not the extension registry, and not the bytes it
// was read from. All of them are destroyed here before the document is used (under the
// address sanitizer a use of any of them would be reported).
TEST(GeneralRocketLoader, TheDocumentOutlivesTheLoaderAndItsEnvironment)
{
    const QtRocket::Test::DefaultUnitsGuard units;
    QtRocket::InMemoryPreferences           preferences;
    QtRocket::Test::storeJavaTestPreferences(preferences);
    const Result<LoadedDocument> loaded = loadAndDestroyTheEnvironment(preferences);

    ASSERT_TRUE(loaded.has_value()) << loaded.error().message;
    OpenRocketDocument& document = *loaded->document;
    EXPECT_THAT(QtRocket::Test::warningTexts(loaded->warnings), IsEmpty());
    // The rocket, with the motor of its flight configuration.
    const std::vector<std::string> rocket = QtRocket::Test::describeRocket(
        document.getRocket(), preferences, std::set<QtRocket::Uuid>{});
    EXPECT_FALSE(rocket.empty());
    EXPECT_TRUE(
        document.getRocket().hasMotors(document.getRocket().getSelectedConfiguration().getId()));
    // The simulation, its extension and its stored data.
    EXPECT_EQ(QtRocket::Test::documentLines(document, nullptr),
              "rocket 'Whole' stages=1 components=3 configurations=1 selected=0\n"
              "config default active=t\n"
              "config 0 active=t\n"
              "sim 'Flight' status=LOADED branches=1 ext=[AirStart]\n"
              "storage fileType=OPENROCKET saveSimulationData=true explicitlySet=false\n"
              "saved=false undoAvailable=false redoAvailable=false undoDescription=null "
              "history=1 position=0 clean=true file=null\n"
              "modids mass=false aero=false tree=false functional=true snapshot=true\n"
              "materials=[]\n"
              "expressions=0 photo={} decals=[decals/a.png]\n");
    ASSERT_EQ(document.getSimulationCount(), 1U);
    EXPECT_THAT(document.getSimulation(0)->getSimulationExtensions().front()->getName(),
                StartsWith("Air-start ("));
    // The decal image, read from the archive the document shares.
    EXPECT_EQ(decalBytes(document, "decals/a.png"), "1 2 3 4 5");
    // The document can be changed, undone and redone.
    document.addUndoPosition("rename");
    document.getRocket().setName("Renamed");
    ASSERT_TRUE(document.isUndoAvailable());
    document.undo();
    EXPECT_EQ(document.getRocket().getName(), "Whole");
}

}  // namespace
