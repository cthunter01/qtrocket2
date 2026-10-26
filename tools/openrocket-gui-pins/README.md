# What OpenRocket's GUI shows: the pin tables of the GUI tests

`tools/openrocket-gui-pins` runs small Java probes against OpenRocket's own Swing classes and turns what
they print into the tables QtRocket's GUI tests compare with. The tables are committed; neither CMake nor
CI ever needs Java. They change only through this tool. Each subdirectory is one family: its probe
(`probe/*.java`, package `probe`) and the Python script that writes the table.

| Family | Table | Probe | Generator | Read by |
|---|---|---|---|---|
| `figure-paint` | `tests/gui/figure/RocketFigurePaintPins.h` | `FigurePaintProbe` | `gen_rocket_figure_paint_pins.py` | `rocket_figure_paint_golden_tests.cpp` |
| `rocket-info` | `tests/gui/figure/RocketInfoPins.h` | `RocketInfoProbe` | `gen_rocket_info_pins.py` | `rocket_info_golden_tests.cpp` |
| `ribbon` | `tests/gui/widgets/RibbonPins.h` | `RibbonProbe` | `gen_ribbon_pins.py` | `ribbon_golden_tests.cpp` |
| `rocket-panel` | `tests/gui/figure/RocketPanelPins.h` | `RocketPanelProbe` | `gen_rocket_panel_pins.py` | `rocket_panel_golden_tests.cpp` |
| `preferences` | `tests/gui/dialogs/PreferencesPagePins.h` | `PreferencesProbe` | `gen_preferences_pins.py` | `preferences_pages_golden_tests.cpp`, `PreferencesDialogTests.cpp` |
| `scale-figure` | `kFigureRows` in `tests/gui/figure/ScaleFigureTests.cpp` | `ScaleProbe` | `figure_rows.py`, `splice_figure_rows.py` | that file |
| `material-binding` | the six row and group tables of `tests/gui/binding/MaterialBindingTests.cpp` | `B2Probe` | `rows.py`, `splice_material_rows.py` | that file |
| `material-combo` | the groups and searches of `tests/gui/widgets/MaterialComboBoxTests.cpp` (checked, not written) | `B4Probe` | `rows.py`, `check_combo_rows.py` | that file |
| `component-editing` | `makeActionsScenarios()` and `clipboardScenarios()` in `tests/gui/main/ComponentEditingTests.cpp` | `ActionsProbe`, `ClipboardProbe` | `gen_component_editing.py` | that file |
| `component-drop` | `dndScenarios()` in `tests/gui/tree/ComponentDropTests.cpp` | `DndProbe` | `gen_component_drop.py` | that file |
| `actions-rules` | `kTranscripts` in `tests/gui/main/RocketActionsTests.cpp` | `ActionsRulesProbe` | `gen_actions_rules.py` | that file |
| `thrust-curves` | `tests/gui/plot/ThrustCurvePins.h` | `ThrustCurvesProbe` | `gen_thrust_curve_pins.py` | `thrust_curve_plot_golden_tests.cpp` |

`shared/probe/DelegatingGraphics2D.java` is a `Graphics2D` that forwards every call to another one (written
once from the JDK's `Graphics2D` with a reflection helper that is not kept; edit it by hand); `figure-paint`
and `rocket-info` record what OpenRocket paints through it. `shared/probe/DeterministicUuids.java` makes
OpenRocket's UUIDs reproducible (see "Reproducibility"). Every probe is compiled with both.

## What the probes pin

Every probe starts OpenRocket with core's or swing's `ServicesForTesting`. Those of the Swing views
(`figure-paint`, `rocket-info`, `ribbon`, `rocket-panel`, `preferences`, `thrust-curves`) bind
`SwingPreferences`, at their defaults (a fresh `java.util.prefs` store), and apply FlatLaf Light, as the
application does (`thrust-curves` switches to FlatLaf Dark for its dark charts and back); `scale-figure`
binds `SwingPreferences` without a theme; the two material probes keep `ServicesForTesting`'s preferences, the
four component probes swing's. All but the material, component and `preferences` probes load OpenRocket's
motor database and no component presets. `figure-paint`, `rocket-info`, `ribbon` and `rocket-panel` settle a
design they load from `data/examples` or build with `TestRockets` as the golden harness settles it (a copy of
`tools/openrocket-goldens`' `AutomaticDimensions`). Every double is printed exactly (`Double.toHexString` or
Java 17's `Double.toString`).

- `figure-paint`: OpenRocket's `RocketFigure` painted headless, as `RocketPanel.create2DPreviewFigure()`
  makes it (96 dpi, fitted to 1000 x 700), into a recording `Graphics2D`: every `draw` and `fill` with its
  colour, stroke and device bounds, the CG and CP carets where `RocketPanel.updateExtras()` puts them, and
  `getComponentsByPoint()` at points of the painted figure. 20 scenarios: "A simple model rocket" in seven
  states (side, selected, back, top at 30 degrees, side at 30 degrees, line styles and colours, a hidden
  body tube), pods (side, back, ShowMarkers), clustered motors, parallel boosters, dual parachutes, two
  stages, tube fins, `TestRockets.makeBeta` and a new design.
- `rocket-info`: `RocketInfoContextHelper.computePhysics()` and `RocketInfo.paint()` (the overlay of the
  design view) on the 16 examples, the 13 test rockets and a few designs of its own: every text with its
  font, colour and row, the carets, CG and CP, the warnings.
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
  (`user.home`, a fresh one in the work directory) is written `<home>`.
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
- `python3`, `clang-format` and, for `rocket-panel` and `thrust-curves`, `xvfb-run`.

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
`GUI_PINS_WORK`; a temporary work directory is kept when the script fails). All twelve families take about a
minute.

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
a hash map orders, and installs `DeterministicUuids` in `bootstrap()` all the same.

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
unchanged.

The other generators of GUI-related tables: `tools/openrocket-panel-pins` (the configuration panels' pins),
`tools/openrocket-goldens/figure-pins` (the core's figure shapes), `tools/openrocket-component-factory`,
`tools/openrocket-fin-import`, `tools/openrocket-expression-parser` and `tools/openrocket-gui-reference`.
Probes whose output tests pin by hand without a table are in `tools/openrocket-probes`.
