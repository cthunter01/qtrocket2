// Tests of figure/BackgroundSimulation.h, the design view's estimate of the flight (OpenRocket's
// RocketPanel.updateExtras() and its background worker): the one-job lane over the tests'
// executor (STEPPED: a run waits until the test runs it; no debounce), the adoption of a run into
// the document's simulation (which marks the design unsaved: U-M4-5), the overlay's flight block,
// OpenRocket bugs 54 and 55, the chain of the auto-run and its end at a failure, the pause and the
// batch's cancelFor(); and once on real threads (the application's executor:
// BackgroundSimulationThreadsTest, which carries ctest's TIMEOUT, tests/gui/CMakeLists.txt, and
// runs under the tsan preset at a merge). The estimate of every open design against OpenRocket's
// is rocket_info_flight_golden_tests.cpp. Fresh runs are of "A simple model rocket" #1 only, and
// no value of a fresh flight is pinned: the overlay is compared with the adopted data.

#include "QtRocket/gui/figure/BackgroundSimulation.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

#include <QCoreApplication>
#include <QEvent>
#include <QObject>
#include <QString>
#include <QTest>
#include <QtCore/qtestsupport_core.h>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "ManualSimulationExecutor.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/document/events/DocumentChangeEvent.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/figure/DesignPhysics.h"
#include "QtRocket/gui/figure/RocketFigure.h"
#include "QtRocket/gui/figure/RocketInfo.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/PollingSimulationExecutor.h"
#include "QtRocket/gui/util/SimulationBatch.h"
#include "QtRocket/gui/util/SimulationExecutor.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/simulation/listeners/system/GroundHitListener.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Signal.h"
#include "RecordingGuardHook.h"
#include "ScriptedUserPrompts.h"
#include "SimulationRunTestSupport.h"
#include "TestPaths.h"
#include "file/DesignFileEnvironment.h"
#include "simulation/SimulationRunnerSupport.h"

namespace QtRocket::Gui::Test
{

namespace
{

using BackgroundSimulationTest        = GuiTest;
using BackgroundSimulationThreadsTest = GuiTest;
using Mode                            = ManualSimulationExecutor::Mode;
using FlightSummary                   = RocketInfo::FlightSummary;

/// The safety net of the real threads' waits: it fails the test instead of hanging it.
constexpr int kThreadsSafetyNetMilliseconds = 120000;

/// "A simple model rocket.ork" of data/examples.
[[nodiscard]] std::filesystem::path simpleModelRocket()
{
    return QtRocket::Test::dataDir() / "examples" / ExampleDesign::kSimpleModelRocket;
}

/// OpenRocket's re-save of the saver-coverage document with a stored flight that aborted.
[[nodiscard]] std::filesystem::path storedFlight()
{
    return QtRocket::Test::testDataDir() / "goldens" / "coverage-stored-flight" / "resave" /
           "rocket.ork";
}

/// The document of @p file, loaded in @p environment; a new rocket's, with a test failure, when
/// it cannot be.
[[nodiscard]] std::unique_ptr<OpenRocketDocument> loadDesign(
    const QtRocket::Test::DesignFileEnvironment& environment, const std::filesystem::path& file)
{
    Result<LoadedDocument> loaded = GeneralRocketLoader(environment.context()).load(file);
    if (!loaded.has_value())
    {
        ADD_FAILURE() << "cannot load the design: " << loaded.error().message;
        return OpenRocketDocumentFactory::createNewRocket();
    }
    return std::move(loaded->document);
}

/// Whether @p summary holds the maxima of @p data.
[[nodiscard]] bool isSummaryOf(const std::optional<FlightSummary>& summary, const FlightData& data)
{
    const auto same = [](double a, double b) { return a == b || (std::isnan(a) && std::isnan(b)); };
    return summary.has_value() && same(summary->maxAltitude, data.getMaxAltitude()) &&
           same(summary->maxVelocity, data.getMaxVelocity()) &&
           same(summary->maxMachNumber, data.getMaxMachNumber()) &&
           same(summary->maxAcceleration, data.getMaxAcceleration());
}

/// Whether @p summary is "N/A" (every number NaN).
[[nodiscard]] bool isNotAvailable(const std::optional<FlightSummary>& summary)
{
    return summary.has_value() && std::isnan(summary->maxAltitude) &&
           std::isnan(summary->maxVelocity) && std::isnan(summary->maxMachNumber) &&
           std::isnan(summary->maxAcceleration);
}

/// A design shown in a RocketFigure with its DesignPhysics and the estimate of the flight over an
/// executor (the tests', STEPPED, unless one is given), its jobs those of a window object, with
/// the estimate on (the preferences' default) and no debounce.
class EstimateView
{
public:
    explicit EstimateView(const std::filesystem::path& file, SimulationExecutor* executor = nullptr)
      : m_document(loadDesign(m_environment, file)),
        m_figure(m_document->getRocket(), m_preferences, 96.0, nullptr),
        m_physics(m_figure, m_preferences),
        m_background(m_physics, *m_document, m_preferences)
    {
        m_background.setDebounceInterval(std::chrono::milliseconds(0));
        m_background.setExecutor(executor != nullptr ? executor : &m_executor, &m_window);
    }

