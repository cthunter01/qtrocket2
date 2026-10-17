export const meta = {
  name: 'ork-loader-top-level-tier9c',
  description: 'Tier 9c: the top of the .ork loader in one worktree (OpenRocketHandler, OpenRocketContentHandler, OpenRocketLoader, GeneralRocketLoader; the 18 recorded files and the integration test; the 16 examples with the ExampleFilesTest port and the fixture for the example goldens); three implementers in turn, three lens reviews, verify, fix and commit',
  phases: [
    { title: 'Implement', detail: 'three implementers in turn: loader, recorded files, examples' },
    { title: 'Review', detail: 'three lenses: fidelity against OpenRocket with whole-file differential probes; robustness and hostile files; the tests against their sources' },
    { title: 'Verify', detail: 'skeptical re-check of every blocker/major finding' },
    { title: 'Fix', detail: 'apply findings, re-verify, commit on the branch' },
  ],
}

const OR = '/home/travis/Development/openrocket/core/src/main/java/info/openrocket/core'
const ORT = '/home/travis/Development/openrocket/core/src/test/java/info/openrocket/core'
// Probe and log directory: the launching session's scratchpad (update when a new session runs this).
const PROBES = '/tmp/claude-1000/-home-travis-Development-qtrocket2/26244a3d-a4bb-4541-8faa-88c7b0123029/scratchpad/probes'
// Probes of earlier tiers (build scripts, the one-ulp libm shim); a tmpfs, may be gone after a reboot.
const OLD_PROBES = '/tmp/claude-1000/-home-travis-Development-qtrocket2/87f548e1-ea1f-4253-ad87-f65f7c95546e/scratchpad/probes'
// The scouting run's reports (one .md per scout) and a copy of the scouts' probe sources and outputs.
const SCOUT = '/home/travis/.claude/projects/-home-travis-Development-qtrocket2/handoff/tier9-scout'
// The commit of main this run starts from (the tier-9b merge).
const BASE = '3a50b0b'

// The final notes of runs 9a and 9b (one file per group; the fixer's notes come first) and copies of their probes.
const NOTES9A = '/home/travis/.claude/projects/-home-travis-Development-qtrocket2/handoff/tier9a-notes'
const EVID9A = '/home/travis/.claude/projects/-home-travis-Development-qtrocket2/handoff/tier9a-evidence'
const NOTES9B = '/home/travis/.claude/projects/-home-travis-Development-qtrocket2/handoff/tier9b-notes'
const EVID9B = '/home/travis/.claude/projects/-home-travis-Development-qtrocket2/handoff/tier9b-evidence'

// What runs 9a and 9b left for this run: the essentials; the notes files have the signatures and the traps in full.
const FROM_EARLIER = `Runs 9a and 9b are merged into main (the base of this run). Required reading before you write code:
${NOTES9B}/rocket-side.md and ${NOTES9B}/simulation-side.md (the handlers and setters of the loader: every class with
its constructor, what it exposes after it ends, how it reports a failure, the traps), ${NOTES9A}/document.md (the
document, the preset database, the loader infrastructure) and ${NOTES9A}/extensions.md (the extension registry and
classes, the validation in simulate()); ${NOTES9A}/golden-groundwork.md for part J3 (the helper that settles
automatic dimensions, the shared golden helpers, the stable-step goldens of the examples). In each file the
fixer's notes come first and supersede the implementers' where they differ. The probes of the two runs (Java
sources, inputs, outputs) are under ${EVID9A}/tier9a-<part>/ and ${EVID9B}/tier9b-<part>/.
From run 9b:
- Simulation side (include/QtRocket/file/openrocket/; the notes file has every signature): SimulationsHandler,
  SingleSimulationHandler, SimulationConditionsHandler and FlightDataHandler are each constructed from the
  DocumentLoadingContext alone and keep it by pointer. SimulationsHandler throws BugError when the context has
  no document or no preference store: the entry checks the environment before it parses (decision T1). The
  context, its document and its store must outlive the handlers, and the store must outlive the simulations.
  OpenRocketContentHandler returns a SimulationsHandler for <simulations>; that handler's closeElement() gets
  the attributes of <simulations> (or another element's after an ignored element inside: decision L1).
- Each closing <simulation> registers the types of the document's custom expressions again
  (CustomExpression::getType() of each, in list order), so the <datatypes> must be in the document before
  <simulations> is read, as the file order has it. A simulation is added to the document when its element
  ends; SingleSimulationHandler::getSimulation() is null until then and after a failure.
- Failures of the simulation side are ErrorCode::INVALID_ARGUMENT with Java's message. One of them has an EMPTY
  message (an id with nothing between two of its dashes, as Java's NumberFormatException has none): the entry
  must never read an empty message as success, and Java's text is then 'Exception loading stream: ' with nothing
  behind it.
- After a failed load the half-built document must be discarded by the entry: simulations read before the
  failure are in it, the rocket has the configurations they made, the preference store keeps a stepper method
  that was read (decision L8) and the flight data type registry keeps the column names. The last two are
  process-wide or caller-owned state that a failed load leaves changed: say so in the header.
- Post-load, as the implementers left it for this run: syncModId() of every simulation before getStatus() is
  asked (a configuration without motors then says CANT_RUN, a stored SIM_ABORT event ABORTED); documentLoaded()
  of every extension has not been called by any handler.
- Simulation::getLandingDispersionSettings() holds the opaque element; Warning::EventAfterLanding carries the
  event id; MessageSource::removed(id) is the source of a component that is not in the rocket.
- Measured by the simulation side (release build): loading is linear in simulations of configurations the rocket
  has, quadratic in simulations that each create a configuration (2000 in 9 s; OpenRocket 6.5 s) and in the
  number of distinct warnings of one load (the warning set searches itself on every add, as Java's does). The
  bounds on these are open questions for the user and not yours to settle; decision T4 bounds the document.
- The comparison of the 54 example simulations with OpenRocket (conditions, stored rows, events) exists only in
  the implementers' probe outputs: tier9b-conditions/out/examples-*.txt, tier9b-flight-data/out/examples-*.out
  and tier9b-simulations/out/examples-*.out under the 9b evidence directory. The committed test of run 9b
  compares statuses, configurations, extensions, summaries, warnings and per-branch counts through the test
  root; part J3 carries the comparison through the real loader.
- Test support of the simulation side: tests/core/TestTempDir.h has CurrentDirectoryGuard (declare it after the
  TempDir it points into; a lookup CSV named by a design is read relative to the current directory of the
  process), ConditionsTestSupport.h, FlightDataTestSupport.h, SimulationTestSupport.h.
- Traps: libstdc++'s path::generic_string() drops repeated separators (compare pathToUtf8() of a path);
  misc-include-cleaner wants TestTempDir.h included directly where TempDir is named; nothing in the worktree
  may be edited while a preset builds, generated tables included.
- Rocket side, the classes the top-level handlers construct (each keeps the context by const reference, which
  must outlive it; own each in a std::unique_ptr): for <rocket>
  ComponentParameterHandler(RocketComponent& component, const DocumentLoadingContext& context) with the
  document's rocket (it creates every component, motor mount, configuration and appearance handler itself and
  leaves nothing to ask for afterwards: the state is in the rocket and the document); for <datatypes>
  DatatypeHandler(context), once; for <photostudio> PhotoStudioHandler(std::map<std::string, std::string>&) with
  document.getPhotoSettings(); for <docprefs> DocumentPreferencesHandler(context), a new one per element.
  DatatypeHandler, DocumentPreferencesHandler and DocumentMaterialHandler throw BugError for a context without a
  document.
- DocumentConfig helpers: kSupportedVersions, isSupportedVersion(), kFileVersionDivisor, attribute(),
  configurationId(), parseInt(), parseDouble(), stringToDouble(), parseFiniteDouble(), findEnum(), kMaxCount,
  kMaxInstances, instanceLoad(const RocketComponent&).
- Failures of the rocket side are ErrorCode::INVALID_ARGUMENT with Java's exception message (an <id> that is no
  UUID, a stage number that is no integer or missing: 'Cannot parse null string', a shine, rotation, x or y that
  is no number, an unknown edge mode, a colour value of the photo studio, 'Illegal material string: <text>').
  Everything else is a warning, in Java's order and with Java's text. QtRocket's own warnings are
  Warning::kFileInvalidParameter for what it refuses, '<Component> is nested too deeply; ignoring this component
  and its subcomponents.' (more than 100 levels) and '<Component> would give the rocket too many component
  instances; ignoring this component and its subcomponents.' (the instance budget: the instances of all
  components times the flight configurations may not exceed 100000). No BugError and no exception leaves the
  handlers for anything a file holds, std::bad_alloc included.
- The load environment, as the handlers read it: getPreferences() also gives the per-class default materials of
  a component whose element names no material; getApplicationMaterials() must be a MaterialStorage with
  addBuiltinMaterials(), else every material of a file becomes a document material and no default material is
  applied; a null or empty preset database gives OpenRocket's 'No matching ComponentPreset ...' warning. The
  goldens were made with OpenRocket's test preferences, which name NO default material: load the examples and
  the recorded files with a preference store that names none (never after loadDefaultComponentMaterials(),
  which makes fins Balsa and nose cones Polystyrene where an element has no <material>). Never freeze the rocket
  during a load. ZipFileAttachmentFactory keeps decal names; FileSystemAttachmentFactory gives
  'decals/<file name>', numbered.
- GeneralRocketLoader.load() ends with doc.getRocket().enableEvents() (GeneralRocketLoader.java:103), which runs
  an update of the rocket: it is one of the final steps (decision T5).
- Comparing an as-loaded state: tests/core/file/openrocket/ComponentHandlerTestSupport.h has describeRocket()
  (with the motors of every flight configuration), RocketLoadFixture, whatUsingTheRocketThrows(),
  motorsThatAreNotTheirMounts() and largestInstanceLoad(). Do not ask a freshly loaded rocket for its length or
  bounds before you compare it (the automatic values store what they compute, and in another order than
  OpenRocket's hash map would): read the values in the order in which OpenRocket's saver calls the getters.
  Run 9b compared 63 design files (the examples, the re-saves, the recorded files) with OpenRocket through the
  test root this way, as SHA-256 of the state: rocket_element_tests.cpp and the fixer's probe directory
  (tier9b-fix-rocket-side: HandlerProbe.java, cases/own.txt with the 117 cases QtRocket answers otherwise on
  purpose and why, order/ with the order of the rocket's change events).
- Not examined by run 9b and yours: the document's change events, the undo state and the modification ids
  during and after a load; what a caller sees after a failed load.
- A warning from run 9b's fixer for everyone who fuzzes: a fuzz runner of a reviewer was found hours later still
  running with 47 GB resident, its parent gone. Every process you start is yours to end (see the rules).
From run 9a (still valid unless run 9b's notes say otherwise):
- A load starts from OpenRocketDocumentFactory::createEmptyRocket(): the rocket's events are on from then
  (decision D3). The document: getRocket(), addSimulation(std::shared_ptr<Simulation>), getDecalImage(attachment)
  (the rocket's Decal must carry the returned image's getName(), not the attribute), getDocumentMaterials()
  (a MaterialStorage: addMaterial()), getDocumentPreferences(), addCustomExpression(), getCustomExpressions(),
  getPhotoSettings()/setPhotoSettings(), getDefaultStorageOptions(), clearUndo(), reloadDocumentMaterials().
  undo() and redo() are never called by a loader.
- include/QtRocket/file/DocumentLoadingContext.h, a copyable value that handlers keep by const reference:
  getFileVersion(), getMotorFinder(), getOpenRocketDocument(), getAttachmentFactory() (never null),
  getApplicationMaterials(), getPreferences(), getComponentPresetDatabase(), getSimulationExtensionRegistry()
  (the last four may be null), each with its setter.
- include/QtRocket/file/openrocket/MotorHandler.h: MotorHandler(context); at the closing motor element the mount
  handler calls getMotor(warnings) (null: no motor), then getDelay(warnings), then getNozzleExitDiameter().
- EntryHandler, EntryHelper and ConfigHandler: EntryHelper::getValueFromEntry(EntryHandler&, attributes, content)
  takes the handler's nested list, so call it exactly once for every entry or pref that closes; list entries are
  loaded (decision D9); nesting is bounded.
- include/QtRocket/file/openrocket/DocumentConfig.h: kSupportedVersions, kFileVersionDivisor, findEnum(text,
  constants, nameFunction), stringToDouble() (Java's: NaN, Inf, -Inf, else Double.parseDouble), parseDouble() and
  parseInt() (Double.parseDouble and Integer.parseInt with Java's exception messages, failing with
  ErrorCode::INVALID_ARGUMENT: use them wherever Java lets the exception fail the load) and parseFiniteDouble()
  for decision U3. The registries go at the line marked HOOK(loader-registry).
- Error codes, fixed by run 9a for the top-level loader of run 9c: a handler's failure that is an
  IllegalArgumentException or NumberFormatException in Java is ErrorCode::INVALID_ARGUMENT with Java's message
  (the loader will add 'Exception loading stream: '); ErrorCode::PARSE means malformed XML and ErrorCode::IO an
  unsupported encoding, both produced by SimpleSax::readXml(bytes, handler, warnings).
- Presets: rocket/preset/ComponentPresetDatabase (find(manufacturer, partNo) in list order) and the owning
  RocketComponent::loadPreset(std::shared_ptr<const ComponentPreset>, options). loadPreset() throws BugError on a
  frozen rocket, for options on a component that is no parachute, and (inside loadFromPreset) for a preset whose
  material type does not fit the component: guard each before the call. A preset registers its document material
  during loadPreset(), before the file's own material: that order is wanted.
- Extensions (include/QtRocket/simulation/extension/): SimulationExtensionRegistry::create(id) (null when no
  provider knows the id; the id is compared as given), UnknownSimulationExtension(id, config) with
  notFoundText(id), JavaCode with setClassName(), ScriptingExtension; documentLoaded() is called by run 9c.
- Simulation::simulate() now refuses non-finite inputs (Simulation::validateInputs(); its comment lists what is
  checked). For the loader half of decision U3 this means: do not apply a non-finite simulation option, delay,
  deployment altitude or separation altitude, nor a lookup table with a non-finite cell. Values reach the options
  also through the preference store that SimulationOptions(preferences) reads. A finite but absurd value (a time
  step of 1e300) is applied as Java applies it: a bound for finite values is an open question for the user, not
  yours to settle.
- Test support: tests/core/file/openrocket/HandlerTestSupport.h (HandlerFixture with a context, a document, a
  scripted motor finder, a map of attachments, preferences and materials; runHandler(handler, xml) returns the
  result, the warnings and what the root saw), EntryTestSupport.h, tests/core/file/RawZip.h,
  tests/core/motor/TestMotorDatabase.h (the bundled database, read once per test process: keep the assertions that
  need it in few tests), tests/core/rocket/preset/ExamplePresets.h (the six presets of the examples and their
  references), tests/core/file/ExampleMotors.h (the 41 motor references of the examples),
  tests/core/document/DocumentTestSupport.h (recorders, an Alpha III document).
- Test data: tests/data/ork holds 18 recorded .ork files (tests/data/ork/README.md) and a test fails for any
  other file there. Hand-written documents are raw strings in the test sources; a fixture that has to be a file
  is built in the test's temporary directory (tests/core/TestTempDir.h).
- Traps the 9a agents met: bugprone-throwing-static-initialization (no namespace-scope std::string or std::vector
  constant in a test: constexpr std::string_view, std::to_array or a function); bugprone-unchecked-optional-access
  flags value() after ASSERT_TRUE(has_value()); modernize-use-designated-initializers for table rows;
  readability-container-contains; GCC -Werror=dangling-reference for a reference bound to a temporary's member;
  an escape for a non-ASCII character that you write through a file tool or a heredoc can arrive in the file as
  the raw character: check new files with grep -nP for bytes above 0x7F and put the escape in with a script;
  do not edit a file while a preset is building; a foreground command is cut off after ten minutes, so run the
  asan and ci-gcc workflows in the background and wait for them; the asan test step takes seven to fifteen
  minutes.`

