#include "QtRocket/rocket/MotorConfiguration.h"

#include <limits>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/MotorConfigurationId.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Inertia.h"
#include "QtRocket/util/ModId.h"
#include "rocket/TestMotorMount.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::BugError;
using QtRocket::ComponentKind;
using QtRocket::Coordinate;
using QtRocket::ErrorCode;
using QtRocket::FlightConfigurationId;
using QtRocket::IgnitionEvent;
using QtRocket::InMemoryPreferences;
using QtRocket::Motor;
using QtRocket::MotorConfiguration;
using QtRocket::MotorConfigurationId;
using QtRocket::ThrustCurveMotor;
using QtRocket::Test::motorC6;
using QtRocket::Test::motorD21;
using QtRocket::Test::motorG77;
using QtRocket::Test::motorM1350;
using QtRocket::Test::TestMotorMount;

/// A body tube mount (0.3 m, radius 0.02 m, overhang 0.01 m) and an inner tube mount.
class MotorConfigurationTest : public ::testing::Test
{
protected:
    MotorConfigurationTest()
    {
        m_mount = TestMotorMount::make(0.3, 0.02);
        m_mount->setInnerRadius(0.019);
        m_mount->setMotorOverhang(0.01);
        m_mount->setName("Mount");
        m_inner = TestMotorMount::make(0.1, 0.01, ComponentKind::INNER_TUBE);
        m_inner->setMotorCount(3);
    }

    std::unique_ptr<TestMotorMount> m_mount;
    std::unique_ptr<TestMotorMount> m_inner;
    FlightConfigurationId           m_fcid;
    InMemoryPreferences             m_preferences;
};

TEST_F(MotorConfigurationTest, NewConfigurationIsEmptyAndAutomatic)
{
    const MotorConfiguration config{*m_mount, m_fcid};
    EXPECT_EQ(&config.getMount(), static_cast<QtRocket::MotorMount*>(m_mount.get()));
    EXPECT_EQ(config.getFcid(), m_fcid);
    EXPECT_EQ(config.getMid(), (MotorConfigurationId{m_mount->getId(), m_fcid.key()}));
    EXPECT_EQ(config.getId(), config.getMid());
    EXPECT_TRUE(config.isEmpty());
    EXPECT_FALSE(config.hasMotor());
    EXPECT_EQ(config.getMotor(), nullptr);
    EXPECT_EQ(config.getEjectionDelay(), 0.0);
    EXPECT_EQ(config.getNozzleExitDiameter(), 0.0);
    EXPECT_EQ(config.getIgnitionEvent(), IgnitionEvent::AUTOMATIC);
    EXPECT_EQ(config.getIgnitionDelay(), 0.0);
    EXPECT_FALSE(config.hasIgnitionOverride());
    EXPECT_GT(config.getModId(), QtRocket::ModId::zero());
    EXPECT_EQ(config.modId(), config.getModId());
    EXPECT_EQ(config.toMotorName(m_preferences), "None");
}

TEST_F(MotorConfigurationTest, EmptyConfigurationHasNoGeometry)
{
    const MotorConfiguration config{*m_mount, m_fcid};
    EXPECT_EQ(config.getX(), 0.0);
    EXPECT_TRUE(config.getPosition().exactlyEquals(Coordinate::kZero));
    EXPECT_EQ(config.getUnitLongitudinalInertia(), 0.0);
    EXPECT_EQ(config.getUnitRotationalInertia(), 0.0);
    EXPECT_EQ(config.getPropellantMass(), 0.0);
    EXPECT_THROW(static_cast<void>(config.getOffset()), BugError) << "Java: NullPointerException";
}

