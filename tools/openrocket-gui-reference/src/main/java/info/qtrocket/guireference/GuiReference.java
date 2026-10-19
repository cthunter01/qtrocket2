package info.qtrocket.guireference;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

import javax.swing.Action;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonArray;
import com.google.gson.JsonObject;
import com.google.inject.Guice;
import com.google.inject.Injector;

import info.openrocket.core.database.Databases;
import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.file.GeneralRocketLoader;
import info.openrocket.core.logging.Warning;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.startup.Application;
import info.openrocket.core.startup.ExceptionHandler;
import info.openrocket.core.util.BuildProperties;
import info.openrocket.swing.gui.util.SwingPreferences;
import info.openrocket.swing.startup.GuiModule;

/**
 * The GUI reference tool: runs the from-scratch scenario on OpenRocket's Swing GUI code, headless,
 * saves the design with OpenRocket's saver and writes QtRocket's reference of it,
 * {@code tests/data/goldens/gui/from-scratch/}: {@code rocket.ork} (the document OpenRocket saved) and
 * {@code state.json} (what each step left, and the saved file as OpenRocket loads it). See README.md.
 *
 * <pre>
 *   GuiReference --scenario &lt;scenario.json&gt; --out &lt;directory&gt; --work &lt;directory&gt;
 *                --commit &lt;OpenRocket commit&gt; --dirty &lt;true|false&gt;
 * </pre>
 */
public final class GuiReference {

	/** The seed of the component ids of the scenario (DeterministicUuids). */
	private static final String UUID_SEED = "gui/from-scratch";

	/** The steps after which a record holds the whole tree, not only its digest. */
	private static final Set<Scenario.Kind> CHECKPOINTS = Set.of(Scenario.Kind.NEW, Scenario.Kind.OK,
			Scenario.Kind.BUTTON, Scenario.Kind.UNDO, Scenario.Kind.REDO, Scenario.Kind.PASTE,
			Scenario.Kind.CHOOSE_FILE);

	private GuiReference() {
	}

	public static void main(String[] args) throws Exception {
		Map<String, String> options = options(args);
		Path scenarioFile = Paths.get(options.get("--scenario"));
		Path out = Paths.get(options.get("--out"));
		Path work = Paths.get(options.get("--work"));

		// Before anything creates a UUID.
		DeterministicUuids.install();
		bootstrap();

		Scenario scenario = Scenario.read(scenarioFile);
		// A first run initializes every class the scenario reaches; some static initializers draw
		// UUIDs, which would shift the ids of the run that comes first.
		run(scenario, work.resolve("warm-up"), false);
		// Two runs of the scenario in two directories: the first records every step, the second asks
		// nothing between the steps. The getters the records call store what automatic dimensions
		// compute, as a GUI's figure and tree do; the two saves are the same file only when that order
		// does not matter for this scenario, which is what makes the reference one a GUI can match.
		Run recorded = run(scenario, work.resolve("recorded"), true);
		Run plain = run(scenario, work.resolve("plain"), false);
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
		JsonObject scenarioInfo = new JsonObject();
		scenarioInfo.addProperty("file", scenarioFile.getFileName().toString());
		scenarioInfo.addProperty("sha256", sha256(Files.readAllBytes(scenarioFile)));
		scenarioInfo.addProperty("title", scenario.title());
		scenarioInfo.addProperty("steps", scenario.steps().size());
		state.add("scenario", scenarioInfo);
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

		Gson gson = new GsonBuilder().setPrettyPrinting().disableHtmlEscaping().create();
		Files.writeString(out.resolve("state.json"), gson.toJson(state) + "\n", StandardCharsets.UTF_8);
		System.err.println("Wrote " + reference + " (" + recorded.document.length + " bytes) and "
				+ out.resolve("state.json"));
		System.exit(0);
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

	/**
	 * OpenRocket's start-up as SwingStartup.runInEDT (:195-239) does it, without its windows: the
	 * GuiModule and PluginModule injector, the databases (the component presets and the motors
	 * bypassed, so both are empty, as QtRocket's start-up has them in Milestone 2), the default units
	 * and the default component materials. The exception handler fails the run instead of showing a
	 * dialog; the look and feel is not set (nothing is drawn).
	 */
	private static void bootstrap() {
		System.setProperty("openrocket.bypass.presets", "true");
		System.setProperty("openrocket.bypass.motors", "true");
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

	private static Run run(Scenario scenario, Path directory, boolean record) throws Exception {
		Files.createDirectories(directory);
		DeterministicUuids.reseed(UUID_SEED);
		JsonArray records = new JsonArray();
		Session session = new Session(directory.toFile());
		if (record) {
			records.add(record(scenario.steps().get(0), session));
		}
		for (Scenario.Step step : scenario.steps().subList(1, scenario.steps().size())) {
			try {
				session.run(step);
			} catch (RuntimeException e) {
				throw new IllegalStateException("step " + step.index() + " (" + step.kind().name + ") failed", e);
			}
			if (record) {
				records.add(record(step, session));
			}
		}
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
		// The selection model can hold the components an undo or a redo replaced: ComponentTreeModel
		// restores the selection by id and keeps it when the ids are the same (restoreSelection,
		// isCurrentSelection). The record names the components of the tree that have those ids.
		JsonArray selection = new JsonArray();
		for (RocketComponent c : session.selection()) {
			RocketComponent live = session.document().getRocket().findComponent(c.getID());
			if (live == RocketComponent.REMOVED) {
				throw new IllegalStateException("a selected component is not in the tree: " + c);
			}
			selection.add(ComponentPaths.json(live));
		}
		object.add("selection", selection);
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
}
