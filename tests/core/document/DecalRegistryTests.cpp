#include "QtRocket/document/DecalRegistry.h"

#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/Attachment.h"
#include "QtRocket/document/DecalImage.h"
#include "QtRocket/document/attachments/FileSystemAttachment.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestTempDir.h"
#include "document/TestAttachments.h"

// OpenRocket has no test of DecalRegistry. Every expectation here is a value OpenRocket gave:
// the scout's LoadProbe.out (tier9-scout-document, "DecalRegistry naming"), and DecalProbe.out of
// this part (probes/tier9a-document-d2), whose sections the tests name. The four deviations from
// the Java are tested as such and say so.

namespace
{

using QtRocket::Attachment;
using QtRocket::BugError;
using QtRocket::DecalImage;
using QtRocket::DecalRegistry;
using QtRocket::ErrorCode;
using QtRocket::FileSystemAttachment;
using QtRocket::Result;
using QtRocket::Test::FailingAttachment;
using QtRocket::Test::MemoryAttachment;
using QtRocket::Test::TempDir;

// One registry per document.
static_assert(!std::is_copy_constructible_v<DecalRegistry>);
static_assert(!std::is_move_constructible_v<DecalRegistry>);
static_assert(!std::is_copy_assignable_v<DecalRegistry>);
static_assert(!std::is_move_assignable_v<DecalRegistry>);

/// An attachment that is not a file (as the one of an archive entry), named @p name.
[[nodiscard]] std::shared_ptr<const Attachment> entry(const std::string& name)
{
    return std::make_shared<FailingAttachment>(name);
}

/// An attachment that is not a file, named @p name, with the bytes @p text.
[[nodiscard]] std::shared_ptr<const Attachment> entry(const std::string& name,
                                                      const std::string& text)
{
    return std::make_shared<MemoryAttachment>(name, text);
}

/// The attachment of the file @p location, named "n". The registry never opens the file, so it
/// need not exist.
[[nodiscard]] std::shared_ptr<const Attachment> file(const std::filesystem::path& location)
{
    return std::make_shared<FileSystemAttachment>("n", location);
}

/// Registers an attachment that is not a file under each of @p names, in order.
void registerEntries(DecalRegistry& registry, std::initializer_list<const char*> names)
{
    for (const char* const name : names)
    {
        static_cast<void>(registry.getDecalImage(entry(name)));
    }
}

/// The names of the registered images, in the order of getDecalList().
[[nodiscard]] std::vector<std::string> listOf(const DecalRegistry& registry)
{
    std::vector<std::string> names;
    for (const std::shared_ptr<DecalImage>& image : registry.getDecalList())
    {
        names.push_back(image->getName());
    }
    return names;
}

/// The bytes of @p image as text, or "<code>: <message>" of the failure.
[[nodiscard]] std::string bytesOrError(const DecalImage& image)
{
    const Result<std::vector<std::byte>> bytes = image.getBytes();
    if (bytes.has_value())
    {
        return "bytes " + QtRocket::bytesToString(*bytes);
    }
    return std::string(QtRocket::toString(bytes.error().code)) + ": " + bytes.error().message;
}

using Names = std::vector<std::string>;

// ------------------------------------------------------------------ the scout's naming pins

/// One request of LoadProbe.java's "DecalRegistry naming": an archive entry of that name, or the
/// file <tmp>/<name> as an attachment named <name>, and the name of the image OpenRocket gave.
struct NamingPin
{
    bool        isFile;
    const char* name;
    const char* imageName;
};

constexpr std::array<NamingPin, 16> kNamingPins{{
    {.isFile = false, .name = "decals/a.png", .imageName = "decals/a.png"},
    {.isFile = false, .name = "decals/a.png", .imageName = "decals/a.png"},
    {.isFile    = false,
     .name      = "/datafiles/textures/balsa.jpg",
     .imageName = "/datafiles/textures/balsa.jpg"},
    {.isFile = false, .name = "plain.png", .imageName = "plain.png"},
    {.isFile = false, .name = "noext", .imageName = "noext"},
    {.isFile = true, .name = "x/a.png", .imageName = "decals/a (1).png"},
    {.isFile = true, .name = "y/a.png", .imageName = "decals/a (2).png"},
    {.isFile = true, .name = "x/a.png", .imageName = "decals/a (1).png"},
    {.isFile = true, .name = "z/a (1).png", .imageName = "decals/a (3).png"},
    {.isFile = true, .name = "z/b.tar.gz", .imageName = "decals/b.tar.gz"},
    {.isFile = true, .name = "z/noext", .imageName = "decals/noext"},
    {.isFile = true, .name = "w/noext", .imageName = " (1)."},
    {.isFile = true, .name = "q/a.png", .imageName = "decals/a (4).png"},
    {.isFile = true, .name = "decals/c.png", .imageName = "decals/c.png"},
    {.isFile = true, .name = "z/a (7).png", .imageName = "decals/a (5).png"},
    {.isFile = true, .name = "r/a.png", .imageName = "decals/a (6).png"},
}};

/// Asks @p registry for the image of every pin, in order, and returns the images.
[[nodiscard]] std::vector<std::shared_ptr<DecalImage>> askNamingPins(DecalRegistry& registry)
{
    const std::filesystem::path              tmp("tmpdir");
    std::vector<std::shared_ptr<DecalImage>> images;
    for (const NamingPin& pin : kNamingPins)
    {
        const std::shared_ptr<const Attachment> attachment =
            pin.isFile ? std::shared_ptr<const Attachment>(
                             std::make_shared<FileSystemAttachment>(pin.name, tmp / pin.name))
                       : entry(pin.name);
        images.push_back(registry.getDecalImage(attachment));
    }
    return images;
}

TEST(DecalRegistry, TheNamesOpenRocketGivesInTheLoadProbesOrder)
{
    DecalRegistry                                  registry;
    const std::vector<std::shared_ptr<DecalImage>> images = askNamingPins(registry);
    Names                                          actual;
    Names                                          expected;
    for (std::size_t i = 0; i < images.size(); i++)
    {
        actual.push_back(images.at(i)->getName());
        expected.emplace_back(kNamingPins.at(i).imageName);
    }
    EXPECT_EQ(actual, expected);
}

TEST(DecalRegistry, TheLoadProbesImagesAndList)
{
    DecalRegistry                                  registry;
    const std::vector<std::shared_ptr<DecalImage>> images = askNamingPins(registry);
    ASSERT_EQ(images.size(), kNamingPins.size());

    // The same entry name and the same file give the same image (the probe's equal ids).
    EXPECT_EQ(images.at(0), images.at(1));
    EXPECT_EQ(images.at(5), images.at(7));
    EXPECT_NE(images.at(5), images.at(6));
    // An entry's image has no decal file; a file's image has the file.
    EXPECT_EQ(images.at(0)->getDecalFile(), std::nullopt);
    EXPECT_EQ(images.at(2)->getDecalFile(), std::nullopt);
    EXPECT_EQ(images.at(5)->getDecalFile(), std::filesystem::path("tmpdir") / "x/a.png");
    EXPECT_EQ(images.at(11)->getDecalFile(), std::filesystem::path("tmpdir") / "w/noext");
    EXPECT_EQ(images.at(14)->getDecalFile(), std::filesystem::path("tmpdir") / "z/a (7).png");

    // Java: "list (sorted): ' (1).' '/datafiles/textures/balsa.jpg' 'decals/a (1).png' ...".
    EXPECT_EQ(listOf(registry),
              (Names{" (1).", "/datafiles/textures/balsa.jpg", "decals/a (1).png",
                     "decals/a (2).png", "decals/a (3).png", "decals/a (4).png", "decals/a (5).png",
                     "decals/a (6).png", "decals/a.png", "decals/b.tar.gz", "decals/c.png",
                     "decals/noext", "noext", "plain.png"}));
    EXPECT_EQ(registry.size(), 14U);
}

// ------------------------------------------------------------------------- makeUniqueName

/// What makeUniqueName(@p asked) gives in a registry that holds one image, named @p registered
/// (empty optional: an empty registry), and what OpenRocket gave.
struct PatternCase
{
    PatternCase(std::optional<std::string> registeredName, std::string askedName,
                std::string expectedName)
      : registered(std::move(registeredName)),
        asked(std::move(askedName)),
        expected(std::move(expectedName))
    {
    }

