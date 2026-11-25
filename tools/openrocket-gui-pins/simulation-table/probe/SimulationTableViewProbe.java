package probe;

import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.OpenRocketDocumentFactory;
import info.openrocket.core.file.GeneralRocketLoader;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.startup.Application;
import info.openrocket.swing.gui.main.SimulationPanel;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.util.GUIUtil;

import java.awt.Component;
import java.awt.Point;
import java.awt.Rectangle;
import java.awt.event.ActionEvent;
import java.awt.event.InputEvent;
import java.awt.event.MouseEvent;
import java.io.File;
import java.lang.reflect.Method;
import java.lang.reflect.RecordComponent;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import javax.swing.Action;
import javax.swing.InputMap;
import javax.swing.JCheckBoxMenuItem;
import javax.swing.JComponent;
import javax.swing.JMenuItem;
import javax.swing.JPopupMenu;
import javax.swing.JTable;
import javax.swing.KeyStroke;
import javax.swing.ListSelectionModel;
import javax.swing.RowSorter;
import javax.swing.SwingUtilities;
import javax.swing.table.JTableHeader;
import javax.swing.table.TableColumnModel;

/**
 * What OpenRocket's simulations table does as a view (SimulationPanel.java:257-387 and its ColumnVisibilityController,
 * 1787-2009), beyond the cells, orders and texts SimulationTableProbe pins, for QtRocket's
 * SimulationColumnVisibilityTests.cpp and SimulationTableViewTests.cpp (tests/gui/simulation/). The tests pin this
 * transcript by hand: the family's hand-pinned.txt keeps it, and generate.sh compares every run with it.
 * <p>
 * The panels are OpenRocket's real SimulationPanel, made for a document and never shown (its RocketActions need a
 * toolkit: xvfb-run), with SimulationTableProbe's bootstrap (the bundled motors, the examples' presets, the
 * preferences in generate.sh's fresh store, MotorNameColumn false) and FlatLaf Light. The column chooser's private
 * methods are called by reflection, as its menu's items call them; the keys are run through the table's input and
 * action maps, as Swing runs a key bound to an action; the header clicks are mouse events dispatched to the header.
 * <p>
 * Output: one line each, its fields separated by tabs.
 * <ul>
 * <li>COLS case docpref=D apppref=A saved=S hidden=H view=V: after a step of the column chooser, the document
 * preference simulation.table.hiddenColumns (&lt;unset&gt; when absent), the application preference
 * simulation.table.hiddenColumns.default, the document's isSaved(), the controller's hidden ids in their order and
 * the view's column ids from the left</li>
 * <li>MENU case item...: the header menu (createMenu()): each check box item's text, [x] or [ ] and (disabled) when
 * it is, --- for the separator, and the last item's text with its tooltip in braces</li>
 * <li>BIND map key action: the action the table's WHEN_FOCUSED (focused) or WHEN_ANCESTOR_OF_FOCUSED_COMPONENT
 * (ancestor) input map binds a key stroke to (&lt;none&gt; when it binds none)</li>
 * <li>KEY case action state: after an action of the table's action map, the selection: the view rows selected,
 * the row selection's lead and anchor, and the id of the column of the column selection's lead</li>
 * <li>HEADER case keys: the row sorter's sort keys (column id and order, the primary first) after mouse events on
 * the header of a column</li>
 * </ul>
 * usage: SimulationTableViewProbe COMPONENTS EXAMPLES
 */
public class SimulationTableViewProbe {

	static final String DOC_KEY = "simulation.table.hiddenColumns";
	static final String APP_KEY = "simulation.table.hiddenColumns.default";

	public static void main(String[] args) throws Exception {
		int status = 0;
		try {
			SimulationTableProbe.bootstrap(new File(args[0]));
			File examples = new File(args[1]);
			SwingUtilities.invokeAndWait(() -> {
				try {
					GUIUtil.setUITheme(UITheme.Themes.LIGHT);
					SimulationPanel.updateColors();
					columnChooser(new File(examples, "A simple model rocket.ork"));
					keys(new File(examples, "A simple model rocket.ork"));
					header(new File(examples, "A simple model rocket.ork"));
				} catch (Exception e) {
					throw new RuntimeException(e);
				}
			});
		} catch (Throwable t) {
			t.printStackTrace();
			status = 1;
		}
		System.out.flush();
		System.exit(status);
	}

