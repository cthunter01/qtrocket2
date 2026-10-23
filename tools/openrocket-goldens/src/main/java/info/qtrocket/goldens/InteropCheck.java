package info.qtrocket.goldens;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HexFormat;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.stream.Stream;
import java.util.zip.GZIPInputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.Module;
import com.google.inject.util.Modules;

import info.openrocket.core.ServicesForTesting;
import info.openrocket.core.appearance.DecalImage;
import info.openrocket.core.database.ComponentPresetDao;
import info.openrocket.core.database.ComponentPresetDatabase;
import info.openrocket.core.database.Databases;
import info.openrocket.core.database.motor.MotorDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSetDatabase;
import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.Simulation;
import info.openrocket.core.document.StorageOptions;
import info.openrocket.core.file.GeneralRocketLoader;
import info.openrocket.core.file.GeneralRocketSaver;
import info.openrocket.core.file.motor.GeneralMotorLoader;
import info.openrocket.core.logging.Warning;
import info.openrocket.core.logging.WarningSet;
import info.openrocket.core.material.Material;
import info.openrocket.core.motor.MotorDigest;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.rocketcomponent.FinSet;
import info.openrocket.core.rocketcomponent.NoseCone;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.simulation.FlightData;
import info.openrocket.core.simulation.FlightDataBranch;
import info.openrocket.core.simulation.FlightDataType;
import info.openrocket.core.simulation.FlightEvent;
import info.openrocket.core.simulation.SimulationOptions;
import info.openrocket.core.startup.Application;
import info.openrocket.core.util.BuildProperties;
import info.openrocket.core.util.TestRockets;

/**
 * Section 7's interop check (README.md, "Interop check"): OpenRocket reads the design files QtRocket
 * saved (the DISABLED_ test of tests/core/goldens/ork_interop_saves_tests.cpp writes them, and the
 * GUI's save the DISABLED_ FromScratchInteropSave of tests/gui/exit/from_scratch_exit_tests.cpp, named
 * {@code <input>.<form>.ork}, the form being {@code zip-data}, {@code zip-nodata}, {@code plain-data}
 * or {@code plain-nodata}) and gets from each the design it gets from its own save of the same input.
 * <p>
 * The inputs are those of the goldens' manifest: the sixteen examples (an archive saved with the
 * document's default storage options, and plain XML without simulation data), the thirteen test
 * rockets (loaded from their re-saves; an archive), the eighteen legacy files of tests/data/ork (an
 * archive with simulation data, or plain XML with it where OpenRocket's archive fails: v1.6-apocd) and
 * the two saver-coverage documents (loaded from their re-saves; an archive); and the GUI's from-scratch
 * design ({@value #GUI_INPUT}: the design QtRocket's GUI made from the scenario of
 * tests/data/goldens/gui/from-scratch and saved with Save as, an archive with the document's default
 * storage options; the input is OpenRocket's own save of the same scenario, rocket.ork there). Each
 * input is loaded in the environment of its goldens: the harness's (the examples and the coverage
 * documents), the legacy files' ({@link GoldenDumper#legacyInjector}), the seven motors of TestRockets
 * with no component preset (the test rockets, whose motors are synthetic) or that of OpenRocket's GUI
 * after its start-up ({@link #guiInjector}: the GUI's design, as tools/openrocket-gui-reference loads
 * it).
 * <p>
 * Per file, with Q OpenRocket's load of QtRocket's save, O its load of the input and OO its load of its
 * own save of O in the same form (the first save of a fresh load, as QtRocket's is):
 * <ul>
 * <li>the load warnings of Q are on the allow-list ({@link #ALLOWED_WARNINGS}: what QtRocket keeps by
 * a decision and OpenRocket warns about), and O gives each of them too; nothing else;</li>
 * <li>Q is no empty document: its components and simulations are those of O and OO (OpenRocket makes
 * an empty rocket, without a warning, of an archive whose first entry is not the document);</li>
 * <li>for a save with the document's default storage options, O's defaults save the simulation data
 * where QtRocket's did;</li>
 * <li>OpenRocket's re-save of Q (OpenRocketSaver, with the simulation data) is its re-save of OO, byte
 * for byte, but for the lines of {@link #ALLOWED_RESAVE_CHANGES} (decided deviations), the ids that the
 * input does not name numbered on both sides (every id for the GUI's design, whose component ids
 * QtRocket's GUI drew). Not the committed re-save: OpenRocket's own round trip changes a design (its
 * saver writes the warning type RecoveryHighSpeedDeployment, which its loader reads as Other);</li>
 * <li>the geometry, mass and aerodynamics documents of Q, written by the harness's dumpers after the
 * re-save and the settling of the automatic dimensions as GoldenDumper writes them, are the goldens,
 * byte for byte, for an example; those of OO for a test rocket (the goldens of a test rocket describe
 * the maker's rocket, which OpenRocket's reload of its re-save changes in fourteen values: README.md,
 * "resave/rocket.ork"), and for a coverage document or a legacy file whose components have the same
 * ids in Q and OO (a loader draws the component ids of the formats 1.0 to 1.8, which store none, and
 * OpenRocket's sums follow them in the last bit; the GUI's design has other ids than OpenRocket's save
 * of the scenario too; the report lists a comparison it leaves out under "not compared");</li>
 * <li>the options of every simulation of Q are those of O (or of OO, where OpenRocket's own save
 * changes them); for an example or a test rocket also, made reproducible as the harness makes them,
 * the options and extensions of the golden simulation documents (and the harness's record of what it
 * changed, for an example);</li>
 * <li>the stored flight data of every simulation of Q (summary values, warnings, every branch with
 * every value of every type bit for bit, its events) is that of O, or that of OO where OpenRocket's own
 * save changes it (a save without the data, the six decimal places a save writes, ...);</li>
 * <li>the decal images of Q are those of O (or of OO) by name and bytes; for an archive, its entries
 * are those of OpenRocket's own archive of O in name and order, each with the same bytes (rocket.ork
 * aside, which the re-saves compare), and every thrust curve in it, {@code thrustcurves/<digest>.rse},
 * loads with a curve of its digest (MotorDigest.isDigestCompatible, as OpenRocket's MotorHandler takes
 * an embedded curve), or is allowed: a synthetic digest of TestRockets ("digest A8 test") in a test rocket,
 * or the one curve whose digest OpenRocket's RockSim writer does not keep ({@link #ALLOWED_CURVES}: an Estes
 * A10 of the bundled database); either has the bytes of OpenRocket's own archive.</li>
 * </ul>
 * Prints a report on standard output, every file with what was checked, allowed, not compared and found, and
 * exits with 0 when every file passes and every allowance was used as often as {@link #EXPECTED_ALLOWANCES}
 * says, 1 otherwise and 2 on a usage error.
 * <p>
 * Usage: {@code InteropCheck --openrocket <checkout> --goldens <tests/data/goldens> --examples <data/examples>
 * --legacy <tests/data/ork> --saves <directory> --work <directory>}
 */
