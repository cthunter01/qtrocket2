#include "QtRocket/gui/figure/BackgroundSimulation.h"

#include <algorithm>
#include <chrono>
#include <deque>
#include <exception>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

#include <QObject>
#include <QString>
#include <QTimer>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/events/DocumentChangeEvent.h"
#include "QtRocket/gui/figure/DesignPhysics.h"
#include "QtRocket/gui/figure/RocketFigure.h"
#include "QtRocket/gui/figure/RocketInfo.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/SimulationExecutor.h"
#include "QtRocket/gui/util/SimulationStatusText.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationRun.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/simulation/listeners/system/GroundHitListener.h"
#include "QtRocket/util/ModId.h"

namespace QtRocket::Gui
{

namespace
{

/// The context of the object's reports (Guard).
[[nodiscard]] QString context()
{
    return QStringLiteral("Background simulation");
}

}  // namespace

BackgroundSimulation::BackgroundSimulation(DesignPhysics& physics, OpenRocketDocument& document,
                                           const Preferences& preferences, QObject* parent)
  : QObject(parent), m_physics(&physics), m_document(&document), m_preferences(&preferences)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(kDebounceInterval);
    connect(&m_debounce, &QTimer::timeout, this, guarded([this] { startNext(); }, context()));
    connect(&physics, &DesignPhysics::updated, this, guarded([this] { update(); }, context()));
    m_documentConnection = document.documentChanged().connect(
        guarded([this](const DocumentChangeEvent& event) { documentChanged(event); }, context()));
    update();
}

BackgroundSimulation::~BackgroundSimulation()
{
    // No callback of the job comes after this: it captures the object.
    abandonJob();
}

void BackgroundSimulation::setExecutor(SimulationExecutor* executor, const QObject* window)
{
    stop();
    m_executor = executor;
    m_window   = window;
    // Estimate anew with the new executor (or none).
    m_estimatedFunctionalId = ModId::invalid();
    m_estimatedConfiguration.reset();
    update();
}

void BackgroundSimulation::setDebounceInterval(std::chrono::milliseconds interval)
{
    m_debounce.setInterval(interval);
}

std::chrono::milliseconds BackgroundSimulation::debounceInterval() const noexcept
{
    return m_debounce.intervalAsDuration();
}

void BackgroundSimulation::setPaused(bool paused)
{
    if (paused == m_paused)
    {
        return;
    }
    m_paused = paused;
    if (paused)
    {
        // No run goes and none is adopted while paused: the running one starts again when the
        // pause ends, first of the chain.
        if (m_job != SimulationExecutor::kNoJob)
        {
            std::shared_ptr<Simulation> simulation = m_runningSimulation;
            abandonJob();
            m_chain.push_front(std::move(simulation));
        }
        m_debounce.stop();
        return;
    }
    if (!m_chain.empty())
    {
        requestStart();
    }
}

void BackgroundSimulation::cancelFor(std::span<const std::shared_ptr<Simulation>> simulations)
{
    const auto among = [simulations](const Simulation* simulation) {
        return std::ranges::any_of(simulations, [simulation](const std::shared_ptr<Simulation>& s) {
            return s.get() == simulation;
        });
    };
    std::erase_if(m_chain, [&among](const std::shared_ptr<Simulation>& simulation) {
        return among(simulation.get());
    });
    if (m_runningSimulation != nullptr && among(m_runningSimulation.get()))
    {
        abandonJob();
        if (!m_chain.empty())
        {
            requestStart();
        }
    }
    if (m_job == SimulationExecutor::kNoJob && m_chain.empty() && m_calculating)
    {
        m_debounce.stop();
        m_calculating = false;
        refreshOverlay();
    }
}

std::vector<std::shared_ptr<Simulation>> BackgroundSimulation::chain() const
{
    return {m_chain.begin(), m_chain.end()};
}

bool BackgroundSimulation::isEnabled() const
{
    return m_executor != nullptr &&
           m_preferences->getBoolean(GuiPreferenceKeys::kBackgroundFlight,
                                     GuiPreferenceKeys::kBackgroundFlightDefault);
}

