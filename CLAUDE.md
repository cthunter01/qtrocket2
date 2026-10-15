# QtRocket

C++23 re-implementation of OpenRocket (Java, GPLv3): the Qt-free library `QtRocket_core` (component model,
aerodynamics, mass, flight simulation, `.ork` files, motor database, CSV export) and the command-line tool
`QtRocket_cli`. Milestone 1, the headless core, is done; the Qt 6 GUI comes from Milestone 2. Built with CMake
presets + Ninja, tested with GoogleTest. Cross-platform: Linux (GCC, Clang), macOS (Apple Clang) and Windows (MSVC).

## Commands
- Build and test (Clang Debug): `cmake --workflow --preset dev`
- Rebuild only: `cmake --build --preset clang-debug`
- Test only: `ctest --preset clang-debug`
- One test: `ctest --preset clang-debug -R 'BodyTube\.'` or
  `build/clang-debug/bin/QtRocket_core_tests --gtest_filter='BodyTube.*'`
- By label: `ctest --preset clang-debug -L golden` (comparisons with OpenRocket's golden data), `-L cli` (the
  command-line tool run as a program), `-LE golden` (everything else)
- The tool: `build/clang-debug/bin/QtRocket_cli info "data/examples/A simple model rocket.ork"` (and `--help`)
- Before finishing a change, also run `cmake --workflow --preset tidy` (clang-tidy, warnings are errors),
  `cmake --workflow --preset asan` (AddressSanitizer + UBSan) and `cmake --workflow --preset ci-gcc` (GCC Release,
  warnings are errors). Run one preset at a time (two runs in one build directory corrupt ninja's log), send the
  output to a log file and grep it: the test suite takes minutes under `dev` and about half an hour under `asan`
- On Windows the presets are `msvc-debug` (workflow `dev-msvc`), `msvc-release` and `ci-msvc`, and cmake must run
  in a Developer PowerShell for VS. `tidy`, `asan`, `tsan` and `coverage` exist on Linux and macOS only
- Formatting is automatic: a Claude Code hook (`.claude/hooks/format-cpp.sh`) runs clang-format on every C/C++
  file right after you edit it. A file written another way (a shell heredoc) needs `clang-format -i`. The
  pre-commit hook (`.githooks/pre-commit`, enabled by `git config core.hooksPath .githooks`) and CI also reject
  unformatted files
- API docs (Doxygen): `cmake --build --preset clang-debug --target docs`, then `build/clang-debug/docs/html/`

Other presets: `clang-release`, `gcc-debug`, `gcc-release`, `tsan`, `coverage`, `ci-gcc`, `ci-clang`, and
`dist-linux`, `dist-macos`, `dist-windows` (release archives, in `build/dist-<os>/package/`).
Each builds into `build/<preset>/`; never edit anything under `build/`. A preset is only available on the
platforms it supports (`gcc-*`: Linux; `clang-*`: Linux and macOS; `msvc-*`: Windows); `cmake --list-presets`
shows this machine's.

Releases: the `Release` GitHub workflow (`.github/workflows/release.yml`) runs only when started by hand. It tags
`v<project VERSION>` and publishes the `dist-*` archives, so the version is raised in `project()` in
`CMakeLists.txt` (keep `project(QtRocket VERSION x.y.z` on one line: the workflow reads it with sed). An archive
holds what the `install()` rules install: `bin/QtRocket_cli`, `share/QtRocket/` (the contents of `data/`) and
`share/doc/QtRocket/` (LICENSE, NOTICE, README.md). Start the workflow or create a tag only when the user asks.

## Layout
- `include/QtRocket/<subsystem>/`: public headers (some subsystems have subdirectories: `file/openrocket/`,
  `simulation/extension/`, ...)
- `src/CMakeLists.txt`: the static library `QtRocket_core` (alias `QtRocket::core`) and its subsystems;
  `src/<subsystem>/CMakeLists.txt` adds the subsystem's sources with `target_sources(QtRocket_core PRIVATE ...)`
- `src/cli/`: `QtRocket_cli_lib` (alias `QtRocket::cli`: the commands, headers in `include/QtRocket/cli/`) and the
  `QtRocket_cli` executable (`main.cpp`). It finds its data in `<directory of the program>/../share/QtRocket`, in
  the build tree (a copy of the motor database) as in an installed archive
- `tests/core/<subsystem>/`: GoogleTest files, all in `QtRocket_core_tests` (`tests/core/CMakeLists.txt`);
  `tests/core/goldens/` compares with OpenRocket's golden data; `tests/core/cli/` also runs the executable through
  `RunCli.cmake` (`tests/core/cli/CMakeLists.txt`)
- Test support to reuse, never duplicate: `tests/core/TestPaths.h`, `tests/core/TestTempDir.h`,
  `tests/core/rocket/TestRockets.h` (OpenRocket's TestRockets), `tests/core/motor/TestMotorDatabase.h`,
  `tests/core/file/DesignFileEnvironment.h`, `tests/core/goldens/Golden*.h`, `tests/core/cli/CliRunSupport.h`
- `tests/data/`: test fixtures (`ork/`, `motors/`, `csv/`, `cli/`, ...) and `goldens/`, OpenRocket's results for
  the 16 examples and the 13 test rockets (generated; never edited by hand)
- `data/`: the shipped data, installed to `share/QtRocket/`: `motors/initial_motors.db` (OpenRocket's motor
  database) and `examples/*.ork` (OpenRocket's 16 example designs)
- `tools/openrocket-goldens/`: the Java harness that writes `tests/data/goldens/` (its README says how); never built
  by CMake or CI
- `cmake/ProjectOptions.cmake`: `QtRocket_configure_target()` (warnings, sanitizers, coverage, tidy)
- `cmake/Dependencies.cmake`: third-party libraries via FetchContent (pugixml, minizip-ng with zlib, SQLite,
  fast_float; GoogleTest and nlohmann_json for the tests only)

## Layers
The subsystems of `QtRocket_core`, in the order of `src/CMakeLists.txt`; each includes only those before it:
`util` → `unit` → `logging` → `preferences` → `material` → `motor` → `rocket` → `models` → `mass` → `aero` →
`simulation` → `document` → `file`. `cli/` sits on top of the core (the GUI will too). So `rocket/` never includes
`aero/`, `mass/` or `simulation/`, `simulation/` never includes `document/`, and `document/` never includes `file/`.
The comment in `src/CMakeLists.txt` explains where this order differs from OpenRocket's packages.

## Conventions
- Headers are `.h` (never `.hpp`) and use `#pragma once`
- One class per file, named exactly after the class, including capitalization: `class BodyTube` lives in
  `include/QtRocket/rocket/BodyTube.h` and `src/rocket/BodyTube.cpp`, and its tests in
  `tests/core/rocket/BodyTubeTests.cpp`. Tests that are not about one class: `lower_snake_case_tests.cpp`
- Code lives in `namespace QtRocket` (test helpers in `QtRocket::Test`); project includes use quotes and the
  subsystem: `#include "QtRocket/rocket/BodyTube.h"`
- Every new target must call `QtRocket_configure_target(<target>)`
- New source files go into their `src/<subsystem>/CMakeLists.txt`, new tests into `tests/core/CMakeLists.txt`
  (grouped by subsystem in the order of `src/`); keep each list alphabetical, ignoring case
- The core is Qt-free: no Qt header or `Q*` type in `include/` or `src/`. Qt code lives only in `gui/` (Milestone 2)
- Errors: a recoverable failure (parsing, loading, I/O, the motor database, the command line) is a `Result<T>`
  (`std::expected<T, Error>`, `util/Error.h`). Loaders and savers return a `Result` and collect warnings; they never
  throw. A broken invariant is a programming error: a `BugError` (`util/BugError.h`, a `std::logic_error`) through
  `QTROCKET_ASSERT`, `bug()` or `QTROCKET_UNREACHABLE()`, never caught by the core; never throw a plain
  `std::logic_error`. A simulation abort is a `FlightEvent` (`SIM_ABORT`), and the `SimulationException` family
  (`simulation/exception/`) is the only exception that crosses the simulation loop
- No owning raw pointers and no raw `new`/`delete`: the component tree owns its children with `std::unique_ptr`;
  observers hold raw pointers
- Port OpenRocket faithfully: read the Java source line by line, never work from memory. A ported JUnit test keeps
  its expectations and tolerances and names its Java original in a comment (`Port of ...`); never loosen or drop an
  assertion to make a test pass. A deliberate difference from OpenRocket is documented where it is made
- Golden tests: their suite names contain `Golden` (files `*_golden_tests.cpp`), which gives them the ctest label
  `golden`. Re-run `ctest --preset clang-debug -L golden` after any change to `aero/`, `mass/`, `simulation/`,
  the geometry of `rocket/`, or `file/`. Golden data and generated test tables change only through their generators
- Tests are deterministic and leave no process-wide state behind (default units, the flight data type registry,
  shared preferences, the current directory). An optional measurement or a tool run by hand is a `DISABLED_` test
- GUI (from Milestone 2): every user-visible string goes through `tr()`; layouts are built in code (no `.ui`
  files); QCustomPlot sits in `third_party/` as its own target with warnings off, excluded from clang-tidy
  (`.clang-tidy` `ExcludeHeaderFilterRegex`) and coverage

## clang-tidy
The `tidy` preset and CI run clang-tidy (`.clang-tidy`) with warnings as errors:
- Naming: types `CamelCase`; functions, methods and variables `camelBack`; private and protected members `m_`;
  constexpr, static and global constants `kCamelCase`; enum constants `UPPER_CASE`
- Every getter and pure function is `[[nodiscard]]`; single-argument constructors are `explicit`
- No pointer arithmetic, `reinterpret_cast` or `const_cast` (`std::bit_cast` for the bits of a double)
- Include exactly the headers whose symbols you use (misc-include-cleaner)
- `std::format`/`std::print`, never printf or iostream formatting
- A suppression is `// NOLINTNEXTLINE(check-name)` on a line of its own with the reason on the line above, never a
  trailing `NOLINT`
- Split long functions into helpers (readability-function-cognitive-complexity). A `TEST` body made only of gtest
  macros is not counted, but a loop, branch or lambda in it brings the full count, about 4 per macro
- No `std::getenv` in a test (concurrency-mt-unsafe)

## Portability
Code must build warning-free and pass its tests on Linux, macOS and Windows (the `ci-*`, `tidy` and `dist-*` presets
build with `-Werror` under GCC and Clang, `/W4 /WX /utf-8` under MSVC). CI builds all three; locally only Linux is
built. Use the standard library
(`<filesystem>`, `<thread>`, `<chrono>`) over POSIX or Win32 APIs; when an OS API is unavoidable, keep it in one
source file behind an `#ifdef _WIN32` / `__APPLE__` / `__linux__` split, with a branch for each platform (as
`src/cli/DataDirectory.cpp` does). Traps CI has caught:
- MSVC: the iterators of `std::array`, `std::vector` and `std::string_view` are classes, so never `auto*` for
  `begin()`/`end()` or an algorithm's result; no implicit narrowing (C4244/C4267); a constant condition is C4127;
  no local named `near`, `far` or `free`; a non-ASCII character in a string literal is written as an escape; node
  containers (`std::map`, ...) are not nothrow-movable; in Release `a >= b ? a : b` on doubles can become `maxsd`,
  so decide signed zeros and NaN explicitly (`MathUtil::javaMax`/`javaMin`)
- Apple libc++ lacks `std::ranges::shift_*`/`fold_*`, `std::views::zip`/`enumerate`/`chunk`/`slide`/`stride`/
  `cartesian_product`/`join_with`/`concat`/`adjacent`, `std::generator`, `<stacktrace>`, `flat_map`,
  `move_only_function`, chrono time zones and floating-point `std::from_chars` (parse with
  `Strings::javaParseDouble` or fast_float)
- macOS: two functions of one namespace that differ only in their return type are one symbol there (Linux links
  them); give every test helper a distinct name
- Files are opened in binary mode and lines end with `'\n'` on every platform; paths go through
  `pathFromUtf8`/`pathToUtf8` (`util/FileIo.h`); `/x` is not `is_absolute()` on Windows
- GoogleTest removes its flags from `main`'s `argv` in place (on Windows the C runtime's `__argv` too): read
  `::testing::internal::GetArgvs()`
- Java, glibc, Apple's libm and MSVC's UCRT differ in the last bits of transcendental functions. Pin a double
  exactly (`EXPECT_EQ`) only for the results of `+ - * /`, `sqrt` and integer operations, or for text read and
  written without arithmetic; otherwise use a tolerance, with an absolute floor for a value that is zero but for
  rounding. A flight at the default 0.05 s time step is chaotic: never pin a row count, a late event time or a
  summary value of a fresh flight (stored flight data round-trips exactly)

## OpenRocket and the plan
- OpenRocket's source is the reference for every port: a read-only checkout in `openrocket` beside the main
  QtRocket checkout (`/home/travis/Development/openrocket`; `tests/data/goldens/manifest.json` names the commit of
  the goldens). Never modify it. Java values that no JUnit test pins come from a small Java program run against its
  compiled core (`core/build/classes/java/main`, JDK 17)
- The roadmap (Milestones 1 to 9) and its decisions: `~/.claude/plans/create-a-plan-to-inherited-torvalds.md`
  (outside the repository)
