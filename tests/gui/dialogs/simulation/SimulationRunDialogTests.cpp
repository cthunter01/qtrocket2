#include "QtRocket/gui/dialogs/simulation/SimulationRunDialog.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <exception>
#include <memory>
#include <stop_token>
#include <string>
#include <vector>

#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLayout>
#include <QObject>
#include <QPoint>
#include <QProgressBar>
#include <QPushButton>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>
#include <QtCore/qtestsupport_core.h>

#include <gtest/gtest.h>

#include "ForwardingUserPrompts.h"
#include "GuiTestSupport.h"
#include "ManualSimulationExecutor.h"
#include "NextModalDialog.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/dialogs/DetailDialog.h"
#include "QtRocket/gui/main/WidgetUserPrompts.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/PollingSimulationExecutor.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/SimulationBatch.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationRun.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/extension/impl/ScriptingExtension.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/BugError.h"
#include "RecordingGuardHook.h"
#include "ScriptedUserPrompts.h"
#include "SimulationRunTestSupport.h"
#include "TestGuiServices.h"
#include "simulation/SimulationRunnerSupport.h"

// OpenRocket's SimulationRunDialog (swing gui/simulation/SimulationRunDialog.java): its texts and
// layout, its progress, Cancel, Escape and the close button, the failures over it, the runs it
// hands back (bug 40 for a cancelled one), and WidgetUserPrompts::runSimulations(), which shows
// it. With the tests' executor, and once on real threads (the application's executor, two runs at
// once, a cancel through the dialog: plan 11.6 E2), which carries ctest's TIMEOUT
// (SimulationRunDialogThreadsTest, tests/gui/CMakeLists.txt) and runs under the tsan preset at a
// merge.

namespace QtRocket::Gui::Test
{

namespace
{

using SimulationRunDialogTest        = GuiTest;
using SimulationRunDialogThreadsTest = GuiTest;
using Mode                           = ManualSimulationExecutor::Mode;
using Answer                         = UserPrompts::RunSimulationsAnswer;
using QtRocket::Test::StepGate;
using QtRocket::Test::StepMeeting;

/// Two runs of the short flight of "A simple model rocket" #1: the design's simulation and a
/// copy of it ("Second").
class TwoSimpleRuns
{
public:
    TwoSimpleRuns()
      : m_design(ExampleDesign::kSimpleModelRocket),
        m_first(m_design.simulation(0)),
        m_second(m_design.addCopyOf(0, "Second"))
    {
    }

    [[nodiscard]] OpenRocketDocument& document() const noexcept { return m_design.document(); }
    [[nodiscard]] const std::shared_ptr<Simulation>& first() const noexcept { return m_first; }
    [[nodiscard]] const std::shared_ptr<Simulation>& second() const noexcept { return m_second; }

    /// The request to run both through @p executor.
    [[nodiscard]] UserPrompts::RunSimulationsRequest request(SimulationExecutor& executor) const
    {
        return runRequestOf(document(), {m_first, m_second}, executor);
    }

private:
    ExampleDesign               m_design;
    std::shared_ptr<Simulation> m_first;
    std::shared_ptr<Simulation> m_second;
};

/// The label of @p dialog named @p name, or null (a test failure).
[[nodiscard]] const QLabel* labelOf(const QWidget& dialog, const char* name)
{
    const auto* const label = dialog.findChild<QLabel*>(QString::fromLatin1(name));
    if (label == nullptr)
    {
        ADD_FAILURE() << "the dialog has no label " << name;
    }
    return label;
}

/// The text of the label of @p dialog named @p name ("" without one: a test failure).
[[nodiscard]] QString labelText(const QWidget& dialog, const char* name)
{
    const QLabel* const label = labelOf(dialog, name);
    return label == nullptr ? QString() : label->text();
}

/// The progress bar of @p dialog.
[[nodiscard]] const QProgressBar* progressBarOf(const QWidget& dialog)
{
    return dialog.findChild<QProgressBar*>(QStringLiteral("progressBar"));
}

/// The Cancel button of @p dialog.
[[nodiscard]] QPushButton* cancelOf(const QWidget& dialog)
{
    return dialog.findChild<QPushButton*>(QStringLiteral("cancel"));
}

/// Lays out @p dialog, which is not shown, and the widgets in it (each layout of a widget in it
/// is activated after the one around it).
void layOut(const QWidget& dialog)
{
    dialog.layout()->activate();
    for (QLayout* const layout : dialog.findChildren<QLayout*>())
    {
        layout->activate();
    }
}

/// The geometry of @p widget in the coordinates of @p dialog.
[[nodiscard]] QRect geometryIn(const QWidget& widget, const QWidget& dialog)
{
    return {widget.mapTo(&dialog, QPoint(0, 0)), widget.size()};
}

/// The label of @p dialog whose text is @p text (the rows' names).
[[nodiscard]] const QLabel* labelWithText(const QWidget& dialog, const QString& text)
{
    for (const QLabel* const label : dialog.findChildren<QLabel*>())
    {
        if (label->text() == text)
        {
            return label;
        }
    }
    return nullptr;
}

/// The texts the values of a dialog showed at one moment.
struct ShownValues
{
    QString simulation;
    QString time;
    QString altitude;
    QString velocity;
    int     progress = -1;
};

/// What a listener of a run does as it runs: it hands the samples so far over (as the
/// application's executor does at a poll) and notes what the dialog then shows.
struct HandOverAndReadTheDialog
{
    ManualSimulationExecutor*         executor;
    const SimulationRunDialog* const* dialog;
    ShownValues*                      shown;

