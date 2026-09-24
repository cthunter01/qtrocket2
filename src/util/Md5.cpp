#include "QtRocket/util/Md5.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace QtRocket
{

namespace
{

// RFC 1321 section 3.4: T[i] = floor(2^32 * abs(sin(i + 1))).
constexpr std::array<std::uint32_t, 64> kSineTable = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};

// RFC 1321 section 3.4: the per-step left-rotation amounts.
constexpr std::array<int, 64> kShifts = {
    7,  12, 17, 22, 7,  12, 17, 22, 7,  12, 17, 22, 7,  12, 17, 22, 5,  9,  14, 20, 5,  9,
    14, 20, 5,  9,  14, 20, 5,  9,  14, 20, 4,  11, 16, 23, 4,  11, 16, 23, 4,  11, 16, 23,
    4,  11, 16, 23, 6,  10, 15, 21, 6,  10, 15, 21, 6,  10, 15, 21, 6,  10, 15, 21};

// RFC 1321 section 3.3: the initial state A, B, C, D.
constexpr std::array<std::uint32_t, 4> kInitialState = {0x67452301, 0xefcdab89, 0x98badcfe,
                                                        0x10325476};

// RFC 1321 section 3.1: a single 1 bit followed by zeros, up to a whole block.
constexpr std::array<std::byte, Md5::kBlockSize> kPadding = [] {
    std::array<std::byte, Md5::kBlockSize> padding{};
    padding[0] = std::byte{0x80};
    return padding;
}();

constexpr std::size_t kLengthOffset = Md5::kBlockSize - 8;  // where the 64-bit bit count goes

[[nodiscard]] std::uint32_t readLittleEndian32(std::span<const std::byte> bytes) noexcept
{
    return std::to_integer<std::uint32_t>(bytes[0]) |
           (std::to_integer<std::uint32_t>(bytes[1]) << 8) |
           (std::to_integer<std::uint32_t>(bytes[2]) << 16) |
           (std::to_integer<std::uint32_t>(bytes[3]) << 24);
}

void writeLittleEndian32(std::span<std::byte> out, std::uint32_t value) noexcept
{
    out[0] = static_cast<std::byte>(value & 0xFFU);
    out[1] = static_cast<std::byte>((value >> 8) & 0xFFU);
    out[2] = static_cast<std::byte>((value >> 16) & 0xFFU);
    out[3] = static_cast<std::byte>((value >> 24) & 0xFFU);
}

void writeLittleEndian64(std::span<std::byte> out, std::uint64_t value) noexcept
{
    writeLittleEndian32(out.first(4), static_cast<std::uint32_t>(value & 0xFFFFFFFFU));
    writeLittleEndian32(out.subspan(4, 4), static_cast<std::uint32_t>(value >> 32));
}

}  // namespace

Md5::Md5() noexcept
{
    reset();
}

void Md5::reset() noexcept
{
    m_state      = kInitialState;
    m_buffer     = {};
    m_bufferSize = 0;
    m_totalBytes = 0;
}

void Md5::update(std::span<const std::byte> data) noexcept
{
    m_totalBytes += data.size();
    const std::span<std::byte> buffer{m_buffer};
    while (!data.empty())
    {
        if (m_bufferSize == 0 && data.size() >= kBlockSize)
        {
            // A whole block straight from the input: no need to stage it in the buffer.
            processBlock(data.first<kBlockSize>());
            data = data.subspan(kBlockSize);
            continue;
        }
        const std::size_t take = std::min(kBlockSize - m_bufferSize, data.size());
        std::ranges::copy(data.first(take), buffer.subspan(m_bufferSize).begin());
        m_bufferSize += take;
        data = data.subspan(take);
        if (m_bufferSize == kBlockSize)
        {
            processBlock(buffer.first<kBlockSize>());
            m_bufferSize = 0;
        }
    }
}

Md5::Digest Md5::finish() noexcept
{
    // The bit count goes into the last 8 bytes of the final block, after at least one padding
    // byte.
    const std::uint64_t bitLength = m_totalBytes * 8;  // modulo 2^64, as RFC 1321 specifies
    const std::size_t   padLength = m_bufferSize < kLengthOffset
                                        ? kLengthOffset - m_bufferSize
                                        : (kBlockSize + kLengthOffset) - m_bufferSize;
    update(std::span<const std::byte>{kPadding}.first(padLength));
    std::array<std::byte, 8> length{};
    writeLittleEndian64(length, bitLength);
    update(length);

    Digest                               digest{};
    const std::span<std::byte>           out{digest};
    const std::span<const std::uint32_t> state{m_state};
    for (std::size_t i = 0; i < state.size(); ++i)
    {
        writeLittleEndian32(out.subspan(4 * i, 4), state[i]);
    }
    reset();
    return digest;
}

void Md5::processBlock(std::span<const std::byte, kBlockSize> block) noexcept
{
    std::array<std::uint32_t, 16>  wordStorage{};
    const std::span<std::uint32_t> words{wordStorage};
    for (std::size_t i = 0; i < words.size(); ++i)
    {
        words[i] = readLittleEndian32(block.subspan(4 * i, 4));
    }

    const std::span<const std::uint32_t> sineTable{kSineTable};
    const std::span<const int>           shifts{kShifts};

    std::uint32_t a = m_state[0];
    std::uint32_t b = m_state[1];
    std::uint32_t c = m_state[2];
    std::uint32_t d = m_state[3];

    for (std::size_t i = 0; i < 64; ++i)
    {
        std::uint32_t f = 0;
        std::size_t   g = 0;
        if (i < 16)
        {
            f = (b & c) | (~b & d);
            g = i;
        }
        else if (i < 32)
        {
            f = (d & b) | (~d & c);
            g = ((5 * i) + 1) % 16;
        }
        else if (i < 48)
        {
            f = b ^ c ^ d;
            g = ((3 * i) + 5) % 16;
        }
        else
        {
            f = c ^ (b | ~d);
            g = (7 * i) % 16;
        }
        const std::uint32_t rotated = std::rotl(a + f + sineTable[i] + words[g], shifts[i]);
        a                           = d;
        d                           = c;
        c                           = b;
        b += rotated;
    }

    m_state[0] += a;
    m_state[1] += b;
    m_state[2] += c;
    m_state[3] += d;
}

Md5::Digest md5(std::span<const std::byte> data) noexcept
{
    Md5 digest;
    digest.update(data);
    return digest.finish();
}

std::string toHex(std::span<const std::byte> bytes)
{
    constexpr std::string_view kDigits = "0123456789abcdef";
    std::string                out;
    out.reserve(bytes.size() * 2);
    for (const std::byte b : bytes)
    {
        const auto value = std::to_integer<unsigned int>(b);
        out.push_back(kDigits[value >> 4]);
        out.push_back(kDigits[value & 0xFU]);
    }
    return out;
}

}  // namespace QtRocket
