#include "QtRocket/file/simplesax/DelegatorHandler.h"

#include <expected>
#include <string>
#include <string_view>
#include <utility>

#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

DelegatorHandler::DelegatorHandler(ElementHandler& initialHandler, WarningSet& warnings)
  : m_warnings(&warnings), m_handlerStack{&initialHandler}
{
    m_elementData.emplace_back();  // Just in case
}

Result<void> DelegatorHandler::startElement(std::string_view           localName,
                                            ElementHandler::Attributes attributes)
{
    // Check for ignore
    if (m_ignore > 0)
    {
        m_ignore++;
        return {};
    }

    // Add layer to data stacks
    m_elementData.emplace_back();
    m_elementAttributes.push_back(std::move(attributes));

    // Call the handler
    Result<ElementHandler*> handler =
        m_handlerStack.back()->openElement(localName, m_elementAttributes.back(), *m_warnings);
    if (!handler)
    {
        return std::unexpected(std::move(handler.error()));
    }
    if (*handler != nullptr)
    {
        m_handlerStack.push_back(*handler);
    }
    else
    {
        // Start ignoring elements
        m_ignore++;
    }
    return {};
}

void DelegatorHandler::characters(std::string_view text)
{
    // Check for ignore
    if (m_ignore > 0)
    {
        return;
    }
    m_elementData.back() += text;
}

Result<void> DelegatorHandler::endElement(std::string_view localName)
{
    // Check for ignore
    if (m_ignore > 0)
    {
        m_ignore--;
        return {};
    }
    // Every element that was not ignored pushed a handler, and every element pushed attributes.
    QTROCKET_ASSERT(m_handlerStack.size() > 1 && !m_elementAttributes.empty());

    // Remove data from stack
    const std::string data = std::move(m_elementData.back());
    m_elementData.pop_back();
    const ElementHandler::Attributes attributes = std::move(m_elementAttributes.back());
    m_elementAttributes.pop_back();

    // Remove last handler and call the next one
    ElementHandler* const handler = m_handlerStack.back();
    m_handlerStack.pop_back();
    if (Result<void> ended = handler->endHandler(localName, attributes, data, *m_warnings); !ended)
    {
        return ended;
    }
    return m_handlerStack.back()->closeElement(localName, attributes, data, *m_warnings);
}

}  // namespace QtRocket
