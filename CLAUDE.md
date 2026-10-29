# QtRocket

C++23 re-implementation of OpenRocket (Java, GPLv3): the Qt-free library `QtRocket_core` (component model,
aerodynamics, mass, flight simulation, `.ork` files, motor database, component presets, CSV export, the 2D
figure's geometry), the command-line tool `QtRocket_cli` (Milestone 1, the headless core) and the Qt 6 Widgets GUI
`QtRocket` (`gui/`: Milestone 2, OpenRocket's Design tab; Milestone 3, the "Motors & Configuration" tab, the motor
chooser and the parts library; no simulations, appearance or 3D views yet: `docs/gui-differences.md` lists what
comes later). Built with CMake presets + Ninja, tested with GoogleTest.
Cross-platform: Linux (GCC, Clang), macOS (Apple Clang) and Windows (MSVC).

## Commands
- Build and test (Clang Debug): `cmake --workflow --preset dev`
- Rebuild only: `cmake --build --preset clang-debug`
- Test only: `ctest --preset clang-debug`
- One test: `ctest --preset clang-debug -R 'BodyTube\.'` or
  `build/clang-debug/bin/QtRocket_core_tests --gtest_filter='BodyTube.*'` (the GUI's: `QtRocket_gui_tests`)
- By label: `ctest --preset clang-debug -L golden` (comparisons with OpenRocket's golden data, the GUI's too),
  `-L cli` (the command-line tool run as a program), `-L gui` (the GUI's tests, offscreen), `-L lint` (checks of
  the source tree: the layers, QtTest's macros), `-LE golden` (everything else)
- The tool: `build/clang-debug/bin/QtRocket_cli info "data/examples/A simple model rocket.ork"` (and `--help`); the
  GUI: `build/clang-debug/bin/QtRocket [file...]`
- Before finishing a change, also run `cmake --workflow --preset tidy` (clang-tidy, warnings are errors),
  `cmake --workflow --preset asan` (AddressSanitizer + UBSan) and `cmake --workflow --preset ci-gcc` (GCC Release,
  warnings are errors), then CI's clang-tidy and clang-format (section clang-tidy). Run one preset at a time (two
  runs in one build directory corrupt ninja's log), send the output to a log file and grep it: the test suite
  takes minutes under `dev` and about half an hour under `asan`
- On Windows the presets are `msvc-debug` (workflow `dev-msvc`), `msvc-release` and `ci-msvc`, and cmake must run
  in a Developer PowerShell for VS. `tidy`, `asan`, `tsan` and `coverage` exist on Linux and macOS only
- Formatting is automatic: a Claude Code hook (`.claude/hooks/format-cpp.sh`) runs clang-format on every C/C++
  file right after you edit it; a file written another way (a shell heredoc) needs `clang-format -i`. The pre-commit
  hook (`.githooks/pre-commit`, enabled by `git config core.hooksPath .githooks`) and CI reject unformatted files
- API docs (Doxygen): `cmake --build --preset clang-debug --target docs`, then `build/clang-debug/docs/html/`

Other presets: `clang-release`, `gcc-debug`, `gcc-release`, `tsan`, `coverage`, `ci-gcc`, `ci-clang`, and
`dist-linux`, `dist-macos`, `dist-windows` (release archives, in `build/dist-<os>/package/`). Each builds into
`build/<preset>/`; never edit anything under `build/`. A preset is only available on the platforms it supports
(`gcc-*`: Linux; `clang-*`: Linux and macOS; `msvc-*`: Windows); `cmake --list-presets` shows this machine's.

Releases: the `Release` GitHub workflow (`.github/workflows/release.yml`) runs only when started by hand. It tags
`v<project VERSION>` and publishes the `dist-*` archives, so the version is raised in `project()` in
`CMakeLists.txt` (keep `project(QtRocket VERSION x.y.z` on one line: the workflow reads it with sed). An archive
holds what the `install()` rules install: `bin/QtRocket_cli`, `share/QtRocket/` (the contents of `data/`) and
`share/doc/QtRocket/` (LICENSE, NOTICE, README.md); the `dist-*` presets build no GUI (until Milestone 9). Start
the workflow or create a tag only when the user asks.

## Layout
- `include/QtRocket/<subsystem>/`: public headers (some subsystems have subdirectories: `file/openrocket/`,
  `simulation/extension/`, ...)
- `src/CMakeLists.txt`: the static library `QtRocket_core` (alias `QtRocket::core`) and its subsystems;
  `src/<subsystem>/CMakeLists.txt` adds the subsystem's sources with `target_sources(QtRocket_core PRIVATE ...)`
- `src/cli/`: `QtRocket_cli_lib` (alias `QtRocket::cli`: the commands, headers in `include/QtRocket/cli/`) and the
  `QtRocket_cli` executable (`main.cpp`). It finds its data in `<directory of the program>/../share/QtRocket`, in
  the build tree (where the target `QtRocket_data` copies `data/`) as in an installed archive; so does the GUI
- `gui/`: the GUI (section GUI): `QtRocket_gui` (alias `QtRocket::gui`), headers in
  `gui/include/QtRocket/gui/<area>/`, sources in `gui/src/<area>/` (a `CMakeLists.txt` per area), `gui/resources/`
  (OpenRocket's icons; `icons/README.md` lists them), and the program `QtRocket` (`gui/main.cpp`)
- `docs/gui-differences.md`: every difference from OpenRocket's GUI that a user can notice (README links it)
- `tests/core/<subsystem>/`: GoogleTest files, all in `QtRocket_core_tests` (`tests/core/CMakeLists.txt`);
  `tests/core/goldens/` compares with OpenRocket's golden data; `tests/core/cli/` also runs the executable through
  `RunCli.cmake` (`tests/core/cli/CMakeLists.txt`). The helper sources (every `.cpp` there without a `TEST`) form
  the Qt-free library `QtRocket_test_support`, which both test executables link: a new helper goes into its list
- `tests/gui/<area>/`: the GUI's tests, all in `QtRocket_gui_tests` (`tests/gui/CMakeLists.txt`, a `CMakeLists.txt`
  per area); `tests/gui/exit/`: the exit suites of Milestone 2 (section GUI); the shared helpers in `tests/gui/`
- `tests/lint/`: the lint tests (`cmake/CheckLayers.cmake`, `cmake/CheckNoQtTestMacros.cmake`), on the source tree
  and on planted trees that each break one rule
- Test support to reuse, never duplicate (`find tests/core tests/gui -name '*.h'` lists it all):
  - core: `tests/core/TestPaths.h`, `tests/core/TestTempDir.h`, `tests/core/unit/DefaultUnitsGuard.h`,
    `tests/core/rocket/TestRockets.h` (OpenRocket's TestRockets), `tests/core/document/TestDocuments.h` (its 16
    document makers), `tests/core/motor/TestMotorDatabase.h` (the bundled motors, `smallMotorDatabase()`,
    `estesA8Database()`), `tests/core/rocket/preset/TestPresetDatabase.h` (the bundled presets,
    `smallPresetDatabase()`) and `ExamplePresets.h`, `tests/core/simulation/FlightDifferences.h`,
    `tests/core/file/DesignFileEnvironment.h`, `DesignFileState.h`, `ExampleOrkFiles.h`, `RecordedOrkFiles.h` and
    `OpenRocketSaverTestMotors.h` in `tests/core/file/openrocket/`, `tests/core/goldens/Golden*.h` (with
    `GoldenGuiReference.h`, the GUI reference's scenario and records) and `GuiReferenceEnvironment.h`,
    `tests/core/figure/FigureTestSupport.h`, `ShapePinSupport.h` and `DesignPinSupport.h`,
    `tests/core/cli/CliRunSupport.h`
  - GUI (`tests/gui/`, namespace `QtRocket::Gui::Test`): `GuiTestSupport.h` (the fixture `GuiTest`),
    `ExpectedQtMessage.h`, `ScriptedUserPrompts.h`, `HookedUserPrompts.h`, `ForwardingUserPrompts.h` (a base for a
    double that overrides one question), `NextModalDialog.h` (for the widget prompts and the dialogs themselves),
    `TestGuiServices.h`, `RecordingGuardHook.h`, `ScopedApplicationPalette.h`; in the areas
    `configdialog/ConfigDialogTestSupport.h` (the fixture `ConfigDialogTest`), `dialogs/PreferencesPageTranscript.h`
    (`chooseItem()`), `main/FileMenusTestSupport.h`, `widgets/WidgetTestSupport.h` (focus, typing, the wheel),
    `figure/RecordingPaintDevice.h` (what is painted, without pixels), `FigureMouse.h`, `tree/TreeViewMouse.h`,
    `exit/FromScratchScenario.h`, `plot/PlotReadback.h` (a plot's series, axes and drawn texts, without
    pixels), `flightconfig/FlightConfigurationTabSupport.h` (the tab in a window of its own),
    `binding/PresetTestSupport.h`, `dialogs/motor/MotorChooserTestSupport.h`,
    `dialogs/preset/PresetTableTestSupport.h`
  - and the `*Support.h`, `*Pins.h` and `*Transcript.h` headers beside each subsystem's or area's tests
  - the bundled motor and preset databases are read once per test process (the presets take a second and a
    half, several under ASan): never per test; models, choosers and `QAbstractItemModelTester` take the small ones
- `tests/data/`: test fixtures (`ork/`, `motors/`, `csv/`, `cli/`, `fin-images/`, ...) and `goldens/`, OpenRocket's
  results for the 16 examples and the 13 test rockets (generated; never edited by hand), with the GUI reference
  `goldens/gui/from-scratch/` (`scenario.json`, its input, is written by hand)
- `data/`: the shipped data, installed to `share/QtRocket/`: `motors/initial_motors.db` (OpenRocket's motor
  database) with `metadata.json` (its version), `components/` (the bundled presets, byte for byte: `database/`,
  the Apache-2.0 openrocket-database, and `internal/`, OpenRocket's legacy files) and `examples/*.ork`
  (OpenRocket's 16 example designs)
- `tools/`: the Java programs that write the generated data, mostly from OpenRocket's compiled classes, each with
  a README that says how to run it; never built by CMake or CI. `openrocket-goldens/` (`tests/data/goldens/`; its
  `figure-pins/`: the core's figure pin tables), `openrocket-test-documents/` (`goldens/test-documents/`),
  `openrocket-gui-reference/` (`goldens/gui/from-scratch/`), `openrocket-gui-pins/` (the GUI tests' pin tables,
  one subdirectory per family), `openrocket-panel-pins/` (the configuration panels' pins),
  `openrocket-component-factory/` (`tests/data/component-factory/`), `openrocket-expression-parser/`
  (`src/util/ExpressionParserTables.h` and its parity cases), `openrocket-fin-import/` (`tests/data/fin-images/`),
  `openrocket-motor-chooser/` (the core's pins of `motor/chooser/` and MotorCorrelation),
  `openrocket-collation/` (`src/util/JavaCollationTables.h`, JDK 17's en_US collator, and its pins),
  `openrocket-probes/` (probes whose output tests pin by hand, with what they printed)
- `cmake/ProjectOptions.cmake`: `QtRocket_configure_target()` (warnings, sanitizers, coverage, tidy),
  `QtRocket_configure_qt_target()` and `QtRocket_automoc_follows_sources()`;
  `cmake/Dependencies.cmake`: third-party libraries via FetchContent (pugixml, minizip-ng with zlib, SQLite,
  fast_float; GoogleTest and nlohmann_json for the tests only; with the GUI, Qt's lookup and rocketplot)

## Layers
The subsystems of `QtRocket_core`, in the order of `src/CMakeLists.txt`; each includes only those before it:
`util` → `unit` → `logging` → `preferences` → `material` → `motor` → `rocket` → `models` → `mass` → `aero` →
`simulation` → `document` → `file` → `figure` (the Qt-free 2D figure geometry). `cli/` sits on top of the core, and
so does the GUI (its own order: section GUI). So `rocket/` never includes `aero/`, `mass/` or `simulation/`,
`simulation/` never includes `document/`, and `document/` never includes `file/`. The comment in
`src/CMakeLists.txt` explains where this order differs from OpenRocket's packages. The test `lint.layers`
(`cmake/CheckLayers.cmake`) checks both orders and that an include names its subsystem (never `../`); a new
subsystem or area is added there too.

## Conventions
- Headers are `.h` (never `.hpp`) and use `#pragma once`
- One class per file, named exactly after the class, including capitalization: `class BodyTube` lives in
  `include/QtRocket/rocket/BodyTube.h` and `src/rocket/BodyTube.cpp`, and its tests in
  `tests/core/rocket/BodyTubeTests.cpp`. Tests that are not about one class: `lower_snake_case_tests.cpp`
- Code lives in `namespace QtRocket` (test helpers in `QtRocket::Test`); project includes use quotes and the
  subsystem: `#include "QtRocket/rocket/BodyTube.h"`
- Every new target must call `QtRocket_configure_target(<target>)` (a Qt target
  `QtRocket_configure_qt_target(<target>)`)
- New source files go into their `src/<subsystem>/CMakeLists.txt`, new tests into `tests/core/CMakeLists.txt`
  (grouped by subsystem in the order of `src/`); keep each list alphabetical, ignoring case
- The core is Qt-free: no Qt header or `Q*` type in `include/` or `src/`. Qt code lives only in `gui/`
- Errors: a recoverable failure (parsing, loading, I/O, the motor database, the command line) is a `Result<T>`
  (`std::expected<T, Error>`, `util/Error.h`). Loaders and savers return a `Result` and collect warnings; they never
  throw. A broken invariant is a programming error: a `BugError` (`util/BugError.h`, a `std::logic_error`) through
  `QTROCKET_ASSERT`, `bug()` or `QTROCKET_UNREACHABLE()`, never caught by the core; never throw a plain
  `std::logic_error`. A simulation abort is a `FlightEvent` (`SIM_ABORT`), and the `SimulationException` family
  (`simulation/exception/`) is the only exception that crosses the simulation loop
- No owning raw pointers and no raw `new`/`delete`: the component tree owns its children with `std::unique_ptr`;
  observers hold raw pointers (the GUI's Qt objects: `makeChild()`, section GUI)
- Threads: a `Rocket`, `OpenRocketDocument` or `Simulation` belongs to one thread. A simulation runs on another
  thread only as a `Simulation::duplicateForIndependentSimulation()` (made on the owning thread; it owns its copy
  of the rocket) and hands its `FlightData` back by value; a loaded motor database is immutable and shared. After
  a change to `simulation/` or to the rocket's copies, also run `cmake --workflow --preset tsan`
- Port OpenRocket faithfully: read the Java source line by line, never work from memory. A ported JUnit test keeps
  its expectations and tolerances and names its Java original in a comment (`Port of ...`); never loosen or drop an
  assertion to make a test pass. A deliberate difference from OpenRocket is documented where it is made; one of the
  GUI that a user can notice also goes into `docs/gui-differences.md`. An OpenRocket bug is fixed only by the
  user's decision, under its number there ("bug 6": the source's comments give the number)
- Golden tests: a GoogleTest suite whose name contains `Golden` gets the ctest label `golden`, whatever its file
  and in both test executables (the comparisons are mostly in `*_golden_tests.cpp`; the CLI's milestone run
  `cli.milestone` carries it too). Re-run `ctest --preset clang-debug -L golden` after any change to `aero/`,
  `mass/`, `simulation/`, the geometry of `rocket/`, `file/` or `figure/`. Golden data and generated test tables
  change only through their generators
- Tests are deterministic and leave no process-wide state behind (default units, the flight data type registry,
  shared preferences, the current directory). An optional measurement or a tool run by hand is a `DISABLED_` test
- Plots come from rocketplot by FetchContent at tag `v0.6.0` (only with the GUI), with `SYSTEM` and without
  `EXCLUDE_FROM_ALL` (which would drop its licence files' install rules); `ROCKETPLOT_SANITIZERS` is set from
  `QTROCKET_SANITIZERS` before `FetchContent_MakeAvailable`, and the `asan`, `tsan` and `coverage` presets set
  `FETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER` (an installed rocketplot has no sanitizer instrumentation)

## GUI
Qt 6 Widgets, Qt 6.8 or newer (Core, Gui, Widgets, Svg; Test for the tests). The option `QTROCKET_BUILD_GUI` (ON,
with Qt required) builds it; `-DQTROCKET_BUILD_GUI=OFF` builds the core and the CLI alone.
- Areas of `gui/`, in include order (each includes only those before it, and any subsystem of the core):
  `util` → `settings` → `binding` → `widgets` → `plot` → `tree` → `figure` → `configdialog` → `dialogs` →
  `flightconfig` → `main` → `app`, with `gui/main.cpp` on top. `binding/` is Qt-free (no Qt header, no other area);
  a lower area reaches a higher one only through signals or injected interfaces; the GUI never includes
  `QtRocket/cli/`. Namespace `QtRocket::Gui` (test helpers `QtRocket::Gui::Test`),
  `#include "QtRocket/gui/util/MakeChild.h"`
- rocketplot: only `plot/` includes it, as `<rocketplot/X.h>` (`lint.layers`, which also refuses a quoted
  include); the other areas use it through `plot/`'s classes (tests may include it to read a plot back). Never
  forward-declare `namespace rocketplot` (clang-tidy's naming check reports it): include the header. Every text
  given to rocketplot goes through `rocketplot::literalText()`: a `$` starts math, of which rocketplot, built
  without its math font, warns once per process (a test would fail or pass by test order). Every plot calls
  `plot/PlotTheme::apply()` when it is made and at a `PaletteChange` of the widget that holds it; rocketplot's own
  dialogs (Export..., Color...) are replaced through `plot/PlotExport`
- Every Qt target calls `QtRocket_configure_qt_target()`: per-target AUTOMOC and `QT_NO_KEYWORDS` (also PUBLIC on
  `QtRocket_gui`: the core's `Signal::emit()`), `QT_ENABLE_STRICT_MODE_UP_TO=0x060800`,
  `QT_WARN_DEPRECATED_UP_TO=0x060800`, `QT_NO_CAST_FROM_ASCII`. So `Q_SIGNALS`/`Q_EMIT`, no `Q_SLOTS` (connect
  member function pointers or lambdas, always with a context object, also for a queued call), and nothing newer
  than Qt 6.8 or deprecated by it (CI builds Qt 6.8.3 on macOS and Windows, the newest on Linux)
- Every header is listed with its source in its area's `CMakeLists.txt` (AUTOMOC reads only listed headers: an
  unlisted `Q_OBJECT` header fails to link, "undefined reference to vtable"; `QtRocket_automoc_follows_sources()`
  makes AUTOMOC run again when a list changes). Resources only through the `qt_add_resources()` call in
  `gui/resources/CMakeLists.txt` (a `.qrc` with AUTORCC in a static library loses its files at run time). Layouts
  are built in code (no `.ui` files). No `<windows.h>`: its macros `ABSOLUTE`, `RELATIVE` and `ERROR` clash with
  enum constants of the core
- Ownership: `util/MakeChild.h` holds the GUI's only `new`: `makeChild<T>(parent, args...)` for a parented
  `QObject`, `makeQtOwned<T>(args...)` for a non-`QObject` that a Qt API takes over at once (a `QIconEngine`). A
  parentless window or dialog lives on the stack or in a `std::unique_ptr`; a parented object goes with
  `deleteLater()`, never `delete`; a modal question's dialog through `dialogs/ModalDialog` (on the heap: its parent
  may be destroyed during its event loop; `dialogs/OptionDialog` for JOptionPane's)
- Services: `util/GuiServices` (preferences, materials, motors, presets, loading context, `UserPrompts`, the
  start's data, by reference; keep it trivially copyable), built for the program only by
  `app/ApplicationContext`. Every modal question goes through `UserPrompts` (widgets:
  `main/WidgetUserPrompts`). The only process-wide state: `main/OpenRocketClipboard::instance()`, the configuration
  dialog (`ComponentConfigDialog`'s statics), `Guard`'s error hook and `UndoRedoActions`' undo-error flag
  (`resetUndoErrorReported()`); `ComponentIcons`' icon cache is a cache. A window's parts ask for the configuration
  dialog by signal (`MainWindow::configDialogRequested`), which `main/ConfigDialogRequests` answers;
  `app/ApplicationSession` makes it, with the error hook, the windows, the file operations and the recent files (a
  `WindowManager` alone answers no request). `gui/main.cpp`'s test hooks: `QTROCKET_TEST_SETTINGS_DIRECTORY`,
  `QTROCKET_TEST_USER_DATA_DIRECTORY`, `QTROCKET_TEST_QUIT_AFTER_START` (the ctests `gui.version`, `gui.help`,
  `gui.start`, labels `gui` and `lint`: nothing may reach the standard error)
- Text: every user-visible string is `tr("<the English text of OpenRocket's messages.properties>")` with a
  `//: <key>` translator comment above it (OpenRocket's typos verbatim; "OpenRocket" becomes "QtRocket" in
  QtRocket's own texts); core enums through `util/EnumText`, menu texts through `util/MenuText` (mnemonics, `&`);
  other literals are `QStringLiteral`; non-ASCII as escapes (no digit right after an octal escape: MSVC's C4125;
  the editing tools can turn a typed `\u` escape into its character: check with `grep -P '[^\x00-\x7F]'`).
  Paths go through `pathFromUtf8`/`pathToUtf8`, never `toStdString()`
- The core's objects: no component pointer survives a `TREE` or `UNDO` event (`Rocket::loadFrom` replaces every
  component: keep ids); never undo, redo or `addUndoPosition` inside a rocket slot (queue it); never `freeze()` a
  frozen rocket; Cancel closes a dialog before it undoes; a pending edit committed before an undo or redo can end
  the redo history (check again). A `BugError` is caught only by `Guard` (`guarded()` around every connection into
  the core) and the `QApplication::notify` backstop
- clang-tidy: include Qt's forwarding headers (`<QWidget>`, `<Qt>` for the `Qt::` enums); `qtmetamacros.h` and
  `QtTest/qtest*.h` (`Q_OBJECT`, `QTest::keyClicks`, ...) are ignored by misc-include-cleaner. `qsizetype` against
  `std::size_t` trips GCC's `-Wuseless-cast`/`-Wsign-conversion` and MSVC's C4267/C4244: convert explicitly, only
  where needed. A `makeChild()` result goes into `auto*` (modernize-use-auto); kept only in a local or a `QPointer`,
  it is a leak to clang-analyzer, which cannot see Qt's parent: hand it to Qt (a layout, `connect()`) or a member.
  misc-const-correctness wants test locals `const` when only const accessors are used (`const MainWindow&`)
- Tests: GoogleTest with QtTest's helpers only (`QTest::mouseClick`, `QTest::keyClicks`, `QSignalSpy`,
  `QTest::qWaitFor`, `QAbstractItemModelTester` in its Warning mode). QtTest's assertion macros (`QVERIFY`,
  `QCOMPARE`, `QTRY_*`, `QFAIL`, `QSKIP`, `QEXPECT_FAIL`, `QFETCH`, `QTEST`) pass silently outside QtTest:
  `lint.no-qttest-macros` rejects them. `GuiTestMain.cpp` runs every test under `QtRocket::Gui::Application`,
  offscreen (unless `QT_QPA_PLATFORM` says otherwise; Qt's default keys, the Windows scheme), in en_US, painting on
  the GUI thread only (`QT_NO_GUI_THREADPOOL`, for TSan), QSettings in a temporary directory, and fails a test on a
  Qt warning no `ExpectedQtMessage` expects or an error report (`Guard`) no `RecordingGuardHook` records. A test is
  a `TEST_F` of the fixture `GuiTest`, through an alias named after the class (`using MainWindow = GuiTest;`),
  which fails it for a top-level widget, clipboard content (`OpenRocketClipboard::instance()` too: give a
  `WindowManager` its own), a default unit, a current directory or a palette left behind; one that opens the
  configuration dialog calls `ComponentConfigDialog::forgetPreviousDialogs()` (or derives from `ConfigDialogTest`).
  The whole application: an `ApplicationSession` over `SettingsPreferences` of a temporary INI file and the test's
  prompts (`app/design_window_integration_tests.cpp`); there `DataPaths::forApplication()` has no bundled presets
  and no user data directory (QStandardPaths' test mode): a test opts in with `withComponentDirectory()` or
  `withUserDataDirectory()`. `TestGuiServices` turns "Automatically open the Parts Library dialog" off (a new
  component would ask for a preset; `GuiTestSupport.h`). Never show a window on the desktop; every modal question
  is answered by a scripted double; no pixel or font assertions (compare geometry and texts); `objectName` = the
  Java property. A test that shows Qt's own `QFileDialog` is a `WidgetFileDialogTest`
  (`main/WidgetUserPromptsTests.cpp`): only that suite runs under the `RESOURCE_LOCK` `qt_file_dialog_settings` of
  `tests/gui/CMakeLists.txt`, as QFileDialog's history lives in the registry or CFPreferences on Windows and macOS,
  where parallel dialogs watch each other's temporary directories
- Labels: `tests/gui/` gives its tests the label `gui` as a directory property and its `*Golden*` suites `golden`
  through a second `gtest_discover_tests()` (a list in its `PROPERTIES` loses its `;`). A `TEST_P`'s printed
  parameter becomes part of the ctest name: print a short name, never a path
- Exit suites (`tests/gui/exit/`): `FromScratchExitGolden` builds the rocket of
  `tests/data/goldens/gui/from-scratch/scenario.json` through the GUI alone (no core setter), saves it and compares
  every step and the save with OpenRocket's; `OpenDesignsExitGolden` opens the examples, the re-saves and that
  reference through File > Open and compares the tree, the dialogs, the figure and the overlay with the goldens. A
  scenario change is followed by `tools/openrocket-gui-reference/generate.sh`, and a step must be one a user can do in
  OpenRocket's own GUI (the tool refuses a disabled button or Automatic box, but no other disabled field). The
  DISABLED_ `FromScratchInteropSave` writes the save into `build/<preset>/interop-saves/` (after the core's
  `OrkInteropSaves`, which removes it as an earlier save) for `tools/openrocket-goldens/interop.sh`, which checks it
  as the input `gui-from-scratch` against OpenRocket's save of the scenario
- Traps: `QCoreApplication::processEvents()` does not run `deleteLater()` (`sendPostedEvents(nullptr,
  QEvent::DeferredDelete)`), nor does a new palette reach the widgets before `sendPostedEvents()`; inside a
  `TEST_F` body `Test::` is `testing::Test` (write `Gui::Test::`); `QTest::mouseDClick()` sends the double click
  alone (a real one is press, release, double click, release); a combo box's item is chosen offscreen with
  `setCurrentIndex()` and `activated()`, not through its popup, and a popup shown offscreen needs
  `OffscreenPopupWarnings` (`widgets/WidgetTestSupport.h`); a `QDialogButtonBox` makes its first accepting button
  the default when it is shown, whatever its `autoDefault` (plain buttons where Return must not accept); a member
  declared after `Q_SIGNALS:` without an access specifier is a signal; `QTreeView::visualRect()` is no
  visibility test; a window shown offscreen warns "This plugin does not support propagateSizeHints()" unless its
  layout was activated before `show()` (the tests' `ExpectedQtMessage` drops it, the program's `gui.start` does
  not); Qt's traps on other platforms are in section Portability. On Windows, Qt's `bin` must be on PATH to run
  the GUI and its tests (also ctest's listing)
- After a change to `gui/`, the `tidy` and `asan` presets as for the core

## clang-tidy
The `tidy` preset and CI run clang-tidy (`.clang-tidy`) with warnings as errors:
- Naming: types, namespaces and template parameters `CamelCase`; functions, methods and variables `camelBack`;
  private and protected members `m_`; mutable globals `g_`, mutable statics and static members `s_`; constexpr,
  static and global constants `kCamelCase` (a local non-`constexpr` `const`: `camelBack`); enum constants and
  macros `UPPER_CASE`
- Every getter and pure function is `[[nodiscard]]`; single-argument constructors are `explicit`
- No pointer arithmetic, `reinterpret_cast` or `const_cast` (`std::bit_cast` for the bits of a double); include
  exactly the headers whose symbols you use (misc-include-cleaner, which does not see a member used through a
  `std::shared_ptr<const T>`: bind a `const T&`); `std::format`/`std::print`, never printf or iostream formatting;
  no `std::getenv` in a test (concurrency-mt-unsafe). GCC's `-Wdangling-reference` (ci-gcc) flags a reference to
  the result of a function given a temporary `const std::string&` or `const QString&`: take `std::string_view`
- A suppression is `// NOLINTNEXTLINE(check-name): reason` on a line of its own, short enough not to be wrapped,
  or for a whole table a `// NOLINTBEGIN(check-name): reason` ... `// NOLINTEND(check-name)` block; never a
  trailing `NOLINT`
- Split long functions into helpers (readability-function-cognitive-complexity). A `TEST` body made only of gtest
  macros is not counted, but a loop, branch or lambda in it brings the full count, about 4 per macro (a helper
  with `EXPECT`s: a named functor or function, not a lambda). bugprone-unchecked-optional-access does not see
  `ASSERT_TRUE(o.has_value())`: return a plain value after `ADD_FAILURE()`
- Versions: CI's Linux jobs take Arch's clang-tidy and clang-format (23.1.3), which flag more than this machine's
  22 (misc-const-correctness on variables, references and pointees, readability-redundant-*, modernize-use-string-
  view, performance-faster-string-find, ...) and format a few constructs differently. Before a push, check with
  CI's version from PyPI: `python3 -m venv <dir> && <dir>/bin/pip install clang-tidy==23.1.3 clang-format==23.1.3`,
  then after a `tidy` preset build `<dir>/bin/clang-tidy -p build/tidy --quiet <file>` on every `.cpp` you created
  or changed and `<dir>/bin/clang-format --dry-run --Werror <files>`; both versions of both tools must be clean
  (reshape code that the two clang-formats format differently)
- A sweep by hand (clang-tidy over `compile_commands.json`, the duplicate-symbol scan of the object files for the
  macOS trap of section Portability) skips `build/<preset>/_deps/`: rocketplot's sources and objects are there

## Portability
Code must build warning-free and pass its tests on Linux, macOS and Windows (the `ci-*`, `tidy` and `dist-*` presets
build with `-Werror` under GCC and Clang, `/W4 /WX /utf-8` under MSVC). CI builds all three; locally only Linux is
built. Use the standard library (`<filesystem>`, `<thread>`, `<chrono>`) over POSIX or Win32 APIs; when an OS API
is unavoidable, keep it in one source file behind an `#ifdef _WIN32` / `__APPLE__` / `__linux__` split, with a
branch for each platform (as `src/cli/DataDirectory.cpp` does). Traps CI has caught:
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
- GCC's `-O3 -Wnull-dereference` (ci-gcc) gives false positives without a line: on `QPointer::operator->` into an
  inline accessor (take `data()` into a raw pointer and `QTROCKET_ASSERT` it), on `EXPECT_EQ` of a
  `std::vector<std::string_view>` (use `std::string`) and on indexing a copied `std::vector` in a `TEST`
- Qt: a `QMessageBox` has no window title on macOS; `QWidget::windowFilePath()` keeps native separators on Windows,
  `QSettings::fileName()` forward slashes; on macOS an exception thrown in an event handler terminates before
  `notify()`; Apple arm64 fuses `a * b + c` (in Qt's `QTransform` and `QPainterPath` too), so a hit test exactly
  on an edge may flip, and text positions pass through `QFixed` (1/64 px); CI's Qt has no TIFF image plugin
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
- OpenRocket's source, the reference for every port, is a read-only checkout beside the main QtRocket checkout
  (`/home/travis/Development/openrocket`; `tests/data/goldens/manifest.json` names the goldens' commit); never
  modify it. Java values no JUnit test pins come from a small Java program run against its compiled core
  (`core/build/classes/java/main`, JDK 17; Swing's in `swing/build/classes/java/main`, and a probe that makes Swing
  panels runs under `xvfb-run` with `-Djava.awt.headless=false`). A probe whose output a test pins goes into
  `tools/` (`openrocket-gui-pins/` for a generated GUI table, `openrocket-probes/` for values pinned by hand)
- The roadmap (Milestones 1 to 9) and its decisions: `~/.claude/plans/create-a-plan-to-inherited-torvalds.md`
