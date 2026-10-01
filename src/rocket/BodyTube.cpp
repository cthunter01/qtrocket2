#include "QtRocket/rocket/BodyTube.h"

#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <utility>

#include "QtRocket/motor/Motor.h"
#include "QtRocket/rocket/BoxBounded.h"
#include "QtRocket/rocket/ClusterConfiguration.h"
#include "QtRocket/rocket/Coaxial.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfigurableComponent.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InsideColorComponent.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorConfigurationSet.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/RadialParent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket
{

namespace
{

/// getFilledVolume(): the volume of a cylinder of radius @p r and length @p l.
[[nodiscard]] double getFilledVolume(double r, double l)
{
    return std::numbers::pi * r * r * l;
}

}  // namespace

BodyTube::BodyTube() : BodyTube(8 * kDefaultRadius, kDefaultRadius)
{
    m_autoRadius = true;
}

BodyTube::BodyTube(double length, double radius)
  : m_motors(*this), m_outerRadius(MathUtil::javaMax(radius, 0))
{
    m_length = MathUtil::javaMax(length, 0);
    setDisplayOrderSide(0);  // Order for displaying the component in the 2D side view
    setDisplayOrderBack(1);  // Order for displaying the component in the 2D back view
}

BodyTube::BodyTube(double length, double radius, double thickness) : BodyTube(length, radius)
{
    m_filled    = false;
    m_thickness = thickness;
}

BodyTube::BodyTube(const BodyTube& other)
  : BoxBounded(other),
    RadialParent(other),
    FlightConfigurableComponent(other),
    MotorMount(other),
    Coaxial(other),
    SymmetricComponent(other),
    InsideColorComponent(other),
    m_motors(other.m_motors, *this),
    m_outerRadius(other.m_outerRadius),
    m_overhang(other.m_overhang),
    m_refCompId(other.m_refCompId),
    // Java's clone keeps a reference into the original tree, which matches no component here.
    m_refComp(other.m_refComp == Reference::NONE ? Reference::NONE : Reference::FOREIGN),
    m_autoRadius(other.m_autoRadius),
    m_isActingMount(other.m_isActingMount)
{
}

std::unique_ptr<RocketComponent> BodyTube::cloneShallow() const
{
    return std::make_unique<BodyTube>(*this);
}

// ========================================================================= outer radius

double BodyTube::getOuterRadius() const
{
    if (m_autoRadius)
    {
        m_outerRadius = getAutoOuterRadius();
    }
    return m_outerRadius;
}

double BodyTube::getAutoOuterRadius() const
{
    // Return auto radius from front or rear
    double                    r = -1;
    const SymmetricComponent* c = getPreviousSymmetricComponent();
    // Don't use the radius of a component who already has its auto diameter enabled
    if (c != nullptr && !c->usesNextCompAutomatic())
    {
        r           = c->getFrontAutoRadius();
        m_refComp   = Reference::COMPONENT;
        m_refCompId = c->getId();
    }
    if (r < 0)
    {
        c = getNextSymmetricComponent();
        // Don't use the radius of a component who already has its auto diameter enabled
        if (c != nullptr && !c->usesPreviousCompAutomatic())
        {
            r           = c->getRearAutoRadius();
            m_refComp   = Reference::COMPONENT;
            m_refCompId = c->getId();
        }
    }
    if (r < 0)
    {
        r = kDefaultRadius;
    }
    return r;
}

bool BodyTube::isReferenceComponent(const SymmetricComponent* component) const noexcept
{
    switch (m_refComp)
    {
        case Reference::NONE:
            return component == nullptr;
        case Reference::COMPONENT:
            return component != nullptr && component->getId() == m_refCompId;
        case Reference::FOREIGN:
            return false;
    }
    return false;
}

void BodyTube::setOuterRadius(double radius)
{
    if ((m_outerRadius == radius) && !m_autoRadius)
    {
        return;
    }

    m_autoRadius  = false;
    m_outerRadius = MathUtil::javaMax(radius, 0);

    // Java's comparison (not a minimum): a NaN radius leaves the thickness alone.
    const bool thickerThanTheRadius = m_thickness > m_outerRadius;
    if (thickerThanTheRadius)
    {
        m_thickness = m_outerRadius;
    }
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
    clearPreset();
}

void BodyTube::setOuterRadiusAutomatic(bool autoRadius)
{
    if (m_autoRadius == autoRadius)
    {
        return;
    }

    m_autoRadius = autoRadius;
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
    clearPreset();
}

bool BodyTube::usesPreviousCompAutomatic() const
{
    return isOuterRadiusAutomatic() && isReferenceComponent(getPreviousSymmetricComponent());
}

bool BodyTube::usesNextCompAutomatic() const
{
    return isOuterRadiusAutomatic() && isReferenceComponent(getNextSymmetricComponent());
}

void BodyTube::loadFromPreset(const ComponentPreset& preset, const PresetLoadOptions& options)
{
    SymmetricComponent::loadFromPreset(preset, options);
    if (preset.has(ComponentPreset::kOuterDiameter))
    {
        m_autoRadius               = false;
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

double BodyTube::getAftRadius() const
{
    return getOuterRadius();
}

double BodyTube::getForeRadius() const
{
    return getOuterRadius();
}

bool BodyTube::isAftRadiusAutomatic() const
{
    return isOuterRadiusAutomatic();
}

bool BodyTube::isForeRadiusAutomatic() const
{
    return isOuterRadiusAutomatic();
}

double BodyTube::getFrontAutoRadius() const
{
    if (isOuterRadiusAutomatic())
    {
        // Search for previous SymmetricComponent
        const SymmetricComponent* c = getPreviousSymmetricComponent();
        if (c != nullptr)
        {
            return c->getFrontAutoRadius();
        }
        return -1;
    }
    return getOuterRadius();
}

double BodyTube::getRearAutoRadius() const
{
    if (isOuterRadiusAutomatic())
    {
        // Search for next SymmetricComponent
        const SymmetricComponent* c = getNextSymmetricComponent();
        if (c != nullptr)
        {
            return c->getRearAutoRadius();
        }
        return -1;
    }
    return getOuterRadius();
}

double BodyTube::getInnerRadius() const
{
    if (m_filled)
    {
        return 0;
    }
    return MathUtil::javaMax(getOuterRadius() - m_thickness, 0);
}

void BodyTube::setInnerRadius(double r)
{
    setThickness(getOuterRadius() - r);
}

// ======================================================================== calculations

double BodyTube::getRadius(double /*x*/) const
{
    return getOuterRadius();
}

double BodyTube::getInnerRadius(double /*x*/) const
{
    if (m_filled)
    {
        return 0.0;
    }
    return MathUtil::javaMax(getOuterRadius() - m_thickness, 0);
}

Coordinate BodyTube::getComponentCG() const
{
    return Coordinate{m_length / 2, 0, 0, getComponentMass()};
}

double BodyTube::getComponentVolume() const
{
    const double r = getOuterRadius();
    if (m_filled)
    {
        return getFilledVolume(r, m_length);
    }
    return getFilledVolume(r, m_length) - getFilledVolume(getInnerRadius(0), m_length);
}

double BodyTube::getLongitudinalUnitInertia() const
{
    // 1/12 * (3 * (r2^2 + r1^2) + h^2)
    return ((3 * (MathUtil::pow2(getOuterRadius()) + MathUtil::pow2(getInnerRadius()))) +
            MathUtil::pow2(getLength())) /
           12;
}

double BodyTube::getRotationalUnitInertia() const
{
    // 1/2 * (r1^2 + r2^2)
    return (MathUtil::pow2(getInnerRadius()) + MathUtil::pow2(getOuterRadius())) / 2;
}

double BodyTube::getComponentWetArea() const
{
    return std::numbers::pi * getOuterRadius() * 2 * getLength();
}

double BodyTube::getComponentPlanformArea() const
{
    return getOuterRadius() * 2 * getLength();
}

BoundingBox BodyTube::getInstanceBoundingBox() const
{
    BoundingBox instanceBounds;

    instanceBounds.update(Coordinate{getLength(), 0, 0});

    const double r = getOuterRadius();
    instanceBounds.update(Coordinate{0, r, r});
    instanceBounds.update(Coordinate{0, -r, -r});

    return instanceBounds;
}

bool BodyTube::isCompatible(ComponentKind kind) const
{
    if (kind == ComponentKind::PARALLEL_STAGE)
    {
        return true;
    }
    if (kind == ComponentKind::POD_SET)
    {
        return true;
    }

    if (isInternal(kind))
    {
        return true;
    }
    return isExternal(kind) && !isBodyComponent(kind);
}

// ========================================================================== motor mount

MotorConfiguration& BodyTube::getDefaultMotorConfig()
{
    return m_motors.getDefault();
}

const MotorConfiguration& BodyTube::getDefaultMotorConfig() const
{
    return m_motors.getDefault();
}

MotorConfigurationSet& BodyTube::getMotorConfigurationSet()
{
    return m_motors;
}

const MotorConfigurationSet& BodyTube::getMotorConfigurationSet() const
{
    return m_motors;
}

MotorConfiguration& BodyTube::getMotorConfig(const FlightConfigurationId& fcid)
{
    return m_motors.get(fcid);
}

const MotorConfiguration& BodyTube::getMotorConfig(const FlightConfigurationId& fcid) const
{
    return m_motors.get(fcid);
}

void BodyTube::setMotorConfig(std::optional<MotorConfiguration> newMotorConfig,
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
            bug(" attempt to add a MotorConfig to a second mount! ");
        }

        m_motors.set(fcid, std::move(*newMotorConfig));
    }

    m_isActingMount = true;
}

void BodyTube::reset(const FlightConfigurationId& fcid)
{
    m_motors.reset(fcid);
}

void BodyTube::copyFlightConfiguration(const FlightConfigurationId& oldConfigId,
                                       const FlightConfigurationId& newConfigId)
{
    m_motors.copyFlightConfiguration(oldConfigId, newConfigId);
}

void BodyTube::setMotorMount(bool acting)
{
    if (m_isActingMount == acting)
    {
        return;
    }
    m_isActingMount = acting;
    fireComponentChangeEvent(ComponentChangeEvent::kMotorChange);
}

bool BodyTube::hasMotor() const
{
    return m_motors.size() > 0;
}

int BodyTube::getMotorCount() const
{
    return getClusterConfiguration().getClusterCount();
}

int BodyTube::getMotorCountIncludingAssemblyCopies() const
{
    // Get the parent assemblies of the motor mount, and multiply the data by the number of
    // instances
    int multiplier = 1;
    for (const RocketComponent* parent : getParentAssemblies())
    {
        multiplier *= parent->getInstanceCount();
    }

    const int count = getMotorCount();
    return count * multiplier;
}

double BodyTube::getMotorMountDiameter() const
{
    return getInnerRadius() * 2;
}

void BodyTube::setMotorOverhang(double overhang)
{
    if (MathUtil::equals(m_overhang, overhang))
    {
        return;
    }
    m_overhang = overhang;
    fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
}

Coordinate BodyTube::getMotorPosition(const FlightConfigurationId& fcid) const
{
    const Motor* motor = m_motors.get(fcid).getMotor().get();
    if (motor == nullptr)
    {
        bug("No motor with id " + fcid.toString() + " defined.");
    }

    return Coordinate{getLength() - motor->getLength() + getMotorOverhang()};
}

std::string BodyTube::toMotorDebug(const Preferences& preferences) const
{
    return m_motors.toDebug(preferences);
}

const ClusterConfiguration& BodyTube::getClusterConfiguration() noexcept
{
    return ClusterConfiguration::single();
}

// HOOK(tube-fin-set): Java's BodyTube.addChild() gives a TubeFinSet added with a NaN thickness
// the tube's getThickness(); with TubeFinSet ported, override childAdded() here to do that.

}  // namespace QtRocket
