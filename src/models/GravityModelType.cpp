#include "QtRocket/models/GravityModelType.h"

#include <optional>
#include <string_view>

#include "QtRocket/util/Strings.h"

namespace QtRocket
{

std::string_view toStringValue(GravityModelType type) noexcept
{
    switch (type)
    {
        case GravityModelType::WGS:
            return "WGS";
        case GravityModelType::CONSTANT:
            return "Constant";
    }
    return "WGS";  // not reached: the switch covers every type
}

std::string_view gravityModelTypeName(GravityModelType type) noexcept
{
    switch (type)
    {
        case GravityModelType::WGS:
            return "WGS";
        case GravityModelType::CONSTANT:
            return "CONSTANT";
    }
    return "WGS";  // not reached
}

std::string_view orkName(GravityModelType type) noexcept
{
    switch (type)
    {
        case GravityModelType::WGS:
            return "wgs";
        case GravityModelType::CONSTANT:
            return "constant";
    }
    return "wgs";  // not reached
}

std::string_view tooltipKey(GravityModelType type) noexcept
{
    switch (type)
    {
        case GravityModelType::WGS:
            return "simedtdlg.GravityModel.WGS84.ttip";
        case GravityModelType::CONSTANT:
            return "simedtdlg.GravityModel.Constant.ttip";
    }
    return "simedtdlg.GravityModel.WGS84.ttip";  // not reached
}

std::optional<GravityModelType> gravityModelTypeFromString(std::string_view text) noexcept
{
    for (const GravityModelType type : kAllGravityModelTypes)
    {
        if (Strings::javaEqualsIgnoreCase(toStringValue(type), text))
        {
            return type;
        }
    }
    return std::nullopt;
}

}  // namespace QtRocket
