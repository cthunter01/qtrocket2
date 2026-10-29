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
import info.openrocket.core.file.GeneralRocketLoader;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.preset.ComponentPreset;
import info.openrocket.core.preset.xml.OpenRocketComponentLoader;
import info.openrocket.core.rocketcomponent.AxialStage;
import info.openrocket.core.rocketcomponent.FlightConfiguration;
import info.openrocket.core.rocketcomponent.FlightConfigurationId;
import info.openrocket.core.rocketcomponent.MotorMount;
import info.openrocket.core.rocketcomponent.RecoveryDevice;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.startup.Application;
import info.openrocket.core.util.Pair;
import info.openrocket.core.util.TestRockets;
import info.openrocket.swing.gui.main.BasicFrame;
import info.openrocket.swing.gui.main.DocumentSelectionModel;
import info.openrocket.swing.gui.main.FlightConfigurationPanel;
import info.openrocket.swing.gui.main.flightconfigpanel.FlightConfigurablePanel;
import info.openrocket.swing.gui.main.flightconfigpanel.MotorConfigurationPanel;
import info.openrocket.swing.gui.main.flightconfigpanel.RecoveryConfigurationPanel;
import info.openrocket.swing.gui.main.flightconfigpanel.SeparationConfigurationPanel;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.util.GUIUtil;
import info.openrocket.swing.gui.util.SwingPreferences;
import sun.misc.Unsafe;

import java.awt.Color;
import java.awt.Component;
import java.awt.Container;
import java.awt.Font;
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
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.TreeSet;
import javax.swing.DefaultListSelectionModel;
import javax.swing.JButton;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JTabbedPane;
import javax.swing.JTable;
import javax.swing.SwingUtilities;
import javax.swing.table.TableModel;
import javax.swing.tree.DefaultTreeSelectionModel;

/**
 * OpenRocket's "Motors &amp; Configuration" tab as it opens for each design of QtRocket's exit suite
 * OpenDesignsExitGolden (tests/gui/exit/open_designs_exit_tests.cpp, plan section 10.5: the 16 examples, the 49
 * re-saves of the golden harness and the design OpenRocket's GUI built from scratch, as listed by designs.py): what
 * the suite's TheConfigurationTabShowsOpenRocketsTables compares the tab of QtRocket's window with. A generalisation
 * of the flight-config family's FlightConfigurationTablesProbe (which runs the panels' table models and renderers
 * alone over the examples and the test rockets' makers) to the LOADED designs, through OpenRocket's real
 * FlightConfigurationPanel, as the M3 tests-exit scout's ConfigTablesProbe built it, and as the flight-config
 * family's FlightConfigurationOperationsProbe does: the panel of a BasicFrame allocated without its constructor (its
 * document, rocket and selection model set), never shown. Its MotorConfigurationPanel makes a MotorChooserDialog, a
 * JDialog, which a headless AWT refuses, so the probe runs under xvfb-run; no window is shown.
 * <p>
 * Each design is loaded as the suite loads it (DesignFileEnvironment::Presets::EXAMPLES): the motors of the bundled
 * database (-Dprobe.motors), but for the thirteen test rockets' re-saves, whose motors are the seven synthetic ones of
 * TestRockets (made by its private generators, in the order of QtRocket's testRocketMotors(), as InteropCheck's
 * test-rocket injector makes them), and the six component presets of the example designs (taken by their digests
 * from the bundled .orc files below the directory given as the second argument, as GoldenDumper's legacy injector
 * takes them). The preferences are SwingPreferences (the panels cast to it) in generate.sh's fresh store, with the one
 * value the suite's windows have otherwise than its defaults that the tab reads: MotorNameColumn false (the suite's
 * DesignFileEnvironment shows a motor by its common name, as ServicesForTesting answers false for every flag); FlatLaf
 * Light.
 * <p>
 * The tab is recorded as BasicFrame has it when the user chooses it: made with the window (its tables select the
 * rocket's selected configuration in their last column), the window's design selection the top stage (BasicFrame's
 * constructor, after the panel), then FlightConfigurationPanel.takeTheSpotlight() (BasicFrame.stateChanged() for
 * the tab), which makes the components of the Motors table's selected columns the design's selection.
 * <p>
 * Every design's UUIDs are seeded (DeterministicUuids, reseeded from the design's name); each design is also loaded
 * a first time from the key "&lt;name&gt;#drawn", to tell a configuration id the loader draws (the two loads differ)
 * from one the file stores.
 * <p>
 * Output: one line each, its fields separated by tabs. A style is font/colour, as FlightConfigurationTablesProbe
 * prints it.
 * <ul>
 * <li>COLORS TEXT TEXT_DIM (as #rrggbb, FlatLaf Light's)</li>
 * <li>D name kind (a design of designs.py)</li>
 * <li>S selectedRow stageCount (selectedRow -1: the rocket's selected configuration is the default)</li>
 * <li>C row key drawn rawName isNameOverridden</li>
 * <li>TAB index title enabled (the three sub-tabs); TABSEL index (the selected one)</li>
 * <li>B panel button enabled (the buttons by their fields: the tab's four, then each sub-panel's)</li>
 * <li>HELP shown (whether the Motors panel shows its help text instead of its table: no acting motor mount)</li>
 * <li>MH|RH|TH col header; M0 row text style; M row col motorText motorStyle ignitionText ignitionStyle
 * hasIgnitionOverride containsId; R|T row col text style isDefault containsId (the three tables, as the
 * FlightConfigurationTablesProbe prints them, each cell rendered by the table's own renderer, not selected and
 * without focus)</li>
 * <li>MS|RS|TS rows cols (each table's selected rows and columns, "[0, 1]")</li>
 * <li>P row isMotorMount name (the rows of the Motors panel's motor mount table)</li>
 * <li>DS names (the design's selected components once the tab took the spotlight, "[A, B]")</li>
 * <li>AFTER modIdChanged saved selectionChanged (what opening the tab changed: never anything)</li>
 * </ul>
 * usage: OpenDesignsTabProbe DESIGNS COMPONENTS
 */
