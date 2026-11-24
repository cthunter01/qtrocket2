#pragma once

#include <exception>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>

#include "QtRocket/util/Error.h"
#include "QtRocket/util/ModId.h"

namespace QtRocket
{

class Simulation;
class SimulationListener;

/// One run of a simulation on a thread of its own, and the hand-over of what came of it to the
/// simulation: what OpenRocket's SimulationWorker does (swing gui/simulation/
/// SimulationWorker.java), with the endings its run dialog tells apart
/// (SimulationRunDialog.java:447-480), without Swing and without the races. An addition.
///
/// Java runs the document's own Simulation on the worker thread (SimulationWorker.java:55)
/// while the table, the design view and the undo history read it on the event thread, and its
/// simulate() drops the data at the start and records the run in a finally block, also when
/// the run is cancelled. Here a run takes three steps on two threads:
/// 1. the constructor, on the thread that owns the simulation and its rocket (the owning
///    thread): a Simulation::duplicateForIndependentSimulation() of the simulation, the
///    modification id its flight configuration has at that moment (getModIdAtDuplication()),
///    and whether it may be simulated at all: an imported simulation may not, which only the
///    simulation can tell (its stored status is EXTERNAL; a duplicate's is NOT_SIMULATED);
/// 2. run(), once, on any thread: the duplicate is simulated, with a stop token and extra
///    listeners, and the outcome says how that ended;
/// 3. adoptInto(), on the owning thread again: the duplicate's record of its run becomes the
///    simulation's (Simulation::adoptRun()), unless the run was cancelled.
/// The simulation is not touched before the third step: it keeps its data and its status while
/// the duplicate runs (Java's table shows no data during a run).
///
/// The outcome (getOutcome()):
/// - SUCCEEDED: simulate() succeeded. A flight that aborted (a SIM_ABORT event) succeeded too:
///   the simulation's status becomes ABORTED, and Java's run dialog shows nothing for it.
/// - CANCELLED: a stop was requested through the token, before the run (nothing runs, as Java's
///   SimulationWorker.doInBackground() does not simulate a worker that is cancelled already) or
///   during it (an InterruptListener, after a step). getError() is ErrorCode::CANCELLED, "The
///   simulation was interrupted."; Java's run dialog shows nothing.
/// - FAILED: simulate() failed with another Error (getError()): a SimulationException of the
///   run (an extension that cannot run, a listener's, a calculation that went wrong, when
///   getCalculationBranchName() names the branch the exception names), an input that is not
///   finite or not in its range, an atmosphere that refuses the launch conditions; or the
///   simulation was refused (isRefused(), nothing runs). Java's run dialog shows an error
///   message with the Error's text (the second and third are QtRocket's own refusals, which
///   it shows the same way).
/// - EXCEPTION: an exception left simulate(), a BugError (a defect of the program) or anything
///   else; getException() holds it. As Java's worker keeps every Throwable of the run
///   (SimulationWorker.java:54-59), the run does not handle it but carries it to the owning
///   thread, whose caller rethrows or reports it (the GUI through Guard::failureOf(), as Java's
///   run dialog hands it to the application's exception handler, "An exception occurred during
///   the simulation"). An exception must not leave the function a std::jthread runs (that is
///   std::terminate()): this is the one place of the core that catches an exception it does
///   not handle.
///
/// Adoption (isAdoptable(), adoptInto()): Java's simulate() records the run in its finally
/// block, on every way out of it. Here the duplicate's simulate() records it in the duplicate,
/// and the simulation takes that record over:
/// - after SUCCEEDED and FAILED, as Java records them: a run that failed leaves the simulation
///   UPTODATE with the data the run got to (with an EXCEPTION event at the end), or without
///   data when nothing ran (a refused input);
/// - after EXCEPTION, as Java records it too, when the duplicate got to record its run;
/// - never after CANCELLED (OpenRocket bug 40: Java's finally block records the partial flight
///   of a cancelled run as up to date, and the results the simulation had are lost; here the
///   simulation keeps them, with its status);
/// - never for a refused (imported) simulation. Java's simulate() refuses to run it, and its
///   finally block then drops the imported data all the same and calls the simulation up to
///   date without data; here it keeps them, with its status. (A bug found while porting: it
///   loses the imported data. No loader makes an imported simulation: a file's "external"
///   status is read as LOADED.)
///
/// Threads: a SimulationRun is not thread-safe, and nothing else may be done to it while run()
/// runs. Between the steps the caller hands it from one thread to the other with a
/// synchronisation that orders them (the start and the join of the thread that runs it, or a
/// mutex the two threads share). The duplicate shares nothing with the simulation it was made
/// from but the preference store, which is thread-safe (see
/// Simulation::duplicateForIndependentSimulation()), so the owning thread may change the
/// simulation, its rocket and the preferences while run() runs. The run holds no reference to
/// the simulation: adoptInto() takes it as an argument, and whoever calls it makes sure that it
/// is the simulation the run was made from and that it is still wanted (in its document).
///
/// Not copyable and not movable: the duplicate is the run's (a run that is handed around is
/// held by a smart pointer).
class SimulationRun
{
public:
    /// How the run ended.
    enum class Outcome
    {
        NOT_RUN,    ///< run() has not been called
        SUCCEEDED,  ///< simulate() succeeded (an aborted flight included)
        CANCELLED,  ///< a stop was requested through the token
        FAILED,     ///< simulate() failed, or the simulation was refused
        EXCEPTION,  ///< an exception left simulate()
    };

