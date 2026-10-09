#include "QtRocket/file/ZipInputStream.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/ZipArchive.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "file/RawZip.h"

namespace
{

using QtRocket::ErrorCode;
using QtRocket::Result;
using QtRocket::ZipInputStream;

using QtRocket::Test::archive;
using QtRocket::Test::deflatedHello;
using QtRocket::Test::kHello;
using QtRocket::Test::kHelloCrc;
using QtRocket::Test::kHelloDeflated;
using QtRocket::Test::kJavaArchiveOf65Zeros;
using QtRocket::Test::kJavaArchiveOfFourBytes;
using QtRocket::Test::LocalEntry;
using QtRocket::Test::put16;
using QtRocket::Test::put32;
using QtRocket::Test::put64;
using QtRocket::Test::stored;

/// The names and contents of every entry, or the first failure's message.
[[nodiscard]] std::vector<std::string> read(const std::vector<std::byte>& data)
{
    std::vector<std::string> result;
    ZipInputStream           zip(data);
    while (true)
    {
        Result<std::optional<ZipInputStream::Entry>> next = zip.nextEntry();
        if (!next)
        {
            result.push_back("error: " + next.error().message);
            return result;
        }
        if (!next->has_value())
        {
            return result;
        }
        Result<std::vector<std::byte>> contents = zip.readEntry();
        if (!contents)
        {
            result.push_back("error: " + contents.error().message);
            return result;
        }
        result.push_back((*next)->name + "=" + QtRocket::bytesToString(*contents));
    }
}

/// The first failure's message of reading @p data, or "" when it reads.
[[nodiscard]] std::string failure(const std::vector<std::byte>& data)
{
    const std::vector<std::string> entries = read(data);
    if (entries.empty() || !entries.back().starts_with("error: "))
    {
        return "";
    }
    return entries.back().substr(7);
}

TEST(ZipInputStream, ReadsEntriesInLocalHeaderOrder)
{
    const std::vector<std::byte> data =
        archive({stored("b.eng", "second"), deflatedHello("a.txt"), stored("dir/", "")});
    EXPECT_EQ(read(data), (std::vector<std::string>{"b.eng=second", "a.txt=hello", "dir/="}));

    ZipInputStream    zip(data);
    std::vector<bool> directories;
    for (std::optional<ZipInputStream::Entry> entry = zip.nextEntry().value(); entry.has_value();
         entry                                      = zip.nextEntry().value())
    {
        directories.push_back(entry->directory);
    }
    EXPECT_EQ(directories, (std::vector<bool>{false, false, true}));
}

TEST(ZipInputStream, EndsAtAnythingButAWholeLocalHeader)
{
    // No entries at all without a local header, including data too short for one.
    for (const std::string_view data :
         {"", "garbage", "PK\x03\x04", "PK\x05\x06", "PK\x03\x04\x14"})
    {
        EXPECT_TRUE(read(QtRocket::stringToBytes(data)).empty()) << data;
    }
    // The entries before the central directory, other data or a cut header are read.
    std::vector<std::byte> data = archive({stored("a.eng", "one"), stored("b.eng", "two")});
    const std::size_t      end  = data.size();
    for (const std::string_view tail : {"PK\x01\x02", "garbage", "PK\x03\x04\x14\x00"})
    {
        data.resize(end);
        const std::vector<std::byte> bytes = QtRocket::stringToBytes(tail);
        data.insert(data.end(), bytes.begin(), bytes.end());
        EXPECT_EQ(read(data), (std::vector<std::string>{"a.eng=one", "b.eng=two"})) << tail;
    }
}

TEST(ZipInputStream, ReadsAnArchiveOfZipWriter)
{
    QtRocket::ZipWriter writer;
    writer.add("a.eng", QtRocket::stringToBytes("first"));
    writer.add("dir/b.rse", QtRocket::stringToBytes(std::string(10000, 'x')));
    EXPECT_EQ(read(writer.finish().value()),
              (std::vector<std::string>{"a.eng=first", "dir/b.rse=" + std::string(10000, 'x')}));
}

TEST(ZipInputStream, ChecksTheCrc)
{
    LocalEntry entry = stored("a.eng", "hello");
    entry.crc        = 0x6c4b3b66;
    EXPECT_EQ(failure(archive({entry})),
              "invalid entry CRC (expected 0x6c4b3b66 but got 0x3610a686)");

    LocalEntry deflated = deflatedHello("a.eng");
    deflated.crc        = 1;
    EXPECT_EQ(failure(archive({deflated})), "invalid entry CRC (expected 0x1 but got 0x3610a686)");

    // An empty STORED entry is at its end at once, without the check.
    LocalEntry empty = stored("a.eng", "");
    empty.crc        = 1;
    EXPECT_EQ(failure(archive({empty})), "");
}

TEST(ZipInputStream, ChecksAnEntryItMovesPast)
{
    LocalEntry entry                  = stored("a.txt", "hello");
    entry.crc                         = 1;
    const std::vector<std::byte> data = archive({entry, stored("b.eng", "x")});
    ZipInputStream               zip(data);
    ASSERT_TRUE(zip.nextEntry().value().has_value());
    const Result<std::optional<ZipInputStream::Entry>> next = zip.nextEntry();
    ASSERT_FALSE(next);
    EXPECT_EQ(next.error().code, ErrorCode::PARSE);
    EXPECT_EQ(next.error().message, "invalid entry CRC (expected 0x1 but got 0x3610a686)");
}

TEST(ZipInputStream, ChecksTheSizesOfADeflatedEntry)
{
    LocalEntry size = deflatedHello("a");
    size.size       = 4;
    EXPECT_EQ(failure(archive({size})), "invalid entry size (expected 4 but got 5 bytes)");
    LocalEntry compressed     = deflatedHello("a");
    compressed.compressedSize = 99;
    EXPECT_EQ(failure(archive({compressed})),
              "invalid entry compressed size (expected 99 but got 7 bytes)");
}

TEST(ZipInputStream, ReadsDataDescriptors)
{
    // Bit 3: the sizes and CRC follow the data, with or without a signature.
    for (const bool signature : {true, false})
    {
        LocalEntry entry     = deflatedHello("a.eng");
        entry.flags          = 8;
        entry.crc            = 0;
        entry.compressedSize = 0;
        entry.size           = 0;
        if (signature)
        {
            put32(entry.data, 0x08074b50);
        }
        put32(entry.data, kHelloCrc);
        put32(entry.data, static_cast<std::uint32_t>(kHelloDeflated.size()));
        put32(entry.data, static_cast<std::uint32_t>(kHello.size()));
        const std::vector<std::byte> data = archive({entry, stored("b.eng", "two")});
        EXPECT_EQ(read(data), (std::vector<std::string>{"a.eng=hello", "b.eng=two"})) << signature;
    }
    LocalEntry storedEntry = stored("a.eng", "x");
    storedEntry.flags      = 8;
    EXPECT_EQ(failure(archive({storedEntry})), "only DEFLATED entries can have EXT descriptor");
}

TEST(ZipInputStream, TakesSizesFromAZip64Field)
{
    LocalEntry entry     = stored("a.eng", "hello");
    entry.compressedSize = 0xFFFFFFFF;
    entry.size           = 0xFFFFFFFF;
    put16(entry.extra, 0x0001);
    put16(entry.extra, 16);
    put64(entry.extra, 5);
    put64(entry.extra, 5);
    EXPECT_EQ(read(archive({entry, stored("b.eng", "two")})),
              (std::vector<std::string>{"a.eng=hello", "b.eng=two"}));

    // Java reads the sizes as a signed long: a negative size is an empty entry, and the next
    // header is looked for right after this one's.
    LocalEntry negative = entry;
    negative.extra.clear();
    put16(negative.extra, 0x0001);
    put16(negative.extra, 16);
    put64(negative.extra, 0xFFFFFFFFFFFFFFFFULL);
    put64(negative.extra, 0xFFFFFFFFFFFFFFFFULL);
    EXPECT_EQ(read(archive({negative})), std::vector<std::string>{"a.eng="});
}

TEST(ZipInputStream, RejectsWhatJavaRejects)
{
    LocalEntry method = stored("a", "x");
    method.method     = 99;
    EXPECT_EQ(failure(archive({method})), "invalid compression method");
    LocalEntry encrypted = stored("a", "x");
    encrypted.flags      = 1;
    EXPECT_EQ(failure(archive({encrypted})), "encrypted ZIP entry not supported");
    LocalEntry cut = stored("a", "hello");
    cut.data.resize(2);
    EXPECT_EQ(failure(archive({cut})), "unexpected EOF");
    LocalEntry deflated = deflatedHello("a");
    deflated.data.resize(3);
    EXPECT_EQ(failure(archive({deflated})), "Unexpected end of ZLIB input stream");
    LocalEntry corrupt = deflatedHello("a");
    corrupt.data[0]    = std::byte{0xFF};
    EXPECT_EQ(failure(archive({corrupt})), "invalid deflate data in ZIP entry");
    // A header cut inside its name (Java: an EOFException without a message).
    std::vector<std::byte> truncated = archive({stored("abcdef", "x")});
    truncated.resize(33);
    EXPECT_EQ(failure(truncated), "Unexpected end of ZIP data");
}

TEST(ZipInputStream, RejectsNamesThatAreNotUtf8)
{
    // Java decodes names in UTF-8 whatever the flags say, and fails at the first malformed
    // sequence (the offsets and lengths are the JDK's, pinned by running it on 12746 names).
    const std::vector<std::pair<std::string_view, std::string_view>> cases{
        {"m\x81nchen.eng", "malformed input off : 1, length : 1"},
        {"\xc2", "malformed input off : 1, length : 1"},
        {"\xc2\x41", "malformed input off : 1, length : 1"},
        {"A\xc2", "malformed input off : 2, length : 1"},
        {"\xe0\x80", "malformed input off : 0, length : 2"},
        {"\xe0\x80\x80", "malformed input off : 0, length : 3"},
        {"\xed\xa0\x80", "malformed input off : 0, length : 3"},
        {"\xe2", "malformed input off : 1, length : 1"},
        {"\xf0\x9f", "malformed input off : 0, length : 1"},
        {"\xf0\x9f\x41", "malformed input off : 0, length : 1"},
        {"\xf5", "malformed input off : 0, length : 1"},
        {"\xc0", "malformed input off : 0, length : 1"},
        {"m\xc3\xbcnchen.eng/", ""},
    };
    for (const auto& [name, message] : cases)
    {
        EXPECT_EQ(failure(archive({stored(name, "")})), message) << name;
    }
}

/// The entry nextEntry() of @p zip gives, or one named "(none)" at the end of the entries.
[[nodiscard]] ZipInputStream::Entry next(ZipInputStream& zip)
{
    return zip.nextEntry().value().value_or(
        ZipInputStream::Entry{.name = "(none)", .directory = false, .size = -99});
}

/// The declared size of the first entry of @p data.
[[nodiscard]] std::int64_t firstSize(const std::vector<std::byte>& data)
{
    ZipInputStream zip(data);
    return next(zip).size;
}

TEST(ZipInputStream, GivesTheSizeTheLocalHeaderDeclares)
{
    // ZipEntry.getSize(): what the header says, true or not, and -1 behind a data descriptor.
    EXPECT_EQ(firstSize(archive({stored("a", "hello")})), 5);
    EXPECT_EQ(firstSize(archive({deflatedHello("b")})), 5);
    EXPECT_EQ(firstSize(archive({stored("dir/", "")})), 0);
    LocalEntry streamed = deflatedHello("streamed");
    streamed.flags      = 8;
    EXPECT_EQ(firstSize(archive({streamed})), -1);
    LocalEntry lie = stored("lie", "hello");
    lie.size       = 200;
    EXPECT_EQ(firstSize(archive({lie})), 200);
}

TEST(ZipInputStream, GivesTheSizesOfJavasArchives)
{
    // Measured with java.util.zip.ZipInputStream on the same bytes: "size=-1".
    const std::vector<std::byte> data = QtRocket::Test::bytesFromHex(kJavaArchiveOfFourBytes);
    ZipInputStream               zip(data);
    const ZipInputStream::Entry  entry = next(zip);
    EXPECT_EQ(entry.name, "decal.png");
    EXPECT_EQ(entry.size, -1);
    EXPECT_FALSE(entry.directory);
    EXPECT_EQ(zip.readEntry().value(),
              (std::vector<std::byte>{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}}));
    EXPECT_FALSE(zip.nextEntry().value().has_value());
}

