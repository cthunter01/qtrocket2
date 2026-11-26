#pragma once

// Reading the references of the GUI's scenarios, tests/data/goldens/gui/<scenario>/
// (tools/openrocket-gui-reference, whose README.md describes the files), of three schemas:
// - version 1, the from-scratch scenario of Milestone 2 (gui/from-scratch/): a rocket designed
//   from scratch, OpenRocket's GUI code run headless;
// - version 2, the scenarios of the Motors & Configuration tab of Milestone 3
//   (gui/flight-configurations/, gui/multi-stage/, kGuiTabScenarios): a design file opened, the
//   tab's operations and their questions, OpenRocket's real panels and dialogs run on a virtual
//   display with the bundled motors and component presets;
// - version 3, the scenario of the Flight simulations tab of Milestone 4 (gui/simulations/,
//   kGuiSimulationsScenario): the steps of version 2 and the tab's operations (New, Edit,
//   Duplicate, Delete, Copy, Cut, Paste, the column chooser), the settings dialog with the wind
//   editor, the CSV import of wind levels, the aerodynamic lookup dialog and their questions, as
//   OpenRocket's real SimulationPanel and its dialogs run them; no simulation is run.
// Each directory holds:
// - scenario.json, the scenario: what a user does in the GUI, step by step, at the level of its
//   actions (File > New or File > Open, a click on rows of the component tree, an add button, a
//   field of a configuration dialog by its Java property name, Undo, Copy, Save as, the tab's
//   buttons with the answers to their questions ...). The same file drives OpenRocket's GUI code
//   in the tool and QtRocket's GUI in the exit tests;
// - rocket.ork, the design OpenRocket saved at the end of the scenario;
// - state.json, what OpenRocket's GUI had after each step (the undo and redo actions, the
//   selection, the open dialog, the component tree in the lines of describeRocket(); in version 2
//   also the window's tab, the questions asked and what the tab showed; in version 3 also what
//   the simulations tab showed and every simulation's options) and the saved file as OpenRocket
//   loads it (the lines of designFileState(), the masses and the length of massAndLength()).
// Test-only and Qt-free: from_scratch_reference_golden_tests.cpp,
// flight_configurations_reference_golden_tests.cpp and simulations_reference_golden_tests.cpp
// check the files against each other and against QtRocket's loader; the GUI's exit tests drive
// the scenarios and compare with the records.

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

namespace QtRocket
{
class OpenRocketDocument;
}  // namespace QtRocket

namespace QtRocket::Test
{

/// tests/data/goldens/gui/from-scratch in the source tree.
[[nodiscard]] std::filesystem::path guiFromScratchDir();

/// tests/data/goldens/gui/@p scenario in the source tree.
[[nodiscard]] std::filesystem::path guiScenarioDir(std::string_view scenario);

/// The scenarios of version 2, the Motors & Configuration tab's (FlightConfigurationsExitGolden).
inline constexpr std::array<std::string_view, 2> kGuiTabScenarios{"flight-configurations",
                                                                  "multi-stage"};

/// The scenario of version 3, the Flight simulations tab's (SimulationsExitGolden).
inline constexpr std::string_view kGuiSimulationsScenario = "simulations";

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
/// scenario of version 2 has every kind but NEW, ADD, COPY, PASTE and those of the simulations
/// tab (SELECT_SIMULATIONS to COLUMN_VISIBILITY), one of version 3 every kind of version 2 and
/// those of the simulations tab.
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
    TAB,                ///< "tab": the window's tab: design, flightConfiguration, simulations (v3)
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
    SELECT_SIMULATIONS,       ///< "selectSimulations": clicks on rows of the table (v3)
    NEW_SIMULATION,           ///< "newSimulation": New simulation, its dialog answered (v3)
    EDIT_SIMULATION,          ///< "editSimulation": Edit of the selected rows, answered (v3)
    DUPLICATE_SIMULATIONS,    ///< "duplicateSimulations": Duplicate (v3)
    DELETE_SIMULATIONS,       ///< "deleteSimulations": Delete, its confirmation answered (v3)
    COPY_SIMULATIONS,         ///< "copySimulations": Copy (v3)
    CUT_SIMULATIONS,          ///< "cutSimulations": Cut, its confirmation answered (v3)
    PASTE_SIMULATIONS,        ///< "pasteSimulations": Paste (v3)
    COLUMN_VISIBILITY,        ///< "columnVisibility": a column shown or hidden (v3)
};

/// The "step" name of @p kind ("new", "editPoint", "selectCells", ...).
[[nodiscard]] std::string_view guiStepName(GuiStepKind kind) noexcept;

/// Whether @p kind is a step of the Motors & Configuration tab (CONFIG_TAB to RESET_SEPARATION).
[[nodiscard]] bool isGuiTabStep(GuiStepKind kind) noexcept;

/// Whether @p kind is an operation of the tab: a step of the tab but CONFIG_TAB and
/// SELECT_CELLS (a click on a sub-tab or on cells changes nothing in the design).
[[nodiscard]] bool isGuiTabOperation(GuiStepKind kind) noexcept;

/// Whether @p kind is a step of the Flight simulations tab (SELECT_SIMULATIONS to
/// COLUMN_VISIBILITY, version 3).
[[nodiscard]] bool isGuiSimulationStep(GuiStepKind kind) noexcept;

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

