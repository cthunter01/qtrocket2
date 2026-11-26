package info.qtrocket.guireference;

import java.awt.Component;
import java.awt.Container;
import java.awt.Toolkit;
import java.awt.datatransfer.DataFlavor;
import java.awt.event.ActionEvent;
import java.nio.file.Path;
import java.util.List;

import javax.swing.Action;
import javax.swing.JCheckBoxMenuItem;
import javax.swing.JComponent;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JPopupMenu;
import javax.swing.JTable;
import javax.swing.ListSelectionModel;
import javax.swing.table.TableColumnModel;

import com.google.gson.JsonArray;
import com.google.gson.JsonNull;
import com.google.gson.JsonObject;

import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.Simulation;
import info.openrocket.core.logging.WarningSet;
import info.openrocket.core.rocketcomponent.ComponentChangeListener;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.swing.gui.adaptors.ColumnTableModel;
import info.openrocket.swing.gui.main.ClipboardListener;
import info.openrocket.swing.gui.main.DocumentSelectionListener;
import info.openrocket.swing.gui.main.DocumentSelectionModel;
import info.openrocket.swing.gui.main.OpenRocketClipboard;
import info.openrocket.swing.gui.main.SimulationPanel;
import info.openrocket.swing.gui.util.Icons;

/**
 * OpenRocket's real Flight simulations tab (SimulationPanel with its SimulationTableModel, ColumnTable and column
 * chooser, and the dialogs its actions open) for the design window of a version 3 scenario, in a frame on the
 * virtual display the tool runs on (xvfb-run; nothing is shown on a desktop), and the steps of the tab done on it
 * by OpenRocket's own actions and table: the technique of the simulation-table pins
 * (tools/openrocket-gui-pins/simulation-table, SimulationTableProbe) and of FlightConfigurationTab.
 * <p>
 * The panel is made before the window's DocumentSelectionModel, as BasicFrame's constructor makes it
 * (BasicFrame.java:231-242), and its table's selection model is the one the DocumentSelectionModel is attached to.
 * BasicFrame's RocketActions (the Edit menu) cannot be made (its actions select the window's tab through the
 * BasicFrame, which the tool allocates without its window), so the steps use the panel's own actions, which
 * RocketActions calls for a selection of simulations (RocketActions.java:597-999, the actions' simulation
 * branches), and the tab registers the listeners RocketActions registers to update them (RocketActions.java:144-172:
 * the clipboard, the document selection, the table selection and the rocket).
 * <p>
 * Every method runs on the event dispatch thread. The modal dialogs are answered by {@link SimulationQuestions}.
 */
final class SimulationsTab {

	/** The panel's actions, by their fields (SimulationTableProbe.ACTIONS); the record names them without "Action". */
	static final String[] ACTIONS = { "newSimulationAction", "editSimulationAction", "cutSimulationAction",
			"copySimulationAction", "pasteSimulationAction", "runSimulationAction", "landingDispersionAction",
			"plotSimulationAction", "duplicateSimulationAction", "deleteSimulationAction", "simTableExportAction",
			"selectedSimsExportAction" };

	/** The ids of the model's columns (SimulationPanel.SIMULATION_TABLE_COLUMN_IDS). */
	static final List<String> COLUMN_IDS = List.of("status", "warnings", "name", "configuration", "simulationStepper",
			"launchRodVelocity", "apogee", "deploymentVelocity", "optimumCoastTime", "maxVelocity", "maxAcceleration",
			"timeToApogee", "flightTime", "groundHitVelocity");

	/** The document preference of the hidden columns (SimulationPanel.PREF_KEY_SIMULATION_TABLE_HIDDEN_COLUMNS). */
	static final String HIDDEN_COLUMNS = "simulation.table.hiddenColumns";

	private final OpenRocketDocument document;
	private final Rocket rocket;
	/** The repository: the files the answers name are relative to it. */
	private final Path repository;
	private final SimulationPanel panel;
	private final JTable table;
	private final ColumnTableModel model;
	private final JFrame window;
	private final SimulationQuestions questions;
	private final ClipboardListener clipboardListener;
	private final ComponentChangeListener rocketListener;
	/** The system clipboard's text after the last step, when it was a Copy or a Cut (null otherwise). */
	private String clipboardText;

	/**
	 * The tab of the window of @p document (BasicFrame.java:231: the SimulationPanel is the first panel made); the
	 * files the answers name are relative to @p repository.
	 */
	SimulationsTab(OpenRocketDocument document, Path repository) {
		this.document = document;
		this.repository = repository;
		this.rocket = document.getRocket();
		// SimulationPanel(parent, doc): the parent is only the owner of the warnings column's double click dialog,
		// which no step uses; the other dialogs are owned by the window the panel is in.
		panel = new SimulationPanel(null, document);
		table = (JTable) Reflection.get(SimulationPanel.class, panel, "simulationTable");
		model = (ColumnTableModel) Reflection.get(SimulationPanel.class, panel, "simulationTableModel");
		questions = new SimulationQuestions(document, repository);
		window = new JFrame("QtRocket GUI reference: simulations");
		window.add(panel);
		window.setSize(1100, 600);
		window.setVisible(true);
		clipboardListener = panel::updateActions;
		rocketListener = e -> panel.updateActions();
	}

