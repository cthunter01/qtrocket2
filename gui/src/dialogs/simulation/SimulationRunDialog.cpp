#include "QtRocket/gui/dialogs/simulation/SimulationRunDialog.h"

#include <cstddef>
#include <optional>

#include <QDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLatin1Char>
#include <QObject>
#include <QProgressBar>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/dialogs/ModalDialog.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/SimulationBatch.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/simulation/SimulationProgressEstimator.h"

namespace QtRocket::Gui
{

namespace
{

/// The progress bar's largest value (JProgressBar's default range, 0 to 100).
constexpr int kProgressMaximum = 100;

/// A label of a row: @p text and a space, as Java's trans.get(...) + " ".
[[nodiscard]] QString rowLabel(const QString& text)
{
    return text + QLatin1Char(' ');
}

}  // namespace

SimulationRunDialog::SimulationRunDialog(UserPrompts&                              prompts,
                                         const UserPrompts::RunSimulationsRequest& request,
                                         const SimulationBatch::Options& options, QWidget* parent)
  : QDialog(parent),
    m_batch(makeChild<SimulationBatch>(this, prompts, request, options)),
    //: SimuRunDlg.lbl.Running
    m_simLabel(makeChild<QLabel>(this, tr("Running\u2026"))),
    m_timeLabel(makeChild<QLabel>(this, QString())),
    m_altLabel(makeChild<QLabel>(this, QString())),
    m_velLabel(makeChild<QLabel>(this, QString())),
    m_progressBar(makeChild<QProgressBar>(this)),
    //: dlg.but.cancel
    m_cancel(makeChild<QPushButton>(this, tr("Cancel")))
{
    setObjectName(QStringLiteral("SimulationRunDialog"));
    //: SimuRunDlg.title.RunSim
    setWindowTitle(tr("Running simulations\u2026"));
    // Dialog.ModalityType.APPLICATION_MODAL.
    setWindowModality(Qt::ApplicationModal);
    setMinimumWidth(kMinimumWidth);

    auto* layout = makeChild<QVBoxLayout>(this);
    // "spanx, wrap para".
    m_simLabel->setObjectName(QStringLiteral("simLabel"));
    layout->addWidget(m_simLabel);
    layout->addSpacing(kParagraphGap);

    // The three rows ("[][grow]": the values take the width), the first label "gapright para",
    // the rows "wrap rel" apart, the last "wrap para".
    auto* rows       = makeChild<QWidget>(this);
    auto* rowsLayout = makeChild<QGridLayout>(rows);
    rowsLayout->setContentsMargins(0, 0, 0, 0);
    rowsLayout->setHorizontalSpacing(kParagraphGap);
    rowsLayout->setColumnStretch(1, 1);
    m_timeLabel->setObjectName(QStringLiteral("timeLabel"));
    m_timeLabel->setMinimumWidth(kValueWidth);
    m_altLabel->setObjectName(QStringLiteral("altLabel"));
    m_velLabel->setObjectName(QStringLiteral("velLabel"));
    const auto addRow = [rows, rowsLayout](int row, const QString& text, QLabel* value) {
        auto* label = makeChild<QLabel>(rows, rowLabel(text));
        rowsLayout->addWidget(label, row, 0);
        value->setParent(rows);
        rowsLayout->addWidget(value, row, 1);
    };
    //: SimuRunDlg.lbl.Simutime
    addRow(0, tr("Simulation time:"), m_timeLabel);
    //: SimuRunDlg.lbl.Altitude
    addRow(1, tr("Altitude:"), m_altLabel);
    //: SimuRunDlg.lbl.Velocity
    addRow(2, tr("Velocity:"), m_velLabel);
    layout->addWidget(rows);
    layout->addSpacing(kParagraphGap);

    // "spanx, growx, wrap para": a bar without a text (JProgressBar paints none by default).
    m_progressBar->setObjectName(QStringLiteral("progressBar"));
    m_progressBar->setRange(0, kProgressMaximum);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    layout->addWidget(m_progressBar);
    layout->addSpacing(kParagraphGap);

    // "spanx, tag cancel": the button bar's Cancel, at the right. No default button
    // (GUIUtil.setDisposableDialogOptions(this, null)): Return does not cancel.
    m_cancel->setObjectName(QStringLiteral("cancel"));
    m_cancel->setAutoDefault(false);
    auto* buttons       = makeChild<QWidget>(this);
    auto* buttonsLayout = makeChild<QHBoxLayout>(buttons);
    buttonsLayout->setContentsMargins(0, 0, 0, 0);
    buttonsLayout->addStretch(1);
    m_cancel->setParent(buttons);
    buttonsLayout->addWidget(m_cancel);
    layout->addWidget(buttons);

    connect(m_cancel, &QPushButton::clicked, this, guarded([this] { cancelSimulations(); }));
    connect(m_batch, &SimulationBatch::progressed, this, guarded([this] { updateProgress(); }));
    connect(m_batch, &SimulationBatch::finished, this, guarded([this] { updateProgress(); }));

    // Java's constructor: the runs are submitted before the dialog is shown (no news comes
    // before the event loop), and the labels show the first simulation at once.
    m_batch->setMessageParent(this);
    m_batch->start();
    updateProgress();
}

SimulationRunDialog::~SimulationRunDialog() = default;

UserPrompts::RunSimulationsAnswer SimulationRunDialog::run(
    QWidget* parent, UserPrompts& prompts, const UserPrompts::RunSimulationsRequest& request,
    const SimulationBatch::Options& options)
{
    ModalDialog<SimulationRunDialog> dialog(parent, prompts, request, options);
    if (const SimulationRunDialog* const made = dialog.get();
        made != nullptr && !made->batch().isFinished())
    {
        dialog.exec();
    }
    const SimulationRunDialog* const ended = dialog.get();
    if (ended == nullptr)
    {
        // Gone with its parent: its runs were abandoned with its batch.
        return {.cancelled = true, .allSuccessful = false};
    }
    return ended->batch().answer();
}

void SimulationRunDialog::cancelSimulations()
{
    m_batch->cancel();
}

void SimulationRunDialog::reject()
{
    // Escape and the close button (Java's windowClosing()): the runs are cancelled, and the
    // dialog closes when they have ended (updateProgress()); a dialog whose runs have all ended
    // closes now.
    if (m_batch->isFinished())
    {
        QDialog::reject();
        return;
    }
    cancelSimulations();
}

void SimulationRunDialog::updateProgress()
{
    const std::optional<std::size_t> shown = m_batch->shownSimulation();
    if (!shown.has_value() || m_batch->isFinished())
    {
        // "Everything is done, close the dialog".
        done(QDialog::Accepted);
        return;
    }
    m_progressBar->setValue(m_batch->progress());
    //: Not translated in OpenRocket (SimulationRunDialog.java): "Running " + the simulation's name
    m_simLabel->setText(tr("Running %1").arg(m_batch->simulationName(*shown)));
    const SimulationProgressEstimator* const estimator = m_batch->estimator(*shown);
    if (estimator == nullptr || !estimator->getLastSample().has_value())
    {
        m_timeLabel->setText(QString());
        m_altLabel->setText(QString());
        m_velLabel->setText(QString());
        return;
    }
    m_timeLabel->setText(QtStrings::fromUtf8(estimator->getTimeText()));
    m_altLabel->setText(QtStrings::fromUtf8(estimator->getAltitudeText()));
    m_velLabel->setText(QtStrings::fromUtf8(estimator->getVelocityText()));
}

}  // namespace QtRocket::Gui
