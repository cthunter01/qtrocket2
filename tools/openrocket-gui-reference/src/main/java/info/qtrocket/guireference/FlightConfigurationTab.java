package info.qtrocket.guireference;

import java.awt.Component;
import java.awt.Container;
import java.awt.Font;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

import javax.swing.JButton;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JTabbedPane;
import javax.swing.JTable;
import javax.swing.table.TableModel;

import com.google.gson.JsonArray;
import com.google.gson.JsonObject;

import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.Simulation;
import info.openrocket.core.motor.MotorConfiguration;
import info.openrocket.core.rocketcomponent.AxialStage;
import info.openrocket.core.rocketcomponent.ComponentChangeListener;
import info.openrocket.core.rocketcomponent.FlightConfigurationId;
import info.openrocket.core.rocketcomponent.MotorMount;
import info.openrocket.core.rocketcomponent.RecoveryDevice;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.util.Pair;
import info.openrocket.swing.gui.dialogs.motor.MotorChooserDialog;
import info.openrocket.swing.gui.main.BasicFrame;
import info.openrocket.swing.gui.main.DocumentSelectionModel;
import info.openrocket.swing.gui.main.FlightConfigurationPanel;
import info.openrocket.swing.gui.main.flightconfigpanel.FlightConfigurablePanel;
import info.openrocket.swing.gui.main.flightconfigpanel.MotorConfigurationPanel;

/**
 * OpenRocket's real Motors &amp; Configuration tab (FlightConfigurationPanel with its three sub-panels and
 * its dialogs) for the design window of a version 2 scenario, in a frame on the virtual display the tool
 * runs on (xvfb-run; nothing is shown on a desktop), and the steps of the tab done on it by OpenRocket's
 * own buttons, tables and dialogs: the technique of the config-tab scout's TabProbe and of the
 * flight-config pins (tools/openrocket-gui-pins, FlightConfigurationOperationsProbe).
 * <p>
 * The panel needs its BasicFrame: New and Duplicate look the document up in BasicFrame's list of frames
 * (BasicFrame.findDocument), and a column of a table selects its components in the design through
 * BasicFrame.setSelectedComponents. A BasicFrame cannot be made without opening OpenRocket's whole window
 * (its menus, the rocket figure, the 3D view), so the tool allocates one without running its constructor,
 * sets the three fields the panel uses (the document, the rocket and the window's DocumentSelectionModel,
 * the one of the design tab's component tree) and registers it in BasicFrame's list of frames.
 * <p>
 * Every method runs on the event dispatch thread. The modal questions are answered by {@link Questions}.
 * Where QtRocket fixes one of OpenRocket's bugs of plan 10.3 the tool either makes the fix in OpenRocket's
 * run (bug 5: the undo positions of New, Duplicate and the motor mount check box, which OpenRocket does not
 * add; bug 6: {@link DefaultIgnitions}) or fails on a step where the bug would show, so that the scenario
 * avoids it (bug 4, and the motor chooser's bugs in {@link Questions}); README.md lists them.
 */
final class FlightConfigurationTab {

	/** The sub-tabs, by index (FlightConfigurationPanel.MOTOR_TAB_INDEX, RECOVERY_TAB_INDEX, SEPARATION_TAB_INDEX). */
	static final String[] SUB_TABS = { "motors", "recovery", "stages" };
	private static final int MOTORS = 0;
	private static final int RECOVERY = 1;
	private static final int STAGES = 2;

	/** The undo positions of QtRocket's fix of bug 5, which OpenRocket's operations do not add. */
	private static final String NEW_CONFIGURATION = "New Configuration";
	private static final String DUPLICATE_CONFIGURATION = "Duplicate Configuration";
	private static final String MODIFY_MOTOR_MOUNT = "Modify motor mount";

	/**
	 * The BasicFrame of the window, allocated without its constructor (BasicFrame's constructor builds
	 * OpenRocket's whole window). Never shown, never asked anything but the fields the panel reads;
	 * toString() is the only method of it that runs (BasicFrame.findFrame logs the frame it found).
	 */
	public static final class WindowFrame extends BasicFrame {
		private static final long serialVersionUID = 1L;