	/** The selection model of the simulations table, which the window's DocumentSelectionModel attaches. */
	ListSelectionModel listSelectionModel() {
		return panel.getSimulationListSelectionModel();
	}

	/**
	 * The listeners of RocketActions' constructor (RocketActions.java:144-172) that update the panel's actions: a
	 * change of the clipboard, of the document's selection, of the table's selection and of the rocket.
	 */
	void connect(DocumentSelectionModel selection) {
		OpenRocketClipboard.addClipboardListener(clipboardListener);
		panel.updateActions();
		DocumentSelectionListener listener = type -> panel.updateActions();
		selection.addDocumentSelectionListener(listener);
		panel.addSimulationTableListSelectionListener(e -> panel.updateActions());
		rocket.addComponentChangeListener(rocketListener);
	}

	/** Closes the window and unregisters the clipboard's listener (OpenRocketClipboard's listeners are static). */
	void dispose() {
		OpenRocketClipboard.removeClipboardListener(clipboardListener);
		rocket.removeComponentChangeListener(rocketListener);
		window.dispose();
	}

	/** The questions the last step asked (SimulationQuestions: each as it opened, with what it was answered with). */
	List<JsonObject> askedQuestions() {
		return questions.asked();
	}

	/** Where the last step's record shows one of QtRocket's fixes the tool made in OpenRocket's run. */
	List<JsonObject> toolFixes() {
		return questions.toolFixes();
	}

	/** Adds a fix of the last step the tool made outside the tab's dialogs (bug 6 after a Cancel's undo). */
	void addToolFix(int bug, String text) {
		questions.addToolFix(bug, text);
	}

	/** The system clipboard's text after a Copy or a Cut, null after another step. */
	String clipboardText() {
		return clipboardText;
	}

	/** Before a step. */
	void beginStep() {
		questions.clear();
		clipboardText = null;
	}

	/**
	 * The window's tab becomes this one: BasicFrame.stateChanged for SIMULATION_TAB (BasicFrame.java:2532-2535):
	 * the focus to the table, the previous selection back or the first row, and the outdated simulations run when
	 * the preference AutoRunSimulations asks for it (off by default; a run would fail the scenario).
	 */
	void takeTheSpotlight() {
		if (info.openrocket.core.startup.Application.getPreferences().getAutoRunSimulations()) {
			throw new IllegalStateException("AutoRunSimulations would run the outdated simulations: no run before"
					+ " Save as");
		}
		panel.takeTheSpotlight();
		panel.activating();
	}

	/** The window's tab leaves this one (BasicFrame.stateChanged, BasicFrame.java:2521-2523). */
	void leave() {
		panel.updatePreviousSelection();
	}

	// ------------------------------------------------------------------ the steps

	void run(Scenario.Step step) {
		switch (step.kind()) {
			case SELECT_SIMULATIONS -> selectRows(step.rows());
			case NEW_SIMULATION -> questions.settingsOperation(() -> perform("newSimulationAction"), step.answer(),
					true);
			case EDIT_SIMULATION -> questions.settingsOperation(() -> perform("editSimulationAction"), step.answer(),
					false);
			case DUPLICATE_SIMULATIONS -> questions.plainOperation(() -> perform("duplicateSimulationAction"));
			case DELETE_SIMULATIONS -> questions.deleteOperation(() -> perform("deleteSimulationAction"), step.answer());
			case COPY_SIMULATIONS -> {
				questions.plainOperation(() -> perform("copySimulationAction"));
				clipboardText = systemClipboardText();
			}
			case CUT_SIMULATIONS -> {
				questions.deleteOperation(() -> perform("cutSimulationAction"), step.answer());
				clipboardText = systemClipboardText();
			}
			case PASTE_SIMULATIONS -> questions.plainOperation(() -> perform("pasteSimulationAction"));
			case COLUMN_VISIBILITY -> columnVisibility(step.column(), step.visible());
			default -> throw new IllegalStateException("not a step of the simulations tab: " + step.kind().name);
		}
	}

	/** Clears the clipboard of simulations a run before this one left (OpenRocketClipboard is static). */
	static void clearClipboard() {
		OpenRocketClipboard.setClipboard(new Simulation[0]);
	}

