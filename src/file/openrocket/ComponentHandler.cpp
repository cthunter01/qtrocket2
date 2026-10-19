#include "QtRocket/file/openrocket/ComponentHandler.h"

#include <format>
#include <memory>
#include <string_view>
#include <utility>

#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/openrocket/ComponentParameterHandler.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/openrocket/LoaderWarnings.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialPreferences.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/ComponentFactory.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

namespace
{

/// How many components stand above @p component: 0 for the rocket, 1 for a stage.
[[nodiscard]] int depthOf(const RocketComponent& component) noexcept
{
    int depth = 0;
    for (const RocketComponent* parent = component.getParent(); parent != nullptr;
         parent                        = parent->getParent())
    {
        ++depth;
    }
    return depth;
}

/// Whether @p materials holds the three built-in materials the preferences fall back on for a
/// class they name no default for. OpenRocket's databases always hold them;
/// getDefaultComponentMaterial() has a BugError for a storage without one.
[[nodiscard]] bool holdsTheFallbackMaterials(const MaterialStorage& materials)
{
    return materials.findMaterial(Material::Type::BULK, kDefaultBulkMaterialName).has_value() &&
           materials.findMaterial(Material::Type::SURFACE, kDefaultSurfaceMaterialName)
               .has_value() &&
           materials.findMaterial(Material::Type::LINE, kDefaultLineMaterialName).has_value();
}

/// Completes Java's constructor of the new @p component, which is not in a tree yet, so that
/// nothing is fired: the materials OpenRocket's constructors ask the application's
/// preferences for (ComponentFactory::applyDefaultMaterials(); see the class comment). Nothing
/// is done without preferences or without application materials that hold the built-in
/// fallbacks.
void applyDefaultMaterials(RocketComponent& component, const DocumentLoadingContext& context)
{
    const Preferences* const     preferences = context.getPreferences();
    const MaterialStorage* const materials   = context.getApplicationMaterials();
    if (preferences == nullptr || materials == nullptr || !holdsTheFallbackMaterials(*materials))
    {
        return;
    }
    ComponentFactory::applyDefaultMaterials(component, *preferences, *materials);
}

}  // namespace

ComponentHandler::ComponentHandler(RocketComponent&              parent,
                                   const DocumentLoadingContext& context) noexcept
  : m_parent(&parent), m_context(&context)
{
}

Result<ElementHandler*> ComponentHandler::openElement(std::string_view element,
                                                      const Attributes& /*attributes*/,
                                                      WarningSet& warnings)
{
    // Attempt to construct new component
    std::unique_ptr<RocketComponent> component = DocumentConfig::createComponent(element);
    if (component == nullptr)
    {
        warnings.add(Warning::fromString(std::format("Unknown element {}, ignoring.", element)));
        return nullptr;
    }

    // Malformed and legacy files may contain component hierarchies that the current model
    // cannot represent. Ignore the complete subtree instead of aborting the document load in
    // RocketComponent::addChild().
    if (!m_parent->isCompatible(*component))
    {
        warnings.add(Warning::fromString(
            std::format("{} cannot be attached to {}; ignoring this component and its "
                        "subcomponents.",
                        component->getComponentName(), m_parent->getComponentName())));
        return nullptr;
    }

    // Not OpenRocket's: see the class comment.
    if (depthOf(*m_parent) >= kMaxDepth)
    {
        warnings.add(LoaderWarnings::nestedTooDeeply(component->getComponentName()));
        return nullptr;
    }

    // Not OpenRocket's either: the stages, booster sets and pod sets of a rocket are bounded
    // (see the class comment), ...
    if (!DocumentConfig::assemblyFits(*m_parent, *component))
    {
        warnings.add(LoaderWarnings::tooManyAssemblies(component->getComponentName()));
        return nullptr;
    }

    // ... and so are its components.
    if (!DocumentConfig::componentFits(*m_parent, *component))
    {
        warnings.add(LoaderWarnings::tooManyComponents(component->getComponentName()));
        return nullptr;
    }

    // Nor this: the instances of the whole rocket are bounded (see the class comment). The
    // parent stands in the tree, so its instances are known.
    if (!DocumentConfig::childFits(*m_parent, *component))
    {
        warnings.add(LoaderWarnings::tooManyInstances(component->getComponentName()));
        return nullptr;
    }

    // Java's constructor has asked the application's preferences for the component's
    // materials. Here whoever makes a component completes it, before it is attached.
    applyDefaultMaterials(*component, *m_context);

    RocketComponent& added = m_parent->addChild(std::move(component));

    m_componentHandler = std::make_unique<ComponentParameterHandler>(added, *m_context);
    return m_componentHandler.get();
}

}  // namespace QtRocket
