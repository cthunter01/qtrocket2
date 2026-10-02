#pragma once

#include <memory>
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
#include "QtRocket/rocket/Instanceable.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorConfigurationSet.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/RadialParent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AnglePositionable.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusPositionable.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "rocket/TestBodyComponent.h"
#include "rocket/TestComponent.h"

namespace QtRocket::Test
{

/// A TestBodyComponent that is a MotorMount, standing in for BodyTube and InnerTube (whose motor
/// mount methods it mirrors): a MotorConfigurationSet rebuilt for the copy in the copy constructor,
/// the acting-mount flag, the overhang, and a settable motor count (the cluster size).
class TestMotorMount final : public TestBodyComponent, public virtual MotorMount
{
public:
    explicit TestMotorMount(ComponentKind kind = ComponentKind::BODY_TUBE,
                            AxialMethod method = AxialMethod::AFTER, double length = 0.0)
      : TestBodyComponent(kind, method, length), m_motors(*this)
    {
    }

    /// A copy whose motor configurations belong to the copy (BodyTube.copyWithOriginalID()).
    TestMotorMount(const TestMotorMount& other)
      : Coaxial(other),
        RadiusPositionable(other),
        AnglePositionable(other),
        Instanceable(other),
        BoxBounded(other),
        RadialParent(other),
        FlightConfigurableComponent(other),
        MotorMount(other),
        TestBodyComponent(other),
        m_motors(other.m_motors, *this),
        m_overhang(other.m_overhang),
        m_motorCount(other.m_motorCount),
        m_isActingMount(other.m_isActingMount)
    {
    }

    TestMotorMount& operator=(const TestMotorMount&) = delete;
    TestMotorMount(TestMotorMount&&)                 = delete;
    TestMotorMount& operator=(TestMotorMount&&)      = delete;
    ~TestMotorMount() override                       = default;

    /// A mount of @p length and outer radius @p radius, in a std::unique_ptr for addChild().
    [[nodiscard]] static std::unique_ptr<TestMotorMount> make(
        double length, double radius, ComponentKind kind = ComponentKind::BODY_TUBE,
        AxialMethod method = AxialMethod::AFTER)
    {
        auto mount = std::make_unique<TestMotorMount>(kind, method, length);
        mount->setOuterRadius(radius);
        return mount;
    }

    // ---- RocketComponent / MotorMount

    [[nodiscard]] int getInstanceCount() const override
    {
        return TestComponent::getInstanceCount();
    }
    [[nodiscard]] double getLength() const override { return TestBodyComponent::getLength(); }
    [[nodiscard]] bool   isMotorMount() const override { return m_isActingMount; }

    // ---- MotorMount

    [[nodiscard]] bool hasMotor() const override { return m_motors.size() > 0; }

    void setMotorMount(bool acting) override
    {
        if (m_isActingMount == acting)
        {
            return;
        }
        m_isActingMount = acting;
        fireComponentChangeEvent(ComponentChangeEvent::kMotorChange);
    }

    [[nodiscard]] MotorConfiguration& getDefaultMotorConfig() override
    {
        return m_motors.getDefault();
    }
    [[nodiscard]] const MotorConfiguration& getDefaultMotorConfig() const override
    {
        return m_motors.getDefault();
    }

    [[nodiscard]] MotorConfigurationSet& getMotorConfigurationSet() override { return m_motors; }
    [[nodiscard]] const MotorConfigurationSet& getMotorConfigurationSet() const override
    {
        return m_motors;
    }

    [[nodiscard]] MotorConfiguration& getMotorConfig(const FlightConfigurationId& fcid) override
    {
        return m_motors.get(fcid);
    }
    [[nodiscard]] const MotorConfiguration& getMotorConfig(
        const FlightConfigurationId& fcid) const override
    {
        return m_motors.get(fcid);
    }

    void setMotorConfig(std::optional<MotorConfiguration> newMotorConfig,
                        const FlightConfigurationId&      fcid) override
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

    /// The first layout of ClusterConfiguration::configurations() with getMotorCount() members,
    /// else single().
    [[nodiscard]] const ClusterConfiguration& getClusterConfiguration() const override
    {
        for (const ClusterConfiguration& cluster : ClusterConfiguration::configurations())
        {
            if (cluster.getClusterCount() == m_motorCount)
            {
                return cluster;
            }
        }
        return ClusterConfiguration::single();
    }

    [[nodiscard]] int getMotorCount() const override { return m_motorCount; }

    [[nodiscard]] int getMotorCountIncludingAssemblyCopies() const override
    {
        int multiplier = 1;
        for (const RocketComponent* parent : getParentAssemblies())
        {
            multiplier *= parent->getInstanceCount();
        }
        return getMotorCount() * multiplier;
    }

    [[nodiscard]] double getMotorOverhang() const override { return m_overhang; }
    void                 setMotorOverhang(double overhang) override
    {
        if (MathUtil::equals(m_overhang, overhang))
        {
            return;
        }
        m_overhang = overhang;
        fireComponentChangeEvent(ComponentChangeEvent::kBothChange);
    }

    [[nodiscard]] double getMotorMountDiameter() const override { return getInnerRadius() * 2; }

    [[nodiscard]] Coordinate getMotorPosition(const FlightConfigurationId& fcid) const override
    {
        const Motor* motor = m_motors.get(fcid).getMotor().get();
        if (motor == nullptr)
        {
            bug("No motor with id " + fcid.toString() + " defined.");
        }
        return Coordinate{getLength() - motor->getLength() + getMotorOverhang()};
    }

    [[nodiscard]] std::string toMotorDebug(const Preferences& preferences) const override
    {
        return m_motors.toDebug(preferences);
    }

    // ---- FlightConfigurableComponent

    void reset(const FlightConfigurationId& fcid) override { m_motors.reset(fcid); }

    void copyFlightConfiguration(const FlightConfigurationId& oldConfigId,
                                 const FlightConfigurationId& newConfigId) override
    {
        m_motors.copyFlightConfiguration(oldConfigId, newConfigId);
    }

    // ---- test controls

    /// Sets the number of motors (a cluster's size).
    void setMotorCount(int count) { m_motorCount = count; }

    /// Gives @p fcid a configuration with @p motor and @p ejectionDelay (as TestRockets does).
    MotorConfiguration& addMotor(const FlightConfigurationId& fcid,
                                 std::shared_ptr<const Motor> motor, double ejectionDelay = 0.0)
    {
        MotorConfiguration config{*this, fcid};
        config.setMotor(std::move(motor));
        config.setEjectionDelay(ejectionDelay);
        setMotorConfig(std::move(config), fcid);
        return getMotorConfig(fcid);
    }

protected:
    [[nodiscard]] std::unique_ptr<RocketComponent> cloneShallow() const override
    {
        return std::make_unique<TestMotorMount>(*this);
    }

private:
    MotorConfigurationSet m_motors;
    double                m_overhang{0.0};
    int                   m_motorCount{1};
    bool                  m_isActingMount{false};
};

}  // namespace QtRocket::Test
