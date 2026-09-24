#include "QtRocket/rocket/Decal.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

Decal::Decal(const Coordinate& offset, const Coordinate& center, const Coordinate& scale,
             double rotation, std::string imageName, EdgeMode mode)
  : m_offset(offset),
    m_center(center),
    m_scale(scale),
    m_rotation(rotation),
    m_imageName(std::move(imageName)),
    m_mode(mode)
{
}

std::string Decal::toString() const
{
    return "Texture [offset=" + m_offset.toString() + ", center=" + m_center.toString() +
           ", scale=" + m_scale.toString() +
           ", rotation=" + Strings::javaDoubleToString(m_rotation) + ", image=" + m_imageName + "]";
}

bool Decal::operator==(const Decal& other) const noexcept
{
    return m_offset.exactlyEquals(other.m_offset) && m_center.exactlyEquals(other.m_center) &&
           m_scale.exactlyEquals(other.m_scale) && m_rotation == other.m_rotation &&
           m_imageName == other.m_imageName && m_mode == other.m_mode;
}

std::string_view edgeModeName(Decal::EdgeMode mode) noexcept
{
    switch (mode)
    {
        case Decal::EdgeMode::REPEAT:
            return "REPEAT";
        case Decal::EdgeMode::MIRROR:
            return "MIRROR";
        case Decal::EdgeMode::CLAMP:
            return "CLAMP";
        case Decal::EdgeMode::STICKER:
            return "STICKER";
    }
    return "REPEAT";
}

std::optional<Decal::EdgeMode> edgeModeFromName(std::string_view name) noexcept
{
    for (const Decal::EdgeMode mode : Decal::kAllEdgeModes)
    {
        if (edgeModeName(mode) == name)
        {
            return mode;
        }
    }
    return std::nullopt;
}

std::string_view displayKey(Decal::EdgeMode mode) noexcept
{
    switch (mode)
    {
        case Decal::EdgeMode::REPEAT:
            return "TextureWrap.Repeat";
        case Decal::EdgeMode::MIRROR:
            return "TextureWrap.Mirror";
        case Decal::EdgeMode::CLAMP:
            return "TextureWrap.Clamp";
        case Decal::EdgeMode::STICKER:
            return "TextureWrap.Sticker";
    }
    return "TextureWrap.Repeat";
}

std::string_view displayName(Decal::EdgeMode mode) noexcept
{
    switch (mode)
    {
        case Decal::EdgeMode::REPEAT:
            return "Repeat";
        case Decal::EdgeMode::MIRROR:
            return "Repeat & Mirror";
        case Decal::EdgeMode::CLAMP:
            return "Clamp Edge Pixels";
        case Decal::EdgeMode::STICKER:
            return "Sticker";
    }
    return "Repeat";
}

}  // namespace QtRocket
