#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "QtRocket/util/Coordinate.h"

namespace QtRocket
{

/// A texture image applied to a component by an Appearance (OpenRocket's appearance.Decal):
/// where the image sits in the component's (u, v) texture space (offset, center, scale as the x
/// and y of Coordinates), its rotation in radians and what happens at its edges. Immutable.
///
/// Java's DecalImage (a file or document attachment with its bytes and change listeners) is not
/// ported here: a decal names its image, and the image bytes live in the document's decal
/// registry (document/, later). The registry fires the TEXTURE_CHANGE events that Java's
/// DecalImage listener fired on the components using the image.
class Decal
{
public:
    /// What happens at the edge of the image (Java: Decal.EdgeMode).
    enum class EdgeMode
    {
        REPEAT,
        MIRROR,
        CLAMP,
        STICKER,
    };

    /// Every edge mode, in declaration order.
    static constexpr std::array<EdgeMode, 4> kAllEdgeModes{EdgeMode::REPEAT, EdgeMode::MIRROR,
                                                           EdgeMode::CLAMP, EdgeMode::STICKER};

    /// @param offset    the image offset (x = u, y = v)
    /// @param center    the image center (x = u, y = v)
    /// @param scale     the image scale (x = u, y = v)
    /// @param rotation  the rotation, in radians
    /// @param imageName the name of the image (Java: DecalImage.getName(), the attachment name
    ///                  the .ork file refers to)
    /// @param mode      the edge behaviour
    Decal(const Coordinate& offset, const Coordinate& center, const Coordinate& scale,
          double rotation, std::string imageName, EdgeMode mode);

    [[nodiscard]] const Coordinate& getOffset() const noexcept { return m_offset; }
    [[nodiscard]] const Coordinate& getCenter() const noexcept { return m_center; }
    [[nodiscard]] const Coordinate& getScale() const noexcept { return m_scale; }
    /// The rotation, in radians.
    [[nodiscard]] double   getRotation() const noexcept { return m_rotation; }
    [[nodiscard]] EdgeMode getEdgeMode() const noexcept { return m_mode; }
    /// The name of the image (Java: getImage().getName()).
    [[nodiscard]] const std::string& getImageName() const noexcept { return m_imageName; }

    /// "Texture [offset=(u,v,0), center=..., scale=..., rotation=r, image=name]" with the
    /// coordinates in Coordinate::toString() form and the rotation as Java prints a double.
    [[nodiscard]] std::string toString() const;

    /// Field-by-field equality, the coordinates exactly (Java has none: Decal compares by
    /// identity). An addition for tests and undo comparisons.
    [[nodiscard]] bool operator==(const Decal& other) const noexcept;

private:
    Coordinate  m_offset;
    Coordinate  m_center;
    Coordinate  m_scale;
    double      m_rotation;
    std::string m_imageName;
    EdgeMode    m_mode;
};

/// The constant's name, e.g. "REPEAT"; the .ork edgemode="..." attribute is written with it.
[[nodiscard]] std::string_view edgeModeName(Decal::EdgeMode mode) noexcept;

/// The edge mode named exactly @p name (Java: EdgeMode.valueOf(), which the loader uses), or
/// nullopt (Java throws IllegalArgumentException).
[[nodiscard]] std::optional<Decal::EdgeMode> edgeModeFromName(std::string_view name) noexcept;

/// The translation key of the display name, e.g. "TextureWrap.Repeat".
[[nodiscard]] std::string_view displayKey(Decal::EdgeMode mode) noexcept;

/// The English display name (Java: toString()), e.g. "Repeat & Mirror".
[[nodiscard]] std::string_view displayName(Decal::EdgeMode mode) noexcept;

}  // namespace QtRocket