	/** The panel's action @p field, which must be enabled, performed (as its button, menu item or key does). */
	private void perform(String field) {
		Action action = (Action) Reflection.get(SimulationPanel.class, panel, field);
		if (!action.isEnabled()) {
			throw new IllegalStateException("the action " + action.getValue(Action.NAME) + " is disabled");
		}
		action.actionPerformed(new ActionEvent(table, ActionEvent.ACTION_PERFORMED, field));
	}

	/**
	 * Clicks on rows of the table (BasicTableUI's mouse handler: JTable.changeSelection, Ctrl toggles, Shift extends),
	 * in the name column, whose single click does nothing more (a click on the status column runs an outdated
	 * simulation, SimulationPanel.java:327-338). No row: a click below the last row, which clears the selection
	 * (SimulationPanel.java:321-325).
	 */
	private void selectRows(List<Scenario.SimulationRow> rows) {
		if (rows.isEmpty()) {
			if (table.getSelectedRowCount() == 0) {
				throw new IllegalStateException("no row of the simulations table is selected: the click checks nothing");
			}
			table.clearSelection();
			return;
		}
		int column = clickColumn();
		for (Scenario.SimulationRow row : rows) {
			if (row.row() < 0 || row.row() >= table.getRowCount()) {
				throw new IllegalArgumentException("the simulations table has no row " + row.row());
			}
			table.changeSelection(row.row(), column, "ctrl".equals(row.with()), "shift".equals(row.with()));
		}
	}

	/** The view column of the name, or the first visible column but the status. */
	private int clickColumn() {
		TableColumnModel columns = table.getColumnModel();
		int fallback = -1;
		for (int c = 0; c < columns.getColumnCount(); c++) {
			Object id = columns.getColumn(c).getIdentifier();
			if ("name".equals(id)) {
				return c;
			}
			if (fallback < 0 && !"status".equals(id)) {
				fallback = c;
			}
		}
		if (fallback < 0) {
			throw new IllegalStateException("the table shows only its status column, whose click runs a simulation");
		}
		return fallback;
	}

	/**
	 * The column chooser of the table's header (its popup menu, ColumnVisibilityController.createMenu(),
	 * SimulationPanel.java:1874-1896): the check box item of @p column clicked, which must change it (an item of
	 * the last visible column is disabled).
	 */
	private void columnVisibility(String column, boolean visible) {
		if (!COLUMN_IDS.contains(column)) {
			throw new IllegalArgumentException("the simulations table has no column " + column + " (" + COLUMN_IDS + ")");
		}
		Object controller = Reflection.get(SimulationPanel.class, panel, "columnVisibilityController");
		JPopupMenu menu = (JPopupMenu) Reflection.invoke(controller, "createMenu");
		for (Component c : menu.getComponents()) {
			if (c instanceof JCheckBoxMenuItem item && column.equals(item.getClientProperty("columnId"))) {
				if (item.isSelected() == visible) {
					throw new IllegalStateException("the column " + column + " is " + (visible ? "shown" : "hidden")
							+ " already");
				}
				if (!item.isEnabled()) {
					throw new IllegalStateException("the column chooser's item of " + column + " is disabled");
				}
				item.doClick(0);
				return;
			}
		}
		throw new IllegalStateException("the column chooser has no item of " + column);
	}

	/** The text of the system clipboard (a Copy of simulations puts the table's text there). */
	private static String systemClipboardText() {
		try {
			return (String) Toolkit.getDefaultToolkit().getSystemClipboard().getData(DataFlavor.stringFlavor);
		} catch (Exception e) {
			throw new IllegalStateException("cannot read the system clipboard", e);
		}
	}

	// ------------------------------------------------------------------ the record

	/** What the tab shows (README.md, "state.json", the member "simulationsTab"). */
	JsonObject record() {
		JsonObject object = new JsonObject();
		object.addProperty("showsHelp", showsHelp());
		JsonArray columns = new JsonArray();
		TableColumnModel columnModel = table.getColumnModel();
		for (int c = 0; c < columnModel.getColumnCount(); c++) {
			JsonObject column = new JsonObject();
			column.addProperty("id", String.valueOf(columnModel.getColumn(c).getIdentifier()));
			Object header = columnModel.getColumn(c).getHeaderValue();
			column.addProperty("header", header == null ? "" : header.toString());
			columns.add(column);
		}
		object.add("columns", columns);
		JsonArray rows = new JsonArray();
		for (int view = 0; view < table.getRowCount(); view++) {
			rows.add(row(table.convertRowIndexToModel(view)));
		}
		object.add("rows", rows);
		JsonArray selected = new JsonArray();
		for (int row : table.getSelectedRows()) {
			selected.add(row);
		}
		object.add("selectedRows", selected);
		JsonObject actions = new JsonObject();
		for (String field : ACTIONS) {
			Action action = (Action) Reflection.get(SimulationPanel.class, panel, field);
			actions.addProperty(field.replace("Action", ""), action.isEnabled());
		}
		object.add("actions", actions);
		JsonArray clipboard = new JsonArray();
		Simulation[] onClipboard = OpenRocketClipboard.getClipboardSimulations();
		if (onClipboard != null) {
			for (Simulation s : onClipboard) {
				clipboard.add(s.getName());
			}
		}
		object.add("clipboardSimulations", clipboard);
		String hidden = document.getDocumentPreferences().getString(HIDDEN_COLUMNS, null);
		object.add("hiddenColumnsPreference", hidden == null ? JsonNull.INSTANCE
				: new com.google.gson.JsonPrimitive(hidden));
		return object;
	}