    [[nodiscard]] OpenRocketDocument&       document() const noexcept { return *m_document; }
    [[nodiscard]] InMemoryPreferences&      preferences() noexcept { return m_preferences; }
    [[nodiscard]] ManualSimulationExecutor& executor() noexcept { return m_executor; }
    [[nodiscard]] const QObject&            window() const noexcept { return m_window; }
    [[nodiscard]] DesignPhysics&            physics() noexcept { return m_physics; }
    [[nodiscard]] const RocketInfo&         info() const noexcept { return m_physics.info(); }
    [[nodiscard]] BackgroundSimulation&     background() noexcept { return m_background; }

    /// The document's simulation @p index.
    [[nodiscard]] std::shared_ptr<Simulation> simulation(std::size_t index) const
    {
        return m_document->getSimulation(index);
    }

    /// The rocket's change events of this pass delivered: the figure's queued update, which the
    /// numbers and the estimate follow.
    void deliverRocketChanges() { QCoreApplication::sendPostedEvents(&m_figure, QEvent::MetaCall); }

    /// Simulation #1 made out of date (its launch rod 0.5 m longer) and its configuration
    /// selected: the estimate asks for its run. Returns it.
    std::shared_ptr<Simulation> selectOutdatedFirstSimulation()
    {
        std::shared_ptr<Simulation> first   = simulation(0);
        SimulationOptions&          options = first->getOptions();
        options.setLaunchRodLength(options.getLaunchRodLength() + 0.5);
        m_document->getRocket().setSelectedConfiguration(first->getFlightConfigurationId());
        deliverRocketChanges();
        return first;
    }

    /// Waits until no run goes or waits (the overlay no longer calculating), at most
    /// @p milliseconds; false when one still does.
    [[nodiscard]] bool waitUntilCalculated(int milliseconds)
    {
        return QTest::qWaitFor([this] { return !m_background.isCalculating(); }, milliseconds);
    }

    /// Waits until the run asked for has started (the debounce is over); false when none did.
    [[nodiscard]] bool waitForStart()
    {
        return QTest::qWaitFor([this] { return !m_background.isStartPending(); }) &&
               m_background.job() != SimulationExecutor::kNoJob;
    }

private:
    QtRocket::Test::DesignFileEnvironment m_environment;
    std::unique_ptr<OpenRocketDocument>   m_document;
    InMemoryPreferences                   m_preferences;
    QObject                               m_window;
    ManualSimulationExecutor              m_executor{Mode::STEPPED};
    RocketFigure                          m_figure;
    DesignPhysics                         m_physics;
    BackgroundSimulation                  m_background;
};

/// The document's change events from now on.
class DocumentEvents
{
public:
    explicit DocumentEvents(OpenRocketDocument& document)
      : m_connection(document.documentChanged().connect(
            [this](const DocumentChangeEvent& event) { m_events.push_back(event); }))
    {
    }

    [[nodiscard]] const std::vector<DocumentChangeEvent>& events() const noexcept
    {
        return m_events;
    }

    /// Whether a SIMULATION event came whose source is @p simulation.
    [[nodiscard]] bool hasSimulationChange(const Simulation& simulation) const
    {
        return std::ranges::any_of(m_events, [&simulation](const DocumentChangeEvent& event) {
            return event.isSimulationChange() && event.getSimulation() == &simulation;
        });
    }

private:
    std::vector<DocumentChangeEvent>                     m_events;
    Signal<const DocumentChangeEvent&>::ScopedConnection m_connection;
};

/// Whether the flight of @p data ends at its ground hit (the GroundHitListener of the estimate):
/// the first branch's SIMULATION_END at the time of its GROUND_HIT.
[[nodiscard]] bool endsAtTheGroundHit(const FlightData& data)
{
    if (data.getBranchCount() == 0)
    {
        return false;
    }
    const FlightDataBranch& branch    = data.getBranch(0);
    const FlightEvent*      groundHit = branch.getFirstEvent(FlightEvent::Type::GROUND_HIT);
    const FlightEvent*      end       = branch.getLastEvent(FlightEvent::Type::SIMULATION_END);
    return groundHit != nullptr && end != nullptr && groundHit->getTime() == end->getTime();
}

/// A configuration of @p rocket with motors other than @p excluded, or nullopt.
[[nodiscard]] std::optional<FlightConfigurationId> otherConfigurationWithMotors(
    const Rocket& rocket, const FlightConfigurationId& excluded)
{
    for (const FlightConfigurationId& id : rocket.getIds())
    {
        if (!(id == excluded) && rocket.getFlightConfiguration(id).hasMotors())
        {
            return id;
        }
    }
    return std::nullopt;
}

/// The extra listeners of a run that fails with a SimulationException at its first step.
[[nodiscard]] std::vector<std::shared_ptr<SimulationListener>> failingWithASimulationException(
    const std::stop_token& /*stop*/)
{
    return {failingRunListener(RunFailure::SIMULATION_EXCEPTION)};
}

/// The extra listeners of a run that throws another exception at its first step.
[[nodiscard]] std::vector<std::shared_ptr<SimulationListener>> failingWithAnotherException(
    const std::stop_token& /*stop*/)
{
    return {failingRunListener(RunFailure::OTHER_EXCEPTION)};
}

/// The extra listeners of every run: one and the same listener, which the executor refuses for a
/// second live job (SimulationExecutor::submit()).
struct OneSharedListener
{
    std::shared_ptr<SimulationListener> listener;