	// ------------------------------------------------------------------------------ helpers

	static void line(Object... fields) {
		SimulationTableProbe.line(fields);
	}

	static OpenRocketDocument load(File file) throws Exception {
		DeterministicUuids.reseed("simulation-table-view:" + file.getName());
		OpenRocketDocument document = new GeneralRocketLoader(file).load();
		document.setSaved(true);
		return document;
	}

	static OpenRocketDocument newDesign() {
		DeterministicUuids.reseed("simulation-table-view:new-design");
		OpenRocketDocument document = OpenRocketDocumentFactory.createNewRocket();
		document.setSaved(true);
		return document;
	}

	static void setAppPref(String value) {
		((ApplicationPreferences) Application.getPreferences()).putString(APP_KEY, value);
	}

	/**
	 * The application preference as stored, read from the preferences node itself: SwingPreferences.getString(key,
	 * def) stores def when the key is absent (so the controller's getString(APP_KEY, "") leaves "" behind).
	 */
	static String appPref() {
		String value = ((ApplicationPreferences) Application.getPreferences()).getPreferences().get(APP_KEY, null);
		return value == null ? "<unset>" : value;
	}

	static JTable table(SimulationPanel panel) throws Exception {
		return (JTable) SimulationTableProbe.field(panel, "simulationTable");
	}

	static Object controller(SimulationPanel panel) throws Exception {
		return SimulationTableProbe.field(panel, "columnVisibilityController");
	}

	static String descriptorId(Object descriptor) throws Exception {
		for (RecordComponent component : descriptor.getClass().getRecordComponents()) {
			if (component.getName().equals("id")) {
				Method accessor = component.getAccessor();
				accessor.setAccessible(true);
				return (String) accessor.invoke(descriptor);
			}
		}
		throw new IllegalStateException("no id");
	}

	static Object descriptor(Object controller, String id) throws Exception {
		for (Object descriptor : (List<?>) SimulationTableProbe.field(controller, "descriptors")) {
			if (descriptorId(descriptor).equals(id)) {
				return descriptor;
			}
		}
		throw new IllegalStateException("no column " + id);
	}

	static Method method(Object o, String name) {
		for (Method m : o.getClass().getDeclaredMethods()) {
			if (m.getName().equals(name)) {
				m.setAccessible(true);
				return m;
			}
		}
		throw new IllegalStateException("no method " + name);
	}

	/** A check box item of the header menu clicked: toggleColumn(descriptor, selected). */
	static void toggle(SimulationPanel panel, String id, boolean visible) throws Exception {
		Object controller = controller(panel);
		method(controller, "toggleColumn").invoke(controller, descriptor(controller, id), visible);
	}

	static void saveAsDefault(SimulationPanel panel) throws Exception {
		Object controller = controller(panel);
		method(controller, "saveHiddenColumnsToApplicationPreferences").invoke(controller);
	}

	static String viewIds(SimulationPanel panel) throws Exception {
		TableColumnModel columns = table(panel).getColumnModel();
		List<String> ids = new ArrayList<>();
		for (int i = 0; i < columns.getColumnCount(); i++) {
			ids.add(String.valueOf(columns.getColumn(i).getIdentifier()));
		}
		return String.join(",", ids);
	}

	static void cols(String step, OpenRocketDocument document, SimulationPanel panel) throws Exception {
		Collection<?> hidden = (Collection<?>) SimulationTableProbe.field(controller(panel), "hiddenColumnIds");
		List<String> ids = new ArrayList<>();
		for (Object id : hidden) {
			ids.add(String.valueOf(id));
		}
		line("COLS", step, "docpref=" + document.getDocumentPreferences().getString(DOC_KEY, "<unset>"),
				"apppref=" + appPref(), "saved=" + document.isSaved(), "hidden=" + String.join(",", ids),
				"view=" + viewIds(panel));
	}