public final class InteropCheck {

	private InteropCheck() {
	}

	/** The kinds of the inputs, by the prefixes of their names. */
	private enum Kind {
		EXAMPLE, TEST_ROCKET, LEGACY, COVERAGE, GUI
	}

	/**
	 * The input of the GUI's from-scratch design, which no manifest lists: the name of QtRocket's save
	 * (FromScratchInteropSave of tests/gui/exit/from_scratch_exit_tests.cpp).
	 */
	private static final String GUI_INPUT = "gui-from-scratch";

	/** OpenRocket's save of the GUI's from-scratch scenario (tools/openrocket-gui-reference), in the goldens. */
	private static final String GUI_REFERENCE = "gui/from-scratch/rocket.ork";

	/**
	 * An input of the manifest, or the GUI's from-scratch design: what OpenRocket loads as the input
	 * ({@code source}), the golden simulation documents of an example or a test rocket ({@code simulations},
	 * relative to the goldens), and the failure of OpenRocket's archive of a legacy file ({@code archiveError},
	 * or null).
	 */
	private record Input(String name, Kind kind, Path source, List<String> simulations, String archiveError) {
	}

	/** How QtRocket saved a file: as an .ork archive or as plain XML, with the simulation data or without. */
	private record Form(boolean archive, boolean withData) {
		String label() {
			return (archive ? "zip" : "plain") + (withData ? "-data" : "-nodata");
		}

		static Form parse(String label) {
			for (boolean archive : new boolean[] { true, false }) {
				for (boolean withData : new boolean[] { true, false }) {
					Form form = new Form(archive, withData);
					if (form.label().equals(label)) {
						return form;
					}
				}
			}
			return null;
		}
	}

	/** A warning OpenRocket gives for what QtRocket keeps by a decision. */
	private record AllowedWarning(String prefix, String suffix, String name, String decision) {
		boolean matches(String text) {
			return text.startsWith(prefix) && text.endsWith(suffix);
		}
	}

	/**
	 * The load warnings a QtRocket save may give in OpenRocket, each only where OpenRocket's load of the
	 * input gives the same warning.
	 */
	private static final List<AllowedWarning> ALLOWED_WARNINGS = List.of(
			new AllowedWarning("Simulation extension with id '", "' not found.", "D11 and L10",
					"decisions D11 and L10: QtRocket keeps a simulation extension whose id no provider knows, with "
							+ "its configuration, and writes it back; OpenRocket drops it with this warning"),
			new AllowedWarning("No matching ComponentPreset for component ", "", "U8",
					"decision U8: QtRocket keeps a <preset> reference that its presets cannot resolve and writes "
							+ "it back; OpenRocket, without that preset too, forgets it with this warning"));

	/** A line of OpenRocket's re-save of QtRocket's save that differs from its re-save of its own save. */
	private record AllowedChange(String input, String ownLine, String qtRocketLine, String name, String decision) {
	}

	/** The differences of the re-saves that a decided deviation makes (the lines without indentation). */
	private static final List<AllowedChange> ALLOWED_RESAVE_CHANGES = List.of(
			new AllowedChange("legacy-v1-7-simulation-extensions-and-scripting",
					"<entry key=\"enabled\" type=\"boolean\">true</entry>",
					"<entry key=\"enabled\" type=\"boolean\">false</entry>", "D1 and E4",
					"decisions D1 and E4: QtRocket runs no script and disables every enabled script with code on "
							+ "load, the roll control script OpenRocket trusts by its hash included, so its save "
							+ "writes it disabled"));

	/** A thrust curve of an archive whose digest OpenRocket's RockSim writer does not keep. */
	private record AllowedCurve(String input, String entry) {
	}

	/**
	 * The thrust curves of the archives of other inputs than the test rockets that load with a curve of another
	 * digest, in OpenRocket's own archive too: the RockSim text of the bundled database's Estes A10 of this digest
	 * reads back as a curve of the digest 6c22c2c6a938ec433d9e0208c31f4476 (OpenRocket's loader takes the database
	 * motor of the digest first and never reads the entry).
	 */
	private static final List<AllowedCurve> ALLOWED_CURVES = List.of(new AllowedCurve(
			"example-pods-powered-with-recovery-deployment", "thrustcurves/e5b53def203dd437ebf0d67846f6cd3b.rse"));

	/** The short name of the allowance of a synthetic digest of TestRockets. */
	private static final String SYNTHETIC_DIGESTS = "TestRockets' synthetic digests";
	/** The short name of the allowance of {@link #ALLOWED_CURVES}. */
	private static final String ROCKSIM_WRITER = "OpenRocket's RockSim writer";

	/**
	 * How often the files of all inputs use each allowance (by the short names of the allow-lists): a count
	 * that differs fails the check, so that an allowance cannot hide a new case of its kind.
	 */
	private static final Map<String, Integer> EXPECTED_ALLOWANCES = Map.of("U8", 13, "D1 and E4", 2,
			"D11 and L10", 1, SYNTHETIC_DIGESTS, 26, ROCKSIM_WRITER, 1);

	/** A synthetic digest of TestRockets, as the name of an entry gives it: "digest A8 test". */
	private static final Pattern SYNTHETIC_DIGEST = Pattern.compile("digest [A-Z][0-9]+ test");

	/** The generators of the motors of TestRockets, in the order the test rockets' motor database has them. */
	private static final List<String> TEST_ROCKET_MOTORS = List.of("generateMotor_A8_18mm",
			"generateMotor_A10_13mm", "generateMotor_B4_18mm", "generateMotor_C6_18mm", "generateMotor_D21_18mm",
			"generateMotor_M1350_75mm", "generateMotor_G77_29mm");

	/** The subdirectory of the process directory that holds OpenRocket's own save of the input at hand. */
	private static final String OWN_DIRECTORY = "own";

	/** How many differing lines a failure shows. */
	private static final int MAX_SHOWN = 12;

	private static final class Arguments {
		Path openrocket;
		Path goldens;
		Path examples;
		Path legacy;
		Path saves;
		Path work;
	}

	/** What the check of one file found. */
	private static final class Report {
		final String file;
		final List<String> checked = new ArrayList<>();
		/** What was allowed, each with the short name of its reason (a decision). */
		final Map<String, String> allowed = new LinkedHashMap<>();
		/** What was not compared, and why. */
		final List<String> notCompared = new ArrayList<>();
		final List<String> failures = new ArrayList<>();

		Report(String file) {
			this.file = file;
		}
	}

	/** The injectors of the four environments. */
	private record Injectors(Injector harness, Injector legacy, Injector testRockets, Injector gui) {
		Injector of(Kind kind) {
			return switch (kind) {
				case EXAMPLE, COVERAGE -> harness;
				case LEGACY -> legacy;
				case TEST_ROCKET -> testRockets;
				case GUI -> gui;
			};
		}
	}

