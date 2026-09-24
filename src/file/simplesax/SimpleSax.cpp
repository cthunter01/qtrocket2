#include "QtRocket/file/simplesax/SimpleSax.h"

#include <cstddef>
#include <expected>
#include <format>
#include <limits>
#include <string>
#include <string_view>

#include <pugixml.hpp>

#include "QtRocket/file/XmlScanner.h"
#include "QtRocket/file/simplesax/DelegatorHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"
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
    constexpr std::string_view kLs  = " ";
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

}  // namespace

Result<void> SimpleSax::readXml(std::string_view text, ElementHandler& initialHandler,
                                WarningSet& warnings)
{
    const std::string        valid  = Strings::toValidUtf8(text);
    const XmlScanner::Report report = XmlScanner::scan(valid);
    DelegatorHandler         sax(initialHandler, warnings);
    const std::size_t        budget =
        report.error.has_value() ? report.elementEvents : std::numeric_limits<std::size_t>::max();

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
        if (!report.error.has_value() && !parsed)
        {
            // XmlScanner accepts nothing pugixml rejects; this is a safety net.
            return fail(ErrorCode::PARSE,
                        std::format("{} (at offset {})", parsed.description(), parsed.offset));
        }
    }
    if (report.error.has_value())
    {
        return std::unexpected(*report.error);
    }
    return {};
}

}  // namespace QtRocket
