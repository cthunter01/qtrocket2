#include "QtRocket/rocket/NoseCone.h"

#include <memory>

#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"

namespace QtRocket
{

NoseCone::NoseCone() : NoseCone(TransitionShape::OGIVE, 6 * kDefaultRadius, kDefaultRadius) { }

NoseCone::NoseCone(TransitionShape type, double length, double radius)
{
    Transition::setShapeType(type);
    Transition::setThickness(0.002);
    Transition::setLength(length);
    Transition::setClipped(false);
    resetForeRadius();

    Transition::setAftRadiusAutomatic(false);
    Transition::setAftRadius(radius);

    setDisplayOrderSide(1);  // Order for displaying the component in the 2D side view
    setDisplayOrderBack(0);  // Order for displaying the component in the 2D back view
}

std::unique_ptr<RocketComponent> NoseCone::cloneShallow() const
{
    return std::make_unique<NoseCone>(*this);
}

// ================================================================================== base

double NoseCone::getBaseRadius() const
{
    return m_isFlipped ? getForeRadius() : getAftRadius();
}

void NoseCone::setBaseRadius(double radius)
{
    if (m_isFlipped)
    {
        setForeRadius(radius);
    }
    else
    {
        setAftRadius(radius);
    }
}

bool NoseCone::isBaseRadiusAutomatic() const
{
    return m_isFlipped ? isForeRadiusAutomatic() : isAftRadiusAutomatic();
}

void NoseCone::setBaseRadiusAutomatic(bool autoRadius)
{
    if (m_isFlipped)
    {
        setForeRadiusAutomatic(autoRadius);
    }
    else
    {
        setAftRadiusAutomatic(autoRadius);
    }
}

double NoseCone::getShoulderLength() const
{
    return m_isFlipped ? getForeShoulderLength() : getAftShoulderLength();
}

void NoseCone::setShoulderLength(double length)
{
    if (m_isFlipped)
    {
        setForeShoulderLength(length);
    }
    else
    {
        setAftShoulderLength(length);
    }
}

double NoseCone::getShoulderRadius() const
{
    return m_isFlipped ? getForeShoulderRadius() : getAftShoulderRadius();
}

void NoseCone::setShoulderRadius(double radius, bool doClamping)
{
    if (m_isFlipped)
    {
        setForeShoulderRadius(radius, doClamping);
    }
    else
    {
        setAftShoulderRadius(radius, doClamping);
    }
}

void NoseCone::setShoulderRadius(double radius)
{
    if (m_isFlipped)
    {
        setForeShoulderRadius(radius);
    }
    else
    {
        setAftShoulderRadius(radius);
    }
}

double NoseCone::getShoulderThickness() const
{
    return m_isFlipped ? getForeShoulderThickness() : getAftShoulderThickness();
}

void NoseCone::setShoulderThickness(double thickness)
{
    if (m_isFlipped)
    {
        setForeShoulderThickness(thickness);
    }
    else
    {
        setAftShoulderThickness(thickness);
    }
}

bool NoseCone::isShoulderCapped() const
{
    return m_isFlipped ? isForeShoulderCapped() : isAftShoulderCapped();
}

void NoseCone::setShoulderCapped(bool capped)
{
    if (m_isFlipped)
    {
        setForeShoulderCapped(capped);
    }
    else
    {
        setAftShoulderCapped(capped);
    }
}

// =========================================================================== orientation

void NoseCone::setFlipped(bool flipped, bool sanityCheck)
{
    if (m_isFlipped == flipped)
    {
        return;
    }

    const bool previousByPass = isBypassComponentChangeEvent();
    setBypassChangeEvent(true);
    if (flipped)
    {
        setForeRadius(getAftRadius());
        setForeRadiusAutomatic(isAftRadiusAutomatic(), sanityCheck);
        setForeShoulderLength(getAftShoulderLength());
        setForeShoulderRadius(getAftShoulderRadius());
        setForeShoulderThickness(getAftShoulderThickness());
        setForeShoulderCapped(isAftShoulderCapped());

        resetAftRadius();
    }
    else
    {
        setAftRadius(getForeRadius());
        setAftRadiusAutomatic(isForeRadiusAutomatic(), sanityCheck);
        setAftShoulderLength(getForeShoulderLength());
        setAftShoulderRadius(getForeShoulderRadius());
        setAftShoulderThickness(getForeShoulderThickness());
        setAftShoulderCapped(isForeShoulderCapped());

        resetForeRadius();
    }
    setBypassChangeEvent(previousByPass);

    m_isFlipped = flipped;
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

void NoseCone::setFlipped(bool flipped)
{
    setFlipped(flipped, true);
}

void NoseCone::resetForeRadius()
{
    setForeRadius(0);
    setForeRadiusAutomatic(false);
    setForeShoulderLength(0);
    setForeShoulderRadius(0);
    setForeShoulderThickness(0);
    setForeShoulderCapped(false);
}

void NoseCone::resetAftRadius()
{
    setAftRadius(0);
    setAftRadiusAutomatic(false);
    setAftShoulderLength(0);
    setAftShoulderRadius(0);
    setAftShoulderThickness(0);
    setAftShoulderCapped(false);
}

bool NoseCone::isClipped() const
{
    return false;
}

void NoseCone::setClipped(bool /*c*/)
{
    // No-op
}

// ======================================================================= RocketComponent

void NoseCone::loadFromPreset(const ComponentPreset& preset, const PresetLoadOptions& options)
{
    // We first need to unflip, because the preset loading always applies settings for a normal
    // nose cone (e.g. aft diameter)
    const bool flipped = m_isFlipped;
    setFlipped(false);
    Transition::loadFromPreset(preset, options);

    setFlipped(flipped);
}

}  // namespace QtRocket
