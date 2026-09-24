#include "QtRocket/util/Uuid.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <mutex>
#include <optional>
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

/// Java's Long.parseLong(group, 16) for one group of a UUID: an optional '+', then one or more
/// hexadecimal digits whose value fits a signed 64-bit long; nullopt otherwise (Java:
/// NumberFormatException). A '-' sign cannot occur, the groups being split at the dashes.
[[nodiscard]] std::optional<std::uint64_t> parseJavaHexLong(std::string_view group) noexcept
{
    if (!group.empty() && group.front() == '+')
    {
        group.remove_prefix(1);
    }
    if (group.empty())
    {
        return std::nullopt;
    }
    constexpr auto kLimit = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    std::uint64_t  value  = 0;
    for (const char ch : group)
    {
        const int digit = hexValue(ch);
        if (digit < 0)
        {
            return std::nullopt;
        }
        const auto digitValue = static_cast<std::uint64_t>(digit);
        if (value > (kLimit - digitValue) / 16)
        {
            return std::nullopt;  // beyond Long.MAX_VALUE
        }
        value = (value * 16) + digitValue;
    }
    return value;
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

Result<Uuid> Uuid::javaFromString(std::string_view text)
{
    // JDK 17's UUID.fromString(): at most 36 characters, exactly four dashes, five groups.
    if (text.size() > kTextLength)
    {
        return fail(ErrorCode::PARSE, std::format("UUID string too large: '{}'", text));
    }
    std::array<std::uint64_t, 5> groups{};
    std::size_t                  groupIndex = 0;
    std::string_view             rest       = text;
    while (true)
    {
        const std::size_t      dash  = rest.find('-');
        const std::string_view group = rest.substr(0, dash);
        if (groupIndex >= groups.size())
        {
            return fail(ErrorCode::PARSE, std::format("Invalid UUID string: '{}'", text));
        }
        const std::optional<std::uint64_t> value = parseJavaHexLong(group);
        if (!value)
        {
            return fail(ErrorCode::PARSE, std::format("Invalid UUID string: '{}'", text));
        }
        groups.at(groupIndex) = *value;
        ++groupIndex;
        if (dash == std::string_view::npos)
        {
            break;
        }
        rest = rest.substr(dash + 1);
    }
    if (groupIndex != groups.size())
    {
        return fail(ErrorCode::PARSE, std::format("Invalid UUID string: '{}'", text));
    }

    // The low 32, 16, 16, 16 and 48 bits of the groups.
    const std::uint64_t most =
        ((groups[0] & 0xFFFFFFFFU) << 32U) | ((groups[1] & 0xFFFFU) << 16U) | (groups[2] & 0xFFFFU);
    const std::uint64_t least = ((groups[3] & 0xFFFFU) << 48U) | (groups[4] & 0xFFFFFFFFFFFFU);
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
