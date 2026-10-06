// Tests for simulation branch listener functionality: OpenRocket's SimulationBranchListenerTest
// (core/src/test/java/info/openrocket/core/simulation/listeners/SimulationBranchListenerTest.java).
// Verifies that startSimulationBranch() and endSimulationBranch() are called correctly for
// single-stage and multi-stage rockets.
//
// The simulations run under the preferences of OpenRocket's test set-up (SimulationRunSupport.h),
// with the random seed of the options fixed (Java: whatever seed the new options drew).

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/exception/SimulationException.h"
#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"
#include "QtRocket/util/Error.h"
#include "rocket/TestRockets.h"
#include "simulation/SimulationRunSupport.h"

namespace
{

using QtRocket::CloneableSimulationListener;
using QtRocket::ErrorCode;
using QtRocket::FlightConfigurationId;
using QtRocket::FlightData;
using QtRocket::FlightDataBranch;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::Simulation;
using QtRocket::SimulationException;
using QtRocket::SimulationStatus;
using QtRocket::Test::JavaTestPreferences;
using QtRocket::Test::simulatedData;
using QtRocket::Test::TestBeta;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::testFcid;

/// Shared state object to track calls across listener clones (Java: SharedState).
struct SharedState
{
    int                      startSimulationCallCount{0};
    int                      endSimulationCallCount{0};
    int                      startBranchCallCount{0};
    int                      endBranchCallCount{0};
    std::vector<std::string> branchNames;
    std::vector<std::string> callOrder;
    /// Java: the exceptions themselves (null for none). An exception lives only as long as the
    /// call it is passed to, so its message is kept: "null" for none.
    std::vector<std::string> endBranchExceptions;
};

/// Test listener that tracks all branch listener calls. Uses a shared state object to track
/// calls across clones (Java: TestBranchListener).
class TestBranchListener final : public CloneableSimulationListener<TestBranchListener>
{
public:
    [[nodiscard]] SharedState& state() const { return *m_state; }

    void startSimulation(SimulationStatus& /*status*/) override
    {
        m_state->startSimulationCallCount++;
        m_state->callOrder.emplace_back("startSimulation");
    }

    void endSimulation(SimulationStatus& /*status*/,
                       const SimulationException* /*exception*/) override
    {
        m_state->endSimulationCallCount++;
        m_state->callOrder.emplace_back("endSimulation");
    }

    void startSimulationBranch(SimulationStatus& status) override
    {
        m_state->startBranchCallCount++;
        const std::shared_ptr<FlightDataBranch>& branch = status.getFlightDataBranch();
        if (branch != nullptr)
        {
            m_state->branchNames.push_back(branch->getName());
        }
        m_state->callOrder.emplace_back("startSimulationBranch");
    }

    void endSimulationBranch(SimulationStatus& /*status*/,
                             const SimulationException* exception) override
    {
        m_state->endBranchCallCount++;
        m_state->endBranchExceptions.emplace_back(exception == nullptr ? "null"
                                                                       : exception->what());
        m_state->callOrder.emplace_back("endSimulationBranch");
    }

private:
    // Use shared state so clones can update the same counters
    std::shared_ptr<SharedState> m_state = std::make_shared<SharedState>();
};

/// Test listener that throws an exception to test exception handling (Java:
/// ExceptionThrowingListener). The count is a value member, as Java's int field: each clone
/// goes on from the count it was cloned with.
class ExceptionThrowingListener final
  : public CloneableSimulationListener<ExceptionThrowingListener>
{
public:
    void postStep(SimulationStatus& /*status*/) override
    {
        m_stepCount++;
        // Throw an exception after a few steps to test exception handling during branch
        // execution
        if (m_stepCount >= 3)
        {
            throw SimulationException("Test exception during branch execution");
        }
    }

private:
    int m_stepCount{0};
};

/// The names of the branches of @p data, in order.
[[nodiscard]] std::vector<std::string> branchNames(const FlightData& data)
{
    std::vector<std::string> names;
    names.reserve(data.getBranchCount());
    for (std::size_t i = 0; i < data.getBranchCount(); i++)
    {
        names.push_back(data.getBranch(i).getName());
    }
    return names;
}

/// SimulationBranchListenerTest.setUpTest(): a new listener, and a simulation of the Estes
/// Alpha III.
class SimulationBranchListenerTest : public ::testing::Test
{
protected:
    SimulationBranchListenerTest() : m_simulation(*m_alpha.rocket, m_preferences.store)
    {
        setUp(m_simulation);
    }

