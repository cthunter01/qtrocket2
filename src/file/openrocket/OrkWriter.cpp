#include "QtRocket/file/openrocket/OrkWriter.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "QtRocket/util/BigDecimal.h"
#include "QtRocket/util/Config.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// U+FFFE and U+FFFF in UTF-8: the two noncharacters of the Basic Multilingual Plane, which
/// XML 1.0's Char production leaves out.
constexpr std::string_view kNoncharacterFffe = "\xEF\xBF\xBE";
constexpr std::string_view kNoncharacterFfff = "\xEF\xBF\xBF";

/// Whether XML 1.0 forbids the C0 control character @p c in a document (all but tab, line feed
/// and carriage return).
[[nodiscard]] bool isForbiddenControl(char c) noexcept
{
    const auto value = static_cast<unsigned char>(c);
    return value < 0x20 && c != '\t' && c != '\n' && c != '\r';
}

/// The type attribute and the text of an entry for a number, string or boolean (not a list).
struct EntryText
{
    std::string_view type;
    std::string      text;
};

/// The type and text of writeEntry() for @p value, which is no list.
[[nodiscard]] EntryText entryText(const Config::Value& value, bool explicitNumberTypes,
                                  OrkWriter& writer)
{
    const Config::Value::Variant& data = value.variant();
    if (const bool* b = std::get_if<bool>(&data))
    {
        return {.type = "boolean", .text = *b ? "true" : "false"};
    }
    if (const std::int32_t* i = std::get_if<std::int32_t>(&data))
    {
        return {.type = explicitNumberTypes ? "integer" : "number", .text = std::format("{}", *i)};
    }
    if (const double* d = std::get_if<double>(&data))
    {
        return {.type = explicitNumberTypes ? "double" : "number",
                .text = Strings::javaDoubleToString(*d)};
    }
    if (const std::int8_t* b = std::get_if<std::int8_t>(&data))
    {
        // std::format would print an int8_t as a character.
        return {.type = "number", .text = std::format("{}", static_cast<int>(*b))};
    }
    if (const std::int16_t* s = std::get_if<std::int16_t>(&data))
    {
        return {.type = "number", .text = std::format("{}", *s)};
    }
    if (const std::int64_t* l = std::get_if<std::int64_t>(&data))
    {
        return {.type = "number", .text = std::format("{}", *l)};
    }
    if (const float* f = std::get_if<float>(&data))
    {
        return {.type = "number", .text = Strings::javaFloatToString(*f)};
    }
    if (const BigDecimal* big = std::get_if<BigDecimal>(&data))
    {
        return {.type = "number", .text = big->toString()};
    }
    const auto& text = std::get<std::string>(data);
    return {.type = "string", .text = writer.escape(text)};
}

}  // namespace

void OrkWriter::line(std::string_view text)
{
    if (text.empty())
    {
        m_text.push_back('\n');
        return;
    }
    for (int level = 0; level < m_indent; ++level)
    {
        m_text.append(kIndent);
    }
    m_text.append(text);
    m_text.push_back('\n');
}

void OrkWriter::blankLine()
{
    line("");
}

std::string OrkWriter::escapeText(std::string_view text, std::size_t& dropped)
{
    const std::string valid = Strings::toValidUtf8(text);
    std::string       kept;
    kept.reserve(valid.size());
    const std::string_view view(valid);
    std::size_t            position = 0;
    while (position < view.size())
    {
        if (isForbiddenControl(view[position]))
        {
            ++dropped;
            ++position;
            continue;
        }
        const std::string_view rest = view.substr(position);
        if (rest.starts_with(kNoncharacterFffe) || rest.starts_with(kNoncharacterFfff))
        {
            // The text is well-formed UTF-8 by now, so these bytes are the whole character.
            ++dropped;
            position += kNoncharacterFfff.size();
            continue;
        }
        kept.push_back(view[position]);
        ++position;
    }
    return Strings::escapeXml(kept);
}

std::string OrkWriter::escape(std::string_view text)
{
    return escapeText(text, m_dropped);
}

void OrkWriter::elementText(std::string_view name, std::string_view text)
{
    line(std::format("<{}>{}</{}>", name, text, name));
}

void OrkWriter::element(std::string_view name, std::string_view text)
{
    elementText(name, escape(text));
}

void OrkWriter::element(std::string_view name, double value)
{
    elementText(name, Strings::javaDoubleToString(value));
}

void OrkWriter::entry(std::string_view tagName, std::optional<std::string_view> key,
                      const Config::Value& value, bool explicitNumberTypes)
{
    const std::string keyAttribute =
        key.has_value() ? std::format("key=\"{}\" ", escape(*key)) : std::string();
    if (const Config::List* list = std::get_if<Config::List>(&value.variant()))
    {
        // Nested element: the elements of the list are entries without a key.
        line(std::format("<{} {}type=\"list\">", tagName, keyAttribute));
        indent();
        for (const Config::Value& element : *list)
        {
            entry(tagName, std::nullopt, element, explicitNumberTypes);
        }
        outdent();
        line(std::format("</{}>", tagName));
        return;
    }
    const EntryText text = entryText(value, explicitNumberTypes, *this);
    line(std::format("<{} {}type=\"{}\">{}</{}>", tagName, keyAttribute, text.type, text.text,
                     tagName));
}

std::string OrkWriter::release() noexcept
{
    return std::exchange(m_text, std::string());
}

}  // namespace QtRocket
