#pragma once

#include <memory>
#include <numbers>
#include <string>
#include <vector>

#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/BoxBounded.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/ExternalComponent.h"
#include "QtRocket/rocket/LineInstanceable.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AnglePositionable.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/AxialPositionable.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket
{

class ComponentPreset;
class MaterialStorage;
class Preferences;

/// A rail button (OpenRocket's RailButton): a spool on the outside of a body tube that runs in
/// the slot of a launch rail. Seen from the side:
///
///        > outer dia  <
///        |            |              v
///   ^     [[[[[[]]]]]]              flange height
/// total     >||||||<=  inner dia     ^
/// height     ||||||            v
///   v     [[[[[[]]]]]]        base height (standoff)
///      ==================      ^
///          (body)
///
/// plus an optional screw head on top, a half ellipsoid of the outer diameter that is the screw
/// height high (above the total height). The reference point is the centre of the button's
/// bottom, and the length is 0. The button sits at an angle around its parent's axis (pi by
/// default) and can be repeated along the axis (LineInstanceable).
///
/// The heights constrain each other through the setters: the base height is at most the total
/// less the flange height, the flange height at most the total less the base height, and the
/// total height at least their sum; the inner diameter is at most the outer one. Each of these
/// setters clears the preset and fires AEROMASS_CHANGE, whether or not the value changes (as
/// Java). The constructors and loadFromPreset() store their values without those checks, as in
/// Java.
///
/// Its distance from the parent's axis follows the body: componentChanged() stores the outer
/// radius of the nearest BodyTube among the ancestors (0 without one), and getInstanceOffsets()
/// places the instances there; see LaunchLug for what that means before an event has reached the
/// button. The CG reads the parent itself (a symmetric component, else radius 0) at x = the
/// button's axial offset.
///
/// A rail button whose axial method is AFTER follows its previous sibling, as Java's does:
/// RailButton does not override isAfter() (LaunchLug and TubeFinSet do, to false), so
/// RocketComponent::setAfter() places it, on the event setAxialMethod() fires and on every later
/// one: its reference point at the end of the sibling before it (at the top of the parent when
/// it is the first child) and an axial offset of 0, which setAxialOffset() cannot change.
///
/// A rail button takes RAIL_BUTTON presets and holds no children.
///
/// Kept as OpenRocket has them: both unit inertias are 0; the instance bounding box spans the
/// total height plus the screw height on both sides of the reference point; setAngleOffset()
/// fires AERODYNAMIC_CHANGE only, although the CG moves; the volume and the CG weigh the screw
/// head with Java's float constant 2.0f / 3.
///
/// Deviations from OpenRocket:
/// - The default material is the built-in "Delrin" (defaultMaterial()), taken from the built-in
///   material table. Java looks "Delrin" up in the global material databases, which hold the
///   user's materials too. applyDefaultMaterial(), which completes Java's constructor for the
///   other external components, ends with the Delrin of the given storage here, as Java's
///   constructor sets it after the preferences' default: a new rail button is made of Delrin
///   whatever the preferences say.
/// - getComponentCG() throws BugError where Java throws BugException (a CG above the button).
///   The setters' constraints rule that out; only a preset whose dimensions contradict each
///   other (they are stored unchecked) gets there.
/// - The multi-edit config listeners are not ported (see RocketComponent).
class RailButton : public ExternalComponent,
                   public virtual AnglePositionable,
                   public virtual AxialPositionable,
                   public virtual BoxBounded,
                   public virtual LineInstanceable
{
public:
    using RocketComponent::getAxialOffset;
    using RocketComponent::isCompatible;

    /// The material of a new rail button: the built-in bulk "Delrin" (Java:
    /// Databases.findMaterial(BULK, "Delrin")), made once from the built-in table. It hides
    /// ExternalComponent::defaultMaterial() ("Cardboard"), which Java's RailButton constructors
    /// replace, so that RailButton::defaultMaterial() is what a new rail button is made of.
    [[nodiscard]] static const Material& defaultMaterial();

    /// A button with outer diameter and total height 9.7 mm, inner diameter 8 mm, flange and
    /// base heights 2 mm and no screw; positioned at the MIDDLE of its parent, at the angle pi;
    /// one instance, with an instance separation of six outer diameters; made of Delrin.
    RailButton();

    /// The default button with setOuterDiameter(@p od) and then setTotalHeight(@p ht) applied:
    /// the inner diameter becomes at most @p od and the total height at least 4 mm (the default
    /// base and flange). The instance separation stays that of the default button.
    RailButton(double od, double ht);

    /// A button of outer diameter @p od, inner diameter @p id, total height @p ht and flange
    /// height @p flangeHeight, stored as given, with setBaseHeight(@p baseHeight) applied (0 ...
    /// @p ht - @p flangeHeight) and no screw; the instance separation is two outer diameters.
    RailButton(double od, double id, double ht, double flangeHeight, double baseHeight);

    [[nodiscard]] ComponentKind kind() const noexcept override
    {
        return ComponentKind::RAIL_BUTTON;
    }

    // ---- the dimensions

    [[nodiscard]] double getBaseHeight() const noexcept { return m_baseHeight; }
    [[nodiscard]] double getOuterDiameter() const noexcept { return m_outerDiameter; }
    [[nodiscard]] double getInnerDiameter() const noexcept { return m_innerDiameter; }

    /// The height of the inner part: the total height less the flange and base heights.
    [[nodiscard]] double getInnerHeight() const noexcept
    {
        return m_totalHeight - m_flangeHeight - m_baseHeight;
    }

    [[nodiscard]] double getTotalHeight() const noexcept { return m_totalHeight; }
    [[nodiscard]] double getFlangeHeight() const noexcept { return m_flangeHeight; }
    [[nodiscard]] double getScrewHeight() const noexcept { return m_screwHeight; }

    /// Sets the base height, clamped to 0 ... getMaxBaseHeight().
    void setBaseHeight(double newBaseHeight);

    /// Sets the flange height, clamped to 0 ... getMaxFlangeHeight().
    void setFlangeHeight(double newFlangeHeight);

    /// Sets the total height, at least getMinTotalHeight().
    void setTotalHeight(double newHeight);

    /// The total height less the flange height.
    [[nodiscard]] double getMaxBaseHeight() const noexcept
    {
        return m_totalHeight - m_flangeHeight;
    }

    /// The total height less the base height.
    [[nodiscard]] double getMaxFlangeHeight() const noexcept
    {
        return m_totalHeight - m_baseHeight;
    }

    /// The base height plus the flange height.
    [[nodiscard]] double getMinTotalHeight() const noexcept
    {
        return m_baseHeight + m_flangeHeight;
    }

    /// Sets the height of the screw head, at least 0.
    void setScrewHeight(double height);

    /// Sets the inner diameter, at most the outer diameter.
    void setInnerDiameter(double newId);

    /// Sets the outer diameter (as given) and reduces the inner diameter to it when it is
    /// larger; fires twice, as Java (once from setInnerDiameter()).
    void setOuterDiameter(double newOd);

    // ---- AnglePositionable

    /// The angle of the button around the parent's axis, in radians (-pi ... pi; 0 is the
    /// positive y axis).
    [[nodiscard]] double getAngleOffset() const override;

    /// Sets the angle, clamped (not reduced) to -pi ... pi; fires AERODYNAMIC_CHANGE unless it
    /// equals the current one (MathUtil::equals).
    void setAngleOffset(double angle) override;

    /// Always RELATIVE.
    [[nodiscard]] AngleMethod getAngleMethod() const override;

    /// Does nothing, as in Java.
    void setAngleMethod(AngleMethod newMethod) override;

    // ---- AxialPositionable (RocketComponent's, see AxialPositionable)

    [[nodiscard]] AxialMethod getAxialMethod() const override
    {
        return RocketComponent::getAxialMethod();
    }

    /// Changes how the position is described, keeping the position
    /// (RocketComponent::setAxialMethod()), and fires NONFUNCTIONAL_CHANGE, also when the method
    /// does not change (as Java).
    void setAxialMethod(AxialMethod newMethod) override;

    /// The stored offset for getAxialMethod().
    [[nodiscard]] double getAxialOffset() const override;

    /// Sets the offset for the current method and fires AEROMASS_CHANGE.
    void setAxialOffset(double newOffset) override;

    // ---- LineInstanceable

    [[nodiscard]] double getInstanceSeparation() const override;

    /// Sets the distance between neighbouring instances (front to front, along +x); fires
    /// AERODYNAMIC_CHANGE unless it equals the current one (MathUtil::equals).
    void setInstanceSeparation(double separation) override;

    // ---- Instanceable

    [[nodiscard]] int getInstanceCount() const override;

    /// Sets the number of instances and fires AEROMASS_CHANGE; nothing happens for the current
    /// count or one that is not positive.
    void setInstanceCount(int newCount) override;

    [[nodiscard]] std::vector<Coordinate> getInstanceLocations() const override;

    /// (i * separation, cos(angle) * d, sin(angle) * d) for each instance i, with d the body
    /// tube radius componentChanged() stored (see the class comment).
    [[nodiscard]] std::vector<Coordinate> getInstanceOffsets() const override;

    /// "<count>-Line".
    [[nodiscard]] std::string getPatternName() const override;

    // ---- BoxBounded

    /// The box around (0, +-(total + screw height), 0) and (+-r, 0, +-r), with r half the outer
    /// diameter.
    [[nodiscard]] BoundingBox getInstanceBoundingBox() const override;

    // ---- RocketComponent / ExternalComponent

    /// Java's constructor after ExternalComponent's: the preferences' default
    /// (ExternalComponent::applyDefaultMaterial()) is replaced by "Delrin" looked up among the
    /// bulk materials of @p storage (Databases.findMaterial(BULK, "Delrin")), or
    /// defaultMaterial() when @p storage has none.
    void applyDefaultMaterial(const Preferences&     preferences,
                              const MaterialStorage& storage) override;

    /// The flange, the inner part and the base as cylinders plus the screw head as a half
    /// ellipsoid (pi r^2 h each; 2/3 pi r^2 h for the screw head), times the instance count.
    [[nodiscard]] double getComponentVolume() const override;

    /// The middle of the row of instances, x = separation * (count - 1) / 2, at the button's
    /// angle, at the parent's radius (when it is a symmetric component, read at x = the button's
    /// axial offset; else 0) plus the height of the button's own CG above its bottom. The weight
    /// is the component mass.
    /// @throws BugError when that height comes out above the total plus the screw height (Java:
    ///         BugException), which the setters' constraints rule out.
    [[nodiscard]] Coordinate getComponentCG() const override;

    /// 0, as in Java.
    [[nodiscard]] double getLongitudinalUnitInertia() const override;

    /// 0, as in Java.
    [[nodiscard]] double getRotationalUnitInertia() const override;

    /// The eight corners (+-r, 0 or total height, +-r), with r half the outer diameter.
    [[nodiscard]] std::vector<Coordinate> getComponentBounds() const override;

    /// Always false.
    [[nodiscard]] bool allowsChildren() const override;

    /// Always false: nothing is attached to a rail button.
    [[nodiscard]] bool isCompatible(ComponentKind kind) const override;

protected:
    using RocketComponent::setAxialOffset;

    /// RocketComponent's, then stores the outer radius of the nearest BodyTube among the
    /// ancestors (0 without one) as the radial distance of the instances and clears the cached
    /// locations again, which were computed with the old one.
    void componentChanged(const ComponentChangeEvent& event) override;

    /// ExternalComponent's properties (the length, the finish and the material), then the
    /// OUTER_DIAMETER, INNER_DIAMETER, HEIGHT, FLANGE_HEIGHT, BASE_HEIGHT and SCREW_HEIGHT the
    /// preset has, stored directly (unchecked); a positive CD overrides the CD; and the sum of
    /// the MASS, SCREW_MASS and NUT_MASS the preset has overrides the mass. Fires
    /// AEROMASS_CHANGE (always).
    void loadFromPreset(const ComponentPreset& preset, const PresetLoadOptions& options) override;

    [[nodiscard]] std::unique_ptr<RocketComponent> cloneShallow() const override;

private:
    /// The outer diameter of a new rail button, in m.
    static constexpr double kDefaultOuterDiameter = 0.0097;

    double m_outerDiameter{kDefaultOuterDiameter};
    double m_innerDiameter{0.008};
    double m_totalHeight{0.0097};
    double m_flangeHeight{0.002};
    double m_baseHeight{0.002};
    double m_screwHeight{0};
    /// The distance of the instances from the parent's axis (Java: radialDistance_m).
    double m_radialDistance{0};
    double m_angleOffsetRad{std::numbers::pi};
    /// Front to front along the positive rocket axis (Java: instanceSeparation).
    double m_instanceSeparation{kDefaultOuterDiameter * 6};
    int    m_instanceCount{1};
};

}  // namespace QtRocket
