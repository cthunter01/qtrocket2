#include "QtRocket/file/GzipStream.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "file/RawZip.h"

namespace
{

TEST(GzipStream, RoundTrips)
{
    std::string text;
    for (int i = 0; i < 2000; ++i)
    {
        text += "<datapoint>1.0,2.0,3.0</datapoint>\n";
    }
    const auto plain      = QtRocket::stringToBytes(text);
    const auto compressed = QtRocket::gzipDeflate(plain);
    ASSERT_TRUE(compressed.has_value()) << compressed.error().toString();
    EXPECT_TRUE(QtRocket::looksLikeGzip(*compressed));
    EXPECT_LT(compressed->size(), plain.size() / 4);

    const auto inflated = QtRocket::gzipInflate(*compressed);
    ASSERT_TRUE(inflated.has_value()) << inflated.error().toString();
    EXPECT_EQ(*inflated, plain);
}

TEST(GzipStream, RoundTripsEmptyInput)
{
    const auto compressed = QtRocket::gzipDeflate({});
    ASSERT_TRUE(compressed.has_value()) << compressed.error().toString();
    const auto inflated = QtRocket::gzipInflate(*compressed);
    ASSERT_TRUE(inflated.has_value()) << inflated.error().toString();
    EXPECT_TRUE(inflated->empty());
}

TEST(GzipStream, RejectsTruncatedAndGarbageInput)
{
    const auto plain      = QtRocket::stringToBytes(std::string(10'000, 'x'));
    const auto compressed = QtRocket::gzipDeflate(plain);
    ASSERT_TRUE(compressed.has_value());
    const std::span<const std::byte> truncated(compressed->data(), compressed->size() / 2);
    EXPECT_FALSE(QtRocket::gzipInflate(truncated).has_value());

    const auto garbage = QtRocket::stringToBytes("this is not gzip data at all");
    EXPECT_FALSE(QtRocket::looksLikeGzip(garbage));
    EXPECT_FALSE(QtRocket::gzipInflate(garbage).has_value());
}

/// A text that does not deflate to nothing: the numbers 0 to @p count - 1, a line each.
[[nodiscard]] std::vector<std::byte> numbers(int count)
{
    std::string text;
    for (int i = 0; i < count; ++i)
    {
        text += std::to_string(i) + "\n";
    }
    return QtRocket::stringToBytes(text);
}

// gzipInflatePrefix() of an intact stream is gzipInflate(): all of the data and no failure,
// also when the data is exactly as long as the limit.
TEST(GzipStream, ThePrefixOfAnIntactStreamIsAllOfIt)
{
    const std::vector<std::byte> plain      = numbers(5000);
    const auto                   compressed = QtRocket::gzipDeflate(plain);
    ASSERT_TRUE(compressed.has_value());

    const QtRocket::InflatedPrefix all = QtRocket::gzipInflatePrefix(*compressed, plain.size());
    EXPECT_FALSE(all.failure.has_value());
    EXPECT_EQ(all.bytes, plain);
}

/// What gzipInflatePrefix() makes of the gzip stream of @p plain with the limit @p maxBytes:
/// "<bytes> bytes" when all of it came, else "<bytes> bytes, <code> <message>", with ", not
/// the start of the data" behind it when the bytes are not that.
[[nodiscard]] std::string prefixOf(const std::vector<std::byte>& stream,
                                   const std::vector<std::byte>& plain, std::size_t maxBytes)
{
    const QtRocket::InflatedPrefix prefix = QtRocket::gzipInflatePrefix(stream, maxBytes);
    std::string                    text   = std::to_string(prefix.bytes.size()) + " bytes";
    if (prefix.failure.has_value())
    {
        text += ", " + std::string(toString(prefix.failure->code)) + " " + prefix.failure->message;
    }
    if (prefix.bytes.size() > plain.size() ||
        !std::equal(prefix.bytes.begin(), prefix.bytes.end(), plain.begin()))
    {
        text += ", not the start of the data";
    }
    return text;
}

// A stream that holds more than the limit is given up there: the failure is that of
// OpenRocket's bounded read, and no more than the limit is held. (The stream is inflated in
// pieces of 64 KiB, and a piece that does not fit is not kept in part.)
TEST(GzipStream, ThePrefixOfAStreamBeyondTheLimitStopsAtTheLimit)
{
    const std::vector<std::byte> plain      = numbers(50000);
    const auto                   compressed = QtRocket::gzipDeflate(plain);
    ASSERT_TRUE(compressed.has_value());
    ASSERT_EQ(plain.size(), 288890U);

    EXPECT_EQ(prefixOf(*compressed, plain, 288890), "288890 bytes");
    EXPECT_EQ(prefixOf(*compressed, plain, 288889),
              "262144 bytes, IO Input exceeds maximum size of 288889 bytes");
    EXPECT_EQ(prefixOf(*compressed, plain, 70000),
              "65536 bytes, IO Input exceeds maximum size of 70000 bytes");
    EXPECT_EQ(prefixOf(*compressed, plain, 1000),
              "0 bytes, IO Input exceeds maximum size of 1000 bytes");
    EXPECT_EQ(prefixOf(*compressed, plain, 0), "0 bytes, IO Input exceeds maximum size of 0 bytes");
}

/// The failure of @p prefix as "<code> <message>", or "none".
[[nodiscard]] std::string failureOf(const QtRocket::InflatedPrefix& prefix)
{
    if (!prefix.failure.has_value())
    {
        return "none";
    }
    const QtRocket::Error& failure = *prefix.failure;
    return std::string(toString(failure.code)) + " " + failure.message;
}

// A stream that is cut off gives everything that could be inflated of it, and the failure
// with the text of Java's EOFException: cut in its data, in the middle and near its end. Cut
// in the check sum behind its data, all of the data is there, and the failure says that it was
// met behind the data.
TEST(GzipStream, ThePrefixOfACutOffStreamIsWhatCouldBeInflated)
{
    const std::vector<std::byte> plain      = numbers(5000);
    const auto                   compressed = QtRocket::gzipDeflate(plain);
    ASSERT_TRUE(compressed.has_value());
    const std::span<const std::byte> stream(*compressed);
    const std::size_t                noLimit = plain.size() * 2;

    const QtRocket::InflatedPrefix half =
        QtRocket::gzipInflatePrefix(stream.first(stream.size() / 2), noLimit);
    EXPECT_EQ(failureOf(half), "PARSE Unexpected end of ZLIB input stream");
    EXPECT_EQ(QtRocket::kUnexpectedEndOfZlibStream, "Unexpected end of ZLIB input stream");
    EXPECT_FALSE(half.failedBehindData);
    EXPECT_EQ(half.firstMemberEnd, std::nullopt);
    EXPECT_GT(half.bytes.size(), 100U);
    EXPECT_LT(half.bytes.size(), plain.size());
    EXPECT_TRUE(std::equal(half.bytes.begin(), half.bytes.end(), plain.begin()));

    for (const std::size_t missing : {std::size_t{1}, std::size_t{4}, std::size_t{8}})
    {
        const QtRocket::InflatedPrefix inTrailer =
            QtRocket::gzipInflatePrefix(stream.first(stream.size() - missing), noLimit);
        EXPECT_EQ(failureOf(inTrailer), "PARSE Unexpected end of GZIP data") << missing;
        EXPECT_TRUE(inTrailer.failedBehindData) << missing;
        EXPECT_EQ(inTrailer.firstMemberEnd, plain.size()) << missing;
        EXPECT_EQ(inTrailer.bytes, plain) << missing;
    }
    EXPECT_EQ(QtRocket::kUnexpectedEndOfGzipStream, "Unexpected end of GZIP data");

    // gzipInflate() gives the same failures and no data.
    const auto whole = QtRocket::gzipInflate(stream.first(stream.size() / 2));
    ASSERT_FALSE(whole.has_value());
    EXPECT_EQ(whole.error().message, QtRocket::kUnexpectedEndOfZlibStream);
    const auto withoutSum = QtRocket::gzipInflate(stream.first(stream.size() - 4));
    ASSERT_FALSE(withoutSum.has_value());
    EXPECT_EQ(withoutSum.error().message, QtRocket::kUnexpectedEndOfGzipStream);
}

// A stream whose check sum or length is wrong inflates to all of its data, and then fails with
// Java's text; a stream that is no gzip stream gives nothing.
TEST(GzipStream, ThePrefixOfAStreamWithAWrongEndIsAllOfItsData)
{
    const std::vector<std::byte> plain      = numbers(5000);
    const auto                   compressed = QtRocket::gzipDeflate(plain);
    ASSERT_TRUE(compressed.has_value());
    ASSERT_EQ(plain.size(), 23890U);
    std::vector<std::byte> wrongSum = *compressed;
    wrongSum.at(wrongSum.size() - 8) ^= std::byte{0x55};
    std::vector<std::byte> wrongLength = *compressed;
    wrongLength.at(wrongLength.size() - 1) ^= std::byte{0x55};

    EXPECT_EQ(prefixOf(wrongSum, plain, plain.size()), "23890 bytes, PARSE Corrupt GZIP trailer");
    EXPECT_EQ(prefixOf(wrongLength, plain, plain.size()),
              "23890 bytes, PARSE Corrupt GZIP trailer");
    EXPECT_EQ(QtRocket::kCorruptGzipTrailer, "Corrupt GZIP trailer");
    EXPECT_TRUE(QtRocket::gzipInflatePrefix(wrongSum, plain.size()).failedBehindData);
    EXPECT_EQ(prefixOf(QtRocket::stringToBytes("this is not gzip data at all"), plain, 100),
              "0 bytes, PARSE Not in GZIP format");
}

// ------------------------------------------------------------------ the gzip container
//
// The container is read as java.util.zip.GZIPInputStream (JDK 17) reads it. The streams of
// these tests are written by hand, with their data in stored blocks, so that every length is
// what the test says. What OpenRocket makes of such streams is pinned through the loader
// (GeneralRocketLoaderTests.cpp: the gzip inputs, each checked against the Java probe).

using Bytes = std::vector<std::byte>;

[[nodiscard]] Bytes joined(std::initializer_list<Bytes> parts)
{
    Bytes all;
    for (const Bytes& part : parts)
    {
        all.insert(all.end(), part.begin(), part.end());
    }
    return all;
}

/// The ten bytes of a header without optional fields, with @p flags and @p method.
[[nodiscard]] Bytes header(unsigned flags = 0, unsigned method = 8)
{
    return {std::byte{0x1f},
            std::byte{0x8b},
            static_cast<std::byte>(method),
            static_cast<std::byte>(flags),
            std::byte{0},
            std::byte{0},
            std::byte{0},
            std::byte{0},
            std::byte{0},
            std::byte{3}};
}

/// Deflate data of one stored block that holds @p text: the last block when @p last, and
/// announcing @p declared bytes when that is not the number there are.
[[nodiscard]] Bytes block(std::string_view text, bool last = true,
                          std::optional<std::size_t> declared = std::nullopt)
{
    const auto length = static_cast<std::uint32_t>(declared.value_or(text.size()));
    Bytes      out{last ? std::byte{1} : std::byte{0}};
    QtRocket::Test::put16(out, length);
    QtRocket::Test::put16(out, ~length & 0xFFFFU);
    const Bytes bytes = QtRocket::stringToBytes(text);
    out.insert(out.end(), bytes.begin(), bytes.end());
    return out;
}

/// A member that holds @p text in one stored block behind @p start.
[[nodiscard]] Bytes member(std::string_view text, const Bytes& start = header())
{
    Bytes out = joined({start, block(text)});
    QtRocket::Test::put32(out, QtRocket::Test::zipCrc32(QtRocket::stringToBytes(text)));
    QtRocket::Test::put32(out, static_cast<std::uint32_t>(text.size()));
    return out;
}

/// What gzipInflatePrefix() makes of @p stream: the data as text in quotes, then "; <code>
/// <message>" for a failure, " behind the data" when it was met there, and "; first member
/// <n>" with the bytes of the first member that gave any.
[[nodiscard]] std::string read(const Bytes& stream)
{
    const QtRocket::InflatedPrefix prefix = QtRocket::gzipInflatePrefix(stream, 100000);
    std::string                    text   = "'" + QtRocket::bytesToString(prefix.bytes) + "'";
    if (prefix.failure.has_value())
    {
        text += "; " + failureOf(prefix) + (prefix.failedBehindData ? " behind the data" : "");
    }
    if (prefix.firstMemberEnd.has_value())
    {
        text += "; first member " + std::to_string(*prefix.firstMemberEnd);
    }
    return text;
}

// The optional fields of a header: an extra field with its length, a name and a comment that
// end with a zero byte, and the lower half of the CRC-32 of the header so far, which is
// checked. The text flag and the three reserved flags are not looked at.
TEST(GzipStream, ReadsTheOptionalFieldsOfAHeader)
{
    EXPECT_EQ(read(member("data")), "'data'; first member 4");

    const Bytes fields = joined({{std::byte{4}, std::byte{0}},
                                 QtRocket::stringToBytes("abcd"),
                                 QtRocket::stringToBytes(std::string_view("name\0", 5)),
                                 QtRocket::stringToBytes(std::string_view("comment\0", 8))});
    EXPECT_EQ(read(member("data", joined({header(0x1C), fields}))), "'data'; first member 4");
    EXPECT_EQ(read(member("data", joined({header(0x08),
                                          QtRocket::stringToBytes(std::string_view("n\0", 2))}))),
              "'data'; first member 4");

    Bytes checked = joined({header(0x1E), fields});
    Bytes wrong   = checked;
    QtRocket::Test::put16(checked, QtRocket::Test::zipCrc32(checked) & 0xFFFFU);
    QtRocket::Test::put16(wrong, (QtRocket::Test::zipCrc32(wrong) ^ 0x100U) & 0xFFFFU);
    EXPECT_EQ(read(member("data", checked)), "'data'; first member 4");
    EXPECT_EQ(read(member("data", wrong)), "''; PARSE Corrupt GZIP header");

    EXPECT_EQ(read(member("data", header(0x01))), "'data'; first member 4");
    EXPECT_EQ(read(member("data", header(0xE0))), "'data'; first member 4");
}

// A header that is none, with Java's texts; where the stream ends inside a header Java's
// exception has no message.
TEST(GzipStream, RefusesAHeaderAsJavaDoes)
{
    EXPECT_EQ(read(member("data", header(0, 7))), "''; PARSE Unsupported compression method");
    EXPECT_EQ(read(QtRocket::stringToBytes("PK and so on")), "''; PARSE Not in GZIP format");
    EXPECT_EQ(read({}), "''; PARSE Unexpected end of GZIP data");
    EXPECT_EQ(read({std::byte{0x1f}}), "''; PARSE Unexpected end of GZIP data");
    EXPECT_EQ(read({std::byte{0x1f}, std::byte{0x8b}}), "''; PARSE Unexpected end of GZIP data");
    const Bytes plain = header();
    EXPECT_EQ(read(Bytes(plain.begin(), plain.begin() + 9)),
              "''; PARSE Unexpected end of GZIP data");
    // The length of the extra field, the extra field, the name, the comment, the check sum.
    EXPECT_EQ(read(joined({header(0x04), {std::byte{4}}})),
              "''; PARSE Unexpected end of GZIP data");
    EXPECT_EQ(read(joined({header(0x04), {std::byte{4}, std::byte{0}, std::byte{1}}})),
              "''; PARSE Unexpected end of GZIP data");
    EXPECT_EQ(read(joined({header(0x08), QtRocket::stringToBytes("name")})),
              "''; PARSE Unexpected end of GZIP data");
    EXPECT_EQ(read(joined({header(0x10), QtRocket::stringToBytes("comment")})),
              "''; PARSE Unexpected end of GZIP data");
    EXPECT_EQ(read(joined({header(0x02), {std::byte{0}}})),
              "''; PARSE Unexpected end of GZIP data");
    // A header with nothing behind it: the deflate data ends before it begins.
    EXPECT_EQ(read(header()), "''; PARSE Unexpected end of ZLIB input stream");
}

// The members of a stream are read one after the other, and their data is one text. The end
// of the first member that gave any data is told.
TEST(GzipStream, ReadsTheMembersOfAStreamOneAfterTheOther)
{
    EXPECT_EQ(read(joined({member("one "), member("two")})), "'one two'; first member 4");
    EXPECT_EQ(read(joined({member("one "), member("two "), member("three")})),
              "'one two three'; first member 4");
    EXPECT_EQ(read(joined({member(""), member(""), member("two"), member("!")})),
              "'two!'; first member 3");
    EXPECT_EQ(read(member("")), "''");
    // The members written by the compressor, too.
    const auto one = QtRocket::gzipDeflate(QtRocket::stringToBytes("first, "));
    const auto two = QtRocket::gzipDeflate(QtRocket::stringToBytes("second"));
    ASSERT_TRUE(one.has_value() && two.has_value());
    EXPECT_EQ(read(joined({*one, *two})), "'first, second'; first member 7");
    const auto both = QtRocket::gzipInflate(joined({*one, *two}));
    ASSERT_TRUE(both.has_value());
    EXPECT_EQ(QtRocket::bytesToString(*both), "first, second");
}

// What follows the last member and is no header of a member ends the stream there, without a
// failure: other data, a header whose check sum is wrong or whose compression method is
// another, a header that is cut off.
TEST(GzipStream, WhatFollowsTheLastMemberAndIsNoMemberIsNotLookedAt)
{
    const Bytes one = member("one");
    EXPECT_EQ(read(joined({one, QtRocket::stringToBytes("garbage after the stream")})),
              "'one'; first member 3");
    EXPECT_EQ(read(joined({one, Bytes(64)})), "'one'; first member 3");
    EXPECT_EQ(read(joined({one, member("two", header(0, 7))})), "'one'; first member 3");
    Bytes wrong = joined({header(0x02)});
    QtRocket::Test::put16(wrong, (QtRocket::Test::zipCrc32(wrong) ^ 0x100U) & 0xFFFFU);
    EXPECT_EQ(read(joined({one, member("two", wrong)})), "'one'; first member 3");
    EXPECT_EQ(read(joined({one, header(0x08),
                           QtRocket::stringToBytes("a name without its end "
                                                   "that is long enough")})),
              "'one'; first member 3");
}

// A failure in a later member: what came before it is there, with what the member gave before
// it failed.
TEST(GzipStream, AFailureInALaterMemberKeepsWhatCameBefore)
{
    const Bytes one = member("one ");
    // Cut off in its data: a stored block that announces 32 bytes and has 20.
    EXPECT_EQ(read(joined({one, header(), block("twenty bytes of data", true, 32)})),
              "'one twenty bytes of data'; PARSE Unexpected end of ZLIB input stream; first "
              "member 4");
    // Data that is no deflate stream: a stored block whose two lengths do not fit.
    EXPECT_EQ(read(joined({one, header(), Bytes(40)})),
              "'one '; PARSE invalid deflate data in GZIP stream; first member 4");
    // A wrong check sum, and one that is cut off.
    Bytes wrongSum = joined({one, member("two")});
    wrongSum.at(wrongSum.size() - 8) ^= std::byte{0x55};
    EXPECT_EQ(read(wrongSum),
              "'one two'; PARSE Corrupt GZIP trailer behind the data; first "
              "member 4");
    const Bytes both = joined({one, member("two and something more")});
    EXPECT_EQ(read(Bytes(both.begin(), both.end() - 3)),
              "'one two and something more'; PARSE Unexpected end of GZIP data behind the data; "
              "first member 4");
}

/// What read() gives for a stream whose members held @p data, the first of them @p first
/// bytes, when the 18 bytes that the tests put behind them were looked at: their "<x>", and
/// the failure of a member that is cut off.
[[nodiscard]] std::string lookedAt(std::string_view data, std::size_t first)
{
    return std::format("'{}<x>'; PARSE Unexpected end of ZLIB input stream; first member {}", data,
                       first);
}

// The bytes behind a member are always looked at for another member, however few they are
// (GZIPInputStream once looked only when its stream had more bytes available or more than 26
// were left over in its inflater, which takes 512 bytes at a time; the JDK 17.0.20 of the
// probes always looks). Behind the members here stand the header of a member and a stored
// block that announces 16 bytes and has 3: they give their bytes, and the failure of data that
// ends too early. (OpenRocket's outcomes for the same streams around a design: the inputs "gzip
// with 18 bytes ..." of GeneralRocketLoaderTests.cpp.)
TEST(GzipStream, AlwaysLooksForAnotherMember)
{
    const Bytes eighteen = joined({header(), block("<x>", true, 16)});
    ASSERT_EQ(eighteen.size(), 18U);

    EXPECT_EQ(read(joined({member("one"), eighteen})), lookedAt("one", 3));
    // Wherever the member ends: at byte 496, 497 and 505 of a stream of 522, 523 and 531.
    const std::string at496(481, 't');
    const std::string at497(482, 't');
    const std::string at505(490, 't');
    EXPECT_EQ(read(joined({member(at496), eighteen})), lookedAt(at496, 481));
    EXPECT_EQ(read(joined({member(at497), eighteen})), lookedAt(at497, 482));
    EXPECT_EQ(read(joined({member(at505), eighteen})), lookedAt(at505, 490));
    // And behind a second member.
    EXPECT_EQ(read(joined({member("one"), member("two"), eighteen})), lookedAt("onetwo", 3));
    // Fewer than ten bytes are no header: the stream ends before them.
    EXPECT_EQ(read(joined({member("one"), Bytes(eighteen.begin(), eighteen.begin() + 9)})),
              "'one'; first member 3");
}

// The limit holds for the data of all members together.
TEST(GzipStream, TheLimitIsThatOfAllMembersTogether)
{
    const Bytes stream = joined({member("0123456789"), member("0123456789")});
    EXPECT_FALSE(QtRocket::gzipInflatePrefix(stream, 20).failure.has_value());
    const QtRocket::InflatedPrefix beyond = QtRocket::gzipInflatePrefix(stream, 19);
    EXPECT_EQ(failureOf(beyond), "IO Input exceeds maximum size of 19 bytes");
    EXPECT_EQ(QtRocket::bytesToString(beyond.bytes), "0123456789");
    EXPECT_FALSE(beyond.failedBehindData);
}

}  // namespace
