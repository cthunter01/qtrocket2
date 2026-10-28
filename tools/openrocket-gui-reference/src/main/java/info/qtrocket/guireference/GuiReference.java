package info.qtrocket.guireference;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.InvocationTargetException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.EnumSet;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.prefs.Preferences;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

import javax.swing.Action;
import javax.swing.SwingUtilities;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonArray;
import com.google.gson.JsonObject;
import com.google.inject.Guice;
import com.google.inject.Injector;

import info.openrocket.core.database.ComponentPresetDao;
import info.openrocket.core.database.ComponentPresetDatabaseLoader;
import info.openrocket.core.database.Databases;
import info.openrocket.core.database.MotorDatabaseLoader;
import info.openrocket.core.database.motor.ThrustCurveMotorSet;
import info.openrocket.core.database.motor.ThrustCurveMotorSetDatabase;
import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.file.GeneralRocketLoader;
import info.openrocket.core.logging.Warning;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.startup.Application;
import info.openrocket.core.startup.ExceptionHandler;
import info.openrocket.core.util.BuildProperties;
import info.openrocket.swing.gui.util.GUIUtil;
import info.openrocket.swing.gui.util.SwingPreferences;
import info.openrocket.swing.startup.GuiModule;

/**
 * The GUI reference tool: runs a scenario of {@code tests/data/goldens/gui/<name>/} on OpenRocket's Swing GUI
 * code, saves the design with OpenRocket's saver and writes QtRocket's reference of it into that directory:
 * {@code rocket.ork} (the document OpenRocket saved) and {@code state.json} (what each step left, and the saved
 * file as OpenRocket loads it). A scenario of version 1 (the from-scratch scenario of Milestone 2) runs
 * headless; one of version 2 (Milestone 3, the Motors &amp; Configuration tab) runs OpenRocket's real panels
 * and dialogs on a virtual display (generate.sh runs it under xvfb-run). See README.md.
 *
 * <pre>
 *   GuiReference --scenario &lt;scenario.json&gt; --out &lt;directory&gt; --work &lt;directory&gt;
 *                --commit &lt;OpenRocket commit&gt; --dirty &lt;true|false&gt;
 *                [--repo &lt;QtRocket repository&gt; --preset-commit &lt;openrocket-database commit&gt;]  (version 2)
 * </pre>
 */
public final class GuiReference {

	/**
	 * The steps that set a value in a dialog: each must change the component tree (its digest), or the step
	 * would check nothing (a value the field already has, which a binding that does nothing also passes).
	 */
	private static final Set<Scenario.Kind> FIELD_STEPS = Set.of(Scenario.Kind.TYPE, Scenario.Kind.CHOOSE,
			Scenario.Kind.CHECK, Scenario.Kind.EDIT_POINT, Scenario.Kind.INSERT_POINT);

	/** The steps after which a record holds the whole tree, not only its digest. */
	private static final Set<Scenario.Kind> CHECKPOINTS = Set.of(Scenario.Kind.NEW, Scenario.Kind.OK,
			Scenario.Kind.BUTTON, Scenario.Kind.UNDO, Scenario.Kind.REDO, Scenario.Kind.PASTE,
			Scenario.Kind.CHOOSE_FILE);

	/** The steps of the tab that change the design: each must change it, but for a cancelled question. */
	private static final Set<Scenario.Kind> TAB_OPERATIONS;

	/** The steps after which a record of version 2 holds the whole tree: those of version 1, open and the operations. */
	private static final Set<Scenario.Kind> CHECKPOINTS_V2;

	static {
		EnumSet<Scenario.Kind> operations = EnumSet.copyOf(Scenario.TAB_KINDS);
		operations.removeAll(EnumSet.of(Scenario.Kind.CONFIG_TAB, Scenario.Kind.SELECT_CELLS));
		TAB_OPERATIONS = Collections.unmodifiableSet(operations);
		EnumSet<Scenario.Kind> checkpoints = EnumSet.copyOf(CHECKPOINTS);
		checkpoints.add(Scenario.Kind.OPEN);
		checkpoints.addAll(operations);
		CHECKPOINTS_V2 = Collections.unmodifiableSet(checkpoints);
	}