    [[nodiscard]] std::vector<std::shared_ptr<SimulationListener>> operator()(
        const std::stop_token& /*stop*/) const
    {
        return {listener};
    }
};

/// The extra listeners of a run of the threads test: the meeting of the two views' runs, watching
/// the job's stop.
struct MeetTheOtherView
{
    const QtRocket::Test::StepMeeting* meeting;

    [[nodiscard]] std::vector<std::shared_ptr<SimulationListener>> operator()(
        const std::stop_token& stop) const
    {
        return {meeting->listener(stop)};
    }
};

/// Gives @p view's estimate the executor @p executor (the application's, on real threads) while
/// it lives, and takes it away again (the view's run abandoned) when it goes. Declared after the
/// executor, which is declared after the view: so the view abandons its job before the executor
/// is destroyed, and the executor joins its threads before the view (the document, the preference
/// store its run reads) goes (SimulationRunner: what a running job refers to outlives it).
class ViewOnTheExecutor
{
public:
    ViewOnTheExecutor(EstimateView& view, SimulationExecutor& executor) : m_view(&view)
    {
        view.background().setExecutor(&executor, &view.window());
    }
    ~ViewOnTheExecutor() { m_view->background().setExecutor(nullptr, nullptr); }
    ViewOnTheExecutor(const ViewOnTheExecutor&)            = delete;
    ViewOnTheExecutor& operator=(const ViewOnTheExecutor&) = delete;
    ViewOnTheExecutor(ViewOnTheExecutor&&)                 = delete;
    ViewOnTheExecutor& operator=(ViewOnTheExecutor&&)      = delete;

private:
    EstimateView* m_view;
};

/// The request's cancelBackgroundRuns of a design window: the estimate's cancelFor().
struct CancelTheEstimate
{
    BackgroundSimulation* background;

