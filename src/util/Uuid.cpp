#include "QtRocket/util/Uuid.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <limits>
#include <mutex>
#include <random>
#include <string>
#include <string_view>

#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

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

/// The largest value of a Java long.
constexpr auto kLongMax = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());

/// The message of the NumberFormatException Long.parseLong(name, begin, end, 16) throws for the
/// character at the byte @p index of @p group (NumberFormatException.forCharSequence()): the
/// place is counted in UTF-16 code units, as Java counts.
[[nodiscard]] std::unexpected<std::string> errorAt(std::string_view group, std::size_t index)
{
    return std::unexpected(std::format("Error at index {} in: \"{}\"",
                                       Strings::javaLength(group.substr(0, index)), group));
}

/// Java's Long.parseLong(name, begin, end, 16) for one group of a UUID: an optional '+', then
/// one or more hexadecimal digits whose value fits a signed 64-bit long. A group that is not
/// that gives the message of Java's NumberFormatException instead: an empty one for an empty
/// group, else errorAt() the first character that is wrong: a character that is no digit, the
/// end of a group that is only a sign, or the digit that would take the value beyond
/// Long.MAX_VALUE. A '-' sign cannot occur, the groups being split at the dashes.
[[nodiscard]] std::expected<std::uint64_t, std::string> parseJavaHexLong(std::string_view group)
{
    if (group.empty())
    {
        return std::unexpected(std::string{});
    }
    std::size_t index = 0;
    // "if (firstChar < '0')": a possible sign; of the characters below '0' only '+' is one here.
    if (static_cast<unsigned char>(group.front()) < static_cast<unsigned char>('0'))
    {
        if (group.front() != '+')
        {
            return errorAt(group, 0);
        }
        index = 1;
        if (group.size() == 1)
        {
            return errorAt(group, 1);  // Cannot have lone "+"
        }
    }
    std::uint64_t value = 0;
    for (; index < group.size(); ++index)
    {
        const int digit = hexValue(group[index]);
        // "digit < 0 || result < multmin": no digit, or sixteen times the value so far is
        // beyond Long.MAX_VALUE.
        if (digit < 0 || value > kLongMax / 16)
        {
            return errorAt(group, index);
        }
        value = (value * 16) + static_cast<std::uint64_t>(digit);
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
    // JDK 17's UUID.fromString(), that is its fromString1(): the quick path it has for the
    // canonical form gives the same value and leaves every other text to fromString1().
    if (Strings::javaLength(text) > kTextLength)
    {
        return fail(ErrorCode::PARSE, "UUID string too large");
    }
    // "dash4 < 0" and "dash5 >= 0": exactly four dashes, whatever stands between them.
    if (std::ranges::count(text, '-') != 4)
    {
        return fail(ErrorCode::PARSE, std::format("Invalid UUID string: {}", text));
    }
    std::array<std::uint64_t, 5> groups{};
    std::string_view             rest = text;
    for (std::uint64_t& group : groups)
    {
        // Long.parseLong(name, begin, end, 16) of each group in turn: the first that is no
        // number fails with its NumberFormatException.
        const std::size_t                               dash = rest.find('-');
        const std::expected<std::uint64_t, std::string> value =
            parseJavaHexLong(rest.substr(0, dash));
        if (!value.has_value())
        {
            return fail(ErrorCode::PARSE, value.error());
        }
        group = *value;
        rest  = dash == std::string_view::npos ? std::string_view{} : rest.substr(dash + 1);
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
