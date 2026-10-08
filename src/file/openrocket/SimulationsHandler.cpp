#include "QtRocket/file/openrocket/SimulationsHandler.h"

#include <memory>
#include <string>
#include <string_view>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/openrocket/SingleSimulationHandler.h"
#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/simulation/customexpression/CustomExpression.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

SimulationsHandler::SimulationsHandler(const DocumentLoadingContext& context) : m_context(&context)
{
    // What every simulation's handler needs: asked for here, so that a context that lacks it
    // shows with every file and not only with one that has a simulation.
    if (context.getOpenRocketDocument() == nullptr)
    {
        bug("the loading context of a simulations handler has no document");
    }
    if (context.getPreferences() == nullptr)
    {
        bug("the loading context of a simulations handler has no preference store");
    }
}

Result<ElementHandler*> SimulationsHandler::openElement(std::string_view element,
                                                        const Attributes& /*attributes*/,
                                                        WarningSet& warnings)
{
    if (element != "simulation")
    {
        warnings.add("Unknown element '" + std::string(element) + "', ignoring.");
        return nullptr;
    }

    m_handler = std::make_unique<SingleSimulationHandler>(*m_context);
    return m_handler.get();
}

Result<void> SimulationsHandler::closeElement(std::string_view  element,
                                              const Attributes& attributes,
                                              std::string_view content, WarningSet& warnings)
{
    // Java removes the key from the map it was given.
    Attributes others = attributes;
    others.erase("status");

    // Finished loading. Java rebuilds the custom expressions of the document here
    // (exp.setExpression(exp.getExpressionString()) for each), in case something has changed
    // such as listener variable come available. Building an expression reads the flight data
    // types of the document (CustomExpression.getAllSymbols()), which asks every expression
    // for its type and so registers it again: that part a later column name can tell, and it
    // is done here. One round leaves the registry as Java's several rounds leave it.
    // HOOK(custom-expression): the rebuilding itself, with the milestone that evaluates them.
    for (const CustomExpression& expression :
         m_context->getOpenRocketDocument()->getCustomExpressions())
    {
        static_cast<void>(expression.getType());
    }

    return AbstractElementHandler::closeElement(element, others, content, warnings);
}

}  // namespace QtRocket