	public static void main(String[] args) throws Exception {
		// Before anything creates a UUID (see DeterministicUuids).
		DeterministicUuids.install();
		Locale.setDefault(Locale.US);
		Arguments a;
		try {
			a = parse(args);
		} catch (IllegalArgumentException e) {
			System.err.println(e.getMessage());
			System.exit(2);
			return;
		}
		Path processDirectory = GoldenDumper.useProcessDirectory(a.work);
		// The directories of OpenRocket's own saves and of the repacked legacy archive, deleted at the exit
		// before the process directory (in the reverse order of the registrations).
		processDirectory.resolve(OWN_DIRECTORY).toFile().deleteOnExit();
		processDirectory.resolve("repacked").toFile().deleteOnExit();
		GoldenDumper.Environment environment = GoldenDumper.bootstrap(a.openrocket.resolve("core"),
				processDirectory);
		Injectors injectors = new Injectors(Application.getInjector(), GoldenDumper.legacyInjector(environment),
				testRocketInjector(), guiInjector());
		Application.setInjector(injectors.harness());

		JsonObject manifest = JsonParser.parseString(Files.readString(a.goldens.resolve("manifest.json")))
				.getAsJsonObject();
		List<Input> inputs = inputs(a, manifest);
		Map<String, Path> saves = savedFiles(a.saves);
		System.out.println("InteropCheck: OpenRocket " + BuildProperties.getVersion() + " reads QtRocket's saves in "
				+ a.saves);

		List<Report> reports = new ArrayList<>();
		Set<String> expected = new TreeSet<>();
		for (Input input : inputs) {
			for (String file : expectedFiles(input, saves, reports)) {
				expected.add(file);
				reports.add(check(a, injectors, input, saves.get(file), processDirectory));
			}
		}
		for (String file : saves.keySet()) {
			if (!expected.contains(file)) {
				Report report = new Report(file);
				report.failures.add("not checked: the file name (<input>.<form>.ork) names no input of the manifest "
						+ "(or " + GUI_INPUT + ") with that form, or a second save of one kind");
				reports.add(report);
			}
		}
		System.exit(printReports(reports, inputs.size()));
	}

	private static Arguments parse(String[] args) {
		Arguments a = new Arguments();
		for (int i = 0; i < args.length; i++) {
			String option = args[i];
			if (i + 1 >= args.length) {
				throw new IllegalArgumentException("Missing value for " + option);
			}
			Path value = Path.of(args[++i]).toAbsolutePath().normalize();
			switch (option) {
				case "--openrocket" -> a.openrocket = value;
				case "--goldens" -> a.goldens = value;
				case "--examples" -> a.examples = value;
				case "--legacy" -> a.legacy = value;
				case "--saves" -> a.saves = value;
				case "--work" -> a.work = value;
				default -> throw new IllegalArgumentException("Unknown option " + option);
			}
		}
		if (a.openrocket == null || a.goldens == null || a.examples == null || a.legacy == null || a.saves == null
				|| a.work == null) {
			throw new IllegalArgumentException("Usage: InteropCheck --openrocket <checkout> --goldens <dir> "
					+ "--examples <dir> --legacy <dir> --saves <dir> --work <dir>");
		}
		return a;
	}

	/**
	 * The injector of the test rockets: OpenRocket's test bootstrap with the seven motors of TestRockets
	 * (made by its private generators, as the C++ tests make them) and no component preset, as tier 9d's
	 * ResaveProbe loads the re-saves. The test rockets' motors are synthetic: with the bundled database a
	 * load would give their mounts real motors of the same designations.
	 */
	private static Injector testRocketInjector() throws ReflectiveOperationException {
		ThrustCurveMotorSetDatabase motors = new ThrustCurveMotorSetDatabase();
		for (String generator : TEST_ROCKET_MOTORS) {
			Method method = TestRockets.class.getDeclaredMethod(generator);
			method.setAccessible(true);
			motors.addMotor((ThrustCurveMotor) method.invoke(null));
		}
		ComponentPresetDatabase presets = new ComponentPresetDatabase();
		Module dbOverrides = new AbstractModule() {
			@Override
			protected void configure() {
				bind(ComponentPresetDao.class).toInstance(presets);
				bind(ThrustCurveMotorSetDatabase.class).toInstance(motors);
				bind(MotorDatabase.class).to(ThrustCurveMotorSetDatabase.class);
			}
		};
		return Guice.createInjector(Modules.override(new ServicesForTesting()).with(dbOverrides),
				new PluginModule());
	}

	/**
	 * The injector of the GUI's from-scratch design: OpenRocket's GUI after its start-up, in which
	 * tools/openrocket-gui-reference loaded the design its scenario saved (its README, "How OpenRocket is
	 * run") and the C++ tests load a save of that scenario (tests/core/goldens/GuiReferenceEnvironment.h):
	 * no component preset and no motor (the start-up bypasses both databases), and the default component
	 * materials the start-up stores (SwingPreferences.loadDefaultComponentMaterials, :911-914: Balsa for fin
	 * sets, Polystyrene for nose cones), with which a new component of the loader starts. The other
	 * preferences are those of OpenRocket's tests, as for the other inputs and in GuiReferenceEnvironment.h.
	 */
	private static Injector guiInjector() {
		GuiStartupPreferences preferences = new GuiStartupPreferences();
		ComponentPresetDatabase presets = new ComponentPresetDatabase();
		ThrustCurveMotorSetDatabase motors = new ThrustCurveMotorSetDatabase();
		Module overrides = new AbstractModule() {
			@Override
			protected void configure() {
				bind(ApplicationPreferences.class).toInstance(preferences);
				bind(ComponentPresetDao.class).toInstance(presets);
				bind(ThrustCurveMotorSetDatabase.class).toInstance(motors);
				bind(MotorDatabase.class).to(ThrustCurveMotorSetDatabase.class);
			}
		};
		Injector injector = Guice.createInjector(Modules.override(new ServicesForTesting()).with(overrides),
				new PluginModule());
		preferences.setDefaultComponentMaterial(FinSet.class, Databases.findMaterial(Material.Type.BULK, "Balsa"));
		preferences.setDefaultComponentMaterial(NoseCone.class,
				Databases.findMaterial(Material.Type.BULK, "Polystyrene"));
		return injector;
	}

	/**
	 * OpenRocket's test preferences (ServicesForTesting.PreferencesForTesting, which keep nothing) that keep
	 * the default component materials (the directory "componentMaterials" of
	 * ApplicationPreferences.setDefaultComponentMaterial and getDefaultComponentMaterial).
	 */
	private static final class GuiStartupPreferences extends ServicesForTesting.PreferencesForTesting {
		private static final String COMPONENT_MATERIALS = "componentMaterials";

