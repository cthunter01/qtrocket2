package probe;

import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.Module;
import com.google.inject.util.Modules;
import info.openrocket.core.database.ComponentPresetDao;
import info.openrocket.core.database.ComponentPresetDatabase;
import info.openrocket.core.database.motor.MotorDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSQLiteDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSetDatabase;
import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.Simulation;
import info.openrocket.core.file.GeneralRocketLoader;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.preset.ComponentPreset;
import info.openrocket.core.preset.xml.OpenRocketComponentLoader;
import info.openrocket.core.rocketcomponent.FlightConfiguration;
import info.openrocket.core.rocketcomponent.FlightConfigurationId;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.simulation.FlightData;
import info.openrocket.core.startup.Application;
import info.openrocket.core.unit.UnitGroup;
import info.openrocket.core.util.StringUtils;
import info.openrocket.core.util.TestRockets;
import info.openrocket.swing.gui.figureelements.RocketInfo;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.util.GUIUtil;
import info.openrocket.swing.gui.util.SwingPreferences;

import java.awt.Graphics2D;
import java.awt.Rectangle;
import java.awt.image.BufferedImage;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.TreeSet;

/**
 * The flight block of OpenRocket's design view overlay (RocketInfo.drawFlightInformation() and drawFlightData(),
 * RocketInfo.java:541-644): what QtRocket's part G2 (RocketInfo's flight block, BackgroundSimulation) must
 * reproduce. For each design of QtRocket's exit suite OpenDesignsExitGolden (designs.py of the open-designs family,
 * loaded as OpenDesignsTabProbe loads them: the bundled motors, or the seven synthetic ones of TestRockets for the
 * test rockets' re-saves, and the six component presets of the example designs), the state RocketPanel.updateExtras()
 * gives the overlay when the design is opened with the estimate on (the preference "backgroundFlight", true by
 * default) and AutoRunSimulations off (its default): its flight part (RocketPanel.java:1572-1626) and updateSims()
 * (:1635-1659) and runBackgroundSimulations() (:1684-1711) replayed line by line, without starting the worker (no
 * fresh flight is pinned): whether the configuration has motors, which simulations updateSims(false) and
 * updateSims(true) would run, whether the overlay is calculating, which simulation it shows. Then RocketInfo is
 * painted in that state into a Graphics2D that records every drawGlyphVector (RocketInfoProbe's Recorder and
 * RecordingFont), and the texts after those of a paint without flight data (the flight block, drawn last) are
 * printed. A few states of the probe's own follow (named "special:..."): a simple design calculating, after a failed
 * run (FlightData.NaN_DATA), without flight data, in imperial units, and the aborted simulation of the coverage
 * re-save "coverage-stored-flight" with its configuration selected.
 * <p>
 * SwingPreferences in generate.sh's fresh store with MotorNameColumn false (as OpenDesignsTabProbe: the suite's
 * DesignFileEnvironment names a motor by its common name, in the configuration's name too); FlatLaf Light; every
 * design's UUIDs seeded from its name (DeterministicUuids).
 * <p>
 * Output (doubles as Double.toHexString):
 * <ul>
 * <li>COLORS active inactive error text (FlatLaf Light's FLIGHTDATA_TEXT_ACTIVE, FLIGHTDATA_TEXT_INACTIVE, ERROR and
 * TEXT, as r,g,b,a)</li>
 * <li>S name kind | file (a design of designs.py, or "special:...", and the file it is loaded from)</li>
 * <li>P estimated|set metric|imperial select (how the scenario is set up: its overlay as updateExtras() leaves it
 * for the design just opened, or set by the probe; the default units; the index of the simulation whose
 * configuration the probe selected before, or - for the design's own selection)</li>
 * <li>C hasMotors | name (the selected configuration: hasMotors(), getName())</li>
 * <li>M index status hasData configMatches configHasMotors altitude velocity mach acceleration | name (each
 * simulation: getStatus() as updateSims() finds it, hasSummaryData(), whether its configuration is the selected one,
 * whether its configuration has motors, its data's maxima)</li>
 * <li>U current|all indices... (the simulations updateSims(false) and updateSims(true) would run, in order)</li>
 * <li>O state calculating shown (state NONE: no flight data; NAN: FlightData.NaN_DATA; DATA: the shown simulation's
 * data; shown: the index of the overlay's simulation, or -)</li>
 * <li>A status | text (the shown simulation's getStatus() at the paint, and removeHTMLTags(getStatusDescription()))</li>
 * <li>F line x1 y2 (the frame of the paint)</li>
 * <li>T font colour x y visualWidth | text (each text of the flight block, in the order RocketInfo draws them)</li>
 * <li>E</li>
 * </ul>
 * usage: RocketInfoFlightProbe DESIGNS COMPONENTS
 */