TEST(ZipInputStream, GivesAZip64SizeAsJavasLong)
{
    LocalEntry entry     = stored("a.eng", "hello");
    entry.compressedSize = 0xFFFFFFFF;
    entry.size           = 0xFFFFFFFF;
    put16(entry.extra, 0x0001);
    put16(entry.extra, 16);
    put64(entry.extra, 0xFFFFFFFFFFFFFFFEULL);
    put64(entry.extra, 0xFFFFFFFFFFFFFFFEULL);
    const std::vector<std::byte> data = archive({entry});
    ZipInputStream               zip(data);
    EXPECT_EQ(next(zip).size, -2);
}

/// The contents of the first entry of @p data read with the limit @p maxBytes, or "error: " and
/// the failure's code and message.
[[nodiscard]] std::string readFirst(const std::vector<std::byte>& data, std::size_t maxBytes)
{
    ZipInputStream zip(data);
    if (const Result<std::optional<ZipInputStream::Entry>> entry = zip.nextEntry(); !entry)
    {
        return "error: " + entry.error().message;
    }
    const Result<std::vector<std::byte>> contents = zip.readEntry(maxBytes);
    if (!contents)
    {
        return "error: " + std::string(QtRocket::toString(contents.error().code)) + " " +
               contents.error().message;
    }
    return QtRocket::bytesToString(*contents);
}

