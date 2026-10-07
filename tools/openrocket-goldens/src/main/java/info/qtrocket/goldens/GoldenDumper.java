package info.qtrocket.goldens;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.FileVisitOption;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.HexFormat;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.function.Consumer;
import java.util.function.Supplier;
import java.util.stream.Stream;

import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.Module;
import com.google.inject.util.Modules;

import info.openrocket.core.ServicesForTesting;
import info.openrocket.core.database.ComponentPresetDao;
import info.openrocket.core.database.ComponentPresetDatabase;
import info.openrocket.core.database.motor.MotorDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSQLiteDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSetDatabase;
import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.OpenRocketDocumentFactory;
import info.openrocket.core.document.Simulation;
import info.openrocket.core.document.StorageOptions;
import info.openrocket.core.file.GeneralRocketLoader;
import info.openrocket.core.file.openrocket.OpenRocketSaver;
import info.openrocket.core.logging.ErrorSet;
import info.openrocket.core.logging.WarningSet;
import info.openrocket.core.models.wind.MultiLevelPinkNoiseWindModel;
import info.openrocket.core.models.wind.PinkNoiseWindModel;
import info.openrocket.core.models.wind.WindModelType;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preset.ComponentPreset;
import info.openrocket.core.preset.xml.OpenRocketComponentLoader;
import info.openrocket.core.rocketcomponent.FlightConfiguration;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.simulation.DefaultSimulationOptionFactory;
import info.openrocket.core.simulation.SimulationOptions;
import info.openrocket.core.simulation.SimulationStepperMethod;
import info.openrocket.core.startup.Application;
import info.openrocket.core.util.BuildProperties;
import info.openrocket.core.util.GeodeticComputationStrategy;
import info.openrocket.core.util.TestRockets;

/**
 * Dumps OpenRocket reference data ("goldens") for the C++ port. See README.md for what is dumped,
 * the file formats and how to regenerate.
 * <p>
 * Usage: {@code GoldenDumper --openrocket <checkout> --examples <dir> --out <dir> --work <dir>
 * [--commit <hash>] [--preset-commit <hash>] [--dirty true|false] [--stable-examples true|false]
 * [--uuid-salt <text>] [--only <input name>]...}
 */
public final class GoldenDumper {

	/** Version of the golden file schema; bump it when a field changes meaning or disappears. */
	static final int SCHEMA_VERSION = 1;

	/** Output directory prefixes owned by this dumper (anything else in the output directory is kept). */
	private static final String EXAMPLE_PREFIX = "example-";
	private static final String TEST_ROCKET_PREFIX = "testrocket-";

	/** The subdirectory of an input directory that holds the input's stable-step set. */
	private static final String STABLE_DIR = "stable";

	/**
	 * A pass over an input. The default pass writes every file of the input. The stable pass, which
	 * follows it, builds or loads the design once more from the same UUID seed and repeats the default
	 * pass call by call, so that its simulations meet the same objects with the same ids in the same
	 * state; it writes only the simulations, each run with {@link SimulationDumper#STABLE_TIME_STEP},
	 * into the input's {@value #STABLE_DIR}/ directory, and checks that every other file it would write
	 * is, byte for byte, the one the default pass wrote. Nothing the default pass writes depends on
	 * whether a stable pass follows.
	 */
	private enum Pass {
		DEFAULT, STABLE
	}

	/** The TestRockets factories, in the order and under the directory names of the goldens. */
	private static final Map<String, Supplier<Rocket>> TEST_ROCKETS = new LinkedHashMap<>();
	static {
		TEST_ROCKETS.put("estes-alpha-iii", TestRockets::makeEstesAlphaIII);
		TEST_ROCKETS.put("beta", TestRockets::makeBeta);
		TEST_ROCKETS.put("simple-2-stage", TestRockets::makeSimple2Stage);
		TEST_ROCKETS.put("big-blue", TestRockets::makeBigBlue);
		TEST_ROCKETS.put("iso-haisu", TestRockets::makeIsoHaisu);
		TEST_ROCKETS.put("falcon-9-heavy", TestRockets::makeFalcon9Heavy);
		TEST_ROCKETS.put("multi-stage-event-test-rocket", TestRockets::makeMultiStageEventTestRocket);
		TEST_ROCKETS.put("end-plate-rocket", TestRockets::makeEndPlateRocket);
		TEST_ROCKETS.put("estes-alpha-iii-with-pods", TestRockets::makeEstesAlphaIIIWithPods);
		TEST_ROCKETS.put("estes-alpha-iii-with-motor-pods", TestRockets::makeEstesAlphaIIIWithMotorPods);
		TEST_ROCKETS.put("estes-alpha-iii-with-second-motor", TestRockets::makeEstesAlphaIIIWithSecondMotor);
		TEST_ROCKETS.put("estes-alpha-iii-with-inline-pod", TestRockets::makeEstesAlphaIIIwithInlinePod);
		TEST_ROCKETS.put("cluster-pods", TestRockets::makeClusterPods);
	}