TEST_F(MotorConfigurationTest, MotorGeometry)
{
    MotorConfiguration                            config{*m_mount, m_fcid};
    const std::shared_ptr<const ThrustCurveMotor> motor = motorD21();
    config.setMotor(motor);
    EXPECT_TRUE(config.hasMotor());
    EXPECT_EQ(config.getMotor().get(), motor.get());

    // mount length - motor length + overhang
    EXPECT_DOUBLE_EQ(config.getX(), 0.3 - 0.070 + 0.01);
    EXPECT_TRUE(config.getPosition().exactlyEquals(Coordinate{config.getX(), 0, 0}));
    EXPECT_DOUBLE_EQ(config.getOffset().x, 0.3 + 0.01 - 0.070);
    EXPECT_DOUBLE_EQ(config.getUnitLongitudinalInertia(),
                     QtRocket::Inertia::filledCylinderLongitudinal(0.009, 0.070));
    EXPECT_DOUBLE_EQ(config.getUnitRotationalInertia(),
                     QtRocket::Inertia::filledCylinderRotational(0.009));
    EXPECT_DOUBLE_EQ(config.getPropellantMass(), motor->getLaunchMass() - motor->getBurnoutMass());
}

TEST_F(MotorConfigurationTest, MotorName)
{
    MotorConfiguration config{*m_mount, m_fcid};
    config.setMotor(motorG77());
    config.setEjectionDelay(7);
    // The designation by default (MotorNameColumn is true), with the delay.
    EXPECT_EQ(config.toMotorName(m_preferences), "G77-7");
    m_preferences.setMotorNameColumn(false);
    EXPECT_EQ(config.toMotorName(m_preferences), "G77-7") << "the common name, the same here";
    config.setEjectionDelay(Motor::kPluggedDelay);
    EXPECT_EQ(config.toMotorName(m_preferences), "G77-P");
}

TEST_F(MotorConfigurationTest, ANewMotorResetsTheNozzleExitDiameter)
{
    MotorConfiguration config{*m_mount, m_fcid};
    const auto         motor = motorM1350();
    config.setMotor(motor);
    ASSERT_TRUE(config.setNozzleExitDiameter(0.05).has_value());
    EXPECT_EQ(config.getNozzleExitDiameter(), 0.05);

    config.setMotor(motor);
    EXPECT_EQ(config.getNozzleExitDiameter(), 0.05) << "the same motor keeps it";

    config.setMotor(motorM1350());
    EXPECT_EQ(config.getNozzleExitDiameter(), 0.0) << "another motor object resets it";
}

/// Whether setNozzleExitDiameter(@p bad) fails with Java's message for a value that is not finite
/// or negative, and leaves the diameter as it was.
::testing::AssertionResult rejectsDiameter(MotorConfiguration& config, double bad)
{
    const double                 before = config.getNozzleExitDiameter();
    const QtRocket::Result<void> result = config.setNozzleExitDiameter(bad);
    if (result.has_value())
    {
        return ::testing::AssertionFailure() << bad << " was accepted";
    }
    if (result.error().code != ErrorCode::INVALID_ARGUMENT ||
        result.error().message != "Nozzle exit diameter must be finite and non-negative")
    {
        return ::testing::AssertionFailure() << result.error().toString();
    }
    if (config.getNozzleExitDiameter() != before)
    {
        return ::testing::AssertionFailure() << "the diameter changed";
    }
    return ::testing::AssertionSuccess();
}

TEST_F(MotorConfigurationTest, NozzleExitDiameterIsValidated)
{
    MotorConfiguration config{*m_mount, m_fcid};
    EXPECT_TRUE(config.setNozzleExitDiameter(1.0).has_value()) << "no motor: no upper bound";
    EXPECT_EQ(config.getNozzleExitDiameter(), 1.0);

    EXPECT_TRUE(rejectsDiameter(config, -0.001));
    EXPECT_TRUE(rejectsDiameter(config, std::numeric_limits<double>::quiet_NaN()));
    EXPECT_TRUE(rejectsDiameter(config, std::numeric_limits<double>::infinity()));
    EXPECT_EQ(config.getNozzleExitDiameter(), 1.0);

    config.setMotor(motorD21());  // 18 mm, resets the diameter
    const QtRocket::Result<void> tooLarge = config.setNozzleExitDiameter(0.019);
    ASSERT_FALSE(tooLarge.has_value());
    EXPECT_EQ(tooLarge.error().message, "Nozzle exit diameter must not exceed the motor diameter");
    EXPECT_TRUE(config.setNozzleExitDiameter(0.018).has_value()) << "the motor's own diameter";
    EXPECT_TRUE(config.setNozzleExitDiameter(0.0).has_value()) << "0: unknown";
}

