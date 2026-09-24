#include "QtRocket/rocket/position/RadiusMethod.h"

#include <optional>
#include <string_view>

#include "QtRocket/rocket/Coaxial.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/RadiusPositionable.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// @p parent as a body tube (Java: parentComponent instanceof BodyTube), else null.
[[nodiscard]] const Coaxial* asBodyTube(const RocketComponent* parent)
{
    if (parent != nullptr && parent->kind() == ComponentKind::BODY_TUBE)
    {
        return dynamic_cast<const Coaxial*>(parent);
    }
    return nullptr;
}

}  // namespace

double getRadius(RadiusMethod method, const RocketComponent* parentComponent,
                 const RocketComponent* thisComponent, double requestedOffset)
{
    switch (method)
    {
        case RadiusMethod::COAXIAL:
            return 0.0;
        case RadiusMethod::FREE:
            return requestedOffset;
        case RadiusMethod::RELATIVE:
        case RadiusMethod::SURFACE:
        {
            double radius = (method == RadiusMethod::RELATIVE) ? requestedOffset : 0.0;
            if (const Coaxial* tube = asBodyTube(parentComponent))
            {
                radius += tube->getOuterRadius();
            }
            if (const auto* positionable = dynamic_cast<const RadiusPositionable*>(thisComponent))
            {
                radius += positionable->getBoundingRadius();
            }
            return radius;
        }
    }
    return 0.0;
}

double getAsOffset(RadiusMethod method, const RocketComponent* parentComponent,
                   const RocketComponent* thisComponent, double radius)
{
    switch (method)
    {
        case RadiusMethod::COAXIAL:
        case RadiusMethod::SURFACE:
            return 0.0;
        case RadiusMethod::FREE:
            return radius;
        case RadiusMethod::RELATIVE:
        {
            double offset = radius;
            if (const Coaxial* tube = asBodyTube(parentComponent))
            {
                offset -= tube->getOuterRadius();
            }
            if (const auto* positionable = dynamic_cast<const RadiusPositionable*>(thisComponent))
            {
                offset -= positionable->getBoundingRadius();
            }
            return offset;
        }
    }
    return 0.0;
}

std::string_view radiusMethodName(RadiusMethod method) noexcept
{
    switch (method)
    {
        case RadiusMethod::COAXIAL:
            return "COAXIAL";
        case RadiusMethod::FREE:
            return "FREE";
        case RadiusMethod::RELATIVE:
            return "RELATIVE";
        case RadiusMethod::SURFACE:
            return "SURFACE";
    }
    return "COAXIAL";
}

std::string_view orkName(RadiusMethod method) noexcept
{
    switch (method)
    {
        case RadiusMethod::COAXIAL:
            return "coaxial";
        case RadiusMethod::FREE:
            return "free";
        case RadiusMethod::RELATIVE:
            return "relative";
        case RadiusMethod::SURFACE:
            return "surface";
    }
    return "coaxial";
}

std::optional<RadiusMethod> radiusMethodFromOrkName(std::string_view text)
{
    for (const RadiusMethod method : kAllRadiusMethods)
    {
        if (Strings::orkEnumNameMatches(text, radiusMethodName(method)))
        {
            return method;
        }
    }
    return std::nullopt;
}

std::string_view displayKey(RadiusMethod method) noexcept
{
    switch (method)
    {
        case RadiusMethod::COAXIAL:
            return "RocketComponent.Position.Method.Radius.COAXIAL";
        case RadiusMethod::FREE:
            return "RocketComponent.Position.Method.Radius.FREE";
        case RadiusMethod::RELATIVE:
            return "RocketComponent.Position.Method.Radius.RELATIVE";
        case RadiusMethod::SURFACE:
            return "RocketComponent.Position.Method.Radius.SURFACE";
    }
    return "RocketComponent.Position.Method.Radius.COAXIAL";
}

std::string_view displayName(RadiusMethod method) noexcept
{
    switch (method)
    {
        case RadiusMethod::COAXIAL:
            return "Same axis as the target component";
        case RadiusMethod::FREE:
            return "Center of the parent component";
        case RadiusMethod::RELATIVE:
            return "Surface of the parent component";
        case RadiusMethod::SURFACE:
            return "Surface of the parent component (without offset)";
    }
    return "Same axis as the target component";
}

}  // namespace QtRocket
