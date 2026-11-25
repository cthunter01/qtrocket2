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
import info.openrocket.core.document.OpenRocketDocumentFactory;
import info.openrocket.core.document.Simulation;
import info.openrocket.core.file.GeneralRocketLoader;
import info.openrocket.core.formatting.RocketDescriptor;
import info.openrocket.core.logging.Warning;
import info.openrocket.core.logging.WarningSet;
import info.openrocket.core.motor.IgnitionEvent;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.preset.ComponentPreset;
import info.openrocket.core.preset.xml.OpenRocketComponentLoader;
import info.openrocket.core.rocketcomponent.BodyTube;
import info.openrocket.core.rocketcomponent.ComponentChangeEvent;
import info.openrocket.core.rocketcomponent.FlightConfigurationId;
import info.openrocket.core.rocketcomponent.MotorMount;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.startup.Application;
import info.openrocket.core.util.ORColor;
import info.openrocket.core.util.TestRockets;
import info.openrocket.swing.file.SimulationTableCSVExport;
import info.openrocket.swing.gui.adaptors.ColumnTableModel;
import info.openrocket.swing.gui.main.SimulationPanel;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.util.GUIUtil;
import info.openrocket.swing.gui.util.Icons;
import info.openrocket.swing.gui.util.SwingPreferences;

import java.awt.Color;
import java.awt.Component;
import java.awt.Container;
import java.awt.Window;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.TreeSet;
import javax.swing.Action;
import javax.swing.Icon;
import javax.swing.ImageIcon;
import javax.swing.JComponent;
import javax.swing.JLabel;
import javax.swing.JTable;
import javax.swing.RowSorter;
import javax.swing.SwingUtilities;
import javax.swing.table.TableColumnModel;

