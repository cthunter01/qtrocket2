#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"

#include <memory>
#include <type_traits>
#include <typeinfo>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/listeners/AbstractSimulationListener.h"
#include "QtRocket/simulation/listeners/SimulationComputationListener.h"
#include "QtRocket/simulation/listeners/SimulationEventListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "simulation/SimulationStatusSupport.h"
#include "simulation/SimulationTestSupport.h"

namespace
{

using QtRocket::AbstractSimulationListener;
using QtRocket::CloneableSimulationListener;
using QtRocket::FlightEvent;
using QtRocket::SimulationComputationListener;
using QtRocket::SimulationEventListener;
using QtRocket::SimulationListener;
using QtRocket::SimulationStatus;
using QtRocket::Test::bugText;
using QtRocket::Test::TestStatus;

/// What a listener written for a test looks like: it counts the steps and remembers the events
/// it was asked about, in members of its own.
class CountingListener final : public CloneableSimulationListener<CountingListener>
{
public:
    void postStep(SimulationStatus& /*status*/) override { m_steps++; }

    [[nodiscard]] bool handleFlightEvent(SimulationStatus& /*status*/,
                                         const FlightEvent& event) override
    {
        m_handled.push_back(event.getType());
        return true;
    }

    [[nodiscard]] int                                   steps() const noexcept { return m_steps; }
    [[nodiscard]] const std::vector<FlightEvent::Type>& handled() const noexcept
    {
        return m_handled;
    }

private:
    int                            m_steps{0};
    std::vector<FlightEvent::Type> m_handled;
};

/// A system listener, as the application's own are.
class SystemListener final : public CloneableSimulationListener<SystemListener>
{
public:
    [[nodiscard]] bool isSystemListener() const override { return true; }
};

/// A listener that another one builds on.
class BaseListener : public CloneableSimulationListener<BaseListener>
{
public:
    [[nodiscard]] bool preStep(SimulationStatus& /*status*/) override { return false; }
    [[nodiscard]] int  value() const noexcept { return m_value; }
    void               setValue(int value) noexcept { m_value = value; }

private:
    int m_value{1};
};

/// Built on BaseListener with a clone() of its own.
class FurtherListener final : public CloneableSimulationListener<FurtherListener, BaseListener>
{
public:
    [[nodiscard]] bool isSystemListener() const override { return true; }
};

/// Built on BaseListener without one.
class SlicedListener final : public BaseListener
{ };

/// A listener as the caller of a simulation writes one: the simulation runs on clones of it, so
/// what the caller wants to read afterwards is in a State the clones share, while the plain
/// members belong to each clone.
class RecordingListener final : public CloneableSimulationListener<RecordingListener>
{
public:
    /// What every clone reports to.
    struct State
    {
        int                            steps{0};
        std::vector<FlightEvent::Type> handled;
    };

    void postStep(SimulationStatus& /*status*/) override
    {
        m_ownSteps++;
        m_ownLog.push_back(m_ownSteps);
        m_shared->steps++;
    }

    [[nodiscard]] bool handleFlightEvent(SimulationStatus& /*status*/,
                                         const FlightEvent& event) override
    {
        m_shared->handled.push_back(event.getType());
        return true;
    }

