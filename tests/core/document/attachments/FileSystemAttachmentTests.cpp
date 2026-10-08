#include "QtRocket/document/attachments/FileSystemAttachment.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/Attachment.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestTempDir.h"

// OpenRocket has no test of FileSystemAttachment. The Java behaviour is that of the probe
// DecalProbe.java, sections G and H (probes/tier9a-document-d2).

namespace
{

using QtRocket::Attachment;
using QtRocket::ErrorCode;
using QtRocket::FileSystemAttachment;
using QtRocket::pathToUtf8;
using QtRocket::Result;
using QtRocket::Test::TempDir;

static_assert(std::is_base_of_v<Attachment, FileSystemAttachment>);
static_assert(!std::is_copy_constructible_v<FileSystemAttachment>);
static_assert(!std::is_move_constructible_v<FileSystemAttachment>);

TEST(FileSystemAttachment, HasANameAndALocation)
{
    // Java: FileSystemAttachment name=n location=some/where.png toString=n
    const FileSystemAttachment attachment("n", "some/where.png");
    EXPECT_EQ(attachment.getName(), "n");
    EXPECT_EQ(attachment.getLocation(), std::filesystem::path("some/where.png"));
    EXPECT_EQ(attachment.toString(), "n");
}

TEST(FileSystemAttachment, TheNameIsNotTheFilesName)
{
    // The name is what the document calls the attachment; the order of attachments looks at it
    // and never at the location.
    const FileSystemAttachment first("decals/b.png", "x/a.png");
    const FileSystemAttachment second("decals/a.png", "x/b.png");
    EXPECT_GT(first.compareTo(second), 0);
    EXPECT_LT(second.compareTo(first), 0);
    EXPECT_EQ(first.compareTo(FileSystemAttachment("decals/b.png", "elsewhere/c.png")), 0);
}

TEST(FileSystemAttachment, ReadsTheBytesOfTheFile)
{
    const TempDir                dir;
    const std::vector<std::byte> data{std::byte{0}, std::byte{'\r'}, std::byte{'\n'},
                                      std::byte{0xFF}, std::byte{'a'}};
    const std::filesystem::path  file = dir.resolve("image.bin");
    ASSERT_TRUE(QtRocket::writeFile(file, data));

    const FileSystemAttachment           attachment("decals/image.bin", file);
    const Result<std::vector<std::byte>> bytes = attachment.getBytes();
    ASSERT_TRUE(bytes.has_value());
    EXPECT_EQ(*bytes, data);

    // Through the base class too.
    const std::unique_ptr<Attachment> base =
        std::make_unique<FileSystemAttachment>("decals/image.bin", file);
    const Result<std::vector<std::byte>> again = base->getBytes();
    ASSERT_TRUE(again.has_value());
    EXPECT_EQ(*again, data);
}

TEST(FileSystemAttachment, ReadsTheFileAnewEveryTime)
{
    // Java opens a new FileInputStream for every getBytes(): nothing is cached, so an image an
    // external editor rewrote is read as it now is.
    const TempDir               dir;
    const std::filesystem::path file = dir.write("image.txt", "before");
    const FileSystemAttachment  attachment("n", file);
    const auto                  before = attachment.getBytes();
    ASSERT_TRUE(before.has_value());
    EXPECT_EQ(QtRocket::bytesToString(*before), "before");

    ASSERT_TRUE(QtRocket::writeTextFile(file, "after the edit"));
    const auto after = attachment.getBytes();
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(QtRocket::bytesToString(*after), "after the edit");
}

TEST(FileSystemAttachment, AnEmptyFileHasNoBytes)
{
    const TempDir              dir;
    const FileSystemAttachment attachment("n", dir.write("empty.png", ""));
    const auto                 bytes = attachment.getBytes();
    ASSERT_TRUE(bytes.has_value());
    EXPECT_TRUE(bytes->empty());
}

TEST(FileSystemAttachment, AMissingFileIsNotFound)
{
    // Java: FileNotFoundException: <tmp>/does-not-exist.png (No such file or directory). The
    // motor loader takes it silently, as it takes a missing archive entry.
    const TempDir               dir;
    const std::filesystem::path missing = dir.resolve("does-not-exist.png");
    const FileSystemAttachment  attachment("x.png", missing);
    const auto                  bytes = attachment.getBytes();
    ASSERT_FALSE(bytes.has_value());
    EXPECT_EQ(bytes.error().code, ErrorCode::NOT_FOUND);
    EXPECT_EQ(bytes.error().message, "cannot open '" + pathToUtf8(missing) + "' for reading");

    // Making the attachment checks nothing: the file may appear later.
    ASSERT_TRUE(QtRocket::writeTextFile(missing, "now"));
    const auto now = attachment.getBytes();
    ASSERT_TRUE(now.has_value());
    EXPECT_EQ(QtRocket::bytesToString(*now), "now");
}

TEST(FileSystemAttachment, AFileInAMissingDirectoryIsNotFound)
{
    const TempDir              dir;
    const FileSystemAttachment attachment("n", dir.resolve("no/such/dir/a.png"));
    const auto                 bytes = attachment.getBytes();
    ASSERT_FALSE(bytes.has_value());
    EXPECT_EQ(bytes.error().code, ErrorCode::NOT_FOUND);
}

TEST(FileSystemAttachment, ADirectoryIsNotFound)
{
    // Java: FileNotFoundException: <tmp>/g (Is a directory).
    const TempDir              dir;
    const FileSystemAttachment attachment("d", dir.path());
    const auto                 bytes = attachment.getBytes();
    ASSERT_FALSE(bytes.has_value());
    EXPECT_EQ(bytes.error().code, ErrorCode::NOT_FOUND);
    EXPECT_EQ(bytes.error().message,
              "cannot read '" + pathToUtf8(dir.path()) + "': is a directory");
}

TEST(FileSystemAttachment, AnEmptyLocationIsNotFound)
{
    // Java: new File("") names nothing that can be opened.
    const FileSystemAttachment attachment("n", std::filesystem::path());
    const auto                 bytes = attachment.getBytes();
    ASSERT_FALSE(bytes.has_value());
    EXPECT_EQ(bytes.error().code, ErrorCode::NOT_FOUND);
}

TEST(FileSystemAttachment, ReadLocationIsWhatGetBytesDoes)
{
    const TempDir               dir;
    const std::filesystem::path file = dir.write("a.txt", "text");
    const auto                  read = FileSystemAttachment::readLocation(file);
    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(QtRocket::bytesToString(*read), "text");

    const auto missing = FileSystemAttachment::readLocation(dir.resolve("b.txt"));
    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error().code, ErrorCode::NOT_FOUND);
    const auto directory = FileSystemAttachment::readLocation(dir.path());
    ASSERT_FALSE(directory.has_value());
    EXPECT_EQ(directory.error().code, ErrorCode::NOT_FOUND);
}

