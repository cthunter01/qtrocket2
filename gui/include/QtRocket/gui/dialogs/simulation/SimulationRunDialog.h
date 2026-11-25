#pragma once

#include <QDialog>
#include <QObject>

#include "QtRocket/gui/util/SimulationBatch.h"
#include "QtRocket/gui/util/UserPrompts.h"

class QLabel;
class QProgressBar;
class QPushButton;
class QWidget;

namespace QtRocket::Gui
{

/// OpenRocket's SimulationRunDialog (swing gui/simulation/SimulationRunDialog.java:91-262):
/// "Running simulations...", the dialog that runs a list of simulations of a document
/// (UserPrompts::runSimulations(); the Simulations tab's Run, the settings dialog's OK) and
/// shows their progress, modal to the application. It is the view of a SimulationBatch, which it
/// owns and starts as it is made (Java's constructor: the seeds, the runs, their submission);
/// from the top ("fill", "[][grow]"):
/// - "Running <name>" ("simLabel", over both columns): the first simulation whose run has not
///   ended (Java's literal "Running " and the simulation's name; "Running...",
///   SimuRunDlg.lbl.Running, until the first update, which the constructor makes at once);
/// - a paragraph gap lower, three rows of a label and its value: "Simulation time: "
///   (SimuRunDlg.lbl.Simutime, a paragraph gap right of it) and "timeLabel" (at least 200 pixels
///   wide), "Altitude: " (SimuRunDlg.lbl.Altitude) and "altLabel", "Velocity: "
///   (SimuRunDlg.lbl.Velocity) and "velLabel": that simulation's last sample, as its progress
///   estimator words it in the default units (the time; the altitude and the maximum altitude;
///   the vertical velocity and its maximum: SimulationProgressEstimator::getTimeText(), ...), or
///   empty before its first sample;
/// - a paragraph gap lower, the progress bar ("progressBar", 0 to 100, without a text, over both
///   columns): the integer mean of the runs' progress (SimulationBatch::progress());
/// - a paragraph gap lower, Cancel ("cancel", dlg.but.cancel) at the right.
/// At least 300 pixels wide. The values change as the batch's news comes (its progressed()).
///
/// Cancel, Escape and the window's close button cancel the runs (Java's cancelSimulations(), the
/// close button's windowClosing()): the queued ones never start, the running ones stop after
/// their step and keep their simulations' old results (OpenRocket bug 40); the dialog stays open
/// until every run has ended (the batch's finished(): Java's dialog counts its cancelled workers
/// before it closes), then closes. It closes the same way when the runs have all ended. The
/// failures come one at a time, as messages over this dialog, through the prompts it is given
/// (SimulationBatch).
///
/// Deviations from OpenRocket: Java's dialog closes as soon as Cancel has cancelled its workers
/// (SwingWorker.done() comes at once), while their threads go on to their next step; here it
/// closes once the runs have noticed the stop (the step a run is taking), so that no run of the
/// batch outlives it.
class SimulationRunDialog final : public QDialog
{
    Q_OBJECT

public:
    /// MigLayout's paragraph gap ("wrap para", "gapright para").
    static constexpr int kParagraphGap = 12;
    /// The least width of the values ("wmin 200lp").
    static constexpr int kValueWidth = 200;
    /// The dialog's least width (setMinimumSize(new Dimension(300, 0))).
    static constexpr int kMinimumWidth = 300;

    /// The dialog of the request @p request, whose batch (with @p options) it starts now: not
    /// shown yet. Its failures are shown through @p prompts, over the dialog. The request's
    /// document, executor and prompts must outlive the dialog.
    /// @throws BugError when the batch cannot start (SimulationBatch::start(): no simulation, no
    ///         document or executor, a run that cannot be prepared)
    SimulationRunDialog(UserPrompts& prompts, const UserPrompts::RunSimulationsRequest& request,
                        const SimulationBatch::Options& options = {}, QWidget* parent = nullptr);
    ~SimulationRunDialog() override;
    SimulationRunDialog(const SimulationRunDialog&)            = delete;
    SimulationRunDialog& operator=(const SimulationRunDialog&) = delete;
    SimulationRunDialog(SimulationRunDialog&&)                 = delete;
    SimulationRunDialog& operator=(SimulationRunDialog&&)      = delete;

    /// Runs the simulations of @p request in a dialog made for the call, parented to @p parent (or
    /// none), modal (ModalDialog): returns when every run has ended, how the batch ended
    /// (SimulationBatch::answer()), or a cancelled batch when the dialog was destroyed with its
    /// parent meanwhile (its runs are abandoned then). Java's runSimulations().
    /// @throws BugError as the constructor, and when it is called from inside a callback of the
    ///         request's executor (SimulationExecutor::isHandingOver(): no news of the runs would
    ///         come while the dialog waits, so it would wait for good)
    [[nodiscard]] static UserPrompts::RunSimulationsAnswer run(
        QWidget* parent, UserPrompts& prompts, const UserPrompts::RunSimulationsRequest& request,
        const SimulationBatch::Options& options = {});

    /// The batch the dialog shows.
    [[nodiscard]] SimulationBatch&       batch() noexcept { return *m_batch; }
    [[nodiscard]] const SimulationBatch& batch() const noexcept { return *m_batch; }

    /// Cancels the runs (Java's public cancelSimulations()): what Cancel does.
    void cancelSimulations();

    /// Escape and the close button: cancelSimulations(); the dialog closes once the runs have
    /// ended (see the class comment).
    void reject() override;

private:
    /// Shows the batch's progress (Java's updateProgress()); closes the dialog once every run has
    /// ended.
    void updateProgress();

    SimulationBatch* m_batch;
    QLabel*          m_simLabel;
    QLabel*          m_timeLabel;
    QLabel*          m_altLabel;
    QLabel*          m_velLabel;
    QProgressBar*    m_progressBar;
    QPushButton*     m_cancel;
};

}  // namespace QtRocket::Gui
