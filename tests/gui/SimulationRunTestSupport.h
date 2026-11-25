#pragma once

// What the GUI's tests of simulation runs share (the executors, the run dialog's batch, the run
// dialog): an example design loaded with what its simulations need to run, the request of a run,
// and listeners that a test adds to a run (SimulationBatch::Options::extraListeners) to act at a
// step of it or to make it fail. Fresh runs use only the short flights of plan 11.6 ("A simple
// model rocket" #1, "Clustered motors" #1, "3D printable" #1) and pin no late value of a fresh
// flight (CLAUDE.md, Portability).

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/simulation/listeners/RunSampleListener.h"
#include "file/DesignFileEnvironment.h"

class QObject;

namespace QtRocket
{
class OpenRocketDocument;
class Simulation;
class SimulationListener;
}  // namespace QtRocket

namespace QtRocket::Gui
{
class SimulationExecutor;
}  // namespace QtRocket::Gui

namespace QtRocket::Gui::Test
{

/// A design of data/examples, loaded as the tests load design files (DesignFileEnvironment: the
/// bundled motors, read once per test process), with the environment whose preference store its
/// simulations keep (declared before the document, so that it outlives it and every run of it).
class ExampleDesign
{
public:
    /// "A simple model rocket.ork".
    static constexpr const char* kSimpleModelRocket = "A simple model rocket.ork";
    /// "Clustered motors.ork".
    static constexpr const char* kClusteredMotors = "Clustered motors.ork";
    /// "Simulation scripting.ork".
    static constexpr const char* kSimulationScripting = "Simulation scripting.ork";
    /// "Three stage low power rocket.ork": stored flights of three stages (no run: they are
    /// long).
    static constexpr const char* kThreeStageLowPowerRocket = "Three stage low power rocket.ork";

    /// The example @p fileName of data/examples (a test failure, and an empty document, when it
    /// cannot be loaded).
    explicit ExampleDesign(const std::string& fileName);
    ~ExampleDesign();
    ExampleDesign(const ExampleDesign&)            = delete;
    ExampleDesign& operator=(const ExampleDesign&) = delete;
    ExampleDesign(ExampleDesign&&)                 = delete;
    ExampleDesign& operator=(ExampleDesign&&)      = delete;

    [[nodiscard]] OpenRocketDocument& document() const noexcept { return *m_document; }

    /// The document's simulation @p index (from 0); null, with a test failure, out of range.
    [[nodiscard]] std::shared_ptr<Simulation> simulation(std::size_t index) const;

    /// A copy of the document's simulation @p index (Simulation::duplicateSimulation(): the same
    /// configuration and options, not simulated), named @p name and added to the end of the
    /// document's simulations: a second run of the same short flight.
    std::shared_ptr<Simulation> addCopyOf(std::size_t index, const std::string& name);

private:
    QtRocket::Test::DesignFileEnvironment m_environment;
    std::unique_ptr<OpenRocketDocument>   m_document;
};

/// The request to run @p simulations of @p document through @p executor, as jobs of @p window,
/// without a background estimate to cancel.
[[nodiscard]] UserPrompts::RunSimulationsRequest runRequestOf(
    OpenRocketDocument& document, std::vector<std::shared_ptr<Simulation>> simulations,
    SimulationExecutor& executor, const QObject* window = nullptr);

/// A listener that calls @p action once, after step @p step of the run (counted over every
/// branch: its clones share the count), on the thread of the run.
[[nodiscard]] std::shared_ptr<SimulationListener> actionAtStep(int                   step,
                                                               std::function<void()> action);

/// How failingRunListener() fails a run.
enum class RunFailure
{
    SIMULATION_EXCEPTION,     ///< SimulationException("the listener gave up"): FAILED
    CALCULATION_WITH_BRANCH,  ///< SimulationCalculationException of the step's branch: FAILED
    OTHER_EXCEPTION,          ///< std::runtime_error("not a simulation exception"): EXCEPTION
};

/// A listener that fails the run at its first step as @p failure says.
[[nodiscard]] std::shared_ptr<SimulationListener> failingRunListener(RunFailure failure);

/// A clock that never moves on (SimulationBatch::Options::sampleClock): a run's sample listener
/// then publishes the first step and the LAUNCH, APOGEE and SIMULATION_END events only, however
/// long the run takes.
[[nodiscard]] RunSampleListener::Clock frozenSampleClock();

}  // namespace QtRocket::Gui::Test
