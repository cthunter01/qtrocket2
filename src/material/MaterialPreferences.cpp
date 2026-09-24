#include "QtRocket/material/MaterialPreferences.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket
{

namespace
{

namespace Keys = PreferenceKeys;

/// The prefix of the keys addUserMaterial() stores under ("material0", "material1", ...).
constexpr std::string_view kUserMaterialKeyPrefix = "material";

/// HashSet<Material>.contains: a material with the same hashCode() that equals() @p material.
[[nodiscard]] bool javaSetContains(const std::vector<Material>& materials, const Material& material)
{
    return std::ranges::any_of(materials, [&material](const Material& element) {
        return element.hashCode() == material.hashCode() && element == material;
    });
}

/// Databases.findMaterial(BULK, name) for SwingPreferences.loadDefaultComponentMaterials().
void setBuiltinDefault(Preferences& preferences, std::string_view className,
                       std::string_view materialName, const MaterialStorage& storage)
{
    setDefaultComponentMaterial(preferences, className,
                                storage.findMaterial(Material::Type::BULK, materialName));
}

}  // namespace

Material getDefaultComponentMaterial(const Preferences& preferences, ComponentClassChain classChain,
                                     Material::Type type, const MaterialStorage& storage)
{
    const std::optional<std::string> stored =
        preferences.getDefaultComponentMaterialString(classChain);
    if (stored.has_value())
    {
        Result<Material> material = Material::fromStorableString(*stored, false, storage);
        if (material.has_value() && material->getType() == type)
        {
            return std::move(*material);
        }
    }

    std::string_view fallback;
    switch (type)
    {
        case Material::Type::LINE:
            fallback = kDefaultLineMaterialName;
            break;
        case Material::Type::SURFACE:
            fallback = kDefaultSurfaceMaterialName;
            break;
        case Material::Type::BULK:
            fallback = kDefaultBulkMaterialName;
            break;
        case Material::Type::CUSTOM:
            bug(std::format("Unknown material type: {}", toString(type)));
    }
    if (std::optional<Material> material = storage.findMaterial(type, fallback);
        material.has_value())
    {
        return std::move(*material);
    }
    bug(std::format("the material storage has no built-in {} material \"{}\"", toString(type),
                    fallback));
}

void setDefaultComponentMaterial(Preferences& preferences, std::string_view className,
                                 const std::optional<Material>& material)
{
    if (!material.has_value())
    {
        preferences.setDefaultComponentMaterialString(className, std::nullopt);
        return;
    }
    preferences.setDefaultComponentMaterialString(className, material->toStorableString());
}

void loadDefaultComponentMaterials(Preferences& preferences, const MaterialStorage& storage)
{
    setBuiltinDefault(preferences, "FinSet", "Balsa", storage);
    setBuiltinDefault(preferences, "NoseCone", "Polystyrene", storage);
}

std::vector<Material> getUserMaterials(const Preferences&     preferences,
                                       const MaterialStorage& storage)
{
    std::vector<Material>    materials;
    const Preferences* const node = preferences.findNode(Keys::kUserMaterialsNode);
    if (node == nullptr)
    {
        return materials;
    }
    for (const std::string& key : node->keys())
    {
        const std::optional<std::string> value = node->get(key);
        if (!value.has_value())
        {
            continue;
        }
        // Java logs "Illegal material string" and goes on.
        Result<Material> material = Material::fromStorableString(*value, true, storage);
        if (material.has_value() && !javaSetContains(materials, *material))
        {
            materials.push_back(std::move(*material));
        }
    }
    return materials;
}

void addUserMaterial(Preferences& preferences, const Material& material,
                     const MaterialStorage& storage)
{
    if (javaSetContains(getUserMaterials(preferences, storage), material))
    {
        return;
    }
    Preferences&      node     = preferences.getNode(Keys::kUserMaterialsNode);
    const std::string storable = material.toStorableString();
    for (std::size_t i = 0;; ++i)
    {
        const std::string key = std::format("{}{}", kUserMaterialKeyPrefix, i);
        if (!node.get(key).has_value())
        {
            node.put(key, storable);
            return;
        }
    }
}

void removeUserMaterial(Preferences& preferences, const Material& material,
                        const MaterialStorage& storage)
{
    Preferences& node = preferences.getNode(Keys::kUserMaterialsNode);
    for (const std::string& key : node.keys())
    {
        const std::optional<std::string> value = node.get(key);
        if (!value.has_value())
        {
            continue;
        }
        const Result<Material> existing = Material::fromStorableString(*value, true, storage);
        if (existing.has_value() && *existing == material)
        {
            node.remove(key);
        }
    }
}

std::size_t addUserMaterials(MaterialStorage& storage, const Preferences& preferences)
{
    std::size_t added = 0;
    for (const Material& material : getUserMaterials(preferences, storage))
    {
        // Never CUSTOM: fromStorableString() rejects that type (Java warns in a default branch).
        if (storage.addMaterial(material))
        {
            ++added;
        }
    }
    return added;
}

std::array<Signal<const Material&>::ScopedConnection, 2> storeUserMaterialChanges(
    MaterialStorage& storage, Preferences& preferences)
{
    using Connection = Signal<const Material&>::ScopedConnection;
    // MaterialStorage.elementAdded stores only user-defined materials; the storage's
    // userMaterialAdded signal already fires for those alone.
    return {Connection{storage.userMaterialAdded.connect(
                [&preferences, &storage](const Material& material) {
                    addUserMaterial(preferences, material, storage);
                })},
            Connection{storage.userMaterialRemoved.connect(
                [&preferences, &storage](const Material& material) {
                    removeUserMaterial(preferences, material, storage);
                })}};
}

}  // namespace QtRocket
