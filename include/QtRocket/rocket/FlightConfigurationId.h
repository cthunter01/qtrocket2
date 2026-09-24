#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

#include "QtRocket/util/Uuid.h"

namespace QtRocket
{

/// The identifier of a flight configuration (OpenRocket's FlightConfigurationId): a Uuid wrapper
/// that gives the configuration keys of components, simulations and .ork files their own type.
///
/// Two special ids exist, with OpenRocket's literal values: defaultValueId() keys the default
/// value of every FlightConfigurableParameterSet, and errorId() stands for "no valid id".
///
/// Deviation: Java decides isDefaultId() and hasError() by the identity of the key's UUID object
/// (and FlightConfigurableParameterSet.set() by the identity of the id), so an id rebuilt from
/// the default key's text is not "the default" there. Ids are values here, so equality decides:
/// the difference shows only for an id constructed from the literal default or error key, which
/// OpenRocket never writes to a file.
class FlightConfigurationId
{
public:
    /// The key of the default id: new UUID(0xF4F2F1F0, 5676) in Java, whose int literal is sign
    /// extended to the long 0xFFFFFFFFF4F2F1F0, i.e. "ffffffff-f4f2-f1f0-0000-00000000162c".
    static constexpr Uuid kDefaultValueUuid{0xFFFFFFFFF4F2F1F0ULL, 5676U};
    /// The key of the error id, new UUID(0xF4F2F1F0, 2489): "ffffffff-f4f2-f1f0-0000-0000000009b9".
    static constexpr Uuid kErrorUuid{0xFFFFFFFFF4F2F1F0ULL, 2489U};

    /// toShortKey() of the default id.
    static constexpr std::string_view kDefaultValueName = "DefaultKey";
    /// toShortKey() of the error id.
    static constexpr std::string_view kErrorKeyName = "ErrorKey";

    /// A new id with a random key (Java: new FlightConfigurationId()).
    FlightConfigurationId();

    /// The id with the key @p key (Java: new FlightConfigurationId(UUID); Java's null gives the
    /// error id, see errorId()).
    constexpr explicit FlightConfigurationId(const Uuid& key) noexcept : m_key(key) { }

    /// Java's new FlightConfigurationId(String): a random key for an empty @p text, the key
    /// java.util.UUID.fromString() parses from it (Uuid::javaFromString(), which also takes
    /// shortened groups such as "1-2-3-4-5"), and otherwise new UUID(0, text.hashCode()) (the
    /// Java String hash, sign-extended).
    [[nodiscard]] static FlightConfigurationId fromString(std::string_view text);

    /// The id every parameter set keeps its default value under (Java: DEFAULT_VALUE_FCID).
    [[nodiscard]] static constexpr FlightConfigurationId defaultValueId() noexcept
    {
        return FlightConfigurationId{kDefaultValueUuid};
    }

    /// The "no valid id" marker (Java: ERROR_FCID).
    [[nodiscard]] static constexpr FlightConfigurationId errorId() noexcept
    {
        return FlightConfigurationId{kErrorUuid};
    }

    /// The key (Java: the public field key).
    [[nodiscard]] constexpr const Uuid& key() const noexcept { return m_key; }

    /// True for the default id.
    [[nodiscard]] constexpr bool isDefaultId() const noexcept { return m_key == kDefaultValueUuid; }

    /// True for the error id.
    [[nodiscard]] constexpr bool hasError() const noexcept { return m_key == kErrorUuid; }

    /// !hasError().
    [[nodiscard]] constexpr bool isValid() const noexcept { return !hasError(); }

    /// "ErrorKey" for the error id, "DefaultKey" for the default id, else the first eight
    /// characters of the key.
    [[nodiscard]] std::string toShortKey() const;

    /// The whole key (Java: toFullKey(), the same as toString()).
    [[nodiscard]] std::string toFullKey() const { return toString(); }

    /// The key's canonical text.
    [[nodiscard]] std::string toString() const { return m_key.toString(); }

    /// toShortKey() (Java: toDebug()).
    [[nodiscard]] std::string toDebug() const { return toShortKey(); }

    /// The key's java.util.UUID.hashCode().
    [[nodiscard]] constexpr std::int32_t hashCode() const noexcept { return m_key.hashCode(); }

    /// Ids compare by key, as Java's compareTo() (UUID.compareTo, signed halves).
    [[nodiscard]] constexpr std::strong_ordering operator<=>(
        const FlightConfigurationId& other) const noexcept = default;
    [[nodiscard]] constexpr bool operator==(const FlightConfigurationId& other) const noexcept =
        default;

private:
    Uuid m_key;
};

}  // namespace QtRocket

template <>
struct std::hash<QtRocket::FlightConfigurationId>
{
    [[nodiscard]] std::size_t operator()(const QtRocket::FlightConfigurationId& id) const noexcept
    {
        return std::hash<QtRocket::Uuid>{}(id.key());
    }
};
