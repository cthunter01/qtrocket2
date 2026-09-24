#pragma once

#include <string_view>

#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// A "simple SAX" XML reader (OpenRocket's SimpleSAX): the elements of a document go to element
/// handlers (see ElementHandler), starting with the one readXml() is given, through a
/// DelegatorHandler. An element may hold text or other elements, but is not expected to hold
/// both, which holds for the OpenRocket and RockSim formats and RockSim engine files.
///
/// OpenRocket parses with the JDK's namespace-aware SAX parser; readXml() checks the document
/// with XmlScanner, which decides as that parser does, builds it with pugixml and hands the
/// handlers the same SAX events: elements by local name, attributes by local name without the
/// namespace declarations, text with its references expanded and its line ends normalised. The
/// JDK's parser reports the elements as it reads them, so when the document is malformed the
/// handlers still see the elements before the error, and a failure of theirs there comes first.
class SimpleSax final
{
public:
    SimpleSax() = delete;

    /// Reads the document @p text (decoded, as UTF-8; a malformed byte reads as U+FFFD) and hands
    /// it to @p initialHandler, which, like @p warnings, receives the handlers' warnings
    /// (readXML()). Fails with the first failure of a handler, or with XmlScanner's for a document
    /// the JDK's parser rejects (ErrorCode::PARSE, with its message) or a document type
    /// declaration XmlScanner does not read (ErrorCode::UNSUPPORTED_FORMAT).
    [[nodiscard]] static Result<void> readXml(std::string_view text, ElementHandler& initialHandler,
                                              WarningSet& warnings);
};

}  // namespace QtRocket