public class OpenDesignsTabProbe {

	/** The digests of the six component presets of the example designs (tests/core/rocket/preset/ExamplePresets.h). */
	static final Set<String> EXAMPLE_PRESET_DIGESTS = Set.of("e9c3e97d32e96762f65e051d74cb9fd7",
			"b595c8a31baaf325291d5c957f3940f8", "9a96b8806749e790db29bacfbc70eb1e", "18363dec5642206a109389ef48efcb32",
			"a59dec8e4034a2fee5955dbf4ff07f1c", "6f564a198c80de3af437608827fa1bc1");

	/** The generators of the motors of TestRockets, in the order of QtRocket's testRocketMotors(). */
	static final List<String> TEST_ROCKET_MOTORS = List.of("generateMotor_A8_18mm", "generateMotor_A10_13mm",
			"generateMotor_B4_18mm", "generateMotor_C6_18mm", "generateMotor_D21_18mm", "generateMotor_M1350_75mm",
			"generateMotor_G77_29mm");

	static Unsafe unsafe;
	static Injector designInjector;
	static Injector testRocketInjector;
	static Color textColor;
	static Color dimColor;

	/** A BasicFrame for the panel, allocated without its constructor. */
	public static class FakeFrame extends BasicFrame {
		public FakeFrame() {
			super(null);
		}
	}

	public static void main(String[] args) throws Exception {
		Field f = Unsafe.class.getDeclaredField("theUnsafe");
		f.setAccessible(true);
		unsafe = (Unsafe) f.get(null);
		int status = 0;
		try {
			bootstrap(new File(args[1]));
			SwingUtilities.invokeAndWait(() -> {
				GUIUtil.setUITheme(UITheme.Themes.LIGHT);
				// FlightConfigurablePanel reads its two colours from the theme when its class is initialised.
				FlightConfigurablePanel.updateColors();
				textColor = UITheme.getColor(UITheme.Keys.TEXT);
				dimColor = UITheme.getColor(UITheme.Keys.TEXT_DIM);
			});
			line("COLORS", rgb(textColor), rgb(dimColor));
			for (String design : Files.readAllLines(Path.of(args[0]), StandardCharsets.UTF_8)) {
				String[] fields = design.split("\t");
				if (fields.length != 3) {
					throw new IllegalStateException("not a design: " + design);
				}
				probe(fields[0], fields[1], new File(fields[2]));
			}
		} catch (Throwable e) {
			e.printStackTrace();
			status = 1;
		}
		System.out.flush();
		System.exit(status);
	}