public class RocketInfoFlightProbe {

	/** The digests of the six component presets of the example designs (tests/core/rocket/preset/ExamplePresets.h). */
	static final Set<String> EXAMPLE_PRESET_DIGESTS = Set.of("e9c3e97d32e96762f65e051d74cb9fd7",
			"b595c8a31baaf325291d5c957f3940f8", "9a96b8806749e790db29bacfbc70eb1e", "18363dec5642206a109389ef48efcb32",
			"a59dec8e4034a2fee5955dbf4ff07f1c", "6f564a198c80de3af437608827fa1bc1");

	/** The generators of the motors of TestRockets, in the order of QtRocket's testRocketMotors(). */
	static final List<String> TEST_ROCKET_MOTORS = List.of("generateMotor_A8_18mm", "generateMotor_A10_13mm",
			"generateMotor_B4_18mm", "generateMotor_C6_18mm", "generateMotor_D21_18mm", "generateMotor_M1350_75mm",
			"generateMotor_G77_29mm");

	static Injector designInjector;
	static Injector testRocketInjector;
	static SwingPreferences prefs;

	/** What the overlay shows of the flight. */
	enum Shown {
		/** RocketInfo.setSimulation(null) with no flight data. */
		NONE,
		/** FlightData.NaN_DATA (with the overlay's simulation as it was). */
		NAN,
		/** The data of the overlay's simulation (setSimulation(), hasSummaryData()). */
		DATA
	}

	/** The overlay's flight state: what RocketPanel set on RocketInfo. */
	static final class FlightState {
		Shown shown = Shown.NONE;
		boolean calculating = false;
		/** The overlay's simulation (RocketInfo.simulation), or null. */
		Simulation simulation = null;
	}

	public static void main(String[] args) throws Exception {
		int status = 0;
		try {
			bootstrap(new File(args[1]));
			UITheme.Themes.LIGHT.applyTheme();
			prefs = (SwingPreferences) Application.getPreferences();
			if (!prefs.computeFlightInBackground() || prefs.getAutoRunSimulations()) {
				throw new IllegalStateException("backgroundFlight off or AutoRunSimulations on");
			}
			line("COLORS", RocketInfoProbe.color(UITheme.getColor(UITheme.Keys.FLIGHTDATA_TEXT_ACTIVE)),
					RocketInfoProbe.color(UITheme.getColor(UITheme.Keys.FLIGHTDATA_TEXT_INACTIVE)),
					RocketInfoProbe.color(UITheme.getColor(UITheme.Keys.ERROR)),
					RocketInfoProbe.color(UITheme.getColor(UITheme.Keys.TEXT)));
			File storedFlight = null;
			File simple = null;
			for (String design : Files.readAllLines(Path.of(args[0]), StandardCharsets.UTF_8)) {
				String[] fields = design.split("\t");
				if (fields.length != 3) {
					throw new IllegalStateException("not a design: " + design);
				}
				File file = new File(fields[2]);
				OpenRocketDocument doc = load(fields[0], fields[1], file);
				scenario(fields[0], fields[1], file, "estimated metric -", doc, opened(doc));
				if (fields[0].equals("coverage_stored_flight")) {
					storedFlight = file;
				} else if (fields[0].equals("example_a_simple_model_rocket")) {
					simple = file;
				}
			}
			if (storedFlight == null || simple == null) {
				throw new IllegalStateException("the designs of the special scenarios are missing");
			}
			specials(simple, storedFlight);
		} catch (Throwable e) {
			e.printStackTrace();
			status = 1;
		}
		System.out.flush();
		System.exit(status);
	}

