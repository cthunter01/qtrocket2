#include "QtRocket/simulation/SimulationRun.h"

#include <cstddef>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/events/DocumentChangeEvent.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/exception/SimulationCalculationException.h"
#include "QtRocket/simulation/exception/SimulationCancelledException.h"
#include "QtRocket/simulation/exception/SimulationException.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"
#include "QtRocket/simulation/listeners/ProgressListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Signal.h"
#include "document/DocumentTestSupport.h"
#include "rocket/TestRockets.h"
#include "simulation/FlightDifferences.h"
#include "simulation/SimulationOptionsSupport.h"
#include "simulation/SimulationRunSupport.h"
#include "simulation/SimulationStatusSupport.h"
#include "simulation/SimulationTestSupport.h"

// SimulationRun: a run of a simulation made on a duplicate of it (prepared on the thread that
// owns the simulation, run on any thread), and the hand-over of what came of it to the
// simulation (Simulation::adoptRun()). The runs here are made on the test's thread;
// simulation_threading_tests.cpp runs them on others. The flights are compared with flights of
// the same simulation, never with pinned values (a fresh flight is chaotic).

namespace
{

using QtRocket::CloneableSimulationListener;
using QtRocket::DocumentChangeEvent;
using QtRocket::ErrorCode;
using QtRocket::FlightData;
using QtRocket::FlightDataBranch;
using QtRocket::FlightEvent;
using QtRocket::ModId;
using QtRocket::OpenRocketDocument;
using QtRocket::ProgressListener;
using QtRocket::Rocket;
using QtRocket::Simulation;
using QtRocket::SimulationCalculationException;
using QtRocket::SimulationCancelledException;
using QtRocket::SimulationException;
using QtRocket::SimulationExtension;
using QtRocket::SimulationListener;
using QtRocket::SimulationOptions;
using QtRocket::SimulationRun;
using QtRocket::SimulationStatus;
using QtRocket::Test::AlphaDocument;
using QtRocket::Test::bugText;
using QtRocket::Test::ChangeCounter;
using QtRocket::Test::flightDifferences;
using QtRocket::Test::JavaTestPreferences;
using QtRocket::Test::newBranch;
using QtRocket::Test::TestEstesAlphaIII;
using QtRocket::Test::testFcid;

using Outcome = SimulationRun::Outcome;
using Status  = Simulation::Status;

static_assert(!std::is_copy_constructible_v<SimulationRun>);
static_assert(!std::is_move_constructible_v<SimulationRun>);

// ============================================================================ test support

/// The simulation of the tests: the Estes Alpha III [A8-0] in TestRockets' TEST_FCID_0, under the
/// preferences of OpenRocket's tests, with the ISA atmosphere, a time step of 0.05 s and a fixed
/// random seed. A short flight of one branch.
struct AlphaSimulation
{
    JavaTestPreferences preferences;
    TestEstesAlphaIII   alpha;
    Simulation          simulation;

