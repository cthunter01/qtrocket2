#include "QtRocket/file/GzipStream.h"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"

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
// with the text of Java's EOFException: cut in its data, in the middle and near its end, and
// cut in the check sum behind its data, where all of the data is there.
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
    EXPECT_GT(half.bytes.size(), 100U);
    EXPECT_LT(half.bytes.size(), plain.size());
    EXPECT_TRUE(std::equal(half.bytes.begin(), half.bytes.end(), plain.begin()));

    const QtRocket::InflatedPrefix inTrailer =
        QtRocket::gzipInflatePrefix(stream.first(stream.size() - 4), noLimit);
    EXPECT_EQ(failureOf(inTrailer), "PARSE Unexpected end of ZLIB input stream");
    EXPECT_EQ(inTrailer.bytes, plain);

    // gzipInflate() gives the same failure and no data.
    const auto whole = QtRocket::gzipInflate(stream.first(stream.size() / 2));
    ASSERT_FALSE(whole.has_value());
    EXPECT_EQ(whole.error().message, QtRocket::kUnexpectedEndOfZlibStream);
}

// A stream whose check sum or length is wrong inflates to all of its data, and then fails
// (with a text of QtRocket's own: Java's is "Corrupt GZIP trailer"); a stream that is no gzip
// stream gives nothing.
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

    EXPECT_EQ(prefixOf(wrongSum, plain, plain.size()),
              "23890 bytes, PARSE gzip: corrupt or truncated stream (minizip error -3)");
    EXPECT_EQ(prefixOf(wrongLength, plain, plain.size()),
              "23890 bytes, PARSE gzip: corrupt or truncated stream (minizip error -3)");
    EXPECT_EQ(prefixOf(QtRocket::stringToBytes("this is not gzip data at all"), plain, 100),
              "0 bytes, PARSE gzip: corrupt or truncated stream (minizip error -3)");
}

}  // namespace