/// The question an operation of a tab asks: the first six the Motors & Configuration tab's
/// (version 2), the others the Flight simulations tab's (version 3).
enum class GuiQuestionKind
{
    MOTOR,                ///< "motor": the motor chooser
    IGNITION,             ///< "ignition": IgnitionSelectionDialog
    DEPLOYMENT,           ///< "deployment": DeploymentSelectionDialog
    SEPARATION,           ///< "separation": SeparationSelectionDialog
    RENAME,               ///< "rename": RenameConfigDialog
    MESSAGE,              ///< "message": the message of the rename dialog's "Save as default"
    SIMULATION,           ///< "simulation": the settings dialog, SimulationConfigDialog (v3)
    RUN,                  ///< "run": the run dialog, SimulationRunDialog (v3)
    CONFIRM,              ///< "confirm": a JOptionPane question (Delete, discard, overwrite; v3)
    WIND_LEVELS,          ///< "windLevels": the wind editor, MultiLevelWindEditDialog (v3)
    CSV_IMPORT_SETTINGS,  ///< "csvImportSettings": CSVImportSettingsDialog (v3)
    OPEN_FILE,            ///< "openFile": a file chooser (the wind import's, the lookup's; v3)
    LOOKUP,               ///< "lookup": the aerodynamic lookup dialog (v3)
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

// ------------------------------------------------------- the answers of version 3's dialogs
//
// The answers of the dialogs of the Flight simulations tab (README.md of the tool, "Version 3"):
// each with exactly the members its dialog takes, applied by the GUI in the order of its members
// here (an answer's edits in their order).

/// The answer to a JOptionPane question (the delete confirmation, the settings dialog's discard
/// question, the wind editor's overwrite question): its button ("ok" or "cancel", "yes" or
/// "no") and whether its "Do not ask me again" box is ticked before it.
struct GuiConfirmAnswer
{
    std::string button;
    bool        doNotAskAgain{false};

    friend bool operator==(const GuiConfirmAnswer&, const GuiConfirmAnswer&) = default;
};

/// The answer to a file chooser: "open" with the file (a path relative to the repository's root,
/// guiRepositoryFile()) selected and approved, or "cancel" (file empty).
struct GuiFileAnswer
{
    std::string button;
    std::string file;

    friend bool operator==(const GuiFileAnswer&, const GuiFileAnswer&) = default;
};

/// The rows of the CSV import settings, by their answers' names, in their order.
inline constexpr std::array<std::string_view, 4> kGuiCsvImportRows{"altitude", "speed", "direction",
                                                                   "standardDeviation"};

/// A row of the CSV import settings in an answer: the row (one of kGuiCsvImportRows), the text
/// typed into its column name field, its column index spinner's value and its unit selector's
/// unit, each left out when it is not set.
struct GuiCsvColumnAnswer
{
    std::string                row;
    std::optional<std::string> name;
    std::optional<int>         index;
    std::optional<std::string> unit;

    friend bool operator==(const GuiCsvColumnAnswer&, const GuiCsvColumnAnswer&) = default;
};

/// The answer to the CSV import settings ("Import levels" of the wind editor): the header box
/// (set to it), the rows (in the order of kGuiCsvImportRows, whatever their order in the file),
/// the separator itself ("," ";" " " for the item SPACE, a tab for the item TAB, or another
/// text typed into the box) and the button, "ok" or "cancel".
struct GuiCsvImportAnswer
{
    std::optional<bool>             hasHeader;
    std::vector<GuiCsvColumnAnswer> columns;
    std::optional<std::string>      separator;
    std::string                     button;

    friend bool operator==(const GuiCsvImportAnswer&, const GuiCsvImportAnswer&) = default;
};

/// The answers of "Import levels": the CSV import settings, the file chooser (after the
/// settings' OK) and the overwrite question (asked while the editor has levels).
struct GuiWindImportAnswer
{
    GuiCsvImportAnswer              settings;
    std::optional<GuiFileAnswer>    file;
    std::optional<GuiConfirmAnswer> overwrite;

    friend bool operator==(const GuiWindImportAnswer&, const GuiWindImportAnswer&) = default;
};

/// What an edit of the wind editor does.
enum class GuiWindEditKind
{
    ALTITUDE_REFERENCE,  ///< "altitudeReference": the radio button "MSL" or "AGL"
    ADD_LEVEL,           ///< "addLevel": "Add new level"
    DELETE_LEVEL,        ///< "deleteLevel": the row selected, then "Delete level" (bug 33)
    DELETE_ROW,          ///< "deleteRow": the row's delete button
    CELL,                ///< "cell": text typed into a field of a row, in its column's unit
    COLUMN_UNIT,         ///< "columnUnit": a unit chosen in a column header's unit selector
    RESET_LEVELS,        ///< "resetLevels": "Reset levels", its overwrite question answered
    IMPORT_LEVELS,       ///< "importLevels": "Import levels", its dialogs answered
};

/// The wind table's columns that hold numbers, in its order (MultiLevelWindTable.COLUMNS).
inline constexpr std::array<std::string_view, 5> kGuiWindColumns{"altitude", "speed", "direction",
                                                                 "standardDeviation", "turbulence"};

/// One edit of the wind editor, with the members of its kind (the others keep their defaults).
struct GuiWindEdit
{
    GuiWindEditKind kind{GuiWindEditKind::ADD_LEVEL};
    /// altitudeReference: "MSL" or "AGL".
    std::string altitudeReference;
    /// deleteLevel, deleteRow, cell: the row of the table (sorted by altitude, from 0).
    int row{0};
    /// cell, columnUnit: the column (one of kGuiWindColumns).
    std::string column;
    /// cell: the text typed; columnUnit: the unit chosen.
    std::string text;
    std::string unit;
    /// resetLevels: the overwrite question's answer.
    GuiConfirmAnswer resetQuestion;
    /// importLevels: its dialogs' answers.
    GuiWindImportAnswer importLevels;

    friend bool operator==(const GuiWindEdit&, const GuiWindEdit&) = default;
};

/// The answer to the wind editor ("Edit levels" of the multi-level wind): its edits in order, and
/// its button, "close".
struct GuiWindEditorAnswer
{
    std::vector<GuiWindEdit> edits;
    std::string              button;

    friend bool operator==(const GuiWindEditorAnswer&, const GuiWindEditorAnswer&) = default;
};

/// What an action of a section of the aerodynamic lookup dialog does.
enum class GuiLookupActionKind
{
    SEPARATOR,  ///< "separator": the section's separator box given the separator itself
    LOAD,       ///< "load": "Load from file...", its file chooser answered
    TEXT,       ///< "text": the loaded data's text area set to the text
    REFRESH,    ///< "refresh": the section's Refresh button
    CLEAR,      ///< "clear": the section's Clear button
};

/// One action of a section of the lookup dialog: the separator or the text (text), the file
/// chooser's answer (file).
struct GuiLookupAction
{
    GuiLookupActionKind kind{GuiLookupActionKind::CLEAR};
    std::string         text;
    GuiFileAnswer       file;