// ---- the size limit and what is no regular file (not OpenRocket's: see the class comment) ------

/// What @p attachment's getBytes() gives: "bytes <text>", or "<code>: <message>".
[[nodiscard]] std::string bytesOrError(const Result<std::vector<std::byte>>& bytes)
{
    if (bytes.has_value())
    {
        return "bytes " + QtRocket::bytesToString(*bytes);
    }
    return std::string(QtRocket::toString(bytes.error().code)) + ": " + bytes.error().message;
}

TEST(FileSystemAttachment, TheLimitIsTheOneOfAnArchiveEntryUnlessItIsGiven)
{
    // ZipFileAttachment.MAX_ATTACHMENT_BYTES: 32 MiB.
    EXPECT_EQ(Attachment::kMaxAttachmentBytes, 33554432U);
    EXPECT_EQ(FileSystemAttachment("n", "a.png").getMaxAttachmentBytes(),
              Attachment::kMaxAttachmentBytes);
    EXPECT_EQ(FileSystemAttachment("n", "a.png", 7).getMaxAttachmentBytes(), 7U);
}

TEST(FileSystemAttachment, AFileBeyondTheLimitIsRefusedWithTheTextsOfAnArchiveEntry)
{
    const TempDir               dir;
    const std::filesystem::path file = dir.write("five.rse", "12345");

    EXPECT_EQ(bytesOrError(FileSystemAttachment("thrustcurves/five.rse", file, 5).getBytes()),
              "bytes 12345");
    // The file says it is too large: ZipFileAttachment's text for an entry that declares more.
    EXPECT_EQ(bytesOrError(FileSystemAttachment("thrustcurves/five.rse", file, 4).getBytes()),
              "IO: Attachment 'thrustcurves/five.rse' exceeds the maximum size of 4 bytes");
    // readLocation() knows no name: FileUtils' text.
    EXPECT_EQ(bytesOrError(FileSystemAttachment::readLocation(file, 4)),
              "IO: Input exceeds maximum size of 4 bytes");
    EXPECT_EQ(bytesOrError(FileSystemAttachment::readLocation(file, 5)), "bytes 12345");
}

/// A file under @p dir that says it holds @p size bytes (sparse where the file system has
/// sparse files); an empty path when it cannot be made.
[[nodiscard]] std::filesystem::path makeFileOfSize(const TempDir& dir, std::uintmax_t size)
{
    const std::filesystem::path file = dir.write("big.png", "x");
    std::error_code             error;
    std::filesystem::resize_file(file, size, error);
    return error ? std::filesystem::path() : file;
}