	/** Loads the design @p name of the kind @p kind from @p file twice and prints its tab. */
	static void probe(String name, String kind, File file) throws Exception {
		Application.setInjector(kind.equals("testrocket") ? testRocketInjector : designInjector);
		DeterministicUuids.reseed(name + "#drawn");
		OpenRocketDocument other = new GeneralRocketLoader(file).load();
		DeterministicUuids.reseed(name);
		OpenRocketDocument doc = new GeneralRocketLoader(file).load();
		SwingUtilities.invokeAndWait(() -> {
			try {
				dump(name, kind, doc, other.getRocket());
			} catch (Exception e) {
				throw new RuntimeException(e);
			} finally {
				for (Window window : Window.getWindows()) {
					window.dispose();
				}
			}
		});
	}

	static void dump(String name, String kind, OpenRocketDocument doc, Rocket other) throws Exception {
		Rocket rocket = doc.getRocket();
		line("D", name, kind);
		FakeFrame frame = (FakeFrame) unsafe.allocateInstance(FakeFrame.class);
		set(BasicFrame.class, frame, "document", doc);
		set(BasicFrame.class, frame, "rocket", rocket);
		DocumentSelectionModel selection = new DocumentSelectionModel(doc);
		selection.attachComponentTreeSelectionModel(new DefaultTreeSelectionModel());
		selection.attachSimulationListSelectionModel(new DefaultListSelectionModel());
		set(BasicFrame.class, frame, "selectionModel", selection);

		Object modId = rocket.getModID();
		boolean saved = doc.isSaved();
		FlightConfigurationId selected = rocket.getSelectedConfiguration().getId();
		FlightConfigurationPanel panel = new FlightConfigurationPanel(frame, doc);
		// BasicFrame's constructor, after its panels: the top stage selected in the design.
		if (rocket.getChildCount() > 0) {
			selection.setSelectedComponent(rocket.getChild(0));
		}

		List<FlightConfigurationId> ids = rocket.getIds();
		line("S", ids.indexOf(rocket.getSelectedConfiguration().getId()), rocket.getStageCount());
		for (int row = 0; row < ids.size(); row++) {
			FlightConfigurationId id = ids.get(row);
			FlightConfiguration fc = rocket.getFlightConfiguration(id);
			boolean drawn = row >= other.getConfigurationCount() || !other.getId(row).equals(id);
			line("C", row, id.key, drawn, fc.getNameRaw(), fc.isNameOverridden());
		}

		JTabbedPane tabs = (JTabbedPane) get(FlightConfigurationPanel.class, panel, "tabs");
		for (int i = 0; i < tabs.getTabCount(); i++) {
			line("TAB", i, tabs.getTitleAt(i), tabs.isEnabledAt(i));
		}
		line("TABSEL", tabs.getSelectedIndex());

		MotorConfigurationPanel motors = (MotorConfigurationPanel) get(FlightConfigurationPanel.class, panel,
				"motorConfigurationPanel");
		RecoveryConfigurationPanel recovery = (RecoveryConfigurationPanel) get(FlightConfigurationPanel.class, panel,
				"recoveryConfigurationPanel");
		SeparationConfigurationPanel stages = (SeparationConfigurationPanel) get(FlightConfigurationPanel.class, panel,
				"separationConfigurationPanel");
		buttons("tab", FlightConfigurationPanel.class, panel, "newConfButton", "renameConfButton", "removeConfButton",
				"duplicateConfButton");
		buttons("motors", MotorConfigurationPanel.class, motors, "selectMotorButton", "deleteMotorButton",
				"selectIgnitionButton", "resetIgnitionButton");
		buttons("recovery", RecoveryConfigurationPanel.class, recovery, "selectDeploymentButton",
				"resetDeploymentButton");
		buttons("stages", SeparationConfigurationPanel.class, stages, "selectSeparationButton",
				"resetDeploymentButton");
		line("HELP", helpShown(motors));

		motorTable(table(motors));
		valueTable("R", table(recovery), RecoveryDevice.class);
		valueTable("T", table(stages), AxialStage.class);

		TableModel mounts = mountModel(motors);
		for (int row = 0; row < mounts.getRowCount(); row++) {
			line("P", row, mounts.getValueAt(row, 0), mounts.getValueAt(row, 1));
		}

		// BasicFrame.stateChanged() when the tab is chosen.
		panel.takeTheSpotlight();
		List<String> names = new ArrayList<>();
		for (RocketComponent component : selection.getSelectedComponents()) {
			names.add(component.getName());
		}
		line("DS", "[" + String.join(", ", names) + "]");
		line("AFTER", !rocket.getModID().equals(modId), doc.isSaved() != saved,
				!rocket.getSelectedConfiguration().getId().equals(selected));
		rocket.removeChangeListener(panel);
	}