	/** Whether the panel's card shows the help text (no configuration with motors) instead of the table. */
	private boolean showsHelp() {
		JPanel cards = (JPanel) Reflection.get(SimulationPanel.class, panel, "cardPanel");
		for (Component c : cards.getComponents()) {
			if (c instanceof JLabel label) {
				return label.isVisible();
			}
		}
		throw new IllegalStateException("the simulations panel has no help card");
	}

	/**
	 * A row of the table as the simulation-table family records a simulation (tools/openrocket-gui-pins,
	 * SimulationTableProbe's R, W and C lines): the status, the text the status label gives its sorter (its
	 * toString()), the status description, the warnings box as text (counts, the icons as [HIGH], [NORMAL] and
	 * [LOW]) with the three counts (-1 without data), and the toString() of every model column from the name on
	 * (null for a missing value).
	 */
	private JsonObject row(int modelRow) {
		Simulation sim = document.getSimulation(modelRow);
		JsonObject row = new JsonObject();
		Object status = model.getValueAt(modelRow, 0);
		row.addProperty("status", sim.getStatus().name());
		row.addProperty("statusSortText", status.toString());
		row.addProperty("description", sim.getStatusDescription());
		Container box = (Container) model.getValueAt(modelRow, 1);
		StringBuilder text = new StringBuilder();
		for (Component child : box.getComponents()) {
			JLabel label = (JLabel) child;
			if (label.getText() != null && !label.getText().isEmpty()) {
				text.append(label.getText());
			}
			if (label.getIcon() != null) {
				text.append('[').append(warningIcon(label.getIcon())).append(']');
			}
		}
		row.addProperty("warnings", text.toString());
		WarningSet warnings = sim.getSimulatedWarnings();
		JsonArray counts = new JsonArray();
		counts.add(warnings == null ? -1 : warnings.getNrOfCriticalWarnings());
		counts.add(warnings == null ? -1 : warnings.getNrOfNormalWarnings());
		counts.add(warnings == null ? -1 : warnings.getNrOfInformationalWarnings());
		row.add("warningCounts", counts);
		row.addProperty("warningsToolTip", ((JComponent) box).getToolTipText());
		JsonArray cells = new JsonArray();
		for (int c = 2; c < model.getColumnCount(); c++) {
			Object v = model.getValueAt(modelRow, c);
			if (v == null) {
				cells.add(JsonNull.INSTANCE);
			} else {
				cells.add(v.toString());
			}
		}
		row.add("cells", cells);
		return row;
	}

	private static String warningIcon(javax.swing.Icon icon) {
		if (icon == Icons.WARNING_HIGH) {
			return "HIGH";
		}
		if (icon == Icons.WARNING_NORMAL) {
			return "NORMAL";
		}
		if (icon == Icons.WARNING_LOW) {
			return "LOW";
		}
		throw new IllegalStateException("an unknown icon in the warnings box: " + icon);
	}

	/**
	 * The simulations of the document (the record's "simulations"): each one's name, its configuration as a row
	 * (-1 for one the rocket does not have), its status, the ten lines of its options
	 * (SimulationLines.optionLines(); a lookup table's CSV file inside the repository by its path relative to it)
	 * and the names of its extensions.
	 */
	JsonArray simulations() {
		JsonArray array = new JsonArray();
		String prefix = repository.toAbsolutePath().normalize().toString() + java.io.File.separator;
		for (Simulation s : document.getSimulations()) {
			JsonObject sim = new JsonObject();
			sim.addProperty("name", s.getName());
			sim.addProperty("configuration", rocket.getIds().indexOf(s.getFlightConfigurationId()));
			sim.addProperty("status", s.getStatus().name());
			JsonArray options = new JsonArray();
			for (String line : SimulationLines.optionLines(s.getOptions())) {
				options.add(line.replace("path:" + prefix, "path:"));
			}
			sim.add("options", options);
			JsonArray extensions = new JsonArray();
			s.getSimulationExtensions().forEach(e -> extensions.add(e.getName()));
			sim.add("extensions", extensions);
			array.add(sim);
		}
		return array;
	}
}