TEST(ZipInputStream, ReadsAnEntryUpToALimit)
{
    // FileUtils.readBytes(stream, maxBytes): measured on Java's own archives.
    const std::vector<std::byte> four = QtRocket::Test::bytesFromHex(kJavaArchiveOfFourBytes);
    EXPECT_EQ(readFirst(four, 4), "\x01\x02\x03\x04");
    EXPECT_EQ(readFirst(four, 5), "\x01\x02\x03\x04");
    EXPECT_EQ(readFirst(four, 3), "error: IO Input exceeds maximum size of 3 bytes");
    EXPECT_EQ(readFirst(four, 0), "error: IO Input exceeds maximum size of 0 bytes");

    const std::vector<std::byte> zeros = QtRocket::Test::bytesFromHex(kJavaArchiveOf65Zeros);
    EXPECT_EQ(readFirst(zeros, 65), std::string(65, '\0'));
    EXPECT_EQ(readFirst(zeros, 64), "error: IO Input exceeds maximum size of 64 bytes");

    // A STORED entry, and one of no bytes, which no limit refuses.
    EXPECT_EQ(readFirst(archive({stored("a", "hello")}), 5), "hello");
    EXPECT_EQ(readFirst(archive({stored("a", "hello")}), 4),
              "error: IO Input exceeds maximum size of 4 bytes");
    EXPECT_EQ(readFirst(archive({stored("a", "")}), 0), "");
    EXPECT_EQ(readFirst(archive({deflatedHello("a")}), 5), "hello");
    EXPECT_EQ(readFirst(archive({deflatedHello("a")}), 4),
              "error: IO Input exceeds maximum size of 4 bytes");
}