	/** The probe's own states. */
	static void specials(File simple, File storedFlight) throws Exception {
		// The simple design's simulation re-running: its data dimmed, "Calculating..." above the block.
		OpenRocketDocument doc = load("special:simple-calculating", "special", simple);
		FlightState state = opened(doc);
		state.calculating = true;
		scenario("special:simple-calculating", "special", simple, "set metric -", doc, state);

		// After a failed run (simulationInterrupted(): FlightData.NaN_DATA, not calculating).
		doc = load("special:simple-failed", "special", simple);
		state = opened(doc);
		state.shown = Shown.NAN;
		scenario("special:simple-failed", "special", simple, "set metric -", doc, state);

		// NaN_DATA while calculating.
		doc = load("special:simple-failed-calculating", "special", simple);
		state = opened(doc);
		state.shown = Shown.NAN;
		state.calculating = true;
		scenario("special:simple-failed-calculating", "special", simple, "set metric -", doc, state);

		// No flight data while calculating: "Calculating..." alone, on the bottom row.
		doc = load("special:simple-calculating-without-data", "special", simple);
		state = new FlightState();
		state.calculating = true;
		scenario("special:simple-calculating-without-data", "special", simple, "set metric -", doc, state);

		// Imperial units.
		UnitGroup.setDefaultImperialUnits();
		doc = load("special:simple-imperial", "special", simple);
		scenario("special:simple-imperial", "special", simple, "estimated imperial -", doc, opened(doc));
		UnitGroup.setDefaultMetricUnits();

		// The aborted simulation of the coverage re-save, its configuration selected: it is not up to date (ABORTED),
		// so updateSims(false) runs it, and the overlay shows its status line over its data, dimmed.
		doc = load("special:stored-flight-aborted", "coverage", storedFlight);
		Simulation aborted = doc.getSimulation(0);
		doc.getRocket().setSelectedConfiguration(aborted.getFlightConfigurationId());
		scenario("special:stored-flight-aborted", "coverage", storedFlight, "estimated metric 0", doc,
				opened(doc));
		// The same, not calculating (once its run would have failed, or after it with the abort again).
		doc = load("special:stored-flight-aborted-shown", "coverage", storedFlight);
		aborted = doc.getSimulation(0);
		doc.getRocket().setSelectedConfiguration(aborted.getFlightConfigurationId());
		state = opened(doc);
		state.calculating = false;
		scenario("special:stored-flight-aborted-shown", "coverage", storedFlight, "set metric 0", doc, state);
	}

	/** Loads the design @p name of the kind @p kind from @p file. */
	static OpenRocketDocument load(String name, String kind, File file) throws Exception {
		Application.setInjector(kind.equals("testrocket") ? testRocketInjector : designInjector);
		DeterministicUuids.reseed(name);
		return new GeneralRocketLoader(file).load();
	}

