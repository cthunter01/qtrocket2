# What OpenRocket's configuration panels show

`tools/openrocket-panel-pins` runs OpenRocket's own Swing configuration panels (Java) and writes the pin
tables QtRocket's panels are compared with: four generated headers in `tests/gui/configdialog/`. The
headers are committed; neither CMake nor CI ever needs Java. They change only through this tool.

| Header | Probe | Generator | Read by |
|---|---|---|---|
| `BodyPanelPins.h` | `src/probe/BodyPanelProbe.java` | `gen_body_panel_pins.py` | `body_panel_golden_tests.cpp` |
| `LugPanelPins.h` | `src/probe/LugPanelProbe.java` | `gen_lug_panel_pins.py` | `lug_panel_golden_tests.cpp` |
| `FinPanelPins.h` | `src/probe/FinPanelProbe.java` | `gen_fin_panel_pins.py` | `fin_panel_golden_tests.cpp` |
| `FreeformPanelPins.h` | `src/probe/FreeformPanelProbe.java` | `gen_freeform_panel_pins.py` | `freeform_panel_golden_tests.cpp` |

- `BodyPanelProbe`: OpenRocket's `BodyTubeConfig`, `NoseConeConfig` and `TransitionConfig` on every body
  tube, nose cone and transition of the 16 examples (`data/examples`, loaded without presets) and the 13
  test rockets: the General tab's widgets (labels, spinner texts, unit selectors, slider positions, check
  boxes with their enabled states and tooltips, the shape combo box), the automatic radii after the panel
  was made, "Flip to tail cone" clicked twice on every nose cone, and the switch-off of an automatic radius
  reaching a second selected component.
- `LugPanelProbe`: `TubeFinSetConfig`, `LaunchLugConfig` and `RailButtonConfig` on every tube fin set,
  launch lug and rail button of the same designs, then edits through the panels' own models on designs it
  builds, and edits with a second selected component.
- `FinPanelProbe`: `TrapezoidFinSetConfig` and `EllipticalFinSetConfig` with their base `FinSetConfig` (the
  General and Fin tabs tabs, "Split fins", "Convert to freeform"), edits, multi-component edits,
  `FinSetConfig.computeFinTabLength()` on `FinSetConfigTest`'s inputs, and "Convert to freeform", "Split
  fins" and "Calculate automatically" through `ComponentConfigDialog` itself.
- `FreeformPanelProbe`: `FreeformFinSetConfig` with its `FinPointFigure` and `FinPointScrollPane` (the
  panel in a dialog that is never shown, the figure in a scroll pane of 600 x 400 pixels at 96 dpi): the
  tabs, the point table, the figure's projection, and steps of table edits, Insert and Delete point, mouse
  presses, drags, releases and clicks, the editor's undo and redo and the CSV export, also with a second
  selected fin set.

Each probe settles a design before every panel (a copy of the golden harness's `AutomaticDimensions`) and
writes its transcript (sections `== <part> | <source> | <design> | ...` and their lines, every double as
Java 17's `Double.toString()`) to the file it is given; its comments say what each line holds. The
generators turn a transcript into a header (the lines per design, the section headers without the source
and the design; a few lines changed on the way, as their comments say), record the OpenRocket commit and
format the header with the repository's `.clang-format`. The golden tests check that commit against the
one of `tests/data/goldens/manifest.json`.

## Regenerating

Requirements: JDK 17 (the script refuses another version, because `Double.toString` changed in JDK 19), a
checkout of OpenRocket at the commit of `tests/data/goldens/manifest.json` (5f164fd0e when the headers were
written), `xvfb-run` (the panels live in a `JDialog`, which needs a display: the probes run with
`-Djava.awt.headless=false` on a virtual one), `python3` and `clang-format`.

```sh
OPENROCKET_DIR=/path/to/openrocket tools/openrocket-panel-pins/generate.sh          # writes the headers
OPENROCKET_DIR=/path/to/openrocket tools/openrocket-panel-pins/generate.sh --check  # compares them
```

The script compiles the four probes with OpenRocket's `core/src/test/.../ServicesForTesting.java` against
the runtime class path of OpenRocket's swing module, runs each under `xvfb-run` with a fixed locale, time
zone and encoding and a fresh `java.util.prefs` store (OpenRocket's preferences at their defaults), and runs
the generators. `--check` writes the headers into a temporary directory and compares them byte for byte with
the committed ones. The class path is `OPENROCKET_CLASSPATH` when set (the committed headers were written so:
the classes and resources of a checkout built before, `core/build/classes/java/main`,
`core/build/resources/main`, `core/libs/script-api-1.0.jar`, `swing/build/classes/java/main`,
`swing/src/main/resources`, and the jars of Gradle's cache); otherwise Gradle builds OpenRocket's swing
module and prints it, as `tools/openrocket-fin-import/generate.sh` does. The head of `generate.sh` lists the
other environment variables (`PINS_OUT`, `PANEL_PINS_WORK`, `ALLOW_DIRTY`). A run takes a few minutes.

A regeneration writes the same bytes: the probes print no time or address, and the panels' fonts are those
OpenRocket bundles (FlatLaf Light with Inter).
