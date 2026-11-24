#include "QtRocket/simulation/SimulationRun.h"

#include <exception>
#include <memory>
#include <optional>
#include <source_location>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>

#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

namespace
{

/// The Error of a run that ends before the duplicate is simulated.
[[nodiscard]] Error errorOf(ErrorCode code, std::string message,
                            std::source_location where = std::source_location::current())
{
    return Error{.code = code, .message = std::move(message), .where = where};
}

}  // namespace

SimulationRun::SimulationRun(const Simulation& simulation)
  : m_duplicate(simulation.duplicateForIndependentSimulation()),
    // Java's simulate() records getActiveConfiguration().getModID(), at the end of the run.
    m_modIdAtDuplication(simulation.getActiveConfiguration().getModId()),
    // Java's simulate() refuses its field EXTERNAL, which a duplicate never has.
    m_refused(simulation.getStoredStatus() == Simulation::Status::EXTERNAL)
{
}

SimulationRun::~SimulationRun() = default;

void SimulationRun::run(const std::stop_token&                               stopToken,
                        std::span<const std::shared_ptr<SimulationListener>> extraListeners)
{
    if (m_outcome != Outcome::NOT_RUN)
    {
        bug("A simulation run is run once");
    }
    if (stopToken.stop_requested())
    {
        // Java: SimulationWorker.doInBackground() of a worker that is cancelled already.
        m_outcome = Outcome::CANCELLED;
        m_error   = errorOf(ErrorCode::CANCELLED, "The simulation was interrupted.");
        return;
    }
    if (m_refused)
    {
        // Java: the SimulationException of simulate() for an imported simulation (its message,
        // and the error code Simulation::simulate() gives a SimulationException).
        m_outcome = Outcome::FAILED;
        m_error   = errorOf(ErrorCode::SIMULATION_ABORTED, "Cannot simulate imported simulation.");
        return;
    }
    try
    {
        std::optional<std::string> branch;
        const Result<void> result = m_duplicate->simulateForRun(stopToken, extraListeners, branch);
        // simulate() records the run on every way out of it.
        m_recorded = true;
        if (result.has_value())
        {
            m_outcome = Outcome::SUCCEEDED;
        }
        else if (result.error().code == ErrorCode::CANCELLED)
        {
            m_outcome = Outcome::CANCELLED;
            m_error   = result.error();
        }
        else
        {
            m_outcome               = Outcome::FAILED;
            m_error                 = result.error();
            m_calculationBranchName = std::move(branch);
        }
    }
    catch (...)
    {
        // Carried to the owning thread (see the class comment). simulate() records the run on
        // its way out of an exception too, unless the exception came from the recording.
        m_exception = std::current_exception();
        m_outcome   = Outcome::EXCEPTION;
        m_recorded  = m_duplicate->getStoredStatus() == Simulation::Status::UPTODATE;
    }
}

bool SimulationRun::isAdoptable() const noexcept
{
    return m_recorded && m_outcome != Outcome::NOT_RUN && m_outcome != Outcome::CANCELLED;
}

bool SimulationRun::adoptInto(Simulation& simulation) const
{
    if (!isAdoptable())
    {
        return false;
    }
    simulation.adoptRun(*m_duplicate, m_modIdAtDuplication);
    return true;
}

std::string_view name(SimulationRun::Outcome outcome) noexcept
{
    switch (outcome)
    {
        case SimulationRun::Outcome::NOT_RUN:
            return "NOT_RUN";
        case SimulationRun::Outcome::SUCCEEDED:
            return "SUCCEEDED";
        case SimulationRun::Outcome::CANCELLED:
            return "CANCELLED";
        case SimulationRun::Outcome::FAILED:
            return "FAILED";
        case SimulationRun::Outcome::EXCEPTION:
            return "EXCEPTION";
    }
    return "NOT_RUN";  // not reached: the switch covers every outcome
}

}  // namespace QtRocket
