#include "QtRocket/file/simplesax/SimpleSax.h"

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <expected>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <pugixml.hpp>

#include "QtRocket/file/XmlScanner.h"
#include "QtRocket/file/simplesax/DelegatorHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// The local name of a qualified name as the namespace-aware parser reports it: what follows the
/// first colon after the first character ("r:engine" gives "engine", ":a" stays ":a").
[[nodiscard]] std::string_view localName(std::string_view name) noexcept
{
    const std::size_t colon = name.find(':', 1);
    return colon == std::string_view::npos ? name : name.substr(colon + 1);
}

/// The attributes of @p element as SAX reports them: by local name, without the namespace
/// declarations (a later attribute with the same local name replaces an earlier one, as in
/// Java's HashMap).
[[nodiscard]] ElementHandler::Attributes attributesOf(const pugi::xml_node& element)
{
    ElementHandler::Attributes attributes;
    for (const pugi::xml_attribute& attribute : element.attributes())
    {
        const std::string_view name = attribute.name();
        if (name == "xmlns" || name.starts_with("xmlns:"))
        {
            continue;
        }
        attributes.insert_or_assign(std::string(localName(name)), std::string(attribute.value()));
    }
    return attributes;
}

/// Feeds the SAX events of the element @p root and its content to @p sax, in document order and
/// without recursion (so that deep nesting cannot exhaust the stack), up to @p budget element
/// events.
[[nodiscard]] Result<void> readElement(const pugi::xml_node& root, DelegatorHandler& sax,
                                       std::size_t budget)
{
    const auto start = [&sax, &budget](const pugi::xml_node& element) -> Result<void> {
        budget--;
        return sax.startElement(localName(element.name()), attributesOf(element));
    };
    if (budget == 0)
    {
        return {};
    }
    if (Result<void> started = start(root); !started)
    {
        return started;
    }
    pugi::xml_node parent = root;
    pugi::xml_node node   = root.first_child();
    while (budget > 0)
    {
        if (node.empty())
        {
            budget--;
            if (Result<void> ended = sax.endElement(localName(parent.name())); !ended)
            {
                return ended;
            }
            if (parent == root)
            {
                return {};
            }
            node   = parent.next_sibling();
            parent = parent.parent();
            continue;
        }
        switch (node.type())
        {
            case pugi::node_element:
                if (Result<void> started = start(node); !started)
                {
                    return started;
                }
                parent = node;
                node   = node.first_child();
                continue;
            case pugi::node_pcdata:
            case pugi::node_cdata:
                sax.characters(node.value());
                break;
            default:  // comments and processing instructions are not character data
                break;
        }
        node = node.next_sibling();
    }
    return {};
}

/// XML 1.1's line ends as its parser reads them: "\r\n", "\r\u0085", "\r", U+0085 and U+2028
/// all become "\n" (pugixml knows only XML 1.0's).
[[nodiscard]] std::string normalizeXml11LineEnds(std::string_view text)
{
    constexpr std::string_view kNel = "\u0085";
    constexpr std::string_view kLs  = "\u2028";
    std::string                normalized;
    normalized.reserve(text.size());
    for (std::size_t i = 0; i < text.size();)
    {
        const std::string_view rest = text.substr(i);
        if (rest.starts_with('\r'))
        {
            i++;
            if (rest.substr(1).starts_with('\n'))
            {
                i++;
            }
            else if (rest.substr(1).starts_with(kNel))
            {
                i += kNel.size();
            }
            normalized += '\n';
        }
        else if (rest.starts_with(kNel) || rest.starts_with(kLs))
        {
            i += rest.starts_with(kNel) ? kNel.size() : kLs.size();
            normalized += '\n';
        }
        else
        {
            normalized += text[i++];
        }
    }
    return normalized;
}