		private WindowFrame() {
			super(null);
		}

		@Override
		public String toString() {
			return "WindowFrame";
		}
	}

	private final OpenRocketDocument document;
	private final Rocket rocket;
	private final WindowFrame frame;
	private final FlightConfigurationPanel panel;
	private final JFrame window;
	private final Questions questions = new Questions();
	private final ComponentChangeListener eventCounter;
	/** The change events of the rocket during the current step. */
	private int events = 0;

	/**
	 * The tab of the window of @p document, whose design selection is @p selectionModel (BasicFrame's
	 * constructor: the FlightConfigurationPanel is made after the DesignPanel, BasicFrame.java:250-251).
	 */
	FlightConfigurationTab(OpenRocketDocument document, DocumentSelectionModel selectionModel) {
		this.document = document;
		this.rocket = document.getRocket();
		frame = allocateFrame();
		Reflection.set(BasicFrame.class, frame, "document", document);
		Reflection.set(BasicFrame.class, frame, "rocket", rocket);
		Reflection.set(BasicFrame.class, frame, "selectionModel", selectionModel);
		frames().clear();
		frames().add(frame);
		panel = new FlightConfigurationPanel(frame, document);
		// The panel registers itself first (FlightConfigurationPanel.java:76), its sub-panels' table models after
		// it; QtRocket's panel connects after its models: registered again, at the end (Session.orderListeners).
		rocket.removeChangeListener(panel);
		rocket.addChangeListener(panel);
		window = new JFrame("QtRocket GUI reference");
		window.add(panel);
		window.setSize(1100, 600);
		window.setVisible(true);
		eventCounter = e -> events++;
		rocket.addComponentChangeListener(eventCounter);
	}

	/** Closes the window and its motor chooser and unregisters the frame. */
	void dispose() {
		rocket.removeComponentChangeListener(eventCounter);
		((MotorChooserDialog) Reflection.get(MotorConfigurationPanel.class, motorPanel(), "motorChooserDialog"))
				.dispose();
		window.dispose();
		frames().remove(frame);
	}

	@SuppressWarnings("unchecked")
	private static List<BasicFrame> frames() {
		return (List<BasicFrame>) Reflection.get(BasicFrame.class, null, "frames");
	}

	/** A WindowFrame allocated by sun.misc.Unsafe.allocateInstance (looked up at run time: an internal API). */
	private static WindowFrame allocateFrame() {
		try {
			Class<?> unsafeClass = Class.forName("sun.misc.Unsafe");
			Field theUnsafe = unsafeClass.getDeclaredField("theUnsafe");
			theUnsafe.setAccessible(true);
			Method allocate = unsafeClass.getMethod("allocateInstance", Class.class);
			return (WindowFrame) allocate.invoke(theUnsafe.get(null), WindowFrame.class);
		} catch (ReflectiveOperationException e) {
			throw new IllegalStateException("cannot allocate the BasicFrame", e);
		}
	}

	/** The questions the last step asked (Questions: each as it opened, with what it was answered with). */
	List<JsonObject> askedQuestions() {
		return questions.asked();
	}

	/** The rocket's change events of the last step. */
	int eventsOfStep() {
		return events;
	}

	/** Before a step. */
	void beginStep() {
		events = 0;
		questions.clear();
	}

	/**
	 * The window's tab becomes this one (BasicFrame.stateChanged for FLIGHT_CONFIGURATION_TAB,
	 * BasicFrame.java:2529-2531).
	 */
	void takeTheSpotlight() {
		panel.takeTheSpotlight();
	}

	// ------------------------------------------------------------------ the steps

