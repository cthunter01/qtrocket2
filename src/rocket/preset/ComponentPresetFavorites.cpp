#include "QtRocket/rocket/preset/ComponentPresetFavorites.h"

#include <string>
#include <utility>

#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"

namespace QtRocket::ComponentPresetFavorites
{

namespace
{

/// The node of @p type's favourites under the favourites node, or nullptr; creates nothing.
[[nodiscard]] const Preferences* findTypeNode(const Preferences&  preferences,
                                              ComponentPresetType type)
{
    const Preferences* const favorites = preferences.findNode(PreferenceKeys::kFavoritePresetsNode);
    return favorites == nullptr ? nullptr : favorites->findNode(componentPresetTypeName(type));
}

}  // namespace

FavoriteKeys getFavorites(const Preferences& preferences, ComponentPresetType type)
{
    FavoriteKeys             keys;
    const Preferences* const node = findTypeNode(preferences, type);
    if (node != nullptr)
    {
        for (std::string& key : node->keys())
        {
            keys.insert(std::move(key));
        }
    }
    return keys;
}

bool isFavorite(const Preferences& preferences, const ComponentPreset& preset,
                ComponentPresetType type)
{
    const Preferences* const node = findTypeNode(preferences, type);
    return node != nullptr && node->get(preset.preferenceKey()).has_value();
}

void setFavorite(Preferences& preferences, const ComponentPreset& preset, ComponentPresetType type,
                 bool favorite)
{
    Preferences& node = preferences.getNode(PreferenceKeys::kFavoritePresetsNode)
                            .getNode(componentPresetTypeName(type));
    if (favorite)
    {
        node.putBoolean(preset.preferenceKey(), true);
    }
    else
    {
        node.remove(preset.preferenceKey());
    }
}

}  // namespace QtRocket::ComponentPresetFavorites
