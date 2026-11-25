# What OpenRocket's GUI shows: the pin tables of the GUI tests

`tools/openrocket-gui-pins` runs small Java probes against OpenRocket's own Swing classes and turns what
they print into the tables QtRocket's GUI tests compare with. The tables are committed; neither CMake nor
CI ever needs Java. They change only through this tool. Each subdirectory is one family: its probe
(`probe/*.java`, package `probe`) and the Python script that writes the table.

| Family | Table | Probe | Generator | Read by |
|---|---|---|---|---|
| `figure-paint` | `tests/gui/figure/RocketFigurePaintPins.h` | `FigurePaintProbe` | `gen_rocket_figure_paint_pins.py` | `rocket_figure_paint_golden_tests.cpp` |
| `rocket-info` | `tests/gui/figure/RocketInfoPins.h`; `tests/data/goldens/gui/rocket-info/flight-blocks.json` | `RocketInfoProbe`; `RocketInfoFlightProbe` | `gen_rocket_info_pins.py`; `gen_rocket_info_flight_pins.py` | `rocket_info_golden_tests.cpp`; `rocket_info_flight_golden_tests.cpp` (through `RocketInfoFlightPins.h`) |
| `ribbon` | `tests/gui/widgets/RibbonPins.h` | `RibbonProbe` | `gen_ribbon_pins.py` | `ribbon_golden_tests.cpp` |
| `rocket-panel` | `tests/gui/figure/RocketPanelPins.h` | `RocketPanelProbe` | `gen_rocket_panel_pins.py` | `rocket_panel_golden_tests.cpp` |
| `preferences` | `tests/gui/dialogs/PreferencesPagePins.h` | `PreferencesProbe` | `gen_preferences_pins.py` | `preferences_pages_golden_tests.cpp`, `PreferencesDialogTests.cpp`, `SimulationPreferencesPanelTests.cpp`, `WarningsPreferencesPanelTests.cpp`, `LaunchPreferencesPanelTests.cpp` |
| `scale-figure` | `kFigureRows` in `tests/gui/figure/ScaleFigureTests.cpp` | `ScaleProbe` | `figure_rows.py`, `splice_figure_rows.py` | that file |
| `material-binding` | the six row and group tables of `tests/gui/binding/MaterialBindingTests.cpp` | `B2Probe` | `rows.py`, `splice_material_rows.py` | that file |
| `material-combo` | the groups and searches of `tests/gui/widgets/MaterialComboBoxTests.cpp` (checked, not written) | `B4Probe` | `rows.py`, `check_combo_rows.py` | that file |
| `component-editing` | `makeActionsScenarios()` and `clipboardScenarios()` in `tests/gui/main/ComponentEditingTests.cpp` | `ActionsProbe`, `ClipboardProbe` | `gen_component_editing.py` | that file |
| `component-drop` | `dndScenarios()` in `tests/gui/tree/ComponentDropTests.cpp` | `DndProbe` | `gen_component_drop.py` | that file |
| `actions-rules` | `kTranscripts` in `tests/gui/main/RocketActionsTests.cpp` | `ActionsRulesProbe` | `gen_actions_rules.py` | that file |
| `thrust-curves` | `tests/gui/plot/ThrustCurvePins.h` | `ThrustCurvesProbe` | `gen_thrust_curve_pins.py` | `thrust_curve_plot_golden_tests.cpp` |
| `flight-config` | `tests/gui/flightconfig/FlightConfigurationTablePins.h`, `tests/gui/flightconfig/FlightConfigurationOperationPins.h` | `FlightConfigurationTablesProbe`, `FlightConfigurationOperationsProbe` | `gen_flight_configuration_table_pins.py`, `gen_flight_configuration_operation_pins.py` | `flight_configuration_tables_golden_tests.cpp`, `flight_configuration_operations_golden_tests.cpp` |
| `motor-chooser` | `tests/gui/dialogs/motor/MotorChooserPins.h` | `MotorChooserMountsProbe` | `gen_motor_chooser_mount_pins.py` | `motor_chooser_mounts_golden_tests.cpp` |
| `preset-chooser` | `tests/gui/dialogs/preset/PresetChooserPins.h`; values pinned by hand | `PresetChooserProbe`; `PresetGuiProbe` (`TableUiProbe`, `PresetModelProbe`, `PresetComboProbe`, `TableRowTraversalProbe`, `ComponentPresetTableProbe`, `NewComponentCancelProbe`), `ConfigPresetRowProbe`, `PresetShownProbe` (`DialogKeysProbe`, `SpaceRobotProbe`, `PresetTableInputProbe`) | `gen_preset_chooser_pins.py`; none | `preset_chooser_golden_tests.cpp`; `TableUiPreferencesTests.cpp`, `PresetBindingTests.cpp`, `PresetComboBoxTests.cpp`, `TableRowTraversalTests.cpp`, `ComponentPresetTableModelTests.cpp`, `ComponentPresetFilterModelTests.cpp`, `ComponentPresetTableTests.cpp`, `ComponentPresetChooserDialogTests.cpp`, `ComponentConfigDialogTests.cpp`, `RocketComponentConfigTests.cpp` |
| `open-designs` | `tests/gui/exit/OpenDesignsTabPins.h`, `tests/gui/exit/OpenDesignsPartsLibraryPins.h` | `OpenDesignsTabProbe`, `OpenDesignsPartsLibraryProbe` | `designs.py`, `gen_open_designs_tab_pins.py`, `gen_open_designs_parts_library_pins.py` | `open_designs_exit_tests.cpp` |
| `plot-widgets` | values pinned by hand | `PlotWidgetsProbe` | none | `DataTypeComboBoxTests.cpp`, `UnitSelectorTests.cpp`, `LineStyleComboBoxTests.cpp`, `ColorChooserButtonTests.cpp` (`tests/gui/widgets/`) |
| `simulation-table` | `tests/data/goldens/gui/simulation-table/simulation-table.json`; values pinned by hand | `SimulationTableProbe`; `SimulationTableViewProbe` | `gen_simulation_table_pins.py` (and `open-designs/designs.py`); none | `tests/gui/simulation/SimulationTablePins.h`: `SimulationTableModelTests.cpp`, `SimulationColumnsTests.cpp`, `SimulationTableViewTests.cpp`, `SimulationPanelTests.cpp`, `SimulationTableCsvExportTests.cpp`, `SimulationEditingTests.cpp`, `simulations_window_tests.cpp`, `open_designs_exit_tests.cpp`; `SimulationTableViewTests.cpp`, `SimulationColumnVisibilityTests.cpp` |
| `simulation-dialog` | `tests/data/goldens/gui/simulation-dialog/simulation-dialog.json` | `SimulationDialogProbe` | `gen_simulation_dialog_pins.py` (and `open-designs/designs.py`) | `tests/gui/dialogs/simulation/SimulationDialogPins.h`: `open_designs_exit_tests.cpp` (through `SimulationDialogPinChecks.h`) |
| `plot-dialog` | values pinned by hand | `PlotDialogProbe`, `PlotTabProbe` | none | `PlotDialogTests.cpp`, `SimulationPlotDialogTests.cpp`, `PlotTypeSelectorTests.cpp`, `SimulationPlotTypeSelectorTests.cpp`, `SimulationPlotPanelTests.cpp` (`tests/gui/dialogs/plot/`), `tests/gui/plot/DataPlotTests.cpp` |

