// Tests of the SHA-256 of the test support (Sha256.h), against the answers of FIPS 180-4's
// examples and of sha256sum for the lengths at which the padding changes its shape.

#include "Sha256.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/util/FileIo.h"

namespace
{

using QtRocket::Test::sha256Hex;

/// The digest of @p text.
[[nodiscard]] std::string digestOf(std::string_view text)
{
    return sha256Hex(QtRocket::stringToBytes(text));
}

/// The digest of @p count letters a.
[[nodiscard]] std::string digestOfLetters(std::size_t count)
{
    return digestOf(std::string(count, 'a'));
}

TEST(Sha256, DigestsTheStandardExamples)
{
    EXPECT_EQ(digestOf(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_EQ(digestOf("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    EXPECT_EQ(digestOf("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
              "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    EXPECT_EQ(digestOfLetters(1000000),
              "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

// The padding is a 1 bit, zeros and the 8 bytes of the length: a message that leaves fewer than
// 9 bytes of its last block free (56 to 63 bytes over a whole number of blocks) gets a block
// more.
TEST(Sha256, PadsAtEveryLengthAroundABlock)
{
    EXPECT_EQ(digestOfLetters(55),
              "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318");
    EXPECT_EQ(digestOfLetters(56),
              "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a");
    EXPECT_EQ(digestOfLetters(63),
              "7d3e74a05d7db15bce4ad9ec0658ea98e3f06eeecf16b4c6fff2da457ddc2f34");
    EXPECT_EQ(digestOfLetters(64),
              "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb");
    EXPECT_EQ(digestOfLetters(65),
              "635361c48bb9eab14198e76ea8ab7f1a41685d6ad62aa9146d301d4f17eb0ae0");
    EXPECT_EQ(digestOfLetters(119),
              "31eba51c313a5c08226adf18d4a359cfdfd8d2e816b13f4af952f7ea6584dcfb");
    EXPECT_EQ(digestOfLetters(120),
              "2f3d335432c70b580af0e8e1b3674a7c020d683aa5f73aaaedfdc55af904c21c");
    EXPECT_EQ(digestOfLetters(128),
              "6836cf13bac400e9105071cd6af47084dfacad4e5e302c94bfed24e013afb73e");
}

TEST(Sha256, DigestsEveryByteValue)
{
    std::vector<std::byte> bytes;
    bytes.reserve(256);
    for (int value = 0; value < 256; ++value)
    {
        bytes.push_back(static_cast<std::byte>(value));
    }
    EXPECT_EQ(sha256Hex(bytes), "40aff2e9d2d8922e47afd4648e6967497158785fbd1da870e7110266bf944880");
}

}  // namespace
