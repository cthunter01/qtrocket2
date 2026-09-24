# OpenRocket golden data harness

`tools/openrocket-goldens` runs OpenRocket (Java) on a fixed set of rocket designs and writes what it
computes into `tests/data/goldens/`: geometry and mass properties, extended-Barrowman aerodynamics,
complete simulations and OpenRocket's own re-save of each design. QtRocket's golden tests
(`tests/core/goldens/*_golden_tests.cpp`, plan section 6.4) compare the C++ port against these
files. The data is committed; neither CMake nor CI ever needs Java.

- `generate.sh`: builds the dumper and regenerates `tests/data/goldens/` (see [Regenerating](#regenerating)).
- `build.gradle`, `settings.gradle`: a stand-alone Gradle project that compiles OpenRocket's `core`
  from a local checkout (read only) together with the dumper.
- `src/main/java/info/qtrocket/goldens/`: `GoldenDumper` (entry point, OpenRocket bootstrap, inputs,
  manifest) and its helpers `GeometryDumper`, `MassDumper`, `AeroDumper`, `SimulationDumper`,
  `JitterRemoval`, `DeterministicUuids`, `ApplicationDefaultsPreferences`, `ComponentIndex`,
  `Values`, `Json`.
- `motors/` (`MotorsDumper.java`, `dump-motors.sh`) belongs to the motor port: it writes
  `tests/data/goldens/motors.json` (digests and derived values of every curve of
  `initial_motors.db` and the test `.eng`/`.rse` files, plus the curves OpenRocket skips). This
  harness never writes or deletes `motors.json` and does not list it in `manifest.json`.

The C++ side reads the files through `tests/core/goldens/GoldenData.h` (nlohmann_json for the JSON,
`file/GzipStream` for the time series); `goldens_schema_tests.cpp` checks that every listed file parses
and has the keys described below.

## Inputs

| Kind | Inputs | Directory |
|---|---|---|
| Example designs | the 16 files of `data/examples/` (identical to OpenRocket's `core/src/main/resources/datafiles/examples/`) | `example-<file name, lower case, runs of other characters as '-'>` |
| Test rockets | the 13 `info.openrocket.core.util.TestRockets` factories `makeEstesAlphaIII`, `makeBeta`, `makeSimple2Stage`, `makeBigBlue`, `makeIsoHaisu`, `makeFalcon9Heavy`, `makeMultiStageEventTestRocket`, `makeEndPlateRocket`, `makeEstesAlphaIIIWithPods`, `makeEstesAlphaIIIWithMotorPods`, `makeEstesAlphaIIIWithSecondMotor`, `makeEstesAlphaIIIwithInlinePod`, `makeClusterPods` | `testrocket-estes-alpha-iii`, `testrocket-beta`, ..., `testrocket-cluster-pods` (see `GoldenDumper.TEST_ROCKETS`) |

Each input directory holds:

| File | Content |
|---|---|
| `geometry.json` | per component: geometry, mass properties, instances; per flight configuration: active stages, motors, reference values, bounds, the transform of every active instance |
| `mass.json` | per flight configuration: the STRUCTURE, LAUNCH, BURNOUT and MOTOR rigid bodies of `MassCalculator` and its CM analysis |
| `aero.json` | per flight configuration: CP/CNα, total forces and the per-component force analysis of `BarrowmanCalculator` at 23 flight conditions, worst CP per Mach number, geometry warnings |
| `sim_<NN>_<name>.json` | one simulation: options, what the harness changed, extensions, result, summary values, warnings, and per branch the columns, events and extremes |
| `sim_<NN>_<name>_branch<i>.csv.gz` | the full time series of branch `i` of that simulation |
| `resave/rocket.ork` | `OpenRocketSaver`'s XML for the design as loaded (as built, for the test rockets), without simulation data |

`<NN>` is the simulation's index in the document (two digits) and `<name>` its name made
file-system safe the same way as the directory names.

## How OpenRocket is run

- **Bootstrap** exactly as OpenRocket's tests do: `BaseTestCase.setUp()` (Guice with
  `ServicesForTesting` overridden by `PluginModule`), then, as `ExampleFilesTest.setUp()` does for the
  example files, a second injector that also binds the component preset database (every `.orc` file
  under `core/src/main/resources/datafiles/components`, sorted by path; the `database/` part is the
  `openrocket-database` submodule's `orc/` directory, used directly when a checkout was never built)
  and the thrust curve database (`initial_motors.db`, read with `ThrustCurveMotorSQLiteDatabase`
  from a private copy in `build/work`). `ServicesForTesting.java` is compiled from OpenRocket's test
  sources, not copied. Its `PreferencesForTesting` answer 0, `false` or `null` to most queries; that
  is the environment OpenRocket's own unit tests (and their pinned values) use.
- **Order**, per input: re-save; `geometry.json`; `mass.json`; `aero.json`; the simulations.
  Per-configuration values are computed with that configuration selected
  (`Rocket.setSelectedConfiguration`, as the simulation engine does for its private copy), and the
  original selection is restored afterwards. Component-level values of `geometry.json` are taken
  with the document's own selected configuration.
- **Simulations of the example files**: the document's own simulations, with their own options,
  except that the harness (`SimulationDumper.makeReproducible`) fixes the random seed to 0 and sets
  the standard deviation of the average wind model and of every level of the multi-level wind model
  to 0 ("calm": the average speed and direction are kept, so the wind is constant). A simulation with
  a JavaScript `ScriptingExtension` is not run (scripting is out of scope for QtRocket): its JSON has
  `"skipped": true` and a `skipReason`, and no CSV. Java extensions (`RollControl`, `AirStart`, ...)
  run as in OpenRocket.
- **Simulations of the test rockets**: one per flight configuration, the default configuration
  included, named after the configuration and added to the document before the re-save. Their
  options are those a fresh OpenRocket installation gives a new simulation
  (`DefaultSimulationOptionFactory` over `ApplicationDefaultsPreferences`, which answers every
  preference with `ApplicationPreferences`' default: rod 1 m into the wind, 2 m/s wind from π/2,
  28.61°N 80.60°W, ISA, WGS gravity, spherical geodetics, time step 0.05 s, RK4, ...), then made
  calm and seeded as above.
- **Pitch/yaw jitter removed**: `AbstractRKSimulationStepper.calculateForces` adds up to ±0.0005 of
  `java.util.Random` noise to Cm and Cyaw after every aerodynamic calculation. The harness runs
  `Simulation.simulate()`'s steps itself with two extra system listeners (`JitterRemoval`): the first
  listener answers the post-aerodynamic-calculation hook fired from that method with the
  calculator's jitter-free result for the same flight conditions (captured by the last listener).
  The landing and tumble steppers, which have no jitter, are left alone. The nested optimum-coast
  simulation keeps both listeners. `result.jitterReplacements` counts the replaced calculations.
- **Determinism**: `DeterministicUuids` installs a seeded `SecureRandom` provider before OpenRocket
  starts, so `UUID.randomUUID()` (new components, flight configurations, simulations, events) gives
  the same sequence on every run; the generator is reseeded from the input name before each input.
  This matters for the test rockets: OpenRocket's hash maps are keyed by those UUIDs, so summation
  orders (and the last bits of sums) would otherwise change from run to run. The CSV column
  `computation_time` (wall-clock time) is left out (branch `excludedColumns`). Nothing written holds
  a timestamp or an absolute path, and running `generate.sh` twice produces byte-identical files.

## File formats

Conventions for every JSON file:

- Every file starts with `"schema"` (`"manifest"`, `"geometry"`, `"mass"`, `"aero"`,
  `"simulation"`), `"schemaVersion"` (currently 1, `GoldenDumper.SCHEMA_VERSION` and
  `kGoldenSchemaVersion` in `GoldenData.h`) and, except the manifest, `"input"` (the input's name).
- Numbers are written with Java's `Double.toString`, which reads back as exactly the same double.
  Values JSON cannot represent are the strings `"NaN"`, `"Infinity"` and `"-Infinity"`
  (`goldenNumber()` in `GoldenData.h` reads both forms).
- Units are SI (m, kg, s, rad, kg·m², Pa, K, N) unless stated otherwise. Dimensionless coefficients
  use the configuration's reference area and length.
- Frames: the rocket (body) frame has its origin at the nose tip, x pointing aft along the axis and
  y, z radial; OpenRocket's rotations follow the left-hand rule. A position is `[x, y, z]`; a weighted
  coordinate is `[x, y, z, w]` with w the mass for a CG and CNα for a CP. A transformation is
  `{"rotation": R (9 numbers, row-major), "translation": t}` mapping p to R·p + t.
- Components are identified by their **path**: the child indices from the rocket, `"/"` for the
  rocket, `"/0"` for its first stage, `"/0/1"` for that stage's second child. Paths are portable
  (the C++ port rebuilds the test rockets with different UUIDs); the `id` UUID is given as well.
- Flight configurations are identified by `index` (their order in
  `Rocket.getFlightConfigurations()`, the default configuration first) and `id`; each entry also
  has `isDefault` and `name` (with `{motors}` substituted, e.g. `"[A8-3]"`).
- A warning is `{"type": class name, "priority", "description", "text", "sources": [paths],
  "parameter"}` (`parameter`: the speed or angle of the parameterised warnings only).

### manifest.json

`openrocket` (`commit`: the OpenRocket commit the classes were compiled from, `version`: its build
version as written into `creator=` of the re-saved files, `presetDatabaseCommit`, `javaVersion`),
`databases` (motor database path, SHA-256 and motor count; the preset files and preset count),
`settings` (Mach numbers, angles of attack, seed, wind deviation, jitter removal),
`tolerances` (below) and `inputs`: per input its `name`, `kind` (`example` or `testrocket`),
`source` (the `data/examples/` file, or the TestRockets method), `sourceSha256` (example files),
the paths of `geometry`, `mass`, `aero` and `resave`, and `simulations` (`name`, `json`,
`branches`: the CSV files). All paths are relative to `tests/data/goldens/`.

### geometry.json

- `rocketName`, `selectedConfiguration` (id), `loadWarnings` (the loader's warnings).
- `components`, depth-first with the rocket first. Each has `path`, `type` (OpenRocket class name),
  `name`, `id`, `stageNumber` (-1 for the rocket), `length`, `axialMethod`, `axialOffset`,
  `position` (relative to the parent), `isAerodynamic`, `isMassive`; the mass properties
  `componentMass`, `componentCG` (component frame, without overrides), `longitudinalUnitInertia`
  and `rotationalUnitInertia` (per kg, about the non-overridden CG), `mass`, `sectionMass`, `cg`,
  `longitudinalInertia`, `rotationalInertia` (overrides applied); `overrides` (mass, CG and CD
  override flags and values, subcomponent flags, `massOverriddenBy`/`cgOverriddenBy` paths);
  `componentBounds` (the points whose convex hull encloses the component, component frame);
  `instanceCount`, `instanceOffsets`, `instanceAngles`, `instanceLocations` (relative to the
  parent), `componentLocations` and `componentAngles` (every instance, parents' instancing
  included, rocket frame). `details` holds whichever of a fixed list of further getters the
  component's class has (`GeometryDumper.OPTIONAL_GETTERS`; key = getter name without `get`/`is`):
  volumes and areas (`componentVolume`, `fullVolume`, `componentWetArea`,
  `componentPlanformArea`, `planformArea`), radii and thickness, transition shape and shoulders,
  fin parameters and points (`finPoints`, `rootPoints`, `tabPoints`), `material`
  (`{name, type, density}`), finish, instance placement, `instanceBoundingBox`, motor mount and
  recovery device parameters, and for the rocket its reference type. Enums are written by name;
  other objects by their `toString()`.
- `configurations`: header, `stageCount`, `activeStages` (stage numbers), `referenceLength`,
  `referenceArea`, `length`, `lengthAerodynamic`, `boundingBox` and `boundingBoxAerodynamic`
  (`{min, max}`), `hasMotors`, `hasRecoveryDevice`, `motors` (per active motor, by mount path:
  `mount`, `motorName`, `designation`, `manufacturer`, `digest`, `ejectionDelay`, `ignitionEvent`,
  `ignitionDelay`, `motorCount`, `motorCountIncludingAssemblyCopies`, `motorOverhang`, `position`),
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
motors have identical aerodynamics: such a configuration has `sameResultsAs` = the index of the first
identical one and no results of its own (`aeroResults()` in `GoldenData.h` follows the reference);
otherwise `sameResultsAs` is null and the configuration has:

- `geometryWarnings` (`checkGeometry` on the whole rocket);
- `points`: 21 grid points (Mach 0.05, 0.3, 0.6, 0.9, 1.1, 1.5, 2.0 × angle of attack 0°, 2°, 10°;
  θ = 0, no rotation) and two more: Mach 0.3, AoA 5°, θ = π/4; and Mach 0.8, AoA 2°, θ = 1 rad,
  roll 20 rad/s, pitch 2 rad/s, yaw 1 rad/s about the structure CG. Each point has `conditions`
  (the exact `FlightConditions` values: `mach`, `aoa`, `theta`, rates, `pitchCenter`, `refLength`,
  `refArea`, `velocity`, `beta`; a C++ test sets these values rather than recomputing them),
  `cp` (`getCP`: `[x, y, z, CNα]`), `forces` (`getAerodynamicForces`: `cp`, `cn`, `cm`, `cside`,
  `cyaw`, `croll`, `crollDamp`, `crollForce`, `cd`, `cdAxial`, `pressureCD`, `baseCD`, `frictionCD`,
  `overrideCD`, `pitchDampingMoment`, `yawDampingMoment`, `axisymmetric`), `components`
  (`getForceAnalysis`: the same fields per component and assembly, with `path`, tree order, as
  OpenRocket returns them, i.e. per instance) and `warnings`;
- `worstCP`: per Mach number at AoA 0, `getWorstCP` (`cp` and the `theta` it found).

### sim_&lt;NN&gt;_&lt;name&gt;.json and the branch CSV files

`index`, `name`, `flightConfiguration` (`index`, `id`, `name`), `optionsSource` (`document` or
`applicationDefaults`), `options` (every `SimulationOptions` value the run used: launch rod length,
into-wind flag, angle and effective direction, wind model type, `averageWind`, `multiLevelWind`,
launch site, geodetic method, atmosphere, time step, maximum time and step angle, seed, gravity
model, stepper, recovery speed warning thresholds, lookup-table flags), `harness` (the document's
own seed and wind deviations and what the harness set), `extensions` (`id`, `type`, `name`,
`config`), `skipped`, `skipReason`. A simulation that ran also has `result` (`status`: `completed`
or `exception`, with `exceptionType`/`exceptionMessage`; an abort is a `SIM_ABORT` event, not an
exception; `jitterReplacements`), `summary` (the `FlightData` values: `maxAltitude`,
`maxVelocity`, `maxAcceleration`, `maxMachNumber`, `timeToApogee`, `flightTime`,
`groundHitVelocity`, `launchRodVelocity`, `deploymentVelocity`, `optimumDelay`, `branchCount`),
`warnings` and `branches`: `index`, `name`, `sourceComponent` (path), `rows`, `optimumAltitude`,
`timeToOptimumAltitude`, `optimumDelay`, `separationTime`, `csv`, `excludedColumns`, `columns`
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

### resave/rocket.ork

The XML that `OpenRocketSaver.save` writes (file version 1.11, `creator="OpenRocket <version>"`),
with `StorageOptions.setSaveSimulationData(false)`: the simulations' definitions and summary values
are kept, their data points are not. For the test rockets the file includes the harness's default
simulations (so their exact options can be read back).

## Tolerances

As planned (section 6.4), recorded in `manifest.json` `tolerances`:

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

## Regenerating

Requirements: a JDK 17 or 21 (`java` on the path, or `JAVA=...`), a checkout of OpenRocket
(`OPENROCKET_DIR`, default `../openrocket` next to the QtRocket repository) with the
`openrocket-database` submodule, and either network access or a Gradle cache that already holds
OpenRocket's dependencies (building OpenRocket once fills it). OpenRocket's own Gradle wrapper is
used; nothing is vendored here.

```sh
tools/openrocket-goldens/generate.sh            # all inputs, then manifest.json
tools/openrocket-goldens/generate.sh --only example-a-simple-model-rocket   # debugging only
```

`generate.sh` refuses a checkout with modified tracked files (`ALLOW_DIRTY=1` overrides), records
`git rev-parse HEAD` of the checkout in `manifest.json`, compiles OpenRocket's `core` (and
`ServicesForTesting`) into `tools/openrocket-goldens/build/`, runs `GoldenDumper` with a fixed
locale and time zone, and prints the size of the data. The run takes well under a minute. It deletes
and rewrites only the `example-*` and `testrocket-*` directories and `manifest.json`.

`--only` rewrites just the named inputs and leaves `manifest.json` alone. Commit only the output of a
full run: OpenRocket keeps process-wide state (the flight data type registry, the UUID sequence of
earlier inputs), so a partial run can differ in labels such as a custom column's symbol.

Checking determinism after a change to the harness:

```sh
cp -r tests/data/goldens /tmp/goldens-before
tools/openrocket-goldens/generate.sh
diff -r /tmp/goldens-before tests/data/goldens   # no output
```

The gzip streams are produced by the JDK's zlib: the bytes of the `.csv.gz` files may differ between
JDK builds, while their content does not. The committed data is about 32 MB (JSON 7.5 MB, time series
23 MB, re-saved designs 0.7 MB).

After regenerating, run the C++ tests (`goldens_schema_tests.cpp` checks the structure; the golden
comparison tests check the values) and review the diff: a change of OpenRocket's results changes the
reference for the port.
