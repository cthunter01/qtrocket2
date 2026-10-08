#include "QtRocket/file/openrocket/DatatypeHandler.h"

#include <format>
#include <memory>
#include <string_view>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/openrocket/CustomExpressionHandler.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

DatatypeHandler::DatatypeHandler(const DocumentLoadingContext& context)
  : m_context(&context), m_document(context.getOpenRocketDocument())
{
    if (m_document == nullptr)
    {
        bug("The loading context of a datatype handler has no document");
    }
}

Result<ElementHandler*> DatatypeHandler::openElement(std::string_view  element,
                                                     const Attributes& attributes,
                                                     WarningSet&       warnings)
{
    // A <type> without source is a NullPointerException in Java; here it is an unknown datatype.
    if (element == "type" && DocumentConfig::attribute(attributes, "source") == "customexpression")
    {
        m_customExpressionHandler = std::make_unique<CustomExpressionHandler>(*m_context);
        return m_customExpressionHandler.get();
    }
    warnings.add(
        Warning::fromString(std::format("Unknown datatype {} defined, ignoring", element)));

    return this;
}

Result<void> DatatypeHandler::closeElement(std::string_view element, const Attributes& attributes,
                                           std::string_view content, WarningSet& warnings)
{
    // Java removes source from the map it was given, which is const here.
    Attributes others = attributes;
    others.erase("source");
    Result<void> closed = AbstractElementHandler::closeElement(element, others, content, warnings);

    // The element of a custom expression handles everything in it itself, so the first element
    // to close here after it opened is that element: its expression is complete. (Java adds
    // the expression of the last such element at every close from then on.)
    if (m_customExpressionHandler != nullptr)
    {
        m_document->addCustomExpression(m_customExpressionHandler->getExpression());
        m_customExpressionHandler.reset();
    }
    return closed;
}

}  // namespace QtRocket