void BackgroundSimulation::update()
{
    ////////  Flight simulation in background

    // Check whether to compute or not
    if (!isEnabled())
    {
        m_shown       = Shown::NOTHING;
        m_calculating = false;
        stop();
        // OpenRocket bug 54: the estimate is forgotten, so that turning it on again shows it at
        // once (Java keeps the ids, and shows nothing until the next change of the design).
        m_estimatedFunctionalId = ModId::invalid();
        m_estimatedConfiguration.reset();
        refreshOverlay();
        return;
    }

    const Rocket&              rocket        = std::as_const(m_document->getRocket());
    const FlightConfiguration& configuration = rocket.getSelectedConfiguration();
    // Check whether data is already up to date
    if (m_estimatedFunctionalId == rocket.getFunctionalModId() &&
        m_estimatedConfiguration == configuration.getId())
    {
        // Java's overlay reads the simulation's status at every paint.
        refreshOverlay();
        return;
    }

    m_estimatedFunctionalId  = rocket.getFunctionalModId();
    m_estimatedConfiguration = configuration.getId();

    // Stop previous computation (if any)
    stop();
    m_failed = false;

    // Check that configuration has motors
    if (!configuration.hasMotors())
    {
        m_shown       = Shown::NOT_AVAILABLE;
        m_calculating = false;
        refreshOverlay();
        return;
    }

    // Update simulations: all of them with the auto-run on and the Simulations tab selected,
    // otherwise the simulation of the current configuration.
    updateSimulations(m_preferences->getAutoRunSimulations() && m_simulationsTabSelected);
    // The overlay shows the first simulation of the selected configuration.
    m_shown = Shown::SIMULATION;
    refreshOverlay();
}

void BackgroundSimulation::updateSimulations(bool updateAllSims)
{
    // Stop previous computation (if any)
    stop();
    m_calculating = true;
    m_chain       = simulationsToRun(updateAllSims);
    // runBackgroundSimulations()
    if (m_chain.empty())
    {
        m_calculating = false;
        return;
    }
    requestStart();
}

std::deque<std::shared_ptr<Simulation>> BackgroundSimulation::simulationsToRun(
    bool updateAllSims) const
{
    const Rocket&                rocket = std::as_const(m_document->getRocket());
    const FlightConfigurationId& curID  = rocket.getSelectedConfiguration().getId();
    // Re-run the present simulation(s)
    std::deque<std::shared_ptr<Simulation>> sims;
    for (const std::shared_ptr<Simulation>& sim : m_document->getSimulations())
    {
        if (Simulation::isStatusUpToDate(sim->getStatus()) ||
            !rocket.getFlightConfiguration(sim->getFlightConfigurationId()).hasMotors())
        {
            continue;
        }

        // Find a Simulation based on the current flight configuration
        if (!updateAllSims)
        {
            if (sim->getFlightConfigurationId() == curID)
            {
                sims.push_back(sim);
                break;
            }
        }
        else
        {
            sims.push_back(sim);
        }
    }
    return sims;
}

void BackgroundSimulation::stop()
{
    abandonJob();
    m_chain.clear();
    m_debounce.stop();
}

void BackgroundSimulation::abandonJob()
{
    if (m_job != SimulationExecutor::kNoJob && m_executor != nullptr)
    {
        m_executor->abandon(m_job);
    }
    m_job = SimulationExecutor::kNoJob;
    m_run.reset();
    m_runningSimulation.reset();
}

void BackgroundSimulation::requestStart()
{
    if (m_paused)
    {
        return;
    }
    m_calculating = true;
    m_debounce.start();
}

void BackgroundSimulation::startNext()
{
    if (m_paused || m_executor == nullptr || m_job != SimulationExecutor::kNoJob)
    {
        return;
    }
    // A simulation that left the document meanwhile is not run.
    while (!m_chain.empty() && !m_document->getSimulationIndex(*m_chain.front()).has_value())
    {
        m_chain.pop_front();
    }
    if (m_chain.empty())
    {
        m_calculating = false;
        refreshOverlay();
        return;
    }
    std::shared_ptr<Simulation> simulation = std::move(m_chain.front());
    m_chain.pop_front();

    // Java's worker simulates the document's simulation with its own options (no seed is drawn),
    // with the InterruptListener (SimulationRun's, for the stop), the GroundHitListener and the
    // custom expressions.
    auto             run = std::make_shared<SimulationRun>(*simulation);
    std::stop_source stop;
    std::vector<std::shared_ptr<SimulationListener>> listeners{GroundHitListener::instance()};
    if (m_extraListeners)
    {
        std::vector<std::shared_ptr<SimulationListener>> extra = m_extraListeners(stop.get_token());
        listeners.insert(listeners.end(), std::make_move_iterator(extra.begin()),
                         std::make_move_iterator(extra.end()));
    }
    SimulationExecutor::Callbacks callbacks{
        .progressed = nullptr,
        .finished   = [this](const std::exception_ptr& exception) { runFinished(exception); }};
    // No callback comes from inside submit(): the job's id is known before.
    m_job = m_executor->submit(m_window, run, nullptr, std::move(listeners), std::move(stop),
                               std::move(callbacks));
    m_run = std::move(run);
    m_runningSimulation = std::move(simulation);
    m_calculating       = true;
}

