#include "QtRocket/gui/util/SimulationBatch.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <vector>

#include <QCoreApplication>
#include <QObject>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "HookedUserPrompts.h"
#include "ManualSimulationExecutor.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/events/DocumentChangeEvent.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/SimulationExecutor.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationProgressEstimator.h"
#include "QtRocket/simulation/SimulationRun.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/extension/impl/ScriptingExtension.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Signal.h"
#include "RecordingGuardHook.h"
#include "ScriptedUserPrompts.h"
#include "SimulationRunTestSupport.h"
#include "TestGuiServices.h"

// The run dialog's batch over the tests' executor (ManualSimulationExecutor): what OpenRocket's
// SimulationRunDialog does with its workers (the seeds, the runs, their progress, their ends, the
// failures, the batch's one SimulationChangeEvent), and bug 40.

namespace QtRocket::Gui::Test
{

namespace
{

using SimulationBatchTest = GuiTest;
using Mode                = ManualSimulationExecutor::Mode;
using Outcome             = SimulationRun::Outcome;
using Kind                = ScriptedUserPrompts::Kind;
using Answer              = UserPrompts::RunSimulationsAnswer;

/// The SIMULATION change events of a document (Java: SimulationChangeEvent), by their source.
class SimulationEvents
{
public:
    explicit SimulationEvents(OpenRocketDocument& document)
      : m_connection(document.documentChanged().connect([this](const DocumentChangeEvent& event) {
            if (event.isSimulationChange())
            {
                m_sources.push_back(event.getSimulation());
            }
        }))
    {
    }
    ~SimulationEvents()                                  = default;
    SimulationEvents(const SimulationEvents&)            = delete;
    SimulationEvents& operator=(const SimulationEvents&) = delete;
    SimulationEvents(SimulationEvents&&)                 = delete;
    SimulationEvents& operator=(SimulationEvents&&)      = delete;

    /// The source of each event, in order.
    [[nodiscard]] const std::vector<const Simulation*>& sources() const noexcept
    {
        return m_sources;
    }

private:
    std::vector<const Simulation*>                       m_sources;
    Signal<const DocumentChangeEvent&>::ScopedConnection m_connection;
};

/// Two runs of the short flight of "A simple model rocket" #1: the design's simulation and a
/// copy of it ("Second").
class TwoSimpleRuns
{
public:
    TwoSimpleRuns()
      : m_design(ExampleDesign::kSimpleModelRocket),
        m_first(m_design.simulation(0)),
        m_second(m_design.addCopyOf(0, "Second"))
    {
    }

    [[nodiscard]] OpenRocketDocument& document() const noexcept { return m_design.document(); }
    [[nodiscard]] const std::shared_ptr<Simulation>& first() const noexcept { return m_first; }
    [[nodiscard]] const std::shared_ptr<Simulation>& second() const noexcept { return m_second; }

    /// The request to run both through @p executor.
    [[nodiscard]] UserPrompts::RunSimulationsRequest request(SimulationExecutor& executor) const
    {
        return runRequestOf(document(), {m_first, m_second}, executor);
    }

private:
    ExampleDesign               m_design;
    std::shared_ptr<Simulation> m_first;
    std::shared_ptr<Simulation> m_second;
};

/// The batch's extra listeners: one made by @p make for the run of simulation @p index, none
/// for the others.
struct ListenerFor
{
    std::size_t                                          index;
    std::function<std::shared_ptr<SimulationListener>()> make;

    [[nodiscard]] std::vector<std::shared_ptr<SimulationListener>> operator()(
        std::size_t run, const std::stop_token& /*stop*/) const
    {
        if (run != index)
        {
            return {};
        }
        return {make()};
    }
};

/// The batch's extra listeners: a failing one for each run, as @p failures says.
struct FailingListeners
{
    std::vector<RunFailure> failures;

    [[nodiscard]] std::vector<std::shared_ptr<SimulationListener>> operator()(
        std::size_t run, const std::stop_token& /*stop*/) const
    {
        return {failingRunListener(failures.at(run))};
    }
};

/// What a listener of a run does as it runs: it cancels the batch (the dialog's Cancel while the
/// run goes on); the batch is known once it is made.
struct CancelTheBatch
{
    /// The batch, once it is made.
    struct Target
    {
        SimulationBatch* batch = nullptr;
    };

    const Target* target;