    void operator()() const
    {
        static_cast<void>(executor->reportProgress(executor->runningJob()));
        shown->simulation = labelText(**dialog, "simLabel");
        shown->time       = labelText(**dialog, "timeLabel");
        shown->altitude   = labelText(**dialog, "altLabel");
        shown->velocity   = labelText(**dialog, "velLabel");
        shown->progress   = progressBarOf(**dialog)->value();
    }
};

/// What a listener of a run does as it runs: it clicks Cancel of the run dialog that is open (the
/// application-modal one).
struct ClickCancelOfTheOpenDialog
{
    bool* clicked;

    void operator()() const
    {
        const auto* const dialog =
            qobject_cast<const SimulationRunDialog*>(QApplication::activeModalWidget());
        if (dialog == nullptr)
        {
            ADD_FAILURE() << "no run dialog is open";
            return;
        }
        QTest::mouseClick(cancelOf(*dialog), Qt::LeftButton);
        *clicked = true;
    }
};

/// The batch's extra listeners: one made by @p make for the run of the simulation @p index.
template <class Action>
struct ListenerAtStep
{
    std::size_t index;
    int         step;
    Action      action;

    [[nodiscard]] std::vector<std::shared_ptr<SimulationListener>> operator()(
        std::size_t run, const std::stop_token& /*stop*/) const
    {
        if (run != index)
        {
            return {};
        }
        return {actionAtStep(step, action)};
    }
};

/// The batch's extra listeners: a failing one for each run, as @p failures says.
struct FailingListeners
{
    std::vector<RunFailure> failures;

    [[nodiscard]] std::vector<std::shared_ptr<SimulationListener>> operator()(
        std::size_t run, const std::stop_token& /*stop*/) const
    {
        return {failingRunListener(failures.at(run))};
    }
};

/// How a NextModalDialog closes the run dialog.
enum class Closing
{
    CANCEL_BUTTON,
    ESCAPE,
    CLOSE_BUTTON,
};

/// What a NextModalDialog does with the run dialog: it closes it as @p closing says, and notes
/// whether the dialog was still open right after (the cancelled runs end in a later turn).
struct CloseTheRunDialog
{
    Closing closing;
    bool*   openAfter;

    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<SimulationRunDialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        switch (closing)
        {
            case Closing::CANCEL_BUTTON:
                QTest::mouseClick(cancelOf(*dialog), Qt::LeftButton);
                break;
            case Closing::ESCAPE:
                QTest::keyClick(dialog, Qt::Key_Escape);
                break;
            case Closing::CLOSE_BUTTON:
                dialog->close();
                break;
        }
        *openAfter = dialog->isVisible();
    }
};

/// What a NextModalDialog does with a failure's message: it notes its parent, title and lines,
/// and closes it.
struct ReadTheMessage
{
    const QWidget** parent;
    QString*        title;
    QStringList*    lines;

