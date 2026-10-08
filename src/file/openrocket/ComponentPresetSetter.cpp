#include "QtRocket/file/openrocket/ComponentPresetSetter.h"

#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/rocket/preset/TypedKey.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

using PresetPtr = std::shared_ptr<const ComponentPreset>;

/// The first preset of @p database for @p manufacturer and @p partNo whose digest is @p digest,
/// or null (Java: the loop over ComponentPresetDao.find(), whose second branch is never taken).
[[nodiscard]] PresetPtr findPreset(const ComponentPresetDatabase* database,
                                   std::string_view manufacturer, std::string_view partNo,
                                   std::optional<std::string_view> digest)
{
    if (database == nullptr || !digest.has_value())
    {
        return nullptr;
    }
    for (PresetPtr& preset : database->find(manufacturer, partNo))
    {
        if (preset->getDigest() == *digest)
        {
            // Found one with matching digest.  Take it.
            return std::move(preset);
        }
    }
    return nullptr;
}

/// Whether a recovery device would take the material @p preset has under @p key and that
/// material is not of @p type. RecoveryDevice::loadFromPreset() and Parachute::loadFromPreset()
/// take a preset's material only when its text ("name (density)", Material::toString()) is
/// longer than 12 characters, as OpenRocket's do, and have the device's default material in
/// the place of a shorter one, whatever its type.
[[nodiscard]] bool takesMaterialOfAnotherType(const ComponentPreset&    preset,
                                              const TypedKey<Material>& key, Material::Type type)
{
    if (!preset.has(key))
    {
        return false;
    }
    const Material& material = preset.get(key);
    return Strings::javaLength(material.toString()) > 12 && material.getType() != type;
}

/// Whether RocketComponent::loadPreset() can load @p preset into @p component: false where it
/// throws BugError (see the class comment).
[[nodiscard]] bool canLoad(const RocketComponent& component, const ComponentPreset& preset)
{
    const ComponentKind kind = component.kind();
    if (isRecoveryDevice(kind) &&
        takesMaterialOfAnotherType(preset, ComponentPreset::kMaterial, Material::Type::SURFACE))
    {
        return false;
    }
    if (kind == ComponentKind::PARACHUTE &&
        takesMaterialOfAnotherType(preset, ComponentPreset::kLineMaterial, Material::Type::LINE))
    {
        return false;
    }
    const auto* const rocket = dynamic_cast<const Rocket*>(&component.getRoot());
    return rocket == nullptr || !rocket->isFrozen();
}

}  // namespace

ComponentPresetSetter::ComponentPresetSetter(LoadFunction load) : m_load(std::move(load))
{
    QTROCKET_ASSERT(m_load != nullptr);
}

Result<void> ComponentPresetSetter::set(RocketComponent&  component, std::string_view /*value*/,
                                        const Attributes& attributes, WarningSet& warnings,
                                        const DocumentLoadingContext& context) const
{
    const std::optional<std::string_view> manufacturerName =
        DocumentConfig::attribute(attributes, "manufacturer");
    if (!manufacturerName.has_value())
    {
        warnings.add(
            std::format("Invalid ComponentPreset for component {}, no manufacturer "
                        "specified.  Ignored",
                        component.getName()));
        return {};
    }

    const std::optional<std::string_view> productNo =
        DocumentConfig::attribute(attributes, "partno");
    if (!productNo.has_value())
    {
        warnings.add(
            std::format("Invalid ComponentPreset for component {}, no partno specified.  Ignored",
                        component.getName()));
        return {};
    }

    const std::optional<std::string_view> digest = DocumentConfig::attribute(attributes, "digest");
    if (!digest.has_value())
    {
        warnings.add(std::format("Invalid ComponentPreset for component {}, no digest specified.",
                                 component.getName()));
    }

    if (!DocumentConfig::attribute(attributes, "type").has_value())
    {
        warnings.add(std::format("Invalid ComponentPreset for component {}, no type specified.",
                                 component.getName()));
    }

    PresetPtr matchingPreset =
        findPreset(context.getComponentPresetDatabase(), *manufacturerName, *productNo, digest);

    // Was any found?
    if (matchingPreset == nullptr)
    {
        warnings.add(
            std::format("No matching ComponentPreset for component {} found matching {} {}",
                        component.getName(), *manufacturerName, *productNo));
        return {};
    }

    // Java loads whatever it found; see the class comment for where that is its end.
    if (!canLoad(component, *matchingPreset))
    {
        warnings.add(Warning::kFileInvalidParameter);
        return {};
    }

    // The preset loader can override the component name, so first store it and then apply it
    // again
    const std::string componentName = component.getName();
    m_load(component, std::move(matchingPreset));
    component.setName(componentName);
    return {};
}

}  // namespace QtRocket
