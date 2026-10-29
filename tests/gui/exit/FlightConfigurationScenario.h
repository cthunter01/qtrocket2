#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "exit/FromScratchScenario.h"
#include "goldens/GoldenGuiReference.h"

class QWidget;

namespace QtRocket::Gui
{
class DeploymentSelectionDialog;
class FlightConfigurationPanel;
class IgnitionSelectionDialog;
class MotorChooserDialog;
class RenameConfigDialog;
class SeparationSelectionDialog;
}  // namespace QtRocket::Gui

namespace QtRocket::Gui::Test
{

class FlightConfigurationPrompts;

/// The scenarios of the Motors & Configuration tab of Milestone 3's exit (plan section 10.5, the
/// exit suite FlightConfigurationsExitGolden) done in QtRocket's GUI as a user does them: the
/// steps of tests/data/goldens/gui/<scenario>/scenario.json (schema version 2, kGuiTabScenarios,
/// parseGuiScenario() of tests/core/goldens/GoldenGuiReference.h), in the running application,
/// through what a user reaches: File > Open, the window's tabs and the tab's sub-tabs, clicks on
/// the cells of the tables and on the motor mount table's check boxes, the tab's buttons, and the
/// widgets of the dialogs their questions open. No step calls a setter of the core or an
/// operation of the tab (FlightConfigurationEditing) itself.
///
/// How each step is done (OpenRocket's tool, tools/openrocket-gui-reference, does the same with
/// OpenRocket's own panels and dialogs; its README describes the steps):
/// - open: File > Open of the window the application started with, the file chooser answered
///   with the step's file (a path of the source tree: guiRepositoryFile()); the opened design
///   replaces the start's New design (BasicFrame.closeIfReplaceable()) and is the scenario's
///   window;
/// - tab, configTab: a click on the tab of the window's tabs ("Rocket design", "Motors &
///   Configuration"), or of the tab's sub-tabs (Motors, Recovery, Stages), which must not be the
///   current one and must be enabled;
/// - selectCells: a click on each cell of the current sub-tab's table, with Ctrl or Shift as the
///   cell says (the row of the configuration, the column of the component; column 0 for none),
///   the cell scrolled into view first;
/// - mountCheck: a click on the check box of the mount's row of the motor mount table (shown on
///   the Motors sub-tab), which must not be in the state asked for;
/// - the operations: a click on the button (newConfButton, renameConfButton, removeConfButton,
///   duplicateConfButton; selectMotorButton, deleteMotorButton, selectIgnitionButton,
///   resetIgnitionButton; selectDeploymentButton, resetDeploymentButton; selectSeparationButton
///   and the Stages sub-tab's reset button), which must be shown and enabled; the question it asks
///   is answered in QtRocket's own dialog (FlightConfigurationPrompts), inside the dialog's modal
///   loop, by the step's answer, as the tool answers OpenRocket's (Questions.java):
///   - the ignition, deployment and separation dialogs: the radio button of the scope clicked
///     when it is not chosen, the event chosen in the event box when it is not the current item,
///     the altitude's unit chosen in its unit selector and the number typed into its field, the
///     delay typed (a field without a unit selector, whose unit must be the answer's), then OK; or
///     Cancel alone. A number equal to what its field shows is not typed, as a user leaves such a
///     field alone, and the altitude is given exactly when its field is enabled;
///   - the rename dialog: the name typed over the text field's, "Save as default" clicked after
///     it (its message is a message of the script: the record), then OK, Reset to default or
///     Cancel;
///   - the motor chooser: the search typed over the search field's text, a click on the table's
///     row of the motor set (by the Manufacturer column's text and the set's designation), the
///     curve chosen in the curve box, the delay chosen among the delay box's items or typed into
///     its editor with Enter, the nozzle exit diameter's unit chosen and the number typed, each
///     when the answer has it and in this order, then OK or Cancel;
///   a step whose operation asks a question must have an answer, one with an answer must be asked
///   one (and only one);
/// - the steps of version 1 (select, edit, choose, ok, undo, redo, Save as and its steps) as
///   FromScratchScenario does them.
/// A step the GUI does not allow (a disabled or hidden button, a tab already current, a cell the
/// table does not have, a field a dialog disables) ends the run with a failure that names the
/// step.
///
/// After every step the observer gets what the GUI shows then, in the form of OpenRocket's record
/// of version 2 (GuiStepRecord): FromScratchScenario's members, the window's tab, the questions
/// the step asked (each as its dialog opened; the chooser also as the answer left it), and the
/// tab's state (GuiFlightConfigurationState: tabState()).
///
/// The application is FromScratchScenario's (its preferences in a new file of the scenario's
/// directory, the build tree's data: the bundled motors), with the questions of
/// FlightConfigurationPrompts.
class FlightConfigurationScenario final : public FromScratchScenario
{
public:
    /// The application (see the class comment), started, with its preferences and the saves in
    /// @p directory, which must exist and outlive the object.
    explicit FlightConfigurationScenario(std::filesystem::path directory);
    ~FlightConfigurationScenario() override;
    FlightConfigurationScenario(const FlightConfigurationScenario&)            = delete;
    FlightConfigurationScenario& operator=(const FlightConfigurationScenario&) = delete;
    FlightConfigurationScenario(FlightConfigurationScenario&&)                 = delete;
    FlightConfigurationScenario& operator=(FlightConfigurationScenario&&)      = delete;