    std::optional<std::string> registered;
    std::string                asked;
    std::string                expected;
};

/// The cases of @p cases that do not give what they expect, described; empty when all do.
[[nodiscard]] Names mismatches(std::span<const PatternCase> cases)
{
    Names wrong;
    for (const PatternCase& pattern : cases)
    {
        DecalRegistry registry;
        if (pattern.registered.has_value())
        {
            static_cast<void>(registry.getDecalImage(entry(*pattern.registered)));
        }
        const std::string actual = registry.makeUniqueName(pattern.asked);
        if (actual != pattern.expected)
        {
            wrong.push_back(std::format("registered '{}', asked '{}': '{}', OpenRocket '{}'",
                                        pattern.registered.value_or("<nothing>"), pattern.asked,
                                        actual, pattern.expected));
        }
    }
    return wrong;
}

TEST(DecalRegistry, MakeUniqueNameOnAnEmptyRegistryOnlyAddsTheFolder)
{
    // DecalProbe.out, section A.
    const std::vector<PatternCase> cases{
        {std::nullopt, "a.png", "decals/a.png"},
        {std::nullopt, "decals/a.png", "decals/a.png"},
        {std::nullopt, "x/a.png", "decals/x/a.png"},
        {std::nullopt, "", "decals/"},
        {std::nullopt, ".", "decals/."},
        {std::nullopt, ".png", "decals/.png"},
        {std::nullopt, "noext", "decals/noext"},
        // The number of the name asked for stays while nothing collides.
        {std::nullopt, "a (3).png", "decals/a (3).png"},
        {std::nullopt, "a (3)).png", "decals/a (3)).png"},
        {std::nullopt, "a b.c d", "decals/a b.c d"},
        // A leading slash stays behind the folder; the folder is case-sensitive and needs its
        // slash.
        {std::nullopt, "/abs/a.png", "decals//abs/a.png"},
        {std::nullopt, "decals", "decals/decals"},
        {std::nullopt, "decals/", "decals/"},
        {std::nullopt, "DECALS/a.png", "decals/DECALS/a.png"},
    };
    EXPECT_EQ(mismatches(cases), Names{});
}

TEST(DecalRegistry, MakeUniqueNameNumbersACopy)
{
    // DecalProbe.out, section B: base and extension the same.
    const std::vector<PatternCase> cases{
        {"decals/a.png", "a.png", "decals/a (1).png"},
        {"decals/a.png", "decals/a.png", "decals/a (1).png"},
        // The number asked for is dropped.
        {"decals/a.png", "a (5).png", "decals/a (1).png"},
        // Base and extension are compared exactly.
        {"decals/a.png", "a.PNG", "decals/a.PNG"},
        {"decals/a.png", "A.png", "decals/A.png"},
        {"decals/a.png", "a.jpg", "decals/a.jpg"},
        // The smallest number no registered name has.
        {"decals/a (3).png", "a.png", "decals/a (1).png"},
        {"decals/a (1).png", "a.png", "decals/a (2).png"},
        // Leading zeros are the number; zero is a number, but the copies start at 1.
        {"decals/a (007).png", "a.png", "decals/a (1).png"},
        {"decals/a (0).png", "a.png", "decals/a (1).png"},
        {"decals/a (00).png", "a.png", "decals/a (1).png"},
        {"decals/a (01).png", "a.png", "decals/a (2).png"},
        {"decals/a (2147483647).png", "a.png", "decals/a (1).png"},
        // Several closing parentheses are part of the number group.
        {"decals/a (1)).png", "a.png", "decals/a (2).png"},
        {"decals/a (1))).png", "a.png", "decals/a (2).png"},
        // The last dot splits base and extension.
        {"decals/a.tar.gz", "a.gz", "decals/a.gz"},
        {"decals/a.tar.gz", "a.tar.gz", "decals/a.tar (1).gz"},
        {"decals/a.tar.gz", "a.tar", "decals/a.tar"},
        // An empty extension and an empty base are an extension and a base.
        {"decals/a.", "a.", "decals/a (1)."},
        {"decals/a.", "a", "decals/a"},
        {"decals/.png", ".png", "decals/ (1).png"},
        {"decals/.", ".", "decals/ (1)."},
        {"decals/..", "..", "decals/. (1)."},
        {"decals/..", ".", "decals/."},
        {"decals/a.p_g", "a.p_g", "decals/a (1).p_g"},
        // The folder is part of the base.
        {"a.png", "a.png", "decals/a.png"},
        {"a.png", "decals/a.png", "decals/a.png"},
        {"decals/decals/a.png", "decals/a.png", "decals/a.png"},
        // The name is no pattern: braces and quotes stay as they are.
        {"decals/a {0}.png", "a {0}.png", "decals/a {0} (1).png"},
        {"decals/a 'q'.png", "a 'q'.png", "decals/a 'q' (1).png"},
        {"decals/a {.png", "a {.png", "decals/a { (1).png"},
    };
    EXPECT_EQ(mismatches(cases), Names{});
}

TEST(DecalRegistry, MakeUniqueNameReadsANumberOnlyInJavasForm)
{
    // DecalProbe.out, section B: " (digits)" with one or more closing parentheses, directly
    // before the last dot. Anything else is part of the base.
    const std::vector<PatternCase> cases{
        {"decals/a ((1)).png", "a.png", "decals/a.png"},
        {"decals/a ().png", "a.png", "decals/a.png"},
        {"decals/a ().png", "a ().png", "decals/a () (1).png"},
        {"decals/a(1).png", "a.png", "decals/a.png"},
        {"decals/a(1).png", "a(1).png", "decals/a(1) (1).png"},
        // Two spaces: the base ends in a space.
        {"decals/a  (1).png", "a.png", "decals/a.png"},
        {"decals/a  (1).png", "a .png", "decals/a  (2).png"},
        {"decals/a (1) .png", "a.png", "decals/a.png"},
        {"decals/a (1) .png", "a (1) .png", "decals/a (1)  (1).png"},
        // Only the last group is the number.
        {"decals/a (1) (2).png", "a.png", "decals/a.png"},
        {"decals/a (1) (2).png", "a (1).png", "decals/a (1).png"},
        {"decals/a (1) (2).png", "a (1) (9).png", "decals/a (1) (1).png"},
        {"decals/a (-1).png", "a.png", "decals/a.png"},
        {"decals/a (-1).png", "a (-1).png", "decals/a (-1) (1).png"},
        {"decals/a (1.5).png", "a.png", "decals/a.png"},
        {"decals/a (1.5).png", "a (1.png", "decals/a (1.png"},
        {"decals/a (1.5).png", "a (1.5).png", "decals/a (1.5) (1).png"},
        // \d is an ASCII digit: U+0663, an Arabic-Indic three, is none.
        {"decals/a (\xD9\xA3).png", "a.png", "decals/a.png"},
        {"decals/a (\xD9\xA3).png", "a (\xD9\xA3).png", "decals/a (\xD9\xA3) (1).png"},
    };
    EXPECT_EQ(mismatches(cases), Names{});
}

TEST(DecalRegistry, MakeUniqueNameOfANameThePatternDoesNotMatch)
{
    // DecalProbe.out, section B. A name without a dot, with something other than ASCII letters,
    // digits and underscores after its last dot, or with a line terminator before that dot has
    // neither base nor extension. It collides with itself, and what it becomes then is " (n)."
    // without the folder.
    const std::vector<PatternCase> cases{
        {"decals/a", "a", " (1)."},
        {"decals/a", "a.", "decals/a."},
        {"decals/a.p-g", "a.p-g", " (1)."},
        {"decals/a.p-g", "a.p", "decals/a.p"},
        {"decals/a.p g", "a.p g", " (1)."},
        // \w is ASCII: an e-acute (U+00E9) in the extension does not match, in the base it does.
        {"decals/a.pn\xC3\xA9", "a.pn\xC3\xA9", " (1)."},
        {"decals/\xC3\xA9.png", "\xC3\xA9.png", "decals/\xC3\xA9 (1).png"},
        // The line terminators: \n, \r, U+0085, U+2028, U+2029. Tab, vertical tab and form feed
        // are none.
        {"decals/a\nb.png", "a\nb.png", " (1)."},
        {"decals/a\nb (1).png", "a\nb.png", "decals/a\nb.png"},
        {"decals/a\rb.png", "a\rb.png", " (1)."},
        {"decals/a\xC2\x85"
         "b.png",
         "a\xC2\x85"
         "b.png",
         " (1)."},
        {"decals/a\xE2\x80\xA8"
         "b.png",
         "a\xE2\x80\xA8"
         "b.png",
         " (1)."},
        {"decals/a\xE2\x80\xA9"
         "b.png",
         "a\xE2\x80\xA9"
         "b.png",
         " (1)."},
        {"decals/a\tb.png", "a\tb.png", "decals/a\tb (1).png"},
        {"decals/a\vb.png", "a\vb.png", "decals/a\vb (1).png"},
        {"decals/a\fb.png", "a\fb.png", "decals/a\fb (1).png"},
        {"decals/a.png\n", "a.png\n", " (1)."},
        {"decals/a.p\ng", "a.p\ng", " (1)."},
        // It collides with every registered name of an empty base and extension, such as the
        // " (1)." an earlier collision made: from then on every name without a dot is a copy.
        {" (1).", "noext", " (2)."},
        {" (1).", "other", " (2)."},
        {" (1).", "decals/", " (2)."},
        {" (4).", "noext", " (1)."},
        {".", "noext", " (1)."},
        {"decals/noext", "noext", " (1)."},
        {"decals/noext", "other", "decals/other"},
        // A registered name with a base or an extension is no such name.
        {" (1).x", "noext", "decals/noext"},
        {"x (1).", "noext", "decals/noext"},
    };
    EXPECT_EQ(mismatches(cases), Names{});
}

TEST(DecalRegistry, ANumberBeyondAnIntIsNotCounted)
{
    // Deviation. DecalProbe.out, section B: "registered 'decals/a (2147483648).png', asked
    // 'a.png' -> THROWS java.lang.NumberFormatException: For input string: "2147483648"". Here
    // the name asked for is a copy all the same, and the number is not counted.
    const std::vector<PatternCase> cases{
        {"decals/a (2147483648).png", "a.png", "decals/a (1).png"},
        {"decals/a (99999999999).png", "a.png", "decals/a (1).png"},
        // As in Java, another base is not affected.
        {"decals/a (99999999999).png", "b.png", "decals/b.png"},
    };
    EXPECT_EQ(mismatches(cases), Names{});
}

TEST(DecalRegistry, MakeUniqueNameTakesTheSmallestFreeNumber)
{
    // DecalProbe.out, section C.
    DecalRegistry registry;
    registerEntries(registry, {"decals/a (2).png", "decals/a (3).png", "decals/a (5).png",
                               "decals/a (1).jpg", "decals/b (1).png"});
    EXPECT_EQ(registry.makeUniqueName("a.png"), "decals/a (1).png");
    static_cast<void>(registry.getDecalImage(entry("decals/a (1).png")));
    EXPECT_EQ(registry.makeUniqueName("a.png"), "decals/a (4).png");
    static_cast<void>(registry.getDecalImage(entry("decals/a (4).png")));
    EXPECT_EQ(registry.makeUniqueName("a.png"), "decals/a (6).png");
    // The numbers are counted per base and extension.
    EXPECT_EQ(registry.makeUniqueName("a.jpg"), "decals/a (2).jpg");
    EXPECT_EQ(registry.makeUniqueName("b.png"), "decals/b (2).png");
    EXPECT_EQ(registry.makeUniqueName("c.png"), "decals/c.png");
}

TEST(DecalRegistry, MakeUniqueNameRegistersNothing)
{
    DecalRegistry registry;
    static_cast<void>(registry.getDecalImage(entry("decals/a.png")));
    EXPECT_EQ(registry.makeUniqueName("a.png"), "decals/a (1).png");
    EXPECT_EQ(registry.makeUniqueName("a.png"), "decals/a (1).png");
    EXPECT_EQ(registry.size(), 1U);
    EXPECT_EQ(registry.find("decals/a (1).png"), nullptr);
}

/// Registers "decals/a.png" and its copies "decals/a (1).png" to "decals/a (<copies>).png" in
/// @p registry, by name.
void registerCopies(DecalRegistry& registry, int copies)
{
    static_cast<void>(registry.getDecalImage(entry("decals/a.png")));
    for (int i = 1; i <= copies; i++)
    {
        static_cast<void>(registry.getDecalImage(entry(std::format("decals/a ({}).png", i))));
    }
}

TEST(DecalRegistry, TheThousandthCopyHasPlainDigits)
{
    DecalRegistry registry;
    registerCopies(registry, 998);
    // DecalProbe.out, section C: "copy 999 -> 'decals/a (999).png' registered=1000".
    EXPECT_EQ(registry.getDecalImage(file("d999/a.png"))->getName(), "decals/a (999).png");
    EXPECT_EQ(registry.size(), 1000U);

    // Deviation. Java: "copy 1000 -> 'decals/a (1,000).png' registered=1001" and "copy 1001 ->
    // 'decals/a (1,000).png' registered=1001": MessageFormat groups the digits, the pattern does
    // not read "1,000" as a number, and the next copy takes the same name and replaces the
    // image. Here the copies go on.
    EXPECT_EQ(registry.getDecalImage(file("d1000/a.png"))->getName(), "decals/a (1000).png");
    EXPECT_EQ(registry.getDecalImage(file("d1001/a.png"))->getName(), "decals/a (1001).png");
    EXPECT_EQ(registry.size(), 1002U);
    EXPECT_NE(registry.find("decals/a (1000).png"), nullptr);
    EXPECT_EQ(registry.find("decals/a (1,000).png"), nullptr);
    // A gap is still filled first.
    EXPECT_TRUE(registry.removeDecal(registry.find("decals/a (500).png").get()));
    EXPECT_EQ(registry.makeUniqueName("a.png"), "decals/a (500).png");
}

// -------------------------------------------------------------------------- getDecalImage

TEST(DecalRegistry, AnAttachmentThatIsNoFileIsFoundByItsName)
{
    // DecalProbe.out, section D: "two attachments of one name: same image=true bytes one
    // decalFile=null ignored=false toString=decals/a.png".
    DecalRegistry                     registry;
    const std::shared_ptr<DecalImage> first  = registry.getDecalImage(entry("decals/a.png", "one"));
    const std::shared_ptr<DecalImage> second = registry.getDecalImage(entry("decals/a.png", "two"));
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first, second);
    EXPECT_EQ(first->getName(), "decals/a.png");
    EXPECT_EQ(first->toString(), "decals/a.png");
    // The first attachment of the name keeps supplying the bytes.
    EXPECT_EQ(bytesOrError(*second), "bytes one");
    EXPECT_EQ(first->getDecalFile(), std::nullopt);
    EXPECT_FALSE(first->isIgnored());
    EXPECT_EQ(registry.size(), 1U);

