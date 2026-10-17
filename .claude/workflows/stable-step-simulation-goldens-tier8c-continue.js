export const meta = {
  name: 'stable-step-simulation-goldens-tier8c-continue',
  description: 'Tier 8c, continuation of run wf_4233e5e0-a0c: the implementer (dc2ff43) and the comparison review are carried as data; runs the harness review, the verification of the findings and the fix',
  phases: [
    { title: 'Review', detail: 'the harness and data lens (the comparison lens is carried over)' },
    { title: 'Verify', detail: 'skeptical re-check of every blocker/major finding' },
    { title: 'Fix', detail: 'apply findings, re-verify, commit on the worktree branch' },
  ],
}

// Probe and log directory: the launching session's scratchpad (update when a new session runs this).
const PROBES = '/tmp/claude-1000/-home-travis-Development-qtrocket2/87f548e1-ea1f-4253-ad87-f65f7c95546e/scratchpad/probes'
// The commit of main this run starts from (the tier-8b merge).
const BASE = '30397b0'

const COMMON = `
Background. QtRocket (C++23) is a port of OpenRocket (Java, read-only reference at
/home/travis/Development/openrocket; never modify it). Its simulation engine was ported in tier 8b and is
compared with OpenRocket through golden files: tests/data/goldens/<input>/sim_<NN>_<name>.json and the branch
CSV files, written by the Java harness in tools/openrocket-goldens (README.md there describes the harness, the
files and how to regenerate them: read all of it first), and compared by
tests/core/goldens/simulation_golden_tests.cpp (read its header comment: it is the reason for this task).

The finding of tier 8b: at OpenRocket's default time step of 0.05 s the pitch oscillation of light rockets is
integrated beyond the stability limit of the Runge-Kutta method, a last-bit difference grows about 1e11 times
within a second of flight, and OpenRocket does not reproduce 30 of its own 50 test-rocket simulations when nothing
but the random component ids change. The committed test therefore compares, for those 30, only the structure and
the first 22 to 77 records. At a time step of 0.01 s the flights are stable: a probe of tier 8b
(${PROBES}/tier8b-goldens/, README.txt there; src/.../StableGoldens.java runs the harness's own SimulationDumper
at another time step; alt-dt0.01/ and alt-dt0.01-b/ are two such Java dumps with different id sequences;
javajava.py compares two dumps; exploration/ holds the measurements against alt-dt0.01) showed QtRocket and
OpenRocket agreeing in all 50 simulations in structure and event sequence, in the row counts of 52 of 53
branches, and to about 2e-9 in the summary values, while columns that are dominated by amplified noise (yaw
rate, lateral accelerations) and one tumbling booster branch still differ between two Java runs.

The decision of the project's owner: keep the 0.05 s goldens exactly as they are, and ADD a second set at a
0.01 s time step, so that whole flights are verified. This task adds that set for the 13 test rockets.

What CI taught after tier 8b (keep it in mind for every time you compare): the default-step test passed on Linux,
under all six patterns of the libm shim and on macOS, and failed on Windows. It compared the time of every event
'the motors time' at 1e-6 s, also beyond a flight's reproducible part; on MSVC the BURNOUT at 2.1 s of three
Estes Alpha III flights was handled at 2.100235 s, which moved the EJECTION_CHARGE by 0.000235 s. The mechanism,
in AbstractRKSimulationStepper.computeTimeStep() and BasicEventSimulationEngine.simulateLoop(): a queued event
gets a time relative to the simulation time at which the event that queues it is HANDLED (EJECTION_CHARGE =
handling time of its BURNOUT + delay; a later IGNITION = handling time of what ignites it + ignition delay; a
STAGE_SEPARATION follows one of those), an event is handled at the end of the first step that reaches its time,
and a step that the angle limits push below a twentieth of the time step is raised to that minimum 'even at the
cost of not being quite on an event', so a handling time can lie up to timeStep / 20 after the event's time
(0.0025 s at the default step, 0.0005 s at the stable one) whenever the step sequence before it differs. The base
commit of this task fixes the default-step test (isTimedInTheReproduciblePart() in simulation_golden_tests.cpp:
beyond the horizon only the LAUNCH and the BURNOUT of a motor ignited in the reproducible part have a fixed
time). Two lessons for the stable-step comparison: the shim does not predict MSVC, so a value is safe to compare
tightly only if it is insensitive for a stated reason (measure the sensitivity of every compared event time and
whole-flight value with a perturbation well above one ulp as well, for example a relative 1e-10 kick to the
velocity after every step, and report by how much each moves); and an event time derived from a handling time
needs a tolerance that covers a step raised to the minimum when the measurements show such steps near it.

Rules (in addition to CLAUDE.md, which you already have):
- tests/data/goldens is OpenRocket's output. No existing file under it may change by a single byte, except
  manifest.json, which gains entries. The new files are produced by the harness only, never edited by hand.
- The goldens are generated with Java 17 and tools/openrocket-goldens/generate.sh, exactly as the README's
  'Regenerating' section describes (the OpenRocket checkout must be clean; the Gradle cache already holds
  OpenRocket's dependencies).
- C++: clang-tidy runs with warnings as errors (tidy preset; clang-tidy 23.1.1, the same as CI). Naming: types
  CamelCase; functions/methods/variables camelBack; private members m_ prefix; constants kCamelCase; enum
  constants UPPER_CASE; [[nodiscard]] on getters and pure functions; explicit single-argument constructors; no
  raw new/delete, no const_cast/reinterpret_cast; a suppression is '// NOLINTNEXTLINE(check-name)' on its own
  line with the reason. misc-include-cleaner is on. Use std::format. Checks earlier tiers tripped over:
  misc-const-correctness, readability-function-cognitive-complexity (a loop, branch or lambda in a TEST body
  brings about 4 per gtest macro; helper functions always count: keep them small), bugprone-float-loop-counter,
  modernize-use-std-numbers. Files written with Bash heredocs bypass the clang-format hook: run
  'clang-format -i' on every C++ file you touch and finish with 'clang-format --dry-run --Werror'.
- Portability: the code must also build warning-free with MSVC (/W4 /WX) and Apple Clang, which only CI builds
  (no std::getenv, no implicit narrowing, never auto* for an iterator, none of the C++23 ranges views Apple's
  libc++ lacks, no locals named 'near' or 'far'). Transcendental functions differ in the last ulp between glibc,
  Apple libm and MSVC's UCRT, and CI runs the tests on all three: every tolerance has to hold there, which the
  one-ulp libm shim ${PROBES}/tier7b-calculators/libm/ulp.c stands in for (the README.txt files of
  ${PROBES}/tier7b-calculators/, ${PROBES}/tier7b-fix/ and ${PROBES}/tier8b-goldens/ say how it is built and
  preloaded; tier 8b ran its golden tests under six patterns of it).
- Verify in this order and make all four pass with zero warnings: 'cmake --workflow --preset dev',
  'cmake --workflow --preset tidy', 'cmake --workflow --preset asan', 'cmake --workflow --preset ci-gcc'
  (one at a time; redirect the output to a log file and grep it). Never edit anything under build/.
- Never loosen, drop or disable an existing assertion, and never choose a tolerance to make a difference go
  away: a tolerance comes from measurements that the header comment of the test reports.
- Put probes, measurements and logs under ${PROBES}/tier8c-<your role>/ with a README.txt. That directory is on
  a RAM-backed file system that earlier agents nearly filled: before you finish, delete the compiled binaries,
  scratch worktrees, build directories and dumps you made there that nobody needs afterwards (keep sources,
  scripts, logs and result tables).
- Your final message is consumed by a script: return the structured result only.
`

