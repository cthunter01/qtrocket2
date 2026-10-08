// The .ork files of old file format versions in tests/data/ork: designs that OpenRocket 0.9.3
// to 23.09 wrote, copied byte for byte out of OpenRocket's git history (and simplerocket.ork,
// which was there before), for the paths of the loader that the sixteen examples of data/examples
// never reach. The examples are all zip archives of the versions 1.10 and 1.11; these are plain
// XML, gzip and zip (with stored entries, a directory entry before the document, entries with
// and without a data descriptor) of the versions 1.0 to 1.9. tests/data/ork/README.md lists
// where each file comes from and what OpenRocket makes of it.
//
// What is checked here is that the files are the recorded ones and that the containers can be
// opened: every file is there with its size and its SHA-256 (.gitattributes keeps a checkout
// from converting the three plain XML files), the directory holds no other design, the
// container is of the recorded kind, QtRocket's gzip and zip readers get the document and the
// attachments out of it, and the document starts with the root element of the recorded version
// and creator. No design is loaded: the .ork loader is run 9b of tier 9, whose tests load these
// files.

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/GzipStream.h"
#include "QtRocket/file/ZipArchive.h"
#include "QtRocket/file/ZipInputStream.h"
#include "QtRocket/util/FileIo.h"
#include "Sha256.h"
#include "TestPaths.h"