	private GuiReference() {
	}

	public static void main(String[] args) {
		try {
			generate(args);
		} catch (Throwable t) {
			// The windows and the event dispatch thread of version 2 would keep the JVM running.
			t.printStackTrace();
			System.exit(1);
		}
		System.exit(0);
	}

	private static void generate(String[] args) throws Exception {
		Map<String, String> options = options(args);
		Path scenarioFile = Paths.get(options.get("--scenario"));
		Path out = Paths.get(options.get("--out"));
		Path work = Paths.get(options.get("--work"));

		// Before anything creates a UUID.
		DeterministicUuids.install();
		Scenario scenario = Scenario.read(scenarioFile);
		// The seed of the component ids: the scenario's directory ("gui/from-scratch").
		String seed = "gui/" + scenarioFile.toAbsolutePath().getParent().getFileName();
		if (scenario.version() == 2) {
			mainV2(options, scenarioFile, scenario, seed, out, work);
			return;
		}
		bootstrap();

		// A first run initializes every class the scenario reaches; some static initializers draw
		// UUIDs, which would shift the ids of the run that comes first.
		run(scenario, work.resolve("warm-up"), false, seed);
		// Two runs of the scenario in two directories: the first records every step, the second asks
		// nothing between the steps. The getters the records call store what automatic dimensions
		// compute, as a GUI's figure and tree do; the two saves are the same file only when that order
		// does not matter for this scenario, which is what makes the reference one a GUI can match.
		Run recorded = run(scenario, work.resolve("recorded"), true, seed);
		Run plain = run(scenario, work.resolve("plain"), false, seed);
		if (!Arrays.equals(recorded.document, plain.document)) {
			throw new IllegalStateException("the save depends on what was asked between the steps: compare "
					+ recorded.file + " with " + plain.file);
		}

		// The reference: the document of the archive GeneralRocketSaver wrote, as a plain .ork file (the
		// archive's entries carry the time of the save).
		Files.createDirectories(out);
		Path reference = out.resolve("rocket.ork");
		Files.write(reference, recorded.document);

		// The saved file as OpenRocket loads it: the reference, and the archive itself, must give the same.
		List<String> loaded = loadedState(reference);
		List<String> loadedArchive = loadedState(recorded.file.toPath());
		if (!loaded.equals(loadedArchive)) {
			throw new IllegalStateException("the plain document and the archive load differently");
		}
		List<String> numbers = RocketState.numbers(new GeneralRocketLoader(reference.toFile()).load());

		JsonObject state = new JsonObject();
		state.addProperty("schema", "gui-from-scratch-reference");
		state.addProperty("schemaVersion", 1);
		state.addProperty("generator", "tools/openrocket-gui-reference/generate.sh (GuiReference)");
		JsonObject openrocket = new JsonObject();
		openrocket.addProperty("commit", options.get("--commit"));
		openrocket.addProperty("version", BuildProperties.getVersion());
		openrocket.addProperty("dirty", Boolean.parseBoolean(options.get("--dirty")));
		openrocket.addProperty("javaVersion", System.getProperty("java.version"));
		state.add("openrocket", openrocket);
		state.add("scenario", scenarioInfo(scenarioFile, scenario));
		JsonObject saved = new JsonObject();
		saved.addProperty("file", reference.getFileName().toString());
		saved.addProperty("savedAs", recorded.file.getName());
		saved.add("archiveEntries", strings(recorded.entries));
		state.add("reference", saved);
		state.add("steps", recorded.records);
		JsonObject load = new JsonObject();
		load.add("lines", strings(loaded));
		load.add("numbers", strings(numbers));
		state.add("loaded", load);
		write(out, state, reference, recorded);
	}

