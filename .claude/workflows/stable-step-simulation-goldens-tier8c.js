export const meta = {
  name: 'stable-step-simulation-goldens-tier8c',
  description: 'Tier 8c: add a second set of simulation goldens at a stable 0.01 s time step for the test rockets (harness change, regenerated data, whole-flight comparison in the golden test); two-lens review; verify; fix and commit',
  phases: [
    { title: 'Implement', detail: 'one worktree agent: harness, data, schema tests, whole-flight golden test' },
    { title: 'Review', detail: 'two lenses: harness and data; the comparison' },
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
   (another id sequence) and check that the header comment and the README state it correctly.`,
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

phase('Implement')
const impl = await agent(implementPrompt(), { label: 'implement:stable-goldens', phase: 'Implement', schema: RESULT_SCHEMA, isolation: 'worktree', effort: 'xhigh' })
if (!impl) throw new Error('the implementer returned nothing')
log(`implemented in ${impl.worktreePath} at ${impl.commit} (${JSON.stringify(impl.verification)}), ${impl.libraryFixes.length} library fixes`)

phase('Review')
const reviews = (await parallel(LENSES.map(lens => () =>
  agent(reviewPrompt(impl, lens), { label: `review:${lens.key}`, phase: 'Review', schema: REVIEW_SCHEMA })
    .then(r => (r ? { lens: lens.key, ...r } : null))))).filter(Boolean)
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
