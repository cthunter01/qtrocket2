#include "QtRocket/file/ZipArchive.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/ZipInputStream.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestPaths.h"
#include "file/RawZip.h"

namespace
{

using QtRocket::Result;
using QtRocket::ZipArchive;
using QtRocket::ZipWriter;

// Deterministic, incompressible-enough filler (a small linear congruential generator; no <random>
// so that clang-tidy does not object to a constant seed).
std::vector<std::byte> randomBytes(std::size_t count)
{
    std::vector<std::byte> bytes(count);
    std::uint32_t          state = 12345;
    for (auto& b : bytes)
    {
        state = (state * 1664525U) + 1013904223U;
        b     = static_cast<std::byte>(state >> 24);
    }
    return bytes;
}

TEST(ZipArchive, RoundTripsEntriesThroughWriter)
{
    const auto text   = QtRocket::stringToBytes("<openrocket version=\"1.11\"/>\n");
    const auto binary = randomBytes(100'000);

    ZipWriter writer;
    writer.add("rocket.ork", text);
    writer.add("thrustcurves/abc.rse", binary);
    writer.add("empty.txt", {});
    const auto bytes = writer.finish();
    ASSERT_TRUE(bytes.has_value()) << bytes.error().toString();
    EXPECT_TRUE(ZipArchive::looksLikeZip(*bytes));

    const auto archive = ZipArchive::fromBytes(*bytes);
    ASSERT_TRUE(archive.has_value()) << archive.error().toString();
    ASSERT_EQ(archive->entries().size(), 3U);
    EXPECT_EQ(archive->entries()[0].name, "rocket.ork");
    EXPECT_TRUE(archive->contains("thrustcurves/abc.rse"));
    EXPECT_FALSE(archive->contains("missing"));
    ASSERT_NE(archive->find("rocket.ork"), nullptr);
    EXPECT_EQ(*archive->find("rocket.ork"), text);
    ASSERT_NE(archive->find("thrustcurves/abc.rse"), nullptr);
    EXPECT_EQ(*archive->find("thrustcurves/abc.rse"), binary);
    ASSERT_NE(archive->find("empty.txt"), nullptr);
    EXPECT_TRUE(archive->find("empty.txt")->empty());
}

TEST(ZipArchive, AnEmptyEntryDoesNotHideTheEntriesBehindItFromAStreamReader)
{
    // A reader of local headers (java.util.zip.ZipInputStream, which OpenRocket finds the
    // attachments of a design with, and ZipInputStream here) reads the data descriptor of an
    // entry with 4-byte sizes. minizip left to itself writes an empty entry in ZIP64 form, with
    // 8-byte sizes, and such a reader then finds no entry behind it (measured with JDK 17.0.20:
    // it lists "decals/" and stops). ZipWriter writes no ZIP64 entry; the same JDK lists all
    // four entries of this archive.
    ZipWriter writer;
    writer.add("decals/", {});
    writer.add("empty.txt", {});
    writer.add("a", QtRocket::stringToBytes("A"));
    writer.add("last", {});
    const std::vector<std::byte> bytes = writer.finish().value();

    QtRocket::ZipInputStream zip(bytes);
    std::vector<std::string> entries;
    for (std::optional<QtRocket::ZipInputStream::Entry> entry = zip.nextEntry().value();
         entry.has_value(); entry                             = zip.nextEntry().value())
    {
        entries.push_back(entry->name + "=" + QtRocket::bytesToString(zip.readEntry().value()));
    }
    EXPECT_EQ(entries, (std::vector<std::string>{"decals/=", "empty.txt=", "a=A", "last="}));
    // The central directory lists the same.
    EXPECT_EQ(ZipArchive::fromBytes(bytes).value().entries().size(), 3U);
}

TEST(ZipArchive, ReadsOpenRocketMotorZip)
{
    const auto bytes = QtRocket::readFile(QtRocket::Test::testDataDir() / "motors" / "test.zip");
    ASSERT_TRUE(bytes.has_value()) << bytes.error().toString();
    const auto archive = ZipArchive::fromBytes(*bytes);
    ASSERT_TRUE(archive.has_value()) << archive.error().toString();
    EXPECT_FALSE(archive->entries().empty());
    for (const auto& entry : archive->entries())
    {
        EXPECT_FALSE(entry.name.empty());
        EXPECT_FALSE(entry.data.empty()) << entry.name;
    }
}

TEST(ZipArchive, ReadsExampleDesign)
{
    const auto bytes =
        QtRocket::readFile(QtRocket::Test::dataDir() / "examples" / "A simple model rocket.ork");
    ASSERT_TRUE(bytes.has_value()) << bytes.error().toString();
    ASSERT_TRUE(ZipArchive::looksLikeZip(*bytes));
    const auto archive = ZipArchive::fromBytes(*bytes);
    ASSERT_TRUE(archive.has_value()) << archive.error().toString();
    ASSERT_NE(archive->find("rocket.ork"), nullptr);
    const std::string xml = QtRocket::bytesToString(*archive->find("rocket.ork"));
    EXPECT_NE(xml.find("<openrocket"), std::string::npos);
}

TEST(ZipArchive, RejectsGarbage)
{
    const auto garbage = randomBytes(1000);
    EXPECT_FALSE(ZipArchive::looksLikeZip(garbage));
    const auto archive = ZipArchive::fromBytes(garbage);
    ASSERT_FALSE(archive.has_value());
    EXPECT_EQ(archive.error().code, QtRocket::ErrorCode::PARSE);
    EXPECT_FALSE(ZipArchive::fromBytes({}).has_value());
}

// ================================================================== the writer's headers

/// The little-endian field of @p width bytes at @p offset of @p bytes (0 beyond its end).
[[nodiscard]] std::uint32_t field(const std::vector<std::byte>& bytes, std::size_t offset,
                                  std::size_t width)
{
    std::uint32_t value = 0;
    for (std::size_t i = width; i > 0; --i)
    {
        const std::size_t at = offset + i - 1;
        value =
            (value << 8U) | (at < bytes.size() ? std::to_integer<std::uint32_t>(bytes[at]) : 0U);
    }
    return value;
}

/// The fields of a zip header that ZipWriter decides, as "name: value" lines in hexadecimal.
struct HeaderFields
{
    std::uint32_t madeBy{0};  ///< of the central header (0 for a local one)
    std::uint32_t needed{0};
    std::uint32_t flags{0};
    std::uint32_t method{0};
    std::uint32_t time{0};
    std::uint32_t date{0};
    std::uint32_t crc{
        0};  ///< of the central header (a local one says 0: a data descriptor follows)
    std::uint32_t size{0};  ///< the uncompressed size

    [[nodiscard]] bool operator==(const HeaderFields&) const = default;
};

// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
void PrintTo(const HeaderFields& f, std::ostream* out)
{
    *out << std::format(
        "madeBy {:#x} needed {:#x} flags {:#06x} method {} time {:#06x} date {:#06x} crc {:#010x} "
        "size {}",
        f.madeBy, f.needed, f.flags, f.method, f.time, f.date, f.crc, f.size);
}

/// The central header of each entry of @p bytes, an archive without a comment, and the local
/// header of the same entry, one after the other.
[[nodiscard]] std::vector<HeaderFields> headersOf(const std::vector<std::byte>& bytes)
{
    std::vector<HeaderFields> headers;
    if (bytes.size() < 22)
    {
        return headers;
    }
    const std::size_t end     = bytes.size() - 22;
    std::size_t       central = field(bytes, end + 16, 4);
    while (central < end && field(bytes, central, 4) == 0x02014b50)
    {
        headers.push_back({.madeBy = field(bytes, central + 4, 2),
                           .needed = field(bytes, central + 6, 2),
                           .flags  = field(bytes, central + 8, 2),
                           .method = field(bytes, central + 10, 2),
                           .time   = field(bytes, central + 12, 2),
                           .date   = field(bytes, central + 14, 2),
                           .crc    = field(bytes, central + 16, 4),
                           .size   = field(bytes, central + 24, 4)});
        const std::size_t local = field(bytes, central + 42, 4);
        EXPECT_EQ(field(bytes, local, 4), 0x04034b50U);
        headers.push_back({.madeBy = 0,
                           .needed = field(bytes, local + 4, 2),
                           .flags  = field(bytes, local + 6, 2),
                           .method = field(bytes, local + 8, 2),
                           .time   = field(bytes, local + 10, 2),
                           .date   = field(bytes, local + 12, 2),
                           .crc    = field(bytes, local + 14, 4),
                           .size   = field(bytes, local + 22, 4)});
        central += 46 + field(bytes, central + 28, 2) + field(bytes, central + 30, 2) +
                   field(bytes, central + 32, 2);
    }
    return headers;
}

// The headers of ZipWriter's entries with a time given: "made by" and "needed" version 2.0
// (0x14, Java's), the flags of a data descriptor and of a UTF-8 name (0x0808, Java's) with
// minizip's bit of the best compression (0x0002), deflated, the time as given (19:12:37 written as
// 19:12:36, Java's halved seconds: the bytes "9299 fa5a" of Java's header for the same time in the
// probe ZipProbe of tier 10a S4), and in the central header the CRC and the size of the contents.
// The local header says the size, where Java says 0; a reader takes it from the central header or
// the data descriptor. The compressed bytes are zlib's and not pinned.
TEST(ZipArchive, TheWriterWritesJavasHeadersWithTheTimeGiven)
{
    const std::vector<std::byte> text = QtRocket::stringToBytes("<openrocket/>");
    ZipWriter                    writer;
    writer.setEntryTime(ZipWriter::EntryTime{
        .year = 2025, .month = 7, .day = 26, .hour = 19, .minute = 12, .second = 37});
    writer.add("rocket.ork", text);
    writer.add("decals/a.png", {});
    const Result<std::vector<std::byte>> bytes = writer.finish();
    ASSERT_TRUE(bytes.has_value()) << bytes.error().toString();

    const HeaderFields central{.madeBy = 0x14,
                               .needed = 0x14,
                               .flags  = 0x080A,
                               .method = 8,
                               .time   = 0x9992,
                               .date   = 0x5AFA,
                               .crc    = QtRocket::Test::zipCrc32(text),
                               .size   = 13};
    HeaderFields       local  = central;
    local.madeBy              = 0;
    local.crc                 = 0;
    HeaderFields emptyCentral = central;
    emptyCentral.crc          = 0;
    emptyCentral.size         = 0;
    HeaderFields emptyLocal   = local;
    emptyLocal.size           = 0;
    EXPECT_EQ(headersOf(*bytes),
              (std::vector<HeaderFields>{central, local, emptyCentral, emptyLocal}));

    const Result<ZipArchive> archive = ZipArchive::fromBytes(*bytes);
    ASSERT_TRUE(archive.has_value());
    ASSERT_EQ(archive->entries().size(), 2U);
    EXPECT_EQ(archive->entries()[0].data, text);
    EXPECT_EQ(archive->entries()[1].name, "decals/a.png");
}

// A time before 1980, which a zip header cannot hold, is 1980-01-01 00:00:00 (Java:
// ZipUtils.DOSTIME_BEFORE_1980; the bytes "0000 2100" of Java's header for 1975-03-04 05:06:07).
TEST(ZipArchive, TheWriterWritesATimeBefore1980AsTheFirstMomentOf1980)
{
    ZipWriter writer;
    writer.setEntryTime(ZipWriter::EntryTime{
        .year = 1975, .month = 3, .day = 4, .hour = 5, .minute = 6, .second = 7});
    writer.add("old", {});
    const Result<std::vector<std::byte>> bytes = writer.finish();
    ASSERT_TRUE(bytes.has_value()) << bytes.error().toString();
    const std::vector<HeaderFields> headers = headersOf(*bytes);
    ASSERT_EQ(headers.size(), 2U);
    EXPECT_EQ(headers[0].date, 0x0021U);
    EXPECT_EQ(headers[0].time, 0U);
    EXPECT_EQ(headers[1].date, 0x0021U);
}

// The MS-DOS form of a time: the date in the high 16 bits, the time in the low; the seconds
// halved; the years 1980 to 2107, a year before as 1980-01-01 00:00:00; nothing for a field out
// of its range, a day its month does not have included (February 29 of a leap year only: 2000
// and 2024, not 2100 or 2025).
TEST(ZipArchive, TheDosFormOfATime)
{
    using Time = ZipWriter::EntryTime;
    EXPECT_EQ(ZipWriter::dosDateTime(Time{
                  .year = 2025, .month = 7, .day = 26, .hour = 19, .minute = 12, .second = 36}),
              0x5AFA9992U);
    EXPECT_EQ(ZipWriter::dosDateTime(Time{
                  .year = 2107, .month = 12, .day = 31, .hour = 23, .minute = 59, .second = 59}),
              0xFF9FBF7DU);
    EXPECT_EQ(ZipWriter::dosDateTime(Time{.year = 1980}), 0x00210000U);
    EXPECT_EQ(ZipWriter::dosDateTime(Time{.year = 1979, .month = 12, .day = 31, .hour = 23}),
              0x00210000U);
    EXPECT_TRUE(ZipWriter::dosDateTime(Time{.year = 2000, .month = 2, .day = 29}).has_value());
    EXPECT_TRUE(ZipWriter::dosDateTime(Time{.year = 2024, .month = 2, .day = 29}).has_value());
    EXPECT_FALSE(ZipWriter::dosDateTime(Time{.year = 2100, .month = 2, .day = 29}).has_value());
    EXPECT_FALSE(ZipWriter::dosDateTime(Time{.year = 2025, .month = 2, .day = 29}).has_value());
    EXPECT_FALSE(ZipWriter::dosDateTime(Time{.year = 2025, .month = 4, .day = 31}).has_value());
    EXPECT_FALSE(ZipWriter::dosDateTime(Time{.year = 2108}).has_value());
    EXPECT_FALSE(ZipWriter::dosDateTime(Time{.year = 2025, .month = 0}).has_value());
    EXPECT_FALSE(ZipWriter::dosDateTime(Time{.year = 2025, .month = 13}).has_value());
    EXPECT_FALSE(ZipWriter::dosDateTime(Time{.year = 2025, .day = 0}).has_value());
    EXPECT_FALSE(ZipWriter::dosDateTime(Time{.year = 2025, .hour = 24}).has_value());
    EXPECT_FALSE(ZipWriter::dosDateTime(Time{.year = 2025, .minute = 60}).has_value());
    EXPECT_FALSE(ZipWriter::dosDateTime(Time{.year = 2025, .second = 60}).has_value());
    EXPECT_FALSE(ZipWriter::dosDateTime(Time{.year = 2025, .hour = -1}).has_value());
    EXPECT_FALSE(ZipWriter::dosDateTime(Time{.year = 1975, .month = 13}).has_value());
}

// A time a header cannot hold gives no archive.
TEST(ZipArchive, TheWriterRefusesATimeAHeaderCannotHold)
{
    ZipWriter writer;
    writer.setEntryTime(ZipWriter::EntryTime{.year = 2108});
    writer.add("a", QtRocket::stringToBytes("A"));
    const Result<std::vector<std::byte>> bytes = writer.finish();
    ASSERT_FALSE(bytes.has_value());
    EXPECT_EQ(bytes.error().code, QtRocket::ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(bytes.error().message,
              "The zip entry time 2108-01-01 00:00:00 is not a date and time from 1980 to 2107.");
}

// Without a time the entries carry the local time of finish(): a date of these years, and the
// same in every header.
TEST(ZipArchive, TheWriterWritesTheLocalTimeOfNowWithoutATime)
{
    ZipWriter writer;
    EXPECT_FALSE(writer.getEntryTime().has_value());
    writer.add("a", QtRocket::stringToBytes("A"));
    writer.add("b", QtRocket::stringToBytes("B"));
    const Result<std::vector<std::byte>> bytes = writer.finish();
    ASSERT_TRUE(bytes.has_value()) << bytes.error().toString();
    const std::vector<HeaderFields> headers = headersOf(*bytes);
    ASSERT_EQ(headers.size(), 4U);
    const std::uint32_t year = 1980 + (headers[0].date >> 9U);
    EXPECT_GE(year, 2025U);
    for (const HeaderFields& header : headers)
    {
        EXPECT_EQ(header.date, headers[0].date);
    }
}

// A name added twice gives no archive, with Java's text (ZipOutputStream's ZipException).
TEST(ZipArchive, TheWriterRefusesANameAddedTwice)
{
    ZipWriter writer;
    writer.add("rocket.ork", QtRocket::stringToBytes("<openrocket/>"));
    writer.add("decals/a.png", QtRocket::stringToBytes("A"));
    writer.add("rocket.ork", QtRocket::stringToBytes("again"));
    const Result<std::vector<std::byte>> bytes = writer.finish();
    ASSERT_FALSE(bytes.has_value());
    EXPECT_EQ(bytes.error().code, QtRocket::ErrorCode::IO);
    EXPECT_EQ(bytes.error().message, "duplicate entry: rocket.ork");
}

/// The Error of finish() of a writer of the one entry @p name, or "" when there is an archive.
[[nodiscard]] std::string refusalOf(const std::string& name)
{
    ZipWriter writer;
    writer.add(name, QtRocket::stringToBytes("x"));
    const Result<std::vector<std::byte>> bytes = writer.finish();
    return bytes.has_value() ? "" : bytes.error().message;
}

// A name that minizip would not write as it is given is refused: a backslash (turned into a slash
// on Windows only), a NUL (the end of a C string), more than 65535 bytes (Java's text; its own
// limit is 65535 characters). A name of 65535 bytes is written and read back.
TEST(ZipArchive, TheWriterRefusesANameItCannotWriteAsGiven)
{
    EXPECT_EQ(refusalOf("decals\\a.png"),
              "The zip entry name 'decals\\a.png' holds a backslash, which an archive written on "
              "Windows would turn into a slash.");
    std::string nul = "a";
    nul.push_back('\0');
    nul.push_back('b');
    EXPECT_EQ(refusalOf(nul),
              "The zip entry name 'a\\0b' holds a NUL character, which an archive cannot hold.");
    EXPECT_EQ(refusalOf(std::string(65536, 'a')), "entry name too long");
    EXPECT_EQ(refusalOf(std::string(65535, 'a')), "");

    ZipWriter writer;
    writer.add(std::string(65535, 'n'), QtRocket::stringToBytes("long"));
    const Result<ZipArchive> archive = ZipArchive::fromBytes(writer.finish().value());
    ASSERT_TRUE(archive.has_value()) << archive.error().toString();
    ASSERT_EQ(archive->entries().size(), 1U);
    EXPECT_EQ(archive->entries()[0].name.size(), 65535U);
}

}  // namespace
