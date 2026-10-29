#pragma once

// Reading the references of the GUI's scenarios, tests/data/goldens/gui/<scenario>/
// (tools/openrocket-gui-reference, whose README.md describes the files), of two schemas:
// - version 1, the from-scratch scenario of Milestone 2 (gui/from-scratch/): a rocket designed
//   from scratch, OpenRocket's GUI code run headless;
// - version 2, the scenarios of the Motors & Configuration tab of Milestone 3
//   (gui/flight-configurations/, gui/multi-stage/, kGuiTabScenarios): a design file opened, the
//   tab's operations and their questions, OpenRocket's real panels and dialogs run on a virtual
//   display with the bundled motors and component presets.
// Each directory holds:
// - scenario.json, the scenario: what a user does in the GUI, step by step, at the level of its
//   actions (File > New or File > Open, a click on rows of the component tree, an add button, a
//   field of a configuration dialog by its Java property name, Undo, Copy, Save as, the tab's
//   buttons with the answers to their questions ...). The same file drives OpenRocket's GUI code
//   in the tool and QtRocket's GUI in the exit tests;
// - rocket.ork, the design OpenRocket saved at the end of the scenario;
// - state.json, what OpenRocket's GUI had after each step (the undo and redo actions, the
//   selection, the open dialog, the component tree in the lines of describeRocket(); in version 2
//   also the window's tab, the questions asked and what the tab showed) and the saved file as
//   OpenRocket loads it (the lines of designFileState(), the masses and the length of
//   massAndLength()).
// Test-only and Qt-free: from_scratch_reference_golden_tests.cpp and
// flight_configurations_reference_golden_tests.cpp check the files against each other and against
// QtRocket's loader; the GUI's exit tests drive the scenarios and compare with the records.

#include <array>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Error.h"

namespace QtRocket::Test
{

/// tests/data/goldens/gui/from-scratch in the source tree.
[[nodiscard]] std::filesystem::path guiFromScratchDir();

/// tests/data/goldens/gui/@p scenario in the source tree.
[[nodiscard]] std::filesystem::path guiScenarioDir(std::string_view scenario);

/// The scenarios of version 2, the Motors & Configuration tab's (FlightConfigurationsExitGolden).
inline constexpr std::array<std::string_view, 2> kGuiTabScenarios{"flight-configurations",
                                                                  "multi-stage"};

/// The file @p path names in the source tree, a path relative to the repository's root with '/'
/// between its names (the "file" of an open step:
/// "tests/data/goldens/gui/from-scratch/rocket.ork"), composed name by name with operator/ (Windows
/// keeps a '/' inside a name as it is). ErrorCode::PARSE for an empty or absolute path, an empty,
/// "." or ".." name, or a backslash.
[[nodiscard]] Result<std::filesystem::path> guiRepositoryFile(std::string_view path);

/// One element of a component path: the name of a child, and which of the children of that
/// name it is (from 1).
struct GuiPathElement
{
    std::string name;
    int         occurrence{1};

