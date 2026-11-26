#pragma once

#include <cstddef>
#include <deque>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "exit/FlightConfigurationScenario.h"
#include "goldens/GoldenGuiReference.h"

class QAbstractButton;
class QString;
class QWidget;

namespace QtRocket::Gui
{
class AerodynamicLookupDialog;
class MultiLevelWindEditDialog;
class SimulationConfigDialog;
class SimulationPanel;
}  // namespace QtRocket::Gui

namespace QtRocket::Gui::Test
{

class ManualSimulationExecutor;
class SimulationsPrompts;

/// The scenario of the Flight simulations tab of Milestone 4's exit (plan section 11.6 E6, the
/// exit suite SimulationsExitGolden) done in QtRocket's GUI as a user does it: the steps of
/// tests/data/goldens/gui/simulations/scenario.json (schema version 3, kGuiSimulationsScenario,
/// parseGuiScenario() of tests/core/goldens/GoldenGuiReference.h), in the running application,
/// through what a user reaches: File > Open, the window's tabs, clicks on the rows of the
/// simulations table, the tab's buttons, the Edit menu, the column chooser of the table's header,
/// and the widgets of the dialogs the operations open. No step calls a setter of the core or an
/// operation of the tab (SimulationPanel, SimulationEditing) itself.
///
/// How each step is done (OpenRocket's tool, tools/openrocket-gui-reference, does the same with
/// OpenRocket's own SimulationPanel and its dialogs; its README describes the steps):
/// - tab "simulations": a click on the window's third tab ("Flight simulations"), which must not
///   be the current one; the tab then takes the spotlight (its first row selected when none is);
/// - selectSimulations: a click on each row in the name column (the table is never sorted: the
///   row is the simulation's place in the document), with Ctrl or Shift as the row says; no row:
///   a click below the last row;
/// - newSimulation, editSimulation: a click on the tab's "New simulation" or "Edit simulation"
///   button; deleteSimulations: on "Delete simulations"; duplicateSimulations, copySimulations,
///   cutSimulations, pasteSimulations: the Edit menu's Duplicate, Copy, Cut and Paste, which
///   act on the selected simulations (RocketActions; OpenRocket's tool calls the panel's own
///   actions, which RocketActions calls). Each must be enabled;
/// - columnVisibility: a right click on the table's header, then a click on the column's item of
///   the column chooser's menu (SimulationColumnVisibility), which must change the column;
/// - the dialogs the operations open are answered by the step's answer inside their modal loops,
///   as the tool answers OpenRocket's (SimulationQuestions.java), each recorded as it opened and,
///   for the dialogs with fields, as the answer left it before its button:
///   - the settings dialog (SimulationConfigDialog): the edits in order, then OK or Cancel; a
///     field by its objectName (the Java property: a number field with its unit chosen in its
///     unit selector first and the text typed with Enter; a check box clicked; a combo box's item
///     by its Java enum constant), brought into view first as a user does it (its tab, the
///     warnings page's Configuration tab, a collapsed section expanded by a click on its header),
///     then shown and enabled; the name typed over the name field; the configuration chosen in
///     its box; a tab clicked; the wind model's radio button; "Fixed random seed" ticked and the
///     seed typed; "Reset to default" or "Save as default" of a page; "Edit levels" and
///     "Configure..." with the wind editor's and the lookup dialog's answers. An edit must change
///     the dialog's simulation or tab (but "Save as default" and the two nested dialogs);
///   - the run dialog of OK (SimulationRunDialog): Cancel, before any of its simulations ran (the
///     application's executor is a ManualSimulationExecutor that runs nothing: executor());
///   - the discard question of Cancel, the delete confirmation and the wind editor's overwrite
///     question (WidgetUserPrompts' dialogs): the "Do not ask me again" box ticked when the answer
///     says so, then the button;
///   - the wind editor (MultiLevelWindEditDialog): the altitude reference's radio button, "Add
///     new level", a row selected (a click on its intensity cell) then "Delete level", a row's
///     delete icon, a cell edited (a click on it opens its editor, the text typed, Enter), a
///     column's unit chosen in the header's unit selector, "Reset levels" and "Import levels"
///     with their questions, then Close;
///   - the CSV import settings (CsvImportSettingsDialog): the header box, each row's column name
///     typed, index typed and unit chosen, the separator chosen in its box (SPACE and TAB for the
///     space and the tab, another text typed into it), then OK or Cancel; a name only while the
///     header box is on and an index while it is off (the row shows one of them), each setting
///     shown, enabled and other than the one shown;
///   - the file choosers of the wind import and of the lookup dialog: the script's answer (Qt's own
///     file dialog is shown by no test but WidgetFileDialogTest's), the step's file in the
///     repository (guiRepositoryFile()) or Cancel;
///   - the aerodynamic lookup dialog (AerodynamicLookupDialog): per section its tab clicked, then
///     its actions: the separator chosen, "Load from file..." with its file chooser, the text
///     pasted over the text area's (select all, then the text inserted in one edit, as OpenRocket's
///     tool sets it), Refresh, Clear; then OK or Cancel; a separator, a text and a load whose
///     file chooser is answered with Open must change what the dialog shows;
///   a dialog that does not open, an answer no dialog took, and an answer whose button leaves its
///   dialog open fail the step (a run dialog its Cancel leaves open, which would never close, is
///   closed by a safety net after two minutes);
///   check boxes and radio buttons are clicked on their indicator and text (the style's click
///   rectangle), other buttons in their middle;
/// - the steps of versions 1 and 2 (open, undo, redo, Save as and its steps) as
///   FlightConfigurationScenario does them.
/// A step the GUI does not allow (a disabled button or action, a field the dialog does not show or
/// disables, a value a field shows already) ends the run with a failure that names the step.
///
/// After every step the observer gets what the GUI shows then, in the form of OpenRocket's record
/// of version 3 (GuiStepRecord): FlightConfigurationScenario's members (the window's tab is
/// "simulations" while the third tab is the current one), the questions of the simulations tab's
/// step (each as its dialog opened, and as the answer left it), what the tab showed
/// (GuiSimulationsTabState: simulationsTabState()), the document's simulations
/// (guiSimulationStates()), and after Copy and Cut the system clipboard's text. No record has
/// "toolFixes": they say where OpenRocket's record came from.
///
/// The application is FlightConfigurationScenario's, with the questions of SimulationsPrompts and
/// a ManualSimulationExecutor that runs nothing (its NEVER mode): no simulation runs before Save
/// as, as in OpenRocket's tool.
class SimulationsScenario final : public FlightConfigurationScenario
{
public:
    /// The application (see the class comment), started, with its preferences and the saves in
    /// @p directory, which must exist and outlive the object.
    explicit SimulationsScenario(std::filesystem::path directory);
    ~SimulationsScenario() override;
    SimulationsScenario(const SimulationsScenario&)            = delete;
    SimulationsScenario& operator=(const SimulationsScenario&) = delete;
    SimulationsScenario(SimulationsScenario&&)                 = delete;
    SimulationsScenario& operator=(SimulationsScenario&&)      = delete;

