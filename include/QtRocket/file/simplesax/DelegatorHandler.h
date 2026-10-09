#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// The SAX handler behind SimpleSax (OpenRocket's DelegatorHandler): it keeps a stack of element
/// handlers, text buffers and attribute maps and hands each SAX event to the handler in charge.
///
/// Kept exactly, including a bookkeeping slip of OpenRocket's: an element whose handler ignores
/// it pushes a text buffer and its attributes but never pops them, so the element that encloses
/// it is later closed with the text that follows the ignored element and with the ignored
/// element's attributes ("<comments>a<b/>c</comments>" closes <comments> with the text "c").
///
/// Deviation from OpenRocket, which nothing a handler is given can show: the layers that the
/// slip leaves behind are dropped once they can no longer be reached. An element takes one
/// layer off each stack when it ends, so of the layers that lie there only as many as there are
/// open elements with a handler can ever be read again, the top ones; OpenRocket keeps the
/// others to the end of the document, one text buffer and one attribute map for every element
/// that was ignored, which a document of nothing but unknown elements makes a great many
/// (retainedLayers()).
class DelegatorHandler final
{
public:
    /// A handler that starts with @p initialHandler; both it and @p warnings must outlive it.
    DelegatorHandler(ElementHandler& initialHandler, WarningSet& warnings);

    /// SAX startElement() for the element @p localName: the handler in charge opens it, and its
    /// failure is returned.
    [[nodiscard]] Result<void> startElement(std::string_view           localName,
                                            ElementHandler::Attributes attributes);

    /// SAX characters(): @p text joins the content of the element being read.
    void characters(std::string_view text);

    /// SAX endElement() for the element @p localName: its handler's endHandler(), then the
    /// enclosing handler's closeElement(), and the first failure is returned.
    [[nodiscard]] Result<void> endElement(std::string_view localName);

    /// The number of text buffers that are kept: one for every open element that has a
    /// handler and one more, and those that ignored elements left behind and that were not
    /// dropped yet (never more than about as many again).
    [[nodiscard]] std::size_t retainedLayers() const noexcept { return m_elementData.size(); }

private:
    /// Drops the layers of the two stacks that no element can take off any more (see the
    /// class comment), when they have become more than the layers that can.
    void dropUnreachableLayers();

    WarningSet*                             m_warnings;
    std::vector<ElementHandler*>            m_handlerStack;
    std::vector<std::string>                m_elementData;
    std::vector<ElementHandler::Attributes> m_elementAttributes;
    /// Ignore all elements as long as this is above 0.
    int m_ignore{0};
};

}  // namespace QtRocket
