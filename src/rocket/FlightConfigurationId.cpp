#include "QtRocket/rocket/FlightConfigurationId.h"

#include <cstdint>
#include <string>
#include <string_view>

#include "QtRocket/util/Strings.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket
{

FlightConfigurationId::FlightConfigurationId() : m_key(Uuid::random()) { }

FlightConfigurationId FlightConfigurationId::fromString(std::string_view text)
{
    if (text.empty())
    {
        return FlightConfigurationId{};
    }
    if (const auto parsed = Uuid::javaFromString(text))
    {
        return FlightConfigurationId{*parsed};
    }
    return FlightConfigurationId{Uuid::fromSigned(0, std::int64_t{Strings::javaHashCode(text)})};
}

std::string FlightConfigurationId::toShortKey() const
{
    if (hasError())
    {
        return std::string{kErrorKeyName};
    }
    if (isDefaultId())
    {
        return std::string{kDefaultValueName};
    }
    return m_key.toString().substr(0, 8);
}

}  // namespace QtRocket
