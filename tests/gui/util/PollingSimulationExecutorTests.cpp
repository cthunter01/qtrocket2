#include "QtRocket/gui/util/PollingSimulationExecutor.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <exception>
#include <memory>
#include <span>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <vector>

#include <QCoreApplication>
#include <QObject>
#include <QString>
#include <QTest>
#include <QThread>
#include <QtCore/qtestsupport_core.h>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/util/SimulationExecutor.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/simulation/RunSample.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationRun.h"
#include "QtRocket/simulation/SimulationStatus.h"
#include "QtRocket/simulation/exception/SimulationException.h"
#include "QtRocket/simulation/listeners/CloneableSimulationListener.h"
#include "QtRocket/simulation/listeners/RunSampleListener.h"
#include "QtRocket/util/BugError.h"
#include "RecordingGuardHook.h"
#include "SimulationRunTestSupport.h"
#include "simulation/SimulationRunnerSupport.h"

// The application's executor: the core's SimulationRunner on threads of its own, polled from the
// GUI thread (U-M4-2). Real threads: the waits have safety nets (a defect fails the test instead
// of hanging it), and the holds watch their job's stop, so a cancel and the executor's destructor
// always end them (SimulationRunnerSupport.h). The suite carries ctest's TIMEOUT
// (tests/gui/CMakeLists.txt) and runs under the tsan preset at a merge.

namespace QtRocket::Gui::Test
{

namespace
{

using PollingSimulationExecutorTest = GuiTest;
using JobId                         = SimulationExecutor::JobId;
using Outcome                       = SimulationRun::Outcome;
using QtRocket::Test::StepGate;
using QtRocket::Test::StepMeeting;

/// How long a test waits in the event loop for what the runner's threads do: a safety net that
/// only a defect reaches (QtRocket::Test::kRunnerSafetyNet).
constexpr int kSafetyNetMilliseconds =
    static_cast<int>(std::chrono::milliseconds(QtRocket::Test::kRunnerSafetyNet).count());

/// A simulation of a new design, without a motor, whose runs end at once (an aborted flight:
/// SUCCEEDED).
class MotorlessFlight
{
public:
    MotorlessFlight()
      : m_document(OpenRocketDocumentFactory::createNewRocket()),
        m_simulation(std::make_shared<Simulation>(m_document->getRocket(), m_preferences))
    {
    }

    /// A new run of the simulation.
    [[nodiscard]] std::shared_ptr<SimulationRun> newRun() const
    {
        return std::make_shared<SimulationRun>(*m_simulation);
    }

private:
    InMemoryPreferences                 m_preferences;
    std::unique_ptr<OpenRocketDocument> m_document;
    std::shared_ptr<Simulation>         m_simulation;
};

/// What the callbacks of a job saw.
struct Seen
{
    bool               ended           = false;
    bool               onTheGuiThread  = true;
    bool               polling         = true;
    int                progressCalls   = 0;
    std::size_t        samples         = 0;
    bool               samplesAfterEnd = false;
    std::exception_ptr exception;
};

/// Whether the caller is on the GUI thread.
[[nodiscard]] bool onTheGuiThread()
{
    return QThread::currentThread() == QCoreApplication::instance()->thread();
}

/// A job's progressed() that notes what it was given.
struct NoteProgress
{
    Seen* seen;

    void operator()(std::span<const RunSample> samples) const
    {
        ++seen->progressCalls;
        seen->samples += samples.size();
        seen->samplesAfterEnd = seen->samplesAfterEnd || seen->ended;
        seen->onTheGuiThread  = seen->onTheGuiThread && onTheGuiThread();
    }
};

/// A job's finished() that notes the end, and whether the executor's poll ran meanwhile.
struct NoteEnd
{
    Seen*                            seen;
    const PollingSimulationExecutor* executor;

    void operator()(const std::exception_ptr& exception) const
    {
        seen->ended          = true;
        seen->exception      = exception;
        seen->polling        = executor->isPolling();
        seen->onTheGuiThread = seen->onTheGuiThread && onTheGuiThread();
    }
};

/// The callbacks that note into @p seen.
[[nodiscard]] SimulationExecutor::Callbacks noting(Seen&                            seen,
                                                   const PollingSimulationExecutor& executor)
{
    return {.progressed = NoteProgress{.seen = &seen},
            .finished   = NoteEnd{.seen = &seen, .executor = &executor}};
}

/// A job's finished() that submits another job of @p flight, whose end it notes into @p next.
struct SubmitAnother
{
    PollingSimulationExecutor* executor;
    const MotorlessFlight*     flight;
    Seen*                      next;

