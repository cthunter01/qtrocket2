#pragma once

#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <vector>

#include <QObject>
#include <QPointer>
#include <QString>
#include <QWidget>

#include "QtRocket/gui/util/SimulationExecutor.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/simulation/RunSample.h"
#include "QtRocket/simulation/SimulationBatchCompletion.h"
#include "QtRocket/simulation/SimulationProgressEstimator.h"
#include "QtRocket/simulation/listeners/RunSampleListener.h"

namespace QtRocket
{
class OpenRocketDocument;
class Simulation;
class SimulationListener;
class SimulationRun;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// The runs of a list of simulations of a document, as OpenRocket's run dialog makes them
/// (swing gui/simulation/SimulationRunDialog.java: the constructor :109-138, the workers'
/// progress :366-422, their ends :427-480, markSimulationComplete() :273-285 and
/// isAllSimulationsSuccessful() :291-297), without the dialog: the run dialog
/// (dialogs/simulation/SimulationRunDialog) shows a batch, and the tests' scripted run dialog
/// (ScriptedUserPrompts) runs one without a window. Everything happens on the GUI thread; the
/// runs run where the request's executor runs them (SimulationExecutor).
///
/// start(), once (Java's constructor):
/// 1. the seed of every simulation is drawn, unless the simulation fixes it
///    (SimulationOptions::randomizeSeedIfNotFixed(), on the document's simulation, before any run
///    is made, as Java draws them first);
/// 2. the request's cancelBackgroundRuns is called with the simulations (the design view's
///    estimate of them is cancelled: the critic's G4);
/// 3. each simulation gets its run (a SimulationRun: a duplicate, prepared now, with the
///    modification id its configuration has now), a progress estimator of the duplicate's flight
///    configuration (SimulationProgressEstimator: Java's burn-out estimate) and a sample listener
///    (RunSampleListener: Java's SimulationProgressListener), then the extra listeners of the
///    options (M7: the custom expressions);
/// 4. every run is submitted to the executor, as a job of the request's window, in the order of
///    the simulations (Java executes the workers in that order).
/// Then, as the executor hands over the news of a job (Java's process(chunks) and done()):
/// - the samples go to the simulation's estimator, and progressed() is emitted (Java's
///   updateProgress());
/// - when a job ends, its estimator finishes (100 %), and its run is adopted by its simulation
///   (SimulationRun::adoptInto(), which isAdoptable() decides: never a run that was stopped,
///   OpenRocket bug 40) as long as the simulation is still in the document (Java's worker
///   records into the document's simulation in simulate()'s finally block); then a failure is
///   shown (Java's simulationInterrupted()): nothing for a CANCELLED run (Java ignores
///   cancellations); for a FAILED one, through UserPrompts::showMessage(), an error titled with
///   the simulation's name and the lines "An error occurred during the simulation:"
///   (SimuRunDlg.msg.errorOccurred), or "An error occurred during simulation branch" and the
///   branch's quoted name (SimuRunDlg.msg.branchErrorOccurred) when the run names the branch
///   whose calculation went wrong, then the error's message (Java's DetailDialog; QtRocket's own
///   refusals, an input out of range or an atmosphere that refuses the launch conditions, are
///   shown the same way, O-M4-15); for an exception, a report through Guard with the context
///   "An exception occurred during the simulation" (Java's handleErrorCondition()). The executor
///   pauses its poll while the message is open, so the failures come one at a time. Then the
///   simulation is marked done (SimulationBatchCompletion), progressed() is emitted, and when
///   that completed the batch, the document is told once (a SIMULATION change event whose source
///   is that simulation: Java fires one SimulationChangeEvent per batch, so that the table sorts
///   once), and finished() is emitted.
/// cancel() (Java's cancelSimulations(), the dialog's Cancel) cancels every job that has not
/// ended: a queued one never runs, a running one stops after its step (SimulationExecutor::
/// cancel()); their ends come as above, the simulations keep their old results (bug 40), and
/// finished() comes when the last one has ended (Java's dialog counts the cancelled workers
/// before it closes).
///
/// The batch's callbacks capture it: it abandons its jobs when it is destroyed
/// (SimulationExecutor::abandon()), so none comes after; it may be destroyed while a failure's
/// message is open (its dialog goes with the window), and then does nothing more once the message
/// has returned. It holds the simulations by shared pointers (the request's), never the document's
/// list: whether a simulation is still in the document is asked when its run ends
/// (OpenRocketDocument::getSimulationIndex()). The document and the executor must outlive the
/// batch, and the prompts too.
///
/// Deviations from OpenRocket:
/// - Java's worker runs the document's own simulation on its thread; here a duplicate runs and
///   the result is adopted on the GUI thread as the run ends (C1's SimulationRun).
/// - A cancelled run keeps the simulation's old results and status (bug 40), where Java's
///   simulate() records the partial flight as up to date.
/// - isAllSuccessful() is false for a cancelled batch, also for a simulation whose queued job was
///   purged (Java's purged worker never runs, keeps no throwable, and counts as successful).
class SimulationBatch final : public QObject
{
    Q_OBJECT

public:
    /// The extra listeners of the run of the simulation @p index, after its sample listener
    /// (Java: the worker's CustomExpressionSimulationListener, which Milestone 7 adds; a test's
    /// holds and failures), which @p stopToken, the token of the job's stop source, stops.
    using ExtraListeners = std::function<std::vector<std::shared_ptr<SimulationListener>>(
        std::size_t index, const std::stop_token& stopToken)>;