	/** The simulations updateSims(@p updateAllSims) would run (RocketPanel.java:1635-1659), by index. */
	static List<Integer> updateSims(OpenRocketDocument document, boolean updateAllSims) {
		FlightConfigurationId curID = document.getSelectedConfiguration().getFlightConfigurationID();
		List<Integer> sims = new LinkedList<>();
		List<Simulation> all = document.getSimulations();
		for (int index = 0; index < all.size(); index++) {
			Simulation sim = all.get(index);
			if (Simulation.isStatusUpToDate(sim.getStatus())
					|| !document.getRocket().getFlightConfiguration(sim.getFlightConfigurationId()).hasMotors())
				continue;

			// Find a Simulation based on the current flight configuration
			if (!updateAllSims) {
				if (sim.getFlightConfigurationId().compareTo(curID) == 0) {
					sims.add(index);
					break;
				}
			} else {
				sims.add(index);
			}
		}
		return sims;
	}

	/** The first simulation of the selected configuration (updateFlightData(), RocketPanel.java:1668-1675), or -1. */
	static int firstOfSelectedConfiguration(OpenRocketDocument document) {
		FlightConfigurationId curID = document.getSelectedConfiguration().getFlightConfigurationID();
		List<Simulation> all = document.getSimulations();
		for (int index = 0; index < all.size(); index++) {
			if (all.get(index).getFlightConfigurationId().compareTo(curID) == 0) {
				return index;
			}
		}
		return -1;
	}

	/**
	 * The overlay as RocketPanel.updateExtras() leaves it for a design just opened (the remembered ids are those of
	 * no design yet), with backgroundFlight on and AutoRunSimulations off; the U lines with what updateSims() would
	 * run.
	 */
	static FlightState opened(OpenRocketDocument document) {
		FlightState state = new FlightState();
		FlightConfiguration curConfig = document.getSelectedConfiguration();
		// Check that configuration has motors
		if (!curConfig.hasMotors()) {
			state.simulation = null;
			state.shown = Shown.NAN;
			state.calculating = false;
			return state;
		}
		// updateSims(false): calculating, and runBackgroundSimulations() of what it finds.
		List<Integer> sims = updateSims(document, false);
		state.calculating = true;
		if (sims.isEmpty()) {
			state.calculating = false;
			int shown = firstOfSelectedConfiguration(document);
			if (shown >= 0) {
				setSimulation(state, document.getSimulation(shown));
			} else {
				state.shown = Shown.NAN;
			}
		}
		// updateExtras()'s loop: the first simulation of the selected configuration.
		int shown = firstOfSelectedConfiguration(document);
		if (shown >= 0) {
			setSimulation(state, document.getSimulation(shown));
		}
		return state;
	}

	/** RocketInfo.setSimulation(). */
	static void setSimulation(FlightState state, Simulation simulation) {
		state.simulation = simulation;
		state.shown = simulation.hasSummaryData() ? Shown.DATA : Shown.NAN;
	}

	static void scenario(String name, String kind, File file, String setup, OpenRocketDocument document,
			FlightState state) throws Exception {
		System.out.println("S " + name + " " + kind + " | " + file.getPath());
		System.out.println("P " + setup);
		FlightConfiguration config = document.getSelectedConfiguration();
		System.out.println("C " + config.hasMotors() + " | " + config.getName());
		FlightConfigurationId curID = config.getFlightConfigurationID();
		List<Simulation> all = document.getSimulations();
		for (int index = 0; index < all.size(); index++) {
			Simulation sim = all.get(index);
			FlightData data = sim.getSimulatedData();
			boolean hasData = sim.hasSummaryData();
			System.out.println("M " + index + " " + sim.getStatus().name() + " " + hasData + " "
					+ (sim.getFlightConfigurationId().compareTo(curID) == 0) + " "
					+ document.getRocket().getFlightConfiguration(sim.getFlightConfigurationId()).hasMotors() + " "
					+ (hasData ? RocketInfoProbe.hex(data.getMaxAltitude()) + " "
							+ RocketInfoProbe.hex(data.getMaxVelocity()) + " "
							+ RocketInfoProbe.hex(data.getMaxMachNumber()) + " "
							+ RocketInfoProbe.hex(data.getMaxAcceleration()) : "- - - -")
					+ " | " + sim.getName());
		}
		if (config.hasMotors()) {
			line("U", "current", indices(updateSims(document, false)));
			line("U", "all", indices(updateSims(document, true)));
		} else {
			line("U", "current", "");
			line("U", "all", "");
		}
		int shownIndex = state.simulation == null ? -1 : all.indexOf(state.simulation);
		line("O", state.shown.name(), Boolean.toString(state.calculating), shownIndex < 0 ? "-" : Integer.toString(shownIndex));
		if (state.simulation != null) {
			System.out.println("A " + state.simulation.getStatus().name() + " | "
					+ StringUtils.removeHTMLTags(state.simulation.getStatusDescription()));
		}
		paint(config, state);
		System.out.println("E");
	}

