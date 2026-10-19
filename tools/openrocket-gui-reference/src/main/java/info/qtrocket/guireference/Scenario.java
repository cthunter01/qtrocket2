package info.qtrocket.guireference;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Set;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;

/**
 * The from-scratch scenario of {@code tests/data/goldens/gui/from-scratch/scenario.json}: what a user
 * does in the GUI, step by step, at the level of its actions (see README.md, "The scenario"). The
 * same file drives QtRocket's GUI in its exit test; this class only reads it and checks its form.
 */
final class Scenario {

	/** The schema name the file must declare. */
	static final String SCHEMA = "gui-from-scratch-scenario";
	/** The schema version this tool reads. */
	static final int SCHEMA_VERSION = 1;

	/** What a step does. */
	enum Kind {
		/** File &gt; New: a new design window with a rocket and its sustainer. */
		NEW("new"),
		/** Click on rows of the component tree (several with Ctrl): {@code paths}. */
		SELECT("select"),
		/** Click the add button of a component class ({@code component}, the Java class name). */
		ADD("add"),
		/** Edit &gt; Edit (or a double click on the selected row): opens the selection's dialog. */
		EDIT("edit"),
		/** Type {@code text} into the dialog field {@code field}, after choosing {@code unit} in its unit selector. */
		TYPE("type"),
		/** Choose {@code item} in the combo box {@code field}. */
		CHOOSE("choose"),
		/** Set the check box {@code field} to {@code checked}. */
		CHECK("check"),
		/** Type {@code text} into the cell of {@code row} and {@code column} of the fin point table. */
		EDIT_POINT("editPoint"),
		/** Select {@code row} of the fin point table and choose "Insert point" in its context menu. */
		INSERT_POINT("insertPoint"),
		/** Click the dialog button {@code button}. */
		BUTTON("button"),
		/** Click the OK button of the open dialog. */
		OK("ok"),
		/** Edit &gt; Undo. */
		UNDO("undo"),
		/** Edit &gt; Redo. */
		REDO("redo"),
		/** Edit &gt; Copy. */
		COPY("copy"),
		/** Edit &gt; Paste. */
		PASTE("paste"),
		/** File &gt; Save as: opens the dialog of the design information first. */
		SAVE_AS("saveAs"),
		/** The file chooser of Save as: the name {@code file} typed, Save clicked. */
		CHOOSE_FILE("chooseFile");

		final String name;

		Kind(String name) {
			this.name = name;
		}

		static Kind of(String name) {
			for (Kind kind : values()) {
				if (kind.name.equals(name)) {
					return kind;
				}
			}
			throw new IllegalArgumentException("unknown step '" + name + "'");
		}
	}

	/** One element of a component path: the name of a child, and which of the children of that name. */
	record PathElement(String name, int occurrence) {
	}

	/** One step of the scenario, with the members its kind uses (the others are null). */
	record Step(int index, Kind kind, String note, List<List<PathElement>> paths, String component,
			String field, String text, String unit, String item, Boolean checked, Integer row, String column,
			String button, String file) {
	}

	private final String title;
	private final List<Step> steps;

	private Scenario(String title, List<Step> steps) {
		this.title = title;
		this.steps = steps;
	}

	String title() {
		return title;
	}

	List<Step> steps() {
		return Collections.unmodifiableList(steps);
	}

	/** Reads and checks the scenario file. */
	static Scenario read(Path file) throws IOException {
		JsonObject root = JsonParser.parseString(Files.readString(file, StandardCharsets.UTF_8)).getAsJsonObject();
		if (!SCHEMA.equals(string(root, "schema")) || root.get("schemaVersion").getAsInt() != SCHEMA_VERSION) {
			throw new IllegalArgumentException(file + " is not a " + SCHEMA + " of version " + SCHEMA_VERSION);
		}
		List<Step> steps = new ArrayList<>();
		JsonArray array = root.getAsJsonArray("steps");
		for (int i = 0; i < array.size(); i++) {
			steps.add(step(i, array.get(i).getAsJsonObject()));
		}
		if (steps.isEmpty() || steps.get(0).kind() != Kind.NEW) {
			throw new IllegalArgumentException("the scenario must start with File > New");
		}
		return new Scenario(string(root, "title"), steps);
	}

	/** The members each kind of step takes (besides "step" and the optional "note"). */
	private static Set<String> membersOf(Kind kind) {
		return switch (kind) {
			case NEW, EDIT, OK, UNDO, REDO, COPY, PASTE, SAVE_AS -> Set.of();
			case SELECT -> Set.of("paths");
			case ADD -> Set.of("component");
			case TYPE -> Set.of("field", "text", "unit");
			case CHOOSE -> Set.of("field", "item");
			case CHECK -> Set.of("field", "checked");
			case EDIT_POINT -> Set.of("row", "column", "text", "unit");
			case INSERT_POINT -> Set.of("row");
			case BUTTON -> Set.of("button");
			case CHOOSE_FILE -> Set.of("file");
		};
	}

	private static Step step(int index, JsonObject object) {
		Kind kind = Kind.of(string(object, "step"));
		Set<String> allowed = membersOf(kind);
		for (String key : object.keySet()) {
			if (!key.equals("step") && !key.equals("note") && !allowed.contains(key)) {
				throw new IllegalArgumentException("step " + index + " (" + kind.name + ") has an unknown member '"
						+ key + "'");
			}
		}
		List<List<PathElement>> paths = null;
		if (object.has("paths")) {
			paths = new ArrayList<>();
			for (JsonElement path : object.getAsJsonArray("paths")) {
				paths.add(path(path.getAsJsonArray()));
			}
		}
		Step step = new Step(index, kind, string(object, "note"), paths, string(object, "component"),
				string(object, "field"), string(object, "text"), string(object, "unit"), string(object, "item"),
				object.has("checked") ? object.get("checked").getAsBoolean() : null,
				object.has("row") ? object.get("row").getAsInt() : null, string(object, "column"),
				string(object, "button"), string(object, "file"));
		for (String required : allowed) {
			if (!required.equals("unit") && !object.has(required)) {
				throw new IllegalArgumentException("step " + index + " (" + kind.name + ") needs '" + required + "'");
			}
		}
		return step;
	}

	/**
	 * A component path: the names from the child of the rocket (a stage) down to the component, an
	 * element being a name (the first child of that name) or {"name": ..., "occurrence": n} (the n-th
	 * child of that name, from 1). The empty path is the rocket.
	 */
	private static List<PathElement> path(JsonArray array) {
		List<PathElement> path = new ArrayList<>();
		for (JsonElement element : array) {
			if (element.isJsonPrimitive()) {
				path.add(new PathElement(element.getAsString(), 1));
			} else {
				JsonObject object = element.getAsJsonObject();
				path.add(new PathElement(string(object, "name"), object.get("occurrence").getAsInt()));
			}
		}
		return path;
	}

	private static String string(JsonObject object, String key) {
		JsonElement element = object.get(key);
		return element == null || element.isJsonNull() ? null : element.getAsString();
	}
}