void BackgroundSimulation::runFinished(const std::exception_ptr& exception)
{
    const std::shared_ptr<SimulationRun> run        = std::move(m_run);
    const std::shared_ptr<Simulation>    simulation = std::move(m_runningSimulation);
    m_job                                           = SimulationExecutor::kNoJob;

    // Java's worker recorded the run in the document's simulation (simulate()'s finally block,
    // also for a run that failed); here the run is adopted while the simulation is in the
    // document (never one that was stopped: isAdoptable()).
    const bool inDocument = m_document->getSimulationIndex(*simulation).has_value();
    if (inDocument)
    {
        Guard::run([&run, &simulation] { static_cast<void>(run->adoptInto(*simulation)); },
                   context());
    }

    const SimulationRun::Outcome outcome = run->getOutcome();
    if (outcome == SimulationRun::Outcome::SUCCEEDED)
    {
        // simulationDone(): not calculating, repaint, the document told, the next simulation.
        m_calculating = false;
        refreshOverlay();
        if (inDocument)
        {
            Guard::run(
                [this, &simulation] {
                    m_document->fireDocumentChangeEvent(
                        {.kind   = DocumentChangeEvent::Kind::SIMULATION,
                         .source = DocumentChangeEvent::Source(simulation.get())});
                },
                context());
        }
        // Run the new simulation after this one has ended
        if (!m_chain.empty())
        {
            requestStart();
            refreshOverlay();
        }
        Q_EMIT runEnded(simulation, true);
        return;
    }

    if (outcome == SimulationRun::Outcome::CANCELLED && run->wasStopped())
    {
        // Stopped by another than this object (the executor's cancel()): the chain ends.
        m_chain.clear();
        m_calculating = false;
        refreshOverlay();
        Q_EMIT runEnded(simulation, false);
        return;
    }

    // simulationInterrupted(): "N/A", not calculating; the chain stops (Java's worker is not
    // replaced). An exception that is no failure of the simulation is a defect: reported.
    if (exception != nullptr)
    {
        Guard::report(Guard::failureOf(
            exception, QStringLiteral("An exception occurred during the background simulation")));
    }
    m_chain.clear();
    m_failed      = true;
    m_calculating = false;
    refreshOverlay();
    Q_EMIT runEnded(simulation, false);
}

void BackgroundSimulation::documentChanged(const DocumentChangeEvent& event)
{
    // The overlay follows the simulations: a change of one (its source), of the list or of all
    // of them (an undo: kind SIMULATION).
    const Simulation* changed = event.getSimulation();
    if (!event.isSimulationChange() && changed == nullptr)
    {
        return;
    }
    // A change of the shown simulation shows its data again after a failed run (Java's change
    // listener sets the overlay's simulation again).
    if (m_failed && changed != nullptr && shownSimulation().get() == changed)
    {
        m_failed = false;
    }
    refreshOverlay();
}

std::shared_ptr<Simulation> BackgroundSimulation::shownSimulation() const
{
    const FlightConfigurationId& curID =
        std::as_const(m_document->getRocket()).getSelectedConfiguration().getId();
    for (const std::shared_ptr<Simulation>& sim : m_document->getSimulations())
    {
        if (sim->getFlightConfigurationId() == curID)
        {
            return sim;
        }
    }
    return nullptr;
}

void BackgroundSimulation::refreshOverlay()
{
    DesignPhysics* const physics = m_physics.data();
    if (physics == nullptr)
    {
        return;
    }
    std::optional<RocketInfo::FlightSummary> data;
    std::optional<QString>                   status;
    switch (m_shown)
    {
        case Shown::NOTHING:
            break;
        case Shown::NOT_AVAILABLE:
            data = RocketInfo::FlightSummary::notAvailable();
            break;
        case Shown::SIMULATION:
        {
            // OpenRocket bug 55: without a simulation of the selected configuration, "N/A" and
            // no status line (Java keeps the simulation of another configuration).
            const std::shared_ptr<Simulation> simulation = shownSimulation();
            if (simulation == nullptr)
            {
                data = RocketInfo::FlightSummary::notAvailable();
                break;
            }
            const std::shared_ptr<FlightData>& flight = simulation->getSimulatedData();
            data = m_failed || flight == nullptr ? RocketInfo::FlightSummary::notAvailable()
                                                 : RocketInfo::FlightSummary::of(*flight);
            // Sim status. Only show on aborts
            if (simulation->getStatus() == Simulation::Status::ABORTED)
            {
                status = SimulationStatusText::plainDescription(*simulation);
            }
            break;
        }
    }
    RocketInfo& info = physics->info();
    info.setFlightData(data);
    info.setAbortedStatus(status);
    info.setCalculatingData(m_calculating);
    if (RocketFigure* const figure = physics->figure(); figure != nullptr)
    {
        figure->update();
    }
    Q_EMIT overlayChanged();
}

}  // namespace QtRocket::Gui
