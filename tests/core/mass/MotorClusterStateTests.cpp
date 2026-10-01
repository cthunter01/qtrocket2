#include "QtRocket/mass/MotorClusterState.h"

#include <cmath>
#include <limits>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "QtRocket/mass/ThrustState.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "rocket/TestMotorMount.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::AxialMethod;
using QtRocket::AxialStage;
using QtRocket::BugError;
using QtRocket::ComponentKind;
using QtRocket::Coordinate;
using QtRocket::FlightConfigurationId;
using QtRocket::IgnitionEvent;
using QtRocket::Motor;
using QtRocket::MotorClusterState;
using QtRocket::MotorConfiguration;
using QtRocket::MotorMount;
using QtRocket::PodSet;
using QtRocket::RadiusMethod;
using QtRocket::Rocket;
using QtRocket::ThrustCurveMotor;
using QtRocket::ThrustState;
using QtRocket::Test::TestMotorMount;

constexpr double kInfinity = std::numeric_limits<double>::infinity();

/// A rocket with one stage holding a motor mount (0.07 m) with an A8 motor (ejection delay 3 s)
/// in one flight configuration; events enabled.
class MotorClusterStateTest : public ::testing::Test
{
protected:
    MotorClusterStateTest()
    {
        m_stage = &m_rocket.addChild(std::make_unique<AxialStage>());
        m_stage->setName("Stage");
        m_mount = &m_stage->addChild(TestMotorMount::make(0.07, 0.009));
        m_mount->setName("Mount");
        m_mount->setMotorMount(true);
        m_motor = QtRocket::Test::motorA8();
        m_mount->addMotor(m_fcid, m_motor, 3.0);
        m_rocket.createFlightConfiguration(m_fcid);
        m_rocket.enableEvents();
    }

    [[nodiscard]] MotorConfiguration& config() { return m_mount->getMotorConfig(m_fcid); }

    Rocket                                  m_rocket;
    FlightConfigurationId                   m_fcid{QtRocket::Test::testFcid(0)};
    AxialStage*                             m_stage{nullptr};
    TestMotorMount*                         m_mount{nullptr};
    std::shared_ptr<const ThrustCurveMotor> m_motor;
};

TEST_F(MotorClusterStateTest, StartsArmedWithEveryTimeInTheFuture)
{
    const MotorClusterState state{config()};

    EXPECT_EQ(state.getState(), ThrustState::ARMED);
    EXPECT_EQ(state.getIgnitionTime(), kInfinity);
    EXPECT_EQ(state.getCutOffTime(), kInfinity);
    EXPECT_EQ(state.getEjectionTime(), kInfinity);
    EXPECT_FALSE(state.isThrusting());
    EXPECT_FALSE(state.isDelaying());
    EXPECT_FALSE(state.isSpent());

    EXPECT_EQ(state.getMotor().get(), m_motor.get());
    EXPECT_EQ(state.getMotorCount(), 1);
    EXPECT_EQ(state.getThrustDuration(), m_motor->getBurnTimeEstimate());
    EXPECT_EQ(state.getBurnTime(), m_motor->getBurnTime());
    EXPECT_EQ(&state.getMount(), static_cast<const MotorMount*>(m_mount));
    EXPECT_EQ(state.getId(), config().getId());
    EXPECT_EQ(state.getConfig(), config());
    EXPECT_EQ(state.getEjectionDelay(), 3.0);
    EXPECT_EQ(state.getIgnitionEvent(), IgnitionEvent::AUTOMATIC);
    EXPECT_EQ(state.getNozzleExitDiameter(), 0.0);
}

TEST_F(MotorClusterStateTest, ConfigurationWithoutMotorIsABug)
{
    const MotorConfiguration empty{*m_mount, m_fcid};
    EXPECT_THROW(static_cast<void>(MotorClusterState{empty}), BugError);
}

