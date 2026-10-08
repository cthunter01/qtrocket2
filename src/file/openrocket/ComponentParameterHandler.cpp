#include "QtRocket/file/openrocket/ComponentParameterHandler.h"

#include <algorithm>
#include <array>
#include <format>
#include <memory>
#include <string>
#include <string_view>

#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/openrocket/ComponentHandler.h"
#include "QtRocket/file/openrocket/DeploymentConfigurationHandler.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/openrocket/FinSetPointHandler.h"
#include "QtRocket/file/openrocket/MotorConfigurationHandler.h"
#include "QtRocket/file/openrocket/MotorMountHandler.h"
#include "QtRocket/file/openrocket/Setter.h"
#include "QtRocket/file/openrocket/StageSeparationConfigurationHandler.h"
#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

namespace
{

/// The elements whose closing asks for nothing more: their handlers have done everything.
/// "flightconfiguration" is not among them, as in OpenRocket.
// TODO: delete 'inside-appearance' when backward compatibility with 22.02.beta.01-22.02.beta.05
// is not needed anymore
constexpr std::array<std::string_view, 9> kHandledElements{"subcomponents",
                                                           "motormount",
                                                           "finpoints",
                                                           "motorconfiguration",
                                                           "appearance",
                                                           "insideappearance",
                                                           "inside-appearance",
                                                           "deploymentconfiguration",
                                                           "separationconfiguration"};

[[nodiscard]] bool isHandledElement(std::string_view element) noexcept
{
    return std::ranges::find(kHandledElements, element) != kHandledElements.end();
}

/// HOOK(R4): what stands in for AppearanceHandler and InsideAppearanceHandler until part R4 of
/// run 9b adds them: it takes an appearance element with everything in it and does nothing.
/// Delete it with the two lines of openElement() that return it.
class PendingAppearanceHandler final : public ElementHandler
{
public:
    [[nodiscard]] static PendingAppearanceHandler& instance() noexcept
    {
        static PendingAppearanceHandler s_instance;
        return s_instance;
    }

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view /*element*/,
                                                      const Attributes& /*attributes*/,
                                                      WarningSet& /*warnings*/) override
    {
        return this;
    }
    [[nodiscard]] Result<void> closeElement(std::string_view /*element*/,
                                            const Attributes& /*attributes*/,
                                            std::string_view /*content*/,
                                            WarningSet& /*warnings*/) override
    {
        return {};
    }
    [[nodiscard]] Result<void> endHandler(std::string_view /*element*/,
                                          const Attributes& /*attributes*/,
                                          std::string_view /*content*/,
                                          WarningSet& /*warnings*/) override
    {
        return {};
    }

private:
    PendingAppearanceHandler() = default;
};

}  // namespace

ComponentParameterHandler::ComponentParameterHandler(RocketComponent&              component,
                                                     const DocumentLoadingContext& context) noexcept
  : m_component(&component), m_context(&context)
{
    // Sometimes setting certain component parameters will clear the preset. We don't want that
    // to happen, so ignore preset clearing.
    m_component->setIgnorePresetClearing(true);
}

template <class Handler, class Component>
ElementHandler* ComponentParameterHandler::handlerFor(std::string_view illegal,
                                                      WarningSet&      warnings)
{
    auto* const typed = dynamic_cast<Component*>(m_component);
    if (typed == nullptr)
    {
        warnings.add(Warning::fromString(std::string(illegal)));
        return nullptr;
    }
    m_childHandler = std::make_unique<Handler>(*typed, *m_context);
    return m_childHandler.get();
}

Result<ElementHandler*> ComponentParameterHandler::openElement(std::string_view element,
                                                               const Attributes& /*attributes*/,
                                                               WarningSet& warnings)
{
    // Check for specific elements that contain other elements
    if (element == "subcomponents")
    {
        m_childHandler = std::make_unique<ComponentHandler>(*m_component, *m_context);
        return m_childHandler.get();
    }
    if (element == "appearance")
    {
        // HOOK(R4): an AppearanceHandler for the component and the context.
        return &PendingAppearanceHandler::instance();
    }
    // TODO: delete 'inside-appearance' when backward compatibility with
    // 22.02.beta.01-22.02.beta.05 is not needed anymore
    if (element == "insideappearance" || element == "inside-appearance")
    {
        // HOOK(R4): an InsideAppearanceHandler for the component and the context.
        return &PendingAppearanceHandler::instance();
    }
    if (element == "motormount")
    {
        return handlerFor<MotorMountHandler, MotorMount>(
            "Illegal component defined as motor mount.", warnings);
    }
    if (element == "finpoints")
    {
        return handlerFor<FinSetPointHandler, FreeformFinSet>(
            "Illegal component defined for fin points.", warnings);
    }
    if (element == "motorconfiguration")
    {
        return handlerFor<MotorConfigurationHandler, Rocket>(
            "Illegal component defined for motor configuration.", warnings);
    }
    if (element == "flightconfiguration")
    {
        return handlerFor<MotorConfigurationHandler, Rocket>(
            "Illegal component defined for flight configuration.", warnings);
    }
    if (element == "deploymentconfiguration")
    {
        return handlerFor<DeploymentConfigurationHandler, RecoveryDevice>(
            "Illegal component defined as recovery device.", warnings);
    }
    if (element == "separationconfiguration")
    {
        return handlerFor<StageSeparationConfigurationHandler, AxialStage>(
            "Illegal component defined as stage.", warnings);
    }

    return &PlainTextHandler::instance();
}

Result<void> ComponentParameterHandler::closeElement(std::string_view  element,
                                                     const Attributes& attributes,
                                                     std::string_view content, WarningSet& warnings)
{
    if (isHandledElement(element))
    {
        return {};
    }

    // Search for the correct setter class
    const DocumentConfig::SetterLookup found =
        DocumentConfig::findSetter(m_component->kind(), element);
    if (found.setter == nullptr)
    {
        // No class of the component has an entry, or one refuses the element.
        warnings.add(
            Warning::fromString(std::format("Unknown parameter type '{}' for {}, ignoring.",
                                            element, m_component->getComponentName())));
        return {};
    }
    return found.setter->set(*m_component, content, attributes, warnings, *m_context);
}

Result<void> ComponentParameterHandler::endHandler(std::string_view  element,
                                                   const Attributes& attributes,
                                                   std::string_view content, WarningSet& warnings)
{
    Result<void> ended = AbstractElementHandler::endHandler(element, attributes, content, warnings);

    // Restore the preset clearing behavior
    m_component->setIgnorePresetClearing(false);
    return ended;
}

}  // namespace QtRocket