	private static Map<String, String> options(String[] args) {
		Map<String, String> options = new HashMap<>();
		for (int i = 0; i + 1 < args.length; i += 2) {
			options.put(args[i], args[i + 1]);
		}
		for (String required : List.of("--scenario", "--out", "--work", "--commit", "--dirty")) {
			if (!options.containsKey(required)) {
				throw new IllegalArgumentException("missing " + required);
			}
		}
		return options;
	}

	private static JsonObject scenarioInfo(Path scenarioFile, Scenario scenario) throws Exception {
		JsonObject scenarioInfo = new JsonObject();
		scenarioInfo.addProperty("file", scenarioFile.getFileName().toString());
		scenarioInfo.addProperty("sha256", sha256(Files.readAllBytes(scenarioFile)));
		scenarioInfo.addProperty("title", scenario.title());
		scenarioInfo.addProperty("steps", scenario.steps().size());
		return scenarioInfo;
	}

	private static void write(Path out, JsonObject state, Path reference, Run recorded) throws IOException {
		Gson gson = new GsonBuilder().setPrettyPrinting().disableHtmlEscaping().create();
		Files.writeString(out.resolve("state.json"), gson.toJson(state) + "\n", StandardCharsets.UTF_8);
		System.err.println("Wrote " + reference + " (" + recorded.document.length + " bytes) and "
				+ out.resolve("state.json"));
	}

	/** The exception handler of the tool's start-up: it fails the run instead of showing a dialog. */
	private static void failingExceptionHandler() {
		Application.setExceptionHandler(new ExceptionHandler() {
			@Override
			public void handleErrorCondition(String message) {
				throw new IllegalStateException(message);
			}

			@Override
			public void handleErrorCondition(String message, Throwable exception) {
				throw new IllegalStateException(message, exception);
			}

			@Override
			public void handleErrorCondition(Throwable exception) {
				throw new IllegalStateException(exception);
			}

			@Override
			public void uncaughtException(Thread thread, Throwable throwable) {
				throw new IllegalStateException(throwable);
			}
		});
	}

	/**
	 * OpenRocket's start-up as SwingStartup.runInEDT (:195-239) does it, without its windows: the
	 * GuiModule and PluginModule injector, the databases (the component presets and the motors
	 * bypassed, so both are empty, as QtRocket's start-up has them in Milestone 2), the default units
	 * and the default component materials. The exception handler fails the run instead of showing a
	 * dialog; the look and feel is not set (nothing is drawn). Version 1.
	 */
	private static void bootstrap() {
		System.setProperty("openrocket.bypass.presets", "true");
		System.setProperty("openrocket.bypass.motors", "true");
		failingExceptionHandler();
		GuiModule guiModule = new GuiModule();
		Injector injector = Guice.createInjector(guiModule, new PluginModule());
		Application.setInjector(injector);
		guiModule.startLoader();
		((SwingPreferences) Application.getPreferences()).loadDefaultUnits();
		((SwingPreferences) Application.getPreferences()).loadDefaultComponentMaterials();
		Databases.fakeMethod();
	}

	/** One run of the scenario: the saved archive, its entries, its document and the step records. */
	private record Run(File file, List<String> entries, byte[] document, JsonArray records) {
	}

	private static Run run(Scenario scenario, Path directory, boolean record, String seed) throws Exception {
		Files.createDirectories(directory);
		DeterministicUuids.reseed(seed);
		JsonArray records = new JsonArray();
		Session session = new Session(directory.toFile());
		JsonObject previous = null;
		if (record) {
			previous = record(scenario.steps().get(0), session);
			records.add(previous);
		}
		for (Scenario.Step step : scenario.steps().subList(1, scenario.steps().size())) {
			try {
				session.run(step);
			} catch (RuntimeException e) {
				throw new IllegalStateException("step " + step.index() + " (" + step.kind().name + ") failed", e);
			}
			if (record) {
				JsonObject current = record(step, session);
				if (FIELD_STEPS.contains(step.kind())
						&& current.get("treeSha256").equals(previous.get("treeSha256"))) {
					throw new IllegalStateException("step " + step.index() + " (" + step.kind().name
							+ ") left the component tree as it was: give the field a value it does not have");
				}
				records.add(current);
				previous = current;
			}
		}
		return finish(session, records);
	}