	/** The Java method each TestRockets entry calls (for the manifest). */
	private static final Map<String, String> TEST_ROCKET_METHODS = Map.ofEntries(
			Map.entry("estes-alpha-iii", "makeEstesAlphaIII"), Map.entry("beta", "makeBeta"),
			Map.entry("simple-2-stage", "makeSimple2Stage"), Map.entry("big-blue", "makeBigBlue"),
			Map.entry("iso-haisu", "makeIsoHaisu"), Map.entry("falcon-9-heavy", "makeFalcon9Heavy"),
			Map.entry("multi-stage-event-test-rocket", "makeMultiStageEventTestRocket"),
			Map.entry("end-plate-rocket", "makeEndPlateRocket"),
			Map.entry("estes-alpha-iii-with-pods", "makeEstesAlphaIIIWithPods"),
			Map.entry("estes-alpha-iii-with-motor-pods", "makeEstesAlphaIIIWithMotorPods"),
			Map.entry("estes-alpha-iii-with-second-motor", "makeEstesAlphaIIIWithSecondMotor"),
			Map.entry("estes-alpha-iii-with-inline-pod", "makeEstesAlphaIIIwithInlinePod"),
			Map.entry("cluster-pods", "makeClusterPods"));

	/**
	 * A harness-defined extra simulation of a test rocket: the default simulation of one flight
	 * configuration with one option changed, for code paths that no document simulation uses (the RK6
	 * stepper, WGS84 geodetics, the multi-level wind model). {@code description} is written into the
	 * simulation's JSON ({@code variant}).
	 */
	private record Variant(String testRocket, String configurationName, String label, String description,
			Consumer<SimulationOptions> apply) {
	}

	/** The variants, in the order they are appended to the document's simulations. */
	private static final List<Variant> VARIANTS = List.of(
			new Variant("estes-alpha-iii", "[C6-5]", "RK6 stepper", "stepperMethod = RK6",
					options -> options.setSimulationStepperMethodChoice(SimulationStepperMethod.RK6)),
			new Variant("estes-alpha-iii", "[C6-5]", "WGS84 geodetics", "geodeticComputation = WGS84",
					options -> options.setGeodeticComputation(GeodeticComputationStrategy.WGS84)),
			new Variant("estes-alpha-iii", "[C6-5]", "multi-level wind",
					"windModelType = MULTI_LEVEL; levels (altitude m, speed m/s, direction rad): "
							+ "(0, 2, pi/2), (100, 4, 2.2), (200, 6, 3)",
					GoldenDumper::useMultiLevelWind));

	private GoldenDumper() {
	}

	private static final class Arguments {
		Path openrocket;
		Path examples;
		Path out;
		Path work;
		String commit = "unknown";
		String presetCommit = "unknown";
		boolean dirty = false;
		/** Whether the stable-step set of the example inputs is written too (the test rockets' always is). */
		boolean stableExamples = false;
		/**
		 * Appended to the name of an input when the UUID sequence is seeded from it. Empty for the
		 * committed goldens; anything else gives every component another id (and OpenRocket's hash maps
		 * another order), to measure how reproducible OpenRocket's results are.
		 */
		String uuidSalt = "";
		final Set<String> only = new LinkedHashSet<>();
	}

