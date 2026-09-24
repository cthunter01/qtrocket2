#include "QtRocket/util/Color.h"

#include <format>
#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// Integer.parseInt of an attribute value, accepted only within a colour channel's 0-255.
[[nodiscard]] std::optional<int> parseChannel(std::string_view text) noexcept
{
    const std::optional<int> value = Strings::parseInt(text);
    if (!value.has_value() || *value < 0 || *value > 255)
    {
        return std::nullopt;
    }
    return value;
}

}  // namespace

std::string Color::toString() const
{
    return std::format("Color [r={}, g={}, b={}, a={}]", m_red, m_green, m_blue, m_alpha);
}

std::string Color::toXmlAttributes() const
{
    return std::format(R"(red="{}" green="{}" blue="{}" alpha="{}")", m_red, m_green, m_blue,
                       m_alpha);
}

std::optional<Color> Color::fromXmlAttributes(std::optional<std::string_view> red,
                                              std::optional<std::string_view> green,
                                              std::optional<std::string_view> blue,
                                              std::optional<std::string_view> alpha)
{
    if (!red.has_value() || !green.has_value() || !blue.has_value())
    {
        return std::nullopt;
    }
    const std::optional<int> r = parseChannel(*red);
    const std::optional<int> g = parseChannel(*green);
    const std::optional<int> b = parseChannel(*blue);
    std::optional<int>       a = kOpaque;
    if (alpha.has_value())
    {
        a = parseChannel(*alpha);
    }
    if (!r.has_value() || !g.has_value() || !b.has_value() || !a.has_value())
    {
        return std::nullopt;
    }
    return Color{*r, *g, *b, *a};
}

}  // namespace QtRocket