		private final Map<String, String> componentMaterials = new TreeMap<>();

		@Override
		public String getString(String directory, String key, String defaultValue) {
			if (COMPONENT_MATERIALS.equals(directory)) {
				return componentMaterials.getOrDefault(key, defaultValue);
			}
			return super.getString(directory, key, defaultValue);
		}

		@Override
		public void putString(String directory, String key, String value) {
			if (!COMPONENT_MATERIALS.equals(directory)) {
				super.putString(directory, key, value);
			} else if (value == null) {
				componentMaterials.remove(key);
			} else {
				componentMaterials.put(key, value);
			}
		}
	}

	// ---------------------------------------------------------------------------------------------
	// The inputs and the files

	/**
	 * The inputs of the manifest: "inputs" (examples and test rockets), then "resaves.inputs"; and last the
	 * GUI's from-scratch design, which the manifest does not list.
	 */
	private static List<Input> inputs(Arguments a, JsonObject manifest) {
		List<Input> inputs = new ArrayList<>();
		for (JsonElement element : manifest.getAsJsonArray("inputs")) {
			JsonObject o = element.getAsJsonObject();
			String name = o.get("name").getAsString();
			boolean example = "example".equals(o.get("kind").getAsString());
			List<String> simulations = new ArrayList<>();
			for (JsonElement simulation : o.getAsJsonArray("simulations")) {
				simulations.add(simulation.getAsJsonObject().get("json").getAsString());
			}
			Path source = example ? a.examples.resolve(fileName(o.get("source").getAsString()))
					: a.goldens.resolve(o.get("resave").getAsString());
			inputs.add(new Input(name, example ? Kind.EXAMPLE : Kind.TEST_ROCKET, source, simulations, null));
		}
		for (JsonElement element : manifest.getAsJsonObject("resaves").getAsJsonArray("inputs")) {
			JsonObject o = element.getAsJsonObject();
			String name = o.get("name").getAsString();
			boolean legacy = "legacy".equals(o.get("kind").getAsString());
			Path source = legacy ? a.legacy.resolve(fileName(o.get("source").getAsString()))
					: a.goldens.resolve(o.get("resave").getAsString());
			String archiveError = legacy && !o.get("archiveError").isJsonNull() ? o.get("archiveError").getAsString()
					: null;
			inputs.add(new Input(name, legacy ? Kind.LEGACY : Kind.COVERAGE, source, List.of(), archiveError));
		}
		inputs.add(new Input(GUI_INPUT, Kind.GUI, a.goldens.resolve(GUI_REFERENCE), List.of(), null));
		return inputs;
	}

	/** The last part of a path of the manifest ("data/examples/A simple model rocket.ork"). */
	private static String fileName(String path) {
		return path.substring(path.lastIndexOf('/') + 1);
	}

	/** The files of {@code directory} named *.ork, by name. */
	private static Map<String, Path> savedFiles(Path directory) throws IOException {
		Map<String, Path> files = new TreeMap<>();
		try (Stream<Path> list = Files.list(directory)) {
			for (Path file : list.filter(Files::isRegularFile).toList()) {
				String name = file.getFileName().toString();
				if (name.endsWith(".ork")) {
					files.put(name, file);
				}
			}
		}
		return files;
	}

	/**
	 * The saved files of {@code input}: its archive (with or without data, as the document's default storage
	 * options save it; a legacy file's with data) and, for an example, its plain XML without data; for a
	 * legacy file whose archive OpenRocket cannot save, its plain XML with data instead. A file that is
	 * missing is a failed report of its own.
	 */
	private static List<String> expectedFiles(Input input, Map<String, Path> saves, List<Report> reports) {
		List<List<String>> choices = new ArrayList<>();
		if (input.kind() == Kind.LEGACY) {
			choices.add(List.of(input.archiveError() == null ? "zip-data" : "plain-data"));
		} else {
			choices.add(List.of("zip-data", "zip-nodata"));
		}
		if (input.kind() == Kind.EXAMPLE) {
			choices.add(List.of("plain-nodata"));
		}
		List<String> files = new ArrayList<>();
		for (List<String> labels : choices) {
			List<String> found = labels.stream().map(label -> input.name() + "." + label + ".ork")
					.filter(saves::containsKey).toList();
			if (found.size() == 1) {
				files.add(found.get(0));
			} else {
				Report report = new Report(input.name() + "." + String.join("|", labels) + ".ork");
				report.failures.add(found.isEmpty() ? "QtRocket's save is missing" : "two saves of one kind: " + found);
				reports.add(report);
			}
		}
		return files;
	}

	// ---------------------------------------------------------------------------------------------
	// The check of one file

	/** What is compared of a loaded document. */
	private record Described(int components, List<String> componentIds, int simulations, List<String> options,
			List<List<String>> flightData, List<String> decals) {
	}

	/** One load: the document and the texts of its warnings. */
	private record Loaded(OpenRocketDocument document, WarningSet warnings, List<String> warningTexts) {
	}

	private static Report check(Arguments a, Injectors injectors, Input input, Path saved, Path work) {
		Report report = new Report(saved.getFileName().toString());
		// On the standard error, where OpenRocket logs, so that what it logs can be told by file.
		System.err.println("[interop] " + report.file);
		String label = report.file.substring(input.name().length() + 1, report.file.length() - ".ork".length());
		Form form = Form.parse(label);
		Injector previous = Application.getInjector();
		Application.setInjector(injectors.of(input.kind()));
		Path own = work.resolve(OWN_DIRECTORY).resolve(report.file);
		Path originalFile = input.source();
		try {
			// OpenRocket reads the first entry of an archive only: a legacy archive whose first entry is not
			// the document is given to it packed again, as the harness gives it (decals-first). The copy
			// stays until the end: the decal images of an archive are read from its file when asked for.
			if (input.kind() == Kind.LEGACY) {
				originalFile = GoldenDumper.documentFirst(input.source(), work);
			}
			DeterministicUuids.reseed(input.name() + "/original");
			Loaded original = load(originalFile);
			Described o = describe(original.document());
			checkDefaultOptions(input, form, original.document(), report);
			saveOwn(original.document(), form, own);
			DeterministicUuids.reseed(input.name() + "/own");
			Loaded ownLoad = load(own);
			Described oo = describe(ownLoad.document());
			DeterministicUuids.reseed(input.name() + "/qtrocket");
			Loaded qtRocket = load(saved);
			Described q = describe(qtRocket.document());

			checkWarnings(qtRocket.warningTexts(), original.warningTexts(), report);
			checkNotEmpty(q, o, oo, report);
			checkOptions(q, o, oo, report);
			checkFlightData(q, o, oo, report);
			checkDecals(q, o, oo, report);
			if (form.archive()) {
				checkArchive(input, saved, own, report);
			}
			// The ids of the input that a save keeps; none for the GUI's design, whose ids QtRocket's GUI drew
			// where the reference's DeterministicUuids drew those of OpenRocket's save.
			Set<String> inputIds = input.kind() == Kind.GUI ? Set.of() : idsOf(documentText(originalFile));
			checkResaves(input, inputIds, qtRocket.document(), ownLoad.document(), report);
			checkGoldenSimulations(a, input, qtRocket.document(), report);
			checkDumps(a, input, qtRocket, ownLoad, q, oo, report);
		} catch (Exception e) {
			report.failures.add("the check stopped: " + e);
		} finally {
			Application.setInjector(previous);
			try {
				Files.deleteIfExists(own);
				if (!originalFile.equals(input.source())) {
					Files.deleteIfExists(originalFile);
				}
			} catch (IOException e) {
				report.failures.add("cannot delete a file of the work directory: " + e);
			}
		}
		return report;
	}