const SPEC = `
THE TASK
1. The harness (tools/openrocket-goldens/src/main/java/info/qtrocket/goldens/ and generate.sh): a second,
   'stable-step' set of simulation goldens.
   - For every simulation of an input the harness runs the same simulation once more with the time step set to
     0.01 s (one named constant) and nothing else changed: the same options otherwise, the same
     makeReproducible(), the same jitter removal, the same call order, the same dump code.
   - Files: tests/data/goldens/<input>/stable/sim_<NN>_<name>.json and
     stable/sim_<NN>_<name>_branch<i>.csv.gz, with the names and the format of the default-step files. The JSON
     says what was changed (in its 'harness' object: the stable time step and the time step the simulation had),
     and 'options.timeStep' shows the step used. manifest.json lists the files per input next to 'simulations'
     (for example 'stableSimulations', entries of the same shape) and records the stable time step in its
     settings. Follow the README's rule on schemaVersion for such an addition.
   - Independence: producing the stable set must not change one byte of what the harness writes today. The
     harness seeds its component ids per input (DeterministicUuids), and every object it creates consumes ids,
     so the stable runs of an input happen in a pass of their own after everything else of that input is
     written (rebuild the rocket or reload the document for that pass as the default pass does, with the ids
     seeded so that the stable files are themselves reproducible). First establish the baseline: regenerate
     with the UNCHANGED harness and confirm 'git status --short tests/data/goldens' is empty (if it is not,
     stop and report what differs: that is a finding about the harness, not something to commit). Then, with
     your change, 'git status' must show only the new stable/ files and manifest.json as modified, and
     'git diff tests/data/goldens/manifest.json' only additions. Regenerate twice and compare: the stable files
     must be byte-identical between two runs.
   - Scope: the 13 testrocket-* inputs are generated and committed. The harness can also write the stable set
     of the 16 example-* inputs (a switch of GoldenDumper and generate.sh, off by default, documented), but those
     files are NOT committed in this tier: generate them once into a scratch directory and report in your notes
     their total size, the number of simulations and rows, and the simulations whose own time step is already
     0.01 s or smaller (for them the rule has to be decided: say what the documents contain).
   - How reproducible is the new set in OpenRocket itself? Dump it a second time with another id sequence (as
     StableGoldens.java's salt does; do not commit that dump) and compare the two with javajava.py: per
     simulation and branch the row counts, the event sequence and times, the summary values and, per column,
     the largest difference as a fraction of the column's scale. This table says what a whole-flight comparison
     can assert, and it goes into your notes and, condensed, into the test's header comment and the README.
   - README.md of the harness: describe the stable set (why it exists, how it is produced, the files, the
     manifest entries, the switch for the examples) where the default-step files are described, and update the
     'Reproducibility of the simulations' section and the file-size remarks.
2. tests/core/goldens/: the loaders and schema tests know the new files (GoldenData's manifest types and
   parser, goldens_schema_tests.cpp: every stable file is listed, every listed file exists and has the schema;
   the rule that every file of an input directory is listed still holds).
3. The whole-flight comparison (tests/core/goldens/simulation_golden_tests.cpp, or a second file next to it if
   that keeps both readable; ctest label 'golden'): every one of the 50 stable simulations is run in QtRocket as
   the harness ran it (the existing set-up of that file with the stable time step) and compared with its stable
   golden over the WHOLE flight:
   - exactly, as the default-step test does for all 50: options, harness block, header, result, branches,
     names, sources, columns, excluded columns, event sequence, event data, warnings;
   - the number of rows of every branch, the summary values, the time of every event, the optimum altitude,
     time and delay, the separation time, the jitter replacement count, the minimum and maximum of every
     column, and every row of every column that carries a physical signal;
   - tolerances from measurements: QtRocket against the stable goldens on glibc and under the six libm shim
     patterns tier 8b used, and OpenRocket against itself (the second dump above). Each tolerance is at least
     100 times the largest of those measurements and never beyond the plan's bounds (event times absolute
     1e-3 s; apogee and maximum velocity relative 1e-4); aim for 1e-6 relative or better for the summary values.
   - What still cannot be compared at such a tolerance (noise-dominated columns, a tumbling branch whose row
     count OpenRocket itself does not reproduce) is neither skipped silently nor hidden behind a wide tolerance:
     the test decides it by a stated rule (the existing two-run sensitivity measurement, or a rule you derive
     from the Java-against-Java table, such as 'the column's largest magnitude is below 1e-6 of ...'), counts
     it, and asserts the sums (compared + excluded == what the golden files hold); the header comment lists
     which columns and branches are excluded, for how many simulations, and why. A simulation that is compared
     only partially must be the exception that the header names, not the rule.
   - Not vacuous: mutation tests as the default-step test has them (a changed golden value late in a flight, a
     changed row count, a removed and a swapped event, a changed summary value: each reported in one line).
   - The default-step comparison stays as it is; its header comment gains a pointer to the stable set.
4. If the stable set reveals a defect in the engine, the steppers or the status (a difference that is not noise:
   it reproduces under the shim and has a first differing row and column), fix it against the Java source with a
   regression test and report it in libraryFixes. Do not adjust the harness or a tolerance to hide it.
Return in 'notes': the sizes (new files, total goldens directory), the run time of the new tests in the dev and
asan presets, the Java-against-Java table, the measured differences and chosen tolerances, what is excluded and
why, and the numbers for the example inputs.`

const VERIFY_OBJ = {
  type: 'object',
  properties: { dev: { type: 'boolean' }, tidy: { type: 'boolean' }, asan: { type: 'boolean' }, gcc: { type: 'boolean' } },
  required: ['dev', 'tidy', 'asan', 'gcc'],
}

const RESULT_SCHEMA = {
  type: 'object',
  properties: {
    worktreePath: { type: 'string' },
    branch: { type: 'string' },
    commit: { type: 'string' },
    files: { type: 'array', items: { type: 'string' } },
    tests: { type: 'array', items: { type: 'string' } },
    deviations: { type: 'array', items: { type: 'string' } },
    libraryFixes: { type: 'array', items: { type: 'string' } },
    verification: VERIFY_OBJ,
    notes: { type: 'string' },
  },
  required: ['worktreePath', 'branch', 'commit', 'files', 'tests', 'deviations', 'libraryFixes', 'verification', 'notes'],
}

const REVIEW_SCHEMA = {
  type: 'object',
  properties: {
    findings: {
      type: 'array',
      items: {
        type: 'object',
        properties: {
          file: { type: 'string' },
          line: { type: 'integer' },
          severity: { type: 'string', enum: ['blocker', 'major', 'minor'] },
          issue: { type: 'string' },
          fix: { type: 'string' },
          evidence: { type: 'string' },
        },
        required: ['file', 'severity', 'issue', 'fix'],
      },
    },
    coverageGaps: { type: 'array', items: { type: 'string' } },
    summary: { type: 'string' },
  },
  required: ['findings', 'coverageGaps', 'summary'],
}

const VERDICT_SCHEMA = {
  type: 'object',
  properties: {
    verdicts: {
      type: 'array',
      items: {
        type: 'object',
        properties: {
          id: { type: 'string' },
          verdict: { type: 'string', enum: ['confirmed', 'modified', 'refuted'] },
          reason: { type: 'string' },
          fix: { type: 'string' },
        },
        required: ['id', 'verdict', 'reason'],
      },
    },
  },
  required: ['verdicts'],
}

const FIX_SCHEMA = {
  type: 'object',
  properties: {
    branch: { type: 'string' },
    commit: { type: 'string' },
    worktreePath: { type: 'string' },
    applied: { type: 'array', items: { type: 'string' } },
    disputed: { type: 'array', items: { type: 'string' } },
    verification: VERIFY_OBJ,
    notes: { type: 'string' },
  },
  required: ['branch', 'commit', 'worktreePath', 'applied', 'disputed', 'verification', 'notes'],
}