const COMMON = `
You are porting part of OpenRocket (Java, GPLv3) to QtRocket (C++23). This is tier 9 of the port: the document
(OpenRocketDocument with undo and redo), loading .ork files, and the 16 example designs compared with the committed
goldens. Tier 9 is four workflow runs; this is run 9c, the top of the .ork loader: three agents work one after the
other in one git worktree. Run 9a is merged (the document, the preset database, the loader infrastructure, the
extensions, the golden-test groundwork) and so is run 9b (every handler and setter below the top level, each tested
through the test root of tests/core/file/openrocket/HandlerTestSupport.h). This run adds the top-level handlers
(OpenRocketHandler, OpenRocketContentHandler), OpenRocketLoader and GeneralRocketLoader, loads the 18 recorded
files of tests/data/ork and the 16 example designs of data/examples as OpenRocket loads them, ports
ExampleFilesTest and swing's IntegrationTest, and leaves the fixture that run 9d uses to compare the examples with
the committed goldens (design, default-step and stable-step simulations, the re-saved test rockets). Saving is
tier 10.
What the earlier runs left for this run (read it first; it names the classes and signatures you build on):
${FROM_EARLIER}
The OpenRocket source is a read-only reference at /home/travis/Development/openrocket (never modify it; commit
5f164fd0e, core sources under ${OR}, tests under ${ORT}).
The approved plan is /home/travis/.claude/plans/create-a-plan-to-inherited-torvalds.md; read sections 3.3, 6.2, 6.4
and the document/ and file/ paragraphs of 6.3 (corrected by the decisions below where they differ).

Scout reports. Before this run seven scouts and a critic examined the tier, with Java probes against OpenRocket.
Their reports are ${SCOUT}/document.md, loader-components.md, loader-simulations.md, examples.md, presets-setup.md,
golden-tests.md, junit-inventory.md and critic.md; a copy of their probe sources, inputs and outputs is under
${SCOUT}/evidence/tier9-scout-<name>/ (the originals, with compiled classes and the unpacked examples, are under
${PROBES}/tier9-scout-<name>/ while this machine stays up). Your specification names the reports and sections that
are required reading for you. They are evidence, not authority: the scouts read QtRocket without building it, they
disagree with each other in places (critic.md lists the contradictions and corrections), and a line number may be
off. Where your specification below settles a point, the specification wins; where a report contradicts the Java
or the QtRocket sources, the sources win and you say so in your notes.

Already ported (read the headers you use; match their style): util/, unit/, logging/ (Warning, WarningSet,
SimulationAbort), preferences/ (Preferences, InMemoryPreferences, DocumentPreferences), material/ (Material as a
value type, MaterialDatabase, MaterialStorage), motor/ (ThrustCurveMotor, MotorDigest, ThrustCurveMotorSetDatabase,
SqliteMotorDatabaseReader), rocket/ (the whole component model, Rocket with copyRocketWithOriginalId() and
loadFrom(), FlightConfiguration, Appearance, Decal, rocket/preset/ with ComponentPreset and ComponentPresetFactory),
models/, mass/, aero/, simulation/ (FlightEvent, FlightData, SimulationOptions, SimulationConditions,
SimulationStatus, Simulation with cloneForUndo(), loadFrom(), copy() and operator==, the steppers, the engine, the
listener interfaces with CloneableSimulationListener, simulation/extension/ with the SimulationExtension and
provider interfaces and util/Config), document/ (OpenRocketDocument with undo and redo, DecalRegistry, StorageOptions), and of file/: GzipStream,
ZipArchive, ZipInputStream, XmlScanner, simplesax/ (a port of OpenRocket's SAX handler stack), file/motor/ (the .eng
and .rse loaders, the .rse writer), the load context with the attachment factories and the motor finder, and
file/openrocket/ (every handler and setter of the .ork loader below the top level).
Test support: tests/core/rocket/TestRockets.h (the 13 test rockets and their motors), tests/core/TestPaths.h and
TestTempDir.h, tests/core/unit/DefaultUnitsGuard.h, tests/core/simulation/*Support.h, tests/core/goldens/.
Reuse what exists; never duplicate its functionality. Where an existing class lacks something or deviates from the
Java, fix it there against the Java source with a regression test and report it in libraryFixes. Programmer errors
throw util/BugError (bug()/QTROCKET_ASSERT), never std::logic_error; recoverable failures return Result<T>
(util/Error.h); loaders return Result and collect warnings, they never throw; SimulationException and its
subclasses are the only exceptions that cross the simulation loop (plan 6.2).

Decisions already taken for tier 9 (not to be changed; if one cannot work, stop and explain in your notes). The
first four are the user's own answers.
 U1. Extensions: RollControl and AirStart are ported now with their providers (the plan had them in Milestone 7),
     so that all 54 example simulations can be compared.
 U2. The 16 examples get a stable-step (0.01 s) golden set as JSON documents only: summary values, events and
     warnings, no time series.
 U3. Non-finite values: the .ork loader does not apply a NaN or an infinity it reads for a simulation option or a
     delay (it adds the warning the handler has, or Warning::kFileInvalidParameter, and the value keeps what it
     was), and Simulation::simulate() validates its inputs before the engine starts and returns an ordinary
     Result error, ErrorCode::INVALID_ARGUMENT with a text that names the value, instead of ending in a BugError.
     The plugged ejection delay (+infinity: "none" in a file, Motor's plugged delay) is a legitimate value and is
     exempt everywhere.
 U4. The default-step golden comparison takes over the stable-step comparison's rule for the out-of-plane noise
     columns: they are compared on the launch rod only, and what the rule excludes is counted and asserted.
 D1. Subsystem order: util, unit, logging, preferences, material, motor, rocket, models, mass, aero, simulation,
     document, file, cli. document/ is new (src/CMakeLists.txt: add_subdirectory(document) between simulation and
     file). A subsystem never includes a later one: document/ never includes file/, simulation/ never document/.
 D2. No global application object: where Java asks Application or Guice (preferences, motor database, preset
     database, extension providers, translator), the object is passed in explicitly. No translation layer: the
     English text of a key from /home/travis/Development/openrocket/core/src/main/resources/l10n/messages.properties.
 D3. Rocket change events are enabled while a file loads, as in OpenRocket: the OpenRocketDocument constructor
     enables the rocket's events and no loader disables or bypasses them (no Java loader calls
     setBypassChangeEvent; comments in QtRocket that say otherwise are corrected).
 D4. The document: it owns its Rocket (std::unique_ptr) and keeps one Rocket object for its lifetime (undo loads
     into it); simulations are std::shared_ptr<Simulation> in the document's list (an undo that changes the
     number of simulations replaces the objects, and callers keep the old ones); the document connects to
     Simulation::changed() when a simulation enters its list and disconnects when it leaves (simulation/ cannot
     call document/); documentChanged is emitted twice per rocket event, as Java's componentChanged() followed by
     stateChanged(), with the document's ModId drawn between the two; getSimulationIndex(simulation) and
     removeSimulation(simulation) search by identity (a documented deviation: Java searches with equals(), which
     would find a simulation's copy), while the clean-state comparison with an undo snapshot uses
     Simulation::operator== as Java; the five undo error sites carry on as Java does and report through a signal
     (not BugError); OpenRocketDocument.copy() and the 16 document makers of TestRockets.java are not part of
     tier 9 (optimizer milestone; tier 10 with the saver tests).
 D5. Document materials: Rocket gets a Signal<const Material&> documentMaterialSet(), emitted at the eight Java
     sites at the moment Java registers the material (the registration order decides the order of <docmaterials>
     in a later save); the document keeps its materials in a MaterialStorage member, not in DocumentPreferences'
     string list. A copy of a Rocket does not keep the pointer to the document (deviation: a copy can outlive it).
 D6. Component presets: a new rocket/preset/ComponentPresetDatabase holds std::shared_ptr<const ComponentPreset>;
     a component co-owns its preset (std::shared_ptr member; getPresetComponent() keeps returning a raw pointer;
     the existing raw-pointer loadPreset() stays as a documented non-owning form so that no existing call site
     changes). Loading .orc preset files stays in Milestone 3: until then the product has no presets, an
     unresolved <preset> gives OpenRocket's warning and the link is lost, as in OpenRocket with an empty database.
     The tests get a fixture of the six Estes presets the examples refer to, built with ComponentPresetFactory.
 D7. Placement: Attachment, FileSystemAttachment, DecalImage and DecalRegistry in document/; ZipFileAttachment
     and the attachment factories in file/ (they read zip data). The loader's handler and setter classes get one
     public header each under include/QtRocket/file/openrocket/ (Java's package name 'importt' is dropped),
     sources under src/file/openrocket/, tests under tests/core/file/openrocket/.
 D8. The load environment, the same for every loader class (include/QtRocket/file/DocumentLoadingContext.h, Java:
     DocumentLoadingContext plus what Java takes from globals): the file version (major * 100 + minor, 0 when
     unknown), a MotorFinder*, an AttachmentFactory*, the OpenRocketDocument* being filled, a const
     MaterialStorage* (Java: Databases, the application's materials), a Preferences* (it must outlive the
     document's simulations: SimulationOptions and Simulation keep it), a const ComponentPresetDatabase* (null or
     empty: no preset is found) and a const SimulationExtensionRegistry* (null: every extension id is unknown;
     include/QtRocket/simulation/extension/SimulationExtensionRegistry.h). All pointers are non-owning.
 D9. Failure policy of loader code: no BugError and no exception may leave a loader for anything a file can
     contain (guard before every call that treats a bad value as a programmer error). Where Java throws an
     IllegalArgumentException that fails the load, return an Error with Java's message; where Java dies of a
     NullPointerException, BugException, IndexOutOfBoundsException or RuntimeException, skip the element with a
     warning; where Java's loader silently drops data that its own saver writes, keep the data (a documented
     deviation, pinned by a test: for example the list entries of an extension's configuration). Warning texts
     are Java's, character for character, Java class names included. simplesax's Attributes stay const: a
     handler that removes attributes in Java copies the map and erases the keys.
 D10. Motors are resolved exactly as MotorHandler.java and DatabaseMotorFinder.java do, the silent match by
     designation included (the goldens were made with those motors); there is no per-document motor list.
 D11. Extensions: a SimulationExtensionRegistry of providers is handed to the loader. ScriptingExtension and
     JavaCode exist as classes that never execute anything. An extension whose id no provider knows is kept as an
     UnknownSimulationExtension (id and Config, so that a save can write it back) with Java's warning, and a run
     of a simulation that has one fails with a SimulationException from initialize(), because the simulation
     says it has an extension that cannot act.
 D12. Legacy test files: 17 .ork files of formats 1.0 to 1.9 from OpenRocket's git history are copied into
     tests/data/ork (run 9a; tests/data/ork/README.md says what OpenRocket makes of each) for the loader tests.
 L1. Elements are applied in document order, the component being attached to its parent before its values
     are set, as Java does: the state after a load (stale automatic values included) depends on it and run 9c
     compares it. simplesax's DelegatorHandler reproduces OpenRocket's bookkeeping slip after an ignored element
     (the attributes and text of the enclosing elements shift): never correct it in a handler; which handler
     returns nothing and which returns a plain-text handler decides the warnings.
 L2. Where Java fails the whole load with an IllegalArgumentException or NumberFormatException, the handler or
     setter returns an Error (ErrorCode::INVALID_ARGUMENT, as run 9a's helpers do) whose message is Java's
     exception message; run 9c's loader adds Java's prefix ('Exception loading stream: '). Everything else is a
     warning with Java's text.
 L3. Decision U3 in the loader: test std::isfinite wherever Java tests isNaN, and wherever Java stores an
     infinity or a NaN for a simulation option, a delay, an altitude, an overhang, an override or a position
     (the scouts' tables list what Java stores); a refused value adds the warning the handler already has for
     an unparsable one (else Warning::kFileInvalidParameter) and is not applied. A wind level with a non-finite
     or missing attribute is skipped with a warning (Java: a failed load or a NullPointerException). The plugged
     ejection delay stays valid. Stored flight data (data points, summary attributes, event times as far as
     FlightEvent accepts them) is data, not an input of a run: it is loaded as Java loads it.
 L4. Where Java dies of an uncaught exception, the element is skipped with a warning (decision D9): a decal
     without name, rotation or edgemode; center, offset or scale without x or y; an empty <finpoints/> (Java's
     'Illegal fin points specification, ignoring.', the default outline stays); an <ignitionconfiguration>
     applies only what it gives; an axial offset that would be NaN; a <type> without source (an unknown
     datatype); a <pref> of type number or list with a key. Before every QtRocket call that throws BugError for
     a bad argument there is a guard (the scouts name: RocketComponent::setAxialOffset with NaN,
     FreeformFinSet::setPoints with no point, Simulation::setFlightConfigurationId with the error id, the
     FlightDataBranch constructor with no type or a type twice, loadPreset with options on a wrong kind or with
     a preset of another type than the component's; find the others: grep the setters you call for bug( and
     QTROCKET_ASSERT).
 L5. Data that Java's loader drops although its saver writes it is kept (decision D9): document preferences of
     type integer and double (putInt, putDouble); one custom expression per <type source="customexpression">
     element, not one per closing child. Each with a test and a line in the header.
 L6. Counts from a file are bounded: an integer that sets a number of instances, fins, lines, sides or the like
     and has no upper clamp in the component (tier 6 named the instance counts of LaunchLug and RailButton;
     find them all) is refused with Warning::kFileInvalidParameter above 10000. OpenRocket has no bound; with
     events enabled during a load such a number would make the loader allocate without limit. A deviation.
 L7. Enum texts are matched exactly as DocumentConfig.findEnum does (run 9a's helper), also where QtRocket has
     a more permissive parser: LineStyle, GeodeticComputationStrategy, Simulation::Status, SimulationAbort::Cause.
 L8. A simulation without <conditions> or without <configid> fails the load with Java's message ('Attempted to
     set the configuration to an error id. Not Allowed!'); an empty <configid></configid> is a new random
     configuration id, as in Java. Loading a <simulationsteppermethod> writes the choice into the preference
     store, as OpenRocket does: say so in the handler's comment.
 L9. A source id that names no component of the rocket is kept, with Java's text for the removed component
     ('<i>Component Removed From Rocket</i>'). An unknown altituderef keeps MSL with a new warning (Java stores
     null). <landingdispersion> is kept opaque on the Simulation (what the element holds, unvalidated, compared
     and copied where Java compares and copies its settings), with '// HOOK(monte-carlo)' for the validation.
     Warning::EventAfterLanding gains the id of its event (the loader reads eventid, tier 10's saver writes it).
 L10. Extensions in a simulation: the id with 'net.sf.openrocket' replaced by 'info.openrocket.core', then the
     registry of the context; no provider: an UnknownSimulationExtension with Java's warning ('Simulation
     extension with id '<id>' not found.'); a non-blank <listener> is a JavaCode extension with that class
     name. documentLoaded() of the extensions is called by run 9c's OpenRocketLoader, not here.
 L11. The preset setter behaves as Java's does, its dead type fallback included: only a candidate with the
     file's digest is taken. The context's preset database may be null or empty: Java's 'No matching
     ComponentPreset ...' warning.
 T1. The public entry of the file subsystem is include/QtRocket/file/GeneralRocketLoader.h. It is given the load
     environment explicitly: a DocumentLoadingContext in which the caller has set what it has (motor finder,
     application materials, preferences, preset database, extension registry); the loader works on a copy and
     sets the document, the file version and the attachment factory itself. Two forms, both returning a Result
     of the loaded document (std::unique_ptr<OpenRocketDocument>) together with the load's WarningSet (one small
     struct in its own header, include/QtRocket/file/LoadedDocument.h): load(path), and a form for bytes in
     memory with an optional base directory (tier 10's round trips, the tests and later the GUI use it). For
     bytes that are a zip container the attachments come from those bytes (Java throws a NullPointerException
     for a zip read from a stream without a file); for a plain or gzip document they are files next to the
     document, or FileSystemAttachmentFactory without a directory when none is given. The header states the
     lifetime rules: what must outlive the returned document (the preferences) and what is needed only during
     the load. AbstractRocketLoader, RocketLoader and RocketLoadException are not ported as classes: say in
     the headers where their content went (the failure is the Result's Error).
 T2. Failure texts are Java's, composed as Java composes them (GeneralRocketLoader.java:74-108 and 131-232,
     OpenRocketLoader.java:37-53): 'Exception loading file: <file> , <message>', 'Exception loading stream:
     <message>' (this prefix goes in front of every Error a handler or setter returns, decision L2),
     'Unsupported or corrupt file.', 'Malformed XML in input.'. Codes: ErrorCode::IO when the file cannot be
     opened or read (the cause's text is QtRocket's own: the operating system's wording is not portable and no
     test pins it); ErrorCode::UNSUPPORTED_FORMAT for 'Unsupported or corrupt file.' and for a document that is
     recognised as RockSim or RASAero (text of your choice that names the format; '// HOOK(rocksim)' and
     '// HOOK(rasaero)'); ErrorCode::PARSE for malformed XML and for a damaged gzip or zip stream;
     ErrorCode::INVALID_ARGUMENT as the handlers return it. A handler's Error may have an empty message (Java's
     exception has none for an id with an empty group): it is a failure all the same, and the text is then the
     prefix alone. A failed load returns no document at all; what it leaves changed outside the document (a
     stepper method written to the preference store, names in the flight data type registry) is stated in the
     header.
 T3. Sniffing is Java's, byte for byte (the first 300 bytes, the ten-byte rule, the matcher that falls back to
     the start without re-testing the byte, so '<<openrocket' is not found; a signature after byte 300, UTF-16
     and a prefixed root are 'Unsupported or corrupt file.'), with one deviation, the zip container: when the
     first entry's name ends in .ork, .rkt or .cdx1 (ASCII case ignored) it is the document, as in Java;
     otherwise the first later entry whose name ends in .ork (ASCII case ignored) is taken, and a container
     without one is 'Unsupported or corrupt file.'. OpenRocket returns the empty rocket without any message
     for such a file; the plan words it 'the *.ork entry is the document', and
     tests/data/ork/v1.6-preset-usage-decals-first.ork (first entry 'decals/') then loads. An attachment is
     found by its exact entry name, a leading slash included.
 T4. Bounds (Java has none; a deviation, stated in the header): the uncompressed document is at most 256 MiB;
     a gzip or zip stream that would give more is refused with an Error before the memory is taken, and
     decompression stops there. An attachment keeps Java's limit (run 9a). Nothing a file contains may make the
     loader allocate in proportion to a number the file merely states.
 T5. After the document is read the steps of OpenRocketLoader.loadFromStream and GeneralRocketLoader.loadUsing
     run in Java's order: applyPreloadedStageActiveness() on every flight configuration (the default one
     included), syncModID() of every simulation, the storage options rule (saveSimulationData when a
     simulation whose status is neither EXTERNAL nor NOT_SIMULATED has a first branch with a time column;
     explicitlySet false; the file type), documentLoaded(document, simulation, warnings) of every extension,
     clearUndo(), then reloadDocumentMaterials(), then the rocket's enableEvents() with which
     GeneralRocketLoader.load() ends (GeneralRocketLoader.java:103; the events were on throughout, decision D3,
     but the call runs an update of the rocket). What isSaved(), the ModIds, the undo state and the events a
     listener of the document has seen are after a load is pinned from a Java probe.
 T6. The loader has no catch-all: it does not turn a BugError or a std::exception into an Error (an exception
     that an API documents for bad data, if there is one, is caught where that API is called). A BugError that
     a file can reach is a defect of a missing guard and is fixed where the guard belongs, with a test, also
     when the handler is run 9b's.
 T7. Example test set-up, one fixture for this run and run 9d: tests/core/goldens/GoldenExamples.h/.cpp. The
     environment: the bundled motor database (tests/core/motor/TestMotorDatabase.h, read once per test process)
     behind a DatabaseMotorFinder; the six presets of tests/core/rocket/preset/ExamplePresets.h in a
     ComponentPresetDatabase, or on request an empty one; an InMemoryPreferences filled by
     storeJavaTestPreferences() (tests/core/simulation/SimulationRunSupport.h) plus setMotorNameColumn(false)
     (the harness's store: with the default the names carry designations and 40 golden files of 8 examples
     mismatch) and naming no default material for any component class (OpenRocket's test preferences name
     none; after loadDefaultComponentMaterials() a fin without <material> would be Balsa);
     SimulationExtensionRegistry::bundled(); the application materials as a MaterialStorage with
     addBuiltinMaterials() (without them every material of a file becomes a document material);
     DefaultUnitsGuard where a name is formatted. It loads an example from its original file
     (data/examples/<name>.ork) or from the golden re-save (tests/data/goldens/example-*/resave/rocket.ork) and
     gives two states: as loaded, and settled (settleAutomaticDimensions() of
     tests/core/goldens/AutomaticDimensions.h, what the golden harness does before it dumps).
 T8. ExampleFilesTest is ported without its maximum altitudes (54 values with a tolerance of 0.5 m): they come
     from flights in turbulence drawn from java.util.Random, QtRocket's PinkNoise draws another sequence (an
     earlier decision), and OpenRocket's own value moves by more than 0.5 m in 27 of the 54 with another
     realisation. The calm goldens compare the flights (run 9d). Report the test as ported in part. Everything
     else of it is ported and runs on the freshly loaded, unsettled document: it is the only check that
     QtRocket, loading with events enabled, ends in OpenRocket's as-loaded state, on which tier 10's re-save
     comparison depends.
 The user's answers of 2026-10-08 on the open points of runs 9a and 9b (U5 to U9). A later run implements
 them; this run does not, except where U5 says so, and builds nothing against them:
 U5. A lookup CSV that a design names will be read only inside the design's own directory (a relative path
     against that directory; an absolute path, or one that climbs out with '..', refused). For that the load
     context carries the directory of the design: part J1 adds it to DocumentLoadingContext (an optional
     std::filesystem::path with getter and setter; the entry sets it for load(path) and from the base
     directory of the bytes form, and leaves it empty otherwise) and leaves CsvLookupHandler as it is.
 U6. The warnings of one load will be capped (about 1000, with one last warning that says more were dropped)
     and a file will get more bounds (about 10,000 components, 1,000 flight configurations, 10,000 simulations,
     10,000 wind levels). Write no test that needs more than that from one file in order to pass; a
     hostile-input test asserts 'a document or an Error, never an exception', not a number of warnings.
 U7. QtRocket's own refusals (a duplicate id, a count over a bound, a preset of the wrong kind, an ignored
     decal) will get warning texts of their own in place of the generic 'Invalid parameter encountered,
     ignoring.': where a test of this run meets such a warning, compare it with Warning::kFileInvalidParameter
     by name and not with its text. A <decal> without name will be ignored with one warning and no stray
     second one.
 U8. An unresolved <preset> reference will be kept on the component for tier 10's saver; its warning stays.
 U9. Confirmed as they are: the default material of a component without <material> comes from the preference
     store; a duplicate id is refused and the component gets a new random id; ASCII digits only; non-finite
     physical values are refused and non-finite cosmetic or stored values are kept; a custom expression
     symbol that is no regular expression loads; four of OpenRocket's eight extension providers exist; the
     instance budget and the nesting bound; ExampleFilesTest without its maximum altitudes; the zip container
     rule; simulations found by identity in the document; finite but absurd simulation inputs will be refused
     by range checks in simulate() and the loader (not yet).
 Not part of Milestone 1 (leave out; mark the place '// HOOK(monte-carlo)', '// HOOK(custom-expression)' and so
 on where a later milestone adds something): Monte Carlo and landing dispersion, evaluation of custom
 expressions, executing scripts or Java code, RockSim and RASAero files, .orc preset files, OBJ and SVG export.

Rules (in addition to CLAUDE.md, which you already have):
- One class per file: include/QtRocket/<subsystem>/ClassName.h and src/<subsystem>/ClassName.cpp, tests in
  tests/core/<subsystem>/ClassNameTests.cpp (tests that are not about one class: lower_snake_case_tests.cpp). A
  Java subpackage is a subdirectory. Add every new .cpp to its subsystem's CMakeLists.txt and every test file to
  tests/core/CMakeLists.txt, each list kept alphabetical as it is (never reorder or reformat existing lines). Everything lives in namespace
  QtRocket.
- Port OpenRocket's semantics faithfully. Read the Java source line by line; do not work from memory. Where Java
  relies on something C++ lacks, use the closest standard facility and leave a one-line comment naming the
  deviation. Each class header documents its deviations from Java, as the existing headers do.
- Port the listed JUnit tests to GoogleTest with the same expectations and tolerances; each ported test names its
  Java original in a comment. Never loosen, drop or disable an assertion to make a test pass. Add tests for
  behaviour OpenRocket did not test. Tests must be deterministic and must not leak process-wide state (default
  units, the FlightDataType registry, a preferences store shared between tests).
- For Java-pinned values that no JUnit test provides, compute them with a small Java program against OpenRocket's
  compiled core (/home/travis/Development/openrocket/core/build/classes/java/main; JDK 17 is installed), kept
  under ${PROBES}/tier9c-<your part>/. The scouts' probes (${SCOUT}/evidence/tier9-scout-*/, each with a run.sh
  or env.sh that shows the classpath and the Application/Guice set-up) are the model; many of the pins you need
  are already in their output files. Print doubles with Double.toString and paste them as literals. Put build
  and test logs there too. That directory is a RAM-backed file system shared by all agents: delete compiled
  probe binaries, object files and copies of the test binary as soon as you have their output (keep sources,
  inputs and outputs), and never leave more than about 300 MB behind. Every process you start is yours to end: run a fuzzer or a
  probe that reads hostile input under 'timeout', with a memory bound ('ulimit -v 8000000' in the shell that
  starts a binary without sanitizers; ASAN_OPTIONS=hard_rss_limit_mb=8000 for one built with them), never
  detached from your shell, and before you return check with 'pgrep -u $USER -af' that nothing of yours is
  left (a fuzz runner of run 9b was found hours later with 47 GB resident, its parent gone). Do not write report files (REPORT.md and the like): the harness refuses them; your
  findings go into your structured result.
- clang-tidy runs with warnings as errors (tidy preset; clang-tidy 23.1.1, the same as CI). Naming: types
  CamelCase; functions/methods/variables camelBack; private and protected members m_ prefix; constexpr/static/
  global constants kCamelCase; enum constants UPPER_CASE. Every getter and pure function is [[nodiscard]].
  Single-argument constructors are explicit. No raw new/delete, no pointer arithmetic, no reinterpret_cast/
  const_cast. A suppression is '// NOLINTNEXTLINE(check-name)' on its own line with the reason, never a trailing
  NOLINT. misc-include-cleaner is on: include exactly the headers whose symbols you use.
  readability-math-missing-parentheses is on. Use std::format, never printf/iostream formatting. Checks that
  earlier tiers tripped over: misc-const-correctness, performance-faster-string-find,
  readability-redundant-lambda-parameter-list, readability-redundant-parentheses, modernize-use-std-numbers (no
  literal pi), bugprone-float-loop-counter, portability-template-virtual-member-function,
  readability-function-cognitive-complexity (split long functions into helpers with the logic and the order of
  evaluation unchanged; a TEST body made only of gtest macros is not counted, but a loop, branch or lambda in it
  brings the full count, about 4 per gtest macro, and helper functions always count), concurrency-mt-unsafe (no
  std::getenv in a test: an optional measurement is a DISABLED_ test). GCC -Wcomment rejects comment lines ending
  in a backslash; GCC -O3 -Wnull-dereference flags front() on a vector built in a loop around an inlined helper.
- Files written with Bash heredocs bypass the clang-format hook, so run 'clang-format -i' on every C++ file you
  create or change, and finish with 'clang-format --dry-run --Werror' on them.
- Verify in this order and make all four pass with zero warnings: 'cmake --workflow --preset dev',
  'cmake --workflow --preset tidy', 'cmake --workflow --preset asan', 'cmake --workflow --preset ci-gcc'. Redirect
  their output to a log file and grep it. Run one preset at a time (two runs in one build directory corrupt
  ninja's log). The first configure of a fresh worktree downloads dependencies and builds everything (expected;
  the first tidy build takes a long time). Never edit anything under build/. Fix whatever you break, including
  in files you did not write. Every test of main passed before this run.
- Portability: the code must also build warning-free with MSVC (/W4 /WX /utf-8) and Apple Clang, which only CI
  builds. Traps CI has caught: MSVC's std::array/std::vector/std::string_view iterators are classes, not pointers
  (never auto* for an algorithm result or begin()/end(); search tables through a std::span or by index); Apple
  libc++ lacks std::ranges::shift_*/fold_*, std::views::zip/enumerate/chunk/slide/stride/cartesian_product/
  join_with/concat/adjacent, std::generator, <stacktrace>, flat_map, move_only_function and floating-point
  std::from_chars (parse with Strings::javaParseDouble or fast_float); MSVC may compile (a >= b) ? a : b on
  doubles to maxsd (use MathUtil::javaMax/javaMin where Java calls Math.max/min or where zero signs or NaN
  matter); no implicit narrowing (C4244/C4267); never name a local 'near' or 'far'; no compiler builtins without
  a portable fallback; write a non-ASCII character in a string literal as an escape (the alpha of a symbol, a
  multiplication sign), never as a source character. Transcendental functions and the <random> distributions
  differ between Java, glibc, Apple libm and MSVC's UCRT: exact pins (EXPECT_EQ on doubles) are valid only for
  results of +, -, *, /, sqrt and integer operations; compare the rest with a tolerance (relative 1e-12 for a
  Java-pinned value of one function call). A flight at the default 0.05 s time step is chaotic for light rockets:
  never pin a row count, a late event time or a summary value of such a flight exactly; a whole-flight pin needs
  a time step of 0.01 s or less and a tolerance derived from a measurement (see the header of
  tests/core/goldens/simulation_golden_tests.cpp for how tier 8 derived its own).
- Commit your part once when everything passes (the exact commands are at the end of your instructions). The
  next agent continues in the same worktree from your commit, so leave the tree clean.
- Your final message is consumed by a script: return the structured result only.
`