    friend bool operator==(const GuiPathElement&, const GuiPathElement&) = default;
};

/// A component path: the names a user reads in the component tree from the stage down to the
/// component; the empty path is the rocket.
using GuiComponentPath = std::vector<GuiPathElement>;

/// @p path as text for messages: the names joined by '/', a later child of its name with
/// "#<occurrence>" ("Sustainer/Body Tube/Parachute#2"), the rocket "(rocket)".
[[nodiscard]] std::string guiComponentPathText(const GuiComponentPath& path);

/// The component @p path names below @p rocket (ErrorCode::NOT_FOUND when there is none).
[[nodiscard]] Result<const RocketComponent*> resolveGuiComponentPath(const Rocket&           rocket,
                                                                     const GuiComponentPath& path);

/// The path of @p component, which must be in a rocket's tree.
[[nodiscard]] GuiComponentPath guiComponentPathOf(const RocketComponent& component);

/// What a step of the scenario does (its "step" member). The steps of version 1 come first; a
/// scenario of version 2 has every kind but NEW, ADD, COPY and PASTE.
enum class GuiStepKind
{
    NEW,                ///< "new": File > New, a new design window; its sustainer is selected (v1)
    SELECT,             ///< "select": a click on the rows of paths (more than one with Ctrl)
    ADD,                ///< "add": the add button of component (a Java class name; v1)
    EDIT,               ///< "edit": Edit > Edit (or a double click): the selection's dialog
    TYPE,               ///< "type": text typed into field, after choosing unit in its selector
    CHOOSE,             ///< "choose": item chosen in the combo box field
    CHECK,              ///< "check": the check box field set to checked
    EDIT_POINT,         ///< "editPoint": text typed into a cell of the fin point table
    INSERT_POINT,       ///< "insertPoint": row selected, "Insert point" of the table's context menu
    BUTTON,             ///< "button": a dialog button ("convertToFreeform")
    OK,                 ///< "ok": the dialog's OK button
    UNDO,               ///< "undo": Edit > Undo
    REDO,               ///< "redo": Edit > Redo
    COPY,               ///< "copy": Edit > Copy (v1)
    PASTE,              ///< "paste": Edit > Paste (v1)
    SAVE_AS,            ///< "saveAs": File > Save as, which opens the design information dialog
    CHOOSE_FILE,        ///< "chooseFile": the file chooser of Save as, the name file typed
    OPEN,               ///< "open": File > Open of file (v2, the first step and only there)
    TAB,                ///< "tab": the window's tab, "design" or "flightConfiguration" (v2)
    CONFIG_TAB,         ///< "configTab": the sub-tab, "motors", "recovery" or "stages" (v2)
    SELECT_CELLS,       ///< "selectCells": clicks on cells of the current sub-tab's table (v2)
    MOUNT_CHECK,        ///< "mountCheck": the motor mount table's check box of mount (v2)
    NEW_CONFIGURATION,  ///< "newConfiguration": New, its follow-up question answered (v2)
    RENAME_CONFIGURATION,     ///< "renameConfiguration": Rename, the rename dialog answered (v2)
    DELETE_CONFIGURATION,     ///< "deleteConfiguration": Remove configuration (v2)
    DUPLICATE_CONFIGURATION,  ///< "duplicateConfiguration": Duplicate configuration (v2)
    SELECT_MOTOR,             ///< "selectMotor": Select motor, the motor chooser answered (v2)
    DELETE_MOTOR,             ///< "deleteMotor": Delete motor (v2)
    SELECT_IGNITION,          ///< "selectIgnition": the ignition dialog answered (v2)
    RESET_IGNITION,           ///< "resetIgnition": Reset ignition (v2)
    SELECT_DEPLOYMENT,        ///< "selectDeployment": the deployment dialog answered (v2)
    RESET_DEPLOYMENT,         ///< "resetDeployment": Reset deployment (v2)
    SELECT_SEPARATION,        ///< "selectSeparation": the separation dialog answered (v2)
    RESET_SEPARATION,         ///< "resetSeparation": Reset separation (v2)
};

/// The "step" name of @p kind ("new", "editPoint", "selectCells", ...).
[[nodiscard]] std::string_view guiStepName(GuiStepKind kind) noexcept;

/// Whether @p kind is a step of the Motors & Configuration tab (CONFIG_TAB to RESET_SEPARATION).
[[nodiscard]] bool isGuiTabStep(GuiStepKind kind) noexcept;

/// Whether @p kind is an operation of the tab: a step of the tab but CONFIG_TAB and
/// SELECT_CELLS (a click on a sub-tab or on cells changes nothing in the design).
[[nodiscard]] bool isGuiTabOperation(GuiStepKind kind) noexcept;

/// A number typed into a field of a question: the text and the unit chosen in the field's unit
/// selector (for a field without one, the unit it shows: "s" for the delays).
struct GuiTypedValue
{
    std::string text;
    std::string unit;