	public static void main(String[] args) throws Exception {
		// Before anything creates a UUID (see DeterministicUuids).
		DeterministicUuids.install();
		Locale.setDefault(Locale.US);

		Arguments a = parse(args);
		Path core = a.openrocket.resolve("core");

		List<Path> exampleFiles;
		try (Stream<Path> files = Files.list(a.examples)) {
			exampleFiles = files.filter(p -> p.getFileName().toString().toLowerCase(Locale.ROOT).endsWith(".ork"))
					.sorted(Comparator.comparing(p -> p.getFileName().toString()))
					.toList();
		}
		Map<String, Path> examplesByName = new LinkedHashMap<>();
		for (Path file : exampleFiles) {
			String stem = file.getFileName().toString().replaceFirst("[.][^.]+$", "");
			if (examplesByName.put(EXAMPLE_PREFIX + slug(stem), file) != null) {
				throw new IllegalStateException("Two example files have the input name " + EXAMPLE_PREFIX + slug(stem));
			}
		}
		checkOnlyNames(a.only, examplesByName.keySet());

		useSqliteExtractionDirectory(a.work);
		Map<String, Object> databases = bootstrap(core, a.work);

		List<Map<String, Object>> inputs = new ArrayList<>();
		cleanOutput(a.out, a.only);
		for (Map.Entry<String, Path> example : examplesByName.entrySet()) {
			String name = example.getKey();
			if (!a.only.isEmpty() && !a.only.contains(name)) {
				continue;
			}
			inputs.add(dumpExample(a, name, example.getValue()));
		}
		for (Map.Entry<String, Supplier<Rocket>> entry : TEST_ROCKETS.entrySet()) {
			String name = TEST_ROCKET_PREFIX + entry.getKey();
			if (!a.only.isEmpty() && !a.only.contains(name)) {
				continue;
			}
			inputs.add(dumpTestRocket(a, name, entry.getKey(), entry.getValue()));
		}

		if (a.only.isEmpty()) {
			writeManifest(a, databases, inputs);
		} else {
			log("--only given: manifest.json not written");
		}
		log("done");
	}

	private static Arguments parse(String[] args) {
		Arguments a = new Arguments();
		for (int i = 0; i < args.length; i++) {
			String option = args[i];
			if (i + 1 >= args.length) {
				throw new IllegalArgumentException("Missing value for " + option);
			}
			String value = args[++i];
			switch (option) {
				case "--openrocket" -> a.openrocket = Path.of(value).toAbsolutePath().normalize();
				case "--examples" -> a.examples = Path.of(value).toAbsolutePath().normalize();
				case "--out" -> a.out = Path.of(value).toAbsolutePath().normalize();
				case "--work" -> a.work = Path.of(value).toAbsolutePath().normalize();
				case "--commit" -> a.commit = value;
				case "--preset-commit" -> a.presetCommit = value;
				case "--dirty" -> a.dirty = parseBoolean(option, value);
				case "--stable-examples" -> a.stableExamples = parseBoolean(option, value);
				case "--uuid-salt" -> a.uuidSalt = value;
				case "--only" -> a.only.add(value);
				default -> throw new IllegalArgumentException("Unknown option " + option);
			}
		}
		if (a.openrocket == null || a.examples == null || a.out == null || a.work == null) {
			throw new IllegalArgumentException(
					"Usage: GoldenDumper --openrocket <dir> --examples <dir> --out <dir> --work <dir> "
							+ "[--commit <hash>] [--preset-commit <hash>] [--dirty true|false] "
							+ "[--stable-examples true|false] [--uuid-salt <text>] [--only <name>]...");
		}
		return a;
	}

	private static boolean parseBoolean(String option, String value) {
		return switch (value) {
			case "true" -> true;
			case "false" -> false;
			default -> throw new IllegalArgumentException(option + " expects true or false, not " + value);
		};
	}

	/**
	 * Fails (before anything is deleted or written) when an {@code --only} name matches no input, listing
	 * the valid names: a typo would otherwise skip every input and still report success.
	 */
	private static void checkOnlyNames(Set<String> only, Set<String> exampleNames) {
		List<String> valid = new ArrayList<>(exampleNames);
		for (String key : TEST_ROCKETS.keySet()) {
			valid.add(TEST_ROCKET_PREFIX + key);
		}
		List<String> unknown = only.stream().filter(name -> !valid.contains(name)).toList();
		if (!unknown.isEmpty()) {
			throw new IllegalArgumentException("--only: no input named " + String.join(", ", unknown)
					+ "; the inputs are: " + String.join(", ", valid));
		}
	}