	void run(Scenario.Step step) {
		switch (step.kind()) {
			case CONFIG_TAB -> configTab(step.tab());
			case SELECT_CELLS -> selectCells(step.cells());
			case MOUNT_CHECK -> mountCheck(step.mount(), step.checked());
			case NEW_CONFIGURATION -> newConfiguration(step.answer());
			case RENAME_CONFIGURATION -> click(configurationButton("renameConfButton"), step.answer(), null);
			case DELETE_CONFIGURATION -> click(configurationButton("removeConfButton"), null, null);
			case DUPLICATE_CONFIGURATION -> {
				JButton button = configurationButton("duplicateConfButton");
				requireEnabled(button);
				// Bug 5: QtRocket's own undo position.
				document.addUndoPosition(DUPLICATE_CONFIGURATION);
				click(button, null, null);
			}
			case SELECT_MOTOR -> click(subPanelButton(MOTORS, "selectMotorButton"), step.answer(), firstCell());
			case DELETE_MOTOR -> click(subPanelButton(MOTORS, "deleteMotorButton"), null, null);
			case SELECT_IGNITION -> {
				JButton button = subPanelButton(MOTORS, "selectIgnitionButton");
				requireEnabled(button);
				if (step.answer() != null && "this".equals(Questions.string(step.answer(), "scope"))) {
					requireOwnMotorConfigurations("an ignition for this configuration only");
				}
				click(button, step.answer(), null);
			}
			case RESET_IGNITION -> {
				JButton button = subPanelButton(MOTORS, "resetIgnitionButton");
				requireEnabled(button);
				requireOwnMotorConfigurations("Reset ignition");
				click(button, null, null);
			}
			case SELECT_DEPLOYMENT -> click(subPanelButton(RECOVERY, "selectDeploymentButton"), step.answer(), null);
			case RESET_DEPLOYMENT -> click(subPanelButton(RECOVERY, "resetDeploymentButton"), null, null);
			case SELECT_SEPARATION -> click(subPanelButton(STAGES, "selectSeparationButton"), step.answer(), null);
			case RESET_SEPARATION -> click(subPanelButton(STAGES, "resetDeploymentButton"), null, null);
			default -> throw new IllegalStateException("not a step of the tab: " + step.kind().name);
		}
	}

	/** A sub-tab (FlightConfigurationPanel.tabs, a JTabbedPane): a click on an enabled tab. */
	private void configTab(String name) {
		int index = List.of(SUB_TABS).indexOf(name);
		if (!tabs().isEnabledAt(index)) {
			throw new IllegalStateException("the sub-tab " + name + " is disabled");
		}
		if (tabs().getSelectedIndex() == index) {
			throw new IllegalStateException("the sub-tab " + name + " is the current one already");
		}
		tabs().setSelectedIndex(index);
	}

	/**
	 * Clicks on cells of the current sub-tab's table (BasicTableUI's mouse handler: JTable.changeSelection
	 * with Ctrl as toggle and Shift as extend), as the flight-config pins do.
	 */
	private void selectCells(List<Scenario.Cell> cells) {
		JTable table = table(current());
		for (Scenario.Cell cell : cells) {
			if (cell.row() < 0 || cell.row() >= table.getRowCount()) {
				throw new IllegalArgumentException("the table has no row " + cell.row());
			}
			int column = cell.column() == null ? 0 : column(table, ComponentPaths.resolve(rocket, cell.column()));
			table.changeSelection(cell.row(), column, "ctrl".equals(cell.with()), "shift".equals(cell.with()));
		}
	}

	/** The view column of @p component in @p table (its cells hold Pair(fcid, component)). */
	private static int column(JTable table, RocketComponent component) {
		if (table.getRowCount() == 0) {
			throw new IllegalStateException("the table has no rows to click");
		}
		for (int c = 1; c < table.getColumnCount(); c++) {
			Object value = table.getValueAt(0, c);
			if (value instanceof Pair<?, ?> pair && pair.getV() == component) {
				return c;
			}
		}
		throw new IllegalArgumentException("the table has no column of " + component.getName());
	}

	/**
	 * The check box of a motor mount in the motor mount table (MotorMountTableModel.setValueAt, which a click
	 * on the box runs: GUIUtil.BooleanTableClickListener), with the undo position of QtRocket's fix of bug 5.
	 * The box must change: QtRocket's operation does nothing for a value the mount has.
	 */
	private void mountCheck(List<Scenario.PathElement> path, boolean checked) {
		requireSubTab(MOTORS);
		RocketComponent component = ComponentPaths.resolve(rocket, path);
		TableModel mounts = mountTable().getModel();
		for (int row = 0; row < mounts.getRowCount(); row++) {
			if (potentialMounts(mounts).get(row) == component) {
				if ((Boolean) mounts.getValueAt(row, 0) == checked) {
					throw new IllegalStateException("the motor mount check box of " + component.getName() + " is "
							+ checked + " already");
				}
				document.addUndoPosition(MODIFY_MOTOR_MOUNT);
				mounts.setValueAt(checked, row, 0);
				return;
			}
		}
		throw new IllegalArgumentException(component.getName() + " is no motor mount of the table");
	}