    friend bool operator==(const GuiTypedValue&, const GuiTypedValue&) = default;
};

/// A motor of the motor chooser: as the table's Manufacturer column shows its manufacturer
/// ("Estes"), the motor set's designation ("D12"), and in a record the digest of the thrust curve
/// (empty in an answer, which clicks the set's row).
struct GuiMotor
{
    std::string manufacturer;
    std::string designation;
    std::string digest;

    friend bool operator==(const GuiMotor&, const GuiMotor&) = default;
};

/// The question an operation of the tab asks.
enum class GuiQuestionKind
{
    MOTOR,       ///< "motor": the motor chooser
    IGNITION,    ///< "ignition": IgnitionSelectionDialog
    DEPLOYMENT,  ///< "deployment": DeploymentSelectionDialog
    SEPARATION,  ///< "separation": SeparationSelectionDialog
    RENAME,      ///< "rename": RenameConfigDialog
    MESSAGE,     ///< "message": the message of the rename dialog's "Save as default"
};

/// The "question" name of @p kind ("motor", "ignition", ...).
[[nodiscard]] std::string_view guiQuestionName(GuiQuestionKind kind) noexcept;

/// The answer to the question of a step of the tab (its "answer" member), with the members of its
/// question (the others keep their defaults), applied by the GUI in this order. Every answer has
/// a button.
struct GuiAnswer
{
    /// "ok" or "cancel"; the rename dialog also "reset".
    std::string button;
    /// The value dialogs (ignition, deployment, separation), OK: the radio button, "all" ("Change
    /// all configurations using the default ... event") or "this" ("Override for the ... flight
    /// configuration only"), the event (the Java enum constant's name), the altitude (deployment
    /// and separation, exactly for the events whose altitude field is enabled: ALTITUDE,
    /// ALTITUDE_ASCENDING, ALTITUDE_DESCENDING) and the delay. A Cancel has none of them.
    std::optional<std::string>   scope;
    std::optional<std::string>   event;
    std::optional<GuiTypedValue> altitude;
    std::optional<GuiTypedValue> delay;
    /// The rename dialog: the text typed into the name field, and whether "Save as default" is
    /// clicked after it (its message answered OK).
    std::optional<std::string> name;
    bool                       saveAsDefault{false};
    /// The motor chooser: the search field's text, a click on the row of a motor set (which the
    /// search and the filters must show), the thrust curve chosen in the curve box (its digest),
    /// the delay box's item or a number of seconds typed into it ("5", "Plugged": the chooser's
    /// "delay" member, a text) and the nozzle exit diameter.
    std::optional<std::string>   search;
    std::optional<GuiMotor>      motor;
    std::optional<std::string>   curve;
    std::optional<std::string>   chooserDelay;
    std::optional<GuiTypedValue> nozzleExitDiameter;
};

/// A click on a cell of the current sub-tab's table (selectCells).
struct GuiCell
{
    /// The configuration's place among the rocket's (0 the first).
    int row{0};
    /// The component of the column; nullopt for column 0 (the configuration names).
    std::optional<GuiComponentPath> column;
    /// The key held: "ctrl" (toggles the cell), "shift" (extends the selection to it), or ""
    /// (a plain click).
    std::string with;
};

/// One step of the scenario, with the members its kind takes (the others keep their defaults).
struct GuiScenarioStep
{
    std::size_t index{0};
    GuiStepKind kind{GuiStepKind::NEW};
    std::string note;
    /// select: the rows clicked.
    std::vector<GuiComponentPath> paths;
    /// add: the Java class of the button ("NoseCone").
    std::string component;
    /// type, choose, check: the Java property name of the dialog field ("Length"; the Automatic
    /// check box beside a spinner is "<property>Automatic").
    std::string field;
    /// type, editPoint: the text typed.
    std::string text;
    /// type, editPoint: the unit chosen in the field's unit selector before typing (the unit of
    /// the table's column for editPoint), none for a field without one.
    std::optional<std::string> unit;
    /// choose: the item, the name of a Java enum constant or of a material.
    std::string item;
    /// check, mountCheck: the state the check box gets.
    bool checked{false};
    /// editPoint, insertPoint: the row of the fin point table.
    int row{0};
    /// editPoint: "X" or "Y".
    std::string column;
    /// button: the button.
    std::string button;
    /// chooseFile: the file name typed; open: the design file, a path relative to the
    /// repository's root (guiRepositoryFile()).
    std::string file;
    /// tab: "design" or "flightConfiguration"; configTab: "motors", "recovery" or "stages".
    std::string tab;
    /// selectCells: the cells clicked, in order (at least one).
    std::vector<GuiCell> cells;
    /// mountCheck: the motor mount whose check box is set.
    GuiComponentPath mount;
    /// The answer to the step's question: required for renameConfiguration, selectMotor,
    /// selectIgnition, selectDeployment and selectSeparation, left out for a newConfiguration
    /// that asks nothing (New on a design without motor mounts, say).
    std::optional<GuiAnswer> answer;
};

/// The scenario of scenario.json.
struct GuiScenario
{
    /// 1 or 2.
    int                          schemaVersion{1};
    std::string                  title;
    std::vector<GuiScenarioStep> steps;
};

/// Parses scenario.json of either version: its schema, every step a step of its version with
/// exactly the members its kind takes (a "note" is allowed everywhere), each of the right type
/// (an answer with the members of its question), the first step File > New (version 1) or File >
/// Open (version 2, and only there) (ErrorCode::PARSE otherwise).
[[nodiscard]] Result<GuiScenario> parseGuiScenario(std::string_view text);

/// The state of the Undo or Redo action: its text ("Undo (Add Nose Cone)") and whether it is
/// enabled.
struct GuiActionState
{
    std::string text;
    bool        enabled{false};