    /// What the batch's runs are made with beside the request.
    struct Options
    {
        /// The extra listeners of each run; none when empty.
        ExtraListeners extraListeners;
        /// The clock of the runs' sample listeners (RunSampleListener::Clock: a test's, so that
        /// which steps publish a sample does not depend on the time a run takes); the steady
        /// clock when empty.
        RunSampleListener::Clock sampleClock;
    };

    /// A batch of the request @p request, whose failures are shown through @p prompts (both are
    /// read now: the request's simulations are kept, their names taken), with the default
    /// options. Runs nothing yet.
    SimulationBatch(UserPrompts& prompts, const UserPrompts::RunSimulationsRequest& request,
                    QObject* parent = nullptr);

    /// A batch as above, made with @p options.
    SimulationBatch(UserPrompts& prompts, const UserPrompts::RunSimulationsRequest& request,
                    Options options, QObject* parent = nullptr);

    /// Abandons the jobs that have not ended (see the class comment).
    ~SimulationBatch() override;

    SimulationBatch(const SimulationBatch&)            = delete;
    SimulationBatch& operator=(const SimulationBatch&) = delete;
    SimulationBatch(SimulationBatch&&)                 = delete;
    SimulationBatch& operator=(SimulationBatch&&)      = delete;

    /// The parent of the failures' messages (the run dialog; Java's DetailDialog has the run
    /// dialog as its parent); none by default. Kept as a QPointer.
    void setMessageParent(QWidget* parent) noexcept { m_messageParent = parent; }

    /// Starts the runs (see the class comment). Once.
    /// @throws BugError when the batch was started before, when the request has no document, no
    ///         executor or no simulations, or a null simulation, or when a run cannot be prepared
    ///         (SimulationRun, SimulationProgressEstimator); nothing is submitted then
    void start();

    /// Cancels the runs that have not ended (see the class comment); nothing once the batch is
    /// finished. Before start() the batch is cancelled and finished at once (without finished()).
    void cancel();

    /// Whether start() was called.
    [[nodiscard]] bool isStarted() const noexcept { return m_started; }
    /// Whether every run has ended (or the batch was cancelled before it started).
    [[nodiscard]] bool isFinished() const noexcept { return m_finished; }
    /// Whether cancel() was called before the batch finished.
    [[nodiscard]] bool isCancelled() const noexcept { return m_cancelled; }
    /// Whether the batch is finished, was not cancelled and every run SUCCEEDED (an aborted
    /// flight included): Java's isAllSimulationsSuccessful() of a dialog that was not cancelled.
    [[nodiscard]] bool isAllSuccessful() const noexcept;
    /// How the run dialog of this batch ends: {isCancelled(), isAllSuccessful()}.
    [[nodiscard]] UserPrompts::RunSimulationsAnswer answer() const noexcept
    {
        return {.cancelled = m_cancelled, .allSuccessful = isAllSuccessful()};
    }