	/** The enabled states of the buttons @p fields of @p panel (an instance of @p owner, which declares them). */
	static void buttons(String tag, Class<?> owner, Object panel, String... fields) {
		for (String field : fields) {
			line("B", tag, field, ((JButton) get(owner, panel, field)).isEnabled());
		}
	}

	/** Whether the Motors panel's card layout shows the help text (the card that is a JLabel). */
	static boolean helpShown(MotorConfigurationPanel motors) {
		JPanel cards = (JPanel) get(MotorConfigurationPanel.class, motors, "cards");
		Boolean shown = null;
		for (Component card : cards.getComponents()) {
			if (card.isVisible()) {
				if (shown != null) {
					throw new IllegalStateException("two cards shown");
				}
				shown = card instanceof JLabel;
			}
		}
		if (shown == null) {
			throw new IllegalStateException("no card shown");
		}
		return shown;
	}

	static void motorTable(JTable table) {
		headers("MH", table);
		for (int row = 0; row < table.getRowCount(); row++) {
			JLabel label = render(table, row, 0);
			line("M0", row, label.getText(), style(label));
			for (int col = 1; col < table.getColumnCount(); col++) {
				@SuppressWarnings("unchecked")
				Pair<FlightConfigurationId, MotorMount> cell = (Pair<FlightConfigurationId, MotorMount>) table
						.getModel().getValueAt(row, col);
				JLabel outer = render(table, row, col);
				List<JLabel> parts = new ArrayList<>();
				for (Component c : outer.getComponents()) {
					if (c instanceof JLabel l) {
						parts.add(l);
					}
				}
				if (parts.size() != 2 || !outer.getText().isEmpty()) {
					throw new IllegalStateException("motor cell " + row + " " + col + ": " + parts.size()
							+ " labels, text '" + outer.getText() + "'");
				}
				MotorMount mount = cell.getV();
				FlightConfigurationId id = cell.getU();
				line("M", row, col, parts.get(0).getText(), style(parts.get(0)), parts.get(1).getText(),
						style(parts.get(1)), mount.getMotorConfig(id).hasIgnitionOverride(),
						mount.getMotorConfigurationSet().containsId(id));
			}
		}
		selectionLine("MS", table);
	}

	static void valueTable(String tag, JTable table, Class<?> componentClass) {
		headers(tag + "H", table);
		for (int row = 0; row < table.getRowCount(); row++) {
			for (int col = 1; col < table.getColumnCount(); col++) {
				Pair<?, ?> cell = (Pair<?, ?>) table.getModel().getValueAt(row, col);
				JLabel label = render(table, row, col);
				FlightConfigurationId id = (FlightConfigurationId) cell.getU();
				boolean isDefault;
				boolean containsId;
				if (componentClass == RecoveryDevice.class) {
					RecoveryDevice device = (RecoveryDevice) cell.getV();
					isDefault = device.getDeploymentConfigurations().isDefault(id);
					containsId = device.getDeploymentConfigurations().containsId(id);
				} else {
					AxialStage stage = (AxialStage) cell.getV();
					isDefault = stage.getSeparationConfigurations().isDefault(id);
					containsId = stage.getSeparationConfigurations().containsId(id);
				}
				line(tag, row, col, label.getText(), style(label), isDefault, containsId);
			}
		}
		selectionLine(tag + "S", table);
	}