/**
 * OpenRocket's simulations table (the "Flight simulations" tab, SimulationPanel with its SimulationTableModel,
 * ColumnTable and ColumnTableRowSorter, swing gui/main/SimulationPanel.java and gui/adaptors/) for the pins of
 * QtRocket's simulation-table family: what the table shows of each simulation of the designs it is given, the
 * orders its header clicks give, the texts of its CSV export and its Copy, and the statuses of the simulations
 * through two scripted series of edits. A generalisation of the M4 sim-tab scout's SimTabProbe.
 * <p>
 * The designs come from a list in designs.py's format (name, kind, file; tools/openrocket-gui-pins/open-designs):
 * the family's generate.sh passes the whole list, the 66 designs of QtRocket's exit suite OpenDesignsExitGolden.
 * Each design is loaded as that suite loads it (DesignFileEnvironment::Presets::EXAMPLES): the bundled
 * motor database (-Dprobe.motors), but for the test rockets' re-saves, whose motors are TestRockets' seven
 * synthetic ones, and the six component presets of the example designs (from the bundled .orc files below the
 * directory of the second argument). The preferences are SwingPreferences in generate.sh's fresh store with the
 * one value the suite's environment has otherwise than its defaults that the table shows: MotorNameColumn false
 * (a motor by its common name in a configuration's name); FlatLaf Light. Every design's UUIDs are seeded from its
 * name (DeterministicUuids), every scenario's from its own.
 * <p>
 * The panel is OpenRocket's real SimulationPanel, made for the document and never shown; its RocketActions need a
 * toolkit (the menu shortcut key), which a headless AWT refuses, so the probe runs under xvfb-run. Its private
 * fields and methods are read by reflection.
 * <p>
 * Output: one line each, its fields separated by tabs; a text field has its backslashes, newlines and tabs
 * escaped as \\, \n and \t; a missing value (Java's null) is &lt;null&gt;.
 * <ul>
 * <li>COLORS text textDim warning error info (the theme's colours of the tooltips, #rrggbb)</li>
 * <li>H column id name toolTip unit (the model's columns: its id in the column chooser, the header, the header's
 * tooltip and the unit of the column's group, the default one: UNITS_NONE's zero-width space for a column without
 * one)</li>
 * <li>D name kind file (a design; then its lines, up to END)</li>
 * <li>V hasValidConfig nextSimulationName simulationCount</li>
 * <li>HV column id preferredWidth minWidth maxWidth (the view's columns after setupAutoSizeColumns())</li>
 * <li>R row status iconDescription sortText description toolTip (the status column of a model row: the status,
 * its icon's description, the label's toString() its sorter compares, getStatusDescription() and the tooltip of
 * every cell but the warnings')</li>
 * <li>W row critical normal informational box toolTip (the warnings column: the counts, -1 without data; the box's
 * labels, a count's text and an icon as [HIGH], [NORMAL] or [LOW]; the box's tooltip)</li>
 * <li>WS row priority text (each warning of the simulation's data, in the set's order)</li>
 * <li>C row cell2 ... cell13 (the toString() of the value of every model column from the name on)</li>
 * <li>S column asc|desc order (the model rows in the view's order after setSortKeys(null) and one or two
 * toggleSortOrder() of the column, OpenRocket's own)</li>
 * <li>SQ 1 asc|desc order (the order QtRocket gives the warnings column, which OpenRocket does not sort: its bug
 * 23. The rows by their critical, then normal, then informational count, from OpenRocket's own WarningSet, a row
 * without data first as a missing value, equal rows in the model's order; descending reverses each comparison, the
 * missing value's too, so that a row without data comes last)</li>
 * <li>A selection action=0|1 ... (the panel's twelve actions with nothing, row 0 and rows 0 and 1 selected)</li>
 * <li>CSV text, COPY text, CSVE text (SimulationTableCSVExport.generateCSVData(",", 3, false, false), the Copy
 * text of every row, generateCSVData("\t", 6, false, true) through SimulationPanel.addStatusColumn(), and
 * generateCSVData(";", 2, true, false); none for a design without simulations)</li>
 * <li>END</li>
 * <li>SCENARIO name file (a status scenario; then its lines, up to END)</li>
 * <li>SIM index name idPrefix configuration (the simulations as loaded)</li>
 * <li>ST step statuses undo redo saved (after each step: every simulation's getStatus(), space separated, the
 * undo and redo descriptions and isSaved())</li>
 * <li>FRESH row (a simulation the scenario ran: its R line follows, but no W, WS or C line, since QtRocket's tests
 * pin no value of a fresh flight)</li>
 * <li>R, W, WS, C lines of the simulations after the last step, S 0 asc|desc and S 1 asc|desc, the status and
 * the warnings columns' orders, and SQ 1 asc|desc, QtRocket's of the warnings column</li>
 * <li>PAIRING name file (the Copy of a table whose rows include one without data; then its lines, up to END)</li>
 * <li>COPY text (OpenRocket's, which pairs the statuses with the exporter's lines by position: bug 25) and COPYQ
 * text (QtRocket's: each line with its own simulation's status), every row selected</li>
 * </ul>
 * usage: SimulationTableProbe DESIGNS COMPONENTS EXAMPLES
 */
public class SimulationTableProbe {

	/** The digests of the six component presets of the example designs (tests/core/rocket/preset/ExamplePresets.h). */
	static final Set<String> EXAMPLE_PRESET_DIGESTS = Set.of("e9c3e97d32e96762f65e051d74cb9fd7",
			"b595c8a31baaf325291d5c957f3940f8", "9a96b8806749e790db29bacfbc70eb1e", "18363dec5642206a109389ef48efcb32",
			"a59dec8e4034a2fee5955dbf4ff07f1c", "6f564a198c80de3af437608827fa1bc1");

	/** The generators of the motors of TestRockets, in the order of QtRocket's testRocketMotors(). */
	static final List<String> TEST_ROCKET_MOTORS = List.of("generateMotor_A8_18mm", "generateMotor_A10_13mm",
			"generateMotor_B4_18mm", "generateMotor_C6_18mm", "generateMotor_D21_18mm", "generateMotor_M1350_75mm",
			"generateMotor_G77_29mm");