    /// FlightConfigurationScenario's record of @p step with the members of version 3 (see the
    /// class comment).
    [[nodiscard]] QtRocket::Test::GuiStepRecord record(
        const QtRocket::Test::GuiScenarioStep& step) const override;

    /// The Flight simulations tab of the scenario's window; null when no window is open.
    [[nodiscard]] SimulationPanel* simulationsTab() const;

    /// The application's executor of simulations: every job the run dialogs submitted, none run.
    [[nodiscard]] ManualSimulationExecutor& executor() const noexcept { return *m_executor; }

    /// What @p panel shows, in the form of the records (the terms of the simulation-table pins:
    /// the help card, the view's columns, every row in the document's order, the selected rows,
    /// the tab's actions but Milestone 7's Monte Carlo, the simulations on the application's
    /// clipboard, the document preference of the hidden columns).
    [[nodiscard]] static QtRocket::Test::GuiSimulationsTabState simulationsTabState(
        const SimulationPanel& panel);

protected:
    /// The steps of version 3 (see the class comment); the others FlightConfigurationScenario's.
    [[nodiscard]] std::string perform(const QtRocket::Test::GuiScenarioStep& step) override;

private:
    [[nodiscard]] std::string showSimulationsTab(const QtRocket::Test::GuiScenarioStep& step);
    [[nodiscard]] std::string selectRows(const QtRocket::Test::GuiScenarioStep& step);
    [[nodiscard]] std::string settingsOperation(const QtRocket::Test::GuiScenarioStep& step);
    [[nodiscard]] std::string deleteOperation(const QtRocket::Test::GuiScenarioStep& step);
    [[nodiscard]] std::string menuOperation(const QtRocket::Test::GuiScenarioStep& step);
    [[nodiscard]] std::string columnVisibility(const QtRocket::Test::GuiScenarioStep& step);

    /// The tab of the window, shown: null with what went wrong in @p problem otherwise.
    [[nodiscard]] SimulationPanel* shownPanel(std::string& problem) const;
    /// Clicks @p button, named @p name in the failures, which must be shown and enabled, its
    /// questions answered as the step queued them (see the class comment), and settles; what went
    /// wrong (a question without an answer among them), or "".
    [[nodiscard]] std::string clickWithAnswers(QAbstractButton& button, std::string_view name);
    /// What went wrong in the answers of the step: an answer left over, a question without one,
    /// a failure inside a dialog; or "".
    [[nodiscard]] std::string answerProblems();
    /// Notes @p problem, the first one counts.
    void fail(const std::string& problem);

