#pragma once

#include <chrono>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <vector>

#include <QObject>
#include <QPointer>
#include <QTimer>

#include "QtRocket/document/events/DocumentChangeEvent.h"
#include "QtRocket/gui/util/SimulationExecutor.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket
{
class OpenRocketDocument;
class Preferences;
class Simulation;
class SimulationListener;
class SimulationRun;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class DesignPhysics;

/// The design view's estimate of the flight (OpenRocket's RocketPanel: the flight part of
/// updateExtras() :1572-1626, updateSims() :1635-1659, updateFlightData() :1668-1675,
/// runBackgroundSimulations() :1684-1711, stopBackgroundSimulation() and the
/// BackgroundSimulationWorker :1728-1798): it runs the simulations of the selected flight
/// configuration that are not up to date in the background, one after the other, adopts their
/// results into the document (which marks the design unsaved, as in Java: the user's decision
/// U-M4-5), and shows the flight block of the overlay (RocketInfo of a DesignPhysics: the
/// apogee, the maximum velocity and acceleration, "Calculating..." while a run goes).
///
/// update() is Java's updateExtras(), from its flight part on; it runs whenever the design
/// view's numbers are computed (DesignPhysics::updated(): a change of the rocket, of the selected
/// configuration, of a CP parameter, of the view, the preferences' refresh of the design view):
/// 1. the estimate is off (the preference GuiPreferenceKeys::kBackgroundFlight, "backgroundFlight",
///    true by default; or no executor, setExecutor()): no flight block, not calculating, the run
///    stopped;
/// 2. the rocket's functional modification id and the selected configuration are those of the
///    last estimate: nothing more;
/// 3. otherwise both are remembered and the run is stopped (stopBackgroundSimulation());
/// 4. a selected configuration without motors: the flight block with "N/A" (Java's
///    FlightData.NaN_DATA), not calculating;
/// 5. updateSims(): calculating, and the simulations to run, in the document's order, each not
///    up to date (Simulation::isStatusUpToDate() of its getStatus(): UPTODATE, LOADED or
///    EXTERNAL) and of a configuration with motors: the first of the selected configuration only
///    (Java's choice; with AutoRunSimulations on and the Simulations tab selected,
///    setSimulationsTabSelected(), all of them). None: not calculating. (Java's branch for a
///    missing simulation, which would make a new one from the preferences, is dead code: the list
///    holds the document's simulations only.)
/// 6. the overlay shows the first simulation of the selected configuration: its flight data's
///    maxima (or "N/A" without data), with its status text above the block when it is ABORTED
///    (SimulationStatusText::plainDescription()).
/// The runs go through the window's SimulationExecutor (setExecutor()), one at a time, each
/// started a debounce interval after it was asked for (Java's worker sleeps 300 ms first:
/// debounceInterval(), kDebounceInterval by default, 0 in the tests): a SimulationRun of the
/// document's simulation made on the GUI thread then (a duplicate, as the run dialog runs; no seed
/// is drawn, as Java's background run draws none), with the GroundHitListener (each branch ends at
/// its ground hit) and the extra listeners (setExtraListeners(): Milestone 7's custom
/// expressions, a test's holds and failures). When a run ends (the executor's finished()):
/// - it is adopted by its simulation (SimulationRun::adoptInto(): never a run that was stopped)
///   while the simulation is still in the document: Java's simulate() records the run in the
///   document's simulation, also one that failed;
/// - one that succeeded (an aborted flight included): not calculating, the document is told (a
///   SIMULATION change event whose source is the simulation: Java's SimulationChangeEvent; only
///   while the simulation is in the document), and the next simulation of the chain is started
///   after the debounce interval;
/// - one that failed: the flight block shows "N/A" (Java's simulationInterrupted(): NaN_DATA)
///   until the next estimate or a change of the shown simulation, not calculating, and the chain
///   stops (kept as Java: (c) of the scout report). An exception that is no simulation's failure
///   (a defect: a BugError) is also reported through Guard.
/// The overlay follows the document's changes of its simulations (a change of a simulation, of
/// the list, an undo; Java's change listener of every simulation) and is repainted.
///
/// Pausing (setPaused(): the window's simulation dialogs, the critic's G4): while a simulation
/// dialog or the run dialog of the window is open, no run starts and none is adopted (the
/// adoption would make the settings dialog's edits and its Cancel's undo another's, and a run
/// of the run dialog and one of the estimate of the same simulation would race). Pausing
/// abandons the running job (its simulation is first in the chain again) and holds the start of
/// the next; resuming starts the chain again after the debounce interval. update() still works
/// out what to show and what to run while paused. cancelFor() takes simulations out of the
/// estimate's hands: the run dialog's batch hands it the simulations it runs
/// (UserPrompts::RunSimulationsRequest::cancelBackgroundRuns).
///
/// Threads: everything on the GUI thread; the run runs where the executor runs it (U-M4-2). The
/// object holds the simulations it runs and shows by std::shared_ptr only while it uses them, and
/// asks the document whether a simulation is still in it before it adopts its run; the overlay's
/// simulation is looked up anew at every refresh. It abandons its job when it is destroyed, so no
/// callback comes after. The document, the preferences and the executor must outlive it; the
/// design view's DesignPhysics may go first (then nothing is shown or repainted).
///
/// Deviations from OpenRocket:
/// - Java runs the document's simulation on its worker thread; here a duplicate runs and the
///   result is adopted on the GUI thread when the run ends (C1's SimulationRun). An edit during
///   the run leaves the simulation OUTDATED (the modification id the run was prepared with),
///   where Java's record reads the live one.
/// - OpenRocket bug 54 (quirk (a)): turning the estimate off and on again showed nothing until
///   the next change of the design (the remembered ids stayed). Here turning it off forgets them,
///   so the estimate is shown again at once.
/// - OpenRocket bug 55 (quirk (b)): when the selected configuration has no simulation, Java keeps
///   the overlay's earlier simulation, whose ABORTED line (and, with AutoRunSimulations, values)
///   of another configuration then stays. Here the block shows "N/A" without a status line.
/// - Not copied (quirk (d)): Java adds a change listener to every simulation at every estimate
///   and never removes one (a leak); as those listeners re-show the last changed simulation of the
///   selected configuration, Java's overlay may show another simulation of the configuration than
///   the first after a run dialog's run. Here the overlay always shows the first.
/// - The pause and cancelFor() are additions (the critic's G4, O-M4-16).
/// - The debounce is a QTimer on the GUI thread (Java: a 300 ms sleep on the worker), and the
///   chain's next run is started after it on the GUI thread too.
/// - An exception that is no failure of the simulation is reported through Guard (Java's worker
///   swallows every Throwable and shows "N/A").
/// - A run that a third party stopped (the executor's cancel() or cancelAll() of the window: no
///   part of QtRocket does it) ends the chain, not calculating; Java's cancelled worker leaves the
///   overlay as it is (only stopBackgroundSimulation() cancels one, before it sets another state).
/// - No thread priority (Java: Thread.MIN_PRIORITY): std::jthread has none. The runs share the
///   application's executor with the run dialog's (Java: a pool of its own for the estimates).
/// - Without an executor the estimate is off (a design view of the tests, or of a window not
///   given one), as with the preference off.
/// - The selected configuration is compared by value (Java: the FlightConfigurationId object, by
///   identity; an undo keeps the objects).
class BackgroundSimulation final : public QObject
{
    Q_OBJECT

public:
    /// Java's Thread.sleep(300) of the worker before it simulates.
    static constexpr std::chrono::milliseconds kDebounceInterval{300};

    /// The extra listeners of a run, after the GroundHitListener, which @p stopToken (the token
    /// of the job's stop source) stops: Milestone 7's custom expressions, a test's holds and
    /// failures.
    using ExtraListeners =
        std::function<std::vector<std::shared_ptr<SimulationListener>>(const std::stop_token&)>;

    /// The estimate of @p document's rocket shown in @p physics' overlay (RocketInfo), with the
    /// preference "backgroundFlight" and AutoRunSimulations of @p preferences, read at each
    /// update(). Runs nothing until it has an executor (setExecutor()). @p physics must show
    /// @p document's rocket; update() is connected to its updated().
    BackgroundSimulation(DesignPhysics& physics, OpenRocketDocument& document,
                         const Preferences& preferences, QObject* parent = nullptr);
    /// Abandons the running job (see the class comment).
    ~BackgroundSimulation() override;

    BackgroundSimulation(const BackgroundSimulation&)            = delete;
    BackgroundSimulation& operator=(const BackgroundSimulation&) = delete;
    BackgroundSimulation(BackgroundSimulation&&)                 = delete;
    BackgroundSimulation& operator=(BackgroundSimulation&&)      = delete;

    /// The executor of the runs, whose jobs belong to @p window (the design window: it abandons
    /// them as it closes); null (the default) for none: the estimate is off. A change stops the
    /// run and estimates again (update()).
    void setExecutor(SimulationExecutor* executor, const QObject* window);
    [[nodiscard]] SimulationExecutor* executor() const noexcept { return m_executor; }

    /// The time between the request of a run and its start (kDebounceInterval by default; 0 in
    /// the tests: the next turn of the event loop).
    void                                    setDebounceInterval(std::chrono::milliseconds interval);
    [[nodiscard]] std::chrono::milliseconds debounceInterval() const noexcept;

    /// The extra listeners of the runs (none by default).
    void setExtraListeners(ExtraListeners listeners) { m_extraListeners = std::move(listeners); }

    /// Whether the window's Simulations tab is selected (BasicFrame.getSelectedTab() ==
    /// SIMULATION_TAB): with AutoRunSimulations on, update() then runs every simulation that is
    /// not up to date. Read at the next update(); false by default.
    void setSimulationsTabSelected(bool selected) noexcept { m_simulationsTabSelected = selected; }
    [[nodiscard]] bool isSimulationsTabSelected() const noexcept
    {
        return m_simulationsTabSelected;
    }

    /// Pauses the runs while a simulation dialog or the run dialog of the window is open (see the
    /// class comment); false resumes them.
    void               setPaused(bool paused);
    [[nodiscard]] bool isPaused() const noexcept { return m_paused; }

    /// Takes @p simulations out of the estimate: they leave the chain, and a run of one of them
    /// is abandoned (the next of the chain starts after the debounce interval, unless paused);
    /// not calculating once nothing is left to run. For the run dialog's batch, which runs them
    /// (UserPrompts::RunSimulationsRequest::cancelBackgroundRuns).
    void cancelFor(std::span<const std::shared_ptr<Simulation>> simulations);

    /// RocketPanel.updateExtras()'s flight part (see the class comment).
    void update();

    /// Whether the overlay says "Calculating...": a run goes or waits to start.
    [[nodiscard]] bool isCalculating() const noexcept { return m_calculating; }
    /// The job of the running run, or SimulationExecutor::kNoJob.
    [[nodiscard]] SimulationExecutor::JobId job() const noexcept { return m_job; }
    /// The simulation whose run goes (submitted), or null.
    [[nodiscard]] const std::shared_ptr<Simulation>& runningSimulation() const noexcept
    {
        return m_runningSimulation;
    }
    /// The simulations still to run after the running one, the next first.
    [[nodiscard]] std::vector<std::shared_ptr<Simulation>> chain() const;
    /// Whether a run waits for the debounce interval to start.
    [[nodiscard]] bool isStartPending() const noexcept { return m_debounce.isActive(); }

Q_SIGNALS:
    /// What the overlay shows changed (it was repainted): the flight block, its status line or
    /// "Calculating...".
    void overlayChanged();
    /// A run ended and was handled (adopted or not, the document told): its simulation and
    /// whether it succeeded.
    void runEnded(std::shared_ptr<QtRocket::Simulation> simulation, bool succeeded);

private:
    /// What the overlay's flight block shows.
    enum class Shown
    {
        /// No block (the estimate is off).
        NOTHING,
        /// "N/A" (a configuration without motors).
        NOT_AVAILABLE,
        /// The first simulation of the selected configuration ("N/A" without one).
        SIMULATION,
    };

    /// Whether the estimate is on: an executor and the preference.
    [[nodiscard]] bool isEnabled() const;
    /// updateSims(@p updateAllSims): the chain, calculating.
    void updateSimulations(bool updateAllSims);
    /// The simulations updateSims(@p updateAllSims) runs.
    [[nodiscard]] std::deque<std::shared_ptr<Simulation>> simulationsToRun(
        bool updateAllSims) const;
    /// stopBackgroundSimulation(): abandons the job and forgets the chain and the pending start.
    void stop();
    /// Abandons the running job, if any (no callback comes).
    void abandonJob();
    /// Starts the debounce interval of the chain's next run (nothing while paused).
    void requestStart();
    /// The debounce interval is over: the chain's next run is submitted.
    void startNext();
    /// The executor's finished() of the running job.
    void runFinished(const std::exception_ptr& exception);
    /// The document changed: the overlay follows its simulations.
    void documentChanged(const DocumentChangeEvent& event);
    /// Gives the overlay what it shows now and repaints it.
    void refreshOverlay();
    /// The first simulation of the selected configuration, or null.
    [[nodiscard]] std::shared_ptr<Simulation> shownSimulation() const;

    QPointer<DesignPhysics>                              m_physics;
    OpenRocketDocument*                                  m_document;
    const Preferences*                                   m_preferences;
    SimulationExecutor*                                  m_executor{nullptr};
    const QObject*                                       m_window{nullptr};
    ExtraListeners                                       m_extraListeners;
    QTimer                                               m_debounce;
    Signal<const DocumentChangeEvent&>::ScopedConnection m_documentConnection;
    /// The rocket's functional modification id and the selected configuration of the last
    /// estimate (flightDataFunctionalID, flightDataMotorID).
    ModId                                m_estimatedFunctionalId{ModId::invalid()};
    std::optional<FlightConfigurationId> m_estimatedConfiguration;
    /// The simulations still to run, the next first (the running one is m_runningSimulation).
    std::deque<std::shared_ptr<Simulation>> m_chain;
    SimulationExecutor::JobId               m_job{SimulationExecutor::kNoJob};
    std::shared_ptr<SimulationRun>          m_run;
    std::shared_ptr<Simulation>             m_runningSimulation;
    Shown                                   m_shown{Shown::NOTHING};
    /// The last run of the chain failed: "N/A" until the next estimate or a change of the shown
    /// simulation.
    bool m_failed{false};
    bool m_calculating{false};
    bool m_simulationsTabSelected{false};
    bool m_paused{false};
};

}  // namespace QtRocket::Gui