    // The name is taken as it is: no folder is added, and another spelling is another image.
    const std::shared_ptr<DecalImage> plain = registry.getDecalImage(entry("a.png"));
    EXPECT_NE(plain, first);
    EXPECT_EQ(plain->getName(), "a.png");
    const std::shared_ptr<DecalImage> upper = registry.getDecalImage(entry("decals/A.png"));
    EXPECT_NE(upper, first);
    EXPECT_EQ(registry.size(), 3U);
}

TEST(DecalRegistry, AFileIsFoundByTheFileAndNamedAfterIt)
{
    // DecalProbe.out, section D.
    const TempDir                     dir;
    const std::filesystem::path       x = dir.write("x/a.png", "file-x");
    DecalRegistry                     registry;
    const std::shared_ptr<DecalImage> entryImage = registry.getDecalImage(entry("decals/a.png"));

    // "file x/a.png, attachment name 'some name' -> 'decals/a (1).png' decalFile=<tmp>/x/a.png
    // bytes file-x": the attachment's name plays no part.
    const std::shared_ptr<DecalImage> fromFile =
        registry.getDecalImage(std::make_shared<FileSystemAttachment>("some name", x));
    ASSERT_NE(fromFile, nullptr);
    EXPECT_NE(fromFile, entryImage);
    EXPECT_EQ(fromFile->getName(), "decals/a (1).png");
    EXPECT_EQ(fromFile->getDecalFile(), x);
    EXPECT_EQ(bytesOrError(*fromFile), "bytes file-x");

    // "the same file again under another attachment name: same image=true".
    EXPECT_EQ(registry.getDecalImage(std::make_shared<FileSystemAttachment>("another name", x)),
              fromFile);
    // "a non-file attachment named as the file image: same image=true".
    EXPECT_EQ(registry.getDecalImage(entry("decals/a (1).png")), fromFile);
    EXPECT_EQ(registry.size(), 2U);
}

