#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace QtRocket
{

/// An RGBA colour with integer channels 0-255 (OpenRocket's ORColor). As in OpenRocket the
/// constructors and setters do not range-check; fromXmlAttributes() is the validating entry point
/// for file input.
class Color
{
public:
    /// Opaque alpha.
    static constexpr int kOpaque = 255;

    constexpr Color(int red, int green, int blue) noexcept : Color(red, green, blue, kOpaque) { }
    constexpr Color(int red, int green, int blue, int alpha) noexcept
      : m_red(red), m_green(green), m_blue(blue), m_alpha(alpha)
    {
    }

    [[nodiscard]] static constexpr Color black() noexcept { return {0, 0, 0}; }
    /// r=g=b=1 (near-black) with zero alpha, as ORColor.INVISIBLE.
    [[nodiscard]] static constexpr Color invisible() noexcept { return {1, 1, 1, 0}; }
    [[nodiscard]] static constexpr Color darkRed() noexcept { return {200, 0, 0}; }

    [[nodiscard]] constexpr int red() const noexcept { return m_red; }
    [[nodiscard]] constexpr int green() const noexcept { return m_green; }
    [[nodiscard]] constexpr int blue() const noexcept { return m_blue; }
    [[nodiscard]] constexpr int alpha() const noexcept { return m_alpha; }

    constexpr void setRed(int red) noexcept { m_red = red; }
    constexpr void setGreen(int green) noexcept { m_green = green; }
    constexpr void setBlue(int blue) noexcept { m_blue = blue; }
    constexpr void setAlpha(int alpha) noexcept { m_alpha = alpha; }

    /// "Color [r=200, g=0, b=0, a=255]" (ORColor prints its own class name).
    [[nodiscard]] std::string toString() const;

    /// The .ork attribute fragment: red="R" green="G" blue="B" alpha="A".
    [[nodiscard]] std::string toXmlAttributes() const;

    /// Parses the "red", "green", "blue" and optional "alpha" attributes of an .ork element, each
    /// given as its text or nullopt when the element lacks it. Gives nullopt when red, green or
    /// blue is missing, or when any given value is not an integer in 0-255. A missing alpha is
    /// kOpaque.
    [[nodiscard]] static std::optional<Color> fromXmlAttributes(
        std::optional<std::string_view> red, std::optional<std::string_view> green,
        std::optional<std::string_view> blue, std::optional<std::string_view> alpha);

    [[nodiscard]] constexpr bool operator==(const Color&) const noexcept = default;

private:
    int m_red;
    int m_green;
    int m_blue;
    int m_alpha;
};

}  // namespace QtRocket

/// Java's Objects.hash(red, green, blue, alpha), so equal colours hash alike.
template <>
struct std::hash<QtRocket::Color>
{
    [[nodiscard]] std::size_t operator()(const QtRocket::Color& color) const noexcept
    {
        // Arrays.hashCode: result = 31 * result + element from 1, wrapping like Java's int.
        std::size_t result = 1;
        result             = (31U * result) + static_cast<std::size_t>(color.red());
        result             = (31U * result) + static_cast<std::size_t>(color.green());
        result             = (31U * result) + static_cast<std::size_t>(color.blue());
        result             = (31U * result) + static_cast<std::size_t>(color.alpha());
        return result;
    }
};