	static void menu(String step, SimulationPanel panel) throws Exception {
		Object controller = controller(panel);
		JPopupMenu menu = (JPopupMenu) method(controller, "createMenu").invoke(controller);
		List<Object> fields = new ArrayList<>(List.of("MENU", step));
		for (Component item : menu.getComponents()) {
			if (item instanceof JCheckBoxMenuItem box) {
				fields.add(box.getText() + (box.isSelected() ? "[x]" : "[ ]") + (box.isEnabled() ? "" : "(disabled)"));
			} else if (item instanceof JMenuItem plain) {
				fields.add(plain.getText() + (plain.isEnabled() ? "" : "(disabled)") + "{" + plain.getToolTipText() + "}");
			} else {
				fields.add("---");
			}
		}
		line(fields.toArray());
	}

	// ------------------------------------------------------------------------------ the column chooser

	static void columnChooser(File simple) throws Exception {
		setAppPref(null);
		// A new design, no preference at all: the stepper is hidden.
		OpenRocketDocument fresh = newDesign();
		SimulationPanel freshPanel = new SimulationPanel(null, fresh);
		cols("new design", fresh, freshPanel);
		menu("new design", freshPanel);

		// The scout's steps and more, in one document: each toggle writes the document preference.
		OpenRocketDocument document = load(simple);
		SimulationPanel panel = new SimulationPanel(null, document);
		cols("loaded", document, panel);
		toggle(panel, "simulationStepper", true);
		cols("show simulationStepper", document, panel);
		toggle(panel, "apogee", false);
		cols("hide apogee", document, panel);
		toggle(panel, "status", false);
		cols("hide status", document, panel);
		toggle(panel, "apogee", true);
		cols("show apogee", document, panel);
		toggle(panel, "apogee", false);
		cols("hide apogee again", document, panel);
		menu("status and apogee hidden", panel);
		saveAsDefault(panel);
		cols("save as default", document, panel);

		// The application's preference for a design without its own.
		OpenRocketDocument fromDefault = newDesign();
		SimulationPanel fromDefaultPanel = new SimulationPanel(null, fromDefault);
		cols("new design after save as default", fromDefault, fromDefaultPanel);
		// An empty document preference counts as none: the application's one applies.
		OpenRocketDocument empty = newDesign();
		empty.getDocumentPreferences().putString(DOC_KEY, "");
		empty.setSaved(true);
		SimulationPanel emptyPanel = new SimulationPanel(null, empty);
		cols("empty docpref, apppref set", empty, emptyPanel);
		// The document's own preference wins over the application's.
		OpenRocketDocument own = newDesign();
		own.getDocumentPreferences().putString(DOC_KEY, "flightTime");
		own.setSaved(true);
		SimulationPanel ownPanel = new SimulationPanel(null, own);
		cols("docpref flightTime, apppref set", own, ownPanel);
		setAppPref(null);

		// Blanks, empty and unknown ids.
		OpenRocketDocument odd = newDesign();
		odd.getDocumentPreferences().putString(DOC_KEY, " name , bogus,,flightTime ,name");
		odd.setSaved(true);
		SimulationPanel oddPanel = new SimulationPanel(null, odd);
		cols("docpref with blanks, an unknown id and a repeat", odd, oddPanel);
		toggle(oddPanel, "apogee", false);
		cols("then hide apogee", odd, oddPanel);

		// Every column hidden: the first comes back.
		OpenRocketDocument all = newDesign();
		all.getDocumentPreferences().putString(DOC_KEY, "status,warnings,name,configuration,simulationStepper,"
				+ "launchRodVelocity,apogee,deploymentVelocity,optimumCoastTime,maxVelocity,maxAcceleration,"
				+ "timeToApogee,flightTime,groundHitVelocity");
		all.setSaved(true);
		SimulationPanel allPanel = new SimulationPanel(null, all);
		cols("docpref hides every column", all, allPanel);
		menu("docpref hides every column", allPanel);
		toggle(allPanel, "name", true);
		cols("then show name", all, allPanel);

		// Down to one column: the last one cannot be hidden.
		OpenRocketDocument one = load(simple);
		SimulationPanel onePanel = new SimulationPanel(null, one);
		for (String id : new String[] {"status", "warnings", "name", "configuration", "simulationStepper",
				"launchRodVelocity", "apogee", "deploymentVelocity", "optimumCoastTime", "maxVelocity",
				"maxAcceleration", "timeToApogee", "flightTime"}) {
			toggle(onePanel, id, false);
		}
		cols("hide all but groundHitVelocity", one, onePanel);
		menu("one column", onePanel);
		toggle(onePanel, "groundHitVelocity", false);
		cols("then hide groundHitVelocity", one, onePanel);
		toggle(onePanel, "maxVelocity", true);
		cols("then show maxVelocity", one, onePanel);

		// Moved columns: a column shown again goes where its model predecessors that are shown count.
		OpenRocketDocument moved = load(simple);
		SimulationPanel movedPanel = new SimulationPanel(null, moved);
		TableColumnModel model = table(movedPanel).getColumnModel();
		model.moveColumn(model.getColumnIndex("apogee"), 0);
		cols("move apogee to the left", moved, movedPanel);
		toggle(movedPanel, "name", false);
		cols("hide name", moved, movedPanel);
		toggle(movedPanel, "name", true);
		cols("show name", moved, movedPanel);
		model.moveColumn(model.getColumnIndex("status"), model.getColumnCount() - 1);
		cols("move status to the right", moved, movedPanel);
		toggle(movedPanel, "simulationStepper", true);
		cols("show simulationStepper", moved, movedPanel);
		toggle(movedPanel, "warnings", false);
		toggle(movedPanel, "warnings", true);
		cols("hide and show warnings", moved, movedPanel);

		// Save as default with every column shown: an empty application preference, which counts as none.
		OpenRocketDocument shown = load(simple);
		SimulationPanel shownPanel = new SimulationPanel(null, shown);
		toggle(shownPanel, "simulationStepper", true);
		saveAsDefault(shownPanel);
		cols("show simulationStepper, save as default", shown, shownPanel);
		OpenRocketDocument afterEmpty = newDesign();
		SimulationPanel afterEmptyPanel = new SimulationPanel(null, afterEmpty);
		cols("new design after an empty default", afterEmpty, afterEmptyPanel);
		setAppPref(null);
	}

