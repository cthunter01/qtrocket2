#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <QPointer>
#include <QString>

#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/util/Uuid.h"
#include "ScriptedUserPrompts.h"
#include "goldens/GoldenGuiReference.h"

class QWidget;

namespace QtRocket::Gui
{
class ApplicationSession;
class DesignPanel;
class SaveDesignInfoDialog;
}  // namespace QtRocket::Gui

namespace QtRocket::Gui::Test
{

class FromScratchPrompts;

/// The from-scratch scenario of Milestone 2's exit (plan section 9.5, the exit suite
/// FromScratchExitGolden) done in QtRocket's GUI as a user does it: the steps of
/// tests/data/goldens/gui/from-scratch/scenario.json (parseGuiScenario() of
/// tests/core/goldens/GoldenGuiReference.h, which describes them), in the running application
/// (ApplicationSession, started as main() starts it), only through what a user reaches: the menus'
/// actions, the add buttons, the mouse on the component tree and on the fin point table, the keys
/// in the fields of the dialogs, the unit selectors, combo boxes and check boxes, the dialogs'
/// buttons. No step calls a setter of the core.
///
/// How each step is done (OpenRocket's tool, tools/openrocket-gui-reference, does the same steps
/// with OpenRocket's own GUI code):
/// - new: File > New of the window the application started with (a New design, which the new
///   window replaces, as BasicFrame.newAction() and closeIfReplaceable() do);
/// - select: a click on the row of each component in the tree (with Ctrl from the second on),
///   the row scrolled into view first;
/// - add: a click on the add button whose object name is the component's Java class (scrolled
///   into view); edit: Edit > Edit;
/// - type, choose, check: in the open dialog (the configuration dialog, or the "Save Design
///   Info" dialog of Save as), the field whose object name is the step's Java property, its tab
///   chosen first (a click on the tab): a number field gets the step's unit from its unit
///   selector (its menu, when the selector shows another unit), its text typed over and Enter;
///   the name field its text and Enter; a text area its text, then the focus taken away (its
///   commit, Java's focus loss); a combo box the item whose Java enum constant or material name
///   the step names (chosen as its popup or keys choose it: Test::chooseItem()); a check box a
///   click when it is not in the step's state;
/// - editPoint, insertPoint: in the "Shape" tab of the freeform fin set's dialog, a click on the
///   cell (which edits it with its text selected), the text and Enter; a right click on the row
///   and "Insert point" of its menu. The step's unit must be the column's (the default length
///   unit);
/// - button: a click on the dialog's button (kButtons: "convertToFreeform" is "Convert to
///   freeform", object name convertButton); ok: a click on the dialog's OK;
/// - undo, redo: the window's Undo and Redo actions (UndoRedoActions); copy, paste: Copy and
///   Paste of the window's component actions (RocketActions);
/// - saveAs: File > Save as, which shows the "Save Design Info" dialog (modal: the steps up to its
///   OK are done in it, as the dialog's event loop runs) and then asks for the file; chooseFile:
///   the file chooser's answer, the step's file in the scenario's directory (FromScratchPrompts
///   gives the question to the scenario).
/// A step the GUI does not allow (a disabled button or action, a field the dialog has not, an
/// item a combo box does not offer, a step for a window that is not open) ends the run with a
/// failure that names the step: the scenario calls no setter in its place.
///
/// After every step the observer gets what the GUI shows then, in the form of OpenRocket's record
/// (GuiStepRecord: the Undo and Redo actions' texts as the menu shows them and their states,
/// whether the design is saved, the selected components, the open window and its component, the
/// tree's lines and their digest); the record of the steps in the "Save Design Info" dialog is
/// taken in it, that of its OK while the file chooser is open.
///
/// The application: an ApplicationContext over a preferences file of the scenario's directory
/// (a new file: the defaults, as OpenRocket's tool starts with its defaults), the build tree's
/// data (DataPaths::forApplication()) and FromScratchPrompts over the scenario's
/// ScriptedUserPrompts (prompts(): every other question goes there, and is a test failure unless
/// the test scripted it). The last window closed leaves the application running with the menus
/// of no window (LastWindowBehaviour::KEEP_RUNNING, the macOS behaviour), so that a closed design
/// can be opened again through File > Open (closeWindow(), openSavedFile()).
///
/// The base of FlightConfigurationScenario, the driver of the scenarios of version 2 (the Motors &
/// Configuration tab, Milestone 3), which does the steps of its own (perform()), adds to the
/// records what version 2 records (record()) and gives the application prompts of its own; the
/// steps of version 1 that a scenario of version 2 has (select, edit, choose, ok, undo, redo, the
/// steps of Save as) are done here, as in the from-scratch scenario. A choose step of the field
/// "ClusterConfiguration" (the inner tube's cluster layouts, ClusterSelectionPanel) is a click on
/// the button of the layout the step names by its XML name ("4-ring"), as OpenRocket's tool does.
class FromScratchScenario
{
public:
    /// What is called after each step with the step and what it left.
    using Observer = std::function<void(const QtRocket::Test::GuiScenarioStep& step,
                                        const QtRocket::Test::GuiStepRecord&   record)>;

