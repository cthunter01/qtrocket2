#include "QtRocket/util/Sha256.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <vector>

namespace QtRocket
{

namespace
{

constexpr std::size_t kBlockSize    = 64;
constexpr std::size_t kLengthOffset = kBlockSize - 8;  // where the 64-bit bit count goes

// FIPS 180-4 section 4.2.2: the first 32 bits of the fractional parts of the cube roots of the
// first 64 primes.
constexpr std::array<std::uint32_t, 64> kRoundConstants = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

// FIPS 180-4 section 5.3.3: the first 32 bits of the fractional parts of the square roots of
// the first 8 primes.
constexpr std::array<std::uint32_t, 8> kInitialState = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

[[nodiscard]] std::uint32_t readBigEndian32(std::span<const std::byte> bytes) noexcept
{
    return (std::to_integer<std::uint32_t>(bytes[0]) << 24) |
           (std::to_integer<std::uint32_t>(bytes[1]) << 16) |
           (std::to_integer<std::uint32_t>(bytes[2]) << 8) |
           std::to_integer<std::uint32_t>(bytes[3]);
}

/// FIPS 180-4 section 6.2.2: one block of 64 bytes into the state.
void processBlock(std::array<std::uint32_t, 8>& state, std::span<const std::byte> block) noexcept
{
    std::array<std::uint32_t, 64> schedule{};
    for (std::size_t t = 0; t < 16; ++t)
    {
        schedule.at(t) = readBigEndian32(block.subspan(4 * t, 4));
    }
    for (std::size_t t = 16; t < schedule.size(); ++t)
    {
        const std::uint32_t before15 = schedule.at(t - 15);
        const std::uint32_t before2  = schedule.at(t - 2);
        const std::uint32_t sigma0 =
            std::rotr(before15, 7) ^ std::rotr(before15, 18) ^ (before15 >> 3);
        const std::uint32_t sigma1 =
            std::rotr(before2, 17) ^ std::rotr(before2, 19) ^ (before2 >> 10);
        schedule.at(t) = schedule.at(t - 16) + sigma0 + schedule.at(t - 7) + sigma1;
    }

    std::array<std::uint32_t, 8> v = state;  // the working variables a to h
    for (std::size_t t = 0; t < schedule.size(); ++t)
    {
        const std::uint32_t sum1   = std::rotr(v[4], 6) ^ std::rotr(v[4], 11) ^ std::rotr(v[4], 25);
        const std::uint32_t choice = (v[4] & v[5]) ^ (~v[4] & v[6]);
        const std::uint32_t temp1  = v[7] + sum1 + choice + kRoundConstants.at(t) + schedule.at(t);
        const std::uint32_t sum0   = std::rotr(v[0], 2) ^ std::rotr(v[0], 13) ^ std::rotr(v[0], 22);
        const std::uint32_t majority = (v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]);
        const std::uint32_t temp2    = sum0 + majority;
        v = {temp1 + temp2, v[0], v[1], v[2], v[3] + temp1, v[4], v[5], v[6]};
    }
    for (std::size_t i = 0; i < state.size(); ++i)
    {
        state.at(i) += v.at(i);
    }
}

/// FIPS 180-4 section 5.1.1: what follows the message in its last block or two, namely a 1 bit,
/// zeros up to 8 bytes before the end of a block, and the length of the message in bits.
[[nodiscard]] std::vector<std::byte> padding(std::size_t messageSize)
{
    const std::size_t used = messageSize % kBlockSize;
    const std::size_t zeros =
        (used < kLengthOffset ? kLengthOffset : kBlockSize + kLengthOffset) - used - 1;
    std::vector<std::byte> tail;
    tail.reserve(1 + zeros + 8);
    tail.push_back(std::byte{0x80});
    tail.insert(tail.end(), zeros, std::byte{0});
    const std::uint64_t bits = static_cast<std::uint64_t>(messageSize) * 8;
    for (int shift = 56; shift >= 0; shift -= 8)
    {
        tail.push_back(static_cast<std::byte>((bits >> shift) & 0xFFU));
    }
    return tail;
}

}  // namespace

std::string sha256Hex(std::span<const std::byte> data)
{
    std::array<std::uint32_t, 8> state = kInitialState;
    const std::size_t            whole = data.size() - (data.size() % kBlockSize);
    for (std::size_t offset = 0; offset < whole; offset += kBlockSize)
    {
        processBlock(state, data.subspan(offset, kBlockSize));
    }
    // The rest of the message and the padding: one block or two.
    const std::span<const std::byte> rest = data.subspan(whole);
    std::vector<std::byte>           last(rest.begin(), rest.end());
    const std::vector<std::byte>     tail = padding(data.size());
    last.insert(last.end(), tail.begin(), tail.end());
    const std::span<const std::byte> lastBlocks{last};
    for (std::size_t offset = 0; offset < lastBlocks.size(); offset += kBlockSize)
    {
        processBlock(state, lastBlocks.subspan(offset, kBlockSize));
    }
    std::string hex;
    for (const std::uint32_t word : state)
    {
        hex += std::format("{:08x}", word);
    }
    return hex;
}

}  // namespace QtRocket
