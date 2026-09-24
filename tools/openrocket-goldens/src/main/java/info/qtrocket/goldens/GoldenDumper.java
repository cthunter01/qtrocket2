package info.qtrocket.goldens;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HexFormat;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
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
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preset.ComponentPreset;
import info.openrocket.core.preset.xml.OpenRocketComponentLoader;
import info.openrocket.core.rocketcomponent.FlightConfiguration;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.simulation.DefaultSimulationOptionFactory;
import info.openrocket.core.simulation.SimulationOptions;
import info.openrocket.core.startup.Application;
import info.openrocket.core.util.BuildProperties;
import info.openrocket.core.util.TestRockets;

/**
 * Dumps OpenRocket reference data ("goldens") for the C++ port. See README.md for what is dumped,
 * the file formats and how to regenerate.
 * <p>
 * Usage: {@code GoldenDumper --openrocket <checkout> --examples <dir> --out <dir> --work <dir>
 * [--commit <hash>] [--preset-commit <hash>] [--only <input name>]...}
 */
public final class GoldenDumper {

	/** Version of the golden file schema; bump it when a field changes meaning or disappears. */
	static final int SCHEMA_VERSION = 1;

	/** Output directory prefixes owned by this dumper (anything else in the output directory is kept). */
	private static final String EXAMPLE_PREFIX = "example-";
	private static final String TEST_ROCKET_PREFIX = "testrocket-";

	/** The TestRockets factories, in the order and under the directory names of the goldens. */
	private static final Map<String, Supplier<Rocket>> TEST_ROCKETS = new java.util.LinkedHashMap<>();
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

	private GoldenDumper() {
	}

	private static final class Arguments {
		Path openrocket;
		Path examples;
		Path out;
		Path work;
		String commit = "unknown";
		String presetCommit = "unknown";
		final Set<String> only = new LinkedHashSet<>();
	}

