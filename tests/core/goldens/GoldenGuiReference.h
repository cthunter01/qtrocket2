#pragma once

// Reading the reference of the GUI's from-scratch scenario, tests/data/goldens/gui/from-scratch/
// (tools/openrocket-gui-reference, whose README.md describes the files):
// - scenario.json, the scenario: what a user does in the GUI, step by step, at the level of its
//   actions (File > New, a click on rows of the component tree, an add button, a field of a
//   configuration dialog by its Java property name, Undo, Copy, Save as ...). The same file drives
//   OpenRocket's GUI code in the tool and QtRocket's GUI in the exit test of Milestone 2;
// - rocket.ork, the design OpenRocket saved at the end of the scenario;
// - state.json, what OpenRocket's GUI had after each step (the undo and redo actions, the
//   selection, the open dialog, the component tree in the lines of describeRocket()) and the
//   saved file as OpenRocket loads it (the lines of designFileState(), the masses and the
//   length of massAndLength()).
// Test-only and Qt-free: from_scratch_reference_golden_tests.cpp checks the files against each
// other and against QtRocket's loader; the GUI's exit test drives the scenario and compares with
// the records.

#include <array>
#include <cstddef>
#include <filesystem>
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

/// What a step of the scenario does (its "step" member).
enum class GuiStepKind
{
    NEW,           ///< "new": File > New, a new design window; its sustainer is selected
    SELECT,        ///< "select": a click on the rows of paths (more than one with Ctrl)
    ADD,           ///< "add": the add button of component (a Java class name)
    EDIT,          ///< "edit": Edit > Edit (or a double click): the selection's dialog
    TYPE,          ///< "type": text typed into field, after choosing unit in its selector
    CHOOSE,        ///< "choose": item chosen in the combo box field
    CHECK,         ///< "check": the check box field set to checked
    EDIT_POINT,    ///< "editPoint": text typed into a cell of the fin point table
    INSERT_POINT,  ///< "insertPoint": row selected, "Insert point" of the table's context menu
    BUTTON,        ///< "button": a dialog button ("convertToFreeform")
    OK,            ///< "ok": the dialog's OK button
    UNDO,          ///< "undo": Edit > Undo
    REDO,          ///< "redo": Edit > Redo
    COPY,          ///< "copy": Edit > Copy
    PASTE,         ///< "paste": Edit > Paste
    SAVE_AS,       ///< "saveAs": File > Save as, which opens the design information dialog
    CHOOSE_FILE,   ///< "chooseFile": the file chooser of Save as, the name file typed
};

/// The "step" name of @p kind ("new", "editPoint", ...).
[[nodiscard]] std::string_view guiStepName(GuiStepKind kind) noexcept;

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
    /// check: the state the check box gets.
    bool checked{false};
    /// editPoint, insertPoint: the row of the fin point table.
    int row{0};
    /// editPoint: "X" or "Y".
    std::string column;
    /// button: the button.
    std::string button;
    /// chooseFile: the file name typed.
    std::string file;
};

/// The scenario of scenario.json.
struct GuiScenario
{
    std::string                  title;
    std::vector<GuiScenarioStep> steps;
};

/// Parses scenario.json: its schema, every step with exactly the members its kind takes (a
/// "note" is allowed everywhere), the first step File > New (ErrorCode::PARSE otherwise).
[[nodiscard]] Result<GuiScenario> parseGuiScenario(std::string_view text);

/// The state of the Undo or Redo action: its text ("Undo (Add Nose Cone)") and whether it is
/// enabled.
struct GuiActionState
{
    std::string text;
    bool        enabled{false};

    friend bool operator==(const GuiActionState&, const GuiActionState&) = default;
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
    /// The lines themselves, after the steps that end an action (new, ok, button, undo, redo,
    /// paste, chooseFile): describeRocket() of ComponentHandlerTestSupport.h with no known id.
    std::optional<std::vector<std::string>> tree;
};

/// The reference of state.json.
struct GuiReference
{
    std::string openRocketCommit;
    std::string openRocketVersion;
    std::string javaVersion;
    bool        dirty{false};
    /// The scenario the reference was made of: its file name, the SHA-256 of its bytes, its title
    /// and its number of steps.
    std::string scenarioFile;
    std::string scenarioSha256;
    std::string scenarioTitle;
    std::size_t scenarioSteps{0};
    /// The design: the reference file ("rocket.ork", the document of the archive OpenRocket
    /// saved), the name the scenario saved it as, and the entries of that archive.
    std::string                referenceFile;
    std::string                savedAs;
    std::vector<std::string>   archiveEntries;
    std::vector<GuiStepRecord> steps;
    /// The saved file as OpenRocket loads it: designFileState() of DesignFileState.h with
    /// StateDetail::WITH_DOCUMENT_PREFERENCES, and massAndLength() of a second load as
    /// "<name>=<value>".
    std::vector<std::string> loadedLines;
    std::vector<std::string> loadedNumbers;
};

/// Parses state.json (ErrorCode::PARSE when its schema or a member is wrong).
[[nodiscard]] Result<GuiReference> parseGuiReference(std::string_view text);

/// The digest of a record's tree: the SHA-256 of @p lines, each followed by a line feed.
[[nodiscard]] std::string guiTreeSha256(std::span<const std::string> lines);

/// A line of a component tree (describeRocket() with no known id) that a save and a reload of
/// the scenario's design change: in the line that starts with @p line, @p saved becomes
/// @p reloaded.
struct GuiReloadChange
{
    std::string_view line;
    std::string_view saved;
    std::string_view reloaded;
};

/// What the save and the reload of the scenario's design change, in OpenRocket as in QtRocket
/// (from_scratch_reference_golden_tests.cpp for OpenRocket's save, the GUI's exit test for
/// QtRocket's):
/// - the inner tube, a motor mount, saves its default ignition (<ignitionevent> of <motormount>),
///   and the loader sets it with MotorConfiguration.setIgnitionEvent(), which marks it an
///   override (MotorMountHandler.java:101-115, MotorConfiguration.java:198-205).
inline constexpr std::array kGuiSaveAndReloadChanges{
    GuiReloadChange{.line     = "      InnerTube 'Inner Tube' ",
                    .saved    = " ign=AUTOMATIC:0.0:false",
                    .reloaded = " ign=AUTOMATIC:0.0:true"},
};

/// @p tree, the tree of the design as it was saved, as its reload gives it: with the changes of
/// kGuiSaveAndReloadChanges made, each exactly once. A change made another number of times (its
/// line is not in @p tree, or more than once) is a line of @p problems.
[[nodiscard]] std::vector<std::string> guiReloadedTree(std::vector<std::string> tree,
                                                       std::string&             problems);

}  // namespace QtRocket::Test