    [[nodiscard]] int                           ownSteps() const noexcept { return m_ownSteps; }
    [[nodiscard]] const std::vector<int>&       ownLog() const noexcept { return m_ownLog; }
    [[nodiscard]] const std::shared_ptr<State>& shared() const noexcept { return m_shared; }

private:
    int                    m_ownSteps{0};
    std::vector<int>       m_ownLog;
    std::shared_ptr<State> m_shared{std::make_shared<State>()};
};

// A cloneable listener is a listener of every kind; clone() uses its copy constructor, and no
// listener is assigned.
static_assert(std::is_base_of_v<AbstractSimulationListener, CountingListener>);
static_assert(std::is_base_of_v<SimulationListener, CountingListener>);
static_assert(std::is_base_of_v<SimulationEventListener, CountingListener>);
static_assert(std::is_base_of_v<SimulationComputationListener, CountingListener>);
static_assert(std::is_base_of_v<BaseListener, FurtherListener>);
static_assert(std::is_default_constructible_v<CountingListener>);
static_assert(std::is_copy_constructible_v<CountingListener>);
static_assert(!std::is_copy_assignable_v<CountingListener>);

/// Calls postStep() on @p listener @p count times.
void step(SimulationListener& listener, int count)
{
    TestStatus        fixture;
    SimulationStatus& status = fixture.status;
    for (int i = 0; i < count; i++)
    {
        listener.postStep(status);
    }
}

/// clone() of @p listener as its own class (null when the clone is of another class).
template <class Listener>
[[nodiscard]] std::shared_ptr<Listener> cloneOf(const SimulationListener& listener)
{
    return std::dynamic_pointer_cast<Listener>(listener.clone());
}

TEST(CloneableSimulationListener, AListenerIsClonedWithItsClassAndItsState)
{
    TestStatus             fixture;
    SimulationStatus&      status = fixture.status;
    const CountingListener empty;
    EXPECT_EQ(empty.steps(), 0);

    const std::shared_ptr<CountingListener> original = std::make_shared<CountingListener>();
    step(*original, 3);
    static_cast<void>(original->handleFlightEvent(status, {FlightEvent::Type::LAUNCH, 0.0}));

    const std::shared_ptr<SimulationListener> base  = original;
    const std::shared_ptr<SimulationListener> clone = base->clone();
    ASSERT_NE(clone, nullptr);
    EXPECT_NE(clone, base);
    const SimulationListener& cloned = *clone;
    EXPECT_TRUE(typeid(cloned) == typeid(CountingListener));
    const auto typed = std::dynamic_pointer_cast<CountingListener>(clone);
    ASSERT_NE(typed, nullptr) << "the clone has the listener's own class";
    EXPECT_EQ(typed->steps(), 3);
    EXPECT_EQ(typed->handled(), (std::vector<FlightEvent::Type>{FlightEvent::Type::LAUNCH}));

    // The clone is its own listener.
    typed->postStep(status);
    EXPECT_EQ(typed->steps(), 4);
    EXPECT_EQ(original->steps(), 3);

    // And can be cloned again.
    const auto again = cloneOf<CountingListener>(*clone);
    ASSERT_NE(again, nullptr);
    EXPECT_EQ(again->steps(), 4);
}

TEST(CloneableSimulationListener, AValueMemberBelongsToEachCloneAndASharedOneToAll)
{
    // What OpenRocket's simulation does with a listener (probes/events-data-verify/
    // ListenerCloneProbe.java: after Simulation.simulate(listener) the caller's object has seen
    // startSimulation() and the first startSimulationBranch() only; every step ran on a clone,
    // and only a counter held by reference saw the 401 steps): the steps are taken on a clone
    // and on a clone of that clone.
    TestStatus                               fixture;
    SimulationStatus&                        status = fixture.status;
    const std::shared_ptr<RecordingListener> mine   = std::make_shared<RecordingListener>();
    step(*mine, 1);

    const std::shared_ptr<RecordingListener> clone = cloneOf<RecordingListener>(*mine);
    ASSERT_NE(clone, nullptr);
    step(*clone, 5);
    static_cast<void>(clone->handleFlightEvent(status, {FlightEvent::Type::LAUNCH, 0.0}));

    const std::shared_ptr<RecordingListener> cloneOfClone = cloneOf<RecordingListener>(*clone);
    ASSERT_NE(cloneOfClone, nullptr);
    step(*cloneOfClone, 3);
    static_cast<void>(cloneOfClone->handleFlightEvent(status, {FlightEvent::Type::APOGEE, 4.0}));

    // The value members: copied when the clone is made, and then each clone's own (Java: a
    // primitive field).
    EXPECT_EQ(mine->ownSteps(), 1) << "what the clones counted never comes back";
    EXPECT_EQ(mine->ownLog(), (std::vector<int>{1}));
    EXPECT_EQ(clone->ownSteps(), 6);
    EXPECT_EQ(clone->ownLog(), (std::vector<int>{1, 2, 3, 4, 5, 6}));
    EXPECT_EQ(cloneOfClone->ownSteps(), 9);
    EXPECT_EQ(cloneOfClone->ownLog(), (std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8, 9}));

    // The shared state: one object for the listener and all its clones (Java: an object the
    // listener refers to), which has every call.
    EXPECT_EQ(clone->shared(), mine->shared());
    EXPECT_EQ(cloneOfClone->shared(), mine->shared());
    EXPECT_EQ(mine->shared().use_count(), 3);
    EXPECT_EQ(mine->shared()->steps, 9);
    EXPECT_EQ(mine->shared()->handled, (std::vector<FlightEvent::Type>{FlightEvent::Type::LAUNCH,
                                                                       FlightEvent::Type::APOGEE}));

    // A listener made afresh has a state of its own.
    const RecordingListener other;
    EXPECT_NE(other.shared(), mine->shared());
    EXPECT_EQ(other.shared()->steps, 0);
}

TEST(CloneableSimulationListener, TheCloneKeepsTheOverriddenAnswers)
{
    const std::shared_ptr<SimulationListener> system = std::make_shared<SystemListener>();
    EXPECT_TRUE(system->isSystemListener());
    EXPECT_TRUE(system->clone()->isSystemListener());
    EXPECT_FALSE(std::make_shared<CountingListener>()->clone()->isSystemListener());
}

TEST(CloneableSimulationListener, TheCloneImplementsTheSameInterfaces)
{
    // SimulationListenerHelper asks every listener, a clone too, which interfaces it implements.
    const std::shared_ptr<SimulationListener> clone = std::make_shared<CountingListener>()->clone();
    EXPECT_NE(dynamic_cast<SimulationEventListener*>(clone.get()), nullptr);
    EXPECT_NE(dynamic_cast<SimulationComputationListener*>(clone.get()), nullptr);
}

TEST(CloneableSimulationListener, AListenerBuiltOnAnotherClonesItsOwnClass)
{
    TestStatus                             fixture;
    SimulationStatus&                      status  = fixture.status;
    const std::shared_ptr<FurtherListener> further = std::make_shared<FurtherListener>();
    further->setValue(7);
    const std::shared_ptr<SimulationListener> clone = further->clone();
    const auto typed = std::dynamic_pointer_cast<FurtherListener>(clone);
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->value(), 7);
    EXPECT_TRUE(typed->isSystemListener());
    EXPECT_FALSE(typed->preStep(status)) << "BaseListener's hook";

    const BaseListener base;
    const auto         baseClone = cloneOf<BaseListener>(base);
    ASSERT_NE(baseClone, nullptr);
    const BaseListener& cloned = *baseClone;
    EXPECT_TRUE(typeid(cloned) == typeid(BaseListener));
}

TEST(CloneableSimulationListener, AClassDerivedFurtherWithoutACloneOfItsOwnIsNotSliced)
{
    // Java's Object.clone() would copy the SlicedListener; BaseListener's clone() would make a
    // BaseListener of it.
    const SlicedListener sliced;
    EXPECT_EQ(bugText([&sliced] { static_cast<void>(sliced.clone()); }),
              "clone() is not overridden by a class derived from a cloneable listener");
}

}  // namespace
