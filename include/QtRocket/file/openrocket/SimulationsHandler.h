#pragma once

#include <memory>
#include <string_view>

#include "QtRocket/file/openrocket/SingleSimulationHandler.h"
#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DocumentLoadingContext;

/// Reads the <simulations> element of a design file: the simulations of the document, each a
/// <simulation> child read by a SingleSimulationHandler of its own, which adds it to the
/// document when it ends (OpenRocket's file/openrocket/importt/SimulationsHandler). The
/// simulations are added in the order of the file.
///
/// - A child that is no <simulation> is ignored with "Unknown element '<name>', ignoring.",
///   which shifts the attributes of the elements around it by one (DelegatorHandler): the
///   <simulations> element then closes with the attributes of that child, and its parent warns
///   of them.
/// - When a <simulation> closes, the status attribute is the one attribute it may have: any
///   other gives "Unknown attributes in element 'simulation', ignoring.", and text in the
///   element (between its children) gives "Unknown text in element 'simulation', ignoring."
///   (AbstractElementHandler::closeElement()). After an ignored element somewhere inside the
///   <simulation>, the attributes it closes with are another element's (see
///   SingleSimulationHandler), and that warning tells of them.
/// - A failure of a simulation's handler ends the load (see SingleSimulationHandler); the
///   simulations before it are in the document then.
///
/// Deviations from OpenRocket:
/// - When a <simulation> closes, Java sets the expression of every custom expression of the
///   document anew, "in case something has changed such as listener variable come available".
///   Custom expressions are not evaluated here, so nothing is done ("HOOK(custom-expression)"
///   in the source file).
/// - Java removes the status attribute from the map it was given; here a copy of the
///   attributes without it is what the warnings are decided on, and the handler's caller keeps
///   its attributes.
/// - The constructor takes the loading context only (Java: the document and the context; the
///   document is the context's). Without a document or a preference store in the context it
///   is a BugError, whether or not the file has a simulation.
class SimulationsHandler final : public AbstractElementHandler
{
public:
    /// A handler of the <simulations> element of the document of @p context. The context and
    /// what it points at must outlive the handler; its preference store must outlive the
    /// simulations (see DocumentLoadingContext::getPreferences()).
    /// @throws BugError when @p context has no document or no preference store
    explicit SimulationsHandler(const DocumentLoadingContext& context);

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

    /// The warnings for what a <simulation> must not have; never fails.
    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

private:
    const DocumentLoadingContext* m_context;
    /// The handler of the <simulation> element opened last.
    std::unique_ptr<SingleSimulationHandler> m_handler;
};

}  // namespace QtRocket
