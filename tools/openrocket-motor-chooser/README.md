# What OpenRocket's motor chooser computes: the pin tables of the core's motor tests

`tools/openrocket-motor-chooser` runs small Java probes against OpenRocket's compiled core and turns what
they print into the tables QtRocket's core tests of the motor chooser's logic compare with. The tables
are committed; neither CMake nor CI ever needs Java. They change only through this tool. Each
subdirectory is one family: its probe (`probe/*.java`, package `probe`) and the Python script that writes
the table.

| Family | Table | Probe | Generator | Read by |
|---|---|---|---|---|
| `correlation` | `tests/core/motor/MotorCorrelationPins.h` | `MotorCorrelationProbe` | `gen_motor_correlation_pins.py` | `MotorCorrelationTests.cpp` (`MotorCorrelationPinsGolden`, `MotorCorrelationPairsGolden`) |

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

## Running

Requirements: Linux or macOS, JDK 17, python3, clang-format (the generators format the headers with the
repository's `.clang-format`; use the version the headers were formatted with, the one of the
`.claude/hooks/format-cpp.sh` hook) and an OpenRocket checkout at the commit of
`tests/data/goldens/manifest.json` (5f164fd0e), which the headers record and `MotorCorrelationPinsGolden`
requires.

```sh
OPENROCKET_DIR=/path/to/openrocket OPENROCKET_CLASSPATH=... tools/openrocket-motor-chooser/generate.sh [--check] [correlation]
```

`OPENROCKET_CLASSPATH` is the runtime class path of OpenRocket's core module (its compiled classes and
resources and the jars of its dependencies; the swing module's, which `tools/openrocket-gui-pins` uses,
holds it). Without it the script asks Gradle in the checkout, as `tools/openrocket-goldens/motors/dump-motors.sh`
does: `./gradlew :core:classes` (incremental) and a task of a temporary init script that prints the class
path (OpenRocket's build files are not changed). The classes must be built from the checkout's HEAD (the
script reads the commit OpenRocket's build stamps into `build.properties`). The head of `generate.sh`
lists the rest of the environment (`JAVA`, `JAVAC`, `CLANG_FORMAT`, `ALLOW_DIRTY`, `ALLOW_OTHER_COMMIT`,
`MOTOR_CHOOSER_PINS_WORK`).

Each table is written into the work directory and compared with the committed one byte for byte: an
equal table leaves the committed file alone, another replaces it (`--check` only compares and exits with
status 1). The probes print the same on every run and on every machine: they use no random number, no
UUID, no preference and no clock, and run with a fixed locale, time zone and encoding. The `correlation`
probe takes about five seconds.
