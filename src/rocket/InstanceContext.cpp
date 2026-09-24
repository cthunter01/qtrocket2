#include "QtRocket/rocket/InstanceContext.h"

#include <cstdint>
#include <format>
#include <string>

#include "QtRocket/rocket/RocketComponent.h"

namespace QtRocket
{

std::string InstanceContext::toString() const
{
    return std::format("Context for {} #{}", component->toString(), instanceNumber);
}

bool InstanceContext::operator==(const InstanceContext& other) const noexcept
{
    return component->equals(*other.component) && transform == other.transform;
}

std::int32_t InstanceContext::hashCode() const noexcept
{
    return component->hashCode();
}

}  // namespace QtRocket