    /// FromScratchScenario's record of @p step with the members of version 2 (see the class
    /// comment).
    [[nodiscard]] QtRocket::Test::GuiStepRecord record(
        const QtRocket::Test::GuiScenarioStep& step) const override;

    /// The Motors & Configuration tab of the scenario's window; null when no window is open.
    [[nodiscard]] FlightConfigurationPanel* tab() const;

    /// What @p panel shows, in the form of the records (the names of OpenRocket's buttons, the
    /// tables' texts as their cells show them, the selected rows and columns, the rocket's
    /// selected configuration and the simulations' configurations as rows).
    [[nodiscard]] static QtRocket::Test::GuiFlightConfigurationState tabState(
        const FlightConfigurationPanel& panel);

protected:
    /// The steps of version 2 (see the class comment); the others FromScratchScenario's.
    [[nodiscard]] std::string perform(const QtRocket::Test::GuiScenarioStep& step) override;

private:
    [[nodiscard]] std::string open(const QtRocket::Test::GuiScenarioStep& step);
    [[nodiscard]] std::string windowTab(const QtRocket::Test::GuiScenarioStep& step);
    [[nodiscard]] std::string configTab(const QtRocket::Test::GuiScenarioStep& step);
    [[nodiscard]] std::string selectCells(const QtRocket::Test::GuiScenarioStep& step);
    [[nodiscard]] std::string mountCheck(const QtRocket::Test::GuiScenarioStep& step);
    [[nodiscard]] std::string operation(const QtRocket::Test::GuiScenarioStep& step);

    /// The tab of the window, shown: null with what went wrong in @p problem otherwise.
    [[nodiscard]] FlightConfigurationPanel* shownTab(std::string& problem) const;

    /// Answers the open dialog @p dialog of the operation's question with the step's answer
    /// (FlightConfigurationPrompts' answer); a failure is kept for the step and closes the dialog.
    void                      answerDialog(QWidget& dialog);
    [[nodiscard]] std::string answerIgnition(IgnitionSelectionDialog&         dialog,
                                             const QtRocket::Test::GuiAnswer& answer);
    [[nodiscard]] std::string answerDeployment(DeploymentSelectionDialog&       dialog,
                                               const QtRocket::Test::GuiAnswer& answer);
    [[nodiscard]] std::string answerSeparation(SeparationSelectionDialog&       dialog,
                                               const QtRocket::Test::GuiAnswer& answer);
    [[nodiscard]] std::string answerRename(RenameConfigDialog&              dialog,
                                           const QtRocket::Test::GuiAnswer& answer);
    [[nodiscard]] std::string answerChooser(MotorChooserDialog&              dialog,
                                            const QtRocket::Test::GuiAnswer& answer);

    FlightConfigurationPrompts* m_tabPrompts{nullptr};
    /// The questions of the step being done, in order.
    std::vector<QtRocket::Test::GuiQuestion> m_questions;
    /// The answer of the operation being done; null when it has none.
    const QtRocket::Test::GuiAnswer* m_answer{nullptr};
    /// The dialogs the operation's questions opened.
    std::size_t m_dialogs{0};
    /// What went wrong while a dialog was answered.
    std::string m_dialogProblem;
};

}  // namespace QtRocket::Gui::Test