    /// The application (see the class comment), started, with its preferences and the saves in
    /// @p directory, which must exist and outlive the object.
    explicit FromScratchScenario(std::filesystem::path directory);
    virtual ~FromScratchScenario();
    FromScratchScenario(const FromScratchScenario&)            = delete;
    FromScratchScenario& operator=(const FromScratchScenario&) = delete;
    FromScratchScenario(FromScratchScenario&&)                 = delete;
    FromScratchScenario& operator=(FromScratchScenario&&)      = delete;

    /// Does the steps of @p scenario in their order (see the class comment), calling @p observer
    /// after each. The failure that ended the run, naming its step ("step 12 (type OuterRadius
    /// 4.1 cm): ..."); empty when every step was done.
    [[nodiscard]] std::string run(const QtRocket::Test::GuiScenario& scenario,
                                  const Observer&                    observer = {});

    /// The application.
    [[nodiscard]] ApplicationSession& session() const noexcept { return *m_session; }
    /// The questions that are no step of the scenario: the script and the record.
    [[nodiscard]] ScriptedUserPrompts& prompts() noexcept { return m_scripted; }
    /// The window of the scenario (File > New's); null before the first step and once closed.
    [[nodiscard]] MainWindow* window() const noexcept { return m_window.data(); }

    /// The file Save as wrote; nullopt before.
    [[nodiscard]] const std::optional<std::filesystem::path>& savedFile() const noexcept
    {
        return m_savedFile;
    }
    /// The design as Save as saved it, taken when the file chooser was answered: the ids of its
    /// components and flight configurations ...
    [[nodiscard]] const std::set<Uuid>& savedIds() const noexcept { return m_savedIds; }
    /// ... describeRocket() with those ids known ...
    [[nodiscard]] const std::vector<std::string>& savedTree() const noexcept { return m_savedTree; }
    /// ... and documentPreferenceLines() of DesignFileState.h.
    [[nodiscard]] const std::string& savedDocumentPreferences() const noexcept
    {
        return m_savedPreferences;
    }

    /// File > Close design of the scenario's window; what went wrong, or "".
    [[nodiscard]] std::string closeWindow();

    /// File > Open of the menus of no window, the file chooser answered with savedFile(): the
    /// window it opened, or null with what went wrong in @p problem.
    [[nodiscard]] MainWindow* openSavedFile(std::string& problem);

    /// What the GUI shows now, as the record of @p step (see the class comment).
    [[nodiscard]] virtual QtRocket::Test::GuiStepRecord record(
        const QtRocket::Test::GuiScenarioStep& step) const;

    /// @p step as the failures name it: "step <index> (<kind> <what it names>)".
    [[nodiscard]] static std::string stepText(const QtRocket::Test::GuiScenarioStep& step);

    /// The text a menu shows for an action's @p text: Qt's mnemonic marks taken out ("&Undo"
    /// is "Undo", "&&" is "&").
    [[nodiscard]] static QString shownMenuText(const QString& text);

    /// A button of a dialog that a step names: the name in the scenario, the object name of
    /// QtRocket's button.
    struct ButtonName
    {
        std::string_view step;
        std::string_view objectName;
    };

