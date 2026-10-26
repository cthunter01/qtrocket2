#include "QtRocket/file/preset/OrcDocument.h"

#include <cstddef>
#include <expected>
#include <format>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <pugixml.hpp>

#include "OrcNodes.h"
#include "QtRocket/file/XmlScanner.h"
#include "QtRocket/file/preset/JaxbLexical.h"
#include "QtRocket/file/preset/OrcElement.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// The namespace the prefix "xml" is bound to in every document.
constexpr std::string_view kXmlNamespace = "http://www.w3.org/XML/1998/namespace";

/// The position of the colon that ends the prefix of a qualified name, as the namespace-aware
/// parser reads it: the first colon after the first character (none in ":a").
[[nodiscard]] std::size_t prefixEnd(std::string_view name) noexcept
{
    return name.find(':', 1);
}

/// The namespace bindings in scope while the elements are walked: for each prefix ("" for the
/// default namespace) the namespaces bound to it, the innermost last.
class NamespaceScope
{
public:
    NamespaceScope() { m_bindings[std::string("xml")].emplace_back(kXmlNamespace); }

    /// Binds the namespaces @p element declares; returns how many it declares.
    std::size_t enter(const pugi::xml_node& element)
    {
        std::size_t declared = 0;
        for (const pugi::xml_attribute& attribute : element.attributes())
        {
            const std::string_view name = attribute.name();
            if (name == "xmlns" || name.starts_with("xmlns:"))
            {
                std::string prefix(name == "xmlns" ? std::string_view() : name.substr(6));
                m_bindings[prefix].emplace_back(attribute.value());
                m_declared.push_back(std::move(prefix));
                declared++;
            }
        }
        return declared;
    }

    /// Undoes the last @p declared bindings.
    void leave(std::size_t declared)
    {
        for (; declared > 0; declared--)
        {
            m_bindings[m_declared.back()].pop_back();
            m_declared.pop_back();
        }
    }

    /// The namespace of the element named @p qualifiedName, given its bindings.
    [[nodiscard]] std::string namespaceOf(std::string_view qualifiedName) const
    {
        const std::size_t colon  = prefixEnd(qualifiedName);
        const std::string prefix = colon == std::string_view::npos
                                       ? std::string()
                                       : std::string(qualifiedName.substr(0, colon));
        const auto        found  = m_bindings.find(prefix);
        if (found == m_bindings.end() || found->second.empty())
        {
            // XmlScanner refuses a document with an unbound prefix.
            QTROCKET_ASSERT(prefix.empty());
            return {};
        }
        return found->second.back();
    }

private:
    std::unordered_map<std::string, std::vector<std::string>> m_bindings;
    std::vector<std::string>                                  m_declared;
};

/// An element being walked.
struct Frame
{
    /// The next child node of the element to look at.
    pugi::xml_node next;
    /// The element's node.
    std::size_t index{0};
    /// The text collected since the start of the element or its last child element.
    std::string pending;
    /// How many namespace bindings the element declares.
    std::size_t declared{0};
};

/// Builds the nodes of the element tree under @p root, in document order, without recursion (so
/// that deep nesting cannot exhaust the stack).
class NodeBuilder
{
public:
    explicit NodeBuilder(std::size_t elements) { m_nodes->nodes.reserve(elements); }

