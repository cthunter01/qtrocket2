# Probes whose output tests pin by hand

`tools/openrocket-probes` keeps Java probes that ran against OpenRocket's compiled classes and whose output
QtRocket's tests pin by hand: the values are written into the tests (with the probe named in a comment),
not generated into a table. Each subdirectory is one probe: its source (`probe/*Probe.java`, package `probe`)
and `expected.txt`, what it printed when the tests were written. Neither CMake nor CI ever runs them.

| Probe | What it prints | Pinned by |
|---|---|---|
| `design-information` (`X1Probe`, Milestone 2 run 4, part X1) | the undo and redo of the rocket's designer, revision, kit name and design type (`Rocket.loadFrom()`), and the copy of a stage's separation on its first access (`AxialStage.getSeparationConfiguration()`) | `tests/core/document/OpenRocketDocumentTests.cpp`, `tests/core/rocket/RocketTests.cpp`, `tests/core/rocket/AxialStageTests.cpp` |

The tables generated from probes have their own tools (`tools/openrocket-gui-pins` and the others its README
lists); the probes of the GUI tables also check the values tests pin by hand beside them.

## Running

Requirements: Linux, JDK 17 and an OpenRocket checkout at the commit of `tests/data/goldens/manifest.json`
(5f164fd0e), as for `tools/openrocket-gui-pins` (its README says how to get the class path).

```sh
OPENROCKET_DIR=/path/to/openrocket OPENROCKET_CLASSPATH=... tools/openrocket-probes/run.sh design-information
```

The script compiles the probe with OpenRocket's `core/src/test/.../ServicesForTesting.java`, runs it with a
fixed locale, time zone and encoding, a fresh `java.util.prefs` store and home directory, OpenRocket's log on
standard error, and compares what it prints with `expected.txt`. A difference fails the run: review the tests
that pin the changed lines, then copy the new transcript (in the work directory, `PROBES_WORK`) over
`expected.txt`.