    void operator()(QWidget& widget) const
    {
        auto* const message = qobject_cast<DetailDialog*>(&widget);
        ASSERT_NE(message, nullptr);
        *parent = message->parentWidget();
        *title  = message->windowTitle();
        *lines  = message->messageLines();
        message->buttonBox()->button(QDialogButtonBox::Ok)->click();
    }
};

/// What a NextModalDialog does with the run dialog: it destroys its parent (the window goes while
/// the runs go on).
struct DestroyTheParent
{
    std::unique_ptr<QWidget>* parent;

    void operator()(QWidget& /*dialog*/) const { parent->reset(); }
};

/// The prompts of a test that notes the parent of every message, and shows it through the
/// scripted ones.
class ParentNotingPrompts final : public ForwardingUserPrompts
{
public:
    using ForwardingUserPrompts::ForwardingUserPrompts;

    void showMessage(QWidget* parent, const Message& message) override
    {
        m_parents.push_back(parent);
        ForwardingUserPrompts::showMessage(parent, message);
    }

    [[nodiscard]] const std::vector<const QWidget*>& parents() const noexcept { return m_parents; }

private:
    std::vector<const QWidget*> m_parents;
};

/// Runs @p request in the run dialog (SimulationRunDialog::run(), without a parent) and closes
/// it as @p closing says once it is open; notes whether it stayed open after the close.
[[nodiscard]] Answer runAndClose(UserPrompts&                              prompts,
                                 const UserPrompts::RunSimulationsRequest& request, Closing closing,
                                 bool& openAfter)
{
    const NextModalDialog close(CloseTheRunDialog{.closing = closing, .openAfter = &openAfter});
    return SimulationRunDialog::run(nullptr, prompts, request);
}

TEST_F(SimulationRunDialogTest, ShowsJavasTextsAndLayout)
{
    TestGuiServices services;
    services.executor().setMode(Mode::STEPPED);
    const TwoSimpleRuns runs;
    {
        const SimulationRunDialog dialog(services.prompts(), runs.request(services.executor()));
        EXPECT_EQ(dialog.objectName(), QStringLiteral("SimulationRunDialog"));
        EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Running simulations\u2026"));
        EXPECT_EQ(dialog.windowModality(), Qt::ApplicationModal);
        EXPECT_EQ(dialog.minimumWidth(), SimulationRunDialog::kMinimumWidth);
        // The constructor's update: the first simulation, without values yet.
        EXPECT_EQ(labelText(dialog, "simLabel"),
                  QStringLiteral("Running ") + QtStrings::fromUtf8(runs.first()->getName()));
        EXPECT_EQ(labelText(dialog, "timeLabel"), QString());
        EXPECT_EQ(labelText(dialog, "altLabel"), QString());
        EXPECT_EQ(labelText(dialog, "velLabel"), QString());
        const QProgressBar* const bar = progressBarOf(dialog);
        ASSERT_NE(bar, nullptr);
        EXPECT_EQ(bar->minimum(), 0);
        EXPECT_EQ(bar->maximum(), 100);
        EXPECT_EQ(bar->value(), 0);
        EXPECT_FALSE(bar->isTextVisible());
        const QPushButton* const cancel = cancelOf(dialog);
        ASSERT_NE(cancel, nullptr);
        EXPECT_EQ(cancel->text(), QStringLiteral("Cancel"));
        EXPECT_FALSE(cancel->autoDefault());
        EXPECT_FALSE(cancel->isDefault());
        const QLabel* const timeLabel = labelOf(dialog, "timeLabel");
        const QLabel* const simLabel  = labelOf(dialog, "simLabel");
        const QLabel* const velLabel  = labelOf(dialog, "velLabel");
        ASSERT_NE(timeLabel, nullptr);
        ASSERT_NE(simLabel, nullptr);
        ASSERT_NE(velLabel, nullptr);
        EXPECT_EQ(timeLabel->minimumWidth(), SimulationRunDialog::kValueWidth);

        // From the top: the simulation, the three rows (a name and its value at its right), the
        // bar, Cancel at the right.
        layOut(dialog);
        const QLabel* const timeName = labelWithText(dialog, QStringLiteral("Simulation time: "));
        const QLabel* const altName  = labelWithText(dialog, QStringLiteral("Altitude: "));
        const QLabel* const velName  = labelWithText(dialog, QStringLiteral("Velocity: "));
        ASSERT_NE(timeName, nullptr);
        ASSERT_NE(altName, nullptr);
        ASSERT_NE(velName, nullptr);
        const QRect simulation = geometryIn(*simLabel, dialog);
        const QRect time       = geometryIn(*timeName, dialog);
        const QRect timeValue  = geometryIn(*timeLabel, dialog);
        const QRect altitude   = geometryIn(*altName, dialog);
        const QRect velocity   = geometryIn(*velName, dialog);
        const QRect velValue   = geometryIn(*velLabel, dialog);
        const QRect progress   = geometryIn(*bar, dialog);
        const QRect button     = geometryIn(*cancel, dialog);
        EXPECT_LT(simulation.bottom(), time.top());
        EXPECT_LT(time.right(), timeValue.left());
        EXPECT_LT(time.bottom(), altitude.top());
        EXPECT_LT(altitude.bottom(), velocity.top());
        EXPECT_EQ(timeValue.left(), velValue.left());
        EXPECT_LT(velocity.bottom(), progress.top());
        EXPECT_LT(progress.bottom(), button.top());
        EXPECT_GT(button.left(), progress.center().x());
        EXPECT_EQ(button.right(), progress.right());
        EXPECT_EQ(services.executor().submittedJobs().size(), 2U);
    }
    // The dialog went before its runs ended: its batch abandoned them.
    EXPECT_EQ(services.executor().abandonedJobs().size(), 2U);
}

TEST_F(SimulationRunDialogTest, ShowsTheFirstSimulationNotDoneAndTheMeanProgress)
{
    // Java's updateProgress(): the bar is the integer mean of the runs' progress, the labels show
    // the first simulation whose run has not ended; the dialog closes when every run has.
    TestGuiServices           services;
    ManualSimulationExecutor& executor = services.executor();
    executor.setMode(Mode::STEPPED);
    const TwoSimpleRuns runs;
    SimulationRunDialog dialog(services.prompts(), runs.request(executor));
    EXPECT_TRUE(executor.runNext());
    EXPECT_EQ(labelText(dialog, "simLabel"), QStringLiteral("Running Second"));
    EXPECT_EQ(labelText(dialog, "timeLabel"), QString());
    EXPECT_EQ(progressBarOf(dialog)->value(), 50);
    EXPECT_EQ(dialog.result(), QDialog::Rejected);
    EXPECT_TRUE(executor.runNext());
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_TRUE(dialog.batch().isFinished());
    EXPECT_EQ(dialog.batch().answer(), (Answer{.cancelled = false, .allSuccessful = true}));
}

TEST_F(SimulationRunDialogTest, ShowsTheValuesOfTheRunningSimulation)
{
    // Its last sample, in the default units: the time; the altitude and the maximum altitude; the
    // vertical velocity and its maximum (SimulationProgressEstimator's texts).
    TestGuiServices                services;
    ManualSimulationExecutor&      executor = services.executor();
    const ExampleDesign            design(ExampleDesign::kSimpleModelRocket);
    const SimulationRunDialog*     made = nullptr;
    ShownValues                    shown;
    const SimulationBatch::Options options{
        .extraListeners =
            ListenerAtStep<HandOverAndReadTheDialog>{
                .index  = 0,
                .step   = 5,
                .action = {.executor = &executor, .dialog = &made, .shown = &shown}},
        .sampleClock = frozenSampleClock()};
    const SimulationRunDialog dialog(
        services.prompts(), runRequestOf(design.document(), {design.simulation(0)}, executor),
        options);
    made = &dialog;
    EXPECT_EQ(executor.runAll(), 1U);
    EXPECT_EQ(shown.simulation,
              QStringLiteral("Running ") + QtStrings::fromUtf8(design.simulation(0)->getName()));
    EXPECT_TRUE(shown.time.endsWith(QStringLiteral(" s"))) << shown.time.toStdString();
    EXPECT_TRUE(shown.altitude.contains(QStringLiteral(" m (max. ")))
        << shown.altitude.toStdString();
    EXPECT_TRUE(shown.altitude.endsWith(QStringLiteral(" m)"))) << shown.altitude.toStdString();
    EXPECT_TRUE(shown.velocity.contains(QStringLiteral(" m/s (max. ")))
        << shown.velocity.toStdString();
    EXPECT_GE(shown.progress, 0);
    EXPECT_LT(shown.progress, 100);
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
}

/// Runs two simulations in the run dialog over an executor that never runs them, and closes it as
/// @p closing says: the runs are cancelled, the dialog stays open until they have ended, and the
/// simulations keep their old results.
void expectTheCloseCancelsTheRuns(Closing closing)
{
    TestGuiServices services;
    services.executor().setMode(Mode::NEVER);
    const TwoSimpleRuns               runs;
    const std::shared_ptr<FlightData> stored    = runs.first()->getSimulatedData();
    bool                              openAfter = false;
    const Answer                      answer =
        runAndClose(services.prompts(), runs.request(services.executor()), closing, openAfter);
    EXPECT_TRUE(openAfter);
    EXPECT_EQ(answer, (Answer{.cancelled = true, .allSuccessful = false}));
    EXPECT_EQ(services.executor().cancelledJobs().size(), 2U);
    EXPECT_EQ(runs.first()->getSimulatedData(), stored);
}

TEST_F(SimulationRunDialogTest, CancelEscapeAndTheCloseButtonCancelTheRuns)
{
    // Java's cancelSimulations() and windowClosing(): the queued runs never start, and the
    // simulations keep their old results (bug 40); the dialog closes once the runs have ended.
    expectTheCloseCancelsTheRuns(Closing::CANCEL_BUTTON);
    expectTheCloseCancelsTheRuns(Closing::ESCAPE);
    expectTheCloseCancelsTheRuns(Closing::CLOSE_BUTTON);
}

TEST_F(SimulationRunDialogTest, ARunCancelledWhileItGoesKeepsItsSimulationsOldResults)
{
    // OpenRocket bug 40: Java's simulate() records the partial flight of a cancelled run as up to
    // date, and the results the simulation had are lost; here it keeps them, with its status.
    TestGuiServices                   services;
    const ExampleDesign               design(ExampleDesign::kSimpleModelRocket);
    const std::shared_ptr<Simulation> simulation = design.simulation(0);
    ASSERT_NE(simulation, nullptr);
    const std::shared_ptr<FlightData> stored  = simulation->getSimulatedData();
    const Simulation::Status          status  = simulation->getStatus();
    bool                              clicked = false;
    const Answer                      answer =
        SimulationRunDialog::run(nullptr, services.prompts(),
                                 runRequestOf(design.document(), {simulation}, services.executor()),
                                 {.extraListeners =
                                      ListenerAtStep<ClickCancelOfTheOpenDialog>{
                                          .index = 0, .step = 5, .action = {.clicked = &clicked}},
                                  .sampleClock = {}});
    EXPECT_TRUE(clicked);
    EXPECT_EQ(answer, (Answer{.cancelled = true, .allSuccessful = false}));
    EXPECT_EQ(simulation->getSimulatedData(), stored);
    EXPECT_EQ(simulation->getStatus(), status);
    EXPECT_TRUE(services.prompts().prompts().empty());
}

TEST_F(SimulationRunDialogTest, TheFailuresAreShownOverTheDialog)
{
    // Java's DetailDialog has the run dialog as its parent.
    TestGuiServices           services;
    ParentNotingPrompts       prompts(services.prompts());
    ManualSimulationExecutor& executor = services.executor();
    executor.setMode(Mode::STEPPED);
    const TwoSimpleRuns runs;
    SimulationRunDialog dialog(
        prompts, runs.request(executor),
        {.extraListeners = FailingListeners{.failures = {RunFailure::SIMULATION_EXCEPTION,
                                                         RunFailure::SIMULATION_EXCEPTION}},
         .sampleClock    = {}});
    EXPECT_EQ(executor.runAll(), 2U);
    EXPECT_EQ(prompts.parents(), (std::vector<const QWidget*>{&dialog, &dialog}));
    ASSERT_EQ(services.prompts().prompts().size(), 2U);
    EXPECT_EQ(services.prompts().prompts().at(1).title, QStringLiteral("Second"));
    EXPECT_EQ(dialog.batch().answer(), (Answer{.cancelled = false, .allSuccessful = false}));
}

TEST_F(SimulationRunDialogTest, WidgetUserPromptsRunTheSimulationsInTheRunDialog)
{
    // The application's prompts: the dialog runs them, and closes when they have ended.
    TestGuiServices                   services;
    const ExampleDesign               design(ExampleDesign::kSimpleModelRocket);
    const std::shared_ptr<Simulation> simulation = design.simulation(0);
    ASSERT_NE(simulation, nullptr);
    const std::shared_ptr<FlightData> stored = simulation->getSimulatedData();
    WidgetUserPrompts                 prompts;
    EXPECT_EQ(prompts.runSimulations(
                  nullptr, runRequestOf(design.document(), {simulation}, services.executor())),
              (Answer{.cancelled = false, .allSuccessful = true}));
    EXPECT_NE(simulation->getSimulatedData(), stored);
    EXPECT_EQ(simulation->getStatus(), Simulation::Status::UPTODATE);
}

TEST_F(SimulationRunDialogTest, WidgetUserPromptsShowAFailureAsAMessageOverTheRunDialog)
{
    // "Simulation scripting.ork"'s roll script, enabled (the critic's G13): QtRocket's refusal of
    // the script, as Java's error of a simulation, in a DetailDialog over the run dialog.
    TestGuiServices           services;
    ManualSimulationExecutor& executor = services.executor();
    executor.setMode(Mode::STEPPED);
    const ExampleDesign               design(ExampleDesign::kSimulationScripting);
    const std::shared_ptr<Simulation> simulation = design.simulation(1);
    ASSERT_NE(simulation, nullptr);
    ASSERT_FALSE(simulation->getSimulationExtensions().empty());
    const auto script = std::dynamic_pointer_cast<ScriptingExtension>(
        simulation->getSimulationExtensions().front());
    ASSERT_NE(script, nullptr);
    script->setEnabled(true);
    WidgetUserPrompts         prompts;
    const SimulationRunDialog dialog(prompts,
                                     runRequestOf(design.document(), {simulation}, executor));
    const QWidget*            parent = nullptr;
    QString                   title;
    QStringList               lines;
    {
        const NextModalDialog read(
            ReadTheMessage{.parent = &parent, .title = &title, .lines = &lines});
        EXPECT_TRUE(executor.runNext());
    }
    EXPECT_EQ(parent, &dialog);
    EXPECT_EQ(title, QtStrings::fromUtf8(simulation->getName()));
    EXPECT_EQ(lines, (QStringList{QStringLiteral("An error occurred during the simulation:"),
                                  QStringLiteral("QtRocket does not support the scripting "
                                                 "language 'JavaScript'")}));
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
}

TEST_F(SimulationRunDialogTest, TheDialogGoesWithItsParent)
{
    // The window is destroyed while its runs go on: the dialog goes with it, its runs are
    // abandoned, and the batch counts as cancelled.
    TestGuiServices services;
    services.executor().setMode(Mode::NEVER);
    const TwoSimpleRuns               runs;
    const std::shared_ptr<FlightData> stored = runs.first()->getSimulatedData();
    auto                              parent = std::make_unique<QWidget>();
    Answer                            answer{.cancelled = false, .allSuccessful = true};
    {
        const NextModalDialog destroy(DestroyTheParent{.parent = &parent});
        answer = SimulationRunDialog::run(parent.get(), services.prompts(),
                                          runs.request(services.executor()));
    }
    EXPECT_EQ(parent, nullptr);
    EXPECT_EQ(answer, (Answer{.cancelled = true, .allSuccessful = false}));
    EXPECT_EQ(services.executor().abandonedJobs().size(), 2U);
    EXPECT_EQ(runs.first()->getSimulatedData(), stored);
}

/// A job's finished() that runs @p runs in a run dialog, which would wait for their callbacks
/// while this one runs (a BugError).
struct RunInADialog
{
    TestGuiServices*     services;
    const TwoSimpleRuns* runs;

