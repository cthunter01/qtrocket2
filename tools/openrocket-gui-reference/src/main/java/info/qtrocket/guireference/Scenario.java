package info.qtrocket.guireference;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collections;
import java.util.EnumSet;
import java.util.List;
import java.util.Set;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;

/**
 * A scenario of {@code tests/data/goldens/gui/<name>/scenario.json}: what a user does in the GUI, step
 * by step, at the level of its actions (see README.md, "The scenarios"). The same file drives QtRocket's
 * GUI in its exit tests; this class only reads it and checks its form.
 * <p>
 * Two schemas: version 1 ({@value #SCHEMA_V1}, the from-scratch scenario of Milestone 2: File &gt; New
 * and the design, headless) and version 2 ({@value #SCHEMA_V2}, Milestone 3: File &gt; Open of a design
 * file, the design steps of version 1 but for the add buttons, Copy and Paste, and the Motors &amp;
 * Configuration tab, whose panels run under a virtual display).
 */
final class Scenario {

	/** The schema name of a version 1 scenario. */
	static final String SCHEMA_V1 = "gui-from-scratch-scenario";
	/** The schema name of a version 2 scenario. */
	static final String SCHEMA_V2 = "gui-scenario";

	/** What a step does. */
	enum Kind {
		/** File &gt; New: a new design window with a rocket and its sustainer (version 1, first step). */
		NEW("new"),
		/** File &gt; Open of the design file {@code file} (version 2, first step). */
		OPEN("open"),
		/** Click on rows of the component tree (several with Ctrl): {@code paths}. */
		SELECT("select"),
		/** Click the add button of a component class ({@code component}, the Java class name; version 1). */
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
		/** Edit &gt; Copy (version 1). */
		COPY("copy"),
		/** Edit &gt; Paste (version 1). */
		PASTE("paste"),
		/** File &gt; Save as: opens the dialog of the design information first. */
		SAVE_AS("saveAs"),
		/** The file chooser of Save as: the name {@code file} typed, Save clicked. */
		CHOOSE_FILE("chooseFile"),
		/** The window's tab {@code tab}: "design" or "flightConfiguration" (version 2). */
		TAB("tab"),
		/** The sub-tab {@code tab} of the Motors &amp; Configuration tab: "motors", "recovery" or "stages". */
		CONFIG_TAB("configTab"),
		/** Clicks on cells of the current sub-tab's table: {@code cells}. */
		SELECT_CELLS("selectCells"),
		/** The check box of the motor mount {@code mount} in the motor mount table set to {@code checked}. */
		MOUNT_CHECK("mountCheck"),
		/** New Configuration; {@code answer} answers the question it asks next (the current sub-tab's). */
		NEW_CONFIGURATION("newConfiguration"),
		/** Rename Configuration, the rename dialog answered with {@code answer}. */
		RENAME_CONFIGURATION("renameConfiguration"),
		/** Remove Configuration. */
		DELETE_CONFIGURATION("deleteConfiguration"),
		/** Duplicate Configuration. */
		DUPLICATE_CONFIGURATION("duplicateConfiguration"),
		/** Select motor, the motor chooser answered with {@code answer}. */
		SELECT_MOTOR("selectMotor"),
		/** Delete motor. */
		DELETE_MOTOR("deleteMotor"),
		/** Select ignition, the ignition dialog answered with {@code answer}. */
		SELECT_IGNITION("selectIgnition"),
		/** Reset ignition. */
		RESET_IGNITION("resetIgnition"),
		/** Select deployment, the deployment dialog answered with {@code answer}. */
		SELECT_DEPLOYMENT("selectDeployment"),
		/** Reset deployment. */
		RESET_DEPLOYMENT("resetDeployment"),
		/** Select separation, the separation dialog answered with {@code answer}. */
		SELECT_SEPARATION("selectSeparation"),
		/** Reset separation. */
		RESET_SEPARATION("resetSeparation");

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

	/** The steps of version 1. */
	static final Set<Kind> V1_KINDS;

	static {
		EnumSet<Kind> kinds = EnumSet.range(Kind.NEW, Kind.CHOOSE_FILE);
		kinds.remove(Kind.OPEN);
		V1_KINDS = Collections.unmodifiableSet(kinds);
	}