    void operator()() const { target->batch->cancel(); }
};

/// What a listener of a run does as it runs: it hands the samples so far over (as the
/// application's executor does at a poll), and notes whether the batch's estimator of the run
/// has taken one then.
struct HandOverAndLook
{
    ManualSimulationExecutor*     executor;
    const SimulationBatch* const* batch;
    bool*                         sampled;

    void operator()() const
    {
        static_cast<void>(executor->reportProgress(executor->runningJob()));
        const SimulationProgressEstimator* const estimator = (*batch)->estimator(0);
        *sampled = estimator != nullptr && estimator->getLastSample().has_value();
    }
};

/// A listener made by @p make: actionAtStep(@p step, @p action).
template <class Action>
struct AtStep
{
    int    step;
    Action action;

    [[nodiscard]] std::shared_ptr<SimulationListener> operator()() const
    {
        return actionAtStep(step, action);
    }
};

/// What the hook before the first failure's message notes: whether the other simulation's run
/// had ended, also after turns of the event loop (the poll is paused while a message is open).
/// It submits a run of @p unrelated first, so that a turn of the executor is pending while the
/// message is open (that turn must wait for the message too).
struct LookBeforeTheMessage
{
    const SimulationBatch*    batch;
    ManualSimulationExecutor* executor;
    const Simulation*         unrelated;
    bool*                     otherDone;

    void operator()() const
    {
        static_cast<void>(executor->submit(nullptr, std::make_shared<SimulationRun>(*unrelated),
                                           nullptr, {}, std::stop_source{},
                                           {.progressed = {}, .finished = {}}));
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        *otherDone = batch->isSimulationDone(1);
    }
};

/// What the batch's cancelBackgroundRuns hook notes: the simulations, and whether the runs were
/// prepared by then.
struct NoteTheBackgroundCancel
{
    const SimulationBatch* const*   batch;
    std::vector<const Simulation*>* simulations;
    bool*                           prepared;