    /// The buttons the steps name: "convertToFreeform", Convert to freeform of a fin set's
    /// dialog (FinSetConfig's convertButton).
    static constexpr std::array<ButtonName, 1> kButtons{
        ButtonName{.step = "convertToFreeform", .objectName = "convertButton"}};

protected:
    /// Makes the application's questions over the scenario's script @p scripted (which outlives
    /// them).
    using PromptsFactory =
        std::function<std::unique_ptr<FromScratchPrompts>(UserPrompts& scripted)>;

    /// The application as the public constructor makes it, its questions those @p makePrompts
    /// makes (a subclass's prompts).
    FromScratchScenario(std::filesystem::path directory, const PromptsFactory& makePrompts);

    /// Does @p step; what went wrong, or "". The steps of version 2 are none of this class's (an
    /// error): a subclass does them.
    [[nodiscard]] virtual std::string perform(const QtRocket::Test::GuiScenarioStep& step);

    /// The application's questions.
    [[nodiscard]] FromScratchPrompts& scenarioPrompts() const noexcept { return *m_prompts; }
    /// Makes @p window the scenario's window (File > Open's).
    void setScenarioWindow(MainWindow* window) { m_window = window; }
    /// The scenario's window, which must be open.
    [[nodiscard]] MainWindow& scenarioWindow() const;

private:
    [[nodiscard]] std::string newDesign();
    [[nodiscard]] std::string select(const QtRocket::Test::GuiScenarioStep& step);
    [[nodiscard]] std::string add(const QtRocket::Test::GuiScenarioStep& step);
    [[nodiscard]] std::string type(const QtRocket::Test::GuiScenarioStep& step);
    [[nodiscard]] std::string choose(const QtRocket::Test::GuiScenarioStep& step);
    [[nodiscard]] std::string check(const QtRocket::Test::GuiScenarioStep& step);
    [[nodiscard]] std::string button(const QtRocket::Test::GuiScenarioStep& step);
    [[nodiscard]] std::string saveAs(const QtRocket::Test::GuiScenarioStep& step);

    /// The steps done in the "Save Design Info" dialog @p dialog, from the next one up to its
    /// OK; what went wrong, or "".
    [[nodiscard]] std::string designInformationSteps(SaveDesignInfoDialog& dialog);
    /// The file chooser of Save as (FromScratchPrompts): the record of the OK before it, then
    /// the chooseFile step.
    [[nodiscard]] std::optional<UserPrompts::SaveFileAnswer> chooseFile();

    /// Calls the observer with the record of @p step.
    void observe(const QtRocket::Test::GuiScenarioStep& step) const;

    /// The window's design panel; the window must be open.
    [[nodiscard]] DesignPanel& designPanel() const;
    /// The dialog the field steps work in: the "Save Design Info" dialog while it is open, else
    /// the visible configuration dialog; null when none is.
    [[nodiscard]] QWidget* openDialog() const;

    std::filesystem::path m_directory;
    /// The questions (declared before the application, which asks them until it goes).
    ScriptedUserPrompts                 m_scripted;
    FromScratchPrompts*                 m_prompts{nullptr};
    std::unique_ptr<ApplicationSession> m_session;
    QPointer<MainWindow>                m_window;

    // The run.
    const std::vector<QtRocket::Test::GuiScenarioStep>* m_steps{nullptr};
    std::size_t                                         m_next{0};
    Observer                                            m_observer;
    /// Set while the "Save Design Info" dialog is open (its steps are done in it).
    SaveDesignInfoDialog* m_designInformation{nullptr};
    /// Set while the file chooser of Save as is open.
    bool m_fileChooserOpen{false};
    /// The OK of the "Save Design Info" dialog, whose record the file chooser takes.
    const QtRocket::Test::GuiScenarioStep* m_pendingOk{nullptr};
    /// The chooseFile step the file chooser did.
    const QtRocket::Test::GuiScenarioStep* m_chosenFile{nullptr};
    /// What went wrong in a step done inside a question of Save as.
    std::string m_saveAsProblem;

    std::optional<std::filesystem::path> m_savedFile;
    std::set<Uuid>                       m_savedIds;
    std::vector<std::string>             m_savedTree;
    std::string                          m_savedPreferences;
};

}  // namespace QtRocket::Gui::Test
