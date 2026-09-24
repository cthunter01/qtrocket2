#pragma once

#include <array>
#include <optional>
#include <string_view>

namespace QtRocket
{

/// The categories materials are grouped into (OpenRocket's MaterialGroup instances), in priority
/// order: comparing two groups with the built-in operators is MaterialGroup.compareTo (a lower
/// priority number sorts first), and equality is by priority as there.
enum class MaterialGroup
{
    METALS,      ///< priority 0
    WOODS,       ///< priority 10
    PLASTICS,    ///< priority 20
    FABRICS,     ///< priority 30
    PAPER,       ///< priority 40, database string "PaperProducts"
    FOAMS,       ///< priority 50
    COMPOSITES,  ///< priority 60
    FIBERS,      ///< priority 70
    ELASTICS,    ///< priority 80
    KEVLARS,     ///< priority 90
    NYLONS,      ///< priority 100
    OTHER,       ///< priority 110
    CUSTOM,      ///< priority 1000, the only user-defined group
};

/// MaterialGroup.ALL_GROUPS, in declaration (priority) order.
inline constexpr std::array<MaterialGroup, 13> kAllMaterialGroups{
    MaterialGroup::METALS,     MaterialGroup::WOODS,  MaterialGroup::PLASTICS,
    MaterialGroup::FABRICS,    MaterialGroup::PAPER,  MaterialGroup::FOAMS,
    MaterialGroup::COMPOSITES, MaterialGroup::FIBERS, MaterialGroup::ELASTICS,
    MaterialGroup::KEVLARS,    MaterialGroup::NYLONS, MaterialGroup::OTHER,
    MaterialGroup::CUSTOM,
};

/// The translation key of the group's display name, e.g. "MaterialGroup.PaperProducts".
[[nodiscard]] std::string_view displayKey(MaterialGroup group) noexcept;

/// The English display name (MaterialGroup.getName with OpenRocket's English messages), e.g.
/// "Paper Products".
[[nodiscard]] std::string_view displayName(MaterialGroup group) noexcept;

/// The name stored in preferences, .ork files and preset databases, e.g. "PaperProducts".
[[nodiscard]] std::string_view databaseString(MaterialGroup group) noexcept;

/// The sort priority: lower sorts first.
[[nodiscard]] int priority(MaterialGroup group) noexcept;

/// True for CUSTOM only.
[[nodiscard]] bool isUserDefined(MaterialGroup group) noexcept;

/// MaterialGroup.loadFromDatabaseString: the group whose databaseString() is exactly @p name,
/// or nullopt (OpenRocket throws IllegalArgumentException; its null-string case, which gives
/// OTHER, has no counterpart). The pre-24.12 name "ThreadsLines" is resolved by
/// MaterialStorage::materialGroupFromLegacyDatabaseString, which needs the material database.
[[nodiscard]] std::optional<MaterialGroup> materialGroupFromDatabaseString(
    std::string_view name) noexcept;

}  // namespace QtRocket
