#include "QtRocket/util/LineStyle.h"

#include <array>
#include <optional>
#include <span>
#include <string_view>

#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

constexpr std::array<double, 2> kSolidDashes{10.0, 0.0};
constexpr std::array<double, 2> kDashedDashes{6.0, 4.0};
constexpr std::array<double, 2> kDottedDashes{2.0, 3.0};
constexpr std::array<double, 4> kDashDotDashes{8.0, 3.0, 2.0, 3.0};

}  // namespace

std::span<const double> dashes(LineStyle style) noexcept
{
    switch (style)
    {
        case LineStyle::SOLID:
            return kSolidDashes;
        case LineStyle::DASHED:
            return kDashedDashes;
        case LineStyle::DOTTED:
            return kDottedDashes;
        case LineStyle::DASHDOT:
            return kDashDotDashes;
    }
    return kSolidDashes;
}

std::string_view displayKey(LineStyle style) noexcept
{
    switch (style)
    {
        case LineStyle::SOLID:
            return "LineStyle.Solid";
        case LineStyle::DASHED:
            return "LineStyle.Dashed";
        case LineStyle::DOTTED:
            return "LineStyle.Dotted";
        case LineStyle::DASHDOT:
            return "LineStyle.Dash-dotted";
    }
    return "LineStyle.Solid";
}

std::string_view toString(LineStyle style) noexcept
{
    switch (style)
    {
        case LineStyle::SOLID:
            return "solid";
        case LineStyle::DASHED:
            return "dashed";
        case LineStyle::DOTTED:
            return "dotted";
        case LineStyle::DASHDOT:
            return "dashdot";
    }
    return "solid";
}

std::optional<LineStyle> lineStyleFromString(std::string_view text) noexcept
{
    const std::string_view name = Strings::trim(text);
    for (const LineStyle style : kAllLineStyles)
    {
        if (Strings::equalsIgnoreAsciiCase(name, toString(style)))
        {
            return style;
        }
    }
    return std::nullopt;
}

}  // namespace QtRocket