// The implementer's result of run wf_4233e5e0-a0c (journal entry of agent a4b01a3503df71b7e).
const IMPL_DONE = {
 "worktreePath": "/home/travis/Development/qtrocket2/.claude/worktrees/wf_4233e5e0-a0c-1",
 "branch": "worktree-wf_4233e5e0-a0c-1",
 "commit": "dc2ff43",
 "files": [
  "tools/openrocket-goldens/src/main/java/info/qtrocket/goldens/GoldenDumper.java",
  "tools/openrocket-goldens/src/main/java/info/qtrocket/goldens/SimulationDumper.java",
  "tools/openrocket-goldens/src/main/java/info/qtrocket/goldens/Json.java",
  "tools/openrocket-goldens/generate.sh",
  "tools/openrocket-goldens/README.md",
  "tests/core/goldens/GoldenData.h",
  "tests/core/goldens/GoldenData.cpp",
  "tests/core/goldens/goldens_schema_tests.cpp",
  "tests/core/goldens/simulation_golden_tests.cpp",
  "tests/data/goldens/manifest.json",
  "tests/data/goldens/testrocket-*/stable/sim_<NN>_<name>.json and sim_<NN>_<name>_branch<i>.csv.gz (new: 103 files in the 13 testrocket directories, 50 simulations, 53 branches)"
 ],
 "tests": [
  "SimulationStableGolden.EverySimulationOfTheRocketOverItsWholeFlight (Makers/..., 13 rockets: the whole-flight comparison of the 50 stable simulations)",
  "SimulationStableGoldenCoverage.EverySimulationOfTheStableStepSetHasAMaker (pins what the files hold and what the rules decide from the files alone)",
  "SimulationStableGoldenMutation.EveryChangeIsReportedInOneLine and .TheFilesAsTheyAreAndChangesWithinTheTolerancesMatch (Changes/..., 4 simulations, 38 mutations)",
  "SimulationStableGoldenMutations.EveryMutationBelongsToASimulationOfTheTests, .AValueTheRulesExcludeIsCountedNotCompared, .ATumblingStageIsComparedUpToItsSeparation",
  "SimulationStableGoldenRules.EveryToleranceIsAHundredTimesTheLargestMeasurement, .AHuntingRowIsARungeKuttaRowInFreeFlightWithoutPitchAndYawRate, .TheApogeeRowIsWhereAnEulerStepperLandsOnTheApogee, .AnOutOfPlaneColumnIsNoiseBelowAHundredthOfItsCounterpart, .ThePlanOfABranchFollowsItsEvents",
  "SimulationStableGoldenHarness.TheStableTimeStepIsSetLastAndRecorded",
  "SimulationStableGoldenMeasurement.DISABLED_PrintsTheDifferencesFromTheGoldenFiles and .DISABLED_PrintsTheSensitivityToAKick (measurement tables, disabled)",
  "GoldenSchema.ManifestListsTheStableStepSetOfTheTestRockets",
  "GoldenInputSchema.StableSimulationsMatchTheirTimeSeriesAndTheirDefaultStepSimulations (Goldens/..., one per input)",
  "GoldenData.ParsesManifest, .RejectsManifestsWithoutARequiredField, .RejectsManifestsWithWrongTypes (extended for settings.stableTimeStep, settings.stableSimulationsOf and stableSimulations)",
  "GoldenSchema.EveryFileOfAnInputIsListed (now covers the stable files through listedFiles())"
 ],
 "deviations": [
  "Manifest: an input without a stable-step set (the 16 examples) has no 'stableSimulations' key instead of an empty list, so its lines are untouched. The textual diff of manifest.json is 77 added lines and 1 replaced line: the one-line 'settings' object, which gained 'stableTimeStep' and 'stableSimulationsOf'. 'Only additions' was checked at JSON level (manifest_additions.py: the old manifest value by value, in the same key order, plus the new keys).",
  "Harness additions the task did not name: UUID_SALT (--uuid-salt, the second id sequence, refused without GOLDENS_OUT), GOLDENS_OUT (scratch output directory), settings.stableSimulationsOf (which input kinds have the set) and settings.uuidSalt (salted dumps only). They were needed for the second dump and for the examples' scratch dump, and are documented in the README.",
  "The stable pass does not use a seed of its own: it reseeds with the SAME seed as the default pass and repeats that pass call by call (re-save, settling, geometry/mass/aero dumps computed again), and it verifies byte for byte that it would write the same design files (GoldenDumper.writeOrVerify, fails otherwise). So a stable simulation has the same component and configuration ids as its default-step twin; the schema test checks that the two documents differ in nothing but the time step, the two harness keys and the results.",
  "OpenRocket against itself was measured with four more id sequences (ten pairs of five dumps), not one. That changed the picture of tier 8b: 4 branches (not 1) have other row counts, and the cause is not amplified rounding but four decisions of OpenRocket's algorithm.",
  "The comparison stayed in tests/core/goldens/simulation_golden_tests.cpp (now 5972 lines) instead of a second file: it reuses the run set-up, the structural comparison (compareSimulation() with nothing taken as reproducible) and the mutation helpers, which live in that file's anonymous namespace; a second file needs them moved into a shared header first. Not done here; worth a follow-up.",
  "Existing code of the default-step test was touched, behaviour unchanged: runSimulation()/simulate() take a RunVariation (stable time step, perturbation listener) instead of a bool, compareHarness() takes the list of keys, ComparisonMode gained stableSet. No assertion was changed; the default-step tests pass as before on glibc and under the six shim patterns.",
  "Rule E is not decided from the golden file alone: it looks at the step the run and the golden run take from the apogee row (extra 1 ms step or not). The number of compared rows therefore depends on the platform (34542 on glibc, 33572 to 34542 under the shim patterns); the test asserts the sums and a floor that the files alone fix (26862 rows, 1662119 values), and the coverage test pins that floor. The minimum/maximum rule is symmetric in the same way (an extreme attained only in excluded rows, in either run, is excluded).",
  "Tolerances are not one number: 1e-9 of the column scale on the launch rod, a per-column table off it (kMeasuredColumns, 61 columns, 1e-9 to 1e-2, each 100 times its largest measurement rounded up to a power of ten). Summary values are measured against the scale of their column, not against themselves. The deployment velocity has 1e-5 (not 1e-6): its largest measurement is 1.4e-8 of the velocity scale, which 1e-6 would cover only 73 times.",
  "The 1e-3 s tolerance of events after a late handling is the plan's bound, chosen for the mechanism (a step may overshoot an event by up to the engine's 1 ms floor), not derived from a measurement: measured there is 1.8e-10 s (1.5e-8 s under the kick). All other times use 1e-4 s (125 times the largest, 8.0e-7 s).",
  "For a simpler rule, flightTime and groundHitVelocity are excluded together when the first branch is not compared to its last row, although the ground hit velocity alone differs by only 1.7e-12 there. Likewise the noise-dominated out-of-plane columns are compared on the rod only in every planar branch, also in the 4 branches without hunting rows, where they would match (3e-15).",
  "The committed sensitivity test (DISABLED_PrintsTheSensitivityToAKick) runs one kicked run per simulation; the numbers quoted in the header are from three such runs of a probe dumper that is not committed (its dumps are deleted; tables kept).",
  "The examples' stable-step set applies 0.01 s to every simulation unconditionally. No rule was needed for 'own step already 0.01 s or smaller' because no document has such a simulation (see notes); the README says it is undecided.",
  "The commit message carries the session trailer line after the requested subject. After ci-gcc failed once (-Werror=null-dereference in the new goldenBranchOf(), a test helper), all four presets were rerun on the fixed tree: the final logs are verify-dev-2, verify-tidy-2, verify-asan-2 and verify-gcc-3."
 ],
 "libraryFixes": [],
 "verification": {
  "dev": true,
  "tidy": true,
  "asan": true,
  "gcc": true
 },
 "notes": "RESULT. The stable-step set (0.01 s) is generated, committed for the 13 test rockets and compared over whole flights; all four presets pass on the final tree (4656 tests, no warning), and the 131 simulation-golden and schema tests also pass under the six libm shim patterns. No defect of the engine, the steppers or the status was found: QtRocket against the stable goldens differs in the same places by the same amounts as OpenRocket against itself.\n\nMAIN FINDING (differs from tier 8b's probe). At 0.01 s OpenRocket still does not reproduce itself, and not because of amplified rounding. Four decisions of its algorithm fall by the last bit, each visible in the golden rows, each with a rule in the test:\n- H, hunting: calculateFlightConditions() zeroes the pitch and yaw rate while the lateral airspeed is below 1 mm/s; a weathercocked rocket reaches that after about 1 s and stays about 2 s, undamped. Rows: Runge-Kutta, free flight, pitch and yaw rate exactly 0 (5552 rows in 30 of the 34 branches that leave the rod). The 21 attitude columns are not compared in those rows (105742 values); they are everywhere else.\n- N, noise-dominated out-of-plane columns (yaw rate, lateral acceleration and position across the wind, their direction; roll rate in two branches): scale below 1e-2 of the in-plane counterpart (measured 2e-6 to 3e-4; 0.2 to 2 in the one flight that leaves its plane). Compared on the launch rod only: 167 columns, 153992 values.\n- E, the Euler stepper's step onto the apogee leaves a vertical velocity of about 1e-17 m/s; its sign decides whether an extra 1 ms step follows. 19 branches (recovery device out before apogee); 12 of 190 Java pairs differ. When run and golden step differently from that row, the rows, events, row count, flight time and ground hit velocity from there on are excluded. On glibc: 1 branch (Estes Alpha III [A8-0], 239 rows; the committed file has 668 rows, the four other Java dumps and QtRocket 667); under the shims 1 to 3 branches (up to 1209 rows).\n- T, a stage that tumbles before its apogee (Beta booster, second stage of the multi-stage rocket): compared up to its separation; 542 rows, its later events, row count, optimum values and the simulation's jitter replacement count excluded.\nOn glibc 34542 of 35323 rows, 2161319 of 2471514 values (87 %) and 6236 of 6920 numbers are compared; every simulation is compared first row to last except those three branches. Floor on every platform: 26862 rows, 1662119 values.\n\nSIZES. New: 103 files, 13.4 MB (JSON 0.58 MB, time series 12.8 MB). tests/data/goldens: 35.0 MB -> 48.4 MB (447 files). No existing file changed; baseline regeneration with the unchanged harness left git status clean; two regenerations with the change are byte-identical; with STABLE_EXAMPLES=1 every committed file is still byte-identical (only manifest.json differs).\n\nRUN TIME of the 61 new tests (sum of per-test times; ctest runs 8 in parallel): dev 48.5 s, longest 5.9 s (the 13 whole-flight tests 26.2 s; the 13 default-step tests 23.0 s in the same run). asan 273.6 s, longest 29.9 s (whole-flight 152.3 s; default-step 126.3 s).\n\nJAVA AGAINST JAVA, stable set, five id sequences, ten pairs, no rules applied:\n- structure (status, branches, warnings, event sequences and data): identical.\n- row counts differ in 4 of 53 branches: multi-stage second stage 563/568/571/571/575; one row in Alpha III [A8-0], motor pods [2xA10-0; B4-3], inline pod [A8-0].\n- jitter replacements differ only in the multi-stage simulation (6444 to 6548).\n- max altitude 4.9e-9, max velocity 5.6e-9, max acceleration 1.0e-9, max Mach 4.4e-10 (relative); time to apogee 3.2e-8 s; optimum delay 5.0e-11 s; flight time 1.3e-4 s (8.0e-7 s where the apogee steps agree); deployment velocity 4.7e-7 of itself (1.3e-8 of the velocity scale).\n- event times: motor- and rod-timed events 0 (1.4e-10 s after a late handling); APOGEE 2.1e-8 s, TUMBLE 9.1e-8 s, GROUND_HIT 8.0e-7 s; in the tumbling stage 5.6e-3, 7.5e-3 and 1.5e-2 s.\n- time series, fraction of column scale: on the rod 2.7e-13; hunting rows up to 8.7e-4 (body lateral acceleration), 4.4e-4 (lateral acceleration), 4.0e-5 (pitch moment); out-of-plane columns up to 1.3 times the scale; elsewhere altitude 4.9e-9, velocity 2.3e-8, angle of attack 4.8e-7, pitch rate 2.7e-6, pitch damping moment 8.7e-6.\n\nMEASURED AND CHOSEN (over what the rules compare; Java ten pairs / QtRocket on glibc and six shim patterns):\n- rod rows: 2.7e-13 / 1.6e-13 -> 1e-9.\n- off the rod, per column (61 columns): e.g. altitude 4.9e-9 / 3.8e-9 -> 1e-6; velocity 2.3e-8 -> 1e-5; angle of attack 4.8e-7 -> 1e-4; pitch rate 2.7e-6, pitch damping 8.9e-6 -> 1e-3; body lateral acceleration 8.7e-7 / 1.2e-5 -> 1e-2 (chute shock of 5800 m/s^2 times the attitude noise, on a column whose scale is 10 m/s^2).\n- summary: max velocity 7.0e-9, altitude 4.9e-9, acceleration 1.4e-9, Mach 5.8e-10, optimum altitude 5.1e-9, warning parameter 2.2e-9 -> 1e-6; launch rod velocity 3.4e-17 -> 1e-9; deployment velocity 1.4e-8 -> 1e-5.\n- times: 8.0e-7 s -> 1e-4 s; after a late handling 1.8e-10 s -> 1e-3 s (3 branches: BURNOUT handled 0.345 ms and 47 microseconds late).\n- Largest compared difference on the final tree: 6.5e-3 to 8.8e-3 of its tolerance across glibc and the six patterns.\nSensitivity to a relative 1e-10 kick per step (three runs): no event sequence changes; row counts change only where E or T allow (apogee step in 4 of 57 runs); motor- and rod-timed events move by 0, APOGEE 3.1e-8 s, TUMBLE 8.6e-8 s, GROUND_HIT 3.7e-7 s, events after a late handling 1.5e-8 s; summary values at most 1.2e-7; altitude 4.9e-7 of scale. Threshold margins in the golden rows: LIFTOFF 9.9e-5 m, LAUNCHROD 6.6e-5 m, APOGEE under Runge-Kutta at least 3.5e-5 m against 7.5e-10 m between runs; steps stretched to events at least 0.32 ms from not being stretched, against about 1e-7 s.\n\nEXAMPLE INPUTS (STABLE_EXAMPLES=1, scratch only, not committed, dump deleted): 54 simulations, all ran (none skipped), 69 branches, 92286 rows (their default-step files: 46346), 35.8 MiB (JSON 0.9, time series 34.9). The documents' own time steps: 0.05 s in 49 simulations, 0.04 s in the 5 of 'Pods--airframes and winglets'. None is 0.01 s or smaller, so the rule for such simulations has nothing to decide yet; the harness sets 0.01 s unconditionally.\n\nOPEN POINTS. (1) MSVC and macOS were not run; the coverage pins and mutation expectations are platform-independent by construction, and rule E absorbs the apogee step, but that is argued from the kick and margin measurements, not observed. (2) The test file is 5972 lines; splitting needs the shared machinery moved to a header first.\n\nProbes, tables and logs: /tmp/claude-1000/-home-travis-Development-qtrocket2/87f548e1-ea1f-4253-ad87-f65f7c95546e/scratchpad/probes/tier8c-implementer/ (README.txt; 17 MB; dumps and copies deleted; tables/rules-merged.txt is the measurement behind the tolerances, logs/verify-*-2.log and verify-gcc-3.log the final presets)."
}

