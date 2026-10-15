// A design whose decal images are files (a plain or gzip design) reads an image only from inside
// its own directory, as it reads a lookup table (decision D8 of 2026-10-14, which applies the
// rule of the user's decision of 2026-10-08; AppearanceHandler has the rule, through
// CsvLookupHandler::fileOfDesign(), and AppearanceHandlerTests.cpp the rule by itself). Here is
// what a whole load makes of it: designs that name their decal image in every form a name can
// take are loaded through GeneralRocketLoader, in every way a design can be loaded, and no load
// may come back with an image that lies outside the design's directory. The images of an
// archive are its entries, whatever their names: an archive is no file system.
// lookup_file_containment_tests.cpp is the same for the lookup tables.
//
// The directories of a test, under a temporary one:
//   design/               the design's directory: the design file, decals/a.png and
//                         decals/sub/b.png
//   outside/a.png         an image no design may read
//   a.png                 another, in the directory above the design's
//   cwd/                  the current directory of the process, with a.png and decals/a.png:
//                         images that OpenRocket, which names a file of a design without a
//                         directory against the current directory, would read
// The three kinds of image have different bytes, so an image says where it was read from.

#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/DecalImage.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/GzipStream.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestTempDir.h"
#include "file/RawZip.h"
#include "file/RocketLoaderTestSupport.h"
#include "file/openrocket/ConditionsTestSupport.h"