	/**
	 * The steps of version 2: those of version 1 but File &gt; New, the add buttons (which open the real
	 * configuration dialog and the parts library under a display), Copy and Paste (OpenRocket's paste of a
	 * motor mount loses its default ignition: QtRocket's fix of bug 6, which the tool makes only for undo and
	 * redo), and the steps of the Motors &amp; Configuration tab.
	 */
	static final Set<Kind> V2_KINDS;

	static {
		EnumSet<Kind> kinds = EnumSet.allOf(Kind.class);
		kinds.removeAll(EnumSet.of(Kind.NEW, Kind.ADD, Kind.COPY, Kind.PASTE));
		V2_KINDS = Collections.unmodifiableSet(kinds);
	}

	/** The steps of the Motors &amp; Configuration tab (version 2). */
	static final Set<Kind> TAB_KINDS = Collections.unmodifiableSet(EnumSet.range(Kind.CONFIG_TAB, Kind.RESET_SEPARATION));

	/** One element of a component path: the name of a child, and which of the children of that name. */
	record PathElement(String name, int occurrence) {
	}

	/**
	 * A click on a cell of a table of the Motors &amp; Configuration tab: its row (the configuration's place
	 * among the rocket's), the component of its column (null for the configuration column, column 0) and the
	 * key held ("ctrl" toggles the cell, "shift" extends the selection to it, null for a plain click).
	 */
	record Cell(int row, List<PathElement> column, String with) {
	}

	/** One step of the scenario, with the members its kind uses (the others are null). */
	record Step(int index, Kind kind, String note, List<List<PathElement>> paths, String component,
			String field, String text, String unit, String item, Boolean checked, Integer row, String column,
			String button, String file, String tab, List<Cell> cells, List<PathElement> mount, JsonObject answer) {
	}

	private final int version;
	private final String title;
	private final List<Step> steps;

	private Scenario(int version, String title, List<Step> steps) {
		this.version = version;
		this.title = title;
		this.steps = steps;
	}

	/** The schema version: 1 or 2. */
	int version() {
		return version;
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
		String schema = string(root, "schema");
		int version = root.has("schemaVersion") ? root.get("schemaVersion").getAsInt() : 0;
		if (!(SCHEMA_V1.equals(schema) && version == 1) && !(SCHEMA_V2.equals(schema) && version == 2)) {
			throw new IllegalArgumentException(file + " is neither a " + SCHEMA_V1 + " of version 1 nor a "
					+ SCHEMA_V2 + " of version 2");
		}
		for (String key : root.keySet()) {
			if (!Set.of("schema", "schemaVersion", "title", "steps").contains(key)) {
				throw new IllegalArgumentException(file + " has an unknown member '" + key + "'");
			}
		}
		Set<Kind> allowed = version == 1 ? V1_KINDS : V2_KINDS;
		Kind first = version == 1 ? Kind.NEW : Kind.OPEN;
		List<Step> steps = new ArrayList<>();
		JsonArray array = root.getAsJsonArray("steps");
		for (int i = 0; i < array.size(); i++) {
			Step step = step(i, array.get(i).getAsJsonObject());
			if (!allowed.contains(step.kind())) {
				throw new IllegalArgumentException("step " + i + " (" + step.kind().name + ") is no step of version "
						+ version);
			}
			if ((i == 0) != (step.kind() == first)) {
				throw new IllegalArgumentException("step " + i + ": the scenario starts with " + first.name
						+ ", and only there");
			}
			steps.add(step);
		}
		if (steps.isEmpty()) {
			throw new IllegalArgumentException("the scenario has no step");
		}
		return new Scenario(version, string(root, "title"), steps);
	}