    void operator()(const std::exception_ptr& /*exception*/) const
    {
        static_cast<void>(executor->submit(nullptr, flight->newRun(), nullptr, {},
                                           std::stop_source{}, noting(*next, *executor)));
    }
};

/// A job's finished() that throws.
struct Throwing
{
    void operator()(const std::exception_ptr& /*exception*/) const
    {
        throw std::runtime_error("a callback with a defect");
    }
};

/// Waits in the event loop until @p done holds, at most the safety net.
template <class Done>
[[nodiscard]] bool waitFor(Done done)
{
    return QTest::qWaitFor(done, kSafetyNetMilliseconds);
}

/// Waits until the jobs that note into @p seen have ended.
template <class... More>
[[nodiscard]] bool waitForTheEnds(const Seen& seen, const More&... more)
{
    return waitFor([&seen, &more...] { return seen.ended && (more.ended && ...); });
}

/// Waits until the run of @p gate has got to it.
[[nodiscard]] bool waitUntilReached(const StepGate& gate)
{
    return waitFor([&gate] { return gate.reached(); });
}

/// Waits until @p executor polls no more (no job is pending).
[[nodiscard]] bool waitUntilIdle(const PollingSimulationExecutor& executor)
{
    return waitFor([&executor] { return !executor.isPolling(); });
}

/// A listener that notes the end of its run (endSimulation(), on the run's thread); its clones
/// share the note.
class RunEndNote final : public CloneableSimulationListener<RunEndNote>
{
public:
    void endSimulation(SimulationStatus& /*status*/,
                       const SimulationException* /*exception*/) override
    {
        m_ended->store(true);
    }

    /// Whether the run has ended.
    [[nodiscard]] bool ended() const noexcept { return m_ended->load(); }

private:
    std::shared_ptr<std::atomic<bool>> m_ended = std::make_shared<std::atomic<bool>>(false);
};

/// How long LetTheOtherEndMeanwhile gives the poll: five of its intervals. Only a defect (a poll
/// that runs while a callback runs) hands the other job's end over in that time; a correct
/// executor never does, however long it is.
constexpr int kPollsMeanwhile =
    static_cast<int>((5 * PollingSimulationExecutor::kPollInterval).count());

/// What LetTheOtherEndMeanwhile saw inside the callback.
struct Meanwhile
{
    bool pollingAtTheStart     = true;
    bool otherRunEnded         = false;
    bool pollingAfterTheSubmit = true;
    bool otherEnded            = true;
};

/// The first job's finished() of NoOtherCallbackComesWhileOneRuns: it releases the gate that holds
/// the other job, waits in the event loop (as a modal message does) until the other job's run is
/// over on its thread, submits a third job of @p flight, then gives the poll five intervals to
/// hand the other job's end over (it must not, while this callback runs), and notes it all.
struct LetTheOtherEndMeanwhile
{
    PollingSimulationExecutor* executor;
    const StepGate*            gate;
    const RunEndNote*          otherRun;
    const MotorlessFlight*     flight;
    const Seen*                other;
    Seen*                      third;
    Meanwhile*                 meanwhile;