    friend bool operator==(const GuiLookupAction&, const GuiLookupAction&) = default;
};

/// The answer to the aerodynamic lookup dialog ("Configure..." of the options page): the actions
/// of the drag section, then of the stability section (each section's tab selected first, also
/// for an empty list; nullopt: the section is left alone), and the button, "ok" or "cancel".
struct GuiLookupAnswer
{
    std::optional<std::vector<GuiLookupAction>> drag;
    std::optional<std::vector<GuiLookupAction>> stability;
    std::string                                 button;

    friend bool operator==(const GuiLookupAnswer&, const GuiLookupAnswer&) = default;
};

/// What an edit of the settings dialog does.
enum class GuiSettingsEditKind
{
    FIELD,          ///< "field": a spinner, check box or combo box by its Java property
    NAME,           ///< "name": the name field set to the text
    CONFIGURATION,  ///< "configuration": the configuration box's item of a row
    TAB,            ///< "tab": a click on a tab of the dialog
    WIND_MODEL,     ///< "windModel": the wind model's radio button
    RANDOM_SEED,    ///< "randomSeed": "Fixed random seed" and its field
    DEFAULTS,       ///< "defaults": "Reset to default" or "Save as default" of a page
    WIND_LEVELS,    ///< "windLevels": "Edit levels", the wind editor answered
    LOOKUP,         ///< "lookup": "Configure..." of the aerodynamic data, the dialog answered
};

/// How a field of the settings dialog is set: a spinner (a number typed, after a unit chosen in
/// its unit selector), a check box or a combo box.
enum class GuiSettingsFieldKind
{
    SPINNER,
    CHECK_BOX,
    COMBO_BOX,
};

/// A field of the settings dialog that the scenarios set (README.md of the tool): the Java
/// property its DoubleModel, BooleanModel or EnumModel binds (QtRocket's objectName of the
/// field), how it is set, and for a combo box its items (the Java enum constants' names,
/// separated by blanks).
struct GuiSettingsField
{
    std::string_view     property;
    GuiSettingsFieldKind kind{GuiSettingsFieldKind::SPINNER};
    std::string_view     items;
};

/// The fields of the settings dialog's three pages that an edit may set: the launch conditions,
/// the options and the warnings.
inline constexpr std::array kGuiSettingsFields{
    GuiSettingsField{.property = "Average", .kind = GuiSettingsFieldKind::SPINNER, .items = ""},
    GuiSettingsField{
        .property = "StandardDeviation", .kind = GuiSettingsFieldKind::SPINNER, .items = ""},
    GuiSettingsField{
        .property = "TurbulenceIntensity", .kind = GuiSettingsFieldKind::SPINNER, .items = ""},
    GuiSettingsField{.property = "Direction", .kind = GuiSettingsFieldKind::SPINNER, .items = ""},
    GuiSettingsField{
        .property = "LaunchTemperature", .kind = GuiSettingsFieldKind::SPINNER, .items = ""},
    GuiSettingsField{
        .property = "LaunchPressure", .kind = GuiSettingsFieldKind::SPINNER, .items = ""},
    GuiSettingsField{
        .property = "LaunchRelativeHumidity", .kind = GuiSettingsFieldKind::SPINNER, .items = ""},
    GuiSettingsField{
        .property = "LaunchLatitude", .kind = GuiSettingsFieldKind::SPINNER, .items = ""},
    GuiSettingsField{
        .property = "LaunchLongitude", .kind = GuiSettingsFieldKind::SPINNER, .items = ""},
    GuiSettingsField{
        .property = "LaunchAltitude", .kind = GuiSettingsFieldKind::SPINNER, .items = ""},
    GuiSettingsField{
        .property = "LaunchRodLength", .kind = GuiSettingsFieldKind::SPINNER, .items = ""},
    GuiSettingsField{
        .property = "LaunchRodAngle", .kind = GuiSettingsFieldKind::SPINNER, .items = ""},
    GuiSettingsField{
        .property = "LaunchRodDirection", .kind = GuiSettingsFieldKind::SPINNER, .items = ""},
    GuiSettingsField{
        .property = "ConstantGravity", .kind = GuiSettingsFieldKind::SPINNER, .items = ""},
    GuiSettingsField{.property = "TimeStep", .kind = GuiSettingsFieldKind::SPINNER, .items = ""},
    GuiSettingsField{
        .property = "MaxSimulationTime", .kind = GuiSettingsFieldKind::SPINNER, .items = ""},
    GuiSettingsField{
        .property = "RecoverySpeedWarning", .kind = GuiSettingsFieldKind::SPINNER, .items = ""},
    GuiSettingsField{.property = "RecoveryDrogueMainLowSpeedWarning",
                     .kind     = GuiSettingsFieldKind::SPINNER,
                     .items    = ""},
    GuiSettingsField{.property = "RecoveryDrogueMainHighSpeedWarning",
                     .kind     = GuiSettingsFieldKind::SPINNER,
                     .items    = ""},
    GuiSettingsField{
        .property = "ISAAtmosphere", .kind = GuiSettingsFieldKind::CHECK_BOX, .items = ""},
    GuiSettingsField{
        .property = "LaunchIntoWind", .kind = GuiSettingsFieldKind::CHECK_BOX, .items = ""},
    GuiSettingsField{.property = "SimulationStepperMethodChoice",
                     .kind     = GuiSettingsFieldKind::COMBO_BOX,
                     .items    = "RK4 RK6"},
    GuiSettingsField{.property = "GeodeticComputation",
                     .kind     = GuiSettingsFieldKind::COMBO_BOX,
                     .items    = "FLAT SPHERICAL WGS84"},
    GuiSettingsField{.property = "GravityModelType",
                     .kind     = GuiSettingsFieldKind::COMBO_BOX,
                     .items    = "WGS CONSTANT"},
};

/// The field of kGuiSettingsFields of @p property; nullptr for none.
[[nodiscard]] const GuiSettingsField* guiSettingsField(std::string_view property) noexcept;

/// The tabs of the settings dialog by their ids, in its order (SimulationConfigDialog's
/// LAUNCH_CONDITIONS_IDX to EXPORT_IDX); an edit selects one of the first three.
inline constexpr std::array<std::string_view, 5> kGuiSettingsTabs{
    "launchConditions", "simulationOptions", "warnings", "plot", "export"};

/// One edit of the settings dialog, with the members of its kind (the others keep their
/// defaults).
struct GuiSettingsEdit
{
    GuiSettingsEditKind kind{GuiSettingsEditKind::NAME};
    /// field: the Java property (kGuiSettingsFields), and exactly the members of its field's
    /// kind: a spinner's text and the unit chosen in its unit selector (nullopt for a field
    /// without one), a check box's state, a combo box's item (a Java enum constant's name).
    std::string                field;
    std::optional<std::string> text;
    std::optional<std::string> unit;
    std::optional<bool>        checked;
    std::optional<std::string> item;
    /// name: the text the name field is set to (not empty).
    std::string name;
    /// configuration: the configuration's place among the rocket's (0 the first).
    int configuration{0};
    /// tab: one of the first three of kGuiSettingsTabs; defaults: the page of the button, the
    /// same ids.
    std::string tab;
    /// windModel: "AVERAGE" or "MULTI_LEVEL".
    std::string windModel;
    /// randomSeed: whether the seed is fixed, and the seed typed into its field (exactly for a
    /// fixed seed).
    bool                       seedFixed{false};
    std::optional<std::string> seedText;
    /// defaults: "reset" or "save".
    std::string defaults;
    /// windLevels: the wind editor's answer.
    GuiWindEditorAnswer windLevels;
    /// lookup: the lookup dialog's answer.
    GuiLookupAnswer lookup;

