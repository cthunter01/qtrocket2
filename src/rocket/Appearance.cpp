#include "QtRocket/rocket/Appearance.h"

#include <optional>
#include <string>
#include <utility>

#include "QtRocket/rocket/Decal.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

const Appearance& Appearance::missing()
{
    static const Appearance kMissing{Color{0, 0, 0}, 1.0};
    return kMissing;
}

Appearance::Appearance(const Color& paint, double shine, std::optional<Decal> texture,
                       bool opacityAffectsTexture)
  : m_paint(paint),
    m_shine(MathUtil::clamp(shine, 0.0, 1.0)),
    m_texture(std::move(texture)),
    m_opacityAffectsTexture(opacityAffectsTexture)
{
}

std::string Appearance::toString() const
{
    return "Appearance [paint=" + m_paint.toString() +
           ", shine=" + Strings::javaDoubleToString(m_shine) +
           ", texture=" + (m_texture ? m_texture->toString() : std::string{"null"}) +
           ", opacityAffectsTexture=" + (m_opacityAffectsTexture ? "true" : "false") + "]";
}

bool Appearance::operator==(const Appearance& other) const noexcept
{
    return m_paint == other.m_paint && m_shine == other.m_shine && m_texture == other.m_texture &&
           m_opacityAffectsTexture == other.m_opacityAffectsTexture;
}

}  // namespace QtRocket