TEST(DecalRegistry, WhichPathsAreTheSameFile)
{
    // DecalProbe.out, section D. Java compares the Files, which are the path texts without
    // doubled and trailing separators: no file is asked whether two paths lead to it.
    const std::filesystem::path       tmp("tmpdir");
    DecalRegistry                     registry;
    const std::shared_ptr<DecalImage> image = registry.getDecalImage(file(tmp / "x" / "a.png"));
    EXPECT_EQ(image->getName(), "decals/a.png");

    // "x//a.png -> ... same image=true".
    EXPECT_EQ(registry.getDecalImage(file(tmp / "x//a.png")), image);
    // "x/./a.png -> 'decals/a (2).png' same image=false" (there with an entry of the name too).
    const std::shared_ptr<DecalImage> dotted = registry.getDecalImage(file(tmp / "x/./a.png"));
    EXPECT_NE(dotted, image);
    EXPECT_EQ(dotted->getName(), "decals/a (1).png");
    // Another directory is another file, a relative path another one than an absolute one.
    EXPECT_NE(registry.getDecalImage(file(tmp / "y" / "a.png")), image);
    EXPECT_NE(registry.getDecalImage(file(std::filesystem::path("x") / "a.png")), image);
    EXPECT_EQ(registry.size(), 4U);
}