    void operator()(const std::exception_ptr& /*exception*/) const
    {
        meanwhile->pollingAtTheStart = executor->isPolling();
        gate->release();
        meanwhile->otherRunEnded = waitFor([this] { return otherRun->ended(); });
        static_cast<void>(executor->submit(nullptr, flight->newRun(), nullptr, {},
                                           std::stop_source{}, noting(*third, *executor)));
        meanwhile->pollingAfterTheSubmit = executor->isPolling();
        meanwhile->otherEnded = QTest::qWaitFor([this] { return other->ended; }, kPollsMeanwhile);
    }
};

TEST_F(PollingSimulationExecutorTest, RunsAJobOnAThreadOfItsRunnerAndReportsItsEndAtAPoll)
{
    const MotorlessFlight                flight;
    const std::shared_ptr<SimulationRun> run = flight.newRun();
    PollingSimulationExecutor            executor(2);
    EXPECT_EQ(executor.threadCount(), 2U);
    EXPECT_EQ(executor.pollInterval(), PollingSimulationExecutor::kPollInterval);
    EXPECT_FALSE(executor.isPolling());
    Seen          seen;
    const QObject window;
    const JobId   job =
        executor.submit(&window, run, nullptr, {}, std::stop_source{}, noting(seen, executor));
    EXPECT_NE(job, SimulationExecutor::kNoJob);
    // Never inside submit(); the poll runs while the job is pending.
    EXPECT_FALSE(seen.ended);
    EXPECT_TRUE(executor.isPending(job));
    EXPECT_TRUE(executor.isPolling());
    EXPECT_TRUE(waitForTheEnds(seen));
    EXPECT_TRUE(seen.onTheGuiThread);
    // The poll is paused while the news is handed over.
    EXPECT_FALSE(seen.polling);
    EXPECT_EQ(seen.exception, nullptr);
    EXPECT_EQ(run->getOutcome(), Outcome::SUCCEEDED);
    EXPECT_FALSE(executor.isPending(job));
    // No job is pending: the poll has stopped.
    EXPECT_FALSE(executor.isPolling());
}

TEST_F(PollingSimulationExecutorTest, HandsOverTheSamplesOfAJobBeforeItsEnd)
{
    const ExampleDesign                  design(ExampleDesign::kSimpleModelRocket);
    const std::shared_ptr<SimulationRun> run =
        std::make_shared<SimulationRun>(*design.simulation(0));
    PollingSimulationExecutor executor(1);
    Seen                      seen;
    static_cast<void>(executor.submit(nullptr, run,
                                      std::make_shared<RunSampleListener>(frozenSampleClock()), {},
                                      std::stop_source{}, noting(seen, executor)));
    EXPECT_TRUE(waitForTheEnds(seen));
    EXPECT_EQ(run->getOutcome(), Outcome::SUCCEEDED);
    // The first step's sample and the events' (the clock never moves on), all before the end.
    EXPECT_GE(seen.progressCalls, 1);
    EXPECT_GE(seen.samples, 2U);
    EXPECT_FALSE(seen.samplesAfterEnd);
    EXPECT_TRUE(seen.onTheGuiThread);
}

TEST_F(PollingSimulationExecutorTest, ACancelledQueuedJobEndsCancelledWithoutRunning)
{
    // One thread, held by the first job: the second waits until it is cancelled.
    const ExampleDesign                  design(ExampleDesign::kSimpleModelRocket);
    const MotorlessFlight                flight;
    const StepGate                       gate(3);
    const std::stop_source               heldStop;
    const std::shared_ptr<SimulationRun> held =
        std::make_shared<SimulationRun>(*design.simulation(0));
    PollingSimulationExecutor executor(1);
    Seen                      heldSeen;
    static_cast<void>(executor.submit(nullptr, held, nullptr, {gate.listener(heldStop.get_token())},
                                      heldStop, noting(heldSeen, executor)));
    const std::shared_ptr<SimulationRun> queued = flight.newRun();
    Seen                                 queuedSeen;
    const JobId job = executor.submit(nullptr, queued, nullptr, {}, std::stop_source{},
                                      noting(queuedSeen, executor));
    EXPECT_TRUE(waitUntilReached(gate));
    executor.cancel(job);
    EXPECT_FALSE(queuedSeen.ended);
    EXPECT_TRUE(waitForTheEnds(queuedSeen));
    EXPECT_EQ(queued->getOutcome(), Outcome::CANCELLED);
    EXPECT_TRUE(queued->wasStopped());
    EXPECT_FALSE(heldSeen.ended);
    gate.release();
    EXPECT_TRUE(waitForTheEnds(heldSeen));
    EXPECT_EQ(held->getOutcome(), Outcome::SUCCEEDED);
}

TEST_F(PollingSimulationExecutorTest, ARunningJobThatIsCancelledEndsAfterItsStep)
{
    const ExampleDesign                  design(ExampleDesign::kSimpleModelRocket);
    const StepGate                       gate(3);
    const std::stop_source               stop;
    const std::shared_ptr<SimulationRun> run =
        std::make_shared<SimulationRun>(*design.simulation(0));
    PollingSimulationExecutor executor(1);
    Seen                      seen;
    const JobId job = executor.submit(nullptr, run, nullptr, {gate.listener(stop.get_token())},
                                      stop, noting(seen, executor));
    EXPECT_TRUE(waitUntilReached(gate));
    executor.cancel(job);
    // The hold sees the stop; the run ends after the step.
    EXPECT_TRUE(waitForTheEnds(seen));
    EXPECT_EQ(run->getOutcome(), Outcome::CANCELLED);
    EXPECT_TRUE(run->wasStopped());
    EXPECT_FALSE(run->isAdoptable());
}

TEST_F(PollingSimulationExecutorTest, TwoJobsRunAtTheSameTime)
{
    // Both runs meet at a step: neither goes on before both are there (a runner that ran them one
    // after the other would hold the first until the safety net).
    const ExampleDesign       design(ExampleDesign::kSimpleModelRocket);
    const StepMeeting         meeting(2, 5);
    const std::stop_source    firstStop;
    const std::stop_source    secondStop;
    PollingSimulationExecutor executor(2);
    Seen                      first;
    Seen                      second;
    static_cast<void>(executor.submit(
        nullptr, std::make_shared<SimulationRun>(*design.simulation(0)), nullptr,
        {meeting.listener(firstStop.get_token())}, firstStop, noting(first, executor)));
    static_cast<void>(executor.submit(
        nullptr, std::make_shared<SimulationRun>(*design.simulation(0)), nullptr,
        {meeting.listener(secondStop.get_token())}, secondStop, noting(second, executor)));
    EXPECT_TRUE(waitForTheEnds(first, second));
    EXPECT_TRUE(meeting.allMet());
}

TEST_F(PollingSimulationExecutorTest, AnAbandonedJobCallsNothing)
{
    const ExampleDesign                  design(ExampleDesign::kSimpleModelRocket);
    const StepGate                       gate(3);
    const std::stop_source               stop;
    const std::shared_ptr<SimulationRun> run =
        std::make_shared<SimulationRun>(*design.simulation(0));
    PollingSimulationExecutor executor(1);
    Seen                      seen;
    const QObject             window;
    const JobId               job =
        executor.submit(&window, run, std::make_shared<RunSampleListener>(),
                        {gate.listener(stop.get_token())}, stop, noting(seen, executor));
    EXPECT_TRUE(waitUntilReached(gate));
    // Samples may have come while the run went to its gate; none comes after the abandon.
    executor.abandonAll(&window);
    const int progressCalls = seen.progressCalls;
    EXPECT_FALSE(executor.isPending(job));
    // The run ends (the stop ends the hold), the poll drops its news and stops.
    EXPECT_TRUE(waitUntilIdle(executor));
    EXPECT_FALSE(seen.ended);
    EXPECT_EQ(seen.progressCalls, progressCalls);
    EXPECT_EQ(run->getOutcome(), Outcome::CANCELLED);
}

TEST_F(PollingSimulationExecutorTest, NoOtherCallbackComesWhileOneRuns)
{
    // The first job's end runs the event loop, as a modal message does: it lets the second job,
    // held at a gate until then, run to its end on its thread meanwhile, and submits a third job
    // (which must not start the poll again); the second job's end comes only after the first's
    // callback has returned (O-M4-15: the failures one at a time).
    const ExampleDesign       design(ExampleDesign::kSimpleModelRocket);
    const MotorlessFlight     flight;
    const StepGate            gate(3);
    const std::stop_source    otherStop;
    const auto                otherRun = std::make_shared<RunEndNote>();
    PollingSimulationExecutor executor(2);
    Seen                      other;
    Seen                      third;
    Meanwhile                 meanwhile;
    static_cast<void>(
        executor.submit(nullptr, flight.newRun(), nullptr, {}, std::stop_source{},
                        {.progressed = {},
                         .finished   = LetTheOtherEndMeanwhile{.executor  = &executor,
                                                               .gate      = &gate,
                                                               .otherRun  = otherRun.get(),
                                                               .flight    = &flight,
                                                               .other     = &other,
                                                               .third     = &third,
                                                               .meanwhile = &meanwhile}}));
    static_cast<void>(executor.submit(
        nullptr, std::make_shared<SimulationRun>(*design.simulation(0)), nullptr,
        {gate.listener(otherStop.get_token()), otherRun}, otherStop, noting(other, executor)));
    EXPECT_TRUE(waitForTheEnds(other, third));
    EXPECT_FALSE(meanwhile.pollingAtTheStart);
    EXPECT_TRUE(meanwhile.otherRunEnded);
    EXPECT_FALSE(meanwhile.pollingAfterTheSubmit);
    EXPECT_FALSE(meanwhile.otherEnded);
    EXPECT_TRUE(waitUntilIdle(executor));
}

TEST_F(PollingSimulationExecutorTest, ACallbackMaySubmitAJob)
{
    const MotorlessFlight     flight;
    PollingSimulationExecutor executor(1);
    Seen                      next;
    static_cast<void>(executor.submit(
        nullptr, flight.newRun(), nullptr, {}, std::stop_source{},
        {.progressed = {},
         .finished   = SubmitAnother{.executor = &executor, .flight = &flight, .next = &next}}));
    EXPECT_TRUE(waitForTheEnds(next));
    EXPECT_TRUE(waitUntilIdle(executor));
}

TEST_F(PollingSimulationExecutorTest, ACallbackThatThrowsIsReportedAndThePollGoesOn)
{
    const RecordingGuardHook  hook;
    const MotorlessFlight     flight;
    PollingSimulationExecutor executor(1);
    Seen                      after;
    static_cast<void>(executor.submit(nullptr, flight.newRun(), nullptr, {}, std::stop_source{},
                                      {.progressed = {}, .finished = Throwing{}}));
    static_cast<void>(executor.submit(nullptr, flight.newRun(), nullptr, {}, std::stop_source{},
                                      noting(after, executor)));
    EXPECT_TRUE(waitForTheEnds(after));
    ASSERT_EQ(hook.failures().size(), 1U);
    EXPECT_TRUE(
        hook.failures().front().message.contains(QStringLiteral("a callback with a defect")));
}

TEST_F(PollingSimulationExecutorTest, CancelAllCancelsTheJobsOfOneWindow)
{
    const ExampleDesign                  design(ExampleDesign::kSimpleModelRocket);
    const MotorlessFlight                flight;
    const StepGate                       gate(3);
    const std::stop_source               stop;
    PollingSimulationExecutor            executor(1);
    const QObject                        closing;
    const QObject                        other;
    Seen                                 held;
    Seen                                 queued;
    Seen                                 kept;
    const std::shared_ptr<SimulationRun> heldRun =
        std::make_shared<SimulationRun>(*design.simulation(0));
    static_cast<void>(executor.submit(&closing, heldRun, nullptr, {gate.listener(stop.get_token())},
                                      stop, noting(held, executor)));
    static_cast<void>(executor.submit(&closing, flight.newRun(), nullptr, {}, std::stop_source{},
                                      noting(queued, executor)));
    const std::shared_ptr<SimulationRun> keptRun = flight.newRun();
    static_cast<void>(
        executor.submit(&other, keptRun, nullptr, {}, std::stop_source{}, noting(kept, executor)));
    EXPECT_TRUE(waitUntilReached(gate));
    executor.cancelAll(&closing);
    EXPECT_TRUE(waitForTheEnds(held, queued, kept));
    EXPECT_EQ(heldRun->getOutcome(), Outcome::CANCELLED);
    EXPECT_EQ(keptRun->getOutcome(), Outcome::SUCCEEDED);
}

TEST_F(PollingSimulationExecutorTest, DestroyingItStopsTheRunsAndCallsNothing)
{
    const ExampleDesign                  design(ExampleDesign::kSimpleModelRocket);
    const StepGate                       gate(3);
    const std::stop_source               stop;
    const std::shared_ptr<SimulationRun> run =
        std::make_shared<SimulationRun>(*design.simulation(0));
    Seen seen;
    {
        auto executor = std::make_unique<PollingSimulationExecutor>(1);
        static_cast<void>(executor->submit(nullptr, run, nullptr, {gate.listener(stop.get_token())},
                                           stop, noting(seen, *executor)));
        EXPECT_TRUE(waitUntilReached(gate));
    }
    // The runner's destructor requested the stop and joined: the run has ended.
    EXPECT_EQ(run->getOutcome(), Outcome::CANCELLED);
    QCoreApplication::processEvents();
    EXPECT_FALSE(seen.ended);
}

TEST_F(PollingSimulationExecutorTest, ItNeedsAThreadAndAPollInterval)
{
    EXPECT_THROW(static_cast<void>(std::make_unique<PollingSimulationExecutor>(0)), BugError);
    EXPECT_THROW(static_cast<void>(
                     std::make_unique<PollingSimulationExecutor>(1, std::chrono::milliseconds(-1))),
                 BugError);
}

}  // namespace

}  // namespace QtRocket::Gui::Test
