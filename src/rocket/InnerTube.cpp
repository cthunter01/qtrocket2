#include "QtRocket/rocket/InnerTube.h"

#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/motor/Motor.h"
#include "QtRocket/rocket/BoxBounded.h"
#include "QtRocket/rocket/ClusterConfiguration.h"
#include "QtRocket/rocket/Clusterable.h"
#include "QtRocket/rocket/Coaxial.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfigurableComponent.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InsideColorComponent.h"
#include "QtRocket/rocket/Instanceable.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorConfigurationSet.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/RadialParent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/ThicknessRingComponent.h"
#include "QtRocket/rocket/position/AxialPositionable.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

InnerTube::InnerTube() : m_motors(*this)
{
    // A-C motor size:
    setOuterRadius(0.019 / 2);
    setInnerRadius(0.018 / 2);
    setLength(0.070);

    setDisplayOrderSide(5);   // Order for displaying the component in the 2D side view
    setDisplayOrderBack(14);  // Order for displaying the component in the 2D back view
}

InnerTube::InnerTube(const InnerTube& other)
  : AxialPositionable(other),
    BoxBounded(other),
    Coaxial(other),
    Instanceable(other),
    Clusterable(other),
    RadialParent(other),
    FlightConfigurableComponent(other),
    MotorMount(other),
    ThicknessRingComponent(other),
    InsideColorComponent(other),
    m_cluster(other.m_cluster),
    m_clusterScale(other.m_clusterScale),
    m_clusterRotation(other.m_clusterRotation),
    m_overhang(other.m_overhang),
    m_isActingMount(other.m_isActingMount),
    m_motors(other.m_motors, *this)
{
}

std::unique_ptr<RocketComponent> InnerTube::cloneShallow() const
{
    return std::make_unique<InnerTube>(*this);
}

bool InnerTube::allowsChildren() const
{
    return true;
}

bool InnerTube::isCompatible(ComponentKind kind) const
{
    return isInternal(kind);
}

bool InnerTube::isAfter() const
{
    return false;
}

