#pragma once

#include <concepts>
#include <memory>
#include <optional>
#include <string>

#include "QtRocket/rocket/BoxBounded.h"
#include "QtRocket/rocket/ClusterConfiguration.h"
#include "QtRocket/rocket/Coaxial.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfigurableComponent.h"
#include "QtRocket/rocket/InsideColorComponent.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorConfigurationSet.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/RadialParent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket
{

class ComponentPreset;
class FlightConfigurationId;
class Preferences;

/// A body tube (OpenRocket's BodyTube): a cylinder of a length and an outer radius, hollow with a
/// wall thickness or filled, whose outer radius may be automatic (taken from the previous
/// symmetric component, else the next one, else kDefaultRadius). It is a motor mount (a single
/// motor, ClusterConfiguration::single()) with a MotorConfigurationSet, and the parent of pod sets,
/// booster sets and every internal and external component except body components. Its volume,
/// CG, unit inertias and areas are closed-form (a cylinder); the full volume and the planform
/// centre are SymmetricComponent's integration, as in Java.
///
/// The automatic radius remembers the component it was last taken from (Java's refComp), which
/// usesPreviousCompAutomatic() and usesNextCompAutomatic() compare with the current neighbours.
/// Java keeps an object reference there; this keeps the component's id, so that it never refers
/// to a destroyed component, and a copy (whose reference in Java still points into the original
/// tree, so that it matches no component of the copy) keeps a reference that matches nothing.
///
/// Deviations from OpenRocket:
/// - setMotorConfig() takes a std::optional (see MotorMount); getMotorPosition() throws BugError
///   where Java throws IllegalArgumentException.
/// - The filled constructor takes exactly a bool, so that BodyTube(l, r, 0) is the thickness
///   constructor (Java's int converts only to double).
/// - The multi-edit config listeners (addConfigListener() and friends, which also link the default
///   motor configurations) are not ported (see RocketComponent).
/// - Java's addChild() override, which gives a TubeFinSet added without a thickness the tube's,
///   waits for TubeFinSet (see the HOOK in BodyTube.cpp).
// NOLINTNEXTLINE(misc-multiple-inheritance): the InsideColorComponent mixin carries data
class BodyTube : public SymmetricComponent,
                 public virtual MotorMount,
                 public virtual Coaxial,
                 public InsideColorComponent
{
public:
    using RocketComponent::isCompatible;
    using SymmetricComponent::getInnerRadius;
    using SymmetricComponent::getOuterRadius;
    using SymmetricComponent::getRadius;

    /// A tube 8 * kDefaultRadius long with an automatic outer radius (kDefaultRadius until a
    /// neighbour gives one).
    BodyTube();

    /// A tube of @p length and fixed outer radius @p radius (negative values become 0), hollow
    /// with kDefaultThickness.
    BodyTube(double length, double radius);

    /// A tube of @p length and fixed outer radius @p radius, filled or hollow (with
    /// kDefaultThickness).
    template <std::same_as<bool> Bool>
    BodyTube(double length, double radius, Bool filled) : BodyTube(length, radius)
    {
        m_filled = filled;
    }

    /// A hollow tube of @p length, fixed outer radius @p radius and wall @p thickness.
    BodyTube(double length, double radius, double thickness);

    /// A copy whose motor configurations belong to the copy (Java's copyWithOriginalID()), with
    /// a reference component that matches no component (see the class comment).
    BodyTube(const BodyTube& other);

    BodyTube& operator=(const BodyTube&) = delete;
    BodyTube(BodyTube&&)                 = delete;
    BodyTube& operator=(BodyTube&&)      = delete;
    ~BodyTube() override                 = default;

    [[nodiscard]] ComponentKind kind() const noexcept override { return ComponentKind::BODY_TUBE; }

    // ---- the outer radius (Coaxial)

    /// The outer radius; when automatic, refreshed first from the neighbours (see the class
    /// comment).
    [[nodiscard]] double getOuterRadius() const override;

    /// Sets a fixed outer radius (negative values become 0), switching the automatic radius off;
    /// a wall thicker than the radius becomes the radius. Fires AEROMASS_CHANGE and clears the
    /// preset, unless the radius is already @p radius and not automatic.
    void setOuterRadius(double radius) override;

    /// Whether the outer radius is automatic.
    [[nodiscard]] bool isOuterRadiusAutomatic() const noexcept { return m_autoRadius; }

    /// Switches the automatic outer radius on or off; fires AEROMASS_CHANGE and clears the preset
    /// when it changes.
    void setOuterRadiusAutomatic(bool autoRadius);

    /// 0 when filled, else the outer radius less the wall thickness, not below 0.
    [[nodiscard]] double getInnerRadius() const override;

    /// setThickness(getOuterRadius() - @p r).
    void setInnerRadius(double r) override;

    /// SymmetricComponent::getThickness() (Coaxial declares it too).
    [[nodiscard]] double getThickness() const override
    {
        return SymmetricComponent::getThickness();
    }

    // ---- SymmetricComponent

    /// isOuterRadiusAutomatic() and the reference component is the previous symmetric component.
    [[nodiscard]] bool usesPreviousCompAutomatic() const override;

    /// isOuterRadiusAutomatic() and the reference component is the next symmetric component.
    [[nodiscard]] bool usesNextCompAutomatic() const override;

    /// getOuterRadius().
    [[nodiscard]] double getAftRadius() const override;
    /// getOuterRadius().
    [[nodiscard]] double getForeRadius() const override;
    /// isOuterRadiusAutomatic().
    [[nodiscard]] bool isAftRadiusAutomatic() const override;
    /// isOuterRadiusAutomatic().
    [[nodiscard]] bool isForeRadiusAutomatic() const override;

    /// With an automatic radius the previous symmetric component's offer (-1 without one), else
    /// the outer radius.
    [[nodiscard]] double getFrontAutoRadius() const override;

    /// With an automatic radius the next symmetric component's offer (-1 without one), else the
    /// outer radius.
    [[nodiscard]] double getRearAutoRadius() const override;

    /// getOuterRadius() everywhere.
    [[nodiscard]] double getRadius(double x) const override;

    /// 0 when filled, else the outer radius less the wall thickness, not below 0.
    [[nodiscard]] double getInnerRadius(double x) const override;

    /// (length / 2, 0, 0) with the component mass as the weight.
    [[nodiscard]] Coordinate getComponentCG() const override;

    /// pi r^2 length, less the bore unless filled.
    [[nodiscard]] double getComponentVolume() const override;

    /// (3 (outer^2 + inner^2) + length^2) / 12.
    [[nodiscard]] double getLongitudinalUnitInertia() const override;

    /// (inner^2 + outer^2) / 2.
    [[nodiscard]] double getRotationalUnitInertia() const override;

    /// pi * 2 r * length.
    [[nodiscard]] double getComponentWetArea() const override;

    /// 2 r * length.
    [[nodiscard]] double getComponentPlanformArea() const override;

    /// From (0, -r, -r) to (length, r, r) with r the outer radius.
    [[nodiscard]] BoundingBox getInstanceBoundingBox() const override;

    /// Booster sets, pod sets, internal components, and external components that are not body
    /// components.
    [[nodiscard]] bool isCompatible(ComponentKind kind) const override;

    // ---- RocketComponent / MotorMount (declared by both)

    [[nodiscard]] double getLength() const override { return SymmetricComponent::getLength(); }
    [[nodiscard]] int    getInstanceCount() const override
    {
        return SymmetricComponent::getInstanceCount();
    }
    [[nodiscard]] bool isMotorMount() const override { return m_isActingMount; }

    // ---- MotorMount

    [[nodiscard]] MotorConfiguration&       getDefaultMotorConfig() override;
    [[nodiscard]] const MotorConfiguration& getDefaultMotorConfig() const override;

    [[nodiscard]] MotorConfigurationSet&       getMotorConfigurationSet() override;
    [[nodiscard]] const MotorConfigurationSet& getMotorConfigurationSet() const override;

    [[nodiscard]] MotorConfiguration& getMotorConfig(const FlightConfigurationId& fcid) override;
    [[nodiscard]] const MotorConfiguration& getMotorConfig(
        const FlightConfigurationId& fcid) const override;

    void setMotorConfig(std::optional<MotorConfiguration> newMotorConfig,
                        const FlightConfigurationId&      fcid) override;

    /// Makes the tube act as a motor mount or not; fires MOTOR_CHANGE when it changes.
    void setMotorMount(bool acting) override;

    [[nodiscard]] bool hasMotor() const override;

    /// The cluster count of getClusterConfiguration(): 1.
    [[nodiscard]] int getMotorCount() const override;

    [[nodiscard]] int getMotorCountIncludingAssemblyCopies() const override;

    /// Twice the inner radius.
    [[nodiscard]] double getMotorMountDiameter() const override;

    [[nodiscard]] double getMotorOverhang() const override { return m_overhang; }

    /// Sets the motor overhang; fires AEROMASS_CHANGE unless it equals the current one
    /// (MathUtil::equals).
    void setMotorOverhang(double overhang) override;

    [[nodiscard]] Coordinate getMotorPosition(const FlightConfigurationId& fcid) const override;

    [[nodiscard]] std::string toMotorDebug(const Preferences& preferences) const override;

    /// A single motor: ClusterConfiguration::single() (static: a body tube is never clustered;
    /// MotorMount does not declare it yet, see there).
    [[nodiscard]] static const ClusterConfiguration& getClusterConfiguration() noexcept;

    // ---- FlightConfigurableComponent

    void reset(const FlightConfigurationId& fcid) override;
    void copyFlightConfiguration(const FlightConfigurationId& oldConfigId,
                                 const FlightConfigurationId& newConfigId) override;

protected:
    /// SymmetricComponent's properties, then OUTER_DIAMETER (a fixed radius; with an
    /// INNER_DIAMETER the wall thickness too), stored directly. Fires AEROMASS_CHANGE.
    void loadFromPreset(const ComponentPreset& preset, const PresetLoadOptions& options) override;

    [[nodiscard]] std::unique_ptr<RocketComponent> cloneShallow() const override;

private:
    /// What refComp refers to.
    enum class Reference
    {
        NONE,       ///< null: no automatic radius taken yet
        COMPONENT,  ///< the component with id m_refCompId
        FOREIGN,    ///< a component of the tree this one was copied from (matches nothing)
    };

    /// The automatic outer radius: the previous component's front offer, unless it takes its own
    /// radius from this one; then the next component's rear offer, likewise; else kDefaultRadius.
    /// Remembers the component it consulted last.
    [[nodiscard]] double getAutoOuterRadius() const;

    /// Java's refComp == @p component.
    [[nodiscard]] bool isReferenceComponent(const SymmetricComponent* component) const noexcept;

    MotorConfigurationSet m_motors;
    mutable double        m_outerRadius{0};
    double                m_overhang{0};
    mutable Uuid          m_refCompId;
    mutable Reference     m_refComp{Reference::NONE};
    bool                  m_autoRadius{false};
    bool                  m_isActingMount{false};
};

}  // namespace QtRocket
