#include "QtRocket/file/ZipInputStream.h"

#include <algorithm>
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

namespace
{

using QtRocket::ErrorCode;
using QtRocket::Result;
using QtRocket::ZipInputStream;

constexpr std::uint16_t kStored   = 0;
constexpr std::uint16_t kDeflated = 8;

/// "hello", its CRC and its raw deflate stream.
constexpr std::string_view            kHello    = "hello";
constexpr std::uint32_t               kHelloCrc = 0x3610a686;
constexpr std::array<std::uint8_t, 7> kHelloDeflated{0xcb, 0x48, 0xcd, 0xc9, 0xc9, 0x07, 0x00};

/// A local file header and what follows it, laid out byte by byte.
struct LocalEntry
{
    std::string_view       name;
    std::vector<std::byte> data;
    std::uint16_t          method{kStored};
    std::uint16_t          flags{0};
    std::uint32_t          crc{0};
    std::uint32_t          compressedSize{0};
    std::uint32_t          size{0};
    std::vector<std::byte> extra;
};

/// The CRC-32 of @p data (bit by bit; the tests' entries are short).
[[nodiscard]] std::uint32_t crc32(const std::vector<std::byte>& data)
{
    std::uint32_t crc = 0xFFFFFFFFU;
    for (const std::byte b : data)
    {
        crc ^= std::to_integer<std::uint32_t>(b);
        for (int bit = 0; bit < 8; bit++)
        {
            crc = (crc & 1U) != 0 ? (crc >> 1U) ^ 0xEDB88320U : crc >> 1U;
        }
    }
    return ~crc;
}

void put16(std::vector<std::byte>& out, std::uint32_t value)
{
    out.push_back(static_cast<std::byte>(value & 0xFFU));
    out.push_back(static_cast<std::byte>((value >> 8U) & 0xFFU));
}

void put32(std::vector<std::byte>& out, std::uint32_t value)
{
    put16(out, value & 0xFFFFU);
    put16(out, value >> 16U);
}

void put64(std::vector<std::byte>& out, std::uint64_t value)
{
    put32(out, static_cast<std::uint32_t>(value & 0xFFFFFFFFU));
    put32(out, static_cast<std::uint32_t>(value >> 32U));
}

void append(std::vector<std::byte>& out, const LocalEntry& entry)
{
    put32(out, 0x04034b50);
    put16(out, 20);
    put16(out, entry.flags);
    put16(out, entry.method);
    put32(out, 0);  // time and date
    put32(out, entry.crc);
    put32(out, entry.compressedSize);
    put32(out, entry.size);
    put16(out, static_cast<std::uint32_t>(entry.name.size()));
    put16(out, static_cast<std::uint32_t>(entry.extra.size()));
    const std::vector<std::byte> name = QtRocket::stringToBytes(entry.name);
    out.insert(out.end(), name.begin(), name.end());
    out.insert(out.end(), entry.extra.begin(), entry.extra.end());
    out.insert(out.end(), entry.data.begin(), entry.data.end());
}

[[nodiscard]] LocalEntry stored(std::string_view name, std::string_view text)
{
    const std::vector<std::byte> data = QtRocket::stringToBytes(text);
    LocalEntry                   entry{.name           = name,
                                       .data           = data,
                                       .method         = kStored,
                                       .flags          = 0,
                                       .crc            = crc32(data),
                                       .compressedSize = static_cast<std::uint32_t>(data.size()),
                                       .size           = static_cast<std::uint32_t>(data.size()),
                                       .extra          = {}};
    return entry;
}

[[nodiscard]] LocalEntry deflatedHello(std::string_view name)
{
    std::vector<std::byte> data(kHelloDeflated.size());
    std::ranges::transform(kHelloDeflated, data.begin(),
                           [](std::uint8_t b) { return static_cast<std::byte>(b); });
    return {.name           = name,
            .data           = data,
            .method         = kDeflated,
            .flags          = 0,
            .crc            = kHelloCrc,
            .compressedSize = static_cast<std::uint32_t>(kHelloDeflated.size()),
            .size           = static_cast<std::uint32_t>(kHello.size()),
            .extra          = {}};
}

[[nodiscard]] std::vector<std::byte> archive(std::initializer_list<LocalEntry> entries)
{
    std::vector<std::byte> out;
    for (const LocalEntry& entry : entries)
    {
        append(out, entry);
    }
    return out;
}

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

}  // namespace
