# OpenRocket's GUI on the scenarios of the GUI's exit tests

`tools/openrocket-gui-reference` runs OpenRocket's own Swing GUI code (Java) on scenarios of user
actions and writes the references QtRocket's GUI is compared with: `tests/data/goldens/gui/<scenario>/`.
The data is committed; neither CMake nor CI ever needs Java.

Three schemas of scenarios:

- **Version 1** (Milestone 2, plan section 9.5, the exit test `FromScratchExitGolden`):
  `tests/data/goldens/gui/from-scratch/`, a rocket designed from scratch. OpenRocket's GUI code runs
  headless, with the dialogs replaced by the models they build, and without component presets and
  motors (see [Version 1: the design, headless](#version-1-the-design-headless)).
- **Version 2** (Milestone 3, plan section 10.5, the exit test `FlightConfigurationsExitGolden`): a
  design file opened, the Motors & Configuration tab and the design steps of version 1 but for the add
  buttons, Copy and Paste. OpenRocket's real `FlightConfigurationPanel`, its sub-panels and their dialogs
  run on a virtual display (`xvfb-run`), with the bundled motors and component presets (see
  [Version 2: the Motors & Configuration tab](#version-2-the-motors--configuration-tab)).
- **Version 3** (Milestone 4, plan section 11.6 E6, the exit test `SimulationsExitGolden`): the steps of
  version 2 and the Flight simulations tab: OpenRocket's real `SimulationPanel` and the dialogs of its
  actions (the settings dialog `SimulationConfigDialog` with the wind editor, the CSV import of wind
  levels and the aerodynamic lookup dialog, the run dialog, the confirmations), on the virtual display of
  version 2, every dialog answered from inside its modal loop; no simulation is run (see
  [Version 3: the Flight simulations tab](#version-3-the-flight-simulations-tab)).

Each scenario directory holds:

| File | What it is |
|---|---|
| `scenario.json` | The scenario, written by hand: what a user does in the GUI, step by step (see [The scenarios](#the-scenarios)). The tool reads it; QtRocket's GUI exit test drives the same steps. |
| `rocket.ork` | The design as OpenRocket's Save as saved it at the end of the scenario: the document of the archive `GeneralRocketSaver` wrote (see [Determinism](#determinism)). Generated. |
| `state.json` | What OpenRocket's GUI had after every step, and the saved file as OpenRocket loads it (see [state.json](#statejson)). Generated. |

Files of the tool:

- `generate.sh`: compiles OpenRocket's core and Swing modules and the tool, runs the tool twice per
  scenario and checks that both runs wrote the same files (see [Regenerating](#regenerating)).
- `build.gradle`, `settings.gradle`: a stand-alone Gradle project that compiles OpenRocket's `core`
  and `swing` modules from a local checkout (read only) into `build/`, with the third-party
  dependencies their `build.gradle` files name, as `tools/openrocket-goldens` does for the core, and
  copies the `orc/` files of OpenRocket's preset submodule (`core/resources-src/datafiles/openrocket-database`)
  to `datafiles/components/database/` of its resources, as OpenRocket's own build does
  (`externalComponentsCopy`), for the start-up of version 2.
- `src/main/java/info/qtrocket/guireference/`: `GuiReference` (entry point, the start-ups, the runs,
  the checks, the files), `Scenario` (reads scenario.json), `Session` (one design window and the
  steps), `DialogFields` (the fields of the configuration dialogs), `EditActions` (Edit, Copy and
  Paste), `ComponentPaths`, `RocketState` (the state lines), `SimulationLines` (the lines of a loaded
  file's simulations), `DeterministicUuids` (a copy of the class of `tools/openrocket-goldens`), for
  version 2 `FlightConfigurationTab` (the tab and its steps), `Questions` (its modal questions,
  answered inside their modal loops), `DefaultIgnitions` (QtRocket's fix of bug 6 in the undo history)
  and `Reflection`, and for version 3 `SimulationsTab` (the Flight simulations tab, its steps and its
  record) and `SimulationQuestions` (the dialogs of the tab, answered inside their modal loops).
- `src/main/resources/logback.xml`: keeps OpenRocket's log to its warnings and errors.

QtRocket reads the files through `tests/core/goldens/GoldenGuiReference.h` (versions 1, 2 and 3).
`tests/core/goldens/from_scratch_reference_golden_tests.cpp` (suite `FromScratchReferenceGolden`,
label `golden`) checks that the from-scratch state.json is the record of the committed scenario, that
the reference comes from the OpenRocket of the other goldens, and that QtRocket loads `rocket.ork`
without warnings into exactly OpenRocket's state.
`tests/core/goldens/flight_configurations_reference_golden_tests.cpp` (suite
`FlightConfigurationsReferenceGolden`, label `golden`) checks the same for the scenarios of version 2,
and moreover that each state.json is the record of its input file, that the start-up loaded QtRocket's
bundled databases, that the questions belong to their steps, that QtRocket loads the input into the
tree of the open step, that the archive held the thrust curves of the design's motors, and that the
expected differences (`kGuiExpectedDifferences`) name recorded questions, that OpenRocket's
Recovery and Stages tables shade no cell (bug 1) and their cells' overrides are those of the tree,
and that the records where the tool's fixes of bugs 5 and 6 show are those `kGuiToolFixes` lists.
`tests/core/goldens/simulations_reference_golden_tests.cpp` (suite `SimulationsReferenceGolden`, label
`golden`) checks the same for the scenario of version 3 (but the motor choosers, which it has none of), and
moreover that its input is the flight-configurations reference's save, that the scenario covers the tab and
its dialogs (plan section 11.6 E6), that no step of the simulations tab changes the component tree, that every
step of the tab changes what it is for (a cancelled one nothing), that the questions of each step are those
its answer answers in the order they open (`guiSimulationStepQuestions()`) and the file choosers chose the
answers' files, that QtRocket loads the input and the save into the simulations the open step and the last
record show (`guiSimulationStates()`), that the records' lookup files are files of the repository, that the
records of the tool's fixes are those `kGuiSimulationToolFixes` lists, and that OpenRocket's Paste follows its
rule in every record, so that `guiExpectedRecord()` makes QtRocket's (bug 27) where it differs (steps 29 to
33). The suites `GuiScenarioReaderV3` and `GuiRecordComparisonV3` there check the reader of version 3 and the
comparison of its records.

The exit tests drive QtRocket's GUI through the same steps: `tests/gui/exit/from_scratch_exit_tests.cpp`
(`FromScratchExitGolden`, the driver `FromScratchScenario`) and
`tests/gui/exit/flight_configurations_exit_tests.cpp` (`FlightConfigurationsExitGolden`, the driver
`FlightConfigurationScenario`, which answers each question of the tab in QtRocket's own dialog, inside
its modal loop, as `Questions` answers OpenRocket's) and `tests/gui/exit/simulations_exit_tests.cpp`
(`SimulationsExitGolden`, the driver `SimulationsScenario` with the questions of `SimulationsPrompts`,
which answers the settings dialog, the wind editor, the CSV import settings, the lookup dialog, the run
dialog and the confirmations as `SimulationQuestions` answers OpenRocket's; its application's executor
runs no simulation, and the file choosers are the test script's). After every step they compare what
QtRocket's GUI shows with the step's record (`guiRecordDifferences()` of `GoldenGuiReference.h`; a record
of versions 2 and 3 after `guiExpectedRecord()`, which makes the expected differences, and in version 3
the Undo item after the cancelled New simulation, which the exit test lists), and then compare the save
with `rocket.ork` and the `loaded` state.

## Regenerating

Requirements as for `tools/openrocket-goldens`: JDK 17 (`java` on the path, or `JAVA=...`; the
script refuses another version, because `Double.toString`, which writes every number of the saved
file, changed in JDK 19), a checkout of OpenRocket (`OPENROCKET_DIR`, default `../openrocket` next
to the QtRocket repository; the reference records its commit, which should be the one of
`tests/data/goldens/manifest.json`), and either network access (Maven Central and JitPack, as
OpenRocket's own build) or a Gradle cache that already holds the dependencies of OpenRocket's core
and Swing modules (building OpenRocket once fills it). OpenRocket's Gradle wrapper is used. The
scenarios of versions 2 and 3 also need `xvfb-run` and the checkout's preset submodule initialized at the
commit the checkout records (`git -C <OpenRocket checkout> submodule update --init`).

```sh
tools/openrocket-gui-reference/generate.sh [--check] [<scenario>...]
```

The scenarios are the directories of `tests/data/goldens/gui` that hold a `scenario.json`, all of them
when none is named. The script checks the checkout as `tools/openrocket-goldens/generate.sh` does (no
modified tracked file, no untracked or ignored file among `core/src/main/java`, `core/src/main/resources`,
`core/build.gradle`, `core/libs`, `swing/src/main/java`, `swing/src/main/resources` and
`swing/build.gradle`, and for a scenario of version 2 or 3 the preset submodule at its recorded commit and
without local changes; `ALLOW_DIRTY=1` overrides and state.json then records `"dirty": true`), builds
with Gradle, and runs the tool twice per scenario with a fixed locale, time zone and encoding and a
fresh `java.util.prefs` store each time (OpenRocket's preferences at their defaults; see
[Determinism](#determinism)): into the scenario's directory and into a temporary one. A scenario of
version 1 runs headless, without a display; one of version 2 or 3 on a virtual display of its own
(`xvfb-run -a`; the session's `DISPLAY` and `WAYLAND_DISPLAY` are unset, so nothing is shown on the
desktop), with a fresh home and temporary directory (OpenRocket's motor library, thrust curve and
preset directories are there). The script fails unless the two runs wrote the same `rocket.ork` and
`state.json`. A run of the from-scratch scenario takes about ten seconds once Gradle has compiled
OpenRocket (the first build takes a minute or two), one of a scenario of version 2 or 3 about twenty
(`generate.sh --check` of all four committed scenarios: about a minute).

`--check` writes nothing into the scenario directories: it runs each scenario as usual into the work
directory and compares the files with the committed ones (`<scenario>/<file>: same` or `DIFFERS`); the
exit status is 1 when one differs or is missing.

The order of the runs: a scenario of version 2 or 3 whose `open` step names a file of another scenario's
directory runs after that scenario when both run in one call, and opens the file that run has just
written (the tool's `--input`: the output directory of the other scenario, which is its own
directory, the one of `GUI_REFERENCE_OUT`, or with `--check` the work directory's). So
`flight-configurations`, which opens the from-scratch reference's `rocket.ork`, runs after
`from-scratch`, and `simulations`, which opens the flight-configurations reference's, after that one;
one call regenerates them consistently (state.json records the SHA-256 of the file it opened, see
below). A scenario named alone opens the committed file.

Every run of the tool has a time limit (`GUI_REFERENCE_TIMEOUT`, in seconds, default 600; coreutils'
`timeout`): a run that takes longer is stopped and the script fails, so that a modal dialog nobody
answers cannot keep it waiting (the tool also closes such a dialog itself and fails the step: see
[The checks](#the-checks)).

Environment: `GUI_REFERENCE_SCENARIOS=<directory>` reads the scenario directories from there instead
of `tests/data/goldens/gui` (to try a scenario before it is committed; its `open` step still names a
file of the repository, or of a scenario directory of the same call, see the order above);
`GUI_REFERENCE_OUT=<directory>` writes `<directory>/<scenario>/` instead of the scenario's
directory; `GUI_REFERENCE_WORK=<directory>` keeps the work files there (per scenario: the archives
every run saved, the second run's output, and for versions 2 and 3 the runs' preferences, home
directories and logs; a recorded run of version 2 or 3 that fails one of [the checks](#the-checks) leaves
the records up to the failed step's in `<scenario>/first-run/saves/recorded/records-of-the-failed-run.json`,
to see what the step left); `GUI_REFERENCE_TIMEOUT=<seconds>` is the time limit of a run.

Edit a `scenario.json` by hand and regenerate: `FromScratchReferenceGolden` fails while the
from-scratch state.json records another scenario (the SHA-256 of its bytes); a state.json of version 2
or 3 records its scenario and its input file the same way, which `FlightConfigurationsReferenceGolden`
(and `SimulationsReferenceGolden`) checks (`flight-configurations` opens the from-scratch reference's
`rocket.ork`, `multi-stage` a re-save of `tools/openrocket-goldens`, `simulations` the flight-configurations
reference's `rocket.ork`: regenerate them after a change of those; a call without scenario names
regenerates `from-scratch` before `flight-configurations` and that before `simulations`).

## How OpenRocket is run

### Version 1: the design, headless

- **Start-up** as `SwingStartup.runInEDT` does it, without its windows: the `GuiModule` and
  `PluginModule` injector, `startLoader()` with the component presets and the motors bypassed
  (`openrocket.bypass.presets`, `openrocket.bypass.motors`: both databases empty, as QtRocket's
  start-up of Milestone 2 has them), `loadDefaultUnits()`, `loadDefaultComponentMaterials()` (Balsa
  for fin sets, Polystyrene for nose cones) and the material databases. The exception handler
  fails the run; no look and feel is set (nothing is drawn).
- **The window** (`Session`): the parts of `BasicFrame` and `DesignPanel` that run headless, wired as
  they wire them: the component tree's `DefaultTreeSelectionModel`, the `DocumentSelectionModel`, a
  `BasicTree` with a `ComponentTreeModel` (what `ComponentTree` is, without its renderer and drag and
  drop), `ComponentAddButtons` with its `ComponentButton`s, the `UndoRedoAction`s, and the sustainer
  selected when the window opens (`BasicFrame.java:363-372`).
- **Three runs** in one JVM, each after `DeterministicUuids.reseed("gui/<scenario>")`
  (`gui/from-scratch`): a warm-up run (static initializers draw UUIDs the first time a class is used),
  a recorded run, whose records go into state.json and whose save is the reference, and a plain run
  that asks nothing between the steps. The recorded run calls every getter of every component after
  each step (the records), as a GUI's figure and tree do, and getters store what automatic dimensions
  compute; the tool fails unless both runs saved the same document, so the reference does not depend
  on when a GUI asks.
- **The load** of the saved file, for state.json: `GeneralRocketLoader` in the same JVM, so in the
  environment of the GUI (its default component materials: a new component of the loader starts with
  them, which shows in the number of change events). The tool loads both the reference and the
  archive and fails unless they give the same state.

#### What runs and what is replaced

What runs is OpenRocket's own code wherever it runs without a display. What cannot is replaced by
the calls it makes, copied line for line (each place names its source lines), but for the calls that
change only the window or the application, which the table lists:

| GUI action | What the tool runs |
|---|---|
| File > New | `OpenRocketDocumentFactory.createNewRocket()` (`BasicFrame.newAction`) and the window above |
| A click on rows of the component tree | `JTree.setSelectionPaths` on the tree, with the paths of `ComponentTreeModel.makeTreePath` |
| An add button | `ComponentButton.doClick()`: Swing's own add logic (`ComponentButton.fireActionPerformed`, `ComponentAddButtons`' body component and stage rules, the undo position "Add ...", the new component selected). It stops with a `HeadlessException` where `ComponentConfigDialog.showDialog` makes the dialog. |
| The configuration dialog opens | What `ComponentConfigDialog.showDialog` does after it made the dialog (`ComponentConfigDialog.java:275-291`): the undo position "Modify <component name>" for an existing component (none for a new one). The parts library it opens for a new component with a preset type (`AutoOpenPartsLibrary`, on by default) is taken as closed without a choice: QtRocket has no parts library before Milestone 3. |
| A field of a dialog | The model the dialog builds for that field, with the same class and arguments (`DialogFields`, each field with its source line): a spinner's `DoubleModel` gets the unit chosen in its `UnitSelector` (`setCurrentUnit`) and the typed text through its `ValueSpinnerModel.setValue(String)` (OpenRocket's `ExpressionParser`, the unit conversion, the bounds), an integer spinner its `IntegerModel` spinner model, a combo box `setSelectedItem` of its `EnumModel`, `TransitionShapeModel` or `MaterialModel` (only the model's own items), a check box its `BooleanModel` (clicked only when its state differs), an Automatic check box its `DoubleModel.getAutomaticAction()` (`putValue(SELECTED_KEY)`; refused where the panel disabled the box: `DialogFields.AutomaticBoxes` copies the rules of `NoseConeConfig`, `BodyTubeConfig` and the packed diameter of `ParachuteConfig`, `ShockCordConfig` and `MassComponentConfig`, with what they do to the component when the dialog opens), the cluster buttons of an inner tube the configuration they set, a text field what its `TextFieldListener` does on focus lost. The dialogs themselves are JDialogs, which a headless JVM cannot make, and their panels need the dialog as their parent. |
| OK | The dialog's `windowClosed` handler: the models invalidated, the component's config listeners cleared (`ComponentConfigDialog.java:73-81`) |
| Edit > Edit (or a double click) | `RocketActions.EditAction.editComponents()`, copied (`EditActions`), then the dialog as above |
| Edit > Copy, Edit > Paste | `RocketActions.CopyAction.copyComponents()` and `PasteAction.pasteComponents()` with their helpers and enabling rules, copied (`EditActions`), on OpenRocket's `OpenRocketClipboard` and `RocketActions.copyComponentsMaintainParent`: the action classes cannot be made headless (their constructors read the menu shortcut key of the toolkit, `RocketActions.KeyStrokes`, which the headless toolkit refuses). Left out: the branches for simulations and the last call, `parentFrame.selectTab(DESIGN_TAB)`. |
| Edit > Undo, Edit > Redo | OpenRocket's `UndoRedoAction`s (`actionPerformed`) |
| Convert to freeform | The button's action (`FinSetConfig.java:66-94`): the undo position "Convert fin set", `FreeformFinSet.convertFinSet`, the old dialog closed and the dialog of the freeform fin set opened as of an existing component |
| The fin point table | `FinPointTableModel.setValueAt` (`FreeformFinSetConfig.java:1000-1036`: the text in the default length unit through `UnitGroup.fromString`, `FreeformFinSet.setPoint`, the first and last rows editable in X only) and `InsertPointAction` / `insertPoint()` (`:592-607`, the midpoint after the selected row). Left out: `beginFinPointEdit()` and `commitFinPointEdit()` around `setPoint` and `addPoint` (`:1024-1029`, `:601-606`), which keep the fin editor's own edit history (its `FinPointFigure`'s, apart from the document's undo), and `updateFields()` after `setPoint` (`:1031`), which refreshes the dialog's fields and figure |
| File > Save as | `BasicFrame.saveAsAction` and `saveAsOpenRocket` (`BasicFrame.java:2167-2250`): the design information dialog when `ShowSaveRocketInfo` asks for it (on by default; the rocket selected first, its Designer, Kit name and Revision fields through `RocketConfig`'s `TextFieldListener`), the file name with `FileHelper.forceExtension(..., "ork")`, `StorageOptionChooser.verifyStorageOptions` (it asks nothing for a design without simulation data), the file type, `GeneralRocketSaver.save(file, document, progress)` as `SaveFileWorker` calls it, `setFile` and `setSaved(true)`. No preview image: `generatePreviewImage()` draws the rocket panel's figure, and QtRocket's saves carry none before Milestone 5 (decision O26). Left out: `MRUDesignFile.addFile` (`:2180-2181`, the list of recent files), `setTitle()` (`:2250`, the window's title) and `restoreFocus()` (`:2185`). The tool fails rather than overwrite a file (Save as would ask). |

The tool fails on anything a GUI would show a message box or a question for: a disabled button or
action, an Automatic check box its panel disabled (a nose cone's base diameter while no body
component follows it, say), an item a combo box does not have, an unknown field, a step of a dialog while none is open
(or the other way round), a step in a dialog that changes the tree (which closes the dialog). It
also fails on a step that sets a value (`type`, `choose`, `check`, `editPoint`, `insertPoint`) but
leaves the component tree as it was (its digest the same as after the step before): a value the
field already has would check nothing, since a GUI whose field does nothing passes it too
(`FromScratchReferenceGolden` checks the committed record for the same). The
insert-position question of the body component and stage buttons is never asked by the scenario:
it adds a body component only when the selected one is the last of its stage.

### Version 2: the Motors & Configuration tab

Everything runs on the event dispatch thread of a JVM with a (virtual) display.

- **Start-up** as `SwingStartup.runInEDT` (`:195-239`) does it, on the event dispatch thread, without
  its windows: the `GuiModule` and `PluginModule` injector; the look and feel of the preferences
  (`GUIUtil.applyLAF()`: FlatLaf Light, OpenRocket's default); the one-time question whether OpenRocket
  may look for updates on the internet answered "Decline" (`requestUpdateCheckPermission`:
  `setUpdateCheckPermission(false)`, which turns off the motor database's update check at start too;
  QtRocket has no network features); `guiModule.startLoader()` with the bundled databases:
  `MotorDatabaseInitializer` copies the bundled motor database (`initial_motors.db` and
  `metadata.json`) into the motor library directory of the run's fresh home directory, which the motor
  loader reads, and the preset loader reads `datafiles/components` (`internal/` and the submodule's
  `database/`, the 31 files of QtRocket's `data/components`); the user's thrust curve and preset
  directories (in that home directory) are empty. The tool waits for both loaders, so that no
  "Loading motors" dialog shows, then `loadDefaultUnits()`, `loadDefaultComponentMaterials()` and the
  material databases. state.json records what was loaded (`startup`: 1458 motor sets of 1588 motors,
  5228 presets). The exception handler fails the run. One JVM runs one scenario, so every scenario
  has a start-up of its own.
- **File > Open** (`Session.open`): `BasicFrame.open(file, parent)` (`:1586-1673`) with
  `OpenFileWorker.doInBackground` (the load from the file the `open` step names, then `setFile` and
  `setSaved(true)`); a load with warnings fails the tool (OpenRocket shows them in a dialog). Left out:
  `MRUDesignFile.addFile` and `closeIfReplaceable()`.
- **The window** (`Session`): as in version 1, with the Motors & Configuration tab after the design
  panel (`BasicFrame.java:251`). The window's tab is a field of the session (`tab` steps): a click on
  the Motors & Configuration tab runs `FlightConfigurationPanel.takeTheSpotlight()`, as
  `BasicFrame.stateChanged` does (the focus to the current sub-tab's table, and the design selection
  becomes its selected columns' components); one on the design tab gives the tree the focus.
- **The tab** (`FlightConfigurationTab`): OpenRocket's real `FlightConfigurationPanel` (with
  `MotorConfigurationPanel`, `RecoveryConfigurationPanel`, `SeparationConfigurationPanel` and the motor
  mount table) in a frame on the virtual display (1100 x 600), the technique of the config-tab scout's
  TabProbe and of the flight-config pins (`tools/openrocket-gui-pins`, FlightConfigurationOperationsProbe).
  The panel needs its BasicFrame (New and Duplicate look the document up in BasicFrame's list of
  frames, `findDocument`; a column of a table selects its components in the design through
  `BasicFrame.setSelectedComponents`): the tool allocates one without running its constructor (which
  builds OpenRocket's whole window), sets the three fields the panel uses (the document, the rocket and
  the window's `DocumentSelectionModel`) and registers it in `BasicFrame.frames`. Every step of the
  tab clicks OpenRocket's own buttons (`JButton.doClick`), tables (`JTable.changeSelection`, as
  BasicTableUI's mouse handler: Ctrl toggles, Shift extends) and sub-tabs (`JTabbedPane.setSelectedIndex`);
  the motor mount check box is `MotorMountTableModel.setValueAt`, which a click on it runs
  (`GUIUtil.BooleanTableClickListener`).
- **The questions** (`Questions`): the operation's modal dialog (the motor chooser, the ignition,
  deployment, separation and rename dialogs, the message of "Save as default") is answered by the
  step's `answer` from inside its own modal loop: before the operation runs, the answer is queued on
  the event dispatch thread, which the dialog's `setVisible(true)` pumps while it is open; the answer
  sets the dialog's widgets as a user does (radio buttons and buttons `doClick`, combo boxes
  `setSelectedItem` of their own items, a spinner's text committed through its model's
  `setValue(String)` after the unit chosen in its unit selector, a text field `setText`, the motor
  chooser's search field typed over one character after the other with `replaceSelection`, a
  document event each, as a user types and as QtRocket's exit test types it) and closes it with its
  button, and the operation goes on with what the dialog left. A value equal to what a field shows
  is left alone, as a user who leaves the field as it is. A step whose operation asks no question
  must have no answer, one that asks must have one; a modal dialog that opens while an operation
  runs and that no queued answer takes (a second question, a message OpenRocket shows) fails the
  step and is closed as it shows (an AWT listener of `COMPONENT_SHOWN`), so that the tool fails
  with the step's name instead of waiting in that dialog's modal loop.
- **Three runs** in one JVM as in version 1, each after `DeterministicUuids.reseed("gui/<scenario>")`
  and with OpenRocket's preferences as the start-up left them (a run stores some: the motor chooser's
  preferred thrust curves and filters, the default configuration name of "Save as default", window
  sizes); the window is closed after each run.
- **The load** of the saved file as in version 1, with the simulations described (`SimulationLines`).

#### QtRocket's fixes of OpenRocket's bugs

QtRocket fixes OpenRocket's bugs of plan section 10.3 (the user's U-M3-5 and the answers of 2026-10-26
and 2026-10-27). Where a fix shows in a scenario, the tool either makes the fix in OpenRocket's run, so
that the reference is OpenRocket with QtRocket's fix, or fails on the step, so that the scenario avoids
the bug:

| Bug | What the tool does |
|---|---|
| 1: the deployment and separation dialogs always start on "this configuration"; the Recovery and Stages tables never shade a cell | Recorded as OpenRocket shows them: the questions' `scope` (where QtRocket's start on the cell's state: `kGuiExpectedDifferences` names each) and the recovery and stage cells' shading (none), with whether each cell's configuration has an override of its own (`containsId`, the state QtRocket's shading shows: `guiExpectedRecord()` expects QtRocket's table to shade every cell without one). An answer always names its scope. |
| 2: the ignition dialog starting on "all configurations" shows the cell's own values | Recorded as OpenRocket shows them (the question's `event` and `delay`); an OK answer gives every value, so that both end alike. |
| 3: Cancel changes the design (the ignition's start values set back through the setters, the separation's live edits, the panels' loops after Cancel) | The tool fails on a cancelled question that changed the component tree ("OpenRocket's Cancel changed the design"). A Cancel answer sets no field. |
| 4: an ignition "for this configuration only", and Reset ignition, on a cell without a motor configuration of its own edit the mount's default | The tool fails on such a cell before it clicks. |
| 5: New, Duplicate and the motor mount check box add no undo position | The fix is made: the tool adds QtRocket's undo positions ("New Configuration", "Duplicate Configuration", "Modify motor mount") right before them, as the flight-config pins do. `kGuiToolFixes` of `GoldenGuiReference.h` lists the records whose Undo or Redo text shows one. |
| 6: a copy of the rocket (every state of the undo history) loses the mounts' default ignitions | The fix is made (`DefaultIgnitions`): the tool keeps beside each state of the undo history the default ignitions the rocket had when the state was stored, and gives them back to the mounts after an undo or a redo. Copy and Paste, whose copies lose them too, are not steps of version 2. `kGuiToolFixes` lists the tree lines of the undo and redo records where a default ignition other than OpenRocket's new one (`AUTOMATIC:0.0:false`) shows; the later records and the save keep it too. |
| 7: the chooser's close button keeps an earlier OK | The window's close button is never a step. |
| 8: the chooser reopened for a cell with a delay that is no standard delay of the motor shows the closest standard one | The tool fails when the chooser opens for a cell with a motor and shows another delay (a NaN delay excepted, which QtRocket opens alike). |
| 11: "NaN" (or an infinity) is taken as a delay | The tool refuses such a delay text. |
| 20: a click on a row or a curve ends on another curve of the set ("Hide very similar thrust curves") | The tool fails when the chooser shows another curve than the one the click chose (`selectMotor(set)`, the preferred or first curve, or the curve of `curve`). |
| 21: the chooser keeps a row of an earlier opening selected, and shows its motor, when the search, the filters or the mount's limits hide the configured motor | The tool fails when the chooser opens for a cell with a motor and shows another one. |
| an answer that changes the design without a change event (QtRocket's operations always fire one) | The tool fails on an operation that changed the component tree while the rocket fired no change event. |

Bugs 9, 10 and 12 to 19 concern the chooser's filters and name column, the thrust-curve plot, the parts
library and the preset loader, which no step reaches.

#### The checks

Besides the failures above and those of version 1 (a disabled button or sub-tab, a click on the tab or
sub-tab already shown, a cell or column the table does not have, a component the motor mount table
does not list or a check box already in the state asked for, an item a combo box does not have, a field
a dialog disables), the recorded run checks every step:

- a field step changes the component tree, as in version 1;
- an operation of the tab (every step of the tab but `configTab` and `selectCells`) changes the
  component tree, but for a question cancelled (`"button": "cancel"`, New excepted, whose new
  configuration is a change), which must leave it as it was; an operation that changes it fires a
  change event of the rocket;
- an operation asks no question that its answer does not take (`Questions`: a second modal dialog
  is closed and fails the step);
- the steps of the tab need the window's tab `flightConfiguration`, a `select` the design tab; the
  steps of a sub-panel (`selectMotor`, `deleteMotor`, `selectIgnition`, `resetIgnition`, `mountCheck`;
  `selectDeployment`, `resetDeployment`; `selectSeparation`, `resetSeparation`) its sub-tab;
- Save as fails where `StorageOptionChooser` would ask how to store simulated data (a simulation with
  data and storage options not set explicitly) or the saver cannot read a decal's image.

### Version 3: the Flight simulations tab

Everything runs as in version 2 (the start-up, File > Open, the window with the design tree and the Motors &
Configuration tab, the three runs, the load), and the window has OpenRocket's real Flight simulations tab:

- **The tab** (`SimulationsTab`): OpenRocket's `SimulationPanel` (its table, a `ColumnTable` with the panel's
  `SimulationTableModel` and a `ColumnTableRowSorter`, its twelve actions and its header's column chooser) in a
  frame of its own on the virtual display (1100 x 600). It is the first panel of the window, as `BasicFrame`
  makes its `SimulationPanel` before anything else (`BasicFrame.java:231-242`), and the window's
  `DocumentSelectionModel` holds the table's selection model, so that a selection of simulations clears the
  component selection and the other way round. `RocketActions` (the Edit menu) is not made: its actions select
  the window's tab through the BasicFrame, which the tool allocates without its window. The steps run the
  panel's own actions, which RocketActions calls for a selection of simulations (the simulation branches of its
  actions, `RocketActions.java:597-999`), and the tab registers the listeners of RocketActions that update them
  (the clipboard, the document's selection, the table's selection and the rocket, `RocketActions.java:144-172`).
  OpenRocket's clipboard (static, `OpenRocketClipboard`) is emptied when a run's window opens. There is no
  `RocketPanel` (its background simulation would change the document's simulations).
- **The window's tab** `simulations`: `BasicFrame.stateChanged` (`:2518-2535`), `takeTheSpotlight()` (the focus
  to the table; the previous selection or, with none, the first row selected) and `activating()` (which runs
  the outdated simulations when the preference AutoRunSimulations asks for it: off by default, and the tool
  fails when it is on); leaving the tab, `updatePreviousSelection()`.
- **The steps of the tab** use OpenRocket's own table, actions and menu: a click on a row is
  `JTable.changeSelection` (BasicTableUI's mouse handler: Ctrl toggles, Shift extends) in the name column,
  whose single click does nothing more (a click on the status column runs an outdated simulation,
  `SimulationPanel.java:327-338`); New, Edit, Duplicate, Delete, Copy, Cut and Paste are `actionPerformed`
  of the panel's enabled action; a column is shown or hidden by a click on its check box item of the header's
  popup menu (`ColumnVisibilityController.createMenu()`, a right click on the header), which the document
  preference `simulation.table.hiddenColumns` keeps.
- **The dialogs** (`SimulationQuestions`) are answered from inside their modal loops by the step's answer, as
  in version 2. An operation can open a dialog inside another (the wind editor of the settings dialog) or one
  after another (the CSV import's settings, its file chooser and the overwrite question), so the answers of the
  dialogs a click is to open are queued in their order before the click: a modal dialog that shows takes the
  next queued answer (an AWT listener of `COMPONENT_SHOWN`), which runs inside the dialog's modal loop after the
  work the dialog queued itself, and closes it with its button. Between two edits of an answer the event queue
  is flushed, as a user's next action comes after what the last one queued (OpenRocket's wind table queues the
  highlight of the rows an altitude edit moved, which fails when a row is deleted before it runs). A dialog that
  shows with no answer queued (a message OpenRocket shows, a question the step does not answer) fails the step
  and is closed as it shows; an answer no dialog took fails the step after its click; an answer that fails
  closes its dialog, so that the operation goes on, and the step fails when it is over.
- **No run.** The settings dialog's OK runs the simulations of the dialog that are not up to date
  (`SimulationConfigDialog.java:414-418`), and a simulation without data never is: the run dialog
  (`SimulationRunDialog`) is answered with Cancel before any of its simulations starts. The tool holds every
  thread of the run dialog's executor (a static pool of `availableProcessors()` threads) from before OK until
  the run dialog's answer is done, so that its simulations stay queued until Cancel cancels them; the
  simulations keep what they had (no flight data, the status of before). A fresh flight's values would be in the
  save (`<flightdata>` without branches), and they cannot be compared (CLAUDE.md: fresh flights are chaotic);
  OpenRocket bug 40 (a cancelled run's partial flight marked up to date) does not show either. Run, Plot,
  the status click and the auto-run are no steps.
- **Files.** The files an answer names (the CSV files of a wind import and of the lookup tables) are paths
  relative to the repository and must be in it (as the `open` step's). A record writes the path of a lookup
  table's file relative to the repository (`path:tests/...`; OpenRocket's lookup dialog stores it absolute).
  Save as fails while a simulation holds a lookup table's file: OpenRocket's saver writes the absolute path
  (`AerodynamicLookupDialog.normalizePath`, `OpenRocketSaver.writeCsvLookup`), which would make the reference
  depend on where the repository is (and QtRocket's loader refuses an absolute path): clear the tables a
  scenario loads before Save as.

#### QtRocket's fixes of OpenRocket's bugs (version 3)

The bugs of plan section 11.4 that the steps of version 3 meet. As in version 2, the tool either makes
QtRocket's fix in OpenRocket's run, and the step's record names it in its member `toolFixes`, or fails on the
step:

| Bug | What the tool does |
|---|---|
| 6 (Milestone 3): an undo loads a copy of the rocket, which loses the mounts' default ignitions | Fixed as in version 2 (`DefaultIgnitions`), also after the undo of the settings dialog's Cancel (`SimulationConfigDialog.discardChanges()`). A record of version 3 where a mount got its kept default back lists `{"bug": 6, ...}` |
| 23, 24, 26, 28: the Warnings column does not sort; Paste, New and Duplicate and the selection mix view rows and model rows in a sorted table; the status click and the warnings double click test view columns | No step sorts the table or clicks its status or warnings column |
| 27: no Paste into an empty list or without a selected row | Paste without a selected row is a disabled action, which the tool refuses. The records hold OpenRocket's state of the action (`pasteSimulation` false); QtRocket's Paste is enabled while simulations are on the clipboard (the record's `clipboardSimulations`) and a row is selected or the table is shown, which `guiExpectedRecord()` of `GoldenGuiReference.h` makes |
| 29: Cancel undoes the user's previous change after an edit `SimulationOptions.equals()` ignores (the geodetic method, launch into wind, the standard atmosphere, the lookup tables' files); 30: Cancel of a new simulation after an edit keeps it | The tool fails on a New or Edit whose Cancel leaves the simulations or the component tree otherwise than they were before the step: QtRocket's Cancel gives them back. It also fails on a Cancel after edits that OpenRocket's settings dialog does not see as a change (no `* ` in its title: the wind editor's, a lookup table cleared), whose Cancel closes the dialog without the discard question and keeps them |
| 31: two wind levels can get the same altitude (then both are deleted, and the saved design cannot be opened) | Refused: a cell edit that gives a level the altitude of another |
| 32: the levels are not re-sorted after an altitude edit | Fixed: the model sorted after an altitude edit that put its levels out of order (`MultiLevelPinkNoiseWindModel.sortLevels()`), `{"bug": 32, ...}` |
| 33: "Delete level" never works with the mouse (the button takes the focus, which deselects the row and disables the button) | Fixed: `deleteLevel` selects the row as a click on one of its fields does (`MultiLevelWindTable.selectRow`, the rows' `focusGained`) and clicks "Delete level", which the tool's click does without moving the focus, so that the selected level is deleted, `{"bug": 33, ...}` |
| 34: the CSV import with SPACE or TAB splits at the word and stores the word as the export separator | Fixed: the settings dialog's separator box is given the separator itself before OK, which the import then splits at and the preference stores, `{"bug": 34, ...}` (the record's `answered` shows the box's item, SPACE or TAB, as the user chose it) |
| 35: the missing key `simedtdlg.error.temperatureTooLow` (a temperature at or below -80 °C without the standard atmosphere) | Refused: such a temperature, or the standard atmosphere turned off below it |
| 36: the multi-level summary refreshes only when the wind editor closes, and its highest altitude starts at `Double.MIN_VALUE` | The summary is not recorded |
| 37: "Import levels" appends .csv to the chosen file | Refused: a chosen file without the extension .csv |
| 38: OpenRocket's new simulation takes neither the geodetic method of the Simulation preferences nor the thresholds of the Warnings preferences (QtRocket's does, and its launch conditions' Reset to default too) | Refused: New simulation and the launch conditions' Reset to default while one of those preferences is not the value of a new `SimulationOptions` ("Save as default" of the options or the warnings page changes them) |
| 40: a cancelled run's partial flight | The run is cancelled before it starts (above) |
| The wind table's delete buttons stay as they were after Reset levels or an import, so that a lone level keeps an enabled button (Milestone 4 run 2's documented difference S2) | Fixed: the rows' buttons updated as QtRocket's table shows them (enabled exactly while there is more than one level), `{"fix": "the wind table's delete buttons ..."}` without a bug number |

Bugs 22 (a pasted simulation stays a listener of its source design: the scenarios have one document), 25
(Copy's status column pairs the statuses by position: no row has data), 39 and 41 to 55 (runs, plots, exports
and the background estimate) concern no step of version 3.

#### The checks (version 3)

Besides those of version 2, the recorded run checks every step of version 3:

- a step of the simulations tab needs the window's tab `simulations`;
- `selectSimulations` changes the table's selection; New and Edit with OK change the simulations, with Cancel
  they leave the simulations and the component tree as they were before the step (bugs 29 and 30); Duplicate,
  Cut and Paste change the simulations, and so does Delete unless its confirmation was cancelled (which changes
  nothing); `columnVisibility` changes the table; no step of the tab changes the component tree but by a
  Cancel's undo, which gives it back;
- every edit of a dialog changes what it edits: a field given the value it has, a box given the state or the
  item it shows and a tab that is the current one are refused (but "Save as default", the wind editor, the
  lookup dialog, Reset levels and Import levels, whose own edits are checked);
- a field is set only when a user can set it: brought into view as a user does it (its tab, the warnings page's
  Configuration sub-tab, a collapsed section expanded), it must be shown (not on another card of its panel: the
  constant gravity with the WGS model, the average wind with the multi-level one) and enabled (the temperature
  with the standard atmosphere, the rod direction with launch into wind);
- an answer has exactly the members its dialog takes; a dialog that opens without an answer, and an answer whose
  dialog does not open, fail the step.

### Determinism

- The component and flight configuration ids come from `DeterministicUuids`, reseeded before each
  run; the warm-up run makes the recorded and the plain run start with every class initialized.
- `GeneralRocketSaver` always writes a zip archive, whose entries carry the time of the save. The
  reference keeps the document of its entry `rocket.ork` as a plain `.ork` file (the
  OpenRocketSaver XML, which QtRocket and OpenRocket both load); state.json lists the archive's
  entries (from-scratch: `["rocket.ork"]`, no decal, no motor file, no preview; version 2 also the
  thrust curves of the configured motors, `thrustcurves/<digest>.rse`, with every entry's SHA-256).
- OpenRocket's preferences start at their defaults and are never the user's: `java.util.prefs.userRoot`
  and `systemRoot` point the store at an empty directory of each run, which only Linux's
  `FileSystemPreferences` honours (macOS keeps preferences in property lists, Windows in the
  registry), so `generate.sh` also sets `openrocket.debug.prefs`: `SwingPreferences` then uses the
  node `OpenRocket-debug`, which it clears when it starts, and `TranslatorProvider` reads no locale
  from the user's node, on every system. A run with and one without the property wrote the same files.
- Version 2: OpenRocket's `Rocket` keeps its listeners in a `HashSet`, most of them hashed by identity,
  so that the order in which the tab's panels and table models see an event changes from one JVM to
  the next, and with it what the tables keep selected (a table model's `fireTableStructureChanged()`
  clears its table's selection, `FlightConfigurationPanel.stateChanged()` selects the rocket's
  configuration again: a table ends with a selection only when the panel comes second). The tool
  replaces the set with a `LinkedHashSet` before the window registers a listener (one of the orders
  OpenRocket may take) and registers the `FlightConfigurationPanel` again after its table models, the
  order QtRocket's panel connects in. The order of the preset files is the file system's
  (`DirectoryIterator`, `File.listFiles`), which nothing recorded depends on.
- Version 3: no simulation runs (see [Version 3](#version-3-the-flight-simulations-tab)). The random seeds
  OpenRocket draws (`new Random()`: a new simulation's, the run dialog's for a seed that is not fixed, the one
  "Fixed random seed" puts into its field when it is ticked) are never recorded or saved: a seed is in a record
  and in the save only while it is fixed, and the answer that ticks the box types its seed in the same edit. A
  stability lookup table lists its columns in the order of a `Set.copyOf()` (`MachAoALookup.getValueColumns()`),
  which changes from one JVM to the next: the record sorts the column list of the lookup dialog's summary.
- Nothing else depends on the time, the machine or the user: `generate.sh` compares two runs of the
  tool byte for byte.

## The scenarios

A step is an object with `"step"` (its kind), an optional `"note"` for the reader, and exactly the
members its kind takes (both readers reject others). A component path is the list of names a user
reads in the tree from the stage down (`["Sustainer", "Body Tube", "Parachute"]`), an element being a
name (the first child of that name) or `{"name": ..., "occurrence": n}` (the n-th child of that
name); `[]` is the rocket.

### Version 1

`scenario.json` is `{"schema": "gui-from-scratch-scenario", "schemaVersion": 1, "title": ...,
"steps": [...]}`:

| step | members | the GUI action |
|---|---|---|
| `new` | | File > New (the first step, and only there): a design window with a rocket and its sustainer, which is selected |
| `select` | `paths` | a click on the rows of the components (more than one: with Ctrl) |
| `add` | `component` | the add button of the component class (`NoseCone`, `BodyTube`, `TrapezoidFinSet`, `InnerTube`, `CenteringRing`, `Parachute`, `ShockCord`, `LaunchLug`, `RailButton`, `MassComponent`, ...); the new component's dialog opens |
| `edit` | | Edit > Edit (or a double click on the selected row): the dialog of the selected component |
| `type` | `field`, `text`, `unit` | the unit chosen in the field's unit selector (none for a field without one), then the text typed into the field and committed (Enter or focus lost) |
| `choose` | `field`, `item` | the item chosen in a combo box: the Java enum constant (`ELLIPSOID`, `BOTTOM`, `ALTIMETER`) or the material name (`Polycarbonate (Lexan)`) |
| `check` | `field`, `checked` | the check box set to that state (clicked when it is not in it) |
| `editPoint` | `row`, `column`, `text`, `unit` | the cell of the fin point table (`column` `X` or `Y`, `unit` the column's, the default length unit) edited |
| `insertPoint` | `row` | the row of the fin point table selected, "Insert point" of the table's context menu |
| `button` | `button` | a dialog button: `convertToFreeform` (Convert to freeform, in the dialog of a fin set) |
| `ok` | | the OK button of the open dialog |
| `undo`, `redo` | | Edit > Undo, Edit > Redo |
| `copy`, `paste` | | Edit > Copy, Edit > Paste |
| `saveAs` | | File > Save as: the design information dialog opens |
| `chooseFile` | `file` | the file chooser of Save as: the name typed (Save as adds `.ork`), Save clicked |

A field is named by the Java property its model binds (`Length`, `OuterRadius`, `MotorMount`, `AxialMethod`,
`DeployEvent`, `Name`; the dialog's object name in QtRocket); the Automatic check box beside a
spinner is `<property>Automatic` (`BaseRadiusAutomatic`). A field shows what its model shows: the
`OuterRadius` field of a body tube is its outer diameter (the model's multiplier is 2), as the
dialog labels it.

The fields the tool knows, by dialog (the panel class `ComponentConfigDialog` picks):

| dialog | fields |
|---|---|
| every component's | `Name`, `Comment` |
| `NoseConeConfig` | `ShapeType`, `ShapeParameter`, `Length`, `BaseRadius`, `Thickness`, `Filled`, `Flipped`, `Material`, `Finish` |
| `BodyTubeConfig` | `Length`, `OuterRadius`, `InnerRadius`, `Thickness`, `Filled`, `Material`, `Finish`, `MotorMount`, `MotorOverhang` |
| `TrapezoidFinSetConfig` | `FinCount`, `CantAngle`, `RootChord`, `TipChord`, `Height`, `Sweep`, `SweepAngle`, `CrossSection`, `Thickness`, `AxialMethod`, `AxialOffset`, `Material`, `Finish` |
| `FreeformFinSetConfig` | `FinCount`, `CrossSection`, `AxialMethod`, `AxialOffset`, `Material`, `Finish`, the fin point table |
| `InnerTubeConfig` | `Length`, `OuterRadius`, `InnerRadius`, `Thickness`, `AxialMethod`, `AxialOffset`, `Material`, `MotorMount`, `MotorOverhang`, `ClusterConfiguration` (`choose` its XML name: `single`, `double`, `3-row`, `3-ring`, `4-row`, `4-ring`, ...), `ClusterScale` (the relative tube separation), `ClusterRotation` |
| `CenteringRingConfig` | `Length` (labelled Thickness), `OuterRadius`, `InnerRadius`, `AxialMethod`, `AxialOffset`, `Material` |
| `ParachuteConfig` | `Diameter`, `SpillDia`, `Material`, `CD`, `LineCount`, `LineLength`, `LineMaterial`, `Length`, `Radius`, `AxialMethod`, `AxialOffset`, `DeployEvent`, `DeployDelay`, `DeployAltitude` (the last three on the default deployment configuration) |
| `ShockCordConfig` | `CordLength`, `Material`, `Length`, `Radius`, `AxialMethod`, `AxialOffset` |
| `LaunchLugConfig` | `Length`, `OuterRadius`, `InnerRadius`, `Thickness`, `InstanceCount`, `InstanceSeparation`, `AngleOffset`, `AxialMethod`, `AxialOffset`, `Material`, `Finish` |
| `RailButtonConfig` | `OuterDiameter`, `InnerDiameter`, `BaseHeight`, `FlangeHeight`, `TotalHeight`, `ScrewHeight`, `InstanceCount`, `InstanceSeparation`, `AngleOffset`, `AxialMethod`, `AxialOffset`, `Material`, `Finish` |
| `MassComponentConfig` | `MassComponentType`, `ComponentMass`, `Density`, `Length`, `Radius`, `AxialMethod`, `AxialOffset` |
| `SaveDesignInfoPanel` (Save as) | `Designer`, `KitName`, `Revision` |

To use another field, add it to `DialogFields` from the dialog's source.

The committed from-scratch scenario, 101 steps: New; the sustainer selected; a nose cone (ellipsoid,
10 cm, 0.15 cm wall, polycarbonate, smooth paint); a body tube after it (30 cm, outer diameter 4.1 cm,
0.05 cm wall); four trapezoidal fins in the tube (chords 7 and 3.5 cm, height 5.5 cm, sweep 4 cm,
2.5 mm thick, rounded); the nose cone edited again (Edit > Edit) for its automatic base diameter,
whose Automatic check box is enabled only now that the body tube follows it; an inner tube as the
motor mount (7.5 cm, diameters 2.6 and 2.4 cm, overhang 0.5 cm); two named centering rings, the fore
one placed from the top (23.7 cm), the aft one from the middle (14.35 cm; a new ring is placed from
the bottom); a parachute (35 cm, 8 lines, deployed at 150 m); a shock cord (60 cm); a launch lug
(5 cm, at 135 degrees) and two rail buttons (15 cm apart, at 90 degrees); an altimeter (a mass
component of 15 g); the fin set converted to freeform, two points edited and one inserted; Undo and
Redo of those edits; the parachute copied and pasted into the tube, the copy renamed "Drogue" and made
15 cm; Save as with a designer, file `rocket`. Every step that sets a value changes the design (see
above).

### Version 2

`scenario.json` is `{"schema": "gui-scenario", "schemaVersion": 2, "title": ..., "steps": [...]}`. The
first step is `open`, and only that one; the steps of version 1 follow but `new`, `add`, `copy` and
`paste`, and the steps of the Motors & Configuration tab:

| step | members | the GUI action |
|---|---|---|
| `open` | `file` | File > Open of the design file, a path relative to the QtRocket repository (`tests/data/goldens/gui/from-scratch/rocket.ork`); the window opens on the design tab with the sustainer selected |
| `tab` | `tab` | a click on the window's tab: `design` or `flightConfiguration` (the Motors & Configuration tab) |
| `configTab` | `tab` | a click on a sub-tab: `motors`, `recovery` or `stages` |
| `selectCells` | `cells` | clicks on cells of the current sub-tab's table, in order: each `{"row": n, "column": <path>, "with": "ctrl" \| "shift"}`, `row` the configuration's place among the rocket's (0 the first), `column` the component of the column (left out: column 0, the configuration names), `with` the key held (left out: a plain click) |
| `mountCheck` | `mount`, `checked` | the check box of the motor mount `mount` (a path) in the motor mount table set to `checked` (it must change) |
| `newConfiguration` | `answer` | New Configuration; the question it asks next (the current sub-tab's: the motor chooser, the deployment or the separation dialog, for the new row and the column then selected) answered with `answer`, left out when it asks none |
| `renameConfiguration` | `answer` | Rename Configuration (the current sub-tab's selected rows); the rename dialog answered |
| `deleteConfiguration` | | Remove Configuration |
| `duplicateConfiguration` | | Duplicate Configuration |
| `selectMotor` | `answer` | Select motor (the selected cells); the motor chooser answered |
| `deleteMotor` | | Delete motor |
| `selectIgnition` | `answer` | Select ignition; the ignition dialog answered |
| `resetIgnition` | | Reset ignition |
| `selectDeployment` | `answer` | Select deployment; the deployment dialog answered |
| `resetDeployment` | | Reset deployment |
| `selectSeparation` | `answer` | Select separation; the separation dialog answered |
| `resetSeparation` | | Reset separation |

An answer is an object with `"button"` and the members of its question; a typed number is
`{"text": "200", "unit": "m"}`, with the unit chosen in the field's unit selector, or for a field
without one the unit it shows (`"s"` for the delays):

| question | answer |
|---|---|
| the ignition dialog | `{"scope": "all" \| "this", "event": <IgnitionEvent name>, "delay": <typed>, "button": "ok"}` or `{"button": "cancel"}` |
| the deployment dialog | `{"scope", "event": <DeployEvent name>, "altitude": <typed> (only for `ALTITUDE`, the one event whose altitude field is enabled), "delay": <typed>, "button": "ok"}` or `{"button": "cancel"}` |
| the separation dialog | `{"scope", "event": <SeparationEvent name>, "altitude": <typed> (only for `ALTITUDE_ASCENDING` and `ALTITUDE_DESCENDING`), "delay": <typed>, "button": "ok"}` or `{"button": "cancel"}` |
| the rename dialog | `{"name": <the text typed into the field>, "saveAsDefault": true, "button": "ok" \| "reset" \| "cancel"}`, `name` and `saveAsDefault` optional; `saveAsDefault` clicks "Save as default" after the name, and its message "This change will only affect new flight configurations." is answered OK |
| the motor chooser | `{"search": <the search field's text>, "motor": {"manufacturer": <as the table's Manufacturer column shows it: "Estes">, "designation": <the motor set's designation: "D12">}, "curve": <the digest of a thrust curve of the curve box>, "delay": <a delay box item or a typed number of seconds: "5", "Plugged">, "nozzleExitDiameter": <typed>, "button": "ok" \| "cancel"}`, every member but `button` optional and applied in this order (`motor` is a click on that row of the table, which the search and the filters must show) |

`scope` is the radio button: `all` "Change all configurations using the default ... event", `this`
"Override for the ... flight configuration only".

Traps of OpenRocket's tab that the tool reports, and how to write around them:

- One motor chooser per window keeps its search text, filters and selected row between openings.
  Reopened for a cell with a motor that the search text of the last opening hides, it shows that
  opening's motor (bug 21): search, in the opening before, for what the next one needs.
- A cell's configured delay must be a standard delay of its motor for the chooser to reopen with it
  (bug 8): "Plugged" for a motor without a plugged delay, or a typed delay, is fine for the last
  opening of that cell.
- After an operation that fires an event, a table keeps its selected cell unless that cell is in row 0
  or column 0, where it selects the rocket's configuration in the last column instead
  (`FlightConfigurablePanel.restoreSelection`: `row <= 0 || col <= 0`); select the cell again before
  the next operation on it.
- The ignition dialog's Cancel changes a cell whose ignition is not overridden yet (bug 3), and the
  deployment dialog's Cancel the other selected cells: cancel on one cell whose ignition is overridden.
- The open design must load without warnings in OpenRocket with the bundled databases (no motor that
  is not in the database), its decals must be readable for Save as (a re-save of `tests/data/goldens`
  has no decal files beside it), and its simulations must hold no flight data (a re-save of an example,
  `tests/data/goldens/example-*/resave/rocket.ork`, has none).

The committed scenarios of version 2 (each directory's README.md tells them step by step):

- `flight-configurations`, 39 steps, on the from-scratch design: the body tube made a second motor
  mount and back, New with the chooser twice (Estes D12-5, C11-7), the nozzle exit diameter, a
  cancelled chooser, the ignition for all configurations and an override, a cancelled ignition
  question, Reset ignition on two rows, a deployment override, a cancelled deployment question,
  Reset deployment, Save as default of the rename dialog (cancelled), Undo and Redo of Reset
  deployment (a state stored before Save as default: the old default name), New on the Recovery
  sub-tab with its deployment question, the renames `{motors} flight` and
  `{manufacturers | motors}`, a Plugged delay, Reset to default, Duplicate, Delete, Undo, Redo and
  Save as.
- `multi-stage`, 35 steps, on OpenRocket's re-save of the example "Two stage high power rocket": the
  booster's separation per configuration (one at an altitude in feet) and for all configurations
  over two rows, Never, Reset separation, New on the Stages sub-tab with its separation question,
  Undo (down to New's undo position) and Redo, the deployment of an upper stage's recovery device
  for all configurations (lower stage separation, which keeps the overrides), the upper stage's
  ignition, the booster's motor of a configuration deleted, the sustainer's mount made a cluster of
  four in its configuration dialog, Undo, Redo and Save as.

Where QtRocket's fixes of OpenRocket's bugs show in them, the record is OpenRocket's with the tool's
fix (bugs 5 and 6, the records `kGuiToolFixes` of `GoldenGuiReference.h` lists), the scenario avoids
the step (bugs 2, 3, 4, 8, 20 and 21), or `guiExpectedRecord()` makes the difference that QtRocket's
GUI shows (bug 1: the scope a deployment or separation question starts on, five questions that
`kGuiExpectedDifferences` names, and the shading of every Recovery and Stages cell without an
override of its own).

### Version 3

`scenario.json` is `{"schema": "gui-scenario", "schemaVersion": 3, "title": ..., "steps": [...]}`. The first
step is `open`, and only that one; the steps of version 2 follow, the window's tab `tab` also takes
`simulations` (the Flight simulations tab), and the steps of that tab:

| step | members | the GUI action |
|---|---|---|
| `selectSimulations` | `rows` | clicks on rows of the simulations table, in order: each `{"row": n, "with": "ctrl" \| "shift"}`, `row` the simulation's place in the document (the table is never sorted), `with` the key held (left out: a plain click); `"rows": []` is a click below the last row, which clears the selection |
| `newSimulation` | `answer` | New simulation (`simpanel.but.newsimulation`): a new simulation after the last, selected, its settings dialog answered with `answer` |
| `editSimulation` | `answer` | Edit simulation of the selected rows (more than one: the multi-simulation edit, whose OK copies the first one's options to the others) |
| `duplicateSimulations` | | Duplicate: copies of the selected simulations after the last, selected |
| `deleteSimulations` | `answer` | Delete of the selected simulations; `answer` answers its confirmation (left out once its "Do not ask me again" was ticked) |
| `copySimulations` | | Copy: the selected simulations on OpenRocket's clipboard, the table's text on the system clipboard |
| `cutSimulations` | `answer` | Cut: Copy, then Delete with its confirmation (`answer`, left out as for Delete) |
| `pasteSimulations` | | Paste: copies of the clipboard's simulations after the first selected row, selected (a row must be selected: bug 27) |
| `columnVisibility` | `column`, `visible` | the check box item of the column `column` (its id: `status`, `warnings`, `name`, `configuration`, `simulationStepper`, `launchRodVelocity`, `apogee`, `deploymentVelocity`, `optimumCoastTime`, `maxVelocity`, `maxAcceleration`, `timeToApogee`, `flightTime`, `groundHitVelocity`) of the header's popup menu, which must change it to `visible` |

The steps of the tab need the window's tab `simulations`; `undo`, `redo` and `saveAs` work on every tab.

An answer has `"button"` and the members of its dialog; a typed number is `{"text": ..., "unit": ...}` as in
version 2. The dialogs, their answers and the order in which an answer's members are applied:

| dialog | answer |
|---|---|
| the settings dialog (`SimulationConfigDialog`, on its Launch conditions tab) | `{"edits": [<edit>, ...], "button": "ok" \| "cancel", "run": <the run dialog's answer>, "discard": <the discard question's answer>}`: the edits in order, then the button; `run` exactly when OK runs simulations (one that is not up to date: always one without data), `discard` exactly when Cancel asks its question (an edited existing simulation, any new one, while the preference asks it) |
| the run dialog (`SimulationRunDialog`) | `{"button": "cancel"}`, the only answer (no simulation runs) |
| the discard question (`SimulationConfigDialog.CancelOperation.*`) | `{"button": "yes", "doNotAskAgain": true}`, `doNotAskAgain` optional; No (which leaves the dialog open) is refused |
| the delete confirmation (`simpanel.dlg.lbl.DeleteSim*`) | `{"button": "ok" \| "cancel", "doNotAskAgain": true}`, `doNotAskAgain` optional (its box ticked before the button) |
| the wind editor (`MultiLevelWindEditDialog`, "Edit levels" of the multi-level wind) | `{"edits": [<wind edit>, ...], "button": "close"}` |
| the CSV import settings (`CSVImportSettingsDialog`, "Import levels") | `{"hasHeader": <the header box>, "columns": {"altitude" \| "speed" \| "direction" \| "standardDeviation": {"name": <the column name field's text>, "index": <the column index spinner's value>, "unit": <the unit selector's unit>}}, "separator": <the separator itself: ",", ";", " " (the item SPACE), a tab (the item TAB), or another text typed into the box>, "button": "ok" \| "cancel"}`, every member but `button` optional, applied in this order |
| a file chooser (the wind import's, the lookup's "Load from file...") | `{"file": <a path relative to the repository>, "button": "open"}` (the file selected and approved) or `{"button": "cancel"}` |
| the overwrite question of the wind editor (Reset levels, and Import levels while there are levels) | `{"button": "yes" \| "no"}` |
| the aerodynamic lookup dialog ("Configure..." of the options page) | `{"drag": [<lookup action>, ...], "stability": [<lookup action>, ...], "button": "ok" \| "cancel"}`: the drag section's actions, then the stability section's (each section's tab selected first), then the button |

An edit of the settings dialog has exactly one of these members (`field` with the members of its kind):

| edit | the GUI action |
|---|---|
| `{"field": <Java property>, "text": ..., "unit": ...}` | a spinner of the launch conditions, options or warnings page by the property its `DoubleModel` binds: the unit chosen in its unit selector (`unit` left out for a field without one), the text typed and committed. `Average`, `StandardDeviation`, `TurbulenceIntensity`, `Direction` (the average wind), `LaunchTemperature`, `LaunchPressure`, `LaunchRelativeHumidity`, `LaunchLatitude`, `LaunchLongitude`, `LaunchAltitude`, `LaunchRodLength`, `LaunchRodAngle`, `LaunchRodDirection`; `ConstantGravity`, `TimeStep`, `MaxSimulationTime`; `RecoverySpeedWarning`, `RecoveryDrogueMainLowSpeedWarning`, `RecoveryDrogueMainHighSpeedWarning` |
| `{"field": <Java property>, "checked": true \| false}` | a check box by its `BooleanModel`'s property: `ISAAtmosphere`, `LaunchIntoWind` |
| `{"field": <Java property>, "item": <Java enum constant>}` | a combo box by its `EnumModel`'s property: `SimulationStepperMethodChoice` (`RK4`, `RK6`), `GeodeticComputation` (`FLAT`, `SPHERICAL`, `WGS84`), `GravityModelType` (`WGS`, `CONSTANT`) |
| `{"name": <text>}` | the name field set to the text (not empty: an empty field leaves the name as it is) |
| `{"configuration": <row>}` | the configuration box: the rocket's configuration of that place among its configurations |
| `{"tab": "launchConditions" \| "simulationOptions" \| "warnings"}` | a click on the dialog's tab (the plot and export tabs need flight data, so they are disabled) |
| `{"windModel": "AVERAGE" \| "MULTI_LEVEL"}` | the wind model's radio button |
| `{"randomSeed": {"fixed": true, "text": <seed>}}` or `{"randomSeed": {"fixed": false}}` | "Fixed random seed" ticked (when it is not; it draws a random seed) and the seed typed into its field, or the box cleared |
| `{"defaults": "reset" \| "save", "panel": "launchConditions" \| "simulationOptions" \| "warnings"}` | "Reset to default" or "Save as default" of that page |
| `{"windLevels": <the wind editor's answer>}` | "Edit levels" (the multi-level wind's card must be shown) |
| `{"lookup": <the lookup dialog's answer>}` | "Configure..." of the aerodynamic data |

A field, a button or a radio button is brought into view as a user brings it there before the click: its
page's tab selected, on the warnings page its Configuration sub-tab, a collapsed section around it expanded
(the dual deployment thresholds). A field must then be shown (not on another card of its panel) and enabled.

An edit of the wind editor has exactly one of `{"altitudeReference": "MSL" \| "AGL"}` (the radio button),
`{"addLevel": true}` ("Add new level"), `{"deleteLevel": <row>}` (the row selected, as a click on one of its
fields selects it, then "Delete level": bug 33), `{"deleteRow": <row>}` (the row's delete button),
`{"cell": {"row": <row>, "column": "altitude" \| "speed" \| "direction" \| "standardDeviation" \|
"turbulence", "text": ...}}` (the text typed into the row's field, in the unit of the column's header),
`{"columnUnit": {"column": ..., "unit": ...}}` (the unit chosen in the column header's unit selector),
`{"resetLevels": <the overwrite question's answer>}` ("Reset levels") and `{"importLevels": {"settings": <the
CSV import settings' answer>, "file": <the file chooser's answer>, "overwrite": <the overwrite question's
answer>}}` ("Import levels"; `file` after the settings' OK, `overwrite` while the editor has levels). The rows
are the table's, sorted by altitude.

A lookup action has exactly one of `{"separator": <the separator itself>}` (the section's separator box, as
in the CSV import settings), `{"load": <the file chooser's answer>}` ("Load from file..."), `{"text": <text>}`
(the loaded data's text area set to the text: editable only with a table loaded), `{"refresh": true}` and
`{"clear": true}`.

Traps of OpenRocket's tab and dialogs that the tool reports, and how to write around them:

- A click on the simulations tab selects the first row when no row is selected; the operations select their
  rows (New and Paste the new ones, Delete and Cut the first row afterwards): a `selectSimulations` of what is
  selected already checks nothing and is refused.
- The settings dialog's OK always asks for the run dialog of a simulation without data: answer `"run":
  {"button": "cancel"}`. Its Cancel asks nothing when an existing simulation was not edited, and always asks for
  a new one; the edits of the wind editor and a cleared lookup table are no edit to OpenRocket's dialog (no `* `
  in its title, and the undo text stays the one before), so that its Cancel would keep them: OK such edits.
- OpenRocket's settings dialog keeps the random seed controls as they were made: a "Reset to default" of the
  launch conditions after a seed was typed is undone by OK's `prepareForSimulation()` (QtRocket's controls
  follow the options; run 2's S4 deviation): do not combine them.
- A spinner commits the text it shows when it loses the focus: type values the field shows as typed (no more
  decimals than its unit shows), or a later focus change can change the value.
- The lookup tables a scenario loads must be cleared (`{"clear": true}` and OK) before Save as.
- "Save as default" of the options page stores the geodetic method, and of the warnings page the thresholds:
  New simulation and the launch conditions' Reset to default are refused after them (bug 38) while they differ
  from OpenRocket's defaults.
- The open design's simulations must hold no flight data (as in version 2), and the scenario runs none.

The committed scenario of version 3 (its directory's README.md tells it step by step):

- `simulations`, 34 steps, on the flight-configurations reference (three configurations, three simulations
  without data, the third without motors): the tab; an edit of Simulation 1 cancelled without a change;
  Simulation 1 renamed and given a multi-level wind in the wind editor (two levels added, a speed and an
  altitude edited, the altitude unit in feet, a level deleted with Delete level, AGL), a rod length and launch
  into wind off, OK with its run cancelled; Simulation 2's options (RK6, WGS84, a constant gravity, the time
  step and the maximum time, a fixed seed), two warning thresholds (one in the collapsed dual deployment
  section), a drag lookup table loaded from a file and edited, the standard atmosphere off and a temperature,
  OK; an edit of it cancelled with its discard question; New simulation with its launch conditions by property
  (renamed, a configuration, the average wind, its turbulence and direction, the launch site, the rod
  direction and angle without launch into wind, the atmosphere: temperature, pressure and humidity), OK;
  Duplicate; the two edited together (the multi-simulation edit); Copy, Paste after the first row, a Delete
  cancelled at its confirmation, Delete of the pasted one with "Do not ask me again", Cut of two rows without
  the confirmation, Undo and Redo; the stepper column shown and the apogee hidden; Paste of the Cut's two
  simulations; two wind imports in the wind editor (a comma-separated file with a header, then a
  space-separated one in feet after a cancelled Reset levels: bug 34); the lookup table cleared; a cancelled
  New simulation (its discard question with "Do not ask me again"); Save as.

Where QtRocket's fixes of OpenRocket's bugs show in it, the record is OpenRocket's with the tool's fix (its
`toolFixes`: bugs 6, 32, 33 and 34, the records `kGuiSimulationToolFixes` of `GoldenGuiReference.h` lists), the
scenario avoids the step, or the golden test's expected record makes the difference QtRocket's GUI shows
(`guiExpectedRecord()`: bug 27's Paste, and no Monte Carlo action, which is Milestone 7's).

## state.json

### Version 1

```
{
  "schema": "gui-from-scratch-reference", "schemaVersion": 1,
  "generator": "tools/openrocket-gui-reference/generate.sh (GuiReference)",
  "openrocket": {"commit": ..., "version": "26.xx-SNAPSHOT-5f164fd0e", "dirty": false, "javaVersion": "17.0.20.1"},
  "scenario": {"file": "scenario.json", "sha256": ..., "title": ..., "steps": <n>},
  "reference": {"file": "rocket.ork", "savedAs": "rocket.ork", "archiveEntries": ["rocket.ork"]},
  "steps": [<record>, ...],
  "loaded": {"lines": [...], "numbers": [...]}
}
```

A record per step of the scenario, in its order, of what the step left:

- `index`, `step`: the step's place and kind;
- `undo`, `redo`: the text (`Undo (Add Nose Cone)`, `Redo`) and the enabled state of the actions;
- `saved`: `OpenRocketDocument.isSaved()`;
- `selection`: the selected components as paths (the components of the tree with the selected ids:
  after an undo or a redo `ComponentTreeModel` keeps a selection by id, so the selection model can
  hold the components the undo replaced);
- `open`: the open window, `{"window": "<dialog panel>", "component": <path>}` or
  `{"window": "file chooser"}`; absent when none is open;
- `treeSha256`: the SHA-256 of the component tree's lines, each followed by a line feed; the lines
  are `describeRocket()` of `tests/core/file/openrocket/ComponentHandlerTestSupport.h` with no known
  id (HandlerProbe of tier 9b, copied into `RocketState`): every component with its class, name and
  what it holds, the decal images, the selected and every flight configuration;
- `tree`: the lines themselves after the steps that end an action (`new`, `ok`, `button`, `undo`,
  `redo`, `paste`, `chooseFile`).

`loaded` is the saved file as OpenRocket loads it in the tool's environment (see
[How OpenRocket is run](#how-openrocket-is-run)): `lines` are `designFileState()` of
`tests/core/file/openrocket/DesignFileState.h` with `StateDetail::WITH_DOCUMENT_PREFERENCES`
(RecordedProbe and ExampleProbe of tier 9c, copied into `RocketState`: the file version, the
warnings, the document with its simulations, the change events of the load, the tree with the ids of
the file, the configuration names and every simulation's configuration, the motors, the decal images,
the simulations (`SimulationLines`, SimProbe of tier 9b with the columns' digests), the document
preferences and materials), `numbers` the masses, centres of mass and the length of a second load
(`massAndLength()`).

Two things a comparison has to know:

- The reload is the saved rocket but for one value: the inner tube's default ignition is an
  override after the reload (`ign=AUTOMATIC:0.0:true`), because the loader sets the saved
  `<ignitionevent>` with `MotorConfiguration.setIgnitionEvent`, which marks it so
  (`MotorMountHandler.java:101-115`, `MotorConfiguration.java:198-205`). In OpenRocket as in
  QtRocket; `GoldenGuiReference.h` lists it (`kGuiSaveAndReloadChanges`).
- The number of change events of the load (`events rocket=`) depends on the default component
  materials of the application's preferences, which OpenRocket's GUI start-up stores; QtRocket's
  test stores them too (`loadDefaultComponentMaterials()`).

### Version 2

```
{
  "schema": "gui-reference", "schemaVersion": 2,
  "generator": "tools/openrocket-gui-reference/generate.sh (GuiReference)",
  "openrocket": {"commit": ..., "presetDatabaseCommit": ..., "version": ..., "dirty": false, "javaVersion": ...},
  "startup": {"motorSets": 1458, "motors": 1588, "presets": 5228},
  "scenario": {"file": "scenario.json", "sha256": ..., "title": ..., "steps": <n>},
  "input": {"file": <the open step's file>, "sha256": <of its bytes>},
  "reference": {"file": "rocket.ork", "savedAs": "rocket.ork", "archiveEntries": [...],
                "archiveSha256": {<entry>: <SHA-256 of its bytes>, ...}},
  "steps": [<record>, ...],
  "loaded": {"lines": [...], "numbers": [...]}
}
```

A record has the members of version 1 (`open` for a configuration dialog or the file chooser; the tab's
questions never stay open across steps), and:

- `tab`: the window's tab, `design` or `flightConfiguration`;
- `questions`: the questions the step asked, in order (absent when none), each as it opened, with
  `question` (`motor`, `ignition`, `deployment`, `separation`, `rename`, `message`) and `title` (the
  dialog's), and
  - ignition: `scope` (the radio button selected: `all` or `this`), `default` and `override` (the two
    radio buttons' texts, the second with the configuration's name), `event`, `delay` (in s);
  - deployment and separation: the same, `altitude` (in m), `altitudeEnabled`, and `events` (the event
    combo box's items: the deployment's without `LOWER_STAGE_SEPARATION` in the bottom stage);
  - rename: `name` (the text field's text, the configuration's raw name);
  - message: `text`;
  - motor: `search`, `rows` (the table's rows), `motor` (`{"manufacturer", "designation", "digest"}` of
    the selected curve, or null), `delayText` (the delay box's text), `delay` and
    `nozzleExitDiameter` (in s and m), and `answered`: the same members after the answer, before its
    button;
  every number a text, as Java 17's `Double.toString` writes it (`"Infinity"` for Plugged);
- `flightConfiguration`, what the tab shows:
  - `configTab` (the current sub-tab), `configTabs` (`[{"tab", "title", "enabled"}]`), `buttons` (the
    enabled state of New, Rename, Remove and Duplicate by their Java field names: `newConfButton`,
    `renameConfButton`, `removeConfButton`, `duplicateConfButton`), `selectedConfiguration` (the
    rocket's selected configuration, its row, -1 for the default), `simulations` (`[{"name",
    "configuration"}]`, each simulation's configuration as a row, -1 for one the rocket does not have);
  - `motors`, `recovery`, `stages`, the three sub-panels: `buttons` (the panel's buttons by their Java
    field names: `selectMotorButton`, `deleteMotorButton`, `selectIgnitionButton`,
    `resetIgnitionButton`; `selectDeploymentButton`, `resetDeploymentButton`; `selectSeparationButton`
    and, as Java names it, `resetDeploymentButton`), `header` (the column headers), `rows` (per row
    `{"configuration": <column 0's text>, "cells": [...]}`, a cell as the table's renderer draws it:
    a motor cell `{"motor": <the motor's text, "4× H148R-0">, "ignition": <its text, "Default (Automatic)">,
    "ignitionDefault": <the ignition label shaded (italic)>}`, a recovery or stage cell `{"text":
    <its text, "Apogee + 1 s">, "shaded": <shaded (italic): never, bug 1>, "override": <the
    configuration has an override of its own, FlightConfigurableParameterSet.containsId>}`),
    `selectedRows`, `selectedColumns`; the Motors sub-panel also `showsHelp` (its card shows the help
    text instead of the table) and `mounts` (the motor mount table: `[{"component", "text",
    "motorMount"}]`);
- `tree` after the steps of version 1 that end an action, after `open` and after every operation of the
  tab (every step of the tab but `configTab` and `selectCells`).

`loaded` as in version 1. The ids of the tree lines are `random`: `flightConfiguration.selectedConfiguration`
and the `simulations` name the configurations by their rows.
The saved file is loaded with the bundled databases of the start-up (`GuiReferenceEnvironment.h`'s
`Databases::BUNDLED`). What a save and a reload change in a scenario's design is listed per scenario
by `guiReloadChanges()` of `GoldenGuiReference.h`: in `flight-configurations` the two configurations
named `Flight {motors}`, the default name at the save, which `RocketSaver` saves without their
`<name>` (`FlightConfiguration.isNameOverridden()`), so that the reload gives them the default name
`[{motors}]`; nothing in `multi-stage`.

### Version 3

The file of version 2 with `"schemaVersion": 3`. A record has the members of version 2 (`selection` the selected
components: none while simulations are selected; `tab` also `simulations`; `questions` also the questions of
the simulations tab; `flightConfiguration` as in version 2; `tree` after the same steps: the steps of the
simulations tab hold the tree's digest only), and:

- `simulationsTab`, what the Flight simulations tab shows, in the terms of the simulation-table pins
  (`tools/openrocket-gui-pins/simulation-table`, SimulationTableProbe):
  - `showsHelp`: the panel's card shows the help text (no configuration with motors) instead of the table;
  - `columns`: the table's columns in the view's order, `[{"id", "header"}]` (the ids of `columnVisibility`,
    the header texts);
  - `rows`: per row (the document's order) `{"status": <Simulation.Status name>, "statusSortText": <the status
    label's toString(), which its sorter compares>, "description": <getStatusDescription()>, "warnings": <the
    warnings box as text: each count and its icon as [HIGH], [NORMAL] or [LOW]>, "warningCounts": [<critical>,
    <normal>, <informational>] (-1 without data), "warningsToolTip": <the box's tooltip>, "cells": [<the text
    of every model column from the name on, null for no value>]}`;
  - `selectedRows`: the table's selected rows;
  - `actions`: the enabled state of the panel's twelve actions by their names without "Action"
    (`newSimulation`, `editSimulation`, `cutSimulation`, `copySimulation`, `pasteSimulation`, `runSimulation`,
    `landingDispersion`, `plotSimulation`, `duplicateSimulation`, `deleteSimulation`, `simTableExport`,
    `selectedSimsExport`; `landingDispersion` is Milestone 7's);
  - `clipboardSimulations`: the names of the simulations on OpenRocket's clipboard;
  - `hiddenColumnsPreference`: the document preference `simulation.table.hiddenColumns` (null when the
    document has none: the table hides the stepper column by default);
- `simulations`: per simulation of the document, in its order, `{"name", "configuration": <the configuration's
  row among the rocket's, -1 for one the rocket does not have>, "status": <getStatus()>, "options": [<the ten
  lines of describeConditions() of tests/core/file/openrocket/ConditionsTestSupport.h (SimulationLines'
  CondProbe lines, all of them): rod, wind, site, stepper, seed, atmosphere, gravity, thresholds, drag and
  stability; the path of a lookup table's file inside the repository written relative to it>], "extensions":
  [<the extensions' names>]}`;
- `toolFixes` (when the step shows one): `[{"bug": <its number, left out for a fix without one>, "fix": <what
  the tool did>}]`, the places where the record is OpenRocket's with one of QtRocket's fixes (see
  [QtRocket's fixes of OpenRocket's bugs (version 3)](#qtrockets-fixes-of-openrockets-bugs-version-3));
- `clipboardText` (after `copySimulations` and `cutSimulations`): the text Copy put on the system clipboard
  (`SimulationPanel.copySimulationValues("\t")`: the status column and the table's header, a line per selected
  simulation with summary data).

The questions of the simulations tab, each as it opened, with `answered`: the same members as the answer left
them, before its button, for the dialogs with fields:

- `simulation` (the settings dialog): `title` (with `* ` once the simulation changed), `name` (the name field),
  `configuration` (the configuration box's item), `status` (the status label's HTML), `tab` (the current tab's
  id), `tabs` (`[{"tab", "title", "enabled"}]` of the five tabs: `launchConditions`, `simulationOptions`,
  `warnings`, `plot`, `export`), `multiSimEdit` (the multi-simulation label's text, null in a single edit);
- `run`: `title`, `simulations` (the names of the simulations it was to run);
- `confirm` (a JOptionPane: the delete confirmation, the discard question, the wind editor's overwrite
  question): `title`, `message` (the texts of its message, in order), `checkBoxes` (the texts of its check
  boxes), `buttons` (its buttons' texts);
- `windLevels` (the wind editor): `title`, `altitudeReference` (`MSL` or `AGL`), `lines` (the lines of the probe
  tools/openrocket-probes/wind-editor: a `HEADER` line per column, a `ROW` line per level and the `MODEL` line;
  QtRocket's `windTableHeaderLines()` and `windTableRowLines()` of
  tests/gui/dialogs/simulation/WindEditorTranscript.h);
- `csvImportSettings`: `title`, `hasHeader`, `columns` (`{"altitude" | "speed" | "direction" |
  "standardDeviation": {"name", "index", "unit"}}`), `separator` (the separator box's item: `SPACE` and `TAB`
  as the box shows them);
- `openFile` (a file chooser): `title`, and `file` (the chosen file, relative to the repository) when it was
  approved;
- `lookup` (the aerodynamic lookup dialog): `title`, `tab` (`drag` or `stability`), and per section `drag` and
  `stability`: `summary` and `summaryToolTip` (the summary label's text and tool tip, with the column list
  sorted, see [Determinism](#determinism)), `border` (the title of the text's border: "Example format:" or
  "Loaded data:"), `text` (the text area), `editable`, `separator` (the separator box's item), `clearEnabled`
  and `refreshEnabled`.

What the save and the reload of the scenario's design change is listed by `guiReloadChanges()` of
`GoldenGuiReference.h`, as for version 2: nothing in `simulations` (its input is the flight-configurations
reference's save, whose reload named its configurations already).