// ---------------------------------------------------------------------------------------------- group 'top-level'

const SPEC_J1 = `
PART J1, 'loader': the top-level handlers, OpenRocketLoader and GeneralRocketLoader; the public entry of file/.
Required reading: ${SCOUT}/loader-components.md (the work items "GeneralRocketLoader (sniffing, attachment choice,
final steps)", "OpenRocketLoader, OpenRocketHandler, OpenRocketContentHandler", "Load context and attachment
factories", "Byte input for SimpleSax: choose the encoding as the JDK parser does (new)" and "What tier 9 must leave
ready for the saver (tier 10)"; the sections decisions and risks), ${SCOUT}/loader-simulations.md (the work item
"SingleSimulationHandler, SimulationsHandler and the post-load steps for simulations": the post-load half),
${SCOUT}/examples.md (the work item "Containers and attachments as the examples use them"), ${SCOUT}/critic.md (in
"gaps": the entry point and the stale statements; in "contradictions": the zip container; in
"orchestratorDecisions": the load environment, the failure policy, the zip container). Evidence:
${SCOUT}/evidence/tier9-scout-loader-components/out/sniff.out and sniff2.out with sniff-work/ and sniff2-work/ (the
54 sniffing inputs and what OpenRocket makes of each), java/ and scripts/ (the probe programs and how they run).
Java (read every line): ${OR}/file/GeneralRocketLoader.java (262), AbstractRocketLoader.java (41), RocketLoader.java
(13), RocketLoadException.java (20), ${OR}/file/openrocket/importt/OpenRocketLoader.java (97), OpenRocketHandler.java
(108), OpenRocketContentHandler.java (78).
- OpenRocketHandler: the root element and its warnings ('Unknown element <e>, ignoring.', 'Multiple document
  elements found, ignoring later ones.', the unsupported-version text with the optional version and the trimmed
  creator), the file version rule (major * 100 + minor for digits.digits, else 0; '1.010' gives 110 and the
  warning), closeElement with version and creator erased from a copy of the attributes.
- OpenRocketContentHandler: rocket, datatypes and simulations once each with Java's three texts, photostudio and
  docprefs any number of times, anything else unknown; text or attributes on these elements warn through the
  inherited closeElement. Java's getDocument() answers null before <rocket> (photostudio or docprefs first is a
  NullPointerException there): in QtRocket the document exists from the start, so these elements are applied in
  whatever order they come; say so. A document without <rocket> is the empty rocket, no error.
- OpenRocketLoader: SimpleSax over the bytes with run 9a's entry (encoding as the JDK parser chooses it), the
  failure texts of decision T2 and the post-load steps of decision T5.
- GeneralRocketLoader (decisions T1 to T4, T6): the sniffing, the container rule, the attachment factory choice,
  the design's directory in the context (decision U5, with its getter, setter and test), the bounded read, the
  final steps. Use ZipInputStream (the first local header, as Java) and not the central
  directory, except for what decision T3 adds. What the entry requires of the environment is checked once, at
  the entry; what each handler does with an absent part (no preset database, no extension registry, no motor
  finder, no application materials) is in run 9b's notes and stays as it is.
- Tests (tests/core/file/GeneralRocketLoaderTests.cpp, tests/core/file/openrocket/OpenRocketLoaderTests.cpp,
  OpenRocketHandlerTests.cpp, OpenRocketContentHandlerTests.cpp): all 54 sniffing inputs with OpenRocket's
  outcome, built in the test from their description (the containers with tests/core/file/RawZip.h and the gzip
  writer; nothing new under tests/data), with the three outcomes that decision T3 changes named as such; every
  warning text of the two handlers, each by the smallest document that gives it, and the bookkeeping slip of
  decision L1 at the top level (an unknown element between <rocket> and <simulations>: pin what OpenRocket
  warns, from a Java probe); every failure text and code of decision T2, with a handler failure deep inside a
  component and inside a simulation arriving with the prefix; each post-load step by a document that makes it
  visible (a stage that a configuration deactivates, a simulation with stored data against one without, an
  extension that warns in documentLoaded, a clean undo state, the document materials after the reload), each
  pinned from a Java probe; the two forms of load() giving the same document for the same bytes; attachments
  from a zip in memory, from a directory and without one; the bounds of decision T4 with a gzip and a zip
  that state and that deliver more than the limit (make the limit a parameter of an internal function so that
  the test does not need 256 MiB); the document that the loader returns outliving the loader, the context copy,
  the motor database and the extension registry (an asan test that loads, destroys them and then walks the
  rocket, the simulations and reads a decal).
- Loose ends: every 'HOOK(loader' marker that this part resolves is removed; comments that announce the top-level
  loader as future work (grep for 'run 9c', 'tier 9', 'the top-level', 'GeneralRocketLoader', 'OpenRocketLoader'
  under include/, src/ and tests/) say what exists.
Do not touch: tests/data/goldens/** and tools/** (golden data stays byte-identical); the golden test sources under
tests/core/goldens/ (run 9d changes them; part J3 only adds GoldenExamples.h/.cpp); data/examples; the handlers and
setters of run 9b beyond what decision T6 or a defect requires (each such change is a library fix with a test).
Exit check: tests/data/ork/simplerocket.ork (gzip), one plain and one zip file of tests/data/ork and one example
of data/examples load through both forms without an Error, and every sniffing input gives OpenRocket's outcome or
the stated deviation.`

