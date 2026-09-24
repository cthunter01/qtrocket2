#pragma once

#include <cstddef>
#include <optional>
#include <string_view>

#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// Decides whether a document is well-formed as OpenRocket's XML parser decides it: the JDK's
/// namespace-aware SAX parser (JDK 17's Xerces, with its default JAXP limits) reading the
/// document from a character stream, the way SimpleSAX reads .rse and .ork files. pugixml, which
/// builds QtRocket's documents, accepts much that Xerces rejects (a bare '&', a '<' in an
/// attribute value, control characters, "--" in a comment, unbound namespace prefixes, ...), so
/// a reader checks a document with scan() first and hands it to pugixml only when it passes.
///
/// scan() walks the document as Xerces's scanner does and stops at the first fatal error, with
/// Xerces's message (SAXParseException.getMessage(): "The reference to entity \"T\" must end with
/// the ';' delimiter.", "Content is not allowed in prolog.", ...). It covers the XML declaration
/// (version, encoding and standalone, in that order; version "1.0" or "1.1", and a 1.1 document
/// gets 1.1's characters, names and line ends), the prolog and trailing section, elements and
/// attributes (duplicates, "<" and invalid characters in values, the Name and QName productions
/// with Xerces's name tables), namespaces (unbound prefixes, the reserved "xml" and "xmlns"
/// bindings, empty prefixed bindings in 1.0, duplicates by expanded name), character and entity
/// references (the five predefined entities), comments, processing instructions, CDATA sections,
/// the Char production, and the JDK's limits of 1000 characters per name and 10000 attributes
/// per element.
///
/// Deviations: document type declarations are read only in part. One without an external subset
/// whose internal subset holds only white space, comments and processing instructions is checked
/// as Xerces checks it. One that names an external subset (SYSTEM or PUBLIC), or whose internal
/// subset declares anything (ELEMENT, ATTLIST, ENTITY, NOTATION) or refers to a parameter entity,
/// ends the scan with ErrorCode::UNSUPPORTED_FORMAT: Xerces would read the external subset and
/// apply the declarations (entity expansion, attribute defaults), which pugixml cannot follow.
/// Two messages can differ, never the decision: for a document that ends right after the colon
/// of a name, Xerces checks a stale character of its buffer and may report the end of the
/// document instead; and XML 1.1's complaint about a name that starts with a colon quotes the
/// name Xerces read last, which with OpenRocket's reused parsers can come from an earlier
/// document, where this names the last one of this document ("null" before any).
class XmlScanner final
{
public:
    /// What scan() found.
    struct Report
    {
        /// The first fatal error (ErrorCode::PARSE, with the JDK's message) or the unsupported
        /// document type declaration (ErrorCode::UNSUPPORTED_FORMAT); empty when the document is
        /// well-formed.
        std::optional<Error> error;
        /// The SAX startElement() and endElement() calls the JDK's parser makes before it stops
        /// (all of them for a well-formed document; an empty element makes both): a handler has
        /// seen the document up to here when the error is reported.
        std::size_t elementEvents{0};
        /// Whether the XML declaration makes this an XML 1.1 document, whose line ends include
        /// U+0085 and U+2028 besides "\r\n" and "\r".
        bool xml11{false};
    };

    /// Scans @p text, the document decoded to UTF-8 (a malformed byte reads as U+FFFD).
    [[nodiscard]] static Report scan(std::string_view text);
};

}  // namespace QtRocket