    AlphaSimulation() : simulation(*alpha.rocket, preferences.store)
    {
        alpha.rocket->setSelectedConfiguration(testFcid(0));
        simulation.setFlightConfigurationId(testFcid(0));
        SimulationOptions& options = simulation.getOptions();
        options.setIsaAtmosphere(true);
        options.setTimeStep(0.05);
        options.setRandomSeed(1);
    }
};

/// Changes an option that the comparison with the simulated conditions sees: the launch rod
/// becomes a metre longer. (The random seed counts only when it is fixed, and the ISA flag not
/// at all.)
void lengthenTheRod(Simulation& simulation)
{
    SimulationOptions& options = simulation.getOptions();
    options.setLaunchRodLength(options.getLaunchRodLength() + 1.0);
}

/// The launch rod length of the simulated conditions of @p simulation; NaN, and a failure of the
/// test, when it has none. (A plain value: GCC's -Wnull-dereference does not see an ASSERT_NE()
/// of the pointer.)
[[nodiscard]] double simulatedRodLength(const Simulation& simulation)
{
    const SimulationOptions* conditions = simulation.getSimulatedConditions();
    if (conditions == nullptr)
    {
        ADD_FAILURE() << "the simulation has no simulated conditions";
        return std::numeric_limits<double>::quiet_NaN();
    }
    return conditions->getLaunchRodLength();
}

/// A stop token that nobody stops.
[[nodiscard]] std::stop_token freeToken()
{
    return {};
}

/// Runs @p run to its end, with a token nobody stops and no extra listener.
void runFreely(SimulationRun& run)
{
    run.run(freeToken());
}

/// Runs @p run with @p listener as its one extra listener and a token nobody stops.
void runWith(SimulationRun& run, const std::shared_ptr<SimulationListener>& listener)
{
    const std::vector<std::shared_ptr<SimulationListener>> listeners{listener};
    run.run(freeToken(), listeners);
}

/// A listener that requests a stop through its stop source after @p steps steps, on the thread
/// of the run: the InterruptListener after it sees the request in the same step.
[[nodiscard]] std::shared_ptr<ProgressListener> stopperAfter(
    int steps, const std::shared_ptr<std::stop_source>& source)
{
    const auto taken = std::make_shared<int>(0);
    return std::make_shared<ProgressListener>([steps, source, taken](const SimulationStatus&) {
        (*taken)++;
        if (*taken == steps)
        {
            source->request_stop();
        }
    });
}

/// A listener that counts the steps of the run in @p steps.
[[nodiscard]] std::shared_ptr<ProgressListener> stepCounter(const std::shared_ptr<int>& steps)
{
    return std::make_shared<ProgressListener>(
        [steps](const SimulationStatus& /*status*/) { (*steps)++; });
}

/// The Error of @p run; an empty one, and a failure of the test, when it has none.
[[nodiscard]] QtRocket::Error errorOfRun(const SimulationRun& run)
{
    if (!run.getError().has_value())
    {
        ADD_FAILURE() << "the run has no error";
    }
    return run.getError().value_or(QtRocket::Error{});
}

/// The what() of the std::runtime_error in @p exception; "" for another exception.
[[nodiscard]] std::string runtimeErrorText(const std::exception_ptr& exception)
{
    try
    {
        std::rethrow_exception(exception);
    }
    catch (const std::runtime_error& e)
    {
        return e.what();
    }
    catch (...)
    {
        return "";
    }
}

/// How the listener of the failing runs fails, at its first step.
enum class Failure
{
    SIMULATION_EXCEPTION,     ///< SimulationException("the listener gave up")
    CALCULATION_WITH_BRANCH,  ///< SimulationCalculationException for the branch of the status
    CALCULATION_NO_BRANCH,    ///< SimulationCalculationException without a branch
    BUG,                      ///< BugError("a listener with a bug")
    OTHER_EXCEPTION,          ///< std::runtime_error("not a simulation exception")
    CANCELLATION,             ///< SimulationCancelledException("the listener cancelled the run")
};

/// A listener that fails the run at its first step.
class FailingListener final : public CloneableSimulationListener<FailingListener>
{
public:
    explicit FailingListener(Failure failure) : m_failure(failure) { }