	@SuppressWarnings("unchecked")
	private static List<MotorMount> potentialMounts(TableModel model) {
		return (List<MotorMount>) Reflection.get(model.getClass(), model, "potentialMounts");
	}

	/**
	 * New Configuration, with the undo position of QtRocket's fix of bug 5; @p answer answers the question
	 * the panel asks next (FlightConfigurationPanel.java:160-172: the current sub-tab's Select motor, Select
	 * deployment or Select separation, when a cell of a column is selected).
	 */
	private void newConfiguration(JsonObject answer) {
		JButton button = configurationButton("newConfButton");
		requireEnabled(button);
		document.addUndoPosition(NEW_CONFIGURATION);
		questions.run(() -> button.doClick(0), answer, null);
	}

	/**
	 * A click on @p button (enabled and on the sub-tab shown), its question answered with @p answer; @p cell
	 * is the first cell's motor configuration for the motor chooser's checks.
	 */
	private void click(JButton button, JsonObject answer, MotorConfiguration cell) {
		requireEnabled(button);
		questions.run(() -> button.doClick(0), answer, cell);
	}

	private static void requireEnabled(JButton button) {
		if (!button.isEnabled()) {
			throw new IllegalStateException("the button " + button.getText() + " is disabled");
		}
	}

	private JButton configurationButton(String name) {
		return (JButton) Reflection.get(FlightConfigurationPanel.class, panel, name);
	}

	/** The button @p name of the sub-panel of @p tab, which must be the sub-tab shown. */
	private JButton subPanelButton(int tab, String name) {
		requireSubTab(tab);
		FlightConfigurablePanel<?> p = subPanel(tab);
		return (JButton) Reflection.get(p.getClass(), p, name);
	}

	private void requireSubTab(int tab) {
		if (tabs().getSelectedIndex() != tab) {
			throw new IllegalStateException("the step needs the sub-tab " + SUB_TABS[tab] + ", the tab shows "
					+ SUB_TABS[tabs().getSelectedIndex()]);
		}
	}

	/** The motor configuration of the first selected cell of the motor table (what the chooser opens for). */
	private MotorConfiguration firstCell() {
		List<MotorMount> mounts = selectedComponents(MOTORS);
		List<FlightConfigurationId> ids = selectedConfigurationIds(MOTORS);
		if (mounts == null || ids == null || mounts.isEmpty() || ids.isEmpty()) {
			return null;
		}
		return mounts.get(0).getMotorConfig(ids.get(0));
	}

	/**
	 * Bug 4 (plan 10.3, extended by the user's answer of 2026-10-27): OpenRocket's ignition dialog and its
	 * Reset ignition edit the mount's default configuration for a cell without a motor configuration of its
	 * own (mount.getMotorConfig(fcid) is the default there), where QtRocket's make the cell's own
	 * configuration or leave the cell alone. The tool fails on such a cell.
	 */
	private void requireOwnMotorConfigurations(String what) {
		List<MotorMount> mounts = selectedComponents(MOTORS);
		List<FlightConfigurationId> ids = selectedConfigurationIds(MOTORS);
		if (mounts == null || ids == null) {
			return;
		}
		for (MotorMount mount : mounts) {
			for (FlightConfigurationId id : ids) {
				if (!mount.getMotorConfigurationSet().containsId(id)) {
					throw new IllegalStateException("bug 4: " + what + " on a cell without a motor configuration of"
							+ " its own (" + ((RocketComponent) mount).getName() + ", row " + rocket.getIds().indexOf(id)
							+ ") changes the mount's default in OpenRocket");
				}
			}
		}
	}

	@SuppressWarnings("unchecked")
	private <T> List<T> selectedComponents(int tab) {
		return (List<T>) Reflection.invoke(subPanel(tab), "getSelectedComponents");
	}