    friend bool operator==(const GuiActionState&, const GuiActionState&) = default;
};

/// What the motor chooser showed: the search text, the number of rows of its table, the selected
/// thrust curve (nullopt for none), the delay box's text (nullopt for none), the delay and the
/// nozzle exit diameter (in s and m, Java's Double.toString: "Infinity" for Plugged).
struct GuiChooserState
{
    std::string                search;
    int                        rows{0};
    std::optional<GuiMotor>    motor;
    std::optional<std::string> delayText;
    std::string                delay;
    std::string                nozzleExitDiameter;

    friend bool operator==(const GuiChooserState&, const GuiChooserState&) = default;
};

/// A question as it opened, with the members of its kind (the others keep their defaults). Every
/// number is a text, as Java 17's Double.toString writes it.
struct GuiQuestion
{
    GuiQuestionKind kind{GuiQuestionKind::MOTOR};
    /// The dialog's title.
    std::string title;
    /// The value dialogs: the radio button selected ("all" or "this"), the texts of the two radio
    /// buttons, the event (a Java enum constant's name) and the delay (in s).
    std::string scope;
    std::string defaultText;
    std::string overrideText;
    std::string event;
    std::string delay;
    /// Deployment and separation: the altitude (in m), whether its field is enabled, and the
    /// event combo box's items.
    std::string              altitude;
    bool                     altitudeEnabled{false};
    std::vector<std::string> events;
    /// Rename: the name field's text (the configuration's raw name).
    std::string name;
    /// Message: its text.
    std::string text;
    /// The motor chooser as it opened, and as the answer left it (before its button).
    GuiChooserState shown;
    GuiChooserState answered;
};

/// A motor cell as the table's renderer draws it: the motor's text ("4× H148R-0", "None"), the
/// ignition's ("Default (Automatic)", "Burnout + 2 s") and whether the ignition is shaded.
struct GuiMotorCell
{
    std::string motor;
    std::string ignition;
    bool        ignitionDefault{false};