    void operator()(const std::exception_ptr& /*exception*/) const
    {
        static_cast<void>(SimulationRunDialog::run(nullptr, services->prompts(),
                                                   runs->request(services->executor())));
    }
};

TEST_F(SimulationRunDialogTest, ItIsNotOpenedFromInsideACallbackOfTheExecutor)
{
    // No news of the runs would come while the callback runs (SimulationExecutor's pause): the
    // dialog would never close. A BugError, which the executor reports through Guard; nothing
    // runs.
    const RecordingGuardHook          hook;
    TestGuiServices                   services;
    const TwoSimpleRuns               runs;
    const std::shared_ptr<FlightData> stored = runs.first()->getSimulatedData();
    static_cast<void>(services.executor().submit(
        nullptr, std::make_shared<SimulationRun>(*runs.first()), nullptr, {}, std::stop_source{},
        {.progressed = {}, .finished = RunInADialog{.services = &services, .runs = &runs}}));
    QCoreApplication::processEvents();
    ASSERT_EQ(hook.failures().size(), 1U);
    EXPECT_EQ(hook.failures().front().kind, Guard::Failure::Kind::BUG);
    EXPECT_TRUE(hook.failures().front().message.contains(
        QStringLiteral("not opened from inside a callback of the simulation executor")));
    EXPECT_EQ(services.executor().submittedJobs().size(), 1U);
    EXPECT_EQ(runs.first()->getSimulatedData(), stored);
    EXPECT_EQ(QApplication::activeModalWidget(), nullptr);
}

TEST_F(SimulationRunDialogTest, ARequestThatCannotRunIsABugError)
{
    TestGuiServices     services;
    const ExampleDesign design(ExampleDesign::kSimpleModelRocket);
    EXPECT_THROW(
        static_cast<void>(SimulationRunDialog::run(
            nullptr, services.prompts(), runRequestOf(design.document(), {}, services.executor()))),
        BugError);
}

/// The listeners of a run of the threads test: the meeting of the two runs, then the gate of the
/// run, both watching the job's stop.
struct MeetAndHold
{
    const StepMeeting*             meeting;
    const std::array<StepGate, 2>* gates;

