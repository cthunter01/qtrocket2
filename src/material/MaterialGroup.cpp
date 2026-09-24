#include "QtRocket/material/MaterialGroup.h"

#include <optional>
#include <string_view>

namespace QtRocket
{

namespace
{

struct GroupInfo
{
    std::string_view displayKey;
    std::string_view displayName;
    std::string_view databaseString;
    int              priority;
    bool             userDefined;
};

/// MaterialGroup's constructor arguments per group, with the English message texts.
[[nodiscard]] const GroupInfo& info(MaterialGroup group) noexcept
{
    static constexpr GroupInfo kMetals{"MaterialGroup.Metals", "Metals", "Metals", 0, false};
    static constexpr GroupInfo kWoods{"MaterialGroup.Woods", "Woods", "Woods", 10, false};
    static constexpr GroupInfo kPlastics{"MaterialGroup.Plastics", "Plastics", "Plastics", 20,
                                         false};
    static constexpr GroupInfo kFabrics{"MaterialGroup.Fabrics", "Fabrics", "Fabrics", 30, false};
    static constexpr GroupInfo kPaper{"MaterialGroup.PaperProducts", "Paper Products",
                                      "PaperProducts", 40, false};
    static constexpr GroupInfo kFoams{"MaterialGroup.Foams", "Foams", "Foams", 50, false};
    static constexpr GroupInfo kComposites{"MaterialGroup.Composites", "Composites", "Composites",
                                           60, false};
    static constexpr GroupInfo kFibers{"MaterialGroup.Fibers", "Fibers", "Fibers", 70, false};
    static constexpr GroupInfo kElastics{"MaterialGroup.Elastics", "Elastics", "Elastics", 80,
                                         false};
    static constexpr GroupInfo kKevlars{"MaterialGroup.Kevlars", "Kevlars", "Kevlars", 90, false};
    static constexpr GroupInfo kNylons{"MaterialGroup.Nylons", "Nylons", "Nylons", 100, false};
    static constexpr GroupInfo kOther{"MaterialGroup.Other", "Other", "Other", 110, false};
    static constexpr GroupInfo kCustom{"MaterialGroup.Custom", "Custom", "Custom", 1000, true};

    switch (group)
    {
        case MaterialGroup::METALS:
            return kMetals;
        case MaterialGroup::WOODS:
            return kWoods;
        case MaterialGroup::PLASTICS:
            return kPlastics;
        case MaterialGroup::FABRICS:
            return kFabrics;
        case MaterialGroup::PAPER:
            return kPaper;
        case MaterialGroup::FOAMS:
            return kFoams;
        case MaterialGroup::COMPOSITES:
            return kComposites;
        case MaterialGroup::FIBERS:
            return kFibers;
        case MaterialGroup::ELASTICS:
            return kElastics;
        case MaterialGroup::KEVLARS:
            return kKevlars;
        case MaterialGroup::NYLONS:
            return kNylons;
        case MaterialGroup::OTHER:
            return kOther;
        case MaterialGroup::CUSTOM:
            return kCustom;
    }
    return kOther;
}

}  // namespace

std::string_view displayKey(MaterialGroup group) noexcept
{
    return info(group).displayKey;
}

std::string_view displayName(MaterialGroup group) noexcept
{
    return info(group).displayName;
}

std::string_view databaseString(MaterialGroup group) noexcept
{
    return info(group).databaseString;
}

int priority(MaterialGroup group) noexcept
{
    return info(group).priority;
}

bool isUserDefined(MaterialGroup group) noexcept
{
    return info(group).userDefined;
}

std::optional<MaterialGroup> materialGroupFromDatabaseString(std::string_view name) noexcept
{
    for (const MaterialGroup group : kAllMaterialGroups)
    {
        if (databaseString(group) == name)
        {
            return group;
        }
    }
    return std::nullopt;
}

}  // namespace QtRocket