	/** The end of a run: the scenario must have saved the design with Save as; the archive's entries. */
	private static Run finish(Session session, JsonArray records) throws IOException {
		if (session.savedFile() == null || session.openWindow() != null) {
			throw new IllegalStateException("the scenario must end with the file chooser of Save as");
		}
		Map<String, byte[]> entries = entries(session.savedFile().toPath());
		if (!entries.containsKey("rocket.ork")) {
			throw new IllegalStateException("the archive has no rocket.ork: " + entries.keySet());
		}
		return new Run(session.savedFile(), new ArrayList<>(entries.keySet()), entries.get("rocket.ork"), records);
	}

	/** What a step left: the window state, the selection, the undo actions and the tree (see README.md). */
	private static JsonObject record(Scenario.Step step, Session session) throws Exception {
		JsonObject object = new JsonObject();
		object.addProperty("index", step.index());
		object.addProperty("step", step.kind().name);
		object.add("undo", action(session.undoAction()));
		object.add("redo", action(session.redoAction()));
		object.addProperty("saved", session.document().isSaved());
		object.add("selection", selection(session));
		String window = session.openWindow();
		if (window != null) {
			JsonObject open = new JsonObject();
			open.addProperty("window", window);
			if (session.dialogComponent() != null) {
				open.add("component", ComponentPaths.json(session.dialogComponent()));
			}
			object.add("open", open);
		}
		List<String> tree = RocketState.dump(session.document(), Collections.emptySet());
		object.addProperty("treeSha256", sha256(String.join("\n", tree).concat("\n").getBytes(StandardCharsets.UTF_8)));
		if (CHECKPOINTS.contains(step.kind())) {
			object.add("tree", strings(tree));
		}
		return object;
	}

	/**
	 * The selection model can hold the components an undo or a redo replaced: ComponentTreeModel restores the
	 * selection by id and keeps it when the ids are the same (restoreSelection, isCurrentSelection). The record
	 * names the components of the tree that have those ids.
	 */
	private static JsonArray selection(Session session) {
		JsonArray selection = new JsonArray();
		for (RocketComponent c : session.selection()) {
			RocketComponent live = session.document().getRocket().findComponent(c.getID());
			if (live == RocketComponent.REMOVED) {
				throw new IllegalStateException("a selected component is not in the tree: " + c);
			}
			selection.add(ComponentPaths.json(live));
		}
		return selection;
	}

	private static JsonObject action(Action action) {
		JsonObject object = new JsonObject();
		object.addProperty("text", (String) action.getValue(Action.NAME));
		object.addProperty("enabled", action.isEnabled());
		return object;
	}

	/**
	 * The state of @p file as OpenRocket loads it (RecordedProbe.run, "full"): a GeneralRocketLoader of
	 * the file, the rocket's change events counted during the load.
	 */
	private static List<String> loadedState(Path file) throws Exception {
		byte[] document = Files.readAllBytes(file);
		if (document.length > 1 && document[0] == 'P' && document[1] == 'K') {
			document = entries(file).get("rocket.ork");
		}
		GeneralRocketLoader loader = new GeneralRocketLoader(file.toFile());
		OpenRocketDocument doc = (OpenRocketDocument) RocketState.field(loader, "doc");
		final int[] events = new int[1];
		doc.getRocket().addComponentChangeListener(e -> events[0]++);
		loader.load();
		List<String> warnings = new ArrayList<>();
		for (Warning w : loader.getWarnings()) {
			warnings.add(w.toString());
		}
		return RocketState.loadedState(document, doc, warnings, events[0]);
	}