    [[nodiscard]] std::vector<std::shared_ptr<SimulationListener>> operator()(
        std::size_t run, const std::stop_token& stop) const
    {
        return {meeting->listener(stop), gates->at(run).listener(stop)};
    }
};

/// How long the threads test waits for the runs: a safety net only a defect reaches.
constexpr int kThreadsSafetyNetMilliseconds =
    static_cast<int>(std::chrono::milliseconds(QtRocket::Test::kRunnerSafetyNet).count());

/// What a NextModalDialog does with the run dialog of the threads test: it waits until both runs
/// are held at their gates (they met on the way, at the same time), then clicks Cancel.
struct CancelWhenBothAreHeld
{
    const std::array<StepGate, 2>* gates;
    bool*                          held;

    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<SimulationRunDialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        *held = QTest::qWaitFor([this] { return gates->at(0).reached() && gates->at(1).reached(); },
                                kThreadsSafetyNetMilliseconds);
        QTest::mouseClick(cancelOf(*dialog), Qt::LeftButton);
    }
};

TEST_F(SimulationRunDialogThreadsTest, TwoRunsGoAtOnceAndTheDialogsCancelStopsThem)
{
    // Plan 11.6 E2, the GUI half of the exit criterion "simulations run concurrently and can be
    // cancelled": the application's executor (two threads) runs both runs of the dialog at the
    // same time (they meet at step 3), holds them at step 6, and the dialog's Cancel stops them;
    // both simulations keep their old results (bug 40) and the dialog closes.
    ScriptedUserPrompts               prompts;
    const TwoSimpleRuns               runs;
    const std::shared_ptr<FlightData> firstStored  = runs.first()->getSimulatedData();
    const std::shared_ptr<FlightData> secondStored = runs.second()->getSimulatedData();
    const StepMeeting                 meeting(2, 3);
    const std::array<StepGate, 2>     gates{StepGate(6), StepGate(6)};
    // After what the runs refer to (the design, the holds): destroyed and joined first, also
    // when the test fails with a run still going.
    PollingSimulationExecutor executor(2);
    bool                      held = false;
    Answer                    answer{.cancelled = false, .allSuccessful = true};
    {
        const NextModalDialog cancel(CancelWhenBothAreHeld{.gates = &gates, .held = &held});
        answer = SimulationRunDialog::run(
            nullptr, prompts, runs.request(executor),
            {.extraListeners = MeetAndHold{.meeting = &meeting, .gates = &gates},
             .sampleClock    = {}});
    }
    EXPECT_TRUE(held);
    EXPECT_TRUE(meeting.allMet());
    EXPECT_EQ(answer, (Answer{.cancelled = true, .allSuccessful = false}));
    EXPECT_EQ(runs.first()->getSimulatedData(), firstStored);
    EXPECT_EQ(runs.second()->getSimulatedData(), secondStored);
    EXPECT_TRUE(prompts.prompts().empty());
    EXPECT_FALSE(executor.isPolling());
}

}  // namespace

}  // namespace QtRocket::Gui::Test
