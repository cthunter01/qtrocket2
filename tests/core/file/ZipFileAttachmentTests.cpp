#include "QtRocket/file/ZipFileAttachment.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/Attachment.h"
#include "QtRocket/file/ZipArchive.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "file/RawZip.h"

// The expectations are what OpenRocket's ZipFileAttachment answers for the same archives
// (probe ZipProbe of part D4; the archives of sections A and B are Java's own bytes).

namespace
{

using QtRocket::Attachment;
using QtRocket::BugError;
using QtRocket::ErrorCode;
using QtRocket::Result;
using QtRocket::ZipFileAttachment;
using QtRocket::Test::archive;
using QtRocket::Test::bytesFromHex;
using QtRocket::Test::deflatedHello;
using QtRocket::Test::kJavaArchiveOf65Zeros;
using QtRocket::Test::kJavaArchiveOfFourBytes;
using QtRocket::Test::LocalEntry;
using QtRocket::Test::stored;

[[nodiscard]] ZipFileAttachment::Archive shared(std::vector<std::byte> bytes)
{
    return std::make_shared<const std::vector<std::byte>>(std::move(bytes));
}

/// What the attachment @p name of @p data answers with the limit @p maxBytes: its contents, or
/// "<code>: <message>".
[[nodiscard]] std::string ask(const std::vector<std::byte>& data, std::string_view name,
                              std::size_t maxBytes = ZipFileAttachment::kMaxAttachmentBytes)
{
    const ZipFileAttachment              attachment(std::string(name), shared(data), maxBytes);
    const Result<std::vector<std::byte>> bytes = attachment.getBytes();
    if (!bytes)
    {
        return std::string(QtRocket::toString(bytes.error().code)) + ": " + bytes.error().message;
    }
    return QtRocket::bytesToString(*bytes);
}

/// The message of the failure for a missing attachment named @p name.
[[nodiscard]] std::string notFound(std::string_view name)
{
    return "NOT_FOUND: " + QtRocket::decalNotFound(name).error().message;
}

/// An archive as OpenRocket writes one: the design first, a directory entry, decals, a texture
/// whose name starts with a slash (as one of OpenRocket's examples has them) and holds spaces
/// and parentheses, an embedded thrust curve, a name that is not ASCII.
[[nodiscard]] std::vector<std::byte> namedArchive()
{
    QtRocket::ZipWriter writer;
    writer.add("rocket.ork", QtRocket::stringToBytes("<openrocket/>"));
    writer.add("decals/", {});
    writer.add("decals/a.png", QtRocket::stringToBytes("A"));
    writer.add("/datafiles/textures/x y (1).jpg", QtRocket::stringToBytes("slash"));
    writer.add("Decals/A.png", QtRocket::stringToBytes("upper"));
    writer.add("thrustcurves/abc.rse", QtRocket::stringToBytes("rse"));
    writer.add("caf\xC3\xA9.png", QtRocket::stringToBytes("utf8"));
    return writer.finish().value();
}

// ZipFileAttachmentTest.readsAttachmentWithinConfiguredLimit
TEST(ZipFileAttachment, ReadsAttachmentWithinConfiguredLimit)
{
    const std::vector<std::byte> contents{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    const ZipFileAttachment attachment("decal.png", shared(bytesFromHex(kJavaArchiveOfFourBytes)),
                                       4);

    const Result<std::vector<std::byte>> bytes = attachment.getBytes();
    ASSERT_TRUE(bytes.has_value());
    EXPECT_EQ(*bytes, contents);
}

// ZipFileAttachmentTest.rejectsInflatedAttachmentBeyondConfiguredLimit
TEST(ZipFileAttachment, RejectsInflatedAttachmentBeyondConfiguredLimit)
{
    const ZipFileAttachment attachment("decal.png", shared(bytesFromHex(kJavaArchiveOf65Zeros)),
                                       64);

    const Result<std::vector<std::byte>> bytes = attachment.getBytes();
    ASSERT_FALSE(bytes.has_value());
    EXPECT_EQ(bytes.error().code, ErrorCode::IO);
    EXPECT_TRUE(bytes.error().message.contains("64"));
    // The whole message: the archive is Java's, whose entry does not declare its size, so the
    // limit is met while inflating.
    EXPECT_EQ(bytes.error().message, "Input exceeds maximum size of 64 bytes");
}

TEST(ZipFileAttachment, TheLimitIsTheMostBytesItReturns)
{
    const std::vector<std::byte> four  = bytesFromHex(kJavaArchiveOfFourBytes);
    const std::vector<std::byte> zeros = bytesFromHex(kJavaArchiveOf65Zeros);
    EXPECT_EQ(ask(four, "decal.png", 4), "\x01\x02\x03\x04");
    EXPECT_EQ(ask(four, "decal.png", 3), "IO: Input exceeds maximum size of 3 bytes");
    EXPECT_EQ(ask(four, "decal.png", 0), "IO: Input exceeds maximum size of 0 bytes");
    EXPECT_EQ(ask(zeros, "decal.png", 65), std::string(65, '\0'));
    EXPECT_EQ(ask(zeros, "decal.png"), std::string(65, '\0'));

    EXPECT_EQ(ZipFileAttachment::kMaxAttachmentBytes, 33554432U);
    EXPECT_EQ(ZipFileAttachment("a", shared({})).getMaxAttachmentBytes(), 33554432U);
    EXPECT_EQ(ZipFileAttachment("a", shared({}), 7).getMaxAttachmentBytes(), 7U);
}

TEST(ZipFileAttachment, AnEntryThatDeclaresMoreThanTheLimitIsRefusedUnread)
{
    // Java's archive of two stored entries, "decal.png" of 65 zero bytes and "after.txt".
    const std::vector<std::byte> data = bytesFromHex(
        "504b03040a0000080000a8b5475d77f7cd1d410000004100000009000000646563616c2e706e67"
        "0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
        "000000000000000000000000000000000000000000504b03040a0000080000a8b5475d414e448905000000"
        "050000000900000061667465722e7478746166746572504b01020a000a0000080000a8b5475d77f7cd1d41"
        "00000041000000090000000000000000000000000000000000646563616c2e706e67504b01020a000a0000"
        "080000a8b5475d414e4489050000000500000009000000000000000000000000006800000061667465722e"
        "747874504b050600000000020002006e000000940000000000");
    EXPECT_EQ(ask(data, "decal.png", 64),
              "IO: Attachment 'decal.png' exceeds the maximum size of 64 bytes");
    EXPECT_EQ(ask(data, "decal.png", 65), std::string(65, '\0'));
    EXPECT_EQ(ask(data, "after.txt", 5), "after");
    EXPECT_EQ(ask(data, "after.txt", 4),
              "IO: Attachment 'after.txt' exceeds the maximum size of 4 bytes");
}

TEST(ZipFileAttachment, FindsAnEntryByItsExactName)
{
    const std::vector<std::byte> data = namedArchive();
    EXPECT_EQ(ask(data, "decals/a.png"), "A");
    EXPECT_EQ(ask(data, "rocket.ork"), "<openrocket/>");
    EXPECT_EQ(ask(data, "thrustcurves/abc.rse"), "rse");
    EXPECT_EQ(ask(data, "caf\xC3\xA9.png"), "utf8");
    // A leading slash, spaces and parentheses are part of the name.
    EXPECT_EQ(ask(data, "/datafiles/textures/x y (1).jpg"), "slash");
    EXPECT_EQ(ask(data, "datafiles/textures/x y (1).jpg"),
              notFound("datafiles/textures/x y (1).jpg"));
    // So is the case, and nothing is normalised.
    EXPECT_EQ(ask(data, "Decals/A.png"), "upper");
    EXPECT_EQ(ask(data, "DECALS/A.PNG"), notFound("DECALS/A.PNG"));
    EXPECT_EQ(ask(data, "./decals/a.png"), notFound("./decals/a.png"));
    EXPECT_EQ(ask(data, "decals//a.png"), notFound("decals//a.png"));
    EXPECT_EQ(ask(data, "decals\\a.png"), notFound("decals\\a.png"));
    EXPECT_EQ(ask(data, "missing.png"), notFound("missing.png"));
    EXPECT_EQ(ask(data, ""), notFound(""));
}

TEST(ZipFileAttachment, ADirectoryEntryIsAnEntryWithoutContents)
{
    const std::vector<std::byte> data = namedArchive();
    EXPECT_EQ(ask(data, "decals/"), "");
    EXPECT_EQ(ask(data, "decals"), notFound("decals"));
    // A raw archive whose directory entry is stored, before the entry asked for.
    EXPECT_EQ(ask(archive({stored("dir/", ""), stored("dir/a", "x")}), "dir/a"), "x");
}

TEST(ZipFileAttachment, ReadsStoredAndDeflatedEntries)
{
    const std::vector<std::byte> data =
        archive({stored("a.txt", "stored"), deflatedHello("b.txt"), stored("c.txt", "last")});
    EXPECT_EQ(ask(data, "a.txt"), "stored");
    EXPECT_EQ(ask(data, "b.txt"), "hello");
    EXPECT_EQ(ask(data, "c.txt"), "last");
    EXPECT_EQ(ask(data, "b.txt", 5), "hello");
    EXPECT_EQ(ask(data, "b.txt", 4), "IO: Attachment 'b.txt' exceeds the maximum size of 4 bytes");
}

TEST(ZipFileAttachment, TheFirstOfTwoEntriesWithOneNameIsTheAttachment)
{
    // Java's ZipOutputStream refuses to write a name twice; a reader takes the first.
    const std::vector<std::byte> data =
        archive({stored("a.png", "first"), stored("a.png", "second")});
    EXPECT_EQ(ask(data, "a.png"), "first");
}

TEST(ZipFileAttachment, AnArchiveWithoutEntriesHasNoAttachment)
{
    EXPECT_EQ(ask({}, "a"), notFound("a"));
    EXPECT_EQ(ask(QtRocket::stringToBytes("this is not a zip archive at all"), "a"), notFound("a"));
    const Result<std::vector<std::byte>> bytes = ZipFileAttachment("a", shared({})).getBytes();
    ASSERT_FALSE(bytes.has_value());
    EXPECT_EQ(bytes.error().code, ErrorCode::NOT_FOUND);
}

TEST(ZipFileAttachment, ATruncatedArchiveFailsWithTheReadersError)
{
    // Cut inside the first entry: whatever is asked for, the walk fails there. (Java: an
    // EOFException without a message.)
    std::vector<std::byte> cut = archive({deflatedHello("rocket.ork"), stored("a.png", "A")});
    cut.resize(30 + 10 + 3);  // the header, the name and three of the seven bytes of data
    EXPECT_EQ(ask(cut, "a.png"), "PARSE: Unexpected end of ZLIB input stream");
    EXPECT_EQ(ask(cut, "rocket.ork"), "PARSE: Unexpected end of ZLIB input stream");

    // A stored entry cut short.
    LocalEntry stump = stored("decal.png", std::string(65, 'x'));
    stump.data.resize(30);
    const std::vector<std::byte> data = archive({stump});
    EXPECT_EQ(ask(data, "decal.png", 100), "PARSE: unexpected EOF");
    EXPECT_EQ(ask(data, "decal.png", 10),
              "IO: Attachment 'decal.png' exceeds the maximum size of 10 bytes");
    EXPECT_EQ(ask(data, "after.txt", 100), "PARSE: unexpected EOF");

    // A header cut inside its name.
    std::vector<std::byte> header = archive({stored("abcdef", "x")});
    header.resize(33);
    EXPECT_EQ(ask(header, "abcdef"), "PARSE: Unexpected end of ZIP data");
}

TEST(ZipFileAttachment, ADeclaredSizeThatLiesIsCaughtByTheChecks)
{
    // Sizes that say more than there is: the data after the entry is read as its contents, and
    // the CRC does not fit. The entry is refused unread when the false size is beyond the limit.
    LocalEntry more                     = stored("decal.png", std::string(65, '\0'));
    more.size                           = 70;
    more.compressedSize                 = 70;
    const std::vector<std::byte> larger = archive({more, stored("after.txt", "after")});
    EXPECT_TRUE(ask(larger, "decal.png", 300).starts_with("PARSE: invalid entry CRC (expected "));
    EXPECT_EQ(ask(larger, "decal.png", 64),
              "IO: Attachment 'decal.png' exceeds the maximum size of 64 bytes");
    EXPECT_TRUE(ask(larger, "after.txt", 300).starts_with("PARSE: invalid entry CRC (expected "));

    // Sizes that say less: the same check, also on the way to a later entry.
    LocalEntry less                      = stored("decal.png", std::string(65, '\0'));
    less.size                            = 10;
    less.compressedSize                  = 10;
    const std::vector<std::byte> smaller = archive({less, stored("after.txt", "after")});
    EXPECT_TRUE(ask(smaller, "decal.png", 300).starts_with("PARSE: invalid entry CRC (expected "));
    EXPECT_TRUE(ask(smaller, "after.txt", 300).starts_with("PARSE: invalid entry CRC (expected "));

    // A deflated entry whose header understates what it inflates to: the limit holds for the
    // bytes that come, whatever the header promised.
    LocalEntry deflated = deflatedHello("a");
    deflated.size       = 1;
    EXPECT_EQ(ask(archive({deflated}), "a", 4), "IO: Input exceeds maximum size of 4 bytes");
    EXPECT_EQ(ask(archive({deflated}), "a", 5),
              "PARSE: invalid entry size (expected 1 but got 5 bytes)");

    // A wrong CRC alone.
    LocalEntry crc = stored("decal.png", "hello");
    crc.crc        = 1;
    EXPECT_EQ(ask(archive({crc}), "decal.png"),
              "PARSE: invalid entry CRC (expected 0x1 but got 0x3610a686)");
    EXPECT_EQ(ask(archive({crc, stored("after.txt", "after")}), "after.txt"),
              "PARSE: invalid entry CRC (expected 0x1 but got 0x3610a686)");
}

TEST(ZipFileAttachment, AnEntryThatInflatesBeyondTheLimitIsRefused)
{
    // 4 MiB of zeros deflate to a few kilobytes. ZipWriter streams, as Java's ZipOutputStream:
    // the entry does not declare its size, and the limit is met while it inflates.
    QtRocket::ZipWriter writer;
    writer.add("decals/bomb.png", std::vector<std::byte>(std::size_t{4} * 1024 * 1024));
    writer.add("decals/small.png", QtRocket::stringToBytes("small"));
    const std::vector<std::byte> data = writer.finish().value();
    EXPECT_LT(data.size(), std::size_t{16} * 1024);
    EXPECT_EQ(ask(data, "decals/bomb.png", std::size_t{1024} * 1024),
              "IO: Input exceeds maximum size of 1048576 bytes");
    EXPECT_EQ(ask(data, "decals/bomb.png").size(), std::size_t{4} * 1024 * 1024);
    // The entry behind it is found: moving past the large one holds none of it.
    EXPECT_EQ(ask(data, "decals/small.png", 5), "small");
}

TEST(ZipFileAttachment, AnEntryThatDeclaresMoreThanTheDefaultLimitIsRefusedUnread)
{
    // One byte more than 32 MiB by its header, and no data at all behind it.
    LocalEntry huge     = stored("decals/huge.png", "");
    huge.size           = static_cast<std::uint32_t>(ZipFileAttachment::kMaxAttachmentBytes + 1);
    huge.compressedSize = huge.size;
    EXPECT_EQ(ask(archive({huge}), "decals/huge.png"),
              "IO: Attachment 'decals/huge.png' exceeds the maximum size of 33554432 bytes");
    // Exactly 32 MiB is within the limit, and then the data that is not there is missed.
    huge.size           = static_cast<std::uint32_t>(ZipFileAttachment::kMaxAttachmentBytes);
    huge.compressedSize = huge.size;
    EXPECT_EQ(ask(archive({huge}), "decals/huge.png"), "PARSE: unexpected EOF");
}

TEST(ZipFileAttachment, SharesTheArchiveAndReadsItEveryTime)
{
    const ZipFileAttachment::Archive data = shared(namedArchive());
    const ZipFileAttachment          a("decals/a.png", data);
    const ZipFileAttachment          b("rocket.ork", data);
    EXPECT_EQ(data.use_count(), 3);
    EXPECT_EQ(QtRocket::bytesToString(a.getBytes().value()), "A");
    EXPECT_EQ(QtRocket::bytesToString(a.getBytes().value()), "A");
    EXPECT_EQ(QtRocket::bytesToString(b.getBytes().value()), "<openrocket/>");
}

TEST(ZipFileAttachment, IsAnAttachmentWithItsName)
{
    const ZipFileAttachment attachment("decals/a.png", shared(namedArchive()));
    const Attachment&       base = attachment;
    EXPECT_EQ(base.getName(), "decals/a.png");
    EXPECT_EQ(base.toString(), "decals/a.png");
    EXPECT_EQ(QtRocket::bytesToString(base.getBytes().value()), "A");
}

TEST(ZipFileAttachment, ANullArchiveIsABug)
{
    EXPECT_THROW(ZipFileAttachment("a", nullptr), BugError);
}

}  // namespace
