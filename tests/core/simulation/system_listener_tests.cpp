// Tests of the system listeners (simulation/listeners/system/): ApogeeEndListener,
// GroundHitListener, InterruptListener, OptimumCoastListener and
// RecoveryDeviceDeploymentEndListener. OpenRocket has no test of its own for them; what they do
// in a whole simulation is tested with the engine.

#include <memory>
#include <stop_token>
#include <string>
#include <string_view>
#include <typeinfo>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/SimulationConditions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/exception/SimulationCancelledException.h"
#include "QtRocket/simulation/exception/SimulationException.h"
#include "QtRocket/simulation/listeners/SimulationComputationListener.h"
#include "QtRocket/simulation/listeners/SimulationEventListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/simulation/listeners/SimulationListenerHelper.h"
#include "QtRocket/simulation/listeners/system/ApogeeEndListener.h"
#include "QtRocket/simulation/listeners/system/GroundHitListener.h"
#include "QtRocket/simulation/listeners/system/InterruptListener.h"
#include "QtRocket/simulation/listeners/system/OptimumCoastListener.h"
#include "QtRocket/simulation/listeners/system/RecoveryDeviceDeploymentEndListener.h"
#include "QtRocket/util/ModId.h"
#include "simulation/SimulationStatusSupport.h"