TEST(ZipInputStream, MeetsTheLimitBeforeTheEntrysEnd)
{
    // The limit is met while reading, so what is wrong with the end of an entry that is too
    // long is never seen: its CRC, its sizes, the data after the limit.
    LocalEntry crc = stored("a", "hello");
    crc.crc        = 1;
    EXPECT_EQ(readFirst(archive({crc}), 4), "error: IO Input exceeds maximum size of 4 bytes");
    EXPECT_EQ(readFirst(archive({crc}), 5),
              "error: PARSE invalid entry CRC (expected 0x1 but got 0x3610a686)");
    LocalEntry size = deflatedHello("a");
    size.size       = 4;
    EXPECT_EQ(readFirst(archive({size}), 4), "error: IO Input exceeds maximum size of 4 bytes");
    EXPECT_EQ(readFirst(archive({size}), 5),
              "error: PARSE invalid entry size (expected 4 but got 5 bytes)");

    // A STORED entry cut short: the limit when that many bytes are there, else the end.
    LocalEntry cut = stored("a", "hello");
    cut.data.resize(2);
    EXPECT_EQ(readFirst(archive({cut}), 1), "error: IO Input exceeds maximum size of 1 bytes");
    EXPECT_EQ(readFirst(archive({cut}), 2), "error: PARSE unexpected EOF");
    EXPECT_EQ(readFirst(archive({cut}), 100), "error: PARSE unexpected EOF");

    // Deflate data that ends or goes wrong before the limit is reported as that.
    LocalEntry truncated = deflatedHello("a");
    truncated.data.resize(3);
    EXPECT_EQ(readFirst(archive({truncated}), 100),
              "error: PARSE Unexpected end of ZLIB input stream");
    LocalEntry corrupt = deflatedHello("a");
    corrupt.data[0]    = std::byte{0xFF};
    EXPECT_EQ(readFirst(archive({corrupt}), 100), "error: PARSE invalid deflate data in ZIP entry");
}