// The comparison review of run wf_4233e5e0-a0c (journal entry of agent a46f46752b1725852).
const COMPARISON_DONE = {
 "findings": [
  {
   "file": "tests/core/goldens/simulation_golden_tests.cpp",
   "line": 3935,
   "severity": "major",
   "issue": "Rule E is decided by the run under test and nothing checks that the difference really is the last-bit one. stableExtent()/stepsAlikeFromTheApogee() only look at whether the run's step from the golden apogee row is <= 1.5 ms; when it differs from the golden step, every row from the apogee row on, the row count, the later events, flightTime and groundHitVelocity are excluded, and the test (line 4888) only asserts the floor (26862 of 35323 rows). A stepper defect in exactly this place therefore removes the descent of up to 19 flights from the comparison without any test failing. A library mutation survives (see evidence).",
   "fix": "Verify the mechanism before excluding. In the 209 branch runs I measured (19 golden branches, 190 QtRocket runs) the step from the apogee row is short exactly when the velocity left is positive, and the velocity left is at most 1.7e-16 of the vertical velocity of the row before. So in the mismatch case require of the run's own rows: (a) |velocity_z[apogeeRow]| <= a few DBL_EPSILON * |velocity_z[apogeeRow - 1]| (not kApogeeVelocity = 1e-9, which the mutant passes), and (b) (velocity_z[apogeeRow] > 0) == (the run's step is short); report a mismatch otherwise. Also keep groundHitVelocity compared in the mismatch case (it does not depend on the time grid: measured 1.7e-12 of the velocity scale; the implementer's deviation list admits this), and correct the header: the velocity left is exactly 0 in about 90 % of the runs, +-1 ulp otherwise, not 'about 1e-17 m/s of either sign'.",
   "evidence": "Mutant 'apogee-step-short-1e-12' (src/simulation/AbstractEulerStepper.cpp: t = std::abs(v / a) * (1.0 - 1e-12)), built out of tree and run with --gtest_filter='*Simulation*Golden*': 0 of 61 tests fail. The measurement test then lists 18 branches under rule E and compares 27101 rows instead of 34542 (5836 numbers instead of 6236). Combined mutant (same change plus the Euler steppers' drag 2 % up while descending): the whole-flight tests of testrocket-beta, testrocket-multi-stage-event-test-rocket and testrocket-estes-alpha-iii-with-second-motor still pass, although flight times are off by about 0.8 s and ground hit velocities by 0.7 %; the four other Alpha III rockets fail only through their C6-5/C6-7 flights, whose apogee is under a Runge-Kutta stepper. Across 47 unmutated runs (glibc, 46 one-ulp patterns) rule E excludes 1 to 4 branches and the compared rows vary from 33538 to 34618. Tables: tier8c-review-comparison/tables/mutants-summary.txt, mutant-apogee-step-short-*-measure.txt, apogee-rows.txt."
  },
  {
   "file": "tests/core/goldens/simulation_golden_tests.cpp",
   "line": 3895,
   "severity": "major",
   "issue": "The hunting decision itself (lateral airspeed below 1 mm/s) is a threshold the comparison depends on with a margin of 2.9, not 100, and the header's list of thresholds (lines 296-309: LIFTOFF, LAUNCHROD, APOGEE, stretched steps) does not mention it. Rule H takes the hunting rows from the golden file alone. pitch_rate is not an attitude column, so it is compared in every row at 1e-3 of its scale; in a hunting row it is stored as exactly 0, in the next row as the real rate (1.2 % to 5.6 % of the scale). If the decision of one row falls the other way in the run, that cell is off by 12 to 56 times its tolerance and the test fails. The sentence at line 293 ('what is sensitive ... is excluded by a rule, not by a tolerance') does not hold for this decision.",
   "fix": "Make rule H symmetric at its edges: take the run's hunting state too (Runge-Kutta row off the rod, pitch and yaw rate exactly 0); in a row where the two runs disagree, exclude and count the columns the decision switches (pitch_rate, yaw_rate, pitch_damping_moment_coeff, the attitude columns), and allow such a row only where the golden lateral airspeed (mach_number * speed_of_sound * sin(aoa)) is within a stated band of 1 mm/s that is 100 times the measured difference (1.2e-3 mm/s measured, so about 12 %); a disagreement outside the band stays a mismatch. Add the measured margin of this threshold to the header's list.",
   "evidence": "In 25 of the 30 hunting branches the hunting ends by the lateral airspeed rising through 1 mm/s at about 0.017 mm/s per row; the nearer of the two rows lies 0.0020 to 0.0080 mm/s from the threshold. Over 65 runs (glibc, 64 one-ulp libm patterns, probe test on a private build) QtRocket differs from the golden run in those rows by up to 1.23e-3 mm/s: margin 2.9 (motor-pods sim_03/sim_05, row 381: golden 0.99641 mm/s), 3.0 (sim_04), 6.2 (motor-pods sim_02), 6.7 (pods sim_03..05), 73 (second-motor sim_03..05), 89 (inline-pod sim_03..05); 13 compared branches are below 100. Inside the hunting the smallest margin of any row is 3.2. No row flipped in any of the 65 runs, and the committed tests pass under all 46 patterns I ran them with, so this is a margin finding, not an observed failure. The failure signature was produced by moving the threshold 0.5 % (mutant 'hunt-threshold-0.5pct'): 'column pitch_rate: 1 of 1337 rows differ, the first at row 461: expected 0.0281, got 0', one such line per affected flight, three whole-flight tests fail (11 with -0.5 %). Tables: tables/hunting-edges-65runs.txt, hunting-margin.txt, mutants-summary.txt."
  },
  {
   "file": "tests/core/goldens/simulation_golden_tests.cpp",
   "line": 3634,
   "severity": "major",
   "issue": "Rule H excludes all 21 'attitude' columns in the 5552 hunting rows (105742 values, plus every minimum or maximum attained only there), but for 13 of them the hunting rows hold signal, not noise, and the measured differences would allow a comparison at 1e-4 to 1e-2 of the scale with the margin of 100. Only aoa, normal_force_coeff and pitch_moment_coeff are noise there (median difference 7e-4 of the value, up to 9 times the value); acceleration_x, acceleration_xy and acceleration_bodyx would need 1e-1. As written, the rocket's pitch angle (orientation_theta), CP location, stability margin, CNa, the stability derivatives, the vertical and total acceleration and the lateral velocity are not compared for about two seconds of the coast of all 30 flights; for example the minimum of acceleration_z (peak deceleration after burnout) is among the excluded extremes of every flight. The header (lines 181-186) quotes 8.7e-4, which is acceleration_bodyx alone.",
   "fix": "Give the hunting rows their own measured tolerance per column instead of a blanket exclusion (a second tolerance in kMeasuredColumns: 100 times the largest hunting-row difference, rounded up), and keep excluding only the columns for which that is not below the size of the values there (aoa, normal_force_coeff, pitch_moment_coeff, acceleration_x, acceleration_xy, acceleration_bodyx). acceleration_y and acceleration_bodyy of the multi-level-wind flight need no exclusion at all: their hunting-row differences are within 1/100 of the tolerance they already have. Update the counts, the coverage pins and the header.",
   "evidence": "Largest hunting-row difference as a fraction of the column scale, QtRocket against the goldens over 47 runs (the implementer's own Java-against-Java column 'hunt J' in tables/rules-merged.txt agrees within a factor 1.6): cp_location 4.0e-7, stability 4.3e-7, damping_ratio 9.4e-7 (tolerance 1e-4 would do); cna 1.1e-6, orientation_theta 1.6e-6, damping_moment_coeff 1.9e-6, damping_moment_coeff_aerodynamic 2.1e-6, natural_frequency 2.6e-6, acceleration_z 4.2e-6, corrective_moment_coeff 5.0e-6, orientation_phi 8.2e-6 (1e-3); velocity_xy 1.6e-5, acceleration_total 1.7e-5 (1e-2); acceleration_y 9.1e-8 and acceleration_bodyy 3.3e-8 in the one flight where they are compared (existing tolerance 1e-5). Size of the values in the hunting rows (median magnitude / scale): orientation_theta 0.95, cp_location 1.0, cna 0.81, corrective_moment_coeff 0.65, velocity_xy 0.64, acceleration_z 0.10, acceleration_total 0.10, stability 0.084. On glibc 660 of the 5652 minima and maxima are excluded (18 to 20 per flight). Tables: tables/hunting-signal.txt, rate-merged.txt, coverage-glibc.txt."
  },
  {
   "file": "tests/core/goldens/simulation_golden_tests.cpp",
   "line": 3681,
   "severity": "major",
   "issue": "Stepper mutations of one part in 1e7 survive. Off the rod every value is compared at one tolerance per column, taken from the column's noisiest phase (Runge-Kutta rows after the hunting), also in the rows of the Euler steppers, where the measured differences are zero or orders of magnitude smaller; and groundHitVelocity (line 4143) is compared at 1e-6 of the velocity scale, 2.9e-5 of itself, where 1.7e-12 of the scale is measured. So the landing and tumbling descent, which only this set compares over whole flights, is checked far more loosely than the measurements allow.",
   "fix": "Split the tolerance table by phase: for the rows of an Euler stepper (aoa is NaN) use the differences measured in those rows; the columns that are exact there in every measurement (drag_coeff, pressure_drag_coeff, base_drag_coeff, friction_drag_coeff, axial_drag_coeff, mass, thrust columns) get kValueRelative. Compare groundHitVelocity at about 1e-8 of itself (measured: 3.9e-11 of itself between Java runs, 1.7e-12 of the velocity scale for QtRocket). That makes the tumble-Cd and Euler-drag mutants visible. The tentative step, the minimum step and the RK6 weight at 1e-7 stay below 100 times the noise; the header should say so instead of leaving it unstated.",
   "evidence": "Out-of-tree library mutants, '*Simulation*Golden*' (61 tests of both sets): tumble Cd * (1+1e-7): 0 failures (first noticed at 1e-6); Euler steppers' drag * (1+1e-7): 1 failure, only the multi-level-wind flight through its 1e-9 columns (at 1e-6 still only that one; 15 failures at 1e-5); Euler tentative step 0.5 s * (1+1e-7): 0 failures (also 0 at 1e-6 and 1e-5; noticed at 1e-4); Euler minimum step: 0 at 1e-7 and 1e-5; RK6 stage weight: 0 at 1e-7 (noticed at 1e-6). Noticed at 1e-7: landing Cd (by 1e-9 of the tolerance of pressure_drag_coeff), pitch moment, axial drag, normal force, gravity, an RK4 weight. On-disk golden mutations in a copy of tests/data/goldens (set B) that all 31 stable tests accept: velocity_z and velocity_total of rows 900..1333 of estes-alpha-iii sim_04 (the whole parachute descent) * (1+1e-4); summary.groundHitVelocity * (1+1e-5); drag_coeff of the parachute rows * (1+5e-7). Measured in Euler rows (QtRocket against goldens, 7 runs; the implementer's javajava-stable-phases.txt has 0.0 for the same columns): drag_coeff, pressure_drag_coeff, mass 0; velocity_z after the first second 1.8e-10 (tolerance 1e-5); acceleration_z 2.6e-10 (1e-5). Set A of on-disk mutations (late altitude, summary value, row count, event time, removed row, removed event, descent velocity 1e-3) was reported line by line. Tables: tables/mutants-summary.txt, mutants-thresholds.txt, phase-diffs.txt, gold/*-notes.txt, logs/gold-A.log, gold-B.log."
  },
  {
   "file": "tests/core/goldens/simulation_golden_tests.cpp",
   "line": 191,
   "severity": "major",
   "issue": "Wrong statement in the header, and an exclusion it is used to justify. 'two runs of OpenRocket differ by more than the column's scale' is given for all out-of-plane columns of rule N; under the rules it is true for acceleration_y only. The roll rate (line 3668) is reproducible to 1e-6 of its own scale and is nevertheless treated as noise in the multi-level-wind flight (the one flight that leaves its plane) and in the multi-stage second stage: 992 values that could be compared.",
   "fix": "State the measured numbers per column in the header. Take roll_rate out of kOutOfPlaneColumns, or compare it at a measured tolerance (1e-3 of its scale covers 1.04e-6 with the margin of 100). For yaw_rate, position_y and position_direction say that the exclusion is because 100 times their differences (0.18, 0.18, 0.013 of the scale) is no meaningful tolerance, not because the columns are scattered beyond their scale.",
   "evidence": "Largest difference in the rows rule N excludes, as a fraction of the column's own scale. The implementer's tables/rules-merged.txt, column 'noise J' (OpenRocket against itself): acceleration_y 1.3, acceleration_bodyy 0.47, yaw_rate 1.2e-3, position_y 1.2e-3, position_direction 1.4e-4, roll_rate 7.8e-7. QtRocket against the goldens over 47 runs: 2.3, 3.1, 1.8e-3, 1.75e-3, 1.3e-4, 1.04e-6. The figures above 1 for yaw_rate and position_y in javajava-stable.txt come from the Beta booster after its separation, which rule T excludes anyway."
  },
  {
   "file": "tools/openrocket-goldens/README.md",
   "line": 419,
   "severity": "major",
   "issue": "The same wrong statement in the README's table of OpenRocket against itself: 'out-of-plane columns of a planar flight | more than the column's scale'. For the planar flights this holds for the lateral acceleration across the wind only; line 431 then names the yaw rate and the position across the wind as scattered by the hunting.",
   "fix": "Replace the row by the measured values per column (acceleration_y 1.3, acceleration_bodyy 0.47, yaw rate 1.2e-3, position across the wind 1.2e-3, its direction 1.4e-4, roll rate 7.8e-7 of the column's scale) and say that the figures above 1 for the yaw rate and the position belong to the tumbling Beta booster.",
   "evidence": "tier8c-implementer/tables/rules-merged.txt ('noise J') and javajava-stable-detail.tsv (beta sim_02 branch 1: yaw_rate 2.63, position_y 1.39); my 47 QtRocket runs give the same picture (tables/rate-merged.txt, final-merged.txt)."
  },
  {
   "file": "tests/core/goldens/simulation_golden_tests.cpp",
   "line": 4446,
   "severity": "minor",
   "issue": "compareStableSeries() compares min(extent.rows, values->size()) rows and says nothing when the run's branch is shorter than the rows that are to be compared. For a branch that is not compared to its last row (rule T, rule E mismatch) the row count is not compared either, so the missing rows are counted as compared (comparedValues() counts from the plan, which makes 'compared + excluded == golden' hold by construction there).",
   "fix": "Report a mismatch when series.values->size() < ours.extent.rows (once per branch, for example in compareStableColumns()), so that the counted rows are rows that were looked at.",
   "evidence": "Code reading (lines 4442-4472, 4488-4493, 3922-3931: stepsAlikeFromTheApogee() returns false for a run that ends before the apogee row, which selects the exclusion). Not reproduced with a mutant: a run that short also changes the event sequence in every case I could construct, which the structural comparison reports."
  }
 ],
 "coverageGaps": [
  "MSVC and Apple Clang/libc++ were not built or run (no toolchain here). In particular std::ranges::count / std::ranges::find over std::vector<bool> (also on a prvalue, in the SimulationStableGoldenRules tests) is verified with libstdc++ only; libc++ is not installed on this machine.",
  "The dev, tidy, asan and ci-gcc presets were not rerun (read-only on the worktree). I relied on the implementer's logs verify-dev-2, verify-tidy-2, verify-asan-2, verify-gcc-3: 4656 tests pass, 0 warnings, all built after the last source change at 19:40. Run times from those logs: the 31 SimulationStableGolden* tests 41.0 s in Debug (longest 5.9 s) and 231.9 s under ASan (longest 29.9 s); the 13 whole-flight tests 26.2 s and 152.3 s, against 23.0 s and 126.3 s for the 13 default-step tests.",
  "The implementer's shim runs (tier8c-implementer/run-tests.sh, filter 'Simulation*Golden*:...') did not include any parameterised test: 'Makers/SimulationStableGolden.*' (the 13 whole-flight tests), 'Makers/SimulationGolden.*' and 'Changes/*' do not match, which is why the notes say 131 tests. I ran them with '*Simulation*Golden*:*Golden*Schema*:GoldenData.*': 171 tests pass on glibc and under each of the six tier-8b patterns, and the 31 stable-step tests pass under 40 further one-ulp patterns (ULP_RATE 2..41).",
  "The Java dumps behind the Java-against-Java numbers were deleted by the implementer; I took those numbers from the implementer's tables and re-measured only the QtRocket side (47 runs for the measurement test, 65 for the hunting edges, 10 full dumps). Over 47 runs every tolerance keeps a margin of at least 100; the smallest are 106 (summary maxVelocity, 9.45e-9 against 1e-6), 114 (pitch_damping_moment_coeff, thrust_weight_ratio), 119 (acceleration_bodyz).",
  "Rule T: I first suspected the exclusion of the Beta booster's row count was not required (631 rows in all five Java dumps and in my first ten runs), but 6 of 47 runs have 632 rows, so the exclusion is justified. Whether the jitter replacement count of that simulation (8620 in the golden file and in all 10 dumps) also varies was not measured in the other 37 runs.",
  "The thresholds of the tumble detector (filtered angle of attack, dynamic pressure floor) were not analysed for their margin; the TUMBLE times of the 6 compared flights differ by at most 1.3e-7 s in 47 runs.",
  "The harness (Java), the generated files, the manifest and the schema tests belong to the other lens; I only checked that the harness applies the stable time step after makeReproducible() and before the run, as runSimulation() does.",
  "Side effect of this review: two 'ctest -N' listings in the worktree rewrote build/clang-debug/Testing/Temporary/LastTest.log (ctest's scratch log, now 121 bytes). Nothing else under the worktree changed; git status is clean; no cmake was run there."
 ],
 "summary": "No blocker. The whole-flight comparison is set up as the harness runs it, shares the structural comparison with the default-step test (whose assertions are unchanged), and passes on glibc and under every one-ulp libm pattern I tried: 171 golden and schema tests under the six tier-8b patterns, the 31 stable-step tests under 40 more. Every tolerance keeps a margin of at least 100 over 47 runs (smallest 106), and all are within the plan's bounds. Seven on-disk golden mutations (late altitude, summary value, row count, event time, removed row, removed event, descent velocity) were each reported in one line. Tests are order-independent (three shuffles, a repeat), use no wall-clock time, and keep only per-process caches.\n\nCoverage on glibc: 47 of 50 simulations are compared to the last row of every branch; 34542 of 35323 rows, 2161319 of 2471514 values, 6236 of 6920 numbers. The 684 excluded numbers are 660 minima/maxima, 11 event times, 3 row counts, 2 jitter counts, 6 optimum values, 1 flight time and 1 ground hit velocity. The per-simulation table is in tier8c-review-comparison/tables/coverage-glibc.txt.\n\nSix major findings and one minor:\n1. Rule E trusts the run under test. Shortening the Euler stepper's step to the apogee by one part in 1e12 takes 18 descents (7441 rows) out of the comparison and no test fails; with a 2 % descent-drag error on top, three rockets' whole-flight tests still pass. A check on the run's own apogee row closes it.\n2. The hunting threshold (1 mm/s) is a discrete decision with a margin of 2.9, not 100, over 65 runs. One flipped row puts pitch_rate 12 to 56 times beyond its tolerance. It did not flip in any run; this is the one place where another platform could fail.\n3. Rule H excludes 21 columns in the hunting rows, but 13 of them are signal there and reproducible to 4e-7 to 1.7e-5 of their scale.\n4. Stepper changes of 1e-7 survive for the tumble Cd, the Euler drag (30 of 31 flights), the Euler tentative and minimum step and an RK6 weight. Tolerances are per column for all phases although the Euler rows are exact or 1e-10, and the ground hit velocity is compared a million times more loosely than measured.\n5. The header and the README say the out-of-plane columns differ by more than their scale; that holds for one of six, and the roll rate is excluded although reproducible to 1e-6.\nMinor: rows are counted as compared when the run's branch is shorter than the compared extent.\n\nProcess note: the implementer's shim verification never ran the 13 whole-flight tests (the gtest filter missed the parameterised names); I ran them and they pass.\n\nProbes, tables and logs: /tmp/claude-1000/-home-travis-Development-qtrocket2/87f548e1-ea1f-4253-ad87-f65f7c95546e/scratchpad/probes/tier8c-review-comparison/ (README.txt; 8 MB; binaries, dumps and golden copies deleted)."
}