	/** The cell (@p row, @p col) of @p table as its renderer draws it, not selected and without focus. */
	static JLabel render(JTable table, int row, int col) {
		return (JLabel) table.getCellRenderer(row, col).getTableCellRendererComponent(table,
				table.getValueAt(row, col), false, false, row, col);
	}

	static void headers(String tag, JTable table) {
		for (int col = 0; col < table.getColumnCount(); col++) {
			line(tag, col, table.getColumnName(col));
		}
	}

	static void selectionLine(String tag, JTable table) {
		line(tag, list(table.getSelectedRows()), list(table.getSelectedColumns()));
	}

	static String list(int[] values) {
		List<String> texts = new ArrayList<>();
		for (int value : values) {
			texts.add(Integer.toString(value));
		}
		return "[" + String.join(", ", texts) + "]";
	}

	static JTable table(FlightConfigurablePanel<?> panel) {
		return (JTable) get(FlightConfigurablePanel.class, panel, "table");
	}

	/** The model of the motor mount table of the Motors panel (MotorMountConfigurationPanel's JTable). */
	static TableModel mountModel(MotorConfigurationPanel motors) {
		List<JTable> tables = new ArrayList<>();
		collectTables(motors, tables);
		TableModel found = null;
		for (JTable table : tables) {
			if (table.getModel().getClass().getSimpleName().equals("MotorMountTableModel")) {
				if (found != null) {
					throw new IllegalStateException("two motor mount tables");
				}
				found = table.getModel();
			}
		}
		if (found == null) {
			throw new IllegalStateException("no motor mount table");
		}
		return found;
	}

	static void collectTables(Component component, List<JTable> tables) {
		if (component instanceof JTable table) {
			tables.add(table);
		}
		if (component instanceof Container container) {
			for (Component child : container.getComponents()) {
				collectTables(child, tables);
			}
		}
	}

	static String style(JLabel label) {
		Font font = label.getFont();
		String face = font.isItalic() ? (font.isBold() ? "bolditalic" : "italic") : (font.isBold() ? "bold" : "plain");
		Color color = label.getForeground();
		String colour = color.equals(textColor) ? "text" : color.equals(dimColor) ? "dim" : rgb(color);
		return face + "/" + colour;
	}

	static String rgb(Color c) {
		return String.format("#%02x%02x%02x%s", c.getRed(), c.getGreen(), c.getBlue(),
				c.getAlpha() == 255 ? "" : String.format("@%d", c.getAlpha()));
	}

	/** Prints @p fields separated by tabs (texts hold " | " and spaces, never a tab). */
	static void line(Object... fields) {
		StringBuilder text = new StringBuilder();
		for (Object field : fields) {
			String value = String.valueOf(field);
			if (value.indexOf('\t') >= 0 || value.indexOf('\n') >= 0) {
				throw new IllegalStateException("a tab or a newline in " + value);
			}
			if (text.length() > 0) {
				text.append('\t');
			}
			text.append(value);
		}
		System.out.println(text);
	}

	static void set(Class<?> owner, Object target, String name, Object value) {
		try {
			Field field = owner.getDeclaredField(name);
			field.setAccessible(true);
			field.set(target, value);
		} catch (ReflectiveOperationException e) {
			throw new RuntimeException(e);
		}
	}

	static Object get(Class<?> owner, Object target, String name) {
		try {
			Field field = owner.getDeclaredField(name);
			field.setAccessible(true);
			return field.get(target);
		} catch (ReflectiveOperationException e) {
			throw new RuntimeException(e);
		}
	}

	// ------------------------------------------------------------------ set-up

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

	/**
	 * Adds to @p presets the six presets of the example designs, from the bundled .orc files below @p components
	 * (each once). The .orc reader needs the application's injector (its materials).
	 */
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
		DeterministicUuids.reseed("open-designs-tab");
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
		// The suite's preference store shows a motor by its common name (DesignFileEnvironment), in the motor
		// cells and the configurations' names (Motor.getMotorName()); a SwingPreferences writes it to the store
		// every other instance reads.
		Application.getPreferences().putBoolean(ApplicationPreferences.MOTOR_NAME_COLUMN, false);
		if (Application.getPreferences().getMotorNameColumn()) {
			throw new IllegalStateException("MotorNameColumn is not false");
		}
	}
}
