#include "QtRocket/file/simplesax/DelegatorHandler.h"

#include <cstddef>
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

namespace
{

/// The unreachable layers that are left lying before any are dropped, beyond as many as there
/// are reachable ones.
constexpr std::size_t kSpareLayers = 16;

}  // namespace

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
    dropUnreachableLayers();
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

void DelegatorHandler::dropUnreachableLayers()
{
    // Every element that ends takes one layer off each stack, and only an element with a
    // handler does: so the layers that can still be read are the top ones, a text buffer for
    // each handler of the stack (the one at the bottom takes the text outside the elements)
    // and an attribute map for each but the first. What an ignored element left below them
    // stays unread for good. It is dropped when it has become as much as what is kept, so
    // that the dropping costs no more than the pushing did.
    const std::size_t texts = m_handlerStack.size();
    if (m_elementData.size() > (2 * texts) + kSpareLayers)
    {
        m_elementData.erase(m_elementData.begin(),
                            m_elementData.end() - static_cast<std::ptrdiff_t>(texts));
    }
    const std::size_t maps = m_handlerStack.size() - 1;
    if (m_elementAttributes.size() > (2 * maps) + kSpareLayers)
    {
        m_elementAttributes.erase(m_elementAttributes.begin(),
                                  m_elementAttributes.end() - static_cast<std::ptrdiff_t>(maps));
    }
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