	/**
	 * The text of the design document of the file {@code file}: the first entry of an archive whose name
	 * ends in .ork (in any case), the contents of a gzip file, or the file.
	 */
	private static String documentText(Path file) throws IOException {
		byte[] bytes = Files.readAllBytes(file);
		if (bytes.length >= 2 && bytes[0] == 'P' && bytes[1] == 'K') {
			try (ZipInputStream in = new ZipInputStream(new ByteArrayInputStream(bytes))) {
				ZipEntry entry;
				while ((entry = in.getNextEntry()) != null) {
					if (entry.getName().toLowerCase(Locale.ROOT).endsWith(".ork")) {
						return new String(in.readAllBytes(), StandardCharsets.UTF_8);
					}
				}
			}
			return "";
		}
		if (bytes.length >= 2 && bytes[0] == (byte) 0x1f && bytes[1] == (byte) 0x8b) {
			try (InputStream in = new GZIPInputStream(new ByteArrayInputStream(bytes))) {
				return new String(in.readAllBytes(), StandardCharsets.UTF_8);
			}
		}
		return new String(bytes, StandardCharsets.UTF_8);
	}

	/** A UUID as UUID.toString() writes it, in either case. */
	private static final Pattern UUID_PATTERN = Pattern
			.compile("[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}");

	/** The UUIDs {@code text} spells out, in lower case. */
	private static Set<String> idsOf(String text) {
		Set<String> ids = new TreeSet<>();
		Matcher matcher = UUID_PATTERN.matcher(text);
		while (matcher.find()) {
			ids.add(matcher.group().toLowerCase(Locale.ROOT));
		}
		return ids;
	}

	/**
	 * {@code text} with every UUID that {@code known} does not hold replaced by "drawn-id-<n>", numbered from 1
	 * in the order of the first occurrences (as withDrawnIdsNumbered() of the C++ tests' GoldenResaves.h): the
	 * ids a loader drew for what the input names no id for (the components of the formats 1.0 to 1.8, the
	 * events of a file that stores none, ...), which OpenRocket's and QtRocket's loads draw otherwise. An id
	 * of the input stays as it is.
	 */
	private static String withDrawnIdsNumbered(String text, Set<String> known) {
		Map<String, String> numbers = new LinkedHashMap<>();
		Matcher matcher = UUID_PATTERN.matcher(text);
		StringBuilder sb = new StringBuilder(text.length());
		while (matcher.find()) {
			String id = matcher.group().toLowerCase(Locale.ROOT);
			String replacement = known.contains(id) ? matcher.group()
					: numbers.computeIfAbsent(id, key -> "drawn-id-" + (numbers.size() + 1));
			matcher.appendReplacement(sb, Matcher.quoteReplacement(replacement));
		}
		matcher.appendTail(sb);
		return sb.toString();
	}

	private static Loaded load(Path file) throws Exception {
		GeneralRocketLoader loader = new GeneralRocketLoader(file.toFile());
		OpenRocketDocument document = loader.load();
		WarningSet warnings = loader.getWarnings();
		List<String> texts = new ArrayList<>();
		for (Warning warning : warnings) {
			texts.add(warning.toString());
		}
		texts.sort(null);
		return new Loaded(document, warnings, texts);
	}

	/** OpenRocket's own save of {@code document} in {@code form}, the first save of a fresh load. */
	private static void saveOwn(OpenRocketDocument document, Form form, Path file) throws Exception {
		Files.createDirectories(file.getParent());
		StorageOptions options = new StorageOptions();
		options.setSaveSimulationData(form.withData());
		if (form.archive()) {
			new GeneralRocketSaver().save(file.toFile(), document, options);
		} else {
			Files.write(file, GoldenDumper.saveXml(document, form.withData()));
		}
	}

	private static Described describe(OpenRocketDocument document) throws IOException {
		ComponentIndex index = new ComponentIndex(document.getRocket());
		List<String> ids = new ArrayList<>();
		for (RocketComponent component : index.components()) {
			ids.add(component.getID().toString());
		}
		List<String> options = new ArrayList<>();
		List<List<String>> flightData = new ArrayList<>();
		for (Simulation simulation : document.getSimulations()) {
			options.add(optionsText(simulation.getOptions()));
			flightData.add(flightData(simulation.getSimulatedData(), index));
		}
		return new Described(index.components().size(), ids, document.getSimulations().size(), options, flightData,
				decals(document));
	}

	/**
	 * The options of a simulation as the golden simulation documents write them, but for a random seed
	 * that is not fixed: such a seed is drawn when the options are made, and no file stores it.
	 */
	private static String optionsText(SimulationOptions options) {
		Map<String, Object> o = SimulationDumper.options(options);
		if (!options.isRandomSeedFixed()) {
			o.remove("randomSeed");
		}
		return new String(Json.bytes(o), StandardCharsets.UTF_8);
	}

	/**
	 * The stored flight data of a simulation, line by line: the summary values and every branch with the
	 * bits of all values of each type (as a SHA-256 digest) and its events; the warnings sorted.
	 */
	private static List<String> flightData(FlightData data, ComponentIndex index) {
		List<String> lines = new ArrayList<>();
		if (data == null) {
			lines.add("no flight data");
			return lines;
		}
		lines.add("summary " + bits(data.getMaxAltitude(), data.getMaxVelocity(), data.getMaxAcceleration(),
				data.getMaxMachNumber(), data.getTimeToApogee(), data.getFlightTime(), data.getGroundHitVelocity(),
				data.getLaunchRodVelocity(), data.getDeploymentVelocity(), data.getOptimumDelay()));
		List<String> warnings = new ArrayList<>();
		for (Warning warning : data.getWarningSet()) {
			warnings.add("warning " + warning.getClass().getSimpleName() + ": " + warning);
		}
		warnings.sort(null);
		lines.addAll(warnings);
		for (int b = 0; b < data.getBranchCount(); b++) {
			FlightDataBranch branch = data.getBranch(b);
			lines.add("branch " + b + " '" + branch.getName() + "' rows " + branch.getLength());
			for (FlightDataType type : branch.getTypes()) {
				lines.add("  type " + type.getName() + " [" + type.getSymbol() + "] " + digest(branch.get(type)));
			}
			for (FlightEvent event : branch.getEvents()) {
				lines.add("  event " + event.getType().name() + " at " + bits(event.getTime()) + " source "
						+ index.path(event.getSource()) + " data " + eventData(event.getData()));
			}
		}
		return lines;
	}