    void postStep(SimulationStatus& status) override
    {
        switch (m_failure)
        {
            case Failure::SIMULATION_EXCEPTION:
                throw SimulationException("the listener gave up");
            case Failure::CALCULATION_WITH_BRANCH:
                throw SimulationCalculationException("the numbers went wrong",
                                                     status.getFlightDataBranch());
            case Failure::CALCULATION_NO_BRANCH:
                throw SimulationCalculationException("the numbers went wrong", nullptr);
            case Failure::BUG:
                QtRocket::bug("a listener with a bug");
            case Failure::OTHER_EXCEPTION:
                throw std::runtime_error("not a simulation exception");
            case Failure::CANCELLATION:
                throw SimulationCancelledException("the listener cancelled the run");
        }
    }

private:
    Failure m_failure;
};

/// Runs @p run with a FailingListener of @p failure.
void runFailing(SimulationRun& run, Failure failure)
{
    runWith(run, std::make_shared<FailingListener>(failure));
}

/// The flight of @p simulation as a duplicate of it gives it when it runs on its own.
[[nodiscard]] std::shared_ptr<FlightData> flightAlone(const Simulation& simulation)
{
    const std::unique_ptr<Simulation> alone  = simulation.duplicateForIndependentSimulation();
    const QtRocket::Result<void>      result = alone->simulate();
    EXPECT_TRUE(result.has_value()) << (result.has_value() ? "" : result.error().message);
    return alone->getSimulatedData();
}

/// The differences between the flights @p expected and @p actual; a text when either is missing.
[[nodiscard]] std::string runFlightDifferences(const std::shared_ptr<FlightData>& expected,
                                               const std::shared_ptr<FlightData>& actual)
{
    if (expected == nullptr || actual == nullptr)
    {
        return "a flight is missing\n";
    }
    return flightDifferences(*expected, *actual);
}

/// A simulation of @p rocket as an import makes one: the status EXTERNAL, with data (one empty
/// branch, made up), new options and no extension.
[[nodiscard]] std::unique_ptr<Simulation> importedSimulation(Rocket& rocket)
{
    const std::vector<std::shared_ptr<FlightDataBranch>> branches{newBranch("imported")};
    return std::make_unique<Simulation>(
        nullptr, rocket, Status::EXTERNAL, "Imported", SimulationOptions(),
        std::vector<std::shared_ptr<SimulationExtension>>{},
        std::make_shared<FlightData>(std::span<const std::shared_ptr<FlightDataBranch>>(branches)));
}

/// What a test checks of a simulation that a run must not have touched.
struct Untouched
{
    Status                      status;
    std::shared_ptr<FlightData> data;
    const SimulationOptions*    conditions;
    ModId                       modId;
};

/// What @p simulation has now, for untouched().
[[nodiscard]] Untouched untouchedState(const Simulation& simulation)
{
    return {.status     = simulation.getStoredStatus(),
            .data       = simulation.getSimulatedData(),
            .conditions = simulation.getSimulatedConditions(),
            .modId      = simulation.getSimulatedConfigurationModId()};
}

/// Whether @p simulation still has what @p before noted.
[[nodiscard]] ::testing::AssertionResult untouched(const Simulation& simulation,
                                                   const Untouched&  before)
{
    if (simulation.getStoredStatus() != before.status)
    {
        return ::testing::AssertionFailure()
               << "the status is " << name(simulation.getStoredStatus());
    }
    if (simulation.getSimulatedData() != before.data)
    {
        return ::testing::AssertionFailure() << "the simulated data changed";
    }
    if (simulation.getSimulatedConditions() != before.conditions)
    {
        return ::testing::AssertionFailure() << "the simulated conditions changed";
    }
    if (simulation.getSimulatedConfigurationModId() != before.modId)
    {
        return ::testing::AssertionFailure() << "the simulated modification id changed";
    }
    return ::testing::AssertionSuccess();
}

// ============================================================================== preparation

TEST(SimulationRun, PreparingARunDuplicatesTheSimulationAndChangesNothing)
{
    AlphaSimulation     a;
    const ChangeCounter events(a.simulation.changed());
    const Untouched     before = untouchedState(a.simulation);

    const SimulationRun run(a.simulation);

    EXPECT_EQ(run.getOutcome(), Outcome::NOT_RUN);
    EXPECT_EQ(run.getError(), std::nullopt);
    EXPECT_EQ(run.getCalculationBranchName(), std::nullopt);
    EXPECT_EQ(run.getException(), nullptr);
    EXPECT_FALSE(run.isRefused());
    EXPECT_FALSE(run.isAdoptable());
    EXPECT_EQ(run.getModIdAtDuplication(), a.simulation.getActiveConfiguration().getModId());

    // A duplicate for a run of its own: another simulation, of a copy of the rocket.
    const Simulation& duplicate = run.getDuplicate();
    EXPECT_NE(&duplicate, &a.simulation);
    EXPECT_NE(&duplicate.getRocket(), a.alpha.rocket.get());
    EXPECT_EQ(duplicate.getDocument(), nullptr);
    EXPECT_EQ(duplicate.getStoredStatus(), Status::NOT_SIMULATED);
    EXPECT_EQ(duplicate.getSimulatedData(), nullptr);
    EXPECT_EQ(duplicate.getName(), a.simulation.getName());
    EXPECT_EQ(duplicate.getFlightConfigurationId(), testFcid(0));
    EXPECT_TRUE(duplicate.getOptions() == a.simulation.getOptions());
    EXPECT_EQ(duplicate.getOptions().getRandomSeed(), 1);

    // The simulation was only read.
    EXPECT_EQ(events.count(), 0);
    EXPECT_TRUE(untouched(a.simulation, before));
}

TEST(SimulationRun, ARunThatHasNotRunIsNotAdopted)
{
    AlphaSimulation     a;
    const SimulationRun run(a.simulation);
    const ChangeCounter events(a.simulation.changed());
    const Untouched     before = untouchedState(a.simulation);

    EXPECT_FALSE(run.adoptInto(a.simulation));

    EXPECT_EQ(events.count(), 0);
    EXPECT_TRUE(untouched(a.simulation, before));
}

TEST(SimulationRun, ARunIsRunOnce)
{
    const AlphaSimulation a;
    SimulationRun         run(a.simulation);
    runFreely(run);
    ASSERT_EQ(run.getOutcome(), Outcome::SUCCEEDED);
    const std::shared_ptr<FlightData> data = run.getDuplicate().getSimulatedData();

    EXPECT_EQ(bugText([&run] { runFreely(run); }), "A simulation run is run once");
    EXPECT_EQ(run.getOutcome(), Outcome::SUCCEEDED);
    EXPECT_EQ(run.getDuplicate().getSimulatedData(), data) << "nothing ran again";
}

// ====================================================================== a run and its adoption

TEST(SimulationRun, ARunIsAdoptedByTheSimulationItWasMadeFrom)
{
    AlphaSimulation                   a;
    const std::shared_ptr<FlightData> alone = flightAlone(a.simulation);
    SimulationRun                     run(a.simulation);
    const ChangeCounter               events(a.simulation.changed());

    runFreely(run);

    EXPECT_EQ(run.getOutcome(), Outcome::SUCCEEDED);
    EXPECT_EQ(run.getError(), std::nullopt);
    EXPECT_EQ(run.getCalculationBranchName(), std::nullopt);
    EXPECT_EQ(run.getException(), nullptr);
    EXPECT_TRUE(run.isAdoptable());
    // The run recorded itself in the duplicate, not in the simulation.
    EXPECT_EQ(run.getDuplicate().getStoredStatus(), Status::UPTODATE);
    EXPECT_EQ(a.simulation.getSimulatedData(), nullptr);
    EXPECT_EQ(a.simulation.getStoredStatus(), Status::NOT_SIMULATED);
    EXPECT_EQ(events.count(), 0);

    EXPECT_TRUE(run.adoptInto(a.simulation));

    // What simulate() records, and changed() once.
    EXPECT_EQ(events.count(), 1);
    EXPECT_EQ(a.simulation.getStoredStatus(), Status::UPTODATE);
    EXPECT_EQ(a.simulation.getStatus(), Status::UPTODATE);
    EXPECT_EQ(a.simulation.getSimulatedData(), run.getDuplicate().getSimulatedData())
        << "the data are shared, not copied";
    ASSERT_NE(a.simulation.getSimulatedConditions(), nullptr);
    EXPECT_NE(a.simulation.getSimulatedConditions(), run.getDuplicate().getSimulatedConditions());
    EXPECT_TRUE(*a.simulation.getSimulatedConditions() == a.simulation.getOptions());
    EXPECT_EQ(a.simulation.getSimulatedConfigurationDescription(), "[A8-0]");
    EXPECT_EQ(a.simulation.getSimulatedConfigurationModId(), run.getModIdAtDuplication());
    EXPECT_EQ(a.simulation.getSimulatedWarnings(),
              &a.simulation.getSimulatedData()->getWarningSet());

    // The flight is the one the simulation gives alone.
    EXPECT_EQ(runFlightDifferences(alone, a.simulation.getSimulatedData()), "");
}

TEST(SimulationRun, ALoadedSimulationTakesTheNewRunAndIsUpToDate)
{
    AlphaSimulation                   a;
    const std::shared_ptr<FlightData> stored = flightAlone(a.simulation);
    // The simulation as a file has it: LOADED, with the data of the file.
    Simulation loaded(nullptr, *a.alpha.rocket, Status::LOADED, "Loaded", a.simulation.getOptions(),
                      {}, stored, {}, &a.preferences.store);
    ASSERT_EQ(loaded.getStatus(), Status::LOADED);

    SimulationRun run(loaded);
    runFreely(run);
    ASSERT_TRUE(run.adoptInto(loaded));

    EXPECT_EQ(loaded.getStatus(), Status::UPTODATE);
    EXPECT_NE(loaded.getSimulatedData(), stored);
    EXPECT_EQ(loaded.getSimulatedData(), run.getDuplicate().getSimulatedData());
    EXPECT_EQ(runFlightDifferences(stored, loaded.getSimulatedData()), "");
}

TEST(SimulationRun, AnAbortedFlightSucceedsAndIsAdopted)
{
    // A simulation that aborts is no failure (Java's dialog shows nothing): its status says it.
    AlphaSimulation a;
    a.alpha.rocket->getFlightConfiguration(testFcid(0)).clearAllMotors();
    SimulationRun run(a.simulation);

    runFreely(run);

    EXPECT_EQ(run.getOutcome(), Outcome::SUCCEEDED);
    ASSERT_TRUE(run.adoptInto(a.simulation));
    EXPECT_TRUE(a.simulation.hasErrors());
    EXPECT_EQ(a.simulation.getStatus(), Status::ABORTED);
}

// ================================================================ a change while the run runs

TEST(SimulationRun, AChangeOfTheOptionsSinceThePreparationLeavesTheSimulationOutdated)
{
    AlphaSimulation a;
    const double    rodLength = a.simulation.getOptions().getLaunchRodLength();
    SimulationRun   run(a.simulation);
    // The duplicate has the options as they were.
    a.simulation.getOptions().setLaunchRodLength(rodLength + 1.0);
    runFreely(run);

    ASSERT_TRUE(run.adoptInto(a.simulation));

    // The record is that of the run: the options it ran with.
    EXPECT_EQ(simulatedRodLength(a.simulation), rodLength);
    EXPECT_EQ(a.simulation.getOptions().getLaunchRodLength(), rodLength + 1.0);
    EXPECT_EQ(a.simulation.getStoredStatus(), Status::UPTODATE);
    EXPECT_EQ(a.simulation.getStatus(), Status::OUTDATED);
}

TEST(SimulationRun, AChangeOfTheDesignSinceThePreparationLeavesTheSimulationOutdated)
{
    AlphaSimulation                   a;
    const std::shared_ptr<FlightData> before = flightAlone(a.simulation);
    SimulationRun                     run(a.simulation);
    a.alpha.body->setLength(a.alpha.body->getLength() + 0.05);
    ASSERT_NE(a.simulation.getActiveConfiguration().getModId(), run.getModIdAtDuplication());
    runFreely(run);

    ASSERT_TRUE(run.adoptInto(a.simulation));

    // The id of the configuration as it was: what the flight is of.
    EXPECT_EQ(a.simulation.getSimulatedConfigurationModId(), run.getModIdAtDuplication());
    EXPECT_EQ(a.simulation.getStatus(), Status::OUTDATED);
    EXPECT_EQ(runFlightDifferences(before, a.simulation.getSimulatedData()), "");
}

TEST(SimulationRun, AnotherConfigurationSinceThePreparationLeavesTheSimulationOutdated)
{
    AlphaSimulation a;
    SimulationRun   run(a.simulation);
    a.simulation.setFlightConfigurationId(testFcid(1));
    runFreely(run);

    ASSERT_TRUE(run.adoptInto(a.simulation));

    // The flight is of TEST_FCID_0, the simulation's configuration is TEST_FCID_1: no id of the
    // record matches it.
    EXPECT_EQ(a.simulation.getSimulatedConfigurationModId(), ModId::invalid());
    EXPECT_EQ(a.simulation.getSimulatedConfigurationDescription(), "[A8-0]");
    EXPECT_EQ(a.simulation.getStatus(), Status::OUTDATED);
}

TEST(SimulationRun, TheResultsOfAnEarlierRunAreReplaced)
{
    AlphaSimulation a;
    SimulationRun   first(a.simulation);
    runFreely(first);
    ASSERT_TRUE(first.adoptInto(a.simulation));
    lengthenTheRod(a.simulation);
    ASSERT_EQ(a.simulation.getStatus(), Status::OUTDATED);

    SimulationRun second(a.simulation);
    runFreely(second);
    ASSERT_TRUE(second.adoptInto(a.simulation));

    EXPECT_EQ(a.simulation.getSimulatedData(), second.getDuplicate().getSimulatedData());
    EXPECT_NE(a.simulation.getSimulatedData(), first.getDuplicate().getSimulatedData());
    EXPECT_EQ(a.simulation.getStatus(), Status::UPTODATE);
}

// ================================================================== an imported simulation

TEST(SimulationRun, AnImportedSimulationIsRefusedAndKeepsItsData)
{
    TestEstesAlphaIII alpha;
    alpha.rocket->setSelectedConfiguration(testFcid(0));
    const std::unique_ptr<Simulation> imported = importedSimulation(*alpha.rocket);
    const ChangeCounter               events(imported->changed());
    const Untouched                   before = untouchedState(*imported);
    ASSERT_NE(before.data, nullptr);

    SimulationRun run(*imported);

    // Only the simulation itself can tell: a duplicate is not simulated.
    EXPECT_TRUE(run.isRefused());
    EXPECT_EQ(run.getDuplicate().getStoredStatus(), Status::NOT_SIMULATED);

    runFreely(run);

    EXPECT_EQ(run.getOutcome(), Outcome::FAILED);
    const QtRocket::Error error = errorOfRun(run);
    EXPECT_EQ(error.code, ErrorCode::SIMULATION_ABORTED);
    EXPECT_EQ(error.message, "Cannot simulate imported simulation.");
    EXPECT_EQ(run.getCalculationBranchName(), std::nullopt);
    EXPECT_EQ(run.getException(), nullptr);
    // Nothing ran.
    EXPECT_EQ(run.getDuplicate().getSimulatedConditions(), nullptr);
    EXPECT_EQ(run.getDuplicate().getStoredStatus(), Status::NOT_SIMULATED);

    // Java's finally block would drop the imported data; the simulation keeps them.
    EXPECT_FALSE(run.isAdoptable());
    EXPECT_FALSE(run.adoptInto(*imported));
    EXPECT_EQ(events.count(), 0);
    EXPECT_TRUE(untouched(*imported, before));
    EXPECT_EQ(imported->getStatus(), Status::EXTERNAL);
}

TEST(SimulationRun, TheRefusalGoesByTheStoredStatusWhenTheRunIsPrepared)
{
    // Java's simulate() checks the field, which getStatus() makes OUTDATED once the options of
    // the imported simulation changed: then it is simulated like any other.
    TestEstesAlphaIII alpha;
    alpha.rocket->setSelectedConfiguration(testFcid(0));
    const std::unique_ptr<Simulation> imported = importedSimulation(*alpha.rocket);
    imported->getOptions().setIsaAtmosphere(true);
    imported->getOptions().setTimeStep(0.05);
    imported->getOptions().setRandomSeed(1);
    lengthenTheRod(*imported);
    ASSERT_EQ(imported->getStatus(), Status::OUTDATED);

    SimulationRun run(*imported);
    EXPECT_FALSE(run.isRefused());
    runFreely(run);

    EXPECT_EQ(run.getOutcome(), Outcome::SUCCEEDED);
    ASSERT_TRUE(run.adoptInto(*imported));
    EXPECT_EQ(imported->getStatus(), Status::UPTODATE);
}

// ============================================================================== cancellation

// OpenRocket bug 40: Java's simulate() records the partial flight of a cancelled run as up to
// date, and the results the simulation had are lost. A cancelled run is not adopted: the
// simulation keeps its results and its status.
TEST(SimulationRun, ACancelledRunLeavesTheSimulationWithTheResultsItHad)
{
    constexpr int kSteps = 10;

    AlphaSimulation a;
    SimulationRun   first(a.simulation);
    runFreely(first);
    ASSERT_TRUE(first.adoptInto(a.simulation));
    lengthenTheRod(a.simulation);
    ASSERT_EQ(a.simulation.getStatus(), Status::OUTDATED);
    const ChangeCounter events(a.simulation.changed());
    const Untouched     before = untouchedState(a.simulation);

    SimulationRun run(a.simulation);
    const auto    source = std::make_shared<std::stop_source>();
    const std::vector<std::shared_ptr<SimulationListener>> listeners{stopperAfter(kSteps, source)};
    run.run(source->get_token(), listeners);

    EXPECT_EQ(run.getOutcome(), Outcome::CANCELLED);
    EXPECT_TRUE(run.wasStopped());
    const QtRocket::Error error = errorOfRun(run);
    EXPECT_EQ(error.code, ErrorCode::CANCELLED);
    EXPECT_EQ(error.message, "The simulation was interrupted.");
    EXPECT_EQ(run.getCalculationBranchName(), std::nullopt);
    EXPECT_EQ(run.getException(), nullptr);
    // The duplicate has what Java would have recorded: the flight of the steps taken. The stop
    // came after the extra listener's step: the interrupt listener follows the extra ones.
    const std::shared_ptr<FlightData>& partial = run.getDuplicate().getSimulatedData();
    ASSERT_NE(partial, nullptr);
    ASSERT_EQ(partial->getBranchCount(), 1U);
    EXPECT_EQ(partial->getBranch(0).getLength(), static_cast<std::size_t>(kSteps));
    EXPECT_NE(partial->getBranch(0).getLastEvent(FlightEvent::Type::EXCEPTION), nullptr);

    EXPECT_FALSE(run.isAdoptable());
    EXPECT_FALSE(run.adoptInto(a.simulation));
    EXPECT_EQ(events.count(), 0);
    EXPECT_TRUE(untouched(a.simulation, before));
    EXPECT_EQ(a.simulation.getSimulatedData(), first.getDuplicate().getSimulatedData());
    EXPECT_EQ(a.simulation.getStatus(), Status::OUTDATED);
}

TEST(SimulationRun, AStopRequestedBeforeTheRunRunsNothing)
{
    // Java: SimulationWorker.doInBackground() of a worker that was cancelled before it started.
    AlphaSimulation a;
    SimulationRun   run(a.simulation);
    const auto      steps  = std::make_shared<int>(0);
    const auto      source = std::make_shared<std::stop_source>();
    source->request_stop();
    const std::vector<std::shared_ptr<SimulationListener>> listeners{stepCounter(steps)};

    run.run(source->get_token(), listeners);

    EXPECT_EQ(run.getOutcome(), Outcome::CANCELLED);
    EXPECT_TRUE(run.wasStopped());
    const QtRocket::Error error = errorOfRun(run);
    EXPECT_EQ(error.code, ErrorCode::CANCELLED);
    EXPECT_EQ(error.message, "The simulation was interrupted.");
    EXPECT_EQ(*steps, 0);
    EXPECT_EQ(run.getDuplicate().getSimulatedConditions(), nullptr);
    EXPECT_EQ(run.getDuplicate().getStoredStatus(), Status::NOT_SIMULATED);
    EXPECT_FALSE(run.adoptInto(a.simulation));
    EXPECT_EQ(a.simulation.getStoredStatus(), Status::NOT_SIMULATED);
}

// Bug 40 is the user's cancel only: a run that a listener (or an extension) cancels with a
// SimulationCancelledException of its own, while no stop was requested through the token, is
// recorded as Java's finally block records it (SimulationRunDialog.java:447-452 shows nothing for
// it, Simulation.java:557-570 records the partial flight as up to date): it replaces the results
// the simulation had.
TEST(SimulationRun, ARunThatCancelsItselfIsAdoptedAsJavaRecordsIt)
{
    AlphaSimulation a;
    SimulationRun   first(a.simulation);
    runFreely(first);
    ASSERT_TRUE(first.adoptInto(a.simulation));
    lengthenTheRod(a.simulation);
    ASSERT_EQ(a.simulation.getStatus(), Status::OUTDATED);
    const ChangeCounter events(a.simulation.changed());

    SimulationRun run(a.simulation);
    runFailing(run, Failure::CANCELLATION);

    EXPECT_EQ(run.getOutcome(), Outcome::CANCELLED);
    EXPECT_FALSE(run.wasStopped());
    const QtRocket::Error error = errorOfRun(run);
    EXPECT_EQ(error.code, ErrorCode::CANCELLED);
    EXPECT_EQ(error.message, "the listener cancelled the run");
    EXPECT_EQ(run.getException(), nullptr);
    EXPECT_TRUE(run.isAdoptable());

    ASSERT_TRUE(run.adoptInto(a.simulation));

    EXPECT_EQ(events.count(), 1);
    EXPECT_EQ(a.simulation.getStatus(), Status::UPTODATE);
    EXPECT_EQ(a.simulation.getSimulatedData(), run.getDuplicate().getSimulatedData());
    EXPECT_NE(a.simulation.getSimulatedData(), first.getDuplicate().getSimulatedData());
    EXPECT_EQ(simulatedRodLength(a.simulation), simulatedRodLength(run.getDuplicate()));
}

// ================================================================================== failures

TEST(SimulationRun, AFailedRunIsAdoptedAsJavaRecordsIt)
{
    // Java's finally block: the data the run got to, up to date.
    AlphaSimulation     a;
    SimulationRun       run(a.simulation);
    const ChangeCounter events(a.simulation.changed());

    runFailing(run, Failure::SIMULATION_EXCEPTION);

    EXPECT_EQ(run.getOutcome(), Outcome::FAILED);
    const QtRocket::Error error = errorOfRun(run);
    EXPECT_EQ(error.code, ErrorCode::SIMULATION_ABORTED);
    EXPECT_EQ(error.message, "the listener gave up");
    EXPECT_EQ(run.getCalculationBranchName(), std::nullopt);
    EXPECT_EQ(run.getException(), nullptr);
    EXPECT_TRUE(run.isAdoptable());

    ASSERT_TRUE(run.adoptInto(a.simulation));

    EXPECT_EQ(events.count(), 1);
    EXPECT_EQ(a.simulation.getStatus(), Status::UPTODATE);
    const std::shared_ptr<FlightData>& data = a.simulation.getSimulatedData();
    ASSERT_NE(data, nullptr);
    ASSERT_EQ(data->getBranchCount(), 1U);
    const FlightEvent* exception = data->getBranch(0).getLastEvent(FlightEvent::Type::EXCEPTION);
    ASSERT_NE(exception, nullptr);
    ASSERT_NE(exception->getMessage(), nullptr);
    EXPECT_EQ(*exception->getMessage(), "the listener gave up");
}

TEST(SimulationRun, AFailedCalculationNamesItsBranch)
{
    // Java's dialog: "An error occurred during simulation branch \"<name>\"" (the sustainer's
    // branch is named after its stage).
    AlphaSimulation a;
    SimulationRun   run(a.simulation);

    runFailing(run, Failure::CALCULATION_WITH_BRANCH);

    EXPECT_EQ(run.getOutcome(), Outcome::FAILED);
    const QtRocket::Error error = errorOfRun(run);
    EXPECT_EQ(error.code, ErrorCode::SIMULATION_ABORTED);
    EXPECT_EQ(error.message, "the numbers went wrong");
    EXPECT_EQ(run.getCalculationBranchName(), "Stage");
    ASSERT_NE(run.getDuplicate().getSimulatedData(), nullptr);
    EXPECT_EQ(run.getDuplicate().getSimulatedData()->getBranch(0).getName(), "Stage");
    EXPECT_TRUE(run.adoptInto(a.simulation));
}

TEST(SimulationRun, ACalculationThatNamesNoBranchNamesNone)
{
    const AlphaSimulation a;
    SimulationRun         run(a.simulation);

    runFailing(run, Failure::CALCULATION_NO_BRANCH);

    EXPECT_EQ(run.getOutcome(), Outcome::FAILED);
    const QtRocket::Error error = errorOfRun(run);
    EXPECT_EQ(error.message, "the numbers went wrong");
    EXPECT_EQ(run.getCalculationBranchName(), std::nullopt);
}

TEST(SimulationRun, InputsThatCannotBeSimulatedFailTheRun)
{
    // QtRocket's own refusal (validateInputs()), shown as Java shows a failure: nothing ran, and
    // the bookkeeping is that of simulate() (up to date, without data).
    AlphaSimulation a;
    a.simulation.getOptions().setLaunchRodLength(std::numeric_limits<double>::quiet_NaN());
    SimulationRun run(a.simulation);

    runFreely(run);

    EXPECT_EQ(run.getOutcome(), Outcome::FAILED);
    const QtRocket::Error error = errorOfRun(run);
    EXPECT_EQ(error.code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(error.message, "Cannot simulate: the launch rod length is not finite (NaN).");
    ASSERT_TRUE(run.adoptInto(a.simulation));
    EXPECT_EQ(a.simulation.getSimulatedData(), nullptr);
    EXPECT_EQ(a.simulation.getStoredStatus(), Status::UPTODATE);
}

TEST(SimulationRun, AnExceptionIsCarriedToTheThreadThatAsks)
{
    AlphaSimulation a;
    SimulationRun   run(a.simulation);

    runFailing(run, Failure::BUG);

    EXPECT_EQ(run.getOutcome(), Outcome::EXCEPTION);
    EXPECT_EQ(run.getError(), std::nullopt);
    ASSERT_NE(run.getException(), nullptr);
    EXPECT_EQ(bugText([&run] { std::rethrow_exception(run.getException()); }),
              "a listener with a bug");

    // simulate() recorded the run on the way out, as Java's finally block does: it is adopted.
    EXPECT_TRUE(run.isAdoptable());
    ASSERT_TRUE(run.adoptInto(a.simulation));
    EXPECT_EQ(a.simulation.getStoredStatus(), Status::UPTODATE);
    ASSERT_NE(a.simulation.getSimulatedData(), nullptr);
    EXPECT_EQ(a.simulation.getSimulatedData()->getBranch(0).getLength(), 1U);
}

TEST(SimulationRun, AnyExceptionIsCarried)
{
    const AlphaSimulation a;
    SimulationRun         run(a.simulation);

    runFailing(run, Failure::OTHER_EXCEPTION);

    EXPECT_EQ(run.getOutcome(), Outcome::EXCEPTION);
    ASSERT_NE(run.getException(), nullptr);
    EXPECT_EQ(runtimeErrorText(run.getException()), "not a simulation exception");
}

// ======================================================================== in a document

/// Records the documentChanged() events of a document while it lives. The log is mutable: the
/// slot adds to it through the `this` the constructor captured, also when it is declared const.
class DocumentEventLog
{
public:
    explicit DocumentEventLog(OpenRocketDocument& document)
      : m_connection(document.documentChanged().connect(
            [this](const DocumentChangeEvent& event) { m_events.push_back(event); }))
    {
    }

    DocumentEventLog(const DocumentEventLog&)            = delete;
    DocumentEventLog& operator=(const DocumentEventLog&) = delete;
    DocumentEventLog(DocumentEventLog&&)                 = delete;
    DocumentEventLog& operator=(DocumentEventLog&&)      = delete;
    ~DocumentEventLog()                                  = default;

    [[nodiscard]] const std::vector<DocumentChangeEvent>& events() const noexcept
    {
        return m_events;
    }

private:
    /// Declared before the connection, which is so disconnected before the log goes.
    mutable std::vector<DocumentChangeEvent>                       m_events;
    QtRocket::Signal<const DocumentChangeEvent&>::ScopedConnection m_connection;
};

/// The simulation that is the source of @p event, or null.
[[nodiscard]] const Simulation* simulationSource(const DocumentChangeEvent& event)
{
    Simulation* const* source = std::get_if<Simulation*>(&event.source);
    return source != nullptr ? *source : nullptr;
}

// The document hears an adoption as any change of a simulation: it is no longer saved, and as
// the change leaves the simulations equal to the undo state's (equality ignores what was
// simulated), it makes no undo step; the state's copy takes the results, which an undo back to
// that state brings along.
TEST(SimulationRun, TheDocumentHearsTheAdoption)
{
    const AlphaDocument               alpha;
    OpenRocketDocument&               document   = alpha.document();
    const std::shared_ptr<Simulation> simulation = alpha.newSimulation("Run", testFcid(0));
    document.addSimulation(simulation);
    document.clearUndo();
    document.setSaved(true);
    const DocumentEventLog log(document);

    SimulationRun run(*simulation);
    runFreely(run);
    ASSERT_TRUE(run.adoptInto(*simulation));

    EXPECT_FALSE(document.isSaved());
    EXPECT_FALSE(document.isUndoAvailable());
    ASSERT_EQ(log.events().size(), 1U);
    EXPECT_EQ(log.events()[0].kind, DocumentChangeEvent::Kind::DOCUMENT);
    EXPECT_EQ(simulationSource(log.events()[0]), simulation.get());

    // An undo back to the state of the adoption brings the results along.
    document.addUndoPosition("Rename");
    simulation->setName("Renamed");
    ASSERT_TRUE(document.isUndoAvailable());
    document.undo();
    ASSERT_EQ(document.getSimulationCount(), 1U);
    const std::shared_ptr<Simulation> restored = document.getSimulation(0);
    EXPECT_EQ(restored->getName(), "Run");
    EXPECT_EQ(restored->getSimulatedData(), run.getDuplicate().getSimulatedData());
    EXPECT_EQ(restored->getStatus(), Status::UPTODATE);
}

// ====================================================================================== texts

TEST(SimulationRunOutcome, TheNamesAreTheConstants)
{
    EXPECT_EQ(name(Outcome::NOT_RUN), "NOT_RUN");
    EXPECT_EQ(name(Outcome::SUCCEEDED), "SUCCEEDED");
    EXPECT_EQ(name(Outcome::CANCELLED), "CANCELLED");
    EXPECT_EQ(name(Outcome::FAILED), "FAILED");
    EXPECT_EQ(name(Outcome::EXCEPTION), "EXCEPTION");
}

}  // namespace