TEST_F(MotorClusterStateTest, KeepsItsOwnCopyOfTheConfiguration)
{
    const MotorClusterState state{config()};
    config().setEjectionDelay(7.0);
    config().setIgnitionEvent(IgnitionEvent::NEVER);

    EXPECT_EQ(state.getEjectionDelay(), 3.0);
    EXPECT_EQ(state.getIgnitionEvent(), IgnitionEvent::AUTOMATIC);
}

TEST_F(MotorClusterStateTest, MotorCountCountsEveryAbsoluteInstanceOfTheMount)
{
    // A three-motor cluster in a two-pod pod set stands for six motors.
    auto& pods = m_mount->addChild(std::make_unique<PodSet>());
    pods.setInstanceCount(2);
    pods.setRadius(RadiusMethod::FREE, 0.05);
    auto& podMount = pods.addChild(TestMotorMount::make(0.07, 0.009));
    podMount.setInstances(
        {Coordinate{0, 0.01, 0}, Coordinate{0, -0.005, 0.0087}, Coordinate{0, -0.005, -0.0087}},
        {0, 0, 0});
    podMount.setMotorMount(true);
    podMount.addMotor(m_fcid, QtRocket::Test::motorG77());

    const MotorClusterState state{podMount.getMotorConfig(m_fcid)};
    EXPECT_EQ(state.getMotorCount(), 6);
}

TEST_F(MotorClusterStateTest, IgnitionStartsTheMotorTimeAndTheThrust)
{
    MotorClusterState state{config()};
    EXPECT_EQ(state.getMotorTime(5.0), 0.0);
    EXPECT_EQ(state.getThrust(1.0), 0.0);

    state.ignite(1.0);
    EXPECT_EQ(state.getState(), ThrustState::THRUSTING);
    EXPECT_TRUE(state.isThrusting());
    EXPECT_EQ(state.getIgnitionTime(), 1.0);

    EXPECT_EQ(state.getMotorTime(1.5), 0.5);
    EXPECT_EQ(state.getMotorTime(0.5), 0.0);  // before the ignition
    EXPECT_EQ(state.getThrust(1.5), m_motor->getThrust(0.5));
    EXPECT_EQ(state.getThrust(2.0), m_motor->getThrust(1.0));
    EXPECT_DOUBLE_EQ(state.getThrust(2.0), 9.0);  // the A8's peak

    // A second ignition is ignored.
    state.ignite(5.0);
    EXPECT_EQ(state.getIgnitionTime(), 1.0);
}

TEST_F(MotorClusterStateTest, MotorTimeFollowsJavasMathMax)
{
    MotorClusterState state{config()};
    state.ignite(2.0);
    EXPECT_TRUE(std::isnan(state.getMotorTime(std::numeric_limits<double>::quiet_NaN())));
    EXPECT_FALSE(std::signbit(state.getMotorTime(2.0)));
    EXPECT_EQ(state.getMotorTime(-1.0), 0.0);
    EXPECT_EQ(state.getMotorTime(kInfinity), kInfinity);
}

TEST_F(MotorClusterStateTest, ThrustScalesWithTheMotorCount)
{
    m_mount->setInstances({Coordinate{0, 0.01, 0}, Coordinate{0, -0.01, 0}}, {0, 0});
    MotorClusterState state{config()};
    ASSERT_EQ(state.getMotorCount(), 2);
    state.ignite(0.0);
    EXPECT_EQ(state.getThrust(0.25), 2 * m_motor->getThrust(0.25));
}

TEST_F(MotorClusterStateTest, BurnoutThenEjection)
{
    MotorClusterState state{config()};
    state.ignite(0.0);
    state.burnOut(2.0);
    EXPECT_EQ(state.getState(), ThrustState::DELAYING);
    EXPECT_TRUE(state.isDelaying());
    EXPECT_FALSE(state.isThrusting());
    EXPECT_EQ(state.getCutOffTime(), 2.0);
    EXPECT_EQ(state.getThrust(1.0), 0.0);

    // Burning out again changes nothing.
    state.cutOff(3.0);
    EXPECT_EQ(state.getCutOffTime(), 2.0);
    EXPECT_TRUE(state.isDelaying());

    state.expend(5.0);
    EXPECT_TRUE(state.isSpent());
    EXPECT_EQ(state.getEjectionTime(), 5.0);

    state.expend(6.0);
    EXPECT_EQ(state.getEjectionTime(), 5.0);
}