	private static String eventData(Object data) {
		return data == null ? "none" : data.getClass().getSimpleName() + ": " + data;
	}

	/** The values with the hexadecimal bits of each. */
	private static String bits(double... values) {
		StringBuilder sb = new StringBuilder();
		for (double value : values) {
			sb.append(sb.length() == 0 ? "" : " ").append(value).append('/')
					.append(Long.toHexString(Double.doubleToRawLongBits(value)));
		}
		return sb.toString();
	}

	/** The number of values and the SHA-256 of their bits. */
	private static String digest(List<Double> values) {
		MessageDigest sha = sha256();
		byte[] bytes = new byte[Long.BYTES];
		for (Double value : values) {
			long bits = value == null ? 0x7ff8dead0000beefL : Double.doubleToRawLongBits(value);
			for (int i = 0; i < Long.BYTES; i++) {
				bytes[i] = (byte) (bits >>> (8 * i));
			}
			sha.update(bytes);
		}
		return values.size() + " values " + HexFormat.of().formatHex(sha.digest());
	}

	private static MessageDigest sha256() {
		try {
			return MessageDigest.getInstance("SHA-256");
		} catch (NoSuchAlgorithmException e) {
			throw new IllegalStateException(e);
		}
	}

	/** The decal images, sorted: each with the SHA-256 of its bytes, or why they cannot be read. */
	private static List<String> decals(OpenRocketDocument document) {
		List<String> decals = new ArrayList<>();
		for (DecalImage image : document.getDecalList()) {
			String what;
			try (InputStream in = image.getBytes()) {
				byte[] bytes = in.readAllBytes();
				what = bytes.length + " bytes " + HexFormat.of().formatHex(sha256().digest(bytes));
			} catch (Exception e) {
				what = "unreadable: " + e.getClass().getSimpleName();
			}
			decals.add(image.getName() + " = " + what);
		}
		decals.sort(null);
		return decals;
	}

	private static void checkDefaultOptions(Input input, Form form, OpenRocketDocument original, Report report) {
		// The archives of the examples, the test rockets and the coverage documents are saved with the
		// document's default storage options, and so is the GUI's design (Save as).
		if (!form.archive() || input.kind() == Kind.LEGACY) {
			return;
		}
		boolean openRocketsDefault = original.getDefaultStorageOptions().getSaveSimulationData();
		if (openRocketsDefault != form.withData()) {
			report.failures.add("QtRocket's default storage options " + (form.withData() ? "save" : "do not save")
					+ " the simulation data, OpenRocket's " + (openRocketsDefault ? "do" : "do not"));
		} else {
			report.checked.add("default storage options as OpenRocket's");
		}
	}

	private static void checkWarnings(List<String> warnings, List<String> originalWarnings, Report report) {
		int allowed = 0;
		for (String warning : warnings) {
			AllowedWarning rule = null;
			for (AllowedWarning candidate : ALLOWED_WARNINGS) {
				if (candidate.matches(warning)) {
					rule = candidate;
				}
			}
			if (rule != null && originalWarnings.contains(warning)) {
				report.allowed.put("load warning \"" + warning + "\" (" + rule.decision() + ")", rule.name());
				allowed++;
			} else {
				report.failures.add("load warning \"" + warning + "\""
						+ (rule == null ? "" : ", which OpenRocket's load of the input does not give"));
			}
		}
		if (allowed == warnings.size()) {
			report.checked.add("load warnings " + (warnings.isEmpty() ? "none" : allowed + " allowed"));
		}
	}

	private static void checkNotEmpty(Described q, Described o, Described oo, Report report) {
		if (q.components() < 2 || q.components() != o.components() || q.components() != oo.components()
				|| q.simulations() != o.simulations() || q.simulations() != oo.simulations()) {
			report.failures.add("components " + q.components() + " and simulations " + q.simulations()
					+ ", where OpenRocket's load of the input has " + o.components() + " and " + o.simulations()
					+ " and of its own save " + oo.components() + " and " + oo.simulations());
		} else {
			report.checked.add(q.components() + " components and " + q.simulations() + " simulations");
		}
	}

	/**
	 * Compares a description of Q with those of O and OO: "original" when it is O's, "own save" when it
	 * is OO's and OO's differs from O's, null otherwise.
	 */
	private static String verdict(Object q, Object o, Object oo) {
		if (q.equals(o)) {
			return "original";
		}
		return q.equals(oo) && !oo.equals(o) ? "own save" : null;
	}

	private static void checkOptions(Described q, Described o, Described oo, Report report) {
		String verdict = verdict(q.options(), o.options(), oo.options());
		if (verdict == null) {
			report.failures.add("the options of the simulations differ from OpenRocket's: "
					+ firstDifference(q.options(), oo.options()));
		} else {
			report.checked.add("the options of " + q.simulations() + " simulations as OpenRocket's "
					+ (verdict.equals("original") ? "load of the input" : "own save"));
		}
	}

	private static void checkFlightData(Described q, Described o, Described oo, Report report) {
		int failures = report.failures.size();
		int asOriginal = 0;
		int asOwnSave = 0;
		for (int i = 0; i < q.flightData().size() && i < o.flightData().size() && i < oo.flightData().size(); i++) {
			String verdict = verdict(q.flightData().get(i), o.flightData().get(i), oo.flightData().get(i));
			if (verdict == null) {
				report.failures.add("the flight data of simulation " + i + " differ from OpenRocket's: "
						+ firstDifference(q.flightData().get(i), oo.flightData().get(i)));
			} else if (verdict.equals("original")) {
				asOriginal++;
			} else {
				asOwnSave++;
				report.checked.add("the flight data of simulation " + i + " as OpenRocket's own save, which changes "
						+ "the input's: " + shortened(firstDifference(oo.flightData().get(i), o.flightData().get(i),
								"the save's", "the input's")));
			}
		}
		if (report.failures.size() == failures) {
			report.checked.add("the flight data of " + asOriginal + " simulations as the input's, of " + asOwnSave
					+ " as OpenRocket's own save");
		}
	}