	/** The panel's actions, by their fields. */
	static final String[] ACTIONS = {"newSimulationAction", "editSimulationAction", "cutSimulationAction",
			"copySimulationAction", "pasteSimulationAction", "runSimulationAction", "landingDispersionAction",
			"plotSimulationAction", "duplicateSimulationAction", "deleteSimulationAction", "simTableExportAction",
			"selectedSimsExportAction"};

	/** The ids of the model's columns (SimulationPanel.SIMULATION_TABLE_COLUMN_IDS). */
	static final String[] COLUMN_IDS = {"status", "warnings", "name", "configuration", "simulationStepper",
			"launchRodVelocity", "apogee", "deploymentVelocity", "optimumCoastTime", "maxVelocity", "maxAcceleration",
			"timeToApogee", "flightTime", "groundHitVelocity"};

	static Injector designInjector;
	static Injector testRocketInjector;
	static RocketDescriptor descriptor;

	public static void main(String[] args) throws Exception {
		int status = 0;
		try {
			bootstrap(new File(args[1]));
			List<String> designs = Files.readAllLines(Path.of(args[0]), StandardCharsets.UTF_8);
			File examples = new File(args[2]);
			SwingUtilities.invokeAndWait(() -> {
				try {
					GUIUtil.setUITheme(UITheme.Themes.LIGHT);
					// SimulationPanel reads its colours from the theme when its class is initialised.
					SimulationPanel.updateColors();
					descriptor = Application.getInjector().getInstance(RocketDescriptor.class);
					line("COLORS", rgb(UITheme.getColor(UITheme.Keys.TEXT)), rgb(UITheme.getColor(UITheme.Keys.TEXT_DIM)),
							rgb(UITheme.getColor(UITheme.Keys.WARNING)), rgb(UITheme.getColor(UITheme.Keys.ERROR)),
							rgb(UITheme.getColor(UITheme.Keys.INFO)));
					columns();
					for (String design : designs) {
						String[] fields = design.split("\t");
						if (fields.length != 3) {
							throw new IllegalStateException("not a design: " + design);
						}
						design(fields[0], fields[1], new File(fields[2]));
					}
					Application.setInjector(designInjector);
					DeterministicUuids.reseed("new-design");
					table("new_design", "new", "", OpenRocketDocumentFactory.createNewRocket());
					statusScenario(new File(examples, "A simple model rocket.ork"));
					cantRunScenario(new File(examples, "Two stage high power rocket.ork"));
					pairing(new File(examples, "A simple model rocket.ork"));
				} catch (Exception e) {
					throw new RuntimeException(e);
				} finally {
					for (Window window : Window.getWindows()) {
						window.dispose();
					}
				}
			});
		} catch (Throwable e) {
			e.printStackTrace();
			status = 1;
		}
		System.out.flush();
		System.exit(status);
	}

	// ------------------------------------------------------------------------------------ output

	static void line(Object... fields) {
		StringBuilder sb = new StringBuilder();
		for (int i = 0; i < fields.length; i++) {
			if (i > 0) {
				sb.append('\t');
			}
			sb.append(fields[i]);
		}
		System.out.println(sb);
	}

	static String esc(String s) {
		if (s == null) {
			return "<null>";
		}
		return s.replace("\\", "\\\\").replace("\n", "\\n").replace("\t", "\\t");
	}

	static String rgb(Color color) {
		return String.format(Locale.ROOT, "#%02x%02x%02x", color.getRed(), color.getGreen(), color.getBlue());
	}

	static Object field(Object o, String name) throws Exception {
		Class<?> c = o.getClass();
		while (c != null) {
			try {
				Field f = c.getDeclaredField(name);
				f.setAccessible(true);
				return f.get(o);
			} catch (NoSuchFieldException e) {
				c = c.getSuperclass();
			}
		}
		throw new NoSuchFieldException(name);
	}

	static String order(JTable table) {
		StringBuilder sb = new StringBuilder();
		for (int i = 0; i < table.getRowCount(); i++) {
			if (i > 0) {
				sb.append(',');
			}
			sb.append(table.convertRowIndexToModel(i));
		}
		return sb.toString();
	}