    friend bool operator==(const GuiSettingsEdit&, const GuiSettingsEdit&) = default;
};

/// The answer to the settings dialog (New and Edit simulation): its edits in order, its button
/// ("ok" or "cancel"), the run dialog's button exactly when OK runs simulations ("cancel", the
/// only answer: no simulation runs), and the discard question's answer exactly when Cancel asks
/// it ("yes", No being refused).
struct GuiSettingsAnswer
{
    std::vector<GuiSettingsEdit>    edits;
    std::string                     button;
    std::optional<std::string>      run;
    std::optional<GuiConfirmAnswer> discard;

    friend bool operator==(const GuiSettingsAnswer&, const GuiSettingsAnswer&) = default;
};

/// A click on a row of the simulations table (selectSimulations): the simulation's place in the
/// document (the table is never sorted by a scenario) and the key held, "ctrl" (toggles the row),
/// "shift" (extends the selection to it) or "" (a plain click).
struct GuiRowClick
{
    int         row{0};
    std::string with;

    friend bool operator==(const GuiRowClick&, const GuiRowClick&) = default;
};

/// The ids of the simulations table's columns, in its model's order
/// (SimulationPanel.SIMULATION_TABLE_COLUMN_IDS): what columnVisibility and the records name.
inline constexpr std::array<std::string_view, 14> kGuiSimulationColumns{
    "status",        "warnings",           "name",
    "configuration", "simulationStepper",  "launchRodVelocity",
    "apogee",        "deploymentVelocity", "optimumCoastTime",
    "maxVelocity",   "maxAcceleration",    "timeToApogee",
    "flightTime",    "groundHitVelocity"};

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
    /// editPoint: "X" or "Y"; columnVisibility: the column's id (kGuiSimulationColumns).
    std::string column;
    /// button: the button.
    std::string button;
    /// chooseFile: the file name typed; open: the design file, a path relative to the
    /// repository's root (guiRepositoryFile()).
    std::string file;
    /// tab: "design" or "flightConfiguration" (and "simulations" in version 3); configTab:
    /// "motors", "recovery" or "stages".
    std::string tab;
    /// selectCells: the cells clicked, in order (at least one).
    std::vector<GuiCell> cells;
    /// mountCheck: the motor mount whose check box is set.
    GuiComponentPath mount;
    /// The answer to the step's question: required for renameConfiguration, selectMotor,
    /// selectIgnition, selectDeployment and selectSeparation, left out for a newConfiguration
    /// that asks nothing (New on a design without motor mounts, say).
    std::optional<GuiAnswer> answer;
    /// selectSimulations: the rows clicked, in order (none: a click below the last row, which
    /// clears the selection).
    std::vector<GuiRowClick> rows;
    /// columnVisibility: whether the column gets shown.
    bool visible{false};
    /// newSimulation, editSimulation: the answer to the settings dialog (its "answer").
    std::optional<GuiSettingsAnswer> settings;
    /// deleteSimulations, cutSimulations: the answer to the delete confirmation (its "answer"),
    /// left out once its "Do not ask me again" was ticked.
    std::optional<GuiConfirmAnswer> confirm;
};

/// The scenario of scenario.json.
struct GuiScenario
{
    /// 1, 2 or 3.
    int                          schemaVersion{1};
    std::string                  title;
    std::vector<GuiScenarioStep> steps;
};

/// Parses scenario.json of any version: its schema, every step a step of its version with
/// exactly the members its kind takes (a "note" is allowed everywhere), each of the right type
/// (an answer with the members of its question; in version 3 the answers of the settings
/// dialog, its wind editor, CSV import and lookup dialog, and the confirmations, as the tool's
/// SimulationQuestions takes them), the first step File > New (version 1) or File > Open
/// (versions 2 and 3, and only there) (ErrorCode::PARSE otherwise).
[[nodiscard]] Result<GuiScenario> parseGuiScenario(std::string_view text);

/// The files the answers of @p scenario name (the wind imports' and the lookup dialog's file
/// choosers'), in the order of the steps, paths relative to the repository's root.
[[nodiscard]] std::vector<std::string> guiScenarioAnswerFiles(const GuiScenario& scenario);

/// The kinds of the questions the answer of @p step asks, in the order they open (the settings
/// dialog, the dialogs of its edits, the run dialog or the discard question; the delete
/// confirmation): what the record of the step lists, for a step of the simulations tab; none for
/// another step.
[[nodiscard]] std::vector<GuiQuestionKind> guiSimulationStepQuestions(const GuiScenarioStep& step);

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

/// A sub-tab of the tab ("motors", its title and whether it is enabled), or a tab of the settings
/// dialog (an id of kGuiSettingsTabs, its title, whether it is enabled).
struct GuiConfigTabState
{
    std::string tab;
    std::string title;
    bool        enabled{false};

