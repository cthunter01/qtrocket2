#pragma once

#include <cstddef>
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "QtRocket/gui/util/UserPrompts.h"
#include "exit/FlightConfigurationPrompts.h"

class QWidget;

namespace QtRocket::Gui::Test
{

/// The questions of the application in the scenario of the Flight simulations tab
/// (SimulationsScenario): those of FlightConfigurationPrompts (the script and its record, the two
/// windows of Save as, the parts library closed, the dialogs of the Motors & Configuration tab),
/// and the questions the simulations tab and its dialogs ask, each in the dialog the application
/// shows for it:
/// - the run dialog of the settings dialog's OK (runSimulations(): SimulationRunDialog::run(), as
///   WidgetUserPrompts runs it);
/// - the settings dialog's discard question (askDiscardChanges()), the delete confirmation of
///   Delete and Cut (askDeleteSimulations()) and the wind editor's overwrite question
///   (askYesNo()): WidgetUserPrompts' dialogs;
/// - the CSV import settings of the wind editor's "Import levels" (askWindCsvImportSettings():
///   CsvImportSettingsDialog, as WidgetUserPrompts shows it).
/// Each is answered once it is open, inside its modal loop (NextModalDialog), by the next of the
/// answers the scenario queued before the click that asks it (queueAnswer()), in their order, as
/// OpenRocket's tool answers OpenRocket's dialogs (tools/openrocket-gui-reference,
/// SimulationQuestions.java: a dialog that shows takes the next queued answer). A question asked
/// while no answer is queued goes to the script, which fails the test (no answer scripted); such
/// questions are counted (unansweredSimulationQuestions()).
///
/// The file choosers (the wind import's and the lookup dialog's "Load from file...") go to the
/// script, whose answerOpenFiles() the scenario scripts (Qt's own file dialog is never shown in a
/// test but WidgetFileDialogTest's), each first told the scenario (setOpenFileObserver()). The
/// run dialog's failures and the CSV import settings' messages are asked of this object, never
/// shown in a dialog of their own: they reach the script (a message is recorded there, any other
/// question fails the test).
class SimulationsPrompts final : public FlightConfigurationPrompts
{
public:
    /// What is told a file chooser's request.
    using OpenFileObserver = std::function<void(const OpenFileRequest& request)>;

    /// The questions of @p scripted, which must outlive this object, but for those above.
    explicit SimulationsPrompts(UserPrompts& scripted) noexcept
      : FlightConfigurationPrompts(scripted)
    {
    }

    /// Queues @p answer, named @p name in the failures, for the next question of the class
    /// comment that has none (after the answers queued before it).
    void queueAnswer(std::string name, DialogAnswer answer);
    /// The names of the answers no question took, in their order; the queue is emptied.
    [[nodiscard]] std::vector<std::string> takeUnusedAnswers();
    /// Tells @p observer every file chooser's request from now on; an empty one tells none.
    void setOpenFileObserver(OpenFileObserver observer) { m_openFiles = std::move(observer); }

    /// The questions of the class comment asked without a queued answer so far.
    [[nodiscard]] std::size_t unansweredSimulationQuestions() const noexcept
    {
        return m_unanswered;
    }

    [[nodiscard]] DiscardAnswer askDiscardChanges(QWidget*        parent,
                                                  DiscardQuestion question) override;
    [[nodiscard]] bool          askYesNo(QWidget* parent, const Question& question) override;
    [[nodiscard]] std::optional<OpenFileAnswer> askOpenFiles(
        QWidget* parent, const OpenFileRequest& request) override;
    [[nodiscard]] DeleteSimulationsAnswer askDeleteSimulations(QWidget*    parent,
                                                               std::size_t count) override;
    [[nodiscard]] RunSimulationsAnswer    runSimulations(
        QWidget* parent, const RunSimulationsRequest& request) override;
    [[nodiscard]] std::optional<WindCsvImportSettings> askWindCsvImportSettings(
        QWidget* parent, const WindCsvImportSettings& initial) override;

private:
    /// The next queued answer, taken from the queue; nullopt (the question counted) when none is
    /// queued.
    [[nodiscard]] std::optional<DialogAnswer> nextAnswer();

    std::deque<std::pair<std::string, DialogAnswer>> m_answers;
    OpenFileObserver                                 m_openFiles;
    std::size_t                                      m_unanswered{0};
};

}  // namespace QtRocket::Gui::Test