const SPEC_J2 = `
PART J2, 'recorded-files': the loader on the 18 recorded files, the integration test, hostile whole files.
Required reading: tests/data/ork/README.md and the test that guards that directory
(tests/core/file/openrocket/legacy_ork_files_tests.cpp), ${SCOUT}/examples.md (the work item "Legacy test files and
the tests on them" and the report's section on old formats), ${SCOUT}/junit-inventory.md (the work item "Port
swing's IntegrationTest.testSimpleRocket" and its tests section), ${SCOUT}/loader-components.md (the work item
"Tests: unit vectors from the probes, the Java loader tests, the examples and the committed resave files").
Evidence: ${SCOUT}/evidence/tier9-scout-examples/out/load-legacy-full.txt, load-history-full.txt,
containers-legacy.json, analysis-legacy.json, history-index.tsv (what OpenRocket makes of each recorded file with
the motor database and the presets) and java/ (the probe that printed them);
${SCOUT}/evidence/tier9-scout-junit-inventory/out/integration-probe.txt and simplerocket/.
Java (read every line): /home/travis/Development/openrocket/swing/src/test/java/info/openrocket/swing/IntegrationTest.java
(372).
- The recorded files (tests/core/file/openrocket/recorded_ork_files_tests.cpp, or the existing file extended): each
  of the 18 files of tests/data/ork loads through GeneralRocketLoader with the bundled motor database, the six
  example presets and the bundled extension registry, and gives OpenRocket's result: the file version; the load
  warnings exactly, texts included (the scouts measured 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 3, 1, 0, 0, 0, 0, 0 for the
  17 legacy files in the README's order, among them the missing motors, the preset that is not found, the three
  'Unknown attributes in element ...' of the appearance in v1.6-apocd.ork, the extension id that no provider
  knows in v1.7-simulation-extensions-and-scripting.ork); the three JavaCode extensions of
  v1.6-simulation-listeners.ork; v1.6-preset-usage-decals-first.ork under decision T3 (OpenRocket: the empty
  rocket) with its decals readable; the stored type names that become the 'Unknown' type. Where the scouts'
  outputs do not hold a number you need, extend their probe: for each file print the as-loaded state before
  anything is settled (the number of components per class in depth-first order, the stages, the configuration
  ids and names, each motor's digest or 'none', the simulations with name, status, branches, rows, events and
  warnings, the rocket's length, and with all stages active the structure mass and centre of mass and the
  launch mass of each configuration) and pin it, texts and counts exactly, the numbers at 1e-9 relative.
  Without the presets and without the motor database the same files give OpenRocket's additional warnings: pin
  the two that the scouts measured and one more of your own.
- IntegrationTest.testSimpleRocket (tests/core/file/openrocket/integration_tests.cpp): simplerocket.ork against a
  motor database that holds only tests/data/motors/Estes_A8.rse, then the 13 steps (mass change, rename, remove,
  startUndo, freeze, move, thaw, stopUndo, undo and redo), each followed by the launch centre of mass and mass,
  the worst-case centre of pressure and CNa, and ten times a simulation whose altitude has Java's tolerance of
  1.0 m. Java's values for every step are in integration-probe.txt (the JUnit literals come first; the probe
  fills what the JUnit only prints). The mass component is found again by id after each undo, as in Java. The
  simulations of this test fly in the conditions Java's test sets: if Java's flight has turbulence, set its
  standard deviation as Java does and compare with Java's tolerance; if the comparison then fails only because
  of the turbulence sequence (decision T8 explains the deviation), fly it calm, pin the calm altitude from a
  Java probe with the same change, and say so in the test and in your notes.
- Hostile whole files (tests/core/file/loader_hostile_input_tests.cpp): a deterministic sweep in the test suite,
  bounded to a few seconds under asan: three small recorded files (one plain, one gzip, one zip) truncated at
  every 97th byte and with single bytes replaced at positions and by values drawn from std::mt19937 with a fixed
  seed (raw 32-bit outputs only: the distributions are not portable), the document XML of one of them with each
  element in turn deleted and in turn duplicated; every outcome is a document or an Error, never an exception.
  A wider run of the same sweep (all recorded files, every byte) is a DISABLED_ test that you run once under
  asan and report; every defect it finds is fixed where its guard belongs (decision T6).
- Loose ends: the statements that critic.md lists under "Stale statements in QtRocket and in the plan" and that
  are still in the repository are corrected (the plan itself is outside the repository: list in your notes
  what its file paragraph should say instead).
Do not touch: as part J1; tests/data/ork (no file is added, removed or changed).
Exit check: the 18 recorded files load with OpenRocket's warnings and as-loaded state, the 13 steps of the
integration test hold Java's values, and the wide sweep ends without an exception under asan.`