    friend bool operator==(const GuiConfigTabState&, const GuiConfigTabState&) = default;
};

/// What the settings dialog showed (version 3): its title (with "* " once the simulation
/// changed), the name field, the configuration box's item, the status label (OpenRocket's HTML),
/// the current tab's id, every tab, and the multi-simulation edit's label (nullopt in a single
/// edit).
struct GuiSettingsDialogState
{
    std::string                    title;
    std::string                    name;
    std::string                    configuration;
    std::string                    status;
    std::string                    tab;
    std::vector<GuiConfigTabState> tabs;
    std::optional<std::string>     multiSimEdit;

    friend bool operator==(const GuiSettingsDialogState&, const GuiSettingsDialogState&) = default;
};

/// What the wind editor showed (version 3): its title, the altitude reference's radio button
/// ("MSL" or "AGL") and the lines of the probe tools/openrocket-probes/wind-editor (a "HEADER"
/// line per column, a "ROW" line per level, the "MODEL" line: windTableHeaderLines() and
/// windTableRowLines() of tests/gui/dialogs/simulation/WindEditorTranscript.h).
struct GuiWindEditorState
{
    std::string              title;
    std::string              altitudeReference;
    std::vector<std::string> lines;

    friend bool operator==(const GuiWindEditorState&, const GuiWindEditorState&) = default;
};

/// A row of the CSV import settings as the dialog shows it: its column name, index and unit.
struct GuiCsvColumnState
{
    std::string name;
    int         index{0};
    std::string unit;

    friend bool operator==(const GuiCsvColumnState&, const GuiCsvColumnState&) = default;
};

/// What the CSV import settings showed (version 3): the title, the header box, the rows in the
/// order of kGuiCsvImportRows, and the separator box's item ("SPACE" and "TAB" as it shows
/// them).
struct GuiCsvImportState
{
    std::string                      title;
    bool                             hasHeader{false};
    std::array<GuiCsvColumnState, 4> columns{};
    std::string                      separator;

    friend bool operator==(const GuiCsvImportState&, const GuiCsvImportState&) = default;
};

/// A section of the aerodynamic lookup dialog as it showed it: the summary label's text and tool
/// tip (OpenRocket's HTML, the column list sorted: guiSortedLookupColumns(); nullopt for no
/// tool tip), the title of the text's border, the text, whether it is editable, the separator
/// box's item, and whether Clear and Refresh are enabled.
struct GuiLookupSectionState
{
    std::string                summary;
    std::optional<std::string> summaryToolTip;
    std::string                border;
    std::string                text;
    bool                       editable{false};
    std::string                separator;
    bool                       clearEnabled{false};
    bool                       refreshEnabled{false};

    friend bool operator==(const GuiLookupSectionState&, const GuiLookupSectionState&) = default;
};

/// What the aerodynamic lookup dialog showed (version 3): its title, the selected section
/// ("drag" or "stability") and both sections.
struct GuiLookupState
{
    std::string           title;
    std::string           tab;
    GuiLookupSectionState drag;
    GuiLookupSectionState stability;

    friend bool operator==(const GuiLookupState&, const GuiLookupState&) = default;
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
    /// Version 3. The settings dialog ("simulation") as it opened and as its answer left it
    /// (before its button).
    GuiSettingsDialogState settingsShown;
    GuiSettingsDialogState settingsAnswered;
    /// The run dialog: the names of the simulations it was to run.
    std::vector<std::string> simulations;
    /// A confirmation: the texts of its message in order, the texts of its check boxes and of its
    /// buttons.
    std::vector<std::string> message;
    std::vector<std::string> checkBoxes;
    std::vector<std::string> buttons;
    /// The wind editor as it opened and as its answer left it.
    GuiWindEditorState windShown;
    GuiWindEditorState windAnswered;
    /// The CSV import settings as they opened and as the answer left them.
    GuiCsvImportState csvShown;
    GuiCsvImportState csvAnswered;
    /// A file chooser: the file chosen (relative to the repository's root) when it was approved.
    std::optional<std::string> file;
    /// The lookup dialog as it opened and as its answer left it.
    GuiLookupState lookupShown;
    GuiLookupState lookupAnswered;
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

/// A cell of the Recovery or Stages table: its text as the table's renderer draws it ("Apogee +
/// 1 s", "Current stage motor burnout"), whether it is shaded (italic and dim), and whether the
/// configuration has an override of its own of the deployment or the separation (the state
/// behind the cell: FlightConfigurableParameterSet.containsId(fcid), QtRocket's
/// containsId()). OpenRocket's renderer never shades such a cell (bug 1:
/// FlightConfigurableParameterSet.isDefault(fcid) compares by identity), QtRocket's shades a cell
/// without an override of its own: guiExpectedRecord() makes that difference.
struct GuiValueCell
{
    std::string text;
    bool        shaded{false};
    bool        override{false};

    friend bool operator==(const GuiValueCell&, const GuiValueCell&) = default;
};

/// A row of a table of the tab: column 0's text (the configuration's name) and the cells of the
/// other columns: motorCells in the Motors table, cells in the Recovery and Stages tables.
struct GuiTableRow
{
    std::string               configuration;
    std::vector<GuiValueCell> cells;
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

/// A column of the simulations table as the view shows it: its id (kGuiSimulationColumns) and its
/// header's text.
struct GuiSimulationTableColumn
{
    std::string id;
    std::string header;

    friend bool operator==(const GuiSimulationTableColumn&,
                           const GuiSimulationTableColumn&) = default;
};

/// A row of the simulations table in the terms of the simulation-table pins
/// (tools/openrocket-gui-pins/simulation-table): the status (a Simulation.Status constant's
/// name), the text the status label gives its sorter, the status description, the warnings box
/// as text (each count and its icon as "[HIGH]", "[NORMAL]" or "[LOW]"), the critical, normal and
/// informational warning counts (-1 without data), the box's tooltip, and the text of every model
/// column from the name on (nullopt for no value).
struct GuiSimulationTableRow
{
    std::string                             status;
    std::string                             statusSortText;
    std::string                             description;
    std::string                             warnings;
    std::array<int, 3>                      warningCounts{-1, -1, -1};
    std::string                             warningsToolTip;
    std::vector<std::optional<std::string>> cells;