`shared/probe/DelegatingGraphics2D.java` is a `Graphics2D` that forwards every call to another one (written
once from the JDK's `Graphics2D` with a reflection helper that is not kept; edit it by hand); `figure-paint`
and `rocket-info` record what OpenRocket paints through it. `shared/probe/DeterministicUuids.java` makes
OpenRocket's UUIDs reproducible (see "Reproducibility"). Every probe is compiled with both.

## What the probes pin

Every probe starts OpenRocket with core's or swing's `ServicesForTesting`. Those of the Swing views
(`figure-paint`, `rocket-info`, `ribbon`, `rocket-panel`, `preferences`, `thrust-curves`,
`flight-config`, `motor-chooser`, `plot-widgets`, `plot-dialog`) bind `SwingPreferences`, at their defaults (a fresh `java.util.prefs` store),
and apply FlatLaf Light, as the application does (`thrust-curves` switches to FlatLaf Dark for its dark charts
and back); `open-designs` binds `SwingPreferences` with one value changed (its parts-library probe: at their
defaults) and FlatLaf Light (below);
`scale-figure` binds `SwingPreferences` without a theme; the two material probes keep
`ServicesForTesting`'s preferences, the four component probes swing's. All but the material, component,
`preferences`, `open-designs` and `plot-widgets` probes load OpenRocket's motor database and no component presets. `figure-paint`, `rocket-info`,
`ribbon`, `rocket-panel` and `motor-chooser` settle a design they load from `data/examples` or build with
`TestRockets` as the golden harness settles it (a copy of `tools/openrocket-goldens`' `AutomaticDimensions`).
Every double is printed exactly (`Double.toHexString` or Java 17's `Double.toString`).

- `figure-paint`: OpenRocket's `RocketFigure` painted headless, as `RocketPanel.create2DPreviewFigure()`
  makes it (96 dpi, fitted to 1000 x 700), into a recording `Graphics2D`: every `draw` and `fill` with its
  colour, stroke and device bounds, the CG and CP carets where `RocketPanel.updateExtras()` puts them, and
  `getComponentsByPoint()` at points of the painted figure. 20 scenarios: "A simple model rocket" in seven
  states (side, selected, back, top at 30 degrees, side at 30 degrees, line styles and colours, a hidden
  body tube), pods (side, back, ShowMarkers), clustered motors, parallel boosters, dual parachutes, two
  stages, tube fins, `TestRockets.makeBeta` and a new design.
- `rocket-info`: `RocketInfoContextHelper.computePhysics()` and `RocketInfo.paint()` (the overlay of the
  design view) on the 16 examples, the 13 test rockets and a few designs of its own: every text with its
  font, colour and row, the carets, CG and CP, the warnings. Its second probe, `RocketInfoFlightProbe`
  (Milestone 4, run 2, part G2), pins the overlay's flight block (`RocketInfo.drawFlightInformation()`) for
  each design of `OpenDesignsExitGolden` (the open-designs family's `designs.py`, loaded as
  `OpenDesignsTabProbe` loads them: the seven synthetic motors for the test rockets' re-saves, the six presets
  of the example designs, `MotorNameColumn` false) and seven states of its own (a simple design calculating,
  after a failed run, without flight data, in imperial units; the aborted stored flight of the coverage
  re-save with its configuration selected, calculating and not): the state `RocketPanel.updateExtras()` gives
  the overlay when the design is opened with the estimate on and `AutoRunSimulations` off, replayed line by
  line without starting the worker (which simulations `updateSims(false)` and `updateSims(true)` would run,
  calculating or not, the shown simulation), every simulation's status and maxima, and the texts of the block
  after those of a paint without flight data, each with its font, colour, row above the bottom one and column
  (the left margin or the values' column), which do not depend on the font. No fresh flight: the four
  designs whose estimate would run show their stored values, dimmed. The generator writes JSON
  (`tests/data/goldens/gui/rocket-info/flight-blocks.json`, the design files relative to the repository).
- `ribbon`: `StageSelector` and `ConfigurationComboBox`, made as `RocketPanel` makes them, on the 16
  examples, the 13 test rockets and five designs of its own: the stage buttons (text, state, tooltip), each
  enabled button clicked twice on a fresh design, every configuration picked, with the rocket's events and
  the document's undo history.
- `rocket-panel`: `ViewRotationControl.snapRotation()`, the rotation of a left drag
  (`RocketPanel.handleMouseDragged()`), the figure's hit lists at points, and the selection after plain,
  Shift, Ctrl, double, middle and right clicks. It needs a display (`GUIUtil.isMenuShortcutDown()` asks the
  toolkit for the menu shortcut key, which a headless AWT refuses), so it runs under `xvfb-run` with
  `-Djava.awt.headless=false`. The generator's `EDITS` table holds QtRocket's deliberate differences (U4),
  each with its reason; the pin keeps OpenRocket's result beside it.
- `preferences`: the Units, Design and General pages of OpenRocket's preferences dialog for a cleared store,
  the Units page after "Default imperial" and "Default metric", the choice and Mach number steps, stored
  out-of-range values and the units node after `storeAndApplyPreferences()`. The probe's home directory
  (`user.home`, a fresh one in the work directory) is written `<home>`. Milestone 4 (run 2, part S5) added
  the pages of the simulations, each over a store just cleared: the Design page's two boxes of the
  simulations clicked twice (`STEPS DESIGN`); the Simulation, Warnings and Launch pages in the form of the
  dialog walk of `tools/openrocket-probes`' simulation probes (`WALKPAGE`, the GUI tests'
  `SimulationDialogWalk.h`), and what a user does on them (`STEPS`: the check boxes, the geodetic and gravity
  combo boxes, the numbers typed on the spinner models, the seed's check box, field, focus loss and
  `prepareForSave()` by reflection, "Reset to default", the thresholds typed and the dual deployment section
  expanded and the single one collapsed, the Launch page's wind, rod length and latitude) with the keys
  they store; a seed Java draws is never printed. The probe binds one `SwingPreferences` for itself and every
  page, as OpenRocket's `GuiModule` does (`Scopes.SINGLETON`): each instance keeps a cache of the stored keys,
  and a getter of one that missed a key another instance stored writes the default over it. The getters of
  `SwingPreferences` store the default they read; QtRocket's do not, so the tests print an absent key with
  the default the probe saw stored.
- `scale-figure`: `RocketFigure`'s fit scale, user scale, preferred size, size, subject origin and auto-zoom
  point for "A simple model rocket", "Two stage high power rocket" and a new `Rocket`, in each view, for seven
  visible areas. The probe also prints the scale selector's items and texts and the ruler's ticks, which
  `ScaleSelectorTests.cpp` and `RulerTests.cpp` pin by hand.
- `material-binding`: `MaterialModel` over a new design whose application and document hold extra
  materials: the combo box rows and the groups of the bulk, line and surface materials. The probe also
  prints the values `MaterialBindingTests.cpp` (the built-in rows, the selections), `IntBindingTests.cpp`,
  `BoolBindingTests.cpp`, `EnumBindingTests.cpp` and `TransitionShapeBindingTests.cpp` pin by hand.
- `material-combo`: `MaterialComboBox.createComboBox()`: its groups popup with OpenRocket's check marks and
  its `filter()` searches, with "Doc foam", "Balsa" and a loose material selected. The probe also prints
  the selected displays and the enum texts that `MaterialComboBoxTests.cpp` and `EnumComboBoxTests.cpp` pin
  by hand.
- `component-editing`: `RocketActions`' Cut, Copy, Paste, Duplicate, Delete, Move up and Move down, run
  headless on the probes' design (`tests/gui/tree/ProbeDesign.h`): `RocketActions` and its inner actions
  are allocated without their constructors (they need a `BasicFrame`) and their methods called by
  reflection; the exception each records at its end (`parentFrame.selectTab()` on the frame the probe has
  not) is cut from the transcript. `ActionsProbe` (the M2 component-tree scout's) prints the enabled states,
  the tree, the clipboard, the selection and the undo history of 42 scenarios, `ClipboardProbe` (run 2, part
  T3) those of 40 more. Three blocks of `ActionsProbe` (the only stage, the paste into another document, the
  clipboard's copies) are pinned by tests of their own.
- `component-drop`: `ComponentTreeTransferHandler`'s drag (`createTransferable()`) and the loop of
  `importData()` over its private `checkImportAction()` and `importDataTarget()`, headless: 20 drops on the
  probes' design (`DndProbe`, the M2 component-tree scout's).
- `actions-rules`: the enabled states of `RocketActions`' twelve component actions over OpenRocket's 13 test
  rockets, for nothing selected, each component alone and with the one before it (`ActionsRulesProbe`, run 2,
  the fixer of group T).
- `thrust-curves`: the motor chooser's thrust-curve charts, `MotorInformationPanel`'s preview (its private
  constructor, `updateData()` and `clearData()` called by reflection) and `ThrustCurveMotorPlotDialog`, for
  every motor set of the bundled database (1458 sets, 1588 curves), each set's curves sorted with
  `ThrustCurveMotorComparator` as the chooser lists them: the curves' keys (manufacturer, designation and
  digest: 708 curves share their digest with a curve of another manufacturer or designation), both charts'
  axis ranges with the first curve selected, and for the 107 sets of more than one curve, with the first and
  the last curve selected, the series of both charts (key, paint, stroke width, marker, points) and the
  dialog's legend items, again for the set with the most curves in FlatLaf Dark; also the curve colours, the
  texts, the empty preview, the dialog's modality, keys, chart size and button, and the thrust axis with each
  force unit as the default unit (OpenRocket's label names the unit, its range stays in newtons). The dialog
  is a `JDialog`, which a headless AWT refuses, so the probe runs under `xvfb-run` with
  `-Djava.awt.headless=false`; no window is shown. The test builds each list from the keys, so it does not
  depend on QtRocket's port of the comparator.
- `flight-config`: the tables of the "Motors & Configuration" tab over the 16 examples (as loaded, not
  settled) and the 13 test rockets, as OpenRocket's own code makes them (`FlightConfigurationTablesProbe`, a
  generalisation of the M3 config-tab scout's `NamesProbe`, which copied the renderers' code): the
  `FlightConfigurableTableModel` subclasses the three panels make in `initializeTable()` (taken from the
  panels' compiled anonymous classes) and `MotorMountTableModel` (package-private, by reflection), every cell
  rendered by the panel's own cell renderer (`MotorTableCellRenderer`, `RecoveryTableCellRenderer`,
  `SeparationTableCellRenderer`: inner classes, whose outer panel is allocated without its constructor, with
  the rocket, the translator and the `RocketDescriptor` its renderers read) into a `JTable`, not selected and
  without focus. It prints the rows (the configuration ids, raw names and overrides), every header, every
  cell's texts and style (plain or italic, `TEXT` or `TEXT_DIM`) with the flags the shading follows
  (`hasIgnitionOverride()`, `isDefault()`, `containsId()`), the motor-mount rows, and column 0 of the last
  row after `setName()` with 14 raw names. Each test rocket is built twice, from two seeds, to tell the ids
  its maker draws (compared by row) from the ones it sets. Part 1 of the family (run 2, part F2).
  Part 2 (run 2, part F3), `FlightConfigurationOperationsProbe` (a generalisation of the M3 config-tab
  scout's `TabProbe`): the tab's operations run by OpenRocket's real `FlightConfigurationPanel` and its three
  sub-panels in a frame on a virtual display, with a `BasicFrame` allocated without its constructor (its
  document, rocket and selection model set, and registered in `BasicFrame`'s list of frames); it needs a
  display (the dialogs are modal `JDialog`s), so it runs under `xvfb-run` with `-Djava.awt.headless=false`,
  and shows no window on a desktop. Ten scenarios over "A simple model rocket", "Two stage high power rocket"
  and a new design: cells selected (`JTable.changeSelection()`), sub-tabs switched, every operation of the
  tab called (the panels' private methods, by reflection; New, Rename, Delete and Duplicate through their
  actions) with each modal dialog answered from a watcher thread inside its modal loop (the value dialogs
  through their radio buttons, combo boxes, spinners and buttons; the motor chooser through its private
  `select()` and its delay field, as `TabProbe` did; the rename dialog and the message of "Save as default"
  through their buttons), multi-cell edits, undo and redo, the motor mount check box and a few design edits.
  After every step it prints the cells the operation acted on, each dialog's first state and its answer, the
  rocket's change events (with their origin: a sub-panel's operation, one of the three of
  `configurationChanged()`, the core), the configurations and simulations, every mount's, recovery device's
  and stage's values per configuration, the three tables and the undo history. Two EDITS of OpenRocket, for
  QtRocket's fixes of its bugs 5 and 6 (plan 10.3), keep the undo histories comparable: the probe adds the
  undo positions of New, Duplicate and the motor mount check box ("New Configuration", "Duplicate
  Configuration", "Modify motor mount") that QtRocket adds, and gives the mounts back the default ignitions
  QtRocket's undo history keeps (OpenRocket's copies of the rocket lose them) after an undo or a redo. The
  generator turns each run of three `configurationChanged()` events into one, as QtRocket fires one; the test
  applies QtRocket's other fixes (bugs 1 to 4) to the few steps where they show, each the last of its
  scenario.
- `motor-chooser`: the motor chooser a design window keeps (`MotorConfigurationPanel`'s one
  `MotorChooserDialog`), opened for every acting motor mount (`isMotorMount()`, in the order of the component
  tree) in every flight configuration (`Rocket.getIds()`) of each of the 16 examples, with "Limit motor
  diameter to mount diameter" and "Limit motor length to mount length" on (the mount's limits applied) and the
  preferences otherwise at their defaults, one chooser per design: OpenRocket's real
  `ThrustCurveMotorSelectionPanel`, headless (the dialog is a `JDialog`; the probe does what it adds:
  `open()`'s `selectMotorFromTable()` and `close(true)`'s `selectedMotor()`). At every open: the filter panel
  (the limit boxes, the diameter and length thumbs, the filter's limits, the mount's dimensions), "Number of
  motors:", the table's rows (the model rows in the view's order and the MD5 of their texts, as
  `tools/openrocket-motor-chooser`'s chooser family hashes them), the selected row and motor, the curve list
  and the delay list; at a mount's first open the rows of four searches; then OK's answer, and after a design
  the preferred curves its OKs stored. The probe runs the designs twice: the second run makes, from outside,
  the fix QtRocket's `ThrustCurveMotorSelectionPanel` makes (opened for a configuration with a motor, the
  chooser forgets the table row an earlier question selected, which OpenRocket selects instead of the
  configured motor when the mount's limits hide its set: seven opens of "Airstart timing", "Dual parachute
  deployment" and "Two stage high power rocket"), and the header keeps of it what differs
  (`kMotorChooserMountFixedOpens`, `kMotorChooserMountFixedPreferred`). The test applies bug 8 (plan 10.3: a
  configured delay is kept) to the pinned delays itself.
- `preset-chooser`: the parts library's helpers. `TableUiProbe` runs `TableUIPreferences` (store and load) on a
  `JTable` of eight columns in the parts library's `XTableColumnModel` (made by reflection: its constructor is
  package-private) whose renderers' preferred widths are 10 pixels per character, so that no width depends on a
  font: the restored order, visibility and widths for stored values of every kind (permutations, duplicates,
  out-of-range and negative indices, hidden columns shown again, missing and fixed widths) and the stored keys.
  `PresetModelProbe` runs `PresetModel` (the preset combo box's model) over the presets of
  `TestPresetDatabase.h`'s `smallPresetDatabase()` (the three files generate.sh passes, in its order) with
  `SwingPreferences` for the favourites: its items, its selected item, and the events, presets and undo history
  of `setSelectedItem()` for one body tube and for two (the second a config listener). `PresetComboProbe` puts
  a `JComboBox` on a recording model with `PresetModel`'s items (the probe's favourites): the item it shows as
  selected, the text its renderer gives, and which choices reach the model (`JComboBox` drops one that `equals()`
  its selected item, also a copy of the preset). `TableRowTraversalProbe` runs `TableRowTraversalPolicy`'s four
  actions beyond its JUnit test (no selection, several rows, no rows, one row, a table of single rows, a laid-out
  table of 100 rows, an empty filtered table) and prints the key bindings and focus traversal keys of a table
  with the policy. `ComponentPresetTableProbe` makes the parts library's `ComponentPresetTable` (headless: the
  dialog is not made) over the same presets and over the bundled ones (`data/components`, the last argument,
  walked as QtRocket's `DirectoryIterator` walks it): the columns of every type, the text of every cell, each
  column sorted alone and header clicks, the header menu of each column (made by reflection, its items clicked)
  with its units and column items, the favourites, the selection through the filters, and the dialog's text,
  diameter and legacy filters (`ComponentPresetRowFilter` made by reflection); the bundled presets' views as the
  MD5 of their keys in view order (`viewKey()`: each preset's digest, but for the six presets whose digest is not
  the same on every platform; "Reproducibility"). Where QtRocket fixes an OpenRocket bug (the header menu's Sort
  on the view index, bug 13; the text filter on the Boolean columns, bug 17) it prints the fixed result too,
  computed with OpenRocket's classes. `NewComponentCancelProbe` replays what the configuration dialog's Cancel (one
  `document.undo()`) leaves of a body tube just added ("Add Body Tube"), after a preset was chosen for it
  (`PresetModel.setSelectedItem()`, as the parts library's Close applies it) and without one, and what a
  second undo leaves: the tube stays after Cancel once a preset was chosen (bug 15, which QtRocket fixes).
  `PresetGuiProbe` runs the six; the tests pin its whole transcript by hand.
  `ConfigPresetRowProbe` makes the configuration panel `ComponentConfigDialog` makes for each component of a
  design with a component of every kind (and for two multi-component selections) and prints its preset row: the
  preset combo box (its tooltip, 25 rows, not editable) and the "Parts Library" button, the start of the panel's
  focus order and the field focused after a preset (`focusElement`). The panels need a parent `JDialog`, which a
  headless AWT refuses, so it runs under `xvfb-run`; none is shown. `PresetShownProbe` shows the parts library on
  xvfb's screen (`ShownChooser`: FlatLaf Light, a main window of 1000 x 800 pixels and a configuration dialog
  owning the parts library, as in OpenRocket) and runs `DialogKeysProbe` (the focus order, Enter and Shift+Enter
  in the table, none of which presses Close, Escape, Ctrl+Tab, Enter in the filter field), `SpaceRobotProbe`
  (Space in the table: it ticks the favourite only in the favourite column) and `PresetTableInputProbe` (typed
  text in the table changes nothing; a click anywhere in a favourite cell ticks it and selects its row, a double
  click ticks and unticks it; the widths of a first open are measured over every row, the legacy ones too). A
  `java.awt.Robot` presses the keys, each followed by the toolkit's idle wait and 150 ms; the clicks are mouse
  events sent to the table, as AWT delivers them. Every probe starts from the preferences of a fresh store
  (`ShownChooser.resetPreferences()`); they print no pixel width (the widths depend on the fonts), only the
  windows' sizes and places, which the probe sets.
  `PresetChooserProbe` makes the parts library itself, `ComponentPresetChooserDialog`, as
  `RocketComponentConfig.selectPreset()` makes it (owned by a configuration dialog of a main window of 1000 x
  800 pixels), over the bundled presets bound as the application's database, with `SwingPreferences` and FlatLaf
  Light, for every kind of component that takes presets in the scouting's design and for a flipped nose cone
  behind a transition: the table's columns, the boxes (text, tooltip, shown, ticked), the diameter filters'
  values and tolerances, the "Always open this dialog" box, and the table's rows (their number and the MD5 of
  their keys in view order, as `ComponentPresetTableProbe` hashes them) after each step (the diameter boxes
  unticked, the legacy box ticked, five texts, the text cleared, the boxes back); then Close with and without a
  selected row, double clicks, the "Always open" box clicked and the "tables" node after each, and the dialog as a
  window (its size, default button, Escape, the focus order of its `LayoutFocusTraversalPolicy`, for which it is
  shown without its modality on xvfb's screen) with the table's Enter and Shift+Enter (`selectNextRowCell` and
  `selectPreviousRowCell`, run through its input and action maps). The dialog is a `JDialog`, which a headless AWT
  refuses, so the probe runs under `xvfb-run` with `-Djava.awt.headless=false`. Its `PresetModel` records the
  preset the dialog applies instead of loading it. Where QtRocket fixes an OpenRocket bug the probe uses the fixed
  filter, made with OpenRocket's classes, and prints both values (the flipped nose cone's base compared with the
  previous component's aft diameter, bug 16); its texts never match a Boolean column's text (bug 17). Before each
  scenario it puts the dialog's preferences back to their defaults and reseeds OpenRocket's UUIDs from the
  scenario's name.
- `open-designs`: the "Motors & Configuration" tab as OpenRocket shows it, when the user chooses it, for each of
  the 66 designs of the exit suite `OpenDesignsExitGolden` (`tests/gui/exit/open_designs_exit_tests.cpp`):
  `designs.py` lists them from the goldens' manifest in the suite's order (the 16 examples of `data/examples`,
  their 16 re-saves, the 13 test rockets' re-saves, the 18 legacy and the 2 coverage re-saves, and the design
  OpenRocket's GUI built from scratch), and `OpenDesignsTabProbe` loads each as the suite loads it: the bundled
  motor database, but for the test rockets' re-saves the seven synthetic motors of `TestRockets` (made by its
  private generators, as `InteropCheck`'s test-rocket injector makes them), and the six component presets of the
  example designs (taken by their digests from the bundled presets, `data/components`, as the golden harness's
  legacy injector takes them); `SwingPreferences` (the panels cast to it) with `MotorNameColumn` false, the one
  value the tab reads that the suite's windows have otherwise than its default (their `DesignFileEnvironment`
  shows a motor by its common name, as `ServicesForTesting` answers false for every flag); FlatLaf Light. It
  makes OpenRocket's real `FlightConfigurationPanel` of a `BasicFrame` allocated without its constructor (its
  document, rocket and selection model set, as the operations probe of `flight-config` makes it), selects the top
  stage in the design, as `BasicFrame`'s constructor does after its panels, and prints the rows (the
  configuration ids, raw names and overrides, column 0), the three sub-tabs (title, enabled, the current one), the
  twelve buttons' enabled states by the Java fields that hold them, whether the Motors panel shows its help text
  instead of its table, the three tables as `FlightConfigurationTablesProbe` prints them (every header, every
  cell's texts and style with the flags the shading follows, through the tables' own renderers) with the rows and
  columns each table selected, the motor mount table, and the design's selection after the panel's
  `takeTheSpotlight()` (what `BasicFrame` does when the tab is chosen); then whether that changed the design (the
  generator stops if it did: it never does). The panel makes a `MotorChooserDialog`, a `JDialog`, which a
  headless AWT refuses, so the probe runs under `xvfb-run` with `-Djava.awt.headless=false`; no window is shown.
  The header reuses the cell types of `FlightConfigurationTablePins.h` and its generator the `flight-config`
  generator's helpers, its rows of those types too (`configuration_row()`, `motor_cell_row()`,
  `value_cell_row()`, `motor_mount_row()` of `gen_flight_configuration_table_pins.py`, the one place that knows
  the order of their fields, which both headers write positionally): a change of those structs regenerates both
  families (`generate.sh flight-config open-designs`); the test applies QtRocket's fix of bug 1 (plan 10.3: a recovery or stage cell without a
  value of its own is shaded, which OpenRocket never shades in a loaded design) as
  `flight_configuration_tables_golden_tests.cpp` does.
  Its second probe, `OpenDesignsPartsLibraryProbe`, opens OpenRocket's parts library
  (`ComponentPresetChooserDialog`) for each component of the 16 examples that takes presets (222 components), as
  the "Parts Library" button of the component's configuration dialog opens it (`RocketComponentConfig.selectPreset()`:
  its owner a configuration dialog of a main window), never shown, over the bundled presets (`data/components`
  walked as `preset-chooser` walks it, bound as the application's `ComponentPresetDao`, so that the examples' six
  presets are found among them), with `SwingPreferences` at their defaults (the default filters), put back before
  each dialog; it records what the dialog would apply through its `PresetModel`, as `PresetChooserProbe` does (it
  never applies anything). It settles each design as the golden harness settles it (`AutomaticDimensions`, as
  `motor-chooser` does): right after a load a body tube's automatic radius can still be the stored one ("Dual
  parachute deployment": 0.025 m where the settled design has 0.028321 m), and OpenRocket's window paints the
  design and its overlay, which refreshes it, before any dialog can be opened (a scratch probe gave the settled
  value after a `RocketFigure` painted and the overlay's physics). It prints, per component, its place in the
  rocket's iteration order, its name, type and the preset its combo box shows, the "Show Legacy Database" box and
  the text of "Filter by text" (the other default filters), the "Match fore diameter" and
  "Match aft diameter" boxes the dialog shows with their filters' diameters and tolerances (QtRocket's fixed filter
  for a flipped nose cone behind a component of other radii, bug 16, as `PresetChooserProbe` does: none of the
  examples has one), "Always open this dialog", the table's view as it opens (its rows and the MD5 of their keys,
  `ComponentPresetTableProbe.viewKey()`) with its first five rows' shown cells, and the view with the ticked
  diameter boxes unticked. The generator shares the first rows of dialogs whose first rows are equal (140 rows
  for the 222 dialogs). `OpenDesignsExitGoldenPartsLibrary` opens each of them from QtRocket's dialog.
