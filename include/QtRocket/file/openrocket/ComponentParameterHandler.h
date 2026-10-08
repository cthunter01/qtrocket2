#pragma once

#include <memory>
#include <string_view>

#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DocumentLoadingContext;
class RocketComponent;

/// Reads what the element of one component holds, the <rocket> element included: it sets the
/// component's parameters with the setters of DocumentConfig's table and hands the elements
/// that hold elements of their own to other handlers (OpenRocket's
/// file/openrocket/importt/ComponentParameterHandler). The component exists and is in the tree
/// already (ComponentHandler).
///
/// From the handler's construction until its element closes (endHandler()) the component
/// ignores what would clear its preset (RocketComponent::setIgnorePresetClearing()): a file
/// names the preset first and the component's values after it, and a value that differs from
/// the preset's would otherwise take the link to the preset away.
///
/// Elements are applied one by one as they close, in the order of the file, each to the
/// component as the elements before it left it. The state a load ends in depends on that
/// order (an automatic value is computed when its element is read, from what is there then),
/// and it is OpenRocket's.
///
/// A child element is handed on by its name:
/// - "subcomponents": a ComponentHandler for the components below this one;
/// - "appearance", and "insideappearance" or "inside-appearance" (the name of some development
///   versions): the appearance handlers (HOOK(R4): see below);
/// - "motormount": a MotorMountHandler, when the component can hold motors (a body tube or an
///   inner tube); else "Illegal component defined as motor mount.";
/// - "finpoints": a FinSetPointHandler, for a freeform fin set; else "Illegal component defined
///   for fin points.";
/// - "motorconfiguration": a MotorConfigurationHandler, for the rocket; else "Illegal component
///   defined for motor configuration.";
/// - "flightconfiguration": the same handler, for the rocket; else "Illegal component defined
///   for flight configuration.";
/// - "deploymentconfiguration": a DeploymentConfigurationHandler, for a parachute or a
///   streamer; else "Illegal component defined as recovery device.";
/// - "separationconfiguration": a StageSeparationConfigurationHandler, for a stage or a booster
///   set; else "Illegal component defined as stage.";
/// - any other element is plain text, a parameter.
/// An element of the wrong component is ignored with everything in it, after its warning.
///
/// When a child element closes: nothing more happens for the names above, with one exception,
/// "flightconfiguration", which is not in OpenRocket's list of them: it is then also looked up
/// as a parameter and gives "Unknown parameter type 'flightconfiguration' for Rocket,
/// ignoring.", after its configuration was made. Every other element is a parameter: the
/// setter DocumentConfig::findSetter() has for the component's class and the element's name is
/// applied to the element's text and attributes; an element no class of the component knows,
/// and one the component's class refuses, gives "Unknown parameter type '<element>' for
/// <component name>, ignoring.". A setter's failure ends the load (the id of a component that
/// is no UUID).
///
/// DelegatorHandler's slip is not corrected here (see DelegatorHandler): a parameter that holds
/// an element, such as `<length>0.5<x/></length>`, is set from the text behind the ignored
/// element, and the component's own element then closes with the parameter's text.
///
/// HOOK(R4): until part R4 of run 9b adds AppearanceHandler and InsideAppearanceHandler, the
/// three appearance elements are passed over with everything in them, without a warning. Part
/// R4 replaces the two marked lines of openElement() and deletes the stand-in.
///
/// Deviation from OpenRocket: Java walks the superclasses of the component's class and looks
/// "Class:element" up in the setter table at each; that walk is DocumentConfig::findSetter().
class ComponentParameterHandler final : public AbstractElementHandler
{
public:
    /// The handler of the element of @p component, which ignores preset clearing from now
    /// until endHandler(). Both @p component and @p context must outlive the handler.
    ComponentParameterHandler(RocketComponent&              component,
                              const DocumentLoadingContext& context) noexcept;
    /// A temporary context would dangle.
    ComponentParameterHandler(RocketComponent&               component,
                              const DocumentLoadingContext&& context) = delete;

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

    /// Restores the preset clearing of the component.
    [[nodiscard]] Result<void> endHandler(std::string_view element, const Attributes& attributes,
                                          std::string_view content, WarningSet& warnings) override;

private:
    /// A new Handler for the component as a Component, kept as the handler of the child
    /// element being read; when the component is no Component, the warning @p illegal and
    /// null, which ignores the element.
    template <class Handler, class Component>
    [[nodiscard]] ElementHandler* handlerFor(std::string_view illegal, WarningSet& warnings);

    RocketComponent*              m_component;
    const DocumentLoadingContext* m_context;
    /// The handler of the child element read last; the next one replaces it.
    std::unique_ptr<ElementHandler> m_childHandler;
};

}  // namespace QtRocket
