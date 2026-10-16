# OpenRocket's saves of the TestRockets document makers

What OpenRocket's `OpenRocketSaver` writes, as plain XML, for the document of each of the sixteen
document makers of `TestRockets` (`core/src/main/java/info/openrocket/core/util/TestRockets.java`:
`makeTestRocket_v100()` ... `makeTestRocket_for_estimateFileSize()`), saved with the simulation
data, as `OpenRocketSaverTest.testCreateLoadSave` saves them. Each file is named after its maker
without the `makeTestRocket_` prefix; `v110_withSimulationExtension.ork` is the maker called with
`OpenRocketSaverTest.SIMULATION_EXTENSION_SCRIPT`. They are the reference of QtRocket's golden test
`TestDocumentsGolden` (`tests/core/goldens/test_documents_golden_tests.cpp`), which saves the
document of the same maker of `tests/core/document/TestDocuments.h` and compares the two texts line
for line.

The makers draw random ids (components, a flight configuration, flight events) and the flight data
holds the wall-clock time of each step (`computation_time`), so the test numbers the random ids on
both sides and sets the computation times aside; the creator attribute is QtRocket's own.

They are written by `tools/openrocket-test-documents/generate.sh` (the dumper
`TestDocumentsDumper.java` there; its README says how to run it), with OpenRocket 5f164fd0e (the
commit of `tests/data/goldens/manifest.json`) on JDK 17.0.20.1, Linux, in the Guice set-up of
`OpenRocketSaverTest`: `ServicesForTesting` (OpenRocket's test preferences: a time step of 0, no
wind, a launch rod of length 0) with an empty component preset database and a motor database of
exactly the Estes A8 of `core/src/test/resources/Estes_A8.rse` and the `A` `F12X` `digestA`. One
thing differs from that test: the translator is `ServicesForTesting`'s English one
(`l10n.messages`), not `OpenRocketSaverTest`'s `DebugTranslator`, so that the default names of the
components are the English ones ("Body Tube") where the test would write
"[BodyTube.BodyTube.BodyTube]". Edit these files through the generator only.

`report.txt` is what the generator printed when it wrote them: per maker, OpenRocket's estimate of
the file size, and what OpenRocket's reload of the plain file (`W` lines: its warnings) and of its
`GeneralRocketSaver` archive (`ZW` lines) gives. The tables `kJavaEstimates`,
`kJavaReloadWarnings` and `kJavaReloadedDocuments` of
`tests/core/file/openrocket/OpenRocketSaverTests.cpp` are copied from it.