- `plot-widgets`: the widgets of the plot tab's rows (Milestone 4), as OpenRocket's own Swing classes make them,
  headless: the data type combo box (`GroupableAndSearchableComboBox` as `PlotTypeSelector` makes it over
  `FlightDataType.ALL_TYPES`, then over a few types that compare equal, and the domain type selector of the
  `PlotPanel` of `PlotTypeSelectorTest`): its placeholder, its MigLayout constraints, its tooltip, its model's
  order (a `HashSet`'s), its groups popup and `filter()` searches (read by reflection, as `material-combo` reads
  them) and its item events after `setSelectedItem()`; `UnitSelector(UnitGroup)`: its text, selected unit,
  `setSelectedUnit()` and `setUnitGroup()` with their item events and refusals; the line style combo box of
  `SimulationPlotTypeSelector`: its items, what its renderer's `LineStyleIcon` paints into a recording
  `Graphics2D` (`shared/probe/DelegatingGraphics2D`) and its events; and `ColorChooserButton`: its `ColorIcon`,
  painted enabled and disabled, and its property change events. It builds no design (its UUIDs are seeded all
  the same) and loads no motors. The tests pin the whole transcript by hand (`DataTypeComboBoxTests.cpp`,
  `UnitSelectorTests.cpp`, `LineStyleComboBoxTests.cpp`, `ColorChooserButtonTests.cpp` of `tests/gui/widgets/`).