    friend bool operator==(const GuiSimulationTableRow&, const GuiSimulationTableRow&) = default;
};

/// What the Flight simulations tab showed after a step (version 3): whether its card shows the
/// help text (no configuration with motors), the view's columns, the rows in the document's
/// order, the selected rows, the enabled state of the panel's twelve actions by their names
/// without "Action" ("newSimulation", ..., "landingDispersion" being Milestone 7's), the names of
/// the simulations on the clipboard, and the document preference simulation.table.hiddenColumns
/// (nullopt when the document has none).
struct GuiSimulationsTabState
{
    bool                                     showsHelp{false};
    std::vector<GuiSimulationTableColumn>    columns;
    std::vector<GuiSimulationTableRow>       rows;
    std::vector<int>                         selectedRows;
    std::map<std::string, bool, std::less<>> actions;
    std::vector<std::string>                 clipboardSimulations;
    std::optional<std::string>               hiddenColumnsPreference;

    friend bool operator==(const GuiSimulationsTabState&, const GuiSimulationsTabState&) = default;
};

/// A simulation of the document after a step (version 3): its name, its configuration as a row
/// among the rocket's (-1 for one the rocket does not have), its status (getStatus()), the ten
/// lines of its options (describeConditions() of
/// tests/core/file/openrocket/ConditionsTestSupport.h, a lookup table's file inside the repository
/// written relative to it: guiRelativeLookupPaths()) and the names of its extensions.
struct GuiSimulationState
{
    std::string              name;
    int                      configuration{-1};
    std::string              status;
    std::vector<std::string> options;
    std::vector<std::string> extensions;

    friend bool operator==(const GuiSimulationState&, const GuiSimulationState&) = default;
};

/// A place where a record of version 3 is OpenRocket's with one of QtRocket's fixes, which the
/// tool made in OpenRocket's run (its member "toolFixes"): the OpenRocket bug (0 for a fix
/// without a number) and what the tool did.
struct GuiRecordedToolFix
{
    int         bug{0};
    std::string fix;