    void operator()(std::span<const std::shared_ptr<Simulation>> cancelled) const
    {
        for (const std::shared_ptr<Simulation>& simulation : cancelled)
        {
            simulations->push_back(simulation.get());
        }
        *prepared = (*batch)->run(0) != nullptr;
    }
};

/// The texts the scripted double records for a run's failure: the severity, then the lines.
[[nodiscard]] QStringList failureTexts(const QString& first, const QString& message)
{
    return {QStringLiteral("ERROR"), first, message};
}

/// The scripting extension of @p simulation (its first extension), or null.
[[nodiscard]] std::shared_ptr<ScriptingExtension> scriptOf(const Simulation& simulation)
{
    if (simulation.getSimulationExtensions().empty())
    {
        return nullptr;
    }
    return std::dynamic_pointer_cast<ScriptingExtension>(
        simulation.getSimulationExtensions().front());
}

TEST_F(SimulationBatchTest, StartDrawsTheSeedsAndSubmitsARunOfEachSimulation)
{
    // Java's constructor: the seeds first, then a worker of each simulation, executed in order.
    TestGuiServices           services;
    ManualSimulationExecutor& executor = services.executor();
    executor.setMode(Mode::STEPPED);
    const TwoSimpleRuns                runs;
    const QObject                      window;
    UserPrompts::RunSimulationsRequest request = runs.request(executor);
    request.window                             = &window;
    const SimulationBatch*         made        = nullptr;
    std::vector<const Simulation*> cancelled;
    bool                           prepared = true;
    request.cancelBackgroundRuns =
        NoteTheBackgroundCancel{.batch = &made, .simulations = &cancelled, .prepared = &prepared};
    SimulationBatch batch(services.prompts(), request);
    made = &batch;
    EXPECT_FALSE(batch.isStarted());
    EXPECT_EQ(batch.size(), 2U);
    EXPECT_EQ(batch.simulationName(1), QStringLiteral("Second"));
    EXPECT_EQ(batch.estimator(0), nullptr);
    EXPECT_EQ(batch.run(0), nullptr);
    EXPECT_THROW(static_cast<void>(batch.simulationName(2)), BugError);

    batch.start();
    // The window's background estimate of them is cancelled before their runs are prepared.
    EXPECT_EQ(cancelled, (std::vector<const Simulation*>{runs.first().get(), runs.second().get()}));
    EXPECT_FALSE(prepared);
    EXPECT_TRUE(batch.isStarted());
    EXPECT_FALSE(batch.isFinished());
    ASSERT_EQ(executor.submittedJobs().size(), 2U);
    EXPECT_EQ(executor.submittedJobs().front().window, &window);
    EXPECT_EQ(executor.waitingJobs().size(), 2U);
    // The duplicates were made after the seeds were drawn: they have the simulations' seeds.
    ASSERT_NE(batch.run(1), nullptr);
    EXPECT_EQ(batch.run(1)->getDuplicate().getOptions().getRandomSeed(),
              runs.second()->getOptions().getRandomSeed());
    EXPECT_EQ(batch.run(1)->getOutcome(), Outcome::NOT_RUN);
    ASSERT_NE(batch.estimator(0), nullptr);
    EXPECT_EQ(batch.estimator(0)->getProgress(), 0);
    EXPECT_EQ(batch.shownSimulation(), std::optional<std::size_t>(0));
    EXPECT_EQ(batch.progress(), 0);
    EXPECT_THROW(batch.start(), BugError);
}

TEST_F(SimulationBatchTest, EachRunIsAdoptedAsItEndsAndTheDocumentIsToldOnce)
{
    TestGuiServices           services;
    ManualSimulationExecutor& executor = services.executor();
    executor.setMode(Mode::STEPPED);
    const TwoSimpleRuns               runs;
    const std::shared_ptr<FlightData> stored = runs.second()->getSimulatedData();
    const SimulationEvents            events(runs.document());
    SimulationBatch                   batch(services.prompts(), runs.request(executor));
    const QSignalSpy                  finished(&batch, &SimulationBatch::finished);
    const QSignalSpy                  progressed(&batch, &SimulationBatch::progressed);
    batch.start();

    EXPECT_TRUE(executor.runNext());
    // The first run is the first simulation's now; the second waits, with its old results.
    EXPECT_TRUE(batch.isSimulationDone(0));
    EXPECT_FALSE(batch.isSimulationDone(1));
    EXPECT_EQ(runs.first()->getStatus(), Simulation::Status::UPTODATE);
    EXPECT_EQ(runs.first()->getSimulatedData(), batch.run(0)->getDuplicate().getSimulatedData());
    EXPECT_EQ(runs.second()->getSimulatedData(), stored);
    EXPECT_EQ(batch.estimator(0)->getProgress(), 100);
    EXPECT_EQ(batch.progress(), 50);
    EXPECT_EQ(batch.shownSimulation(), std::optional<std::size_t>(1));
    EXPECT_TRUE(events.sources().empty());
    EXPECT_EQ(finished.count(), 0);
    EXPECT_GE(progressed.count(), 1);

    EXPECT_TRUE(executor.runNext());
    EXPECT_EQ(runs.second()->getStatus(), Simulation::Status::UPTODATE);
    EXPECT_NE(runs.second()->getSimulatedData(), stored);
    // One SimulationChangeEvent, of the simulation that completed the batch, then finished().
    EXPECT_EQ(events.sources(), std::vector<const Simulation*>{runs.second().get()});
    EXPECT_EQ(finished.count(), 1);
    EXPECT_TRUE(batch.isFinished());
    EXPECT_EQ(batch.shownSimulation(), std::nullopt);
    EXPECT_EQ(batch.progress(), 100);
    EXPECT_EQ(batch.answer(), (Answer{.cancelled = false, .allSuccessful = true}));
    // Nothing was shown.
    EXPECT_TRUE(services.prompts().prompts().empty());
}

TEST_F(SimulationBatchTest, TheEstimatorOfARunTakesItsSamplesAsTheyCome)
{
    // The samples a poll hands over reach the run's estimator at once (Java's process()).
    TestGuiServices           services;
    ManualSimulationExecutor& executor = services.executor();
    const ExampleDesign       design(ExampleDesign::kSimpleModelRocket);
    const SimulationBatch*    made    = nullptr;
    bool                      sampled = false;
    SimulationBatch           batch(
        services.prompts(), runRequestOf(design.document(), {design.simulation(0)}, executor),
        {.extraListeners =
             ListenerFor{.index = 0,
                         .make  = AtStep<HandOverAndLook>{.step   = 3,
                                                          .action = {.executor = &executor,
                                                                     .batch    = &made,
                                                                     .sampled  = &sampled}}},
         .sampleClock = frozenSampleClock()});
    made = &batch;
    batch.start();
    QCoreApplication::processEvents();
    EXPECT_TRUE(sampled);
    EXPECT_TRUE(batch.isFinished());
    ASSERT_NE(batch.estimator(0), nullptr);
    EXPECT_EQ(batch.estimator(0)->getProgress(), 100);
    EXPECT_GT(batch.estimator(0)->getMaxAltitude(), 0.0);
}

TEST_F(SimulationBatchTest, ACancelledBatchKeepsTheOldResults)
{
    // OpenRocket bug 40: the runs that had not ended keep their simulations' old results and
    // status; Java records the partial flight as up to date.
    TestGuiServices           services;
    ManualSimulationExecutor& executor = services.executor();
    executor.setMode(Mode::NEVER);
    const TwoSimpleRuns               runs;
    const std::shared_ptr<FlightData> firstStored  = runs.first()->getSimulatedData();
    const std::shared_ptr<FlightData> secondStored = runs.second()->getSimulatedData();
    const Simulation::Status          firstStatus  = runs.first()->getStatus();
    const SimulationEvents            events(runs.document());
    SimulationBatch                   batch(services.prompts(), runs.request(executor));
    const QSignalSpy                  finished(&batch, &SimulationBatch::finished);
    batch.start();
    batch.cancel();
    // The cancelled jobs end in a later turn, and the batch with them.
    EXPECT_TRUE(batch.isCancelled());
    EXPECT_FALSE(batch.isFinished());
    QCoreApplication::processEvents();
    EXPECT_TRUE(batch.isFinished());
    EXPECT_EQ(finished.count(), 1);
    EXPECT_EQ(batch.run(0)->getOutcome(), Outcome::CANCELLED);
    EXPECT_EQ(runs.first()->getSimulatedData(), firstStored);
    EXPECT_EQ(runs.first()->getStatus(), firstStatus);
    EXPECT_EQ(runs.second()->getSimulatedData(), secondStored);
    EXPECT_EQ(batch.answer(), (Answer{.cancelled = true, .allSuccessful = false}));
    // Java's dialog counts the cancelled workers as done: the document is told all the same.
    EXPECT_EQ(events.sources().size(), 1U);
    // Nothing is shown for a cancellation.
    EXPECT_TRUE(services.prompts().prompts().empty());
    // Nothing more once the batch is finished.
    batch.cancel();
    EXPECT_EQ(executor.cancelledJobs().size(), 2U);
}

TEST_F(SimulationBatchTest, ARunCancelledWhileItRunsKeepsTheOldResults)
{
    // Bug 40 again, for a run the cancel stops after its step.
    TestGuiServices                   services;
    ManualSimulationExecutor&         executor = services.executor();
    const ExampleDesign               design(ExampleDesign::kSimpleModelRocket);
    const std::shared_ptr<Simulation> simulation = design.simulation(0);
    ASSERT_NE(simulation, nullptr);
    const std::shared_ptr<FlightData> stored = simulation->getSimulatedData();
    CancelTheBatch::Target            made;
    SimulationBatch                   batch(
        services.prompts(), runRequestOf(design.document(), {simulation}, executor),
        {.extraListeners =
             ListenerFor{.index = 0,
                         .make  = AtStep<CancelTheBatch>{.step = 5, .action = {.target = &made}}},
         .sampleClock = {}});
    made.batch = &batch;
    batch.start();
    QCoreApplication::processEvents();
    EXPECT_TRUE(batch.isFinished());
    EXPECT_EQ(batch.run(0)->getOutcome(), Outcome::CANCELLED);
    EXPECT_TRUE(batch.run(0)->wasStopped());
    EXPECT_EQ(simulation->getSimulatedData(), stored);
    EXPECT_EQ(batch.answer(), (Answer{.cancelled = true, .allSuccessful = false}));
    EXPECT_TRUE(services.prompts().prompts().empty());
}

TEST_F(SimulationBatchTest, FailuresAreShownOneAtATime)
{
    // Java's DetailDialog: the simulation's name as the title, "An error occurred during the
    // simulation:" or, for a calculation of a branch, "An error occurred during simulation
    // branch" and the branch's quoted name, then the message. While one is open, no other run's
    // end comes.
    HookedUserPrompts        prompts;
    ManualSimulationExecutor executor;
    const TwoSimpleRuns      runs;
    SimulationBatch          batch(
        prompts, runs.request(executor),
        {.extraListeners = FailingListeners{.failures = {RunFailure::SIMULATION_EXCEPTION,
                                                         RunFailure::CALCULATION_WITH_BRANCH}},
         .sampleClock    = {}});
    bool otherDone = true;
    prompts.beforeNextMessage(LookBeforeTheMessage{.batch     = &batch,
                                                   .executor  = &executor,
                                                   .unrelated = runs.first().get(),
                                                   .otherDone = &otherDone});
    batch.start();
    QCoreApplication::processEvents();
    EXPECT_FALSE(otherDone);
    EXPECT_TRUE(batch.isFinished());
    EXPECT_EQ(batch.answer(), (Answer{.cancelled = false, .allSuccessful = false}));
    ASSERT_EQ(batch.run(1)->getOutcome(), Outcome::FAILED);
    const QString branch =
        QtStrings::fromUtf8(batch.run(1)->getCalculationBranchName().value_or(std::string()));
    EXPECT_FALSE(branch.isEmpty());
    const std::vector<ScriptedUserPrompts::Prompt>& shown = prompts.scripted().prompts();
    ASSERT_EQ(shown.size(), 2U);
    EXPECT_EQ(shown.at(0).kind, Kind::MESSAGE);
    EXPECT_EQ(shown.at(0).title, QtStrings::fromUtf8(runs.first()->getName()));
    EXPECT_EQ(shown.at(0).texts,
              failureTexts(QStringLiteral("An error occurred during the simulation:"),
                           QStringLiteral("the listener gave up")));
    EXPECT_EQ(shown.at(1).title, QStringLiteral("Second"));
    EXPECT_EQ(shown.at(1).texts,
              failureTexts(QStringLiteral("An error occurred during simulation branch \"") +
                               branch + QStringLiteral("\""),
                           QStringLiteral("the numbers went wrong")));
    // A failed run is the simulation's, as Java records it (with what the run got to).
    EXPECT_EQ(runs.second()->getStatus(), Simulation::Status::UPTODATE);
}

TEST_F(SimulationBatchTest, AnExceptionOfARunIsReportedThroughGuard)
{
    // Java: handleErrorCondition("An exception occurred during the simulation", t).
    const RecordingGuardHook hook;
    TestGuiServices          services;
    const ExampleDesign      design(ExampleDesign::kSimpleModelRocket);
    SimulationBatch          batch(
        services.prompts(),
        runRequestOf(design.document(), {design.simulation(0)}, services.executor()),
        {.extraListeners = FailingListeners{.failures = {RunFailure::OTHER_EXCEPTION}},
         .sampleClock    = {}});
    batch.start();
    QCoreApplication::processEvents();
    EXPECT_TRUE(batch.isFinished());
    EXPECT_EQ(batch.run(0)->getOutcome(), Outcome::EXCEPTION);
    ASSERT_EQ(hook.failures().size(), 1U);
    EXPECT_EQ(hook.failures().front().context,
              QStringLiteral("An exception occurred during the simulation"));
    EXPECT_TRUE(
        hook.failures().front().message.contains(QStringLiteral("not a simulation exception")));
    EXPECT_TRUE(services.prompts().prompts().empty());
    EXPECT_EQ(batch.answer(), (Answer{.cancelled = false, .allSuccessful = false}));
}

TEST_F(SimulationBatchTest, ASimulationRemovedDuringItsRunDoesNotGetIt)
{
    TestGuiServices           services;
    ManualSimulationExecutor& executor = services.executor();
    executor.setMode(Mode::STEPPED);
    const TwoSimpleRuns               runs;
    const std::shared_ptr<FlightData> stored = runs.second()->getSimulatedData();
    SimulationBatch                   batch(services.prompts(), runs.request(executor));
    batch.start();
    runs.document().removeSimulation(*runs.second());
    const SimulationEvents events(runs.document());
    EXPECT_EQ(executor.runAll(), 2U);
    EXPECT_TRUE(batch.isFinished());
    EXPECT_EQ(runs.second()->getSimulatedData(), stored);
    EXPECT_EQ(runs.first()->getStatus(), Simulation::Status::UPTODATE);
    // The document is told all the same (Java fires the event of the worker's simulation).
    EXPECT_EQ(events.sources(), std::vector<const Simulation*>{runs.second().get()});
}

TEST_F(SimulationBatchTest, AScriptQtRocketCannotRunIsAFailureOfTheRun)
{
    // The critic's G13. "Simulation scripting.ork" stores its scripts disabled, and a script
    // read from a file is disabled anyway (ScriptingExtension::documentLoaded()): as loaded, its
    // simulations run without them. A script that is enabled (Milestone 7's configuration
    // dialog) refuses the run with QtRocket's text, which the run dialog shows as Java's error
    // of a simulation.
    TestGuiServices                   services;
    const ExampleDesign               design(ExampleDesign::kSimulationScripting);
    const std::shared_ptr<Simulation> simulation = design.simulation(1);
    ASSERT_NE(simulation, nullptr);
    const std::shared_ptr<ScriptingExtension> script = scriptOf(*simulation);
    ASSERT_NE(script, nullptr);
    EXPECT_FALSE(script->isEnabled());
    script->setEnabled(true);
    SimulationBatch batch(services.prompts(),
                          runRequestOf(design.document(), {simulation}, services.executor()));
    batch.start();
    QCoreApplication::processEvents();
    EXPECT_TRUE(batch.isFinished());
    EXPECT_EQ(batch.run(0)->getOutcome(), Outcome::FAILED);
    ASSERT_EQ(services.prompts().prompts().size(), 1U);
    const ScriptedUserPrompts::Prompt& shown = services.prompts().prompts().front();
    EXPECT_EQ(shown.kind, Kind::MESSAGE);
    EXPECT_EQ(shown.title, QtStrings::fromUtf8(simulation->getName()));
    EXPECT_EQ(shown.texts,
              failureTexts(QStringLiteral("An error occurred during the simulation:"),
                           QStringLiteral("QtRocket does not support the scripting language "
                                          "'JavaScript'")));
}

TEST_F(SimulationBatchTest, ARequestThatCannotRunIsRefused)
{
    // Java: IllegalArgumentException("Called with no simulations to run").
    TestGuiServices     services;
    const ExampleDesign design(ExampleDesign::kSimpleModelRocket);
    SimulationBatch     none(services.prompts(),
                             runRequestOf(design.document(), {}, services.executor()));
    EXPECT_THROW(none.start(), BugError);
    SimulationBatch withANull(services.prompts(),
                              runRequestOf(design.document(), {nullptr}, services.executor()));
    EXPECT_EQ(withANull.simulationName(0), QString());
    EXPECT_THROW(withANull.start(), BugError);
    UserPrompts::RunSimulationsRequest request =
        runRequestOf(design.document(), {design.simulation(0)}, services.executor());
    request.executor = nullptr;
    SimulationBatch withoutAnExecutor(services.prompts(), request);
    EXPECT_THROW(withoutAnExecutor.start(), BugError);
    EXPECT_TRUE(services.executor().submittedJobs().empty());
}

TEST_F(SimulationBatchTest, ABatchCancelledBeforeItStartsIsFinished)
{
    TestGuiServices     services;
    const ExampleDesign design(ExampleDesign::kSimpleModelRocket);
    SimulationBatch     batch(
        services.prompts(),
        runRequestOf(design.document(), {design.simulation(0)}, services.executor()));
    const QSignalSpy finished(&batch, &SimulationBatch::finished);
    batch.cancel();
    EXPECT_TRUE(batch.isFinished());
    EXPECT_TRUE(batch.isCancelled());
    EXPECT_EQ(finished.count(), 0);
    EXPECT_EQ(batch.answer(), (Answer{.cancelled = true, .allSuccessful = false}));
}

TEST_F(SimulationBatchTest, ABatchThatGoesAbandonsItsJobs)
{
    // Its callbacks capture it: none comes after it.
    TestGuiServices           services;
    ManualSimulationExecutor& executor = services.executor();
    executor.setMode(Mode::STEPPED);
    const TwoSimpleRuns runs;
    {
        SimulationBatch batch(services.prompts(), runs.request(executor));
        batch.start();
        EXPECT_TRUE(executor.runNext());
    }
    EXPECT_EQ(executor.abandonedJobs().size(), 1U);
    EXPECT_EQ(executor.runAll(), 0U);
    QCoreApplication::processEvents();
}

}  // namespace

}  // namespace QtRocket::Gui::Test
