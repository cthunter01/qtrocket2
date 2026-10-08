#include "QtRocket/file/openrocket/CustomExpressionHandler.h"

#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

CustomExpressionHandler::CustomExpressionHandler(const DocumentLoadingContext& /*context*/) noexcept
{
}

Result<ElementHandler*> CustomExpressionHandler::openElement(std::string_view /*element*/,
                                                             const Attributes& /*attributes*/,
                                                             WarningSet& /*warnings*/)
{
    return this;
}

Result<void> CustomExpressionHandler::closeElement(std::string_view  element,
                                                   const Attributes& attributes,
                                                   std::string_view content, WarningSet& warnings)
{
    // Java adds the expression to the document for a child named "type"; here the document
    // gets one expression per element, from DatatypeHandler.

    if (element == "name")
    {
        m_expression.setName(std::string(content));
    }

    if (element == "symbol")
    {
        m_expression.setSymbol(std::string(content));
    }

    if (element == "unit")
    {
        const std::optional<std::string_view> unitType =
            DocumentConfig::attribute(attributes, "unittype");
        if (!unitType.has_value())
        {
            // Java: a NullPointerException.
            warnings.add(Warning::kFileInvalidParameter);
        }
        else if (*unitType == "auto")
        {
            m_expression.setUnit(std::string(content));
        }
    }

    if (element == "expression")
    {
        // HOOK(custom-expression): the formula is kept as text; nothing parses it.
        m_expression.setExpression(std::string(content));
    }
    return {};
}

}  // namespace QtRocket