/// Reads the document @p valid, well-formed UTF-8, for readXml(). With @p decodeFailure,
/// @p valid is only the part of a document that could be decoded, and that is why the rest
/// could not be: the JDK's parser fails with it where it asks its reader for the characters
/// after @p valid, unless an error in @p valid stops it before.
[[nodiscard]] Result<void> read(std::string_view valid, ElementHandler& initialHandler,
                                WarningSet& warnings, const Error* decodeFailure)
{
    const XmlScanner::Report report = XmlScanner::scan(valid);
    DelegatorHandler         sax(initialHandler, warnings);
    std::optional<Error>     failure = report.error;
    std::size_t              budget =
        report.error.has_value() ? report.elementEvents : std::numeric_limits<std::size_t>::max();
    if (decodeFailure != nullptr && report.eventsBeforeEnd.has_value())
    {
        failure = *decodeFailure;
        budget  = *report.eventsBeforeEnd;
    }

    if (report.elementEvents > 0)
    {
        std::string      normalized;
        std::string_view document = valid;
        if (report.xml11)
        {
            normalized = normalizeXml11LineEnds(valid);
            document   = normalized;
        }
        pugi::xml_document dom;
        // parse_ws_pcdata keeps whitespace-only text, which SAX reports as characters too;
        // parse_fragment keeps what precedes an error in a malformed document.
        const pugi::xml_parse_result parsed =
            dom.load_buffer(document.data(), document.size(),
                            pugi::parse_default | pugi::parse_ws_pcdata | pugi::parse_fragment,
                            pugi::encoding_utf8);
        if (const pugi::xml_node root = dom.find_child(
                [](const pugi::xml_node& node) { return node.type() == pugi::node_element; });
            !root.empty())
        {
            if (Result<void> read = readElement(root, sax, budget); !read)
            {
                return read;
            }
        }
        if (!failure.has_value() && !parsed)
        {
            // XmlScanner accepts nothing pugixml rejects; this is a safety net.
            return fail(ErrorCode::PARSE,
                        std::format("{} (at offset {})", parsed.description(), parsed.offset));
        }
    }
    if (failure.has_value())
    {
        return std::unexpected(std::move(*failure));
    }
    return {};
}

// ---- the encoding of a document given as bytes ---------------------------------------------

/// The encodings a document given as bytes is read in.
enum class Charset
{
    UTF_8,          ///< strictly, as Xerces's UTF8Reader
    UTF_8_LENIENT,  ///< as Java's decoder, which replaces what is malformed
    ISO_8859_1,
    US_ASCII,
    WINDOWS_1252,
    WIDE,  ///< UTF-16 or UTF-32 named in a document that is not: no markup comes of it
    UCS,   ///< UCS-2 or UCS-4 named in a document that is not: Xerces knows no byte order for it
};

struct NamedCharset
{
    std::string_view name;
    Charset          charset;
};

/// The names an XML declaration may give an encoding, upper case or not: the ones Xerces maps
/// (EncodingMap) and the further ones Java's Charset.forName() knows, each measured with the
/// JDK's parser.
constexpr auto kCharsets = std::to_array<NamedCharset>({
    {.name = "UTF-8", .charset = Charset::UTF_8},
    {.name = "UTF8", .charset = Charset::UTF_8_LENIENT},
    {.name = "unicode-1-1-utf-8", .charset = Charset::UTF_8_LENIENT},
    {.name = "ISO-8859-1", .charset = Charset::ISO_8859_1},
    {.name = "ISO_8859-1", .charset = Charset::ISO_8859_1},
    {.name = "ISO_8859_1", .charset = Charset::ISO_8859_1},
    {.name = "ISO8859_1", .charset = Charset::ISO_8859_1},
    {.name = "ISO8859-1", .charset = Charset::ISO_8859_1},
    {.name = "ISO-IR-100", .charset = Charset::ISO_8859_1},
    {.name = "LATIN1", .charset = Charset::ISO_8859_1},
    {.name = "L1", .charset = Charset::ISO_8859_1},
    {.name = "IBM819", .charset = Charset::ISO_8859_1},
    {.name = "IBM-819", .charset = Charset::ISO_8859_1},
    {.name = "CP819", .charset = Charset::ISO_8859_1},
    {.name = "CSISOLATIN1", .charset = Charset::ISO_8859_1},
    {.name = "US-ASCII", .charset = Charset::US_ASCII},
    {.name = "ASCII", .charset = Charset::US_ASCII},
    {.name = "ISO646-US", .charset = Charset::US_ASCII},
    {.name = "ANSI_X3.4-1968", .charset = Charset::US_ASCII},
    {.name = "ANSI_X3.4-1986", .charset = Charset::US_ASCII},
    {.name = "US", .charset = Charset::US_ASCII},
    {.name = "IBM367", .charset = Charset::US_ASCII},
    {.name = "IBM-367", .charset = Charset::US_ASCII},
    {.name = "CP367", .charset = Charset::US_ASCII},
    {.name = "CSASCII", .charset = Charset::US_ASCII},
    {.name = "ISO-IR-6", .charset = Charset::US_ASCII},
    {.name = "WINDOWS-1252", .charset = Charset::WINDOWS_1252},
    {.name = "CP1252", .charset = Charset::WINDOWS_1252},
    {.name = "IBM-1252", .charset = Charset::WINDOWS_1252},
    {.name = "IBM1252", .charset = Charset::WINDOWS_1252},
    {.name = "CP5348", .charset = Charset::WINDOWS_1252},
    {.name = "UTF-16", .charset = Charset::WIDE},
    {.name = "UTF-16BE", .charset = Charset::WIDE},
    {.name = "UTF-16LE", .charset = Charset::WIDE},
    {.name = "UTF-32", .charset = Charset::WIDE},
    {.name = "UTF-32BE", .charset = Charset::WIDE},
    {.name = "UTF-32LE", .charset = Charset::WIDE},
    {.name = "ISO-10646-UCS-2", .charset = Charset::UCS},
    {.name = "ISO-10646-UCS-4", .charset = Charset::UCS},
});

