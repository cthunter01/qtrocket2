#pragma once

#include <string_view>

#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/simulation/customexpression/CustomExpression.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DocumentLoadingContext;

/// Reads a <type source="customexpression"> element of a design file's <datatypes>: one custom
/// expression, a flight data type the user defined by a formula (OpenRocket's
/// file/openrocket/importt/CustomExpressionHandler). The expression starts as four empty texts
/// and is carried opaquely (see CustomExpression): nothing is parsed or checked.
///
/// The handler handles every element below its own, however deep, and when one closes its
/// text, which is not trimmed, is taken by its name:
/// - <name>: the name of the expression;
/// - <symbol>: its symbol;
/// - <unit unittype="auto">: its unit; a <unit> of another unittype is passed over;
/// - <expression>: the formula.
/// A later element of a name replaces what an earlier one set. Nothing is warned of: any other
/// element, text and attribute is passed over without a word. DatatypeHandler takes the
/// expression (getExpression()) when the <type> element closes and adds it to the document.
///
/// HOOK(custom-expression): the milestone that evaluates custom expressions checks them here.
///
/// Deviations from OpenRocket:
/// - A <unit> without the attribute unittype is passed over with
///   Warning::kFileInvalidParameter (OpenRocket: a NullPointerException).
/// - A <type> element inside the element does nothing. In OpenRocket it adds the expression
///   to the document as it is at that moment, the same object that is added again when the
///   enclosing <type> closes, so the document then lists it twice: by decision a file gives
///   one expression per <type source="customexpression"> element.
/// - The expression has no document (see CustomExpression), so making it does not register
///   the flight data types of the document's other expressions, and no symbol of theirs can
///   make it fail (in OpenRocket a symbol that is no regular expression, such as "(", does).
///   The constructor takes the loading context only (Java: the content handler and the
///   context) and does not need it; neither does Java's.
class CustomExpressionHandler final : public AbstractElementHandler
{
public:
    /// The handler of one <type source="customexpression"> element.
    explicit CustomExpressionHandler(const DocumentLoadingContext& context) noexcept;

    /// Itself, for every element.
    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

    /// The expression as read so far (Java: the public field currentExpression).
    [[nodiscard]] const CustomExpression& getExpression() const noexcept { return m_expression; }

private:
    CustomExpression m_expression;
};

}  // namespace QtRocket
