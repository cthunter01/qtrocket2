#pragma once

#include <memory>

#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"

namespace QtRocket
{

class ComponentPreset;

/// A nose cone (OpenRocket's NoseCone): a Transition whose fore radius is 0. Flipped
/// (setFlipped()), it becomes a tail cone: its fore and aft dimensions trade places, so the base
/// is then at the front. The base accessors (getBaseRadius(), getShoulderLength(), ...) work in
/// both orientations and are the ones to use; the Transition accessors keep their fore/aft
/// meaning. A nose cone is never clipped.
///
/// Deviation from OpenRocket: the multi-edit config listeners are not ported (see
/// RocketComponent). The inside appearance is Transition's mixin (Java's NoseCone declares a
/// handler of its own that shadows Transition's; the behaviour is the same).
class NoseCone : public Transition
{
public:
    /// An ogive nose cone, 6 * kDefaultRadius long with base radius kDefaultRadius.
    NoseCone();

    /// A nose cone of @p type, @p length and base radius @p radius: thickness 0.002 m, not
    /// clipped, fore radius 0 and no fore shoulder, fixed aft radius.
    NoseCone(TransitionShape type, double length, double radius);

    [[nodiscard]] ComponentKind kind() const noexcept override { return ComponentKind::NOSE_CONE; }

    // ---- the base (independent of the orientation)

    /// The base radius: the fore radius when flipped, else the aft radius.
    [[nodiscard]] double getBaseRadius() const;
    void                 setBaseRadius(double radius);

    [[nodiscard]] bool isBaseRadiusAutomatic() const;
    void               setBaseRadiusAutomatic(bool autoRadius);

    [[nodiscard]] double getShoulderLength() const;
    void                 setShoulderLength(double length);

    [[nodiscard]] double getShoulderRadius() const;
    /// Sets the shoulder radius, clamped to the base radius with @p doClamping.
    void setShoulderRadius(double radius, bool doClamping);
    void setShoulderRadius(double radius);

    [[nodiscard]] double getShoulderThickness() const;
    void                 setShoulderThickness(double thickness);

    [[nodiscard]] bool isShoulderCapped() const;
    void               setShoulderCapped(bool capped);

    // ---- orientation

    /// Whether the nose cone is flipped into a tail cone.
    [[nodiscard]] bool isFlipped() const noexcept { return m_isFlipped; }

    /// Flips the nose cone into a tail cone (true) or back: with events bypassed, the base
    /// radius, its automatic flag (subject to the sanity check when @p sanityCheck), and the
    /// shoulder move to the other end, and the end they leave is reset (radius 0, not automatic,
    /// no shoulder). Then fires AEROMASS_CHANGE. Nothing happens for the current orientation.
    void setFlipped(bool flipped, bool sanityCheck);

    /// setFlipped(flipped, true).
    void setFlipped(bool flipped);

    // ---- Transition

    /// False: a nose cone is never clipped.
    [[nodiscard]] bool isClipped() const override;

    /// Does nothing.
    void setClipped(bool c) override;

protected:
    /// Unflips the nose cone (presets describe a normal one), loads Transition's properties and
    /// restores the orientation.
    void loadFromPreset(const ComponentPreset& preset, const PresetLoadOptions& options) override;

    [[nodiscard]] std::unique_ptr<RocketComponent> cloneShallow() const override;

private:
    /// Fore radius 0, not automatic, no fore shoulder.
    void resetForeRadius();

    /// Aft radius 0, not automatic, no aft shoulder.
    void resetAftRadius();

    bool m_isFlipped{false};
};

}  // namespace QtRocket
