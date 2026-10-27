package probe;

import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Module;
import com.google.inject.util.Modules;
import info.openrocket.core.ServicesForTesting;
import info.openrocket.core.database.ComponentPresetDao;
import info.openrocket.core.database.ComponentPresetDatabase;
import info.openrocket.core.database.motor.MotorDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSQLiteDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSetDatabase;
import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.file.GeneralRocketLoader;
import info.openrocket.core.formatting.RocketDescriptor;
import info.openrocket.core.l10n.Translator;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preferences.ApplicationPreferences;
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
import info.openrocket.swing.gui.main.flightconfigpanel.FlightConfigurablePanel;
import info.openrocket.swing.gui.main.flightconfigpanel.FlightConfigurableTableModel;
import info.openrocket.swing.gui.main.flightconfigpanel.MotorConfigurationPanel;
import info.openrocket.swing.gui.main.flightconfigpanel.RecoveryConfigurationPanel;
import info.openrocket.swing.gui.main.flightconfigpanel.SeparationConfigurationPanel;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.util.GUIUtil;
import info.openrocket.swing.gui.util.SwingPreferences;

import java.awt.Color;
import java.awt.Component;
import java.awt.Font;
import java.io.File;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JTable;
import javax.swing.SwingUtilities;
import javax.swing.table.AbstractTableModel;
import javax.swing.table.TableCellRenderer;
import javax.swing.table.TableModel;

/**
 * The tables of OpenRocket's "Motors &amp; Configuration" tab as OpenRocket's own code makes them, headless with
 * SwingPreferences and FlatLaf Light, over the 16 examples and the 13 test rockets: what QtRocket's
 * FlightConfigurableTableModel, FlightConfigurationCellText and MotorMountTableModel (part F2 of Milestone 3,
 * run 2) must show. A generalisation of the M3 config-tab scout's NamesProbe, which copied the renderers' code:
 * this probe runs the real ones.
 * <p>
 * Each table is OpenRocket's own model: the FlightConfigurableTableModel subclass each panel makes in its
 * initializeTable() (MotorConfigurationPanel's of the acting mounts, RecoveryConfigurationPanel's of every
 * recovery device, SeparationConfigurationPanel's of the stages of stage number &gt; 0), taken from the panel's
 * compiled anonymous class, and MotorMountTableModel (package-private). Each cell is rendered by the panel's own
 * cell renderer (MotorTableCellRenderer, RecoveryTableCellRenderer, SeparationTableCellRenderer: inner classes
 * of the panels, whose outer instance is allocated without its constructor, a JPanel the renderers use for its
 * rocket and descriptor only) into a JTable over the model, not selected and without focus, as the table
 * paints it.
 * <p>
 * Every design's UUIDs are seeded (DeterministicUuids, reseeded from the scenario's name); a test rocket is also
 * built a second time from another seed, to tell the configuration ids its maker draws (they differ between the
 * two builds) from the ones it sets.
 * <p>
 * Output: one line each, its fields separated by tabs (texts hold " | " and spaces). A style is font/colour:
 * the font plain, italic, bold or bolditalic, the colour text (UITheme's TEXT), dim (TEXT_DIM) or #rrggbb.
 * <ul>
 * <li>COLORS TEXT TEXT_DIM (as #rrggbb, FlatLaf Light's)</li>
 * <li>D source:design</li>
 * <li>S selectedRow stageCount (selectedRow -1: the rocket's selected configuration is the default)</li>
 * <li>C row key drawn rawName isNameOverridden (drawn: the maker draws the id, which another seed changes)</li>
 * <li>MH|RH|TH col header (the motor, recovery and stage tables' getColumnName(), column 0 included)</li>
 * <li>M0 row text style (column 0 of the motor table)</li>
 * <li>M row col motorText motorStyle ignitionText ignitionStyle hasIgnitionOverride containsId (the two labels of
 * a motor cell; hasIgnitionOverride of getMotorConfig(id), containsId of the mount's set)</li>
 * <li>R row col text style isDefault containsId (the recovery table's cells of column &gt; 0)</li>
 * <li>T row col text style isDefault containsId (the stage table's cells of column &gt; 0)</li>
 * <li>P row isMotorMount name (MotorMountTableModel's rows: getValueAt(row, 0) and (row, 1))</li>
 * <li>N raw text (column 0 of the last row, rendered after setName(raw); the raw name is put back)</li>
 * </ul>
 * The recovery and stage tables' column 0 is the motor table's (the base class's renderer code).
 * usage: FlightConfigurationTablesProbe EXAMPLEDIR
 */
