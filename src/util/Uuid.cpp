#include "QtRocket/util/Uuid.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <mutex>
#include <random>
#include <string>
#include <string_view>

#include "QtRocket/util/Error.h"

namespace QtRocket
{

namespace
{

constexpr std::size_t kTextLength = 36;  // 32 hexadecimal digits and 4 dashes

[[nodiscard]] constexpr bool isDashPosition(std::size_t index) noexcept
{
    return index == 8 || index == 13 || index == 18 || index == 23;
}

/// The value of a hexadecimal digit in either case, or -1.
[[nodiscard]] constexpr int hexValue(char ch) noexcept
{
    if (ch >= '0' && ch <= '9')
    {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f')
    {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F')
    {
        return ch - 'A' + 10;
    }
    return -1;
}

[[nodiscard]] std::mt19937_64 makeSeededEngine()
{
    // std::random_device yields 32 bits per call; a seed sequence spreads several of them over
    // the engine's whole state.
    std::random_device                             device;
    std::array<std::random_device::result_type, 8> entropy{};
    for (auto& word : entropy)
    {
        word = device();
    }
    std::seed_seq seed(entropy.begin(), entropy.end());
    return std::mt19937_64{seed};
}

}  // namespace

Uuid Uuid::random()
{
    static std::mutex      s_mutex;
    static std::mt19937_64 s_engine = makeSeededEngine();

    std::uint64_t most  = 0;
    std::uint64_t least = 0;
    {
        const std::scoped_lock lock{s_mutex};
        most  = s_engine();
        least = s_engine();
    }
    // RFC 4122 section 4.4, as java.util.UUID.randomUUID(): version 4 in bits 12-15 of the most
    // significant half, variant 0b10 in the top two bits of the least significant half.
    most  = (most & ~std::uint64_t{0xF000U}) | std::uint64_t{0x4000U};
    least = (least & ~(std::uint64_t{0x3} << 62)) | (std::uint64_t{0x2} << 62);
    return Uuid{most, least};
}

Result<Uuid> Uuid::parse(std::string_view text)
{
    if (text.size() != kTextLength)
    {
        return fail(ErrorCode::PARSE, std::format("not a UUID: '{}'", text));
    }
    std::uint64_t most   = 0;
    std::uint64_t least  = 0;
    std::size_t   digits = 0;
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        const char ch = text[i];
        if (isDashPosition(i))
        {
            if (ch != '-')
            {
                return fail(ErrorCode::PARSE, std::format("not a UUID: '{}'", text));
            }
            continue;
        }
        const int value = hexValue(ch);
        if (value < 0)
        {
            return fail(ErrorCode::PARSE, std::format("not a UUID: '{}'", text));
        }
        std::uint64_t& half = digits < 16 ? most : least;
        half                = (half << 4) | static_cast<std::uint64_t>(value);
        ++digits;
    }
    return Uuid{most, least};
}

std::string Uuid::toString() const
{
    // java.util.UUID.toString(): time_low-time_mid-time_hi_and_version-variant_and_seq-node.
    return std::format("{:08x}-{:04x}-{:04x}-{:04x}-{:012x}", m_mostSignificantBits >> 32,
                       (m_mostSignificantBits >> 16) & 0xFFFFU, m_mostSignificantBits & 0xFFFFU,
                       m_leastSignificantBits >> 48, m_leastSignificantBits & 0xFFFFFFFFFFFFU);
}

}  // namespace QtRocket