const SPEC_J3 = `
PART J3, 'examples': the 16 example designs load as OpenRocket loads them; ExampleFilesTest; the fixture for run 9d.
Required reading: ${SCOUT}/examples.md (the report and the work item "ExampleFilesTests: the 16 examples load and
describe as in Java"), ${SCOUT}/junit-inventory.md (the work item "Port ExampleFilesTest, one test per example file",
its decision 1 and its tests section), ${SCOUT}/presets-setup.md (whole: the harness set-up, the fixture, the
preferences, the order of work), ${SCOUT}/golden-tests.md (the work items "Fixture that loads a golden example
(original file or re-save) for the tests", "Test helper that settles the automatic dimensions of a loaded rocket"
and "Port ExampleFilesTest as far as it holds without presets and without Java's wind noise"), ${SCOUT}/critic.md
(in "gaps": 'Test support is specified three times' and 'The one behaviour on which tier 10 depends'; in
"contradictions": the preference store, the element counts, the extensions; in "orchestratorDecisions": the example
test set-up), ${NOTES9A}/golden-groundwork.md (what run 9a left for the example goldens). Evidence:
${SCOUT}/evidence/tier9-scout-junit-inventory/out/example-table.txt, presets-seed0.txt, nopresets-seed0.txt and
presets-seeds8.txt (ExampleFilesTest's quantities from OpenRocket with and without presets and for eight seeds),
${SCOUT}/evidence/tier9-scout-presets-setup/WarnProbe.txt (the load warnings with an empty preset database),
${SCOUT}/evidence/tier9-scout-loader-simulations/ExamplesProbe.out and expected-motors.tsv (the stored simulations
and the motor each motor element gets), ${SCOUT}/evidence/tier9-scout-examples/out/load-full.txt and dump-full.txt,
and run 9b's comparison of the 54 example simulations with OpenRocket, which no committed test carries yet:
${EVID9B}/tier9b-conditions/out/examples-*.txt, ${EVID9B}/tier9b-flight-data/out/examples-*.out and
${EVID9B}/tier9b-simulations/out/examples-*.out (conditions, stored rows and events as OpenRocket loads them).
Java (read every line): ${ORT}/file/openrocket/ExampleFilesTest.java (1357), ${ORT}/startup/OpenRocketCoreTest.java
(the methods that load or simulate a file, lines 222 to 394),
/home/travis/Development/qtrocket2/tools/openrocket-goldens (GoldenDumper: the set-up of the harness, which the
fixture mirrors).
- The fixture (decision T7), tests/core/goldens/GoldenExamples.h/.cpp: the list of the 16 examples (golden
  directory name and file name, from the manifest or the golden inputs, not a second hand-written list if one
  exists); the environment, built once per test process and outliving every document it loads; a function that
  loads an example from the original or from the re-save, with the six presets or with an empty preset
  database, and returns the document with its load warnings in the as-loaded state; settling as a separate
  step. A document is loaded anew for each use (it is mutable); only the environment is shared. The header says
  what run 9d may rely on. Tests of the fixture itself: the two sources of every example load without an Error.
- ExampleFilesTest (tests/core/file/openrocket/example_files_tests.cpp), one parameterised test over the 16 files
  so that each runs in its own process, on the freshly loaded, unsettled document, in Java's order of
  statements: the load warnings by priority (0, 0, 0 for every file with the six presets); the flight
  configurations in order with name and the sorted lines '<mount name> -> <motor name> x<count>' (the three
  tables transcribed from the Java, without the dead entries of 'Base drag hack (short-wide).ork'); the design
  metrics of the configuration without motors with all stages active at Java's tolerance (structure mass,
  centre of mass, length, bounds, centre of pressure and drag coefficient at Mach 0.3, 0.8, 1.0, 1.2, 2.0, 3.0
  and zero angle of attack); per simulation the name, the launch mass and centre of mass before the run, then a
  run with seed 0 as Java's test starts it, hasErrors() false and the simulation's warning counts by priority.
  A warning count that differs from Java's only because of the turbulence sequence (decision T8: show it with
  presets-seeds8.txt or a Java run with other seeds) is not loosened: assert what is independent of the
  sequence, and report the rest in deferredTests with the measurement. The maximum altitude is not compared
  (decision T8); say so at the place where Java asserts it.
- The same files without presets: exactly the six 'No matching ComponentPreset ...' warnings, 1, 2 and 3 in the
  three files that carry presets (WarnProbe.txt has the texts), and the two geometry values that change.
- What OpenRocket's own test does not look at, pinned from the scouts' outputs or your own Java probe, per
  example: the file version; the simulations (54 in all: name, status LOADED, the configuration id, the
  extensions with their ids and the names OpenRocket shows: 6 extension elements in 4 simulations; no
  extension warning), the stored data (69 branches, 36,962 rows, 735 events, 20 warnings in all, and per
  simulation the branch names, the number of rows and events, the first and the last row of the first branch
  at 1e-12 relative, the summary values that OpenRocket recomputes on loading); the motor of each of the 65
  motor elements (tests/core/file/ExampleMotors.h: 41 references; 7 elements get another curve than their own
  by the silent designation match); the decals (17 names; the six '/datafiles/textures/...' entries of
  'Parallel booster staging.ork' readable through the document's registry with their byte counts); the document
  materials in OpenRocket's order (tier 10's <docmaterials> depends on it); the document preferences, the photo
  settings where a file has them, the default storage options, isSaved(), no undo available; and the
  as-loaded against the settled state: for 'Dual parachute deployment.ork' the structure centre of mass is
  0.885775814 as loaded and 0.8784464812860964 settled, and a Java probe gives the same pair for every example
  whose two states differ.
- The 12 methods of OpenRocketCoreTest that load or simulate an example, folded into the same file where
  ExampleFilesTest does not already cover them (each named in a comment), and findMotors("Estes", "B6") not
  empty on the bundled database. Its other methods are about Guice bindings and do not apply: list them in
  deferredTests as not applicable, once, with that reason.
- The re-saves: tests/data/goldens/example-*/resave/rocket.ork (plain XML, version 1.11) load without an Error and
  without a warning and give the same components, configurations and simulations (counts and names) as the
  original. Their comparison with the goldens is run 9d's.
Do not touch: as part J1; the existing files under tests/core/goldens/ (you add GoldenExamples.h/.cpp and their
test only); no golden comparison of the examples (design, simulations, stable step): that is run 9d.
Exit check of the group: for all 16 examples the design metrics and the launch mass and centre of mass of
ExampleFilesTest hold at Java's tolerance on the unsettled document; zero load warnings with the six presets and
exactly the six preset warnings without; 54 simulations LOADED with the stored data counts above; 65 motor elements
with OpenRocket's motor; the whole suite green in the four presets, and the time the new tests add under asan
measured and reported (the motor database is read once per test process: keep the number of processes that need
it small).`

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
    deferredTests: { type: 'array', items: { type: 'string' } },
    deviations: { type: 'array', items: { type: 'string' } },
    libraryFixes: { type: 'array', items: { type: 'string' } },
    verification: VERIFY_OBJ,
    notes: { type: 'string' },
  },
  required: ['worktreePath', 'branch', 'commit', 'files', 'tests', 'deferredTests', 'deviations', 'libraryFixes', 'verification', 'notes'],
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
          javaReference: { type: 'string' },
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
    deferredTests: { type: 'array', items: { type: 'string' } },
    verification: VERIFY_OBJ,
    notes: { type: 'string' },
  },
  required: ['branch', 'commit', 'worktreePath', 'applied', 'disputed', 'deferredTests', 'verification', 'notes'],
}