namespace
{

using QtRocket::ApogeeEndListener;
using QtRocket::FlightEvent;
using QtRocket::GroundHitListener;
using QtRocket::InterruptListener;
using QtRocket::ModId;
using QtRocket::OptimumCoastListener;
using QtRocket::RecoveryDeviceDeploymentEndListener;
using QtRocket::SimulationCancelledException;
using QtRocket::SimulationException;
using QtRocket::SimulationListener;
using QtRocket::SimulationListenerHelper;
using QtRocket::SimulationStatus;
using QtRocket::WarningSet;
using QtRocket::Test::TestStatus;

/// The warnings of a status.
[[nodiscard]] const WarningSet& warningsOf(const SimulationStatus& status)
{
    return *status.getWarnings();
}

/// The types of the queued events of @p status, in array order.
[[nodiscard]] std::vector<FlightEvent::Type> queued(const SimulationStatus& status)
{
    std::vector<FlightEvent::Type> types;
    for (const FlightEvent& event : status.getEventQueue())
    {
        types.push_back(event.getType());
    }
    return types;
}

/// Asks @p listener to handle an event of every type but @p except; none of them may queue
/// anything, and each must be let through.
void expectOtherEventsIgnored(SimulationListener& listener, FlightEvent::Type except)
{
    TestStatus fixture;
    auto*      events = dynamic_cast<QtRocket::SimulationEventListener*>(&listener);
    ASSERT_NE(events, nullptr);
    for (const FlightEvent::Type type : FlightEvent::kAllTypes)
    {
        if (type == except || type == FlightEvent::Type::SIM_WARN ||
            type == FlightEvent::Type::SIM_ABORT)
        {
            continue;  // the two need data of their own; they are no trigger either
        }
        EXPECT_TRUE(events->handleFlightEvent(fixture.status, FlightEvent(type, 1.0)));
    }
    EXPECT_TRUE(fixture.status.getEventQueue().empty());
}

/// What every stateless system listener has in common: a shared instance, which is a system
/// listener of all three kinds, clones of its own class, and AbstractSimulationListener's hooks
/// where it has none of its own. Gives what is wrong, one line each.
template <class Listener>
[[nodiscard]] std::string sharedSystemListenerProblems()
{
    std::string problems;
    const auto  expect = [&problems](bool holds, std::string_view what) {
        if (!holds)
        {
            problems += what;
            problems += '\n';
        }
    };
    const std::shared_ptr<Listener> instance = Listener::instance();
    if (instance == nullptr)
    {
        return "no instance";
    }
    expect(Listener::instance() == instance, "one object for every caller");
    expect(instance->isSystemListener(), "a system listener");
    const std::shared_ptr<SimulationListener> base  = instance;
    const std::shared_ptr<SimulationListener> clone = base->clone();
    if (clone == nullptr)
    {
        return problems + "no clone";
    }
    const SimulationListener& cloned = *clone;
    expect(clone != base, "a clone is another object");
    expect(typeid(cloned) == typeid(Listener), "a clone of its own class");
    expect(clone->isSystemListener(), "the clone is a system listener");
    expect(dynamic_cast<QtRocket::SimulationEventListener*>(clone.get()) != nullptr,
           "the clone listens to events");
    expect(dynamic_cast<QtRocket::SimulationComputationListener*>(clone.get()) != nullptr,
           "the clone listens to computations");

    // The hooks it does not override are AbstractSimulationListener's.
    TestStatus fixture;
    expect(instance->preStep(fixture.status), "preStep() lets the step be taken");
    instance->postStep(fixture.status);
    expect(instance->addFlightEvent(fixture.status, FlightEvent(FlightEvent::Type::APOGEE, 1)),
           "addFlightEvent() lets the event be added");
    return problems;
}

// ============================================================================ ApogeeEndListener

TEST(ApogeeEndListener, IsASharedSystemListener)
{
    EXPECT_EQ(sharedSystemListenerProblems<ApogeeEndListener>(), "");
}

TEST(ApogeeEndListener, QueuesTheEndOfTheSimulationAtApogee)
{
    TestStatus        fixture;
    SimulationStatus& status = fixture.status;
    status.setSimulationTime(7.5);
    const ModId       before = status.getModId();
    ApogeeEndListener listener;

    // The event's own time does not matter: the end is queued at the simulation time.
    EXPECT_TRUE(listener.handleFlightEvent(status, FlightEvent(FlightEvent::Type::APOGEE, 6.0)));

    ASSERT_EQ(status.getEventQueue().size(), 1U);
    const FlightEvent* end = status.getEventQueue().peek();
    EXPECT_EQ(end->getType(), FlightEvent::Type::SIMULATION_END);
    EXPECT_EQ(end->getTime(), 7.5);
    EXPECT_EQ(end->getSource(), nullptr);
    EXPECT_FALSE(end->hasData());
    EXPECT_EQ(status.getModId(), before) << "the queue is not the status";
}

TEST(ApogeeEndListener, LetsEveryOtherEventPass)
{
    ApogeeEndListener listener;
    expectOtherEventsIgnored(listener, FlightEvent::Type::APOGEE);
}

TEST(ApogeeEndListener, ThroughTheHelperItEndsTheSimulationWithoutAWarning)
{
    TestStatus fixture;
    fixture.conditions->getSimulationListenerList().push_back(ApogeeEndListener::instance());
    fixture.status.setSimulationTime(3.0);
    EXPECT_TRUE(SimulationListenerHelper::fireHandleFlightEvent(
        fixture.status, FlightEvent(FlightEvent::Type::APOGEE, 3.0)));
    EXPECT_EQ(queued(fixture.status),
              (std::vector<FlightEvent::Type>{FlightEvent::Type::SIMULATION_END}));
    EXPECT_TRUE(warningsOf(fixture.status).empty()) << "a system listener";
    EXPECT_TRUE(fixture.branch->getEvents().empty());
}

// ============================================================================ GroundHitListener

TEST(GroundHitListener, IsASharedSystemListener)
{
    EXPECT_EQ(sharedSystemListenerProblems<GroundHitListener>(), "");
}

TEST(GroundHitListener, QueuesTheEndOfTheSimulationAtTheGroundHit)
{
    TestStatus        fixture;
    SimulationStatus& status = fixture.status;
    status.setSimulationTime(42.25);
    GroundHitListener listener;

    EXPECT_TRUE(
        listener.handleFlightEvent(status, FlightEvent(FlightEvent::Type::GROUND_HIT, 42.0)));

    ASSERT_EQ(status.getEventQueue().size(), 1U);
    EXPECT_EQ(status.getEventQueue().peek()->getType(), FlightEvent::Type::SIMULATION_END);
    EXPECT_EQ(status.getEventQueue().peek()->getTime(), 42.25);
}

TEST(GroundHitListener, LetsEveryOtherEventPass)
{
    GroundHitListener listener;
    expectOtherEventsIgnored(listener, FlightEvent::Type::GROUND_HIT);
}

// ========================================================================= OptimumCoastListener

TEST(OptimumCoastListener, IsASharedSystemListener)
{
    EXPECT_EQ(sharedSystemListenerProblems<OptimumCoastListener>(), "");
}

TEST(OptimumCoastListener, QueuesTheEndOfTheSimulationAtApogee)
{
    TestStatus        fixture;
    SimulationStatus& status = fixture.status;
    status.setSimulationTime(5.0);
    OptimumCoastListener listener;

    EXPECT_TRUE(listener.handleFlightEvent(status, FlightEvent(FlightEvent::Type::APOGEE, 5.0)));

    EXPECT_EQ(queued(status), (std::vector<FlightEvent::Type>{FlightEvent::Type::SIMULATION_END}));
    EXPECT_EQ(status.getEventQueue().peek()->getTime(), 5.0);
}

TEST(OptimumCoastListener, LetsEveryOtherEventPass)
{
    OptimumCoastListener listener;
    expectOtherEventsIgnored(listener, FlightEvent::Type::APOGEE);
}

TEST(OptimumCoastListener, RefusesEveryRecoveryDeviceDeployment)
{
    TestStatus           fixture;
    OptimumCoastListener listener;
    EXPECT_FALSE(listener.recoveryDeviceDeployment(fixture.status, *fixture.alpha.chute));
    EXPECT_TRUE(fixture.status.getEventQueue().empty());

    // Through the helper: the deployment is refused, and a system listener leaves no warning.
    fixture.conditions->getSimulationListenerList().push_back(OptimumCoastListener::instance());
    EXPECT_FALSE(SimulationListenerHelper::fireRecoveryDeviceDeployment(fixture.status,
                                                                        *fixture.alpha.chute));
    EXPECT_TRUE(warningsOf(fixture.status).empty());
}

// ========================================================= RecoveryDeviceDeploymentEndListener

TEST(RecoveryDeviceDeploymentEndListener, IsASharedSystemListener)
{
    EXPECT_EQ(sharedSystemListenerProblems<RecoveryDeviceDeploymentEndListener>(), "");
}

TEST(RecoveryDeviceDeploymentEndListener, QueuesTheEndOfTheSimulationAndLetsTheDeviceDeploy)
{
    TestStatus        fixture;
    SimulationStatus& status = fixture.status;
    status.setSimulationTime(9.0);
    RecoveryDeviceDeploymentEndListener listener;

    EXPECT_TRUE(listener.recoveryDeviceDeployment(status, *fixture.alpha.chute));

    EXPECT_EQ(queued(status), (std::vector<FlightEvent::Type>{FlightEvent::Type::SIMULATION_END}));
    EXPECT_EQ(status.getEventQueue().peek()->getTime(), 9.0);

    // No flight event makes it act.
    EXPECT_TRUE(listener.handleFlightEvent(
        status, FlightEvent(FlightEvent::Type::RECOVERY_DEVICE_DEPLOYMENT, 9.0)));
    EXPECT_TRUE(listener.handleFlightEvent(status, FlightEvent(FlightEvent::Type::APOGEE, 9.0)));
    EXPECT_EQ(status.getEventQueue().size(), 1U);
}

// ============================================================================ InterruptListener

/// A stop source for a test. It is held through a pointer because it must not be const:
/// request_stop() is a const member in libstdc++ only, so a const stop source, which
/// misc-const-correctness asks for where it sees libstdc++, does not compile elsewhere.
using StopSource = std::shared_ptr<std::stop_source>;

[[nodiscard]] StopSource newStopSource()
{
    return std::make_shared<std::stop_source>();
}

TEST(InterruptListener, IsASystemListenerWithoutASharedInstance)
{
    const InterruptListener listener{std::stop_token{}};
    EXPECT_TRUE(listener.isSystemListener());
}

TEST(InterruptListener, DoesNothingUntilAStopIsRequested)
{
    TestStatus        fixture;
    const StopSource  source = newStopSource();
    InterruptListener listener(source->get_token());
    EXPECT_NO_THROW(listener.postStep(fixture.status));
    EXPECT_NO_THROW(listener.postStep(fixture.status));
    EXPECT_TRUE(listener.preStep(fixture.status));
}

/// The message of the SimulationCancelledException that postStep() of @p listener throws, or
/// "<none>".
[[nodiscard]] std::string cancellation(SimulationListener& listener, SimulationStatus& status)
{
    try
    {
        listener.postStep(status);
    }
    catch (const SimulationCancelledException& exception)
    {
        return exception.what();
    }
    return "<none>";
}

TEST(InterruptListener, CancelsTheSimulationOnceAStopIsRequested)
{
    TestStatus        fixture;
    const StopSource  source = newStopSource();
    InterruptListener listener(source->get_token());
    EXPECT_EQ(cancellation(listener, fixture.status), "<none>");
    source->request_stop();
    EXPECT_EQ(cancellation(listener, fixture.status), "The simulation was interrupted.");
    // The request stays (Java's Thread.interrupted() clears the flag).
    EXPECT_EQ(cancellation(listener, fixture.status), "The simulation was interrupted.");
    // The exception is of the family that crosses the simulation loop.
    EXPECT_THROW(listener.postStep(fixture.status), SimulationException);
}

TEST(InterruptListener, ItsClonesWatchTheSameStopSource)
{
    // The simulation runs on clones of its listeners; the one request reaches them all.
    TestStatus                                fixture;
    const StopSource                          source = newStopSource();
    const std::shared_ptr<SimulationListener> listener =
        std::make_shared<InterruptListener>(source->get_token());
    const std::shared_ptr<SimulationListener> clone        = listener->clone();
    const std::shared_ptr<SimulationListener> cloneOfClone = clone->clone();
    EXPECT_EQ(cancellation(*cloneOfClone, fixture.status), "<none>");
    source->request_stop();
    EXPECT_EQ(cancellation(*clone, fixture.status), "The simulation was interrupted.");
    EXPECT_EQ(cancellation(*cloneOfClone, fixture.status), "The simulation was interrupted.");
    EXPECT_TRUE(cloneOfClone->isSystemListener());
}

TEST(InterruptListener, ATokenThatCannotBeStoppedNeverCancels)
{
    TestStatus        fixture;
    InterruptListener listener{std::stop_token{}};
    EXPECT_EQ(cancellation(listener, fixture.status), "<none>");
}

TEST(InterruptListener, ThroughTheHelperTheExceptionLeavesTheStep)
{
    TestStatus       fixture;
    const StopSource source = newStopSource();
    fixture.conditions->getSimulationListenerList().push_back(
        std::make_shared<InterruptListener>(source->get_token()));
    EXPECT_NO_THROW(SimulationListenerHelper::firePostStep(fixture.status));
    source->request_stop();
    EXPECT_THROW(SimulationListenerHelper::firePostStep(fixture.status),
                 SimulationCancelledException);
    EXPECT_TRUE(SimulationListenerHelper::firePreStep(fixture.status)) << "only postStep() looks";
}

}  // namespace