namespace
{

using QtRocket::DecalImage;
using QtRocket::GeneralRocketLoader;
using QtRocket::LoadedDocument;
using QtRocket::OpenRocketDocument;
using QtRocket::Result;
using QtRocket::stringToBytes;
using QtRocket::Warning;
using QtRocket::Test::CurrentDirectoryGuard;
using QtRocket::Test::genericUtf8;
using QtRocket::Test::LoaderEnvironment;
using QtRocket::Test::replaceAll;
using QtRocket::Test::TempDir;

using Bytes = std::vector<std::byte>;
using Texts = std::vector<std::string>;

/// The bytes of the image of the design's directory.
constexpr std::string_view kInsideImage = "inside";
/// The bytes of the images no design may read.
constexpr std::string_view kOutsideImage = "OUTSIDE";
/// The bytes of the images of the current directory.
constexpr std::string_view kCurrentImage = "CURRENT";
/// The bytes of an entry of an archive.
constexpr std::string_view kEntryImage = "entry";

/// The directories of a test (see the head of this file), with the current directory of the
/// process in "cwd" for as long as it lives.
class Site
{
public:
    Site() : m_current(m_directory.resolve("cwd"))
    {
        EXPECT_TRUE(written("design/decals/a.png", kInsideImage));
        EXPECT_TRUE(written("design/decals/sub/b.png", kInsideImage));
        EXPECT_TRUE(written("outside/a.png", kOutsideImage));
        EXPECT_TRUE(written("a.png", kOutsideImage));
        EXPECT_TRUE(written("cwd/a.png", kCurrentImage));
        EXPECT_TRUE(written("cwd/decals/a.png", kCurrentImage));
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

/// @p text as the text of an attribute between double quotes.
[[nodiscard]] std::string xmlText(std::string text)
{
    text = replaceAll(std::move(text), "&", "&amp;");
    text = replaceAll(std::move(text), "<", "&lt;");
    return replaceAll(std::move(text), "\"", "&quot;");
}

/// A design whose body tube has a decal of the image @p file.
[[nodiscard]] std::string designNaming(std::string_view file)
{
    return std::format(
        R"(<?xml version='1.0' encoding='utf-8'?>
<openrocket version="1.10" creator="test">
<rocket><name>R</name><subcomponents><stage><subcomponents><bodytube><appearance><decal name="{}" rotation="0" edgemode="REPEAT"/></appearance></bodytube></subcomponents></stage></subcomponents></rocket>
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

/// Whether a design loaded @p how is an archive, whose images are its entries.
[[nodiscard]] bool isArchive(How how) noexcept
{
    return how == How::ARCHIVE_FILE || how == How::ARCHIVE_WITH_DIRECTORY || how == How::ARCHIVE;
}

/// The bytes of the design @p design as @p how wants them; an archive has an entry named
/// @p entry with the bytes kEntryImage besides, unless @p entry is empty.
[[nodiscard]] Bytes bytesOf(const std::string& design, How how, std::string_view entry)
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
    if (isArchive(how))
    {
        if (entry.empty())
        {
            return QtRocket::Test::archive({QtRocket::Test::stored("rocket.ork", design)});
        }
        return QtRocket::Test::archive({QtRocket::Test::stored("rocket.ork", design),
                                        QtRocket::Test::stored(entry, kEntryImage)});
    }
    return stringToBytes(design);
}

/// Loads the design that names @p file @p how at @p site; an archive holds an entry of that
/// name when @p withEntry.
[[nodiscard]] Result<LoadedDocument> loadDesign(const GeneralRocketLoader& loader, const Site& site,
                                                const std::string& file, How how,
                                                bool withEntry = false)
{
    Bytes bytes = bytesOf(designNaming(file), how, withEntry ? std::string_view(file) : "");
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

/// Where the one image of @p document was read from, told by its bytes: "inside" for the image
/// of the design's directory, "OUTSIDE" and "CURRENT" for the images no design may read,
/// "entry" for an entry of an archive, "missing" for an image that cannot be read and "none"
/// without an image.
[[nodiscard]] std::string imageOf(const OpenRocketDocument& document)
{
    const std::vector<std::shared_ptr<DecalImage>>& images = document.getDecalList();
    if (images.empty())
    {
        return "none";
    }
    if (images.size() != 1)
    {
        return std::format("{} images", images.size());
    }
    const Result<Bytes> bytes = images.front()->getBytes();
    return bytes.has_value() ? QtRocket::bytesToString(*bytes) : std::string("missing");
}

/// The kind of the warning @p warning of a load: "refused" for an image file a design may not
/// read, "device" for the name of a device, and "other: <text>" for anything else.
[[nodiscard]] std::string kindOf(const Warning& warning)
{
    const std::string text = warning.toString();
    if (text.starts_with("The decal image file '") &&
        text.ends_with("a design may name such a file only by a relative path inside its own "
                       "directory; ignoring the decal."))
    {
        return "refused";
    }
    if (text.starts_with("The decal image file '") &&
        text.ends_with("a name that Windows reserves for a device; ignoring the decal."))
    {
        return "device";
    }
    return "other: " + text;
}

/// What a load gave: where its image was read from, then, behind a bar, the kinds of the
/// warnings: "inside |", "none | refused".
[[nodiscard]] std::string outcomeOf(const Result<LoadedDocument>& loaded)
{
    if (!loaded.has_value())
    {
        return "no document: " + loaded.error().message;
    }
    std::string text = imageOf(*loaded->document) + " |";
    for (const Warning& warning : loaded->warnings)
    {
        text += " " + kindOf(warning);
    }
    return text;
}

/// A name a design gives its decal image, and what a load of a plain or gzip design with a
/// directory gives for it. "{TOP}" in the name stands for the temporary directory of the test,
/// spelled with '/', and "{NATIVE}" for the same as the platform spells it: they make the name
/// absolute.
struct Form
{
    std::string_view file;
    std::string_view expected;
};

constexpr std::string_view kRead    = "inside |";
constexpr std::string_view kRefused = "none | refused";
constexpr std::string_view kMissing = "missing |";
/// A name of a device of Windows: refused on Windows, where it opens the device, and a file of
/// the design's directory elsewhere, which is not there.
#ifdef _WIN32
constexpr std::string_view kDevice = "none | device";
#else
constexpr std::string_view kDevice = kMissing;
#endif

constexpr auto kForms = std::to_array<Form>({
    // Relative names that stay inside: the image of the design's directory. (A ".." behind a
    // directory that exists, so that every platform reaches the same file: the attachment opens
    // the name as the design writes it.)
    {.file = "decals/a.png", .expected = kRead},
    {.file = "./decals/a.png", .expected = kRead},
    {.file = "decals//a.png", .expected = kRead},
    {.file = "decals/sub/../a.png", .expected = kRead},
    {.file = "decals/../decals/./a.png", .expected = kRead},
    {.file = "decals/sub/b.png", .expected = kRead},
    // Relative names that stay inside and name no file there, although the current directory
    // and the directory above have a file of that name.
    {.file = "a.png", .expected = kMissing},
    {.file = "outside/a.png", .expected = kMissing},
    {.file = "cwd/a.png", .expected = kMissing},
    // Absolute paths: to an image outside, to the image inside, to the current directory's,
    // and OpenRocket's names of its built-in textures.
    {.file = "{TOP}/outside/a.png", .expected = kRefused},
    {.file = "{NATIVE}/outside/a.png", .expected = kRefused},
    {.file = "{TOP}/a.png", .expected = kRefused},
    {.file = "{TOP}/design/decals/a.png", .expected = kRefused},
    {.file = "{TOP}/cwd/a.png", .expected = kRefused},
    {.file = "{TOP}/design/../outside/a.png", .expected = kRefused},
    {.file = "/datafiles/textures/balsa.jpg", .expected = kRefused},
    {.file = "/", .expected = kRefused},
    {.file = "//", .expected = kRefused},
    // Two dots at the start, in the middle and after a subdirectory.
    {.file = "..", .expected = kRefused},
    {.file = "../a.png", .expected = kRefused},
    {.file = "../outside/a.png", .expected = kRefused},
    {.file = "../cwd/a.png", .expected = kRefused},
    {.file = "../cwd/decals/a.png", .expected = kRefused},
    {.file = "./../a.png", .expected = kRefused},
    {.file = "..//outside//a.png", .expected = kRefused},
    {.file = "decals/../../a.png", .expected = kRefused},
    {.file = "decals/../../outside/a.png", .expected = kRefused},
    {.file = "decals/sub/../../../outside/a.png", .expected = kRefused},
    {.file = "decals/./../.././outside/a.png", .expected = kRefused},
    // Out of the directory and into it again.
    {.file = "../design/decals/a.png", .expected = kRefused},
    {.file = "decals/../../design/decals/a.png", .expected = kRefused},
    // Elements of dots and blanks.
    {.file = ".../a.png", .expected = kRefused},
    {.file = "decals/.. /a.png", .expected = kRefused},
    {.file = ".. /outside/a.png", .expected = kRefused},
    // The names of devices of Windows (kDevice: what is expected of them differs by platform).
    {.file = "nul", .expected = kDevice},
    {.file = "decals/nul.png", .expected = kDevice},
    {.file = "aux.png", .expected = kDevice},
    {.file = "COM1", .expected = kDevice},
// The forms of a Windows path. On Windows they have a root, lead out of the directory or
// name a device; elsewhere each is one odd name inside the design's directory, where no
// such file is (see CsvLookupHandlerTests.cpp).
#ifdef _WIN32
    {.file = R"(..\outside\a.png)", .expected = kRefused},
    {.file = R"(decals\..\..\outside\a.png)", .expected = kRefused},
    {.file = R"(C:\Windows\win.ini)", .expected = kRefused},
    {.file = "C:a.png", .expected = kRefused},
    {.file = R"(\Windows\win.ini)", .expected = kRefused},
    {.file = R"(\\localhost\C$\Windows\win.ini)", .expected = kRefused},
    {.file = R"(\\.\NUL)", .expected = kRefused},
    {.file = R"(\\?\C:\Windows\win.ini)", .expected = kRefused},
    {.file = R"(decals\nul)", .expected = kDevice},
    {.file = R"(decals\a.png)", .expected = kRead},
#else
    {.file = R"(..\outside\a.png)", .expected = kMissing},
    {.file = R"(decals\..\..\outside\a.png)", .expected = kMissing},
    {.file = R"(C:\Windows\win.ini)", .expected = kMissing},
    {.file = "C:a.png", .expected = kMissing},
    {.file = R"(\Windows\win.ini)", .expected = kMissing},
    {.file = R"(\\localhost\C$\Windows\win.ini)", .expected = kMissing},
    {.file = R"(\\.\NUL)", .expected = kMissing},
    {.file = R"(\\?\C:\Windows\win.ini)", .expected = kMissing},
    {.file = R"(decals\nul)", .expected = kMissing},
    {.file = R"(decals\a.png)", .expected = kMissing},
#endif
});

/// The name of @p form with the directory of @p site in the place of its marks.
[[nodiscard]] std::string nameOf(const Form& form, const Site& site)
{
    std::string name = replaceAll(std::string(form.file), "{TOP}", genericUtf8(site.top()));
    return replaceAll(std::move(name), "{NATIVE}", QtRocket::pathToUtf8(site.top()));
}

/// What a load @p how is expected to give for @p form: the form's outcome for a plain or gzip
/// design with a directory, a refusal for one without, and for an archive, which holds no image
/// entry here, an image that cannot be read whatever the name (no file is read, and no name is
/// refused).
[[nodiscard]] std::string_view expectedOf(const Form& form, How how) noexcept
{
    if (isArchive(how))
    {
        return kMissing;
    }
    return how == How::BYTES ? kRefused : form.expected;
}

/// Loads a design for every form of kForms @p how and returns the forms whose load did not
/// give what is expected (expectedOf()), each as "<name>: <outcome>, not <expected>".
[[nodiscard]] Texts wrongOutcomes(How how)
{
    const Site                site;
    LoaderEnvironment         environment;
    const GeneralRocketLoader loader(environment.context());
    Texts                     wrong;
    for (const Form& form : kForms)
    {
        const std::string_view expected = expectedOf(form, how);
        const std::string outcome = outcomeOf(loadDesign(loader, site, nameOf(form, site), how));
        if (outcome != expected)
        {
            wrong.push_back(std::format("{}: {}, not {}", form.file, outcome, expected));
        }
    }
    return wrong;
}

// The sweep stands on its images: what a load gives where it reads each of them.
TEST(DecalFileContainment, AnImageTellsWhereItWasReadFrom)
{
    const Site                site;
    LoaderEnvironment         environment;
    const GeneralRocketLoader loader(environment.context());

    // A design in the directory above reads "a.png" there, and one in the current directory the
    // image of that.
    EXPECT_EQ(outcomeOf(loader.load(stringToBytes(designNaming("a.png")), site.top())),
              "OUTSIDE |");
    EXPECT_EQ(outcomeOf(loader.load(stringToBytes(designNaming("a.png")), site.top() / "cwd")),
              "CURRENT |");
    EXPECT_EQ(outcomeOf(loader.load(stringToBytes(designNaming("decals/a.png")), site.design())),
              kRead);
}

TEST(DecalFileContainment, APlainDesignFileReadsOnlyInsideItsDirectory)
{
    EXPECT_EQ(wrongOutcomes(How::PLAIN_FILE), Texts{});
}

TEST(DecalFileContainment, AGzipDesignFileReadsOnlyInsideItsDirectory)
{
    EXPECT_EQ(wrongOutcomes(How::GZIP_FILE), Texts{});
}

TEST(DecalFileContainment, ADesignFromMemoryReadsOnlyInsideItsBaseDirectory)
{
    EXPECT_EQ(wrongOutcomes(How::BYTES_WITH_DIRECTORY), Texts{});
}

// No directory: no image is read, not even the one that lies under the very name in the
// current directory of the process (which is where OpenRocket would look for it).
TEST(DecalFileContainment, ADesignFromMemoryWithoutADirectoryReadsNoImage)
{
    EXPECT_EQ(wrongOutcomes(How::BYTES), Texts{});
}

// An archive reads no file whatever its images are named, with a directory or without: an
// image without an entry of its name cannot be read, also when the file system has a file of
// that name, and no name is refused.
TEST(DecalFileContainment, AnArchiveReadsNoFile)
{
    EXPECT_EQ(wrongOutcomes(How::ARCHIVE_FILE), Texts{});
    EXPECT_EQ(wrongOutcomes(How::ARCHIVE_WITH_DIRECTORY), Texts{});
    EXPECT_EQ(wrongOutcomes(How::ARCHIVE), Texts{});
}

// The names of an archive's entries are free: an image is the entry of its name, an absolute
// name and one with ".." included (OpenRocket's own example "Parallel booster staging" holds its
// textures as entries "/datafiles/textures/*").
TEST(DecalFileContainment, AnArchiveFindsAnImageAsTheEntryOfItsName)
{
    const Site                site;
    LoaderEnvironment         environment;
    const GeneralRocketLoader loader(environment.context());
    Texts                     wrong;
    for (const std::string& file :
         {std::string("decals/a.png"), std::string("/datafiles/textures/balsa.jpg"),
          std::string("../outside/a.png"), genericUtf8(site.top()) + "/outside/a.png",
          std::string("nul")})
    {
        for (const How how : {How::ARCHIVE_FILE, How::ARCHIVE_WITH_DIRECTORY, How::ARCHIVE})
        {
            const std::string outcome = outcomeOf(loadDesign(loader, site, file, how, true));
            if (outcome != "entry |")
            {
                wrong.push_back(std::format("{}: {}", file, outcome));
            }
        }
    }
    EXPECT_EQ(wrong, Texts{});
}

// A design file that is named without a directory is one of the current directory, which is
// then the design's own (GeneralRocketLoader gives the context "." for it).
TEST(DecalFileContainment, AFileNamedWithoutADirectoryHasTheCurrentDirectoryAsItsOwn)
{
    const Site                site;
    LoaderEnvironment         environment;
    const GeneralRocketLoader loader(environment.context());
    ASSERT_TRUE(QtRocket::writeFile(site.top() / "cwd" / "here.ork",
                                    stringToBytes(designNaming("decals/a.png"))));
    ASSERT_TRUE(QtRocket::writeFile(site.top() / "cwd" / "above.ork",
                                    stringToBytes(designNaming("../outside/a.png"))));

    EXPECT_EQ(outcomeOf(loader.load(std::filesystem::path("here.ork"))), "CURRENT |");
    EXPECT_EQ(outcomeOf(loader.load(std::filesystem::path("above.ork"))), kRefused);
}

}  // namespace
