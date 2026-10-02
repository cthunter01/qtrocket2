#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/rocket/ClusterConfiguration.h"
#include "QtRocket/rocket/Clusterable.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/InsideColorComponent.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorConfigurationSet.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/RadialParent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/ThicknessRingComponent.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket
{

class ComponentPreset;
class FlightConfigurationId;
class Preferences;

/// An inner tube (OpenRocket's InnerTube): a tube inside the body that can be a motor mount and
/// can be clustered. A new one is an A-C motor tube: outer radius 9.5 mm, inner radius 9 mm,
/// 70 mm long.
///
/// Cluster: one of the ClusterConfiguration layouts (single by default), scaled so that the
/// centers of the closest tubes are getClusterSeparation() = 2 * outer radius * scale apart, and
/// rotated by getClusterRotation(). Each cluster member is an instance (getInstanceOffsets() are
/// the cluster points, shifted by the radial position); the instance count is the cluster's and
/// cannot be set directly. makeIndividualClusterComponent() makes one tube of a cluster as its
/// own component (the GUI's "split cluster").
///
/// Motor mount: a MotorConfigurationSet (one motor per flight configuration; the copy constructor
/// rebuilds it for the copy, Java's copyWithOriginalID()), whether the tube acts as a motor
/// mount, and the motor overhang. It holds as many motors as the cluster has tubes.
///
/// Deviations from OpenRocket: setInstanceCount() does nothing (Java logs a programmer error);
/// getMotorPosition() throws BugError where Java throws IllegalArgumentException;
/// makeIndividualClusterComponent() takes an InnerTube (Java casts a RocketComponent). Not
/// ported: the multi-edit config listener overrides (addConfigListener() and friends), by
/// decision (see RocketComponent).
// NOLINTNEXTLINE(misc-multiple-inheritance): the InsideColorComponent mixin carries data
class InnerTube : public ThicknessRingComponent,
                  public virtual Clusterable,
                  public virtual RadialParent,
                  public virtual MotorMount,
                  public InsideColorComponent
{
public:
    using RocketComponent::isCompatible;
    using ThicknessRingComponent::getInnerRadius;
    using ThicknessRingComponent::getOuterRadius;

    /// An A-C motor tube: outer radius 0.019 / 2, inner radius 0.018 / 2, length 0.07; a single
    /// tube, not acting as a motor mount, positioned BOTTOM.
    InnerTube();

    /// A copy whose motor configurations belong to the copy (Java: copyWithOriginalID()).
    InnerTube(const InnerTube& other);

    InnerTube& operator=(const InnerTube&) = delete;
    InnerTube(InnerTube&&)                 = delete;
    InnerTube& operator=(InnerTube&&)      = delete;
    ~InnerTube() override                  = default;

    [[nodiscard]] ComponentKind kind() const noexcept override { return ComponentKind::INNER_TUBE; }

    /// Always true.
    [[nodiscard]] bool allowsChildren() const override;

    /// Accepts every internal component.
    [[nodiscard]] bool isCompatible(ComponentKind kind) const override;

    /// False: an inner tube is positioned relative to its parent.
    [[nodiscard]] bool isAfter() const override;

    // ---- RadialParent

    /// getInnerRadius(): a tube's inner radius is the same along its length.
    [[nodiscard]] double getInnerRadius(double x) const override;

    /// getOuterRadius().
    [[nodiscard]] double getOuterRadius(double x) const override;

    /// The length (RocketComponent's; also RadialParent's and MotorMount's).
    [[nodiscard]] double getLength() const override;

    // ---- cluster

    [[nodiscard]] const ClusterConfiguration& getClusterConfiguration() const override
    {
        return *m_cluster;
    }

    /// Sets the cluster layout; fires MASS_CHANGE when it is another layout (layouts compare by
    /// identity).
    void setClusterConfiguration(const ClusterConfiguration& cluster) override;

    /// The cluster scaling: 1 packs the tubes touching each other, larger values separate them
    /// and smaller ones overlap them.
    [[nodiscard]] double getClusterScale() const noexcept { return m_clusterScale; }

    /// Sets the cluster scaling (negative values become 0); fires MASS_CHANGE unless it equals
    /// (within MathUtil::equals()) the current one.
    void setClusterScale(double scale);

    /// The cluster scaling as a distance: (scale - 1) * the outer diameter, the gap between
    /// neighbouring tubes (0 when they touch).
    [[nodiscard]] double getClusterScaleAbsolute() const;

    /// Sets the scaling from a gap @p scale: setClusterScale(scale / (outer diameter) + 1).
    void setClusterScaleAbsolute(double scale);

    /// The rotation of the cluster about the axis, in radians.
    [[nodiscard]] double getClusterRotation() const noexcept { return m_clusterRotation; }

    /// Sets the rotation, reduced to -pi ... pi; fires MASS_CHANGE when it changes.
    void setClusterRotation(double rotation);

    /// The distance between the centers of the closest two tubes: 2 * outer radius * scale.
    [[nodiscard]] double getClusterSeparation() const override;

    /// The center of each tube of the cluster: the layout's points, rotated by the cluster
    /// rotation minus the radial direction, scaled by getClusterSeparation() and shifted by the
    /// radial position (as (0, y, z)).
    [[nodiscard]] std::vector<Coordinate> getClusterPoints() const;

    // ---- Instanceable

    [[nodiscard]] std::vector<Coordinate> getInstanceLocations() const override;

    /// getClusterPoints().
    [[nodiscard]] std::vector<Coordinate> getInstanceOffsets() const override;

    /// The cluster's count.
    [[nodiscard]] int getInstanceCount() const override;

    /// Does nothing: the count follows the cluster configuration (Java logs a programmer error).
    void setInstanceCount(int newCount) override;

    /// The cluster's xml name, e.g. "3-ring".
    [[nodiscard]] std::string getPatternName() const override;

    // ---- MotorMount

    [[nodiscard]] bool hasMotor() const override;

    /// Makes the tube act as a motor mount or not; fires MOTOR_CHANGE when it changes.
    void setMotorMount(bool active) override;

    [[nodiscard]] bool isMotorMount() const override;

    [[nodiscard]] MotorConfiguration&       getDefaultMotorConfig() override;
    [[nodiscard]] const MotorConfiguration& getDefaultMotorConfig() const override;

    [[nodiscard]] MotorConfigurationSet&       getMotorConfigurationSet() override;
    [[nodiscard]] const MotorConfigurationSet& getMotorConfigurationSet() const override;

    [[nodiscard]] MotorConfiguration& getMotorConfig(const FlightConfigurationId& fcid) override;
    [[nodiscard]] const MotorConfiguration& getMotorConfig(
        const FlightConfigurationId& fcid) const override;

    void setMotorConfig(std::optional<MotorConfiguration> newMotorConfig,
                        const FlightConfigurationId&      fcid) override;

    /// The cluster's count.
    [[nodiscard]] int getMotorCount() const override;

    [[nodiscard]] int getMotorCountIncludingAssemblyCopies() const override;

    [[nodiscard]] double getMotorOverhang() const override;

    /// Sets the overhang; fires AEROMASS_CHANGE unless it equals (within MathUtil::equals()) the
    /// current one.
    void setMotorOverhang(double overhang) override;

    /// Twice the inner radius.
    [[nodiscard]] double getMotorMountDiameter() const override;

    /// (length - motor length + overhang, 0, 0) for the motor of @p fcid.
    /// @throws BugError when @p fcid has no motor.
    [[nodiscard]] Coordinate getMotorPosition(const FlightConfigurationId& fcid) const override;

    [[nodiscard]] std::string toMotorDebug(const Preferences& preferences) const override;

    // ---- FlightConfigurableComponent

    void copyFlightConfiguration(const FlightConfigurationId& oldConfigId,
                                 const FlightConfigurationId& newConfigId) override;
    void reset(const FlightConfigurationId& fcid) override;

    // ---- splitting a cluster

    /// One tube of the cluster @p innerTube as a component of its own (Java:
    /// makeIndividualClusterComponent(), the GUI's "split cluster", called once per tube): a
    /// copy of @p innerTube and its children with new ids (copyWithNewIds()), made a single tube
    /// without rotation and scale 1, shifted radially to the y and z of @p coord (the tube's
    /// location), and named @p splitName.
    [[nodiscard]] static std::unique_ptr<InnerTube> makeIndividualClusterComponent(
        const Coordinate& coord, std::string_view splitName, const InnerTube& innerTube);

protected:
    [[nodiscard]] std::unique_ptr<RocketComponent> cloneShallow() const override;

    /// ThicknessRingComponent's values, then the outer radius from an OUTER_DIAMETER and, with
    /// an INNER_DIAMETER too, the thickness, again; fires AEROMASS_CHANGE.
    void loadFromPreset(const ComponentPreset& preset, const PresetLoadOptions& options) override;

private:
    const ClusterConfiguration* m_cluster{&ClusterConfiguration::single()};
    double                      m_clusterScale{1.0};
    double                      m_clusterRotation{0.0};
    double                      m_overhang{0};
    bool                        m_isActingMount{false};
    MotorConfigurationSet       m_motors;
};

}  // namespace QtRocket
