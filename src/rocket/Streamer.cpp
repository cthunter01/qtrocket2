#include "QtRocket/rocket/Streamer.h"

#include <memory>

#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

namespace
{

/// OpenRocket's streamer drag coefficient estimate for a strip of @p stripLength made of a
/// material of surface density @p density, at most Streamer::kMaxComputedCd.
[[nodiscard]] double estimateCd(double density, double stripLength) noexcept
{
    double cd = 0.034 * ((density + 0.025) / 0.105) * (stripLength + 1) / stripLength;
    cd        = MathUtil::min(cd, Streamer::kMaxComputedCd);
    return cd;
}

}  // namespace

Streamer::Streamer()
{
    setDisplayOrderSide(10);  // Order for displaying the component in the 2D side view
    setDisplayOrderBack(8);   // Order for displaying the component in the 2D back view
}

std::unique_ptr<RocketComponent> Streamer::cloneShallow() const
{
    return std::make_unique<Streamer>(*this);
}

void Streamer::setStripLength(double stripLength)
{
    if (MathUtil::equals(m_stripLength, stripLength))
    {
        return;
    }
    m_stripLength = stripLength;
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

void Streamer::setStripWidth(double stripWidth)
{
    if (MathUtil::equals(m_stripWidth, stripWidth))
    {
        return;
    }
    m_stripWidth = stripWidth;

    clearPreset();
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

double Streamer::getAspectRatio() const
{
    if (m_stripWidth > 0.0001)
    {
        return m_stripLength / m_stripWidth;
    }
    return 1000;
}

void Streamer::setAspectRatio(double ratio)
{
    if (MathUtil::equals(getAspectRatio(), ratio))
    {
        return;
    }

    ratio             = MathUtil::javaMax(ratio, 0.01);
    const double area = getArea();
    m_stripWidth      = MathUtil::safeSqrt(area / ratio);
    m_stripLength     = ratio * m_stripWidth;
    clearPreset();
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

double Streamer::getArea() const
{
    return m_stripWidth * m_stripLength;
}

void Streamer::setArea(double area)
{
    if (MathUtil::equals(getArea(), area))
    {
        return;
    }

    const double ratio = MathUtil::javaMax(getAspectRatio(), 0.01);
    m_stripWidth       = MathUtil::safeSqrt(area / ratio);
    m_stripLength      = ratio * m_stripWidth;
    clearPreset();
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

void Streamer::loadFromPreset(const ComponentPreset& preset, const PresetLoadOptions& options)
{
    RecoveryDevice::loadFromPreset(preset, options);

    if (preset.has(ComponentPreset::kLength))
    {
        m_stripLength = preset.get(ComponentPreset::kLength);
    }
    if (preset.has(ComponentPreset::kWidth))
    {
        m_stripWidth = preset.get(ComponentPreset::kWidth);
    }
    // Set the CD when a preset is selected after a manual CD change.
    m_cd          = estimateCd(getMaterial().getDensity(), m_stripLength);
    m_cdAutomatic = true;

    // RocketComponent assigns the preset's LENGTH to the length; it is the strip width here.
    m_length = m_stripWidth;
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

double Streamer::getComponentCD(double /*mach*/) const
{
    return estimateCd(getMaterial().getDensity(), m_stripLength);
}

bool Streamer::allowsChildren() const
{
    return false;
}

bool Streamer::isCompatible(ComponentKind /*kind*/) const
{
    return false;
}

}  // namespace QtRocket