// Hostile input: with the default limit a file of 32 MiB and one byte is refused by its size,
// unread.
TEST(FileSystemAttachment, TheDefaultLimitRefusesAFileOfMoreThan32MiB)
{
    const TempDir               dir;
    const std::filesystem::path file = makeFileOfSize(dir, Attachment::kMaxAttachmentBytes + 1);
    ASSERT_FALSE(file.empty());

    EXPECT_EQ(bytesOrError(FileSystemAttachment("decals/big.png", file).getBytes()),
              "IO: Attachment 'decals/big.png' exceeds the maximum size of 33554432 bytes");
    EXPECT_EQ(bytesOrError(FileSystemAttachment::readLocation(file)),
              "IO: Input exceeds maximum size of 33554432 bytes");
}

/// Whether this machine has the device @p device (a POSIX system).
[[nodiscard]] bool hasDevice(const std::filesystem::path& device)
{
    std::error_code error;
    return std::filesystem::is_character_file(device, error);
}

/// What reading the device @p device as an attachment gives, by getBytes() and by
/// readLocation(), which must agree; "differ" when they do not.
[[nodiscard]] std::string readDevice(const std::string& device)
{
    const std::string byAttachment = bytesOrError(FileSystemAttachment("n", device).getBytes());
    const std::string byLocation   = bytesOrError(FileSystemAttachment::readLocation(device, 16));
    return byAttachment == byLocation ? byAttachment : "differ";
}

// Hostile input: a name can lead to a device. Java opens it and reads until its consumer gives
// up (or for ever); here it is not opened.
TEST(FileSystemAttachment, WhatIsNoRegularFileIsNotRead)
{
    if (!hasDevice("/dev/zero") || !hasDevice("/dev/null"))
    {
        GTEST_SKIP() << "no /dev/zero and /dev/null here";
    }
    EXPECT_EQ(readDevice("/dev/zero"), "IO: cannot read '/dev/zero': not a regular file");
    EXPECT_EQ(readDevice("/dev/null"), "IO: cannot read '/dev/null': not a regular file");
}

/// Makes @p link a symbolic link to @p target; false where links cannot be made.
[[nodiscard]] bool makeLink(const std::filesystem::path& target, const std::filesystem::path& link)
{
    std::error_code error;
    std::filesystem::create_symlink(target, link, error);
    return !error;
}

TEST(FileSystemAttachment, ALinkIsWhatItLeadsTo)
{
    const TempDir               dir;
    const std::filesystem::path file = dir.write("real.png", "image");
    const std::filesystem::path link = dir.resolve("link.png");
    if (!makeLink(file, link))
    {
        GTEST_SKIP() << "cannot create links here";
    }
    EXPECT_EQ(bytesOrError(FileSystemAttachment("n", link).getBytes()), "bytes image");
    // A link to a file beyond the limit is beyond the limit.
    EXPECT_EQ(bytesOrError(FileSystemAttachment("n", link, 2).getBytes()),
              "IO: Attachment 'n' exceeds the maximum size of 2 bytes");
}

TEST(FileSystemAttachment, ALinkToADeviceIsNotRead)
{
    const TempDir               dir;
    const std::filesystem::path endless = dir.resolve("endless.png");
    if (!hasDevice("/dev/zero") || !makeLink("/dev/zero", endless))
    {
        GTEST_SKIP() << "no /dev/zero or no links here";
    }
    EXPECT_EQ(bytesOrError(FileSystemAttachment("n", endless).getBytes()),
              "IO: cannot read '" + pathToUtf8(endless) + "': not a regular file");
}

TEST(FileSystemAttachment, AFileNameOutsideAsciiIsRead)
{
    // The path is a std::filesystem::path, so a name outside the platform's narrow character
    // set is read, and the message of a failure has it in UTF-8.
    const TempDir               dir;
    const std::filesystem::path file =
        dir.path() / std::filesystem::path(u8"d\u00E9cor \u706B.png");
    ASSERT_TRUE(QtRocket::writeTextFile(file, "image"));
    const FileSystemAttachment attachment("decals/decor.png", file);
    const auto                 bytes = attachment.getBytes();
    ASSERT_TRUE(bytes.has_value());
    EXPECT_EQ(QtRocket::bytesToString(*bytes), "image");

    const std::filesystem::path missing =
        dir.path() / std::filesystem::path(u8"abs\u00E9nt \u706B.png");
    const auto none = FileSystemAttachment("n", missing).getBytes();
    ASSERT_FALSE(none.has_value());
    EXPECT_EQ(none.error().message, "cannot open '" + pathToUtf8(missing) + "' for reading");
    EXPECT_NE(none.error().message.find("abs\xC3\xA9nt \xE7\x81\xAB.png"), std::string::npos);
}

}  // namespace