TEST(ZipInputStream, TheLimitHoldsForContentsOfAnySize)
{
    // An entry that inflates to far more than the limit (40 KiB of zeros in 59 bytes) is not
    // read to its end to learn that.
    QtRocket::ZipWriter writer;
    writer.add("zeros", std::vector<std::byte>(std::size_t{40} * 1024));
    const std::vector<std::byte> data = writer.finish().value();
    EXPECT_EQ(readFirst(data, 40959), "error: IO Input exceeds maximum size of 40959 bytes");
    EXPECT_EQ(readFirst(data, 100), "error: IO Input exceeds maximum size of 100 bytes");
    EXPECT_EQ(readFirst(data, 40960).size(), 40960U);
}

TEST(ZipInputStream, ALimitedReadSkipsNothingOfTheNextEntry)
{
    const std::vector<std::byte> data = archive({deflatedHello("a"), stored("b", "two")});
    ZipInputStream               zip(data);
    ASSERT_TRUE(zip.nextEntry().value().has_value());
    EXPECT_EQ(QtRocket::bytesToString(zip.readEntry(5).value()), "hello");
    EXPECT_TRUE(zip.readEntry(0).value().empty());  // once read, the entry is empty
    EXPECT_EQ(next(zip).name, "b");
    EXPECT_EQ(QtRocket::bytesToString(zip.readEntry(3).value()), "two");
}

TEST(ZipInputStream, StopsAfterAnEntryWithAZip64DataDescriptorAsJava17Does)
{
    // "{decals/ size=0 read=0} end": java.util.zip.ZipInputStream of JDK 17.0.20 on the archive
    // minizip makes of an empty entry and a second one (probe ZipListProbe of part D4). The
    // descriptor's 8-byte sizes are read as 4-byte ones, which fit, and the next header is then
    // looked for 8 bytes early.
    const std::vector<std::byte> data =
        archive({QtRocket::Test::zip64Empty("decals/"), stored("a", "A")});
    EXPECT_EQ(read(data), std::vector<std::string>{"decals/="});
}

TEST(ZipInputStream, ReadEntryIsEmptyOnceRead)
{
    const std::vector<std::byte> data = archive({stored("a", "x")});
    ZipInputStream               zip(data);
    EXPECT_TRUE(zip.readEntry().value().empty());  // before the first entry
    ASSERT_TRUE(zip.nextEntry().value().has_value());
    EXPECT_EQ(QtRocket::bytesToString(zip.readEntry().value()), "x");
    EXPECT_TRUE(zip.readEntry().value().empty());
    EXPECT_FALSE(zip.nextEntry().value().has_value());
}

/// What readEntryInto() gives for the first entry of @p data with the limit @p maxBytes: the
/// contents it left, and "ok" or the failure, as "<contents> | <code> <message>".
[[nodiscard]] std::string readInto(const std::vector<std::byte>& data, std::size_t maxBytes)
{
    ZipInputStream zip(data);
    if (const auto entry = zip.nextEntry(); !entry || !entry->has_value())
    {
        return "no entry";
    }
    // What the vector held before is gone.
    std::vector<std::byte> contents = QtRocket::stringToBytes("stale");
    const Result<void>     read     = zip.readEntryInto(contents, maxBytes);
    return QtRocket::bytesToString(contents) + " | " +
           (read ? std::string("ok")
                 : std::string(toString(read.error().code)) + " " + read.error().message);
}