	static String indices(List<Integer> list) {
		StringBuilder text = new StringBuilder();
		for (int index : list) {
			if (text.length() > 0) {
				text.append(' ');
			}
			text.append(index);
		}
		return text.toString();
	}

	/** The overlay of @p config with the flight state @p state: the texts of its flight block. */
	static void paint(FlightConfiguration config, FlightState state) throws Exception {
		RocketInfo info = new RocketInfo(config);
		info.setCurrentConfig(config);
		List<String> baseline = paintLines(info);
		info.setSimulation(state.simulation);
		switch (state.shown) {
			case NONE:
				info.setFlightData(null);
				break;
			case NAN:
				info.setFlightData(FlightData.NaN_DATA);
				break;
			case DATA:
				break;
		}
		info.setCalculatingData(state.calculating);
		List<String> lines = paintLines(info);
		for (int index = 0; index < baseline.size(); index++) {
			if (!lines.get(index).equals(baseline.get(index))) {
				throw new IllegalStateException("the flight state changed the overlay's other texts");
			}
		}
		System.out.println("F " + RocketInfoProbe.hex((Float) RocketInfoProbe.field(info, "line")) + " "
				+ RocketInfoProbe.hex((Float) RocketInfoProbe.field(info, "x1")) + " "
				+ RocketInfoProbe.hex((Float) RocketInfoProbe.field(info, "y2")));
		for (String text : lines.subList(baseline.size(), lines.size())) {
			System.out.println(text);
		}
	}

	/** What @p info paints into (0, 0, 1000, 700), as RocketInfoProbe.paint() records it. */
	static List<String> paintLines(RocketInfo info) throws Exception {
		float size = prefs.getRocketInfoFontSize();
		RocketInfoProbe.setField(info, "font", new RocketInfoProbe.RecordingFont(
				GUIUtil.createUIFont(GUIUtil.UI_FONT_STYLE_REGULAR, size, 0.0f), "regular"));
		RocketInfoProbe.setField(info, "smallFont", new RocketInfoProbe.RecordingFont(
				GUIUtil.createUIFont(GUIUtil.UI_FONT_STYLE_REGULAR, (float) (size - 1.5), 0.0f), "small"));
		RocketInfoProbe.setField(info, "boldFont", new RocketInfoProbe.RecordingFont(
				GUIUtil.createUIFont(GUIUtil.UI_FONT_STYLE_BOLD, (float) (size + 1.5), 0.0f), "bold"));
		BufferedImage image = new BufferedImage(1000, 700, BufferedImage.TYPE_INT_ARGB);
		Graphics2D g2 = image.createGraphics();
		RocketInfoProbe.Recorder recorder = new RocketInfoProbe.Recorder(g2);
		try {
			info.paint(recorder, 1.0, new Rectangle(0, 0, 1000, 700));
		} finally {
			g2.dispose();
		}
		return recorder.lines;
	}

	static void line(String... fields) {
		System.out.println(String.join(" ", fields));
	}

	// ------------------------------------------------------------ the designs' environment (OpenDesignsTabProbe)

