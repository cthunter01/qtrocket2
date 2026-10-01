#pragma once

#include <memory>

#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/rocket/RocketComponent.h"

namespace QtRocket
{

class ComponentPreset;

/// A streamer (OpenRocket's Streamer): a strip of a surface material. Its automatic drag
/// coefficient is OpenRocket's estimate from the material's surface density and the strip
/// length (getComponentCD()). A new streamer is 0.5 m by 0.05 m and holds no children.
///
/// Not ported: the multi-edit config listeners (see RocketComponent).
class Streamer : public RecoveryDevice
{
public:
    using RocketComponent::isCompatible;

    /// DEFAULT_CD (unused by OpenRocket itself: a new streamer starts from Parachute's).
    static constexpr double kDefaultCd = 0.6;
    /// The upper limit of the estimated drag coefficient (MAX_COMPUTED_CD).
    static constexpr double kMaxComputedCd = 0.4;

    /// A strip 0.5 m long and 0.05 m wide.
    Streamer();

    [[nodiscard]] ComponentKind kind() const noexcept override { return ComponentKind::STREAMER; }

    [[nodiscard]] double getStripLength() const noexcept { return m_stripLength; }

    /// Sets the strip length; fires AEROMASS_CHANGE unless it equals (within MathUtil::equals())
    /// the current one. It does not clear the preset (OpenRocket's clearPreset() is commented
    /// out).
    void setStripLength(double stripLength);

    [[nodiscard]] double getStripWidth() const noexcept { return m_stripWidth; }

    /// Sets the strip width; unless it equals (within MathUtil::equals()) the current one,
    /// clears the preset and fires AEROMASS_CHANGE.
    void setStripWidth(double stripWidth);

    /// Length / width, or 1000 for a width of at most 0.0001.
    [[nodiscard]] double getAspectRatio() const;

    /// Keeps the area and changes the aspect ratio to @p ratio (at least 0.01): the width becomes
    /// sqrt(area / ratio) (MathUtil::safeSqrt()) and the length ratio * width. Unless the ratio
    /// equals (within MathUtil::equals()) the current one, clears the preset and fires
    /// AEROMASS_CHANGE.
    void setAspectRatio(double ratio);

    /// Width * length.
    [[nodiscard]] double getArea() const override;

    /// Keeps the aspect ratio (at least 0.01) and changes the area to @p area, as
    /// setAspectRatio(); unless the area equals (within MathUtil::equals()) the current one,
    /// clears the preset and fires AEROMASS_CHANGE.
    void setArea(double area);

    /// OpenRocket's estimate: 0.034 * ((density + 0.025) / 0.105) * (length + 1) / length with
    /// the material's surface density and the strip length, at most kMaxComputedCd
    /// (MathUtil::min()). @p mach is not used.
    [[nodiscard]] double getComponentCD(double mach) const override;

    /// Always false.
    [[nodiscard]] bool allowsChildren() const override;

    /// Always false.
    [[nodiscard]] bool isCompatible(ComponentKind kind) const override;

protected:
    [[nodiscard]] std::unique_ptr<RocketComponent> cloneShallow() const override;

    /// RecoveryDevice's values (the base one sets the length to the preset's LENGTH), then the
    /// strip length from LENGTH and the width from WIDTH, an automatic drag coefficient set to
    /// the estimate (getComponentCD()), and the packed length set to the strip width. Fires
    /// AEROMASS_CHANGE. Java overrides only the one-argument loadFromPreset(), so with
    /// parameters (which only a parachute's preset gets) it would skip this; see RocketComponent.
    void loadFromPreset(const ComponentPreset& preset, const PresetLoadOptions& options) override;

private:
    double m_stripLength{0.5};
    double m_stripWidth{0.05};
};

}  // namespace QtRocket