// readEntryInto() of an intact entry is readEntry(): the contents, of a stored and of a
// deflated entry, and nothing once the entry was read.
TEST(ZipInputStream, ReadEntryIntoGivesTheContentsOfAnIntactEntry)
{
    EXPECT_EQ(readInto(archive({stored("a", "hello")}), 5), "hello | ok");
    EXPECT_EQ(readInto(archive({deflatedHello("a")}), 5), "hello | ok");
    EXPECT_EQ(readInto(archive({stored("a", "")}), 0), " | ok");

    const std::vector<std::byte> data = archive({stored("a", "x")});
    ZipInputStream               zip(data);
    std::vector<std::byte>       contents = QtRocket::stringToBytes("stale");
    ASSERT_TRUE(zip.readEntryInto(contents, 10).has_value());  // before the first entry
    EXPECT_TRUE(contents.empty());
    ASSERT_TRUE(zip.nextEntry().value().has_value());
    ASSERT_TRUE(zip.readEntryInto(contents, 10).has_value());
    EXPECT_EQ(QtRocket::bytesToString(contents), "x");
    ASSERT_TRUE(zip.readEntryInto(contents, 10).has_value());  // once read
    EXPECT_TRUE(contents.empty());
}

// A damaged entry leaves what it gave before the failure, as Java's reader has the bytes that
// came before the exception: the whole contents of an entry whose check sum or size is wrong,
// the bytes there are of a stored entry that is cut short, and what could be inflated of a
// deflated entry whose data ends too early.
TEST(ZipInputStream, ReadEntryIntoLeavesWhatADamagedEntryGave)
{
    LocalEntry wrongSum = stored("a", "hello");
    wrongSum.crc ^= 1U;
    EXPECT_EQ(readInto(archive({wrongSum}), 5),
              "hello | PARSE invalid entry CRC (expected 0x3610a687 but got 0x3610a686)");
    LocalEntry wrongDeflatedSum = deflatedHello("a");
    wrongDeflatedSum.crc ^= 1U;
    EXPECT_EQ(readInto(archive({wrongDeflatedSum}), 5),
              "hello | PARSE invalid entry CRC (expected 0x3610a687 but got 0x3610a686)");
    LocalEntry wrongSize = deflatedHello("a");
    wrongSize.size       = 4;
    EXPECT_EQ(readInto(archive({wrongSize}), 5),
              "hello | PARSE invalid entry size (expected 4 but got 5 bytes)");

    // A stored entry of five bytes of which three are there.
    std::vector<std::byte> cutStored = archive({stored("a", "hello")});
    cutStored.resize(cutStored.size() - 2);
    EXPECT_EQ(readInto(cutStored, 5), "hel | PARSE unexpected EOF");
    EXPECT_EQ(readInto(cutStored, 3), "hel | PARSE unexpected EOF");
    // The limit comes first, as for readEntry(maxBytes).
    EXPECT_EQ(readInto(cutStored, 2), " | IO Input exceeds maximum size of 2 bytes");

    // A deflated entry whose last byte is missing: its five bytes are inflated all the same.
    std::vector<std::byte> cutDeflated = archive({deflatedHello("a")});
    cutDeflated.resize(cutDeflated.size() - 1);
    EXPECT_EQ(readInto(cutDeflated, 5), "hello | PARSE Unexpected end of ZLIB input stream");
    // With one more byte missing nothing can be inflated.
    cutDeflated.resize(cutDeflated.size() - 5);
    EXPECT_EQ(readInto(cutDeflated, 5), " | PARSE Unexpected end of ZLIB input stream");
}

// The limit holds for readEntryInto() as for readEntry(maxBytes): contents beyond it fail with
// the bounded read's failure, and no more than the limit is left.
TEST(ZipInputStream, ReadEntryIntoHoldsNoMoreThanTheLimit)
{
    EXPECT_EQ(readInto(archive({stored("a", "hello")}), 4),
              " | IO Input exceeds maximum size of 4 bytes");
    EXPECT_EQ(readInto(archive({deflatedHello("a")}), 4),
              " | IO Input exceeds maximum size of 4 bytes");
}

}  // namespace