	// ------------------------------------------------------------------------------------ the table

	/** The model's columns, from the table of a new design. */
	static void columns() throws Exception {
		Application.setInjector(designInjector);
		DeterministicUuids.reseed("columns");
		SimulationPanel panel = new SimulationPanel(null, OpenRocketDocumentFactory.createNewRocket());
		ColumnTableModel model = (ColumnTableModel) field(panel, "simulationTableModel");
		if (model.getColumnCount() != COLUMN_IDS.length) {
			throw new IllegalStateException("the table has " + model.getColumnCount() + " columns");
		}
		for (int c = 0; c < model.getColumnCount(); c++) {
			line("H", c, COLUMN_IDS[c], esc(model.getColumnName(c)), esc(model.getColumn(c).getToolTip()),
					esc(model.getColumn(c).getUnits().getDefaultUnit().getUnit()));
		}
	}

	/** Loads the design @p name of the kind @p kind from @p file and prints its table. */
	static void design(String name, String kind, File file) throws Exception {
		Application.setInjector(kind.equals("testrocket") ? testRocketInjector : designInjector);
		DeterministicUuids.reseed(name);
		OpenRocketDocument doc = new GeneralRocketLoader(file).load();
		table(name, kind, file.getName(), doc);
	}

	static void table(String name, String kind, String file, OpenRocketDocument doc) throws Exception {
		line("D", name, kind, esc(file));
		SimulationPanel panel = new SimulationPanel(null, doc);
		ColumnTableModel model = (ColumnTableModel) field(panel, "simulationTableModel");
		JTable table = (JTable) field(panel, "simulationTable");
		line("V", field(panel, "hasValidConfig"), esc(doc.getNextSimulationName()), doc.getSimulationCount());
		TableColumnModel cm = table.getColumnModel();
		for (int c = 0; c < cm.getColumnCount(); c++) {
			line("HV", c, cm.getColumn(c).getIdentifier(), cm.getColumn(c).getPreferredWidth(),
					cm.getColumn(c).getMinWidth(), cm.getColumn(c).getMaxWidth());
		}
		for (int r = 0; r < model.getRowCount(); r++) {
			row(panel, model, doc, r, false);
		}
		// The orders of a click on each column's header, and of a second click.
		RowSorter<?> sorter = table.getRowSorter();
		for (int c = 0; c < model.getColumnCount(); c++) {
			sorter.setSortKeys(null);
			sorter.toggleSortOrder(c);
			line("S", c, "asc", order(table));
			sorter.toggleSortOrder(c);
			line("S", c, "desc", order(table));
		}
		sorter.setSortKeys(null);
		line("SQ", 1, "asc", warningsOrder(doc, true));
		line("SQ", 1, "desc", warningsOrder(doc, false));
		actions(panel, table, "none", new int[0]);
		if (model.getRowCount() > 0) {
			actions(panel, table, "row0", new int[] {0});
		}
		if (model.getRowCount() > 1) {
			actions(panel, table, "rows01", new int[] {0, 1});
		}
		table.clearSelection();
		if (model.getRowCount() > 0) {
			SimulationTableCSVExport exporter = new SimulationTableCSVExport(doc, table, model);
			line("CSV", esc(exporter.generateCSVData(",", 3, false, false)));
			line("COPY", esc(copyText(panel, table, model, doc)));
			line("CSVE", esc(exporter.generateCSVData(";", 2, true, false)));
		}
		table.clearSelection();
		line("END");
	}