const LENSES = [
  {
    key: 'harness',
    text: `Lens: THE HARNESS AND THE DATA.
1. Independence and determinism, proved by running: in a scratch copy (git worktree add under
   ${PROBES}/tier8c-review-harness/, or the harness's --out pointed at a scratch directory; never write into the
   worktree under review) regenerate the goldens with the changed harness and compare byte for byte with the
   committed tests/data/goldens: every file that existed before this tier ('git diff --stat ${BASE}...HEAD --
   tests/data/goldens' names what changed) and every new stable file. Regenerate a second time and compare
   again. Any difference is a blocker.
2. The stable runs against the default-step runs in the Java code: the same options but for the time step, the
   same makeReproducible(), the same listeners in the same order, the same dump code; the id seeding of the
   stable pass; what happens to a skipped simulation, a simulation that throws, an input given with --only; the
   switch for the examples and its default.
3. The files: JSON fields (the harness object says what changed; options.timeStep), names, the manifest entries
   and settings, schemaVersion, README.md (accurate and complete for the new set; nothing stale about the old
   one), .gitattributes treatment of the new .gz files, the size of the addition.
4. The loaders and schema tests in tests/core/goldens: every new file is checked as the old ones are; the
   manifest parser rejects a malformed stable entry; the 'every file is listed' rule covers the stable/
   directories in both directions.
5. The Java-against-Java table the implementer reports: reproduce a sample of it with a dump of your own
   (another id sequence) and check that the header comment and the README state it correctly.

RESUME NOTE (2026-10-07). An earlier attempt at this review was interrupted before it reported anything; it wrote
no README.txt and recorded no finding. What it left under ${PROBES}/tier8c-review-harness/:
- wt/: a detached scratch worktree of the work at dc2ff43, registered with git and clean. Reuse it, do not add
  another one.
- logs/generate-1.log and logs/status-after-1.txt: a regeneration inside wt/; the status file (empty) is its
  'git status --short tests/data/goldens'.
- out-2/ and logs/generate-out-2.log: a regeneration through GOLDENS_OUT into an empty directory, ended with
  exit 0. Its first 'diff -rq' against the committed goldens printed one line: motors.json exists only in the
  committed directory. Find out whether that is what the harness documents for GOLDENS_OUT or a finding.
- out-examples/ and logs/generate-out-examples.log: the examples' stable set, CUT OFF after 12 of the 16
  examples. Incomplete: delete it and run it again if you need it.
- base-goldens/: tests/data/goldens of ${BASE}, from 'git archive'.
Treat all of it as unchecked material: confirm by a comparison of your own that each directory is what this note
says before you rely on it, redo what is incomplete, and carry out every point of the lens. Write the README.txt
and clean up as the rules ask.`,
  },
  {
    key: 'comparison',
    text: `Lens: THE WHOLE-FLIGHT COMPARISON.
1. Set-up: the 50 stable simulations are run as the harness ran them (options, order, jitter removal, the stable
   time step and nothing else changed); the comparison code is shared with the default-step test where it is
   the same.
2. Coverage: for each of the 50 simulations, what is compared over the whole flight and what is excluded. Run
   the test binary's measurement (the implementer's notes say how; reading and running the built binary of the
   worktree's build/clang-debug is fine, running cmake is not) and tabulate: rows, events, summary values and
   columns compared against those in the golden files. Every exclusion must follow from a stated rule, be
   counted, and be justified by the Java-against-Java measurement; an exclusion that the measurements do not
   require is a finding; a simulation compared only in part must be named in the header comment.
3. Tolerances: each at least 100 times the largest measured difference (glibc, the six shim patterns, Java
   against Java) and within the plan's bounds; none chosen without a measurement. Run the new tests under the
   shim patterns yourself (preloading the shim does not write to the build directory) and report every margin
   below 100.
4. Not vacuous: change values in a copy of a stable golden (late in a flight, in a summary, a row count, an
   event) and, separately, a constant of a stepper by one part in 1e7 in a private build under
   ${PROBES}/tier8c-review-comparison/ (not in the worktree): the tests have to notice each. A mutation that
   survives is a finding.
5. Hygiene: no dependence on wall-clock time or on test order, no process-wide state, the run time of the new
   tests (they run in the dev, tidy, asan, GCC, MSVC and macOS jobs: report the time in Debug and under ASan
   from the implementer's logs), clang-tidy-visible problems the build would not catch, the existing
   default-step test unchanged in what it asserts.`,
  },
]

