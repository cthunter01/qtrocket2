#pragma once

#include <array>
#include <optional>
#include <string_view>

namespace QtRocket
{

/// How a component's axial position is measured relative to its parent (OpenRocket's
/// position.AxialMethod). The position is the x of the component's front in its parent's frame;
/// the offset is what the user edits. For a component of length inner inside a parent of length
/// outer:
///
/// | method   | position from offset                | offset from position                |
/// |----------|-------------------------------------|-------------------------------------|
/// | ABSOLUTE | offset (from the rocket's tip)      | position                            |
/// | AFTER    | outer + offset (after its sibling)  | position - outer                    |
/// | TOP      | offset                              | position                            |
/// | MIDDLE   | offset + (outer - inner) / 2        | position + (inner - outer) / 2      |
/// | BOTTOM   | offset + (outer - inner)            | position + (inner - outer)          |
///
/// (RocketComponent resolves ABSOLUTE against the parent's absolute location and AFTER against
/// the previous sibling itself; the table is the arithmetic of the enum.)
///
/// Java's enum constants carry a translated description; here displayKey() gives the key and
/// displayName() the English text.
enum class AxialMethod
{
    ABSOLUTE,  ///< from the tip of the rocket
    AFTER,     ///< after the previous sibling
    TOP,       ///< from the top of the parent to the top of this component
    MIDDLE,    ///< from the middle of the parent to the middle of this component
    BOTTOM,    ///< from the bottom of the parent to the bottom of this component
};

/// Every method, in declaration order (AxialMethod.values()).
inline constexpr std::array<AxialMethod, 5> kAllAxialMethods{
    AxialMethod::ABSOLUTE, AxialMethod::AFTER, AxialMethod::TOP, AxialMethod::MIDDLE,
    AxialMethod::BOTTOM};

/// The methods offered to the user for an axial offset (AxialMethod.axialOffsetMethods): every
/// method except AFTER.
inline constexpr std::array<AxialMethod, 4> kAxialOffsetMethods{
    AxialMethod::ABSOLUTE, AxialMethod::TOP, AxialMethod::MIDDLE, AxialMethod::BOTTOM};

/// Whether an offset near zero should snap to zero (DistanceMethod.clampToZero()): false for
/// every axial method.
[[nodiscard]] constexpr bool clampToZero(AxialMethod /*method*/) noexcept
{
    return false;
}

/// The position (the component's front in its parent's frame) of a component with @p offset,
/// length @p innerLength, in a parent of length @p outerLength (AxialMethod.getAsPosition()).
[[nodiscard]] constexpr double getAsPosition(AxialMethod method, double offset, double innerLength,
                                             double outerLength) noexcept
{
    switch (method)
    {
        case AxialMethod::ABSOLUTE:
        case AxialMethod::TOP:
            return offset;
        case AxialMethod::AFTER:
            return outerLength + offset;
        case AxialMethod::MIDDLE:
            return offset + ((outerLength - innerLength) / 2);
        case AxialMethod::BOTTOM:
            return offset + (outerLength - innerLength);
    }
    return offset;
}

/// The offset of a component at @p position, the inverse of getAsPosition()
/// (AxialMethod.getAsOffset()).
[[nodiscard]] constexpr double getAsOffset(AxialMethod method, double position, double innerLength,
                                           double outerLength) noexcept
{
    switch (method)
    {
        case AxialMethod::ABSOLUTE:
        case AxialMethod::TOP:
            return position;
        case AxialMethod::AFTER:
            return position - outerLength;
        case AxialMethod::MIDDLE:
            return position + ((innerLength - outerLength) / 2);
        case AxialMethod::BOTTOM:
            return position + (innerLength - outerLength);
    }
    return position;
}

/// The constant's name, e.g. "ABSOLUTE" (Java: name()).
[[nodiscard]] std::string_view axialMethodName(AxialMethod method) noexcept;

/// The .ork spelling, the lower-cased name the savers write in method="..." ("absolute",
/// "after", "top", "middle", "bottom").
[[nodiscard]] std::string_view orkName(AxialMethod method) noexcept;

/// The method @p text names, matched as DocumentConfig.findEnum() does (trimmed, against the
/// lower-cased name without underscores); nullopt for anything else.
[[nodiscard]] std::optional<AxialMethod> axialMethodFromOrkName(std::string_view text);

/// The translation key of the description, e.g. "RocketComponent.Position.Method.Axial.TOP".
[[nodiscard]] std::string_view displayKey(AxialMethod method) noexcept;

/// The English description (Java: toString()), e.g. "Top of the parent component".
[[nodiscard]] std::string_view displayName(AxialMethod method) noexcept;

}  // namespace QtRocket
