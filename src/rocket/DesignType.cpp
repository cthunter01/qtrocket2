#include "QtRocket/rocket/DesignType.h"

#include <optional>
#include <string_view>

#include "QtRocket/util/Strings.h"

namespace QtRocket
{

std::string_view designTypeName(DesignType type) noexcept
{
    switch (type)
    {
        case DesignType::ORIGINAL:
            return "ORIGINAL";
        case DesignType::COMMERCIAL_KIT:
            return "COMMERCIAL_KIT";
        case DesignType::CLONE_KIT:
            return "CLONE_KIT";
        case DesignType::UPSCALE_KIT:
            return "UPSCALE_KIT";
        case DesignType::DOWNSCALE_KIT:
            return "DOWNSCALE_KIT";
        case DesignType::MODIFIED_KIT:
            return "MODIFIED_KIT";
        case DesignType::KIT_BASH:
            return "KIT_BASH";
    }
    return "ORIGINAL";
}

std::string_view orkName(DesignType type) noexcept
{
    switch (type)
    {
        case DesignType::ORIGINAL:
            return "original";
        case DesignType::COMMERCIAL_KIT:
            return "commercialkit";
        case DesignType::CLONE_KIT:
            return "clonekit";
        case DesignType::UPSCALE_KIT:
            return "upscalekit";
        case DesignType::DOWNSCALE_KIT:
            return "downscalekit";
        case DesignType::MODIFIED_KIT:
            return "modifiedkit";
        case DesignType::KIT_BASH:
            return "kitbash";
    }
    return "original";
}

std::optional<DesignType> designTypeFromOrkName(std::string_view text)
{
    for (const DesignType type : kAllDesignTypes)
    {
        if (Strings::orkEnumNameMatches(text, designTypeName(type)))
        {
            return type;
        }
    }
    return std::nullopt;
}

std::string_view displayKey(DesignType type) noexcept
{
    switch (type)
    {
        case DesignType::ORIGINAL:
            return "DesignType.Originaldesign";
        case DesignType::COMMERCIAL_KIT:
            return "DesignType.Commercialkit";
        case DesignType::CLONE_KIT:
            return "DesignType.Clonekit";
        case DesignType::UPSCALE_KIT:
            return "DesignType.Upscalekit";
        case DesignType::DOWNSCALE_KIT:
            return "DesignType.Downscalekit";
        case DesignType::MODIFIED_KIT:
            return "DesignType.Modificationkit";
        case DesignType::KIT_BASH:
            return "DesignType.Kitbashkit";
    }
    return "DesignType.Originaldesign";
}

std::string_view displayName(DesignType type) noexcept
{
    switch (type)
    {
        case DesignType::ORIGINAL:
            return "Original Design/Other";
        case DesignType::COMMERCIAL_KIT:
            return "Commercial Kit";
        case DesignType::CLONE_KIT:
            return "Clone of Commercial Kit";
        case DesignType::UPSCALE_KIT:
            return "Upscale of Commercial Kit";
        case DesignType::DOWNSCALE_KIT:
            return "Downscale of Commercial Kit";
        case DesignType::MODIFIED_KIT:
            return "Modification of a Commercial Kit";
        case DesignType::KIT_BASH:
            return "Kit Bash of Commercial Kits";
    }
    return "Original Design/Other";
}

}  // namespace QtRocket