    /// What the Java test sets on every simulation it makes.
    static void setUp(Simulation& simulation)
    {
        simulation.setFlightConfigurationId(testFcid(0));
        simulation.getOptions().setIsaAtmosphere(true);
        simulation.getOptions().setTimeStep(0.05);
        simulation.getOptions().setRandomSeed(0);
    }

    [[nodiscard]] const std::shared_ptr<TestBranchListener>& listener() const { return m_listener; }
    [[nodiscard]] SharedState&         state() const { return m_listener->state(); }
    [[nodiscard]] Simulation&          simulation() { return m_simulation; }
    [[nodiscard]] JavaTestPreferences& preferences() { return m_preferences; }

private:
    JavaTestPreferences                 m_preferences;
    std::shared_ptr<TestBranchListener> m_listener = std::make_shared<TestBranchListener>();
    TestEstesAlphaIII                   m_alpha;
    Simulation                          m_simulation;
};

// SimulationBranchListenerTest.testSingleStageRocket: Test that branch listeners are called for
// a single-stage rocket.
TEST_F(SimulationBranchListenerTest, SingleStageRocket)
{
    const Result<void> result = simulation().simulate({listener()});
    ASSERT_TRUE(result.has_value());

    // Should have one branch
    const FlightData& data = simulatedData(simulation());
    ASSERT_EQ(1U, data.getBranchCount()) << "Single-stage rocket should have one branch";

    // Verify listener was called correctly
    // Note: nested simulations (like computeCoastTime) no longer trigger user listeners,
    // so we should get exactly one call per event for a single-stage rocket
    EXPECT_EQ(1, state().startSimulationCallCount)
        << "startSimulation should be called exactly once";
    EXPECT_EQ(1, state().endSimulationCallCount) << "endSimulation should be called exactly once";
    EXPECT_EQ(1, state().startBranchCallCount)
        << "startSimulationBranch should be called exactly once for single stage";
    EXPECT_EQ(1, state().endBranchCallCount)
        << "endSimulationBranch should be called exactly once for single stage";
    // Verify they're called the same number of times
    EXPECT_EQ(state().startBranchCallCount, state().endBranchCallCount)
        << "startSimulationBranch and endSimulationBranch should be called the same number of "
           "times";

    // Verify branch names match
    ASSERT_EQ(1U, state().branchNames.size()) << "Should have exactly one branch name recorded";
    EXPECT_EQ(data.getBranch(0).getName(), state().branchNames[0]) << "Branch name should match";
}

// SimulationBranchListenerTest.testMultiStageRocket: Test that branch listeners are called for
// each branch in a multi-stage rocket.
TEST_F(SimulationBranchListenerTest, MultiStageRocket)
{
    // Use a two-stage rocket
    const TestBeta beta;
    Rocket&        rocket = *beta.rocket;
    Simulation     multiStage(rocket, preferences().store);
    setUp(multiStage);
    rocket.getSelectedConfiguration().setAllStages();
    const FlightConfigurationId fcid = rocket.getSelectedConfiguration().getFlightConfigurationId();
    multiStage.setFlightConfigurationId(fcid);

    const Result<void> result = multiStage.simulate({listener()});
    ASSERT_TRUE(result.has_value());

    // Should have multiple branches (sustainer + boosters)
    const FlightData& data        = simulatedData(multiStage);
    const int         branchCount = static_cast<int>(data.getBranchCount());
    EXPECT_TRUE(branchCount >= 2) << "Multi-stage rocket should have at least 2 branches";

    // Verify listener was called correctly
    // Note: nested simulations (like computeCoastTime) no longer trigger user listeners,
    // so we should get exactly one call per event
    EXPECT_EQ(1, state().startSimulationCallCount)
        << "startSimulation should be called exactly once";
    EXPECT_EQ(1, state().endSimulationCallCount) << "endSimulation should be called exactly once";
    // Branch listeners should be called exactly once per branch
    EXPECT_EQ(branchCount, state().startBranchCallCount)
        << "startSimulationBranch should be called exactly once per branch";
    EXPECT_EQ(branchCount, state().endBranchCallCount)
        << "endSimulationBranch should be called exactly once per branch";
    // Verify they're called the same number of times
    EXPECT_EQ(state().startBranchCallCount, state().endBranchCallCount)
        << "startSimulationBranch and endSimulationBranch should be called the same number of "
           "times";

    // Verify branch names match (the size and every name, branch by branch)
    EXPECT_EQ(branchNames(data), state().branchNames)
        << "Should have recorded branch name for each branch, and each should match";
}

// SimulationBranchListenerTest.testListenerCallOrder: Test that branch listeners are called in
// the correct order.
TEST_F(SimulationBranchListenerTest, ListenerCallOrder)
{
    const Result<void> result = simulation().simulate({listener()});
    ASSERT_TRUE(result.has_value());

    // Verify call order: startSimulation -> startBranch -> endBranch -> endSimulation
    const std::vector<std::string>& callOrder = state().callOrder;
    ASSERT_TRUE(callOrder.size() >= 4) << "Should have at least 4 listener calls";

    // First call should be startSimulation
    EXPECT_EQ("startSimulation", callOrder[0]) << "First call should be startSimulation";

    // Second call should be startSimulationBranch
    EXPECT_EQ("startSimulationBranch", callOrder[1])
        << "Second call should be startSimulationBranch";

    // Last call should be endSimulation
    EXPECT_EQ("endSimulation", callOrder[callOrder.size() - 1])
        << "Last call should be endSimulation";

    // Second-to-last call should be endSimulationBranch
    EXPECT_EQ("endSimulationBranch", callOrder[callOrder.size() - 2])
        << "Second-to-last call should be endSimulationBranch";
}

// SimulationBranchListenerTest.testBranchListenerCalledOnException: Test that
// endSimulationBranch is called even when an exception occurs.
TEST_F(SimulationBranchListenerTest, BranchListenerCalledOnException)
{
    // Create a listener that throws an exception during branch execution
    const std::shared_ptr<ExceptionThrowingListener> exceptionListener =
        std::make_shared<ExceptionThrowingListener>();

    // Expected - the exception listener throws an exception (Java: caught and ignored; here
    // simulate() returns it as its error)
    const Result<void> result = simulation().simulate({exceptionListener, listener()});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::SIMULATION_ABORTED);
    EXPECT_EQ(result.error().message, "Test exception during branch execution");

