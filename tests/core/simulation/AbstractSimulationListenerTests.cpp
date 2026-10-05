#include "QtRocket/simulation/listeners/AbstractSimulationListener.h"

#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/mass/RigidBody.h"
#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/simulation/AccelerationData.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/exception/SimulationCancelledException.h"
#include "QtRocket/simulation/exception/SimulationException.h"
#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"
#include "QtRocket/simulation/listeners/SimulationComputationListener.h"
#include "QtRocket/simulation/listeners/SimulationEventListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Quaternion.h"
#include "simulation/SimulationStatusSupport.h"
#include "simulation/SimulationTestSupport.h"

namespace
{

using QtRocket::AbstractSimulationListener;
using QtRocket::AccelerationData;
using QtRocket::AerodynamicForces;
using QtRocket::AtmosphericConditions;
using QtRocket::CloneableSimulationListener;
using QtRocket::Coordinate;
using QtRocket::FlightConditions;
using QtRocket::FlightEvent;
using QtRocket::Quaternion;
using QtRocket::RigidBody;
using QtRocket::SimulationCancelledException;
using QtRocket::SimulationComputationListener;
using QtRocket::SimulationEventListener;
using QtRocket::SimulationException;
using QtRocket::SimulationListener;
using QtRocket::SimulationStatus;
using QtRocket::Test::bugText;
using QtRocket::Test::EventTestRocket;
using QtRocket::Test::TestStatus;

// Java's hierarchy: SimulationComputationListener extends SimulationListener,
// SimulationEventListener stands alone, AbstractSimulationListener implements all three.
static_assert(std::is_base_of_v<SimulationListener, SimulationComputationListener>);
static_assert(!std::is_base_of_v<SimulationListener, SimulationEventListener>);
static_assert(std::is_base_of_v<SimulationListener, AbstractSimulationListener>);
static_assert(std::is_base_of_v<SimulationComputationListener, AbstractSimulationListener>);
static_assert(std::is_base_of_v<SimulationEventListener, AbstractSimulationListener>);
static_assert(std::is_abstract_v<SimulationListener>);
static_assert(std::is_abstract_v<SimulationEventListener>);
static_assert(std::is_abstract_v<SimulationComputationListener>);
static_assert(!std::is_abstract_v<AbstractSimulationListener>);
static_assert(std::is_default_constructible_v<AbstractSimulationListener>);
// A listener is copied by clone() only.
static_assert(!std::is_copy_constructible_v<AbstractSimulationListener>);
static_assert(!std::is_copy_assignable_v<AbstractSimulationListener>);
static_assert(std::has_virtual_destructor_v<SimulationListener>);
static_assert(std::has_virtual_destructor_v<SimulationEventListener>);

/// What a listener written for a test looks like: it counts the steps and remembers the events
/// it was asked about.
class CountingListener final : public CloneableSimulationListener<CountingListener>
{
public:
    void postStep(SimulationStatus& /*status*/) override { m_steps++; }

    [[nodiscard]] bool handleFlightEvent(SimulationStatus& /*status*/,
                                         const FlightEvent& event) override
    {
        m_handled.push_back(event.getType());
        return event.getType() != FlightEvent::Type::TUMBLE;
    }

    [[nodiscard]] double preGravityModel(SimulationStatus& /*status*/) override { return 1.62; }

    [[nodiscard]] int                                   steps() const noexcept { return m_steps; }
    [[nodiscard]] const std::vector<FlightEvent::Type>& handled() const noexcept
    {
        return m_handled;
    }

private:
    int                            m_steps{0};
    std::vector<FlightEvent::Type> m_handled;
};

/// A listener that cancels the simulation and records how it ended.
class CancellingListener final : public CloneableSimulationListener<CancellingListener>
{
public:
    void postStep(SimulationStatus& /*status*/) override
    {
        throw SimulationCancelledException("The simulation was interrupted.");
    }

    void endSimulation(SimulationStatus& /*status*/, const SimulationException* exception) override
    {
        m_ending = exception == nullptr ? "normal" : exception->what();
    }