    void operator()(std::span<const std::shared_ptr<Simulation>> simulations) const
    {
        background->cancelFor(simulations);
    }
};

TEST_F(BackgroundSimulationTest, IsOffWithoutAnExecutor)
{
    // A design view without an executor (the tests' panels, M2 and M3's goldens) shows no flight
    // block, whatever "backgroundFlight" says.
    const QtRocket::Test::DesignFileEnvironment environment;
    std::unique_ptr<OpenRocketDocument> document = loadDesign(environment, simpleModelRocket());
    const InMemoryPreferences           preferences;
    RocketFigure                        figure(document->getRocket(), preferences, 96.0, nullptr);
    DesignPhysics                       physics(figure, preferences);
    const BackgroundSimulation          background(physics, *document, preferences);
    EXPECT_EQ(background.executor(), nullptr);
    EXPECT_EQ(background.debounceInterval(), BackgroundSimulation::kDebounceInterval);
    EXPECT_FALSE(physics.info().flightData().has_value());
    EXPECT_FALSE(physics.info().isCalculatingData());
    EXPECT_FALSE(background.isCalculating());
}

TEST_F(BackgroundSimulationTest, ShowsTheFirstSimulationOfTheSelectedConfiguration)
{
    // "A simple model rocket": every simulation is up to date (loaded), so nothing runs, and the
    // block shows the stored data of the first simulation of the selected configuration (#4).
    EstimateView view(simpleModelRocket());
    EXPECT_FALSE(view.background().isCalculating());
    EXPECT_FALSE(view.background().isStartPending());
    EXPECT_TRUE(view.executor().submittedJobs().empty());
    EXPECT_TRUE(isSummaryOf(view.info().flightData(), *view.simulation(3)->getSimulatedData()));
    EXPECT_FALSE(view.info().abortedStatus().has_value());
    EXPECT_FALSE(view.info().isCalculatingData());
}

TEST_F(BackgroundSimulationTest, RunsTheSimulationThatIsNotUpToDateAndAdoptsItsRun)
{
    // updateSims(false): the first simulation of the selected configuration that is not up to
    // date runs after the debounce, as a job of the window, while the overlay shows its old data
    // dimmed; its run is adopted by the document's simulation (up to date, the design unsaved:
    // U-M4-5), the document is told, and the overlay shows the new data.
    EstimateView                      view(simpleModelRocket());
    const std::shared_ptr<Simulation> first = view.selectOutdatedFirstSimulation();
    const std::shared_ptr<FlightData> old   = first->getSimulatedData();
    ASSERT_NE(old, nullptr);
    EXPECT_TRUE(view.background().isCalculating());
    EXPECT_TRUE(view.info().isCalculatingData());
    EXPECT_TRUE(isSummaryOf(view.info().flightData(), *old));
    ASSERT_TRUE(view.waitForStart());
    EXPECT_EQ(view.background().runningSimulation(), first);
    ASSERT_EQ(view.executor().submittedJobs().size(), 1U);
    EXPECT_EQ(view.executor().submittedJobs().front().window, &view.window());

    view.document().setSaved(true);
    const DocumentEvents events(view.document());
    ASSERT_TRUE(view.executor().runNext());
    EXPECT_EQ(first->getStatus(), Simulation::Status::UPTODATE);
    ASSERT_NE(first->getSimulatedData(), nullptr);
    EXPECT_NE(first->getSimulatedData(), old);
    EXPECT_TRUE(endsAtTheGroundHit(*first->getSimulatedData()));
    EXPECT_FALSE(view.document().isSaved());
    EXPECT_TRUE(events.hasSimulationChange(*first));
    EXPECT_FALSE(view.background().isCalculating());
    EXPECT_FALSE(view.info().isCalculatingData());
    EXPECT_EQ(view.background().job(), SimulationExecutor::kNoJob);
    EXPECT_TRUE(isSummaryOf(view.info().flightData(), *first->getSimulatedData()));
}

TEST_F(BackgroundSimulationTest, TheRunOfASimulationRemovedMeanwhileIsNotAdopted)
{
    // Adopted only while its simulation is still in the document (U-M4-2): the run of #1 ends
    // after #1 left the document, which keeps its old data and status and hears nothing of it.
    EstimateView                      view(simpleModelRocket());
    const std::shared_ptr<Simulation> first = view.selectOutdatedFirstSimulation();
    ASSERT_TRUE(view.waitForStart());
    const std::shared_ptr<FlightData> old    = first->getSimulatedData();
    const Simulation::Status          status = first->getStatus();
    view.document().removeSimulation(*first);
    view.document().setSaved(true);
    const DocumentEvents events(view.document());
    ASSERT_TRUE(view.executor().runNext());
    EXPECT_EQ(first->getSimulatedData(), old);
    EXPECT_EQ(first->getStatus(), status);
    EXPECT_TRUE(events.events().empty());
    EXPECT_TRUE(view.document().isSaved());
    EXPECT_FALSE(view.background().isCalculating());
    EXPECT_EQ(view.background().job(), SimulationExecutor::kNoJob);
}

TEST_F(BackgroundSimulationTest, ASimulationThatLeftTheDocumentBeforeItsTurnIsNotRun)
{
    // The auto-run's chain [#1, "Second"]: "Second" is removed while #1 runs, so its turn never
    // comes (no job), and the estimate is no longer calculating.
    EstimateView                      view(simpleModelRocket());
    const std::shared_ptr<Simulation> first = view.simulation(0);
    const std::shared_ptr<Simulation> second =
        first->duplicateSimulation(view.document().getRocket());
    second->setName("Second");
    view.document().addSimulation(second);
    view.preferences().setAutoRunSimulations(true);
    view.background().setSimulationsTabSelected(true);
    static_cast<void>(view.selectOutdatedFirstSimulation());
    ASSERT_TRUE(view.waitForStart());
    ASSERT_EQ(view.background().chain(), (std::vector<std::shared_ptr<Simulation>>{second}));
    view.document().removeSimulation(*second);
    ASSERT_TRUE(view.executor().runNext());
    EXPECT_EQ(first->getStatus(), Simulation::Status::UPTODATE);
    EXPECT_FALSE(view.waitForStart());
    EXPECT_FALSE(view.background().isStartPending());
    EXPECT_EQ(view.executor().submittedJobs().size(), 1U);
    EXPECT_EQ(second->getSimulatedData(), nullptr);
    EXPECT_FALSE(view.background().isCalculating());
    EXPECT_FALSE(view.info().isCalculatingData());
}

TEST_F(BackgroundSimulationTest, TwoDesignViewsEstimateAtTheSameTimeOverOneExecutor)
{
    // The application's executor serves every window: two estimates are live at once, each run
    // with a GroundHitListener of its own (a listener is one live job's; Java shares its
    // INSTANCE).
    ManualSimulationExecutor          executor(Mode::STEPPED);
    EstimateView                      one(simpleModelRocket(), &executor);
    EstimateView                      other(simpleModelRocket(), &executor);
    const std::shared_ptr<Simulation> first  = one.selectOutdatedFirstSimulation();
    const std::shared_ptr<Simulation> second = other.selectOutdatedFirstSimulation();
    ASSERT_TRUE(one.waitForStart());
    ASSERT_TRUE(other.waitForStart());
    EXPECT_EQ(executor.waitingJobs().size(), 2U);
    EXPECT_EQ(executor.runAll(), 2U);
    EXPECT_EQ(first->getStatus(), Simulation::Status::UPTODATE);
    EXPECT_EQ(second->getStatus(), Simulation::Status::UPTODATE);
    EXPECT_TRUE(endsAtTheGroundHit(*second->getSimulatedData()));
    EXPECT_FALSE(one.background().isCalculating());
    EXPECT_FALSE(other.background().isCalculating());
}

TEST_F(BackgroundSimulationTest, ARunTheExecutorRefusesIsReportedAndCalculatesNoLonger)
{
    // A defect (here an extra listener that another live job has): the executor's BugError is
    // reported through Guard, and the view does not stay on "Calculating...".
    const RecordingGuardHook hook;
    ManualSimulationExecutor executor(Mode::STEPPED);
    EstimateView             one(simpleModelRocket(), &executor);
    EstimateView             other(simpleModelRocket(), &executor);
    const OneSharedListener  shared{.listener = std::make_shared<GroundHitListener>()};
    one.background().setExtraListeners(shared);
    other.background().setExtraListeners(shared);
    static_cast<void>(one.selectOutdatedFirstSimulation());
    ASSERT_TRUE(one.waitForStart());
    static_cast<void>(other.selectOutdatedFirstSimulation());
    EXPECT_FALSE(other.waitForStart());
    ASSERT_EQ(hook.failures().size(), 1U);
    EXPECT_EQ(hook.failures().front().kind, Guard::Failure::Kind::BUG);
    EXPECT_TRUE(
        hook.failures().front().message.contains(QStringLiteral("the listener of one job")));
    EXPECT_EQ(hook.failures().front().context, QStringLiteral("Background simulation"));
    EXPECT_FALSE(other.background().isCalculating());
    EXPECT_FALSE(other.info().isCalculatingData());
    EXPECT_TRUE(other.background().chain().empty());
    EXPECT_EQ(other.background().job(), SimulationExecutor::kNoJob);
    EXPECT_EQ(executor.submittedJobs().size(), 1U);
    // The first view's run goes on.
    EXPECT_TRUE(one.background().isCalculating());
    EXPECT_EQ(executor.runAll(), 1U);
    EXPECT_FALSE(one.background().isCalculating());
}

TEST_F(BackgroundSimulationTest, AFunctionalChangeAbandonsTheRunAndEstimatesAgain)
{
    // stopBackgroundSimulation(): a change of the design while a run goes abandons it (nothing of
    // it is adopted) and asks for a new one.
    EstimateView                      view(simpleModelRocket());
    const std::shared_ptr<Simulation> first = view.selectOutdatedFirstSimulation();
    ASSERT_TRUE(view.waitForStart());
    const SimulationExecutor::JobId job    = view.background().job();
    Rocket&                         rocket = view.document().getRocket();
    rocket.getChild(0).getChild(0).setName("A renamed nose cone");
    view.deliverRocketChanges();
    // A rename is no functional change: the run goes on.
    EXPECT_EQ(view.background().job(), job);
    EXPECT_TRUE(view.executor().abandonedJobs().empty());
    rocket.getChild(0).getChild(0).setOverrideMass(0.1);
    rocket.getChild(0).getChild(0).setMassOverridden(true);
    view.deliverRocketChanges();
    EXPECT_EQ(view.executor().abandonedJobs(), (std::vector<SimulationExecutor::JobId>{job}));
    EXPECT_TRUE(view.background().isCalculating());
    ASSERT_TRUE(view.waitForStart());
    EXPECT_NE(view.background().job(), job);
    EXPECT_EQ(view.background().runningSimulation(), first);
}

TEST_F(BackgroundSimulationTest, TurningTheEstimateOffAndOnShowsItAgainAtOnce)
{
    // OpenRocket bug 54 (quirk (a)): off, the block goes; on again (the preferences' refresh of
    // the design view: DesignPhysics::update()), it shows at once, where Java's remembered ids
    // keep it hidden until the next change of the design.
    EstimateView view(simpleModelRocket());
    ASSERT_TRUE(view.info().flightData().has_value());
    view.preferences().putBoolean(GuiPreferenceKeys::kBackgroundFlight, false);
    view.physics().update();
    EXPECT_FALSE(view.info().flightData().has_value());
    view.preferences().putBoolean(GuiPreferenceKeys::kBackgroundFlight, true);
    view.physics().update();
    EXPECT_TRUE(isSummaryOf(view.info().flightData(), *view.simulation(3)->getSimulatedData()));
}

TEST_F(BackgroundSimulationTest, TurningTheEstimateOffStopsTheRun)
{
    EstimateView view(simpleModelRocket());
    static_cast<void>(view.selectOutdatedFirstSimulation());
    ASSERT_TRUE(view.waitForStart());
    const SimulationExecutor::JobId job = view.background().job();
    view.preferences().putBoolean(GuiPreferenceKeys::kBackgroundFlight, false);
    view.physics().update();
    EXPECT_EQ(view.executor().abandonedJobs(), (std::vector<SimulationExecutor::JobId>{job}));
    EXPECT_FALSE(view.background().isCalculating());
    EXPECT_FALSE(view.info().isCalculatingData());
    EXPECT_FALSE(view.info().flightData().has_value());
}

TEST_F(BackgroundSimulationTest, AConfigurationWithoutMotorsShowsNotAvailable)
{
    // RocketPanel.updateExtras(): a configuration without motors shows the block with "N/A".
    const QtRocket::Test::DesignFileEnvironment environment;
    std::unique_ptr<OpenRocketDocument>         document = loadDesign(environment, storedFlight());
    const InMemoryPreferences                   preferences;
    ManualSimulationExecutor                    executor(Mode::NEVER);
    RocketFigure         figure(document->getRocket(), preferences, 96.0, nullptr);
    DesignPhysics        physics(figure, preferences);
    BackgroundSimulation background(physics, *document, preferences);
    background.setExecutor(&executor, nullptr);
    ASSERT_FALSE(document->getRocket().getSelectedConfiguration().hasMotors());
    EXPECT_TRUE(isNotAvailable(physics.info().flightData()));
    EXPECT_FALSE(physics.info().abortedStatus().has_value());
    EXPECT_FALSE(background.isCalculating());
}

TEST_F(BackgroundSimulationTest, AnAbortedSimulationShowsItsStatusOverTheBlockAndRunsAgain)
{
    // The stored flight that aborted: its status text over its values (RocketInfo.java:614-626),
    // dimmed, as an ABORTED simulation is not up to date and its run is asked for.
    EstimateView                      view(storedFlight());
    const std::shared_ptr<Simulation> aborted = view.simulation(0);
    view.document().getRocket().setSelectedConfiguration(aborted->getFlightConfigurationId());
    view.deliverRocketChanges();
    EXPECT_EQ(view.info().abortedStatus(),
              std::optional(QStringLiteral("Simulation Aborted: No motors ignited")));
    EXPECT_TRUE(isSummaryOf(view.info().flightData(), *aborted->getSimulatedData()));
    EXPECT_TRUE(view.background().isCalculating());
    EXPECT_EQ(view.background().chain(), (std::vector<std::shared_ptr<Simulation>>{aborted}));
}

TEST_F(BackgroundSimulationTest, AConfigurationWithoutSimulationShowsNoStatusOfAnother)
{
    // OpenRocket bug 55 (quirk (b)): from the aborted simulation's configuration to another one
    // with motors and no simulation, the block shows "N/A" and no status line (Java keeps the
    // earlier simulation and its ABORTED line).
    EstimateView                      view(storedFlight());
    const std::shared_ptr<Simulation> aborted = view.simulation(0);
    Rocket&                           rocket  = view.document().getRocket();
    rocket.setSelectedConfiguration(aborted->getFlightConfigurationId());
    view.deliverRocketChanges();
    ASSERT_TRUE(view.info().abortedStatus().has_value());
    const std::optional<FlightConfigurationId> other =
        otherConfigurationWithMotors(rocket, aborted->getFlightConfigurationId());
    ASSERT_TRUE(other.has_value());
    rocket.setSelectedConfiguration(other.value_or(FlightConfigurationId()));
    view.deliverRocketChanges();
    EXPECT_TRUE(isNotAvailable(view.info().flightData()));
    EXPECT_FALSE(view.info().abortedStatus().has_value());
    EXPECT_FALSE(view.background().isCalculating());
}

TEST_F(BackgroundSimulationTest, TheOverlayFollowsTheSimulationsOfTheDocument)
{
    // Java's change listener of every simulation: a new first simulation of the selected
    // configuration (not simulated) is shown at once, with "N/A".
    const EstimateView                view(simpleModelRocket());
    const std::shared_ptr<Simulation> shown = view.simulation(3);
    const std::shared_ptr<Simulation> copy =
        shown->duplicateSimulation(view.document().getRocket());
    view.document().addSimulation(copy, 0);
    EXPECT_TRUE(isNotAvailable(view.info().flightData()));
    view.document().removeSimulation(*copy);
    EXPECT_TRUE(isSummaryOf(view.info().flightData(), *shown->getSimulatedData()));
}

TEST_F(BackgroundSimulationTest, TheAutoRunOnTheSimulationsTabRunsEverySimulationOneAfterTheOther)
{
    // updateSims(true): with AutoRunSimulations on and the Simulations tab selected, every
    // simulation that is not up to date runs, one after the other (Java's chain of workers).
    EstimateView                      view(simpleModelRocket());
    const std::shared_ptr<Simulation> first = view.simulation(0);
    const std::shared_ptr<Simulation> second =
        first->duplicateSimulation(view.document().getRocket());
    second->setName("Second");
    view.document().addSimulation(second);
    view.preferences().setAutoRunSimulations(true);
    view.background().setSimulationsTabSelected(true);
    static_cast<void>(view.selectOutdatedFirstSimulation());
    ASSERT_TRUE(view.waitForStart());
    EXPECT_EQ(view.background().runningSimulation(), first);
    EXPECT_EQ(view.background().chain(), (std::vector<std::shared_ptr<Simulation>>{second}));
    ASSERT_TRUE(view.executor().runNext());
    EXPECT_EQ(first->getStatus(), Simulation::Status::UPTODATE);
    EXPECT_TRUE(view.background().isCalculating());
    ASSERT_TRUE(view.waitForStart());
    EXPECT_EQ(view.background().runningSimulation(), second);
    ASSERT_TRUE(view.executor().runNext());
    EXPECT_EQ(second->getStatus(), Simulation::Status::UPTODATE);
    EXPECT_FALSE(view.background().isCalculating());
    EXPECT_EQ(view.executor().submittedJobs().size(), 2U);
}

TEST_F(BackgroundSimulationTest, AFailedRunShowsNotAvailableAndEndsTheChain)
{
    // simulationInterrupted(): "N/A", not calculating, and the chain stops (kept as Java: (c));
    // the run is adopted (Java records a failed run as up to date); a change of the shown
    // simulation shows its data again.
    EstimateView                      view(simpleModelRocket());
    const std::shared_ptr<Simulation> first = view.simulation(0);
    const std::shared_ptr<Simulation> second =
        first->duplicateSimulation(view.document().getRocket());
    second->setName("Second");
    view.document().addSimulation(second);
    view.preferences().setAutoRunSimulations(true);
    view.background().setSimulationsTabSelected(true);
    view.background().setExtraListeners(&failingWithASimulationException);
    static_cast<void>(view.selectOutdatedFirstSimulation());
    ASSERT_TRUE(view.waitForStart());
    ASSERT_TRUE(view.executor().runNext());
    EXPECT_TRUE(isNotAvailable(view.info().flightData()));
    EXPECT_FALSE(view.background().isCalculating());
    EXPECT_TRUE(view.background().chain().empty());
    EXPECT_FALSE(view.background().isStartPending());
    EXPECT_EQ(first->getStoredStatus(), Simulation::Status::UPTODATE);
    first->setName("Renamed");
    ASSERT_NE(first->getSimulatedData(), nullptr);
    EXPECT_TRUE(isSummaryOf(view.info().flightData(), *first->getSimulatedData()));
    EXPECT_EQ(view.executor().submittedJobs().size(), 1U);
}

TEST_F(BackgroundSimulationTest, AnExceptionOfTheRunIsReported)
{
    // A defect in a run (no failure of the simulation) is reported through Guard; Java's worker
    // only shows "N/A".
    const RecordingGuardHook hook;
    EstimateView             view(simpleModelRocket());
    view.background().setExtraListeners(&failingWithAnotherException);
    static_cast<void>(view.selectOutdatedFirstSimulation());
    ASSERT_TRUE(view.waitForStart());
    ASSERT_TRUE(view.executor().runNext());
    ASSERT_EQ(hook.failures().size(), 1U);
    EXPECT_EQ(hook.failures().front().message, QStringLiteral("not a simulation exception"));
    EXPECT_EQ(hook.failures().front().context,
              QStringLiteral("An exception occurred during the background simulation"));
    EXPECT_TRUE(isNotAvailable(view.info().flightData()));
}

TEST_F(BackgroundSimulationTest, NoRunStartsOrIsAdoptedWhilePaused)
{
    // The critic's G4: while a simulation dialog or the run dialog of the window is open, the
    // running job is abandoned and no run starts; resuming starts it again.
    EstimateView                      view(simpleModelRocket());
    const std::shared_ptr<Simulation> first = view.selectOutdatedFirstSimulation();
    ASSERT_TRUE(view.waitForStart());
    const SimulationExecutor::JobId job = view.background().job();
    view.background().setPaused(true);
    EXPECT_TRUE(view.background().isPaused());
    EXPECT_EQ(view.executor().abandonedJobs(), (std::vector<SimulationExecutor::JobId>{job}));
    EXPECT_EQ(view.background().chain(), (std::vector<std::shared_ptr<Simulation>>{first}));
    EXPECT_TRUE(view.background().isCalculating());
    QCoreApplication::processEvents();
    EXPECT_EQ(view.background().job(), SimulationExecutor::kNoJob);
    EXPECT_FALSE(view.background().isStartPending());
    view.background().setPaused(false);
    ASSERT_TRUE(view.waitForStart());
    EXPECT_EQ(view.background().runningSimulation(), first);
    EXPECT_EQ(view.executor().submittedJobs().size(), 2U);
}

TEST_F(BackgroundSimulationTest, AnEstimateAskedForWhilePausedStartsWhenResumed)
{
    EstimateView view(simpleModelRocket());
    view.background().setPaused(true);
    const std::shared_ptr<Simulation> first = view.selectOutdatedFirstSimulation();
    EXPECT_TRUE(view.background().isCalculating());
    EXPECT_FALSE(view.background().isStartPending());
    EXPECT_EQ(view.background().chain(), (std::vector<std::shared_ptr<Simulation>>{first}));
    view.background().setPaused(false);
    ASSERT_TRUE(view.waitForStart());
    EXPECT_EQ(view.background().runningSimulation(), first);
}

TEST_F(BackgroundSimulationTest, CancelForTakesSimulationsOutOfTheEstimate)
{
    // The run dialog's batch takes the simulations it runs: their runs are abandoned, the next of
    // the chain starts, and with nothing left the overlay is no longer calculating.
    EstimateView                      view(simpleModelRocket());
    const std::shared_ptr<Simulation> first = view.simulation(0);
    const std::shared_ptr<Simulation> second =
        first->duplicateSimulation(view.document().getRocket());
    second->setName("Second");
    view.document().addSimulation(second);
    view.preferences().setAutoRunSimulations(true);
    view.background().setSimulationsTabSelected(true);
    static_cast<void>(view.selectOutdatedFirstSimulation());
    ASSERT_TRUE(view.waitForStart());
    const SimulationExecutor::JobId job = view.background().job();
    view.background().cancelFor(std::vector{first});
    EXPECT_EQ(view.executor().abandonedJobs(), (std::vector<SimulationExecutor::JobId>{job}));
    ASSERT_TRUE(view.waitForStart());
    EXPECT_EQ(view.background().runningSimulation(), second);
    view.background().cancelFor(std::vector{second});
    EXPECT_EQ(view.background().job(), SimulationExecutor::kNoJob);
    EXPECT_FALSE(view.background().isCalculating());
    EXPECT_FALSE(view.info().isCalculatingData());
}

TEST_F(BackgroundSimulationTest, TheRunDialogsBatchCancelsTheEstimateOfItsSimulations)
{
    // UserPrompts::RunSimulationsRequest::cancelBackgroundRuns (G1's hook) filled with
    // cancelFor(): the batch's start abandons the estimate's run of the simulation it runs, and
    // its own run is adopted.
    EstimateView                      view(simpleModelRocket());
    const std::shared_ptr<Simulation> first = view.selectOutdatedFirstSimulation();
    ASSERT_TRUE(view.waitForStart());
    const SimulationExecutor::JobId    job = view.background().job();
    ScriptedUserPrompts                prompts;
    UserPrompts::RunSimulationsRequest request =
        runRequestOf(view.document(), {first}, view.executor(), &view.window());
    request.cancelBackgroundRuns = CancelTheEstimate{.background = &view.background()};
    SimulationBatch batch(prompts, request);
    batch.start();
    EXPECT_EQ(view.executor().abandonedJobs(), (std::vector<SimulationExecutor::JobId>{job}));
    EXPECT_FALSE(view.background().isCalculating());
    view.executor().runAll();
    EXPECT_TRUE(batch.isFinished());
    EXPECT_EQ(first->getStatus(), Simulation::Status::UPTODATE);
    EXPECT_TRUE(isSummaryOf(view.info().flightData(), *first->getSimulatedData()));
}

TEST_F(BackgroundSimulationTest, DestroyingItAbandonsItsRun)
{
    ManualSimulationExecutor  executor(Mode::STEPPED);
    SimulationExecutor::JobId job = SimulationExecutor::kNoJob;
    {
        EstimateView view(simpleModelRocket(), &executor);
        static_cast<void>(view.selectOutdatedFirstSimulation());
        ASSERT_TRUE(view.waitForStart());
        job = view.background().job();
    }
    EXPECT_EQ(executor.abandonedJobs(), (std::vector<SimulationExecutor::JobId>{job}));
    EXPECT_TRUE(executor.waitingJobs().empty());
}

TEST_F(BackgroundSimulationTest, ANewExecutorEstimatesAgain)
{
    // setExecutor(): the run of the old one abandoned, the estimate made anew with the new one;
    // none switches it off.
    EstimateView view(simpleModelRocket());
    static_cast<void>(view.selectOutdatedFirstSimulation());
    ASSERT_TRUE(view.waitForStart());
    const SimulationExecutor::JobId job = view.background().job();
    ManualSimulationExecutor        other(Mode::STEPPED);
    view.background().setExecutor(&other, &view.window());
    EXPECT_EQ(view.executor().abandonedJobs(), (std::vector<SimulationExecutor::JobId>{job}));
    ASSERT_TRUE(view.waitForStart());
    EXPECT_EQ(other.submittedJobs().size(), 1U);
    view.background().setExecutor(nullptr, nullptr);
    EXPECT_EQ(other.abandonedJobs().size(), 1U);
    EXPECT_FALSE(view.info().flightData().has_value());
}

TEST_F(BackgroundSimulationThreadsTest, TheEstimateRunsOnTheApplicationsExecutor)
{
    // The application's executor (a worker thread, the GUI thread polling its mailbox): the run
    // of simulation #1 is adopted on the GUI thread and shown.
    EstimateView                      view(simpleModelRocket());
    PollingSimulationExecutor         executor(1);
    const ViewOnTheExecutor           onTheExecutor(view, executor);
    const std::shared_ptr<Simulation> first = view.selectOutdatedFirstSimulation();
    const std::shared_ptr<FlightData> old   = first->getSimulatedData();
    ASSERT_TRUE(view.waitUntilCalculated(kThreadsSafetyNetMilliseconds));
    EXPECT_EQ(first->getStatus(), Simulation::Status::UPTODATE);
    ASSERT_NE(first->getSimulatedData(), nullptr);
    EXPECT_NE(first->getSimulatedData(), old);
    EXPECT_TRUE(isSummaryOf(view.info().flightData(), *first->getSimulatedData()));
    EXPECT_FALSE(view.document().isSaved());
    EXPECT_FALSE(executor.isPolling());
}

TEST_F(BackgroundSimulationThreadsTest, TwoDesignViewsEstimateAtTheSameTime)
{
    // Two windows' estimates on the application's executor at once (their runs meet at step 3:
    // both are live jobs then), each with listeners of its own; both are adopted.
    const QtRocket::Test::StepMeeting meeting(2, 3);
    EstimateView                      one(simpleModelRocket());
    EstimateView                      other(simpleModelRocket());
    one.background().setExtraListeners(MeetTheOtherView{.meeting = &meeting});
    other.background().setExtraListeners(MeetTheOtherView{.meeting = &meeting});
    PollingSimulationExecutor         executor(2);
    const ViewOnTheExecutor           oneOnTheExecutor(one, executor);
    const ViewOnTheExecutor           otherOnTheExecutor(other, executor);
    const std::shared_ptr<Simulation> first  = one.selectOutdatedFirstSimulation();
    const std::shared_ptr<Simulation> second = other.selectOutdatedFirstSimulation();
    ASSERT_TRUE(one.waitUntilCalculated(kThreadsSafetyNetMilliseconds));
    ASSERT_TRUE(other.waitUntilCalculated(kThreadsSafetyNetMilliseconds));
    EXPECT_TRUE(meeting.allMet());
    EXPECT_EQ(first->getStatus(), Simulation::Status::UPTODATE);
    EXPECT_EQ(second->getStatus(), Simulation::Status::UPTODATE);
    EXPECT_TRUE(isSummaryOf(one.info().flightData(), *first->getSimulatedData()));
    EXPECT_TRUE(isSummaryOf(other.info().flightData(), *second->getSimulatedData()));
    EXPECT_FALSE(executor.isPolling());
}

}  // namespace

}  // namespace QtRocket::Gui::Test
