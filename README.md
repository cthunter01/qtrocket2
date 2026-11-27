# QtRocket

QtRocket is a model rocket design and flight simulation program: a C++23 re-implementation of
[OpenRocket](https://openrocket.info) (Java). It follows OpenRocket's design and algorithms: the component model
(nose cones, body tubes, fins, transitions, pods, inner tubes, recovery devices, mass components, ...), the
extended Barrowman aerodynamics, the mass and inertia calculations, the event-driven six-degree-of-freedom flight
simulation (Runge-Kutta steppers, staging, recovery, tumbling, ground hit), the ISA atmosphere, wind and gravity
models, the thrust-curve motor database and motor files (`.eng`, `.rse`), the `.ork` design files (it loads file
format versions 1.0 to 1.11 and writes 1.11, OpenRocket's current format), the component presets (`.orc` files)
and the CSV export of flight data.

Version 0.2.0 is Milestone 1: the headless core, a Qt-free library (`QtRocket_core`), and the command-line tool
`QtRocket_cli`. Its results are checked against OpenRocket's own: golden data that OpenRocket computed for its 16
example designs and 13 test rockets (geometry, masses, aerodynamics, complete flights, re-saved files) and ports of
OpenRocket's unit tests. Milestone 2 adds the graphical program `QtRocket` (Qt 6 Widgets), which designs rockets
as OpenRocket's Design tab does, Milestone 3 its "Motors & Configuration" tab, the motor chooser with its thrust
curve plots, and the component presets with the parts library, and Milestone 4 its "Flight simulations" tab: the
simulations' settings, running them with progress and cancel, the design view's flight estimate, the plots of
their flight data and its CSV export; appearance, the 3D views and the analysis tools come later
([The GUI](#the-gui)).

<a id="requirements"></a>

## Requirements
- CMake 3.28+ and Ninja
- A C++23 compiler with `<print>`:
  - Linux: GCC 14+ or Clang 18+
  - macOS: Xcode 16.3+ or its Command Line Tools (Apple Clang 17+)
  - Windows: Visual Studio 2022 17.7+ (MSVC) with the "Desktop development with C++" workload
- Qt 6.8 or newer, for the GUI: the modules Core, Gui, Widgets and Svg, and Test for the tests. Without Qt,
  configure with `-DQTROCKET_BUILD_GUI=OFF` (for example `cmake --preset clang-debug -DQTROCKET_BUILD_GUI=OFF`):
  that builds the core and the command-line tool alone. CMake finds Qt where the system installs it, or through
  `CMAKE_PREFIX_PATH` (the directory that holds Qt's `lib/cmake/Qt6`):
  - Fedora: `sudo dnf install qt6-qtbase-devel qt6-qtsvg-devel`
  - Arch Linux: `sudo pacman -S qt6-base qt6-svg`
  - Debian 13+ and Ubuntu 25.04+: `sudo apt install qt6-base-dev qt6-svg-dev` (Ubuntu 24.04's Qt 6.4 is too old:
    use Qt's online installer or [aqtinstall](https://github.com/miurahr/aqtinstall) there)
  - macOS: Qt's online installer or aqtinstall (the macOS build of Qt 6.8 or newer), or Homebrew's `qt`
  - Windows: Qt's online installer or aqtinstall, the "MSVC 2022 64-bit" build of Qt 6.8 or newer; put its `bin`
    directory on `PATH` (the program and the tests load Qt's DLLs from there) and pass its directory as
    `CMAKE_PREFIX_PATH` unless `bin` is already on `PATH` when CMake configures
- Optional: clang-tidy, clang-format, llvm-cov/llvm-profdata (coverage), Doxygen (docs), ccache.
  On macOS, clang-tidy comes from Homebrew (`brew install llvm`). Coverage uses Xcode's llvm-cov.

The libraries are taken from the system when installed, otherwise downloaded at configure time and built with the
project (`cmake/Dependencies.cmake`): pugixml (XML), minizip-ng with zlib (zip and gzip), SQLite (the motor
database) and fast_float (number parsing); GoogleTest and nlohmann_json for the tests only, rocketplot 0.8.0 (the
plots) for the GUI only. The release archives always build them from source.

## Build
Linux and macOS:
```sh
cmake --workflow --preset dev          # configure + build + test, Clang Debug
./build/clang-debug/bin/QtRocket_cli --help
./build/clang-debug/bin/QtRocket       # the GUI
```

Windows, from a **Developer PowerShell for VS** (Ninja needs MSVC's environment; VS Code's CMake Tools and
Visual Studio set it up themselves):
```powershell
cmake --workflow --preset dev-msvc     # configure + build + test, MSVC Debug
.\build\msvc-debug\bin\QtRocket_cli.exe --help
.\build\msvc-debug\bin\QtRocket.exe     # the GUI
```

| Preset | Platforms | What it is |
| --- | --- | --- |
| `clang-debug`, `clang-release` | Linux, macOS | Everyday builds (Apple Clang on macOS) |
| `gcc-debug`, `gcc-release` | Linux | Everyday builds |
| `msvc-debug`, `msvc-release` | Windows | Everyday builds |
| `asan` | Linux, macOS | Clang Debug with AddressSanitizer + UndefinedBehaviorSanitizer |
| `tsan` | Linux, macOS | Clang RelWithDebInfo with ThreadSanitizer |
| `tidy` | Linux, macOS | Clang Debug running clang-tidy on every file; findings are errors. Its workflows `tidy-core` and `tidy-gui` build and test the core's and the GUI's halves, as CI does |
| `coverage` | Linux, macOS | `cmake --workflow --preset coverage` writes `build/coverage/coverage/html/index.html` |
| `ci-gcc`, `ci-clang`, `ci-msvc` | as their compiler | Release builds with warnings as errors, as run in CI |
| `dist-linux`, `dist-macos`, `dist-windows` | Linux, macOS, Windows | The release archives (see [Releases](#releases)); no GUI yet |

A preset exists only on the platforms it supports; `cmake --list-presets` shows the ones for this machine.
Each workflow preset (`dev`, `dev-msvc`, `ci-gcc`, `ci-clang`, `ci-msvc`, `asan`, `tsan`, `tidy`, `tidy-core`,
`tidy-gui`, `coverage`) configures, builds and tests in one command, and the `dist-*` ones also package.
Separate steps: `cmake --preset <p>`, `cmake --build --preset <p>`, `ctest --preset <p>`.

CI (GitHub Actions) builds and tests on all three in seven jobs: Linux (`ci-gcc`, `ci-clang`, `asan`, and `tidy`
in two jobs, the workflows `tidy-core` and `tidy-gui`, which build and test the core's and the GUI's halves),
macOS (`ci-clang`) and Windows (`ci-msvc`), with the GUI: against the newest Qt on Linux (Arch Linux's packages)
and Qt 6.8.3, the oldest Qt supported, on macOS and Windows. An eighth job checks the formatting (clang-format).
The `tsan` preset runs in no CI job: it is run locally after a change to the simulations or their threads.

API docs: `cmake --build --preset clang-debug --target docs`, then open `build/clang-debug/docs/html/index.html`.

<a id="the-gui"></a>

## The GUI
`QtRocket` is the graphical program. It follows OpenRocket's GUI: its windows, dialogs, texts and rules.
Milestones 2 to 4 give it what OpenRocket's "Rocket design", "Motors & Configuration" and "Flight simulations"
tabs offer but appearance, the 3D views, the analysis tools (Monte Carlo, component analysis, optimization),
custom expressions and the configuration of simulation extensions
([docs/gui-differences.md](docs/gui-differences.md) lists what comes later):

- A window per design: the component tree beside the buttons that add components, and below them the design view,
  the rocket from the side, the top or the back, with zoom, rotation, rulers, the CG and CP, the stage and flight
  configuration selectors, and the length, diameter, masses, stability, CG and CP over the figure.
- The configuration dialog of every kind of component, also for several components at once: the shape, the
  placement, materials, the motor mount tab, the stage's separation and recovery tabs, the Override (mass, CG,
  CD) and Comment tabs, and the freeform fin editor (point table, dragging points, its own undo, CSV export,
  import from an image).
- Component presets: the preset box and "Parts Library" of every component that takes one, with OpenRocket's
  5228 bundled parts and the user's own `.orc` files: a table to filter by text and by the neighbours'
  diameters, to sort, and to mark favourites, which the preset box lists.
- The "Motors & Configuration" tab: the flight configurations (new, rename with `{motors}` and the like,
  delete, duplicate) and their tables of motors, recovery and stages, with the ignition, deployment and
  separation dialogs and the motor mounts.
- The motor chooser: OpenRocket's motor database and the user's own thrust curve files, searched by text and
  filtered by manufacturer, impulse class, diameter and length (also the mount's), the thrust curve, delay and
  details of each motor, and the plot of its thrust curves (zoom, pan, export to PNG, JPEG, SVG, PDF or CSV).
- The "Flight simulations" tab: the design's simulations in a table (status, warnings and the key figures of
  each flight, sorted by any column, with a choice of columns kept in the design), New, Edit, Run, Delete and
  Plot / Export, cut, copy (also as text on the system clipboard), paste and duplicate, and the table's export
  to a CSV file.
- The settings of a simulation, in its settings dialog (also for several simulations at once): the launch
  conditions (the launch site and rod, the atmosphere, the average wind or a wind profile of several altitudes,
  edited in a table beside its plot or imported from a CSV file), the simulation options (time step, geodetic
  calculations, gravity, random seed, aerodynamic lookup tables, the list of the simulation's extensions), the
  thresholds of the recovery warnings, and for a simulation with results its Plot data and Export data tabs.
- Runs: the simulations run on as many threads as the processor offers, with the run dialog's progress and
  Cancel (a cancelled simulation keeps its earlier results). The design view of every window shows the flight
  estimate of the selected flight configuration (apogee, maximum velocity and acceleration), run in the
  background whenever the design changes.
- Plots: OpenRocket's nine preset plots of a simulation's flight data, or the quantities chosen, on two y axes,
  with the flight events and the stages (zoom, pan, image export); the export of the flight data to a CSV
  file, the quantities, units, stage and format chosen.
- Edit: undo and redo, cut, copy, paste, duplicate, delete, move up and down, dragging components in the tree
  (also from one window to another), hiding and showing components, and disabling and enabling stages.
- File: New, Open (the `.ork` files `QtRocket_cli` reads), Open recent, Open example (OpenRocket's 16 example
  designs), Save, Save as (with the design information), Properties, Export simulation table as CSV file,
  Close design and Quit.
- Preferences: the units, the design options, the defaults of new simulations (their options, launch
  conditions and warning thresholds), the user's thrust curve and component preset files, "Open last design
  file on startup" and the automatic Parts Library.

`QtRocket [file...]` opens the design files it is given, each in a window of its own; without one it opens the most
recent design when the preference "Open last design file on startup" is set, and a New design otherwise. On macOS,
the files the Finder asks it to open open too. `QtRocket --version` prints its version and `QtRocket --help` its
options, also without a display; on Windows the program has no console of its own, so their text shows only when it
goes to a file or a pipe (`QtRocket.exe --version | more`). The program finds the motor database, the component
presets and the example designs in `<directory of the program>/../share/QtRocket`, as `QtRocket_cli` does; the
build tree has them there. It reads the user's own thrust curves (`.eng`, `.rse`, `.zip`, `.db`) and component
presets (`.orc`) from the files and directories listed under Preferences > General, by default the folders
`ThrustCurves` and `Components` of its data directory, which it makes at start while the lists are unset:
`~/.local/share/QtRocket` on Linux (under `$XDG_DATA_HOME` when that is set), `~/Library/Application
Support/QtRocket` on macOS, `%LOCALAPPDATA%\QtRocket` on Windows. A change of the lists takes effect at the next
start. The thrust-curve and preset files whose contents it cannot import are listed in a message after the first
window opens; other problems (a listed file that is missing or cannot be opened) are not shown, as OpenRocket only
writes them to its log, and special files such as named pipes are skipped.

It needs a desktop and, at run time, the Qt it was built with (Qt 6.8 or newer, the modules of
[Requirements](#requirements); on Windows, Qt's `bin` on `PATH`). It uses the platform's widget style and follows
its light or dark scheme. Its preferences are kept in an INI file: `~/.config/QtRocket/QtRocket.ini` on Linux
(under `$XDG_CONFIG_HOME` when that is set) and macOS, `%APPDATA%\QtRocket\QtRocket.ini` on Windows; OpenRocket's
preferences are not read. The release archives hold the command-line tool alone until the GUI is released
(Milestone 9).

[docs/gui-differences.md](docs/gui-differences.md) lists every difference from OpenRocket's GUI that a user can
notice: the OpenRocket bugs QtRocket fixes, editing several components at once, and the smaller differences of
each window, with what comes in later milestones.

## The command-line tool
`QtRocket_cli` loads `.ork` designs, prints their key figures, runs their simulations and lists motors. The
designs find the component presets they name among the bundled ones (`info` and `simulate` read them; the
user's own preset and thrust curve files are the GUI's only):

```sh
QtRocket_cli info "data/examples/A simple model rocket.ork"
QtRocket_cli simulate "data/examples/A simple model rocket.ork" --calm --time-step 0.01 --csv flight.csv
QtRocket_cli simulate "data/examples/Two stage high power rocket.ork" --all --seed 42
QtRocket_cli motors --search "estes c6"
```

`QtRocket_cli --help` prints:

```text
Usage: QtRocket_cli <command> [options]

Commands:
  info FILE [--config NAME|NUMBER] [--mach M] [--db FILE]
      Print the length, the masses, the CG, the CP and the stability of every
      flight configuration of the design FILE, or of the one --config names
      (its exact name, else its number as 'info' prints it: 0 is the default
      configuration). The CP is computed at Mach M (default 0.3).

  simulate FILE [--sim NAME|NUMBER | --all] [--calm] [--seed N]
           [--time-step S] [--csv OUT] [--db FILE]
      Run the first simulation of the design FILE, the one --sim names (its
      exact name, else its number from 1), or with --all every simulation, and
      print the summary, the events and the warnings of each flight.
        --calm         no turbulence: the standard deviation of the wind is 0
                       (the average wind stays)
        --seed N       the random seed (default: a new one, unless the design
                       fixes one; the seed used is printed)
        --time-step S  the time step in seconds, from 0.0001 to 10
        --csv OUT      write the flight data of the first branch to the CSV
                       file OUT; with several simulations, simulation N writes
                       to OUT with -N before its extension

  motors [--search TEXT] [--db FILE]
      List the motors of the motor database: all, or those whose manufacturer,
      designation, common name or case contains every word of TEXT (ignoring
      case).

  motors --load FILE
      List the motors of a motor file (.eng, .rse, .zip or .db).

  --version
      Print the version.

  --help
      Print this help.

--db FILE reads the motors from the motor database FILE instead of
motors/initial_motors.db in the data directory (share/QtRocket beside the
directory of the program). 'info' and 'simulate' read the component presets
of components/ in the data directory, where a design finds the presets it
names.

Every value is printed in SI units. The warnings of a load go to the standard
error.

Exit codes:
  0  success
  1  usage error: a wrong command, option or value, or a simulation or flight
     configuration the design does not have
  2  a design file or a motor file cannot be loaded
  3  a simulation fails or is aborted
  4  an output file or the standard output cannot be written, or the motor
     database cannot be read
  5  internal error
```

`info` prints, per flight configuration, the length, the masses with and without motors, the CG, the CP and CNα,
the worst CP and the stability in calibers. `simulate` prints the summary (apogee, velocities, acceleration, Mach
number, times, ...), the flight events and the warnings of each simulation, and the random seed it used, so that
`--seed` repeats the flight. A CSV file holds every column of the flight data of the first branch except the
computation time, as OpenRocket's CSV export writes it with the options its export dialog starts with (each
column in its default unit, `#` comments, the flight events between the rows). The tool never changes a design
file; lines always end with `\n`, on Windows too.

## Data files and the installed layout
`data/` holds what QtRocket ships, copied unchanged from OpenRocket:
- `data/motors/initial_motors.db`: the thrust-curve motor database (SQLite), and `data/motors/metadata.json`,
  whose version the motor chooser shows ("Motor DB version").
- `data/components/`: the component presets (31 `.orc` files, 5231 parts, of which the database keeps 5228):
  `database/`, the 16 files of the [openrocket-database](https://github.com/openrocket/openrocket-database)
  (Apache License 2.0, with its `LICENSE` and `README.md`), and `internal/`, OpenRocket's 15 legacy files.
- `data/examples/*.ork`: OpenRocket's 16 example designs.

An installation (and every release archive) has this layout:

```text
bin/QtRocket_cli
share/QtRocket/motors/initial_motors.db, metadata.json
share/QtRocket/components/database/*.orc, LICENSE, README.md
share/QtRocket/components/internal/*.orc
share/QtRocket/examples/*.ork
share/doc/QtRocket/LICENSE, NOTICE, README.md
```

`QtRocket_cli` finds its data beside itself, in `<directory of the program>/../share/QtRocket`, so an archive works
wherever it is unpacked. The build tree has the same layout: building the programs copies `data/` to
`build/<preset>/share/QtRocket/`. `--db FILE` reads another motor database instead. A system installation
uses the standard directories (`cmake --install build/<preset> --prefix <dir>`).

## Tests
The core's tests are in `QtRocket_core_tests` (GoogleTest, `tests/core/<subsystem>/`), plus the tests that run
`QtRocket_cli` as a program and check its exit codes, output and files (`tests/core/cli/`). The GUI's tests are in
`QtRocket_gui_tests` (GoogleTest with QtTest's helpers, `tests/gui/<area>/`); they run on Qt's offscreen platform,
so no window appears. The exit suites check the GUI as a whole against OpenRocket (`tests/gui/exit/`):
`FromScratchExitGolden` designs a rocket from scratch through the GUI alone (the add buttons, the dialogs' fields,
undo, copy and paste, Save as), then compares every step and the saved file with what OpenRocket's own GUI code did
with the same steps (`tools/openrocket-gui-reference/`); `FlightConfigurationsExitGolden` does the same in the
"Motors & Configuration" tab, with two designs (that reference design and a two-stage example): the tab's
buttons and tables, the ignition, deployment and separation dialogs, the motor chooser, renames, undo and redo,
Save as; `OpenDesignsExitGolden` opens OpenRocket's 16 examples, the 49 re-saves of its golden harness and that
reference design in QtRocket's windows and compares the tree, the dialogs' fields, the figure, the information
overlay, the "Motors & Configuration" tab, the "Flight simulations" tab and the settings dialog of every
simulation with OpenRocket's golden data and OpenRocket's own tabs and dialog; `OpenDesignsExitGoldenPartsLibrary`
opens the parts library of every component of the examples that takes presets and compares it with OpenRocket's;
and `OpenDesignsFlightBlockExitGolden` opens each of those designs with the design view's flight estimate on and
compares its flight block with OpenRocket's, and `OpenDesignsFlightBlockRunExitGolden` what the estimate then runs
for the four designs whose estimate runs. `SimulationsExitGolden` does in the "Flight simulations" tab what
`FlightConfigurationsExitGolden` does in its tab, on the design OpenRocket saved at the end of that tab's first
scenario: the tab's buttons, the Edit menu's cut, copy, paste and duplicate, the table's columns, the settings
dialog of one simulation and of two at once with the wind profile editor, the wind import and the aerodynamic
lookup dialog, the confirmations, undo and redo, Save as; every run the dialogs start is cancelled before it
begins. `FromScratchReferenceGolden`, `FlightConfigurationsReferenceGolden` and `SimulationsReferenceGolden` (in
`QtRocket_core_tests`) check the references these suites compare with: each the record of its scenario, made by
the OpenRocket of the other goldens, and OpenRocket's save loaded by QtRocket into OpenRocket's state.

The interop check, run by hand at a milestone's end (`tools/openrocket-goldens/interop.sh`, after the `DISABLED_`
writers `OrkInteropSaves` and the GUI's `*InteropSave*` tests; CI has no Java), has OpenRocket open QtRocket's
saves and checks that it gets from each the design it gets from its own save of the same input, but for a short
list of allowed differences: the examples, the test rockets, 18 older design files and two coverage documents,
saved by the core; the designs the GUI's exit scenarios made (the inputs `gui-from-scratch`,
`gui-flight-configurations`, `gui-multi-stage` and `gui-simulations`); and flight data QtRocket ran
(`gui-run-example-a-simple-model-rocket` and `gui-run-example-clustered-motors`: the first simulation of each
example, run with the Run button of the "Flight simulations" tab and saved with its data), which OpenRocket saves
again line for line and reads back from that save bit for bit, the summary values equal to those it computes or
within the precision they are stored with (`tools/openrocket-goldens/README.md`, "Interop check").

The simulations are compared with what OpenRocket's own classes make of the examples' stored simulations: the
core's plot model with OpenRocket's plots of each of them in the nine presets (`SimulationPlotGolden`), and in the
GUI the simulations table, its CSV text and Copy (`SimulationTableModelGolden`, `SimulationTableCsvExportGolden`
and the other `SimulationTable*Golden` suites), the tab's actions (`SimulationPanelGolden`), the tab in its window
with the menu items' states (`SimulationsWindowGolden`), the plot tab and the plot itself
(`SimulationPlotPanelGolden`, `SimulationPlotChartGolden`), every stored simulation of the examples in each of the
nine presets through the plot dialog (the exit suite `SimulationPlotPresetsGolden`, `tests/gui/dialogs/plot/`, run
as one ctest test in one process), the files of the Export data tab (`SimulationExportGolden`: OpenRocket's own
exports, byte for byte, and the core's CSV export), the design view's flight block (`RocketInfoFlightGolden`) and
what its flight estimate runs and shows (`BackgroundSimulationGolden`). The suites that run simulations on threads
of their own (the core's `SimulationRunner`, `SimulationThreading` and `RunSampleListener`, the GUI's
`BackgroundSimulationThreadsTest`, `PollingSimulationExecutorTest`, `SimulationRunDialogThreadsTest`,
`SimulationWindowIntegrationThreadsTest` and `OpenDesignsFlightBlockThreadsTest`) have a time limit, so that a
broken cancel fails a test instead of hanging the run; the `tsan` preset checks them for data races. The lint tests
(`tests/lint/`) check rules of the source tree that the compiler cannot, such as the include order of the layers.
Test data lives in `tests/data/`. ctest labels select parts of the suite:

```sh
ctest --preset clang-debug                 # everything
ctest --preset clang-debug -L golden       # the comparisons with OpenRocket's golden data
ctest --preset clang-debug -L cli          # QtRocket_cli run as a program
ctest --preset clang-debug -L gui          # the GUI's tests
ctest --preset clang-debug -L lint         # the checks of the source tree
ctest --preset clang-debug -LE golden      # everything but the golden comparisons
build/clang-debug/bin/QtRocket_core_tests --gtest_filter='BodyTube.*'
```

A GoogleTest suite whose name contains `Golden` gets the label `golden`, whatever its file, in both test
executables (the GUI's keep the label `gui` too); the comparisons are mostly in `*_golden_tests.cpp`, and the CLI's
milestone run (`cli.milestone`) carries the label too. Re-run them after any change to the aerodynamics, mass,
simulation, component geometry, figure or file code.

### Golden data
`tests/data/goldens/` holds what OpenRocket computes for its 16 example designs and the 13 rockets of its
`TestRockets` class: geometry, mass and aerodynamics per flight configuration, the complete simulations at the
designs' own time step, the simulations at a stable 0.01 s step (complete for the test rockets; the summaries and
events for the examples), the shapes of the 2D figure in five views, and OpenRocket's own re-save of each design.
The Java harness in `tools/openrocket-goldens/` writes it from a local OpenRocket checkout; CMake and CI never need
Java. To regenerate (JDK 17 and an OpenRocket checkout at the commit `tests/data/goldens/manifest.json` records):

```sh
tools/openrocket-goldens/generate.sh
ctest --preset clang-debug -L golden
```

`tools/openrocket-goldens/README.md` describes the harness, the file formats, the tolerances of the comparisons
and the other fixtures it can write.

The preset golden `presets/presets.json` holds every component preset OpenRocket reads from the bundled `.orc`
files, with every property, and the order of its preset database (`generate.sh --presets` writes it alone).

The GUI's references come from OpenRocket's Swing classes the same way. `tools/openrocket-gui-reference/` runs
OpenRocket's GUI code on scenarios of GUI steps, in three versions of their schema (the design, headless; the
"Motors & Configuration" tab; the "Flight simulations" tab and its dialogs: both with OpenRocket's own panels on a
virtual display), and writes `tests/data/goldens/gui/from-scratch/`, `flight-configurations/`, `multi-stage/` and
`simulations/` (each a scenario, the steps' states and OpenRocket's save). The tables the GUI's tests compare with
(the `*Pins.h` headers, which name their generator, the JSON tables of `tests/data/goldens/gui/simulation-table/`,
`simulation-dialog/` and `rocket-info/`, and a few tables in test files) are written by Java probes in
`tools/openrocket-gui-pins/` and `tools/openrocket-panel-pins/`, and `tools/openrocket-probes/` keeps other probes
whose output tests pin by hand (those of the simulation settings' panels among them). The core's tables of the
motor chooser come from `tools/openrocket-motor-chooser/`, the core's plot goldens (`tests/data/goldens/plots/`,
OpenRocket's plots of the examples' stored simulations) from `tools/openrocket-plots/`, and the tables of JDK 17's
text collation, which OpenRocket's tables sort with, from `tools/openrocket-collation/`. Each tool's README says
how to run it.

<a id="releases"></a>

## Releases
The Release workflow (`.github/workflows/release.yml`) runs only when started by hand, never on a push:
1. Raise `VERSION` in `project()` in `CMakeLists.txt`, then commit and push.
2. Start it from the Actions tab (Release > Run workflow, pick the branch) or with `gh workflow run release.yml`
   (`-f prerelease=true` marks it a pre-release).

It stops at once if the tag `v<version>` already exists. Otherwise it runs all of CI and builds, tests and
packages an archive on each platform. Only when every job passes does it tag the commit `v<version>` and
publish a GitHub release with the archives, a `SHA256SUMS` file and generated release notes.

| Archive | Built with | Runs on |
| --- | --- | --- |
| `QtRocket-<version>-linux-x86_64.tar.gz` | GCC 14, Ubuntu 24.04 | x86-64 Linux with glibc 2.39+ (Ubuntu 24.04+, Debian 13+, Fedora 40+, RHEL 10+) |
| `QtRocket-<version>-macos-universal.tar.gz` | Apple Clang | macOS 14+, Apple silicon and Intel |
| `QtRocket-<version>-windows-x86_64.zip` | MSVC | 64-bit Windows |

The C++ runtime is linked into the Linux and Windows executables, so users need no libstdc++ or VC++
Redistributable. Dependencies are always built from source, never taken from the build machine. An archive holds
what the `install()` rules install (the layout above): add rules for anything else it should ship.
Build one locally with `cmake --workflow --preset dist-linux` (or `dist-macos`, `dist-windows`); it lands in
`build/dist-<os>/package/`.

The executables are not code-signed. A macOS browser download needs `xattr -d com.apple.quarantine bin/QtRocket_cli`
before it runs, and Windows SmartScreen warns the first time.

## Licence
QtRocket is free software under the GNU General Public License, version 3 or (at your option) any later version:
see [LICENSE](LICENSE). It is a re-implementation of OpenRocket, Copyright (C) 2007-2025 Sampo Niskanen and
others, also licensed under the GPL version 3 or later; the motor database, the example designs and the test
designs are OpenRocket's, and so are the GUI's icons (some of them Lucide's). [NOTICE](NOTICE) gives the details
and the notices of the third-party libraries and icons that the programs contain.