	/** The entries of a zip archive in their order, with their bytes. */
	private static Map<String, byte[]> entries(Path file) throws IOException {
		Map<String, byte[]> entries = new LinkedHashMap<>();
		try (InputStream in = Files.newInputStream(file); ZipInputStream zip = new ZipInputStream(in)) {
			ZipEntry entry;
			while ((entry = zip.getNextEntry()) != null) {
				ByteArrayOutputStream bytes = new ByteArrayOutputStream();
				zip.transferTo(bytes);
				entries.put(entry.getName(), bytes.toByteArray());
			}
		}
		return entries;
	}

	private static JsonArray strings(List<String> values) {
		JsonArray array = new JsonArray();
		for (String value : values) {
			array.add(value);
		}
		return array;
	}

	private static String sha256(byte[] bytes) throws Exception {
		StringBuilder hex = new StringBuilder();
		for (byte b : MessageDigest.getInstance("SHA-256").digest(bytes)) {
			hex.append(String.format("%02x", b));
		}
		return hex.toString();
	}

	// ====================================================================== version 2

	/** Something that runs on the event dispatch thread and may fail. */
	private interface EdtTask<T> {
		T run() throws Exception;
	}

	/** Runs @p task on the event dispatch thread and waits for it; its exception is rethrown. */
	private static <T> T onEdt(EdtTask<T> task) throws Exception {
		Object[] result = new Object[1];
		try {
			SwingUtilities.invokeAndWait(() -> {
				try {
					result[0] = task.run();
				} catch (Exception e) {
					throw new EdtFailure(e);
				}
			});
		} catch (InvocationTargetException e) {
			Throwable cause = e.getCause();
			if (cause instanceof EdtFailure failure) {
				throw failure.exception;
			}
			if (cause instanceof Exception exception) {
				throw exception;
			}
			throw new IllegalStateException(cause);
		}
		@SuppressWarnings("unchecked")
		T value = (T) result[0];
		return value;
	}

	/** The exceptions no task of the tool caught (version 2). */
	private static final List<Throwable> UNCAUGHT = new ArrayList<>();

	/** Fails when an exception went uncaught since the last call. */
	private static void requireNoUncaughtException(String when) {
		synchronized (UNCAUGHT) {
			if (!UNCAUGHT.isEmpty()) {
				throw new IllegalStateException("an exception went uncaught " + when, UNCAUGHT.get(0));
			}
		}
	}

	/** An exception of an EdtTask, carried out of invokeAndWait. */
	private static final class EdtFailure extends RuntimeException {
		private static final long serialVersionUID = 1L;
		final Exception exception;

		EdtFailure(Exception exception) {
			super(exception);
			this.exception = exception;
		}
	}

