# OpenRocket golden data harness

`tools/openrocket-goldens` runs OpenRocket (Java) on a fixed set of rocket designs and writes what it
computes into `tests/data/goldens/`: geometry and mass properties, extended-Barrowman aerodynamics,
complete simulations, OpenRocket's own re-save of each design and what its 2D rocket figure draws,
and the component presets it reads from its bundled `.orc` files.
QtRocket's golden tests (`tests/core/goldens/*_golden_tests.cpp`, plan section 6.4) compare the C++
port against these files. The data is committed; neither CMake nor CI ever needs Java.

- `generate.sh`: builds the dumper and regenerates `tests/data/goldens/` (see [Regenerating](#regenerating)).
- `build.gradle`, `settings.gradle`: a stand-alone Gradle project that compiles OpenRocket's `core`
  and the shape providers of its Swing module's 2D rocket figure (the package
  `info.openrocket.swing.gui.rocketfigure`) from a local checkout (read only) together with the
  dumper.
- `src/main/java/info/qtrocket/goldens/`: `GoldenDumper` (entry point, OpenRocket bootstrap, inputs,
  manifest) and its helpers `GeometryDumper`, `MassDumper`, `AeroDumper`, `FigureDumper`,
  `SimulationDumper`, `JitterRemoval`, `AutomaticDimensions`, `DeterministicUuids`,
  `LastBitPerturbation`, `ApplicationDefaultsPreferences`, `ComponentIndex`, `Values`, `Json`, and
  `CoverageDocuments`
  (the saver-coverage documents, see [Re-saves alone](#re-saves-alone)), and `PresetDumper`, which
  writes the preset golden `presets/presets.json` in a full run and runs alone too
  (`generate.sh --presets`, see [presets/presets.json](#presetspresetsjson)).
- `src/main/java/info/qtrocket/goldens/CsvExportDumper.java` is not run by `generate.sh`: it writes
  the fixtures of the CSV export test, `tests/data/csv/` (see
  [CSV export fixtures](#csv-export-fixtures)).
- `interop.sh` and `src/main/java/info/qtrocket/goldens/InteropCheck.java`: the interop check of
  plan section 7, run by hand: OpenRocket reads the design files QtRocket saved (see
  [Interop check](#interop-check)). It writes nothing into `tests/data`.
- `figure-pins/` (`generate.sh`, the probes `src/probe/BodyShapesProbe.java` and
  `FinMassShapesProbe.java`, the generators `gen_body_shape_pins.py` and `gen_fin_mass_shape_pins.py`)
  is not run by `generate.sh` either: it writes the pin tables of the figure's shape tests,
  `tests/core/figure/BodyShapePins.h`, `FinMassShapePins.h` and `FigureGeometryPins.h` (see
  [Figure pin tables](#figure-pin-tables)).
- `motors/` (`MotorsDumper.java`, `dump-motors.sh`) belongs to the motor port: it writes
  `tests/data/goldens/motors.json` (digests and derived values of every curve of
  `initial_motors.db` and the test `.eng`/`.rse` files, plus the curves OpenRocket skips). This
  harness never writes or deletes `motors.json` and does not list it in `manifest.json`.

The C++ side reads the files through `tests/core/goldens/GoldenData.h` (nlohmann_json for the JSON,
`file/GzipStream` for the time series); `goldens_schema_tests.cpp` checks that every listed file parses
and has the keys described below. A test rocket is rebuilt in C++ by its maker
(`tests/core/rocket/TestRockets.h`) and, once more, loaded from its re-save; an example design is
loaded by the `.ork` loader from the file of `data/examples` and from its re-save
(`tests/core/goldens/GoldenExamples.h`), in the harness's environment: the bundled motor database,
the component presets the examples name, OpenRocket's test preferences.

## Inputs

| Kind | Inputs | Directory |
|---|---|---|
| Example designs | the 16 files of `data/examples/` (identical to OpenRocket's `core/src/main/resources/datafiles/examples/`) | `example-<file name, lower case, runs of other characters as '-'>` |
| Test rockets | the 13 `info.openrocket.core.util.TestRockets` factories `makeEstesAlphaIII`, `makeBeta`, `makeSimple2Stage`, `makeBigBlue`, `makeIsoHaisu`, `makeFalcon9Heavy`, `makeMultiStageEventTestRocket`, `makeEndPlateRocket`, `makeEstesAlphaIIIWithPods`, `makeEstesAlphaIIIWithMotorPods`, `makeEstesAlphaIIIWithSecondMotor`, `makeEstesAlphaIIIwithInlinePod`, `makeClusterPods` | `testrocket-estes-alpha-iii`, `testrocket-beta`, ..., `testrocket-cluster-pods` (see `GoldenDumper.TEST_ROCKETS`) |
| Legacy files (re-save alone) | the 18 design files of `tests/data/ork` (OpenRocket 0.9 to 23.09, file formats 1.0 to 1.9; `tests/data/ork/README.md`) | `legacy-<file name, as for the examples>`: `legacy-simplerocket`, `legacy-v1-0-roll-stabilized`, ..., `legacy-v1-9-chute-release` |
| Saver-coverage documents (re-save alone) | the documents `CoverageDocuments` builds: `kitchenSink()`, `storedFlight()` | `coverage-kitchen-sink`, `coverage-stored-flight` |

Each input directory of an example or a test rocket holds (the directory of a re-save alone holds
its `resave/rocket.ork` and nothing else; see [Re-saves alone](#re-saves-alone)):

| File | Content |
|---|---|
| `geometry.json` | per component: geometry, mass properties, instances; per flight configuration: active stages, motors, reference values, bounds, the transform of every active instance |
| `mass.json` | per flight configuration: the STRUCTURE, LAUNCH, BURNOUT and MOTOR rigid bodies of `MassCalculator` and its CM analysis |
| `aero.json` | per flight configuration: CP/CNα, total forces and the per-component force analysis of `BarrowmanCalculator` at 25 flight conditions, worst CP per Mach number, geometry warnings |
| `figure.json` | what OpenRocket's 2D rocket figure (`RocketFigure`) draws of the selected flight configuration in five views: the subject bounds, the shapes of every instance with their display order, and the motors |
| `sim_<NN>_<name>.json` | one simulation: options, what the harness changed, extensions, result, summary values, warnings, and per branch the columns, events and extremes |
| `sim_<NN>_<name>_branch<i>.csv.gz` | the full time series of branch `i` of that simulation |
| `stable/sim_<NN>_<name>.json`, `stable/sim_<NN>_<name>_branch<i>.csv.gz` | the stable-step set: the same simulation run with a time step of 0.01 s and nothing else changed, in the same format. The test rockets have both files; the example designs have the documents alone, without the time series (see [How OpenRocket is run](#how-openrocket-is-run)) |
| `resave/rocket.ork` | `OpenRocketSaver`'s XML for the design as loaded (as built, for the test rockets), without simulation data |
| `resave-data/rocket.ork.gz` | two examples only ("Deployable payload", "Pods--powered with recovery deployment"): `OpenRocketSaver`'s XML for the design as loaded WITH its simulation data, gzip-compressed (see [Re-saves alone](#re-saves-alone)) |

`<NN>` is the simulation's index in the document (two digits) and `<name>` its name made
file-system safe the same way as the directory names.

## How OpenRocket is run

- **Bootstrap** exactly as OpenRocket's tests do: `BaseTestCase.setUp()` (Guice with
  `ServicesForTesting` overridden by `PluginModule`), then, as `ExampleFilesTest.setUp()` does for the
  example files, a second injector that also binds the component preset database and the thrust
  curve database (`initial_motors.db`, read with `ThrustCurveMotorSQLiteDatabase` from a private
  copy in `build/work`). The presets are the `.orc` files `ExampleFilesTest` loads (everything under
  `core/src/main/resources/datafiles/components`, sorted by path), read from tracked sources only:
  `internal/` from that directory and `database/` from the `openrocket-database` submodule's `orc/`
  directory (walked recursively, as OpenRocket's `externalComponentsCopy` does). The `database/`
  copy that OpenRocket's own build leaves in `core/src/main/resources` is git-ignored and may come
  from another submodule commit (the copy task can run `git submodule update --remote`), so it is
  never read; `manifest.json` records the SHA-256 of every preset file that was loaded.
  `ServicesForTesting.java` is compiled from OpenRocket's test sources, not copied. Its
  `PreferencesForTesting` answer 0, `false` or `null` to most queries; that is the environment
  OpenRocket's own unit tests (and their pinned values) use.
- **Order**, per input: re-save; settle the automatic dimensions (below); `geometry.json`;
  `mass.json`; `aero.json`; `figure.json`; the simulations. Per-configuration values are computed
  with that configuration selected (`Rocket.setSelectedConfiguration`, as the simulation engine does
  for its private copy), and the original selection is restored afterwards. Component-level values of
  `geometry.json` are taken with the document's own selected configuration.
- **Settled automatic dimensions** (`AutomaticDimensions`): OpenRocket computes automatic
  dimensions lazily and stores them in the component. `BodyTube.getOuterRadius()` (auto radius) and
  `MassObject.getRadius()` (auto radius, which also rescales the packed length) refresh the stored
  value; other getters read the stored value as it is (`MassObject.getLength()` divides the volume by
  the stored radius, and a body tube's auto radius depends on the neighbour it last took its radius
  from). Right after loading, the first calculations therefore mix stale and refreshed values, and
  what a dump sees would depend on which getters ran before it. The harness calls the refreshing
  getters of every component in tree order until a whole pass changes nothing, and every golden
  except the re-save describes that settled state. Consequences:
  - `resave/rocket.ork` is written before the settling, so it is OpenRocket's save of the design
    as loaded and can hold stale automatic values. In "Dual parachute deployment" the first body
    tube is saved as `auto 0.025` (settled: 0.028321) and the parachute's `packedlength` as
    0.11285771168170697 (settled: `geometry.json` length 0.08709673571783902).
  - `ExampleFilesTest` pins the first mass calculation after loading, a pre-settling value: its
    structure CG x for "Dual parachute deployment" is 0.885775814, while `mass.json` (settled) has
    0.8784464812860964. A port of that JUnit test and the golden comparison cannot both use the
    same state; the golden is the settled one.
- **Simulations of the example files**: the document's own simulations, with their own options,
  except that the harness (`SimulationDumper.makeReproducible`) fixes the random seed to 0 and sets
  the standard deviation of the average wind model and of every level of the multi-level wind model
  to 0 ("calm": the average speed and direction are kept, so the wind is constant). A simulation with
  an *enabled* JavaScript `ScriptingExtension` is not run (scripting is out of scope for QtRocket):
  its JSON has `"skipped": true` and a `skipReason`, and no CSV. A *disabled* scripting extension
  (`config.enabled` false, as "Simulation scripting.ork" stores all of its scripts; OpenRocket also
  disables untrusted scripts on load) is a no-op, since `ScriptingExtension.initialize` adds no
  listener, and the simulation runs like a plain one, in OpenRocket and here. Java extensions (`RollControl`,
  `AirStart`, ...) run as in OpenRocket.
- **Simulations of the test rockets**: one per flight configuration, the default configuration
  included, named after the configuration and added to the document before the re-save. Their
  options are those a fresh OpenRocket installation gives a new simulation
  (`DefaultSimulationOptionFactory` over `ApplicationDefaultsPreferences`, which answers every
  preference with `ApplicationPreferences`' default: rod 1 m into the wind, 2 m/s wind from π/2,
  28.61°N 80.60°W, ISA, WGS gravity, spherical geodetics, time step 0.05 s, RK4, ...; and the
  multi-level wind model's initial level, which `MultiLevelPinkNoiseWindModel` takes from the
  preferences loaded with the class, i.e. the test preferences, is set to what an installation gives:
  one level at 0 m with 2 m/s from π/2 and the average wind's standard deviation), then made calm
  and seeded as above.
- **Variant simulations**: three more simulations of `testrocket-estes-alpha-iii`, appended after
  the per-configuration ones, cover code paths that no other golden simulation uses: the `[C6-5]`
  configuration with the default options except the RK6 stepper (`sim_06_c6-5-rk6-stepper`),
  WGS84 geodetics (`sim_07_c6-5-wgs84-geodetics`), and the multi-level wind model with three calm
  levels (`sim_08_c6-5-multi-level-wind`: 2 m/s from π/2 at 0 m, 4 m/s from 2.2 rad at 100 m,
  6 m/s from 3 rad at 200 m, altitudes above mean sea level). Their JSON says what was changed in
  `variant` (null for every other simulation). Not covered by any golden simulation: the
  nozzle-exit thrust and base-drag corrections (no design sets a nozzle exit diameter; see the
  thrusting-nozzle point of `aero.json` for the base drag).
- **Stable-step set**: every simulation is run a second time with the time step set to
  `SimulationDumper.STABLE_TIME_STEP` = 0.01 s and nothing else changed, and written to
  `<input>/stable/` under the same names and in the same format. It exists because the first set
  is run at the installation's time step of 0.05 s, at which most of the flights are not
  reproducible beyond their first tenths of a second (see
  [Reproducibility of the simulations](#reproducibility-of-the-simulations)); at 0.01 s the whole
  flights can be compared. How it is produced:
  - In a pass of its own (`GoldenDumper.Pass.STABLE`), after every file of the input has been
    written: the UUID sequence is reseeded from the input's name, the rocket is built and the
    document set up again (an example design is loaded again), and the default pass is repeated
    call by call: the re-save, the settling of the automatic dimensions, the geometry, mass,
    aerodynamic and figure dumps, then the simulations in the same order, each made reproducible as
    before.
    So a stable simulation meets the same components with the same ids in the same state as its
    default-step simulation, and its `flightConfiguration.id` is the one of `geometry.json`.
  - The stable pass writes only the simulations. Everything else it would write it compares with
    what the default pass wrote, byte for byte (`GoldenDumper.writeOrVerify`), and fails when a
    file differs: that is what makes it a repetition of the default pass.
  - The time step is set at the last moment before the run (`SimulationDumper.useStableTimeStep`);
    the simulation's `harness` records the stable time step (`timeStep`) and the one the
    simulation had (`documentTimeStep`), and `options.timeStep` shows the step used.
  - Nothing the default pass writes depends on the stable pass: with the stable-step set added,
    every other file of `tests/data/goldens` but `manifest.json`, which lists the new files, is
    byte-identical to before.
  - The stable-step set of the 13 test rockets has the time series of its branches. That of the
    16 example designs is written as **documents alone**: `stable/sim_<NN>_<name>.json` with the
    options, the result, the summary values, the warnings and, per branch, the number of rows, the
    columns with their minima and maxima, the events, the optimum altitude and the separation
    time; `csv` of such a branch is null, the manifest lists no branch file for the simulation,
    and no `.csv.gz` is written (54 documents, 0.9 MB; the time series of their 69 branches and
    92286 rows would add 36.6 MB). `manifest.json` says which kinds of inputs have a stable-step
    set (`settings.stableSimulationsOf`) and which of those have its time series
    (`settings.stableTimeSeriesOf`). The stable pass over an example is the same either way
    (`--stable-examples documents` of `GoldenDumper`, the default): only the files of the time
    series are left out, so a document is, but for its `csv` entries, the one a dump with time
    series holds. `STABLE_EXAMPLES=full` writes the time series too and `STABLE_EXAMPLES=none`
    no stable-step set of the examples at all, both into a scratch directory only (see
    [Regenerating](#regenerating)). Every simulation of the examples is given the 0.01 s; their
    own time steps are 0.05 s (49 simulations) and 0.04 s (the 5 of "Pods--airframes and
    winglets"), so none is coarsened by it. What to do with a simulation whose own step is
    already 0.01 s or smaller is not decided, since no document has one.
- **Pitch/yaw jitter removed**: `AbstractRKSimulationStepper.calculateForces` adds up to ±0.0005 of
  `java.util.Random` noise to Cm and Cyaw after every aerodynamic calculation. The harness runs
  `Simulation.simulate()`'s steps itself with two extra system listeners (`JitterRemoval`): the first
  listener answers the post-aerodynamic-calculation hook fired from that method with the
  calculator's jitter-free result for the same flight conditions (captured by the last listener).
  The landing and tumble steppers, which have no jitter, are left alone. The nested optimum-coast
  simulation keeps both listeners. `result.jitterReplacements` counts the replaced calculations. A
  failure of these listeners aborts the run (it is never recorded as an OpenRocket exception, and is
  also caught when it happens inside the nested optimum-coast simulation, whose exceptions OpenRocket
  only logs).
- **Determinism**: `DeterministicUuids` installs a seeded `SecureRandom` provider before OpenRocket
  starts, so `UUID.randomUUID()` (new components, flight configurations, simulations, events) gives
  the same sequence on every run; the generator is reseeded from the input name (and the salt of
  `UUID_SALT`, see [Regenerating](#regenerating)) before each pass over an input, the default
  pass and the stable pass.
  This matters for the test rockets: OpenRocket's hash maps are keyed by those UUIDs, so summation
  orders (and the last bits of sums) would otherwise change from run to run. (A design loaded
  from a file keeps the ids stored in it; to see how reproducible OpenRocket's results are for
  those, a run can be perturbed in the last bit instead: `LAST_BIT`, see
  [Regenerating](#regenerating).) The CSV column
  `computation_time` (wall-clock time) is left out (branch `excludedColumns`). Nothing written holds
  a timestamp or an absolute path, and running `generate.sh` twice on the same machine produces
  byte-identical files (see [Regenerating](#regenerating) for other machines). Because the UUID
  sequence is the same in every process, sqlite-jdbc's native library (extracted under a UUID-based
  name) goes into a per-process directory under `build/work` (`process-<pid>`, deleted when the
  dumper exits), and so does the dumper's private copy of the motor database: concurrent runs
  that write into different directories (`GOLDENS_OUT`) share no file and cannot collide.

## File formats

Conventions for every JSON file:

- Every file starts with `"schema"` (`"manifest"`, `"geometry"`, `"mass"`, `"aero"`,
  `"figure"`, `"simulation"`, `"presets"`), `"schemaVersion"` (currently 1,
  `GoldenDumper.SCHEMA_VERSION` and `kGoldenSchemaVersion` in `GoldenData.h`) and, except the
  manifest and `presets/presets.json`, which belong to no input, `"input"` (the input's name).
  The version is raised when a field changes its meaning or disappears, not for an addition: the
  stable-step set added files, manifest entries and two keys of `harness` in its own files, and
  left it at 1. (This rule was written down with that addition; before it the README only gave
  the number.) So did the stable-step set of the examples, which added files, the manifest's
  `settings.stableTimeSeriesOf` and, in its own files only, a null as the `csv` of a branch
  (every file there was before is unchanged, and a branch of those still names its time series).
  The loader of a revision can still require what that revision's tests read:
  `parseGoldenManifest()` fails on a manifest without `settings.stableTimeStep`,
  `settings.stableSimulationsOf` and `settings.stableTimeSeriesOf`, so the tests of this revision
  do not read the data of an earlier one.
- Numbers are written with Java's `Double.toString`, which reads back as exactly the same double.
  Values JSON cannot represent are the strings `"NaN"`, `"Infinity"` and `"-Infinity"`
  (`goldenNumber()` in `GoldenData.h` reads both forms).
- Units are SI (m, kg, s, rad, kg·m², Pa, K, N) unless stated otherwise. Dimensionless coefficients
  use the configuration's reference area and length. The exceptions are latitudes and longitudes,
  which are in degrees as OpenRocket stores them: `options.launchLatitude` and
  `options.launchLongitude` of the simulation JSON, and the latitude and longitude CSV columns.
- Frames: the rocket (body) frame has its origin at the nose tip, x pointing aft along the axis and
  y, z radial; OpenRocket's rotations follow the left-hand rule. A position is `[x, y, z]`; a weighted
  coordinate is `[x, y, z, w]` with w the mass for a CG and CNα for a CP. A transformation is
  `{"rotation": R (9 numbers, row-major), "translation": t}` mapping p to R·p + t.
- Components are identified by their **path**: the child indices from the rocket, `"/"` for the
  rocket, `"/0"` for its first stage, `"/0/1"` for that stage's second child. Paths are portable
  (the C++ port rebuilds the test rockets with different UUIDs); the `id` UUID is given as well,
  and is compared for a design that was loaded from a file, which has the ids of the file.
- Flight configurations are identified by `index` (their order in
  `Rocket.getFlightConfigurations()`, the default configuration first) and `id`; each entry also
  has `isDefault` and `name` (with `{motors}` substituted, e.g. `"[A8-3]"`).
- A warning is `{"type": class name, "priority", "description", "text", "sources": [paths],
  "parameter"}` (`parameter`: the speed or angle of the parameterised warnings only).

### manifest.json

`openrocket` (`commit`: the OpenRocket commit the classes were compiled from, `version`: its build
version as written into `creator=` of the re-saved files, `presetDatabaseCommit`: the
`openrocket-database` commit the superproject records, `dirty`: true only when `ALLOW_DIRTY=1`
let a checkout with local changes through, `javaVersion`), `databases` (motor database path,
SHA-256 and motor count; the preset files, the SHA-256 of each (`presetFileSha256`) and the preset
count),
`settings` (Mach numbers, angles of attack, seed, wind deviation, jitter removal;
`stableTimeStep`, the time step of the stable-step set; `stableSimulationsOf`, the kinds of the
inputs that have one: `["example", "testrocket"]` (`["testrocket"]` with `STABLE_EXAMPLES=none`);
`stableTimeSeriesOf`, the kinds of those whose stable-step set has the time series of its branches:
`["testrocket"]` (`["example", "testrocket"]` with `STABLE_EXAMPLES=full`), the set of the other
kinds being the documents alone; and, in a dump that is never the committed data, `uuidSalt` (made
with `UUID_SALT`) or `lastBitPerturbation` (made with `LAST_BIT`: the pattern):
`GoldenSchema.ManifestDescribesItsSource` fails on a manifest that has either),
`tolerances` (below) and `inputs`: per input its `name`, `kind` (`example` or `testrocket`),
`source` (the `data/examples/` file, or the TestRockets method), `sourceSha256` (example files;
`GoldenSchema.ManifestListsEveryInput` holds the files of `data/examples/` against it),
the paths of `geometry`, `mass`, `aero`, `resave` and `figure`, `simulations` (`name`, `json`,
`branches`: the CSV files) and, for an input with a stable-step set, `stableSimulations`: the same
simulations in the same order with their files in `<input>/stable/` (entries of the same shape,
with an empty `branches` in a set without time series; an input without the set has no such key).
All paths are relative to `tests/data/goldens/`. Last, `resaves`, which the re-saves alone add
(see [Re-saves alone](#re-saves-alone)): `legacyPresetDigests` (the digests of the six presets of the
environment of the legacy files, sorted), `withData` (`input` and `resave`, the file, of each
re-save with simulation data of an example) and `inputs`: per re-save alone its `name`, `kind`
(`legacy` or `coverage`), `source` (the `tests/data/ork` file, or the `CoverageDocuments` method),
`sourceSha256` (legacy files; null for a coverage document) and `resave`; a legacy file has
`documentEntryMovedFirst` (whether the archive was packed again with its document first),
`archiveEntries` (the entries of OpenRocket's `.ork` archive of the design as loaded, saved with the
simulation data, in their order) and `archiveError` (when that archive cannot be saved:
`DecalNotFoundException: <the decal's name>`; then `archiveEntries` is null), a coverage document
`saveSimulationData` (how its re-save was saved). The last entry, `presets`, is the preset
golden's (see [presets/presets.json](#presetspresetsjson)): `json` (`presets/presets.json`), its
`sha256`, `fileCount` (31), `presetCount` (5231) and `databaseCount` (5228); the full run writes
it, and `generate.sh --presets` replaces its line.

### presets/presets.json

What OpenRocket reads from the bundled component preset files (`PresetDumper`): the 31 `.orc` files
of the harness's preset database (`GoldenDumper.presetFiles()`: `internal/` from OpenRocket's
resources, `database/` from the `openrocket-database` submodule; QtRocket ships them, byte for byte,
in `data/components/`), each read with `OpenRocketComponentLoader` and added to a
`ComponentPresetDatabase` with `addAll()`, in the order in which QtRocket's
`ComponentPresetDatabaseLoader` reads the bundled directory: depth first, the entries of each
directory in the order of their UTF-8 names compared byte by byte (`DirectoryIterator`; for these
files the order of the sorted paths that `databases.presetFiles` of the manifest lists).

`fileCount`, `presetCount` (5231) and `files`, per file: `file` (its path below the components
directory, `database/BMS.ORC`), `sha256`, `presetCount` and `presets`, each with `type` (the
`ComponentPreset.Type` constant), `manufacturer` (the simple name), `manufacturerDisplayName`,
`partNo`, `digest` and `properties`: every property in the order the loader put it into the
preset's `TypedPropertyMap` (read through reflection: the preset has no accessor for it), with its
value in SI units: a double or an integer as such, a boolean, a string, a manufacturer by its simple
name, a preset type, a shape and a finish by the constant's name, a material as `{"type" (the
constant), "name", "density", "shearModulus", "group" (its database string), "userDefined",
"documentMaterial"}` and an image in base64. Then `databaseCount` (5228: three presets of
`semroc.orc` have the digests of others) and `databaseOrder`, the database's presets in its order,
each as `"<file>#<index in the file>"`. The UUID sequence is reseeded before the files are read
(`DeterministicUuids`), although reading presets makes none.

`tests/core/goldens/preset_golden_tests.cpp` (`GoldenPresets.h`) reads every bundled file with
QtRocket's loader and compares, file by file, every preset exactly (doubles bit for bit), and the
order of the database. The exception: six nose cones of `internal/semroc-legacy.orc` whose
material's density OpenRocket's factory derives from the volume of a HAACK or a POWER shape (acos,
sin, pow: the last bits differ between platforms), compared within a relative 1e-10 and without
their digests.

### geometry.json

- `rocketName`, `selectedConfiguration` (id), `loadWarnings` (the loader's warnings).
- `components`, depth-first with the rocket first. Each has `path`, `type` (OpenRocket class name),
  `name`, `id`, `stageNumber` (-1 for the rocket), `length`, `axialMethod`, `axialOffset`,
  `position` (relative to the parent), `isAerodynamic`, `isMassive`; the mass properties
  `componentMass`, `componentCG` (component frame, without overrides), `longitudinalUnitInertia`
  and `rotationalUnitInertia` (per kg, about the non-overridden CG), `mass`, `sectionMass`, `cg`,
  `longitudinalInertia`, `rotationalInertia` (overrides applied); `overrides` (mass, CG and CD
  override flags `massOverridden`, `cgOverridden`, `cdOverridden`; the values `overrideMass`,
  `overrideCGX`, `overrideCD`, null unless the override is set; subcomponent flags,
  `cdOverriddenByAncestor`, `massOverriddenBy`/`cgOverriddenBy` paths). Without the override,
  OpenRocket's `getOverrideCD()` returns no stored value but the component's CD from a force
  analysis at the default Mach number, which is 0 under the test preferences and 0.3 in an
  installed OpenRocket, so it is not dumped; `getOverrideMass()`/`getOverrideCGX()` would only
  repeat `componentMass` and `componentCG`;
  `componentBounds` (the points whose convex hull encloses the component, component frame);
  `instanceCount`, `instanceOffsets`, `instanceAngles`, `instanceLocations` (relative to the
  parent), `componentLocations` and `componentAngles` (every instance, parents' instancing
  included, rocket frame). `details` holds whichever of a fixed list of further getters the
  component's class has (`GeometryDumper.OPTIONAL_GETTERS`; key = getter name without `get`/`is`,
  first letter lower-cased, or all lower-case for an all-capitals name: `getCD` gives `cd`):
  volumes and areas (`componentVolume`, `fullVolume`, `componentWetArea`,
  `componentPlanformArea`, `planformArea`), radii and thickness, transition shape and shoulders,
  fin parameters and points (`finPoints`, `rootPoints`, `tabPoints`), `material`
  (`{name, type, density}`), finish, instance placement, `instanceBoundingBox`, motor mount and
  recovery device parameters, and for the rocket its reference type. Enums are written by name;
  other objects by their `toString()`.
- `configurations`: header, `stageCount`, `activeStages` (stage numbers), `referenceLength`,
  `referenceArea`, `length`, `lengthAerodynamic`, `boundingBox` and `boundingBoxAerodynamic`
  (`{min, max}`), `hasMotors`, `hasRecoveryDevice`, `motors` (per active motor, by mount path:
  `mount`, `motorName`, `designation`, `manufacturer`, `digest`, `ejectionDelay`,
  `nozzleExitDiameter` (0: unknown, which disables the nozzle corrections; every input has 0),
  `ignitionEvent`, `ignitionDelay`, `motorCount`, `motorCountIncludingAssemblyCopies`,
  `motorOverhang`, `position`),
  `activeComponents` (paths) and `instances`: per active component (tree order) its
  `InstanceContext`s: `instanceNumber`, `location`, `transform` and `parentTransform`
  (component → rocket frame).

### mass.json

`configurations`: header, then `structure`, `launch`, `burnout` and `motor` rigid bodies
(`MassCalculator.calculateStructure/Launch/Burnout/Motor`): `mass`, `cm` `[x, y, z, mass]`
(rocket frame), `ixx` (about the x axis), `iyy`, `izz`, `longitudinalInertia`, `rotationalInertia`
(about the CM). `cmAnalysis` (`MassCalculator.getCMAnalysis`): rows `{kind, path, name, eachMass,
totalCM}` for the components (tree order), the motors (`kind: "motor"`, by designation) and last the
`total` (the rocket row).

### aero.json

`atmosphere` (the default `AtmosphericConditions` every point uses: temperature, pressure, relative
humidity, Mach speed, density, kinematic viscosity), `stallAngle`, and `configurations`: header,
`referenceLength`, `referenceArea` and `sameResultsAs`. Configurations that differ only in their
motors have identical aerodynamics (no point depends on the motors): such a configuration has
`sameResultsAs` = the index of the first identical one and no results of its own (`aeroResults()`
in `GoldenData.h` follows the reference); otherwise `sameResultsAs` is null and the configuration
has the following, computed with a `BarrowmanCalculator` of its own (the calculator caches the
damping geometry of the active components across configurations):

- `geometryWarnings` (`checkGeometry` on the whole rocket);
- `points`: 21 grid points (Mach 0.05, 0.3, 0.6, 0.9, 1.1, 1.5, 2.0 × angle of attack 0°, 2°, 10°;
  θ = 0, no rotation; index = 3 × Mach index + AoA index) and four more:
  - 21: Mach 0.3, AoA 5°, θ = π/4;
  - 22: Mach 0.8, AoA 2°, θ = 1 rad, roll 20 rad/s, pitch 2 rad/s, yaw 1 rad/s about the default
    pitch centre, the nose tip (the simulation always damps about it);
  - 23: the same rates about the structure CG (`FlightConditions.setPitchCenter`, which OpenRocket
    itself never calls);
  - 24: Mach 0.6, AoA 0 with thrusting nozzles (powered base drag): every active motor mount
    contributes `getMotorCount()` nozzles of half its `getMotorMountDiameter()` to its component
    assembly (motor-independent, whether or not the configuration gives the mount a motor).

  `yawDampingMoment` is 0 at every point: OpenRocket bounds its magnitude by the total Cyaw, which
  every Barrowman component calculation sets to 0. Each point has `conditions` (the exact
  `FlightConditions` values: `mach`, `aoa`, `theta`, rates, `pitchCenter`, `refLength`, `refArea`,
  `velocity`, `beta`, and `thrustingNozzleExitAreas`: `{assembly, area}` per component assembly
  path, empty except at point 24; a C++ test sets these values rather than recomputing them),
  `cp` (`getCP`: `[x, y, z, CNα]`), `forces` (`getAerodynamicForces`: `cp`, `cn`, `cm`, `cside`,
  `cyaw`, `croll`, `crollDamp`, `crollForce`, `cd`, `cdAxial`, `pressureCD`, `baseCD`, `frictionCD`,
  `overrideCD`, `pitchDampingMoment`, `yawDampingMoment`, `axisymmetric`), `components`
  (`getForceAnalysis`: the same fields per component and assembly, with `path`, tree order, as
  OpenRocket returns them, i.e. per instance) and `warnings`;
- `worstCP`: per Mach number at AoA 0, `getWorstCP` (`cp` and the `theta` it found). The `theta`
  is meaningful only where the CP depends on the wind direction (of the test rockets: the
  Iso-Haisu, with its two control fins). With three or four equal fins the 360 CPs that
  `getWorstCP` tries differ by a few units in the last place, and the direction that happens to
  give the smallest x follows the order in which OpenRocket sums the components (the order of
  their ids) and the last bit of the sines: another run of OpenRocket with other ids finds another
  direction. A port compares the `cp`; of the `theta` it can check that the one it finds is one of
  the 360 directions `getWorstCP` tries, that the CP at it is the golden worst CP, and that the
  golden `theta` is an equally bad direction for the port (`compareWorstTheta()` in
  `tests/core/goldens/GoldenDesign.h`, tested in `aero_golden_tests.cpp`), but not which of
  several equally bad directions is found.

### figure.json

What OpenRocket's 2D rocket figure draws of the design (`FigureDumper`): the figure of the rocket's
selected flight configuration, as `info.openrocket.swing.gui.scalefigure.RocketFigure` computes it
before it paints, in the figure's coordinates (metres, y up, before the projection to pixels).
RocketFigure itself needs Swing (its preferences and a component), so the harness replicates its
geometry half line by line: `updateSubjectDimensions()`, `updateShapes()` with
`addShapesFromInstanceEntries()` and `addThisShape()`, and the shapes `drawMotors()` fills. The shapes
themselves come from OpenRocket's own shape providers, the 20 classes of
`swing/src/main/java/info/openrocket/swing/gui/rocketfigure` (which import nothing but the core,
`java.awt.geom` and `java.util`), compiled read-only by `build.gradle` together with their service
registration (`swing/src/main/resources/META-INF/services/...RocketComponentShapeService`) and asked
through `RocketComponentShapeProvider` as RocketFigure asks them; `generate.sh` checks those files of
the checkout as it checks the core's. The figure is dumped after `aero.json` and changes nothing in
the design (with it added, every other file of `tests/data/goldens` but `manifest.json`, which lists
it, is byte-identical to before); the stable pass checks it byte for byte like the other design
documents.

- `configuration`: the selected configuration's header (`index`, `id`, `isDefault`, `name`, as in the
  other documents); `showMarkers`: RocketFigure's preference "only show pod set/booster markers when
  selected" (`isShowMarkers()`), false as in a new installation, so every pod set and booster set
  draws its marker; `selection`: the paths of the selected components, none.
- `views`, five: the side, top and back views at rotation 0, and the side view rotated about the
  rocket's axis by π/6 (30°, the first angle the rotation control snaps to) and by 5π/4 (225°: sine
  and cosine both negative, the rocket seen from below and behind). Each has `view` (`side`, `top`,
  `back`), `rotation` (radians), `figureRotation` (`getFigureRotation()`: `rotate_x(rotation)`, in
  the top view applied after `rotate_x(-π/2)`; a transformation as in `geometry.json`), which every
  instance's transformation (`figureRotation` applied after its `InstanceContext`'s, those of
  `geometry.json`) and the motors go through, `subjectBounds` (`[x, y, width, height]`:
  `(min x, -maxR, length, 2 maxR)` in the side and top views, `(-maxR, -maxR, 2 maxR, 2 maxR)` in
  the back view, maxR the larger `Math.hypot(y, z)` of the two corners of the configuration's
  bounding box), `instances` and `motors`.
- `instances`: every instance RocketFigure asks shapes for (the active instances, then the extra
  render instances, but those of the rocket and of plain stages, which `addThisShape()` skips), each
  with `path`, `extraRender`, `context` (its index among the component's `InstanceContext`s),
  `instanceNumber`, `displayOrder` (`getDisplayOrder_side()` in the side and top views,
  `getDisplayOrder_back()` in the back view; RocketFigure draws the highest first, so the lowest
  ends on top), `color` (the component's `getColor()` as `[r, g, b, a]`, or null) and `lineStyle`
  (`getLineStyle()` by name, or null), which every shape of the instance carries but the invisible
  ones, and `shapes` in the provider's order.
- `motors`: for every active motor whose mount is visible, at each of the mount's
  `getComponentLocations()`, `mount` (path), `location` (the index) and `shape` (a `rect` from the
  front of the motor, its length long and its diameter high, in the side and top views; an
  `ellipse` of its diameter in the back view).
- A shape is `{"type", its parameters, "visible"}`: `rect` and `ellipse` with `frame` (`[x, y,
  width, height]`), `roundRect` with `frame` and `arc` (`[arc width, arc height]`, full diameters as
  Java keeps them), `arc` with `frame`, `angles` (`[start, extent]` in degrees) and `arcType` (`open`,
  `chord`, `pie`), `line` with `points` (`[x1, y1, x2, y2]`), and `path` with `precision` (`float`
  for a `Path2D.Float`: fin outlines, tabs and roots, conical transitions, the fins' back views;
  `double` for a `Path2D.Double`, which `AffineTransform.createTransformedShape` makes of a rotated
  rail button too), `winding` (`nonZero` or `evenOdd`), `segments` (the letters of its
  `PathIterator`'s segments, M, L, Q, C and Z, a run of one letter written as the letter and its
  length: `"ML182Z"` is a move, 182 lines and a close) and `coordinates` (the x and y of every point
  of every segment in their order: one point for M and L, two for Q, three for C, none for Z; a
  `Path2D.Float`'s floats written as the doubles they are). `visible` is false for a shape the
  provider gives the colour `ORColor.INVISIBLE`: the selection areas of rail buttons and of the pod
  set and booster set markers. These six classes are all the providers make; any other fails the
  dump.
- Orders. RocketFigure keeps its shapes in a `PriorityQueue` ordered by display order alone, fed from
  `InstanceMap`s, which are hash maps (keyed by components, which hash by their ids), and
  `drawMotors()` iterates `getActiveMotors()`, a hash map too: neither order is reproducible, and
  neither is kept. The instances are written in the tree order of their components, the active
  ones before the extra render ones, a component's in context order; the motors in the tree order
  of their mounts, a mount's by location. Sorted the same way, a port's instances have to be these
  one for one, which gives every display order the same set of instances in both.
- Not dumped: the figure with the markers preference on or with a selection (the pin tests of
  `tests/core/figure`, made with probes, cover those), the other flight configurations, carets, the
  info overlay and the 3D views. No golden design has a deployment charge, the one shape OpenRocket
  draws at random (an unseeded `java.util.Random` on every paint).
- Size: 3.0 MB for the 29 designs (238 KB for "Pods--airframes and winglets", the largest; 4 KB for
  the two body tubes of "simple-2-stage"), every number written with `Double.toString` as in every
  other file, so a float path keeps its exact floats.

How QtRocket compares them: `tests/core/goldens/figure_golden_tests.cpp` builds
`RocketFigureGeometry` (`figure/`) for the selected configuration of each design, loaded and settled
as the other golden tests load it (`GoldenExamples.h`; the test rockets built by their makers and
settled), in each view, and compares the subject bounds, the instances (QtRocket's drawing order,
display order descending with ties in the order above, is checked first) and the motors. A value is
compared exactly where the arithmetic allows it: when QtRocket's figure rotation and the instance's
`InstanceContext` transformation are Java's bit for bit (`figureRotation` and `geometry.json`) and the
component's shapes use nothing but `+ - * /` and `sqrt`; otherwise within 1e-14 relative to
max(1, |value|), a float path within 2^-23. On Linux 5201 of the 5472 shapes and all 280 motors are
compared exactly (the figure rotations and the instance transformations are Java's there); the 271
others are those of the five rail buttons, the seven canted fin sets, the elliptical fin set and
the two power nose cones, of which all but six are Java's bit for bit as well: six shapes of the
two instances of the rail button of "Parallel booster staging" (angle offset -π/2) differ from
Java's in the last bit, by 4e-19 m. The subject bounds go through `Math.hypot`, fdlibm's, and are compared within 1e-14. The deployment
charge's rays (none in the goldens) would be compared by their number and range only.

### sim_&lt;NN&gt;_&lt;name&gt;.json and the branch CSV files

`index`, `name`, `flightConfiguration` (`index`, `id`, `name`), `optionsSource` (`document` or
`applicationDefaults`), `variant` (what a variant simulation changed in the default options, else
null), `options` (every `SimulationOptions` value the run used: launch rod length,
into-wind flag, angle and effective direction, wind model type, `averageWind`, `multiLevelWind`,
launch site, geodetic method, atmosphere, time step, maximum time and step angle, seed, gravity
model, stepper, recovery speed warning thresholds, lookup-table flags), `harness` (the document's
own seed and wind deviations and what the harness set; in a file of the stable-step set also
`documentTimeStep`, the time step the simulation had, and `timeStep`, the stable one it was run
with), `extensions` (`id`, `type`, `name`,
`config`), `skipped`, `skipReason`. A simulation that ran also has `result` (`status`: `completed`
or `exception`, with `exceptionType`/`exceptionMessage`; an abort is a `SIM_ABORT` event, not an
exception; `jitterReplacements`), `summary` (the `FlightData` values: `maxAltitude`,
`maxVelocity`, `maxAcceleration`, `maxMachNumber`, `timeToApogee`, `flightTime`,
`groundHitVelocity`, `launchRodVelocity`, `deploymentVelocity`, `optimumDelay`, `branchCount`),
`warnings` and `branches`: `index`, `name`, `sourceComponent` (path), `rows`, `optimumAltitude`,
`timeToOptimumAltitude`, `optimumDelay`, `separationTime`, `csv` (the name of the branch's time
series file, or null in a stable-step set that is written without time series: everything else
of the branch is there all the same), `excludedColumns`, `columns`
(`key`, `name`, `symbol`, `builtin`, `min`, `max`, in OpenRocket's column order) and `events` in the
order OpenRocket recorded them: `time`, `type` (`FlightEvent.Type` name), `source` (path or null)
and `data` (motor events: `mount`, `designation`, `motorCount`; `SIM_ABORT`: `cause`,
`description`; `SIM_WARN`: the warning; `EXCEPTION`: the message).

The CSV (gzip-compressed, UTF-8, `\n` line ends) has one header line of column keys and one line
per data point. A key is the data type's save key (`time`, `altitude`, `drag_coeff`, ...) or, for
types that are not built in (extension data), `custom:<name>`. Values use `Double.toString`
(`NaN`, `Infinity`). Each column is in its data type's SI unit, except latitude and longitude, which
OpenRocket stores in degrees. Rows keep OpenRocket's two-phase storage (a row is opened by
`addPoint()` and filled by later `setValue` calls).

How QtRocket compares them (the test-rocket inputs: `tests/core/goldens/simulation_golden_tests.cpp`
for the default-step set and `simulation_stable_golden_tests.cpp` for the stable-step set, both on
`GoldenSimulations.h`; the simulations of an example design are those of the document that the
`.ork` loader makes of its file, flown with their own options and extensions by the golden tests
of the examples' simulations in the same directory): each test rocket is built by its C++ maker, the
options are set as the harness sets them, and
the run has the same two listeners. `tests/core/simulation/JitterRemoval.h` is the port of
`JitterRemoval`: the forces listener is the first simulation listener and the conditions listener
the last, both are system listeners, the stepper draws its random numbers all the same, and the
jittered forces are replaced by the calculator's result for the captured flight conditions (the
port tells the Runge-Kutta steppers' hook from the landing and tumble steppers' by the normal force
coefficient of the forces, which the latter leave NaN, where the harness walks the call stack);
the number of replacements is compared with `result.jitterReplacements`. Compared per simulation:
the options and the `harness` block, `result`, the summary values, the warnings, and per branch its
header, the columns (key, name, symbol, order, minimum and maximum), the events in order (type,
source, data and time) and every value of the CSV. One order is not OpenRocket's to keep: an event
that ignites the motors of several mounts queues their IGNITION events in the order of a hash map
of the mounts' ids, so consecutive IGNITION events with the same time are compared as a set (10 of
the 50 simulations of either set record such a pair, in an order that changes with the ids).
Everything that is not a number of the trajectory
is compared exactly (but for the one option that is computed with mathematical functions, the launch
rod direction of a launch into a multi-level wind: 1e-12); the trajectory of the default-step set at
1e-9 of a column's scale (times: 1e-6 s) as far as the flight is reproducible, and that of the
stable-step set over the whole flight, at tolerances and under rules taken from measurements (see
[Reproducibility of the simulations](#reproducibility-of-the-simulations)).

### resave/rocket.ork

The XML that `OpenRocketSaver.save` writes (file version 1.11, `creator="OpenRocket <version>"`),
with `StorageOptions.setSaveSimulationData(false)`: the simulations' definitions and summary values
are kept, their data points are not. It is written before the automatic dimensions are settled
(see [How OpenRocket is run](#how-openrocket-is-run)), so it can hold stale automatic values. For
the test rockets the file includes the harness's default and variant simulations (so their exact
options can be read back).

What QtRocket's tests do with these files:

- The deployment, separation and ignition settings of a design and the component presets its
  components were made from (`<preset>`, six in the examples), which no golden document records,
  are compared with the elements of its re-save, for the rebuilt test rockets and for the loaded
  examples (`tests/core/goldens/GoldenSettings.h`).
- The re-save of an example is loaded like the original file and has to give the same design: all of
  `geometry.json`, `mass.json` and `aero.json` (`example_design_golden_tests.cpp`).
- The re-save of a test rocket is loaded against a motor database of exactly the seven motors of
  `TestRockets` (the makers' motors are synthetic; the bundled database has real motors of the same
  designations, with other digests and masses) and compared with the design documents of the rocket
  and, in the options of its 50 simulations, with the simulation documents
  (`resave_golden_tests.cpp`). All 50 launch into the wind, so `launchRodDirection` of their
  `options` is the direction of the wind; the direction stored for the rod is compared with the
  `<launchroddirection>` of the file itself. OpenRocket's own reload of these files does not give
  the maker's rocket back in 14 values of `geometry.json`, which that test pins with both numbers:
  - a fin tab without a height is not saved, and the loader centres the tab on the fin where the
    maker's fin set had kept it at the front (`tabOffset` and the x of the four `tabPoints` of
    `/0/1/0` of `testrocket-big-blue` and of `/1/0/0` of `testrocket-multi-stage-event-test-rocket`);
  - the last point of a freeform fin is saved as `getFinPoints()` computes it in the body frame,
    0.15000000000000002 for 0.15 (`length`, and by one unit in the last place `rotationalInertia`
    and `rotationalUnitInertia`, of the same fin set of `testrocket-big-blue`; six values of its
    `mass.json` follow the inertia, also by one unit in the last place);
  - a `BodyTube(length, 0)` keeps the default wall of 2 mm on a tube of radius 0, which no setter
    allows, so the loaded tube has none (`details.thickness` of `/0/1/4/0` of
    `testrocket-estes-alpha-iii-with-pods`).
  After the load 36 of the 50 simulations are `NOT_SIMULATED` and the 14 of a configuration
  without motors `CANT_RUN`.

### Re-saves alone

Three kinds of re-saves have no other golden: they pin what QtRocket's saver writes against what
OpenRocket's writes, where the examples and the test rockets do not reach (the user's decision E6
of 2026-10-14). The harness writes them after every example and test rocket (the re-saves with data
of an example right after its passes, from a load of their own), and nothing they do reaches another
file: with them added, every other file of `tests/data/goldens` but `manifest.json`, which lists
them under `resaves` at its end, is byte-identical to before (checked file by file in run 10b).

- **The legacy files** (`legacy-*/resave/rocket.ork`): OpenRocket's re-save of each design file of
  `tests/data/ork` as loaded, as `resave/rocket.ork` of the examples (plain XML, version 1.11,
  without simulation data). They are loaded in the harness's environment, but for the presets: the
  six presets the example designs name, taken from the harness's preset database by their digests
  (`resaves.legacyPresetDigests`). Those are the presets the C++ tests have
  (`tests/core/rocket/preset/ExamplePresets.h`) until QtRocket reads preset files, so OpenRocket's
  load finds what QtRocket's finds, and its re-save is what a port of its saver writes for the
  design as QtRocket loads it (`tests/core/file/openrocket/RecordedOrkFiles.h` records the loads in
  the same environment): the references the six presets do not hold (seven `<preset>` elements of
  SEMROC parts in `v1.5-preset-usage.ork` and in its copy with decals, one of FlisKits in
  `v1.7-tube-fin.ork`) are refused by both loaders with a warning; OpenRocket forgets them,
  QtRocket keeps them and writes them back (decision U8). Every file is a fixed point of
  OpenRocket's own save and load after this first save (the probe FixedPointProbe of the tier-10
  scouts). Two files need a word:
  - `v1.6-preset-usage-decals-first.ork` is an archive whose first entry is the directory
    `decals/`: OpenRocket reads the first entry of an archive only and makes the empty rocket of
    it. The harness gives OpenRocket the archive packed again with its document first
    (`documentEntryMovedFirst`), as `RecordedOrkFiles.h` does; QtRocket takes the first `*.ork`
    entry of the file as it is (decision T3).
  - `v1.6-apocd.ork` is plain XML that names five decal images, files beside it which
    `tests/data/ork` does not have: its plain re-save is written, but OpenRocket's `.ork` archive
    of it cannot be (`archiveError`: `DecalNotFoundException` for its first decal image).
  A loader draws an id wherever the file stores none (every component of the formats 1.0 to 1.8,
  which store no component id; a flight configuration the file does not name, such as those of
  `simplerocket.ork`'s simulations with an empty `<configid>`): the re-saves hold the ids of the
  harness's sequence, which a port does not draw; a test numbers them (see below).
- **The saver-coverage documents** (`coverage-*/resave/rocket.ork`): designs `CoverageDocuments`
  builds, which hold every element and attribute name OpenRocket's saver writes that the other
  OpenRocket outputs among the goldens do not hold. `kitchen-sink`: TestRockets' Estes Alpha III
  with a kit name, a custom reference length, the CD and CG overrides of a body tube with their
  subcomponent flags, its line style and colour, a drogue, a nozzle exit diameter, a custom
  expression, two document preferences, all 29 settings of the photo studio, and a simulation that
  never ran with the extended ISA atmosphere, a constant gravity, a fixed random seed, a
  multi-level wind, landing dispersion settings, plot appearances, and drag and stability lookup
  tables with their rows (one with a file name); saved without simulation data. `stored-flight`:
  the same rocket with a simulation whose flight data is made by the harness (not simulated): a
  branch with an event after the landing (its warning's `eventid`) and an abort (`cause`); saved
  with the simulation data, which a save without it leaves out. With them, the goldens hold all 231
  element names and 61 attribute names that the saver's sources (`OpenRocketSaver.java`,
  `savers/*.java`, code that is commented out left aside) can write. Each document has to be a
  fixed point of OpenRocket's save and load (`GoldenDumper.dumpCoverage` fails the dump otherwise):
  its save, loaded again without a warning and saved, gives the same bytes. Hence its motors are
  database motors (Estes A8, B4 and C6 of the bundled database, the smallest digest of each
  designation, in place of the makers' synthetic ones, which a reload would not find), its document
  preferences a string and a boolean (a number is written with its type, which OpenRocket's loader
  does not read back), and it has no component preset and no script.
- **The re-saves with simulation data** (`resave-data/rocket.ork.gz` of "Deployable payload" and
  "Pods--powered with recovery deployment", `resaves.withData`): `OpenRocketSaver`'s XML of the
  example as loaded, with its simulation data, gzip-compressed at the best compression; the first
  save of a load of its own (a save stores the automatic dimensions its getters compute, so the
  saves of one loaded design can differ: neither of these two is stale as loaded). Between them they
  hold stored events of every kind the examples have but `TUMBLE` (which "3D printable nose cone and
  fins", "Parallel booster staging" and "Three stage low power rocket" store; the C++ test pins the
  digests of QtRocket's saves with data of all sixteen examples, `kJavaSavesWithData`), stored
  warnings with their sources, the boosters' branches and the 70 columns of the powered pods: 0.4 MB
  compressed (2 MB of text).

What QtRocket's tests do with them: `tests/core/goldens/ork_resave_diff_golden_tests.cpp` saves each
legacy file, loaded in the same environment (DesignFileEnvironment), and compares the save with the
re-save line by line, the creator aside and the ids a loader drew numbered by their first
occurrence on both sides: the same text but for QtRocket's own decisions (the references to presets
it keeps, the scripts it disables on load, decision D1, and the extension of an unknown id it keeps,
decision D11), each line listed with its line number and its indentation; it saves each coverage
document loaded from its re-save as the harness
saved it, which has to be the re-save, every line; and it compares its saves with data of the two
examples with the re-saves with data, the whole text.
`tests/core/file/openrocket/legacy_ork_roundtrip_tests.cpp` saves each legacy file as an `.ork`
archive with its data, whose entries are to be
`archiveEntries` (and which fails as OpenRocket's for `v1.6-apocd.ork`, whose plain save is taken
then), loads it back and compares the state of the reload with the state of the file as loaded
(`RecordedOrkFiles.h`): the changes are those OpenRocket's own round trip makes (printed by a Java
probe of run 10b, `LegacyRoundTripProbe`, in the same lines), but for the same decisions.
`goldens_schema_tests.cpp` checks the entries of `resaves` against the files.

## Tolerances

As planned (section 6.4); `manifest.json` `tolerances` records all but the last row (the motor
digests are compared exactly and live in `motors.json`):

| Quantity | Tolerance |
|---|---|
| geometry, mass and aerodynamic coefficients | relative 1e-9 |
| CP and CG positions | absolute 1e-9 m |
| table-interpolated aerodynamics (nose drag, NACA charts) | relative 1e-7 |
| simulation event times | absolute 1e-3 s |
| apogee and maximum velocity (calm, no jitter) | relative 1e-4 |
| noisy runs (wind turbulence, jitter) | relative 2e-2, event sequence equal |
| motor digests (`motors.json`) | exact |

OpenRocket's hash-map iteration orders are not reproduced by the port (whose instance maps are
insertion ordered), so sums may differ in the last bits; that is why the tightest tolerances are
relative 1e-9 rather than bit equality.

### Reproducibility of the simulations

The two simulation rows of the table assume that a calm run without jitter is deterministic. At the
installation's time step of 0.05 s it is not: 30 of the 50 test-rocket simulations amplify a
difference in the last bit (the pitch oscillation is integrated beyond the stability limit of the
Runge-Kutta method, and the step size control follows it) until row counts, event times and maxima
change. OpenRocket does so itself: with other random component ids, and so another summation order
of its hash maps, the harness's own code gives other results. In each of eight dumps made with
`UUID_SALT` (see [Regenerating](#regenerating)), 15 to 20 of the 50 simulations have another number
of rows than the committed file (21 of them in at least one dump), 10 to 13 another maximum
velocity (by more than 1e-6) and 7 another apogee time (by more than 1 ms); the `[C6-5]` flight of
the Estes Alpha III has 796, 803 or 817 rows and its apogee at 5.971 s, 5.954 s or 5.967 s. A golden
simulation is one such run. `tests/core/goldens/simulation_golden_tests.cpp` therefore compares
every simulation as far as it is reproducible (it measures that with a second, last-bit perturbed
run): everything that is not a number of the trajectory, and the trajectory up to its horizon, at
relative 1e-9. A row on the launch rod is reproducible when the perturbed run moves no value of it
by more than 1e-12 of its column's scale, a row after the rod when by no more than 1e-13 (a
difference grows in free flight from wherever it started, and the golden run started elsewhere
than the perturbed one); for those 30 flights that is the launch rod and the first 0.007 s to
0.29 s of free flight, 1289 of the 22330 rows on Linux. The noise-dominated out-of-plane columns
(below) are compared on the launch rod only, as in the stable-step set, and what that leaves out is
counted. The header of the test has the measurements.

The stable-step set is there so that whole flights are compared. At 0.01 s the pitch oscillation is
integrated within the stability limit and nothing is amplified by the step size any more; what is
left is not rounding noise but four places where OpenRocket's algorithm itself decides by the last
bit. Measured with OpenRocket against itself: nine dumps of the harness that differ in nothing but
the ids of the components (the committed set and eight made with `UUID_SALT=salt-b`, `salt-c`,
`salt-d`, `salt-e`, `review-r1`, `review-r2`, `verify-v1` and `verify-v2`), every pair of them,
50 simulations with 53 branches and 35323 rows:

| What | Largest difference between two runs of OpenRocket |
|---|---|
| status, branches, warnings, event sequences, event data | none, but for the order of IGNITION events with the same time, which follows the hash order of the motor mounts' ids: 10 of the 50 simulations (the flights of the two Estes Alpha III with a second motor mount) record such a pair in either order |
| number of rows | differs in 7 of the 53 branches: the two stages that tumble before their apogee (the second stage of the multi-stage rocket: 563, 564, 567, 568, 571, 572 or 575 rows; the booster of the Beta: 631 or 632) and by one row five flights that pass their apogee under the parachute |
| number of jitter replacements | differs in the multi-stage simulation only (6444 to 6548) |
| maximum altitude, velocity, acceleration, Mach number | 4.9e-9, 5.6e-9, 2.1e-9, 8.9e-10 (relative) |
| time to apogee; optimum delay | 3.2e-8 s; 5.0e-11 s |
| flight time | 1.4e-4 s; 8.0e-7 s between runs that step alike from the apogee |
| ground hit velocity | 3.9e-11 of itself, also between runs that step differently from the apogee |
| times of the events the motors and the launch rod time | none; 2.6e-10 s after a late handling |
| times of APOGEE, TUMBLE, GROUND_HIT | 2.1e-8 s, 1.2e-7 s, 8.0e-7 s between runs that step alike from the apogee; 1.0e-3 s (APOGEE) and 1.4e-4 s (GROUND_HIT, SIMULATION_END) where one run takes the extra 1 ms step; in a stage that tumbles before its apogee 6.3e-3 s, 8.9e-3 s, 1.7e-2 s |
| time series on the launch rod | 2.7e-13 of a column's scale |
| time series off the rod, where compared at the column's tolerance | 4.9e-9 (altitude), 2.3e-8 (velocity), 4.8e-7 (angle of attack), 2.6e-6 (pitch rate), 8.7e-6 (pitch damping moment coefficient) of the column's scale |
| attitude columns in hunting rows | six are noise there: up to 8.8e-4 of the column's scale (lateral acceleration in body coordinates), as much as their values; twelve keep their signal: 2.6e-7 (centre of pressure) to 1.8e-5 (lateral velocity) |
| lateral airspeed near the hunting threshold of 1 mm/s | 1.1e-6 m/s within 0.15 mm/s of it; no row is decided otherwise by two runs |
| out-of-plane columns of a planar flight, off the rod | 1.3 and 0.58 of the column's scale (lateral acceleration across the wind, in world and in body coordinates), 1.2e-3 (yaw rate), 1.2e-3 (position across the wind), 1.4e-4 (its direction) |
| rows of an Euler stepper | drag coefficients, mass, centre of gravity, inertias: none (1e-16 of the scale at most); vertical velocity 5.6e-9 |

How the numbers are computed (the scripts are not part of the repository; this is what they do): a
difference of a time series is the largest absolute difference of two values in the same row and
column, as a fraction of the column's scale, the largest finite magnitude of the column in the first
dump of the pair; "relative" is relative to the larger of the two values; consecutive IGNITION
events with the same time are sorted by their source before two event sequences are compared; the
rows of the first three lines and the times are compared without any rule, the time series under
the rules below (the plan of a pair is taken from its first dump, as the test takes it from the
golden files), and a pair "steps alike from the apogee" when both runs have the same number of rows.

The four places, each of which the golden rows show, and what
`tests/core/goldens/simulation_stable_golden_tests.cpp` (`SimulationStableGolden`) does about them:

- **Hunting**: `AbstractSimulationStepper.calculateFlightConditions` sets the pitch and yaw rate of
  the flight conditions to zero while the lateral airspeed is below 1 mm/s, which a weathercocked
  rocket reaches after a second of flight; without damping its attitude wanders within the
  threshold. In such a row (pitch rate and yaw rate exactly zero under a Runge-Kutta stepper in
  free flight: 5552 rows of 30 branches) the six columns that are noise there (the angle of attack
  and what answers it in the row) are not compared; twelve attitude columns that keep their signal
  (the orientation, the centre of pressure, the stability derivatives, ...) are compared at a
  tolerance of their own, 1e-4 to 1e-2 of their scales, and every other column as elsewhere. The
  decision itself is compared wherever it has room: the pitch rate of a hunting row has to be zero
  in QtRocket as well. Only in the 517 rows whose lateral airspeed is within 0.15 mm/s of the
  threshold (100 times what two runs differ by there) can a platform decide the other way, so the
  pitch rate, the yaw rate and the pitch damping moment are not compared in those; the threshold
  itself is pinned by a unit test of the stepper
  (`AbstractSimulationStepper.TheThresholdsOfTheLateralAirspeedAreExact`).
- **Noise-dominated columns**: the flights are planar but for the Coriolis acceleration, and the
  out-of-plane columns (yaw rate, lateral acceleration and position across the wind, its direction)
  hold 2e-6 to 3e-4 of their counterparts in the plane, which the hunting scatters (see the table).
  A column below 1e-2 of its counterpart is compared on the launch rod only (167 columns); in the
  one flight that leaves its plane (multi-level wind) these columns are compared. The roll rate of
  two branches (1e-7 of the pitch rate) is treated the same way: two runs reproduce it to 7.8e-7 of
  its scale, but a relative perturbation of the rotation of 1e-10 after every step moves it by
  3.8e-3 of it.
- **The step to the apogee of an Euler stepper**: `AbstractEulerStepper.step` lands a step on the
  apogee and leaves a rounding error of the vertical velocity (nothing in nine runs of ten, else at
  most 1.4e-16 of the velocity before, of either sign); when it is positive an extra step of 1 ms
  follows, so the last bit decides (50 of 684 pairs of runs differ, in 5 of the 19 branches whose
  recovery device is out before the apogee). Where a run and the golden run step differently from
  that row, the rows from there on are not compared (one branch on Linux), since the rest of the
  flight is on another time grid; but only when the run's own rows show the mechanism (a velocity
  left of at most 1e-13 of the one before, and the short step exactly after a positive one):
  anything else is reported.
- **A stage that tumbles before its apogee** (the booster of the Beta and the second stage of the
  multi-stage rocket) amplifies what the hunting left: compared up to its stage separation.

Everything else is compared over the whole flight: the structure exactly; the row counts; the
summary values at 1e-6 of their scales (the ground hit velocity at 1e-6 of itself, the deployment
velocity at 1e-5 and the launch rod velocity at 1e-9 of the velocity's scale); the times at 1e-4 s
(1e-3 s for the events that follow a late handling, an event that a step overshot); the time series
at 1e-9 of a column's scale on the launch rod and at 100 times the column's largest measured
difference off it (1e-9 to 1e-2), with tighter tolerances in the rows of an Euler stepper for the
columns that are constants there (1e-9) and for the vertical velocity (1e-6). QtRocket against the
committed files differs by the same amounts as OpenRocket against itself, on glibc and under 46
patterns of a libm moved by one ulp; on Linux 34542 of the 35323 rows and 90 % of the values are
compared, and the floor that holds on every platform is 26862 rows. The header of the test has the
rules, the measurements and the sensitivity to a perturbation a million times larger than an ulp.

**The example designs at the stable time step.** A design that is loaded from a file keeps the ids
of its components, so `UUID_SALT` changes none of the examples' results; `LAST_BIT` (see
[Regenerating](#regenerating)) perturbs every run in the last bit instead: after every step a
listener moves components of the rocket's velocity, rotation velocity, position or orientation to
a neighbouring representable number. Measured with 22 dumps, the committed one and 21 with the
patterns `both-away`, `both-toward`, `velocity-away`, `velocity-toward`, `rotation-away`,
`rotation-toward`, `position-away`, `position-toward`, `orientation-away`, `orientation-toward`,
`all-random-a` to `all-random-d`, `all-random-review1`, `all-random-review2`, `both-random-rv`,
`velocity-random-rv`, `rotation-random-rv`, `position-random-rv` and `orientation-random-rv`, each
with the time series (`STABLE_EXAMPLES=full`). For the test rockets the committed dump and the
twelve patterns that were measured first (the eight without a seed and without the orientation,
and `all-random-a` to `all-random-d`) give what the nine `UUID_SALT` dumps gave (maximum altitude
6.2e-9, maximum velocity 7.0e-9, time to apogee 3.5e-8 s, flight time 1.0e-6 s between runs that
step alike and 1.4e-4 s otherwise, 8 of the 53 branches with other numbers of rows, the same two
tumbling stages), so the perturbation stands in for another summation order; all 22 give a
little more (maximum altitude 6.5e-9, maximum velocity 9.5e-9, flight time 1.1e-6 s, 11 of the 53
branches with other numbers of rows).

For the 54 simulations of the examples at 0.01 s (69 branches, 734 events, 92286 rows), the
largest difference between two of the 22 runs. **An entry is the largest difference seen in these
22 runs, not a bound**: the thirteen dumps that were measured first gave 3.7e-9 for the maximum
altitude, 3.5e-7 s for the flight time, 2.8e-14 for the launch rod velocity and 0.39 s for the
TUMBLE of a dropped stage, and nine more patterns raised them to the values below; other
patterns, another JDK or another machine can raise them again. No entry left the rule it stands
under, and nothing of the structure differed before the last stage separation of a tumbling
dropped stage, in any run.

| What | Largest difference between two of the 22 runs of OpenRocket |
|---|---|
| status, branches, warnings, event sequences, event data | none up to the last stage separation of a dropped stage that tumbles; after it a tumbling dropped stage may record no TUMBLE at all: two of the seven did (the lowest stage of "Three stage low power rocket" in simulation 1, in three runs, and in simulation 2, in one) |
| number of rows | differs in 19 of the 69 branches: by one row in 13 of the 16 branches whose recovery device is out before the apogee (603 of the 3696 pairs of runs), and in 6 of the 7 dropped stages that tumble (589 to 1429 rows in the lowest stage of simulation 2 of the rocket named above) |
| number of jitter replacements | differs in the 4 simulations with such a stage only (13696 to 17188 in the one just named) |
| maximum altitude, velocity, acceleration, Mach number | 6.1e-9, 2.1e-9, 1.0e-11, 2.6e-11 (relative) |
| deployment velocity; launch rod velocity | 2.0e-8; 1.1e-13 (of themselves) |
| ground hit velocity | 2.4e-14 of itself between runs that step alike from the apogee; 9.2e-7 otherwise ("Pods--powered with recovery deployment", whose descent is still settling when it reaches the ground after 10 s: the landing stepper's steps of 0.5 s fall elsewhere) |
| time to apogee; optimum delay; optimum altitude of a branch | 1.35e-10 s; 2.9e-9 s; 6.1e-9 (relative) |
| flight time | 5.7e-7 s between runs that step alike from the apogee; 1.9e-4 s otherwise |
| times of the events the motors and the launch rod time | none; 1.2e-11 s after a late handling |
| times of APOGEE, RECOVERY_DEVICE_DEPLOYMENT, SIM_WARN, GROUND_HIT | 1.35e-10 s, 2.4e-7 s, 6.6e-8 s, 5.7e-7 s between runs that step alike from the apogee; 1.0e-3 s (APOGEE) and 1.9e-4 s (GROUND_HIT, SIMULATION_END) where one run takes the extra 1 ms step |
| a dropped stage that tumbles, after its separation | APOGEE 9.2e-3 s, TUMBLE 2.1 s, GROUND_HIT 1.3 s, optimum altitude 1.0e-4 (relative) |
| a warning's parameter | 9.0e-12 (relative) |
| minimum and maximum of a column (branches that step alike, without the tumbling stages) | 30 of the 71 columns agree to 1e-9 of the column's scale; 10 differ by more than 1e-5: `acceleration_y` 2.8e-3, `acceleration_bodyy` 5.2e-4, `position_y` 5.2e-4, `yaw_rate` 5.1e-4, `stability` 1.5e-4, `acceleration_x` 1.1e-4, `position_direction` 1.0e-4, `corrective_moment_coeff` 5.5e-5, `natural_frequency` 2.7e-5, `cp_location` 1.2e-5 |

So the examples meet the same four places as the test rockets, two of them more often and one in
a new form:

- **The step to the apogee of an Euler stepper** decides 13 branches of the examples (9 of the
  test rockets in the same 22 dumps), and in one of them the ground hit velocity follows the time
  grid as well. A document says which step its run took from the apogee of the first branch: the
  APOGEE event is 1 ms after the summary's time to apogee exactly when the extra step was taken
  (it never says otherwise in the 22 dumps). For a later branch the document does not say (the
  booster of "Deployable payload" in its simulations 3 and 4: the committed runs took the
  regular step, and two of the 22 runs each the extra one).
- **A dropped stage that tumbles** does so after its apogee in 5 of the 7 cases, which the rule of
  the test rockets (TUMBLE before APOGEE) does not name. None of the 7 is reproducible after its
  separation, whichever comes first: the times differ by up to two seconds, and a stage can reach
  the ground without having tumbled (two of the seven did, in three and in one of the 22 runs).
  The two sustainers that tumble after their apogee and the dropped stages that descend under a
  recovery device are reproducible like every other branch.
- **Hunting** goes with the differences of 1e-9: they are largest in the flights with the most
  hunting rows (6.1e-9 of the maximum altitude with 803 of them), and the six branches without
  one that are no tumbling stages agree to 1.4e-12 in every value and to 2.4e-10 s in every time.
- **Late handlings** (an event without a row at its time) are in 26 of the 69 branches: in 19 it
  is a RECOVERY_DEVICE_DEPLOYMENT, which the engine queues 1 ms after the event that deploys the
  device and handles at the end of the step, in 6 a BURNOUT that a step overshot by 0.06 to
  0.13 ms, in one a SIM_WARN. The events after one do not move by more than the 5.7e-7 s above.

The test that compares QtRocket with these documents loads each example with the `.ork` loader
(the stable-step comparison of the examples in `tests/core/goldens`). Its tolerances are to be
chosen with this table, not computed from it: an entry is
a largest difference seen, so a hundred times an entry is no safe tolerance (a hundred times the
3.7e-9 of the first thirteen dumps would be 60 times the 6.1e-9 of the 22). The tolerances of
the test rockets' stable-step comparison, 1e-6 for a summary value, 1e-5 for the deployment
velocity and 1e-4 s for a time, are 163, 494 and 174 times the entries above; the ground hit
velocity between runs that step differently from the apogee would hold at 1e-6 by a factor of
1.08 only, so it is not to be compared there.

Whole flights are also compared with OpenRocket outside the golden data, in tests whose
expectations are pasted from Java probes run at a time step of 0.005 s:
`tests/core/simulation/engine_stable_run_tests.cpp` (the jitter removed as here; a single-stage and
a two-stage flight, the tumbling booster with tolerances of its own) and
`tests/core/simulation/engine_jitter_run_tests.cpp` (with the jitter, which the port draws from a
reproduction of `java.util.Random`: RK4 and RK6, two stages, a flight on lookup tables, and the
sequence of the random numbers with the places where the stepper starts it anew). The structure of
a run (the order of the events and of the listener calls, the warnings, the aborts) is pinned for
32 scenarios in `tests/core/simulation/BasicEventSimulationEngineTests.cpp`.

## Regenerating

Requirements: JDK 17 (`java` on the path, or `JAVA=...`; `generate.sh` refuses any other version,
because `Double.toString`, which writes every number, changed in JDK 19 (JDK-4511638) and would
rewrite committed values), a checkout of OpenRocket (`OPENROCKET_DIR`, default `../openrocket` next
to the QtRocket repository) with the `openrocket-database` submodule, and either network access or a
Gradle cache that already holds OpenRocket's dependencies (building OpenRocket once fills it).
OpenRocket's own Gradle wrapper is used; nothing is vendored here.

To reproduce the committed data, check out the commit recorded in `manifest.json`
(`openrocket.commit`) with its submodule first:

```sh
git -C ../openrocket checkout <openrocket.commit>
git -C ../openrocket submodule update --init core/resources-src/datafiles/openrocket-database
tools/openrocket-goldens/generate.sh            # all inputs, then manifest.json
tools/openrocket-goldens/generate.sh --only example-a-simple-model-rocket   # debugging only
```

`generate.sh` checks the checkout and refuses it (`ALLOW_DIRTY=1` overrides, and `manifest.json`
then records `"dirty": true`) when it has modified tracked files, untracked or ignored files among
what the build compiles or packages (`core/src/main/java`, `core/src/main/resources`,
`core/build.gradle`, `core/libs`, `ServicesForTesting.java`, the Swing module's
`gui/rocketfigure` sources and their service registration; the two ignored copies OpenRocket's own
build leaves in `core/src/main/resources`, `ReleaseNotes.md` and the preset `database/`, are
excluded from the class path by `build.gradle` and not read), or when the preset submodule is not
initialized at the commit the superproject records or has local changes. It warns when the
checkout's commit differs from the one in the existing `manifest.json`. It then records
`git rev-parse HEAD` and the submodule's recorded commit in `manifest.json`, compiles OpenRocket's
`core` (and `ServicesForTesting` and the figure's shape providers) into
`tools/openrocket-goldens/build/`, runs `GoldenDumper` with a fixed locale and time zone, and prints
the size of the data. A run takes a few minutes. It deletes
and rewrites only the `example-*`, `testrocket-*`, `legacy-*` and `coverage-*` directories,
`presets/presets.json` and `manifest.json`.

`generate.sh --presets` runs `PresetDumper` alone, with the same checks of the checkout, in a few
seconds: it rewrites `presets/presets.json` and the line of its entry in `manifest.json` (which must
have one: an older manifest needs a full run), and nothing else. What it writes depends on the
preset files and OpenRocket's code alone, so it is what a full run writes, byte for byte; the
variables of the scratch dumps (`UUID_SALT`, `LAST_BIT`, `STABLE_EXAMPLES`) do not apply to it and
are refused, `GOLDENS_OUT` is honoured. The rest of the manifest stays, so the checkout must be the
one it records: a dirty checkout (`ALLOW_DIRTY=1`), another OpenRocket commit or preset commit
than the manifest's, or a manifest that is dirty itself is refused, and so is a `GOLDENS_OUT`
without a `manifest.json` that has the entry (copy the committed one there first), all before
anything is built or written.

The version written into `manifest.json` and `creator=` of the re-saved files is OpenRocket's
`build.version` with the commit hash appended as OpenRocket's build does, except that the hash is
always 9 characters long (`git rev-parse --short=9`): git's automatic length depends on the size of
the clone, and the goldens must not.

`--only` rewrites just the named inputs and leaves `manifest.json` alone (an input of a re-save
alone, `legacy-*` or `coverage-*`, can be named too; an example with a re-save with data writes it
with its passes); a name that matches no input is an error (before anything is deleted). Commit only
the output of a full run: OpenRocket keeps
process-wide state (the flight data type registry, the UUID sequence of earlier inputs), so a
partial run can differ in labels such as a custom column's symbol.

Four environment variables are for looking at other data than the committed set:

```sh
GOLDENS_OUT=/tmp/goldens tools/openrocket-goldens/generate.sh             # into a scratch directory
STABLE_EXAMPLES=full GOLDENS_OUT=/tmp/goldens tools/openrocket-goldens/generate.sh   # with the time series of the examples' stable-step set
UUID_SALT=salt-b GOLDENS_OUT=/tmp/goldens-b tools/openrocket-goldens/generate.sh  # other component ids
LAST_BIT=all-random-a GOLDENS_OUT=/tmp/goldens-c tools/openrocket-goldens/generate.sh  # runs perturbed in the last bit
```

`GOLDENS_OUT` writes everything into another directory instead of `tests/data/goldens` (the
`example-*` and `testrocket-*` directories and `manifest.json` there are replaced).
`STABLE_EXAMPLES` says what is written of the stable-step set of the example designs (see
[How OpenRocket is run](#how-openrocket-is-run)): `documents`, the default and the committed
data; `full`, with the time series of the branches (36.6 MB more); or `none`. `UUID_SALT` seeds
the UUID sequence of every input with its name and the salt, so that every component, flight
configuration and event gets another id and OpenRocket's hash maps another order; `manifest.json`
records the salt (`settings.uuidSalt`). Two such dumps differ wherever OpenRocket's results depend
on the summation order: that is the measurement behind
[Reproducibility of the simulations](#reproducibility-of-the-simulations). Designs loaded from
files keep the ids stored in them, so the salt changes none of the examples' files.

`LAST_BIT` is the measurement for those (`LastBitPerturbation`, `--last-bit` of `GoldenDumper`):
every simulation run of the dump, of either pass, gets a system listener that, after every step,
moves components of the rocket's state to a neighbouring representable number, which changes a
run by as little as another summation order does. It is placed right after the forces listener of
the jitter removal, as the perturbation of QtRocket's own golden tests is. The value is a pattern,
`<quantity>-<direction>`: the quantity is `velocity`, `rotation` (the rotation velocity),
`position`, `orientation` (the four components of the orientation quaternion), `both` (velocity
and rotation velocity: `LastBitListener` of the C++ tests) or `all` (the three vectors, velocity,
rotation velocity and position; not the orientation, which was added later, so that the patterns
of the measurement keep their meaning); the direction is `away` (every component to the next
number away from zero),
`toward` (toward zero) or `random-<seed>` (every component, after every step, one number up, one
down or not at all, drawn from a `java.util.Random` seeded from the pattern's text, the seed
being any text). A zero, an infinity and NaN are never moved. `manifest.json` records the pattern
(`settings.lastBitPerturbation`); nothing but the simulations' files and the manifest differs
from an unperturbed dump.

A dump made with `UUID_SALT`, with `LAST_BIT` or with another `STABLE_EXAMPLES` than `documents`
is never the committed data: `generate.sh` refuses each of them unless `GOLDENS_OUT` names another
directory than `tests/data/goldens`, and takes no argument but `--only <input name>` (everything
else `GoldenDumper` accepts is set by the script, so that nothing passed through can replace the
output directory, the salt or the pattern it checked).

Checking determinism after a change to the harness (same machine and JDK: the bytes must match):

```sh
cp -r tests/data/goldens /tmp/goldens-before
tools/openrocket-goldens/generate.sh
diff -r /tmp/goldens-before tests/data/goldens   # no output
```

The gzip streams are produced by the zlib the JDK links against, so on another machine (another JDK
build, zlib-ng, ...) the bytes of the `.csv.gz` files may differ while their content does not.
Compare the content there:

```sh
diff -r -x '*.csv.gz' /tmp/goldens-before tests/data/goldens   # no output
for f in $(cd tests/data/goldens && find . -name '*.csv.gz'); do
    cmp -s <(zcat "/tmp/goldens-before/$f") <(zcat "tests/data/goldens/$f") || echo "differs: $f"
done
```

`manifest.json` also records `javaVersion`, which changes with every JDK update. The committed data
is about 53 MB (JSON 12.4 MB, of which the figures 3.0 MB; time series 37.7 MB; re-saved designs
1.2 MB: those of the examples and the test rockets 0.7 MB, of the legacy files 0.4 MB, of the
coverage documents 0.03 MB; the re-saves with simulation data 0.4 MB), of which the
stable-step set of the test rockets is 13.4 MB (103 files: JSON 0.6 MB, time series 12.8 MB) and
that of the example designs 0.9 MB (54 documents); the preset golden `presets/presets.json` adds
4.3 MB; `motors.json`, which the motor harness writes, is another 1.5 MB.

After regenerating, run the C++ tests (`goldens_schema_tests.cpp` checks the structure; the golden
comparison tests check the values) and review the diff: a change of OpenRocket's results changes the
reference for the port.

## Interop check

Plan section 7 asks that a design QtRocket saved opens in OpenRocket without a warning. `InteropCheck`
asks more of every file: that OpenRocket gets from QtRocket's save the design it gets from its own
save of the same input. It is run by hand, at a milestone exit or after a change to the saver; CI
has no Java and never runs it. Two steps:

```sh
# 1. QtRocket writes its saves (DISABLED_ tests, which ctest and CI never run), the core's first:
cmake --build --preset clang-debug
build/clang-debug/bin/QtRocket_core_tests --gtest_also_run_disabled_tests --gtest_filter='OrkInteropSaves.*'
build/clang-debug/bin/QtRocket_gui_tests --gtest_also_run_disabled_tests --gtest_filter='*InteropSave*'
# 2. OpenRocket reads them:
tools/openrocket-goldens/interop.sh            # or: interop.sh <saves directory>
```

The test `OrkInteropSaves.DISABLED_WritesTheSavesOfTheInteropCheck`
(`tests/core/goldens/ork_interop_saves_tests.cpp`) writes into `interop-saves` of the build tree, the
directory above the `bin/` of the test program (`build/clang-debug/interop-saves` here, which is
the default directory of `interop.sh`); a command-line argument `--interop-saves=<directory>` of the
test program names another directory. It removes the saves of an earlier run from the directory
first (the files of the names it can write, and the GUI's saves below, nothing else) and writes 65
files, each named `<input>.<form>.ork` after the input of `manifest.json` and the form of the save
(`zip-data`, `zip-nodata`: an `.ork` archive of `GeneralRocketSaver` with or without the simulation
data; `plain-data`, `plain-nodata`: the plain XML of `OpenRocketSaver`). A directory that holds any
other `*.ork` file is refused: the test fails, names the files and writes nothing. Each save is the
first save of a load of its own, as OpenRocket's save it is compared with. The GUI tests'
`FromScratchInteropSave.DISABLED_WritesTheSaveForTheInteropCheck`
(`tests/gui/exit/from_scratch_exit_tests.cpp`, the same `--interop-saves=<directory>`) then adds a
66th file, `gui-from-scratch.zip-nodata.ork`, the input `gui-from-scratch`, which `manifest.json` does
not list: the rocket QtRocket's GUI designs from scratch in the exit test `FromScratchExitGolden` (the
steps of `tests/data/goldens/gui/from-scratch/scenario.json`), saved with Save as. Their
`Scenarios/FlightConfigurationsInteropSave.DISABLED_WritesTheSaveForTheInteropCheck/<scenario>`
(`tests/gui/exit/flight_configurations_exit_tests.cpp`) adds the 67th and 68th,
`gui-flight-configurations.zip-nodata.ork` and `gui-multi-stage.zip-nodata.ork`, the inputs
`gui-<scenario>` (`InteropCheck.GUI_TAB_SCENARIOS`, which no manifest lists either): the design file
each scenario of the Motors & Configuration tab opens, changed in QtRocket's tab by the steps of
`tests/data/goldens/gui/<scenario>/scenario.json` in the exit test `FlightConfigurationsExitGolden`, and
saved with Save as. The filter `*InteropSave*` runs the three writers.

| Inputs | Loaded by QtRocket | Saved as |
|---|---|---|
| the 16 examples | from `data/examples`, with the six presets of the examples (`goldenExampleEnvironment()`) | an archive with the document's default storage options (all 16 save their flight data), and plain XML without the simulation data |
| the 13 test rockets | from their re-saves, against the seven motors of TestRockets (`loadResave()`) | an archive with the document's default storage options (no data) |
| the 18 legacy files | from `tests/data/ork`, as `RecordedOrkFiles.h` records them | an archive with the simulation data; plain XML with it for `v1.6-apocd.ork`, whose archive OpenRocket cannot save either (`archiveError`) |
| the 2 coverage documents | from their re-saves | an archive with the document's default storage options |
| the GUI's from-scratch design | made in QtRocket's GUI (`FromScratchScenario`), not loaded; its input is OpenRocket's own save of the same scenario, `tests/data/goldens/gui/from-scratch/rocket.ork` (`tools/openrocket-gui-reference`) | Save as: an archive with the document's default storage options (no data: the design has no simulation) |
| the 2 designs of the tab's scenarios | opened (File > Open) from the file the scenario names (the from-scratch save, the two-stage example's re-save) and changed in QtRocket's tab (`FlightConfigurationScenario`); the input is OpenRocket's own save of the same scenario, `tests/data/goldens/gui/<scenario>/rocket.ork` (`tools/openrocket-gui-reference`, version 2) | Save as: an archive with the document's default storage options (no data: the scenarios' simulations were never run) |

`interop.sh` needs what `generate.sh` needs (Java 17; the OpenRocket checkout at the commit of the
goldens with its preset submodule, without local changes, or `ALLOW_DIRTY=1`; it warns when the
commit is not the one `manifest.json` records), builds the harness as `generate.sh` does and runs
`InteropCheck` in the harness's bootstrap, which takes about half a minute. It prints a report on
standard output, every file with what was checked, what was allowed and why, what was not compared
and why, and what failed, and exits with 0 when every file passes and every allowance of the list
below was used exactly as often as the list says (`InteropCheck.EXPECTED_ALLOWANCES`), 1 otherwise
and 2 on a usage error. A file that is missing, or an `.ork` file of the directory that belongs to no
input, fails too. Gradle leaves a daemon running: stop it afterwards (`<openrocket>/gradlew --stop`).

Each input is loaded in the environment of its goldens: the harness's (the examples and the coverage
documents), the legacy files' (six presets) or, for the test rockets, a motor database of exactly
the seven motors of TestRockets and no preset (their motors are synthetic: the bundled database would
give the mounts real motors of the same designations); the GUI's design in that of OpenRocket's GUI
after its start-up, in which `tools/openrocket-gui-reference` loads the saved design (as
`GuiReferenceEnvironment.h` of the C++ tests has it): no preset, no motor, and the default component
materials the start-up stores (Balsa for fin sets, Polystyrene for nose cones), the test preferences
otherwise (`InteropCheck.guiInjector()`); the designs of the tab's scenarios in that of version 2's
start-up (`GuiReferenceEnvironment(Databases::BUNDLED)`): the bundled motors and component presets (the
harness's: 1458 motor sets of 1588 motors, 5228 presets) with the same default component materials
(`InteropCheck.guiTabInjector()`). The design file a scenario opens is the one its `state.json` names
(`input.file`), with the SHA-256 recorded there; another one stops the check (exit status 1) before
any file is read. With Q OpenRocket's load of QtRocket's save,
O its load of the input and OO its load of its own save of O in the same form, a file passes when:

- every load warning of Q is on the allow-list below and O gives the same warning;
- Q is not empty: it has the components and simulations of O and OO (OpenRocket makes an empty rocket,
  without a warning, of an archive whose first entry is not the document);
- for an archive saved with the default storage options, O's defaults save the data where QtRocket's
  did;
- OpenRocket's re-save of Q (`OpenRocketSaver`, with the simulation data) is its re-save of OO, line for
  line, the ids the loaders drew for a design that names none numbered by their first occurrence
  (`withDrawnIdsNumbered()` of `GoldenResaves.h`; every id for the GUI's from-scratch design, whose ids
  QtRocket's GUI drew at random and the reference tool's `DeterministicUuids` for OpenRocket's save of
  the scenario; for a design of the tab's scenarios, every id the file it opens does not name: those of
  the configurations New and Duplicate made), but for the lines on the allow-list. OO, not the
  committed re-save: OpenRocket's own round trip changes a design (its saver writes the warning type
  `RecoveryHighSpeedDeployment`, which its loader reads as `Other` without the speed);
- the geometry, mass and aerodynamics documents of Q, dumped after the re-save and the settling of
  the automatic dimensions as `GoldenDumper` dumps them, are the committed `geometry.json`,
  `mass.json` and `aero.json` of an example, byte for byte; those of OO for a test rocket (whose
  goldens describe the maker's rocket, which OpenRocket's reload of the re-save changes in 14 values:
  [resave/rocket.ork](#resaverocketork)), and for a legacy file or a coverage document whose
  components have the same ids in Q and OO (the coverage documents and `v1.9-chute-release.ork`; the
  files of the formats 1.0 to 1.8 name no component id, so the two loaders draw other ones, and
  OpenRocket's sums follow the ids in the last bit: the report lists these seventeen under "not
  compared", and the GUI's from-scratch design, whose components have other ids than in OpenRocket's
  save of the scenario; it has no goldens of its own either); and those of OO for a design of the tab's
  scenarios, whose components keep the ids of the file it opens, the ids of the configurations the
  scenario made numbered on both sides as in the re-saves (the dumps name the configurations by id);
- the options of every simulation are those of O, or of OO where OpenRocket's own save changes them
  (a random seed that is not fixed is drawn by each load and left out); for an example or a test
  rocket, made reproducible as the harness makes them, also the options and extensions of the golden
  simulation documents (and, for an example, the harness's record of what it changed);
- the stored flight data of every simulation (the summary values, the warnings, each branch with
  the bits of every value of every type and its events) is that of O, or that of OO where OpenRocket's
  own save changes it (the report says how: a plain save without the data; the warning type above;
  the six decimal places and the recomputed summary values of a legacy file's first save);
- the decal images (by name, with the SHA-256 of their bytes, or unreadable as in OpenRocket's own
  plain save) are those of O, or of OO;
- an archive has the entries of OpenRocket's own archive of O, in name and order, each with the same
  bytes (`rocket.ork` aside, which the re-saves compare), and every `thrustcurves/<digest>.rse` in it
  loads with a curve of its digest (`MotorDigest.isDigestCompatible()`, as OpenRocket's
  `MotorHandler` takes an embedded curve), but for the allowed ones.

The allow-list (`InteropCheck.ALLOWED_WARNINGS`, `ALLOWED_RESAVE_CHANGES`, and the thrust curves:
a synthetic digest in a test rocket, `ALLOWED_CURVES` in any other input). Each allowance has to be
used exactly as often as `EXPECTED_ALLOWANCES` says over all 68 files (the counts below), so that it
cannot hide a new case of its kind:

| What | Where | Why |
|---|---|---|
| load warning "Simulation extension with id '...' not found." | `legacy-v1-7-simulation-extensions-and-scripting` (1) | decisions D11 and L10: QtRocket keeps the extension of an unknown id and writes it back; OpenRocket warns and drops it, in its load of the input too |
| load warning "No matching ComponentPreset for component ..." | `legacy-v1-5-preset-usage`, `legacy-v1-6-preset-usage-decals-first` (6 each), `legacy-v1-7-tube-fin` (1) | decision U8: QtRocket keeps a `<preset>` reference its presets cannot resolve and writes it back; OpenRocket, without the preset as well (the six presets of the legacy files' environment), warns and forgets it, in its load of the input too |
| re-save lines `<entry key="enabled" type="boolean">false</entry>` where OpenRocket's own has `true` | `legacy-v1-7-simulation-extensions-and-scripting` (2) | decisions D1 and E4: QtRocket runs no script and disables every enabled script with code on load, the roll control script OpenRocket trusts by its hash included |
| a thrust curve whose digest no curve of it gives | the 26 of the test rockets, each named by a synthetic digest (`digest <designation> test`) | the synthetic digests of TestRockets ("digest A8 test"); OpenRocket's own archive has the same bytes |
| the same | `thrustcurves/e5b53def203dd437ebf0d67846f6cd3b.rse` of "Pods--powered with recovery deployment" (1; this input and this entry only) | the RockSim text of the bundled database's Estes A10 of that digest, which read back gives a curve of the digest 6c22c2c6a938ec433d9e0208c31f4476: OpenRocket's RockSim writer does not keep this digest, and its own archive has the same bytes (its loader takes the database motor of the digest first and never reads the entry) |

The run of tier 10 (run 10b, QtRocket at the commit of the run, OpenRocket 5f164fd0e, JDK 17):
65 files of 49 inputs, 0 failed; allowed: U8 13, D1 and E4 2, D11 and L10 1, the test rockets'
thrust curves 26, OpenRocket's RockSim writer 1; not compared: the dumps of the seventeen legacy
files of the formats 1.0 to 1.8. On the standard error, where the tool names each file
before its check (`[interop] <file>`), OpenRocket logs four errors "attempt to add an empty motorConfig"
with their stack traces, all while it loads legacy inputs (`v1.0-roll-stabilized.ork`,
`v1.4-roll-stabilized.ork`, twice `v1.6-boosted-dart.ork`: an empty motor of a flight configuration),
none for a QtRocket save. The whole check takes about 25 s. The run of Milestone 2's exit (QtRocket
at the merge of its run 4, OpenRocket 5f164fd0e, JDK 17), with the GUI's save: 66 files of 50
inputs, 0 failed; the same allowances; not compared: the dumps of those seventeen legacy files and of
the GUI's design (whose re-save, 235 lines, is OpenRocket's re-save of its own save of the scenario).
The run of Milestone 3 (run 3, part X4: QtRocket at the commit of the part, OpenRocket 5f164fd0e,
JDK 17), with the saves of the tab's two scenarios: 68 files of 52 inputs, 0 failed; the same
allowances; not compared: the same dumps. `gui-flight-configurations` (14 components, 3 simulations;
an archive of `rocket.ork` and 2 thrust curves; a re-save of 390 lines) and `gui-multi-stage` (50
components, 3 simulations; 2 thrust curves; 1085 lines) pass every check, the dumps included; OpenRocket
logs nothing for them. About 20 s.

## Figure pin tables

`figure-pins/generate.sh` writes the three generated tables of the figure's shape tests in
`tests/core/figure`, which pin what OpenRocket's shape providers return for single instances
(`figure.json` holds whole figures): `BodyShapePins.h` (body tubes, transitions and nose cones,
rings, launch lugs, tube fins, `EmptyShapes`), `FinMassShapePins.h` (fin sets, mass components,
recovery devices, shock cords, rail buttons, the markers of pod sets, booster sets and the rocket) and
`FigureGeometryPins.h` (the figure of whole designs, also with the markers preference on and a pod set
or booster set selected, which `figure.json` does not dump; a rocket with booster sets without
children, the only extra render instances; a new design). The probes were written by parts F2 and F3
of milestone 2 (run 1); the selection of what is pinned is in the two generators. The script needs
what `generate.sh` needs (Java 17, the OpenRocket checkout), builds the harness as `generate.sh` does,
compiles the two probes against its classes (and the Swing module's self-contained `PrintUnit`, of
which `BodyShapesProbe` asks the print scale) into `build/figure-pins`, runs them headless on the
examples of `data/examples` and the test rockets (with no component presets, unlike the goldens:
three examples then name presets OpenRocket cannot find), with every double written by
`Double.toHexString` into `build/figure-pins/*.txt`, and writes the headers through the generators
(`PINS_OUT` writes them into another directory). The dumps are the same on every run but for the rays
of the hand-built deployment charge (an unseeded `java.util.Random`), which the generators leave out:
the headers are byte for byte the same. They were moved here from the probes' scratch directories in
part F4; the dumps were then byte-identical to the probes' own, which ran against OpenRocket's own
build of its core and Swing classes, but for those rays.

The script checks the checkout as `generate.sh` does, among what the harness compiles and the probes
read (`core/src/main/java`, `core/src/main/resources`, `core/build.gradle`, `core/libs`,
`ServicesForTesting.java`, `swing/.../gui/rocketfigure`, its `RocketComponentShapeService` service
file and `swing/.../gui/print/PrintUnit.java`; not the preset submodule, since the probes load no
presets): it refuses a modified tracked file or an untracked or ignored file there unless
`ALLOW_DIRTY=1`. Each header records the commit and whether the
checkout had local changes, in its head comment and as `kOpenRocketCommit` and `kOpenRocketDirty`;
`FigurePinsGolden` (`tests/core/figure/figure_pins_provenance_tests.cpp`) requires the commit of
`manifest.json` and a clean checkout. The probes run with a fresh `java.util.prefs` store as the
other tools do (`java.util.prefs.userRoot`, which only Linux honours) and with `openrocket.debug.prefs`,
which keeps OpenRocket off the user's own preferences on every system (the node `OpenRocket-debug`,
cleared when `SwingPreferences` starts; `TranslatorProvider` reads no locale from it). The hand-built
cases end with three boundary cases that no design reaches: a ring wall of exactly 0.0012 m, a nose
cone whose samples lie just over 1 mm apart, and a canted fin set with a tab shorter than the root's
divisions (its back view's search for the lowest tab point passes over points of equal height).

## CSV export fixtures

`CsvExportDumper` writes `tests/data/csv/*.csv.gz`, the fixtures of QtRocket's `CsvExport` golden
test (`tests/core/file/csv_export_golden_tests.cpp`): what OpenRocket's `CSVExport.exportCSV()`
writes for flight data STORED in the example designs. The designs are loaded and never simulated
again, so the numbers are those of the files and the export alone decides the text, which QtRocket
reproduces byte for byte. `tests/data/csv/README.md` lists the nine fixtures (a branch of each of
five examples, one of them also with four other sets of options) and what they cover.

- **Environment**: OpenRocket's test bootstrap as for the goldens (`ServicesForTesting` overridden
  by `PluginModule`, then a second injector with the thrust curve database read from
  `initial_motors.db`, so that every motor of the examples is found and every simulation gets the
  status OpenRocket gives it after a load), but no component presets: the stored data does not
  depend on them. The bootstrap is the dumper's own; nothing of `GoldenDumper` runs but `slug()`.
- **Export**: every type of the branch in the order of `getTypes()`, each in the default unit of
  its group, with the options OpenRocket's export dialog opens with (`,`, 3 decimal places,
  exponential notation, `#`, the simulation, field and event comments on) or, for the first
  fixture, also with the four sets of `CsvExportDumper.VARIANTS` (`_variant1` to `_variant4`).
- **Files**: `<example slug>_sim<NN>_branch<B>[_variant<V>].csv.gz`, gzipped at the best
  compression. `CSVExport` ends a line with the platform's line separator; the dumper refuses to
  run where that is not `\n`. QtRocket always writes `\n` (a documented deviation).
- `--all` writes every branch of every simulation of every example (73 files, 3.1 MB with the
  variants) into a scratch directory instead: QtRocket's export matched all 73 when the fixtures
  were made (tier 10a, OpenRocket 5f164fd0e).

The fixtures are independent of `tests/data/goldens` and of `manifest.json`. To regenerate them,
with the checkout at the commit of the goldens and Java 17 (the numbers go through
`TextUtil.doubleToString()` and `java.util.Formatter`, whose digits of some very large and very
small values differ in later JDKs, JDK-8300869):

```sh
"$OPENROCKET_DIR/gradlew" --quiet -p tools/openrocket-goldens -PopenrocketDir="$OPENROCKET_DIR" goldensClasspath
java -Djava.awt.headless=true -Duser.language=en -Duser.country=US -Duser.timezone=UTC -Dfile.encoding=UTF-8 \
    -cp "$(cat tools/openrocket-goldens/build/goldens-classpath.txt)" info.qtrocket.goldens.CsvExportDumper \
    --examples data/examples --motors data/motors/initial_motors.db --out tests/data/csv
```

(`data/motors/initial_motors.db` is OpenRocket's bundled database, byte for byte.) The gzip bytes
depend on the JDK's zlib, so compare the content of a regenerated file, not its bytes
(`cmp <(zcat old) <(zcat new)`); the test does the same.