    // The dialogs (see the class comment): each records its question as it opened, does its
    // answer's edits in order, records what they left and closes the dialog with the answer's
    // button; what goes wrong is noted (fail()) and closes the dialog, so that the operation goes
    // on. @p shown is the modal dialog that opened (NextModalDialog), of the class the step
    // expects (a failure otherwise).

    /// The settings dialog (SimulationConfigDialog).
    void answerSettings(QWidget& shown, const QtRocket::Test::GuiSettingsAnswer& answer);
    /// OK of the settings dialog @p dialog, the run dialog's answer queued, or Cancel, the discard
    /// question's queued; what went wrong, or "".
    [[nodiscard]] std::string settingsButton(SimulationConfigDialog&                  dialog,
                                             const QtRocket::Test::GuiSettingsAnswer& answer);
    /// Closes the settings dialog @p dialog of a failed answer (Cancel, its discard question
    /// answered Yes).
    void closeSettingsDialog(SimulationConfigDialog& dialog);
    /// One edit of the settings dialog @p dialog; what went wrong, or "".
    [[nodiscard]] std::string settingsEdit(SimulationConfigDialog&                dialog,
                                           const QtRocket::Test::GuiSettingsEdit& edit);
    /// "Edit levels" of the multi-level wind, the wind editor answered with @p answer.
    [[nodiscard]] std::string windLevelsEdit(SimulationConfigDialog&                    dialog,
                                             const QtRocket::Test::GuiWindEditorAnswer& answer);
    /// "Configure..." of the aerodynamic data, the lookup dialog answered with @p answer.
    [[nodiscard]] std::string lookupEdit(SimulationConfigDialog&                dialog,
                                         const QtRocket::Test::GuiLookupAnswer& answer);
    /// The wind editor (MultiLevelWindEditDialog) of the first simulation of @p settings.
    void answerWindEditor(QWidget& shown, const QtRocket::Test::GuiWindEditorAnswer& answer,
                          const SimulationConfigDialog& settings);
    /// One edit of the wind editor @p editor; what went wrong, or "".
    [[nodiscard]] std::string windEdit(MultiLevelWindEditDialog&          editor,
                                       const QtRocket::Test::GuiWindEdit& edit);
    /// "Import levels" of @p editor, the answers of its questions queued; what went wrong, or "".
    [[nodiscard]] std::string importLevels(MultiLevelWindEditDialog&                  editor,
                                           const QtRocket::Test::GuiWindImportAnswer& answer);
    /// The CSV import settings (CsvImportSettingsDialog).
    void answerCsvSettings(QWidget& shown, const QtRocket::Test::GuiCsvImportAnswer& answer);
    /// The aerodynamic lookup dialog (AerodynamicLookupDialog).
    void answerLookup(QWidget& shown, const QtRocket::Test::GuiLookupAnswer& answer);
    /// The actions of the section @p section (0 drag, 1 stability) of the lookup dialog @p dialog,
    /// its tab clicked first; nothing for nullopt. What went wrong, or "".
    [[nodiscard]] std::string lookupSection(
        AerodynamicLookupDialog& dialog, int section,
        const std::optional<std::vector<QtRocket::Test::GuiLookupAction>>& actions);
    /// One action of the section @p section of the lookup dialog @p dialog; what went wrong, or
    /// "".
    [[nodiscard]] std::string lookupAction(AerodynamicLookupDialog& dialog, int section,
                                           const QtRocket::Test::GuiLookupAction& action);
    /// A confirmation (an OptionDialog of WidgetUserPrompts: the delete confirmation, the
    /// discard question, the wind editor's overwrite question).
    void answerConfirm(QWidget& shown, const QtRocket::Test::GuiConfirmAnswer& answer);
    /// The run dialog (SimulationRunDialog): Cancel, before any of its simulations ran.
    void answerRun(QWidget& shown);
    /// Queues @p answer for the next file chooser (see the class comment).
    void queueFileAnswer(const QtRocket::Test::GuiFileAnswer& answer);
    /// A file chooser titled @p title was asked for: in a step of the simulations tab its question
    /// recorded, and the next queued file answer given to the script (none: the script fails the
    /// test).
    void fileChooserAsked(const QString& title);

    SimulationsPrompts*       m_simulationPrompts{nullptr};
    ManualSimulationExecutor* m_executor{nullptr};
    /// The questions of the step being done, in the order they opened.
    std::vector<QtRocket::Test::GuiQuestion> m_questions;
    /// The system clipboard's text after Copy and Cut.
    std::optional<std::string> m_clipboardText;
    /// The answers of the file choosers the step is to ask, in order.
    std::deque<QtRocket::Test::GuiFileAnswer> m_fileAnswers;
    /// The file choosers asked without a queued answer.
    std::size_t m_unansweredFiles{0};
    /// What went wrong inside a dialog of the step (the first problem).
    std::string m_problem;
    /// Whether the step being done is one of the simulations tab (its file choosers are its
    /// questions).
    bool m_simulationStep{false};
};

}  // namespace QtRocket::Gui::Test
