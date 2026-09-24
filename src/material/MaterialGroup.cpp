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
    static constexpr GroupInfo kMetals{.displayKey     = "MaterialGroup.Metals",
                                       .displayName    = "Metals",
                                       .databaseString = "Metals",
                                       .priority       = 0,
                                       .userDefined    = false};
    static constexpr GroupInfo kWoods{.displayKey     = "MaterialGroup.Woods",
                                      .displayName    = "Woods",
                                      .databaseString = "Woods",
                                      .priority       = 10,
                                      .userDefined    = false};
    static constexpr GroupInfo kPlastics{.displayKey     = "MaterialGroup.Plastics",
                                         .displayName    = "Plastics",
                                         .databaseString = "Plastics",
                                         .priority       = 20,
                                         .userDefined    = false};
    static constexpr GroupInfo kFabrics{.displayKey     = "MaterialGroup.Fabrics",
                                        .displayName    = "Fabrics",
                                        .databaseString = "Fabrics",
                                        .priority       = 30,
                                        .userDefined    = false};
    static constexpr GroupInfo kPaper{.displayKey     = "MaterialGroup.PaperProducts",
                                      .displayName    = "Paper Products",
                                      .databaseString = "PaperProducts",
                                      .priority       = 40,
                                      .userDefined    = false};
    static constexpr GroupInfo kFoams{.displayKey     = "MaterialGroup.Foams",
                                      .displayName    = "Foams",
                                      .databaseString = "Foams",
                                      .priority       = 50,
                                      .userDefined    = false};
    static constexpr GroupInfo kComposites{.displayKey     = "MaterialGroup.Composites",
                                           .displayName    = "Composites",
                                           .databaseString = "Composites",
                                           .priority       = 60,
                                           .userDefined    = false};
    static constexpr GroupInfo kFibers{.displayKey     = "MaterialGroup.Fibers",
                                       .displayName    = "Fibers",
                                       .databaseString = "Fibers",
                                       .priority       = 70,
                                       .userDefined    = false};
    static constexpr GroupInfo kElastics{.displayKey     = "MaterialGroup.Elastics",
                                         .displayName    = "Elastics",
                                         .databaseString = "Elastics",
                                         .priority       = 80,
                                         .userDefined    = false};
    static constexpr GroupInfo kKevlars{.displayKey     = "MaterialGroup.Kevlars",
                                        .displayName    = "Kevlars",
                                        .databaseString = "Kevlars",
                                        .priority       = 90,
                                        .userDefined    = false};
    static constexpr GroupInfo kNylons{.displayKey     = "MaterialGroup.Nylons",
                                       .displayName    = "Nylons",
                                       .databaseString = "Nylons",
                                       .priority       = 100,
                                       .userDefined    = false};
    static constexpr GroupInfo kOther{.displayKey     = "MaterialGroup.Other",
                                      .displayName    = "Other",
                                      .databaseString = "Other",
                                      .priority       = 110,
                                      .userDefined    = false};
    static constexpr GroupInfo kCustom{.displayKey     = "MaterialGroup.Custom",
                                       .displayName    = "Custom",
                                       .databaseString = "Custom",
                                       .priority       = 1000,
                                       .userDefined    = true};

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