	/** The members each kind of step takes (besides "step" and the optional "note"). */
	private static Set<String> membersOf(Kind kind) {
		return switch (kind) {
			case NEW, EDIT, OK, UNDO, REDO, COPY, PASTE, SAVE_AS, DELETE_CONFIGURATION, DUPLICATE_CONFIGURATION,
					DELETE_MOTOR, RESET_IGNITION, RESET_DEPLOYMENT, RESET_SEPARATION -> Set.of();
			case OPEN -> Set.of("file");
			case SELECT -> Set.of("paths");
			case ADD -> Set.of("component");
			case TYPE -> Set.of("field", "text", "unit");
			case CHOOSE -> Set.of("field", "item");
			case CHECK -> Set.of("field", "checked");
			case EDIT_POINT -> Set.of("row", "column", "text", "unit");
			case INSERT_POINT -> Set.of("row");
			case BUTTON -> Set.of("button");
			case CHOOSE_FILE -> Set.of("file");
			case TAB, CONFIG_TAB -> Set.of("tab");
			case SELECT_CELLS -> Set.of("cells");
			case MOUNT_CHECK -> Set.of("mount", "checked");
			case NEW_CONFIGURATION, RENAME_CONFIGURATION, SELECT_MOTOR, SELECT_IGNITION, SELECT_DEPLOYMENT,
					SELECT_SEPARATION -> Set.of("answer");
		};
	}

	/** The members a step may leave out: a unit (a field without a unit selector), New's answer (no question). */
	private static boolean isOptional(Kind kind, String member) {
		return member.equals("unit") || (kind == Kind.NEW_CONFIGURATION && member.equals("answer"));
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
		for (String required : allowed) {
			if (!isOptional(kind, required) && !object.has(required)) {
				throw new IllegalArgumentException("step " + index + " (" + kind.name + ") needs '" + required + "'");
			}
		}
		List<List<PathElement>> paths = null;
		if (object.has("paths")) {
			paths = new ArrayList<>();
			for (JsonElement path : object.getAsJsonArray("paths")) {
				paths.add(path(path.getAsJsonArray()));
			}
		}
		List<Cell> cells = null;
		if (object.has("cells")) {
			cells = new ArrayList<>();
			for (JsonElement cell : object.getAsJsonArray("cells")) {
				cells.add(cell(index, cell.getAsJsonObject()));
			}
			if (cells.isEmpty()) {
				throw new IllegalArgumentException("step " + index + " (" + kind.name + ") clicks no cell");
			}
		}
		JsonObject answer = null;
		if (object.has("answer")) {
			answer = object.getAsJsonObject("answer");
			if (!answer.has("button")) {
				throw new IllegalArgumentException("step " + index + " (" + kind.name + "): the answer needs 'button'");
			}
		}
		String tab = string(object, "tab");
		if (tab != null) {
			Set<String> tabs = kind == Kind.TAB ? Set.of("design", "flightConfiguration")
					: Set.of("motors", "recovery", "stages");
			if (!tabs.contains(tab)) {
				throw new IllegalArgumentException("step " + index + " (" + kind.name + "): no tab '" + tab + "'");
			}
		}
		return new Step(index, kind, string(object, "note"), paths, string(object, "component"),
				string(object, "field"), string(object, "text"), string(object, "unit"), string(object, "item"),
				object.has("checked") ? object.get("checked").getAsBoolean() : null,
				object.has("row") ? object.get("row").getAsInt() : null, string(object, "column"),
				string(object, "button"), string(object, "file"), tab, cells,
				object.has("mount") ? path(object.getAsJsonArray("mount")) : null, answer);
	}

	/** A cell of selectCells: {"row": n, "column": path (absent for column 0), "with": "ctrl" | "shift" (absent)}. */
	private static Cell cell(int index, JsonObject object) {
		for (String key : object.keySet()) {
			if (!Set.of("row", "column", "with").contains(key)) {
				throw new IllegalArgumentException("step " + index + ": a cell has an unknown member '" + key + "'");
			}
		}
		if (!object.has("row")) {
			throw new IllegalArgumentException("step " + index + ": a cell needs 'row'");
		}
		String with = string(object, "with");
		if (with != null && !with.equals("ctrl") && !with.equals("shift")) {
			throw new IllegalArgumentException("step " + index + ": a cell is clicked with ctrl or shift, not " + with);
		}
		return new Cell(object.get("row").getAsInt(), object.has("column") ? path(object.getAsJsonArray("column")) : null,
				with);
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
