#pragma once

#include <array>
#include <optional>
#include <string_view>

namespace QtRocket
{

/// What kind of design a rocket is: an original design, a commercial kit, a clone of one, ...
/// (OpenRocket's DesignType).
enum class DesignType
{
    ORIGINAL,
    COMMERCIAL_KIT,
    CLONE_KIT,
    UPSCALE_KIT,
    DOWNSCALE_KIT,
    MODIFIED_KIT,
    KIT_BASH,
};

/// Every design type, in declaration order (DesignType.values()).
inline constexpr std::array<DesignType, 7> kAllDesignTypes{
    DesignType::ORIGINAL,    DesignType::COMMERCIAL_KIT, DesignType::CLONE_KIT,
    DesignType::UPSCALE_KIT, DesignType::DOWNSCALE_KIT,  DesignType::MODIFIED_KIT,
    DesignType::KIT_BASH};

/// The constant's name, e.g. "KIT_BASH" (Java: name()).
[[nodiscard]] std::string_view designTypeName(DesignType type) noexcept;

/// The .ork spelling (Java: getStorableString()): the name lower-cased without underscores,
/// e.g. "commercialkit".
[[nodiscard]] std::string_view orkName(DesignType type) noexcept;

/// The design type @p text names, matched as DocumentConfig.findEnum() does; nullopt for anything
/// else.
[[nodiscard]] std::optional<DesignType> designTypeFromOrkName(std::string_view text);

/// The translation key of the display name, e.g. "DesignType.Commercialkit".
[[nodiscard]] std::string_view displayKey(DesignType type) noexcept;

/// The English display name (Java: getName()), e.g. "Commercial Kit".
[[nodiscard]] std::string_view displayName(DesignType type) noexcept;

}  // namespace QtRocket
