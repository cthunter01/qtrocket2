# What OpenRocket's add buttons make

`tools/openrocket-component-factory` runs OpenRocket (Java) to write `tests/data/component-factory/`:
what OpenRocket makes when the user adds a component through one of the add buttons of its design
window (`ComponentAddButtons`, `ComponentButton.fireActionPerformed()`: the no-argument constructor of
the button's class), for the tests of QtRocket's `rocket/ComponentFactory`. The data is committed;
neither CMake nor CI ever needs Java.

- `generate.sh`: compiles the dumper with swing's `ServicesForTesting` against OpenRocket's compiled
  core and swing classes and runs it.
- `ComponentFactoryDumper.java`: the dumper. In the Guice set-up of swing's `ServicesForTesting` (its
  in-memory `SwingPreferences`, its English translator) and `PluginModule`, it prints the report
  (below) and writes `add-buttons-rocket.ork.xml`.
- `logback.xml`: keeps OpenRocket's log to its warnings and errors.

## Regenerating

With the OpenRocket checkout at the commit of `tests/data/goldens/manifest.json` and Java 17:

```sh
OPENROCKET_DIR=/path/to/openrocket tools/openrocket-component-factory/generate.sh \
    > tests/data/component-factory/report.txt
```

The script writes `add-buttons-rocket.ork.xml` into `tests/data/component-factory/` (or into the
directory given as its argument) and prints the report to standard output. Without
`OPENROCKET_CLASSPATH`, it builds OpenRocket's swing module in the checkout with OpenRocket's Gradle
wrapper and asks Gradle for its class path, as `tools/openrocket-test-documents/generate.sh` does for
the core; with `OPENROCKET_CLASSPATH` set to that class path (the compiled classes and resources of
core and swing and the jars of their dependencies), Gradle does not run. The head of `generate.sh`
lists the environment variables. (The committed files were written with `OPENROCKET_CLASSPATH`: the
classes of a checkout built before, and the jars of Gradle's cache.)

A regeneration rewrites the 27 ids of `add-buttons-rocket.ork.xml` (random UUIDs) and nothing else; the
report is the same.

## The report

1. `== materials`: the materials of a new component of each of the 21 classes of the add buttons
   (`getMaterial`, `getFilletMaterial`, `getLineMaterial` and the private fields `defaultMaterial` and
   `DEFAULT_LINE_MATERIAL`, which no getter reads, as `Material.toStorableString()` writes them) with
   three sets of preferences: none stored; `SwingPreferences.loadDefaultComponentMaterials()`, which
   OpenRocket's start (`SwingStartup`) runs ("Balsa" for `FinSet`, "Polystyrene" for `NoseCone`); and
   seven defaults stored along the class chains (the block `-- stored along the class chains`:
   `ExternalComponent`, `Transition`, `RingComponent`, `CenteringRing`, `RecoveryDevice`, `Parachute`
   with a surface material, which the parachute's lines then pass over, and `MassObject` with a line
   material).
2. `== defaults`: every value Java's constructors give a new component of each class, read through its
   getters (a getter path such as `getDefaultMotorConfig.getIgnitionEvent` is followed step by step),
   after `loadDefaultComponentMaterials()`. Not asked: `getOverrideMass()` and `getOverrideCGX()`, which
   give the component's own mass and CG while nothing is overridden (computed, not set by a
   constructor), and the points of an elliptical fin (cosines and sines).
3. `== clicks`: 25 clicks on the add buttons of a new design (`OpenRocketDocumentFactory.createNewRocket()`;
   both insert preferences 2, "add to the end"; the scenario of the M2 scout's `AddProbe.java`), each with
   its undo text and the parent and position of the new component, then the tree they built. The
   configuration dialog that each click opens last throws `HeadlessException`, after the component was
   added and selected; the dumper ignores it. The design is then saved with `GeneralRocketSaver`, and
   the archive's `rocket.ork` is written, byte for byte, to `add-buttons-rocket.ork.xml`.

Every block of the report starts with a line `-- <title>` or `== <title>`. The tests read the blocks
(`tests/core/rocket/ComponentFactoryReport.h`) and write the same values of QtRocket's components in the
report's notation: `tests/core/rocket/ComponentFactoryTests.cpp` sections 1 and 2, block by block (a
block `-- <class>` per class in section 2), and `tests/core/file/openrocket/add_buttons_save_tests.cpp`
section 3, whose design it then saves and compares with `add-buttons-rocket.ork.xml`. A change of the
dumper's notation therefore needs the same change in those tests.