TEST(DecalRegistry, AFilesNameIsItsLastElement)
{
    // DecalProbe.out, section D: Java's File.getName(), which drops a separator at the end.
    const std::filesystem::path tmp("tmpdir");
    DecalRegistry               registry;
    // "the directory 'x/' -> 'decals/x'".
    const std::shared_ptr<DecalImage> directory = registry.getDecalImage(file(tmp / "x/"));
    EXPECT_EQ(directory->getName(), "decals/x");
    // The same file without the separator.
    EXPECT_EQ(registry.getDecalImage(file(tmp / "x")), directory);
    // "the root '/' -> 'decals/'".
    EXPECT_EQ(registry.getDecalImage(file("/"))->getName(), "decals/");
    // "'<tmp>/.' -> 'decals/.'" and "'<tmp>/x/..' -> 'decals/..'".
    EXPECT_EQ(registry.getDecalImage(file(tmp / "."))->getName(), "decals/.");
    EXPECT_EQ(registry.getDecalImage(file(tmp / "x" / ".."))->getName(), "decals/..");
    // "File("") -> ' (1).' decalFile=''": no name at all, which the root's image already has.
    const std::shared_ptr<DecalImage> empty = registry.getDecalImage(file(std::filesystem::path()));
    EXPECT_EQ(empty->getName(), " (1).");
    EXPECT_EQ(empty->getDecalFile(), std::filesystem::path());

    EXPECT_EQ(listOf(registry), (Names{" (1).", "decals/", "decals/.", "decals/..", "decals/x"}));
}

