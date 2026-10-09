#pragma once

#include <memory>
#include <optional>
#include <string_view>

#include "QtRocket/file/openrocket/OpenRocketContentHandler.h"
#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DocumentLoadingContext;

/// The starting point of the handlers of a design file: it accepts the one <openrocket> element
/// of a document and hands its content to an OpenRocketContentHandler (OpenRocket's
/// file/openrocket/importt/OpenRocketHandler). It is the handler OpenRocketLoader gives
/// SimpleSax.
///
/// openElement():
/// - an element of another name: "Unknown element <name>, ignoring." and the element is
///   ignored with everything in it, so a document whose root is something else loads as the
///   empty rocket, with that warning;
/// - a second <openrocket>: "Multiple document elements found, ignoring later ones.". No
///   document can bring this about (an XML document has one root, and an <openrocket> inside
///   the first one goes to the content handler); the text is there for a caller that uses the
///   handler otherwise, as in Java;
/// - the attribute version, when it is not one of DocumentConfig::kSupportedVersions:
///   "Unsupported document version", then " <version>" when the attribute is there, then
///   " (written using '<creator>')" when the attribute creator is there and not blank, the
///   creator trimmed as String.trim() trims, then ", attempting to read file anyway.". The
///   file is read all the same;
/// - the file version of the context is set from the attribute version (see parseVersion()),
///   which fails the load for a number an int cannot hold, after the warning above.
///
/// closeElement(), when the root element closes: AbstractElementHandler's warnings for text in
/// the element ("Unknown text in element 'openrocket', ignoring.") and for attributes other
/// than version and creator ("Unknown attributes in element 'openrocket', ignoring."). As
/// everywhere in the loader, the text and the attributes are those of the layer SimpleSax's
/// DelegatorHandler pops, which after an ignored element inside <openrocket> (an unknown
/// element, a second <rocket>) is the ignored element's: its attributes are then judged in
/// the place of the root's own, and of the text only what follows it counts (OpenRocket's
/// bookkeeping slip, kept on purpose).
///
/// Deviations from OpenRocket:
/// - The constructor takes the context by reference to non-const, because the handler sets the
///   file version in it; the handlers below read the same object.
/// - Java removes version and creator from the attribute map, when the element opens and again
///   when it closes; the attributes being const here, the closing warning is decided on a copy
///   without them. Nothing differs by that: at the opening the map is the root's own and the
///   content handler does not read it, and at the closing nobody is handed the map afterwards.
/// - getDocument() is not ported: nobody calls it (Java's loader takes the document from the
///   context too), and it throws a NullPointerException there before the element is opened.
/// - parseVersion() is public and static, for the tests.
/// - A version is a number when it is ASCII digits, a full stop and ASCII digits; Java's
///   pattern [0-9]+ says the same.
class OpenRocketHandler final : public AbstractElementHandler
{
public:
    /// The handler of a document that is read into the document of @p context, whose file
    /// version it sets. The context and what it points at must outlive the handler; what the
    /// context must hold is what OpenRocketContentHandler asks of it.
    explicit OpenRocketHandler(DocumentLoadingContext& context) noexcept;
    /// A temporary context would dangle.
    explicit OpenRocketHandler(DocumentLoadingContext&& context) = delete;

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

    /// The file version the attribute version of the root element stands for (parseVersion()):
    /// major * DocumentConfig::kFileVersionDivisor + minor when @p docVersion is "<digits>.
    /// <digits>" and nothing else, so "1.10" and "1.010" are 110 and "01.5" is 105, and 0
    /// when it is anything else or missing. As in Java the arithmetic is that of an int, which
    /// wraps ("21474837.0" is -2147483596), and a major or minor number beyond an int fails
    /// with ErrorCode::INVALID_ARGUMENT and the message of Java's NumberFormatException (`For
    /// input string: "99999999999"`), which fails the load.
    [[nodiscard]] static Result<int> parseVersion(std::optional<std::string_view> docVersion);

private:
    DocumentLoadingContext* m_context;
    /// The handler of the content of the root element; null until that element opened.
    std::unique_ptr<OpenRocketContentHandler> m_handler;
};

}  // namespace QtRocket