	/**
	 * Gives sqlite-jdbc a per-process directory for its native library. It extracts the library into
	 * java.io.tmpdir under a name built from UUID.randomUUID(), which DeterministicUuids makes the same
	 * in every process, so concurrent dumpers would otherwise share (and delete) one file. The
	 * directory is deleted when the JVM exits (after the extracted files, which sqlite-jdbc registers
	 * for deletion later).
	 */
	private static void useSqliteExtractionDirectory(Path work) throws IOException {
		Path dir = work.resolve("sqlite-native-" + ProcessHandle.current().pid());
		Files.createDirectories(dir);
		dir.toFile().deleteOnExit();
		System.setProperty("org.sqlite.tmpdir", dir.toString());
	}

	static void log(String message) {
		System.err.println("[goldens] " + message);
	}

	/**
	 * A file-system-safe name: lower-case ASCII letters and digits, every other run of characters
	 * replaced by one '-'.
	 */
	static String slug(String text) {
		StringBuilder sb = new StringBuilder();
		boolean dash = false;
		for (char c : text.toLowerCase(Locale.ROOT).toCharArray()) {
			if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
				if (dash && sb.length() > 0) {
					sb.append('-');
				}
				dash = false;
				sb.append(c);
			} else {
				dash = true;
			}
		}
		return sb.length() == 0 ? "unnamed" : sb.toString();
	}

	// ---------------------------------------------------------------------------------------------
	// OpenRocket bootstrap

	/**
	 * Sets up OpenRocket's Application exactly as its tests do: BaseTestCase.setUp() (Guice with
	 * ServicesForTesting overridden by PluginModule), then, as ExampleFilesTest.setUp() does for the
	 * example files, a second injector that also binds the component preset database (the .orc files
	 * of {@link #presetFiles}, sorted) and the bundled thrust curve database (initial_motors.db, read
	 * with ThrustCurveMotorSQLiteDatabase from a private copy).
	 */
	private static Map<String, Object> bootstrap(Path core, Path work) throws Exception {
		// BaseTestCase.setUp()
		Module testModule = new ServicesForTesting();
		Application.setInjector(Guice.createInjector(Modules.override(testModule).with(new PluginModule())));

		Map<String, Path> presetFiles = presetFiles(core);
		ComponentPresetDatabase presets = new ComponentPresetDatabase();
		OpenRocketComponentLoader presetLoader = new OpenRocketComponentLoader();
		for (Map.Entry<String, Path> entry : presetFiles.entrySet()) {
			try (InputStream is = Files.newInputStream(entry.getValue())) {
				List<ComponentPreset> loaded = new ArrayList<>(
						presetLoader.load(is, entry.getValue().getFileName().toString()));
				presets.addAll(loaded);
			}
		}

		Path bundledDb = core.resolve("src/main/resources/datafiles/thrustcurves/initial_motors.db");
		Files.createDirectories(work);
		Path dbCopy = work.resolve("initial_motors.db");
		Files.copy(bundledDb, dbCopy, StandardCopyOption.REPLACE_EXISTING);
		List<ThrustCurveMotor> motorList = ThrustCurveMotorSQLiteDatabase.readDatabase(dbCopy.toFile());
		ThrustCurveMotorSetDatabase motors = new ThrustCurveMotorSetDatabase();
		for (ThrustCurveMotor motor : motorList) {
			motors.addMotor(motor);
		}

		// ExampleFilesTest.setUp()
		Module applicationModule = new ServicesForTesting();
		Module pluginModule = new PluginModule();
		Module dbOverrides = new AbstractModule() {
			@Override
			protected void configure() {
				bind(ComponentPresetDao.class).toInstance(presets);
				bind(ThrustCurveMotorSetDatabase.class).toInstance(motors);
				bind(MotorDatabase.class).to(ThrustCurveMotorSetDatabase.class);
			}
		};
		Injector injector = Guice.createInjector(Modules.override(applicationModule).with(dbOverrides), pluginModule);
		Application.setInjector(injector);

		Map<String, Object> o = Json.object();
		o.put("motorDatabase", "core/src/main/resources/datafiles/thrustcurves/initial_motors.db");
		o.put("motorDatabaseSha256", sha256(bundledDb));
		o.put("motorCount", motorList.size());
		o.put("presetFiles", new ArrayList<Object>(presetFiles.keySet()));
		Map<String, Object> presetSha256 = Json.object();
		for (Map.Entry<String, Path> entry : presetFiles.entrySet()) {
			presetSha256.put(entry.getKey(), sha256(entry.getValue()));
		}
		o.put("presetFileSha256", presetSha256);
		o.put("presetCount", presets.listAll().size());
		log("motor database: " + motorList.size() + " motors; presets: " + presets.listAll().size());
		return o;
	}

	/** The build copy of the preset submodule inside core/src/main/resources/datafiles/components. */
	private static final String PRESET_DATABASE_DIR = "database";

	/**
	 * The .orc files ExampleFilesTest loads (every file under core/src/main/resources/datafiles/components,
	 * sorted by path), taken from tracked sources only: internal/ (and any other tracked directory)
	 * from that directory, and database/ from the openrocket-database submodule's orc/ directory,
	 * walked recursively (*.bak files, which OpenRocket's externalComponentsCopy leaves out, never end
	 * in .orc). The database/ copy that OpenRocket's build leaves in core/src/main/resources is git-ignored
	 * and may come from another submodule commit (the copy task can run {@code git submodule update
	 * --remote}), so it is never read. Symbolic links are followed.
	 */
	private static Map<String, Path> presetFiles(Path core) throws IOException {
		Path components = core.resolve("src/main/resources/datafiles/components");
		Path buildCopy = components.resolve(PRESET_DATABASE_DIR);
		Path submodule = core.resolve("resources-src/datafiles/openrocket-database/orc");
		if (!Files.isDirectory(submodule)) {
			throw new IOException("The openrocket-database submodule is missing (" + submodule
					+ "); run: git -C <OpenRocket checkout> submodule update --init");
		}
		TreeMap<String, Path> files = new TreeMap<>();
		try (Stream<Path> walk = Files.walk(components, FileVisitOption.FOLLOW_LINKS)) {
			walk.filter(p -> !p.startsWith(buildCopy)).filter(GoldenDumper::isPresetFile)
					.forEach(p -> files.put(components.relativize(p).toString().replace('\\', '/'), p));
		}
		try (Stream<Path> walk = Files.walk(submodule, FileVisitOption.FOLLOW_LINKS)) {
			walk.filter(GoldenDumper::isPresetFile).forEach(p -> files.put(
					PRESET_DATABASE_DIR + "/" + submodule.relativize(p).toString().replace('\\', '/'), p));
		}
		return files;
	}

	private static boolean isPresetFile(Path p) {
		return Files.isRegularFile(p) && p.getFileName().toString().toLowerCase(Locale.ROOT).endsWith(".orc");
	}

	// ---------------------------------------------------------------------------------------------
	// Inputs

	private static Map<String, Object> dumpExample(Arguments a, String name, Path file) throws Exception {
		log(name);
		Map<String, Object> entry = Json.object();
		entry.put("name", name);
		entry.put("kind", "example");
		entry.put("source", "data/examples/" + file.getFileName());
		entry.put("sourceSha256", sha256(file));

		List<Object> simulations = examplePass(a, name, file, Pass.DEFAULT, entry);
		entry.put("simulations", simulations);
		// An input without a stable-step set has no "stableSimulations" (see "stableSimulationsOf").
		if (a.stableExamples) {
			log(name + ": stable time step");
			List<Object> stableSimulations = examplePass(a, name, file, Pass.STABLE, entry);
			entry.put("stableSimulations", stableSimulations);
		}
		return entry;
	}

	/** One pass over an example design; returns the manifest entries of the simulations it wrote. */
	private static List<Object> examplePass(Arguments a, String name, Path file, Pass pass, Map<String, Object> entry)
			throws Exception {
		DeterministicUuids.reseed(name + a.uuidSalt);
		GeneralRocketLoader loader = new GeneralRocketLoader(file.toFile());
		OpenRocketDocument doc = loader.load();

		Path dir = a.out.resolve(name);
		resave(pass, dir, doc);
		Rocket rocket = doc.getRocket();
		ComponentIndex index = new ComponentIndex(rocket);
		dumpDesign(pass, dir, name, rocket, index, loader.getWarnings(), entry);

		List<Object> sims = Json.array();
		int simIndex = 0;
		for (Simulation sim : doc.getSimulations()) {
			Map<String, Object> harness = SimulationDumper.makeReproducible(sim.getOptions());
			sims.add(dumpSimulation(pass, dir, name, simIndex, sim, rocket, index, "document", null, harness));
			simIndex++;
		}
		return sims;
	}

	private static Map<String, Object> dumpTestRocket(Arguments a, String name, String key, Supplier<Rocket> factory)
			throws Exception {
		log(name);
		Map<String, Object> entry = Json.object();
		entry.put("name", name);
		entry.put("kind", "testrocket");
		entry.put("source", "info.openrocket.core.util.TestRockets." + TEST_ROCKET_METHODS.get(key) + "()");
		entry.put("sourceSha256", null);

		List<Object> simulations = testRocketPass(a, name, key, factory, Pass.DEFAULT, entry);
		entry.put("simulations", simulations);
		log(name + ": stable time step");
		List<Object> stableSimulations = testRocketPass(a, name, key, factory, Pass.STABLE, entry);
		entry.put("stableSimulations", stableSimulations);
		return entry;
	}

	/** One pass over a test rocket; returns the manifest entries of the simulations it wrote. */
	private static List<Object> testRocketPass(Arguments a, String name, String key, Supplier<Rocket> factory,
			Pass pass, Map<String, Object> entry) throws Exception {
		DeterministicUuids.reseed(name + a.uuidSalt);
		Rocket rocket = factory.get();
		OpenRocketDocument doc = OpenRocketDocumentFactory.createDocumentFromRocket(rocket);

		// A default simulation per flight configuration (the default configuration included), with
		// the options a fresh OpenRocket installation gives a new simulation, then the variants.
		SimulationOptions defaultOptions = applicationDefaultOptions();
		List<Map<String, Object>> harnesses = new ArrayList<>();
		List<String> variants = new ArrayList<>();
		for (FlightConfiguration config : rocket.getFlightConfigurations()) {
			Simulation sim = new Simulation(doc, rocket);
			sim.setFlightConfigurationId(config.getId());
			sim.setName(config.getName());
			sim.getOptions().copyConditionsFrom(defaultOptions);
			harnesses.add(SimulationDumper.makeReproducible(sim.getOptions()));
			variants.add(null);
			doc.addSimulation(sim);
		}
		for (Variant variant : VARIANTS) {
			if (!variant.testRocket().equals(key)) {
				continue;
			}
			FlightConfiguration config = configurationNamed(rocket, variant.configurationName());
			Simulation sim = new Simulation(doc, rocket);
			sim.setFlightConfigurationId(config.getId());
			sim.setName(config.getName() + " " + variant.label());
			sim.getOptions().copyConditionsFrom(defaultOptions);
			variant.apply().accept(sim.getOptions());
			harnesses.add(SimulationDumper.makeReproducible(sim.getOptions()));
			variants.add(variant.description());
			doc.addSimulation(sim);
		}

		Path dir = a.out.resolve(name);
		resave(pass, dir, doc);
		ComponentIndex index = new ComponentIndex(rocket);
		dumpDesign(pass, dir, name, rocket, index, new WarningSet(), entry);

		List<Object> sims = Json.array();
		int simIndex = 0;
		for (Simulation sim : doc.getSimulations()) {
			sims.add(dumpSimulation(pass, dir, name, simIndex, sim, rocket, index, "applicationDefaults",
					variants.get(simIndex), harnesses.get(simIndex)));
			simIndex++;
		}
		return sims;
	}

	/**
	 * Runs and writes one simulation of a pass and returns its manifest entry. The stable pass gives it
	 * the stable time step first, at the last moment before the run, and writes into the input's
	 * {@value #STABLE_DIR}/ directory.
	 */
	private static Map<String, Object> dumpSimulation(Pass pass, Path dir, String name, int simIndex, Simulation sim,
			Rocket rocket, ComponentIndex index, String optionsSource, String variant, Map<String, Object> harness)
			throws IOException {
		Path simulationDir = dir;
		String prefix = name + "/";
		if (pass == Pass.STABLE) {
			SimulationDumper.useStableTimeStep(sim.getOptions(), harness);
			simulationDir = dir.resolve(STABLE_DIR);
			prefix = name + "/" + STABLE_DIR + "/";
		}
		return simulationEntry(prefix, sim, SimulationDumper.dump(simulationDir, name, simIndex, sim, rocket, index,
				optionsSource, variant, harness));
	}

	/**
	 * The options a fresh OpenRocket installation gives a new simulation: those of
	 * {@link DefaultSimulationOptionFactory} over {@link ApplicationDefaultsPreferences}, plus the other
	 * application defaults a new simulation reads, and the multi-level wind model's initial level.
	 * {@code MultiLevelPinkNoiseWindModel} takes that level from the preferences that were current
	 * when the class was loaded (here the test preferences, which answer 0), so it is set explicitly
	 * to what an installed OpenRocket gives: one level at 0 m with the average wind's speed,
	 * direction and standard deviation.
	 */
	private static SimulationOptions applicationDefaultOptions() {
		ApplicationDefaultsPreferences defaults = new ApplicationDefaultsPreferences();
		SimulationOptions options = new DefaultSimulationOptionFactory(defaults).getDefault();
		options.setTimeStep(defaults.getTimeStep());
		options.setMaxSimulationTime(defaults.getMaxSimulationTime());
		options.setGeodeticComputation(defaults.getGeodeticComputation());
		options.setGravityModelType(defaults.getGravityModel());
		options.setConstantGravity(defaults.getConstantGravityValue());
		PinkNoiseWindModel averageWind = defaults.getAverageWindModel();
		MultiLevelPinkNoiseWindModel multiLevel = options.getMultiLevelWindModel();
		multiLevel.clearLevels();
		multiLevel.addWindLevel(0, averageWind.getAverage(), averageWind.getDirection(),
				averageWind.getStandardDeviation());
		return options;
	}

	/** The multi-level wind variant: three levels up to 200 m (the harness makes them calm). */
	private static void useMultiLevelWind(SimulationOptions options) {
		MultiLevelPinkNoiseWindModel multiLevel = options.getMultiLevelWindModel();
		multiLevel.clearLevels();
		multiLevel.addWindLevel(0, 2.0, Math.PI / 2, 0.0);
		multiLevel.addWindLevel(100, 4.0, 2.2, 0.0);
		multiLevel.addWindLevel(200, 6.0, 3.0, 0.0);
		options.setWindModelType(WindModelType.MULTI_LEVEL);
	}

	private static FlightConfiguration configurationNamed(Rocket rocket, String name) {
		for (FlightConfiguration config : rocket.getFlightConfigurations()) {
			if (config.getName().equals(name)) {
				return config;
			}
		}
		throw new IllegalStateException("No flight configuration named " + name + " in " + rocket.getName());
	}

	private static void dumpDesign(Pass pass, Path dir, String name, Rocket rocket, ComponentIndex index,
			WarningSet loadWarnings, Map<String, Object> entry) throws IOException {
		// Geometry, mass and aero describe the settled automatic dimensions (see AutomaticDimensions).
		int changedPasses = AutomaticDimensions.settle(rocket, index);
		if (changedPasses > 0 && pass == Pass.DEFAULT) {
			log("  automatic dimensions: " + changedPasses + " settling pass(es) changed values");
		}
		writeOrVerify(pass, dir.resolve("geometry.json"),
				Json.bytes(GeometryDumper.dump(name, rocket, index, loadWarnings)));
		writeOrVerify(pass, dir.resolve("mass.json"), Json.bytes(MassDumper.dump(name, rocket, index)));
		writeOrVerify(pass, dir.resolve("aero.json"), Json.bytes(AeroDumper.dump(name, rocket, index)));
		if (pass == Pass.DEFAULT) {
			entry.put("geometry", name + "/geometry.json");
			entry.put("mass", name + "/mass.json");
			entry.put("aero", name + "/aero.json");
			entry.put("resave", name + "/resave/rocket.ork");
		}
	}

	/** The manifest entry of a simulation whose files {@code files} are in the directory {@code prefix}. */
	private static Map<String, Object> simulationEntry(String prefix, Simulation sim, List<String> files) {
		Map<String, Object> o = Json.object();
		o.put("name", sim.getName());
		o.put("json", prefix + files.get(0));
		List<Object> branches = Json.array();
		for (String file : files.subList(1, files.size())) {
			branches.add(prefix + file);
		}
		o.put("branches", branches);
		return o;
	}

	/**
	 * resave/rocket.ork: OpenRocketSaver's XML for the document as loaded (as built, for TestRockets),
	 * without simulation data (StorageOptions.setSaveSimulationData(false)).
	 */
	private static void resave(Pass pass, Path dir, OpenRocketDocument doc) throws IOException {
		StorageOptions options = new StorageOptions();
		options.setSaveSimulationData(false);
		WarningSet warnings = new WarningSet();
		ErrorSet errors = new ErrorSet();
		ByteArrayOutputStream out = new ByteArrayOutputStream(1 << 16);
		new OpenRocketSaver().save(out, doc, options, warnings, errors);
		if (!errors.isEmpty()) {
			throw new IOException("OpenRocketSaver reported errors: " + errors);
		}
		writeOrVerify(pass, dir.resolve("resave/rocket.ork"), out.toByteArray());
	}

	/**
	 * The default pass writes {@code bytes} to {@code file}. The stable pass writes nothing here: it
	 * checks that the default pass wrote these very bytes, which is what makes it a repetition of the
	 * default pass (the same components with the same ids in the same state).
	 */
	private static void writeOrVerify(Pass pass, Path file, byte[] bytes) throws IOException {
		if (pass == Pass.DEFAULT) {
			Files.createDirectories(file.getParent());
			Files.write(file, bytes);
		} else if (!Arrays.equals(bytes, Files.readAllBytes(file))) {
			throw new IllegalStateException(
					"The stable pass does not repeat the default pass: it would write another " + file);
		}
	}

	// ---------------------------------------------------------------------------------------------
	// Output

	/** Deletes the directories this dumper writes (all of them, or only the --only ones). */
	private static void cleanOutput(Path out, Set<String> only) throws IOException {
		Files.createDirectories(out);
		List<Path> dirs;
		try (Stream<Path> list = Files.list(out)) {
			dirs = list.filter(Files::isDirectory).filter(p -> {
				String n = p.getFileName().toString();
				boolean ours = n.startsWith(EXAMPLE_PREFIX) || n.startsWith(TEST_ROCKET_PREFIX);
				return ours && (only.isEmpty() || only.contains(n));
			}).toList();
		}
		for (Path dir : dirs) {
			try (Stream<Path> walk = Files.walk(dir)) {
				for (Path p : walk.sorted(Comparator.reverseOrder()).toList()) {
					Files.delete(p);
				}
			}
		}
		if (only.isEmpty()) {
			Files.deleteIfExists(out.resolve("manifest.json"));
		}
	}

	private static void writeManifest(Arguments a, Map<String, Object> databases, List<Map<String, Object>> inputs)
			throws IOException {
		Map<String, Object> root = Json.object();
		root.put("schema", "manifest");
		root.put("schemaVersion", SCHEMA_VERSION);
		root.put("generator", "tools/openrocket-goldens/generate.sh (GoldenDumper)");

		Map<String, Object> openrocket = Json.object();
		openrocket.put("commit", a.commit);
		openrocket.put("version", BuildProperties.getVersion());
		openrocket.put("presetDatabaseCommit", a.presetCommit);
		openrocket.put("dirty", a.dirty);
		openrocket.put("javaVersion", System.getProperty("java.version"));
		root.put("openrocket", openrocket);
		root.put("databases", databases);

		Map<String, Object> settings = Json.object();
		settings.put("machs", Json.numbers(AeroDumper.MACHS));
		settings.put("aoasDeg", Json.numbers(AeroDumper.AOAS_DEG));
		settings.put("simulationRandomSeed", SimulationDumper.RANDOM_SEED);
		settings.put("windStandardDeviation", 0.0);
		settings.put("pitchYawJitterRemoved", true);
		settings.put("resaveIncludesSimulationData", false);
		settings.put("stableTimeStep", SimulationDumper.STABLE_TIME_STEP);
		List<Object> stableKinds = Json.array();
		if (a.stableExamples) {
			stableKinds.add("example");
		}
		stableKinds.add("testrocket");
		settings.put("stableSimulationsOf", stableKinds);
		if (!a.uuidSalt.isEmpty()) {
			settings.put("uuidSalt", a.uuidSalt);
		}
		root.put("settings", settings);

		Map<String, Object> tolerances = Json.object();
		tolerances.put("geometryMassAeroRelative", 1e-9);
		tolerances.put("cpCgAbsoluteMetres", 1e-9);
		tolerances.put("tableInterpolatedAeroRelative", 1e-7);
		tolerances.put("eventTimeAbsoluteSeconds", 1e-3);
		tolerances.put("apogeeMaxVelocityRelative", 1e-4);
		tolerances.put("noisyRunsRelative", 2e-2);
		root.put("tolerances", tolerances);

		root.put("inputs", new ArrayList<Object>(inputs));
		Json.write(a.out.resolve("manifest.json"), root);
	}

	static String sha256(Path file) throws IOException {
		try {
			MessageDigest digest = MessageDigest.getInstance("SHA-256");
			return HexFormat.of().formatHex(digest.digest(Files.readAllBytes(file)));
		} catch (NoSuchAlgorithmException e) {
			throw new IllegalStateException(e);
		}
	}
}