    friend bool operator==(const GuiMotorCell&, const GuiMotorCell&) = default;
};

/// A row of a table of the tab: column 0's text (the configuration's name) and the cells of the
/// other columns: motorCells in the Motors table, cells (their texts) in the Recovery and Stages
/// tables.
struct GuiTableRow
{
    std::string               configuration;
    std::vector<std::string>  cells;
    std::vector<GuiMotorCell> motorCells;
};

/// A row of the motor mount table: the component, its text and its check box.
struct GuiMountRow
{
    GuiComponentPath component;
    std::string      text;
    bool             motorMount{false};
};

/// One of the tab's three sub-panels: its buttons' enabled states by their Java field names
/// ("selectMotorButton"), the column headers, the rows, the selected rows and columns (in
/// order), and for the Motors sub-panel whether it shows its help text instead of the table and
/// the motor mount table.
struct GuiConfigurationTable
{
    std::map<std::string, bool, std::less<>> buttons;
    std::vector<std::string>                 header;
    std::vector<GuiTableRow>                 rows;
    std::vector<int>                         selectedRows;
    std::vector<int>                         selectedColumns;
    bool                                     showsHelp{false};
    std::vector<GuiMountRow>                 mounts;
};

/// A sub-tab of the tab: its name ("motors"), its title and whether it is enabled.
struct GuiConfigTabState
{
    std::string tab;
    std::string title;
    bool        enabled{false};
};

/// A simulation of the document: its name and its configuration as a row (-1 for one the rocket
/// does not have).
struct GuiSimulationRow
{
    std::string name;
    int         configuration{-1};
};

/// What the Motors & Configuration tab showed after a step (version 2).
struct GuiFlightConfigurationState
{
    /// The current sub-tab, and every sub-tab.
    std::string                    configTab;
    std::vector<GuiConfigTabState> configTabs;
    /// New, Rename, Remove and Duplicate by their Java field names ("newConfButton").
    std::map<std::string, bool, std::less<>> buttons;
    /// The rocket's selected configuration as a row, -1 for the default.
    int                           selectedConfiguration{-1};
    std::vector<GuiSimulationRow> simulations;
    GuiConfigurationTable         motors;
    GuiConfigurationTable         recovery;
    GuiConfigurationTable         stages;
};

/// What a step left in OpenRocket's GUI.
struct GuiStepRecord
{
    std::size_t    index{0};
    GuiStepKind    kind{GuiStepKind::NEW};
    GuiActionState undo;
    GuiActionState redo;
    /// OpenRocketDocument::isSaved().
    bool saved{false};
    /// The selected components, in the selection's order.
    std::vector<GuiComponentPath> selection;
    /// The open window: the panel class of the configuration dialog ("NoseConeConfig",
    /// "SaveDesignInfoPanel") or "file chooser"; nullopt when none is open.
    std::optional<std::string> window;
    /// The component of the open dialog.
    std::optional<GuiComponentPath> windowComponent;
    /// guiTreeSha256() of the component tree's lines.
    std::string treeSha256;
    /// The lines themselves, after the steps that end an action (new, open, ok, button, undo,
    /// redo, paste, chooseFile, and every operation of the tab): describeRocket() of
    /// ComponentHandlerTestSupport.h with no known id.
    std::optional<std::vector<std::string>> tree;
    /// Version 2: the window's tab ("design" or "flightConfiguration"), the questions the step
    /// asked in order (each as it opened), and what the tab showed.
    std::string                                tab;
    std::vector<GuiQuestion>                   questions;
    std::optional<GuiFlightConfigurationState> flightConfiguration;
};

/// What OpenRocket's start-up loaded (version 2): motor sets, motors and component presets.
struct GuiStartup
{
    int motorSets{0};
    int motors{0};
    int presets{0};
};

/// The reference of state.json.
struct GuiReference
{
    /// 1 or 2.
    int         schemaVersion{1};
    std::string openRocketCommit;
    std::string openRocketVersion;
    std::string javaVersion;
    bool        dirty{false};
    /// Version 2: the commit of OpenRocket's preset submodule, and what the start-up loaded.
    std::string presetDatabaseCommit;
    GuiStartup  startup;
    /// The scenario the reference was made of: its file name, the SHA-256 of its bytes, its title
    /// and its number of steps.
    std::string scenarioFile;
    std::string scenarioSha256;
    std::string scenarioTitle;
    std::size_t scenarioSteps{0};
    /// Version 2: the design file the open step names and the SHA-256 of its bytes.
    std::string inputFile;
    std::string inputSha256;
    /// The design: the reference file ("rocket.ork", the document of the archive OpenRocket
    /// saved), the name the scenario saved it as, and the entries of that archive; version 2 also
    /// the SHA-256 of each entry.
    std::string                                     referenceFile;
    std::string                                     savedAs;
    std::vector<std::string>                        archiveEntries;
    std::map<std::string, std::string, std::less<>> archiveSha256;
    std::vector<GuiStepRecord>                      steps;
    /// The saved file as OpenRocket loads it: designFileState() of DesignFileState.h with
    /// StateDetail::WITH_DOCUMENT_PREFERENCES, and massAndLength() of a second load as
    /// "<name>=<value>".
    std::vector<std::string> loadedLines;
    std::vector<std::string> loadedNumbers;
};

/// Parses state.json of either version (ErrorCode::PARSE when its schema or a member is wrong).
[[nodiscard]] Result<GuiReference> parseGuiReference(std::string_view text);

/// The digest of a record's tree: the SHA-256 of @p lines, each followed by a line feed.
[[nodiscard]] std::string guiTreeSha256(std::span<const std::string> lines);

/// A line of a component tree (describeRocket() with no known id) that a save and a reload of a
/// scenario's design change: in each line that starts with @p line and holds @p saved, @p saved
/// becomes @p reloaded, in @p times lines.
struct GuiReloadChange
{
    std::string_view line;
    std::string_view saved;
    std::string_view reloaded;
    std::size_t      times{1};
};

/// What the save and the reload of the from-scratch scenario's design change, in OpenRocket as
/// in QtRocket (from_scratch_reference_golden_tests.cpp for OpenRocket's save, the GUI's exit
/// test for QtRocket's):
/// - the inner tube, a motor mount, saves its default ignition (<ignitionevent> of <motormount>),
///   and the loader sets it with MotorConfiguration.setIgnitionEvent(), which marks it an
///   override (MotorMountHandler.java:101-115, MotorConfiguration.java:198-205).
inline constexpr std::array kGuiSaveAndReloadChanges{
    GuiReloadChange{.line     = "      InnerTube 'Inner Tube' ",
                    .saved    = " ign=AUTOMATIC:0.0:false",
                    .reloaded = " ign=AUTOMATIC:0.0:true"},
};

/// What the save and the reload of the flight-configurations scenario's design change
/// (flight_configurations_reference_golden_tests.cpp): a configuration whose name is the default
/// name of the preferences at the save, "Flight {motors}" (the scenario's Save as default stored
/// it; its Reset to default and its last New gave it to two configurations), is saved without
/// its <name> (RocketSaver.java:85 writes it only when FlightConfiguration.isNameOverridden(),
/// FlightConfiguration.java:593-594: the name differs from getDefaultName()), so the reload
/// gives both the default name of the loading preferences, OpenRocket's "[{motors}]". The same in
/// QtRocket (AssemblySaver.cpp's FlightConfiguration::isNameOverridden(preferences)), whose GUI
/// saves with its own preferences, which then hold the name Save as default stored.
inline constexpr std::array kGuiFlightConfigurationsReloadChanges{
    GuiReloadChange{.line     = "config ",
                    .saved    = " name='Flight {motors}' ",
                    .reloaded = " name='[{motors}]' ",
                    .times    = 2},
};

/// What the save and the reload of the multi-stage scenario's design change: nothing (its
/// motor mounts' default ignitions are overrides already, OpenRocket's load of the example marked
/// them).
inline constexpr std::array<GuiReloadChange, 0> kGuiMultiStageReloadChanges{};

/// The changes of the scenario @p scenario ("from-scratch", "flight-configurations",
/// "multi-stage"); none for another name.
[[nodiscard]] std::span<const GuiReloadChange> guiReloadChanges(std::string_view scenario) noexcept;

/// @p tree, the tree of the design as it was saved, as its reload gives it: with each of
/// @p changes made exactly its number of times. A change made another number of times is a line
/// of @p problems.
[[nodiscard]] std::vector<std::string> guiReloadedTree(std::vector<std::string>         tree,
                                                       std::span<const GuiReloadChange> changes,
                                                       std::string&                     problems);

/// guiReloadedTree() with the changes of the from-scratch scenario (kGuiSaveAndReloadChanges).
[[nodiscard]] std::vector<std::string> guiReloadedTree(std::vector<std::string> tree,
                                                       std::string&             problems);

/// A member of a recorded question in which QtRocket's GUI shows something else than OpenRocket's,
/// on purpose: QtRocket fixes the OpenRocket bug @p bug there (plan section 10.3, the user's
/// U-M3-5). The tool records OpenRocket's value (tools/openrocket-gui-reference/README.md,
/// "QtRocket's fixes of OpenRocket's bugs"); the GUI's exit test expects QtRocket's.
struct GuiExpectedDifference
{
    /// The scenario, the step's index and the question's place among the step's questions.
    std::string_view scenario;
    std::size_t      step{0};
    std::size_t      question{0};
    /// The member of GuiQuestion ("scope").
    std::string_view member;
    std::string_view openRocket;
    std::string_view qtRocket;
    int              bug{0};
};

/// The expected differences of the scenarios of version 2, by scenario and step. Bug 1:
/// OpenRocket's deployment and separation dialogs always start on "this configuration"
/// (FlightConfigurableParameterSet.isDefault(fcid) compares by identity and is false for a
/// configuration without an override), QtRocket's start on the radio button of the cell's state,
/// "all configurations" for a cell without an override of its own (FlightConfigurationEditing's
/// deploymentRequest() and separationRequest()). The answers always name their scope, so that
/// both GUIs end alike. The ignition questions of the scenarios all start on "this
/// configuration" in both (the cells have motor configurations of their own whose ignitions are
/// overridden): bug 2 shows in none of them.
inline constexpr std::array kGuiExpectedDifferences{
    // The parachute of the first configuration, without an override.
    GuiExpectedDifference{.scenario   = "flight-configurations",
                          .step       = 16,
                          .member     = "scope",
                          .openRocket = "this",
                          .qtRocket   = "all",
                          .bug        = 1},
    // New on the Recovery sub-tab: the drogue of the new configuration.
    GuiExpectedDifference{.scenario   = "flight-configurations",
                          .step       = 21,
                          .member     = "scope",
                          .openRocket = "this",
                          .qtRocket   = "all",
                          .bug        = 1},
    // The booster of the second configuration, whose override the default separation removed.
    GuiExpectedDifference{.scenario   = "multi-stage",
                          .step       = 10,
                          .member     = "scope",
                          .openRocket = "this",
                          .qtRocket   = "all",
                          .bug        = 1},
    // New on the Stages sub-tab: the booster of the new configuration.
    GuiExpectedDifference{.scenario   = "multi-stage",
                          .step       = 13,
                          .member     = "scope",
                          .openRocket = "this",
                          .qtRocket   = "all",
                          .bug        = 1},
};

/// The member @p member of @p question as its text ("scope", "event", "delay", "altitude",
/// "defaultText", "overrideText", "name", "text", "title"); nullopt for another name.
[[nodiscard]] std::optional<std::string> guiQuestionMember(const GuiQuestion& question,
                                                           std::string_view   member);

}  // namespace QtRocket::Test
