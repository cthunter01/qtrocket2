# What OpenRocket's motor chooser computes: the pin tables of the core's motor tests

`tools/openrocket-motor-chooser` runs small Java probes against OpenRocket's compiled core and turns what
they print into the tables QtRocket's core tests of the motor chooser's logic compare with. The tables
are committed; neither CMake nor CI ever needs Java. They change only through this tool. Each
subdirectory is one family: its probe (`probe/*.java`, package `probe`) and the Python script that writes
the table.

| Family | Table | Probe | Generator | Read by |
|---|---|---|---|---|
| `correlation` | `tests/core/motor/MotorCorrelationPins.h` | `MotorCorrelationProbe` | `gen_motor_correlation_pins.py` | `MotorCorrelationTests.cpp` (`MotorCorrelationPinsGolden`, `MotorCorrelationPairsGolden`) |
| `chooser` | `tests/core/motor/chooser/MotorChooserPins.h` | `MotorChooserProbe` (with `ChooserScenarios`) | `gen_motor_chooser_pins.py` | `motor_chooser_golden_tests.cpp` (`MotorChooserPinsGolden`, `MotorChooserCurvesGolden`) |

## What the probes pin

The probes need no Swing class and no injector: they read `data/motors/initial_motors.db` (OpenRocket's
bundled database, byte for byte, which the tests read too) with `ThrustCurveMotorSQLiteDatabase.readDatabase()`
and fill a `ThrustCurveMotorSetDatabase` with `addMotor()` in the order read, as OpenRocket's chooser and
`Test::bundledMotorDatabase()` (`tests/core/motor/TestMotorDatabase.h`) do. Every double is printed exactly
(`Double.toHexString`, and Java 17's `Double.toString` beside it where a person reads it).

- `correlation`: `utils/MotorCorrelation` (Milestone 3, run 1, part C1). First the times at which
  `crossCorrelation()` asks a motor for its thrust (a `java.lang.reflect.Proxy` of `Motor` that records
  them is correlated with itself): how many (100,001), the second and the last, and their sum in the order
  asked, which together pin Java's accumulated `t += 0.01`. Then the database's counts (1591 curves read,
  1458 sets holding 1588), and for each of the 107 sets of more than one curve its manufacturer,
  designation and the digests of its curves in the order of `getMotors()`, and for each of the 322 ordered
  pairs of two different curves `similarity()` and `crossCorrelation()`. The test computes every pair's
  similarity and every unordered pair's cross-correlation and requires them bit for bit (only `+ - * /`
  are involved), and with them the decisions of "Hide very similar thrust curves" (a similarity below 0.95
  keeps the curve: 214 of the 322). The similarities of the two orders of a pair are equal in Java, also
  bit for bit.

- `chooser`: the motor chooser's core logic (`motor/chooser/`, Milestone 3, run 1, part C4). The probe builds
  OpenRocket's real `ThrustCurveMotorSelectionPanel` headless (a `JPanel` works without a display) over
  the bundled database and drives it the way a user would, through its own widgets and members
  (`ChooserScenarios` sits in OpenRocket's package `info.openrocket.swing.gui.dialogs.motor.thrustcurve`
  to reach the package-private ones; private fields are read by reflection): the table's rows after
  each step (the count, the sort keys, the MD5 of every row's model index and six texts, the first ten
  rows) for the filters, 12 search texts, a 28 mm mount with the limit boxes, header clicks, the name
  column, a motor of no set, "Hide motors already used in the mount" for two mounts, and inches; the
  filter's limits; the filter panel's manufacturers and diameter labels; for each of the 107 sets of
  more than one curve the curve selected through the table and the curve list, with "Hide very similar
  thrust curves" on and off; the selections, delay texts and curve-list choices of the scout's probe
  (M3 scouting, motor-chooser) and more; `setMotorMountAndConfig()` for configurations with standard,
  non-standard, NaN and plugged delays and custom motors; the preferred-curve node after OK; and
  `MotorClass` for 22 impulses. Each group of steps starts from cleared preferences and default units
  (the preferences node is cleared, not removed: `FlightConfiguration` keeps a `SwingPreferences` of its
  own), and the rockets it builds have seeded UUIDs (`DeterministicUuids` of `tools/openrocket-gui-pins`,
  compiled with the probe). The test replays every group on a `ThrustCurveMotorSelection` and compares;
  where QtRocket fixes one of OpenRocket's bugs (M3's bugs 8, 9 and 11: the configured delay kept on
  reopening, the used motors of the current mount only, a typed NaN refused) it expects QtRocket's
  answer, derived from OpenRocket's. The custom motor is `tests/data/motors/test1.eng`. The probe needs
  OpenRocket's swing classes, and runs on Linux only, as `tools/openrocket-gui-pins` does: its
  `SwingPreferences` live in the `java.util.prefs` store the script isolates in the work directory,
  which only Linux's implementation reads from there (elsewhere it would clear the user's own
  OpenRocket preferences). It takes about ten seconds.

## Running

Requirements: Linux or macOS (Linux for `chooser`), JDK 17, python3, clang-format (the generators format
the headers with the repository's `.clang-format`; use the version the headers were formatted with, the
one of the `.claude/hooks/format-cpp.sh` hook) and an OpenRocket checkout at the commit of
`tests/data/goldens/manifest.json` (5f164fd0e), which the headers record and `MotorCorrelationPinsGolden`
and `MotorChooserPinsGolden` require.

```sh
OPENROCKET_DIR=/path/to/openrocket OPENROCKET_CLASSPATH=... tools/openrocket-motor-chooser/generate.sh [--check] [correlation] [chooser]
```

`OPENROCKET_CLASSPATH` is the runtime class path of OpenRocket's core module (its compiled classes and
resources and the jars of its dependencies; the swing module's, which `tools/openrocket-gui-pins` uses,
holds it), or of the swing module for the `chooser` family. Without it the script asks Gradle in the
checkout, as `tools/openrocket-goldens/motors/dump-motors.sh` does: `./gradlew :core:classes` (or
`:swing:classes`, incremental) and a task of a temporary init script that prints the class path
(OpenRocket's build files are not changed). Every probe runs with a home directory and a
`java.util.prefs` store of its own in the work directory. The classes must be built from the checkout's HEAD (the
script reads the commit OpenRocket's build stamps into `build.properties`). The head of `generate.sh`
lists the rest of the environment (`JAVA`, `JAVAC`, `CLANG_FORMAT`, `ALLOW_DIRTY`, `ALLOW_OTHER_COMMIT`,
`MOTOR_CHOOSER_PINS_WORK`).

Each table is written into the work directory and compared with the committed one byte for byte: an
equal table leaves the committed file alone, another replaces it (`--check` only compares and exits with
status 1). The probes print the same on every run and on every machine: they use no random number and no
clock (the `chooser` probe's UUIDs are seeded, and its preferences start cleared), and run with a fixed
locale, time zone and encoding. The `correlation` probe takes about five seconds.
