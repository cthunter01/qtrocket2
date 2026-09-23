#include "QtRocket/file/GzipStream.h"

#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include <gtest/gtest.h>

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

}  // namespace
