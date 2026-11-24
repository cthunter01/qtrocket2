# What OpenRocket's simulation plots show: the core's plot goldens

`tools/openrocket-plots` runs a Java probe against OpenRocket's compiled classes and turns what it prints into
the two JSON tables QtRocket's core plot model is compared with (Milestone 4, run 1, part P3). The tables are
committed; neither CMake nor CI ever needs Java. They change only through this tool.

| Table | What it holds | Read by |
|---|---|---|
| `tests/data/goldens/plots/simulation-plots.json` (about 1.2 MB) | every stored simulation of the 16 examples (54) with each of the 9 preset plot configurations: 486 plots, compact | `tests/core/plot/simulation_plot_golden_tests.cpp` (`SimulationPlotGolden`, `SimulationPlotGoldenTables`) through `tests/core/plot/SimulationPlotGoldens.h` |
| `tests/data/goldens/plots/simulation-plot-details.json` (about 0.85 MB) | ten of those simulations, all 9 plots each, in detail | the same |

`SimulationPlotGoldens.h` (in `QtRocket_test_support`, so that the GUI's tests of the plot dialog can read the
same tables) reads both into structs; `simulation_plot_golden_tests.cpp` loads each example as the goldens'
harness loads it (`loadGoldenExample()`, a `DesignFileEnvironment`), builds every plot with
`SimulationPlotConfiguration` and `SimulationPlotModel` and compares everything exactly, to the bit.

## The probe

`probe/SimulationPlotsProbe.java` sits in OpenRocket's package `info.openrocket.swing.gui.plot`, so that it
reaches the package-private and protected members of the plot classes (`Plot.filledConfig`, `Plot.data`, the
legend items, `SimulationPlot`'s series names; private fields and methods by reflection). It runs headless on the
core's `ServicesForTesting` with `SwingPreferences`, the bundled motor database (`data/motors/initial_motors.db`)
and no component presets, with the UUIDs of `tools/openrocket-gui-pins`' `DeterministicUuids` (reseeded for each
design). For every simulation of the examples (`data/examples`, in the order of the goldens' manifest) that has
stored data, and for every preset, it

- chooses the preset through OpenRocket's own `SimulationPlotPanel`, as a user does (its preset combo box: a copy in
  the default units), and prints the configuration the panel holds;
- prints the automatic axes `fillAutoAxes()` chooses on the first branch, with the ranges it fitted them to, and
  every candidate it scores (the automatic rows on the left or the right axis, the first automatic row the most
  significant, the left axis first), with its goodness and ranges;
- prints the colour the panel shows for each row (`computeDefaultPlotColors()`);
- makes the plot as the plot dialog does (`SimulationPlot.create()`, before `setShowEvents()` draws anything) and
  prints the filled configuration's ranges over every branch, the titles, the domain axis (label and range), the
  y axes (labels, ranges and series), the series (name, axis, branch, row, unit, number of points, a digest of the
  points, the extremes and the first, middle and last point), three readout tool tips of each series (its first,
  middle and last point) with the series' name there, and the legend;
- replays the events from `SimulationPlot`'s `eventList`: the list, the groups of every branch and of each branch
  (`fillEventLists()`), the domain widened for the event lines (`plotVerticalLineMarkers()`, the time domain only)
  and the icons of every branch from the last (`plotIconMarkers()`), with their tool tips through
  `Plot.formatTooltip()`.

Every double is printed with `Double.toHexString`; `gen_simulation_plots.py` writes it as the shortest decimal that
reads back as the same double (NaN and the infinities as the strings `"NaN"`, `"Infinity"` and `"-Infinity"`).

### Where QtRocket's answers are not OpenRocket's

QtRocket fixes some of OpenRocket's plot bugs (the plan's section 11.4); the tables hold QtRocket's answers, derived
with OpenRocket's own code:

- Bug 41: `fillEventLists()` keeps a group's events in a `HashSet` of identity hashes, so the time OpenRocket draws a
  group at (the set's first event), its label's order and its icons' tool tips change from run to run. The probe
  replays `fillEventLists()` with each group's events in `eventList` order (stably sorted by time) and draws the
  group at its first event, as `SimulationPlotModel` does; everything else of the replay is OpenRocket's code.
- Bug 45: the common zero of `PlotConfiguration.fitAxes()` divides by zero when the left range starts or ends at
  exactly zero and the right one spans zero, which gives the right axis an infinite bound. The probe's
  configurations are a `CommonZeroFixedConfiguration` (`SimulationPlotConfiguration` with `fitAxes()` line for line
  and the fix: the common zero is left out when it would give an infinite bound), whose clones (`fillAutoAxes()`'s
  candidates, the plot's filled configuration) fit their axes the same way. It changes one plot of the 486,
  "Dual parachute deployment" simulation 0 with the time step preset: there the table also records OpenRocket's own
  choice (`"openRocket": {"bug": 45, ...}`, with its candidates, one of which has an infinite bound), and the
  generator refuses any other difference.
- Bug 42: OpenRocket's renderer draws every row of an axis in its first row's colour with several branches;
  QtRocket's series and legend take the colour the panel shows for their row, which is what the tables hold
  (`panelColors`).
- Bug 43 (the domain axis loses its label when the event lines widen it): the probe reads the domain axis before
  the events are drawn; the widened range is replayed.

### Digests

Two kinds of values are stored as digests, the first 16 hexadecimal digits of a SHA-256:

- A series' points (`"digest"` of a series): the x values, then the y values, each as the 8 little-endian bytes
  of `Double.doubleToLongBits()` (every NaN as `0x7ff8000000000000`). The probe computes it.
- A plot's icons (`"icons"` of the compact table): `"digest"` of the lines
  `"<series> <group> <time> <x> <y> <y on the left axis> <look>\n"` of every icon in drawing order (the doubles as
  the 16 hexadecimal digits of `doubleToLongBits()`; the look is the type of the group's event of the highest
  ordinal, for a warning followed by `/` and its priority: `SIM_WARN/NORMAL`), and `"toolTipDigest"` of the icons'
  tool tips, each followed by `"\n"`. The generator computes them from the icons the probe prints.

`SimulationPlotGoldens.h` computes both the same way.

### The tool tips and the libm

`DecimalFormatter.df()` counts the digits of a value below 0.1 with `Math.floor(Math.log10(1 / v))`, which
another `log10` could floor to the integer below when `1 / v` is a power of ten or within a rounding of one. The
probe marks every value of a tool tip for which `1 / v` is within 1e-12 of a power of ten and the two digit counts
format it differently, and the generator refuses a table with such a value: none of the stored values is one (the
exact powers of ten, 0.01 and 0.001, format alike either way), so the tool tips compare exactly on every platform.

## The tables

`simulation-plots.json`: `"presets"` (OpenRocket's 9, with their rows and events) and `"simulations"`, each with
its branches (name, points, separation time and index), its series labels (`Util.generateSeriesLabels()`) and
`"plots"`, one per preset: the panel's configuration (`"rows"`: type, unit, axis), `"auto"`, `"candidates"`,
`"filled"` (axes and the left and right range), the titles, `"domainLabel"`, `"domainRange"`,
`"domainRangeWithEventLines"` (null for a domain other than the time), `"yAxes"`, `"panelColors"`, `"series"`,
`"legend"`, `"groups"` (of every branch: time, look, events `[type, time, branch]`, label) and `"icons"` (count
and digests).

`simulation-plot-details.json`: for the simulations of `DETAILS` in `gen_simulation_plots.py`, each plot's series
(extremes, first, middle and last point, in the order of the compact table's series), readout tool tips, event
list, groups of each branch and icons (series, group, time, x, y, y on the left axis, look and tool tip).
Simulations are numbered from 0, in the order of their document. The ten cover a single stage (A simple model
rocket 0, beside the plot model's hand pins), warnings (3D printable nose cone and fins 2, "Simulation 3 - too
short delay"), payloads with two branches (ARC payload rocket 0; Deployable payload 2, its too short delay), bug
45's plot (Dual parachute deployment 0), a parallel booster's branch (Parallel booster staging 0), powered pods
(Pods--powered with recovery deployment 0), roll control with an air start and a flight data type OpenRocket does
not know (Simulation extensions 2, "Roll control + air-start"), three branches of which two have the same name
(Three stage low power rocket 0) and two stages (Two stage high power rocket 0).

The generator checks what the tests rely on: the panel kept every preset as it is (no row dropped, no
appearance), the filled configuration's axes are the automatic ones, no candidate's range is infinite after
bug 45's fix, OpenRocket's own choice differs only where bug 45 gave a candidate an infinite bound, and no tool tip
depends on the libm.

## Running

Requirements: Linux, JDK 17, python3 and an OpenRocket checkout at the commit of
`tests/data/goldens/manifest.json` (5f164fd0e), which the tables record and `SimulationPlotGoldenTables` requires.

```sh
OPENROCKET_DIR=/path/to/openrocket OPENROCKET_CLASSPATH=... tools/openrocket-plots/generate.sh [--check]
```

`OPENROCKET_CLASSPATH` is the runtime class path of OpenRocket's swing module (as for
`tools/openrocket-gui-pins`); without it the script asks Gradle in the checkout (`./gradlew :swing:classes` and a
task of a temporary init script that prints the class path; OpenRocket's build files are not changed). The
classes must be built from the checkout's HEAD (the script reads the commit OpenRocket's build stamps into
`build.properties`). The head of `generate.sh` lists the rest of the environment (`JAVA`, `JAVAC`, `ALLOW_DIRTY`,
`ALLOW_OTHER_COMMIT`, `PLOTS_WORK`).

Each table is written into the work directory and compared with the committed one byte for byte: an equal table
leaves the committed file alone, another replaces it (`--check` only compares and exits with status 1). The probe
prints the same on every run: it uses no random number and no clock (the UUIDs are seeded), and runs with a fixed
locale, time zone and encoding, a fresh home directory and `java.util.prefs` store in the work directory (its
`SwingPreferences` live there, which only Linux's implementation reads from that place: elsewhere it would use the
user's own OpenRocket preferences). It takes about fifteen seconds.