/// The characters of windows-1252's bytes 0x80 to 0x9F as Java's decoder gives them: U+FFFD
/// for the five it leaves undefined.
constexpr std::array<char32_t, 32> kWindows1252{
    0x20AC, 0xFFFD, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160,
    0x2039, 0x0152, 0xFFFD, 0x017D, 0xFFFD, 0xFFFD, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022,
    0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0xFFFD, 0x017E, 0x0178};

[[nodiscard]] std::optional<Charset> charsetOf(std::string_view name) noexcept
{
    for (const NamedCharset& known : kCharsets)
    {
        if (Strings::equalsIgnoreAsciiCase(known.name, name))
        {
            return known.charset;
        }
    }
    return std::nullopt;
}

/// XMLChar.isValidIANAEncoding(): an ASCII letter, then letters, digits, '.', '_' and '-'.
[[nodiscard]] bool isValidEncodingName(std::string_view name) noexcept
{
    const auto letter = [](char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); };
    if (name.empty() || !letter(name.front()))
    {
        return false;
    }
    return std::ranges::all_of(name.substr(1), [&letter](char c) {
        return letter(c) || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
    });
}

/// What decoding a document gave.
struct Decoded
{
    /// The document in UTF-8, or the part of it before the bytes that cannot be decoded.
    std::string text;
    /// Why the document cannot be decoded, or not beyond @p text.
    std::optional<Error> failure;
    /// Whether @p failure is reported before any handler is called (and @p text is not read).
    bool beforeHandlers{false};
};

[[nodiscard]] Decoded refused(ErrorCode code, std::string message)
{
    return {.text = {}, .failure = fail(code, std::move(message)).error(), .beforeHandlers = true};
}

[[nodiscard]] unsigned byteAt(std::span<const std::byte> bytes, std::size_t index) noexcept
{
    return std::to_integer<unsigned>(bytes[index]);
}

[[nodiscard]] bool isContinuation(unsigned b) noexcept
{
    return (b & 0xC0U) == 0x80U;
}

// Xerces's UTF8Reader reads a sequence byte by byte and fails at the first byte that is
// missing ("Expected byte ...", at the end of the data) or wrong ("Invalid byte ...").

