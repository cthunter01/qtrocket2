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
  `JitterRemoval`, `AutomaticDimensions`, `DeterministicUuids`, `ApplicationDefaultsPreferences`,
  `ComponentIndex`, `Values`, `Json`.
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
| `aero.json` | per flight configuration: CP/CNα, total forces and the per-component force analysis of `BarrowmanCalculator` at 25 flight conditions, worst CP per Mach number, geometry warnings |
| `sim_<NN>_<name>.json` | one simulation: options, what the harness changed, extensions, result, summary values, warnings, and per branch the columns, events and extremes |
| `sim_<NN>_<name>_branch<i>.csv.gz` | the full time series of branch `i` of that simulation |
| `stable/sim_<NN>_<name>.json`, `stable/sim_<NN>_<name>_branch<i>.csv.gz` | the stable-step set: the same simulation run with a time step of 0.01 s and nothing else changed, in the same format (test rockets only; see [How OpenRocket is run](#how-openrocket-is-run)) |
| `resave/rocket.ork` | `OpenRocketSaver`'s XML for the design as loaded (as built, for the test rockets), without simulation data |

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
  `mass.json`; `aero.json`; the simulations. Per-configuration values are computed with that
  configuration selected (`Rocket.setSelectedConfiguration`, as the simulation engine does for its
  private copy), and the original selection is restored afterwards. Component-level values of
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
- **Stable-step set**: every simulation of a test rocket is run a second time with the time step
  set to `SimulationDumper.STABLE_TIME_STEP` = 0.01 s and nothing else changed, and written to
  `<input>/stable/` under the same names and in the same format. It exists because the first set
  is run at the installation's time step of 0.05 s, at which most of the flights are not
  reproducible beyond their first tenths of a second (see
  [Reproducibility of the simulations](#reproducibility-of-the-simulations)); at 0.01 s the whole
  flights can be compared. How it is produced:
  - In a pass of its own (`GoldenDumper.Pass.STABLE`), after every file of the input has been
    written: the UUID sequence is reseeded from the input's name, the rocket is built and the
    document set up again (an example design is loaded again), and the default pass is repeated
    call by call: the re-save, the settling of the automatic dimensions, the geometry, mass and
    aerodynamic dumps, then the simulations in the same order, each made reproducible as before.
    So a stable simulation meets the same components with the same ids in the same state as its
    default-step simulation, and its `flightConfiguration.id` is the one of `geometry.json`.
  - The stable pass writes only the simulations. Everything else it would write it compares with
    what the default pass wrote, byte for byte (`GoldenDumper.writeOrVerify`), and fails when a
    file differs: that is what makes it a repetition of the default pass.
  - The time step is set at the last moment before the run (`SimulationDumper.useStableTimeStep`);
    the simulation's `harness` records the stable time step (`timeStep`) and the one the
    simulation had (`documentTimeStep`), and `options.timeStep` shows the step used.
  - Nothing the default pass writes depends on the stable pass: with the stable-step set added,
    every other file of `tests/data/goldens` is byte-identical to before.
  - The harness can write the stable-step set of the 16 example designs as well:
    `STABLE_EXAMPLES=1 tools/openrocket-goldens/generate.sh` (`--stable-examples true` of
    `GoldenDumper`). It is off by default and those files are not committed: QtRocket cannot run
    the example designs before it reads `.ork` files, and the set would add 36 MB (54 simulations,
    69 branches, 92286 rows). It changes no other file but `manifest.json`. Every simulation of
    the examples is given the 0.01 s; their own time steps are 0.05 s (49 simulations) and 0.04 s
    (the 5 of "Pods--airframes and winglets"), so none is coarsened by it. What to do with a
    simulation whose own step is already 0.01 s or smaller is not decided, since no document has
    one.
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
  the same sequence on every run; the generator is reseeded from the input name before each input.
  This matters for the test rockets: OpenRocket's hash maps are keyed by those UUIDs, so summation
  orders (and the last bits of sums) would otherwise change from run to run. The CSV column
  `computation_time` (wall-clock time) is left out (branch `excludedColumns`). Nothing written holds
  a timestamp or an absolute path, and running `generate.sh` twice on the same machine produces
  byte-identical files (see [Regenerating](#regenerating) for other machines). Because the UUID
  sequence is the same in every process, sqlite-jdbc's native library (extracted under a UUID-based
  name) goes into a per-process directory under `build/work`, so concurrent runs cannot collide.

## File formats

Conventions for every JSON file:

- Every file starts with `"schema"` (`"manifest"`, `"geometry"`, `"mass"`, `"aero"`,
  `"simulation"`), `"schemaVersion"` (currently 1, `GoldenDumper.SCHEMA_VERSION` and
  `kGoldenSchemaVersion` in `GoldenData.h`) and, except the manifest, `"input"` (the input's name).
  The version is raised when a field changes its meaning or disappears, not for an addition: the
  stable-step set added files, manifest entries and two keys of `harness` in its own files, and
  left it at 1.
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
  (the C++ port rebuilds the test rockets with different UUIDs); the `id` UUID is given as well.
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
`stableTimeStep`, the time step of the stable-step set, and `stableSimulationsOf`, the kinds of the
inputs that have one: `["testrocket"]`, or `["example", "testrocket"]` with `STABLE_EXAMPLES=1`; and
`uuidSalt` in a dump made with `UUID_SALT`, which is never the committed data),
`tolerances` (below) and `inputs`: per input its `name`, `kind` (`example` or `testrocket`),
`source` (the `data/examples/` file, or the TestRockets method), `sourceSha256` (example files),
the paths of `geometry`, `mass`, `aero` and `resave`, `simulations` (`name`, `json`,
`branches`: the CSV files) and, for an input with a stable-step set, `stableSimulations`: the same
simulations in the same order with their files in `<input>/stable/` (entries of the same shape; an
input without the set has no such key). All paths are relative to `tests/data/goldens/`.

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
  `tests/core/goldens/aero_golden_tests.cpp`), but not which of several equally bad directions is
  found.

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

How QtRocket compares them (`tests/core/goldens/simulation_golden_tests.cpp`, the test-rocket
inputs): each rocket is built by its C++ maker, the options are set as the harness sets them, and
the run has the same two listeners. `tests/core/simulation/JitterRemoval.h` is the port of
`JitterRemoval`: the forces listener is the first simulation listener and the conditions listener
the last, both are system listeners, the stepper draws its random numbers all the same, and the
jittered forces are replaced by the calculator's result for the captured flight conditions (the
port tells the Runge-Kutta steppers' hook from the landing and tumble steppers' by the normal force
coefficient of the forces, which the latter leave NaN, where the harness walks the call stack);
the number of replacements is compared with `result.jitterReplacements`. Compared per simulation:
the options and the `harness` block, `result`, the summary values, the warnings, and per branch its
header, the columns (key, name, symbol, order, minimum and maximum), the events in order (type,
source, data and time) and every value of the CSV. Everything that is not a number of the trajectory
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
of its hash maps, the harness's own code gives other results for 8 of the 50 (the `[C6-5]` flight of
the Estes Alpha III: 796 or 803 rows, apogee at 5.971 s or 5.954 s). A golden simulation is one such
run. `tests/core/goldens/simulation_golden_tests.cpp` therefore compares every simulation as far as
it is reproducible (it measures that with a second, last-bit perturbed run): everything that is not
a number of the trajectory, and the trajectory up to its horizon, which for those 30 flights is the
launch rod and the first tenths of a second of free flight, at relative 1e-9; its header has the
measurements.

The stable-step set is there so that whole flights are compared. At 0.01 s the pitch oscillation is
integrated within the stability limit and nothing is amplified by the step size any more; what is
left is not rounding noise but four places where OpenRocket's algorithm itself decides by the last
bit. Measured with OpenRocket against itself: five dumps of the harness that differ in nothing but
the ids of the components (the committed set and four made with `UUID_SALT`, see
[Regenerating](#regenerating)), every pair of them, 50 simulations with 53 branches and 35323 rows:

| What | Largest difference between two runs of OpenRocket |
|---|---|
| status, branches, warnings, event sequences, event data | none |
| number of rows | differs in 4 of the 53 branches: the second stage of the multi-stage rocket, which tumbles (563, 568, 571, 571 and 575 rows), and by one row three flights that pass their apogee under the parachute |
| number of jitter replacements | differs in the multi-stage simulation only (6444 to 6548) |
| maximum altitude, velocity, acceleration, Mach number | 4.9e-9, 5.6e-9, 1.0e-9, 4.4e-10 (relative) |
| time to apogee; optimum delay | 3.2e-8 s; 5.0e-11 s |
| flight time | 1.3e-4 s; 8.0e-7 s between runs that step alike from the apogee |
| times of the events the motors and the launch rod time | none; 1.4e-10 s after a late handling |
| times of APOGEE, TUMBLE, GROUND_HIT | 2.1e-8 s, 9.1e-8 s, 8.0e-7 s; in the tumbling stage 5.6e-3 s, 7.5e-3 s, 1.5e-2 s |
| time series on the launch rod | 2.7e-13 of a column's scale |
| time series off the rod, where compared | 4.9e-9 (altitude), 2.3e-8 (velocity), 4.8e-7 (angle of attack), 2.7e-6 (pitch rate), 8.7e-6 (pitch damping moment coefficient) of the column's scale |
| attitude columns in hunting rows | up to 8.7e-4 of the column's scale (lateral acceleration in body coordinates) |
| out-of-plane columns of a planar flight | more than the column's scale |

The four places, each of which the golden rows show, and what
`tests/core/goldens/simulation_golden_tests.cpp` (`SimulationStableGolden`) does about them:

- **Hunting**: `AbstractSimulationStepper.calculateFlightConditions` sets the pitch and yaw rate of
  the flight conditions to zero while the lateral airspeed is below 1 mm/s, which a weathercocked
  rocket reaches after a second of flight; without damping its attitude wanders within the
  threshold. In such a row (pitch rate and yaw rate exactly zero under a Runge-Kutta stepper in
  free flight: 5552 rows of 30 branches) the columns that answer the angle of attack are not
  compared; elsewhere they are, at 1e-5 to 1e-2 of their scales.
- **Noise-dominated columns**: the flights are planar but for the Coriolis acceleration, and the
  out-of-plane columns (yaw rate, lateral acceleration and position across the wind) hold 2e-6 to
  3e-4 of their counterparts in the plane, which the hunting scatters. A column below 1e-2 of its
  counterpart is compared on the launch rod only (167 columns); in the one flight that leaves its
  plane (multi-level wind) these columns are compared.
- **The step to the apogee of an Euler stepper**: `AbstractEulerStepper.step` lands a step on the
  apogee and leaves a vertical velocity of about 1e-17 m/s; its sign decides whether an extra step
  of 1 ms follows (12 of 190 pairs of runs differ, in the 19 branches whose recovery device is out
  before the apogee). Where a run and the golden run step differently from that row, the rows from
  there on are not compared (one branch on Linux); the rest of the flight is on another time grid.
- **A stage that tumbles before its apogee** (the booster of the Beta and the second stage of the
  multi-stage rocket) amplifies what the hunting left: compared up to its stage separation.

Everything else is compared over the whole flight: the structure exactly; the row counts; the
summary values at 1e-6 of their scales; the times at 1e-4 s (1e-3 s for the events that follow a
late handling, an event that a step overshot); the time series at 1e-9 of a column's scale on the
launch rod and at 100 times the column's largest measured difference off it (1e-9 to 1e-2). QtRocket
against the committed files differs by the same amounts as OpenRocket against itself, on glibc and
under six patterns of a libm moved by one ulp; on Linux 34542 of the 35323 rows and 87 % of the
values are compared, and the floor that holds on every platform is 26862 rows. The header of the
test has the rules, the measurements and the sensitivity to a perturbation a million times larger
than an ulp.

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
`core/build.gradle`, `core/libs`, `ServicesForTesting.java`; the two ignored copies OpenRocket's own
build leaves in `core/src/main/resources`, `ReleaseNotes.md` and the preset `database/`, are
excluded from the class path by `build.gradle` and not read), or when the preset submodule is not
initialized at the commit the superproject records or has local changes. It warns when the
checkout's commit differs from the one in the existing `manifest.json`. It then records
`git rev-parse HEAD` and the submodule's recorded commit in `manifest.json`, compiles OpenRocket's
`core` (and `ServicesForTesting`) into `tools/openrocket-goldens/build/`, runs `GoldenDumper` with a
fixed locale and time zone, and prints the size of the data. A run takes a few minutes. It deletes
and rewrites only the `example-*` and `testrocket-*` directories and `manifest.json`.

The version written into `manifest.json` and `creator=` of the re-saved files is OpenRocket's
`build.version` with the commit hash appended as OpenRocket's build does, except that the hash is
always 9 characters long (`git rev-parse --short=9`): git's automatic length depends on the size of
the clone, and the goldens must not.

`--only` rewrites just the named inputs and leaves `manifest.json` alone; a name that matches no input
is an error (before anything is deleted). Commit only the output of a full run: OpenRocket keeps
process-wide state (the flight data type registry, the UUID sequence of earlier inputs), so a
partial run can differ in labels such as a custom column's symbol.

Three environment variables are for looking at other data than the committed set:

```sh
GOLDENS_OUT=/tmp/goldens tools/openrocket-goldens/generate.sh             # into a scratch directory
STABLE_EXAMPLES=1 GOLDENS_OUT=/tmp/goldens tools/openrocket-goldens/generate.sh   # with the examples' stable-step set
UUID_SALT=b GOLDENS_OUT=/tmp/goldens-b tools/openrocket-goldens/generate.sh       # other component ids
```

`GOLDENS_OUT` writes everything into another directory instead of `tests/data/goldens` (the
`example-*` and `testrocket-*` directories and `manifest.json` there are replaced).
`STABLE_EXAMPLES=1` adds the stable-step set of the example designs (see
[How OpenRocket is run](#how-openrocket-is-run)). `UUID_SALT` seeds the UUID sequence of every
input with its name and the salt, so that every component, flight configuration and event gets
another id and OpenRocket's hash maps another order; `generate.sh` refuses it without `GOLDENS_OUT`.
Two such dumps differ wherever OpenRocket's results depend on the summation order: that is the
measurement behind [Reproducibility of the simulations](#reproducibility-of-the-simulations).
Designs loaded from files keep the ids stored in them, so the salt changes little for the examples.

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
is about 47 MB (JSON 8.5 MB, time series 38 MB, re-saved designs 0.7 MB), of which the stable-step
set of the test rockets is 13.4 MB (103 files: JSON 0.6 MB, time series 12.8 MB); `motors.json`,
which the motor harness writes, is another 1.5 MB.

After regenerating, run the C++ tests (`goldens_schema_tests.cpp` checks the structure; the golden
comparison tests check the values) and review the diff: a change of OpenRocket's results changes the
reference for the port.