namespace
{

/// How the XML document of a design is stored.
enum class Container
{
    PLAIN,  ///< the XML document itself
    GZIP,   ///< the XML document, gzip-compressed
    ZIP     ///< a zip archive with the document "rocket.ork" and the decal images
};

/// A design of tests/data/ork, as tests/data/ork/README.md records it.
struct LegacyFile
{
    std::string_view name;
    std::size_t      size;
    std::string_view sha256;
    Container        container;
    /// The attributes of the root element of the document.
    std::string_view version;
    std::string_view creator;
    /// The entries of a zip archive in the order of the file, each with the size of its
    /// contents ("" for the other containers). A name that ends with '/' is a directory.
    std::string_view entries;
};

/// gtest prints the parameter of a test with this: the file name says it all.
std::ostream& operator<<(std::ostream& out, const LegacyFile& file)
{
    return out << file.name;
}

constexpr std::array<LegacyFile, 18> kLegacyFiles{{
    {.name      = "simplerocket.ork",
     .size      = 2177,
     .sha256    = "cd69389d569a69ec5a00e1a44c910fff90a8815dd8300a63d83c54368db70133",
     .container = Container::GZIP,
     .version   = "1.2",
     .creator   = "OpenRocket 1.1.3pre",
     .entries   = ""},
    {.name      = "v1.0-roll-stabilized.ork",
     .size      = 63794,
     .sha256    = "b919470a5dd702dda1e282ea99b9e464afde9b914d05c8ba44c91fd75b5412bc",
     .container = Container::PLAIN,
     .version   = "1.0",
     .creator   = "OpenRocket 0.9.3",
     .entries   = ""},
    {.name      = "v1.4-roll-stabilized.ork",
     .size      = 69469,
     .sha256    = "c10171709ee4a24892ac53e98f47780fc410c790c9f92ca4a66ad5582841b504",
     .container = Container::PLAIN,
     .version   = "1.4",
     .creator   = "OpenRocket 12.03",
     .entries   = ""},
    {.name      = "v1.5-preset-usage.ork",
     .size      = 2074,
     .sha256    = "510539d7ede719fe8aa29da8d1d9f9b60f8259a2947ecee27122a9dce8cc4612",
     .container = Container::GZIP,
     .version   = "1.5",
     .creator   = "OpenRocket 12.03dev",
     .entries   = ""},
    {.name      = "v1.6-a-simple-model-rocket.ork",
     .size      = 3494,
     .sha256    = "1730385211fd4bd30b3e11bec0a13a31761c382f938fc00074e35c8460d501e7",
     .container = Container::ZIP,
     .version   = "1.6",
     .creator   = "OpenRocket 13.04beta1",
     .entries   = "rocket.ork 16126, decals/BodyStripe.png 257, decals/TailStripe.png 267"},
    {.name      = "v1.6-apocd.ork",
     .size      = 30942,
     .sha256    = "7df43dcdbfa02163091a19f8160ce3f7acf2d5a1442f9afeed9640835d08dbbd",
     .container = Container::PLAIN,
     .version   = "1.6",
     .creator   = "OpenRocket 12.03dev",
     .entries   = ""},
    {.name      = "v1.6-boosted-dart.ork",
     .size      = 13052,
     .sha256    = "540376adb5cdae6c44c26538e8529379f5af3ac30dc0aae992e7221cf740eaf6",
     .container = Container::ZIP,
     .version   = "1.6",
     .creator   = "OpenRocket 13.04beta1",
     .entries   = "rocket.ork 22766, decals/openRocket.png 7256, decals/us.png 2486"},
    {.name      = "v1.6-high-power-airstart.ork",
     .size      = 3682,
     .sha256    = "f788dae84beba6038bd97273be31b2a4414f226cc2aa5843013ef4b17be6965f",
     .container = Container::ZIP,
     .version   = "1.6",
     .creator   = "OpenRocket 13.04beta1",
     .entries   = "rocket.ork 23354, decals/patriot.png 2121"},
    {.name      = "v1.6-preset-usage-decals-first.ork",
     .size      = 5228,
     .sha256    = "03bcfbbb050d39fab7f3bcfe1cee06d0ecdc08a4cfbac0a31d8cec0c0e281b4f",
     .container = Container::ZIP,
     .version   = "1.6",
     .creator   = "OpenRocket 13.04beta1",
     .entries   = "decals/ 0, rocket.ork 9182, decals/beta.png 674, decals/open.png 1901"},
    {.name      = "v1.6-simulation-listeners.ork",
     .size      = 18811,
     .sha256    = "0fa06afa1bf649f71f4189928b4d3b6793722b8ce16f6e8056f86ea0c8316a9b",
     .container = Container::ZIP,
     .version   = "1.6",
     .creator   = "OpenRocket 13.04beta1",
     .entries   = "rocket.ork 21086, decals/skunk.jpg 15601, decals/sticker.gif 1109"},
    {.name      = "v1.6-tarc-payloader.ork",
     .size      = 4316,
     .sha256    = "218dd51b7436c83b1404393a385f35a865cc3aa4fbe408ba075b8693073ec339",
     .container = Container::ZIP,
     .version   = "1.6",
     .creator   = "OpenRocket 13.04beta1",
     .entries   = "rocket.ork 12606, decals/spiral-wound-alpha.png 2044"},
    {.name      = "v1.6-three-stage-rocket.ork",
     .size      = 3541,
     .sha256    = "2a3d965c8d92d663a028db124c49406088b6784e5c7fdb8d80e363b54cf0f1c0",
     .container = Container::ZIP,
     .version   = "1.6",
     .creator   = "OpenRocket 13.04beta1",
     .entries   = "rocket.ork 24903, decals/gStripe.png 225"},
    {.name      = "v1.7-simulation-extensions-and-scripting.ork",
     .size      = 19459,
     .sha256    = "b2d05a79569b60418e64a3653fffeb035e2c405e4d2df42597d7ed5e7eae9c36",
     .container = Container::ZIP,
     .version   = "1.7",
     .creator   = "OpenRocket 14.11dev",
     .entries   = "rocket.ork 24241, decals/skunk.jpg 15601, decals/sticker.gif 1109"},
    {.name      = "v1.7-tube-fin.ork",
     .size      = 34520,
     .sha256    = "f0d4caade4c958df622064d8343e62e835aa3357fe6bb8b520c024ee93a07295",
     .container = Container::ZIP,
     .version   = "1.7",
     .creator   = "OpenRocket 14.11dev",
     .entries   = "rocket.ork 111881"},
    {.name      = "v1.8-logo-rocket.ork",
     .size      = 1628,
     .sha256    = "532d5899247a025336ffb4d1877ef57fceeee610255ceedaf324255324b93266",
     .container = Container::ZIP,
     .version   = "1.8",
     .creator   = "OpenRocket 22.02.beta.05",
     .entries   = "rocket.ork 7856"},
    {.name      = "v1.8-parallel-staging-example.ork",
     .size      = 2525,
     .sha256    = "a13a92808020852b3763a82dbd0c3d0d16461d978cdf9ea7ae0a89551e3d9c27",
     .container = Container::ZIP,
     .version   = "1.8",
     .creator   = "OpenRocket 19-xx-alpha-12",
     .entries   = "rocket.ork 12032"},
    {.name      = "v1.8-pods-example.ork",
     .size      = 2482,
     .sha256    = "95b1abaa386150e53a934816dc5bfaeb0781782045859774f1fe2d1d7162cfd2",
     .container = Container::ZIP,
     .version   = "1.8",
     .creator   = "OpenRocket 19-xx-alpha-12",
     .entries   = "rocket.ork 12011"},
    {.name      = "v1.9-chute-release.ork",
     .size      = 82364,
     .sha256    = "0a31a72cbd42261e42aaf02730418c65f61c3294b8b908d81e859640743080ea",
     .container = Container::ZIP,
     .version   = "1.9",
     .creator   = "OpenRocket 23.09.beta.01",
     .entries   = "rocket.ork 359199, decals/BodyStripe.png 257, decals/TailStripe.png 267"},
}};

[[nodiscard]] std::filesystem::path orkDir()
{
    return QtRocket::Test::testDataDir() / "ork";
}

/// The bytes of the design @p file; none, after a test failure, when it cannot be read.
[[nodiscard]] std::vector<std::byte> bytesOf(const LegacyFile& file)
{
    auto bytes = QtRocket::readFile(orkDir() / file.name);
    if (!bytes)
    {
        ADD_FAILURE() << file.name << ": " << bytes.error().toString();
        return {};
    }
    return std::move(*bytes);
}

/// Whether @p bytes starts with the bytes of @p signature.
[[nodiscard]] bool startsWith(std::span<const std::byte> bytes, std::string_view signature)
{
    return bytes.size() >= signature.size() &&
           QtRocket::bytesToString(bytes.first(signature.size())) == signature;
}

/// The container @p bytes is, by its first bytes: a gzip stream (1f 8b), a zip archive ("PK"
/// and a local file header) or an XML document; nullopt for anything else.
[[nodiscard]] std::optional<Container> containerOf(std::span<const std::byte> bytes)
{
    if (startsWith(bytes, "\x1f\x8b"))
    {
        return Container::GZIP;
    }
    if (QtRocket::ZipArchive::looksLikeZip(bytes))
    {
        return Container::ZIP;
    }
    if (startsWith(bytes, "<?xml"))
    {
        return Container::PLAIN;
    }
    return std::nullopt;
}

/// The entries of the zip archive @p bytes as Java's ZipInputStream meets them, which is how
/// the loader reads an archive: "name size" of each in the order of the file, the directory
/// entries included; the failure in place of the entry that cannot be read.
[[nodiscard]] std::string zipEntries(std::span<const std::byte> bytes)
{
    QtRocket::ZipInputStream stream{bytes};
    std::string              entries;
    while (true)
    {
        const auto entry = stream.nextEntry();
        if (!entry)
        {
            return entries + " FAILED: " + entry.error().message;
        }
        if (!entry->has_value())
        {
            return entries;
        }
        const auto contents = stream.readEntry();
        if (!contents)
        {
            return entries + " FAILED: " + contents.error().message;
        }
        entries +=
            std::format("{}{} {}", entries.empty() ? "" : ", ", (*entry)->name, contents->size());
    }
}

/// The XML document of the design @p bytes in the container @p container, read with the
/// library's readers; the failure, which starts with "FAILED", when it cannot be read.
[[nodiscard]] std::string documentOf(std::span<const std::byte> bytes, Container container)
{
    if (container == Container::PLAIN)
    {
        return QtRocket::bytesToString(bytes);
    }
    if (container == Container::GZIP)
    {
        const auto plain = QtRocket::gzipInflate(bytes);
        return plain ? QtRocket::bytesToString(*plain) : "FAILED: " + plain.error().message;
    }
    const auto archive = QtRocket::ZipArchive::fromBytes(bytes);
    if (!archive)
    {
        return "FAILED: " + archive.error().message;
    }
    const std::vector<std::byte>* document = archive->find("rocket.ork");
    return document != nullptr ? QtRocket::bytesToString(*document) : "FAILED: no rocket.ork";
}

/// The first two lines of the document of a design of the version @p version that
/// @p creator wrote: OpenRocketSaver's XML declaration and root element.
[[nodiscard]] std::string documentStart(std::string_view version, std::string_view creator)
{
    return std::format(
        "<?xml version='1.0' encoding='utf-8'?>\n"
        "<openrocket version=\"{}\" creator=\"{}\">\n",
        version, creator);
}

class LegacyOrkFiles : public ::testing::TestWithParam<LegacyFile>
{ };

TEST_P(LegacyOrkFiles, IsTheRecordedFile)
{
    const LegacyFile&            file  = GetParam();
    const std::vector<std::byte> bytes = bytesOf(file);
    EXPECT_EQ(bytes.size(), file.size);
    EXPECT_EQ(QtRocket::Test::sha256Hex(bytes), file.sha256);
}

TEST_P(LegacyOrkFiles, ContainerHoldsTheDocumentOfTheRecordedVersion)
{
    const LegacyFile&            file  = GetParam();
    const std::vector<std::byte> bytes = bytesOf(file);
    ASSERT_EQ(containerOf(bytes), file.container);
    const std::string document = documentOf(bytes, file.container);
    const std::string start    = documentStart(file.version, file.creator);
    EXPECT_EQ(document.substr(0, start.size()), start);
    EXPECT_TRUE(document.ends_with("</openrocket>\n")) << "a document cut short";
    EXPECT_EQ(file.container == Container::ZIP ? zipEntries(bytes) : "", file.entries);
}

/// The test name of a design: its file name without the extension, with '_' for what a test
/// name cannot hold.
[[nodiscard]] std::string legacyFileTestName(const ::testing::TestParamInfo<LegacyFile>& info)
{
    std::string name{info.param.name.substr(0, info.param.name.rfind('.'))};
    std::ranges::replace(name, '-', '_');
    std::ranges::replace(name, '.', '_');
    return name;
}

INSTANTIATE_TEST_SUITE_P(Files, LegacyOrkFiles, ::testing::ValuesIn(kLegacyFiles),
                         legacyFileTestName);

// The table is the directory: a design that is added without its size and hash, or one that is
// removed, fails here (and tests/data/ork/README.md has to follow).
TEST(LegacyOrkFilesDirectory, HoldsTheRecordedFilesAndNoOther)
{
    std::vector<std::string> found;
    for (const auto& entry : std::filesystem::directory_iterator(orkDir()))
    {
        if (entry.path().filename() != "README.md")
        {
            found.push_back(entry.path().filename().string());
        }
    }
    std::ranges::sort(found);
    std::vector<std::string> recorded;
    recorded.reserve(kLegacyFiles.size());
    for (const LegacyFile& file : kLegacyFiles)
    {
        recorded.emplace_back(file.name);
    }
    std::ranges::sort(recorded);
    EXPECT_EQ(found, recorded);
    EXPECT_TRUE(std::filesystem::is_regular_file(orkDir() / "README.md"));
}

}  // namespace