	/** The R, W, WS and C lines of the model row @p r (only R for a @p fresh simulation). */
	static void row(SimulationPanel panel, ColumnTableModel model, OpenRocketDocument doc, int r, boolean fresh)
			throws Exception {
		Method statusTip = SimulationPanel.class.getDeclaredMethod("getSimulationStatusToolTip", Simulation.class,
				boolean.class);
		statusTip.setAccessible(true);
		Simulation sim = doc.getSimulation(r);
		Object status = model.getValueAt(r, 0);
		Icon icon = ((JLabel) status).getIcon();
		String iconName = icon instanceof ImageIcon ii ? ii.getDescription() : String.valueOf(icon);
		line("R", r, sim.getStatus().name(), esc(iconName), esc(status.toString()), esc(sim.getStatusDescription()),
				esc((String) statusTip.invoke(null, sim, true)));
		if (fresh) {
			return;
		}
		Container box = (Container) model.getValueAt(r, 1);
		StringBuilder text = new StringBuilder();
		for (Component child : box.getComponents()) {
			JLabel label = (JLabel) child;
			if (label.getText() != null && !label.getText().isEmpty()) {
				text.append(label.getText());
			}
			if (label.getIcon() != null) {
				Icon ic = label.getIcon();
				text.append('[').append(description(ic)).append(']');
			}
		}
		WarningSet warnings = sim.getSimulatedWarnings();
		int critical = warnings == null ? -1 : warnings.getNrOfCriticalWarnings();
		int normal = warnings == null ? -1 : warnings.getNrOfNormalWarnings();
		int informational = warnings == null ? -1 : warnings.getNrOfInformationalWarnings();
		line("W", r, critical, normal, informational, esc(text.toString()),
				esc(((JComponent) box).getToolTipText()));
		if (warnings != null) {
			for (Warning warning : warnings) {
				line("WS", r, warning.getPriority(), esc(warning.toString()));
			}
		}
		StringBuilder cells = new StringBuilder("C\t" + r);
		for (int c = 2; c < model.getColumnCount(); c++) {
			Object v = model.getValueAt(r, c);
			cells.append('\t').append(v == null ? "<null>" : esc(v.toString()));
		}
		System.out.println(cells);
	}