	/**
	 * A scenario of version 2: the start-up with the bundled motors and presets, the three runs (as for
	 * version 1) on the event dispatch thread, each with OpenRocket's preferences as the start-up left them,
	 * and state.json of version 2.
	 */
	private static void mainV2(Map<String, String> options, Path scenarioFile, Scenario scenario, String seed,
			Path out, Path work) throws Exception {
		for (String required : List.of("--repo", "--preset-commit")) {
			if (!options.containsKey(required)) {
				throw new IllegalArgumentException("missing " + required + " (a scenario of version 2)");
			}
		}
		Path repo = Paths.get(options.get("--repo"));
		Path input = repo.resolve(scenario.steps().get(0).file());
		// An exception of the event dispatch thread outside the tool's own tasks (a listener, a renderer)
		// fails the run: OpenRocket's GUI would show its error dialog.
		Thread.setDefaultUncaughtExceptionHandler((thread, exception) -> {
			synchronized (UNCAUGHT) {
				UNCAUGHT.add(exception);
			}
			exception.printStackTrace();
		});
		JsonObject startup = onEdt(GuiReference::bootstrapV2);
		byte[] preferences = exportPreferences();

		run2(scenario, input, work.resolve("warm-up"), false, seed, preferences);
		Run recorded = run2(scenario, input, work.resolve("recorded"), true, seed, preferences);
		Run plain = run2(scenario, input, work.resolve("plain"), false, seed, preferences);
		if (!Arrays.equals(recorded.document, plain.document)) {
			throw new IllegalStateException("the save depends on what was asked between the steps: compare "
					+ recorded.file + " with " + plain.file);
		}

		Files.createDirectories(out);
		Path reference = out.resolve("rocket.ork");
		Files.write(reference, recorded.document);
		// The load in the environment of the start-up: the preferences as it left them (a run's "Save as
		// default" names the default configuration of every load after it).
		onEdt(() -> {
			restorePreferences(preferences);
			return null;
		});
		List<String> loaded = onEdt(() -> loadedState(reference));
		List<String> loadedArchive = onEdt(() -> loadedState(recorded.file.toPath()));
		if (!loaded.equals(loadedArchive)) {
			throw new IllegalStateException("the plain document and the archive load differently");
		}
		List<String> numbers = onEdt(() -> RocketState.numbers(new GeneralRocketLoader(reference.toFile()).load()));

		JsonObject state = new JsonObject();
		state.addProperty("schema", "gui-reference");
		state.addProperty("schemaVersion", 2);
		state.addProperty("generator", "tools/openrocket-gui-reference/generate.sh (GuiReference)");
		JsonObject openrocket = new JsonObject();
		openrocket.addProperty("commit", options.get("--commit"));
		openrocket.addProperty("presetDatabaseCommit", options.get("--preset-commit"));
		openrocket.addProperty("version", BuildProperties.getVersion());
		openrocket.addProperty("dirty", Boolean.parseBoolean(options.get("--dirty")));
		openrocket.addProperty("javaVersion", System.getProperty("java.version"));
		state.add("openrocket", openrocket);
		state.add("startup", startup);
		state.add("scenario", scenarioInfo(scenarioFile, scenario));
		JsonObject opened = new JsonObject();
		opened.addProperty("file", scenario.steps().get(0).file());
		opened.addProperty("sha256", sha256(Files.readAllBytes(input)));
		state.add("input", opened);
		JsonObject saved = new JsonObject();
		saved.addProperty("file", reference.getFileName().toString());
		saved.addProperty("savedAs", recorded.file.getName());
		saved.add("archiveEntries", strings(recorded.entries));
		JsonObject entrySha256 = new JsonObject();
		for (Map.Entry<String, byte[]> entry : entries(recorded.file.toPath()).entrySet()) {
			entrySha256.addProperty(entry.getKey(), sha256(entry.getValue()));
		}
		saved.add("archiveSha256", entrySha256);
		state.add("reference", saved);
		state.add("steps", recorded.records);
		JsonObject load = new JsonObject();
		load.add("lines", strings(loaded));
		load.add("numbers", strings(numbers));
		state.add("loaded", load);
		write(out, state, reference, recorded);
	}