public class FlightConfigurationTablesProbe {

	static final String[] MAKERS = {"makeEstesAlphaIII", "makeBeta", "makeSimple2Stage", "makeBigBlue",
			"makeIsoHaisu", "makeFalcon9Heavy", "makeMultiStageEventTestRocket", "makeEndPlateRocket",
			"makeEstesAlphaIIIWithPods", "makeEstesAlphaIIIWithMotorPods", "makeEstesAlphaIIIWithSecondMotor",
			"makeEstesAlphaIIIwithInlinePod", "makeClusterPods"};

	/** The NamesProbe's names: substitutions and plain text, set on the last configuration. */
	static final String[] NAMES = {"{motors}", "{manufacturers}", "{cases}", "{manufacturers motors}",
			"{motors | manufacturers}", "{manufacturers, motors}", "[{motors}] {cases}", "{motorsX}", "{}",
			"{ motors }", "Flight {motors} and {motors}", "{cases | motors | manufacturers}", "plain name", ""};

	static Color textColor;
	static Color dimColor;
	static RocketDescriptor descriptor;

	public static void main(String[] args) throws Exception {
		bootstrap();
		int status = 0;
		try {
			SwingUtilities.invokeAndWait(() -> {
				try {
					run(new File(args[0]));
				} catch (Exception e) {
					throw new RuntimeException(e);
				}
			});
		} catch (Exception e) {
			e.printStackTrace();
			status = 1;
		}
		System.out.flush();
		System.exit(status);
	}

	static void run(File dir) throws Exception {
		GUIUtil.setUITheme(UITheme.Themes.LIGHT);
		// FlightConfigurablePanel reads its two colours from the theme when its class is initialised.
		FlightConfigurablePanel.updateColors();
		textColor = UITheme.getColor(UITheme.Keys.TEXT);
		dimColor = UITheme.getColor(UITheme.Keys.TEXT_DIM);
		descriptor = Application.getInjector().getInstance(RocketDescriptor.class);
		line("COLORS", rgb(textColor), rgb(dimColor));

		String[] examples = dir.list((d, n) -> n.endsWith(".ork"));
		Arrays.sort(examples);
		for (String name : examples) {
			String scenario = "example:" + name;
			DeterministicUuids.reseed(scenario);
			OpenRocketDocument doc = new GeneralRocketLoader(new File(dir, name)).load();
			dump(scenario, doc.getRocket(), null);
		}
		for (String maker : MAKERS) {
			String scenario = "testrocket:" + maker;
			DeterministicUuids.reseed(scenario + "#drawn");
			Rocket other = (Rocket) TestRockets.class.getMethod(maker).invoke(null);
			DeterministicUuids.reseed(scenario);
			Rocket rocket = (Rocket) TestRockets.class.getMethod(maker).invoke(null);
			dump(scenario, rocket, other);
		}
	}

