#include "QtRocket/document/DecalImage.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/Attachment.h"
#include "QtRocket/document/attachments/FileSystemAttachment.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestTempDir.h"
#include "document/TestAttachments.h"
#include "simulation/SimulationOptionsSupport.h"

// OpenRocket has no test of DecalImage or of DecalRegistry.DecalImageImpl. The Java behaviour is
// that of the probe DecalProbe.java, sections D, E, G and H (probes/tier9a-document-d2), and of
// the scout's DocumentProbe2.out, section C.

namespace
{

using QtRocket::Attachment;
using QtRocket::BugError;
using QtRocket::DecalImage;
using QtRocket::ErrorCode;
using QtRocket::FileSystemAttachment;
using QtRocket::pathToUtf8;
using QtRocket::Result;
using QtRocket::Test::ChangeCounter;
using QtRocket::Test::FailingAttachment;
using QtRocket::Test::MemoryAttachment;
using QtRocket::Test::TempDir;

// An image is shared and heard: it stays where it is.
static_assert(!std::is_copy_constructible_v<DecalImage>);
static_assert(!std::is_move_constructible_v<DecalImage>);
static_assert(!std::is_copy_assignable_v<DecalImage>);
static_assert(!std::is_move_assignable_v<DecalImage>);
// No implicit conversion from an attachment.
static_assert(!std::is_convertible_v<std::shared_ptr<const Attachment>, DecalImage>);

/// Java's message for a decal whose source @p source (a name or an absolute path) is missing.
[[nodiscard]] std::string notFoundText(const std::string& source)
{
    return "Could not find decal source file '" + source +
           "'. <br> <br>Would you like to look for this file?";
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

/// The text of the file @p file, or "<unreadable>".
[[nodiscard]] std::string textOf(const std::filesystem::path& file)
{
    return QtRocket::readTextFile(file).value_or("<unreadable>");
}

TEST(DecalImage, TakesItsNameFromTheAttachmentOrHasItsOwn)
{
    // Java: getName() is `name != null ? name : delegate.getName()`.
    const auto       attachment = std::make_shared<MemoryAttachment>("decals/a.png", "A");
    const DecalImage unnamed(attachment);
    EXPECT_EQ(unnamed.getName(), "decals/a.png");
    EXPECT_EQ(unnamed.toString(), "decals/a.png");

    const DecalImage named("decals/a (1).png", attachment);
    EXPECT_EQ(named.getName(), "decals/a (1).png");
    EXPECT_EQ(named.toString(), "decals/a (1).png");

    // An empty name is a name of its own, not "none".
    const DecalImage empty("", attachment);
    EXPECT_EQ(empty.getName(), "");
}

TEST(DecalImage, ANewImageHasNoDecalFileAndIsNotIgnored)
{
    // Java: decalFile=null ignored=false
    const DecalImage image(std::make_shared<MemoryAttachment>("decals/a.png", "A"));
    EXPECT_EQ(image.getDecalFile(), std::nullopt);
    EXPECT_FALSE(image.isIgnored());
}

TEST(DecalImage, ANullAttachmentIsABug)
{
    // Java: a NullPointerException at the first getName() or getBytes().
    EXPECT_THROW(const DecalImage image(nullptr), BugError);
    EXPECT_THROW(const DecalImage image("decals/a.png", nullptr), BugError);
}

TEST(DecalImage, KeepsItsAttachmentAlive)
{
    std::shared_ptr<MemoryAttachment> attachment =
        std::make_shared<MemoryAttachment>("decals/a.png", "A");
    const DecalImage image(attachment);
    EXPECT_EQ(attachment.use_count(), 2);
    attachment.reset();
    EXPECT_EQ(image.getName(), "decals/a.png");
    EXPECT_EQ(bytesOrError(image), "bytes A");
}

TEST(DecalImage, ReadsTheAttachmentWithoutADecalFile)
{
    // Java: "export of an attachment image: MEM".
    const auto       attachment = std::make_shared<MemoryAttachment>("decals/mem.png", "MEM");
    const DecalImage image(attachment);
    EXPECT_EQ(bytesOrError(image), "bytes MEM");
    EXPECT_EQ(attachment->reads(), 1);
    // Nothing is cached.
    EXPECT_EQ(bytesOrError(image), "bytes MEM");
    EXPECT_EQ(attachment->reads(), 2);
}

TEST(DecalImage, ReadsTheDecalFileFirst)
{
    // Java: "with a decal file: bytes DECALFILE name='decals/mem.png'": the file supplies the
    // bytes, the name stays, and the attachment is not asked.
    const TempDir dir;
    const auto    attachment = std::make_shared<MemoryAttachment>("decals/mem.png", "MEM");
    DecalImage    image(attachment);
    const std::filesystem::path decalFile = dir.write("g/decal.bin", "DECALFILE");
    image.setDecalFile(decalFile);
    EXPECT_EQ(image.getDecalFile(), decalFile);
    EXPECT_EQ(bytesOrError(image), "bytes DECALFILE");
    EXPECT_EQ(image.getName(), "decals/mem.png");
    EXPECT_EQ(attachment->reads(), 0);

    // The file is read anew every time (an external editor rewrites it).
    ASSERT_TRUE(QtRocket::writeTextFile(decalFile, "EDITED"));
    EXPECT_EQ(bytesOrError(image), "bytes EDITED");

    // Java: "decal file cleared: bytes MEM".
    image.setDecalFile(std::nullopt);
    EXPECT_EQ(image.getDecalFile(), std::nullopt);
    EXPECT_EQ(bytesOrError(image), "bytes MEM");
    EXPECT_EQ(attachment->reads(), 1);
}

TEST(DecalImage, AMissingDecalFileIsNotFoundWithItsAbsolutePath)
{
    // Java: "decal file deleted: DecalNotFoundException: Could not find decal source file
    // '<tmp>/g/decal.bin'. <br> <br>Would you like to look for this file?", although the
    // attachment could supply the bytes.
    const TempDir dir;
    const auto    attachment = std::make_shared<MemoryAttachment>("decals/mem.png", "MEM");
    DecalImage    image(attachment);
    const std::filesystem::path decalFile = dir.write("g/decal.bin", "DECALFILE");
    image.setDecalFile(decalFile);
    ASSERT_TRUE(std::filesystem::remove(decalFile));

    const Result<std::vector<std::byte>> bytes = image.getBytes();
    ASSERT_FALSE(bytes.has_value());
    EXPECT_EQ(bytes.error().code, ErrorCode::NOT_FOUND);
    EXPECT_EQ(bytes.error().message,
              notFoundText(pathToUtf8(std::filesystem::absolute(decalFile))));
    EXPECT_EQ(attachment->reads(), 0);
}

TEST(DecalImage, ARelativeDecalFileIsReportedFromTheCurrentDirectory)
{
    // Java: "relative decal file that is missing: ... '<cwd>/relative/none.png' ..."
    // (File.getAbsolutePath()).
    DecalImage                  image(std::make_shared<MemoryAttachment>("decals/mem.png", "MEM"));
    const std::filesystem::path relative("qtrocket-no-such-directory/none.png");
    image.setDecalFile(relative);

    const Result<std::vector<std::byte>> bytes = image.getBytes();
    ASSERT_FALSE(bytes.has_value());
    EXPECT_EQ(bytes.error().code, ErrorCode::NOT_FOUND);
    EXPECT_EQ(bytes.error().message, notFoundText(pathToUtf8(std::filesystem::absolute(relative))));
    EXPECT_NE(bytes.error().message.find(pathToUtf8(std::filesystem::current_path())),
              std::string::npos);
}

TEST(DecalImage, AnEmptyDecalFileIsNotNoDecalFile)
{
    // Java: new File("") is a file that does not exist, and its absolute path is the current
    // directory. It is asked first like any other decal file.
    DecalImage image(std::make_shared<MemoryAttachment>("decals/mem.png", "MEM"));
    image.setDecalFile(std::filesystem::path());
    ASSERT_TRUE(image.getDecalFile().has_value());

    const Result<std::vector<std::byte>> bytes = image.getBytes();
    ASSERT_FALSE(bytes.has_value());
    EXPECT_EQ(bytes.error().code, ErrorCode::NOT_FOUND);
    EXPECT_EQ(bytes.error().message, notFoundText(pathToUtf8(std::filesystem::current_path())));
}

TEST(DecalImage, ADecalFileThatIsADirectoryIsNotFoundAsAFile)
{
    // Java: "decal file is a directory: FileNotFoundException: <tmp>/g (Is a directory)". It
    // exists, so it is not the decal message; it cannot be opened, so it is "not found".
    const TempDir dir;
    DecalImage    image(std::make_shared<MemoryAttachment>("decals/mem.png", "MEM"));
    image.setDecalFile(dir.path());

    const Result<std::vector<std::byte>> bytes = image.getBytes();
    ASSERT_FALSE(bytes.has_value());
    EXPECT_EQ(bytes.error().code, ErrorCode::NOT_FOUND);
    EXPECT_EQ(bytes.error().message,
              "cannot read '" + pathToUtf8(dir.path()) + "': is a directory");
}

TEST(DecalImage, AnAttachmentThatIsNotFoundIsReportedByItsName)
{
    // Java: "attachment without data: DecalNotFoundException: Could not find decal source file
    // 'decals/gone.png'. <br> <br>Would you like to look for this file?" (the image catches the
    // attachment's exception and throws its own with delegate.getName()).
    const DecalImage image(std::make_shared<FailingAttachment>("decals/gone.png"));
    EXPECT_EQ(bytesOrError(image), "NOT_FOUND: " + notFoundText("decals/gone.png"));

    // The attachment's name, also when the image has a name of its own.
    const DecalImage named("decals/gone (1).png",
                           std::make_shared<FailingAttachment>("decals/gone.png"));
    EXPECT_EQ(bytesOrError(named), "NOT_FOUND: " + notFoundText("decals/gone.png"));

    // And whatever the attachment's own text was.
    const DecalImage other(std::make_shared<FailingAttachment>("decals/x.png", ErrorCode::NOT_FOUND,
                                                               "no entry of that name"));
    EXPECT_EQ(bytesOrError(other), "NOT_FOUND: " + notFoundText("decals/x.png"));
}

TEST(DecalImage, AnyOtherFailureOfTheAttachmentIsPassedOn)
{
    // Java: an IOException of the attachment is not caught by the image.
    const DecalImage io(std::make_shared<FailingAttachment>(
        "decals/big.png", ErrorCode::IO,
        "Attachment 'decals/big.png' exceeds the maximum size of 64 bytes"));
    EXPECT_EQ(bytesOrError(io),
              "IO: Attachment 'decals/big.png' exceeds the maximum size of 64 bytes");
    const DecalImage parse(std::make_shared<FailingAttachment>("decals/bad.png", ErrorCode::PARSE,
                                                               "the archive is damaged"));
    EXPECT_EQ(bytesOrError(parse), "PARSE: the archive is damaged");
}

TEST(DecalImage, AFileSystemAttachmentWithoutADecalFile)
{
    // The registry gives the image of a file the file as its decal file, so this is reached
    // only after the decal file was cleared. Java: "the same with the decal file cleared:
    // FileNotFoundException: <tmp>/does-not-exist.png (No such file or directory)". Deviation:
    // here it is the decal message with the attachment's name, as for every attachment whose
    // source is missing.
    const TempDir               dir;
    const std::filesystem::path file = dir.resolve("does-not-exist.png");
    const DecalImage            image(std::make_shared<FileSystemAttachment>("x.png", file));
    EXPECT_EQ(bytesOrError(image), "NOT_FOUND: " + notFoundText("x.png"));

    ASSERT_TRUE(QtRocket::writeTextFile(file, "there now"));
    EXPECT_EQ(bytesOrError(image), "bytes there now");
}

TEST(DecalImage, ExportWritesTheBytes)
{
    // Java: "export of an attachment image: MEM", "export: DECALFILE".
    const TempDir               dir;
    const std::filesystem::path out = dir.resolve("exported.bin");
    DecalImage                  image(std::make_shared<MemoryAttachment>("decals/mem.png", "MEM"));
    ASSERT_TRUE(image.exportImage(out));
    EXPECT_EQ(textOf(out), "MEM");

    const std::filesystem::path decalFile = dir.write("g/decal.bin", "DECALFILE");
    image.setDecalFile(decalFile);
    ASSERT_TRUE(image.exportImage(out));
    EXPECT_EQ(textOf(out), "DECALFILE");

    // Java: "export onto the decal file itself: DECALFILE": the bytes are read before the file
    // is opened for writing.
    ASSERT_TRUE(image.exportImage(decalFile));
    EXPECT_EQ(textOf(decalFile), "DECALFILE");
}

TEST(DecalImage, ExportOfAMissingImageLeavesTheTargetAlone)
{
    // Java: "export of it: DecalNotFoundException file created=false".
    const TempDir               dir;
    const std::filesystem::path out = dir.resolve("exported.bin");
    const DecalImage            image(std::make_shared<FailingAttachment>("decals/gone.png"));
    const Result<void>          exported = image.exportImage(out);
    ASSERT_FALSE(exported.has_value());
    EXPECT_EQ(exported.error().code, ErrorCode::NOT_FOUND);
    EXPECT_EQ(exported.error().message, notFoundText("decals/gone.png"));
    EXPECT_FALSE(std::filesystem::exists(out));

    // An existing target keeps its content.
    const std::filesystem::path existing = dir.write("existing.bin", "KEEP");
    EXPECT_FALSE(image.exportImage(existing).has_value());
    EXPECT_EQ(textOf(existing), "KEEP");
}

TEST(DecalImage, ExportIntoAMissingDirectoryFails)
{
    // Java: a FileNotFoundException from the FileOutputStream.
    const TempDir      dir;
    const DecalImage   image(std::make_shared<MemoryAttachment>("decals/mem.png", "MEM"));
    const Result<void> exported = image.exportImage(dir.resolve("no/such/dir/out.bin"));
    ASSERT_FALSE(exported.has_value());
    EXPECT_EQ(exported.error().code, ErrorCode::IO);
}

TEST(DecalImage, TheIgnoredFlag)
{
    DecalImage image(std::make_shared<MemoryAttachment>("decals/a.png", "A"));
    image.setIgnored(true);
    EXPECT_TRUE(image.isIgnored());
    // The flag does not change what the image is or gives.
    EXPECT_EQ(image.getName(), "decals/a.png");
    EXPECT_EQ(bytesOrError(image), "bytes A");
    image.setIgnored(false);
    EXPECT_FALSE(image.isIgnored());
}

TEST(DecalImage, IsOrderedByItsName)
{
    // Java: compareTo() is getName().compareTo(o.getName()): the image's own name counts, not
    // the attachment's.
    const auto       attachment = std::make_shared<MemoryAttachment>("decals/m.png", "M");
    const DecalImage m(attachment);
    const DecalImage a("decals/a.png", attachment);
    const DecalImage z("decals/z.png", attachment);
    EXPECT_GT(m.compareTo(a), 0);
    EXPECT_LT(m.compareTo(z), 0);
    EXPECT_EQ(m.compareTo(m), 0);
    EXPECT_EQ(m.compareTo(DecalImage("decals/m.png", attachment)), 0);
    // String.compareTo: the difference of the first differing UTF-16 code units ('a' - 'B'),
    // and U+1F600, a surrogate pair, before U+FFEE.
    EXPECT_EQ(DecalImage("a", attachment).compareTo(DecalImage("B", attachment)), 31);
    EXPECT_EQ(DecalImage("\xF0\x9F\x98\x80", attachment)
                  .compareTo(DecalImage("\xEF\xBF\xAE", attachment)),
              -10161);
}

TEST(DecalImage, FireChangeEventReachesTheListeners)
{
    DecalImage          image(std::make_shared<MemoryAttachment>("decals/a.png", "A"));
    const ChangeCounter heard(image.changed());
    image.fireChangeEvent();
    EXPECT_EQ(heard.count(), 1);
    image.fireChangeEvent();
    EXPECT_EQ(heard.count(), 2);
}

TEST(DecalImage, NoSetterFires)
{
    // Java: neither setDecalFile() nor setIgnored() fires; OpenRocket fires by hand after an
    // external editor has rewritten the decal file.
    const TempDir       dir;
    DecalImage          image(std::make_shared<MemoryAttachment>("decals/a.png", "A"));
    const ChangeCounter heard(image.changed());
    image.setDecalFile(dir.write("a.png", "FILE"));
    image.setIgnored(true);
    image.setIgnored(false);
    image.setDecalFile(std::nullopt);
    EXPECT_TRUE(image.getBytes().has_value());
    EXPECT_TRUE(image.exportImage(dir.resolve("out.png")).has_value());
    EXPECT_EQ(heard.count(), 0);
}

TEST(DecalImage, TheAttachmentsChangeIsNotTheImages)
{
    // Java: the image has a change source of its own and does not listen to its attachment.
    const auto          attachment = std::make_shared<MemoryAttachment>("decals/a.png", "A");
    DecalImage          image(attachment);
    const ChangeCounter heard(image.changed());
    attachment->fireChangeEvent();
    EXPECT_EQ(heard.count(), 0);
}

TEST(DecalImage, ACopyHasTheAttachmentAndTheDecalFileUnderAnotherName)
{
    // Java: clone() makes a DecalImageImpl of the same delegate and copies the decal file; the
    // registry then names it. "registered 'decals/a.png' -> 'decals/a (1).png' same=false
    // ignored=false bytes A".
    const TempDir dir;
    const auto    attachment = std::make_shared<MemoryAttachment>("decals/a.png", "A");
    DecalImage    original(attachment);
    original.setIgnored(true);

    const std::shared_ptr<DecalImage> copy = original.copyWithName("decals/a (1).png");
    ASSERT_NE(copy, nullptr);
    EXPECT_NE(copy.get(), &original);
    EXPECT_EQ(copy->getName(), "decals/a (1).png");
    EXPECT_EQ(original.getName(), "decals/a.png");
    EXPECT_FALSE(copy->isIgnored());
    EXPECT_EQ(copy->getDecalFile(), std::nullopt);
    EXPECT_EQ(bytesOrError(*copy), "bytes A");
    // The same attachment object: the copy read it.
    EXPECT_EQ(attachment->reads(), 1);

    // Java: "file image 'decals/b.png' -> 'decals/b (1).png' decalFile same=true bytes file-y".
    const std::filesystem::path decalFile = dir.write("y/b.png", "file-y");
    original.setDecalFile(decalFile);
    const std::shared_ptr<DecalImage> fileCopy = original.copyWithName("decals/b (1).png");
    EXPECT_EQ(fileCopy->getDecalFile(), decalFile);
    EXPECT_EQ(bytesOrError(*fileCopy), "bytes file-y");
    // Afterwards each has a decal file of its own to change.
    fileCopy->setDecalFile(std::nullopt);
    EXPECT_EQ(original.getDecalFile(), decalFile);
}

TEST(DecalImage, ACopyDoesNotShareTheListeners)
{
    // Java: "the copy fires: original's listener heard 0", "the original fires: heard 1".
    DecalImage          original(std::make_shared<MemoryAttachment>("decals/a.png", "A"));
    const ChangeCounter heardOriginal(original.changed());
    const std::shared_ptr<DecalImage> copy = original.copyWithName("decals/a (1).png");
    const ChangeCounter               heardCopy(copy->changed());

    copy->fireChangeEvent();
    EXPECT_EQ(heardOriginal.count(), 0);
    EXPECT_EQ(heardCopy.count(), 1);
    original.fireChangeEvent();
    EXPECT_EQ(heardOriginal.count(), 1);
    EXPECT_EQ(heardCopy.count(), 1);
}

TEST(DecalImage, ACopyOutlivesItsOriginal)
{
    std::shared_ptr<DecalImage> copy;
    {
        const DecalImage original("decals/a.png",
                                  std::make_shared<MemoryAttachment>("ignored name", "A"));
        copy = original.copyWithName("decals/a (1).png");
    }
    EXPECT_EQ(copy->getName(), "decals/a (1).png");
    EXPECT_EQ(bytesOrError(*copy), "bytes A");
}

}  // namespace
