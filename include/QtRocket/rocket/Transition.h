#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/InsideColorComponent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket
{

class ComponentPreset;

/// A transition between two body diameters (OpenRocket's Transition), and the base of NoseCone:
/// a profile of one of the six TransitionShapes between a fore and an aft radius, either of
/// which may be automatic (taken from the previous / next symmetric component), with an optional
/// shoulder (a hollow cylinder, optionally capped) at each end. The shape's parameter is clamped
/// to its range; a clippable shape may be clipped (the profile of a longer component, cut at the
/// fore radius), and the clip length is cached until the next change event.
///
/// The automatic radii are computed when read and stored, as in Java (the stored value is what
/// the radius setters compare with and what they clamp the thickness against): getForeRadius()
/// and getAftRadius() are const and refresh mutable members.
///
/// Shoulders are added to the volume, CG and unit inertias by calculateProperties() (Java's
/// formulas, including OpenRocket's use of the fore shoulder thickness for the length of the aft
/// cap in its longitudinal inertia); the wetted and planform areas and the full volume are the
/// transition's alone.
///
/// Deviations from OpenRocket: setShapeType() takes an enum, which cannot be null (Java throws
/// IllegalArgumentException for null); the multi-edit config listeners are not ported (see
/// RocketComponent).
// NOLINTNEXTLINE(misc-multiple-inheritance): the InsideColorComponent mixin carries data
class Transition : public SymmetricComponent, public InsideColorComponent
{
public:
    using RocketComponent::isCompatible;
    using SymmetricComponent::getInnerRadius;
    using SymmetricComponent::getRadius;

    /// Shoulders and other features shorter than this (1 mm, MINFEATURE) are ignored.
    static constexpr double kMinFeature = kTransitionMinFeature;

    /// A conical transition, kDefaultRadius * 3 long, both radii automatic (kDefaultRadius until
    /// a neighbour gives them), clipped, no shoulders.
    Transition();

    [[nodiscard]] ComponentKind kind() const noexcept override { return ComponentKind::TRANSITION; }

    // ---- length

    /// Clears the preset, then SymmetricComponent::setLength(), when the length changes.
    void setLength(double length) override;

    // ---- fore radius

    /// The fore radius; when automatic, refreshed first from getAutoForeRadius().
    [[nodiscard]] double getForeRadius() const override;

    /// Sets a fixed fore radius (negative values become 0) and switches the automatic fore radius
    /// off; with @p doClamping a thickness beyond both radii becomes the larger radius. Then clears
    /// the preset, fires AEROMASS_CHANGE and re-applies the fore shoulder radius (clamped with
    /// @p doClamping). Nothing happens for the current radius when it is not automatic.
    void setForeRadius(double radius, bool doClamping);

    /// setForeRadius(radius, true).
    void setForeRadius(double radius);

    [[nodiscard]] bool isForeRadiusAutomatic() const override;

    /// Switches the automatic fore radius on or off; with @p sanityCheck it is switched on only
    /// when canUsePreviousCompAutomatic(). Clears the preset and fires AEROMASS_CHANGE when it
    /// changes.
    void setForeRadiusAutomatic(bool autoRadius, bool sanityCheck);

    /// setForeRadiusAutomatic(autoRadius, false).
    void setForeRadiusAutomatic(bool autoRadius);

    // ---- aft radius

    /// The aft radius; when automatic, refreshed first from getAutoAftRadius().
    [[nodiscard]] double getAftRadius() const override;

    /// As setForeRadius() for the aft end, except that the aft shoulder radius is always
    /// re-applied with clamping (as in Java).
    void setAftRadius(double radius, bool doClamping);

    /// setAftRadius(radius, true).
    void setAftRadius(double radius);

    [[nodiscard]] bool isAftRadiusAutomatic() const override;

    /// As setForeRadiusAutomatic() for the aft end (canUseNextCompAutomatic()).
    void setAftRadiusAutomatic(bool autoRadius, bool sanityCheck);

    /// setAftRadiusAutomatic(autoRadius, false).
    void setAftRadiusAutomatic(bool autoRadius);

    // ---- radius automatics

    /// -1 when the aft radius is automatic, else the aft radius.
    [[nodiscard]] double getFrontAutoRadius() const override;

    /// -1 when the fore radius is automatic, else the fore radius.
    [[nodiscard]] double getRearAutoRadius() const override;

    /// isForeRadiusAutomatic().
    [[nodiscard]] bool usesPreviousCompAutomatic() const override;

    /// isAftRadiusAutomatic().
    [[nodiscard]] bool usesNextCompAutomatic() const override;

    /// Whether the fore radius can be automatic: there is a previous symmetric component and it
    /// does not take its own radius from this one.
    [[nodiscard]] bool canUsePreviousCompAutomatic() const;

    /// Whether the aft radius can be automatic: there is a next symmetric component and it does
    /// not take its own radius from this one.
    [[nodiscard]] bool canUseNextCompAutomatic() const;

    // ---- shape

    [[nodiscard]] TransitionShape getShapeType() const noexcept { return m_type; }

    /// Sets the shape, with clipping as the shape allows (isClippable()) and the shape's default
    /// parameter; clears the preset and fires AEROMASS_CHANGE, when it changes.
    void setShapeType(TransitionShape type);

    [[nodiscard]] double getShapeParameter() const noexcept { return m_shapeParameter; }

    /// Sets the shape parameter, clamped to the shape's range; clears the preset and fires
    /// AEROMASS_CHANGE, unless it is exactly the current parameter.
    void setShapeParameter(double n);

    /// Whether the profile is clipped: never for a shape that is not clippable.
    [[nodiscard]] virtual bool isClipped() const;

    /// Sets the clipping flag; fires AEROMASS_CHANGE when it changes.
    virtual void setClipped(bool c);

    /// Whether the shape can be clipped (isClippable(getShapeType())).
    [[nodiscard]] bool isClippedEnabled() const noexcept;

    [[nodiscard]] double getShapeParameterMin() const noexcept;
    [[nodiscard]] double getShapeParameterMax() const noexcept;

    // ---- shoulders

    [[nodiscard]] double getForeShoulderRadius() const noexcept { return m_foreShoulderRadius; }

    /// Sets the fore shoulder radius (with @p doClamping limited to the fore radius, and the
    /// shoulder thickness then limited to the new radius); clears the preset and fires
    /// MASS_CHANGE, unless it equals the current radius (MathUtil::equals).
    void setForeShoulderRadius(double foreShoulderRadius, bool doClamping);

    /// setForeShoulderRadius(radius, true).
    void setForeShoulderRadius(double foreShoulderRadius);

    [[nodiscard]] double getForeShoulderThickness() const noexcept
    {
        return m_foreShoulderThickness;
    }

    /// Sets the fore shoulder thickness; fires MASS_CHANGE unless it equals the current one
    /// (MathUtil::equals). It keeps the preset.
    void setForeShoulderThickness(double foreShoulderThickness);

    [[nodiscard]] double getForeShoulderLength() const noexcept { return m_foreShoulderLength; }

    /// Sets the fore shoulder length (it keeps the preset), unless it equals the current one
    /// (MathUtil::equals). A length of 0 also zeroes the shoulder radius and thickness and
    /// uncaps it; the first non-zero length gives a zero shoulder radius the automatic fore
    /// radius less the automatic fore thickness (the previous component's, else this one's) and
    /// a zero shoulder thickness that thickness, and caps the shoulder. Fires MASS_CHANGE.
    void setForeShoulderLength(double foreShoulderLength);

    [[nodiscard]] bool isForeShoulderCapped() const noexcept { return m_foreShoulderCapped; }

    /// Sets whether the fore shoulder is capped; fires MASS_CHANGE when it changes.
    void setForeShoulderCapped(bool capped);

    [[nodiscard]] double getAftShoulderRadius() const noexcept { return m_aftShoulderRadius; }

    /// As setForeShoulderRadius() for the aft shoulder (limited to the aft radius).
    void setAftShoulderRadius(double aftShoulderRadius, bool doClamping);

    /// setAftShoulderRadius(radius, true).
    void setAftShoulderRadius(double aftShoulderRadius);

    [[nodiscard]] double getAftShoulderThickness() const noexcept { return m_aftShoulderThickness; }

    /// As setForeShoulderThickness() for the aft shoulder.
    void setAftShoulderThickness(double aftShoulderThickness);

    [[nodiscard]] double getAftShoulderLength() const noexcept { return m_aftShoulderLength; }

    /// As setForeShoulderLength() for the aft shoulder (the next component's radius and
    /// thickness).
    void setAftShoulderLength(double aftShoulderLength);

    [[nodiscard]] bool isAftShoulderCapped() const noexcept { return m_aftShoulderCapped; }

    /// Sets whether the aft shoulder is capped; fires MASS_CHANGE when it changes.
    void setAftShoulderCapped(bool capped);

    // ---- the profile

    /// The radius at @p x: the fore radius ahead of the transition, the aft radius from its end
    /// on, else the shape's profile between the two radii (getTransitionRadius(), with the clip
    /// length cached).
    [[nodiscard]] double getRadius(double x) const override;

    /// getRadius(x) less the wall thickness, not below 0 (Java's Math.max).
    [[nodiscard]] double getInnerRadius(double x) const override;

    /// SymmetricComponent's bounds, plus the far end of each shoulder longer than kMinFeature.
    [[nodiscard]] std::vector<Coordinate> getComponentBounds() const override;

    // ---- children

    /// A transition accepts internal components and freeform fin sets.
    [[nodiscard]] bool isCompatible(ComponentKind kind) const override;

protected:
    /// The radius the previous symmetric component offers (its getFrontAutoRadius(), which may be
    /// -1), or kDefaultRadius without one.
    [[nodiscard]] double getAutoForeRadius() const;

    /// The previous symmetric component's thickness, or this one's without one.
    [[nodiscard]] double getAutoForeThickness() const;

    /// The radius the next symmetric component offers (its getRearAutoRadius(), which may be -1),
    /// or kDefaultRadius without one.
    [[nodiscard]] double getAutoAftRadius() const;

    /// The next symmetric component's thickness, or this one's without one.
    [[nodiscard]] double getAutoAftThickness() const;

    /// SymmetricComponent's integration, then, when either shoulder is longer than kMinFeature,
    /// the shoulders and their caps added to the volume, CG and unit inertias.
    void calculateProperties() const override;

    /// SymmetricComponent's invalidation, and the clip length is forgotten.
    void componentChanged(const ComponentChangeEvent& event) override;

    /// SymmetricComponent's properties, then the preset's shape (with its clipping and default
    /// parameter), aft diameter (fixed), aft shoulder length and diameter (and thickness when the
    /// preset is filled), fore diameter, fore shoulder length and diameter through the setters;
    /// with a THICKNESS the shoulder thicknesses become the wall thickness. Fires
    /// AEROMASS_CHANGE.
    void loadFromPreset(const ComponentPreset& preset, const PresetLoadOptions& options) override;

    [[nodiscard]] std::unique_ptr<RocketComponent> cloneShallow() const override;

    // The radii as last set or computed (Java: protected; read them through getForeRadius() and
    // getAftRadius(), whose meaning a flipped nose cone changes).
    mutable double m_foreRadius{kDefaultRadius};
    mutable double m_aftRadius{kDefaultRadius};
    bool           m_autoForeRadius{true};
    bool           m_autoAftRadius{true};

private:
    TransitionShape               m_type{TransitionShape::CONICAL};
    double                        m_shapeParameter{0};
    bool                          m_clipped{true};  // not to be read: use isClipped()
    double                        m_foreShoulderRadius{0};
    double                        m_foreShoulderThickness{0};
    double                        m_foreShoulderLength{0};
    bool                          m_foreShoulderCapped{false};
    double                        m_aftShoulderRadius{0};
    double                        m_aftShoulderThickness{0};
    double                        m_aftShoulderLength{0};
    bool                          m_aftShoulderCapped{false};
    mutable std::optional<double> m_clipLength;  // Java: clipLength, -1 until solved
};

}  // namespace QtRocket
