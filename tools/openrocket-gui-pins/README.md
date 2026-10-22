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
| `material-combo` | the texts of `tests/gui/widgets/MaterialComboBoxTests.cpp` (checked, not written) | `B4Probe` | `rows.py`, `check_combo_rows.py` | that file |

`shared/probe/DelegatingGraphics2D.java` is a `Graphics2D` that forwards every call to another one (written
once from the JDK's `Graphics2D` with a reflection helper); `figure-paint` and `rocket-info` record what
OpenRocket paints through it. Every probe is compiled with it.

## What the probes pin

Every probe starts OpenRocket with core's `ServicesForTesting`. Those of the Swing views (`figure-paint`,
`rocket-info`, `ribbon`, `rocket-panel`, `preferences`) bind `SwingPreferences`, at their defaults (a fresh
`java.util.prefs` store), and apply FlatLaf Light, as the application does; `scale-figure` binds
`SwingPreferences` without a theme; the two material probes keep `ServicesForTesting`'s preferences. All
but the material probes and `preferences` load OpenRocket's motor database and no component presets.
`figure-paint`, `rocket-info`, `ribbon` and `rocket-panel` settle a design they load from `data/examples` or
build with `TestRockets` as the golden harness settles it (a copy of `tools/openrocket-goldens`'
`AutomaticDimensions`). Every double is printed exactly (`Double.toHexString` or Java 17's
`Double.toString`).

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
  out-of-range values and the units node after `storeAndApplyPreferences()`. The generator replaces the
  user's home directory by `<home>`.
- `scale-figure`: `RocketFigure`'s fit scale, user scale, preferred size, size, subject origin and auto-zoom
  point for "A simple model rocket", "Two stage high power rocket" and a new `Rocket`, in each view, for seven
  visible areas. The probe also prints the scale selector's items and texts and the ruler's ticks, which
  `ScaleSelectorTests.cpp` and `RulerTests.cpp` pin by hand.
- `material-binding`: `MaterialModel` over a new design whose application and document hold extra
  materials: the combo box rows and the groups of the bulk, line and surface materials. The probe also
  prints the values `IntBindingTests.cpp`, `BoolBindingTests.cpp`, `EnumBindingTests.cpp` and
  `TransitionShapeBindingTests.cpp` pin by hand.
- `material-combo`: `MaterialComboBox.createComboBox()`: its groups popup with OpenRocket's check marks and
  its `filter()` searches, with "Doc foam", "Balsa" and a loose material selected. The probe also prints
  the enum texts `EnumComboBoxTests.cpp` pins by hand.

The headers record the OpenRocket commit (`k...Commit`), which their golden tests check against
`tests/data/goldens/manifest.json`; the three test files do not.

## Regenerating

Requirements: JDK 17 (the script refuses another version: `Double.toString` changed in JDK 19), a checkout of
OpenRocket at the commit of `tests/data/goldens/manifest.json` (5f164fd0e when the tables were written; the
script refuses a modified checkout unless `ALLOW_DIRTY=1`), `python3`, `clang-format` and, for
`rocket-panel`, `xvfb-run`.

```sh
OPENROCKET_DIR=/path/to/openrocket tools/openrocket-gui-pins/generate.sh                    # every family
OPENROCKET_DIR=/path/to/openrocket tools/openrocket-gui-pins/generate.sh ribbon preferences # some of them
OPENROCKET_DIR=/path/to/openrocket tools/openrocket-gui-pins/generate.sh --check            # compare only
```

For each family the script compiles the probe with OpenRocket's `core/src/test/.../ServicesForTesting.java`
and `shared/`, against the runtime class path of OpenRocket's swing module, and runs it with a fixed locale,
time zone and encoding, a fresh `java.util.prefs` store, OpenRocket's bundled motor database
(`-Dprobe.motors`) and OpenRocket's log on standard error (`logback.xml`), the transcript on standard
output. The generator writes the table into the work directory (a spliced test file into a copy of the
committed one, which `clang-format` formats with the repository's `.clang-format`); a table equal to the
committed one leaves it as it is, another one replaces it, and `--check` only reports (exit status 1 when a
table differs). The class path is `OPENROCKET_CLASSPATH` when set (the committed tables were checked so: the
classes and resources of a checkout built before, `core/build/classes/java/main`, `core/build/resources/main`,
`core/libs/script-api-1.0.jar`, `swing/build/classes/java/main`, `swing/src/main/resources`, and the jars of
Gradle's cache); otherwise Gradle builds OpenRocket's swing module and prints it, as
`tools/openrocket-panel-pins/generate.sh` does. The head of `generate.sh` lists the other environment
variables (`CLANG_FORMAT`, `GUI_PINS_WORK`). All eight families take under a minute.

The three tables in test code: `scale-figure` replaces the rows of `kFigureRows` (a "clang-format off"
block) and `material-binding` the bodies of `kBulkRows`, `kBulkGroups`, `kLineRows`, `kLineGroups`,
`kSurfaceRows` and `kSurfaceGroups` (with their sizes), then formats the file. `MaterialComboBoxTests.cpp`
assembles its tables by hand from `rows.py`'s lines (single lines of a popup, the searches in a function),
so `material-combo` only checks that every group line of the probe is one string of the file and that
every search's rows follow one another there; a change has to be made in the test by hand.

## Reproducibility

Six probes print the same transcript on every run. Two do not, because OpenRocket's hash maps are keyed by
components whose `hashCode()` is that of their UUID, which is random for a design the probe builds (the
`TestRockets` makers, a new design); designs loaded from a file keep the UUIDs stored in it:

- `figure-paint`: for `makeBeta`, the order of the shapes of equal display order, of the two motors and of
  the hit points the probe picks from the painted shapes changes from run to run.
  `rocket_figure_paint_golden_tests.cpp` compares a run of shapes of equal display order as a set and looks
  every hit point up on its own, but compares the motors in order: the committed table has them in the
  order QtRocket draws them.
- `rocket-info`: the CG and CP are sums over such maps, so their last bits change (seen: up to 2 units in
  the last place, for `makeFalcon9Heavy` and `makeClusterPods`); `rocket_info_golden_tests.cpp` compares
  them within a relative 1e-9.

So these two families compare apart from that noise (`figure-paint/compare_paint_pins.py`,
`rocket-info/compare_info_pins.py`), and a difference of that kind alone leaves the committed table as it is.
A real change rewrites the whole table, noise included: check the motors of `makeBeta` in the result. Seeding
OpenRocket's UUIDs, as `tools/openrocket-goldens` does (`DeterministicUuids`), would make both families
reproducible, at the price of a one-time reordering of the two tables.

## History

The probes and generators were written by Milestone 2's parts G1 (`scale-figure`), G2 (`figure-paint`), G3
(`rocket-info`), G4 (`ribbon`), B2 (`material-binding`) and B4 (`material-combo`) of run 2 and P1
(`rocket-panel`) and P2 (`preferences`) of run 3, and moved here unchanged in run 4 but for: the motor
database comes from `-Dprobe.motors` (it was a path of the author's machine); OpenRocket's log goes to
standard error, so the transcripts need no filtering; the generators format with the repository's
`.clang-format` wherever they write, and name this directory in the headers they write. Every table
regenerated as committed (the two families above apart from their noise).

The other generators of GUI-related tables: `tools/openrocket-panel-pins` (the configuration panels' pins),
`tools/openrocket-goldens/figure-pins` (the core's figure shapes), `tools/openrocket-component-factory`,
`tools/openrocket-fin-import`, `tools/openrocket-expression-parser` and `tools/openrocket-gui-reference`.