    // Verify that endSimulationBranch was still called
    EXPECT_TRUE(state().endBranchCallCount > 0)
        << "endSimulationBranch should be called even when exception occurs";
    EXPECT_TRUE(!state().endBranchExceptions.empty())
        << "endSimulationBranch should receive exception parameter";

    // Beyond the Java test: the branch's end and the simulation's end both got the exception,
    // the simulation's first (BasicEventSimulationEngine.simulateLoop() calls endSimulation()
    // before the exception reaches the finally block of simulate()).
    EXPECT_EQ(state().endBranchExceptions,
              std::vector<std::string>{"Test exception during branch execution"});
    EXPECT_EQ(state().callOrder,
              (std::vector<std::string>{"startSimulation", "startSimulationBranch", "endSimulation",
                                        "endSimulationBranch"}));
}

// SimulationBranchListenerTest.testBranchListenerNormalCompletion: Test that
// endSimulationBranch receives null exception when branch completes normally.
TEST_F(SimulationBranchListenerTest, BranchListenerNormalCompletion)
{
    const Result<void> result = simulation().simulate({listener()});
    ASSERT_TRUE(result.has_value());

    // Verify that endSimulationBranch was called with null exception
    EXPECT_EQ(1, state().endBranchCallCount) << "endSimulationBranch should be called exactly once";
    ASSERT_EQ(1U, state().endBranchExceptions.size()) << "Should have exactly one exception record";
    EXPECT_EQ("null", state().endBranchExceptions[0])
        << "Exception should be null for normal completion";
}

}  // namespace
