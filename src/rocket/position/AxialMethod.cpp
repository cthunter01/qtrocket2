#include "QtRocket/rocket/position/AxialMethod.h"

#include <optional>
#include <string_view>

#include "QtRocket/util/Strings.h"

namespace QtRocket
{

std::string_view axialMethodName(AxialMethod method) noexcept
{
    switch (method)
    {
        case AxialMethod::ABSOLUTE:
            return "ABSOLUTE";
        case AxialMethod::AFTER:
            return "AFTER";
        case AxialMethod::TOP:
            return "TOP";
        case AxialMethod::MIDDLE:
            return "MIDDLE";
        case AxialMethod::BOTTOM:
            return "BOTTOM";
    }
    return "AFTER";
}

std::string_view orkName(AxialMethod method) noexcept
{
    switch (method)
    {
        case AxialMethod::ABSOLUTE:
            return "absolute";
        case AxialMethod::AFTER:
            return "after";
        case AxialMethod::TOP:
            return "top";
        case AxialMethod::MIDDLE:
            return "middle";
        case AxialMethod::BOTTOM:
            return "bottom";
    }
    return "after";
}

std::optional<AxialMethod> axialMethodFromOrkName(std::string_view text)
{
    for (const AxialMethod method : kAllAxialMethods)
    {
        if (Strings::orkEnumNameMatches(text, axialMethodName(method)))
        {
            return method;
        }
    }
    return std::nullopt;
}

std::string_view displayKey(AxialMethod method) noexcept
{
    switch (method)
    {
        case AxialMethod::ABSOLUTE:
            return "RocketComponent.Position.Method.Axial.ABSOLUTE";
        case AxialMethod::AFTER:
            return "RocketComponent.Position.Method.Axial.AFTER";
        case AxialMethod::TOP:
            return "RocketComponent.Position.Method.Axial.TOP";
        case AxialMethod::MIDDLE:
            return "RocketComponent.Position.Method.Axial.MIDDLE";
        case AxialMethod::BOTTOM:
            return "RocketComponent.Position.Method.Axial.BOTTOM";
    }
    return "RocketComponent.Position.Method.Axial.AFTER";
}

std::string_view displayName(AxialMethod method) noexcept
{
    switch (method)
    {
        case AxialMethod::ABSOLUTE:
            return "Tip of the rocket";
        case AxialMethod::AFTER:
            return "After the sibling component";
        case AxialMethod::TOP:
            return "Top of the parent component";
        case AxialMethod::MIDDLE:
            return "Middle of the parent component";
        case AxialMethod::BOTTOM:
            return "Bottom of the parent component";
    }
    return "After the sibling component";
}

}  // namespace QtRocket
