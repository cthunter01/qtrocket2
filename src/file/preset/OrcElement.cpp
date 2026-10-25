#include "QtRocket/file/preset/OrcElement.h"

#include <cstddef>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "OrcNodes.h"
#include "QtRocket/file/preset/JaxbLexical.h"

namespace QtRocket
{

namespace
{

/// The last value @p parse reads from @p texts, or nullopt when it reads none.
template <class T, class Parse>
[[nodiscard]] std::optional<T> lastParsed(std::span<const std::string> texts, Parse parse)
{
    for (const std::string& text : std::views::reverse(texts))
    {
        if (std::optional<T> value = parse(text))
        {
            return value;
        }
    }
    return std::nullopt;
}

/// @p state after JAXB hands a Boolean field @p text.
[[nodiscard]] std::optional<bool> readBoolean(std::optional<bool> state, std::string_view text)
{
    switch (JaxbLexical::parseBoolean(text))
    {
        case JaxbLexical::ParsedBoolean::IS_TRUE:
            return true;
        case JaxbLexical::ParsedBoolean::IS_FALSE:
            return false;
        case JaxbLexical::ParsedBoolean::IS_NULL:
            return std::nullopt;
        case JaxbLexical::ParsedBoolean::REJECTED:
            break;
    }
    return state;
}

}  // namespace

OrcElement::OrcElement(const OrcNodes& nodes, std::size_t index) noexcept
  : m_nodes(&nodes), m_index(index)
{
}

std::string_view OrcElement::namespaceUri() const noexcept
{
    return m_nodes->nodes[m_index].namespaceUri;
}

std::string_view OrcElement::localName() const noexcept
{
    return m_nodes->nodes[m_index].localName;
}

bool OrcElement::isNamed(std::string_view name) const noexcept
{
    const OrcNodes::Node& node = m_nodes->nodes[m_index];
    return node.namespaceUri.empty() && node.localName == name;
}

std::vector<OrcElement> OrcElement::children() const
{
    std::vector<OrcElement> children;
    for (const std::size_t child : m_nodes->nodes[m_index].children)
    {
        children.push_back(OrcElement(*m_nodes, child));
    }
    return children;
}

std::vector<OrcElement> OrcElement::children(std::string_view name) const
{
    std::vector<OrcElement> children;
    for (const std::size_t child : m_nodes->nodes[m_index].children)
    {
        if (const OrcElement element(*m_nodes, child); element.isNamed(name))
        {
            children.push_back(element);
        }
    }
    return children;
}

std::optional<OrcElement> OrcElement::lastChild(std::string_view name) const
{
    for (const std::size_t child : std::views::reverse(m_nodes->nodes[m_index].children))
    {
        if (const OrcElement element(*m_nodes, child); element.isNamed(name))
        {
            return element;
        }
    }
    return std::nullopt;
}

std::optional<std::string_view> OrcElement::attribute(std::string_view name) const
{
    for (const OrcNodes::Attribute& attribute : m_nodes->nodes[m_index].attributes)
    {
        if (attribute.name == name)
        {
            return attribute.value;
        }
    }
    return std::nullopt;
}

std::span<const std::string> OrcElement::texts() const noexcept
{
    return m_nodes->nodes[m_index].texts;
}

const std::string& OrcElement::text() const noexcept
{
    return m_nodes->nodes[m_index].texts.back();
}

std::optional<double> OrcElement::doubleValue() const
{
    return lastParsed<double>(texts(), JaxbLexical::parseDouble);
}

std::optional<int> OrcElement::intValue() const
{
    return lastParsed<int>(texts(), JaxbLexical::parseInt);
}

std::optional<bool> OrcElement::booleanValue() const
{
    std::optional<bool> value;
    for (const std::string& text : texts())
    {
        value = readBoolean(value, text);
    }
    return value;
}

std::optional<std::string_view> OrcElement::stringProperty(std::string_view name) const
{
    if (const std::optional<OrcElement> child = lastChild(name))
    {
        return child->text();
    }
    return std::nullopt;
}

std::optional<double> OrcElement::doubleProperty(std::string_view name) const
{
    if (const std::optional<OrcElement> child = lastChild(name))
    {
        return child->doubleValue();
    }
    return std::nullopt;
}

std::optional<double> OrcElement::boxedDoubleProperty(std::string_view name) const
{
    std::optional<double> value;
    for (const OrcElement& child : children(name))
    {
        if (const std::optional<double> read = child.doubleValue())
        {
            value = read;
        }
    }
    return value;
}

std::optional<int> OrcElement::integerProperty(std::string_view name) const
{
    std::optional<int> value;
    for (const OrcElement& child : children(name))
    {
        if (const std::optional<int> read = child.intValue())
        {
            value = read;
        }
    }
    return value;
}

std::optional<bool> OrcElement::booleanProperty(std::string_view name) const
{
    std::optional<bool> value;
    for (const OrcElement& child : children(name))
    {
        for (const std::string& text : child.texts())
        {
            value = readBoolean(value, text);
        }
    }
    return value;
}

std::optional<std::vector<std::byte>> OrcElement::base64BinaryProperty(std::string_view name) const
{
    std::optional<std::vector<std::byte>> value;
    for (const OrcElement& child : children(name))
    {
        for (const std::string& text : child.texts())
        {
            if (std::optional<std::vector<std::byte>> read = JaxbLexical::parseBase64Binary(text))
            {
                value = std::move(read);
            }
        }
    }
    return value;
}

}  // namespace QtRocket