const NOT_FINDINGS = `The decisions U1 to U9, D1 to D12, L1 to L11 and T1 to T8 of the rules are NOT findings (among them: events enabled
during a load; the reproduced bookkeeping slip of the handler stack; non-finite values refused; crash paths of Java
skipped with a warning; data kept that Java drops; the bound on counts and on the document size; the zip container
rule that takes the first .ork entry; a recognised RockSim or RASAero document refused; the operating system's text
of an unreadable file not reproduced; no catch-all around the loader; ExampleFilesTest without its maximum
altitudes; warning texts with Java class names; everything decisions U5 to U9 announce for a later run: the
missing cap on warnings, the missing bounds, the generic warning text, the lookup CSV still read as before), nor
are the deviations that the notes of run 9b list for its
handlers and setters (among them the instance budget of 100000, the nesting bound of 100 levels, a duplicate id
refused with a warning, the default materials taken from the preference store, digits of other scripts not read
as digits), nor is the work of run 9d (the comparison of the examples with
the design, default-step and stable-step goldens; the re-saved test rockets) or of tier 10 (saving, the round
trip, CSV export, the command line) missing from this branch.`

const J_FILES = `${OR}/file/GeneralRocketLoader.java, AbstractRocketLoader.java, RocketLoader.java, RocketLoadException.java, DocumentLoadingContext.java, AttachmentFactory.java, FileSystemAttachmentFactory.java, ZipFileAttachmentFactory.java, ${OR}/file/openrocket/importt/OpenRocketLoader.java, OpenRocketHandler.java, OpenRocketContentHandler.java (and, below them, every handler and setter of that package, ported in run 9b), ${OR}/document/OpenRocketDocument.java, OpenRocketDocumentFactory.java, StorageOptions.java`
const J_JUNIT = `${ORT}/file/openrocket/ExampleFilesTest.java (16 cases), ${ORT}/startup/OpenRocketCoreTest.java (the 12 methods that load or simulate a file), /home/travis/Development/openrocket/swing/src/test/java/info/openrocket/swing/IntegrationTest.java (1)`