	// ------------------------------------------------------------------------------ the keys

	static final String[] BOUND = {"ENTER", "shift ENTER", "ctrl A", "DELETE", "ctrl DELETE", "ctrl E", "ctrl X",
			"ctrl C", "ctrl V", "ctrl D", "SPACE", "ESCAPE", "F2", "TAB", "shift TAB", "RIGHT", "LEFT", "UP", "DOWN"};

	static String bound(InputMap map, String stroke) {
		Object action = map.get(KeyStroke.getKeyStroke(stroke));
		return action == null ? "<none>" : String.valueOf(action);
	}

	static String state(JTable table) {
		ListSelectionModel rows = table.getSelectionModel();
		List<String> selected = new ArrayList<>();
		for (int row : table.getSelectedRows()) {
			selected.add(Integer.toString(row));
		}
		int lead = table.getColumnModel().getSelectionModel().getLeadSelectionIndex();
		String column = lead < 0 || lead >= table.getColumnCount() ? "<none>"
				: String.valueOf(table.getColumnModel().getColumn(lead).getIdentifier());
		return "rows [" + String.join(" ", selected) + "] lead " + rows.getLeadSelectionIndex() + " anchor "
				+ rows.getAnchorSelectionIndex() + " column " + column;
	}

	static void act(String step, JTable table, String stroke) {
		Object name = table.getInputMap(JComponent.WHEN_ANCESTOR_OF_FOCUSED_COMPONENT).get(KeyStroke.getKeyStroke(stroke));
		if (name == null) {
			name = table.getInputMap(JComponent.WHEN_FOCUSED).get(KeyStroke.getKeyStroke(stroke));
		}
		Action action = table.getActionMap().get(name);
		action.actionPerformed(new ActionEvent(table, ActionEvent.ACTION_PERFORMED, String.valueOf(name)));
		line("KEY", step, stroke, state(table));
	}

	/** A click on a cell (JTable.changeSelection(), what BasicTableUI does for a press). */
	static void click(String step, JTable table, int row, String column, boolean toggle, boolean extend) {
		table.changeSelection(row, table.getColumnModel().getColumnIndex(column), toggle, extend);
		line("KEY", step, "click", state(table));
	}