	/**
	 * The warning level of one of the box's icons, by identity: HIGH, NORMAL or LOW for Icons.WARNING_HIGH,
	 * WARNING_NORMAL and WARNING_LOW (their SVG icons are derived copies that do not keep the description).
	 */
	static String description(Icon icon) {
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

	/** The warnings column's order as QtRocket sorts it (bug 23): see SQ in the class comment. */
	static String warningsOrder(OpenRocketDocument doc, boolean ascending) {
		List<Integer> rows = new ArrayList<>();
		for (int r = 0; r < doc.getSimulationCount(); r++) {
			rows.add(r);
		}
		Comparator<Integer> counts = (a, b) -> {
			WarningSet wa = doc.getSimulation(a).getSimulatedWarnings();
			WarningSet wb = doc.getSimulation(b).getSimulatedWarnings();
			int result;
			if (wa == null || wb == null) {
				// A row without data is a missing value: first ascending, last descending (DefaultRowSorter's
				// null, which it compares before the comparator and reverses with it).
				result = Boolean.compare(wa != null, wb != null);
			} else {
				result = Integer.compare(wa.getNrOfCriticalWarnings(), wb.getNrOfCriticalWarnings());
				if (result == 0) {
					result = Integer.compare(wa.getNrOfNormalWarnings(), wb.getNrOfNormalWarnings());
				}
				if (result == 0) {
					result = Integer.compare(wa.getNrOfInformationalWarnings(), wb.getNrOfInformationalWarnings());
				}
			}
			return ascending ? result : -result;
		};
		// List.sort is stable: equal rows keep the model's order.
		rows.sort(counts);
		StringBuilder sb = new StringBuilder();
		for (int r : rows) {
			if (sb.length() > 0) {
				sb.append(',');
			}
			sb.append(r);
		}
		return sb.toString();
	}

	static void actions(SimulationPanel panel, JTable table, String what, int[] rows) throws Exception {
		table.clearSelection();
		for (int r : rows) {
			table.addRowSelectionInterval(r, r);
		}
		panel.updateActions();
		StringBuilder sb = new StringBuilder("A\t" + what);
		for (String a : ACTIONS) {
			Action action = (Action) field(panel, a);
			sb.append('\t').append(a.replace("Action", "")).append('=').append(action.isEnabled() ? 1 : 0);
		}
		System.out.println(sb);
	}

	/** The Copy text of every row of the table, as SimulationPanel.copySimulationValues("\t") makes it. */
	static String copyText(SimulationPanel panel, JTable table, ColumnTableModel model, OpenRocketDocument doc)
			throws Exception {
		table.clearSelection();
		table.addRowSelectionInterval(0, model.getRowCount() - 1);
		SimulationTableCSVExport exporter = new SimulationTableCSVExport(doc, table, model);
		String copy = exporter.generateCSVData("\t", 6, false, true);
		Method addStatus = SimulationPanel.class.getDeclaredMethod("addStatusColumn", String.class, int[].class,
				String.class);
		addStatus.setAccessible(true);
		return (String) addStatus.invoke(panel, copy, table.getSelectedRows(), "\t");
	}

	// ------------------------------------------------------------------------------------ the scenarios

	static String statuses(OpenRocketDocument doc) {
		StringBuilder sb = new StringBuilder();
		for (Simulation s : doc.getSimulations()) {
			if (sb.length() > 0) {
				sb.append(' ');
			}
			sb.append(s.getStatus().name());
		}
		return sb.toString();
	}

	static void step(OpenRocketDocument doc, String what) {
		line("ST", esc(what), statuses(doc), esc(doc.getUndoDescription()), esc(doc.getRedoDescription()),
				doc.isSaved());
	}

	static OpenRocketDocument scenario(String name, File file) throws Exception {
		Application.setInjector(designInjector);
		DeterministicUuids.reseed(name);
		OpenRocketDocument doc = new GeneralRocketLoader(file).load();
		line("SCENARIO", name, esc(file.getName()));
		for (int i = 0; i < doc.getSimulationCount(); i++) {
			Simulation s = doc.getSimulation(i);
			line("SIM", i, esc(s.getName()), s.getId().key.toString().substring(0, 8),
					esc(descriptor.format(doc.getRocket(), s.getId())));
		}
		step(doc, "loaded");
		return doc;
	}

	/** The rows of @p doc after a scenario: R (W, WS, C) of each, and the status and warnings columns' orders. */
	static void scenarioRows(OpenRocketDocument doc, Set<Simulation> fresh) throws Exception {
		SimulationPanel panel = new SimulationPanel(null, doc);
		ColumnTableModel model = (ColumnTableModel) field(panel, "simulationTableModel");
		JTable table = (JTable) field(panel, "simulationTable");
		for (int r = 0; r < model.getRowCount(); r++) {
			boolean isFresh = fresh.contains(doc.getSimulation(r));
			if (isFresh) {
				line("FRESH", r);
			}
			row(panel, model, doc, r, isFresh);
		}
		RowSorter<?> sorter = table.getRowSorter();
		for (int c = 0; c <= 1; c++) {
			sorter.setSortKeys(null);
			sorter.toggleSortOrder(c);
			line("S", c, "asc", order(table));
			sorter.toggleSortOrder(c);
			line("S", c, "desc", order(table));
		}
		sorter.setSortKeys(null);
		// The warnings column's orders: rows without data beside rows with data in the first scenario.
		line("SQ", 1, "asc", warningsOrder(doc, true));
		line("SQ", 1, "desc", warningsOrder(doc, false));
		line("END");
	}

	static <T> T first(Rocket rocket, Class<T> type) {
		for (RocketComponent c : rocket) {
			if (type.isInstance(c)) {
				return type.cast(c);
			}
		}
		throw new IllegalStateException("no " + type.getSimpleName());
	}

	static MotorMount firstMountWithMotor(Rocket rocket) {
		for (RocketComponent c : rocket) {
			if (c instanceof MotorMount m && m.isMotorMount() && m.hasMotor()) {
				return m;
			}
		}
		throw new IllegalStateException("no motor mount with a motor");
	}

	/**
	 * Every status but EXTERNAL on "A simple model rocket" (the scout's first scenario, with its runs moved to
	 * "Simulation 1", the only simulation of the design QtRocket's tests may run, plan 11.6): an options change,
	 * a configuration's rename and selection, a motor change for one configuration, a body tube change with its
	 * undo and redo, a colour, a run, a time step, a new simulation with its undo and redo, and a run whose motor
	 * never ignites.
	 */
	static void statusScenario(File file) throws Exception {
		OpenRocketDocument doc = scenario("status_a_simple_model_rocket", file);
		Rocket rocket = doc.getRocket();
		Simulation sim0 = doc.getSimulation(0);
		double rodLength = sim0.getOptions().getLaunchRodLength();
		sim0.getOptions().setLaunchRodLength(rodLength);
		step(doc, "sim0 rod length set to the same value");
		sim0.getOptions().setLaunchRodLength(1.5);
		step(doc, "sim0 rod length 1.5");
		sim0.getOptions().setLaunchRodLength(rodLength);
		step(doc, "sim0 rod length back");
		FlightConfigurationId id1 = doc.getSimulation(1).getId();
		rocket.getFlightConfiguration(id1).setName("Renamed {motors}");
		rocket.fireComponentChangeEvent(ComponentChangeEvent.NONFUNCTIONAL_CHANGE, new FlightConfigurationId[] {id1});
		step(doc, "rename sim1's configuration");
		rocket.setSelectedConfiguration(doc.getSimulation(2).getId());
		step(doc, "select sim2's configuration");
		MotorMount mount = firstMountWithMotor(rocket);
		mount.getMotorConfig(id1).setEjectionDelay(mount.getMotorConfig(id1).getEjectionDelay() + 1);
		rocket.fireComponentChangeEvent(ComponentChangeEvent.MOTOR_CHANGE, new FlightConfigurationId[] {id1});
		step(doc, "sim1's ejection delay +1 (MOTOR_CHANGE for its id)");
		doc.addUndoPosition("Modify body tube");
		BodyTube tube = first(rocket, BodyTube.class);
		tube.setLength(tube.getLength() + 0.01);
		step(doc, "body tube +1 cm");
		doc.undo();
		step(doc, "undo");
		doc.redo();
		step(doc, "redo");
		first(rocket, BodyTube.class).setColor(new ORColor(10, 20, 30));
		step(doc, "body tube colour (GRAPHIC)");
		Set<Simulation> fresh = java.util.Collections.newSetFromMap(new java.util.IdentityHashMap<>());
		Simulation run = doc.getSimulation(0);
		run.simulate();
		fresh.add(run);
		step(doc, "sim0 simulated");
		doc.getSimulation(0).getOptions().setTimeStep(0.02);
		step(doc, "sim0 time step 0.02");
		Simulation added = new Simulation(doc, rocket);
		added.setName(doc.getNextSimulationName());
		doc.addUndoPosition("Add " + added.getName());
		doc.addSimulation(added);
		step(doc, "new simulation " + added.getName() + " config=" + descriptor.format(rocket, added.getId()));
		doc.undo();
		step(doc, "undo add");
		doc.redo();
		step(doc, "redo add");
		FlightConfigurationId id0 = doc.getSimulation(0).getId();
		MotorMount mount0 = firstMountWithMotor(rocket);
		mount0.getMotorConfig(id0).setIgnitionEvent(IgnitionEvent.NEVER);
		rocket.fireComponentChangeEvent(ComponentChangeEvent.MOTOR_CHANGE, new FlightConfigurationId[] {id0});
		step(doc, "sim0's motor never ignites (MOTOR_CHANGE for its id)");
		Simulation aborted = doc.getSimulation(0);
		try {
			aborted.simulate();
		} catch (Exception e) {
			line("SIMULATE-EXCEPTION", esc(e.toString()));
		}
		fresh.add(aborted);
		step(doc, "sim0 simulated");
		scenarioRows(doc, fresh);
	}

	/** CANT_RUN and OUTDATED on "Two stage high power rocket" (the scout's second scenario, without its run). */
	static void cantRunScenario(File file) throws Exception {
		OpenRocketDocument doc = scenario("status_two_stage_high_power_rocket", file);
		Rocket rocket = doc.getRocket();
		FlightConfigurationId id0 = doc.getSimulation(0).getId();
		for (RocketComponent c : rocket) {
			if (c instanceof MotorMount m && m.isMotorMount()) {
				m.setMotorConfig(null, id0);
			}
		}
		rocket.fireComponentChangeEvent(ComponentChangeEvent.MOTOR_CHANGE, new FlightConfigurationId[] {id0});
		step(doc, "motors of sim0's configuration removed");
		doc.getSimulation(0).getOptions().setLaunchRodLength(2.0);
		step(doc, "sim0 rod length 2.0");
		FlightConfigurationId id1 = doc.getSimulation(1).getId();
		for (RocketComponent c : rocket) {
			if (c instanceof MotorMount m && m.isMotorMount() && m.getMotorConfig(id1) != null
					&& !m.getMotorConfig(id1).isEmpty()) {
				m.getMotorConfig(id1).setIgnitionEvent(IgnitionEvent.NEVER);
			}
		}
		rocket.fireComponentChangeEvent(ComponentChangeEvent.MOTOR_CHANGE, new FlightConfigurationId[] {id1});
		step(doc, "sim1's motors never ignite");
		scenarioRows(doc, Set.of());
	}

	/**
	 * The Copy of every row of "A simple model rocket" with a new simulation (no data) inserted as the second row
	 * and "Simulation 3 - too short delay" made OUTDATED (its launch rod length): OpenRocket's exporter leaves the
	 * row without data out, and its addStatusColumn() then gives each later line the status of the row before it
	 * (bug 25). COPYQ pairs each line with its own simulation, as QtRocket does.
	 */
	static void pairing(File file) throws Exception {
		Application.setInjector(designInjector);
		DeterministicUuids.reseed("copy_status_pairing");
		OpenRocketDocument doc = new GeneralRocketLoader(file).load();
		line("PAIRING", "copy_status_pairing", esc(file.getName()));
		Simulation added = new Simulation(doc, doc.getRocket());
		added.setName(doc.getNextSimulationName());
		doc.addSimulation(added, 1);
		Simulation outdated = doc.getSimulation(3);
		outdated.getOptions().setLaunchRodLength(outdated.getOptions().getLaunchRodLength() + 0.5);
		step(doc, "a new simulation at row 1, row 3's rod length +0.5 m");
		SimulationPanel panel = new SimulationPanel(null, doc);
		ColumnTableModel model = (ColumnTableModel) field(panel, "simulationTableModel");
		JTable table = (JTable) field(panel, "simulationTable");
		line("COPY", esc(copyText(panel, table, model, doc)));
		// QtRocket's: the exporter's lines are those of the selected rows with summary data, in the view's order.
		SimulationTableCSVExport exporter = new SimulationTableCSVExport(doc, table, model);
		String[] lines = exporter.generateCSVData("\t", 6, false, true).split("\n");
		StringBuilder result = new StringBuilder("Status\t" + lines[0]);
		int next = 1;
		for (int viewRow : table.getSelectedRows()) {
			Simulation sim = doc.getSimulation(table.convertRowIndexToModel(viewRow));
			if (!sim.hasSummaryData()) {
				continue;
			}
			result.append('\n').append(info.openrocket.core.util.StringUtils.removeHTMLTags(sim.getStatusDescription()))
					.append('\t').append(lines[next++]);
		}
		if (next != lines.length) {
			throw new IllegalStateException("the exporter left out a row with data");
		}
		line("COPYQ", esc(result.toString()));
		table.clearSelection();
		line("END");
	}

	// ------------------------------------------------------------------------------------ set-up

	/** The .orc files below @p directory, depth first, each directory in name order (DirectoryIterator's walk). */
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
		DeterministicUuids.reseed("simulation-table");
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
		// configurations' names (Motor.getMotorName()); a SwingPreferences writes it to the store every other
		// instance reads. The simulations are deleted without the confirmation (never asked here).
		Application.getPreferences().putBoolean(ApplicationPreferences.MOTOR_NAME_COLUMN, false);
		if (Application.getPreferences().getMotorNameColumn()) {
			throw new IllegalStateException("MotorNameColumn is not false");
		}
	}
}