TEST(DecalRegistry, AFileNameOutsideAsciiIsTheImagesNameInUtf8)
{
    DecalRegistry               registry;
    const std::filesystem::path location =
        std::filesystem::path("tmpdir") / std::filesystem::path(u8"d\u00E9cor \u706B.png");
    EXPECT_EQ(registry.getDecalImage(file(location))->getName(),
              "decals/d\xC3\xA9"
              "cor \xE7\x81\xAB.png");
}

TEST(DecalRegistry, ANullAttachmentIsABug)
{
    // Java: a NullPointerException.
    DecalRegistry registry;
    EXPECT_THROW(static_cast<void>(registry.getDecalImage(nullptr)), BugError);
    EXPECT_TRUE(registry.empty());
}

// ------------------------------------------------------------------------ makeUniqueImage

TEST(DecalRegistry, MakeUniqueImageRegistersACopyUnderANewName)
{
    // DecalProbe.out, section E: "registered 'decals/a.png' -> 'decals/a (1).png' same=false
    // ignored=false bytes A list: 'decals/a (1).png' 'decals/a.png'".
    DecalRegistry                     registry;
    const std::shared_ptr<DecalImage> a = registry.getDecalImage(entry("decals/a.png", "A"));
    a->setIgnored(true);
    const std::shared_ptr<DecalImage> first = registry.makeUniqueImage(a);
    ASSERT_NE(first, nullptr);
    EXPECT_NE(first, a);
    EXPECT_EQ(first->getName(), "decals/a (1).png");
    EXPECT_FALSE(first->isIgnored());
    EXPECT_EQ(bytesOrError(*first), "bytes A");
    EXPECT_EQ(listOf(registry), (Names{"decals/a (1).png", "decals/a.png"}));
    EXPECT_EQ(registry.find("decals/a (1).png"), first);

    // "again -> 'decals/a (2).png'; of the copy -> 'decals/a (3).png'".
    EXPECT_EQ(registry.makeUniqueImage(a)->getName(), "decals/a (2).png");
    EXPECT_EQ(registry.makeUniqueImage(first)->getName(), "decals/a (3).png");
    EXPECT_EQ(listOf(registry),
              (Names{"decals/a (1).png", "decals/a (2).png", "decals/a (3).png", "decals/a.png"}));
    // The original is as it was.
    EXPECT_EQ(a->getName(), "decals/a.png");
    EXPECT_TRUE(a->isIgnored());
}

TEST(DecalRegistry, MakeUniqueImageOfANameWithoutTheFolder)
{
    // DecalProbe.out, section E: the copy of an image that an archive entry outside "decals/"
    // gave gets the folder first, and only the next copy a number.
    DecalRegistry                     registry;
    const std::shared_ptr<DecalImage> plain = registry.getDecalImage(entry("plain.png"));
    EXPECT_EQ(registry.makeUniqueImage(plain)->getName(), "decals/plain.png");
    EXPECT_EQ(registry.makeUniqueImage(plain)->getName(), "decals/plain (1).png");

    const std::shared_ptr<DecalImage> noext = registry.getDecalImage(entry("noext"));
    EXPECT_EQ(registry.makeUniqueImage(noext)->getName(), "decals/noext");
    EXPECT_EQ(registry.makeUniqueImage(noext)->getName(), " (1).");
    EXPECT_EQ(registry.makeUniqueImage(noext)->getName(), " (2).");

    const std::shared_ptr<DecalImage> slash =
        registry.getDecalImage(entry("/datafiles/textures/balsa.jpg"));
    EXPECT_EQ(registry.makeUniqueImage(slash)->getName(), "decals//datafiles/textures/balsa.jpg");
    EXPECT_EQ(registry.makeUniqueImage(slash)->getName(),
              "decals//datafiles/textures/balsa (1).jpg");

    EXPECT_EQ(
        listOf(registry),
        (Names{" (1).", " (2).", "/datafiles/textures/balsa.jpg",
               "decals//datafiles/textures/balsa (1).jpg", "decals//datafiles/textures/balsa.jpg",
               "decals/noext", "decals/plain (1).png", "decals/plain.png", "noext", "plain.png"}));
}

TEST(DecalRegistry, MakeUniqueImageOfAnImageOfAnotherRegistry)
{
    // DecalProbe.out, section E.
    DecalRegistry registry;
    static_cast<void>(registry.getDecalImage(entry("decals/a.png", "A")));
    DecalRegistry other;

    // "an image of another registry, name free here -> same=true registered here=false": the
    // image itself comes back and nothing is registered.
    const std::shared_ptr<DecalImage> foreign = other.getDecalImage(entry("decals/z.png", "Z"));
    EXPECT_EQ(registry.makeUniqueImage(foreign), foreign);
    EXPECT_EQ(registry.size(), 1U);
    EXPECT_EQ(registry.find("decals/z.png"), nullptr);

    // "an image of another registry, name taken here -> 'decals/a (4).png' same=false bytes
    // ZA" (the number there after three copies; here it is the first).
    const std::shared_ptr<DecalImage> foreignA = other.getDecalImage(entry("decals/a.png", "ZA"));
    const std::shared_ptr<DecalImage> copy     = registry.makeUniqueImage(foreignA);
    EXPECT_NE(copy, foreignA);
    EXPECT_EQ(copy->getName(), "decals/a (1).png");
    EXPECT_EQ(bytesOrError(*copy), "bytes ZA");
    EXPECT_EQ(registry.size(), 2U);
    EXPECT_EQ(other.size(), 2U);
}