/// The failure of the byte at @p index, the @p position-th of a sequence of @p count bytes,
/// which is wrong when @p accept refuses it; none when it is there and right.
template <std::predicate<unsigned> Accept>
[[nodiscard]] std::optional<std::string> byteFault(std::span<const std::byte> bytes,
                                                   std::size_t index, int position, int count,
                                                   Accept accept)
{
    if (index >= bytes.size())
    {
        return std::format("Expected byte {} of {}-byte UTF-8 sequence.", position, count);
    }
    if (!accept(byteAt(bytes, index)))
    {
        return std::format("Invalid byte {} of {}-byte UTF-8 sequence.", position, count);
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> threeByteFault(std::span<const std::byte> bytes,
                                                        std::size_t                start)
{
    const unsigned b0 = byteAt(bytes, start);
    // no encoded surrogate (ED A0..BF), no overlong form (E0 80..9F)
    const auto second = [b0](unsigned b1) {
        return isContinuation(b1) && (b0 != 0xED || b1 < 0xA0) &&
               ((b0 & 0x0FU) != 0 || (b1 & 0x20U) != 0);
    };
    if (std::optional<std::string> fault = byteFault(bytes, start + 1, 2, 3, second))
    {
        return fault;
    }
    return byteFault(bytes, start + 2, 3, 3, isContinuation);
}

[[nodiscard]] std::optional<std::string> fourByteFault(std::span<const std::byte> bytes,
                                                       std::size_t                start)
{
    const unsigned b0 = byteAt(bytes, start);
    // no overlong form (F0 80..8F)
    const auto second = [b0](unsigned b1) {
        return isContinuation(b1) && ((b1 & 0x30U) != 0 || (b0 & 0x07U) != 0);
    };
    if (std::optional<std::string> fault = byteFault(bytes, start + 1, 2, 4, second))
    {
        return fault;
    }
    if (std::optional<std::string> fault = byteFault(bytes, start + 2, 3, 4, isContinuation))
    {
        return fault;
    }
    if (std::optional<std::string> fault = byteFault(bytes, start + 3, 4, 4, isContinuation))
    {
        return fault;
    }
    // beyond U+10FFFF: the five bits that make the high surrogate
    const unsigned uuuuu = ((b0 << 2U) & 0x1CU) | ((byteAt(bytes, start + 1) >> 4U) & 0x03U);
    if (uuuuu > 0x10)
    {
        return std::format(
            "High surrogate bits in UTF-8 sequence must not exceed 0x10 but found 0x{:x}.", uuuuu);
    }
    return std::nullopt;
}

/// The failure of the sequence that the byte at @p start (0x80 or above) begins, or none, with
/// its length in @p length.
[[nodiscard]] std::optional<std::string> sequenceFault(std::span<const std::byte> bytes,
                                                       std::size_t start, std::size_t& length)
{
    const unsigned b0 = byteAt(bytes, start);
    if ((b0 & 0xE0U) == 0xC0U && (b0 & 0x1EU) != 0)
    {
        length = 2;
        return byteFault(bytes, start + 1, 2, 2, isContinuation);
    }
    if ((b0 & 0xF0U) == 0xE0U)
    {
        length = 3;
        return threeByteFault(bytes, start);
    }
    if ((b0 & 0xF8U) == 0xF0U)
    {
        length = 4;
        return fourByteFault(bytes, start);
    }
    return "Invalid byte 1 of 1-byte UTF-8 sequence.";
}

/// @p bytes read as Xerces's UTF8Reader reads them: the text up to the first sequence that is
/// not UTF-8, and that sequence's failure.
[[nodiscard]] Decoded decodeUtf8(std::span<const std::byte> bytes)
{
    std::size_t next = 0;
    while (next < bytes.size())
    {
        if (byteAt(bytes, next) < 0x80)
        {
            next++;
            continue;
        }
        std::size_t length = 1;
        if (std::optional<std::string> fault = sequenceFault(bytes, next, length))
        {
            return {.text           = bytesToString(bytes.first(next)),
                    .failure        = fail(ErrorCode::PARSE, std::move(*fault)).error(),
                    .beforeHandlers = false};
        }
        next += length;
    }
    return {.text = bytesToString(bytes), .failure = {}, .beforeHandlers = false};
}

[[nodiscard]] std::string decodeWindows1252(std::span<const std::byte> bytes)
{
    std::u32string text;
    text.reserve(bytes.size());
    for (const std::byte b : bytes)
    {
        const auto value = std::to_integer<unsigned>(b);
        text += value >= 0x80 && value < 0xA0 ? kWindows1252.at(value - 0x80)
                                              : static_cast<char32_t>(value);
    }
    return Strings::fromCodePoints(text);
}

/// Where the XML declaration that @p body starts with ends: after its "?>", which is the first
/// one outside a quoted value; the size of @p body when there is none; 0 when @p body does not
/// start with "<?xml".
[[nodiscard]] std::size_t xmlDeclarationSize(std::span<const std::byte> body) noexcept
{
    constexpr std::string_view kStart = "<?xml";
    if (body.size() < kStart.size() ||
        !std::ranges::equal(body.first(kStart.size()), kStart,
                            [](std::byte b, char c) { return std::to_integer<char>(b) == c; }))
    {
        return 0;
    }
    char quote = '\0';
    for (std::size_t i = kStart.size(); i < body.size(); i++)
    {
        const auto c = std::to_integer<char>(body[i]);
        if (quote != '\0')
        {
            quote = c == quote ? '\0' : quote;
        }
        else if (c == '\'' || c == '"')
        {
            quote = c;
        }
        else if (c == '>' && std::to_integer<char>(body[i - 1]) == '?')
        {
            return i + 1;
        }
    }
    return body.size();
}

/// @p rest, what follows the XML declaration @p head (already UTF-8), read in @p charset, which
/// the declaration names @p name.
[[nodiscard]] Decoded decodeRest(const std::string& head, std::span<const std::byte> rest,
                                 Charset charset, std::string_view name)
{
    switch (charset)
    {
        case Charset::UTF_8:
        {
            Decoded decoded = decodeUtf8(rest);
            decoded.text.insert(0, head);
            return decoded;
        }
        case Charset::UTF_8_LENIENT:
            return {.text           = head + Strings::toValidUtf8(bytesToString(rest)),
                    .failure        = {},
                    .beforeHandlers = false};
        case Charset::ISO_8859_1:
            return {.text           = head + Strings::latin1ToUtf8(bytesToString(rest)),
                    .failure        = {},
                    .beforeHandlers = false};
        case Charset::WINDOWS_1252:
            return {.text = head + decodeWindows1252(rest), .failure = {}, .beforeHandlers = false};
        case Charset::US_ASCII:
        {
            const auto high = std::ranges::find_if(
                rest, [](std::byte b) { return std::to_integer<unsigned>(b) >= 0x80; });
            if (high != rest.end())
            {
                return refused(
                    ErrorCode::PARSE,
                    std::format("Byte \"{}\" is not a member of the (7-bit) ASCII character set.",
                                std::to_integer<unsigned>(*high)));
            }
            return {.text = head + bytesToString(rest), .failure = {}, .beforeHandlers = false};
        }
        case Charset::WIDE:
            // Read as UTF-16 or UTF-32, bytes that are not give characters that are no markup.
            return refused(ErrorCode::PARSE, rest.empty() ? "Premature end of file."
                                                          : "Content is not allowed in prolog.");
        case Charset::UCS:
            return refused(
                ErrorCode::PARSE,
                std::format("Given byte order for encoding \"{}\" is not supported.", name));
    }
    QTROCKET_UNREACHABLE();
}

/// @p body, a document without its byte order mark that is UTF-8 unless its XML declaration
/// says otherwise.
[[nodiscard]] Decoded decodeBody(std::span<const std::byte> body)
{
    const std::size_t          headSize = xmlDeclarationSize(body);
    std::optional<std::string> encoding;
    Decoded                    head;
    if (headSize > 0)
    {
        // The declaration itself is read in UTF-8; one that cannot be names no encoding.
        head = decodeUtf8(body.first(headSize));
        if (!head.failure.has_value())
        {
            encoding = XmlScanner::declaredEncoding(head.text);
        }
    }
    if (!encoding.has_value())
    {
        return decodeUtf8(body);
    }
    if (!isValidEncodingName(*encoding))
    {
        return refused(ErrorCode::PARSE, std::format("Invalid encoding name \"{}\".", *encoding));
    }
    const std::optional<Charset> charset = charsetOf(*encoding);
    if (!charset.has_value())
    {
        // Java's UnsupportedEncodingException, whose message is the name.
        return refused(ErrorCode::IO, *encoding);
    }
    return decodeRest(head.text, body.subspan(headSize), *charset, *encoding);
}

/// The encoding Xerces takes from the first four bytes of a document
/// (XMLEntityManager.getEncodingInfo()).
enum class Detected
{
    UTF_8,
    UTF_8_WITH_BOM,
    UTF_16_BE_WITH_BOM,
    UTF_16_LE_WITH_BOM,
    UTF_16_BE,
    UTF_16_LE,
    UCS_4,
    EBCDIC,
};

[[nodiscard]] Detected detect(std::span<const std::byte> data) noexcept
{
    // Xerces reads four bytes into a byte array, and a missing one is the -1 of the end of the
    // stream stored as a byte: 0xFF. (So a file of the one byte FE is a UTF-16 byte order
    // mark to it.)
    const auto at = [data](std::size_t index) {
        return index < data.size() ? byteAt(data, index) : 0xFFU;
    };
    const std::array<unsigned, 4> b{at(0), at(1), at(2), at(3)};
    const auto                    is = [&b](unsigned b0, unsigned b1, unsigned b2, unsigned b3) {
        return b == std::array<unsigned, 4>{b0, b1, b2, b3};
    };
    if (b[0] == 0xFE && b[1] == 0xFF)
    {
        return Detected::UTF_16_BE_WITH_BOM;
    }
    if (b[0] == 0xFF && b[1] == 0xFE)
    {
        return Detected::UTF_16_LE_WITH_BOM;
    }
    if (b[0] == 0xEF && b[1] == 0xBB && b[2] == 0xBF)
    {
        return Detected::UTF_8_WITH_BOM;
    }
    if (is(0x00, 0x00, 0x00, 0x3C) || is(0x3C, 0x00, 0x00, 0x00) || is(0x00, 0x00, 0x3C, 0x00) ||
        is(0x00, 0x3C, 0x00, 0x00))
    {
        return Detected::UCS_4;
    }
    if (is(0x00, 0x3C, 0x00, 0x3F))
    {
        return Detected::UTF_16_BE;
    }
    if (is(0x3C, 0x00, 0x3F, 0x00))
    {
        return Detected::UTF_16_LE;
    }
    if (is(0x4C, 0x6F, 0xA7, 0x94))
    {
        return Detected::EBCDIC;
    }
    return Detected::UTF_8;
}

/// A document that starts with a UTF-16 byte order mark: one that is nothing else is at its
/// end, as the JDK's parser finds; any other is in an encoding this does not read.
[[nodiscard]] Decoded utf16WithBom(std::span<const std::byte> data, std::string_view name)
{
    if (data.size() <= 2)
    {
        return refused(ErrorCode::PARSE, "Premature end of file.");
    }
    return refused(ErrorCode::IO, std::string(name));
}

/// The document @p data as the JDK's parser decodes it (see SimpleSax::readXml()).
[[nodiscard]] Decoded decode(std::span<const std::byte> data)
{
    switch (detect(data))
    {
        case Detected::UTF_8:
            return decodeBody(data);
        case Detected::UTF_8_WITH_BOM:
            return decodeBody(data.subspan(3));
        case Detected::UTF_16_BE_WITH_BOM:
            return utf16WithBom(data, "UTF-16BE");
        case Detected::UTF_16_LE_WITH_BOM:
            return utf16WithBom(data, "UTF-16LE");
        case Detected::UTF_16_BE:
            return refused(ErrorCode::IO, "UTF-16BE");
        case Detected::UTF_16_LE:
            return refused(ErrorCode::IO, "UTF-16LE");
        case Detected::UCS_4:
            return refused(ErrorCode::IO, "ISO-10646-UCS-4");
        case Detected::EBCDIC:
            return refused(ErrorCode::IO, "CP037");
    }
    QTROCKET_UNREACHABLE();
}

}  // namespace

Result<void> SimpleSax::readXml(std::string_view text, ElementHandler& initialHandler,
                                WarningSet& warnings)
{
    return read(Strings::toValidUtf8(text), initialHandler, warnings, nullptr);
}

Result<void> SimpleSax::readXml(std::span<const std::byte> data, ElementHandler& initialHandler,
                                WarningSet& warnings)
{
    Decoded decoded = decode(data);
    if (decoded.failure.has_value() && decoded.beforeHandlers)
    {
        return std::unexpected(std::move(*decoded.failure));
    }
    return read(decoded.text, initialHandler, warnings,
                decoded.failure.has_value() ? &*decoded.failure : nullptr);
}

}  // namespace QtRocket