	static void keys(File simple) throws Exception {
		setAppPref(null);
		OpenRocketDocument document = load(simple);
		SimulationPanel panel = new SimulationPanel(null, document);
		JTable table = table(panel);
		for (String stroke : BOUND) {
			line("BIND", "focused", stroke, bound(table.getInputMap(JComponent.WHEN_FOCUSED), stroke));
			line("BIND", "ancestor", stroke, bound(table.getInputMap(JComponent.WHEN_ANCESTOR_OF_FOCUSED_COMPONENT),
					stroke));
		}
		line("KEY", "start", "-", state(table));
		act("no selection", table, "ENTER");
		table.clearSelection();
		table.getSelectionModel().setAnchorSelectionIndex(-1);
		table.getSelectionModel().setLeadSelectionIndex(-1);
		table.getColumnModel().getSelectionModel().setAnchorSelectionIndex(-1);
		table.getColumnModel().getSelectionModel().setLeadSelectionIndex(-1);
		line("KEY", "cleared", "-", state(table));
		act("no selection", table, "shift ENTER");

		click("row 2 apogee", table, 2, "apogee", false, false);
		act("row 2 apogee", table, "ENTER");
		act("row 2 apogee", table, "ENTER");
		act("row 2 apogee", table, "ENTER");
		act("row 2 apogee", table, "shift ENTER");
		act("row 2 apogee", table, "shift ENTER");

		click("last cell", table, 4, "groundHitVelocity", false, false);
		act("last cell", table, "ENTER");
		click("first cell", table, 0, "status", false, false);
		act("first cell", table, "shift ENTER");

		click("rows 1-3 name", table, 1, "name", false, false);
		click("rows 1-3 name", table, 3, "name", false, true);
		for (int i = 0; i < 4; i++) {
			act("rows 1-3 name", table, "ENTER");
		}
		for (int i = 0; i < 2; i++) {
			act("rows 1-3 name", table, "shift ENTER");
		}
		click("rows 0 and 3 apogee", table, 0, "apogee", false, false);
		click("rows 0 and 3 apogee", table, 3, "apogee", true, false);
		for (int i = 0; i < 3; i++) {
			act("rows 0 and 3 apogee", table, "ENTER");
		}
		act("rows 0 and 3 apogee", table, "ctrl A");
		act("all", table, "ENTER");

		// The lead on a row that is not selected (a Ctrl+click that deselects it).
		click("rows 1 and 2, lead on 3", table, 1, "name", false, false);
		click("rows 1 and 2, lead on 3", table, 2, "name", true, false);
		click("rows 1 and 2, lead on 3", table, 3, "name", true, false);
		click("rows 1 and 2, lead on 3", table, 3, "name", true, false);
		act("rows 1 and 2, lead on 3", table, "ENTER");
		act("rows 1 and 2, lead on 3", table, "ENTER");
		click("row 1, lead on 3", table, 1, "name", false, false);
		click("row 1, lead on 3", table, 3, "name", true, false);
		click("row 1, lead on 3", table, 3, "name", true, false);
		act("row 1, lead on 3", table, "ENTER");
		click("row 1, lead on 3", table, 1, "name", false, false);
		click("row 1, lead on 3", table, 3, "name", true, false);
		click("row 1, lead on 3", table, 3, "name", true, false);
		act("row 1, lead on 3", table, "shift ENTER");
		// Nothing selected, the lead kept (JTable.clearSelection()).
		click("cleared after row 2 apogee", table, 2, "apogee", false, false);
		table.clearSelection();
		line("KEY", "cleared after row 2 apogee", "clearSelection", state(table));
		act("cleared after row 2 apogee", table, "ENTER");
		click("cleared after row 2 apogee", table, 2, "apogee", false, false);
		table.clearSelection();
		act("cleared after row 2 apogee", table, "shift ENTER");
		// The lead in a column that is hidden afterwards.
		click("lead in a hidden column", table, 2, "apogee", false, false);
		toggle(panel, "apogee", false);
		line("KEY", "lead in a hidden column", "hide apogee", state(table));
		act("lead in a hidden column", table, "ENTER");
		toggle(panel, "apogee", true);

		// Sorted by apogee descending: the keys move by view rows.
		RowSorter<?> sorter = table.getRowSorter();
		sorter.toggleSortOrder(table.getColumnModel().getColumn(table.getColumnModel().getColumnIndex("apogee"))
				.getModelIndex());
		sorter.toggleSortOrder(table.getColumnModel().getColumn(table.getColumnModel().getColumnIndex("apogee"))
				.getModelIndex());
		click("sorted row 0 name", table, 0, "name", false, false);
		act("sorted row 0 name", table, "ENTER");
	}