    friend bool operator==(const GuiRecordedToolFix&, const GuiRecordedToolFix&) = default;
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
    /// Version 2: the window's tab ("design" or "flightConfiguration", in version 3 also
    /// "simulations"), the questions the step asked in order (each as it opened, a dialog opened
    /// inside another after it), and what the Motors & Configuration tab showed.
    std::string                                tab;
    std::vector<GuiQuestion>                   questions;
    std::optional<GuiFlightConfigurationState> flightConfiguration;
    /// Version 3: what the Flight simulations tab showed, the document's simulations, the fixes
    /// of QtRocket the tool made in the step, and the system clipboard's text after Copy and Cut
    /// of simulations (SimulationPanel.copySimulationValues("\t"); nullopt after another step).
    std::optional<GuiSimulationsTabState> simulationsTab;
    std::vector<GuiSimulationState>       simulations;
    std::vector<GuiRecordedToolFix>       toolFixes;
    std::optional<std::string>            clipboardText;
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
    /// 1, 2 or 3.
    int         schemaVersion{1};
    std::string openRocketCommit;
    std::string openRocketVersion;
    std::string javaVersion;
    bool        dirty{false};
    /// Versions 2 and 3: the commit of OpenRocket's preset submodule, and what the start-up
    /// loaded.
    std::string presetDatabaseCommit;
    GuiStartup  startup;
    /// The scenario the reference was made of: its file name, the SHA-256 of its bytes, its title
    /// and its number of steps.
    std::string scenarioFile;
    std::string scenarioSha256;
    std::string scenarioTitle;
    std::size_t scenarioSteps{0};
    /// Versions 2 and 3: the design file the open step names and the SHA-256 of its bytes.
    std::string inputFile;
    std::string inputSha256;
    /// The design: the reference file ("rocket.ork", the document of the archive OpenRocket
    /// saved), the name the scenario saved it as, and the entries of that archive; versions 2 and
    /// 3 also the SHA-256 of each entry.
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

/// Parses state.json of any version (ErrorCode::PARSE when its schema or a member is wrong).
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

/// What the save and the reload of the simulations scenario's design change: nothing (its input
/// is the flight-configurations reference, whose reload named its configurations already, and
/// no step of the simulations tab changes the rocket).
inline constexpr std::array<GuiReloadChange, 0> kGuiSimulationsReloadChanges{};

/// The changes of the scenario @p scenario ("from-scratch", "flight-configurations",
/// "multi-stage", "simulations"); none for another name.
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
/// overridden): bug 2 shows in none of them. The other half of bug 1, the shading of the
/// Recovery and Stages tables' cells, is a rule of guiExpectedRecord() for every cell, not an
/// entry here.
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
                          .step       = 23,
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
    // The sustainer's drogue in the new configuration, which has no override of its own.
    GuiExpectedDifference{.scenario   = "multi-stage",
                          .step       = 18,
                          .member     = "scope",
                          .openRocket = "this",
                          .qtRocket   = "all",
                          .bug        = 1},
};

/// A place where a record of version 2 is not OpenRocket's own behaviour but OpenRocket's with
/// one of QtRocket's fixes, which the tool makes in OpenRocket's run
/// (tools/openrocket-gui-reference/README.md, "QtRocket's fixes of OpenRocket's bugs"), so that
/// the exit test compares QtRocket's GUI with the records as they are:
/// - bug 5: OpenRocket's New, Duplicate and the motor mount check box add no undo position; the
///   tool adds QtRocket's ("New Configuration", "Duplicate Configuration", "Modify motor mount",
///   kGuiToolUndoPositions) before them, and the record's Undo or Redo text (@p member "undo" or
///   "redo") is @p value where OpenRocket's would name the undo position before;
/// - bug 6: an undo or a redo of OpenRocket loads a copy of the rocket, which gives every motor
///   mount a new default ignition (kGuiFreshDefaultIgnition, MotorConfigurationSet.java:30-40);
///   the tool gives the mounts back the default ignitions the state had when it was stored, as
///   QtRocket keeps them, and the line of the record's tree (@p member "tree") that starts with
///   @p line holds @p value where OpenRocket's would hold kGuiFreshDefaultIgnition. The later
///   records and the save keep those defaults too (until a step sets them).
/// flight_configurations_reference_golden_tests.cpp checks that each holds in the committed
/// record and that no other record shows one of these fixes.
struct GuiToolFix
{
    std::string_view scenario;
    std::size_t      step{0};
    int              bug{0};
    /// "undo", "redo" or "tree".
    std::string_view member;
    /// "tree": the start of the line (its indentation, class and name); empty for the others.
    std::string_view line;
    std::string_view value;
};

/// The undo positions of QtRocket's fix of bug 5, which OpenRocket's operations do not add.
inline constexpr std::array<std::string_view, 3> kGuiToolUndoPositions{
    "New Configuration", "Duplicate Configuration", "Modify motor mount"};

/// A motor mount's default ignition in OpenRocket's copy of a rocket (bug 6), as a tree line has
/// it: AUTOMATIC without delay, not overridden.
inline constexpr std::string_view kGuiFreshDefaultIgnition = " ign=AUTOMATIC:0.0:false";

/// The fixes of bugs 5 and 6 in the records of the scenarios of version 2 (GuiToolFix).
inline constexpr std::array kGuiToolFixes{
    // flight-configurations: the motor mount check box of the body tube, on and off.
    GuiToolFix{.scenario = "flight-configurations",
               .step     = 2,
               .bug      = 5,
               .member   = "undo",
               .line     = "",
               .value    = "Undo (Modify motor mount)"},
    GuiToolFix{.scenario = "flight-configurations",
               .step     = 4,
               .bug      = 5,
               .member   = "undo",
               .line     = "",
               .value    = "Undo (Modify motor mount)"},
    // The undo and the redo of Reset deployment: the inner tube's default ignition, Launch + 1 s
    // since step 9.
    GuiToolFix{.scenario = "flight-configurations",
               .step     = 21,
               .bug      = 6,
               .member   = "tree",
               .line     = "      InnerTube 'Inner Tube' ",
               .value    = " ign=LAUNCH:1.0:true"},
    GuiToolFix{.scenario = "flight-configurations",
               .step     = 22,
               .bug      = 6,
               .member   = "tree",
               .line     = "      InnerTube 'Inner Tube' ",
               .value    = " ign=LAUNCH:1.0:true"},
    // Duplicate, and the undo of the Delete after it.
    GuiToolFix{.scenario = "flight-configurations",
               .step     = 31,
               .bug      = 5,
               .member   = "undo",
               .line     = "",
               .value    = "Undo (Duplicate Configuration)"},
    GuiToolFix{.scenario = "flight-configurations",
               .step     = 33,
               .bug      = 5,
               .member   = "undo",
               .line     = "",
               .value    = "Undo (Duplicate Configuration)"},
    GuiToolFix{.scenario = "flight-configurations",
               .step     = 33,
               .bug      = 6,
               .member   = "tree",
               .line     = "      InnerTube 'Inner Tube' ",
               .value    = " ign=LAUNCH:1.0:true"},
    GuiToolFix{.scenario = "flight-configurations",
               .step     = 34,
               .bug      = 6,
               .member   = "tree",
               .line     = "      InnerTube 'Inner Tube' ",
               .value    = " ign=LAUNCH:1.0:true"},
    // multi-stage: the undo of the new configuration's separation, down to New's undo position,
    // and its redo; the loaded mounts' default ignitions are overrides (the loader marks them).
    GuiToolFix{.scenario = "multi-stage",
               .step     = 14,
               .bug      = 5,
               .member   = "undo",
               .line     = "",
               .value    = "Undo (New Configuration)"},
    GuiToolFix{.scenario = "multi-stage",
               .step     = 14,
               .bug      = 6,
               .member   = "tree",
               .line     = "        InnerTube 'Sustainer Motor Mount' ",
               .value    = " ign=AUTOMATIC:0.0:true"},
    GuiToolFix{.scenario = "multi-stage",
               .step     = 14,
               .bug      = 6,
               .member   = "tree",
               .line     = "      InnerTube 'Booster motor mount' ",
               .value    = " ign=AUTOMATIC:0.0:true"},
    GuiToolFix{.scenario = "multi-stage",
               .step     = 15,
               .bug      = 6,
               .member   = "tree",
               .line     = "        InnerTube 'Sustainer Motor Mount' ",
               .value    = " ign=AUTOMATIC:0.0:true"},
    GuiToolFix{.scenario = "multi-stage",
               .step     = 15,
               .bug      = 6,
               .member   = "tree",
               .line     = "      InnerTube 'Booster motor mount' ",
               .value    = " ign=AUTOMATIC:0.0:true"},
    // The undo and the redo of the cluster made in the design tab.
    GuiToolFix{.scenario = "multi-stage",
               .step     = 30,
               .bug      = 6,
               .member   = "tree",
               .line     = "        InnerTube 'Sustainer Motor Mount' ",
               .value    = " ign=AUTOMATIC:0.0:true"},
    GuiToolFix{.scenario = "multi-stage",
               .step     = 30,
               .bug      = 6,
               .member   = "tree",
               .line     = "      InnerTube 'Booster motor mount' ",
               .value    = " ign=AUTOMATIC:0.0:true"},
    GuiToolFix{.scenario = "multi-stage",
               .step     = 31,
               .bug      = 6,
               .member   = "tree",
               .line     = "        InnerTube 'Sustainer Motor Mount' ",
               .value    = " ign=AUTOMATIC:0.0:true"},
    GuiToolFix{.scenario = "multi-stage",
               .step     = 31,
               .bug      = 6,
               .member   = "tree",
               .line     = "      InnerTube 'Booster motor mount' ",
               .value    = " ign=AUTOMATIC:0.0:true"},
};

/// A record of version 3 whose member "toolFixes" lists the fix of the OpenRocket bug @p bug (0
/// for a fix without a number): the tool's fixes of QtRocket in OpenRocket's run
/// (tools/openrocket-gui-reference/README.md, "QtRocket's fixes of OpenRocket's bugs (version
/// 3)"), where the record is OpenRocket's with the fix:
/// - bug 6: an undo (Undo, Redo, the undo of the settings dialog's Cancel) loads a copy of the
///   rocket that gives every motor mount a new default ignition; the tool gives the mounts back
///   the default ignitions of the state the undo loaded, as QtRocket keeps them;
/// - bug 32: the wind levels sorted after an altitude edit that put them out of order;
/// - bug 33: "Delete level" deletes the selected level;
/// - bug 34: the wind CSV import's separator box given the separator itself (SPACE, TAB), which
///   the import splits at.
/// simulations_reference_golden_tests.cpp checks that the records list exactly these.
struct GuiSimulationToolFix
{
    std::string_view scenario;
    std::size_t      step{0};
    int              bug{0};
};

/// The fixes of the tool in the records of the scenario of version 3 (GuiSimulationToolFix).
inline constexpr std::array kGuiSimulationToolFixes{
    // Simulation 1's wind levels: the third level's altitude edit, then Delete level of the 0 m
    // level, which leaves the sorted levels in the records ("MODEL 50.0 100.0").
    GuiSimulationToolFix{.scenario = "simulations", .step = 3, .bug = 32},
    GuiSimulationToolFix{.scenario = "simulations", .step = 3, .bug = 33},
    // The undo of the cancelled edit of Simulation 2.
    GuiSimulationToolFix{.scenario = "simulations", .step = 6, .bug = 6},
    // Undo and Redo of the Cut.
    GuiSimulationToolFix{.scenario = "simulations", .step = 19, .bug = 6},
    GuiSimulationToolFix{.scenario = "simulations", .step = 20, .bug = 6},
    // The space-separated wind import.
    GuiSimulationToolFix{.scenario = "simulations", .step = 26, .bug = 34},
    // The undo of the cancelled New simulation.
    GuiSimulationToolFix{.scenario = "simulations", .step = 29, .bug = 6},
};

/// @p lines, options lines of describeConditions() (GuiSimulationState::options), with the
/// repository's root (the parent of data/, '/' between its names, then '/') removed where it
/// follows "path:": a lookup table's file inside the repository as the records write it
/// ("path:tests/data/..."; OpenRocket and QtRocket store the absolute path).
[[nodiscard]] std::vector<std::string> guiRelativeLookupPaths(std::vector<std::string> lines);

/// The simulations of @p document as the records of version 3 have them (GuiSimulationState),
/// in its order: each one's name, its configuration's place among the rocket's ids (-1 for one
/// the rocket does not have), the name of its status (getStatus(), which works the status out and
/// may change it, as Java's does), the lines of describeConditions() with
/// guiRelativeLookupPaths(), and its extensions' names. What the exit test compares with the
/// records' "simulations", and the golden test with QtRocket's loads of the input and the save.
[[nodiscard]] std::vector<GuiSimulationState> guiSimulationStates(OpenRocketDocument& document);

/// @p text, the text or the tool tip of a lookup summary, with its column list sorted, as the
/// records have it (the tool's SimulationQuestions.sortedColumns(): OpenRocket lists a stability
/// table's columns in the order of a Set.copyOf(), which changes from one JVM to the next): the
/// names after "columns: " up to " <i>(modified)</i>", " (modified)", "</html>" or the end,
/// separated by ", ", sorted by their UTF-16 code units (Java's String.compareTo); @p text as it
/// is when it has no such list.
[[nodiscard]] std::string guiSortedLookupColumns(std::string_view text);

/// The member @p member of @p question as its text ("scope", "event", "delay", "altitude",
/// "defaultText", "overrideText", "name", "text", "title"); nullopt for another name.
[[nodiscard]] std::optional<std::string> guiQuestionMember(const GuiQuestion& question,
                                                           std::string_view   member);

/// What QtRocket's GUI is to show where OpenRocket's GUI recorded @p record, a record of the
/// scenario @p scenario: @p record with the expected differences of its step
/// (kGuiExpectedDifferences) made, each question's member set to QtRocket's value, and every cell
/// of the Recovery and Stages tables shaded exactly when its configuration has no override of its
/// own (bug 1: OpenRocket shades none; GuiValueCell). A difference whose question or member does
/// not hold OpenRocket's value, and a cell OpenRocket shaded, is a line of @p problems, and is not
/// made. A record of version 3 also gets the rules of the simulations tab:
/// - its Monte Carlo action ("landingDispersion") is dropped: Milestone 7's (U-M4-3);
/// - bug 27: OpenRocket's Paste needs simulations on the clipboard and a selected row, QtRocket's
///   is enabled while simulations are on the clipboard and a row is selected or the table is shown
///   (SimulationPanel::updateActions()); a Paste state that is not OpenRocket's rule is a line of
///   @p problems and is left as it is.
[[nodiscard]] GuiStepRecord guiExpectedRecord(std::string_view scenario, GuiStepRecord record,
                                              std::string& problems);

/// The members of versions 2 and 3 of @p record as lines, one per member and per table row ("tab:
/// ...", "question 0: motor 'Select a rocket motor'", "motors row 0: [D12-5] | D12-5 / Automatic",
/// ...): the window's tab, the questions, what the Motors & Configuration tab showed, and in
/// version 3 what the simulations tab showed ("simulationsTab row 0: ..."), every simulation
/// ("simulation 0: ...", a line per option) and the clipboard's text; in the texts of version 3
/// a backslash, a line feed and a tab are written as \\, \n and \t. The tool's fixes
/// (toolFixes) are not a line: they say where the record came from, not what a GUI shows. None
/// for a record of version 1.
[[nodiscard]] std::vector<std::string> guiRecordLines(const GuiStepRecord& record);

/// What differs between @p expected, OpenRocket's record of a step, and @p found, what another
/// GUI showed after the same step, one line each ("undo: OpenRocket ..., QtRocket ..."): the
/// texts and states of Undo and Redo, whether the design is saved, the selection, the open window
/// and the component tree (its digest; where they differ, the first line that differs when
/// @p expected has the lines, else @p found's tree), then the lines of guiRecordLines() that
/// differ (at most kGuiRecordDifferenceLines of them). Empty when they agree.
[[nodiscard]] std::vector<std::string> guiRecordDifferences(const GuiStepRecord& expected,
                                                            const GuiStepRecord& found);

/// The most lines of guiRecordLines() guiRecordDifferences() names; the others are counted.
inline constexpr std::size_t kGuiRecordDifferenceLines = 20;

}  // namespace QtRocket::Test