const LENS_FIDELITY = `Lens: FIDELITY AGAINST OPENROCKET, on whole files and line by line.
1. Completeness: every branch, text and step of GeneralRocketLoader.java, OpenRocketLoader.java, OpenRocketHandler.java
   and OpenRocketContentHandler.java is there, or the omission is documented and justified by the decisions. List
   the gaps (coverageGaps).
2. The as-loaded state, by differential testing. This is the check the goldens cannot make (they are settled).
   Write a Java program against the compiled core (the scouts' run.sh and env.sh show the set-up; the golden
   harness's GoldenDumper shows the preferences, the motor database and the six presets) that loads a file and
   prints, before anything is settled, every component in depth-first order with its class, name, id, axial
   offset and method, length, fore and aft radius where it has them, component mass, material name and density,
   preset part number, appearance and decal name, instance count, and for the rocket the stages, every
   configuration with its id, name, active stages and per motor mount the motor digest, delay, ignition event
   and delay, the deployment and separation settings, then per simulation the name, status, options (every
   value the conditions element can hold), extensions with their configuration, and the number of branches, rows,
   events and warnings. Read the values in the order in which OpenRocket's saver calls the getters and never ask
   for the rocket's length or bounds first (an automatic value stores what it computes). Run 9b's HandlerProbe.java
   and describeRocket() of tests/core/file/openrocket/ComponentHandlerTestSupport.h are the model
   (${EVID9B}/tier9b-fix-rocket-side/; its cases/own.txt lists what QtRocket answers otherwise on purpose). Write
   the same dump in C++ against the worktree's build/clang-debug/src/libQtRocket_core.a through
   GeneralRocketLoader. Compare the two for all 16 examples, the 16 golden re-saves
   (tests/data/goldens/example-*/resave/rocket.ork) and the 18 recorded files. Every difference that the
   decisions do not name is a finding with the file, the component and both values; a difference in a stale
   automatic value means a setter order or an event differs and is at least 'major'.
3. The load warnings of the same 50 files against OpenRocket's, text for text and in order, with the six presets
   and with an empty preset database; the sniffing inputs the implementers did not use (other entry orders,
   names in other letter case, nested containers, a byte order mark, other encodings named in the declaration).
4. The post-load steps and what a caller sees afterwards (the storage options, the saved flag, the undo state,
   the ModIds, the document materials and their order, what documentLoaded did), each against a Java probe.`

const LENS_ROBUSTNESS = `Lens: ROBUSTNESS AND HOSTILE FILES, through the public entry.
1. No BugError, exception, crash, hang or unbounded allocation for any bytes. Fuzz against the worktree's build/asan
   library through GeneralRocketLoader (both forms): mutate the recorded files and the unpacked examples at the
   byte level and at the XML level (delete, duplicate, reorder and nest elements and attributes; numbers replaced
   by NaN, infinities, huge, negative and empty values; very long names; thousands of components, stages,
   configurations, simulations and data points; a component tree a thousand levels deep), and at the container
   level (a gzip or zip that states one size and delivers another, a member that expands a million times,
   entries with the same name, names with '..', an absolute name or a drive letter, a zip inside a zip, a
   truncated stream at every position of a small file). Report every failure with its smallest input, and what
   the memory use was for the expanding inputs (decision T4).
2. Attachments and paths: what a decal name or a thrust-curve digest in a hostile file can make the loader open
   or read at load time and later through the document's registry (a plain document next to other files; a name
   that is absolute or climbs out of the directory); say exactly what is read when, and whether that is what
   OpenRocket does (FileSystemAttachmentFactory.java, DecalRegistry.java, MotorHandler.java).
3. Lifetime and ownership: what the returned document still points to after the loader, its context copy, the
   motor database, the preset database, the extension registry and the input bytes are gone, checked under asan;
   which objects the header says must outlive it and whether the code agrees; two loads at the same time on two
   threads with the same environment (which parts of the environment are written during a load: the preference
   store, the flight data type registry) under the tsan preset's flags if you can build a probe that way.
4. A failed load: nothing of the half-built document escapes, no process-wide state is left changed except what
   the decisions name, and the Error tells the user which file and why.
5. Error policy, layering (file/ may include everything; nothing below includes file/), portability (MSVC paths
   and wide file names, Apple Clang), CMake wiring, naming, one class per file, suppressions that bypass a check.`

const LENS_TESTS = `Lens: THE TESTS AGAINST THEIR SOURCES, and whether they would catch a broken loader.
1. The JUnit ports, statement by statement: ExampleFilesTest (the three tables entry by entry against the Java
   file; every assertion present except the maximum altitude; the order of statements, because the unsettled
   state depends on it; the tolerances), the 12 methods of OpenRocketCoreTest, IntegrationTest.testSimpleRocket
   (the 13 steps and every value against the Java literals and against integration-probe.txt). A pin that was
   changed to make a test pass, or an assertion that was weakened, is a blocker; a value that is not Java's must
   have its measurement and its reason next to it.
2. Every table pasted from a scout's or an implementer's probe output against that output, and a sample of them
   recomputed with your own Java run (at least: the warnings and as-loaded numbers of three recorded files, the
   stored-data pins of three examples, six motor picks including two of the seven silent matches).
3. Mutation checks. Break the loader in ways a real defect would (copy the worktree's sources into your probe
   directory and build there; never edit the worktree): skip one post-load step at a time; attach a component
   after its values instead of before; apply two setters of one component in the other order; drop the silent
   designation match; take the first zip entry only; return the settled instead of the as-loaded state from the
   fixture; ignore the preset database; lose one extension; read 299 bytes instead of 300. For each, name the
   test that fails or report that none does (a 'major' finding with the test that should exist).
4. The fixture for run 9d: does GoldenExamples give exactly the harness's set-up (compare with GoldenDumper: the
   preferences key by key, the motor database, the presets, the extension providers, the units), can two tests in
   one process disturb each other through it, and does a test that needs only one example pay for all 16.
5. Cost and isolation: the time the new tests add in the dev and asan presets, measured (ctest's own timing per
   test); tests that depend on execution order, on the current directory, on the machine's locale or time zone,
   or that write outside their temporary directory; anything left behind in the source tree.`

const GROUPS = [
  {
    name: 'top-level',
    commitSubject: 'Add the top of the .ork loader: GeneralRocketLoader, the recorded files and the 16 examples',
    java: J_FILES,
    junit: J_JUNIT,
    parts: [
      { key: 'J1', name: 'loader', spec: SPEC_J1, commit: 'WIP: top-level handlers, OpenRocketLoader and GeneralRocketLoader' },
      { key: 'J2', name: 'recorded-files', spec: SPEC_J2, commit: 'WIP: the recorded .ork files, the integration test and hostile whole files' },
      { key: 'J3', name: 'examples', spec: SPEC_J3, commit: 'WIP: the 16 examples, ExampleFilesTest and the fixture for the example goldens' },
    ],
    lenses: [
      { key: 'fidelity', text: LENS_FIDELITY },
      { key: 'robustness', text: LENS_ROBUSTNESS },
      { key: 'tests', text: LENS_TESTS },
    ],
  },
]

function partPrompt(g, i, prev) {
  const p = g.parts[i]
  const head = i === 0
    ? `You are the first of ${g.parts.length} implementer(s) of the group '${g.name}'. Your current working directory is a
fresh git worktree of the QtRocket repository. First run 'pwd', 'git status', 'git branch --show-current' and
'git log --oneline -1' (expected: ${BASE} or a descendant. A fresh worktree is made from the pushed main, which can be
behind: if 'git merge-base --is-ancestor ${BASE} HEAD' fails, run 'git merge --ff-only ${BASE}' before anything else and
check again; the commit is in this repository even when it is not pushed yet); if
the branch name is empty, run 'git checkout -b wf/tier9c-${g.name}' immediately.`
    : `You are implementer ${i + 1} of ${g.parts.length} of the group '${g.name}'. Work inside the existing worktree at
${prev.worktreePath} on branch ${prev.branch} (cd there for every command; edit files by absolute path under that
directory). First run 'git -C ${prev.worktreePath} status' and 'git -C ${prev.worktreePath} log --oneline -${i + 1}': the
tree must be clean, with the previous part's commit ${prev.commit} on top. (If it is not clean, an earlier attempt at
your part was interrupted: check what is there against your specification, do not trust it, and continue from it.)
The parts before you reported these deviations: ${JSON.stringify(prev.deviations)}, these library fixes:
${JSON.stringify(prev.libraryFixes)}, these tests still deferred: ${JSON.stringify(prev.deferredTests)}, and these
notes:
${prev.notes}`
  const whole = g.parts.map(q => `part ${q.key} '${q.name}'`).join(', ')
  return `${COMMON}
${head}
The whole group, so that you know what comes before and after you: ${whole}. You do part ${p.key}; the other parts are
not yours to implement, but the parts after you build on what you leave.
${p.spec}
Java sources of this group: ${g.java}.
JUnit files of this group: ${g.junit}.
Return in 'deferredTests' the Java test methods of this group's JUnit files that are still not ported after your part
(yours and the earlier ones that remain), each with its reason and the part or run that should port it, in
'libraryFixes' every change you made to files that existed before this run under include/ or src/ beyond what your
specification asks for, in 'deviations' every deliberate difference from the Java, and in 'notes' what the next
parts, run 9d and tier 10 need to know (API decisions with the signatures others will call, traps you found,
measurements, anything in the scout reports that turned out wrong). When done, run the four verification presets,
run clang-format --dry-run --Werror on your C++ files, then commit once: 'git add -A' and
'git commit -m "${p.commit}"' (plain separate commands). Return the structured result (worktreePath from pwd,
branch from git branch --show-current, commit from git rev-parse --short HEAD).`
}