	static void collectOrcFiles(File directory, List<File> files) {
		File[] children = directory.listFiles();
		if (children == null) {
			throw new IllegalStateException("not a directory: " + directory);
		}
		Arrays.sort(children, (a, b) -> a.getName().compareTo(b.getName()));
		for (File child : children) {
			if (child.getName().startsWith(".")) {
				continue;
			}
			if (child.isDirectory()) {
				collectOrcFiles(child, files);
			} else if (child.getName().toLowerCase(Locale.ROOT).endsWith(".orc")) {
				files.add(child);
			}
		}
	}

	/** Adds to @p presets the six presets of the example designs, from the bundled .orc files below @p components. */
	static void addExamplePresets(ComponentPresetDatabase presets, File components) throws Exception {
		List<File> files = new ArrayList<>();
		collectOrcFiles(components, files);
		Set<String> found = new TreeSet<>();
		int count = 0;
		for (File file : files) {
			try (InputStream in = new FileInputStream(file)) {
				for (ComponentPreset preset : new OpenRocketComponentLoader().load(in, file.getName())) {
					if (EXAMPLE_PRESET_DIGESTS.contains(preset.getDigest())) {
						presets.add(preset);
						found.add(preset.getDigest());
						++count;
					}
				}
			}
		}
		if (count != EXAMPLE_PRESET_DIGESTS.size() || !found.equals(EXAMPLE_PRESET_DIGESTS)) {
			throw new IllegalStateException("the bundled presets hold " + count
					+ " presets of the example designs' digests, not each of the six once: " + found);
		}
	}

	static Injector injector(ThrustCurveMotorSetDatabase motors, ComponentPresetDatabase presets) {
		Module overrides = new AbstractModule() {
			@Override
			protected void configure() {
				bind(ApplicationPreferences.class).to(SwingPreferences.class);
				bind(ComponentPresetDao.class).toInstance(presets);
				bind(ThrustCurveMotorSetDatabase.class).toInstance(motors);
				bind(MotorDatabase.class).to(ThrustCurveMotorSetDatabase.class);
			}
		};
		return Guice.createInjector(Modules.override(new info.openrocket.swing.ServicesForTesting()).with(overrides),
				new PluginModule());
	}

	static void bootstrap(File components) throws Exception {
		// Before anything makes a UUID: each design gets the same UUIDs on every run.
		DeterministicUuids.install();
		DeterministicUuids.reseed("rocket-info-flight");
		Path bundledDb = Path.of(System.getProperty("probe.motors"));
		Path dbCopy = Files.createTempFile("probe-motors", ".db");
		Files.copy(bundledDb, dbCopy, StandardCopyOption.REPLACE_EXISTING);
		dbCopy.toFile().deleteOnExit();
		ThrustCurveMotorSetDatabase bundled = new ThrustCurveMotorSetDatabase();
		for (ThrustCurveMotor motor : ThrustCurveMotorSQLiteDatabase.readDatabase(dbCopy.toFile())) {
			bundled.addMotor(motor);
		}
		ThrustCurveMotorSetDatabase synthetic = new ThrustCurveMotorSetDatabase();
		for (String generator : TEST_ROCKET_MOTORS) {
			Method method = TestRockets.class.getDeclaredMethod(generator);
			method.setAccessible(true);
			synthetic.addMotor((ThrustCurveMotor) method.invoke(null));
		}
		ComponentPresetDatabase presets = new ComponentPresetDatabase();
		designInjector = injector(bundled, presets);
		testRocketInjector = injector(synthetic, presets);
		Application.setInjector(designInjector);
		addExamplePresets(presets, components);
		// The suite's preference store shows a motor by its common name (DesignFileEnvironment), in the
		// configurations' names too (Motor.getMotorName()); a SwingPreferences writes it to the store every other
		// instance reads.
		Application.getPreferences().putBoolean(ApplicationPreferences.MOTOR_NAME_COLUMN, false);
		if (Application.getPreferences().getMotorNameColumn()) {
			throw new IllegalStateException("MotorNameColumn is not false");
		}
	}
}