function implementPrompt() {
  return `${COMMON}
You are the implementer. Your current working directory is a fresh git worktree of the QtRocket repository. First
run 'pwd', 'git status', 'git branch --show-current' and 'git log --oneline -1' (expected: ${BASE} or a
descendant; if it is older, run 'git merge --ff-only origin/main'); if the branch name is empty, run
'git checkout -b wf/tier8c' immediately.
${SPEC}
Return in 'deviations' every point where you departed from the task text and why, and in 'libraryFixes' every
change under include/ or src/. When done, run the four verification presets and clang-format --dry-run --Werror
on your C++ files, then commit once: 'git add -A' and 'git commit -m "WIP: stable-step simulation goldens"'
(plain separate commands). Return the structured result (worktreePath from pwd, branch from
git branch --show-current, commit from git rev-parse --short HEAD).`
}

function reviewPrompt(impl, lens) {
  return `You are an adversarial reviewer of tier 8c of the QtRocket project. Assume the work contains mistakes and
find them. READ-ONLY on the repository and on the worktree: do not modify files there, do not run cmake there
(another reviewer shares its build directories), do not commit. Put everything you write under
${PROBES}/tier8c-review-${lens.key}/.
Worktree with the work: ${impl.worktreePath} (branch ${impl.branch}); 'git -C ${impl.worktreePath} diff --stat ${BASE}...HEAD'
lists everything that changed.
What the implementer was given:
${COMMON}
${SPEC}
The implementer reported these deviations: ${JSON.stringify(impl.deviations)}, these library fixes:
${JSON.stringify(impl.libraryFixes)} and these notes:
${impl.notes}
Not findings: that the default-step goldens and their test stay as they are; that the examples' stable set is
not committed; that noise-dominated columns are excluded by a stated, counted rule.
${lens.text}
Report concrete findings with file path (relative to the worktree), line, severity, a precise fix and your
evidence. Severity: 'blocker' = an existing golden file changed, a non-deterministic or wrong golden, a test that
can fail on a supported platform or cannot fail at all, or something that does not build/verify; 'major' = a
whole-flight value that could be compared and is not, a tolerance without a measurement or with a margin below
100, a wrong statement in the README or the header comment, a missing schema check; 'minor' = polish. Return the
structured result only.`
}