TEST_F(MotorConfigurationTest, IgnitionSettersMarkTheOverride)
{
    MotorConfiguration config{*m_mount, m_fcid};
    config.setIgnitionEvent(IgnitionEvent::BURNOUT);
    EXPECT_TRUE(config.hasIgnitionOverride());
    EXPECT_EQ(config.getIgnitionEvent(), IgnitionEvent::BURNOUT);

    MotorConfiguration other{*m_mount, m_fcid};
    other.setIgnitionDelay(2.5);
    EXPECT_TRUE(other.hasIgnitionOverride());
    EXPECT_EQ(other.getIgnitionDelay(), 2.5);
    EXPECT_EQ(other.getIgnitionEvent(), IgnitionEvent::AUTOMATIC);
}

TEST_F(MotorConfigurationTest, UseDefaultIgnitionLeavesTheOverrideSetAsJavaDoes)
{
    MotorConfiguration config{*m_mount, m_fcid};
    config.setIgnitionEvent(IgnitionEvent::NEVER);
    config.setIgnitionDelay(3);
    config.useDefaultIgnition();
    EXPECT_EQ(config.getIgnitionEvent(), IgnitionEvent::AUTOMATIC);
    EXPECT_EQ(config.getIgnitionDelay(), 0.0);
    // Java clears the flag, then its setters set it again.
    EXPECT_TRUE(config.hasIgnitionOverride());
}

TEST_F(MotorConfigurationTest, ASourceGivesItsSettingsIncludingTheIgnition)
{
    // The .ork loader makes each configuration from the mount's default one, which carries the
    // mount's default ignition.
    MotorConfiguration& defaults = m_mount->getDefaultMotorConfig();
    defaults.setIgnitionEvent(IgnitionEvent::EJECTION_CHARGE);
    defaults.setIgnitionDelay(1.5);

    const FlightConfigurationId other;
    const MotorConfiguration    fromDefault{*m_mount, other, defaults};
    EXPECT_EQ(fromDefault.getFcid(), other);
    EXPECT_EQ(fromDefault.getMid(), (MotorConfigurationId{m_mount->getId(), other.key()}));
    EXPECT_EQ(fromDefault.getIgnitionEvent(), IgnitionEvent::EJECTION_CHARGE);
    EXPECT_EQ(fromDefault.getIgnitionDelay(), 1.5);
    EXPECT_TRUE(fromDefault.hasIgnitionOverride());

    MotorConfiguration source{*m_mount, m_fcid};
    source.setMotor(motorM1350());
    source.setEjectionDelay(6);
    ASSERT_TRUE(source.setNozzleExitDiameter(0.04).has_value());
    const MotorConfiguration copied{*m_inner, other, source};
    EXPECT_EQ(&copied.getMount(), static_cast<QtRocket::MotorMount*>(m_inner.get()));
    EXPECT_EQ(copied.getMotor(), source.getMotor());
    EXPECT_EQ(copied.getEjectionDelay(), 6.0);
    EXPECT_EQ(copied.getNozzleExitDiameter(), 0.04);
    EXPECT_FALSE(copied.hasIgnitionOverride());
    EXPECT_EQ(copied.getIgnitionEvent(), IgnitionEvent::AUTOMATIC);
}

