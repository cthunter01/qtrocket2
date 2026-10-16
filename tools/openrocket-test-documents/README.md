# OpenRocket's saves of the TestRockets document makers

`tools/openrocket-test-documents` runs OpenRocket (Java) to write `tests/data/goldens/test-documents/`:
what OpenRocket's `OpenRocketSaver` writes, with the simulation data, for the document of each of the
sixteen document makers of `TestRockets` (`makeTestRocket_v100()` ...
`makeTestRocket_for_estimateFileSize()`). QtRocket's golden test `TestDocumentsGolden`
(`tests/core/goldens/test_documents_golden_tests.cpp`) compares its own save of the same makers
(`tests/core/document/TestDocuments.h`) with these files. The data is committed; neither CMake nor CI
ever needs Java.

- `generate.sh`: compiles the dumper with OpenRocket's `ServicesForTesting` against OpenRocket's
  compiled core and runs it.
- `TestDocumentsDumper.java`: the dumper. In the Guice set-up of `OpenRocketSaverTest` (no component
  presets, a motor database of exactly the Estes A8 of `core/src/test/resources/Estes_A8.rse` and the
  `A` `F12X` `digestA`), but with `ServicesForTesting`'s English translator in place of the test's
  `DebugTranslator`, it saves each maker's document to `<maker>.ork` (the maker's name without the
  `makeTestRocket_` prefix) and loads it back; it also saves it with `GeneralRocketSaver` into a
  temporary directory and loads that back.
- `logback.xml`: keeps OpenRocket's log to its warnings and errors.

## Regenerating

With the OpenRocket checkout at the commit of `tests/data/goldens/manifest.json` and Java 17:

```sh
OPENROCKET_DIR=/path/to/openrocket tools/openrocket-test-documents/generate.sh \
    > tests/data/goldens/test-documents/report.txt
```

The script writes the sixteen `.ork` files into `tests/data/goldens/test-documents/` (or into the
directory given as its argument) and prints the report (below) to standard output. Without
`OPENROCKET_CLASSPATH`, it builds OpenRocket's core in the checkout with OpenRocket's Gradle wrapper
and asks Gradle for its class path, as `tools/openrocket-goldens/motors/dump-motors.sh` does; with
`OPENROCKET_CLASSPATH` set to that class path (the compiled classes and resources of core and the jars
of its dependencies), Gradle does not run. The head of `generate.sh` lists the environment variables.

The makers draw random ids (components, a flight configuration, flight events), and the flight data
holds the wall-clock time of each step (`computation_time`). A regeneration therefore rewrites those
ids and times in every file, and nothing else: the golden test numbers the random ids on both sides
and sets the computation times aside.

## The report

Per maker, in the order of `OpenRocketSaverTest.testCreateLoadSave`: the estimate of
`OpenRocketSaver.estimateFileSize()`, the presets and the statuses of the made document; then the
reload of the plain file (its size, components, simulations and statuses, and its warnings as `W`
lines) and of the `GeneralRocketSaver` archive (its entries, components, simulations and statuses,
and its warnings as `ZW` lines). Three tables of `tests/core/file/openrocket/OpenRocketSaverTests.cpp`
are copied from it by hand: `kJavaEstimates` (the estimates), `kJavaReloadWarnings` (the `ZW`
lines) and `kJavaReloadedDocuments` (the archive reloads). The report of the committed files is
committed beside them as `tests/data/goldens/test-documents/report.txt` (OpenRocket 5f164fd0e, JDK
17.0.20.1, Linux); a regeneration gives the same report, but for the sizes of the plain files, which
change when a computation time is written with another number of digits.
