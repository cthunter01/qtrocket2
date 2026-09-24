#pragma once

#include <compare>
#include <cstdint>
#include <string>

namespace QtRocket
{

/// A modification id (OpenRocket's ModID): a number that is unique during this program execution,
/// positive and monotonically increasing. Rocket components, configurations and calculators keep
/// one to tell whether a cached result is still current: a new one is drawn whenever something
/// changes, and two ids are equal only when they are the same draw.
///
/// The two constants match OpenRocket's ModID.ZERO (0) and ModID.INVALID (-1): a few places want a
/// "never drawn" value that sorts below every real id.
class ModId
{
public:
    /// Draws a fresh id from the global counter. Thread-safe and fast.
    ModId() noexcept;

    /// ModID.ZERO: the constant 0, below every drawn id.
    [[nodiscard]] static constexpr ModId zero() noexcept { return ModId{0}; }

    /// ModID.INVALID: the constant -1, below zero() and every drawn id.
    [[nodiscard]] static constexpr ModId invalid() noexcept { return ModId{-1}; }

    /// The numeric value: -1 for invalid(), 0 for zero(), positive for a drawn id.
    [[nodiscard]] constexpr std::int64_t toInt() const noexcept { return m_id; }

    /// The numeric value in decimal (ModID.toString()).
    [[nodiscard]] std::string toString() const;

    /// Ids compare by value: draw order for drawn ids, with invalid() < zero() < any draw.
    [[nodiscard]] constexpr std::strong_ordering operator<=>(const ModId& other) const noexcept =
        default;
    [[nodiscard]] constexpr bool operator==(const ModId& other) const noexcept = default;

private:
    constexpr explicit ModId(std::int64_t id) noexcept : m_id(id) { }

    std::int64_t m_id;
};

}  // namespace QtRocket