    /// Prepares a run of @p simulation, on the thread that owns it and its rocket: makes the
    /// duplicate, notes the modification id of the simulation's flight configuration
    /// (getActiveConfiguration().getModId(), the id Java's simulate() records), and refuses an
    /// imported simulation (stored status EXTERNAL: Java's simulate() checks the field, not
    /// getStatus()). Changes nothing of @p simulation; in particular no random seed is drawn
    /// (Java's run dialog draws them, SimulationOptions::randomizeSeedIfNotFixed(), before it
    /// starts the runs, and so does the caller; the background estimate draws none).
    explicit SimulationRun(const Simulation& simulation);

    SimulationRun(const SimulationRun&)            = delete;
    SimulationRun& operator=(const SimulationRun&) = delete;
    SimulationRun(SimulationRun&&)                 = delete;
    SimulationRun& operator=(SimulationRun&&)      = delete;
    ~SimulationRun();

    /// Runs the duplicate, on any thread, and sets the outcome: Simulation::simulate(
    /// @p stopToken, @p extraListeners) of the duplicate, so with an InterruptListener for
    /// @p stopToken after @p extraListeners (Java: the worker's CancelListener after
    /// getExtraListeners()). Nothing runs when a stop was requested already (CANCELLED) or the
    /// simulation was refused (FAILED, "Cannot simulate imported simulation.", Java's text), in
    /// that order, as in Java. @p extraListeners are the very objects (see SimulationListener,
    /// "Clones", for what they see of the run); they are called on this thread. Nothing that
    /// happens in the run leaves run(): an exception becomes the outcome EXCEPTION.
    /// @throws BugError when the run has been run before (nothing else is done then)
    void run(const std::stop_token&                               stopToken,
             std::span<const std::shared_ptr<SimulationListener>> extraListeners = {});

    /// How the run ended; NOT_RUN before run().
    [[nodiscard]] Outcome getOutcome() const noexcept { return m_outcome; }

    /// The Error of a CANCELLED or FAILED run, whose message Java's run dialog shows in its
    /// error message, under the first line; nullopt for the other outcomes.
    [[nodiscard]] const std::optional<Error>& getError() const noexcept { return m_error; }

    /// The name of the flight data branch whose calculation went wrong in a FAILED run (Java:
    /// SimulationCalculationException.getFlightDataBranch().getName(); the run dialog's first
    /// line is then "An error occurred during simulation branch" followed by the quoted name,
    /// SimuRunDlg.msg.branchErrorOccurred, in place of "An error occurred during the
    /// simulation:", SimuRunDlg.msg.errorOccurred); nullopt for any other ending, and for an
    /// exception that names no branch.
    [[nodiscard]] const std::optional<std::string>& getCalculationBranchName() const noexcept
    {
        return m_calculationBranchName;
    }

    /// The exception of an EXCEPTION run; null for the other outcomes.
    [[nodiscard]] const std::exception_ptr& getException() const noexcept { return m_exception; }

    /// Whether the simulation was refused: it was imported (its stored status was EXTERNAL when
    /// the run was prepared), and run() fails without running it.
    [[nodiscard]] bool isRefused() const noexcept { return m_refused; }

    /// The modification id the simulation's flight configuration had when the run was
    /// prepared: what Simulation::adoptRun() records.
    [[nodiscard]] ModId getModIdAtDuplication() const noexcept { return m_modIdAtDuplication; }

    /// The duplicate that the run simulates; it owns the copy of the rocket it simulates. Read
    /// it on the thread that has the run, never while run() runs: before run() (a progress
    /// estimate reads its flight configuration), and after it (its record of the run).
    [[nodiscard]] Simulation&       getDuplicate() noexcept { return *m_duplicate; }
    [[nodiscard]] const Simulation& getDuplicate() const noexcept { return *m_duplicate; }

    /// Whether adoptInto() adopts the run (see the class comment): it has run, was not
    /// cancelled, and the duplicate recorded its run (which a refused simulation and some
    /// exceptions never get to).
    [[nodiscard]] bool isAdoptable() const noexcept;

    /// Hands the run over to @p simulation, the simulation the run was prepared from, on the
    /// thread that owns it: Simulation::adoptRun(getDuplicate(), getModIdAtDuplication()) when
    /// isAdoptable(), else nothing. Returns whether it adopted the run. The run stays as it is;
    /// from then on its duplicate and @p simulation share the flight data.
    bool adoptInto(Simulation& simulation) const;

private:
    std::unique_ptr<Simulation> m_duplicate;
    ModId                       m_modIdAtDuplication;
    bool                        m_refused;
    /// Whether the duplicate recorded its run (Simulation::simulate()'s bookkeeping).
    bool                       m_recorded{false};
    Outcome                    m_outcome{Outcome::NOT_RUN};
    std::optional<Error>       m_error;
    std::optional<std::string> m_calculationBranchName;
    std::exception_ptr         m_exception;
};

/// The constant's name, e.g. "CANCELLED".
[[nodiscard]] std::string_view name(SimulationRun::Outcome outcome) noexcept;

}  // namespace QtRocket
