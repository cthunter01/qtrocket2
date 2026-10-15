# OpenRocket's saves of the TestRockets document makers

What OpenRocket's `OpenRocketSaver` writes, as plain XML, for the document of each of the sixteen
document makers of `TestRockets` (`core/src/main/java/info/openrocket/core/util/TestRockets.java`:
`makeTestRocket_v100()` ... `makeTestRocket_for_estimateFileSize()`), saved with the simulation
data, as `OpenRocketSaverTest.testCreateLoadSave` saves them. Each file is named after its maker
without the `makeTestRocket_` prefix; `v110_withSimulationExtension.ork` is the maker called with
`OpenRocketSaverTest.SIMULATION_EXTENSION_SCRIPT`. They are the reference of QtRocket's golden test
`TestDocumentsGolden` (`tests/core/document/test_documents_tests.cpp`), which saves the document of
the same maker of `tests/core/document/TestDocuments.h` and compares the two texts line for line.

The makers draw random ids (components, a flight configuration, flight events) and the flight data
holds the wall-clock time of each step (`computation_time`), so the test numbers the random ids on
both sides and sets the computation times aside; the creator attribute is QtRocket's own.

They were written by the probe `TestDocumentsProbe.java` of run 10b (part J1) with OpenRocket
5f164fd0e (the commit of `tests/data/goldens/manifest.json`) on JDK 17.0.20.1, Linux, in the Guice
set-up of `OpenRocketSaverTest`: `ServicesForTesting` (OpenRocket's test preferences: a time step
of 0, no wind, a launch rod of length 0) with an empty component preset database and a motor
database of exactly the Estes A8 of `core/src/test/resources/Estes_A8.rse` and the `A` `F12X`
`digestA`. One thing differs from that test: the translator is `ServicesForTesting`'s English one
(`l10n.messages`), not `OpenRocketSaverTest`'s `DebugTranslator`, so that the default names of the
components are the English ones ("Body Tube") where the test would write "[BodyTube.BodyTube.BodyTube]".