function verifyPrompt(impl, serious) {
  return `You are a skeptical verifier for tier 8c of the QtRocket project, in the worktree ${impl.worktreePath}
(branch ${impl.branch}). Reviewers reported the findings below. REFUTE each one if you can: read the cited files, and
where the claim is behavioural, prove it either way (the built test binary of the worktree's build/clang-debug, a
dump of the Java harness into a scratch directory, the reviewers' material under ${PROBES}/tier8c-review-*/), with
your own files under ${PROBES}/tier8c-verify/. READ-ONLY on the repository and the worktree: no edits, no cmake
there, no commits.
The task the implementer was given:
${SPEC}
For each finding return its id and a verdict: 'confirmed' (real, the fix is right), 'modified' (real, but the fix or
severity must change: give the corrected fix), or 'refuted' (the claim is wrong or the proposed fix would make
things worse). A finding survives only with evidence.
Findings (JSON): ${JSON.stringify(serious)}
Return the structured result only.`
}

function fixPrompt(impl, items, gaps, dropped) {
  return `You are the fixer for tier 8c of the QtRocket project. Work inside the existing worktree at
${impl.worktreePath} on branch ${impl.branch} (cd there for every command; edit files by absolute path under that
directory).
${COMMON}
The task the implementer was given:
${SPEC}
The implementer's notes:
${impl.notes}
Two reviewers examined the work and a skeptical verifier re-checked every blocker/major finding. 'status' tells you
how each item was vetted: 'confirmed' and 'modified' (use the given fix) are verified — apply them; 'minor' items
were not verified — apply them unless they are wrong; 'unverified' (the verifier failed) — check first. Close the
coverage gaps that are in scope. If you disagree with an item after checking, leave it out and explain why in
'disputed'. If a fix changes the harness, regenerate, and show again that no file that existed before this tier
changed and that two regenerations agree.
Findings (JSON): ${JSON.stringify(items)}
Coverage gaps (JSON): ${JSON.stringify(gaps)}
For context only — refuted by the verifier, do NOT apply: ${JSON.stringify(dropped)}
After the changes: 'clang-format -i' on touched C++ files, then dev, tidy, asan and ci-gcc must all pass with zero
warnings (one preset at a time). Then commit: 'git add -A' and 'git commit -m "Add stable-step simulation goldens
for the test rockets" -m "<short body: the harness change, the data, the comparison, the review fixes>"' (plain
separate commands) and return the structured result with the commit hash from 'git rev-parse --short HEAD'; in
'notes' give the final sizes, run times, measurements, tolerances and exclusions.`
}

