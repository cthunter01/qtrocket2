#include "QtRocket/gui/binding/PresetDiameterFilters.h"

#include <algorithm>
#include <optional>
#include <span>

#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedKey.h"

namespace QtRocket::Gui
{

namespace
{

/// The last key of ORDERED_KEY_LIST that the type displays and that is @p first or @p second
/// (the dialog's loop over the list, which keeps the last match).
[[nodiscard]] std::optional<AnyTypedKey> lastDisplayedOf(ComponentPresetType type,
                                                         const AnyTypedKey&  first,
                                                         const AnyTypedKey&  second)
{
    const std::span<const AnyTypedKey> displayed = ComponentPreset::displayedColumns(type);
    std::optional<AnyTypedKey>         found;
    for (const AnyTypedKey& key : ComponentPreset::orderedKeyList())
    {
        if ((key == first || key == second) && std::ranges::find(displayed, key) != displayed.end())
        {
            found = key;
        }
    }
    return found;
}

}  // namespace

std::optional<AnyTypedKey> PresetDiameterFilters::foreDiameterKey(ComponentPresetType type)
{
    return lastDisplayedOf(type, ComponentPreset::kOuterDiameter,
                           ComponentPreset::kForeOuterDiameter);
}

std::optional<AnyTypedKey> PresetDiameterFilters::aftDiameterKey(ComponentPresetType type)
{
    return lastDisplayedOf(type, ComponentPreset::kOuterDiameter,
                           ComponentPreset::kAftOuterDiameter);
}

PresetDiameterFilters PresetDiameterFilters::forComponent(const RocketComponent& component)
{
    PresetDiameterFilters                    filters;
    const std::optional<ComponentPresetType> type = component.getPresetType();
    const auto* symmetric = dynamic_cast<const SymmetricComponent*>(&component);
    if (!type.has_value() || symmetric == nullptr)
    {
        return filters;
    }
    const SymmetricComponent* previous = symmetric->getPreviousSymmetricComponent();
    if (previous != nullptr && foreDiameterKey(*type).has_value())
    {
        filters.foreDiameter = previous->getAftRadius() * 2.0;
    }
    const auto* noseCone    = dynamic_cast<const NoseCone*>(symmetric);
    filters.flippedNoseCone = noseCone != nullptr && noseCone->isFlipped();
    if (!aftDiameterKey(*type).has_value())
    {
        return filters;
    }
    if (filters.flippedNoseCone)
    {
        // The base meets the previous component's aft end (Java: its fore radius, bug 16).
        if (previous != nullptr)
        {
            filters.aftDiameter = previous->getAftRadius() * 2.0;
        }
    }
    else if (const SymmetricComponent* next = symmetric->getNextSymmetricComponent();
             next != nullptr)
    {
        filters.aftDiameter = next->getForeRadius() * 2.0;
    }
    return filters;
}

}  // namespace QtRocket::Gui
