#pragma once

#include <functional>
#include <map>
#include <string>
#include <string_view>

#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// A "simple XML" element handler (OpenRocket's simplesax ElementHandler): an object that handles
/// one element of a document SimpleSax reads. For
///
///     <foo>
///       <bar>message</bar>
///     </foo>
///
/// read with the initial handler initHandler:
/// 1. initHandler.openElement() is called for <foo> and returns fooHandler;
/// 2. fooHandler.openElement() is called for <bar> and returns barHandler;
/// 3. barHandler.endHandler() is called for </bar>;
/// 4. fooHandler.closeElement() is called for </bar>, with the text "message";
/// 5. fooHandler.endHandler() is called for </foo>;
/// 6. initHandler.closeElement() is called for </foo>.
/// endHandler() is never called for the initial handler.
///
/// A failure a handler returns ends the reading (Java's SAXException), and SimpleSax::readXml()
/// returns it. Handlers own the handlers they return (or return long-lived ones, such as
/// PlainTextHandler::instance()); SimpleSax only borrows them.
class ElementHandler
{
public:
    /// An element's attributes by local name (Java's HashMap<String, String>).
    using Attributes = std::map<std::string, std::string, std::less<>>;

    ElementHandler(const ElementHandler&)            = delete;
    ElementHandler(ElementHandler&&)                 = delete;
    ElementHandler& operator=(const ElementHandler&) = delete;
    ElementHandler& operator=(ElementHandler&&)      = delete;
    virtual ~ElementHandler()                        = default;

    /// Called for the opening tag of a contained element @p element: the handler of the elements
    /// within it (possibly this one), nullptr to ignore the element and all of its contents, or a
    /// failure.
    [[nodiscard]] virtual Result<ElementHandler*> openElement(std::string_view  element,
                                                              const Attributes& attributes,
                                                              WarningSet&       warnings) = 0;

    /// Called for the closing tag of a contained element this handler did not ignore, with its
    /// attributes and its text content (@p content), which is what simple text elements are read
    /// from.
    [[nodiscard]] virtual Result<void> closeElement(std::string_view  element,
                                                    const Attributes& attributes,
                                                    std::string_view  content,
                                                    WarningSet&       warnings) = 0;

    /// Called when the element this handler handles closes.
    [[nodiscard]] virtual Result<void> endHandler(std::string_view  element,
                                                  const Attributes& attributes,
                                                  std::string_view  content,
                                                  WarningSet&       warnings) = 0;

protected:
    ElementHandler() = default;
};

}  // namespace QtRocket
