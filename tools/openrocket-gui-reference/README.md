# OpenRocket's GUI on the from-scratch scenario

`tools/openrocket-gui-reference` runs OpenRocket's own Swing GUI code (Java) on a scenario of user
actions, a rocket designed from scratch, and writes the reference QtRocket's GUI is compared with
(plan section 9.5, the exit test `FromScratchExitGolden`): `tests/data/goldens/gui/from-scratch/`.
The data is committed; neither CMake nor CI ever needs Java.

| File | What it is |
|---|---|
| `tests/data/goldens/gui/from-scratch/scenario.json` | The scenario, written by hand: what a user does in the GUI, step by step (see [The scenario](#the-scenario)). The tool reads it; QtRocket's GUI exit test drives the same steps. |
| `tests/data/goldens/gui/from-scratch/rocket.ork` | The design as OpenRocket's Save as saved it at the end of the scenario: the document of the archive `GeneralRocketSaver` wrote (see [Determinism](#determinism)). Generated. |
| `tests/data/goldens/gui/from-scratch/state.json` | What OpenRocket's GUI had after every step, and the saved file as OpenRocket loads it (see [state.json](#statejson)). Generated. |

Files of the tool:

- `generate.sh`: compiles OpenRocket's core and Swing modules and the tool, runs the tool twice and
  checks that both runs wrote the same files (see [Regenerating](#regenerating)).
- `build.gradle`, `settings.gradle`: a stand-alone Gradle project that compiles OpenRocket's `core`
  and `swing` modules from a local checkout (read only) into `build/`, with the third-party
  dependencies their `build.gradle` files name, as `tools/openrocket-goldens` does for the core.
- `src/main/java/info/qtrocket/guireference/`: `GuiReference` (entry point, start-up, the runs, the
  files), `Scenario` (reads scenario.json), `Session` (one design window and the steps),
  `DialogFields` (the fields of the configuration dialogs), `EditActions` (Edit, Copy and Paste),
  `ComponentPaths`, `RocketState` (the state lines) and `DeterministicUuids` (a copy of the class of
  `tools/openrocket-goldens`).
- `src/main/resources/logback.xml`: keeps OpenRocket's log to its warnings and errors.

QtRocket reads the files through `tests/core/goldens/GoldenGuiReference.h`.
`tests/core/goldens/from_scratch_reference_golden_tests.cpp` (suite `FromScratchReferenceGolden`,
label `golden`) checks that state.json is the record of the committed scenario, that the reference
comes from the OpenRocket of the other goldens, and that QtRocket loads `rocket.ork` without
warnings into exactly OpenRocket's state.

## Regenerating

Requirements as for `tools/openrocket-goldens`: JDK 17 (`java` on the path, or `JAVA=...`; the
script refuses another version, because `Double.toString`, which writes every number of the saved
file, changed in JDK 19), a checkout of OpenRocket (`OPENROCKET_DIR`, default `../openrocket` next
to the QtRocket repository; the reference records its commit, which should be the one of
`tests/data/goldens/manifest.json`), and either network access (Maven Central and JitPack, as
OpenRocket's own build) or a Gradle cache that already holds the dependencies of OpenRocket's core
and Swing modules (building OpenRocket once fills it). OpenRocket's Gradle wrapper is used.

```sh
tools/openrocket-gui-reference/generate.sh
```

The script checks the checkout as `tools/openrocket-goldens/generate.sh` does (no modified tracked
file, no untracked or ignored file among `core/src/main/java`, `core/src/main/resources`,
`core/build.gradle`, `core/libs`, `swing/src/main/java`, `swing/src/main/resources` and
`swing/build.gradle`; `ALLOW_DIRTY=1` overrides and state.json then records `"dirty": true`), builds
with Gradle, and runs the tool twice with a fixed locale, time zone and encoding, headless AWT, no
display and a fresh `java.util.prefs` store each time (OpenRocket's preferences at their defaults; see
[Determinism](#determinism)): into `tests/data/goldens/gui/from-scratch/` and into a temporary
directory. It fails unless the two runs wrote the same `rocket.ork` and `state.json`. A run takes about ten seconds once Gradle has
compiled OpenRocket (the first build takes a minute or two).

`GUI_REFERENCE_OUT=<directory>` writes into another directory (the scenario is still read from
`tests/data/goldens/gui/from-scratch/`); `GUI_REFERENCE_WORK=<directory>` keeps the work files there
(the archives every run saved, the second run's output).

Edit `scenario.json` by hand and regenerate: `FromScratchReferenceGolden` fails while state.json
records another scenario (the SHA-256 of its bytes).

## How OpenRocket is run

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
- **Three runs** in one JVM, each after `DeterministicUuids.reseed("gui/from-scratch")`: a warm-up
  run (static initializers draw UUIDs the first time a class is used), a recorded run, whose records
  go into state.json and whose save is the reference, and a plain run that asks nothing between the
  steps. The recorded run calls every getter of every component after each step (the records),
  as a GUI's figure and tree do, and getters store what automatic dimensions compute; the tool fails
  unless both runs saved the same document, so the reference does not depend on when a GUI asks.
- **The load** of the saved file, for state.json: `GeneralRocketLoader` in the same JVM, so in the
  environment of the GUI (its default component materials: a new component of the loader starts with
  them, which shows in the number of change events). The tool loads both the reference and the
  archive and fails unless they give the same state.

### What runs and what is replaced

What runs is OpenRocket's own code wherever it runs without a display. What cannot is replaced by
the calls it makes, copied line for line (each place names its source lines), but for the calls that
change only the window or the application, which the table lists:

| GUI action | What the tool runs |
|---|---|
| File > New | `OpenRocketDocumentFactory.createNewRocket()` (`BasicFrame.newAction`) and the window above |
| A click on rows of the component tree | `JTree.setSelectionPaths` on the tree, with the paths of `ComponentTreeModel.makeTreePath` |
| An add button | `ComponentButton.doClick()`: Swing's own add logic (`ComponentButton.fireActionPerformed`, `ComponentAddButtons`' body component and stage rules, the undo position "Add ...", the new component selected). It stops with a `HeadlessException` where `ComponentConfigDialog.showDialog` makes the dialog. |
| The configuration dialog opens | What `ComponentConfigDialog.showDialog` does after it made the dialog (`ComponentConfigDialog.java:275-291`): the undo position "Modify <component name>" for an existing component (none for a new one). The parts library it opens for a new component with a preset type (`AutoOpenPartsLibrary`, on by default) is taken as closed without a choice: QtRocket has no parts library before Milestone 3. |
| A field of a dialog | The model the dialog builds for that field, with the same class and arguments (`DialogFields`, each field with its source line): a spinner's `DoubleModel` gets the unit chosen in its `UnitSelector` (`setCurrentUnit`) and the typed text through its `ValueSpinnerModel.setValue(String)` (OpenRocket's `ExpressionParser`, the unit conversion, the bounds), an integer spinner its `IntegerModel` spinner model, a combo box `setSelectedItem` of its `EnumModel`, `TransitionShapeModel` or `MaterialModel` (only the model's own items), a check box its `BooleanModel` (clicked only when its state differs), an Automatic check box its `DoubleModel.getAutomaticAction()` (`putValue(SELECTED_KEY)`; refused where the panel disabled the box: `DialogFields.AutomaticBoxes` copies the rules of `NoseConeConfig`, `BodyTubeConfig` and the packed diameter of `ParachuteConfig`, `ShockCordConfig` and `MassComponentConfig`, with what they do to the component when the dialog opens), a text field what its `TextFieldListener` does on focus lost. The dialogs themselves are JDialogs, which a headless JVM cannot make, and their panels need the dialog as their parent. |
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

### Determinism

- The component and flight configuration ids come from `DeterministicUuids`, reseeded before each
  run; the warm-up run makes the recorded and the plain run start with every class initialized.
- `GeneralRocketSaver` always writes a zip archive, whose entries carry the time of the save. The
  reference keeps the document of its only entry, `rocket.ork`, as a plain `.ork` file (the
  OpenRocketSaver XML, which QtRocket and OpenRocket both load); state.json lists the archive's
  entries (`["rocket.ork"]`: no decal, no motor file, no preview).
- OpenRocket's preferences start at their defaults and are never the user's: `java.util.prefs.userRoot`
  and `systemRoot` point the store at an empty directory of each run, which only Linux's
  `FileSystemPreferences` honours (macOS keeps preferences in property lists, Windows in the
  registry), so `generate.sh` also sets `openrocket.debug.prefs`: `SwingPreferences` then uses the
  node `OpenRocket-debug`, which it clears when it starts, and `TranslatorProvider` reads no locale
  from the user's node, on every system. A run with and one without the property wrote the same files.
- Nothing else depends on the time, the machine or the user: `generate.sh` compares two runs of the
  tool byte for byte.

## The scenario

`scenario.json` is `{"schema": "gui-from-scratch-scenario", "schemaVersion": 1, "title": ...,
"steps": [...]}`. A step is an object with `"step"` (its kind), an optional `"note"` for the reader,
and exactly the members its kind takes (both readers reject others):

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
dialog labels it. A component path is the list of names a user reads in the tree from the stage
down (`["Sustainer", "Body Tube", "Parachute"]`), an element being a name (the first child of that
name) or `{"name": ..., "occurrence": n}` (the n-th child of that name); `[]` is the rocket.

The fields the tool knows, by dialog (the panel class `ComponentConfigDialog` picks):

| dialog | fields |
|---|---|
| every component's | `Name`, `Comment` |
| `NoseConeConfig` | `ShapeType`, `ShapeParameter`, `Length`, `BaseRadius`, `Thickness`, `Filled`, `Flipped`, `Material`, `Finish` |
| `BodyTubeConfig` | `Length`, `OuterRadius`, `InnerRadius`, `Thickness`, `Filled`, `Material`, `Finish`, `MotorMount`, `MotorOverhang` |
| `TrapezoidFinSetConfig` | `FinCount`, `CantAngle`, `RootChord`, `TipChord`, `Height`, `Sweep`, `SweepAngle`, `CrossSection`, `Thickness`, `AxialMethod`, `AxialOffset`, `Material`, `Finish` |
| `FreeformFinSetConfig` | `FinCount`, `CrossSection`, `AxialMethod`, `AxialOffset`, `Material`, `Finish`, the fin point table |
| `InnerTubeConfig` | `Length`, `OuterRadius`, `InnerRadius`, `Thickness`, `AxialMethod`, `AxialOffset`, `Material`, `MotorMount`, `MotorOverhang` |
| `CenteringRingConfig` | `Length` (labelled Thickness), `OuterRadius`, `InnerRadius`, `AxialMethod`, `AxialOffset`, `Material` |
| `ParachuteConfig` | `Diameter`, `SpillDia`, `Material`, `CD`, `LineCount`, `LineLength`, `LineMaterial`, `Length`, `Radius`, `AxialMethod`, `AxialOffset`, `DeployEvent`, `DeployDelay`, `DeployAltitude` (the last three on the default deployment configuration) |
| `ShockCordConfig` | `CordLength`, `Material`, `Length`, `Radius`, `AxialMethod`, `AxialOffset` |
| `LaunchLugConfig` | `Length`, `OuterRadius`, `InnerRadius`, `Thickness`, `InstanceCount`, `InstanceSeparation`, `AngleOffset`, `AxialMethod`, `AxialOffset`, `Material`, `Finish` |
| `RailButtonConfig` | `OuterDiameter`, `InnerDiameter`, `BaseHeight`, `FlangeHeight`, `TotalHeight`, `ScrewHeight`, `InstanceCount`, `InstanceSeparation`, `AngleOffset`, `AxialMethod`, `AxialOffset`, `Material`, `Finish` |
| `MassComponentConfig` | `MassComponentType`, `ComponentMass`, `Density`, `Length`, `Radius`, `AxialMethod`, `AxialOffset` |
| `SaveDesignInfoPanel` (Save as) | `Designer`, `KitName`, `Revision` |

To use another field, add it to `DialogFields` from the dialog's source.

The committed scenario, 101 steps: New; the sustainer selected; a nose cone (ellipsoid, 10 cm, 0.15
cm wall, polycarbonate, smooth paint); a body tube after it (30 cm, outer diameter 4.1 cm, 0.05 cm
wall); four trapezoidal fins in the tube (chords 7 and 3.5 cm, height 5.5 cm, sweep 4 cm, 2.5 mm
thick, rounded); the nose cone edited again (Edit > Edit) for its automatic base diameter, whose
Automatic check box is enabled only now that the body tube follows it; an inner tube as the motor
mount (7.5 cm, diameters 2.6 and 2.4 cm, overhang 0.5 cm); two named centering rings, the fore one
placed from the top (23.7 cm), the aft one from the middle (14.35 cm; a new ring is placed from the
bottom); a parachute (35 cm, 8 lines, deployed at 150 m); a shock cord (60 cm); a launch lug (5 cm,
at 135 degrees) and two rail buttons (15 cm apart, at 90 degrees); an altimeter (a mass component of
15 g); the fin set converted to freeform, two points edited and one inserted; Undo and Redo of those
edits; the parachute copied and pasted into the tube, the copy renamed "Drogue" and made 15 cm; Save
as with a designer, file `rocket`. Every step that sets a value changes the design (see above).

## state.json

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
warnings, the document, the change events of the load, the tree with the ids of the file, the
configuration names, the motors, the decal images, the document preferences and materials; the
document has no simulation, which the tool checks), `numbers` the masses, centres of mass and the
length of a second load (`massAndLength()`).

Two things a comparison has to know:

- The reload is the saved rocket but for one value: the inner tube's default ignition is an
  override after the reload (`ign=AUTOMATIC:0.0:true`), because the loader sets the saved
  `<ignitionevent>` with `MotorConfiguration.setIgnitionEvent`, which marks it so
  (`MotorMountHandler.java:101-115`, `MotorConfiguration.java:198-205`). In OpenRocket as in
  QtRocket; `from_scratch_reference_golden_tests.cpp` lists it (`kSaveAndReloadChanges`).
- The number of change events of the load (`events rocket=`) depends on the default component
  materials of the application's preferences, which OpenRocket's GUI start-up stores; QtRocket's
  test stores them too (`loadDefaultComponentMaterials()`).