	public static void main(String[] args) throws Exception {
		// Before anything creates a UUID (see DeterministicUuids).
		DeterministicUuids.install();
		Locale.setDefault(Locale.US);

		Arguments a = parse(args);
		Path core = a.openrocket.resolve("core");

		Map<String, Object> databases = bootstrap(core, a.work);

		List<Map<String, Object>> inputs = new ArrayList<>();
		List<Path> exampleFiles;
		try (Stream<Path> files = Files.list(a.examples)) {
			exampleFiles = files.filter(p -> p.getFileName().toString().toLowerCase(Locale.ROOT).endsWith(".ork"))
					.sorted(Comparator.comparing(p -> p.getFileName().toString()))
					.toList();
		}

		cleanOutput(a.out, a.only);
		for (Path file : exampleFiles) {
			String stem = file.getFileName().toString().replaceFirst("[.][^.]+$", "");
			String name = EXAMPLE_PREFIX + slug(stem);
			if (!a.only.isEmpty() && !a.only.contains(name)) {
				continue;
			}
			inputs.add(dumpExample(a, name, file));
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
				case "--only" -> a.only.add(value);
				default -> throw new IllegalArgumentException("Unknown option " + option);
			}
		}
		if (a.openrocket == null || a.examples == null || a.out == null || a.work == null) {
			throw new IllegalArgumentException(
					"Usage: GoldenDumper --openrocket <dir> --examples <dir> --out <dir> --work <dir> "
							+ "[--commit <hash>] [--preset-commit <hash>] [--only <name>]...");
		}
		return a;
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
	 * example files, a second injector that also binds the component preset database (every .orc file
	 * under core/src/main/resources/datafiles/components, sorted) and the bundled thrust curve
	 * database (initial_motors.db, read with ThrustCurveMotorSQLiteDatabase from a private copy).
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
		o.put("presetCount", presets.listAll().size());
		log("motor database: " + motorList.size() + " motors; presets: " + presets.listAll().size());
		return o;
	}

	/**
	 * The .orc files ExampleFilesTest loads: every file under core/src/main/resources/datafiles/components
	 * (the tracked internal/ directory and database/, which OpenRocket's build copies from the
	 * openrocket-database submodule), sorted by path. When database/ has not been copied (a checkout
	 * that was never built) the submodule's orc/ directory stands in for it.
	 */
	private static Map<String, Path> presetFiles(Path core) throws IOException {
		Path components = core.resolve("src/main/resources/datafiles/components");
		TreeMap<String, Path> files = new TreeMap<>();
		try (Stream<Path> walk = Files.walk(components)) {
			walk.filter(p -> p.getFileName().toString().toLowerCase(Locale.ROOT).endsWith(".orc"))
					.forEach(p -> files.put(components.relativize(p).toString().replace('\\', '/'), p));
		}
		if (!Files.isDirectory(components.resolve("database"))) {
			Path submodule = core.resolve("resources-src/datafiles/openrocket-database/orc");
			try (Stream<Path> list = Files.list(submodule)) {
				list.filter(p -> p.getFileName().toString().toLowerCase(Locale.ROOT).endsWith(".orc"))
						.forEach(p -> files.put("database/" + p.getFileName(), p));
			}
		}
		return files;
	}

	// ---------------------------------------------------------------------------------------------
	// Inputs

	private static Map<String, Object> dumpExample(Arguments a, String name, Path file) throws Exception {
		log(name);
		DeterministicUuids.reseed(name);
		GeneralRocketLoader loader = new GeneralRocketLoader(file.toFile());
		OpenRocketDocument doc = loader.load();

		Path dir = a.out.resolve(name);
		Map<String, Object> entry = Json.object();
		entry.put("name", name);
		entry.put("kind", "example");
		entry.put("source", "data/examples/" + file.getFileName());
		entry.put("sourceSha256", sha256(file));

		resave(dir, doc);
		Rocket rocket = doc.getRocket();
		ComponentIndex index = new ComponentIndex(rocket);
		dumpDesign(dir, name, rocket, index, loader.getWarnings(), entry);

		List<Object> sims = Json.array();
		int simIndex = 0;
		for (Simulation sim : doc.getSimulations()) {
			Map<String, Object> harness = SimulationDumper.makeReproducible(sim.getOptions());
			sims.add(simulationEntry(name, sim, SimulationDumper.dump(dir, name, simIndex, sim, rocket, index,
					"document", harness)));
			simIndex++;
		}
		entry.put("simulations", sims);
		return entry;
	}

	private static Map<String, Object> dumpTestRocket(Arguments a, String name, String key, Supplier<Rocket> factory)
			throws Exception {
		log(name);
		DeterministicUuids.reseed(name);
		Rocket rocket = factory.get();
		OpenRocketDocument doc = OpenRocketDocumentFactory.createDocumentFromRocket(rocket);

		// A default simulation per flight configuration (the default configuration included), with
		// the options a fresh OpenRocket installation gives a new simulation.
		ApplicationDefaultsPreferences defaults = new ApplicationDefaultsPreferences();
		SimulationOptions defaultOptions = new DefaultSimulationOptionFactory(defaults).getDefault();
		defaultOptions.setTimeStep(defaults.getTimeStep());
		defaultOptions.setMaxSimulationTime(defaults.getMaxSimulationTime());
		defaultOptions.setGeodeticComputation(defaults.getGeodeticComputation());
		defaultOptions.setGravityModelType(defaults.getGravityModel());
		defaultOptions.setConstantGravity(defaults.getConstantGravityValue());
		List<Map<String, Object>> harnesses = new ArrayList<>();
		for (FlightConfiguration config : rocket.getFlightConfigurations()) {
			Simulation sim = new Simulation(doc, rocket);
			sim.setFlightConfigurationId(config.getId());
			sim.setName(config.getName());
			sim.getOptions().copyConditionsFrom(defaultOptions);
			harnesses.add(SimulationDumper.makeReproducible(sim.getOptions()));
			doc.addSimulation(sim);
		}

		Path dir = a.out.resolve(name);
		Map<String, Object> entry = Json.object();
		entry.put("name", name);
		entry.put("kind", "testrocket");
		entry.put("source", "info.openrocket.core.util.TestRockets." + TEST_ROCKET_METHODS.get(key) + "()");
		entry.put("sourceSha256", null);

		resave(dir, doc);
		ComponentIndex index = new ComponentIndex(rocket);
		dumpDesign(dir, name, rocket, index, new WarningSet(), entry);

		List<Object> sims = Json.array();
		int simIndex = 0;
		for (Simulation sim : doc.getSimulations()) {
			sims.add(simulationEntry(name, sim, SimulationDumper.dump(dir, name, simIndex, sim, rocket, index,
					"applicationDefaults", harnesses.get(simIndex))));
			simIndex++;
		}
		entry.put("simulations", sims);
		return entry;
	}

	private static void dumpDesign(Path dir, String name, Rocket rocket, ComponentIndex index, WarningSet loadWarnings,
			Map<String, Object> entry) throws IOException {
		Json.write(dir.resolve("geometry.json"), GeometryDumper.dump(name, rocket, index, loadWarnings));
		Json.write(dir.resolve("mass.json"), MassDumper.dump(name, rocket, index));
		Json.write(dir.resolve("aero.json"), AeroDumper.dump(name, rocket, index));
		entry.put("geometry", name + "/geometry.json");
		entry.put("mass", name + "/mass.json");
		entry.put("aero", name + "/aero.json");
		entry.put("resave", name + "/resave/rocket.ork");
	}

	private static Map<String, Object> simulationEntry(String name, Simulation sim, List<String> files) {
		Map<String, Object> o = Json.object();
		o.put("name", sim.getName());
		o.put("json", name + "/" + files.get(0));
		List<Object> branches = Json.array();
		for (String file : files.subList(1, files.size())) {
			branches.add(name + "/" + file);
		}
		o.put("branches", branches);
		return o;
	}

	/**
	 * resave/rocket.ork: OpenRocketSaver's XML for the document as loaded (as built, for TestRockets),
	 * without simulation data (StorageOptions.setSaveSimulationData(false)).
	 */
	private static void resave(Path dir, OpenRocketDocument doc) throws IOException {
		Path file = dir.resolve("resave/rocket.ork");
		Files.createDirectories(file.getParent());
		StorageOptions options = new StorageOptions();
		options.setSaveSimulationData(false);
		WarningSet warnings = new WarningSet();
		ErrorSet errors = new ErrorSet();
		try (OutputStream out = Files.newOutputStream(file)) {
			new OpenRocketSaver().save(out, doc, options, warnings, errors);
		}
		if (!errors.isEmpty()) {
			throw new IOException("OpenRocketSaver reported errors: " + errors);
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