	/**
	 * OpenRocket's start-up as SwingStartup.runInEDT (:195-239) does it, on the event dispatch thread,
	 * without its windows (version 2): the GuiModule and PluginModule injector, the look and feel of the
	 * preferences (GUIUtil.applyLAF(): FlatLaf Light, OpenRocket's default), the one-time question whether
	 * OpenRocket may look for updates on the internet answered "Decline" (requestUpdateCheckPermission:
	 * setUpdateCheckPermission(false), which also turns off the motor database's update check at start;
	 * QtRocket has no network features), the databases (GuiModule.startLoader(): the bundled motor database,
	 * copied by MotorDatabaseInitializer into the motor library directory of the run's own home directory, and
	 * the bundled component presets of datafiles/components, internal/ and the openrocket-database submodule's
	 * database/; the user's thrust curve and preset directories are empty, in that home directory too), waited
	 * for here so that no "Loading motors" dialog shows, then the default units and the default component
	 * materials. The exception handler fails the run instead of showing a dialog. Returns what was loaded.
	 */
	private static JsonObject bootstrapV2() {
		failingExceptionHandler();
		GuiModule guiModule = new GuiModule();
		Injector injector = Guice.createInjector(guiModule, new PluginModule());
		Application.setInjector(injector);
		GUIUtil.applyLAF();
		Application.getPreferences().setUpdateCheckPermission(false);
		guiModule.startLoader();
		((MotorDatabaseLoader) Reflection.get(GuiModule.class, guiModule, "motorLoader")).getDatabase();
		((ComponentPresetDatabaseLoader) Reflection.get(GuiModule.class, guiModule, "presetLoader")).getDatabase();
		((SwingPreferences) Application.getPreferences()).loadDefaultUnits();
		((SwingPreferences) Application.getPreferences()).loadDefaultComponentMaterials();
		Databases.fakeMethod();

		JsonObject startup = new JsonObject();
		List<ThrustCurveMotorSet> sets = injector.getInstance(ThrustCurveMotorSetDatabase.class).getMotorSets();
		int motors = 0;
		for (ThrustCurveMotorSet set : sets) {
			motors += set.getMotorCount();
		}
		startup.addProperty("motorSets", sets.size());
		startup.addProperty("motors", motors);
		startup.addProperty("presets", injector.getInstance(ComponentPresetDao.class).listAll().size());
		return startup;
	}

	/** OpenRocket's preferences node (SwingPreferences' "OpenRocket-debug") as the start-up left it. */
	private static byte[] exportPreferences() throws Exception {
		Preferences node = ((SwingPreferences) Application.getPreferences()).getPreferences();
		node.flush();
		ByteArrayOutputStream bytes = new ByteArrayOutputStream();
		node.exportSubtree(bytes);
		return bytes.toByteArray();
	}

	/**
	 * OpenRocket's preferences back as the start-up left them, before a run: a run stores some (the motor
	 * chooser's preferred thrust curves and filters, the default configuration name of "Save as default", the
	 * windows' sizes), which would change the next run. The node keeps its object (SwingPreferences holds it);
	 * its keys and children are replaced, and SwingPreferences' cache of the keys it knows is cleared.
	 */
	private static void restorePreferences(byte[] snapshot) throws Exception {
		SwingPreferences prefs = (SwingPreferences) Application.getPreferences();
		Preferences node = prefs.getPreferences();
		for (String child : node.childrenNames()) {
			node.node(child).removeNode();
		}
		node.clear();
		Preferences.importPreferences(new ByteArrayInputStream(snapshot));
		((Map<?, ?>) Reflection.get(SwingPreferences.class, prefs, "cachedNodeKeys")).clear();
		node.flush();
	}

	/**
	 * One run of a scenario of version 2, every step and record on the event dispatch thread: File &gt; Open of
	 * @p input (the session's window with the Motors &amp; Configuration tab), the steps, the checks of the
	 * recorded run (README.md, "The checks"), and the window closed.
	 */
	private static Run run2(Scenario scenario, Path input, Path directory, boolean record, String seed,
			byte[] preferences) throws Exception {
		Files.createDirectories(directory);
		onEdt(() -> {
			restorePreferences(preferences);
			return null;
		});
		DeterministicUuids.reseed(seed);
		Session session = onEdt(() -> Session.open(directory.toFile(), input.toFile()));
		try {
			JsonArray records = new JsonArray();
			JsonObject previous = null;
			if (record) {
				Scenario.Step first = scenario.steps().get(0);
				previous = onEdt(() -> record2(first, session));
				records.add(previous);
			}
			for (Scenario.Step step : scenario.steps().subList(1, scenario.steps().size())) {
				try {
					onEdt(() -> {
						session.run(step);
						return null;
					});
				} catch (Exception e) {
					throw new IllegalStateException("step " + step.index() + " (" + step.kind().name + ") failed", e);
				}
				// What the step queued (repaints, a listener's invokeLater) runs before the record.
				onEdt(() -> null);
				requireNoUncaughtException("in step " + step.index() + " (" + step.kind().name + ")");
				if (record) {
					JsonObject current = onEdt(() -> record2(step, session));
					check(step, previous, current, session);
					records.add(current);
					previous = current;
				}
			}
			return finish(session, records);
		} finally {
			onEdt(() -> {
				session.dispose();
				return null;
			});
		}
	}