TEST(DecalRegistry, ACopyOfAFileImageSharesTheFile)
{
    // DecalProbe.out, section E: "file image 'decals/b.png' -> 'decals/b (1).png' decalFile
    // same=true bytes file-y".
    const TempDir                     dir;
    const std::filesystem::path       y = dir.write("y/b.png", "file-y");
    DecalRegistry                     registry;
    const std::shared_ptr<DecalImage> image = registry.getDecalImage(file(y));
    EXPECT_EQ(image->getName(), "decals/b.png");
    const std::shared_ptr<DecalImage> copy = registry.makeUniqueImage(image);
    EXPECT_EQ(copy->getName(), "decals/b (1).png");
    EXPECT_EQ(copy->getDecalFile(), y);
    EXPECT_EQ(bytesOrError(*copy), "bytes file-y");

    // Two images have the file now. Deviation: the first in name order is found; Java finds the
    // one its HashMap lists first (in the probe, the same one).
    EXPECT_EQ(registry.getDecalImage(file(y)), copy);
    EXPECT_EQ(registry.size(), 2U);

    // Once the copy has a file of its own (OpenRocket exports it for editing), the file leads
    // to the original again.
    copy->setDecalFile(dir.resolve("edited.png"));
    EXPECT_EQ(registry.getDecalImage(file(y)), image);
    EXPECT_EQ(registry.getDecalImage(file(dir.resolve("edited.png"))), copy);
}

TEST(DecalRegistry, AnImageIsFoundByADecalFileItWasGivenLater)
{
    // Java's findDecalForFile() asks every registered image for its decal file, also the image
    // of an archive entry that was exported for editing (DecalImage.setDecalFile()).
    const std::filesystem::path       exported = std::filesystem::path("tmpdir") / "edit.png";
    DecalRegistry                     registry;
    const std::shared_ptr<DecalImage> fromEntry = registry.getDecalImage(entry("decals/a.png"));
    static_cast<void>(registry.getDecalImage(entry("decals/b.png")));
    fromEntry->setDecalFile(exported);
    EXPECT_EQ(registry.getDecalImage(file(exported)), fromEntry);
    EXPECT_EQ(registry.size(), 2U);

    // Without the decal file the file is new, and it is named after itself.
    fromEntry->setDecalFile(std::nullopt);
    const std::shared_ptr<DecalImage> fromFile = registry.getDecalImage(file(exported));
    EXPECT_NE(fromFile, fromEntry);
    EXPECT_EQ(fromFile->getName(), "decals/edit.png");
    EXPECT_EQ(registry.size(), 3U);
}

TEST(DecalRegistry, TheListIsASnapshot)
{
    DecalRegistry registry;
    registerEntries(registry, {"decals/b.png", "decals/a.png"});
    const std::vector<std::shared_ptr<DecalImage>> list = registry.getDecalList();
    ASSERT_EQ(list.size(), 2U);
    EXPECT_TRUE(registry.removeDecal(list.at(0).get()));
    static_cast<void>(registry.getDecalImage(entry("decals/c.png")));
    // The list a caller holds does not follow the registry, and its images stay whole.
    EXPECT_EQ(list.size(), 2U);
    EXPECT_EQ(list.at(0)->getName(), "decals/a.png");
    EXPECT_EQ(list.at(1)->getName(), "decals/b.png");
    EXPECT_EQ(listOf(registry), (Names{"decals/b.png", "decals/c.png"}));
}

TEST(DecalRegistry, MakeUniqueImageOfNothingIsNothing)
{
    // Java: `original instanceof DecalImageImpl` is false for null, which is returned.
    DecalRegistry registry;
    EXPECT_EQ(registry.makeUniqueImage(nullptr), nullptr);
    EXPECT_TRUE(registry.empty());
}

// --------------------------------------------------------- getDecalList, removeDecal, find

/// A registry with the eight names of DecalProbe.out, section F, registered in the probe's
/// order: 'b.png' 'a.png' 'B.png' e-acute.png 'a' '' U+1F600.png U+FFEE.png.
void registerTheProbesNames(DecalRegistry& registry)
{
    registerEntries(registry, {"b.png", "a.png", "B.png", "\xC3\xA9.png", "a", "",
                               "\xF0\x9F\x98\x80.png", "\xEF\xBF\xAE.png"});
}

TEST(DecalRegistry, TheListIsSortedAsJavaSortsStrings)
{
    // Java: "list: '' 'B.png' 'a' 'a.png' 'b.png' <e-acute>.png <U+1F600>.png <U+FFEE>.png": by
    // UTF-16 code units, so U+1F600 (a surrogate pair) comes before U+FFEE, where the UTF-8
    // bytes would put it last.
    DecalRegistry registry;
    registerTheProbesNames(registry);
    EXPECT_EQ(listOf(registry), (Names{"", "B.png", "a", "a.png", "b.png", "\xC3\xA9.png",
                                       "\xF0\x9F\x98\x80.png", "\xEF\xBF\xAE.png"}));
    EXPECT_EQ(registry.size(), 8U);
    EXPECT_FALSE(registry.empty());
}

TEST(DecalRegistry, AnEmptyRegistry)
{
    const DecalRegistry registry;
    EXPECT_TRUE(registry.empty());
    EXPECT_EQ(registry.size(), 0U);
    EXPECT_TRUE(registry.getDecalList().empty());
    EXPECT_EQ(registry.find("decals/a.png"), nullptr);
    EXPECT_EQ(registry.find(""), nullptr);
}