	// ------------------------------------------------------------------------------ the header

	static String sortKeys(JTable table) {
		List<String> keys = new ArrayList<>();
		for (RowSorter.SortKey key : table.getRowSorter().getSortKeys()) {
			keys.add(columnId(table, key.getColumn()) + " " + key.getSortOrder());
		}
		return keys.isEmpty() ? "<none>" : String.join(", ", keys);
	}

	static String columnId(JTable table, int modelColumn) {
		TableColumnModel columns = table.getColumnModel();
		for (int i = 0; i < columns.getColumnCount(); i++) {
			if (columns.getColumn(i).getModelIndex() == modelColumn) {
				return String.valueOf(columns.getColumn(i).getIdentifier());
			}
		}
		return "model" + modelColumn;
	}

	/** A press, a release and a click of @p button with the click count @p count on the header of @p column. */
	static void clickHeader(String step, JTable table, String column, int button, int count) {
		JTableHeader header = table.getTableHeader();
		header.setSize(header.getPreferredSize());
		Rectangle rect = header.getHeaderRect(table.getColumnModel().getColumnIndex(column));
		Point at = new Point(rect.x + rect.width / 2, rect.y + rect.height / 2);
		int mask = button == MouseEvent.BUTTON1 ? InputEvent.BUTTON1_DOWN_MASK : InputEvent.BUTTON3_DOWN_MASK;
		long when = System.currentTimeMillis();
		// Never a popup trigger: the column chooser would show its menu, which needs a header on the screen.
		header.dispatchEvent(new MouseEvent(header, MouseEvent.MOUSE_PRESSED, when, mask, at.x, at.y, count, false,
				button));
		header.dispatchEvent(new MouseEvent(header, MouseEvent.MOUSE_RELEASED, when, 0, at.x, at.y, count, false,
				button));
		header.dispatchEvent(new MouseEvent(header, MouseEvent.MOUSE_CLICKED, when, 0, at.x, at.y, count, false,
				button));
		line("HEADER", step, sortKeys(table));
	}

	static void header(File simple) throws Exception {
		OpenRocketDocument document = load(simple);
		SimulationPanel panel = new SimulationPanel(null, document);
		JTable table = table(panel);
		table.setSize(table.getPreferredSize());
		line("HEADER", "start", sortKeys(table));
		clickHeader("name, click 1", table, "name", MouseEvent.BUTTON1, 1);
		clickHeader("name, click 2 of a double click", table, "name", MouseEvent.BUTTON1, 2);
		clickHeader("name, click 3 of a triple click", table, "name", MouseEvent.BUTTON1, 3);
		clickHeader("name, a new click", table, "name", MouseEvent.BUTTON1, 1);
		clickHeader("apogee, click 1", table, "apogee", MouseEvent.BUTTON1, 1);
		clickHeader("flightTime, click 1", table, "flightTime", MouseEvent.BUTTON1, 1);
		clickHeader("status, click 1", table, "status", MouseEvent.BUTTON1, 1);
		clickHeader("apogee, right click", table, "apogee", MouseEvent.BUTTON3, 1);
		// A hidden column's neighbour, after the stepper column was shown and the apogee column moved left.
		toggle(panel, "simulationStepper", true);
		TableColumnModel model = table.getColumnModel();
		model.moveColumn(model.getColumnIndex("apogee"), 0);
		toggle(panel, "warnings", false);
		clickHeader("simulationStepper after moves, click 1", table, "simulationStepper", MouseEvent.BUTTON1, 1);
		clickHeader("apogee after moves, click 1", table, "apogee", MouseEvent.BUTTON1, 1);
	}
}