	private static void checkDecals(Described q, Described o, Described oo, Report report) {
		String verdict = verdict(q.decals(), o.decals(), oo.decals());
		if (verdict == null) {
			report.failures.add(
					"the decal images differ from OpenRocket's: " + firstDifference(q.decals(), oo.decals()));
		} else if (!q.decals().isEmpty()) {
			report.checked.add(q.decals().size() + " decal images as OpenRocket's "
					+ (verdict.equals("original") ? "load of the input" : "own save"));
		}
	}

	/** The first line where {@code ours} (QtRocket's) and {@code theirs} (OpenRocket's) differ. */
	private static String firstDifference(List<String> ours, List<String> theirs) {
		return firstDifference(ours, theirs, "QtRocket's", "OpenRocket's");
	}

	/** The first line where {@code first} and {@code second} differ, or their numbers of lines. */
	private static String firstDifference(List<String> first, List<String> second, String firstName,
			String secondName) {
		for (int i = 0; i < Math.min(first.size(), second.size()); i++) {
			if (!first.get(i).equals(second.get(i))) {
				return "line " + (i + 1) + ": " + firstName + " \"" + first.get(i) + "\", " + secondName + " \""
						+ second.get(i) + "\"";
			}
		}
		return firstName + " " + first.size() + " lines, " + secondName + " " + second.size();
	}

	/** {@code text} cut after 400 characters. */
	private static String shortened(String text) {
		return text.length() <= 400 ? text : text.substring(0, 400) + " ...";
	}

	// ---------------------------------------------------------------------------------------------
	// The archive

	/** The entries of the zip archive {@code file}, by name in their order. */
	private static Map<String, byte[]> entries(Path file) throws IOException {
		Map<String, byte[]> entries = new LinkedHashMap<>();
		try (ZipInputStream in = new ZipInputStream(Files.newInputStream(file))) {
			ZipEntry entry;
			while ((entry = in.getNextEntry()) != null) {
				entries.put(entry.getName(), in.readAllBytes());
			}
		}
		return entries;
	}

	private static void checkArchive(Input input, Path saved, Path own, Report report) throws IOException {
		Map<String, byte[]> ours = entries(saved);
		Map<String, byte[]> theirs = entries(own);
		if (!new ArrayList<>(ours.keySet()).equals(new ArrayList<>(theirs.keySet()))) {
			report.failures.add("the entries " + ours.keySet() + ", OpenRocket's archive " + theirs.keySet());
			return;
		}
		int failures = report.failures.size();
		int decals = 0;
		int curves = 0;
		for (Map.Entry<String, byte[]> entry : ours.entrySet()) {
			String name = entry.getKey();
			if (name.equals("rocket.ork")) {
				continue;
			}
			if (!Arrays.equals(entry.getValue(), theirs.get(name))) {
				report.failures.add("the entry " + name + " differs from OpenRocket's");
				continue;
			}
			if (name.startsWith("thrustcurves/") && name.endsWith(".rse")) {
				curves++;
				checkThrustCurve(input, name, entry.getValue(), report);
			} else {
				decals++;
			}
		}
		if (report.failures.size() == failures) {
			report.checked.add("the " + ours.size() + " entries as OpenRocket's archive (" + decals
					+ " decal images, " + curves + " thrust curves)");
		}
	}

	/** A thrust curve of the archive: it loads, with a curve of the digest its name gives. */
	private static void checkThrustCurve(Input input, String name, byte[] bytes, Report report) throws IOException {
		String digest = name.substring("thrustcurves/".length(), name.length() - ".rse".length());
		List<ThrustCurveMotor.Builder> motors = new GeneralMotorLoader().load(new ByteArrayInputStream(bytes),
				digest + ".rse");
		boolean compatible = false;
		for (ThrustCurveMotor.Builder builder : motors) {
			compatible |= MotorDigest.isDigestCompatible(builder.build(), digest);
		}
		if (compatible) {
			return;
		}
		List<String> loaded = new ArrayList<>();
		for (ThrustCurveMotor.Builder builder : motors) {
			ThrustCurveMotor motor = builder.build();
			loaded.add(motor.getManufacturer().getSimpleName() + " " + motor.getDesignation() + " of the digest "
					+ motor.getDigest());
		}
		// The entry has the bytes of the same entry of OpenRocket's own archive (checkArchive()).
		if (motors.isEmpty()) {
			report.failures.add("the thrust curve " + name + " holds no motor");
		} else if (input.kind() == Kind.TEST_ROCKET && SYNTHETIC_DIGEST.matcher(digest).matches()) {
			report.allowed.put("the thrust curve " + name + " " + loaded + " (a synthetic digest of TestRockets, "
					+ "which no curve gives: OpenRocket's own archive has the same bytes)", SYNTHETIC_DIGESTS);
		} else if (ALLOWED_CURVES.contains(new AllowedCurve(input.name(), name))) {
			report.allowed.put("the thrust curve " + name + " " + loaded + " (OpenRocket's RockSim writer does not "
					+ "keep the digest of this curve: OpenRocket's own archive has the same bytes)", ROCKSIM_WRITER);
		} else {
			report.failures.add("the thrust curve " + name + " " + loaded + " loads with no curve of its digest");
		}
	}

	// ---------------------------------------------------------------------------------------------
	// The re-saves, the golden simulations and the dumps

	private static void checkResaves(Input input, Set<String> inputIds, OpenRocketDocument q, OpenRocketDocument oo,
			Report report) throws IOException {
		List<String> ours = lines(withDrawnIdsNumbered(text(GoldenDumper.saveXml(q, true)), inputIds));
		List<String> theirs = lines(withDrawnIdsNumbered(text(GoldenDumper.saveXml(oo, true)), inputIds));
		if (ours.size() != theirs.size()) {
			report.failures.add("OpenRocket's re-save of QtRocket's save has " + ours.size()
					+ " lines, its re-save of its own save " + theirs.size() + ": " + firstDifference(ours, theirs));
			return;
		}
		int shown = 0;
		for (int i = 0; i < ours.size(); i++) {
			String mine = ours.get(i).strip();
			String own = theirs.get(i).strip();
			if (mine.equals(own)) {
				continue;
			}
			AllowedChange rule = null;
			for (AllowedChange candidate : ALLOWED_RESAVE_CHANGES) {
				if (candidate.input().equals(input.name()) && candidate.ownLine().equals(own)
						&& candidate.qtRocketLine().equals(mine)) {
					rule = candidate;
				}
			}
			if (rule != null) {
				report.allowed.put("re-save line " + (i + 1) + " \"" + mine + "\" where OpenRocket's own has \"" + own
						+ "\" (" + rule.decision() + ")", rule.name());
			} else if (shown++ < MAX_SHOWN) {
				report.failures.add("re-save line " + (i + 1) + ": \"" + mine + "\", OpenRocket's own \"" + own + "\"");
			}
		}
		if (shown > MAX_SHOWN) {
			report.failures.add((shown - MAX_SHOWN) + " more differing lines of the re-saves");
		}
		if (shown == 0) {
			report.checked.add("OpenRocket's re-save (" + ours.size() + " lines) as of its own save");
		}
	}

