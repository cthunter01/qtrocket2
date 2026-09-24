#include "QtRocket/util/Md5.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/util/FileIo.h"

namespace
{

using QtRocket::Md5;

std::string md5Hex(std::string_view text)
{
    return QtRocket::toHex(QtRocket::md5(QtRocket::stringToBytes(text)));
}

std::string md5Hex(std::span<const std::byte> bytes)
{
    return QtRocket::toHex(QtRocket::md5(bytes));
}

// RFC 1321 appendix A.5, the test suite.
TEST(Md5, Rfc1321TestSuite)
{
    EXPECT_EQ(md5Hex(""), "d41d8cd98f00b204e9800998ecf8427e");
    EXPECT_EQ(md5Hex("a"), "0cc175b9c0f1b6a831c399e269772661");
    EXPECT_EQ(md5Hex("abc"), "900150983cd24fb0d6963f7d28e17f72");
    EXPECT_EQ(md5Hex("message digest"), "f96b697d7cb7938d525a2f31aaf161d0");
    EXPECT_EQ(md5Hex("abcdefghijklmnopqrstuvwxyz"), "c3fcd3d76192e4007dfb496cca67e13b");
    EXPECT_EQ(md5Hex("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"),
              "d174ab98d277d9f5a5611c2c9f419d9f");
    EXPECT_EQ(
        md5Hex("12345678901234567890123456789012345678901234567890123456789012345678901234567890"),
        "57edf4a22be3c955ac49da2e2107b67a");
}

// Inputs around the padding boundary: 55 bytes leave exactly room for the 0x80 byte and the
// length in one block, 56 and 64 bytes force a second block. Reference values from Python's
// hashlib.
TEST(Md5, PaddingBoundaries)
{
    EXPECT_EQ(md5Hex(std::string(55, 'a')), "ef1772b6dff9a122358552954ad0df65");
    EXPECT_EQ(md5Hex(std::string(56, 'a')), "3b0c8ac703f828b04c6c197006d17218");
    EXPECT_EQ(md5Hex(std::string(63, 'a')), "b06521f39153d618550606be297466d5");
    EXPECT_EQ(md5Hex(std::string(64, 'a')), "014842d480b571495a4a0363793f7367");
    EXPECT_EQ(md5Hex(std::string(65, 'a')), "c743a45e0d2e6a95cb859adae0248435");
    EXPECT_EQ(md5Hex(std::string(119, 'a')), "8a7bd0732ed6a28ce75f6dabc90e1613");
    EXPECT_EQ(md5Hex(std::string(120, 'a')), "5f61c0ccad4cac44c75ff505e1f1e537");
    EXPECT_EQ(md5Hex(std::string(128, 'a')), "e510683b3f5ffe4093d021808bc6ff70");
}

TEST(Md5, LongInput)
{
    EXPECT_EQ(md5Hex(std::string(1000, 'a')), "cabe45dcc9ae5b66ba86600cca6b8ba8");
}

TEST(Md5, AllByteValues)
{
    std::vector<std::byte> bytes(256);
    for (std::size_t i = 0; i < bytes.size(); ++i)
    {
        bytes[i] = static_cast<std::byte>(i);
    }
    EXPECT_EQ(md5Hex(bytes), "e2c865db4162bed963bfaa9ef6ac18f0");
}

TEST(Md5, UpdateInPiecesEqualsOneUpdate)
{
    const std::string text(200, 'x');
    const auto        bytes    = QtRocket::stringToBytes(text);
    const auto        expected = QtRocket::md5(bytes);

    // Every split point, including ones that straddle the 64-byte block boundary.
    for (std::size_t split = 0; split <= bytes.size(); split += 7)
    {
        Md5                              digest;
        const std::span<const std::byte> all{bytes};
        digest.update(all.first(split));
        digest.update(all.subspan(split));
        EXPECT_EQ(digest.finish(), expected) << "split at " << split;
    }

    // One byte at a time.
    Md5 byteWise;
    for (const std::byte b : bytes)
    {
        byteWise.update(std::span<const std::byte>{&b, 1});
    }
    EXPECT_EQ(byteWise.finish(), expected);

    // Empty updates change nothing.
    Md5 withEmpty;
    withEmpty.update({});
    withEmpty.update(bytes);
    withEmpty.update({});
    EXPECT_EQ(withEmpty.finish(), expected);
}

TEST(Md5, FinishResetsForReuse)
{
    Md5 digest;
    digest.update(QtRocket::stringToBytes("abc"));
    EXPECT_EQ(QtRocket::toHex(digest.finish()), "900150983cd24fb0d6963f7d28e17f72");
    // As with java.security.MessageDigest.digest(): the object starts over.
    EXPECT_EQ(QtRocket::toHex(digest.finish()), "d41d8cd98f00b204e9800998ecf8427e");
    digest.update(QtRocket::stringToBytes("a"));
    EXPECT_EQ(QtRocket::toHex(digest.finish()), "0cc175b9c0f1b6a831c399e269772661");
}

TEST(Md5, DigestSize)
{
    static_assert(Md5::kDigestSize == 16);
    static_assert(Md5::Digest{}.size() == 16);
    EXPECT_EQ(md5Hex("").size(), 32U);
}

TEST(Md5, ToHexIsLowercaseAndZeroPadded)
{
    const std::array<std::byte, 6> bytes = {std::byte{0x00}, std::byte{0x0a}, std::byte{0xff},
                                            std::byte{0x10}, std::byte{0xab}, std::byte{0x7f}};
    EXPECT_EQ(QtRocket::toHex(bytes), "000aff10ab7f");
    EXPECT_EQ(QtRocket::toHex({}), "");
}

// MotorDigest.java feeds each value as a big-endian int32: the data type's order, the value
// count, then the (scaled, rounded) values. This is TIME_ARRAY {0 s, 1 s} then FORCE_PER_TIME
// {0 N, 10 N, 0 N}; the expected value comes from Python's hashlib fed the same bytes.
TEST(Md5, ReproducesMotorDigestByteFeeding)
{
    const auto bigEndian = [](std::int32_t value) {
        const auto bits = static_cast<std::uint32_t>(value);
        return std::array<std::byte, 4>{static_cast<std::byte>((bits >> 24) & 0xFFU),
                                        static_cast<std::byte>((bits >> 16) & 0xFFU),
                                        static_cast<std::byte>((bits >> 8) & 0xFFU),
                                        static_cast<std::byte>(bits & 0xFFU)};
    };
    Md5        digest;
    const auto feed = [&](std::int32_t order, const std::vector<std::int32_t>& values) {
        digest.update(bigEndian(order));
        digest.update(bigEndian(static_cast<std::int32_t>(values.size())));
        for (const std::int32_t v : values)
        {
            digest.update(bigEndian(v));
        }
    };
    feed(0, {0, 1000});      // TIME_ARRAY, multiplier 1000 (ms)
    feed(5, {0, 10000, 0});  // FORCE_PER_TIME, multiplier 1000 (mN)
    EXPECT_EQ(QtRocket::toHex(digest.finish()), "2c8ad61393e52079f4acfaee69593c92");
}

}  // namespace