	@SuppressWarnings("unchecked")
	private List<FlightConfigurationId> selectedConfigurationIds(int tab) {
		return (List<FlightConfigurationId>) Reflection.invoke(subPanel(tab), "getSelectedConfigurationIds");
	}

	// ------------------------------------------------------------------ the panels

	private JTabbedPane tabs() {
		return (JTabbedPane) Reflection.get(FlightConfigurationPanel.class, panel, "tabs");
	}

	private FlightConfigurablePanel<?> current() {
		return subPanel(tabs().getSelectedIndex());
	}

	private MotorConfigurationPanel motorPanel() {
		return (MotorConfigurationPanel) subPanel(MOTORS);
	}

	private FlightConfigurablePanel<?> subPanel(int tab) {
		String name = switch (tab) {
			case MOTORS -> "motorConfigurationPanel";
			case RECOVERY -> "recoveryConfigurationPanel";
			default -> "separationConfigurationPanel";
		};
		return (FlightConfigurablePanel<?>) Reflection.get(FlightConfigurationPanel.class, panel, name);
	}

	private static JTable table(FlightConfigurablePanel<?> p) {
		return (JTable) Reflection.get(FlightConfigurablePanel.class, p, "table");
	}

	/** The motor mount table of the Motors sub-panel (MotorMountConfigurationPanel's JTable). */
	private JTable mountTable() {
		JTable configurations = table(motorPanel());
		for (Component c : Questions.all(motorPanel())) {
			if (c instanceof JTable t && t != configurations) {
				return t;
			}
		}
		throw new IllegalStateException("the Motors sub-panel has no motor mount table");
	}

	// ------------------------------------------------------------------ the record

	/** What the tab shows (README.md, "state.json", the member "flightConfiguration"). */
	JsonObject record() {
		JsonObject object = new JsonObject();
		object.addProperty("configTab", SUB_TABS[tabs().getSelectedIndex()]);
		JsonArray subTabs = new JsonArray();
		for (int i = 0; i < SUB_TABS.length; i++) {
			JsonObject tab = new JsonObject();
			tab.addProperty("tab", SUB_TABS[i]);
			tab.addProperty("title", tabs().getTitleAt(i));
			tab.addProperty("enabled", tabs().isEnabledAt(i));
			subTabs.add(tab);
		}
		object.add("configTabs", subTabs);
		object.add("buttons", buttons(FlightConfigurationPanel.class, panel));
		object.addProperty("selectedConfiguration", rocket.getIds().indexOf(rocket.getSelectedConfiguration().getId()));
		JsonArray simulations = new JsonArray();
		for (Simulation simulation : document.getSimulations()) {
			JsonObject s = new JsonObject();
			s.addProperty("name", simulation.getName());
			s.addProperty("configuration", rocket.getIds().indexOf(simulation.getFlightConfigurationId()));
			simulations.add(s);
		}
		object.add("simulations", simulations);
		for (int tab = 0; tab < SUB_TABS.length; tab++) {
			object.add(SUB_TABS[tab], subPanelRecord(tab));
		}
		return object;
	}

	/** The enabled state of every JButton field @p type declares, by field name (in the order of the names). */
	private static JsonObject buttons(Class<?> type, Object object) {
		Map<String, Boolean> states = new TreeMap<>();
		for (Field field : type.getDeclaredFields()) {
			if (JButton.class.isAssignableFrom(field.getType())) {
				states.put(field.getName(), ((JButton) Reflection.get(type, object, field.getName())).isEnabled());
			}
		}
		JsonObject buttons = new JsonObject();
		states.forEach(buttons::addProperty);
		return buttons;
	}