    [[nodiscard]] std::unique_ptr<const OrcNodes> build(const pugi::xml_node& root)
    {
        enter(root);
        while (!m_frames.empty())
        {
            const pugi::xml_node node = m_frames.back().next;
            if (node.empty())
            {
                leave();
                continue;
            }
            m_frames.back().next = node.next_sibling();
            switch (node.type())
            {
                case pugi::node_pcdata:
                case pugi::node_cdata:
                    m_frames.back().pending += node.value();
                    break;
                case pugi::node_element:
                    handOverBeforeChild();
                    enter(node);
                    break;
                default:  // comments and processing instructions, which pugixml drops anyway
                    break;
            }
        }
        return std::move(m_nodes);
    }

private:
    void enter(const pugi::xml_node& element)
    {
        const std::size_t      declared = m_scope.enter(element);
        const std::string_view name     = element.name();
        const std::size_t      colon    = prefixEnd(name);
        OrcNodes::Node         node;
        node.namespaceUri = m_scope.namespaceOf(name);
        node.localName    = colon == std::string_view::npos ? name : name.substr(colon + 1);
        for (const pugi::xml_attribute& attribute : element.attributes())
        {
            const std::string_view attributeName = attribute.name();
            if (attributeName != "xmlns" && prefixEnd(attributeName) == std::string_view::npos)
            {
                node.attributes.push_back(OrcNodes::Attribute{.name  = std::string(attributeName),
                                                              .value = attribute.value()});
            }
        }
        const std::size_t index = m_nodes->nodes.size();
        m_nodes->nodes.push_back(std::move(node));
        if (!m_frames.empty())
        {
            m_nodes->nodes[m_frames.back().index].children.push_back(index);
        }
        m_frames.push_back(Frame{
            .next = element.first_child(), .index = index, .pending = {}, .declared = declared});
    }

    /// At a child element: the text collected before it goes to the element, unless it is
    /// white space only.
    void handOverBeforeChild()
    {
        Frame& frame = m_frames.back();
        if (!JaxbLexical::isWhiteSpaceOnly(frame.pending))
        {
            m_nodes->nodes[frame.index].texts.push_back(std::move(frame.pending));
        }
        frame.pending.clear();
    }

    /// At the end of an element: the text after its last child element goes to it, always.
    void leave()
    {
        Frame& frame = m_frames.back();
        m_nodes->nodes[frame.index].texts.push_back(std::move(frame.pending));
        m_scope.leave(frame.declared);
        m_frames.pop_back();
    }

    std::unique_ptr<OrcNodes> m_nodes = std::make_unique<OrcNodes>();
    std::vector<Frame>        m_frames;
    NamespaceScope            m_scope;
};

}  // namespace

Result<OrcDocument> OrcDocument::parse(std::span<const std::byte> data, std::size_t maxElements)
{
    return parseText(Strings::toValidUtf8(bytesToString(data)), maxElements);
}

Result<OrcDocument> OrcDocument::parseText(std::string_view text, std::size_t maxElements)
{
    const XmlScanner::Report report = XmlScanner::scan(text);
    if (report.error.has_value())
    {
        return std::unexpected(*report.error);
    }
    // Every element makes a startElement() and an endElement() call.
    const std::size_t elements = report.elementEvents / 2;
    if (elements > maxElements)
    {
        return fail(ErrorCode::UNSUPPORTED_FORMAT,
                    std::format("The document has more than {} elements.", maxElements));
    }

    std::string      normalized;
    std::string_view document = text;
    if (report.xml11)
    {
        normalized = XmlScanner::normalizeXml11LineEnds(text);
        document   = normalized;
    }
    pugi::xml_document dom;
    // parse_ws_pcdata keeps white-space-only text, which JAXB collects too.
    const pugi::xml_parse_result parsed =
        dom.load_buffer(document.data(), document.size(),
                        pugi::parse_default | pugi::parse_ws_pcdata, pugi::encoding_utf8);
    const pugi::xml_node root = dom.document_element();
    if (!parsed || root.empty())
    {
        // XmlScanner accepts nothing pugixml rejects; this is a safety net.
        return fail(ErrorCode::PARSE,
                    std::format("{} (at offset {})", parsed.description(), parsed.offset));
    }
    return OrcDocument(NodeBuilder(elements).build(root));
}

OrcDocument::OrcDocument(std::unique_ptr<const OrcNodes> nodes) noexcept : m_nodes(std::move(nodes))
{
}

OrcDocument::OrcDocument(OrcDocument&&) noexcept            = default;
OrcDocument& OrcDocument::operator=(OrcDocument&&) noexcept = default;
OrcDocument::~OrcDocument()                                 = default;

OrcElement OrcDocument::root() const
{
    QTROCKET_ASSERT(m_nodes != nullptr);
    return {*m_nodes, 0};
}

std::size_t OrcDocument::elementCount() const noexcept
{
    return m_nodes == nullptr ? 0 : m_nodes->nodes.size();
}

}  // namespace QtRocket