function reviewPrompt(g, impl, lens) {
  return `You are an adversarial reviewer of the group '${g.name}' of tier 9c of a C++23 port of OpenRocket. Assume the
work contains bugs and find them. READ-ONLY on the repository: do not modify files, do not run cmake (other
reviewers share the worktree's build directories), do not commit, do not write report files. Put any probe you
write under ${PROBES}/tier9c-review-${g.name}-${lens.key}/.
Worktree with the work: ${impl.worktreePath} (branch ${impl.branch}); 'git -C ${impl.worktreePath} diff --stat ${BASE}...HEAD'
lists everything that changed, in ${impl.partsDone} commit(s) (one per part).
Java originals: ${g.java}. JUnit tests: ${g.junit}.
The implementers reported these deviations: ${JSON.stringify(impl.deviations)}, these library fixes:
${JSON.stringify(impl.libraryFixes)}, these deferred tests: ${JSON.stringify(impl.deferredTests)} and these notes:
${impl.notes}
The rules and decisions the implementers were given:
${COMMON}
The specifications of the group's parts${impl.partsDone < g.parts.length ? ` (only the first ${impl.partsDone} were implemented: review those and say so)` : ''}:
${g.parts.map(p => p.spec).join('\n')}
${NOT_FINDINGS}
${lens.text}
Report concrete findings with file path (relative to the worktree), line, severity, a precise fix, the Java
reference (file:line) and your evidence. Severity: 'blocker' = wrong results, crash, UB, a data race, lost or
changed golden data, or does not build/verify on a supported platform; 'major' = deviation from OpenRocket
semantics that the decisions do not name, a lost or vacuous check, or a missing API/test the rest of the tier will
need; 'minor' = polish. Do not report things clang-tidy or the compiler already enforce unless you can show they
were bypassed. Return the structured result only.`
}

function verifyPrompt(g, impl, serious) {
  return `You are a skeptical verifier for the group '${g.name}' of tier 9c of a C++23 port of OpenRocket, in the worktree
${impl.worktreePath} (branch ${impl.branch}). Reviewers reported the findings below. REFUTE each one if you can: read
the cited C++ and the Java original (${g.java}; JUnit: ${g.junit}), and where the claim is behavioural, prove it
either way with a probe (standalone C++ against ${impl.worktreePath}/build/clang-debug/src/libQtRocket_core.a, or with
-fsanitize=address against the library in build/asan for a lifetime claim, and a small Java program against
/home/travis/Development/openrocket/core/build/classes/java/main; JDK 17 is installed; the reviewers' probes are
under ${PROBES}/tier9c-review-${g.name}-*/, the implementers' under ${PROBES}/tier9c-*/ and the scouts' under
${SCOUT}/evidence/) under ${PROBES}/tier9c-verify-${g.name}/.
READ-ONLY on the repository: no edits, no cmake, no commits, no report files.
The decisions the implementers were bound by (a finding that only disagrees with one of them is refuted):
${NOT_FINDINGS}
For each finding return its id and a verdict: 'confirmed' (real defect, fix is right), 'modified' (real, but the fix
or severity must change: give the corrected fix), or 'refuted' (the code matches Java or the claim misreads it, or the
proposed fix would make it wrong). A finding survives only with evidence.
Findings (JSON): ${JSON.stringify(serious)}
Return the structured result only.`
}

function fixPrompt(g, impl, items, gaps, dropped) {
  return `You are the fixer for the group '${g.name}' of tier 9c of a C++23 port of OpenRocket. Work inside the existing
worktree at ${impl.worktreePath} on branch ${impl.branch} (cd there for every command; edit files by absolute path
under that directory). Java originals: ${g.java}. JUnit: ${g.junit}.
The rules the implementers followed (apply them to your changes too):
${COMMON}
The specifications they were given, including each part's 'Do not touch' list, which binds you too:
${g.parts.map(p => p.spec).join('\n')}
The implementers' notes:
${impl.notes}
Reviewers examined the work and a skeptical verifier re-checked every blocker/major finding. 'status' tells you how
each item was vetted: 'confirmed' and 'modified' (use the given fix) are verified defects: apply them; 'minor' items
were not verified: apply them unless the Java or the sources show they are wrong; 'unverified' (the verifier
failed): check against the Java first. Close the coverage gaps that are in scope. If you disagree with an item
after checking, leave it out and explain why in 'disputed'. Every fix of the library gets a regression test. Golden
data that existed before this run stays byte-identical.
Findings (JSON): ${JSON.stringify(items)}
Coverage gaps (JSON): ${JSON.stringify(gaps)}
For context only, refuted by the verifier, do NOT apply: ${JSON.stringify(dropped)}
The implementers deferred these Java tests: ${JSON.stringify(impl.deferredTests)}. Return in 'deferredTests' the
list that is still deferred after your changes (Class.method with the reason), and in 'notes' what run 9d and tier 10
need to know about this group's final state (the APIs they will call, with signatures; measurements; open points).
After the changes: 'clang-format -i' on touched C++ files, then dev, tidy, asan and ci-gcc must all pass with zero
warnings (one preset at a time). Then commit: 'git add -A' and 'git commit -m "${g.commitSubject}" -m "<short body
listing the classes, tests, library fixes and review fixes>"' (plain separate commands) and return the structured
result with the commit hash from 'git rev-parse --short HEAD'.`
}

async function implementGroup(g) {
  const done = []
  let prev = null
  for (let i = 0; i < g.parts.length; i++) {
    const p = g.parts[i]
    log(`${g.name}: part ${p.key} '${p.name}' starts`)
    const r = await agent(partPrompt(g, i, prev), {
      label: `implement:${g.name}:${p.key}`, phase: 'Implement', schema: RESULT_SCHEMA, effort: 'xhigh',
      ...(i === 0 ? { isolation: 'worktree' } : {}),
    })
    if (!r) {
      if (i === 0) throw new Error(`${g.name}: part ${p.key} returned nothing`)
      log(`WARNING ${g.name}: part ${p.key} returned nothing; its later parts are skipped and the review covers parts ${g.parts[0].key} to ${g.parts[i - 1].key}`)
      break
    }
    log(`${g.name}: part ${p.key} at ${r.commit} (${JSON.stringify(r.verification)}), ${r.libraryFixes.length} library fixes, ${r.deferredTests.length} still deferred`)
    done.push({ key: p.key, ...r })
    prev = {
      worktreePath: done[0].worktreePath,
      branch: done[0].branch,
      commit: r.commit,
      deviations: done.flatMap(d => d.deviations.map(x => `[${d.key}] ${x}`)),
      libraryFixes: done.flatMap(d => d.libraryFixes.map(x => `[${d.key}] ${x}`)),
      deferredTests: r.deferredTests,
      notes: done.map(d => `Part ${d.key}: ${d.notes}`).join('\n'),
    }
  }
  return {
    ...prev,
    partsDone: done.length,
    partCommits: done.map(d => `${d.key}: ${d.commit}`),
    files: [...new Set(done.flatMap(d => d.files))],
    verification: done[done.length - 1].verification,
  }
}

phase('Implement')
log(`Tier 9c: ${GROUPS.length} groups in isolated worktrees: ${GROUPS.map(g => `${g.name} (${g.parts.length} part(s), ${g.lenses.length} lenses)`).join(', ')}`)

const results = await pipeline(
  GROUPS,
  g => implementGroup(g),
  (impl, g) => parallel(g.lenses.map(lens => () =>
    agent(reviewPrompt(g, impl, lens), { label: `review:${g.name}:${lens.key}`, phase: 'Review', schema: REVIEW_SCHEMA })
      .then(r => (r ? { lens: lens.key, ...r } : null))))
    .then(reviews => ({ impl, reviews: reviews.filter(Boolean) })),
  ({ impl, reviews }, g) => {
    const all = reviews.flatMap(r => r.findings.map((f, i) => ({ id: `${r.lens}-${i}`, lens: r.lens, ...f })))
    const gaps = reviews.flatMap(r => r.coverageGaps.map(x => `[${r.lens}] ${x}`))
    const serious = all.filter(f => f.severity !== 'minor')
    const counts = all.reduce((c, f) => { c[f.severity] = (c[f.severity] || 0) + 1; return c }, {})
    log(`${g.name}: ${reviews.length}/${g.lenses.length} reviews, ${all.length} findings ${JSON.stringify(counts)}, ${gaps.length} gaps`)
    if (reviews.length < g.lenses.length) log(`WARNING ${g.name}: ${g.lenses.length - reviews.length} review(s) returned nothing; the fixer works from the rest`)
    const verdictsP = serious.length
      ? agent(verifyPrompt(g, impl, serious), { label: `verify:${g.name}`, phase: 'Verify', schema: VERDICT_SCHEMA }).then(v => (v ? v.verdicts : null))
      : Promise.resolve([])
    return verdictsP.then(verdicts => {
      const items = [], dropped = []
      for (const f of all) {
        if (f.severity === 'minor') { items.push({ ...f, status: 'minor' }); continue }
        const v = verdicts ? verdicts.find(x => x.id === f.id) : null
        if (!v) { items.push({ ...f, status: 'unverified' }); continue }
        if (v.verdict === 'refuted') { dropped.push({ ...f, reason: v.reason }); continue }
        items.push({ ...f, status: v.verdict, verifierReason: v.reason, ...(v.verdict === 'modified' && v.fix ? { fix: v.fix } : {}) })
      }
      log(`${g.name}: ${items.length} to fix, ${dropped.length} refuted${verdicts ? '' : ' (verifier returned nothing: serious findings are unverified)'}`)
      return { impl, items, gaps, dropped, counts, reviewCount: reviews.length, findingCount: all.length }
    })
  },
  (r, g) => agent(fixPrompt(g, r.impl, r.items, r.gaps, r.dropped), { label: `fix:${g.name}`, phase: 'Fix', schema: FIX_SCHEMA, effort: 'xhigh' })
    .then(fix => {
      if (!fix) log(`WARNING ${g.name}: the fixer returned nothing; the branch holds the implementers' commits only`)
      return { group: g.name, ...r, fix }
    }),
)

const summary = results.filter(Boolean).map(r => ({
  group: r.group,
  branch: r.fix ? r.fix.branch : r.impl.branch,
  commit: r.fix ? r.fix.commit : r.impl.commit,
  worktreePath: r.impl.worktreePath,
  partsDone: r.impl.partsDone,
  partCommits: r.impl.partCommits,
  verification: r.fix ? r.fix.verification : r.impl.verification,
  reviews: r.reviewCount,
  findings: r.findingCount,
  severities: r.counts,
  toFix: r.items.length,
  refuted: r.dropped.map(d => `${d.file}: ${d.issue}`.slice(0, 300)),
  applied: r.fix ? r.fix.applied.length : 0,
  disputed: r.fix ? r.fix.disputed : [],
  deviations: r.impl.deviations,
  libraryFixes: r.impl.libraryFixes,
  deferredTests: r.fix ? r.fix.deferredTests : r.impl.deferredTests,
  implNotes: r.impl.notes,
  fixNotes: r.fix ? r.fix.notes : 'fixer returned nothing',
}))
const failed = GROUPS.map(g => g.name).filter(n => !summary.some(s => s.group === n && s.commit))
if (failed.length) log(`Groups that did not complete: ${failed.join(', ')}`)
return { summary, failed }
