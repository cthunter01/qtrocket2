#pragma once

#include <cstddef>
#include <span>
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

    /// Reads the document @p data, the bytes of a file, as the JDK's parser reads an
    /// InputStream: it finds the encoding itself, from the first bytes and from the XML
    /// declaration, and then reads the text as readXml(text) does. This is how OpenRocket
    /// reads a design file (SimpleSAX.readXML() on an InputSource made of the stream).
    ///
    /// The encoding:
    /// - A UTF-8 byte order mark (EF BB BF) is skipped, and the document is UTF-8 unless its
    ///   XML declaration names another encoding, which then holds for everything after the
    ///   declaration, whatever the first bytes said.
    /// - "UTF-8" (the names are compared ignoring the case of ASCII letters) is read strictly:
    ///   a byte sequence that is not UTF-8 fails with ErrorCode::PARSE and the JDK's message
    ///   ("Invalid byte 2 of 3-byte UTF-8 sequence.", "Expected byte 2 of 2-byte UTF-8
    ///   sequence." at the end of the data, "High surrogate bits in UTF-8 sequence must not
    ///   exceed 0x10 but found 0x11."). The handlers see what the JDK's parser reports before
    ///   it asks for the characters that cannot be decoded: every element up to the sequence,
    ///   and nothing at all when fewer than five characters precede it (see
    ///   XmlScanner::Report::eventsBeforeEnd). An error in the XML before the sequence comes
    ///   first, as does a failure of a handler.
    /// - "UTF8" and "unicode-1-1-utf-8", Java's own names, are read by Java's lenient decoder:
    ///   a malformed sequence is U+FFFD, as in readXml(text).
    /// - ISO-8859-1 ("ISO-8859-1", "ISO_8859-1", "ISO_8859_1", "ISO8859_1", "ISO8859-1",
    ///   "ISO-IR-100", "latin1", "l1", "IBM819", "IBM-819", "CP819", "csISOLatin1"): every byte
    ///   is the character of its value.
    /// - US-ASCII ("US-ASCII", "ASCII", "ISO646-US", "ANSI_X3.4-1968", "ANSI_X3.4-1986", "US",
    ///   "IBM367", "IBM-367", "CP367", "csASCII", "ISO-IR-6") is read strictly: a byte above
    ///   127 fails with ErrorCode::PARSE and `Byte "233" is not a member of the (7-bit) ASCII
    ///   character set.`, before any handler is called.
    /// - windows-1252 ("windows-1252", "cp1252", "ibm-1252", "ibm1252", "cp5348"): as
    ///   ISO-8859-1 but for 0x80 to 0x9F, which are the euro sign, the typographic quotes and
    ///   so on; the five bytes it leaves undefined (81, 8D, 8F, 90, 9D) are U+FFFD.
    /// - An encoding name that is not a name (it must start with an ASCII letter and go on with
    ///   letters, digits, '.', '_' and '-') fails with ErrorCode::PARSE and `Invalid encoding
    ///   name "<name>".`.
    /// - Any other name fails with ErrorCode::IO and the name itself as the message, as it is
    ///   written: Java's UnsupportedEncodingException for an encoding it does not know
    ///   ("klingon"). A loader makes "I/O error: klingon" of it, and "Malformed XML in input."
    ///   of every ErrorCode::PARSE.
    ///
    /// Deviations from the JDK's parser:
    /// - The JDK reads every encoding Java has. A document in one that is not listed above
    ///   (ISO-8859-15, Shift_JIS, ...) fails here with the ErrorCode::IO of an unknown
    ///   encoding. So does a document that is UTF-16, UCS-4 or EBCDIC by its first bytes (a
    ///   byte order mark FE FF or FF FE, or "<?" in one of them), with the name "UTF-16BE",
    ///   "UTF-16LE", "ISO-10646-UCS-4" or "CP037"; one that is nothing but a UTF-16 byte order
    ///   mark is the JDK's "Premature end of file.".
    /// - An XML declaration that names UTF-16 or UTF-32 ("UTF-16", "UTF-16BE", "UTF-16LE",
    ///   "UTF-32", "UTF-32BE", "UTF-32LE") in a document that is not that by its first bytes
    ///   fails with ErrorCode::PARSE and "Content is not allowed in prolog." ("Premature end of
    ///   file." when nothing follows the declaration). This is what the JDK makes of a UTF-8
    ///   document that claims to be UTF-16, the common case: it reads the rest in the wide
    ///   encoding and finds no markup. The JDK does read a document whose bytes really change
    ///   to UTF-16 after an ASCII declaration; this does not. ("ISO-10646-UCS-2" and
    ///   "ISO-10646-UCS-4" there fail as in the JDK: `Given byte order for encoding "<name>" is
    ///   not supported.`)
    /// - For US-ASCII the JDK refuses the whole block of up to 8192 bytes a byte above 127 is
    ///   in, so its handlers see the elements before that block only (none in a short
    ///   document). Here no handler is called in a document with such a byte.
    [[nodiscard]] static Result<void> readXml(std::span<const std::byte> data,
                                              ElementHandler& initialHandler, WarningSet& warnings);
};

}  // namespace QtRocket