TEST(DecalRegistry, RemoveDecalGoesByTheName)
{
    // DecalProbe.out, section F.
    DecalRegistry registry;
    registerTheProbesNames(registry);
    const std::shared_ptr<DecalImage> b = registry.find("b.png");
    ASSERT_NE(b, nullptr);

    // "removeDecal(null)=false".
    EXPECT_FALSE(registry.removeDecal(nullptr));
    EXPECT_EQ(registry.size(), 8U);

    // "removeDecal(an image of another registry with the name 'b.png')=true": the name decides,
    // not the object.
    DecalRegistry                     other;
    const std::shared_ptr<DecalImage> sameName = other.getDecalImage(entry("b.png"));
    EXPECT_TRUE(registry.removeDecal(sameName.get()));
    EXPECT_EQ(listOf(registry), (Names{"", "B.png", "a", "a.png", "\xC3\xA9.png",
                                       "\xF0\x9F\x98\x80.png", "\xEF\xBF\xAE.png"}));
    EXPECT_EQ(other.size(), 1U);

    // "removeDecal(the removed image)=false".
    EXPECT_FALSE(registry.removeDecal(b.get()));
    // The image is still whole for whoever holds it.
    EXPECT_EQ(b->getName(), "b.png");

    // "asked again: same image=false": the name is free again.
    const std::shared_ptr<DecalImage> again = registry.getDecalImage(entry("b.png"));
    EXPECT_NE(again, b);
    EXPECT_EQ(registry.size(), 8U);
}

TEST(DecalRegistry, RemovingAnImageFreesItsNumber)
{
    DecalRegistry                     registry;
    const std::shared_ptr<DecalImage> a      = registry.getDecalImage(entry("decals/a.png"));
    const std::shared_ptr<DecalImage> first  = registry.makeUniqueImage(a);
    const std::shared_ptr<DecalImage> second = registry.makeUniqueImage(a);
    EXPECT_EQ(second->getName(), "decals/a (2).png");
    EXPECT_TRUE(registry.removeDecal(first.get()));
    EXPECT_EQ(registry.makeUniqueName("a.png"), "decals/a (1).png");
    // Without the original and the copies the name itself is free.
    EXPECT_TRUE(registry.removeDecal(a.get()));
    EXPECT_TRUE(registry.removeDecal(second.get()));
    EXPECT_EQ(registry.makeUniqueName("a.png"), "decals/a.png");
    EXPECT_TRUE(registry.empty());
}

TEST(DecalRegistry, FindGivesTheImageOfAName)
{
    // An addition: a rocket/Decal holds the name of its image.
    const TempDir                     dir;
    DecalRegistry                     registry;
    const std::shared_ptr<DecalImage> fromEntry = registry.getDecalImage(entry("decals/a.png"));
    const std::shared_ptr<DecalImage> fromFile =
        registry.getDecalImage(file(dir.resolve("x/a.png")));
    const std::shared_ptr<DecalImage> slash =
        registry.getDecalImage(entry("/datafiles/textures/balsa.jpg"));

    EXPECT_EQ(registry.find("decals/a.png"), fromEntry);
    EXPECT_EQ(registry.find(fromFile->getName()), fromFile);
    EXPECT_EQ(registry.find("decals/a (1).png"), fromFile);
    EXPECT_EQ(registry.find("/datafiles/textures/balsa.jpg"), slash);
    // The name exactly: nothing is added, trimmed or folded.
    EXPECT_EQ(registry.find("a.png"), nullptr);
    EXPECT_EQ(registry.find("decals/A.png"), nullptr);
    EXPECT_EQ(registry.find("datafiles/textures/balsa.jpg"), nullptr);
    EXPECT_EQ(registry.find("decals/a.png "), nullptr);
    // find() registers nothing.
    EXPECT_EQ(registry.size(), 3U);
}

TEST(DecalRegistry, AnImageOutlivesItsRegistry)
{
    std::shared_ptr<DecalImage> image;
    {
        DecalRegistry registry;
        image = registry.getDecalImage(entry("decals/a.png", "A"));
        EXPECT_EQ(image.use_count(), 2);
    }
    EXPECT_EQ(image.use_count(), 1);
    EXPECT_EQ(image->getName(), "decals/a.png");
    EXPECT_EQ(bytesOrError(*image), "bytes A");
}

TEST(DecalRegistry, TheErrorOfAMissingImageNamesWhatTheRegistryKnows)
{
    // The scout's DocumentProbe2.out, section C: "getBytes of a missing zip entry:
    // DecalNotFoundException: Could not find decal source file 'decals/a.png'. <br> <br>Would
    // you like to look for this file?" and, for a file, the same with its absolute path.
    const TempDir                     dir;
    DecalRegistry                     registry;
    const std::shared_ptr<DecalImage> missingEntry  = registry.getDecalImage(entry("decals/a.png"));
    const Result<std::vector<std::byte>> entryBytes = missingEntry->getBytes();
    ASSERT_FALSE(entryBytes.has_value());
    EXPECT_EQ(entryBytes.error().code, ErrorCode::NOT_FOUND);
    EXPECT_EQ(entryBytes.error().message,
              "Could not find decal source file 'decals/a.png'. <br> "
              "<br>Would you like to look for this file?");

    const std::filesystem::path       missing = dir.resolve("does-not-exist.png");
    const std::shared_ptr<DecalImage> missingFile =
        registry.getDecalImage(std::make_shared<FileSystemAttachment>("x.png", missing));
    const Result<std::vector<std::byte>> fileBytes = missingFile->getBytes();
    ASSERT_FALSE(fileBytes.has_value());
    EXPECT_EQ(fileBytes.error().code, ErrorCode::NOT_FOUND);
    EXPECT_EQ(fileBytes.error().message,
              "Could not find decal source file '" +
                  QtRocket::pathToUtf8(std::filesystem::absolute(missing)) +
                  "'. <br> <br>Would you like to look for this file?");
}

}  // namespace