	private JsonObject subPanelRecord(int tab) {
		FlightConfigurablePanel<?> p = subPanel(tab);
		JTable table = table(p);
		JsonObject object = new JsonObject();
		object.add("buttons", buttons(p.getClass(), p));
		if (tab == MOTORS) {
			// The card of the Motors sub-panel: the help text while no component is a motor mount.
			Container cards = (Container) Reflection.get(MotorConfigurationPanel.class, p, "cards");
			JLabel help = null;
			for (Component c : cards.getComponents()) {
				if (c instanceof JLabel l) {
					help = l;
				}
			}
			object.addProperty("showsHelp", help != null && help.isVisible());
			JsonArray mounts = new JsonArray();
			TableModel model = mountTable().getModel();
			for (int row = 0; row < model.getRowCount(); row++) {
				JsonObject mount = new JsonObject();
				mount.add("component", ComponentPaths.json((RocketComponent) potentialMounts(model).get(row)));
				mount.addProperty("text", (String) model.getValueAt(row, 1));
				mount.addProperty("motorMount", (Boolean) model.getValueAt(row, 0));
				mounts.add(mount);
			}
			object.add("mounts", mounts);
		}
		JsonArray header = new JsonArray();
		for (int c = 0; c < table.getColumnCount(); c++) {
			Object value = table.getColumnModel().getColumn(c).getHeaderValue();
			header.add(value == null ? table.getColumnName(c) : value.toString());
		}
		object.add("header", header);
		JsonArray rows = new JsonArray();
		for (int r = 0; r < table.getRowCount(); r++) {
			JsonObject row = new JsonObject();
			row.addProperty("configuration", rendered(table, r, 0).getText());
			JsonArray cells = new JsonArray();
			for (int c = 1; c < table.getColumnCount(); c++) {
				JLabel label = rendered(table, r, c);
				if (tab == MOTORS) {
					// MotorTableCellRenderer.format(): a label holding the motor's label and the ignition's.
					List<JLabel> parts = new ArrayList<>();
					for (Component k : label.getComponents()) {
						if (k instanceof JLabel l) {
							parts.add(l);
						}
					}
					JsonObject cell = new JsonObject();
					cell.addProperty("motor", parts.get(0).getText());
					cell.addProperty("ignition", parts.get(1).getText());
					Font font = parts.get(1).getFont();
					cell.addProperty("ignitionDefault", font != null && font.isItalic());
					cells.add(cell);
				} else {
					cells.add(valueCell(table, r, c, label));
				}
			}
			row.add("cells", cells);
			rows.add(row);
		}
		object.add("rows", rows);
		object.add("selectedRows", ints(table.getSelectedRows()));
		object.add("selectedColumns", ints(table.getSelectedColumns()));
		return object;
	}

	/**
	 * A cell of the Recovery or Stages table: its text and whether the renderer shades it (italic, as
	 * FlightConfigurableCellRenderer.shaded() sets it: RecoveryTableCellRenderer and
	 * SeparationTableCellRenderer shade a cell when isDefault(fcid), which compares by identity and is false
	 * for every configuration, bug 1), and whether the configuration has an override of its own
	 * (FlightConfigurableParameterSet.containsId(fcid), the state QtRocket's shading shows: QtRocket shades a
	 * cell without one).
	 */
	private static JsonObject valueCell(JTable table, int row, int column, JLabel label) {
		Pair<?, ?> value = (Pair<?, ?>) table.getValueAt(row, column);
		FlightConfigurationId id = (FlightConfigurationId) value.getU();
		boolean override;
		if (value.getV() instanceof RecoveryDevice device) {
			override = device.getDeploymentConfigurations().containsId(id);
		} else if (value.getV() instanceof AxialStage stage) {
			override = stage.getSeparationConfigurations().containsId(id);
		} else {
			throw new IllegalStateException("a cell of " + value.getV() + " in the Recovery or Stages table");
		}
		JsonObject cell = new JsonObject();
		cell.addProperty("text", label.getText());
		Font font = label.getFont();
		cell.addProperty("shaded", font != null && font.isItalic());
		cell.addProperty("override", override);
		return cell;
	}

	/** A cell as the table's renderer draws it (not selected, without focus). */
	private static JLabel rendered(JTable table, int row, int column) {
		return (JLabel) table.getCellRenderer(row, column).getTableCellRendererComponent(table,
				table.getValueAt(row, column), false, false, row, column);
	}

	private static JsonArray ints(int[] values) {
		JsonArray array = new JsonArray();
		for (int v : values) {
			array.add(v);
		}
		return array;
	}
}