    /// The number of simulations.
    [[nodiscard]] std::size_t size() const noexcept { return m_entries.size(); }
    /// The name simulation @p index had when the batch was made (Java: simulationNames).
    /// @throws BugError when @p index is not below size()
    [[nodiscard]] const QString& simulationName(std::size_t index) const;
    /// The progress estimator of the run of simulation @p index; null before start().
    /// @throws BugError when @p index is not below size()
    [[nodiscard]] const SimulationProgressEstimator* estimator(std::size_t index) const;
    /// The run of simulation @p index (null before start()); its outcome is read once its job
    /// has ended (isSimulationDone()).
    /// @throws BugError when @p index is not below size()
    [[nodiscard]] std::shared_ptr<const SimulationRun> run(std::size_t index) const;
    /// Whether the job of simulation @p index has ended.
    /// @throws BugError when @p index is not below size()
    [[nodiscard]] bool isSimulationDone(std::size_t index) const;
    /// The simulation whose values the run dialog shows: the first one whose job has not ended
    /// (Java's updateProgress()); nullopt when every one has.
    [[nodiscard]] std::optional<std::size_t> shownSimulation() const noexcept
    {
        return m_completion.getFirstIncomplete();
    }
    /// The progress of the batch in percent, 0 ... 100: the integer mean of the estimators'
    /// progress (Java: the sum of the workers' progress divided by their number); 0 before
    /// start().
    [[nodiscard]] int progress() const noexcept;

Q_SIGNALS:
    /// Something the run dialog shows changed: samples came, or a run ended.
    void progressed();
    /// Every run has ended (emitted once, after the document was told).
    void finished();

private:
    /// A simulation of the batch and its run.
    struct Entry
    {
        std::shared_ptr<Simulation>                simulation;
        QString                                    name;
        std::shared_ptr<SimulationRun>             run;
        std::shared_ptr<RunSampleListener>         sampler;
        std::optional<SimulationProgressEstimator> estimator;
        /// The job, until it has ended.
        SimulationExecutor::JobId job{SimulationExecutor::kNoJob};
    };

    /// The entry @p index; a BugError out of range.
    [[nodiscard]] const Entry& entry(std::size_t index) const;
    /// Prepares the run of every simulation (step 3 of the class comment).
    void prepareRuns();
    /// Submits the run of every simulation (step 4).
    void submitRuns();
    /// The executor's progressed() of the job of simulation @p index.
    void runProgressed(std::size_t index, std::span<const RunSample> samples);
    /// The executor's finished() of the job of simulation @p index.
    void runFinished(std::size_t index, const std::exception_ptr& exception);
    /// Adopts the run of @p entry, which has ended, while its simulation is in the document.
    void adopt(const Entry& entry) const;
    /// Shows the failure of the run of @p entry, which ended with @p exception (may run an event
    /// loop).
    void showFailure(const Entry& entry, const std::exception_ptr& exception) const;
    /// Tells the document that the batch, ended by the run of @p entry, is done, then emits
    /// finished().
    void finish(const Entry& entry);

    UserPrompts*                                                      m_prompts;
    const QObject*                                                    m_window;
    OpenRocketDocument*                                               m_document;
    SimulationExecutor*                                               m_executor;
    std::function<void(std::span<const std::shared_ptr<Simulation>>)> m_cancelBackgroundRuns;
    std::vector<std::shared_ptr<Simulation>>                          m_simulations;
    Options                                                           m_options;
    std::vector<Entry>                                                m_entries;
    SimulationBatchCompletion                                         m_completion;
    QPointer<QWidget>                                                 m_messageParent;
    bool                                                              m_started{false};
    bool                                                              m_finished{false};
    bool                                                              m_cancelled{false};
};

}  // namespace QtRocket::Gui
