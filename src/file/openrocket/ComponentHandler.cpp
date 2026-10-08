#include "QtRocket/file/openrocket/ComponentHandler.h"

#include <format>
#include <memory>
#include <string_view>
#include <utility>

#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/openrocket/ComponentParameterHandler.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
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
        warnings.add(Warning::fromString(
            std::format("{} is nested too deeply; ignoring this component and its subcomponents.",
                        component->getComponentName())));
        return nullptr;
    }

    RocketComponent& added = m_parent->addChild(std::move(component));

    m_componentHandler = std::make_unique<ComponentParameterHandler>(added, *m_context);
    return m_componentHandler.get();
}

}  // namespace QtRocket