- `simulation-table`: OpenRocket's simulations table (Milestone 4: the "Flight simulations" tab), through its real
  `SimulationPanel` (its `SimulationTableModel`, `ColumnTable` and `ColumnTableRowSorter`, read by reflection),
  never shown; its `RocketActions` ask the toolkit for the menu shortcut key, so the probe runs under `xvfb-run`
  with `-Djava.awt.headless=false`. It takes a list of designs in `open-designs/designs.py`'s format (name, kind,
  file): `generate.sh` passes the whole list, every design of `OpenDesignsExitGolden` (the sixteen examples, the
  49 re-saves and the GUI reference), and the probe loads each kind as that suite loads it, as
  `OpenDesignsTabProbe` does: the bundled motors, `TestRockets`' seven synthetic ones for the test rockets'
  re-saves, the six presets of the examples from `data/components`; `SwingPreferences` with `MotorNameColumn`
  false; FlatLaf Light; every design's UUIDs seeded from its name. For each design and a new
  one it prints the model's columns (id, header, tooltip, unit), the view's widths, every row's status (with its
  icon's description, the status label's sort text, `getStatusDescription()` and the cells' tooltip), its
  warnings (the counts, the box's labels with each icon named by its level, the tooltip, each warning), its cells,
  the orders of one and two `toggleSortOrder()` of each column, the panel's twelve actions with nothing, row 0
  and rows 0 and 1 selected, and the texts of `SimulationTableCSVExport.generateCSVData()` (",", 3 decimals;
  ";", 2 decimals, exponential) and of the Copy of every row (`addStatusColumn()`). Then two status scenarios of
  edits (the M4 sim-tab scout's, on "A simple model rocket" and "Two stage high power rocket": options changes, a
  configuration renamed and selected, a motor change, a body tube change with its undo and redo, a colour, a run
  of "Simulation 1" (the only fresh flight QtRocket's tests may make of the design: plan 11.6), a new simulation
  with its undo and redo, motors removed, motors that never ignite and a run that aborts) with every status, the
  undo and redo descriptions and the saved state after each step, and the rows and the status and warnings
  columns' orders after the last step (a simulation the scenario ran has no cells, warnings or counts in the
  table: no value of a fresh flight is pinned; the first scenario's warnings orders put its new simulation, which
  has no data, among simulations with data); and a Copy of every row of a table whose rows include one without data. Where QtRocket
  fixes one of OpenRocket's bugs the table holds QtRocket's answer, derived with OpenRocket's own data, and
  OpenRocket's beside it: the warnings column's orders (bug 23: by the critical, then the normal, then the
  informational count, from OpenRocket's `WarningSet`, a row without data first ascending and last descending,
  as `DefaultRowSorter` sorts a missing value; OpenRocket's own orders, which
  keep the model's order, under `openRocket:warnings`) and the Copy whose statuses OpenRocket pairs with the
  lines by position (bug 25: QtRocket's pairs each line with its own simulation; OpenRocket's own under
  `openRocketCopy`). The table is JSON (one design, scenario or row per line), read once per test process by
  `tests/gui/simulation/SimulationTablePins.h`. Its second probe, `SimulationTableViewProbe`, has no table: on
  real `SimulationPanel`s (under `xvfb-run`, with `SimulationTableProbe`'s bootstrap, never shown) it prints the
  column chooser's steps (its `ColumnVisibilityController`'s private `toggleColumn()` and
  `saveHiddenColumnsToApplicationPreferences()`, called by reflection as the menu's items call them, on new and
  loaded designs with and without stored choices, moved columns, every column hidden, one column left: the
  document preference `simulation.table.hiddenColumns`, the application preference read from its
  `java.util.prefs` node, since `SwingPreferences.getString()` stores the default it is given, the saved state,
  the hidden ids and the view's columns) and its menus (`createMenu()`), the table's key bindings and what its
  Enter and Shift+Enter do (`selectNextRowCell`, `selectPreviousRowCell` run through the action maps from
  selections made with `changeSelection()`), and the sort keys after mouse events dispatched to the header. The
  tests pin its whole transcript by hand (`SimulationColumnVisibilityTests.cpp`, `SimulationTableViewTests.cpp`).
- `simulation-dialog`: OpenRocket's simulation settings dialog (`SimulationConfigDialog`, Milestone 4, run 3,
  part Y2, the exit check E4), over every simulation of every design of `OpenDesignsExitGolden` (the list
  `open-designs/designs.py` writes), each design loaded as `simulation-table` loads it (the same motors, presets,
  `MotorNameColumn` false, FlatLaf Light, the UUIDs seeded from the design's name) and left as `OpenFileWorker`
  leaves a document File > Open loads (its file set, saved), with one `SwingPreferences` (`Scopes.SINGLETON`, as
  OpenRocket's `GuiModule` binds it). For each simulation in the document's order, on the one document, it makes
  the dialog the tab's Edit opens (`SimulationPanel.openDialog()`: the settings tab; never shown, so the probe runs
  under `xvfb-run`) and prints its title, its walk in the form of `tools/openrocket-probes/simulation-options`'s
  and `simulation-warnings`' walks (the top rows, the line of the tabs, each tab's title with the pages of the
  Launch conditions, Simulation options and Warnings tabs, the buttons), each tab's title, enabled state and
  tooltip, the icons of each extension's buttons, and for a multi-level wind the wind editor's table (the dialog
  "Edit levels" opens, `MultiLevelWindEditDialog`, made over the settings dialog, never shown: the
  `tools/openrocket-probes/wind-editor` probe's HEADER, ROW and MODEL lines); then whether the dialog was modified,
  its Cancel (`doClick()`; a dialog that would ask its discard question fails the probe, as the question would
  block), the change events every simulation fired meanwhile, and the document's undo state (its private undo
  fields, read by reflection), saved state and statuses before the dialog and after its Cancel. The table is JSON:
  the walks share their lines ("lines" lists each distinct line once, a walk is the indices of its lines: 196
  walks of about 23,000 lines, of which a few hundred differ), one simulation per line; read once per test
  process by `tests/gui/dialogs/simulation/SimulationDialogPins.h`.
- `plot-dialog`: OpenRocket's plot dialog (`PlotDialog` and `SimulationPlotDialog`, Milestone 4, run 2, part
  G4), made through `SimulationPlotDialog.getPlot()` as the plot tab's `doPlot()` makes it, over stored flights of
  the examples ("A simple model rocket" #1 under the first preset and the drag coefficients over the Mach number,
  "Three stage low power rocket" #1, and a copy of that flight with an abort added to its first and last stage):
  the window (title, modality, close operation, Escape, the default button), the panel's components in order
  (kind, text, icon, visibility, state, MigLayout constraints, the stage box's items), the chart's size after
  `pack()`, the gap below it, the row's order and gaps (after the layout that hides "Show errors"), then what
  "Show data points", "Show events", "Show errors", each stage and the zoom buttons (with and without Alt) do to
  the chart and the preferences, and what the dialog remembers of its size and place. It prints nothing that
  depends on the fonts (no bounds but the chart's; the ranges after a zoom, which ChartPanel computes in pixels,
  to 6 digits). The dialog is a `JDialog`, which a headless AWT refuses, so the probe runs under `xvfb-run` with
  `-Djava.awt.headless=false`; no window is shown. Its second probe, `PlotTabProbe` (part G5), prints OpenRocket's
  plot tab (`PlotPanel`, `PlotTypeSelector`, `SimulationPlotPanel` and `SimulationPlotTypeSelector`) made by
  `SimulationPlotPanel.create()` of "A simple model rocket" #1, "Three stage low power rocket" #1 and the flight
  of `SimulationChartTest`'s colour test, in a `JFrame` that is packed and never shown: the panel's components with
  their MigLayout constraints, the gaps between them after `pack()` (no bounds that depend on the fonts), the
  preset box's items, the rows (type, unit, axis, colour, line style, the appearance buttons' states), the events
  table, the marker style and the description; then what each preset, the X axis type and unit, a row's type,
  unit, axis, colour and line style, New Y axis plot type, the remove buttons, All, None, a click on an event's
  name, the marker style, Set as default, Reset to factory default and `doPlot()` do, set as Swing sets them
  (`setSelectedItem()`, `setSelectedIndex()`, `doClick()`), with the configuration the next tab starts from.
  Its transcript is appended to `PlotDialogProbe`'s. The tests pin the whole transcript by hand.

The two component generators hold QtRocket's deliberate differences from OpenRocket's transcripts in their
`EDITS` tables (30 rows: the selection after a Cut or a Delete, which the probe's tree-less selection model
cannot show; the user's decision U4 for the Cut of every stage and for drops; the disabled moves, which the
probe calls all the same; the dragged components in tree order), each with the lines it replaces, the comment
the test file has above the row and the reason. An edit whose OpenRocket line is no longer in the transcript
stops the generator: decide the row again.

The headers record the OpenRocket commit (`k...Commit`). `ribbon_golden_tests.cpp`,
`rocket_info_golden_tests.cpp` and `rocket_figure_paint_golden_tests.cpp` compare theirs with the literal
commit 5f164fd0e05d6e188857b0185601293b7a6e9eaf; no test reads `kRocketPanelPinsCommit`
(`RocketPanelPins.h`) or `kOpenRocketCommit` (`PreferencesPagePins.h`), and the tables in test files record
none. The script refuses another commit than the manifest's (below), so a table cannot silently come from
another one.

## Regenerating

Requirements:

- Linux. The probes get a fresh preferences store through `java.util.prefs.userRoot`, which only Linux's
  `FileSystemPreferences` reads: macOS keeps the store in a plist and Windows in the registry, where the
  probes' `clearPreferences()` and `storeAndApplyPreferences()` would act on your own OpenRocket preferences,
  so the script refuses to run elsewhere (OpenRocket's `-Dopenrocket.debug.prefs` is no way out: every
  `SwingPreferences` made under it removes the node the earlier ones hold, and three probes fail). The
  committed tables were made on x86_64: HotSpot's `Math.sin` and `Math.cos` intrinsics may differ in the last
  bit on another architecture, which changes the rotated scenarios' doubles (the script warns).
- JDK 17 (the script refuses another version: `Double.toString` changed in JDK 19).
- A checkout of OpenRocket at the commit of `tests/data/goldens/manifest.json` (5f164fd0e when the tables were
  written), with no modified tracked file and its preset submodule initialized at the commit the
  superproject records: clone with `--recurse-submodules`, or run `git submodule update --init` (never
  `--remote`; OpenRocket's own build runs `git submodule update --remote` when the submodule is missing,
  which moves it). The script refuses another commit (`ALLOW_OTHER_COMMIT=1` overrides; the headers then
  record the other commit and their tests fail until they are updated) and a modified checkout
  (`ALLOW_DIRTY=1`).
- OpenRocket's classes built from that checkout: the script reads `build.version` from core's
  `build.properties` on the class path (`<version>-SNAPSHOT-<short commit>`, stamped by OpenRocket's build)
  and refuses classes of another commit (`ALLOW_OTHER_COMMIT=1` overrides).
- `python3`, `clang-format` and, for `rocket-panel`, `thrust-curves`, `flight-config`, `preset-chooser`,
  `open-designs`, `simulation-table`, `simulation-dialog` and `plot-dialog`, `xvfb-run`.

```sh
OPENROCKET_DIR=/path/to/openrocket OPENROCKET_CLASSPATH=... tools/openrocket-gui-pins/generate.sh        # every family
OPENROCKET_DIR=/path/to/openrocket OPENROCKET_CLASSPATH=... tools/openrocket-gui-pins/generate.sh ribbon # some of them
OPENROCKET_DIR=/path/to/openrocket OPENROCKET_CLASSPATH=... tools/openrocket-gui-pins/generate.sh --check
```

`OPENROCKET_CLASSPATH` is the runtime class path of OpenRocket's swing module. The committed tables were
checked with one made by hand from a checkout built before: `core/build/classes/java/main`,
`core/build/resources/main`, `core/libs/script-api-1.0.jar`, `swing/build/classes/java/main`,
`swing/src/main/resources`, then every jar of `~/.gradle/caches/modules-2/files-2.1` whose path names one of
OpenRocket's dependencies (Guice, SLF4J, Logback, Jakarta Inject, AOP Alliance, Guava, Apache Commons,
OpenCSV, ICU4J, ClassGraph, JTS, de.javagl, ASM, Jakarta Activation, JAXB, Eclipse, Gson, SQLite (xerial),
CommonMark, GraalVM, Jakarta XML Bind, Jakarta JSON, istack, Error Prone, FindBugs, FlatLaf, MigLayout,
JFreeChart, iText, RSyntaxTextArea (fifesoft), weisj, g00fy2, Dansoftowner, JOML, LWJGL, LWJGLX, JNA, OSHI,
jangassen), sorted, without sources and javadoc jars. Without it the script runs Gradle in the checkout, as
`tools/openrocket-panel-pins/generate.sh` does (`./gradlew :swing:classes`, then a task of a temporary init
script prints the class path); that route was never run for these tables (it builds inside the checkout and
needs the submodule as above).

For each family the script compiles the probe with OpenRocket's `ServicesForTesting` (core's and swing's,
from their test sources) and `shared/`, against that class path, and runs it with a fixed locale, time zone
and encoding, a fresh `java.util.prefs` store and home directory, OpenRocket's bundled motor database
(`-Dprobe.motors`) and OpenRocket's log on standard error (`logback.xml`), the transcript on standard output.
The generator writes the table into the work directory (a spliced test file into a copy of the committed
one, which `clang-format` formats with the repository's `.clang-format`); a table equal to the committed one
byte for byte (`cmp`) leaves it as it is, another one replaces it, and `--check` only reports (exit status 1
when a table differs). The head of `generate.sh` lists the other environment variables (`CLANG_FORMAT`,
`GUI_PINS_WORK`; a temporary work directory is kept when the script fails). All sixteen families take a few
minutes.

The tables in test code: `scale-figure` replaces the rows of `kFigureRows` (a "clang-format off" block),
`material-binding` the bodies of `kBulkRows`, `kBulkGroups`, `kLineRows`, `kLineGroups`, `kSurfaceRows` and
`kSurfaceGroups` (with their sizes; all six must be in the transcript), `component-editing` the bodies of
`makeActionsScenarios()` and of `clipboardScenarios()`'s `kScenarios`, `component-drop` that of
`dndScenarios()`'s `kScenarios` and `actions-rules` that of `kTranscripts` (with its size), then the file is
formatted. `MaterialComboBoxTests.cpp` assembles its tables by hand from `rows.py`'s lines, so
`material-combo` only checks them, exactly and in both directions: the "Doc foam" popup's group lines are
`kDocFoamGroups` and its searches `docFoamSearches()` (each text with its rows, in order) and
`kSearchNylon`; the "Balsa" and loose popups' lines are `kDocFoamGroups`' with their own Woods and Foams
lines (`kBalsaWoods`, `kBalsaFoams`, `kLooseWoods`, `kLooseFoams`), and their searches `kSearchBalsa` and
none. A change has to be made in the test by hand.

## Values pinned by hand

Three probes print more than their tables: `ScaleProbe` (everything but its `== figure` lines), `B2Probe`
(everything but the rows and groups of the six tables) and `B4Probe` (everything but the popups' menus and
searches). Tests pin those values by hand, as listed above, so they cannot be regenerated; the family keeps
the lines as the probe printed them when the tests were written in `hand-pinned.txt`
(`shared/check_hand_pins.py`), and every run compares the probe's lines with it, in write mode too. A
difference is reported (exit status 1) and never written: review the tests that pin the changed lines, then
copy the new lines (`<work>/<probe>.hand-pinned.txt`) over `hand-pinned.txt`. The three snapshots equal the
lines of the run-2 transcripts the tests were written from (`b2-probe.out`, `b4-probe.out` and G1's
`out.txt` in the Milestone 2 handoff).

`PlotWidgetsProbe` (`plot-widgets`) has no table either: the four tests above pin its whole transcript by hand,
and its `hand-pinned.txt` is that transcript.

`SimulationTableViewProbe` (`simulation-table`; its other probe, `SimulationTableProbe`, writes the family's
JSON table) has no table either: `SimulationColumnVisibilityTests.cpp` pins its COLS and MENU lines and
`SimulationTableViewTests.cpp` its BIND, KEY and HEADER lines, by hand, and the family's `hand-pinned.txt` is its
whole transcript.

`PlotDialogProbe` and `PlotTabProbe` (`plot-dialog`) have no table either: the tests above pin their whole
transcripts by hand, and the family's `hand-pinned.txt` is the two, `PlotTabProbe`'s after `PlotDialogProbe`'s.

`PresetGuiProbe`, `ConfigPresetRowProbe` and `PresetShownProbe` (`preset-chooser`; its other probe,
`PresetChooserProbe`, writes `PresetChooserPins.h`) have no table: the tests pin their whole transcripts by hand
(`TableUiPreferencesTests.cpp` the `TableUiProbe` part, `PresetBindingTests.cpp` the `PresetModelProbe` part,
`PresetComboBoxTests.cpp` the `PresetComboProbe` part, `TableRowTraversalTests.cpp` the `TableRowTraversalProbe`
part, the `ComponentPreset*Tests.cpp` of `tests/gui/dialogs/preset/` the `ComponentPresetTableProbe`,
`DialogKeysProbe`, `SpaceRobotProbe` and `PresetTableInputProbe` parts,
`tests/gui/configdialog/ComponentConfigDialogTests.cpp` the `NewComponentCancelProbe` part,
`tests/gui/configdialog/RocketComponentConfigTests.cpp` the `ConfigPresetRowProbe` part). The family's one
`hand-pinned.txt` keeps the three transcripts in this order (generate.sh checks them together, as
`PresetHandPins`).

## Reproducibility

Every probe prints the same transcript on every run, so every table regenerates byte for byte.

OpenRocket keys hash maps by components, whose `hashCode()` is that of their UUID, and a design a probe
builds (the `TestRockets` makers, a new design) draws random UUIDs; designs loaded from a file keep the UUIDs
stored in it. Two families print what such maps order: `figure-paint` (for `makeBeta`, the order of the
shapes of equal display order, of the two motors and of the hit points the probe picks from the painted
shapes) and `rocket-info` (the CG and CP are sums over such maps; unseeded, their last bits changed from run
to run, by up to 2 units in the last place, for `makeFalcon9Heavy`, `makeMultiStageEventTestRocket` and
`makeClusterPods`). Their probes install `DeterministicUuids` in `bootstrap()`, before the first UUID, as
`tools/openrocket-goldens` does (a seeded `SecureRandom` provider inserted first, which `UUID.randomUUID()`
then draws from), and reseed it from the scenario's name before each scenario's design, so that a scenario's
UUIDs depend neither on the run nor on the scenarios before it.

Their tables were first written from unseeded runs and rewritten once in the seeded order (the user's
decision of 2026-10-23): 36 lines of `RocketFigurePaintPins.h` changed (18 out, 18 in: the shapes of equal
display order and the hit points of `beta-side`; the two motors kept their order) and 30 of `RocketInfoPins.h`
(15 out, 15 in: 17 values of the three designs above, each 1 unit in the last place). The seeded order is one
of Java's hash orders, not QtRocket's: `rocket_figure_paint_golden_tests.cpp` compares a run of shapes of
equal display order as a set and looks every hit point up on its own, and `rocket_info_golden_tests.cpp`
compares the CG and CP within a relative 1e-9. The figure-paint test compares the motors in order, which holds
because the seeded order of `makeBeta`'s two motors is QtRocket's; another reseed key, a change of `makeBeta`
or another OpenRocket commit may swap them, and that test then fails.

The other nine families of Milestone 2 print nothing such a map orders, and their probes are not seeded: each
printed the same transcript in two unseeded runs and in four runs seeded with different keys (2026-10-23),
where the two seeded probes print another transcript for every other key. The families of Milestone 3 seed
their probes whatever they print (the rule since then): `thrust-curves` builds no design and prints nothing
a hash map orders, and installs `DeterministicUuids` in `bootstrap()` all the same. `flight-config`
reseeds from the scenario's name before each design, and builds each test rocket a second time, from the
key `"<scenario>#drawn"`, first; its operations probe reseeds before each scenario's design too (the new
configurations' ids, which the transcript names only by their row). The operations probe answers its dialogs
from a watcher thread, but what it prints does not depend on when the thread runs: each answer runs on the
event dispatch thread inside the dialog's modal loop, and the transcript holds no time. Two runs gave the
same transcript.
`motor-chooser` loads its designs (their UUIDs come from the files) and reseeds before each one all the same.
`open-designs` reseeds from the design's name before each design, and loads each a first time from the key
`"<name>#drawn"`, to tell a configuration id the loader draws from one the file stores (no row of the 66 designs
is drawn: every id is the file's); two runs gave the same transcript. Its parts-library probe reseeds from the
design's name before each example, as `PresetChooserProbe` reseeds before each scenario (what it prints depends on
no UUID); three runs gave the same transcript.
`simulation-table` (Milestone 4) reseeds from the design's name before each design, from the scenario's before
each scenario (a new simulation and a new design draw UUIDs; the transcript names a configuration id by its first
8 characters, all of them the files'); its one fresh flight's values are not in the table. Two runs gave the same
table. Its `SimulationTableViewProbe` reseeds from the design's file name before each load and before each new
design (it prints no UUID); two runs gave the same transcript.
`simulation-dialog` (Milestone 4) reseeds from the design's name before each design (its loads draw no UUID the
table shows); two runs gave the same table.

The tables are also compared on every platform CI builds, so what they pin must not depend on the platform's
libm. The view hashes of `preset-chooser` did: they hashed the bundled presets' digests, and a digest covers the
preset's material density, which `ComponentPresetFactory` derives for a nose cone with a HAACK or POWER shape and
a mass (the six nose cones of `internal/semroc-legacy.orc`) from the volume of that shape, through `acos`, `sin`
and `pow`. macOS gives BNC-55ACP a density 1 ulp above Linux's and Java's, which changed every nose cone view
that holds it (CI run 40560521356: the 1252 and 725 rows of `PresetChooserPins.h`, the NOSE_CONE view and sorts
of `hand-pinned.txt`). So a view hashes a key per row: the preset's digest, but for a preset of that rule
`<type>|<manufacturer's simple name>|<part number>` (the identity an `.ork` file stores for a preset, without the
digest): `ComponentPresetTableProbe.viewKey()` here, `presetViewKey()` in
`tests/gui/dialogs/preset/PresetTableTestSupport.h`, whose rule is the core preset golden's
`hasTranscendentalDensity()` (which compares those six densities within a tolerance and not their digests,
decision O-M3-14). Every other digest comes from `+ - * /`, `sqrt` and the files' texts. The six part numbers
differ, so a hash still sees each row's place. No view order or text filter depends on such a value: the only
cell that shows one, the Material's, rounds the density to 0.001 g/cm³, and none of the six lies within 3e-5
g/cm³ of a rounding boundary.

## History

The probes and generators were written by Milestone 2's parts G1 (`scale-figure`), G2 (`figure-paint`), G3
(`rocket-info`), G4 (`ribbon`), B2 (`material-binding`), B4 (`material-combo`), T3 (`ClipboardProbe` and the
component tables' generators) and the fixer of group T (`actions-rules`) of run 2, P1 (`rocket-panel`) and P2
(`preferences`) of run 3, and the M2 component-tree scout (`ActionsProbe`, `DndProbe`), and moved here in run
4, unchanged but for: the probes' package (`probe`); the motor database comes from `-Dprobe.motors` (it was a
path of the author's machine); OpenRocket's log goes to standard error, so the transcripts need no filtering;
the generators format with the repository's `.clang-format` wherever they write, name this directory in the
headers they write and read their probes' sources from it; the two component generators carry the run-2
amendments of the tables as `EDITS`; `figure-paint` and `rocket-info` seed OpenRocket's UUIDs. Every table
regenerated as committed, but for those two families, which until then compared apart from the unseeded
runs' noise (`compare_paint_pins.py`, `compare_info_pins.py`): their tables were rewritten once in the seeded
order, and they compare byte for byte like the others ("Reproducibility").

Milestone 3 added `thrust-curves` (run 1, part K5, from the M3 plot scout's `ThrustCurveChartProbe`): the
first family of a probe that runs under `xvfb-run` besides `rocket-panel`, and the first whose table pins
every motor set of the bundled database. The run's fixer had its generator write each table as a
`std::array` of a stated size instead of through `std::to_array` (the repository's other tables of hundreds
of rows are written so; MSVC had never evaluated a `std::to_array` of over a thousand rows), the rows
unchanged. Run 2's part M4 added `motor-chooser`, the first family whose probe runs OpenRocket twice, once as
it is and once with a QtRocket fix made from outside.

Run 2 added `flight-config` (part F2, from the M3 config-tab scout's `NamesProbe`, run on OpenRocket's own
table models and renderers instead of copies of their code, its output tab-separated, as the names hold
" | "), and its part 2 (part F3, from the scout's `TabProbe`: its scenarios split so that a step where
QtRocket's fix of an OpenRocket bug shows ends its scenario, the dialogs' answers read back from the dialogs,
the events' origins, the bug 5 and bug 6 EDITS, and the whole state after every step).

`preset-chooser` came in run 2 (group S): parts S1 to S3 wrote `PresetGuiProbe` and its five probes, whose
values the tests pin by hand; part S4 added `PresetChooserProbe` (from the M3 presets scout's
`PresetChooserProbe`, seeded, its scenarios kept and its transcript made a table) and `PresetChooserPins.h`;
part S5 added `NewComponentCancelProbe` to `PresetGuiProbe`. The group's fixer committed the scratch probes of
parts S4 and S5 that tests pinned by hand (`ConfigPresetRowProbe`, `DialogKeysProbe` and `SpaceRobotProbe`, their
transcripts as the parts printed them, under a section line each) and added `PresetTableInputProbe` and `PresetShownProbe`, with
`ShownChooser` for what the shown probes share. After the run's merge, macOS CI failed the nose cone views that
hold BNC-55ACP, one of the six presets whose digest is not the same on every platform, and the views took a key
per row instead of the digest ("Reproducibility"): the section line and 21 MD5s on 13 lines of `hand-pinned.txt`
and 9 views of `PresetChooserPins.h` changed.

Run 3's part Y1 added `open-designs` (from the M3 tests-exit scout's `ConfigTablesProbe`, which built OpenRocket's
real panel without a frame over the examples and the test rockets' makers: generalised to the exit suite's 66
loaded designs in their environments, seeded, with the panel of a `BasicFrame` as the `flight-config` operations
probe makes it, the design's selection after the tab took the spotlight, and the tables printed as
`FlightConfigurationTablesProbe` prints them). Part Y2 added its second probe, `OpenDesignsPartsLibraryProbe` (the
M3 tests-exit scout's E4: the parts library of each component of the examples, `PresetChooserProbe`'s dialog
made for the loaded and settled designs over the bundled presets). Its review moved the rows of the structs both
`flight-config` and `open-designs` write into the `flight-config` generator's row functions (both headers stayed
byte for byte the same).

Milestone 4's run 2, part T1, added `simulation-table` (from the M4 sim-tab scout's `SimTabProbe`: its table and
status scenarios, taking a list of designs, seeded, the scenarios' runs moved to the one simulation tests may run,
QtRocket's answers for bugs 23 and 25 beside OpenRocket's), the first family whose table is JSON
(`tests/data/goldens/gui/`) rather than a C++ header. Part T2 added its `SimulationTableViewProbe` (the column
chooser, the keys and the header of the table view; pinned by hand). Its review gave QtRocket's warnings order a
row without data last when descending (as `DefaultRowSorter` and QtRocket sort a missing value) and the scenarios
the warnings column's orders (the designs' rows stayed byte for byte the same). Run 3's part Y1 passed it the
whole list of `OpenDesignsExitGolden`'s 66 designs (the sixteen examples' tables stayed byte for byte the same), for
the suite's test of the "Flight simulations" tab.

Milestone 4's run 3, part Y2, added `simulation-dialog` (the M4 tests-exit scout's E4; the walk of the M4
sim-config scout's `SimConfigDialogWalk` in the form of run 2's probes `simulation-options` and
`simulation-warnings`, over every simulation of `OpenDesignsExitGolden`'s 66 designs as the tab's Edit opens it,
then cancelled), for the suite's test of the settings dialog of every simulation.

The other generators of GUI-related tables: `tools/openrocket-panel-pins` (the configuration panels' pins),
`tools/openrocket-goldens/figure-pins` (the core's figure shapes), `tools/openrocket-component-factory`,
`tools/openrocket-fin-import`, `tools/openrocket-expression-parser` and `tools/openrocket-gui-reference`.
Probes whose output tests pin by hand without a table are in `tools/openrocket-probes`.
