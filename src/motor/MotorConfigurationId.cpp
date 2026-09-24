#include "QtRocket/motor/MotorConfigurationId.h"

#include <cstddef>
#include <cstdint>
#include <string>

#include "QtRocket/util/Uuid.h"

namespace QtRocket
{

namespace
{

/// `((long) mountId.hashCode()) << 32`: the sign extension of the int is shifted out, so the
/// hash lands unchanged in the upper 32 bits and the lower ones are zero.
[[nodiscard]] std::uint64_t mountHashBits(const Uuid& mountId) noexcept
{
    return static_cast<std::uint64_t>(static_cast<std::uint32_t>(mountId.hashCode())) << 32U;
}

}  // namespace

MotorConfigurationId::MotorConfigurationId(const Uuid& mountId,
                                           const Uuid& flightConfigurationKey) noexcept
  : m_key(mountHashBits(mountId), flightConfigurationKey.mostSignificantBits())
{
}

std::string MotorConfigurationId::toString() const
{
    return m_key.toString();
}

std::string MotorConfigurationId::toShortKey() const
{
    const std::string     keyString    = m_key.toString();
    const std::size_t     lastIndex    = keyString.size() - 1;
    constexpr std::size_t kChunkLength = 4;
    // the head and tail of the full id; the tail stops one short of the last character
    return keyString.substr(0, kChunkLength) + "/" +
           keyString.substr(lastIndex - kChunkLength, kChunkLength);
}

std::string MotorConfigurationId::toDebug() const
{
    return toShortKey();
}

}  // namespace QtRocket