TEST_F(MotorClusterStateTest, TransitionsFromTheWrongStateAreIgnored)
{
    MotorClusterState state{config()};
    state.burnOut(1.0);
    EXPECT_EQ(state.getState(), ThrustState::ARMED);
    EXPECT_EQ(state.getCutOffTime(), kInfinity);

    state.expend(1.0);
    EXPECT_EQ(state.getState(), ThrustState::ARMED);
    EXPECT_EQ(state.getEjectionTime(), kInfinity);

    state.ignite(0.5);
    state.expend(1.0);  // still thrusting
    EXPECT_EQ(state.getState(), ThrustState::THRUSTING);
}

TEST_F(MotorClusterStateTest, PluggedMotorIsSpentAtBurnout)
{
    config().setEjectionDelay(Motor::kPluggedDelay);
    MotorClusterState state{config()};
    EXPECT_TRUE(state.isPlugged());
    EXPECT_FALSE(state.hasEjectionCharge());

    state.ignite(0.0);
    state.burnOut(2.0);
    EXPECT_TRUE(state.isSpent());
    EXPECT_EQ(state.getCutOffTime(), 2.0);
}

TEST_F(MotorClusterStateTest, PluggedMotorBurnedOutBeforeIgnitionIsSpentToo)
{
    // Java sets SPENT for a plugged motor whatever the state burnOut() finds.
    config().setEjectionDelay(Motor::kPluggedDelay);
    MotorClusterState state{config()};
    state.burnOut(1.0);
    EXPECT_TRUE(state.isSpent());
    EXPECT_EQ(state.getCutOffTime(), kInfinity);
}

TEST_F(MotorClusterStateTest, ResetReturnsToPreflight)
{
    MotorClusterState state{config()};
    state.ignite(0.0);
    state.burnOut(2.0);
    state.expend(5.0);
    state.reset();

    EXPECT_EQ(state.getState(), ThrustState::ARMED);
    EXPECT_EQ(state.getIgnitionTime(), kInfinity);
    EXPECT_EQ(state.getCutOffTime(), kInfinity);
    EXPECT_EQ(state.getEjectionTime(), kInfinity);
}

TEST_F(MotorClusterStateTest, PropellantMasses)
{
    const MotorClusterState state{config()};
    EXPECT_DOUBLE_EQ(state.getPropellantMass(), 0.0164 - 0.0131);
    EXPECT_EQ(state.getPropellantMass(), m_motor->getLaunchMass() - m_motor->getBurnoutMass());
    // Java subtracts the burnout mass from the motor's propellant mass, as here.
    EXPECT_EQ(state.getPropellantMass(0.5),
              m_motor->getPropellantMass(0.5) - m_motor->getBurnoutMass());
}

TEST_F(MotorClusterStateTest, DescriptionAndString)
{
    MotorClusterState state{config()};
    const std::string debugName = m_mount->getDebugName();
    ASSERT_LT(debugName.size(), 32U);
    EXPECT_EQ(state.toDescription(),
              std::string(32 - debugName.size(), ' ') + debugName + " /   A8 - Armed");
    state.ignite(0.0);
    EXPECT_EQ(state.toDescription(),
              std::string(32 - debugName.size(), ' ') + debugName + " /   A8 - Thrusting");
    EXPECT_EQ(state.toString(), "A8");
}

TEST_F(MotorClusterStateTest, NonConstMountIsTheMountsComponent)
{
    MotorClusterState state{config()};
    EXPECT_EQ(&QtRocket::asComponent(state.getMount()),
              static_cast<QtRocket::RocketComponent*>(m_mount));
}

}  // namespace
