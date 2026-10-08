#pragma once

#include <memory>
#include <string_view>

#include "QtRocket/file/openrocket/CustomExpressionHandler.h"
#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DocumentLoadingContext;
class OpenRocketDocument;

/// Reads the <datatypes> element of a design file: the flight data types the document defines
/// beside the built-in ones, which are its custom expressions (OpenRocket's
/// file/openrocket/importt/DatatypeHandler).
///
/// A <type source="customexpression"> child is read by a CustomExpressionHandler, and when it
/// closes its expression is added to the document (OpenRocketDocument::addCustomExpression()),
/// behind the ones already there and also when it equals one of them. The element stands
/// before <simulations> in a file, so the stored columns of the simulations find their types
/// among the document's expressions.
///
/// Any other child, a <type> of another source included, gives "Unknown datatype <name>
/// defined, ignoring" (without a full stop) and is handled by this handler itself, so the same
/// holds for every element in it, at any depth: a <type source="customexpression"> in an
/// unknown element is read as one. When a child closes, AbstractElementHandler's warnings are
/// given for its text and for its attributes other than source.
///
/// Deviations from OpenRocket:
/// - One expression per <type source="customexpression"> element, added when that element
///   closes (a decision of the port). OpenRocket adds the expression of the custom expression
///   element read last whenever ANY element closes on this handler from then on, so every
///   unknown child that follows one, and every element in such a child, stores it once more
///   (the same object: a document with one expression and three other elements lists it four
///   times, and writes it four times when saved). A file as OpenRocket writes it has nothing
///   but custom expression elements and loads alike in both.
/// - A <type> without the attribute source is an unknown datatype, with that warning
///   (OpenRocket: a NullPointerException).
/// - Java removes source from the attribute map before it warns of the others; the attributes
///   being const here, the warning is decided on a copy without it.
/// - The types of the expressions are not registered with FlightDataType while the file loads
///   (see CustomExpressionHandler); CustomExpression::getType() registers one when it is asked.
/// - The constructor takes the loading context only (Java: the content handler, which it asks
///   for the document, and the context; the document is the context's). Without a document in
///   the context it is a BugError.
class DatatypeHandler final : public AbstractElementHandler
{
public:
    /// The handler of the <datatypes> element of the document of @p context. The context and
    /// its document must outlive the handler.
    /// @throws BugError when @p context has no document
    explicit DatatypeHandler(const DocumentLoadingContext& context);
    /// A temporary context would dangle.
    explicit DatatypeHandler(const DocumentLoadingContext&& context) = delete;

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

private:
    const DocumentLoadingContext* m_context;
    OpenRocketDocument*           m_document;
    /// The handler of the custom expression element that is open: it was returned by
    /// openElement() and its element has not closed yet. Null at any other time.
    std::unique_ptr<CustomExpressionHandler> m_customExpressionHandler;
};

}  // namespace QtRocket