void InnerTube::loadFromPreset(const ComponentPreset& preset, const PresetLoadOptions& options)
{
    ThicknessRingComponent::loadFromPreset(preset, options);
    if (preset.has(ComponentPreset::kOuterDiameter))
    {
        const double outerDiameter = preset.get(ComponentPreset::kOuterDiameter);
        m_outerRadius              = outerDiameter / 2.0;
        if (preset.has(ComponentPreset::kInnerDiameter))
        {
            const double innerDiameter = preset.get(ComponentPreset::kInnerDiameter);
            m_thickness                = (outerDiameter - innerDiameter) / 2.0;
        }
    }

    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

// ---- RadialParent

double InnerTube::getInnerRadius(double /*x*/) const
{
    return getInnerRadius();
}

double InnerTube::getOuterRadius(double /*x*/) const
{
    return getOuterRadius();
}

double InnerTube::getLength() const
{
    return ThicknessRingComponent::getLength();
}

// ---- cluster

void InnerTube::setClusterConfiguration(const ClusterConfiguration& cluster)
{
    if (&cluster == m_cluster)
    {
        return;  // no change
    }
    m_cluster = &cluster;
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

void InnerTube::setClusterScale(double scale)
{
    scale = MathUtil::javaMax(scale, 0.0);

    if (MathUtil::equals(m_clusterScale, scale))
    {
        return;
    }
    m_clusterScale = scale;
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

double InnerTube::getClusterScaleAbsolute() const
{
    return (getClusterScale() - 1) * getOuterRadius() * 2;
}

void InnerTube::setClusterScaleAbsolute(double scale)
{
    const double scaleRel = (scale / (getOuterRadius() * 2)) + 1;
    setClusterScale(scaleRel);
}

void InnerTube::setClusterRotation(double rotation)
{
    rotation = MathUtil::reducePi(rotation);
    if (m_clusterRotation == rotation)
    {
        return;
    }
    m_clusterRotation = rotation;
    fireComponentChangeEvent(ComponentChangeEvent::kMassChange);
}

double InnerTube::getClusterSeparation() const
{
    return 2 * getOuterRadius() * m_clusterScale;
}

std::vector<Coordinate> InnerTube::getClusterPoints() const
{
    const std::vector<double> points =
        m_cluster->getPoints(m_clusterRotation - getRadialDirection());
    const double separation = getClusterSeparation();
    const double yOffset    = m_radialPosition * std::cos(m_radialDirection);
    const double zOffset    = m_radialPosition * std::sin(m_radialDirection);

    std::vector<Coordinate> list;
    list.reserve(points.size() / 2);
    for (std::size_t i = 0; i < points.size() / 2; i++)
    {
        list.emplace_back(0, (points[2 * i] * separation) + yOffset,
                          (points[(2 * i) + 1] * separation) + zOffset);
    }
    return list;
}

// ---- Instanceable

std::vector<Coordinate> InnerTube::getInstanceLocations() const
{
    return RocketComponent::getInstanceLocations();
}

std::vector<Coordinate> InnerTube::getInstanceOffsets() const
{
    return getClusterPoints();
}

int InnerTube::getInstanceCount() const
{
    return m_cluster->getClusterCount();
}

void InnerTube::setInstanceCount(int /*newCount*/)
{
    // Java logs "Programmer Error: cannot set the instance count of an InnerTube directly.
    // Please set setClusterConfiguration(ClusterConfiguration) instead." and changes nothing.
}

std::string InnerTube::getPatternName() const
{
    return std::string{m_cluster->getXmlName()};
}

// ---- MotorMount

bool InnerTube::hasMotor() const
{
    // The default configuration is the empty one; any other has a motor.
    return m_motors.size() > 0;
}

void InnerTube::setMotorMount(bool active)
{
    if (m_isActingMount == active)
    {
        return;
    }
    m_isActingMount = active;
    fireComponentChangeEvent(ComponentChangeEvent::kMotorChange);
}

bool InnerTube::isMotorMount() const
{
    return m_isActingMount;
}

MotorConfiguration& InnerTube::getDefaultMotorConfig()
{
    return m_motors.getDefault();
}

const MotorConfiguration& InnerTube::getDefaultMotorConfig() const
{
    return m_motors.getDefault();
}

MotorConfigurationSet& InnerTube::getMotorConfigurationSet()
{
    return m_motors;
}

const MotorConfigurationSet& InnerTube::getMotorConfigurationSet() const
{
    return m_motors;
}

MotorConfiguration& InnerTube::getMotorConfig(const FlightConfigurationId& fcid)
{
    return m_motors.get(fcid);
}

const MotorConfiguration& InnerTube::getMotorConfig(const FlightConfigurationId& fcid) const
{
    return m_motors.get(fcid);
}

void InnerTube::setMotorConfig(std::optional<MotorConfiguration> newMotorConfig,
                               const FlightConfigurationId&      fcid)
{
    if (!newMotorConfig)
    {
        m_motors.remove(fcid);
    }
    else
    {
        if (&newMotorConfig->getMount() != static_cast<const MotorMount*>(this))
        {
            bug(" attempt to add a MotorConfig to a second mount!");
        }
        m_motors.set(fcid, std::move(*newMotorConfig));
    }

    m_isActingMount = true;
}

int InnerTube::getMotorCount() const
{
    return getClusterConfiguration().getClusterCount();
}

int InnerTube::getMotorCountIncludingAssemblyCopies() const
{
    // The mount's motors times the instances of every assembly above it.
    int multiplier = 1;
    for (const RocketComponent* parent : getParentAssemblies())
    {
        multiplier *= parent->getInstanceCount();
    }
    return getMotorCount() * multiplier;
}

double InnerTube::getMotorOverhang() const
{
    return m_overhang;
}

void InnerTube::setMotorOverhang(double overhang)
{
    if (MathUtil::equals(m_overhang, overhang))
    {
        return;
    }
    m_overhang = overhang;
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

double InnerTube::getMotorMountDiameter() const
{
    return getInnerRadius() * 2;
}

Coordinate InnerTube::getMotorPosition(const FlightConfigurationId& fcid) const
{
    const Motor* motor = m_motors.get(fcid).getMotor().get();
    if (motor == nullptr)
    {
        bug("No motor with id " + fcid.toString() + " defined.");
    }
    return Coordinate{getLength() - motor->getLength() + getMotorOverhang()};
}

std::string InnerTube::toMotorDebug(const Preferences& preferences) const
{
    return m_motors.toDebug(preferences);
}

// ---- FlightConfigurableComponent

void InnerTube::copyFlightConfiguration(const FlightConfigurationId& oldConfigId,
                                        const FlightConfigurationId& newConfigId)
{
    m_motors.copyFlightConfiguration(oldConfigId, newConfigId);
}

void InnerTube::reset(const FlightConfigurationId& fcid)
{
    m_motors.reset(fcid);
}

// ---- splitting a cluster

std::unique_ptr<InnerTube> InnerTube::makeIndividualClusterComponent(const Coordinate& coord,
                                                                     std::string_view  splitName,
                                                                     const InnerTube&  innerTube)
{
    std::unique_ptr<InnerTube> copy = componentCast<InnerTube>(innerTube.copyWithNewIds());
    // Java's clearConfigListeners(): the config listeners are not ported.
    copy->setClusterConfiguration(ClusterConfiguration::single());
    copy->setClusterRotation(0.0);
    copy->setClusterScale(1.0);
    copy->setRadialShift(coord.y, coord.z);
    copy->setName(splitName);
    return copy;
}

}  // namespace QtRocket