TEST_F(MotorConfigurationTest, CopyCloneAndCopyFrom)
{
    MotorConfiguration source{*m_mount, m_fcid};
    source.setMotor(motorC6());
    source.setEjectionDelay(5);
    source.setIgnitionEvent(IgnitionEvent::LAUNCH);
    source.setIgnitionDelay(0.5);

    const MotorConfiguration clone = source.clone();
    EXPECT_EQ(clone, source) << "equal: the same mid";
    EXPECT_EQ(clone.getFcid(), m_fcid);
    EXPECT_EQ(clone.getMotor(), source.getMotor());
    EXPECT_EQ(clone.getEjectionDelay(), 5.0);
    EXPECT_EQ(clone.getIgnitionEvent(), IgnitionEvent::LAUNCH);
    EXPECT_EQ(clone.getIgnitionDelay(), 0.5);
    EXPECT_TRUE(clone.hasIgnitionOverride());
    EXPECT_NE(clone.getModId(), source.getModId()) << "a new configuration, a new id";

    const FlightConfigurationId newId;
    const MotorConfiguration    copy = source.copy(newId);
    EXPECT_NE(copy, source) << "another flight configuration: another mid";
    EXPECT_EQ(copy.getFcid(), newId);
    EXPECT_EQ(copy.getMid(), (MotorConfigurationId{m_mount->getId(), newId.key()}));
    EXPECT_EQ(copy.getMotor(), source.getMotor());
    EXPECT_EQ(copy.getIgnitionEvent(), IgnitionEvent::LAUNCH);

    MotorConfiguration target{*m_inner, newId};
    target.copyFrom(source);
    EXPECT_EQ(target.getMotor(), source.getMotor());
    EXPECT_EQ(target.getEjectionDelay(), 5.0);
    EXPECT_EQ(target.getIgnitionDelay(), 0.5);
    EXPECT_TRUE(target.hasIgnitionOverride());
    EXPECT_EQ(&target.getMount(), static_cast<QtRocket::MotorMount*>(m_inner.get()))
        << "the mount, fcid and mid stay";
    EXPECT_EQ(target.getFcid(), newId);
    EXPECT_EQ(target.hashCode(), target.getMid().hashCode());
}

TEST_F(MotorConfigurationTest, MotorCountIsTheClusterOfAnInnerTube)
{
    const MotorConfiguration inInner{*m_inner, m_fcid};
    EXPECT_EQ(inInner.getMotorCount(), 3);
    const MotorConfiguration inBody{*m_mount, m_fcid};
    m_mount->setMotorCount(5);
    EXPECT_EQ(inBody.getMotorCount(), 1) << "only an inner tube is a cluster";
}

TEST_F(MotorConfigurationTest, Descriptions)
{
    MotorConfiguration config{*m_mount, m_fcid};
    EXPECT_EQ(config.toIgnitionDescription(), "AUTOMATIC + 0.0s ");
    config.setIgnitionEvent(IgnitionEvent::EJECTION_CHARGE);
    config.setIgnitionDelay(1.5);
    EXPECT_EQ(config.toIgnitionDescription(), "EJECTION_CHARGE + 1.5s ");

    config.setMotor(motorD21());
    config.setEjectionDelay(4);
    EXPECT_EQ(config.toDescription(m_preferences),
              "D21-4 in: " + m_mount->getDebugName() + " ign@: EJECTION_CHARGE + 1.5s ");

    const std::string detail = config.toDebugDetail(m_preferences);
    // Java: "[in: %28s][fcid %10s][mid %10s][    %8s ign@: %12s]".
    const std::string expected =
        "[in: " + std::string(28 - m_mount->getDebugName().size(), ' ') + m_mount->getDebugName() +
        "][fcid " + std::string(10 - m_fcid.toShortKey().size(), ' ') + m_fcid.toShortKey() +
        "][mid  " + config.getMid().toDebug() + "][       D21-4 ign@: EJECTION_CHARGE + 1.5s ]";
    EXPECT_EQ(detail, expected);
}

}  // namespace
