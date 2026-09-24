#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "QtRocket/util/Uuid.h"

namespace QtRocket
{

/// The immutable identifier of a motor configuration: the motor of one mount in one flight
/// configuration (OpenRocket's MotorConfigurationId). Its key is a UUID whose most significant
/// half is the Java hashCode() of the mount component's id shifted into the upper 32 bits, and
/// whose least significant half is the most significant half of the flight configuration's key,
/// bit for bit as OpenRocket derives it.
///
/// OpenRocket's constructor takes the MotorMount and the FlightConfigurationId; the rocket model
/// lives above this layer, so the caller passes the mount's getID() and the configuration's key.
/// OpenRocket's toString() also has an ERROR_KEY branch, but it compares the key with a
/// separately made UUID by identity, which never holds; that dead branch is not ported.
class MotorConfigurationId
{
public:
    MotorConfigurationId(const Uuid& mountId, const Uuid& flightConfigurationKey) noexcept;

    /// The derived key.
    [[nodiscard]] const Uuid& getKey() const noexcept { return m_key; }

    /// Equal when the keys are (equals()).
    [[nodiscard]] bool operator==(const MotorConfigurationId& other) const noexcept = default;

    /// The key's java.util.UUID.hashCode() (hashCode()).
    [[nodiscard]] std::int32_t hashCode() const noexcept { return m_key.javaHashCode(); }

    /// The key in the canonical UUID form.
    [[nodiscard]] std::string toString() const;

    /// The first four characters of toString(), "/", and the four before its last character:
    /// "4ae4/d11d" for "4ae455d2-0000-0000-6ba7-b8109dad11d1" (toShortKey()).
    [[nodiscard]] std::string toShortKey() const;

    /// toShortKey() (toDebug()).
    [[nodiscard]] std::string toDebug() const;

private:
    Uuid m_key;
};

}  // namespace QtRocket

template <>
struct std::hash<QtRocket::MotorConfigurationId>
{
    [[nodiscard]] std::size_t operator()(const QtRocket::MotorConfigurationId& id) const noexcept
    {
        return std::hash<QtRocket::Uuid>{}(id.getKey());
    }
};
