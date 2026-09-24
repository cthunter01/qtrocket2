#include "QtRocket/rocket/AppearanceBuilder.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <string>
#include <utility>

#include "QtRocket/rocket/Appearance.h"
#include "QtRocket/rocket/Decal.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

AppearanceBuilder::AppearanceBuilder()
{
    resetToDefaults();
}

AppearanceBuilder::AppearanceBuilder(const std::optional<Appearance>& appearance)
{
    setAppearance(appearance);
}

void AppearanceBuilder::resetToDefaults()
{
    m_paint                 = Color{187, 187, 187};
    m_shine                 = 0.3;
    m_offsetU               = 0;
    m_offsetV               = 0;
    m_centerU               = 0;
    m_centerV               = 0;
    m_scaleU                = 1;
    m_scaleV                = 1;
    m_rotation              = 0;
    m_image                 = std::nullopt;
    m_edgeMode              = Decal::EdgeMode::REPEAT;
    m_opacityAffectsTexture = false;
    fireChangeEvent();
}

void AppearanceBuilder::setAppearance(const std::optional<Appearance>& appearance)
{
    batch([this, &appearance] {
        resetToDefaults();
        if (appearance)
        {
            setPaint(appearance->getPaint());
            setShine(appearance->getShine());
            setDecal(appearance->getTexture());
            setOpacityAffectsTexture(appearance->isOpacityAffectsTexture());
        }
    });
}

void AppearanceBuilder::setDecal(const std::optional<Decal>& decal)
{
    if (decal)
    {
        setOffset(decal->getOffset().x, decal->getOffset().y);
        setCenter(decal->getCenter().x, decal->getCenter().y);
        setScaleUV(decal->getScale().x, decal->getScale().y);
        setRotation(decal->getRotation());
        setEdgeMode(decal->getEdgeMode());
        setImage(decal->getImageName());
    }
    fireChangeEvent();
}

Appearance AppearanceBuilder::getAppearance() const
{
    std::optional<Decal> texture;
    if (m_image)
    {
        texture.emplace(Coordinate{m_offsetU, m_offsetV}, Coordinate{m_centerU, m_centerV},
                        Coordinate{m_scaleU, m_scaleV}, m_rotation, *m_image, m_edgeMode);
    }
    return Appearance{m_paint, m_shine, std::move(texture), m_opacityAffectsTexture};
}

void AppearanceBuilder::setPaint(const Color& paint)
{
    m_paint = paint;
    fireChangeEvent();
}

void AppearanceBuilder::setShine(double shine)
{
    m_shine = shine;
    fireChangeEvent();
}

double AppearanceBuilder::getOpacity() const noexcept
{
    return static_cast<double>(m_paint.alpha()) / 255;
}

void AppearanceBuilder::setOpacity(double opacity)
{
    // Java: Math.max(0, Math.min(1, opacity)), which keeps a NaN (and (int) NaN is 0).
    const double clamped = std::isnan(opacity) ? opacity : std::max(0.0, std::min(1.0, opacity));
    // A new colour rather than a changed alpha, as Java does (it keeps undo working there).
    setPaint(Color{m_paint.red(), m_paint.green(), m_paint.blue(),
                   MathUtil::javaIntCast(clamped * 255)});
}

void AppearanceBuilder::setOpacityAffectsTexture(bool opacityAffectsTexture)
{
    m_opacityAffectsTexture = opacityAffectsTexture;
    fireChangeEvent();
}

void AppearanceBuilder::setOffsetU(double offsetU)
{
    m_offsetU = offsetU;
    fireChangeEvent();
}

void AppearanceBuilder::setOffsetV(double offsetV)
{
    m_offsetV = offsetV;
    fireChangeEvent();
}

void AppearanceBuilder::setOffset(double u, double v)
{
    setOffsetU(u);
    setOffsetV(v);
}

void AppearanceBuilder::setCenterU(double centerU)
{
    m_centerU = centerU;
    fireChangeEvent();
}

void AppearanceBuilder::setCenterV(double centerV)
{
    m_centerV = centerV;
    fireChangeEvent();
}

void AppearanceBuilder::setCenter(double u, double v)
{
    setCenterU(u);
    setCenterV(v);
}

void AppearanceBuilder::setScaleU(double scaleU)
{
    m_scaleU = scaleU;
    fireChangeEvent();
}

void AppearanceBuilder::setScaleV(double scaleV)
{
    m_scaleV = scaleV;
    fireChangeEvent();
}

void AppearanceBuilder::setScaleUV(double u, double v)
{
    setScaleU(u);
    setScaleV(v);
}

void AppearanceBuilder::setScaleX(double scaleX)
{
    setScaleU(1.0 / scaleX);
}

void AppearanceBuilder::setScaleY(double scaleY)
{
    setScaleV(1.0 / scaleY);
}

void AppearanceBuilder::setRotation(double rotation)
{
    m_rotation = rotation;
    fireChangeEvent();
}

void AppearanceBuilder::setImage(std::optional<std::string> image)
{
    m_image = std::move(image);
    fireChangeEvent();
}

void AppearanceBuilder::setEdgeMode(Decal::EdgeMode edgeMode)
{
    m_edgeMode = edgeMode;
    fireChangeEvent();
}

void AppearanceBuilder::batch(const std::function<void()>& changes)
{
    m_batch = true;
    changes();
    m_batch = false;
    fireChangeEvent();
}

void AppearanceBuilder::fireChangeEvent()
{
    if (!m_batch)
    {
        m_changed.emit();
    }
}

}  // namespace QtRocket
