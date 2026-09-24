#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// A 128-bit universally unique identifier with the same textual form as java.util.UUID: the
/// canonical 8-4-4-4-12 lowercase hexadecimal form, e.g. "123e4567-e89b-12d3-a456-426614174000".
/// Components, flight configurations, warnings and flight events are identified by one, and the
/// ids are written into .ork files, so the text must round-trip with OpenRocket exactly.
///
/// The value is held as java.util.UUID does, as the most and least significant 64 bits.
class Uuid
{
public:
    /// The nil UUID, all zero (also java.util.UUID's `new UUID(0, 0)`).
    constexpr Uuid() noexcept = default;

    /// java.util.UUID's `new UUID(mostSigBits, leastSigBits)`; takes the bits as they are, with no
    /// version or variant set. Java's arguments are signed longs: a 32-bit Java int (e.g. a
    /// String.hashCode(), as in FlightConfigurationId(String)) is sign-extended into the low 64
    /// bits there, so pass such a value through fromSigned() rather than casting it to unsigned.
    constexpr Uuid(std::uint64_t mostSignificantBits, std::uint64_t leastSignificantBits) noexcept
      : m_mostSignificantBits(mostSignificantBits), m_leastSignificantBits(leastSignificantBits)
    {
    }

    /// `new UUID(long, long)` with Java's signed arguments: fromSigned(0, -1) is
    /// 00000000-0000-0000-ffff-ffffffffffff, and a 32-bit int passed here is sign-extended first,
    /// as Java widens it.
    [[nodiscard]] static constexpr Uuid fromSigned(std::int64_t mostSignificantBits,
                                                   std::int64_t leastSignificantBits) noexcept
    {
        return Uuid{static_cast<std::uint64_t>(mostSignificantBits),
                    static_cast<std::uint64_t>(leastSignificantBits)};
    }

    /// A random RFC 4122 version 4 UUID (java.util.UUID.randomUUID()). Thread-safe: the generator
    /// is seeded once from std::random_device and shared under a lock.
    [[nodiscard]] static Uuid random();

    /// Parses the canonical 8-4-4-4-12 form, in either case. Anything else, including the
    /// shortened groups java.util.UUID.fromString() tolerates, fails with ErrorCode::PARSE.
    [[nodiscard]] static Result<Uuid> parse(std::string_view text);

    [[nodiscard]] static constexpr Uuid nil() noexcept { return Uuid{}; }

    [[nodiscard]] constexpr std::uint64_t mostSignificantBits() const noexcept
    {
        return m_mostSignificantBits;
    }
    [[nodiscard]] constexpr std::uint64_t leastSignificantBits() const noexcept
    {
        return m_leastSignificantBits;
    }

    /// The version number in bits 12-15 of the most significant half (4 for random UUIDs).
    [[nodiscard]] constexpr int version() const noexcept
    {
        return static_cast<int>((m_mostSignificantBits >> 12) & 0xFU);
    }

    /// The variant field (java.util.UUID.variant()): 0 for NCS, 2 for RFC 4122, 6 for Microsoft,
    /// 7 reserved.
    [[nodiscard]] constexpr int variant() const noexcept
    {
        // Java shifts by (64 - top two bits) and masks a shift of 64 to 0; C++ cannot shift by
        // 64, so the top-bit-clear case is answered directly (Java gives 0 for it too).
        if ((m_leastSignificantBits >> 63) == 0)
        {
            return 0;
        }
        return static_cast<int>(m_leastSignificantBits >> (64 - (m_leastSignificantBits >> 62)));
    }

    [[nodiscard]] constexpr bool isNil() const noexcept
    {
        return m_mostSignificantBits == 0 && m_leastSignificantBits == 0;
    }

    /// The lowercase canonical form, exactly what java.util.UUID.toString() prints.
    [[nodiscard]] std::string toString() const;

    /// java.util.UUID.compareTo(): the most significant halves, then the least significant ones,
    /// each compared as a signed 64-bit value.
    [[nodiscard]] constexpr std::strong_ordering operator<=>(const Uuid& other) const noexcept
    {
        const std::strong_ordering most = static_cast<std::int64_t>(m_mostSignificantBits) <=>
                                          static_cast<std::int64_t>(other.m_mostSignificantBits);
        if (most != std::strong_ordering::equal)
        {
            return most;
        }
        return static_cast<std::int64_t>(m_leastSignificantBits) <=>
               static_cast<std::int64_t>(other.m_leastSignificantBits);
    }
    [[nodiscard]] constexpr bool operator==(const Uuid& other) const noexcept = default;

private:
    std::uint64_t m_mostSignificantBits{0};
    std::uint64_t m_leastSignificantBits{0};
};

}  // namespace QtRocket

template <>
struct std::hash<QtRocket::Uuid>
{
    [[nodiscard]] std::size_t operator()(const QtRocket::Uuid& id) const noexcept
    {
        // Boost's hash_combine of the two halves: unlike java.util.UUID.hashCode()'s plain xor,
        // swapping the halves gives a different value.
        const std::hash<std::uint64_t> hash;
        std::size_t                    seed = hash(id.mostSignificantBits());
        seed ^= hash(id.leastSignificantBits()) + 0x9e3779b9U + (seed << 6) + (seed >> 2);
        return seed;
    }
};
