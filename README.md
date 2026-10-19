# QtRocket

QtRocket is a model rocket design and flight simulation program: a C++23 re-implementation of
[OpenRocket](https://openrocket.info) (Java). It follows OpenRocket's design and algorithms: the component model
(nose cones, body tubes, fins, transitions, pods, inner tubes, recovery devices, mass components, ...), the
extended Barrowman aerodynamics, the mass and inertia calculations, the event-driven six-degree-of-freedom flight
simulation (Runge-Kutta steppers, staging, recovery, tumbling, ground hit), the ISA atmosphere, wind and gravity
models, the thrust-curve motor database and motor files (`.eng`, `.rse`), the `.ork` design files (it loads file
format versions 1.0 to 1.11 and writes 1.11, OpenRocket's current format) and the CSV export of flight data.

Version 0.2.0 is Milestone 1: the headless core, a Qt-free library (`QtRocket_core`), and the command-line tool
`QtRocket_cli`. Its results are checked against OpenRocket's own: golden data that OpenRocket computed for its 16
example designs and 13 test rockets (geometry, masses, aerodynamics, complete flights, re-saved files) and ports of
OpenRocket's unit tests. Milestone 2, under way, builds the graphical user interface: the program `QtRocket`
(Qt 6 Widgets), which so far opens the window of a new design with its menus.

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
database) and fast_float (number parsing); GoogleTest and nlohmann_json for the tests only. The release archives
always build them from source.

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
| `tidy` | Linux, macOS | Clang Debug running clang-tidy on every file; findings are errors |
| `coverage` | Linux, macOS | `cmake --workflow --preset coverage` writes `build/coverage/coverage/html/index.html` |
| `ci-gcc`, `ci-clang`, `ci-msvc` | as their compiler | Release builds with warnings as errors, as run in CI |
| `dist-linux`, `dist-macos`, `dist-windows` | Linux, macOS, Windows | The release archives (see [Releases](#releases)); no GUI yet |

A preset exists only on the platforms it supports; `cmake --list-presets` shows the ones for this machine.
Each workflow preset (`dev`, `dev-msvc`, `ci-gcc`, `ci-clang`, `ci-msvc`, `asan`, `tsan`, `tidy`, `coverage`)
configures, builds and tests in one command, and the `dist-*` ones also package. Separate steps:
`cmake --preset <p>`, `cmake --build --preset <p>`, `ctest --preset <p>`.

CI (GitHub Actions) builds and tests on all three: Linux (`ci-gcc`, `ci-clang`, `asan`, `tidy`), macOS
(`ci-clang`) and Windows (`ci-msvc`), with the GUI: against the newest Qt on Linux (Arch Linux's packages) and Qt
6.8.3, the oldest Qt supported, on macOS and Windows.

API docs: `cmake --build --preset clang-debug --target docs`, then open `build/clang-debug/docs/html/index.html`.

## The GUI
`QtRocket` is the graphical program (Qt 6 Widgets), which Milestone 2 builds: so far it opens the window of a new
design, with its menus (New, Close design and Quit work; the design panel, the figure and the other items are still
empty). Design files named on the command line are not opened yet. `QtRocket --version` prints its version and
`QtRocket --help` its options. Its preferences are kept in an INI file: `~/.config/QtRocket/QtRocket.ini` on Linux
(under `$XDG_CONFIG_HOME` when that is set) and macOS, `%APPDATA%\QtRocket\QtRocket.ini` on Windows. The release
archives hold the command-line tool alone until the GUI is released (Milestone 9).

## The command-line tool
`QtRocket_cli` loads `.ork` designs, prints their key figures, runs their simulations and lists motors:

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
directory of the program).

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
- `data/motors/initial_motors.db`: the thrust-curve motor database (SQLite).
- `data/examples/*.ork`: OpenRocket's 16 example designs.

An installation (and every release archive) has this layout:

```text
bin/QtRocket_cli
share/QtRocket/motors/initial_motors.db
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
so no window appears. The lint tests (`tests/lint/`) check rules of the source tree that the compiler cannot, such
as the include order of the layers. Test data lives in `tests/data/`. ctest labels select parts of the suite:

```sh
ctest --preset clang-debug                 # everything
ctest --preset clang-debug -L golden       # the comparisons with OpenRocket's golden data
ctest --preset clang-debug -L cli          # QtRocket_cli run as a program
ctest --preset clang-debug -L gui          # the GUI's tests
ctest --preset clang-debug -L lint         # the checks of the source tree
ctest --preset clang-debug -LE golden      # everything but the golden comparisons
build/clang-debug/bin/QtRocket_core_tests --gtest_filter='BodyTube.*'
```

A GoogleTest suite whose name contains `Golden` gets the label `golden`, whatever its file; the comparisons are
mostly in `*_golden_tests.cpp`, and the CLI's milestone run (`cli.milestone`) carries the label too. Re-run them
after any change to the aerodynamics, mass, simulation, component geometry or file code.

### Golden data
`tests/data/goldens/` holds what OpenRocket computes for its 16 example designs and the 13 rockets of its
`TestRockets` class: geometry, mass and aerodynamics per flight configuration, the complete simulations at the
designs' own time step, the simulations at a stable 0.01 s step (complete for the test rockets; the summaries and
events for the examples), and OpenRocket's own re-save of each design. The Java harness in
`tools/openrocket-goldens/` writes it from a local OpenRocket checkout; CMake and CI never need Java. To regenerate
(JDK 17 and an OpenRocket checkout at the commit `tests/data/goldens/manifest.json` records):

```sh
tools/openrocket-goldens/generate.sh
ctest --preset clang-debug -L golden
```

[tools/openrocket-goldens/README.md](tools/openrocket-goldens/README.md) describes the harness, the file formats,
the tolerances of the comparisons and the other fixtures it can write.

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
designs are OpenRocket's. [NOTICE](NOTICE) gives the details and the notices of the third-party libraries that the
program contains.