	/**
	 * The checks of a step of the recorded run (README.md, "The checks"): a field step changes the tree (as in
	 * version 1); an operation of the tab changes it, but for one whose question was cancelled, which must
	 * leave it as it was (QtRocket's fix of bug 3: Cancel changes nothing), and every operation that changes it
	 * fires a change event of the rocket (QtRocket's operations always fire one; OpenRocket's panels fire
	 * only when their "update" says so).
	 */
	private static void check(Scenario.Step step, JsonObject previous, JsonObject current, Session session) {
		boolean changed = !current.get("treeSha256").equals(previous.get("treeSha256"));
		String name = "step " + step.index() + " (" + step.kind().name + ")";
		if (FIELD_STEPS.contains(step.kind()) && !changed) {
			throw new IllegalStateException(name + " left the component tree as it was: give the field a value it"
					+ " does not have");
		}
		if (!TAB_OPERATIONS.contains(step.kind())) {
			return;
		}
		boolean cancelled = step.answer() != null && "cancel".equals(Questions.string(step.answer(), "button"))
				&& step.kind() != Scenario.Kind.NEW_CONFIGURATION;
		if (cancelled && changed) {
			throw new IllegalStateException(name + ": OpenRocket's Cancel changed the design (bug 3: QtRocket's"
					+ " Cancel changes nothing)");
		}
		if (!cancelled && !changed) {
			throw new IllegalStateException(name + " left the component tree as it was: it checks nothing");
		}
		if (changed && session.tab().eventsOfStep() == 0) {
			throw new IllegalStateException(name + " changed the design without a change event (QtRocket's"
					+ " operations always fire one; OpenRocket's tables, its configurations' motors and its undo"
					+ " history miss the change)");
		}
	}

	/** What a step of version 2 left (README.md, "state.json"). */
	private static JsonObject record2(Scenario.Step step, Session session) throws Exception {
		JsonObject object = new JsonObject();
		object.addProperty("index", step.index());
		object.addProperty("step", step.kind().name);
		object.add("undo", action(session.undoAction()));
		object.add("redo", action(session.redoAction()));
		object.addProperty("saved", session.document().isSaved());
		object.add("selection", selection(session));
		object.addProperty("tab", session.windowTab());
		String window = session.openWindow();
		if (window != null) {
			JsonObject open = new JsonObject();
			open.addProperty("window", window);
			if (session.dialogComponent() != null) {
				open.add("component", ComponentPaths.json(session.dialogComponent()));
			}
			object.add("open", open);
		}
		List<JsonObject> asked = session.tab().askedQuestions();
		if (!asked.isEmpty()) {
			JsonArray questions = new JsonArray();
			asked.forEach(questions::add);
			object.add("questions", questions);
		}
		object.add("flightConfiguration", session.tab().record());
		List<String> tree = RocketState.dump(session.document(), Collections.emptySet());
		object.addProperty("treeSha256", sha256(String.join("\n", tree).concat("\n").getBytes(StandardCharsets.UTF_8)));
		if (CHECKPOINTS_V2.contains(step.kind())) {
			object.add("tree", strings(tree));
		}
		return object;
	}
}
