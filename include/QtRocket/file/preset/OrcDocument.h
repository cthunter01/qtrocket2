#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <string_view>

#include "QtRocket/file/preset/OrcElement.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

struct OrcNodes;

/// An .orc component preset file read as OpenRocket's OpenRocketComponentLoader hands it to
/// JAXB's unmarshaller: the XML of the file, checked by XmlScanner as the JDK's parser checks
/// it, with its elements as JAXB sees them (OrcElement). It knows nothing of the DTOs: which
/// root element is an .orc file and what the elements mean is OpenRocketComponentLoader's.
///
/// OpenRocket reads the file through an InputStreamReader for UTF-8 (parse(bytes)): the encoding
/// the XML declaration names is ignored, a malformed byte sequence becomes U+FFFD ([81] of the
/// probe tools/openrocket-probes/orc-loader), and a byte order mark is a character U+FEFF before
/// the XML declaration, which the parser refuses ("Content is not allowed in prolog.": [78]);
/// so is a UTF-16 document ([82]).
///
/// Deviations: a document type declaration that declares anything (an entity, say) is refused
/// with ErrorCode::UNSUPPORTED_FORMAT, as XmlScanner refuses it (Java expands the entity: [72]);
/// and a document of more than kMaxElements elements is refused (Java reads it).
class OrcDocument final
{
public:
    /// The most elements a document may have: sixty times as many as the largest bundled file
    /// (semroc.orc, 16618).
    static constexpr std::size_t kMaxElements = 1000000;

    /// Reads the bytes of an .orc file: decoded as UTF-8 by Java's lenient decoder
    /// (Strings::toValidUtf8()), then parseText(). Fails as parseText() does.
    [[nodiscard]] static Result<OrcDocument> parse(std::span<const std::byte> data,
                                                   std::size_t maxElements = kMaxElements);

    /// Reads @p text, an .orc document decoded to UTF-8 (a malformed sequence is U+FFFD). Fails
    /// with XmlScanner's error for a document the JDK's parser refuses (ErrorCode::PARSE with its
    /// message, "Premature end of file." for an empty one) or a document type declaration it
    /// does not read (ErrorCode::UNSUPPORTED_FORMAT), and with ErrorCode::UNSUPPORTED_FORMAT and
    /// "The document has more than <maxElements> elements." for a larger one (an application
    /// leaves @p maxElements alone; the tests of the bound lower it), which pugixml is then never
    /// given.
    [[nodiscard]] static Result<OrcDocument> parseText(std::string_view text,
                                                       std::size_t      maxElements = kMaxElements);

    OrcDocument(const OrcDocument&)            = delete;
    OrcDocument& operator=(const OrcDocument&) = delete;
    OrcDocument(OrcDocument&&) noexcept;
    OrcDocument& operator=(OrcDocument&&) noexcept;
    ~OrcDocument();

    /// The root element (any name: the loader decides whether it is an .orc file's). A
    /// document that was moved from has none: asking it is a BugError.
    [[nodiscard]] OrcElement root() const;

    /// The number of elements, the root included (none in a document that was moved from).
    [[nodiscard]] std::size_t elementCount() const noexcept;

private:
    explicit OrcDocument(std::unique_ptr<const OrcNodes> nodes) noexcept;

    std::unique_ptr<const OrcNodes> m_nodes;
};

}  // namespace QtRocket