// Run wf_4233e5e0-a0c (2026-10-06) got this far before its session ended: the implementer's result and the
// comparison review, copied from that run's journal. Only the harness review, the verification and the fix run.
const impl = IMPL_DONE
log(`carried over: implemented in ${impl.worktreePath} at ${impl.commit} (${JSON.stringify(impl.verification)}), ${impl.libraryFixes.length} library fixes`)

phase('Review')
const harnessLens = LENSES.find(lens => lens.key === 'harness')
const harness = await agent(reviewPrompt(impl, harnessLens), { label: 'review:harness', phase: 'Review', schema: REVIEW_SCHEMA })
const reviews = [harness ? { lens: 'harness', ...harness } : null, { lens: 'comparison', ...COMPARISON_DONE }].filter(Boolean)
const all = reviews.flatMap(r => r.findings.map((f, i) => ({ id: `${r.lens}-${i}`, lens: r.lens, ...f })))
const gaps = reviews.flatMap(r => r.coverageGaps.map(x => `[${r.lens}] ${x}`))
const serious = all.filter(f => f.severity !== 'minor')
const counts = all.reduce((c, f) => { c[f.severity] = (c[f.severity] || 0) + 1; return c }, {})
log(`${reviews.length}/${LENSES.length} reviews, ${all.length} findings ${JSON.stringify(counts)}, ${gaps.length} gaps`)
if (reviews.length < LENSES.length) log(`WARNING ${LENSES.length - reviews.length} review(s) returned nothing; the fixer works from the rest`)

phase('Verify')
let verdicts = []
if (serious.length) {
  const v = await agent(verifyPrompt(impl, serious), { label: 'verify', phase: 'Verify', schema: VERDICT_SCHEMA })
  verdicts = v ? v.verdicts : null
}
const items = [], dropped = []
for (const f of all) {
  if (f.severity === 'minor') { items.push({ ...f, status: 'minor' }); continue }
  const v = verdicts ? verdicts.find(x => x.id === f.id) : null
  if (!v) { items.push({ ...f, status: 'unverified' }); continue }
  if (v.verdict === 'refuted') { dropped.push({ ...f, reason: v.reason }); continue }
  items.push({ ...f, status: v.verdict, verifierReason: v.reason, ...(v.verdict === 'modified' && v.fix ? { fix: v.fix } : {}) })
}
log(`${items.length} to fix, ${dropped.length} refuted${verdicts ? '' : ' (verifier returned nothing: serious findings are unverified)'}`)

phase('Fix')
const fix = await agent(fixPrompt(impl, items, gaps, dropped), { label: 'fix', phase: 'Fix', schema: FIX_SCHEMA })
if (!fix) log('WARNING: the fixer returned nothing; the branch holds the implementer\'s commit only')

return {
  branch: fix ? fix.branch : impl.branch,
  commit: fix ? fix.commit : impl.commit,
  worktreePath: impl.worktreePath,
  verification: fix ? fix.verification : impl.verification,
  reviews: reviews.length,
  findings: all.length,
  severities: counts,
  toFix: items.length,
  refuted: dropped.map(d => `${d.file}: ${d.issue}`.slice(0, 300)),
  applied: fix ? fix.applied.length : 0,
  disputed: fix ? fix.disputed : [],
  deviations: impl.deviations,
  libraryFixes: impl.libraryFixes,
  implNotes: impl.notes,
  fixNotes: fix ? fix.notes : 'fixer returned nothing',
}