	private static String text(byte[] bytes) {
		return new String(bytes, StandardCharsets.UTF_8);
	}

	private static List<String> lines(String text) {
		return List.of(text.split("\n", -1));
	}

	/**
	 * For an example or a test rocket: the options and extensions of every simulation of Q, made
	 * reproducible as the harness makes them (SimulationDumper.makeReproducible), are those of the golden
	 * simulation document; for an example (whose re-save was written before the harness changed the
	 * options) also the harness's record of what it changed.
	 */
	private static void checkGoldenSimulations(Arguments a, Input input, OpenRocketDocument q, Report report)
			throws IOException {
		if (input.kind() != Kind.EXAMPLE && input.kind() != Kind.TEST_ROCKET) {
			return;
		}
		List<Simulation> simulations = q.getSimulations();
		if (simulations.size() != input.simulations().size()) {
			report.failures.add(simulations.size() + " simulations, the goldens " + input.simulations().size());
			return;
		}
		int failures = report.failures.size();
		for (int i = 0; i < simulations.size(); i++) {
			JsonObject golden = JsonParser.parseString(Files.readString(a.goldens.resolve(input.simulations().get(i))))
					.getAsJsonObject();
			SimulationOptions options = simulations.get(i).getOptions().clone();
			Map<String, Object> harness = SimulationDumper.makeReproducible(options);
			compareJson("simulation " + i + " options", SimulationDumper.options(options), golden.get("options"),
					report);
			compareJson("simulation " + i + " extensions", SimulationDumper.extensions(simulations.get(i)),
					golden.get("extensions"), report);
			if (input.kind() == Kind.EXAMPLE) {
				compareJson("simulation " + i + " harness", harness, golden.get("harness"), report);
			}
		}
		if (report.failures.size() == failures) {
			report.checked.add("the options and extensions of " + simulations.size()
					+ " simulations as the golden simulations'");
		}
	}

	private static void compareJson(String what, Object ours, JsonElement golden, Report report) {
		JsonElement parsed = JsonParser.parseString(new String(Json.bytes(ours), StandardCharsets.UTF_8));
		if (!parsed.equals(golden)) {
			report.failures.add(what + ": " + parsed + ", the goldens " + golden);
		}
	}

	/** The geometry, mass and aerodynamics documents of a settled design, as GoldenDumper writes them. */
	private static List<byte[]> dumps(String name, OpenRocketDocument document, WarningSet loadWarnings) {
		Rocket rocket = document.getRocket();
		ComponentIndex index = new ComponentIndex(rocket);
		AutomaticDimensions.settle(rocket, index);
		return List.of(Json.bytes(GeometryDumper.dump(name, rocket, index, loadWarnings)),
				Json.bytes(MassDumper.dump(name, rocket, index)), Json.bytes(AeroDumper.dump(name, rocket, index)));
	}

	private static final List<String> DUMP_FILES = List.of("geometry.json", "mass.json", "aero.json");

	private static void checkDumps(Arguments a, Input input, Loaded qtRocket, Loaded ownLoad, Described q,
			Described oo, Report report) throws IOException {
		List<byte[]> expected;
		String reference;
		if (input.kind() == Kind.EXAMPLE) {
			expected = new ArrayList<>();
			for (String file : DUMP_FILES) {
				expected.add(Files.readAllBytes(a.goldens.resolve(input.name()).resolve(file)));
			}
			reference = "the goldens";
		} else if (q.componentIds().equals(oo.componentIds())) {
			// The warnings are checked on their own; the design is compared here.
			expected = dumps(input.name(), ownLoad.document(), new WarningSet());
			reference = "those of OpenRocket's own save";
		} else {
			String drawn = input.kind() == Kind.GUI ? "drawn by QtRocket's GUI, and by DeterministicUuids for "
					+ "OpenRocket's save of the scenario" : "drawn by the loaders";
			report.notCompared.add("geometry.json, mass.json and aero.json: the components have other ids than in "
					+ "OpenRocket's own save (" + drawn + "), and OpenRocket's sums follow the ids in the last bit");
			return;
		}
		WarningSet warnings = input.kind() == Kind.EXAMPLE ? qtRocket.warnings() : new WarningSet();
		List<byte[]> ours = dumps(input.name(), qtRocket.document(), warnings);
		int failures = report.failures.size();
		for (int i = 0; i < DUMP_FILES.size(); i++) {
			if (!Arrays.equals(ours.get(i), expected.get(i))) {
				report.failures.add(DUMP_FILES.get(i) + " differs from " + reference + ": "
						+ firstDifference(lines(text(ours.get(i))), lines(text(expected.get(i)))));
			}
		}
		if (report.failures.size() == failures) {
			report.checked.add("geometry.json, mass.json and aero.json after settling as " + reference);
		}
	}

	// ---------------------------------------------------------------------------------------------
	// The report

	/** Prints the reports and a summary; returns the exit status. */
	private static int printReports(List<Report> reports, int inputs) {
		int failed = 0;
		Map<String, Integer> allowed = new TreeMap<>();
		for (Report report : reports) {
			boolean ok = report.failures.isEmpty();
			failed += ok ? 0 : 1;
			System.out.println();
			System.out.println(report.file + ": " + (ok ? "OK" : "FAILED"));
			for (String line : report.checked) {
				System.out.println("  checked: " + line);
			}
			for (Map.Entry<String, String> line : report.allowed.entrySet()) {
				System.out.println("  allowed: " + line.getKey());
				allowed.merge(line.getValue(), 1, Integer::sum);
			}
			for (String line : report.notCompared) {
				System.out.println("  not compared: " + line);
			}
			for (String line : report.failures) {
				System.out.println("  FAILED: " + line);
			}
		}
		System.out.println();
		List<String> wrongCounts = new ArrayList<>();
		Set<String> names = new TreeSet<>(EXPECTED_ALLOWANCES.keySet());
		names.addAll(allowed.keySet());
		for (String name : names) {
			int expected = EXPECTED_ALLOWANCES.getOrDefault(name, 0);
			int found = allowed.getOrDefault(name, 0);
			if (found != expected) {
				wrongCounts.add(name + " " + found + " times, not " + expected);
			}
		}
		for (String line : wrongCounts) {
			System.out.println("FAILED: an allowance used otherwise than expected: " + line);
		}
		System.out.println("InteropCheck: " + inputs + " inputs, " + reports.size() + " files, " + failed
				+ " failed; allowed: " + (allowed.isEmpty() ? "nothing" : allowed)
				+ (wrongCounts.isEmpty() ? "" : "; " + wrongCounts.size() + " allowances used otherwise than expected"));
		return failed == 0 && wrongCounts.isEmpty() ? 0 : 1;
	}
}