	/** The tables of @p rocket; @p other is the same design built from another seed (null: loaded). */
	static void dump(String scenario, Rocket rocket, Rocket other) throws Exception {
		line("D", scenario);
		List<FlightConfigurationId> ids = rocket.getIds();
		FlightConfigurationId selected = rocket.getSelectedConfiguration().getId();
		line("S", ids.indexOf(selected), rocket.getStageCount());
		for (int row = 0; row < ids.size(); row++) {
			FlightConfigurationId id = ids.get(row);
			FlightConfiguration fc = rocket.getFlightConfiguration(id);
			boolean drawn = other != null && !other.getId(row).equals(id);
			line("C", row, id.key, drawn, fc.getNameRaw(), fc.isNameOverridden());
		}

		Table motors = table(MotorConfigurationPanel.class, "MotorTableCellRenderer", MotorMount.class, rocket);
		Table recovery = table(RecoveryConfigurationPanel.class, "RecoveryTableCellRenderer", RecoveryDevice.class,
				rocket);
		Table stages = table(SeparationConfigurationPanel.class, "SeparationTableCellRenderer", AxialStage.class,
				rocket);

		headers("MH", motors);
		for (int row = 0; row < motors.model.getRowCount(); row++) {
			JLabel label = motors.render(row, 0);
			line("M0", row, label.getText(), style(label));
			for (int col = 1; col < motors.model.getColumnCount(); col++) {
				Pair<FlightConfigurationId, MotorMount> cell = cell(motors, row, col);
				JLabel outer = motors.render(row, col);
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

		headers("RH", recovery);
		for (int row = 0; row < recovery.model.getRowCount(); row++) {
			for (int col = 1; col < recovery.model.getColumnCount(); col++) {
				Pair<FlightConfigurationId, RecoveryDevice> cell = cell(recovery, row, col);
				JLabel label = recovery.render(row, col);
				RecoveryDevice device = cell.getV();
				FlightConfigurationId id = cell.getU();
				line("R", row, col, label.getText(), style(label), device.getDeploymentConfigurations().isDefault(id),
						device.getDeploymentConfigurations().containsId(id));
			}
		}

		headers("TH", stages);
		for (int row = 0; row < stages.model.getRowCount(); row++) {
			for (int col = 1; col < stages.model.getColumnCount(); col++) {
				Pair<FlightConfigurationId, AxialStage> cell = cell(stages, row, col);
				JLabel label = stages.render(row, col);
				AxialStage stage = cell.getV();
				FlightConfigurationId id = cell.getU();
				line("T", row, col, label.getText(), style(label), stage.getSeparationConfigurations().isDefault(id),
						stage.getSeparationConfigurations().containsId(id));
			}
		}

		TableModel mounts = mountModel(rocket);
		for (int row = 0; row < mounts.getRowCount(); row++) {
			line("P", row, mounts.getValueAt(row, 0), mounts.getValueAt(row, 1));
		}

		if (!ids.isEmpty()) {
			int last = ids.size() - 1;
			FlightConfiguration fc = rocket.getFlightConfiguration(ids.get(last));
			String raw = fc.getNameRaw();
			for (String name : NAMES) {
				fc.setName(name);
				line("N", name, motors.render(last, 0).getText());
			}
			fc.setName(raw);
		}
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

	static void headers(String tag, Table table) {
		for (int col = 0; col < table.model.getColumnCount(); col++) {
			line(tag, col, table.model.getColumnName(col));
		}
	}

	@SuppressWarnings("unchecked")
	static <T> Pair<FlightConfigurationId, T> cell(Table table, int row, int col) {
		return (Pair<FlightConfigurationId, T>) table.model.getValueAt(row, col);
	}

	/** A panel's table: its model, a JTable over it and the panel's renderer. */
	static final class Table {
		final AbstractTableModel model;
		final JTable table;
		final TableCellRenderer renderer;

		Table(AbstractTableModel model, TableCellRenderer renderer) {
			this.model = model;
			this.table = new JTable(model);
			this.renderer = renderer;
		}

		JLabel render(int row, int col) {
			return (JLabel) renderer.getTableCellRendererComponent(table, table.getValueAt(row, col), false, false,
					row, col);
		}
	}

	/**
	 * The table of the panel @p panelClass for @p rocket: the panel's own FlightConfigurableTableModel subclass
	 * (its anonymous class, or the base class where the panel uses it as it is) and its renderer @p rendererName.
	 */
	static Table table(Class<? extends FlightConfigurablePanel<?>> panelClass, String rendererName,
			Class<?> componentClass, Rocket rocket) throws Exception {
		FlightConfigurablePanel<?> panel = allocate(panelClass);
		setField(FlightConfigurablePanel.class, panel, "rocket", rocket);
		// What the field initialisers of the panel and its base class would have set and the renderers read:
		// the translator (RecoveryConfigurationPanel's own instance field) and the RocketDescriptor.
		for (Class<?> c = panelClass; c != JPanel.class; c = c.getSuperclass()) {
			for (Field field : c.getDeclaredFields()) {
				if (Modifier.isStatic(field.getModifiers())) {
					continue;
				}
				if (field.getType() == Translator.class) {
					setField(c, panel, field.getName(), Application.getTranslator());
				} else if (field.getType() == RocketDescriptor.class) {
					setField(c, panel, field.getName(), descriptor);
				}
			}
		}
		AbstractTableModel model = panelModel(panelClass, panel, componentClass, rocket);
		Class<?> rendererClass = Class.forName(panelClass.getName() + "$" + rendererName);
		Constructor<?> constructor = rendererClass.getDeclaredConstructor(panelClass);
		constructor.setAccessible(true);
		return new Table(model, (TableCellRenderer) constructor.newInstance(panel));
	}

	/**
	 * The model @p panelClass makes in initializeTable(): the anonymous subclass of FlightConfigurableTableModel
	 * among its compiled classes (the motor and stage panels: includeComponent() isMotorMount() and
	 * getStageNumber() &gt; 0), or FlightConfigurableTableModel itself (the recovery panel has none).
	 */
	static AbstractTableModel panelModel(Class<?> panelClass, Object panel, Class<?> componentClass, Rocket rocket)
			throws Exception {
		Class<?> found = null;
		for (int n = 1; n <= 20; n++) {
			Class<?> candidate;
			try {
				candidate = Class.forName(panelClass.getName() + "$" + n);
			} catch (ClassNotFoundException e) {
				break;
			}
			if (candidate.getSuperclass() == FlightConfigurableTableModel.class) {
				if (found != null) {
					throw new IllegalStateException("two table models in " + panelClass.getName());
				}
				found = candidate;
			}
		}
		if (found == null) {
			return new FlightConfigurableTableModel<>(cast(componentClass), rocket);
		}
		Constructor<?>[] constructors = found.getDeclaredConstructors();
		if (constructors.length != 1 || constructors[0].getParameterCount() != 3) {
			throw new IllegalStateException("unexpected constructors of " + found.getName());
		}
		constructors[0].setAccessible(true);
		return (AbstractTableModel) constructors[0].newInstance(panel, componentClass, rocket);
	}

	@SuppressWarnings({"unchecked", "rawtypes"})
	static Class cast(Class<?> componentClass) {
		return componentClass;
	}

	/** OpenRocket's MotorMountTableModel (package-private) over @p rocket. */
	static TableModel mountModel(Rocket rocket) throws Exception {
		Class<?> modelClass = Class.forName(
				"info.openrocket.swing.gui.dialogs.flightconfiguration.MotorMountTableModel");
		Constructor<?> constructor = modelClass.getDeclaredConstructor(Rocket.class);
		constructor.setAccessible(true);
		return (TableModel) constructor.newInstance(rocket);
	}

	@SuppressWarnings("unchecked")
	static <T> T allocate(Class<T> type) throws Exception {
		Field field = Class.forName("sun.misc.Unsafe").getDeclaredField("theUnsafe");
		field.setAccessible(true);
		Object unsafe = field.get(null);
		return (T) unsafe.getClass().getMethod("allocateInstance", Class.class).invoke(unsafe, type);
	}

	static void setField(Class<?> owner, Object target, String name, Object value) throws Exception {
		Field field = owner.getDeclaredField(name);
		field.setAccessible(true);
		field.set(target, value);
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

	static void bootstrap() throws Exception {
		// Before anything makes a UUID: each scenario's design gets the same UUIDs on every run.
		DeterministicUuids.install();
		DeterministicUuids.reseed("flight-config-tables");
		Path bundledDb = Path.of(System.getProperty("probe.motors"));
		Path dbCopy = Files.createTempFile("probe-motors", ".db");
		Files.copy(bundledDb, dbCopy, StandardCopyOption.REPLACE_EXISTING);
		dbCopy.toFile().deleteOnExit();
		Module swingPrefs = new AbstractModule() {
			@Override
			protected void configure() {
				bind(ApplicationPreferences.class).to(SwingPreferences.class);
			}
		};
		Application.setInjector(Guice.createInjector(Modules.override(new ServicesForTesting()).with(swingPrefs),
				new PluginModule()));
		List<ThrustCurveMotor> motorList = ThrustCurveMotorSQLiteDatabase.readDatabase(dbCopy.toFile());
		ThrustCurveMotorSetDatabase motors = new ThrustCurveMotorSetDatabase();
		for (ThrustCurveMotor m : motorList) {
			motors.addMotor(m);
		}
		ComponentPresetDatabase presets = new ComponentPresetDatabase();
		Module dbOverrides = new AbstractModule() {
			@Override
			protected void configure() {
				bind(ApplicationPreferences.class).to(SwingPreferences.class);
				bind(ComponentPresetDao.class).toInstance(presets);
				bind(ThrustCurveMotorSetDatabase.class).toInstance(motors);
				bind(MotorDatabase.class).to(ThrustCurveMotorSetDatabase.class);
			}
		};
		Application.setInjector(Guice.createInjector(Modules.override(new ServicesForTesting()).with(dbOverrides),
				new PluginModule()));
	}
}