    [[nodiscard]] const std::string& ending() const noexcept { return m_ending; }

private:
    std::string m_ending{"running"};
};

/// A listener derived from the base directly, without a clone() of its own.
class ForgetfulListener final : public AbstractSimulationListener
{
public:
    [[nodiscard]] bool preStep(SimulationStatus& /*status*/) override { return false; }
};

/// A listener that implements the plain interface only.
class PlainListener final : public SimulationListener
{
public:
    void startSimulation(SimulationStatus& /*status*/) override { }
    void endSimulation(SimulationStatus& /*status*/,
                       const SimulationException* /*exception*/) override
    {
    }
    void startSimulationBranch(SimulationStatus& /*status*/) override { }
    void endSimulationBranch(SimulationStatus& /*status*/,
                             const SimulationException* /*exception*/) override
    {
    }
    [[nodiscard]] bool preStep(SimulationStatus& /*status*/) override { return true; }
    void               postStep(SimulationStatus& /*status*/) override { }
    [[nodiscard]] bool isSystemListener() const override { return false; }
    [[nodiscard]] std::shared_ptr<SimulationListener> clone() const override
    {
        return std::make_shared<PlainListener>();
    }
};

/// A listener that implements the plain interface and the event hooks, but not the computation
/// hooks.
class EventOnlyListener final : public SimulationListener, public SimulationEventListener
{
public:
    void startSimulation(SimulationStatus& /*status*/) override { }
    void endSimulation(SimulationStatus& /*status*/,
                       const SimulationException* /*exception*/) override
    {
    }
    void startSimulationBranch(SimulationStatus& /*status*/) override { }
    void endSimulationBranch(SimulationStatus& /*status*/,
                             const SimulationException* /*exception*/) override
    {
    }
    [[nodiscard]] bool preStep(SimulationStatus& /*status*/) override { return true; }
    void               postStep(SimulationStatus& /*status*/) override { }
    [[nodiscard]] bool isSystemListener() const override { return false; }
    [[nodiscard]] std::shared_ptr<SimulationListener> clone() const override
    {
        return std::make_shared<EventOnlyListener>();
    }
    [[nodiscard]] bool addFlightEvent(SimulationStatus& /*status*/,
                                      const FlightEvent& /*event*/) override
    {
        return false;
    }
    [[nodiscard]] bool handleFlightEvent(SimulationStatus& /*status*/,
                                         const FlightEvent& /*event*/) override
    {
        return false;
    }
    [[nodiscard]] bool motorIgnition(SimulationStatus& /*status*/,
                                     const QtRocket::MotorConfigurationId& /*motorId*/,
                                     const QtRocket::MotorMount& /*mount*/,
                                     QtRocket::MotorClusterState& /*instance*/) override
    {
        return false;
    }
    [[nodiscard]] bool recoveryDeviceDeployment(
        SimulationStatus& /*status*/, const QtRocket::RecoveryDevice& /*recoveryDevice*/) override
    {
        return false;
    }
};

// ============================================================================ the defaults

TEST(AbstractSimulationListener, TheStepAndSimulationHooksLetTheSimulationGoOn)
{
    TestStatus                 fixture;
    SimulationStatus&          status = fixture.status;
    AbstractSimulationListener listener;
    const SimulationException  exception("failed");

    EXPECT_NO_THROW(listener.startSimulation(status));
    EXPECT_NO_THROW(listener.endSimulation(status, nullptr));
    EXPECT_NO_THROW(listener.endSimulation(status, &exception));
    EXPECT_NO_THROW(listener.startSimulationBranch(status));
    EXPECT_NO_THROW(listener.endSimulationBranch(status, nullptr));
    EXPECT_NO_THROW(listener.endSimulationBranch(status, &exception));
    EXPECT_TRUE(listener.preStep(status));
    EXPECT_NO_THROW(listener.postStep(status));
    EXPECT_FALSE(listener.isSystemListener());
}

TEST(AbstractSimulationListener, TheEventHooksLetEverythingHappen)
{
    const EventTestRocket      r;
    TestStatus                 fixture;
    SimulationStatus&          status = fixture.status;
    AbstractSimulationListener listener;
    const FlightEvent          event{FlightEvent::Type::IGNITION, 0.0, r.sustainerMount, r.state};

    EXPECT_TRUE(listener.addFlightEvent(status, event));
    EXPECT_TRUE(listener.handleFlightEvent(status, event));
    EXPECT_TRUE(listener.motorIgnition(status, r.state->getId(), *r.sustainerMount, *r.state));
    EXPECT_TRUE(listener.recoveryDeviceDeployment(status, *r.chute));
}

TEST(AbstractSimulationListener, ThePreComputationHooksChangeNothing)
{
    TestStatus                 fixture;
    SimulationStatus&          status = fixture.status;
    AbstractSimulationListener listener;

    EXPECT_EQ(listener.preAccelerationCalculation(status), std::nullopt);
    EXPECT_FALSE(listener.preAerodynamicCalculation(status).has_value());
    EXPECT_FALSE(listener.preAtmosphericModel(status).has_value());
    EXPECT_FALSE(listener.preFlightConditions(status).has_value());
    EXPECT_TRUE(std::isnan(listener.preGravityModel(status)));
    EXPECT_FALSE(listener.preMassCalculation(status).has_value());
    EXPECT_TRUE(std::isnan(listener.preSimpleThrustCalculation(status)));
    EXPECT_EQ(listener.preWindModel(status), std::nullopt);
}

TEST(AbstractSimulationListener, ThePostComputationHooksChangeNothing)
{
    TestStatus                 fixture;
    SimulationStatus&          status = fixture.status;
    AbstractSimulationListener listener;
    const AccelerationData     acceleration(Coordinate{1, 2, 3}, Coordinate{4, 5, 6}, std::nullopt,
                                            std::nullopt, Quaternion{});

    EXPECT_FALSE(listener.postAccelerationCalculation(status, acceleration).has_value());
    EXPECT_FALSE(listener.postAerodynamicCalculation(status, AerodynamicForces{}).has_value());
    EXPECT_FALSE(listener.postAtmosphericModel(status, AtmosphericConditions{}).has_value());
    EXPECT_FALSE(listener.postFlightConditions(status, FlightConditions{}).has_value());
    EXPECT_TRUE(std::isnan(listener.postGravityModel(status, 9.81)));
    EXPECT_FALSE(listener.postMassCalculation(status, RigidBody::kEmpty).has_value());
    EXPECT_TRUE(std::isnan(listener.postSimpleThrustCalculation(status, 12.5)));
    EXPECT_FALSE(listener.postWindModel(status, Coordinate{1, 2, 3}).has_value());
}

// ============================================================================ the interfaces

TEST(SimulationListeners, AListenerIsAskedWhichInterfacesItImplements)
{
    // SimulationListenerHelper's `l instanceof SimulationEventListener`.
    const std::shared_ptr<SimulationListener> full = std::make_shared<AbstractSimulationListener>();
    const std::shared_ptr<SimulationListener> plain    = std::make_shared<PlainListener>();
    const std::shared_ptr<SimulationListener> events   = std::make_shared<EventOnlyListener>();
    const std::shared_ptr<SimulationListener> counting = std::make_shared<CountingListener>();

    EXPECT_NE(dynamic_cast<SimulationEventListener*>(full.get()), nullptr);
    EXPECT_NE(dynamic_cast<SimulationComputationListener*>(full.get()), nullptr);
    EXPECT_EQ(dynamic_cast<SimulationEventListener*>(plain.get()), nullptr);
    EXPECT_EQ(dynamic_cast<SimulationComputationListener*>(plain.get()), nullptr);
    EXPECT_NE(dynamic_cast<SimulationEventListener*>(events.get()), nullptr);
    EXPECT_EQ(dynamic_cast<SimulationComputationListener*>(events.get()), nullptr);
    EXPECT_NE(dynamic_cast<SimulationEventListener*>(counting.get()), nullptr);
    EXPECT_NE(dynamic_cast<SimulationComputationListener*>(counting.get()), nullptr);

    // A computation listener is a listener: one object, reached either way.
    auto* computation = dynamic_cast<SimulationComputationListener*>(full.get());
    ASSERT_NE(computation, nullptr);
    const SimulationListener* base = computation;
    EXPECT_EQ(base, full.get());
}

TEST(SimulationListeners, TheHooksAreReachedThroughTheInterfaces)
{
    const EventTestRocket                     r;
    TestStatus                                fixture;
    SimulationStatus&                         status   = fixture.status;
    const std::shared_ptr<CountingListener>   counting = std::make_shared<CountingListener>();
    const std::shared_ptr<SimulationListener> listener = counting;

    listener->postStep(status);
    listener->postStep(status);
    EXPECT_EQ(counting->steps(), 2);
    EXPECT_TRUE(listener->preStep(status)) << "not overridden: the default";

    auto* events = dynamic_cast<SimulationEventListener*>(listener.get());
    ASSERT_NE(events, nullptr);
    EXPECT_TRUE(events->handleFlightEvent(status, FlightEvent{FlightEvent::Type::APOGEE, 1.0}));
    EXPECT_FALSE(events->handleFlightEvent(status, FlightEvent{FlightEvent::Type::TUMBLE, 2.0}));
    EXPECT_TRUE(events->addFlightEvent(status, FlightEvent{FlightEvent::Type::TUMBLE, 2.0}));
    EXPECT_EQ(counting->handled(), (std::vector<FlightEvent::Type>{FlightEvent::Type::APOGEE,
                                                                   FlightEvent::Type::TUMBLE}));

    auto* computation = dynamic_cast<SimulationComputationListener*>(listener.get());
    ASSERT_NE(computation, nullptr);
    EXPECT_EQ(computation->preGravityModel(status), 1.62);
    EXPECT_TRUE(std::isnan(computation->postGravityModel(status, 9.81)));
}

TEST(SimulationListeners, AHookMayThrowASimulationException)
{
    TestStatus                                fixture;
    SimulationStatus&                         status     = fixture.status;
    const std::shared_ptr<CancellingListener> cancelling = std::make_shared<CancellingListener>();
    const std::shared_ptr<SimulationListener> listener   = cancelling;

    EXPECT_EQ(cancelling->ending(), "running");
    try
    {
        listener->postStep(status);
        ADD_FAILURE() << "postStep() did not throw";
    }
    catch (const SimulationException& exception)
    {
        // What the engine does: it tells the listeners how the simulation ended.
        listener->endSimulation(status, &exception);
    }
    EXPECT_EQ(cancelling->ending(), "The simulation was interrupted.");
    listener->endSimulation(status, nullptr);
    EXPECT_EQ(cancelling->ending(), "normal");
}

// ============================================================================ clone()

// The clone() of a listener derived through CloneableSimulationListener is tested in
// CloneableSimulationListenerTests.cpp.

TEST(SimulationListenerClone, TheBaseClonesItself)
{
    const AbstractSimulationListener          listener;
    const std::shared_ptr<SimulationListener> clone = listener.clone();
    ASSERT_NE(clone, nullptr);
    EXPECT_NE(clone.get(), static_cast<const SimulationListener*>(&listener));
    const SimulationListener& cloned = *clone;
    EXPECT_TRUE(typeid(cloned) == typeid(AbstractSimulationListener));
    EXPECT_FALSE(clone->isSystemListener());
}

TEST(SimulationListenerClone, AListenerWithoutACloneOfItsOwnIsNotSliced)
{
    // Java's Object.clone() would copy the ForgetfulListener; a copy made by the base would be a
    // listener that does nothing.
    const ForgetfulListener forgetful;
    EXPECT_EQ(bugText([&forgetful] { static_cast<void>(forgetful.clone()); }),
              "clone() is not overridden by a simulation listener: derive it from "
              "CloneableSimulationListener");
}

}  // namespace
