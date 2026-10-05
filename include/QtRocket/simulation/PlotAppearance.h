#pragma once

#include <optional>

#include "QtRocket/util/Color.h"
#include "QtRocket/util/LineStyle.h"

namespace QtRocket
{

/// How one data series of a simulation is plotted, where the user overrode the default: a color,
/// a line style or both (OpenRocket's document/PlotAppearance). A Simulation stores one per
/// flight data type, keyed by the type's symbol.
///
/// A value type: a copy is Java's clone(), and operator== is Java's equals() (the same color, or
/// none on both sides, and the same line style, or none on both sides).
///
/// Placement: OpenRocket keeps this class in its document package. It lives in simulation/ here
/// with Simulation, which holds it (see Simulation).
///
/// Deviations from OpenRocket:
/// - The color and the line style are std::optional, nullopt for Java's null.
/// - FACTORY_DEFAULT is factoryDefault().
/// - hashCode() is not ported: it hashes the line style, a Java enum, by its identity.
class PlotAppearance
{
public:
    /// An appearance with @p color and @p lineStyle, each nullopt for "not overridden".
    constexpr PlotAppearance(std::optional<Color>     color,
                             std::optional<LineStyle> lineStyle) noexcept
      : m_color(color), m_lineStyle(lineStyle)
    {
    }

    /// The built-in factory default: no color override and a solid line (Java:
    /// FACTORY_DEFAULT).
    [[nodiscard]] static constexpr PlotAppearance factoryDefault() noexcept
    {
        return {std::nullopt, LineStyle::SOLID};
    }

    [[nodiscard]] constexpr const std::optional<Color>& getColor() const noexcept
    {
        return m_color;
    }
    constexpr void setColor(std::optional<Color> color) noexcept { m_color = color; }

    [[nodiscard]] constexpr const std::optional<LineStyle>& getLineStyle() const noexcept
    {
        return m_lineStyle;
    }
    constexpr void setLineStyle(std::optional<LineStyle> lineStyle) noexcept
    {
        m_lineStyle = lineStyle;
    }

    /// Whether nothing is overridden: neither a color nor a line style.
    [[nodiscard]] constexpr bool isEmpty() const noexcept
    {
        return !m_color.has_value() && !m_lineStyle.has_value();
    }

    /// Java's equals().
    [[nodiscard]] constexpr bool operator==(const PlotAppearance&) const noexcept = default;

private:
    std::optional<Color>     m_color;
    std::optional<LineStyle> m_lineStyle;
};

}  // namespace QtRocket
